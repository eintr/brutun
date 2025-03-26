#ifndef CARRIER_H
#define CARRIER_H

#include "cJSON.h"

#include "carrier_interface.h"

struct carrier {
	void *handler;
	void *context;
	carrier_interface_t *interface;
};

struct carrier *carrier_load(const cJSON *conf, void(*on_recv)(void *tun, void*, size_t), void *tun);
void carrier_unload(struct carrier *);

#endif
