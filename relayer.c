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

extern int hup_notified;

#define	BUFSIZE	(65536+4096)
#define	DEFAULT_DUP_LEVEL	3

#define	CODE_DATA	0
#define	CODE_PING	1
#define	CODE_PONG	1

struct relayer_st {
	int fd_tun;
	int fd_socket;
	int dup_level;

	int terminate;
	uint8_t magic[8];
	pthread_t tid_net2tun, tid_tun2net;
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len = 0;
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

static void *thr_socket2tun(void *p)
{
	struct arg_relay_st *arg=p;
	struct sockaddr_in from_addr;
	socklen_t from_addr_len;
	int len, ret, i;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	uint64_t serial_prev = 0;
	uint32_t data_len;

	from_addr_len = sizeof(from_addr);
	while(!arg->terminate) {
		len = recvfrom(arg->socket, &ubuf, sizeof(ubuf), 0, (void*)&from_addr, &from_addr_len);
		if (len==0) {
			continue;
		}

		if (memcmp(ubuf.pkt.magic, magic, 8)!=0) {
			//fprintf(stderr, "Ignored unknown source packet\n");
			continue;
		}

		peer_addr.sin_addr.s_addr = from_addr.sin_addr.s_addr;
		peer_addr.sin_port = from_addr.sin_port;
		peer_addr_len = from_addr_len;

		data_len = ntohs(ubuf.pkt.len);

		if (ntohu64(ubuf.pkt.serial)==serial_prev) {
			// fprintf(stderr, "Drop redundent packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
			continue;
		}

		//fprintf(stderr, "Accepted packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
		serial_prev = ntohu64(ubuf.pkt.serial);

		dec(ubuf.pkt.data, data_len, magic);

		while (1) {
			ret = write(arg->tun, ubuf.pkt.data, data_len);
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

static void *thr_tun2socket(void *p)
{
	struct arg_relay_st *arg=p;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	int len, i;
	uint64_t serial = rand();
	ssize_t ret;

	memcpy(ubuf.pkt.magic, magic, 8);
	while(!arg->terminate) {
		len = read(arg->tun, ubuf.pkt.data, sizeof(ubuf)-sizeof(struct pkt_st));
		if (len==0) {
			continue;
		}
		if (peer_addr_len==0) {
			fprintf(stderr, "Warning: Passive side can't send packets before peer address is discovered, drop.\n");
			continue;
		}

		ubuf.pkt.len = htons(len);
		ubuf.pkt.serial = htonu64(serial);

		enc(ubuf.pkt.data, len, magic);

		for (i=0; i<arg->dup_level; ++i) {
			ret = sendto(arg->socket, ubuf.buffer, sizeof(ubuf.pkt)+len, 0, (void*)&peer_addr, peer_addr_len);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			fprintf(stderr, "sent(serial %llu)\n", serial);
		}
		serial++;
		if (hup_notified) {
			hup_notified = 0;
		}
	}
	pthread_exit(NULL);
}

static int open_icmp_socket(cJSON *L2conf)
{
	int sd;
	struct sockaddr_in local_addr;

	sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (*sd<0) {
		perror("socket()");
		abort();
	}
	return 0;
}

void *relay_start(int tunfd, cJSON *conf)
{
	struct relayer_st *ctx;
	int err;
	char *remote_ip;
	const char* magic_word;

	ctx = calloc(sizeof(*ctx), 1);

	magic_word = conf_get_str("MagicWord", "Brutun1", conf);
	strncpy((ctx->magic, magic_word, 8);
	fprintf(stderr, "Magic=%s\n", magic);

	remote_ip = (void*)conf_get_str("RemoteAddress", NULL, conf);
	if (remote_ip!=NULL) {
		ctx->peer_addr.sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &ctx->peer_addr.sin_addr);
		ctx->peer_addr_len = sizeof(ctx->peer_addr);
		fprintf(stderr, "RemoteAddress =%s, RemotePort=%d\n", remote_ip, remote_port);
	} else {
		fprintf(stderr, "No RemoteAddress specified, running in passive mode.\n");
	}

	ctx->fd_socket = open_icmp_socket(conf);

	ctx->fd_tun = tunfd;
	ctx->dup_level = conf_get_int("DupLevel", DEFAULT_DUP_LEVEL, conf);
	fprintf(stderr, "DupLevel=%d\n", ctx->dup_level);

	err = pthread_create(&ctx->tid_net2tun, NULL, thr_socket2tun, ctx);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&ctx->tid_tun2net, NULL, thr_tun2socket, ctx);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	return ctx;
}

void relay_stop(void *ctx)
{
	pthread_join(ctx->tid_tun2net, NULL);
	pthread_join(ctx->tid_net2tun, NULL);
}

