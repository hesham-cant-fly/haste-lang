#include "span.h"
#include "my_string_view.h"

string_view_t span_to_string_view(const span_t span) {
  return (string_view_t){
      .data = span.start,
      .len = span.len,
  };
}
