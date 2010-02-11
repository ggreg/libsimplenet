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
	struct peer_client *client = (struct peer_client *) w;
	char *data = client->server->prv;
	simple_buffer_append(bufwrite, data, strlen(data));
	simple_buffer_append(bufwrite,
			simple_buffer_get_data(bufread),
			simple_buffer_size(bufread));
	simple_buffer_clear(bufread);
	*done = 1;
	return 0;
}

int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int port = 12345;
	int backlog = 256;

	struct server *server = server_new(2);
	if (server == NULL)
		exit(errno);

	struct server_callbacks callbacks = {
		.log = syslog,
		.accept = NULL,
		.do_request = client_callback_do_request
	};

	server->prv = "hello, ";
	int err = server_init(server, &callbacks, SERVER_NONBLOCKING);
	if (err) goto fail_server_init;
	server_listen(server, host, port, backlog);

fail_server_init:
	server_stop(server, err);
	exit(err);
}

/* vim: ts=8:sw=8:noet
*/
