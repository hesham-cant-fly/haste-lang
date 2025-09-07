#include "utf8.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

bool utf8_iter_init(utf8_iter_t *iter, const char *str) {
  if (!is_valid_utf8_cstr((const unsigned char *)str)) {
    return false;
  }
  *iter = (utf8_iter_t) {
    .curr_codepoint = 0,
    .prev_codepoint = 0,
    .current = (const unsigned char *)str,
  };
  return utf8_next(iter) != -1;
}

int utf8_next(utf8_iter_t *iter) {
  iter->prev_codepoint = iter->curr_codepoint;
  assert(iter->current != NULL);
  if (iter->current[0] == '\0') {
    iter->current += 1;
    iter->curr_codepoint = 0;
    return 1;
  }
  
  unsigned char first_byte = iter->current[0];
  size_t expected_len = get_utf8_char_length(first_byte);
  if (expected_len > 0) {
    for (size_t i = 0; i < expected_len; i++) {
      if (iter->current[i] == '\0') {
        return -1;
      }
    }
  }
  
  int len = decode_utf8(iter->current, &iter->curr_codepoint);
  if (len == -1) return len;
  iter->current += len;
  return len;
}

uint32_t utf8_prev(utf8_iter_t iter) {
  return iter.prev_codepoint;
}

uint32_t utf8_peek(utf8_iter_t iter) {
  return iter.curr_codepoint;
}

size_t get_utf8_char_length(const unsigned char first_byte) {
  if (first_byte <= 0x7F) return 1;          // ASCII
  if ((first_byte & 0xE0) == 0xC0) return 2; // 2-byte
  if ((first_byte & 0xF0) == 0xE0) return 3; // 3-byte
  if ((first_byte & 0xF8) == 0xF0) return 4; // 4-byte
  return 0; // Invalid
}

int decode_utf8(const unsigned char *str, uint32_t *out) {
  if (str[0] < 0x80) {
    *out = str[0];
    return 1;
  } else if ((str[0] & 0xE0) == 0xC0) {
    if ((str[1] & 0xC0) != 0x80)
      return -1;
    *out = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
    if (*out < 0x80) return -1;
    return 2;
  } else if ((str[0] & 0xF0) == 0xE0) {
    if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80)
      return -1;
    *out = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
    if (*out < 0x800) return -1;
    if (*out >= 0xD800 && *out <= 0xDFFF) return -1;
    return 3;
  } else if ((str[0] & 0xF8) == 0xF0) {
    if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || (str[3] & 0xC0) != 0x80)
      return -1;
    *out = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) |
          (str[3] & 0x3F);
    if (*out < 0x10000) return -1;
    if (*out > 0x10FFFF) return -1;
    return 4;
  }
  return -1;
}

bool is_valid_utf8_cstr(const unsigned char *str) {
  if (str == NULL)
    return false;

  const unsigned char *ptr = str;

  while (*ptr != '\0') {
    if (ptr[0] >= 0xF0 && ptr[3] == '\0') return false; // 4-byte truncated
    if (ptr[0] >= 0xE0 && ptr[2] == '\0') return false; // 3-byte truncated
    if (ptr[0] >= 0xC0 && ptr[1] == '\0') return false; // 2-byte truncated

    int len = 0;
    uint32_t codepoint = 0;
    if ((len = decode_utf8(ptr, &codepoint)) == -1) return false;
    ptr += len;
  }

  return true;
}
