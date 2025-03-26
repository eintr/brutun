#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include "util_cjson.h"
#include "tunnel.h"

static int shell(const char *fmt, ...)
{
	const size_t max_cmdlen = 128 * 1024;
	int ret;
	char *cmd;

	cmd = malloc(max_cmdlen);
	{
		va_list al;
		va_start(al, fmt);
		vsnprintf(cmd, max_cmdlen - 1, fmt, al);
		cmd[max_cmdlen - 1] = 0;
		va_end(al);
	}
	fprintf(stderr, "run: %s  ...  ", cmd);
	ret = system(cmd);
	if (ret == -1) {
		fprintf(stderr, "failed: %m.\n");
	} else {
		fprintf(stderr, "status=%d.\n", ret);
	}
	free(cmd);
	return ret;
}

static int tun_alloc(char *dev, int tap)
{
	struct ifreq ifr;
	int fd;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		return fd;
	}
	memset(&ifr, 0, sizeof(ifr));
	if (tap == 0) {
		ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	} else {
		ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	}
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}
	assert(ioctl(fd, TUNSETIFF, (void *)&ifr) == 0);
	strcpy(dev, ifr.ifr_name);
	return fd;
}

static void cb_recv(void *ptr, void *data, size_t len)
{
	struct tunnel_ctx *t = ptr;
	ssize_t ret;
	if (t->tun_fd >= 0) {
		dec(data, len, t->magic);
		ret = write(t->tun_fd, data, len);
		if (len != ret) {
			fprintf(stderr, "write(tun) incomplete: %m\n");
		}
	}
}

static void *thr_tun_reader(void *p)
{
	struct tunnel_ctx *ctx = p;
#define PKTSIZE 65536
	uint8_t *buffer;
	struct pollfd pfd;

	buffer = malloc(PKTSIZE);
	pfd.fd = ctx->tun_fd;
	pfd.events = POLLIN;
	while (ctx->flag_loop) {
		ssize_t len;
		pfd.revents = 0;
		poll(&pfd, 1, 100);
		if (pfd.revents & POLLIN) {
			len = read(ctx->tun_fd, buffer, PKTSIZE);
			if (len <= 0) {
				continue;
			}
			if (ctx->carrier != NULL) {
				dec(buffer, len, ctx->magic);
				ctx->carrier->interface->send_packet(ctx->carrier->context, buffer, len);
			}
		}
	}
	free(buffer);
	pthread_exit(NULL);
#undef PKTSIZE
}

struct tunnel_ctx *tunnel_new(const cJSON *conf)
{
	struct tunnel_ctx *ret;

	ret = malloc(sizeof(*ret));
	assert(ret != NULL);

	ret->tun_fd = -1;

	ret->carrier = carrier_load(cJSON_lookup_obj(conf, ".Carrier", NULL), cb_recv, ret);
	assert(ret->carrier != NULL);

	strncpy(ret->tun_name, cJSON_lookup_str(conf, ".IFName", ""), IFNAMSIZ);
	ret->tun_name[IFNAMSIZ - 1] = 0;

	{
		const char *mode;
		mode = cJSON_lookup_str(conf, ".Mode", "tun");
		if (strcmp(mode, "tun") == 0) {
			int fd;
			const cJSON *routes;
			const char *tun_local_addr, *tun_peer_addr, *default_route;

			fd = tun_alloc(ret->tun_name, 0);
			assert(fd >= 0);

			tun_local_addr = cJSON_lookup_str(conf, ".LocalAddr", NULL);
			tun_peer_addr = cJSON_lookup_str(conf, ".PeerAddr", NULL);
			if (tun_local_addr == NULL || tun_peer_addr == NULL) {
				fprintf(stderr, "Must define TunnelLocalAddr and TunnelPeerAddr in config file!\n");
				abort();
			}

			shell("ip addr add dev %s %s peer %s", ret->tun_name, tun_local_addr, tun_peer_addr);

			routes = cJSON_lookup_obj(conf, ".RoutePrefix", NULL);
			if (routes && routes->type == cJSON_Array) {
				for (int i = 0; i < cJSON_GetArraySize(routes); ++i) {
					const cJSON *entry;
					entry = cJSON_GetArrayItem(routes, i);
					if (entry->type == cJSON_String) {
						shell("ip route add %s dev %s via %s", entry->valuestring, ret->tun_name, tun_peer_addr);
					}
				}
			}

			default_route = cJSON_lookup_str(conf, ".DefaultOfTable", NULL);
			if (default_route != NULL) {
				shell("ip route add default dev %s table %s", ret->tun_name, default_route);
			}
			ret->tun_fd = fd;
		} else if (strcmp(mode, "tap") == 0) {
			int fd;
			const char *join_bridge;

			fd = tun_alloc(ret->tun_name, 1);
			assert(fd >= 0);

			join_bridge = cJSON_lookup_str(conf, ".JoinBridge", NULL);
			if (join_bridge != NULL) {
				shell("ip li set dev %s master %s", ret->tun_name, join_bridge);
			} else {
				const char *addr;
				addr = cJSON_lookup_str(conf, ".LocalAddr", NULL);
				if (addr != NULL) {
					shell("ip addr add dev %s %s", ret->tun_name, addr);
				}
			}
			ret->tun_fd = fd;
		} else {
			fprintf(stderr, "Tunnel mode %s not supported\n", mode);
			abort();
		}
	}

	shell("ip link set dev %s up", ret->tun_name);
	ret->flag_loop = 1;
	assert(pthread_create(&ret->tid_tun_reader, NULL, thr_tun_reader, ret) == 0);

	return ret;
}

void tunnel_delete(struct tunnel_ctx *self)
{
	int save_fd;

	self->flag_loop = 0;
	pthread_join(self->tid_tun_reader, NULL);

	save_fd = self->tun_fd;
	self->tun_fd = -1;
	close(save_fd);

	carrier_unload(self->carrier);
	free(self);
}
