#ifndef __SPAN_H
#define __SPAN_H

#include "my_string_view.h"
#include <stddef.h>

typedef struct Span {
  size_t line;
  size_t column;
  size_t start;
  size_t end;
} span_t;

string_view_t span_to_string_view(const span_t span, const char *src);

#endif // !__SPAN_H
