#ifndef CARRIER_INTERFACE_H
#define CARRIER_INTERFACE_H

typedef struct {
	const char *name;
	void *(*init)(const cJSON *);
	int (*send_packet)(void*, const void *p, size_t len);
	int (*on_packet_receive)(void*, void(*)(void *tun, void*, size_t), void *tun);
	void (*destroy)(void *);
} carrier_interface_t;

#endif
