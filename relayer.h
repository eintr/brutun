#ifndef RELAYER_H
#define RELAYER_H

#include "cJSON.h"

void *relayer_start(int tunfd, cJSON *L2conf);
void relayer_stop(void *);

#endif

