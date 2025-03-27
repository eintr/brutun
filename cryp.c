#include <stddef.h>
#include <assert.h>

#include "cryp.h"

#define SALT	0x1027e6f8905c76a3ULL;

void enc(uint8_t *buf, size_t bufsize, uint8_t magic[SZ_MAGIC])
{
	assert(SZ_MAGIC == 8);
	int i, len64;
	uint64_t *buf64 = (void *)buf;
	uint64_t *magic64 = (void *)magic;

	len64 = bufsize / SZ_MAGIC;

	for (i = 0; i < len64; ++i) {
		buf64[i] = buf64[i] ^ *magic64 ^ SALT;
	}
}

void dec(uint8_t *buf, size_t bufsize, uint8_t magic[SZ_MAGIC])
{
	enc(buf, bufsize, magic);
}

#ifdef UT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void test_simple()
{
	uint8_t magic[SZ_MAGIC];
	uint8_t buf0[65536], *buf1;

	printf("\tTEST: %s\n", __FUNCTION__);
	for (int i=0; i<SZ_MAGIC; ++i) {
		magic[i]=random();
	}
	for (int len=1; len<65536; ++len) {
		uint8_t *buf1 = malloc(len);
		printf("\t\tTEST: %d\r", len);
		fflush(stdout);
		for (int i=0; i<len; ++i) {
			buf0[i]=random();
			buf1[i]=buf0[i];
		}
		enc(buf1, len, magic);
		dec(buf1, len, magic);
		assert(memcmp(buf0, buf1, len)==0);
		free(buf1);
	}
}
int main()
{
	srandom(getpid());
	printf("TEST: %s\n", __FILE__);
	test_simple();
	printf("PASS\n");
	return 0;
}
#endif
