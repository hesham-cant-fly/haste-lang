#include "my_math.h"
#include <assert.h>
#include <stdint.h>

/// Just a simple implementation
int64_t powi64(const int64_t a, const int64_t b)
{
	assert(b >= 0);

	int64_t result = 1;

	for (int64_t i=0; i<b; i += 1)
		result *= a;

	return result;
}
