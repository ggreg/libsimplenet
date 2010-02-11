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
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <assert.h>

#include "network_socket.h"



int
socket_init(int *fd)
{
	*fd = socket(AF_INET, SOCK_STREAM, 0);
	if (*fd == -1)
		return errno;

	int optval = 1;
	int err = setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR,
			&optval, sizeof(optval));
	if (err == -1) {
		err = errno;
		goto fail_setsockopt;
	}

	return 0;
fail_setsockopt:
	close(*fd);
	return err;
}



int
socket_set_nonblocking(int fd)
{
	int err = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (err == -1)
		return errno;
	return 0;
}



int
socket_listen(int fd, struct sockaddr_in *addr,
		const char *host, int port, int backlog)
{
	const int addr_family = AF_INET;
	addr->sin_family = addr_family;
	int ok = inet_pton(addr_family, host, &addr->sin_addr.s_addr);
	if (ok == 0)
		return EINVAL;
	if (ok == -1)
		return errno;
	addr->sin_port = htons(port);
	int err = bind(fd, (const struct sockaddr *) addr,
			sizeof(*addr));
	if (err == -1)
		return errno;
	err = listen(fd, backlog);
	if (err == -1)
		return errno;

	return 0;
}



int
socket_close(int fd)
{
	int err = close(fd);
	if (err == -1)
		return errno;
	return 0;
}

/* vim: ts=8:sw=8:noet
*/
