#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <util_cjson.h>

#include <cryp.h>
#include <carrier_interface.h>
#include <util_time.h>

#include "protocol.h"

#define	BUFSIZE	(65536+4096)
#define	DEFAULT_DUP_LEVEL	2

#define	CODE_DATA	0
#define	CODE_PING	1
#define	CODE_PONG	2

struct pkt_st {
	struct icmphdr hdr;
	uint8_t magic[8];
	uint16_t len;
	uint8_t data[];
} __attribute__((packed));

union pkt_buf {
	char buffer[BUFSIZE];
	struct pkt_st pkt;
};

#define	htonu64(X)	ntohu64(X)

static void *thr_rcv(void *);

static unsigned short in_cksum(const unsigned short *addr, register int len, unsigned short csum)
{
	register int nleft = len;
	const unsigned short *w = addr;
	register unsigned short answer;
	register int sum = csum;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += (*(unsigned char *)w);	/* le16toh() may be unavailable on old systems */

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);	/* add carry */
	answer = ~sum;		/* truncate to 16 bits */
	return (answer);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
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
#else
static uint64_t ntohu64(uint64_t input)
{
	return input;
}
#endif

static int open_socket(void)
{
	int sd;
	struct sockaddr_in local_addr;

	sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sd < 0) {
		perror("socket()");
		abort();
	}

	local_addr.sin_family = PF_INET;
	inet_pton(AF_INET, "0.0.0.0", &local_addr.sin_addr);
	local_addr.sin_port = 0;
	if (bind(sd, (void *)&local_addr, sizeof(local_addr)) < 0) {
		abort();
	}
	return sd;
}

/****************************/

struct context_st {
	uint8_t magic[8];
	pthread_t tid_rcv;
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len;
	int loop;

	void (*on_recv)(const void *, size_t);

	int *ports;
	int socket;
	int dup_level;
};

static void *mod_init(const cJSON *conf)
{
	struct context_st *ctx;
	const char *magic;
	int err, remote_port;
	const char *remote_ip;

	//fprintf(stderr, "%J\n", conf);

	ctx = malloc(sizeof(*ctx));
	magic = strdup(cJSON_lookup_str(conf, ".MagicWord", "Brutun2"));
	memset(ctx->magic, 0, 8);
	strncpy((void *)ctx->magic, magic, 8);
	fprintf(stderr, "Magic=%s\n", magic);

	remote_ip = cJSON_lookup_str(conf, ".RemoteAddress", NULL);
	if (remote_ip != NULL) {
		ctx->peer_addr.sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &ctx->peer_addr.sin_addr);
		ctx->peer_addr.sin_port = htons(remote_port);
		ctx->peer_addr_len = sizeof(ctx->peer_addr);
		fprintf(stderr, "RemoteAddress =%s, RemotePort=%d\n", remote_ip, remote_port);
	} else {
		ctx->peer_addr_len = 0;
		fprintf(stderr, "No RemoteAddress specified, running in passive mode.\n");
	}

	ctx->socket = open_socket();

	ctx->dup_level = cJSON_lookup_int(conf, ".DupLevel", DEFAULT_DUP_LEVEL);
	ctx->loop = 1;
	fprintf(stderr, "DupLevel=%d\n", ctx->dup_level);

	err = pthread_create(&ctx->tid_rcv, NULL, thr_rcv, ctx);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}
	return ctx;
}

static void mod_destroy(void *p)
{
	struct context_st *ctx = p;
	ctx->loop = 0;
	pthread_join(ctx->tid_rcv, NULL);
	free(ctx);
}

static int mod_send_packet(void *p, const void *data, size_t len)
{
	struct context_st *ctx = p;
	static int socket_id = 0;
	static uint64_t serial = 0;
	int i;
	union pkt_buf ubuf;

	if (len > 65535) {
		fprintf(stderr, "packet size too big: %d, drop\n", (int)len);
		return -1;
	}

	if (ctx->peer_addr_len == 0) {
		fprintf(stderr, "Peer address not discovered, drop\n");
		return -1;
	}

	*(uint64_t *) (&ubuf.pkt.magic) = *(uint64_t *) (ctx->magic);
	ubuf.pkt.hdr.type = ICMP_ECHO;
	ubuf.pkt.hdr.code = CODE_DATA;
	ubuf.pkt.hdr.checksum = 0;
	ubuf.pkt.hdr.un.echo.id = htonl((uint32_t) (serial >> 32));
	ubuf.pkt.hdr.un.echo.sequence = htonl((uint32_t) (serial & 0xffffffff));
	ubuf.pkt.len = htons(len);
	memcpy(ubuf.pkt.data, data, len);

	enc(ubuf.pkt.data, len, ctx->magic);

	ubuf.pkt.hdr.checksum = in_cksum((void *)ubuf.buffer, sizeof(ubuf.pkt) + len, 0);

	for (i = 0; i < ctx->dup_level; ++i) {
		ssize_t ret;
		//fprintf(stderr, "sendto(%d, buf, %d, ...)\n", ctx->sockets[socket_id], (int)(sizeof(ubuf.pkt)+len));
		ret = sendto(ctx->socket, ubuf.buffer, sizeof(ubuf.pkt) + len, 0, (void *)&ctx->peer_addr, ctx->peer_addr_len);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
		}
	}
	return 0;
}

static void *thr_rcv(void *p)
{
	struct context_st *ctx = p;
	struct sockaddr_in from_addr;
	socklen_t from_addr_len;

	int ret, i;
	struct {
		uint8_t __padding__[20];
		union pkt_buf ubuf;
	} ipbuf;
	uint64_t serial_prev = 0;
	uint32_t data_len;
	struct pollfd pfd;
	int nr_pfd;

	pfd.fd = ctx->socket;
	pfd.events = POLLIN;

	from_addr_len = sizeof(from_addr);
	while (ctx->loop) {
		if (poll(&pfd, 1, 500) <= 0) {
			continue;
		}
		uint64_t serial;
		ssize_t len;
		len = recvfrom(ctx->socket, &ipbuf, sizeof(ipbuf), 0, (void *)&from_addr, &from_addr_len);
		if (len == 0) {
			continue;
		}
		if (memcmp(ipbuf.ubuf.pkt.magic, ctx->magic, 8) != 0) {
			//fprintf(stderr, "Ignored unknown source packet\n");
			continue;
		}
		if (ipbuf.ubuf.pkt.hdr.code != CODE_DATA) {
			//fprintf(stderr, "Not support non-data packet yet\n");
			continue;
		}
		ctx->peer_addr.sin_family = from_addr.sin_family;
		memcpy(&ctx->peer_addr.sin_addr, &from_addr.sin_addr, sizeof(ctx->peer_addr.sin_addr));
		ctx->peer_addr.sin_port = from_addr.sin_port;
		ctx->peer_addr_len = from_addr_len;

		data_len = ntohs(ipbuf.ubuf.pkt.len);

		serial = (uint64_t) ntohl(ipbuf.ubuf.pkt.hdr.un.echo.sequence) + (((uint64_t) ntohl(ipbuf.ubuf.pkt.hdr.un.echo.id)) << 32);
		if (serial == serial_prev) {
			// fprintf(stderr, "Drop redundent packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
			continue;
		}
		//fprintf(stderr, "Accepted packet %llu\n", (long long unsigned)ntohu64(ubuf.pkt.serial));
		serial_prev = serial;

		dec(ipbuf.ubuf.pkt.data, data_len, ctx->magic);

		//fprintf(stderr, "tunfd: Got %d bytes.\n", data_len);
		ctx->on_recv(ipbuf.ubuf.pkt.data, data_len);
		//fprintf(stderr, "tunfd: relayed %d bytes.\n", ret);
	}
 quit:
	pthread_exit(NULL);
}

static int mod_on_packet_receive(void *p, void (*cb)(const void *, size_t))
{
	struct context_st *ctx = p;
	ctx->on_recv = cb;
	return 0;
}

carrier_interface_t carrier_interface = {
	.name = "simple_icmp4",
	.init = mod_init,
	.send_packet = mod_send_packet,
	.on_packet_receive = mod_on_packet_receive,
	.destroy = mod_destroy,
};
