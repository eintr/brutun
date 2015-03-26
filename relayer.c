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
#include "json_conf.h"
#include "myrand.h"
#include "util_time.h"
#include "ds_llist.h"

#define	BUFSIZE	(65536+4096)

struct arg_relay_st {
	int socket, tun;
	struct sockaddr_in *peer_addr;

	int tbf_cps, tbf_burst;

	int drop_shift, drop_num;

	int latency;
	llist_t *q_tbf, *q_delay, *q_drop, *q_send;
};

static int token = 0;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void *thr_tbf_keeper(void *p)
{
	struct arg_relay_st *arg=p;
	time_t now, time0;

	time0 = systimestamp_ms();
	while(1) {
		now = systimestamp_ms();
		pthread_mutex_lock(&mut);
		token += arg->tbf_cps*(now-time0)/1000;
		if (token > arg->tbf_burst) {
			token = arg->tbf_burst;
		}
		//fprintf(stderr, "%d ms passed, token += %d (= %d).\n", now-time0, arg->tbf_cps*(now-time0)/1000, token);
		time0 = now;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mut);
		usleep(10000);
	}
	pthread_exit(NULL);
}

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
		//fprintf(stderr, "socket: pop %d bytes.\n", len);
		while (1) {
			ret = write(arg->tun, buffer, len);
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

struct pkt_st {
	time_t arrive_time;
	int len;
	void *pkt;
};

static void *thr_snder_send(void *p)
{
	struct arg_relay_st *arg=p;
	struct pkt_st *buf;
	int ret;

	fprintf(stderr, "thr_snder_send(): started.\n");

	while (1) {
		llist_fetch_head(arg->q_send, &buf);
		while (1) {
			ret = sendto(arg->socket, buf->pkt, buf->len, 0, (void*)arg->peer_addr, sizeof(*arg->peer_addr));
			if (ret<0) {
				if (errno==EINTR) {
					continue;
				}
				fprintf(stderr, "sendto(sd): %m, drop\n");
			}
			break;
		}
		//fprintf(stderr, "thr_snder_send(): %d bytes sent to socket.\n", ret);
		free(buf->pkt);
		free(buf);
	}
	pthread_exit(NULL);
}

static void *thr_snder_drop(void *p)
{
	struct arg_relay_st *arg=p;
	struct pkt_st *buf;
	unsigned int seed;

	fprintf(stderr, "thr_snder_drop(): started.\n");

	seed = getpid();

	while(1) {
		llist_fetch_head(arg->q_drop, &buf);
		if (!p_judge(&seed, arg->drop_shift, arg->drop_num)) {
			llist_append(arg->q_send, buf);
		}
	}
	pthread_exit(NULL);
}

static void *thr_snder_delay(void *p)
{
	struct arg_relay_st *arg=p;
	struct pkt_st *buf;
	time_t now;

	fprintf(stderr, "thr_snder_delay(): started.\n");

	while (1) {
		llist_fetch_head(arg->q_delay, &buf);
		now = systimestamp_ms();
		//fprintf(stderr, "thr_snder_delay(): now=%lld, packet(%lld), delay=%lld\n", now, buf->arrive_time, arg->latency);
		while (buf->arrive_time + arg->latency > now) {
			//fprintf(stderr, "thr_snder_delay(): \tsleep %d ms.\n", (buf->arrive_time + arg->latency)-now);
			usleep(((buf->arrive_time + arg->latency)-now)*1000);
			now = systimestamp_ms();
		}
		llist_append(arg->q_drop, buf);
	}
	pthread_exit(NULL);
}

static void *thr_snder_tbf(void *p)
{
	struct arg_relay_st *arg=p;
	struct pkt_st *buf;

	fprintf(stderr, "thr_snder_tbf(): started.\n");

	while (1) {
		llist_fetch_head(arg->q_tbf, &buf);
		pthread_mutex_lock(&mut);
		while (token < buf->len) {
			pthread_cond_wait(&cond, &mut);
		}
		token -= buf->len;
		pthread_mutex_unlock(&mut);
		llist_append(arg->q_delay, buf);
		//fprintf(stderr, "thr_snder_tbf(): %d bytes passed.\n", buf->len);
	}
	pthread_exit(NULL);
}

static void *thr_snder_pkt(void *p)
{
	struct arg_relay_st *arg=p;
	struct pkt_st *buf;
	char buffer[BUFSIZE];
	int len;

	fprintf(stderr, "thr_snder_pkt(): started.\n");

	while(1) {
		len = read(arg->tun, buffer, BUFSIZE);
		if (len==0) {
			continue;
		}
		if (arg->peer_addr==NULL) {
			fprintf(stderr, "Warning: Passive side can't send packets before peer address is discovered, drop.\n");
			continue;
		}

		buf = malloc(sizeof(*buf));
		buf->arrive_time = systimestamp_ms();
		buf->len = len;
		buf->pkt = malloc(len);
		memcpy(buf->pkt, buffer, len);

		if (llist_append_nb(arg->q_tbf, buf)!=0) {
			fprintf(stderr, "llist_append_nb() failed, drop packet!\n");
			free(buf->pkt);
			free(buf);
		} else {
			//fprintf(stderr, "thr_snder_pkt(): %d bytes passed.\n", len);
		}
	}
	pthread_exit(NULL);
}

void relay(int sd, int tunfd, cJSON *conf)
{
	struct arg_relay_st arg;
	pthread_t rcver, snder_pkt, snder_tbf, snder_delay, snder_drop, snder_send, tbf_keeper;
	int err;
	char *remote_ip;
	double droprate;
	struct sockaddr_in *peer_addr;

	remote_ip = conf_get_str("RemoteAddress", conf);
	if (remote_ip!=NULL) {
		peer_addr = malloc(sizeof(*peer_addr));

		peer_addr->sin_family = PF_INET;
		inet_pton(PF_INET, remote_ip, &peer_addr->sin_addr);
		peer_addr->sin_port = htons(conf_get_int("RemotePort", conf));
	} else {
		printf("No RemoteAddress specified, running in passive mode.\n");
		peer_addr = NULL;
	}

	arg.socket = sd;
	arg.tun = tunfd;
	arg.peer_addr = peer_addr;
	arg.tbf_cps = conf_get_int("TBF_Bps", conf);
	arg.tbf_burst = conf_get_int("TBF_burst", conf);
	droprate = conf_get_double("DropRate", conf)*1000.0;
	arg.drop_shift = 3;
	arg.drop_num = (int)droprate;
	arg.latency = conf_get_int("Delay", conf);

	arg.q_tbf = llist_new(150000);	// 150000 = 15000(qps of 100M ethernet) * 10(seconds)
	if (arg.q_tbf==NULL) {
		fprintf(stderr, "Can't init queue.\n");
		exit(1);
	}

	arg.q_delay = llist_new(150000);	// 150000 = 15000(qps of 100M ethernet) * 10(seconds)
	if (arg.q_delay==NULL) {
		fprintf(stderr, "Can't init queue.\n");
		exit(1);
	}

	arg.q_drop = llist_new(150000);	// 150000 = 15000(qps of 100M ethernet) * 10(seconds)
	if (arg.q_drop==NULL) {
		fprintf(stderr, "Can't init queue.\n");
		exit(1);
	}

	arg.q_send = llist_new(150000); // 150000 = 15000(qps of 100M ethernet) * 10(seconds)
	if (arg.q_send==NULL) {
		fprintf(stderr, "Can't init queue.\n");
		exit(1);
	}

	err = pthread_create(&snder_send, NULL, thr_snder_send, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&snder_drop, NULL, thr_snder_drop, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&snder_delay, NULL, thr_snder_delay, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&snder_tbf, NULL, thr_snder_tbf, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&snder_pkt, NULL, thr_snder_pkt, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&tbf_keeper, NULL, thr_tbf_keeper, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	err = pthread_create(&rcver, NULL, thr_rcver, &arg);
	if (err) {
		fprintf(stderr, "pthread_create(): %s\n", strerror(err));
		exit(1);
	}

	pthread_join(snder_send, NULL);
}

