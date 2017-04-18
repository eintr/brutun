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
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <json_conf.h>

#include "protocol.h"
#include "util_time.h"

#define	BUFSIZE	(65536+4096)
#define	DEFAULT_DUP_LEVEL	3

struct arg_relay_st {
	int tun, sockets[1];
	int nr_sockets;
	int dup_level;
};

struct pkt_st {
	uint8_t magic[8];
	uint64_t serial;
	uint16_t len;
	uint8_t data[];
};

#define	htonu64(X)	ntohu64(X)

static char magic[8];
static pthread_t tid_udp2tun, tid_tun2udp;
static struct sockaddr_in peer_addr;
static socklen_t peer_addr_len = 0;

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

static void *thr_udp2tun(void *p)
{
	struct arg_relay_st *arg=p;
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
	while(1) {
		len = recvfrom(arg->sockets[0], &ubuf, sizeof(ubuf), 0, (void*)&from_addr, &from_addr_len);
		if (len==0) {
			continue;
		}

		if (memcmp(ubuf.pkt.magic, magic, 8)!=0) {
			//fprintf(stderr, "Ignored unknown source packet\n");
			continue;
		}

		memcpy(&peer_addr, &from_addr, sizeof(struct sockaddr_in));
		peer_addr_len = from_addr_len;

		data_len = ntohs(ubuf.pkt.len);

		if (ntohu64(ubuf.pkt.serial)==serial_prev) {
			// fprintf(stderr, "Drop redundent packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
			continue;
		}

		//fprintf(stderr, "Accepted packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
		serial_prev = ntohu64(ubuf.pkt.serial);

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

static void *thr_tun2udp(void *p)
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
	while(1) {
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

		for (i=0; i<arg->dup_level; ++i) {
			ret = sendto(arg->sockets[0], ubuf.buffer, sizeof(ubuf.pkt)+len, 0, (void*)&peer_addr, peer_addr_len);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			//fprintf(stderr, "sent(serial %llu)\n", serial);
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
	const char* magic_word;

	magic_word = (void*)conf_get_str("MagicWord", "Brutun1", conf);
	memset(magic, 0, 8);
	strncpy(magic, magic_word, 8);
	fprintf(stderr, "Magic=%s\n", magic);
	
	remote_ip = (void*)conf_get_str("RemoteAddress", NULL, conf);
	remote_port = conf_get_int("RemotePort", 60001, conf);
	if (remote_ip!=NULL) {
		peer_addr.sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &peer_addr.sin_addr);
		peer_addr.sin_port = htons(remote_port);
		peer_addr_len = sizeof(peer_addr);
		fprintf(stderr, "RemoteAddress =%s, RemotePort=%d\n", remote_ip, remote_port);
	} else {
		fprintf(stderr, "No RemoteAddress specified, running in passive mode.\n");
	}

	arg.sockets[0] = sd;
	arg.tun = tunfd;
	arg.dup_level = conf_get_int("DupLevel", DEFAULT_DUP_LEVEL, conf);
	fprintf(stderr, "DupLevel=%d\n", arg.dup_level);

	err = pthread_create(&tid_udp2tun, NULL, thr_udp2tun, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&tid_tun2udp, NULL, thr_tun2udp, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	pthread_join(tid_tun2udp, NULL);
	pthread_join(tid_udp2tun, NULL);
}

