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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h> /* only for log levels constants */

#include <ev.h>

#include "network_list.h"
#include "network_buffer.h"
#include "network_socket.h"
#include "network_server.h"


static void server_callback_accept(struct ev_loop *, ev_io *, int);
static void server_callback_read(struct ev_loop *, ev_io *, int);

static void server_callback_read(struct ev_loop *, ev_io *, int);
static void server_callback_write(struct ev_loop *, ev_io *, int);
static void server_callback_disconnect(struct ev_loop *, ev_io *, int);

struct server *_server = NULL;

static int server_listen_unix(struct server *, const void *);
static int server_listen_tcp(struct server *, const void *);

typedef int (*networkserver_create_t)(void);
typedef int (*networkserver_listen_t)(struct server *, const void *);

struct socket_ops {
	networkserver_create_t	create;
	networkserver_listen_t	listen;
};

static
const struct socket_ops socket_type_ops[] = {
[SOCKET_UNIX]	{socket_unix, server_listen_unix},
[SOCKET_TCP]	{socket_tcp, server_listen_tcp},
[SOCKET_INVALID]{NULL,NULL}
};

/* Public API */

struct server *
server_new(socket_type_t type, uint32_t max_clients)
{
	if (type < SOCKET_UNIX && type >= SOCKET_INVALID) {
		errno = EAFNOSUPPORT;
		return NULL;
	}
	struct server *server = malloc(sizeof(*server));
	if (server == NULL) return NULL;
	server->type = type;
	memset(&server->watcher, 0, sizeof(server->watcher));
	server->fd = -1;
	INIT_LIST_HEAD(&server->clients);
	server->nr_clients = 0;
	server->max_clients = max_clients;
	server->addr = NULL;
	memset(&server->callbacks, 0, sizeof(server->callbacks));

	return server;
}

void
server_free(struct server *server)
{
	assert(server != NULL);
	if (server->addr)
		free(server->addr);
	free(server);
}

static struct peer_client *peer_client_new(struct server *);
static void peer_client_free(struct peer_client *);
static void server_add_client(struct server *, struct peer_client *);
static void server_del_client(struct server *, struct peer_client *);

int
server_stop(struct server *server, int err)
{
	struct list_head *pos, *cur;
	list_for_each_safe(pos, cur, &server->clients) {
		struct peer_client *client;
		client = list_entry(pos, struct peer_client, list);
		ev_io_stop(EV_DEFAULT, &client->watcher_read);
		ev_io_stop(EV_DEFAULT, &client->watcher_write);
		socket_close(client->watcher_read.fd);
		server_del_client(client->server, client);
		peer_client_free(client);
	}
	socket_close(server->fd);
	if (server->callbacks.stop)
		server->callbacks.stop(server->prv);
	server_free(server);
	ev_unloop(EV_DEFAULT, EVUNLOOP_ALL);
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
		struct server_callbacks *callbacks,
		void *prv,
		server_flags_t flags)
{
	if (server->type < SOCKET_UNIX && server->type >= SOCKET_INVALID)
		return EAFNOSUPPORT;
	networkserver_create_t _socket = socket_type_ops[server->type].create;

	int err;
	server->fd = _socket();
	if (server->fd == -1) {
		err = errno;
		goto fail_socket;
	}
	err = socket_init(server->fd);
	if (err) goto fail_socket;
	if (flags & SERVER_NONBLOCKING)
		err = socket_set_nonblocking(server->fd);
	if (err) goto fail_socket;
	if (flags & SERVER_TCPNODELAY)
		err = socket_set_tcpnodelay(server->fd);
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
	server->callbacks.postlisten = callbacks->postlisten;
	server->callbacks.stop = callbacks->stop;
	server->prv = prv;

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

static
int
server_listen_unix(struct server *server, const void *conf_)
{
	const struct socket_config_unix *conf = conf_;
	server->addr = malloc(sizeof(struct sockaddr_un));
	if (server->addr == NULL) return errno;
	memset(server->addr, 0, sizeof(struct sockaddr_un));
	return socket_listen_unix(server->fd,
			(struct sockaddr_un *) server->addr,
			conf->path, conf->backlog);
}

static
int
server_listen_tcp(struct server *server, const void *conf_)
{
	const struct socket_config_tcp *conf = conf_;
	server->addr = malloc(sizeof(struct sockaddr_in));
	if (server->addr == NULL) return errno;
	memset(server->addr, 0, sizeof(struct sockaddr_in));
	return socket_listen_tcp(server->fd,
			(struct sockaddr_in *) server->addr,
			conf->ip, conf->port, conf->backlog);
}

int
server_listen(struct server *server, const void *conf)
{
	if (server->type < SOCKET_UNIX && server->type >= SOCKET_INVALID)
		return EAFNOSUPPORT;
	networkserver_listen_t _listen = socket_type_ops[server->type].listen;
	int err = _listen(server, conf);
	if (err) return err;
	if (server->callbacks.postlisten)
		server->callbacks.postlisten(server->prv);

	struct ev_loop *loop = ev_default_loop(0);
	struct ev_io *watcher = &server->watcher;
	ev_io_init(watcher, server_callback_accept, server->fd, EV_READ);
	ev_io_start(loop, &server->watcher);
	ev_loop(loop, 0);

	return 0;
}


/* Private functions */

static
void
server_callback_disconnect(struct ev_loop *loop, ev_io *w, int revents)
{
	struct peer_client *client;
	client = container_of(w, struct peer_client, watcher_read);
	server_callback_write(loop, &client->watcher_write, revents);
	ev_io_stop(loop, &client->watcher_read);
	ev_io_stop(loop, &client->watcher_write);
	socket_close(w->fd);
	server_del_client(client->server, client);
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
	struct peer_client *client =
		container_of(w, struct peer_client, watcher_write);
	unsigned int bufsz =
		simple_buffer_size(client->buffer_write);
	if (bufsz == 0) return ;
	ssize_t n = write(w->fd,
			simple_buffer_get_head(client->buffer_write),
			bufsz);
	if (n == -1) {
		if (errno == EAGAIN)
			return ;
		LOG_SERVER(client->server, LOG_ERR,
			"cannot write to socket (%s:%d): %d",
			client->hostname, client->port, errno);
		/* Handle error. Might disconnect */
		simple_buffer_clear(client->buffer_write);
		ev_io_stop(loop, &client->watcher_write);
		return ;
	}
	if (n == 0) {
		return ;
		/* LOG partial write? */
	}
	simple_buffer_pull(client->buffer_write, n);
	if (simple_buffer_size(client->buffer_write) == 0) {
		simple_buffer_clear(client->buffer_write);
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
	struct peer_client *client =
		container_of(w, struct peer_client, watcher_read);
	const unsigned int bufsz = client->buffer_read->chunk_size;
	char buf[bufsz];
	for (;;) {
		ssize_t n = read(w->fd, buf, bufsz);
		if (n == -1) {
			if (errno == EAGAIN)
				break;
			LOG_SERVER(client->server, LOG_ERR,
				"cannot read socket (%s:%d): %d",
				client->hostname, client->port, errno);
			/* XXX: Handle error */
			goto disconnect;
		}
		if (n == 0) {
			LOG_SERVER(client->server, LOG_INFO,
				"remote connection closed (%s:%d)",
				client->hostname, client->port);
			goto disconnect;
		}
		simple_buffer_append(client->buffer_read, buf, n);
		for (;;) {
			int err = client->server->callbacks.do_request(
					client->server->prv,
					client->buffer_write,
					client->buffer_read,
					&client->done_read);
			if (client->done_read) {
				ev_io_start(loop, &client->watcher_write);
				client->done_read = 0;
			}
			if (err == ECONNABORTED)
				goto disconnect;
			if (err == EAGAIN)
				break;
			if (simple_buffer_size(client->buffer_read) == 0)
				break;
			if (err) {
				LOG_SERVER(client->server, LOG_ERR,
					"error: %s\n", strerror(err));
				break;
			}
		}
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
	client->buffer_read = simple_buffer_new(16*getpagesize());
	if (client->buffer_read == NULL) {
		err = errno;
		LOG_SERVER(server, LOG_ERR,
			"buffer_new error (%s:%d %s)",
			__FILE__, __LINE__, __func__);
		goto fail_buffer_read;
	}
	client->done_read = 0;
	client->buffer_write = simple_buffer_new(16*getpagesize());
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
	memset(client->hostname, 0, NI_MAXHOST);
	client->port = -1;

	return client;
fail_buffer_write:
	simple_buffer_free(client->buffer_read);
fail_buffer_read:
	free(client);
	errno = err;
	return NULL;
}

static
void
peer_client_free(struct peer_client *client)
{
	assert(client != NULL);
	if (client->buffer_read) {
		if (simple_buffer_size(client->buffer_read)) {
			LOG_SERVER(client->server, LOG_WARNING,
				"remaining data in read buffer (%s:%d)",
				client->hostname, client->port);
		}
		simple_buffer_free(client->buffer_read);
	}
	if (client->buffer_write) {
		if (simple_buffer_size(client->buffer_write)) {
			LOG_SERVER(client->server, LOG_WARNING,
				"remaining unsent data in write buffer (%s:%d)",
				client->hostname, client->port);
		}
		simple_buffer_free(client->buffer_write);
	}
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
	int fd = accept(server->fd, server->addr, &socklen);
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
	peer_client_set_addr(client, server->addr, socklen);
	server_add_client(server, client);

	LOG_SERVER(server, LOG_INFO,
		"connection from: %s:%d\n", client->hostname, client->port);
	struct ev_io *watcher_read = &client->watcher_read;
	ev_io_init(watcher_read, server_callback_read, fd, EV_READ);
	ev_io_start(loop, &client->watcher_read);

	/* Init-only watcher_write. It will be started when data are
	 * available in client->buffer_write.
	 */
	struct ev_io *watcher_write = &client->watcher_write;
	ev_io_init(watcher_write, server_callback_write, fd, EV_WRITE);
	if (server->callbacks.accept)
		server->callbacks.accept(server->prv, client, fd);
	return ;
}


/* vim: ts=8:sw=8:noet
*/
