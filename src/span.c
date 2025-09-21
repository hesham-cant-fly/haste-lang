#include "span.h"
#include "my_string_view.h"

size_t spanlen(span_t span) {
  return span.end - span.start;
}

string_view_t span_to_string_view(const span_t span, const char *src) {
  return (string_view_t){
      .data = src + span.start,
      .len = span.end - span.start,
  };
}
