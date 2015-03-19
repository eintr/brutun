#ifndef RELAYER_H
#define RELAYER_H

#include "cJSON.h"

void relay(int sd, int tunfd, cJSON *conf);

#endif

