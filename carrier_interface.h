#ifndef CARRIER_INTERFACE_H
#define CARRIER_INTERFACE_H

#include "cJSON.h"

typedef struct {
	const char *name;
	void *(*init)(const cJSON *);
	int (*send_packet)(void*, const void *p, size_t len);
	int (*on_packet_receive)(void*, void(*)(const void*, size_t));
	void (*destroy)(void *);
} carrier_interface_t;

extern int hup_notified;

#endif
