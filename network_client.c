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

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "network_buffer.h"
#include "network_socket.h"
#include "network_client.h"

typedef int (*networkclient_create_t)(void);
typedef int (*networkclient_connect_t)(struct network_client *, const void *);

struct socket_ops {
    networkclient_create_t	create;
    networkclient_connect_t	connect;
};

static int network_client_connect_unix(struct network_client *, const void *);
static int network_client_connect_tcp(struct network_client *, const void *);

static
const struct socket_ops socket_type_ops[] = {
[SOCKET_UNIX]	{socket_unix, network_client_connect_unix},
[SOCKET_TCP]	{socket_tcp, network_client_connect_tcp},
[SOCKET_INVALID]{NULL,NULL}
};

struct network_client *
network_client_new(socket_type_t type)
{
    if (type < SOCKET_UNIX && type >= SOCKET_INVALID) {
	errno = EAFNOSUPPORT;
	return NULL;
    }
    struct network_client *client = malloc(sizeof(*client));
    if (client == NULL) return NULL;
    client->type = type;
    networkclient_create_t socket_create = socket_type_ops[type].create;
    client->fd = socket_create();
    return client;
}



void
network_client_free(struct network_client *client)
{
    assert(client != NULL);
    free(client);
}



int
network_client_close(struct network_client *client)
{
    return socket_close(client->fd);
}



int
network_client_connect(struct network_client *client, void *conf)
{
    if (client->type < SOCKET_UNIX && client->type >= SOCKET_INVALID)
	return EINVAL;
    networkclient_connect_t socket_connect;
    socket_connect = socket_type_ops[client->type].connect;
    return socket_connect(client, conf);
}


static
int
network_client_connect_unix(struct network_client *client, const void *conf_)
{
    const struct socket_config_unix *conf = conf_;
    return socket_connect_unix(client->fd, conf->path);
}


static
int
network_client_connect_tcp(struct network_client *client, const void *conf_)
{
    const struct socket_config_tcp *conf = conf_;
    return socket_connect_tcp(client->fd, conf->ip, conf->port);
}



int
network_client_send(struct network_client *client,
	struct simple_buffer *data, unsigned int len)
{
    while (len) {
	ssize_t n = write(client->fd, simple_buffer_get_head(data), len);
	if (n == 0) return EAGAIN;
	if (n == -1) return errno;
	simple_buffer_pull(data, n);
	len -= n;
    }
    return 0;
}



int
network_client_recv(struct network_client *client,
	struct simple_buffer *data, unsigned int len)
{
    simple_buffer_resize_tail(data, len);
    while (len) {
	ssize_t n = read(client->fd, simple_buffer_get_tail(data), len);
	if (n == 0)return EAGAIN;
	if (n == -1) return errno;
	simple_buffer_move_tail(data, n);
	len -= n;
    }
    return 0;
}



/* vim: ts=8:sw=8:noet
*/
