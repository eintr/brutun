#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <resolv.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "ping4_socket.h"

struct ping4_sock_st {
	int sd;
	unsigned int cnt;
	struct sockaddr_in peer, local;
};

struct packet_st {
	struct icmphdr hdr;
	char payload[0];
};

static uint16_t tcpip_csum(const void *b, int len)
{
	const uint16_t *buf = b;
	uint32_t sum=0;
	uint16_t result;

	for ( sum = 0; len > 1; len -= 2 ) {
		sum += *buf++;
	}
	if ( len == 1 ) {
		sum += *(uint8_t*)buf;
	}
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

ping4_sock_t *ping4_socket(int flags)
{
	struct ping4_sock_st *r;

	r = malloc(sizeof(*r));
	r->sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (r->sd<0) {
		perror("socket()");
	}
	r->cnt = 0;
	return r;
}

int ping4_connect(ping4_sock_t *self, struct sockaddr_in *peer)
{
	struct ping4_sock_st *s=self;
	memcpy(&s->peer, peer, sizeof(struct sockaddr_in));
	return 0;
}

ssize_t ping4_recv(ping4_sock_t *self, void *buf, size_t bufsize, int flags)
{
	struct ping4_sock_st *p=self;
	struct sockaddr_in from;
	ssize_t len, paylen;
	struct iphdr *ip;
	struct icmphdr *icmp;
	char tmpbuf[65536], *payload;

	socklen_t fromlen = sizeof(from);
	while (1) {
		len = recvfrom(p->sd, tmpbuf, 65536, flags, (void*)&from, &fromlen);
		fprintf(stderr, "recvfrom() => %d\n", (int)len);
		ip = (void*)tmpbuf;
		icmp = (void*)(tmpbuf + ip->ihl*4);
		payload = (void*)(tmpbuf + ip->ihl*4 + 8);
		paylen = len - ip->ihl*4 - 8;
		if (p->peer.sin_addr.s_addr==0 || memcmp(&from, &p->peer, sizeof(struct sockaddr_in))==0) {
			if (icmp->type == ICMP_ECHO) {
				memcpy(buf, payload, paylen);
				return paylen;
			} else {
				fprintf(stderr, "Ignored unknown unkown code\n");
			}
		} else {
			fprintf(stderr, "Ignored unknown source\n");
		}
	}
}

ssize_t ping4_send(ping4_sock_t *self, const void *data, size_t len, int flags)
{
	struct ping4_sock_st *p=self;
	struct packet_st* buf;
	int buflen;

	buflen = sizeof(struct packet_st) + len;

	buf = alloca(buflen);
	memset(buf, 0, buflen); 

	buf->hdr.type = ICMP_ECHO;
	buf->hdr.un.echo.id = getpid();
	buf->hdr.un.echo.sequence = p->cnt++;
	memcpy(buf->payload, data, len);
	buf->hdr.checksum = tcpip_csum(buf, buflen);

	return sendto(p->sd, buf, buflen, flags, (void*)&p->peer, sizeof(struct sockaddr_in)); 
}

#ifdef UT

#include <assert.h>

static void dump(const void *p, int len)
{   
	int i;
	for (i=0; i<len; ++i) {
		printf("%.2x, ", ((unsigned char*)p)[i]);
	}
	printf("\n");
}

	int
main()
{   
	ping4_sock_t *c;
	struct sockaddr_in dst;

	c = ping4_socket(0);

	memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	inet_pton(AF_INET, "10.33.106.206", &dst.sin_addr);

	ping4_connect(c, &dst);

	while (1) {
		struct timeval out, in;
		ssize_t r;
		gettimeofday(&out, NULL);

		r = ping4_send(c, &out, sizeof(out), 0)==sizeof(out);
		printf("Sent %d bytes\n", r);

		r = ping4_recv(c, &out, sizeof(out), 0)==sizeof(out);
		printf("Recv %d bytes\n", r);

		gettimeofday(&in, NULL);

		fprintf(stderr, "rtt=%ld\n", (in.tv_sec - out.tv_sec)*1000+(in.tv_usec - out.tv_usec)/1000);
		sleep(1);
	}
}

#endif

