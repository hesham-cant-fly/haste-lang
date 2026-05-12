#include "haste.h"
#include <ctype.h>

struct span span_to_trimed(struct span span)
{
	struct span result = span;

	if (result.len == 0) {
		return result;
	}

	while (result.len > 0 and isspace(result.start[result.len - 1])) {
		result.len -= 1;
	}

	while (isspace(*result.start) and *result.start != '\0' and result.len != 0) {
		result.start += 1;
		result.len -= 1;
	}

	return result;
}
