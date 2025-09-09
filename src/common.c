#include "common.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

_Noreturn void _unreachable(const char *file, const size_t line) {
  fprintf(stderr, "%s:%zu: ERROR: Reached unreachable code.\n", file, line);
  exit(20);
}

_Noreturn void _panic(const char *restrict file, const size_t line,
                      const char *restrict fmt, ...) {
  fprintf(stderr, "%s:%zu: Error: ", file, line);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(20);
}

void to_lower_case(char *str) {
  for (; str[0] != '\0'; ++str) {
    if (str[0] >= 'A' && str[0] <= 'Z') {
      str[0] += 32;
    }
  }
}
