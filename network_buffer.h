/*
 * This file is part of libsimplenet.
 *
 * Copyright (C) 2010 Greg Leclercq <ggl@0x80.net> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 or version 3.0 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _NETWORK_BUFFER_
#define _NETWORK_BUFFER_ 1

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


/*
 * data    tail
 * v        v
 *  ______________
 * |--|XX|XX|00|00|
 *    ^           ^
 *   head        end
 */
struct simple_buffer {
        char    *data;
        char    *head;
	char	*tail;
	char	*userptr;
        uint32_t size;
        uint32_t max_size;
        uint32_t chunk_size;
};


static inline
struct simple_buffer *
simple_buffer_new(uint32_t chunk_size)
{
	struct simple_buffer *buf = malloc(sizeof(*buf));
	if (buf == NULL) return NULL;
	buf->chunk_size = chunk_size;
	buf->max_size = buf->chunk_size;
	buf->data = malloc(buf->max_size);
	if (buf->data == NULL) goto fail_data;
	buf->head = buf->data;
	buf->tail = buf->head;
	buf->userptr = buf->head;
	buf->size = 0;

	return buf;
fail_data:
	free(buf);
	return NULL;
}

static inline
void
simple_buffer_free(struct simple_buffer *buf)
{
	assert(buf != NULL);
	if (buf->data)
		free(buf->data);
	free(buf);
}

static inline
unsigned int
simple_buffer_size(struct simple_buffer *buf)
{
	return buf->size; 
}

static inline
unsigned int
simple_buffer_size_from_userptr(struct simple_buffer *buf)
{
	return buf->userptr - buf->head;
}

static inline
char *
simple_buffer_get_data(struct simple_buffer *buf)
{
	return buf->data;
}

static inline
char *
simple_buffer_get_head(struct simple_buffer *buf)
{
	return buf->head;
}

static inline
char *
simple_buffer_get_tail(struct simple_buffer *buf)
{
	return buf->tail;
}

static inline
char *
simple_buffer_get_userptr(struct simple_buffer *buf)
{
	return buf->userptr;
}

static inline
int
simple_buffer_resize(struct simple_buffer * const buf, size_t newsize)
{
	if (newsize != buf->max_size) {
		size_t head_offset = buf->head - buf->data;
		size_t tail_offset = buf->tail - buf->data;
		size_t userptr_offset = buf->userptr - buf->data;
		buf->max_size = (newsize/buf->chunk_size+1) * buf->chunk_size;
		char *newbufdata = realloc(buf->data, buf->max_size);
		if (newbufdata == NULL)
			return errno;
		buf->data = newbufdata;
		buf->head = buf->data + head_offset;
		buf->tail = buf->data + tail_offset;
		buf->userptr = buf->data + userptr_offset;
	}
	return 0;
}

static inline
int
simple_buffer_resize_tail(struct simple_buffer *buf, size_t newsize)
{
	size_t tailsize = buf->data+buf->max_size-buf->tail;
	if (newsize != tailsize) {
		size_t data_to_tail = buf->tail-buf->data;
		int err;
		err = simple_buffer_resize(buf, data_to_tail+newsize);
		return err;
	}
	return 0;
}

static inline
int
simple_buffer_append(struct simple_buffer * const buf,
		const char * const data,
		const size_t data_len)
{
	assert(buf != NULL);
	if (buf->tail > buf->data + buf->max_size - data_len) {
		int err = simple_buffer_resize(buf, buf->max_size+data_len);
		if (err) return err;
	}
	memcpy(buf->tail, (char *) data, data_len);
	buf->tail += data_len;
	buf->size += data_len;

	return 0;
}

static inline
int
simple_buffer_pull(struct simple_buffer * const buf, uint32_t len)
{
	if (len > buf->size)
		len = buf->size;
	buf->head += len;
	buf->size -= len;
	return 0;
}

static const char endstring = '\0';

static inline
int
simple_buffer_terminate_string(struct simple_buffer * const buf)
{
	return simple_buffer_append(buf, &endstring, sizeof(endstring));
}

static inline
int
simple_buffer_append_string(struct simple_buffer * const buf,
		const char * const str)
{
	const size_t len = strlen((char *) str);
	
	int err = simple_buffer_append(buf, str, len);
	if (err) return err;
	err = simple_buffer_terminate_string(buf);
	return err;
}

static inline
uint32_t
simple_buffer_move_userptr(struct simple_buffer * const buf, uint32_t len)
{
	if (len > buf->size)
		len = buf->size;
	buf->userptr += len;
	return len;
}

static inline
uint32_t
simple_buffer_move_tail(struct simple_buffer *buf, uint32_t len)
{
	uint32_t tailsize = buf->data-buf->tail+buf->max_size;
	if (len > tailsize)
		len = tailsize;
	buf->tail += len;
	buf->size += len;
	return len;
}

static inline
int
simple_buffer_rewind(struct simple_buffer * const buf)
{
	buf->head = buf->data;
	buf->tail = buf->head;
	buf->userptr = buf->head;
	buf->size = 0;
	return 0;
}

static inline
int
simple_buffer_clear(struct simple_buffer * const buf)
{
	if (buf->max_size > buf->chunk_size) {
		buf->data = realloc(buf->data, buf->chunk_size);
		if (buf->data == NULL) return errno;
		buf->max_size = buf->chunk_size;
	}
	return simple_buffer_rewind(buf);
}


#endif

/* vim=ts=8:sw=8:noet
*/
