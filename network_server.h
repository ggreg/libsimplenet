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

#ifndef SIMPLE_NETWORK_SERVER_H_
#define SIMPLE_NETWORK_SERVER_H_ 1

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifndef __USE_MISC
# define __USE_MISC
#endif
#include <netdb.h>

#include <ev.h>

#include "network_list.h"
#include "network_buffer.h"
#include "network_socket.h"

#define LOG_SERVER(server, prio, fmt, args...) \
	(server)->callbacks.log(prio, fmt, ##args);

static inline void server_log_null(int prio, const char *fmt, ...) {};

typedef enum {
	SERVER_NONBLOCKING = O_NONBLOCK,
	SERVER_TCPNODELAY = TCP_NODELAY	
} server_flags_t;

struct peer_client {
	ev_io   watcher_read;
	ev_io	watcher_write;
	struct server *server;
	struct list_head list;
        char    hostname[NI_MAXHOST]; /* NI_MAXHOST = 1025 */
        int     port;
	struct simple_buffer	*buffer_read;
	struct simple_buffer	*buffer_write;
	int	done_read;
	int	done_write;
};

typedef void (*callback_log_t)(int priority, const char *fmt, ...);
typedef int (*callback_accept_t)(
		void *prv,
		struct peer_client *client,
		int fd);
typedef int (*callback_request_t)(
		void *prv,
		struct simple_buffer *bufwrite,
		struct simple_buffer *bufread,
		int *done);
typedef int (*callback_postlisten_t)(void *prv);
typedef int (*callback_stop_t)(void *prv);

struct server_callbacks {
	callback_log_t		log;
	callback_accept_t	accept;
	callback_request_t	do_request;
	callback_postlisten_t	postlisten;
	callback_stop_t		stop;
};

struct server {
        ev_io   watcher;
	socket_type_t type;
        int     fd;
        struct list_head clients;
        uint32_t nr_clients;
	uint32_t max_clients;
        void *addr;
	struct server_callbacks callbacks;
	void *prv;
};

/** Allocate and initialize a new server.
 * Allocate server->clients and initialize every attribute.
 * @param max_clients maximum number of clients. No fixed limit but the memory
 * available.
 * @return pointer to an allocated and initialized `struct server`. NULL if an
 * error happened.
 * @see server_free().
 */
struct server *server_new(socket_type_t type, uint32_t max_clients);

/** Free a server.
 * Free the memory allocated for server and for server->clients. Assert if
 * server is NULL.
 * @param server pointer to an allocated `struct server`.
 * @see server_new().
 */
void server_free(struct server *server);

/** Initialize the file descriptor of a server.
 * @param server pointer to the server to initialize.
 * @param flags flags to set (see definition of server_flags_t).
 * @return 0 on success, errno value on error.
 */
int server_init(struct server *server,
		struct server_callbacks *callbacks, void *prv, server_flags_t flags);

/** Listen of the file descriptor.
 * Start the main event loop and listen for incoming connections on the file
 * descriptor.
 * @param server pointer to the server that will listen.
 * @param host address to listen on.
 * @param port service to listen on.
 * @param backlog maximum number of requests to queue in socket backlog.
 * @return 0 on success, errno value on error.
 */
int server_listen(struct server *server, const void *conf);

/** Stop a server and clean internal structures.
 * @param server pointer to the server to stop.
 * @param err error code to return at exit.
 */
int server_stop(struct server *server, int err);


#endif

/* vim: ts=8:sw=8:noet
*/
