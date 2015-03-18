#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include "protocol.h"

#define	BUFSIZE	(65536+4096)

struct arg_relay_st {
	int socket, tun;
	struct sockaddr_in *peer_addr;
};

static void *thr_rcver(void *p)
{
	struct arg_relay_st *arg=p;
	char buffer[BUFSIZE];
	struct sockaddr_in from_addr;
	socklen_t from_addr_len;
	int len, ret;
	struct sockaddr_in *ptr;
	char ipv4str[16];

	from_addr_len = sizeof(from_addr);
	while(1) {
		len = recvfrom(arg->socket, buffer, BUFSIZE, 0, (void*)&from_addr, &from_addr_len);
		if (len==0) {
			continue;
		}
		if (arg->peer_addr==NULL) {
			ptr = malloc(sizeof(*ptr));
			ptr->sin_family = PF_INET;
			ptr->sin_addr.s_addr = from_addr.sin_addr.s_addr;
			ptr->sin_port = from_addr.sin_port;

			inet_ntop(PF_INET, &from_addr.sin_addr, ipv4str, 16);
			fprintf(stderr, "Passive side got the first pkt, assuming %s:%d as peer.\n", ipv4str, ntohs(from_addr.sin_port));
			arg->peer_addr = ptr;
		} else {
			if (	from_addr.sin_addr.s_addr != arg->peer_addr->sin_addr.s_addr
					||	from_addr.sin_port != arg->peer_addr->sin_port) {
				fprintf(stderr, "Unknown source packet, drop.\n");
				continue;
			}
		}
		fprintf(stderr, "socket: pop %d bytes.\n", len);
		while (1) {
			ret = write(arg->tun, buffer, len);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "write(tunfd): %m\n");
				goto quit;
			}
			if (ret==0) {
				goto quit;
			}
			break;
		}
		fprintf(stderr, "tunfd: relayed %d bytes.\n", ret);
	}
quit:
	pthread_exit(NULL);
}

static void *thr_snder(void *p)
{
	struct arg_relay_st *arg=p;
	char buffer[BUFSIZE];
	int len, ret;

	while(1) {
		len = read(arg->tun, buffer, BUFSIZE);
		if (len==0) {
			continue;
		}
		fprintf(stderr, "tunfd: pop %d bytes.\n", len);
		if (arg->peer_addr==NULL) {
			fprintf(stderr, "Warning: Passive side can't send packets before peer address is discovered, drop.\n");
			continue;
		}
		while (1) {
			ret = sendto(arg->socket, buffer, len, 0, (void*)arg->peer_addr, sizeof(*arg->peer_addr));
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m\n");
				exit(1);
			}
			break;
		}
		fprintf(stderr, "socket: relayed %d bytes.\n", ret);
	}
	pthread_exit(NULL);
}

void relay(int sd, int tunfd, struct sockaddr_in *peer_addr)
{
	struct arg_relay_st arg;
	pthread_t snder, rcver;
	int err;

	arg.socket = sd;
	arg.tun = tunfd;
	arg.peer_addr = peer_addr;

	err = pthread_create(&snder, NULL, thr_snder, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&rcver, NULL, thr_rcver, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	pthread_join(snder, NULL);
	pthread_join(rcver, NULL);
}

