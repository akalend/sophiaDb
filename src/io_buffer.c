#include "io_buffer.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DISCARD_THRESHOLD	1024

/** Initialize input buffer structure.
  * @param $b input buffer.
  * @param $pool pool for memory allocation.
  * @param $initsize initial capacity.
  */
void ibuffer_init(struct ibuffer *b, bsize_t initsize, void *s_buf)
{
	if (s_buf) {
		b->data = s_buf;
		b->flags |= IBUFFER_STATIC;
	} else {
		b->data = malloc(initsize);
		b->flags = 0;
	}
	b->length = 0;
	b->capacity = initsize;
}

/** Reset input buffer structure.
  * @param $b input buffer.
  * @param $pool pool for memory allocation.
  * @param $initsize initial capacity.
  */
void ibuffer_reset(struct ibuffer *b, bsize_t initsize, void *s_buf)
{
	if (!(b->flags & IBUFFER_STATIC) && b->data)
		free(b->data);
	ibuffer_init(b, initsize, s_buf);
}

/** Free dynamically allocated buffer.
  * @param $b input buffer.
  */
void ibuffer_free(struct ibuffer *b)
{
	if (!(b->flags & IBUFFER_STATIC) && b->data)
		free (b->data);
}

/** Resize input buffer so it can contain as lease size bytes.
  * @param $b input buffer.
  * @param $size new capacity.
  */
void ibuffer_reserve(struct ibuffer *b, bsize_t size)
{
	while (b->capacity < size)
		b->capacity = (b->capacity + 1) * 2;
	if (b->flags & IBUFFER_STATIC) {
		void *new_data = malloc(b->capacity);
		if (b->length) memcpy(new_data, b->data, b->length);
		b->data = new_data;
		b->flags &= ~IBUFFER_STATIC;
	} else {
		b->data = realloc(b->data, b->capacity);
	}
}

/** Append data to buffer contents (internal). */
static void ibuffer_append(struct ibuffer *b, const void *data, bsize_t size)
{
	if (b->length + size <= b->capacity) {
		ibuffer_reserve(b, b->length + size);
	}
	memcpy(b->u_data + b->length, data, size);
	b->length += size;
}

/** Discard data from input queue.
  * @param $b input buffer.
  * @param $size bytes to discard.
  */
void ibuffer_discard(struct ibuffer *b, bsize_t size)
{
	/* Disard all data */
	if (size == 0 || size >= b->length) {
		b->length = 0;
	} else {
		memmove(b->u_data, b->u_data + size, b->length - size);
		b->length -= size;
	}
}

/** Read from file descriptor until buffer fully filled (capacity reached).
  * @param $b input buffer.
  * @param $fd file descriptor to read from.
  * @return end-of-file flag in b->flags and <ul>
  *         <li>-1 and errno from last read operation on error.
  *         <li>number of bytes read from $fd.
  *         </ul>
  */
bssize_t ibuffer_read(struct ibuffer *b, int fd)
{
	bssize_t bytes = 0;
	ssize_t nbytes;
	if (b->length == b->capacity) ibuffer_reserve(b, (b->capacity + 1) * 2);
	while (b->length + bytes < b->capacity) {
		bsize_t s = b->capacity - b->length - bytes;
		nbytes = read(fd, b->u_data + b->length + bytes, s);
		if (nbytes > 0) {
			bytes += nbytes;
		} else if (nbytes < 0) {
			if (errno == EAGAIN) break;
			if (errno == EINTR) continue;
			return -1;
		} else {
			b->flags |= IBUFFER_EOF;
			break; // EOF
		}
	}
	b->length += bytes;
	return bytes;
}

/** Read from file descriptor up to N bytes.
  * @param $b input buffer.
  * @param $size maximum count of bytes to read.
  * @param $fd file descriptor to read from.
  * @return end-of-file flag in b->eof and <ul>
  *         <li>-1 and errno from last read operation on error.
  *         <li>number of bytes read from $fd.
  *         </ul>
  */
bssize_t ibuffer_readn(struct ibuffer *b, int fd, bsize_t size)
{
	if (b->length + size < b->capacity) ibuffer_reserve(b, b->length + size);
	return ibuffer_read(b,fd);
}

/** Send shared data.
  * @param $b buffer.
  * @param $data data to send (MUST be available while buffer object exists).
  * @param $size data length in bytes.
  */
void obuffer_init(struct obuffer *b, const void *data, bsize_t size)
{
	b->data = data;
	b->pos = 0;
	b->length = size;
}

/** Send data to socket.
  * @param $b buffer.
  * @param $fd file descriptor to send to.
  * @return -1 on error, bytes letf to send when success, so 0 meand all data had been sent.
  */
int obuffer_send(struct obuffer *b, int fd)
{
	while (b->pos < b->length) {
		ssize_t bytes = write(fd, b->u_data + b->pos, b->length - b->pos);
		if (bytes > 0) {
			b->pos += bytes;
		} else {
			if (errno == EAGAIN) break;
			if (errno == EINTR) continue;
			return -1;
		}
	}
	return b->length - b->pos;
}

