#include "main.h"
#include "ini.h"
#include "mc.h"

extern void parse(const char* fname, conf_t *server_ctx);

int parser(void* pctx, const char* section, const char* name,
           const char* value) {
	conf_t* ctx = (conf_t*) pctx;
	static datatype_t *p = NULL;
	datatype_t *tmp = NULL;

	int tmp_int = 0;
	int max_num=0;
	
    if (strcmp(section, "daemon") == 0) {
//		printf("[%s] %s=%s;\n", section, name, value);
		
		if(strcmp("daemon", name)==0) {
			sscanf(value,"%d",&tmp_int);
			ctx->is_demonize = tmp_int;			
		}
		
		if(strcmp("trace" , name)==0) {
			sscanf(value,"%d",&tmp_int);
			ctx->trace = tmp_int;
		}
		
		if(strcmp("level", name)==0) {			
			//sscanf(value,"%d",&tmp_int);
			ctx->level = 0;
			if (strcmp("error",value) ) 	ctx->level = 1;
			if (strcmp("warning",value) ) 	ctx->level = 2;
			if (strcmp("notice",value) ) 	ctx->level = 3;
		}

		if(strcmp("logfile", name)==0) {			
			ctx->logfile = strdup(value);
		}
		
		if(strcmp("pidfile", name)==0) {			
			ctx->pidfile = strdup(value);
		}
		
		if(strcmp("listen", name)==0) {			
				ctx->listen = strdup(value);
		}

		if(strcmp("username", name)==0) {			
			if (strcmp("any", value))
				ctx->username = strdup(value);
		}

		if(strcmp("datadir", name)==0) {			
			ctx->datadir = strdup(value);
		}

	} else if (strcmp(section, "data") == 0){

		if (strcmp("number", name) == 0) {
			if(ctx->list_datatypes) {
				tmp = p;
				
				p = malloc(sizeof(datatype_t));
				if (!p) {
					(void)fprintf(stderr, "mpool Error : %s\n",	strerror(errno));
			 	}
				
				// printf("alloc %x\n", (unsigned) p);
				
				assert(p);				
				p->next = NULL;
				p->comment = NULL;
				tmp->next =  p;//(datatype_t*)
			} else {				

				p = (datatype_t*) calloc(1, sizeof(datatype_t));
				// printf("alloc %x\n", (unsigned) p);
				if (!p) {
					(void)fprintf(stderr, "mpool Error:%s %s\n", __LINE__,	strerror(errno));
			 	}

				ctx->list_datatypes = p;
				assert(ctx->list_datatypes);				
			}
			
			sscanf(value,"%d",&tmp_int);
			p->number = tmp_int;
			
			ctx->list_size++;
			if (tmp_int > ctx->max_num) 
				ctx->max_num = tmp_int;
		}

		assert(p);
		
		if(strcmp("comment", name)==0) {
			
			p->comment = strdup(value);
			
			if (!p->comment) {
				(void)fprintf(stderr, "mpool Error:%d : %s\n",	__LINE__, strerror(errno));
		 	}
		}
		
		if(strcmp("datadir", name)==0) {
			
			p->datadir = strdup(value);
			
			if (!p->datadir) {
				(void)fprintf(stderr, "mpool Error:%d  %s\n", __LINE__, strerror(errno));
		 	}
		}


		if(strcmp("type", name)==0) {			
			if (strcmp("int",value)==0) {
				p->type = SPHDB_INT;
			}

			if (strcmp("long",value)==0) {
				p->type = SPHDB_LONG;
			}

			if (strcmp("string",value)==0) {
				p->type = SPHDB_STRING;
			}

		}

	}
	
	return 0;
}

void parse(const char* fname, conf_t *server_ctx) {
    bzero(server_ctx, sizeof(conf_t));
    ini_parse(fname, parser, (void*)server_ctx);
    
}
