#ifndef __SPAN_H
#define __SPAN_H

#include "my_string_view.h"
#include <stddef.h>

typedef struct Span {
  size_t line;
  size_t column;
  char *start;
  size_t len;
} span_t;

#define SPANArgs(spn) (int)(spn).len, (spn).start

size_t spanlen(span_t span);
string_view_t span_to_string_view(const span_t span);

#endif // !__SPAN_H
