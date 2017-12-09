#ifndef ICMP_SOCKET_H
#define ICMP_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>

typedef void ping4_sock_t;

ping4_sock_t *ping4_socket(int flags);

int ping4_connect(ping4_sock_t *, struct sockaddr_in*);

ssize_t ping4_recv(ping4_sock_t*, void *buf, size_t, int flags);

ssize_t ping4_send(ping4_sock_t*, const void *buf, size_t, int flags);

#endif

