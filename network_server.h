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

typedef enum {
	SERVER_NONBLOCKING = O_NONBLOCK
} server_flags_t;

struct peer_client {
        ev_io   watcher;
        char    hostname[NI_MAXHOST]; /* NI_MAXHOST = 1025 */
        int     port;
};

struct server {
        ev_io   watcher;
        int     fd;
        struct peer_client  *clients;
        uint32_t nr_clients;
        struct sockaddr_in  addr;
};

struct server *server_new(unsigned int max_clients);
void server_free(struct server *server);

int server_init(struct server *, server_flags_t);
int server_listen(struct server *, const char *, int, int);

#endif

/* vim: ts=8:sw=8:noet
*/
