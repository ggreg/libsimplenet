/*
 *       Filename:  network_server.c
 *
 *    Description:  
 *
 *        Version:  0.1
 *        Created:  01/29/2010 04:37:37 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Greg Leclercq (ggl), ggl@0x80.net 
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

#include <ev.h>

#include "list.h"
#include "network_socket.h"
#include "network_server.h"


static void server_callback_accept(EV_P_ ev_io *w, int revents);
static void server_callback_read(EV_P_ ev_io *w, int revents);


/* Public API */

struct server *
server_new(uint32_t max_clients)
{
	struct server *server = malloc(sizeof(*server));
	if (server == NULL)
		return NULL;
	memset(&server->watcher, 0, sizeof(server->watcher));
	server->fd = -1;
	INIT_LIST_HEAD(&server->clients);
	server->nr_clients = 0;
	server->max_clients = max_clients;
	memset(&server->addr, 0, sizeof(server->addr));

	return server;
}

void
server_free(struct server *server)
{
	assert(server != NULL);
	free(server);
}

int
server_init(struct server *server, server_flags_t flags)
{
	int err = socket_init(&server->fd);
	if (err) goto fail_socket;
	if (flags & SERVER_NONBLOCKING)
		err = socket_set_nonblocking(server->fd);
	if (err) goto fail_socket;

	return err;

fail_socket:
	socket_close(server->fd);
	return err;
}

int
server_listen(struct server *server, const char *host, int port, int backlog)
{
	int err = socket_listen(server->fd, &server->addr, host, port, backlog);
	if (err)
		return err;

	struct ev_loop *loop = ev_default_loop(0);
	ev_io_init(&server->watcher, server_callback_accept, server->fd, EV_READ);
	ev_io_start(loop, &server->watcher);
	ev_loop(loop, 0);

	return err;
}


/* Private functions */

static
void
server_callback_disconnect(struct ev_loop *loop, ev_io *w, int revents)
{
	struct peer_client *client = (struct peer_client *) w;
	ev_io_stop(loop, w);
	socket_close(w->fd);

	struct server *server = client->server;
	if (list_is_singular(&server->clients))
		INIT_LIST_HEAD(&server->clients);
	else
		list_del(&server->clients, &client->list);
	server->nr_clients--;
	free(client);
}

static
void
server_callback_read(struct ev_loop *loop, ev_io *w, int revents)
{
	char buf[4096];

	ssize_t n = read(w->fd, buf, 64);
	if (n == -1) {
		/* LOG read error on client */
		/* Handle error. Might disconnect. */
		goto disconnect;
	}
	if (n == 0) {
		/* LOG connection closed by client */
		goto disconnect;
	}
	buf[n] = '\0';
	printf("%s\n", buf);

	return ;

disconnect:
	server_callback_disconnect(loop, w, revents);
}


static
void
server_callback_accept(EV_P_ ev_io *w, int revents)
{
	struct server *server = (struct server *) w;
	socklen_t socklen = sizeof(server->addr);
	int fd = accept(server->fd, (struct sockaddr *) &server->addr, &socklen);
	if (fd == -1) {
		/* LOG accept failed */	
		return ;
	}

	if (server->nr_clients == server->max_clients) {
		socket_close(fd);
		/* LOG max clients reached */
		return ;
	}
	struct peer_client *client = malloc(sizeof(*client));
	char port[NI_MAXSERV]; /* size = 32 */
	int err = getnameinfo((struct sockaddr *) &server->addr, socklen,
			client->hostname, NI_MAXHOST, port, NI_MAXSERV,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (err == -1) {
		/* LOG cannot getnameinfo() */
		return ;
	}
	client->port = atoi(port);
	client->server = server;
	INIT_LIST_HEAD(&client->list);
	list_add(&client->list, &server->clients);
	server->nr_clients++;

	/* LOG "connection from: %s:%d\n", client->hostname, client->port */
	ev_io_init(&client->watcher, server_callback_read, fd, EV_READ);
	ev_io_start(loop, &client->watcher);
}


/* vim: ts=8:sw=8:noet
*/
