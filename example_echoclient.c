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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "network_buffer.h"
#include "network_socket.h"
#include "network_client.h"

void
usage(char **argv)
{
    fprintf(stderr, "%s tcp <ip> <port>\n", argv[0]);
    fprintf(stderr, "%s unix <path_to_socket>\n", argv[0]);
}

int
main(int argc, char **argv)
{
    if (argc < 3) {
	usage(argv);
	exit(EINVAL);
    }
    int socket_type;
    void *conf;

    struct socket_config_unix unixcf;
    if (strncmp("unix", argv[1], 4) == 0) {
	    unixcf.path = argv[2];
	    conf = &unixcf;
	    socket_type = SOCKET_UNIX;
    }

    struct socket_config_tcp tcpcf;
    if (strncmp("tcp", argv[1], 3) == 0) {
	if (argc < 4) {
		usage(argv);
		exit(EINVAL);
	}
	tcpcf.ip = argv[2];
	tcpcf.port = atoi(argv[3]);
	conf = &tcpcf;
	socket_type = SOCKET_TCP;
    };
    struct network_client *client = network_client_new(socket_type);
    if (client == NULL) {
	    fprintf(stderr,
		    "error in network_client_new(): %s\n",
		    strerror(errno));
	    exit(EXIT_FAILURE);
    }

    struct simple_buffer *bufwrite = simple_buffer_new(getpagesize());
    simple_buffer_append(bufwrite, "client", 6);
    struct simple_buffer *bufread = simple_buffer_new(getpagesize());
    int err = network_client_connect(client, conf);
    if (err) {
	    fprintf(stderr,
		    "error in network_client_connect(): %s\n",
		    strerror(err));
	    exit(err);
    }
    err = network_client_send(client, bufwrite, 6);
    if (err) {
	    fprintf(stderr,
		    "error cannot write to socket: %s\n",
		    strerror(err));
	    goto end;
    }
    err = network_client_recv(client, bufread, 13);
    if (err) {
	    fprintf(stderr,
		    "error cannot read from socket: %s\n",
		    strerror(err));
	    goto end;
    }
    write(1, simple_buffer_get_head(bufread), 13);
    write(1, "\n", 1);

end:
    network_client_free(client);

    simple_buffer_free(bufread);
    simple_buffer_free(bufwrite);
    exit(err);
}

/* vim: ts=8:sw=8:noet
*/
