#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  include <stdnoreturn.h>
#  define NORETURN noreturn
#elif defined(__GNUC__)
#  define NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#  define NORETURN __declspec(noreturn)
#else
#  define NORETURN /* nothing */
#endif

#define VERSION "0.0.1-alpha"
#define panic(fmt, ...)                                                        \
  _panic(__FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)
#define unreachable0()                                                         \
  do {                                                                         \
    _unreachable(__FILE__, __LINE__);                                          \
    exit(69);                                                                  \
  } while (0)
#define unused(expr) ((void)(expr))

NORETURN void _unreachable(const char *file, const size_t line);
NORETURN void _panic(const char *restrict file, const size_t line,
                     const char *restrict fmt, ...);
void to_lower_case(char *str);

#endif // !__COMMON_H
