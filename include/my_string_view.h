#ifndef __MY_STRING_VIEW_H
#define __MY_STRING_VIEW_H

#include <stddef.h>
#include <string.h>

#ifndef STRINGVIEWDEF
#  define STRINGVIEWDEF
#endif // STRINGVIEWDEF

typedef struct StringView {
  size_t len;
  const char *data;
} string_view_t;

#define SV_NULL make_sv(NULL, 0)

// string_view_t sv = ...;
// printf("%.*s", SVArgs(sv));
#define SVArgs(sv) (int) (sv).len, (sv).data

STRINGVIEWDEF string_view_t make_sv(const char *data, const size_t len);
STRINGVIEWDEF string_view_t sv_from_start_end(const char *start,
                                              const char *end);
STRINGVIEWDEF string_view_t sv_from_cstr(const char *data);
STRINGVIEWDEF bool sv_eq(string_view_t a, string_view_t b);
STRINGVIEWDEF bool sv_eq_ignorecase(string_view_t a, string_view_t b);

#ifdef STRING_VIEW_IMPLEMENTATION
STRINGVIEWDEF string_view_t make_sv(const char *data, const size_t len) {
  return (string_view_t){
      len,
      data,
  };
}

STRINGVIEWDEF string_view_t sv_from_start_end(const char *start,
                                              const char *end) {
  return (string_view_t){
      .data = start,
      .len = (size_t)(end - start),
  };
}

STRINGVIEWDEF string_view_t sv_from_cstr(const char *data) {
  return (string_view_t){
      .data = data,
      .len = strlen(data) - 1,
  };
}

STRINGVIEWDEF bool sv_eq(string_view_t a, string_view_t b) {
  if (a.len != b.len) return false;
  if (a.data == b.data) return true;
  return memcpy((void *)a.data, (void *)b.data, a.len) == 0;
}

STRINGVIEWDEF bool sv_eq_ignorecase(string_view_t a, string_view_t b) {
  if (a.len != b.len) return false;
  char x, y;
  for (size_t i = 0; i < a.len; ++i) {
    x = 'A' <= a.data[i] && a.data[i] <= 'Z' ? a.data[i] + 32 : a.data[i];
    y = 'A' <= b.data[i] && b.data[i] <= 'Z' ? b.data[i] + 32 : b.data[i];

    if (x != y) return false;
  }
  return true;
}

#endif // STRING_VIEW_IMPLEMENTATION
#endif // !__MY_STRING_VIEW_H
