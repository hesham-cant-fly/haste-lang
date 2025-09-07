#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define panic(fmt, ...) _panic(__FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define unreachable0() _unreachable(__FILE__, __LINE__)

_Noreturn void _unreachable(const char *file, const size_t line);
_Noreturn void _panic(const char *restrict file, const size_t line,
                      const char *restrict fmt, ...);

#endif // !__COMMON_H
