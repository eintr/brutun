#include <stdlib.h>
#include <math.h>

int p_judge(unsigned int *seed, int mask, int num)
{
	int r;

	r = rand_r(seed);
	if (r%((int)(exp10((double)mask))) < num) {
		return 1;
	}
	return 0;
}


