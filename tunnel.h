#ifndef TUNNEL_H_INCLUDED
#define TUNNEL_H_INCLUDED

#include <stdint.h>
#include <pthread.h>
#include <net/if.h>

#include "util_cjson.h"
#include "cryp.h"

#include "carrier.h"

struct tunnel_statics {
	uint64_t cnt_pkts_sent, cnt_pkts_recv;
	uint64_t cnt_bytes_sent, cnt_bytes_recv;
	uint64_t timestamp_ms_start;
};

struct tunnel_ctx {
	char tun_name[IFNAMSIZ];
	struct tunnel_statics statics;
	struct carrier *carrier;
	pthread_t tid_tun_reader;
	int tun_fd;
	int flag_loop;
	uint8_t magic[SZ_MAGIC];
};

struct tunnel_ctx *tunnel_new(const cJSON *conf);
void tunnel_delete(struct tunnel_ctx *self);

cJSON *tunnel_statics(struct tunnel_ctx *self);

#endif
