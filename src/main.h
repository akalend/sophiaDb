#ifndef __mymc_main__
#define __mymc_main__
#endif
#ifdef __mymc_main__

#include <unistd.h>
#include <fcntl.h>

#include <ev.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <sys/resource.h>
#include <syslog.h>
#include <sys/types.h>
#include <pwd.h>
#include <getopt.h>

#include <assert.h>
#include <string.h>

#include <tchdb.h>
#include <tcutil.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

//#include "mqueue.h"
#include "io_buffer.h"

#define LIKE_CACHESIZE 			16777216; 

#define MAX_COMMAND_LEN			128

#define LK_STORAGE_FILENAME  "storage.tch"
#define LK_STORAGE_FILENAME_LEN  sizeof(LK_STORAGE_FILENAME)


#define LK_KEYSIZE sizeof(lk_key)
#define LK_LIKESIZE sizeof(unsigned)


// typedef struct {
// 	unsigned data;
// 	unsigned crc;	
// } lk_data;

void perror_fatal(const char *what);

#endif
