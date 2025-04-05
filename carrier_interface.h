#ifndef CARRIER_INTERFACE_H
#define CARRIER_INTERFACE_H

typedef struct {
	const char *name;
	void *(*init)(const cJSON *, const void *tun);
	int (*send_packet)(void *ctx, const void *p, size_t len);
	int (*on_packet_receive)(void *ctx, void(*)(void *tun, void*, size_t));
	int (*on_fail)(void *ctx, void(*)(void *tun, const char *reason));
	void (*destroy)(void *ctx);
} carrier_interface_t;

#endif
