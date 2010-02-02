/*
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  0.1
 *        Created:  09/29/2009 02:29:14 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Greg Leclercq (ggl), ggl@0x80.net 
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <syslog.h>

#include "network_server.h"

/** Simple echo callback.
 *
 */
void
client_callback_do_request(struct ev_loop *loop, ev_io *w,
		struct buffer *bufwrite,
		struct buffer *bufread,
		int *done)
{
	struct peer_client *client = (struct peer_client *) w;
	buffer_append(bufwrite,
			buffer_get_data(bufread), buffer_get_size(bufread));
	buffer_clear(bufread);
	*done = 1;
	ev_io_start(loop, &client->watcher_write);
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

	int err = server_init(server, &callbacks, SERVER_NONBLOCKING);
	if (err) goto fail_server_init;
	server_listen(server, host, port, backlog);

fail_server_init:
	server_stop(server, err);
}

/* vim: ts=8:sw=8:noet
*/
