/*
 *       Filename:  buffer.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  02/02/2010 02:11:12 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Greg Leclercq (ggl), ggl@0x80.net 
 */

#ifndef _NETWORK_BUFFER_
#define _NETWORK_BUFFER_ 1

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define BUFFER_CHUNK_SIZE 4096


struct buffer {
        char    *base;
        char    *data;
        char    *ptr;
        unsigned int size;
        unsigned int max_size;
        unsigned int chunk_size;
};

static inline
struct buffer *
buffer_new(void)
{
	struct buffer *buf = malloc(sizeof(*buf));
	if (!buf)
		return NULL;

	buf->chunk_size = BUFFER_CHUNK_SIZE;
	buf->max_size = buf->chunk_size;
	buf->base = malloc(buf->max_size);
	if (!buf->base)
		goto fail_base;
	buf->data = buf->base;
	buf->ptr = buf->data;
	buf->size = 0;

	return buf;

fail_base:
	free(buf);
	return NULL;
}

static inline
void
buffer_free(struct buffer *buf)
{
	assert(buf != NULL);
	if (buf->base)
		free(buf->base);
	free(buf);
}

static inline
unsigned int
buffer_get_size(struct buffer *buf)
{
	return buf->size; 
}

static inline
char *
buffer_get_data(struct buffer *buf)
{
	return buf->data;
}

static inline
int
buffer_append(struct buffer * const buf,
		const char * const data,
		const size_t data_len)
{
	assert(buf != NULL);
	if (data_len > buf->max_size-buf->size) {
		buf->max_size += buf->chunk_size;
		buf->base = realloc(buf->data, buf->max_size);
		if (!buf->data)
			return errno;
	}
	memcpy(buf->ptr, (char *) data, data_len);
	buf->ptr += data_len;
	buf->size += data_len;

	return 0;
}

static const char endstring = '\0';

static inline
int
buffer_terminate_string(struct buffer * const buf)
{
	return buffer_append(buf, &endstring, sizeof(endstring));
}

static inline
int
buffer_append_string(struct buffer * const buf,
		const char * const str)
{
	const size_t len = strlen((char *) str);
	
	int err = buffer_append(buf, str, len);
	if (err)
		return err;
	err = buffer_terminate_string(buf);
	return err;
}

static inline
uint32_t
buffer_pull(struct buffer * const buf, uint32_t len)
{
	if (buf->ptr - buf->data > len)
		len = buf->data - buf->ptr;
	buf->data += len;
	buf->size -= len;
	return len;
}

static inline
uint32_t
buffer_pull_into(struct buffer * const buf,
		char * const dst, uint32_t len)
{
	assert(buf != NULL);
	memcpy(dst, buf->ptr, len);
	return buffer_pull(buf, len);
}

static inline
int
buffer_clear(struct buffer * const buf)
{
	if (buf->max_size > buf->chunk_size) {
		buf->base = realloc(buf->base, buf->chunk_size);
		if (buf->base == NULL)
			return errno;
		buf->max_size = buf->chunk_size;
	}
	buf->data = buf->base;
	buf->ptr = buf->data;
	buf->size = 0; 
	return 0;
}


#endif

/* vim=ts=8:sw=8:noet
*/
