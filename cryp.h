#ifndef CRYP_H
#define CRYP_H

#include <stdint.h>

#define SZ_MAGIC	8

void enc(uint8_t *buf, size_t bufsize, uint8_t magic[SZ_MAGIC]);
void dec(uint8_t *buf, size_t bufsize, uint8_t magic[SZ_MAGIC]);

#endif

