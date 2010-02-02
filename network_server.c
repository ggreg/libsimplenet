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
#include "buffer.h"
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
	ev_io_stop(loop, &client->watcher_read);
	ev_io_stop(loop, &client->watcher_write);
	socket_close(w->fd);

	/* remove from clients list */
	struct server *server = client->server;
	list_del(&client->list);
	server->nr_clients--;

	/* peer_client_free */
	if (buffer_get_size(client->buffer_read)) {
		/* LOG remaining data in read buffer */
	}
	buffer_free(client->buffer_read);
	if (buffer_get_size(client->buffer_write)) {
		/* LOG remaining unsent data in write buffer */
	}
	buffer_free(client->buffer_write);
	free(client);
}

/** Write response stored in client->buffer_write.
 * The event loop triggers this callback once the file descriptor is available
 * for writing. As the socket is configured in non-blocking mode, the event
 * loop may interrupt the write. It would lead to a partial write that we must
 * handle.
 * Each time the callback calls write(), it writes n bytes. Then the buffer
 * pointer is increment of n bytes. client->done_write is 0 until the buffer
 * pointer reaches the end of the buffer data.
 */
static
void
server_callback_write(struct ev_loop *loop, ev_io *w, int revents)
{
	struct peer_client *client = (struct peer_client *) w;
	unsigned int bufsz = buffer_get_size(client->buffer_write);
	printf("bufsz = %d\n", bufsz);
	if (bufsz == 0) return ;
	ssize_t n = write(w->fd,
			buffer_get_data(client->buffer_write),
			buffer_get_size(client->buffer_write));
	if (n == -1) {
		/* LOG write error */
		/* Handle error. Might disconnect */
	}
	if (n == 0) {
		/* LOG error */
	}
	buffer_pull(client->buffer_write, n);
	if (buffer_get_size(client->buffer_write) == 0) {
		buffer_clear(client->buffer_write);
		ev_io_stop(loop, &client->watcher_write);
	}

	return ;
}

/** Simple echo callback.
 *
 */
static
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

/** Read data from socket and process _synchronously_.
 * As the socket is configured in non-blocking mode, a read may be interrupted.
 * The callback will resume it later. We need to track the state of the buffer
 * and data that was read from the socket.
 * The read loop appends data to `client->buffer_read`. Then it calls
 * client_callback_do_request(client) to process the request.
 * If the request was full, the callback fills `client->buffer_write`, resets
 * `client->buffer_read` and sets `client->done` to 1. Otherwise,
 * `client->done` is still 0 and the runtime iterates again in the read loop.
 * As the loop fills `client->buffer_read`, the client_callback_do_request()
 * may find messages boundaries and finally fills the response buffer.
 */
static
void
server_callback_read(struct ev_loop *loop, ev_io *w, int revents)
{
	const unsigned int bufsz = 1024;
	char buf[bufsz];
	struct peer_client *client = (struct peer_client *) w;
	for (;;) {
		ssize_t n = read(w->fd, buf, bufsz);
		if (n == -1) {
			/* LOG read error on client */
			/* Handle error. Might disconnect. */
			if (errno == EAGAIN)
				break;
			goto disconnect;
		}
		if (n == 0) {
			/* LOG connection closed by client */
			goto disconnect;
		}
		printf("[%s:%d] read %ld bytes\n",
			client->hostname, client->port, n);
		buffer_append(client->buffer_read, buf, n);
		client_callback_do_request(loop, w,
				client->buffer_write,
				client->buffer_read,
				&client->done_read);
		if (client->done_read)
			server_callback_write(loop, w, revents);
	}

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
	socket_set_nonblocking(fd);

	if (server->nr_clients == server->max_clients) {
		socket_close(fd);
		/* LOG max clients reached */
		return ;
	}

	/* peer_client_new */
	struct peer_client *client = malloc(sizeof(*client));
	if (client == NULL) {
		/* LOG malloc error */
		socket_close(fd);
		return ;
	}
	client->buffer_read = buffer_new();
	if (client->buffer_read == NULL) {
		/* LOG malloc error */
		goto fail_buffer_read;
	}
	client->done_read = 0;
	client->buffer_write = buffer_new();
	if (client->buffer_write == NULL) {
		/* LOG malloc error */
		goto fail_buffer_write;
	}
	client->done_write = 0;

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
	/* ! peer_client_new */

	/* server_add_client */
	list_add(&client->list, &server->clients);
	server->nr_clients++;

	/* LOG "connection from: %s:%d\n", client->hostname, client->port */
	ev_io_init(&client->watcher_read,
			server_callback_read, fd, EV_READ);
	ev_io_start(loop, &client->watcher_read);

	ev_io_init(&client->watcher_write,
			server_callback_write, fd, EV_WRITE);
	/* ! server_add_client */

	return ;
fail_buffer_write:
	buffer_free(client->buffer_read);
fail_buffer_read:
	free(client);
}


/* vim: ts=8:sw=8:noet
*/
