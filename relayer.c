#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <json_conf.h>

#include "protocol.h"
#include "util_time.h"
#include "cryp.h"
#include "ping4_socket.h"

extern int hup_notified;

#define	BUFSIZE	(65536+4096)
#define	DEFAULT_DUP_LEVEL	3

#define	CODE_DATA	0
#define	CODE_PING	1
#define	CODE_PONG	1

struct relayer_st {
	int fd_tun;
	ping4_sock_t *net_socket;
	int dup_level;

	int terminate;
	uint8_t magic[8];
	pthread_t tid_net2tun, tid_tun2net;
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;
};

struct pkt_st {
	uint8_t magic[8];
	uint8_t code;
	uint64_t serial;
	uint16_t len;
	uint8_t data[];
}__attribute__((packed));

#define	htonu64(X)	ntohu64(X)


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

static void *thr_net2tun(void *p)
{
	struct relayer_st *arg=p;
	struct sockaddr_in from_addr;
	socklen_t from_addr_len;
	int len, ret;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	uint64_t serial_prev = 0;
	uint32_t data_len;

	from_addr_len = sizeof(from_addr);
	while(!arg->terminate) {
		len = ping4_recv(arg->net_socket, &ubuf, sizeof(ubuf), 0);
		if (len==0) {
			continue;
		}

		if (memcmp(ubuf.pkt.magic, arg->magic, 8)!=0) {
			//fprintf(stderr, "Ignored unknown source packet\n");
			continue;
		}

		arg->peer_addr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
		arg->peer_addr.sin_port = from_addr.sin_port;
		arg->peer_addr_len = from_addr_len;

		data_len = ntohs(ubuf.pkt.len);

		if (ntohu64(ubuf.pkt.serial)==serial_prev) {
			// fprintf(stderr, "Drop redundent packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
			continue;
		}

		//fprintf(stderr, "Accepted packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
		serial_prev = ntohu64(ubuf.pkt.serial);

		dec(ubuf.pkt.data, data_len, arg->magic);

		while (1) {
			ret = write(arg->fd_tun, ubuf.pkt.data, data_len);
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

static void *thr_tun2net(void *p)
{
	struct relayer_st *arg=p;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	int len, i;
	uint64_t serial = rand();
	ssize_t ret;

	memcpy(ubuf.pkt.magic, arg->magic, 8);
	while(!arg->terminate) {
		len = read(arg->fd_tun, ubuf.pkt.data, sizeof(ubuf)-sizeof(struct pkt_st));
		if (len==0) {
			continue;
		}
		if (arg->peer_addr_len==0) {
			fprintf(stderr, "Warning: Passive side can't send packets before peer address is discovered, drop.\n");
			continue;
		}

		ubuf.pkt.len = htons(len);
		ubuf.pkt.serial = htonu64(serial);

		enc(ubuf.pkt.data, len, arg->magic);

		for (i=0; i<arg->dup_level; ++i) {
			ret = ping4_send(arg->net_socket, ubuf.buffer, sizeof(ubuf.pkt)+len, 0);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			fprintf(stderr, "sent(serial %llu)\n", (unsigned long long)serial);
		}
		serial++;
		if (hup_notified) {
			hup_notified = 0;
		}
	}
	pthread_exit(NULL);
}

void *relayer_start(int tunfd, cJSON *conf)
{
	struct relayer_st *ctx;
	int err;
	char *remote_ip;
	const char* magic_word;

	ctx = calloc(sizeof(*ctx), 1);

	magic_word = conf_get_str("MagicWord", "Brutun1", conf);
	strncpy((void*)ctx->magic, magic_word, 8);
	fprintf(stderr, "Magic=%s\n", ctx->magic);

	ctx->net_socket = ping4_socket(0);

	remote_ip = (void*)conf_get_str("RemoteAddress", NULL, conf);
	if (remote_ip!=NULL) {
		ctx->peer_addr.sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &ctx->peer_addr.sin_addr);
		ctx->peer_addr_len = sizeof(ctx->peer_addr);
		ping4_connect(ctx->net_socket, &ctx->peer_addr);
		fprintf(stderr, "RemoteAddress =%s\n", remote_ip);
	} else {
		fprintf(stderr, "No RemoteAddress specified, running in passive mode.\n");
	}

	ctx->fd_tun = tunfd;
	ctx->dup_level = conf_get_int("DupLevel", DEFAULT_DUP_LEVEL, conf);
	fprintf(stderr, "DupLevel=%d\n", ctx->dup_level);

	err = pthread_create(&ctx->tid_net2tun, NULL, thr_net2tun, ctx);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&ctx->tid_tun2net, NULL, thr_tun2net, ctx);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	return ctx;
}

void relayer_stop(void *p)
{
	struct relayer_st *ctx=p;
	pthread_join(ctx->tid_tun2net, NULL);
	pthread_join(ctx->tid_net2tun, NULL);
}

