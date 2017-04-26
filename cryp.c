#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "cryp.h"

#define SALT	0x1027e6f8905c76a3ULL;

void enc(uint8_t *buf, size_t bufsize, uint8_t *magic)
{
	int i, len64;
	uint64_t *buf64 = (void*)buf;
	uint64_t *magic64 = (void*)magic;

	len64 = bufsize/8;

	for (i=0; i<len64; ++i) {
		buf64[i] = buf64[i] ^ *magic64 ^ SALT;
	}
}

void dec(uint8_t *buf, size_t bufsize, uint8_t *magic)
{
	enc(buf, bufsize, magic);
}

