#include "mylib/string.hpp"
#include "mylib/allocator.hpp"
#include "mylib/option.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>

auto utf8_char_len(const unsigned char byte) -> std::size_t {
  if (byte <= 0x7F)          return 1; // ASCII
  if ((byte & 0xE0) == 0xC0) return 2; // 2-byte
  if ((byte & 0xF0) == 0xE0) return 3; // 3-byte
  if ((byte & 0xF8) == 0xF0) return 4; // 4-byte
  return 0;                            // Invalid
}

auto decode_utf8(const unsigned char *str, uint32_t &out)
    -> Option<std::size_t> {
  unsigned char byte = str[0];

  if (byte < 0x80) {
    out = byte;
    return 1;
  }

  std::size_t expected_len = utf8_char_len(byte);
  if (expected_len == 0) return null; // invalid leading byte

  switch (expected_len) {
    case 2:
      if ((str[1] & 0xC0) != 0x80) return null;
      out = ((str[0] & 0x1F) << 6) |
            (str[1] & 0x3F);
      if (out < 0x80) return null; // overlong encoding
      return 2;

    case 3:
      if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80)
        return null;
      out = ((str[0] & 0x0F) << 12) |
            ((str[1] & 0x3F) << 6) |
            (str[2] & 0x3F);
      if (out < 0x800) return null;                // overlong
      if (out >= 0xD800 && out <= 0xDFFF) return null; // surrogate range
      return 3;

    case 4:
      if ((str[1] & 0xC0) != 0x80 ||
          (str[2] & 0xC0) != 0x80 ||
          (str[3] & 0xC0) != 0x80)
        return null;
      out = ((str[0] & 0x07) << 18) |
            ((str[1] & 0x3F) << 12) |
            ((str[2] & 0x3F) << 6) |
            (str[3] & 0x3F);
      if (out < 0x10000) return null;   // overlong
      if (out > 0x10FFFF) return null; // beyond Unicode max
      return 4;
  }

  return null;
}

string::Iterator::Iterator(unsigned char *const ptr, std::size_t len)
    : items(ptr), len(len) {
}

auto string::Iterator::done() const -> bool {
  return current >= len;
}

auto string::Iterator::peek() const -> std::uint32_t {
  return current_codepoint;
}

auto string::Iterator::prev() const -> std::uint32_t {
  return previous_codepoint;
}

auto string::Iterator::next() -> Option<std::uint32_t> {
  if (items == nullptr) return null;
  if (done()) return null;
  previous_codepoint = current_codepoint;

  auto codepoint_len = decode_utf8(items + current, current_codepoint);
  if (!codepoint_len) return null;
  current += codepoint_len.unwrap();
  return previous_codepoint;
}

string::string(const char *src, Allocator &alloc) : allocator(alloc) {
  push(src);
}

string::string(const unsigned char *src, Allocator &alloc) : allocator(alloc) {
  push(src);
}

string::string(std::initializer_list<unsigned char> init, Allocator &alloc)
    : allocator(alloc) {
  push(init);
}

auto string::deinit() -> void {
  allocator.destroy(items);
  items = nullptr;
  len = 0;
  cap = 0;
}

auto string::push(unsigned char ch) -> void {
  if (len + 1 >= cap)
    reserve(cap == 0 ? 1 : cap * 2);
  items[len++] = ch;
  items[len] = '\0';
}

auto string::push(const unsigned char *str) -> void {
  std::size_t str_len = std::strlen((char *)str);
  if (len + str_len >= cap) {
    std::size_t newcap = cap * 2;
    if (len + str_len > newcap)
      newcap = len + str_len;
    reserve(newcap);
  }

  std::memcpy(items + len, str, str_len);
  len += str_len;
  items[len] = '\0';
}

auto string::push(const string &other) -> void{
  if (len + other.len >= cap) {
    std::size_t newcap = cap * 2;
    if (len + other.len > newcap)
      newcap = len + other.len;
    reserve(newcap);
  }

  std::memcpy(items + len, other.items, other.len);
  len += other.len;
  items[len] = '\0';
}

auto string::push(const std::initializer_list<unsigned char> list) -> void {
  for (const auto &ch : list) {
    push(ch);
  }
}

auto string::reserve(std::size_t new_cap) -> void {
  items = allocator.realloc(items, new_cap + 1);
  assert(items && "Buy more RAM lol.");
  cap = new_cap;
}

auto string::c_str() const -> const unsigned char * { return items; }

auto string::bytes() const -> Slice<unsigned char> {
  return Slice(items, len);
}

auto string::utf8_iter() const -> Option<string::Iterator> {
  if (!is_valid_utf8()) {
    return null;
  }
  return string::Iterator(items, len);
}

auto string::begin() -> Slice<unsigned char>::Iterator {
  return Slice(items, len).begin();
}

auto string::end() -> Slice<unsigned char>::Iterator {
  return Slice(items, len).end();
}

auto string::is_valid_utf8() const -> bool {
  if (items == nullptr) return false;

  std::size_t i = 0;
  while (i < len) {
    std::uint32_t codepoint = 0;
    Option<std::size_t> len = decode_utf8(items + i, codepoint);
    if (!len) return false;
    i += len.unwrap();
  }

  return true;
}

auto string::operator==(const unsigned char *other) const -> bool {
  std::size_t str_len = std::strlen((char *)other);
  if (len != str_len)
    return false;

  return std::strncmp((char *)items, (char *)other, len) == 0;
}

auto string::operator==(const string &other) const -> bool {
  if (len != other.len)
    return false;

  return std::strncmp((char *)items, (char *)other.items, len) == 0;
}

auto string::operator[](const std::size_t index) -> unsigned char & {
  return items[index];
}

auto string::operator[](const std::size_t index) const -> const unsigned char & {
  return items[index];
}
