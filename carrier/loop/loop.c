#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <util_cjson.h>

#include <carrier_interface.h>

/****************************/

struct context_st {
	void (*on_recv)(void *, void *, size_t);
	const void *tunnel;
};

static void *mod_init(const cJSON *conf, const void *tun)
{
	struct context_st *ctx;

	ctx = malloc(sizeof(*ctx));
	ctx->on_recv = NULL;
	ctx->tunnel = tun;

	return ctx;
}

static void mod_destroy(void *p)
{
	free(p);
}

static int mod_send_packet(void *p, const void *data, size_t len)
{
	struct context_st *ctx = p;
	void *buf;

	if (len > 65535) {
		fprintf(stderr, "packet size too big: %d, drop\n", (int)len);
		return -1;
	}
	buf = malloc(len);
	memcpy(buf, data, len);
	ctx->on_recv((void*)ctx->tunnel, buf, len);
	free(buf);
	return 0;
}

static int mod_on_packet_receive(void *p, void (*cb)(void *tun, void *, size_t))
{
	struct context_st *ctx = p;
	ctx->on_recv = cb;
	return 0;
}

carrier_interface_t carrier_interface = {
	.name = "loop",
	.init = mod_init,
	.send_packet = mod_send_packet,
	.on_packet_receive = mod_on_packet_receive,
	.destroy = mod_destroy,
};
