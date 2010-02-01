/*
 *       Filename:  network_server.h
 *
 *    Description:  
 *
 *        Version:  0.1
 *        Created:  01/29/2010 04:37:49 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Greg Leclercq (ggl), ggl@0x80.net 
 *
 */

#ifndef SIMPLE_NETWORK_SERVER_H_
#define SIMPLE_NETWORK_SERVER_H_ 1

#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#include <ev.h>

#include "list.h"


typedef enum {
	SERVER_NONBLOCKING = O_NONBLOCK
} server_flags_t;

struct peer_client {
        ev_io   watcher;
	struct server *server;
	struct list_head list;
        char    hostname[NI_MAXHOST]; /* NI_MAXHOST = 1025 */
        int     port;
};

struct server {
        ev_io   watcher;
        int     fd;
        struct list_head clients;
        uint32_t nr_clients;
	uint32_t max_clients;
        struct sockaddr_in  addr;
};


/** Allocate and initialize a new `struct server`.
 * Allocates server->clients and initialize every attribute.
 * @param max_clients: maximum number of clients. No fixed limit but the memory
 * available.
 * @return pointer to an allocated and initialized `struct server`. NULL if an
 * error happened.
 * @see server_free().
 */
struct server *server_new(uint32_t max_clients);

/** Free a `struct server`.
 * Free the memory allocated for server and for server->clients. Assert if
 * server is NULL.
 * @param server: pointer to an allocated `struct server`.
 * @see server_new().
 */
void server_free(struct server *server);

int server_init(struct server *, server_flags_t);
int server_listen(struct server *, const char *, int, int);

#endif

/* vim: ts=8:sw=8:noet
*/
