#ifndef __sophiadb_mch__
#define __sophiadb_mch__
#endif
#ifdef __sophiadb_mch__

#include "main.h"
#include <sys/socket.h>


#define set_client_timeout(io,t_o) clients[(io)->fd].timeout = ev_now(EV_A) + t_o
#define reset_client_timeout(io) clients[(io)->fd].timeout = 0

#define	TIMEOUT_CHECK_INTERVAL	10
#define	TIME_CHECK_INTERVAL		5

#define	RECV_TIMEOUT			5

#define	LPOLL_TIMEOUT			25		/* interval of time after which client will get 304 not modified response */


#define SPHDB_STRLEN			256
enum {
	SPHDB_UNDEF,
	SPHDB_INT,
	SPHDB_LONG,
	SPHDB_STRING
};

typedef void (*cleanup_proc)(ev_io *data);



typedef struct {
	void* 			env;
	void* 			db;
	char*			datadir;
	int 			type;
	int 			datalen;
} sophiadb_t;


typedef struct {
	void * 			next;
	int 			number;
	char* 			comment; 				// the data type information
	int 			type;
	char* 			datadir;
} datatype_t;

typedef struct {
	char* 			logfile;
	int 			level; // error output level	
	char* 			listen;
	char* 			pidfile;
	char * 			username;
	short 			is_demonize;	
	short 			trace;
	char* 			datadir;
	int 			list_size;
	int 			max_num;
	datatype_t* 	list_datatypes;
	sophiadb_t* 	db; 
} conf_t;


typedef struct {
	union {
		struct sockaddr		name;
		struct sockaddr_in	in_name;
		struct sockaddr_un	un_name;
	};
	int			pf_family;
	socklen_t	namelen;
	char		*a_addr;
} addr_t;

typedef struct {
	ev_io		io;							/**< io descriptor */
	char		cmd[MAX_COMMAND_LEN];		/**< buffer for line-buffered input */
	int			cmd_len;					/**< bytes in line buffer */
	struct		obuffer response;			/**< response data */
	char		*value;						/**< key value from last set command */
	int			value_len;					/**< number of bytes in value buffer */
	int			value_size;					/**< capacity of value buffer */	
	int			data_size;					/**< value   size into cmd */	
	unsigned	flag;						/**< request flag read */
	unsigned	exptime;					/**< request expire read */
	char*		key;						/**< request key read */
	int 		mode;
} db_t;

typedef struct  {
	ev_tstamp					timeout;		/**< time, when this socked should be closed. */
	int							flags;			/**< flags mask */
	cleanup_proc				cleanup;		/**< cleanup handler */
	struct timeval				time;
	union {
		ev_io					*io;			/**< private data */
		db_t					*mc;		/**< publisher */
	};
} fd_ctx;


void cllear_mc_all();

void 
close_io(EV_P_ ev_io *io);

void 
close_all(EV_P);


/* Serve  */
void
memcached_on_connect(EV_P_ ev_io *io, int revents);

void
periodic_watcher(EV_P_ ev_timer *t, int revents);


/*  declared server.c */
int
set_nonblock(int sock,int value);

#endif
