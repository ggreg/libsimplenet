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

#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>

#include "network_socket.h"
#include "network_server.h"


/** Simple echo callback.
 *
 */
int
client_callback_do_request(struct ev_loop *loop, ev_io *w,
		struct simple_buffer *bufwrite,
		struct simple_buffer *bufread,
		int *done)
{
	struct peer_client *client =
		container_of(w, struct peer_client, watcher_read);
	char *data = client->server->prv;
	simple_buffer_append(bufwrite, data, strlen(data));
	simple_buffer_append(bufwrite,
			simple_buffer_get_head(bufread),
			simple_buffer_size(bufread));
	simple_buffer_clear(bufread);
	*done = 1;
	return 0;
}

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
		unixcf.path = "/tmp/simpleserver.socket";
		unixcf.backlog = 2;
		socket_type = SOCKET_UNIX;
		conf = &unixcf;
	};

	struct socket_config_tcp tcpcf;
	if (strncmp("tcp", argv[1], 3) == 0) {
		if (argc < 4) {
			usage(argv);
			exit(EINVAL);
		}
		tcpcf.ip = "127.0.0.1";
		tcpcf.port = 12345;
		tcpcf.backlog = 2;
		conf = &tcpcf;
		socket_type = SOCKET_TCP;
	};

	struct server *server = server_new(socket_type, 2);
	if (server == NULL)
		exit(errno);

	struct server_callbacks callbacks = {
		.log = syslog,
		.accept = NULL,
		.do_request = client_callback_do_request
	};


	int err = server_init(server, &callbacks, "hello, ", SERVER_NONBLOCKING);
	if (err) goto fail_server_init;
	server_listen(server, conf);

fail_server_init:
	server_stop(server, err);
	exit(err);
}

/* vim: ts=8:sw=8:noet
*/
