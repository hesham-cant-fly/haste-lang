#ifndef __CONVERTERS_H
#define __CONVERTERS_H

#include "error.h"

#include <stddef.h>
#include <stdint.h>

error strn_to_i64(const char *str, size_t n, int base, int64_t *out);
error strn_to_double(const char *str, size_t n, double *out);

#endif // !__CONVERTERS_H
