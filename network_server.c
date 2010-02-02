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
#include <signal.h>
#include <syslog.h> /* only for log levels constants */

#include <ev.h>

#include "list.h"
#include "buffer.h"
#include "network_socket.h"
#include "network_server.h"


static void server_callback_accept(struct ev_loop *, ev_io *, int);
static void server_callback_read(struct ev_loop *, ev_io *, int);

struct server *_server = NULL;

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

static struct peer_client *peer_client_new(struct server *);
static void peer_client_free(struct peer_client *);
static void server_add_client(struct server *, struct peer_client *);
static void server_del_client(struct server *, struct peer_client *);

int
server_stop(struct server *server, int err)
{
	ev_io_stop(EV_DEFAULT, &server->watcher);
	ev_unloop(EV_DEFAULT, EVUNLOOP_ALL);
	struct list_head *pos, *cur;
	list_for_each_safe(pos, cur, &server->clients) {
		struct peer_client *client;
		client = list_entry(cur, struct peer_client, list);
		peer_client_free(client);
	}
	socket_close(server->fd);
	server_free(server);
	exit(err);
}

void
server_stop_global(void)
{
	server_stop(_server, EXIT_SUCCESS);
}

void
server_stop_sighandler(int signum)
{
	server_stop_global();
}

static inline void signal_ignore(int signum) {};
int
server_init(struct server *server,
		struct server_callbacks *callbacks, server_flags_t flags)
{
	int err = socket_init(&server->fd);
	if (err) goto fail_socket;
	if (flags & SERVER_NONBLOCKING)
		err = socket_set_nonblocking(server->fd);
	if (err) goto fail_socket;

	if (callbacks->log == NULL)
		server->callbacks.log = server_log_null;
	else
		server->callbacks.log = callbacks->log;
	server->callbacks.accept = callbacks->accept;
	if (callbacks->do_request == NULL) {
		err = EINVAL;
		goto fail_missing_callback;
	}
	server->callbacks.do_request = callbacks->do_request;

	signal(SIGPIPE, signal_ignore);
	signal(SIGTERM, server_stop_sighandler);
	signal(SIGINT, server_stop_sighandler);
	_server = server;

	return err;

fail_missing_callback:
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

	struct server *server = client->server;
	server_del_client(server, client);

	peer_client_free(client);
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
	if (bufsz == 0) return ;
	ssize_t n = write(w->fd,
			buffer_get_data(client->buffer_write),
			buffer_get_size(client->buffer_write));
	if (n == -1) {
		LOG_SERVER(client->server, LOG_ERR,
			"cannot write to socket (%s:%d): %d",
			client->hostname, client->port, errno);
		/* Handle error. Might disconnect */
	}
	if (n == 0) {
		/* LOG partial write? */
	}
	buffer_pull(client->buffer_write, n);
	if (buffer_get_size(client->buffer_write) == 0) {
		buffer_clear(client->buffer_write);
		ev_io_stop(loop, &client->watcher_write);
	}

	return ;
}

/** Read data from socket and process _synchronously_.
 * As the socket is configured in non-blocking mode, a read may be interrupted.
 * The callback will resume it later. We need to track the state of the buffer
 * and data that was read from the socket.
 * The read loop appends data to `client->buffer_read`. Then it calls
 * client_callback_do_request(client) to process the request.
 * If the request was full, the callback fills `client->buffer_write`, resets
 * `client->buffer_read` and sets `client->done_read` to 1. Otherwise,
 * `client->done_read` is still 0 and the runtime iterates again in the read loop.
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
			if (errno == EAGAIN)
				break;
			LOG_SERVER(client->server, LOG_ERR,
				"cannot read socket (%s:%d): %d",
				client->hostname, client->port, errno);
			/* Handle error. Might disconnect. */
			goto disconnect;
		}
		if (n == 0) {
			LOG_SERVER(client->server, LOG_INFO,
				"remote connection closed (%s:%d)",
				client->hostname, client->port);
			goto disconnect;
		}
		buffer_append(client->buffer_read, buf, n);
		client->server->callbacks.do_request(loop, w,
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
int peer_client_set_addr(struct peer_client *client,
		struct sockaddr *addr, socklen_t socklen)
{
	char port[NI_MAXSERV]; /* size = 32 */
	int err = getnameinfo(addr, socklen,
			client->hostname, NI_MAXHOST, port, NI_MAXSERV,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (err == -1) {
		LOG_SERVER(client->server, LOG_ERR,
			"getnameinfo failed: %d", errno);
		return errno;
	}
	client->port = atoi(port);
	return 0;
}

static
struct peer_client *
peer_client_new(struct server *server)
{
	struct peer_client *client = malloc(sizeof(*client));
	if (client == NULL) {
		return NULL;
	}
	int err = 0;
	client->buffer_read = buffer_new();
	if (client->buffer_read == NULL) {
		err = errno;
		LOG_SERVER(server, LOG_ERR,
			"buffer_new error (%s:%d %s)",
			__FILE__, __LINE__, __func__);
		goto fail_buffer_read;
	}
	client->done_read = 0;
	client->buffer_write = buffer_new();
	if (client->buffer_write == NULL) {
		err = errno;
		LOG_SERVER(server, LOG_ERR,
			"buffer_new error (%s:%d %s)",
			__FILE__, __LINE__, __func__);
		goto fail_buffer_write;
	}
	client->done_write = 0;
	client->server = server;
	INIT_LIST_HEAD(&client->list);

	return client;
fail_buffer_write:
	buffer_free(client->buffer_read);
fail_buffer_read:
	free(client);
	errno = err;
	return NULL;
}

static
void
peer_client_free(struct peer_client *client)
{
	if (buffer_get_size(client->buffer_read)) {
		LOG_SERVER(client->server, LOG_WARNING,
			"remaining data in read buffer (%s:%d)",
			client->hostname, client->port);
	}
	buffer_free(client->buffer_read);
	if (buffer_get_size(client->buffer_write)) {
		LOG_SERVER(client->server, LOG_WARNING,
			"remaining unsent data in write buffer (%s:%d)",
			client->hostname, client->port);
	}
	buffer_free(client->buffer_write);
	free(client);
}

static
void
server_add_client(struct server *server, struct peer_client *client)
{
	list_add(&client->list, &server->clients);
	server->nr_clients++;
}

static
void
server_del_client(struct server *server, struct peer_client *client)
{
	list_del(&client->list);
	server->nr_clients--;
}

static
void
server_callback_accept(struct ev_loop *loop, ev_io *w, int revents)
{
	struct server *server = (struct server *) w;
	socklen_t socklen = sizeof(server->addr);
	int fd = accept(server->fd, (struct sockaddr *) &server->addr, &socklen);
	if (fd == -1) {
		LOG_SERVER(server, LOG_ERR, "accept failed: %d", errno);
		return ;
	}
	socket_set_nonblocking(fd);

	if (server->nr_clients == server->max_clients) {
		socket_close(fd);
		LOG_SERVER(server, LOG_ERR,
			"max clients (%u) reached", server->max_clients);
		return ;
	}

	struct peer_client *client = peer_client_new(server);
	if (client == NULL) {
		LOG_SERVER(server, LOG_ERR,
			"peer_client_new error (%s:%d %s): %d",
			__FILE__, __LINE__, __func__, errno);
		socket_close(fd);
	}
	peer_client_set_addr(client, (struct sockaddr*) &server->addr, socklen);
	server_add_client(server, client);

	LOG_SERVER(server, LOG_INFO,
		"connection from: %s:%d\n", client->hostname, client->port);
	ev_io_init(&client->watcher_read,
			server_callback_read, fd, EV_READ);
	ev_io_start(loop, &client->watcher_read);

	ev_io_init(&client->watcher_write,
			server_callback_write, fd, EV_WRITE);
	if (server->callbacks.accept)
		server->callbacks.accept(server, client, fd);
	return ;
}


/* vim: ts=8:sw=8:noet
*/
