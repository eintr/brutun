#ifndef CRYP_H
#define CRYP_H

#include <stdint.h>

void enc(uint8_t *buf, size_t bufsize, uint8_t *magic);
void dec(uint8_t *buf, size_t bufsize, uint8_t *magic);

#endif

