#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <json_conf.h>

#include "protocol.h"
#include "myrand.h"
#include "util_time.h"

#define	MAGIC	0x45a7
#define	BUFSIZE	(65536+4096)
#define	DEFAULT_DUP_LEVEL	3

struct arg_relay_st {
	int tun, sockets[1];
	int nr_sockets;
	struct sockaddr_in *peer_addr;
	int dup_level;
};

struct pkt_st {
	uint64_t serial;
	uint16_t len;
	uint8_t data[];
};

#define	CHECK_DELAY	5000

#define	htonu64	ntohu64

static pthread_t tid_up, tid_down;

static uint64_t ntohu64(uint64_t input)
{
	union {
		uint32_t u32[2];
		uint64_t u64;
	} a, b;
	a.u64 = input;

	b.u32[0] = ntohl(a.u32[1]);
	b.u32[1] = ntohl(a.u32[0]);
	return b.u64;
}

static void *thr_down(void *p)
{
	struct arg_relay_st *arg=p;
	struct sockaddr_in from_addr;
	socklen_t from_addr_len;
	int len, ret;
	struct sockaddr_in *ptr;
	char ipv4str[16];
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	static uint64_t serial_prev;

	from_addr_len = sizeof(from_addr);
	while(1) {
		len = recvfrom(arg->sockets[0], &ubuf, sizeof(ubuf), 0, (void*)&from_addr, &from_addr_len);
		if (len==0) {
			continue;
		}
		if (arg->peer_addr==NULL) {
			ptr = malloc(sizeof(*ptr));
			ptr->sin_family = PF_INET;
			ptr->sin_addr.s_addr = from_addr.sin_addr.s_addr;
			ptr->sin_port = from_addr.sin_port;

			inet_ntop(PF_INET, &from_addr.sin_addr, ipv4str, 16);
			fprintf(stderr, "Passive side got a magic pkt, treat %s:%d as peer.\n", ipv4str, ntohs(from_addr.sin_port));
			arg->peer_addr = ptr;
			serial_prev = ntohu64(ubuf.pkt.serial);
			fprintf(stderr, "Initial serial = %llu\n", serial_prev);
		} else {
			if (	from_addr.sin_addr.s_addr != arg->peer_addr->sin_addr.s_addr
					||	from_addr.sin_port != arg->peer_addr->sin_port) {
				fprintf(stderr, "Drop unknown source packet\n");
				continue;
			} else if (ntohu64(ubuf.pkt.serial)==serial_prev) {
				// Drop redundent packets.
				fprintf(stderr, "Drop redundent packet %llu\n", ntohu64(ubuf.pkt.serial));
				continue;
			}
		}
		fprintf(stderr, "Accepted packet %llu\n", ntohu64(ubuf.pkt.serial));
		serial_prev = ntohu64(ubuf.pkt.serial);

		while (1) {
			ret = write(arg->tun, ubuf.pkt.data, ubuf.pkt.len);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				//fprintf(stderr, "write(tunfd): %m\n");
				goto quit;
			}
			if (ret==0) {
				goto quit;
			}
			break;
		}
		//fprintf(stderr, "tunfd: relayed %d bytes.\n", ret);
	}
quit:
	pthread_exit(NULL);
}

static void *thr_up(void *p)
{
	struct arg_relay_st *arg=p;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	int len, i;
	uint64_t serial = rand();

	fprintf(stderr, "thr_snder_pkt(): started.\n");

	while(1) {
		len = read(arg->tun, ubuf.pkt.data, sizeof(ubuf)-sizeof(struct pkt_st));
		if (len==0) {
			continue;
		}
		if (arg->peer_addr==NULL) {
			fprintf(stderr, "Warning: Passive side can't send packets before peer address is discovered, drop.\n");
			continue;
		}

		ubuf.pkt.len = htons(len);
		ubuf.pkt.serial = htonu64(serial);

		for (i=0; i<arg->dup_level; ++i) {
			int ret = sendto(arg->sockets[0], &ubuf, len+sizeof(struct pkt_st), 0, (void*)arg->peer_addr, sizeof(*arg->peer_addr));
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			fprintf(stderr, "sent(serial %llu)\n", serial);
		}
		serial++;
	}
	pthread_exit(NULL);
}

void relay(int sd, int tunfd, cJSON *conf)
{
	struct arg_relay_st arg;
	int err, remote_port;
	char *remote_ip;
	struct sockaddr_in *peer_addr;

	remote_ip = (void*)conf_get_str("RemoteAddress", NULL, conf);
	remote_port = conf_get_int("RemotePort", 60001, conf);
	if (remote_ip!=NULL) {
		peer_addr = malloc(sizeof(*peer_addr));

		peer_addr->sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &peer_addr->sin_addr);
		peer_addr->sin_port = htons(remote_port);
		printf("RemoteAddress =%s, RemotePort=%d\n", remote_ip, remote_port);
	} else {
		printf("No RemoteAddress specified, running in passive mode.\n");
		peer_addr = NULL;
	}

	arg.sockets[0] = sd;
	arg.tun = tunfd;
	arg.peer_addr = peer_addr;
	arg.dup_level = conf_get_int("DupLevel", DEFAULT_DUP_LEVEL, conf);

	err = pthread_create(&tid_up, NULL, thr_up, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&tid_down, NULL, thr_down, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	pthread_join(tid_up, NULL);
	pthread_join(tid_down, NULL);
}

