#ifndef __MY_STRING_H
#define __MY_STRING_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define MY_STRING_IMPL

#ifndef GROW_FACTOR
#define GROW_FACTOR 2
#endif

typedef struct String {
  char *data;
  size_t len;
  size_t cap;
} String;


#define string_ends_with(self, other)                                          \
  _Generic((other),                                                            \
      char: string_ends_with_char,                                             \
      char *: string_ends_with_cstr,                                           \
      String *: string_ends_with_string)(self, other)

#define string_starts_with(self, other)                                        \
  _Generic((other),                                                            \
      char: string_starts_with_char,                                           \
      char *: string_starts_with_cstr,                                         \
      String *: string_starts_with_string)(self, other)

#define string_push(self, target)                                              \
  _Generic((target),                                                           \
      char: string_push_char,                                                  \
      char *: string_push_cstr,                                                \
      String *: string_push_string)(self, target)

#define string_eq(self, target)                                                \
  _Generic((target),                                                           \
      char *: string_eq_cstr,                                                  \
      String *: string_eq_string)(self, target)

String string_from_chars(const char *restrict chs);
String string_from_chars_copy(const char *restrict chs);
String string_new(const size_t cap);
const char *string_as_cstr(const String *restrict self);
size_t string_set_len(String *self, size_t len);
String string_format(const char *restrict fmt, ...);
void string_reserve(String *restrict self, const size_t new_cap);
void string_resize(String *restrict self, const size_t new_size);
void string_delete(String *restrict self);
void string_push_char(String *restrict self, const char ch);
void string_push_cstr(String *restrict self, const char *restrict cstr);
void string_push_string(String *restrict self, const String *restrict other);
void string_pushf(String *restrict self, const char *restrict fmt, ...);
bool string_eq_cstr(const String *restrict self, const char *restrict other);
bool string_eq_string(const String *restrict self, const String *restrict other);
bool string_starts_with_char(const String *restrict self, const char other);
bool string_starts_with_cstr(const String *self, const char *other);
bool string_starts_with_string(const String *restrict self,
                               const String *restrict other);
bool string_ends_with_char(const String *restrict self, const char other);
bool string_ends_with_cstr(const String *restrict self,
                           const char *restrict other);
bool string_ends_with_string(const String *restrict self,
                             const String *restrict other);

#ifdef MY_STRING_IMPL
static inline size_t string_actual_cap(const String *restrict self) {
  return self->cap + 1;
}

static inline size_t string_actual_len(const String *restrict self) {
  return self->len + 1;
}

String string_from_chars(const char *restrict chs) {
  size_t len = strlen(chs);
  return (String){(char *)chs, len, len};
}

String string_from_chars_copy(const char *restrict chs) {
  size_t len = strlen(chs);
  char *buffer = malloc(len + 1);
  strcpy(buffer, chs);
  String res = {
      .cap = len,
      .len = len,
      .data = buffer,
  };
  return res;
}

String string_new(const size_t cap) {
  String res = {
      .cap = cap,
      .len = 0,
      .data = malloc(sizeof(char) * cap + 1),
  };
  res.data[0] = '\0';
  return res;
}

const char *string_as_cstr(const String *restrict self) {
  return self->data;
}

size_t string_set_len(String *self, size_t len) {
  self->len = len;
  self->data[len] = '\0';
  return self->len;
}

String string_format(const char *restrict fmt, ...) {
  va_list args;

  va_start(args, fmt);
  size_t len = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  String result = string_new(len);

  va_start(args, fmt);
  vsnprintf(result.data, len + 1, fmt, args);
  va_end(args);

  result.len = len;
  return result;
}

void string_reserve(String *restrict self, const size_t new_cap) {
  self->data = realloc(self->data, new_cap + 1);
  self->cap = new_cap;

  if (self->data == NULL) {
    printf("Cannot reserve new capacity.\n");
    exit(-1);
  }
}

void string_resize(String *restrict self, const size_t new_size) {
  if (self->cap < new_size) {
    self->data = realloc(self->data, new_size + 1);
  }

  if (self->len < new_size) {
    memset(self->data + self->len, 0, new_size - self->len);
  }

  self->len = new_size;
  self->data[self->len] = '\0';
}

void string_delete(String *restrict self) {
  self->len = 0;
  self->cap = 0;
  free(self->data);
}

void string_push_char(String *restrict self, const char ch) {
  if (self->len + 1 >= self->cap) {
    string_reserve(self, self->cap * GROW_FACTOR);
  }

  self->data[self->len++] = ch;
  self->data[self->len] = '\0';
}

void string_push_cstr(String *restrict self, const char *restrict cstr) {
  size_t len = strlen(cstr);
  if (self->len + len >= self->cap) {
    size_t newcap = self->cap * GROW_FACTOR;
    if (self->len + len > newcap)
      newcap = self->len + len;
    string_reserve(self, newcap);
  }

  memcpy(self->data + self->len, cstr, len);
  self->len += len;
  self->data[self->len] = '\0';
}

void string_push_string(String *restrict self, const String *restrict other) {
  if (self->len + other->len >= self->cap) {
    size_t newcap = self->cap * GROW_FACTOR;
    if (self->len + other->len > newcap)
      newcap = self->len + other->len;
    string_reserve(self, newcap);
  }

  memcpy(self->data + self->len, other->data, other->len);
  self->len += other->len;
  self->data[self->len] = '\0';
}

void string_pushf(String *restrict self, const char *restrict fmt, ...) {
#define va_restart(args, a) do { va_end(args); va_start(args, a); } while (0)
  va_list args;
  int x = 0;

  va_start(args, fmt);
  x = vsnprintf(NULL, 0, fmt, args);

  if (self->len + x > self->cap) {
    size_t new_cap = self->cap * GROW_FACTOR;
    if (new_cap < self->len + x) new_cap = self->len + x;
    string_reserve(self, new_cap);
  }

  va_restart(args, fmt);
  vsnprintf(self->data + self->len, x + 1, fmt, args);
  va_end(args);

  self->len += x;
#undef va_restart
}

bool string_eq_cstr(const String *restrict self, const char *restrict other) {
  size_t len = strlen(other);
  if (self->len != len)
    return false;

  return strncmp(self->data, other, len) == 0;
}

bool string_eq_string(const String *restrict self, const String *restrict other) {
  if (self->len != other->len)
    return false;

  return strncmp(self->data, other->data, self->len) == 0;
}

bool string_starts_with_char(const String *restrict self, const char other) {
  if (self->len != 1)
    return false;

  return self->data[0] == other;
}

bool string_starts_with_cstr(const String *self, const char *other) {
  size_t len = strlen(other);
  if (self->len < len)
    return false;

  return strncmp(self->data, other, len) == 0;
}

bool string_starts_with_string(const String *restrict self,
                               const String *restrict other) {
  if (self->len < other->len)
    return false;

  return strncmp(self->data, other->data, self->len);
}

bool string_ends_with_char(const String *restrict self, const char other) {
  if (self->len < 1)
    return false;

  return self->data[self->len - 1] == other;
}

bool string_ends_with_cstr(const String *restrict self,
                           const char *restrict other) {
  size_t len = strlen(other);
  if (self->len < len)
    return false;

  return strncmp(self->data + self->len - len, other, len) == 0;
}

bool string_ends_with_string(const String *restrict self,
                             const String *restrict other) {
  if (self->len < other->len)
    return false;

  return strncmp(self->data + self->len - other->len, other->data,
                 other->len) == 0;
}
#endif // MY_STRING_IMPL
#endif // __MY_STRING_H
