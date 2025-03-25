#include <unistd.h>
#include <stdint.h>

#include "util_time.h"

time_t systimestamp_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

