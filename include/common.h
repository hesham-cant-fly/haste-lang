#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>

#define VERSION "0.0.1-alpha"
#define panic(fmt, ...) _panic(__FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define unreachable0() _unreachable(__FILE__, __LINE__)
#define unused(expr) ((void)(expr))

_Noreturn void _unreachable(const char *file, const size_t line);
_Noreturn void _panic(const char *restrict file, const size_t line,
                      const char *restrict fmt, ...);
void to_lower_case(char *str);

#endif // !__COMMON_H
