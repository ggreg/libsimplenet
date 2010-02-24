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
#include "network_client.h"

void
usage(char **argv)
{
    fprintf(stderr, "%s <ip> <port>\n", argv[0]);
}

int
main(int argc, char **argv)
{
    if (argc < 3) {
	usage(argv);
	exit(EINVAL);
    }
    struct socket_config_tcp conf = {
	.ip = argv[1],
	.port = atoi(argv[2])
    };
    struct network_client *client = network_client_new(SOCKET_TCP);
    if (client == NULL) exit(EXIT_FAILURE);

    struct simple_buffer *bufwrite = simple_buffer_new(getpagesize());
    simple_buffer_append(bufwrite, "client", 6);
    struct simple_buffer *bufread = simple_buffer_new(getpagesize());
    network_client_connect(client, &conf);
    int err = network_client_send(client, bufwrite, 6);
    err = network_client_recv(client, bufread, 13); 
    write(1, simple_buffer_get_head(bufread), 13);
    write(1, "\n", 1);
    network_client_free(client);

    simple_buffer_free(bufread);
    simple_buffer_free(bufwrite);
    exit(err);
}

/* vim: ts=8:sw=8:noet
*/
