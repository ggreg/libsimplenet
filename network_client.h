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

#ifndef _NETwORK_CLIENT_H_
#define _NETwORK_CLIENT_H_ 1

#include "network_socket.h"


struct network_client {
    int	fd;
    socket_type_t type;
};


struct network_client *network_client_new(socket_type_t type);
void network_client_free(struct network_client *client);
int network_client_close(struct network_client *client);
int network_client_connect(struct network_client *client, void *conf);
int network_client_send(struct network_client *client,
	struct simple_buffer *data, unsigned int len);
int network_client_recv(struct network_client *client,
	struct simple_buffer *data, unsigned int len);

#endif

/* vim: ts=8:sw=8:noet
*/
