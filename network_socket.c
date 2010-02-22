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
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include <errno.h>
#include <assert.h>

#include "network_socket.h"



/* Create and close */
inline
int
socket_unix(void)
{
        return socket(AF_UNIX, SOCK_STREAM, 0);
}



inline
int
socket_tcp(void)
{
        return socket(PF_INET, SOCK_STREAM, 0);
}



int
socket_init(int fd)
{
	int optval = 1;
	int err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			&optval, sizeof(optval));
	if (err == -1) return errno;
	return 0;
}



int
socket_close(int fd)
{
	int err = close(fd);
	if (err == -1) return errno;
	return 0;
}



/* Settings */
int
socket_set_nonblocking(int fd)
{
	int err = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (err == -1) return errno;
	return 0;
}



int
socket_set_tcpnodelay(int fd)
{
	int optval = 1;
	socklen_t optlen = sizeof(optval);
	int err = setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &optval, optlen);
	if (err == -1) return errno;
	return 0;
}



/* Server API */
int
socket_listen_tcp(int fd, struct sockaddr_in *addr,
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



/* client API */
inline
int
socket_connect_unix(const int fd, const char *path_unix)
{
        assert(fd != -1 && path_unix != NULL);
        struct sockaddr_un server_addr;
        const socklen_t addr_len = sizeof(server_addr);

        memset(&server_addr, 0, addr_len);
        server_addr.sun_family = AF_UNIX;
        strcpy(server_addr.sun_path, path_unix);

        return connect(fd, (const struct sockaddr *) &server_addr, addr_len);
}



inline
int
socket_connect_tcp(const int fd, const char *ip, const int port)
{
        assert(fd != -1 && ip != NULL && port > 0);
        struct sockaddr_in server_addr;
        const socklen_t addr_len = sizeof(server_addr);

        memset(&server_addr, 0, addr_len);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr);
        memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

        return connect(fd, (const struct sockaddr *) &server_addr, addr_len);
}



/* vim: ts=8:sw=8:noet
*/
