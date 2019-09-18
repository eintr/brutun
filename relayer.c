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

struct arg_relay_st {
	int tun;
	int *ports;
	int *sockets;
	int nr_sockets;
	int dup_level;
};

struct pkt_st {
	uint8_t magic[8];
	uint8_t code;
	uint64_t serial;
	uint16_t len;
	uint8_t data[];
}__attribute__((packed));

#define	htonu64(X)	ntohu64(X)

static uint8_t magic[8];
static pthread_t tid_udp2tun, tid_tun2udp;
static struct sockaddr_in6 peer_addr;
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
	struct sockaddr_in6 from_addr;
	socklen_t from_addr_len;
	int len, ret, i;
	union {
		char buffer[BUFSIZE];
		struct pkt_st pkt;
	} ubuf;
	uint64_t serial_prev = 0;
	uint32_t data_len;
	struct pollfd *pfd;
	int nr_pfd;

	nr_pfd = 0;
	pfd = NULL;

	from_addr_len = sizeof(from_addr);
	while(1) {
		if (nr_pfd<arg->nr_sockets) {
			pfd = realloc(pfd, sizeof(struct pollfd)*arg->nr_sockets);
		}
		for (i=0; i<arg->nr_sockets; ++i) {
			pfd[i].fd = arg->sockets[i];
			pfd[i].events = POLLIN;
		}

		while (poll(pfd, arg->nr_sockets, -1)<=0) {
			perror("poll()");
		}
		for (i=0; i<arg->nr_sockets; ++i) {
			if (pfd[i].revents&POLLIN) {

				len = recvfrom(arg->sockets[i], &ubuf, sizeof(ubuf), 0, (void*)&from_addr, &from_addr_len);
				if (len==0) {
					continue;
				}

				if (memcmp(ubuf.pkt.magic, magic, 8)!=0) {
					//fprintf(stderr, "Ignored unknown source packet\n");
					continue;
				}

				if (ubuf.pkt.code != CODE_DATA) {
					//fprintf(stderr, "Not support non-data packet\n");
					continue;
				}

				peer_addr.sin6_family = from_addr.sin6_family;
				memcpy(&peer_addr.sin6_addr, &from_addr.sin6_addr, sizeof(peer_addr.sin6_addr));
				peer_addr.sin6_port = from_addr.sin6_port;
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
		}
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
	int len, i, socket_id=0;
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
		ubuf.pkt.code = CODE_DATA;
		ubuf.pkt.len = htons(len);
		ubuf.pkt.serial = htonu64(serial);

		enc(ubuf.pkt.data, len, magic);

		for (i=0; i<arg->dup_level; ++i) {
			ret = sendto(arg->sockets[socket_id], ubuf.buffer, sizeof(ubuf.pkt)+len, 0, (void*)&peer_addr, peer_addr_len);
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			//fprintf(stderr, "sent(serial %llu)\n", serial);
			socket_id = (socket_id+1)%arg->nr_sockets;
		}
		serial++;
/*		if (hup_notified) {
			hup_notified = 0;
			socket_id = (socket_id+1)%arg->nr_sockets;
			fprintf(stderr, "Source port changed\n");
		} */
	}
	pthread_exit(NULL);
}

static int open_udp_socket(int port)
{
	int sd;
	struct sockaddr_in6 local_addr;

	sd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (sd<0) {
		perror("socket()");
		abort();
	}

	local_addr.sin6_family = PF_INET6;
	inet_pton(AF_INET6, "::", &local_addr.sin6_addr);
	local_addr.sin6_port = htons(port);
	if (bind(sd, (void*)&local_addr, sizeof(local_addr))<0) {
		fprintf(stderr, "bind(%d): %m", port);
		abort();
	}
	return sd;
}

static void open_udp_sockets(int **sdarr, int *sdarr_sz, cJSON *conf)
{
	cJSON *port_conf;

	port_conf = conf_get("LocalPort", NULL, conf);
	if (port_conf==NULL) {
		*sdarr_sz = 1;
		*sdarr = malloc(sizeof(int));
		*sdarr[0] = open_udp_socket(60001);
		fprintf(stderr, "Opened default UDP port: 60001\n");
	} else if (port_conf->type==cJSON_Number) {
		*sdarr_sz = 1;
		*sdarr = malloc(sizeof(int));
		*sdarr[0] = open_udp_socket(port_conf->valueint);
		fprintf(stderr, "Opened single UDP port: %d\n", port_conf->valueint);
	} else if (port_conf->type==cJSON_Array) {
		int i;
		*sdarr_sz = cJSON_GetArraySize(port_conf);
		*sdarr = malloc(sizeof(int)*(*sdarr_sz));
		for (i=0; i<cJSON_GetArraySize(port_conf); ++i) {
			cJSON *jport;
			jport = cJSON_GetArrayItem(port_conf, i);
			if (jport->type!=cJSON_Number) {
				fprintf(stderr, "Illegal LocalPort[%d]!\n", i);
				abort();
			}
			(*sdarr)[i] = open_udp_socket(jport->valueint);
			fprintf(stderr, "Opened UDP port: %d\n", jport->valueint);
		}
	} else if (port_conf->type==cJSON_Object) {
		int port_start, port_end, i, p;
		port_start = conf_get_int("Start", 60001, port_conf);
		port_end = conf_get_int("End", 60010, port_conf);
		if (port_start > port_end) {
			fprintf(stderr, "Illegal LocalPort range!\n");
			abort();
		}
		*sdarr_sz = port_end - port_start + 1;
		*sdarr = malloc(sizeof(int)*(*sdarr_sz));
		for (i=0,p=port_start; p<=port_end; ++p) {
			int sd;
			sd = open_udp_socket(p);
			if (sd>=0) {
				(*sdarr)[i++] = sd;
				fprintf(stderr, "Opened UDP port: %d\n", p);
			}
		}
		if (i==0) {
			fprintf(stderr, "No LocalPorts availlable!\n");
			abort();
		}
		*sdarr_sz = i;
	} else {
		fprintf(stderr, "Illegal LocalPort!\n");
		abort();
	}
}

void relay(int tunfd, cJSON *conf)
{
	struct arg_relay_st arg;
	int err, remote_port;
	char *remote_ip;
	const char* magic_word;

	magic_word = (void*)conf_get_str("MagicWord", "Brutun1", conf);
	memset(magic, 0, 8);
	strncpy((void*)magic, magic_word, 8);
	fprintf(stderr, "Magic=%s\n", magic);
	
	remote_ip = (void*)conf_get_str("RemoteAddress", NULL, conf);
	remote_port = conf_get_int("RemotePort", 60001, conf);
	if (remote_ip!=NULL) {
		peer_addr.sin6_family = PF_INET6;
		inet_pton(PF_INET6, remote_ip, &peer_addr.sin6_addr);
		peer_addr.sin6_port = htons(remote_port);
		peer_addr_len = sizeof(peer_addr);
		fprintf(stderr, "RemoteAddress =%s, RemotePort=%d\n", remote_ip, remote_port);
	} else {
		fprintf(stderr, "No RemoteAddress specified, running in passive mode.\n");
	}

	open_udp_sockets(&arg.sockets, &arg.nr_sockets, conf);

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

