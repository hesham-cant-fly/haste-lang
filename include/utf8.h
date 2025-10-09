#ifndef __UTF8_ITER_H
#define __UTF8_ITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct UTF8Iter {
  const unsigned char *current;
  uint32_t curr_codepoint;
  uint32_t prev_codepoint;
} utf8_iter_t;

bool utf8_iter_init(utf8_iter_t *iter, const char *str);
int utf8_next(utf8_iter_t *iter);
uint32_t utf8_prev(utf8_iter_t iter);
uint32_t utf8_peek(utf8_iter_t iter);

int decode_utf8(const unsigned char *str, uint32_t *out);
bool is_valid_utf8_cstr(const unsigned char *str);
size_t get_utf8_char_length(const unsigned char first_byte);

#endif // !__UTF8_ITER_H
