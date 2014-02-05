#include "mc.h"

#ifndef __sophiadb_mc__
#define __sophiadb_mc__
#endif
#ifdef __sophiadb_mc__

#include <sophia.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <ev.h>
#define BUFSIZE 512

//TODO nax...

extern FILE 			*flog;
extern int				max_clients;
extern fd_ctx			*clients;
extern int 				is_finish;
extern int 				is_trace;

extern struct timeval t_start; 				// start timeinterval
extern struct timeval t_end;				// finish timeinterval

extern 	 struct {
	/* some stat data */
	unsigned	connections;				//  active connections (memcache clients) 
	unsigned	cnn_count;				//  count connectionss 
	unsigned	cmd_per;					//  count of commands into period	
	unsigned	cmd_count;					//  count of commands
	float		rps;						//  last count of commands per second
	float		rps_peak;					//  peak of commands per second	
	unsigned	get;						//  count of get commands
	unsigned	set;						//  count of set/append/prepend/incr/decr  commands
	unsigned	del;						//  count of delete  commands
	unsigned	inc;						//  count of increment/decrement  commands		
	unsigned	miss;						//  count of miss keys (key not found)
	time_t 		uptime;						// uptime server
	unsigned	err;						//  count of errors
} stats;

void periodic_watcher(EV_P_ ev_timer *t, int revents);
static ev_io* memcached_client_new(int sock);
static void memcached_client(EV_P_ ev_io *io, int revents);
static void memcached_client_free(db_t *ctx);
static int setup_socket(int sock);

int num_digits(unsigned x)  
{  
    return (x < 10 ? 1 :   
        (x < 100 ? 2 :   
        (x < 1000 ? 3 :   
        (x < 10000 ? 4 :   
        (x < 100000 ? 5 :   
        (x < 1000000 ? 6 :   
        (x < 10000000 ? 7 :  
        (x < 100000000 ? 8 :  
        (x < 1000000000 ? 9 :  
        10)))))))));  
}


void close_io(EV_P_ ev_io *io)
{
	ev_io_stop(EV_A_ io);
	close(io->fd);
}

void close_all(EV_P) {
	int i;
	for (i=0; i < max_clients; i++) {
		if (clients[i].flags ) {
			close_io(EV_A_ clients[i].io);
		}

		if(clients[i].mc) {
			if(clients[i].mc->value)
				free(clients[i].mc->value);
			clients[i].mc->value = NULL;
		
			free(clients[i].mc);
			clients[i].mc = NULL;
		}
	}
}

void cllear_mc_all()
{
	int i;
	for (i=0; i < max_clients; i++) {
		if (clients[i].mc) {
			if (clients[i].mc->value) {
				free(clients[i].mc->value);
				clients[i].mc->value = NULL;
			}	
			free(clients[i].mc);
			clients[i].mc = NULL;
		}
		//if(FD_ACTIVE) close_io(EV_A_ clients[i].io);
	}

}

void
periodic_watcher(EV_P_ ev_timer *t, int revents)
{
	gettimeofday(&t_end, NULL);
	long mtime, seconds, useconds;    

    seconds  = t_end.tv_sec  - t_start.tv_sec;
    useconds = t_end.tv_usec - t_start.tv_usec;
    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	stats.rps = stats.cmd_per * 1000 /mtime ;
	if(stats.rps_peak < stats.rps)	stats.rps_peak = stats.rps;
	stats.cmd_per = 0;

	gettimeofday(&t_start, NULL);
}


/* Set misc socket parameters: non-blocking mode, linger, enable keep alive. */
static int 
setup_socket(int sock)
{
	int keep_alive = 1;
	struct linger l = {0, 0};
	if (set_nonblock(sock, 1) == -1) return -1;
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) return -1;
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) == -1) return -1;
	return 0;
}

static void
memcached_client_free(db_t *ctx)
{
	stats.connections--;	
	if(is_trace)
		printf("connection fd=%d free [%d]\n", ctx->io.fd, stats.connections);

	if (!ctx) return;

	// if (ctx->value) free(ctx->value);
	// free(ctx);
	// clients[ctx->io.fd].mc = NULL;
}

/*process data for set*/
static int 
memcached_process_set( db_t *mctx, sophiadb_t* ctx) {

	printf("save[len=%d] %s:%s\n",strlen(mctx->value),  mctx->key, mctx->value);
	int rc = sp_set(ctx->db, &mctx->key, strlen(mctx->key), mctx->value, sizeof(mctx->value));
	if (rc == -1) {
		printf("sp_set: %s\n", sp_error(ctx->db));
		return 1;
	}
	
	memset(mctx->value, BUFSIZE, '\0');
	memset(mctx->key, BUFSIZE, '\0');
	
	stats.set ++;
	return 0;
}	


static void 
memcached_cb_set(EV_P_ ev_io *io, int revents) {

	db_t *mctx = (db_t*)io;

	if(is_trace) 
		printf("\n--------  data len=%d -----------\n'%s'\n", mctx->value_size - mctx->value_len, mctx->value);

	while (mctx->value_len < mctx->value_size) {
		size_t bytes = read(io->fd, mctx->value + mctx->value_len, mctx->value_size - mctx->value_len);
		if (bytes > 0) {
			if (bytes > BUFSIZE) {
				fprintf(flog, "%s reciive bytes %d more than BUFSIZE\n", __FUNCTION__, (int)bytes);
				goto error;
			}
			mctx->value_len += bytes;
		} else if(bytes == -1){
			if (errno == EAGAIN) return;  
			if (errno == EINTR) continue; 
			goto disconnect;
		}		
	}
	
	set_client_timeout(io, RECV_TIMEOUT);
	mctx->value[mctx->value_size-2] = 0;
	
	sophiadb_t* ctx = ev_userdata(EV_A);	
	int ret = memcached_process_set(mctx, ctx);
				
	switch (ret) {
		case 1 : goto error; break;
		case 2 : goto notstored; break;
	}

	if(mctx->key) {
		free(mctx->key);
		mctx->key = NULL;
	}
	mctx->cmd_len = 0;
	ev_io_stop(EV_A_ io);
	ev_set_cb(io, memcached_client);
	ev_io_set(io, io->fd, EV_WRITE);
	obuffer_init(&mctx->response, "STORED\r\n", sizeof("STORED\r\n") - 1);
	ev_io_start(EV_A_ io);
	return;

error:
	mctx->cmd_len = 0;

	ev_io_stop(EV_A_ io);
	ev_set_cb(io, memcached_client);
	ev_io_set(io, io->fd, EV_WRITE);
	obuffer_init(&mctx->response, "ERROR\r\n", sizeof("ERROR\r\n") - 1);
	ev_io_start(EV_A_ io);
	return;

notstored:
	mctx->cmd_len = 0;
	ev_io_stop(EV_A_ io);
	ev_set_cb(io, memcached_client);
	ev_io_set(io, io->fd, EV_WRITE);
	obuffer_init(&mctx->response, "NOT_STORED\r\n", sizeof("NOT_STORED\r\n") - 1);
	ev_io_start(EV_A_ io);
	return;

disconnect:	
	close_io(EV_A_ io);	
	return;	
}

/* memcache GET handler */
int memcache_get(EV_P_  db_t *mctx) {
	int len=0;
	char *key = mctx->cmd + 4;
	char *p = key;

	while(1) {
		if (*p == ' ' || *p == '\r' || *p == '\n' ) break;
		p++;
		len++;
	}
	
	*(key+len-1) = '\0';

	sophiadb_t* ctx = ev_userdata(EV_A);		
	memset(mctx->value, BUFSIZE, '\0');

	printf("key: %s\n", key);
	
	char* pdata = NULL;
	size_t valuesize;

	int rc = sp_get(ctx->db, key, len-1, &pdata, &valuesize);
	if (rc == -1) {
		printf("sp_get: %s\n", sp_error(ctx->db));
		goto error;
	}

	printf("key[%d]: '%s', len=%d value='%s'\n", len,key, valuesize,  pdata);
	
	free(pdata);

	len = snprintf(mctx->value, BUFSIZE, "VALUE %s 0 %d\r\n%s\r\nEND\r\n", key, valuesize, pdata);
	
	if (pdata) free(pdata);

	obuffer_init(&mctx->response, mctx->value, len);
	
	stats.get ++;
	return 0;

error:
	return 1;
end:
	stats.miss ++;
	return 2;	

}


/* memcache DELETE handler */
int memcache_del(EV_P_  db_t *mctx) {
	int len;
	char  *key = mctx->cmd + 7;
	char *p=key;

	while(1) {
		if (*p == ' ' || *p == '\r' || *p == '\n' ) break;
		p++;
		len++;
	}
	
	*(key+len-1) = '\0';
	sophiadb_t* ctx = ev_userdata(EV_A);	

	memset(mctx->value, BUFSIZE, '\0');
	
	// ctx->ecode = 0;
	// bool res = tchdbout2(ctx->hdb, key);
	// if (res) {
	// 	len=9;
	// 	strcpy(mctx->value, "DELETED\r\n" );		
	// } else {
	// 	ctx->ecode = tchdbecode(ctx->hdb);
	// 	if (ctx->ecode == TCENOREC) {
	// 		len=11;
	// 		stats.miss ++;
	// 		strcpy(mctx->value, "NOT_FOUND\r\n" );	
			
	// 	} else 	{	
	// 		fprintf(flog, "res=false delete tcdb error[%d]: %s\n",ctx->ecode, tchdberrmsg(ctx->ecode));
	// 		goto error;
	// 	}
	// }
	
	mctx->value[len] = '\0';
	obuffer_init(&mctx->response, mctx->value, len);
	
	stats.del ++;
	return 0;

error:	
	return 1;
}


/* memcache SET handler */
int memcache_set(EV_P_  db_t *mctx, int readed, int  end) {

	/* Set command */
	unsigned  bytes;
	char *p;	
	p = mctx->cmd + 4;

	*(mctx->key) = '\0';
	
	if (sscanf(p, "%s %u %u %u", &mctx->key, &mctx->flag, &mctx->exptime, &bytes) != 4) {
		fprintf(flog, "%s[%d] sscanf error\n", __FUNCTION__,__LINE__);
		return 1;
	}

	int len = mctx->cmd_len - end;

	/* Don't send messages longer than 1 megabyte */
	if (!bytes || bytes > 1 << 20) {
		fprintf(flog, "%s invalid message length: %d\n", __FUNCTION__, bytes);
		return 1;
	}

	mctx->cmd_len = 0;

	mctx->data_size = bytes;
	mctx->value_size = bytes+2; // TODO ????
	mctx->value_len = len;
	
	if (readed > end) {
		if(is_trace) 
			printf("\n--------  data -----------\n'%s'\nlen=%d", mctx->cmd+end, readed-end-3);
			
		memcpy(mctx->value,mctx->cmd+end, readed-end);
		mctx->value[readed-end] = '\0';
		sophiadb_t* ctx = ev_userdata(EV_A);	

		if(memcached_process_set(mctx, ctx)) {
			return 1;
		}

		obuffer_init(&mctx->response, "STORED\r\n", sizeof("STORED\r\n") - 1);
		return 0;
	}
	

	if (len) {
		memcpy(mctx->value, mctx->cmd + end, len);
		/* Maybe we've read all value? */
		if (len == mctx->value_size) {
			mctx->value[bytes] = 0;
			fprintf( flog, " ATTENTION!! %s we are read all data\n", __FUNCTION__);
			//mctx->value = NULL;
			obuffer_init(&mctx->response, "STORED\r\n", sizeof("STORED\r\n") - 1);
			stats.err ++;
			*(mctx->key) = '\0';
			return 0;
		}

		*(mctx->key) = '\0';
		return 1;
	}
	
	set_client_timeout((ev_io *)mctx, RECV_TIMEOUT);	
	ev_set_cb((ev_io *)mctx, memcached_cb_set);
	
	return 2;
}

static void 
memcached_stats(EV_P_ db_t *mctx) {
			int len;
			char *statsbuf = malloc(BUFSIZE);

			time_t t;
			time(&t);
			len = snprintf(statsbuf, BUFSIZE,
					"STAT pid %ld\r\n"
					"STAT uptime %d\r\n"
					"STAT curent connections %u\r\n"
					"STAT total connections %u\r\n"
					"STAT rps %4.2f\r\n"
					"STAT peak rps %4.2f\r\n"					
					"STAT request %u\r\n"
					"STAT get %u\r\n"
					"STAT set %u\r\n"
					"STAT inc %u\r\n"
					"STAT del %u\r\n"
					"STAT miss %u\r\n"
					"STAT error %u\r\n"
					"END\r\n",
					(long)getpid(), (int)(t-stats.uptime) , stats.connections, stats.cnn_count, stats.rps, stats.rps_peak,
					stats.cmd_count, stats.get, stats.set, stats.inc, stats.del, stats.miss, stats.err );
			
			memcpy(mctx->value, statsbuf, len);
			free(statsbuf);
			obuffer_init(&mctx->response, mctx->value, len);

}

/* Handle line-based memcached-like protocol */
static void
memcached_client(EV_P_ ev_io *io, int revents) {
	
	db_t *mctx = ( db_t*)io;
	if (revents & EV_READ) {
		int end = 0;
		int i = mctx->cmd_len ? mctx->cmd_len - 1 : 0;
				
		/* Read command till '\r\n' (actually -- till '\n') */
		size_t bytes =0;
		while (mctx->cmd_len < MAX_COMMAND_LEN && !end) {
			bytes = read(io->fd, mctx->cmd + mctx->cmd_len, MAX_COMMAND_LEN - mctx->cmd_len);
			if (bytes > 0) {

				if (bytes > BUFSIZE) {
					fprintf( flog, "%s readed=%d more as BUFSIZE\n", __FUNCTION__, (int)bytes);
					goto send_error;
				}
				mctx->cmd_len += bytes;
				while (i < mctx->cmd_len - 1) {
					if (mctx->cmd[i] == '\r' && mctx->cmd[i+1] == '\n') {
						end = i + 2;						
						mctx->cmd[i] = 0;
						break;
					}
					i++;
				}
			} else if (bytes == -1) {
				if (errno == EAGAIN) break;
				if (errno == EINTR) continue;
				goto disconnect;
			} else goto disconnect;
		}
		
		/* If there is no EOL but string is too long, disconnect client */
		if (mctx->cmd_len >= MAX_COMMAND_LEN && !end) goto disconnect;
		/* If we haven't read whole command, set timeout */
		if (!end) {
			set_client_timeout(io, RECV_TIMEOUT);
			return;
		}
		
		stats.cmd_count++;
		stats.cmd_per++;
		/* handle set command */
		if (strncmp(mctx->cmd, "set ", 4) == 0) {

			switch (memcache_set(EV_A_ mctx, bytes, end)) {
			case 0:
				goto send_reply;
			case 1:
				goto send_error;
			case 2:
				return;
			}

		/* Handle "Get" command */	
		} else if (strncmp(mctx->cmd, "get ", 4) == 0) {

			switch (memcache_get(EV_A_ mctx)) {
			case 0:
				goto send_reply;
			case 1:
				goto send_error;
			case 2:
				goto send_end;
			}


		/* Handle "Dalete" command */
		} else if (strncmp(mctx->cmd, "delete ", 7) == 0) {

			if (memcache_del(EV_A_ mctx))
				goto send_error;
			else
				goto send_reply;				

		} else if (strncmp(mctx->cmd, "flush_all", 9) == 0) {
			sophiadb_t* ctx = ev_userdata(EV_A);	
			if(true) { //tchdbvanish(ctx->hdb)
				obuffer_init(&mctx->response, "OK\r\n", sizeof("OK\r\n") - 1);

				stats.inc = stats.rps_peak = stats.rps = stats.cmd_count = stats.get = stats.set = stats.del = stats.miss = stats.err = 0;

				goto send_reply;
			} else 
				goto send_error;

		/* Close connection */
		} if (strncmp(mctx->cmd, "quit", 4) == 0) {
			if (is_finish) goto exit;
			goto disconnect;			
			
		/* Terminate server */
		} if (strncmp(mctx->cmd, "term", 4) == 0) {
			goto exit;
						
		/* Statistics */
		} else if (strncmp(mctx->cmd, "stats", 5) == 0) {
				memcached_stats(EV_A_ mctx);
				goto send_reply;
		} else {
			/* Invalid command */
			fprintf(flog, "invalid command: \"%s\"\n", mctx->cmd);
			goto send_error;
		}
		
	} else if (revents & EV_WRITE) {
		
		switch (obuffer_send(&mctx->response, io->fd)) {
			case 0:	
				mctx->cmd_len = 0;			
				memset(mctx->cmd, 0, MAX_COMMAND_LEN);				
				reset_client_timeout(io);
				ev_io_stop(EV_A_ io);
				if (is_finish) goto exit;
				ev_io_set(io, io->fd, EV_READ);
				ev_io_start(EV_A_ io);
				break;
			case -1:
				goto disconnect;
		}
	}
	return;
send_error:
	stats.err ++;
	obuffer_init(&mctx->response, "ERROR\r\n", sizeof("ERROR\r\n") - 1);
	goto send_reply;

send_end:

	obuffer_init(&mctx->response, "END\r\n", sizeof("END\r\n") - 1);

send_reply:
	set_client_timeout(io, RECV_TIMEOUT);
	mctx->cmd_len = 0;
	ev_io_stop(EV_A_ io);
	ev_io_set(io, io->fd, EV_WRITE);
	ev_io_start(EV_A_ io);
	return;
disconnect:
	close_io(EV_A_ io);
	memcached_client_free(mctx);

	return;
exit:
	close_all(EV_A );	
	ev_unloop(EV_A_ EVUNLOOP_ALL); 	
}

/* Create connections context */
static ev_io*
memcached_client_new(int sock) {

	if(is_trace)
		printf("%s: new connection [%d]", __FUNCTION__, sock);

	if (setup_socket(sock) != -1) {
		db_t *mctx = calloc(1, sizeof(db_t));

		if (!mctx) {
			fprintf(flog, "%s: allocate error size=%d\n", __FUNCTION__, (int)sizeof(db_t));
			return NULL;
		}

		if (!mctx->value) {
			mctx->value = malloc(BUFSIZE);		
			if (!mctx->value) {
				fprintf(flog, "%s: allocate error size=%d\n", __FUNCTION__, BUFSIZE);
				return NULL;
			}
		}


	if (!mctx->key) {
		mctx->key = malloc(BUFSIZE);
		if (!mctx->key) {
			fprintf(flog, "%s: allocate error size=%d\n", __FUNCTION__, BUFSIZE);
			return NULL;
		}
	}


	ev_io_init(&mctx->io, memcached_client, sock, EV_READ);
	clients[sock].io = &mctx->io;
	clients[sock].cleanup = (cleanup_proc)memcached_client_free;
	clients[sock].mc = mctx;

	stats.connections++;
	stats.cnn_count++;
	if(is_trace)
		printf(" Ok\n");
		return &mctx->io;
	} else {
		if(is_trace)
			printf(" Fail\n");
		return NULL;
	}
}

/* Serve connectionss */
void
memcached_on_connect(EV_P_ ev_io *io, int revents) {
	
	while (1) {
		int client = accept(io->fd, NULL, NULL);
		
		if (client >= 0) {				
			ev_io *mctx = memcached_client_new(client);
			if (mctx) {
				ev_io_start(EV_A_ mctx);
			} else {
				fprintf(flog, "failed to create connections context %s", strerror(errno));
				close(client);
			}
		} else {
			if (errno == EAGAIN)
				return;
			if (errno == EMFILE || errno == ENFILE) {
				fprintf(flog, "out of file descriptors, dropping all clients. %s", strerror(errno));
				close_all(EV_A);
			} else if (errno != EINTR) {
				fprintf(flog, "accept_connections error: %s", strerror(errno));
			}
		}
		
	}
}

#endif
