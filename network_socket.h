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

#ifndef _SIMPLENET_H_
#define _SIMPLENET_H_ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ev.h>

int socket_init(int *fd);
int socket_set_nonblocking(int fd);
int socket_listen(int fd, struct sockaddr_in *addr,
		const char *host, int port, int backlog);
int socket_close(int fd);

#endif

/* vim: ts=8:sw=8:noet
*/
