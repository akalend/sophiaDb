#pragma once
#include <stdint.h>

typedef unsigned bsize_t;
typedef int bssize_t;

#define IBUFFER_EOF		1
#define	IBUFFER_STATIC	2

/** Input buffer */
struct ibuffer {
	union {
		char		*c_data;		/**< buffer data as character array */
		uint8_t		*u_data;		/**< buffer data as array of bytes */
		void		*data;			/**< opaque */
	};
	bsize_t		length;				/**< length of data (in bytes) */
	bsize_t		capacity;			/**< size of data in bytes */
	int			flags;				/**< set when EOF received from last read operation. */
};

struct obuffer {
	union {
		const char		*c_data;
		const uint8_t	*u_data;
		const void		*data;
	};
	bsize_t		pos;
	bsize_t		length;
};

/* Initialize buffer of specified capacity. */
void ibuffer_init(struct ibuffer *b, bsize_t initsize, void *s_buf);
/** Resize input buffer so it can contain as lease size bytes. */
void ibuffer_reserve(struct ibuffer *b, bsize_t size);
/* Remove first pos bytes from buffer */
void ibuffer_discard(struct ibuffer *b, bsize_t pos);
/* Read buffer until it's full capacity or EAGAIN/EOF */
bssize_t ibuffer_read(struct ibuffer *b, int fd);
/* Free dynamically allocated buffer */
void ibuffer_free(struct ibuffer *b);
/* Reset buffer size */
void ibuffer_reset(struct ibuffer *b, bsize_t initsize, void *s_buf);


void obuffer_init(struct obuffer *b, const void *data, bsize_t size);
int obuffer_send(struct obuffer *b, int fd);

