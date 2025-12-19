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

float powf(const float x, const float y)
{
	if (x == 0.0f) {
		if (y > 0.0f)
			return 0.0f;
		return 0.0f / 0.0f;
	}

	if (x < 0.0f) {
		int yi = (int)y;
		if ((float)yi != y) {
			return 0.0f / 0.0f;
		}

		float r = 1.0f;
		float b = x;
		int n	= yi;

		if (n < 0) {
			b = 1.0f / b;
			n = -n;
		}

		while (n) {
			if (n & 1)
				r *= b;
			b *= b;
			n >>= 1;
		}
		return r;
	}

	return expf(y * logf(x));
}

float expf(const float x) {
	if (x > 88.0f)	return 3.4028235e38f;
	if (x < -88.0f) return 0.0f;

	const float inv_ln2 = 1.4426950408889634f;
	float fx = x * inv_ln2;

	int32_t i = (int32_t)(fx + (fx >= 0 ? 0.5f : -0.5f));
	float f = fx - i;

	float p = 1.0f
		+ f * 0.69314718f
		+ f * f * 0.24022651f
		+ f * f * f * 0.05550411f;

	union {
		uint32_t i;
		float f;
	} u;

	u.i = (uint32_t)((i + 127) << 23);
	return u.f * p;
}

float logf(const float x) {
	if (x <= 0.0f) {
		return 0.0f / 0.0f;
	}

	union {
		float f;
		uint32_t i;
	} u = { x };

	int32_t e = ((u.i >> 23) & 0xff) - 127;

	u.i = (u.i & 0x007fffff) | 0x3f800000;
	float m = u.f;

	float y = m - 1.0f;

	float log_m = y
		- y * y * 0.5f
		+ y * y * y * 0.3333333f
		- y * y * y * y * 0.25f;

	return log_m + e * 0.69314718f;
}
