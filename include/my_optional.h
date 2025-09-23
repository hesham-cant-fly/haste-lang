#ifndef __MY_OPTIONAL_H
#define __MY_OPTIONAL_H

/*
 * Usage:
 * typedef struct OptionalInt {
 *   int value;
 *   bool has_value;
 * } optional_int_t;
 *
 * optional_int_t i = none(optional_int_t);
 * if (is_some(i)) {
 *   int value = unwrap(i);
 * }
 */

#include <assert.h>
#include "common.h"

#define optional(_T) optional_##_T
#define none(_T) (_T){ .has_value = 0, }
#define some(_T, ...) (_T){ .value = (__VA_ARGS__), .has_value = true }
#define is_some(_vr) ((_vr).has_value)
#define is_none(_vr) (!((_vr).has_value))

#endif // !__MY_OPTIONAL_H
