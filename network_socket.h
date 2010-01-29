/*
 *       Filename:  network_socket.h
 *
 *    Description:  
 *
 *        Version:  0.1
 *        Created:  01/29/2010 11:41:08 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Greg Leclercq (ggl), ggl@0x80.net
 *
 */

#ifndef _SIMPLENET_H_
#define _SIMPLENET_H_ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ev.h>

int socket_init(int *fd);
int socket_set_nonblocking(int fd);
int socket_listen(int fd, struct sockaddr_in *addr,
		const char *host, int port, int backlog);
int socket_close(int fd);

#endif

/* vim: ts=8:sw=8:noet
*/
