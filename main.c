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

#include "network_server.h"


int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int port = 12345;
	int backlog = 256;

	struct server *server = server_new(256);
	if (server == NULL)
		exit(errno);

	int err = server_init(server, SERVER_NONBLOCKING);
	if (err) goto fail_server_init;
	server_listen(server, host, port, backlog);

fail_server_init:
	server_free(server);
	exit(err);
}

/* vim: ts=8:sw=8:noet
*/
