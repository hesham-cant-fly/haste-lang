#ifndef __STRING_HPP
#define __STRING_HPP

#include "mylib/allocator.hpp"
#include "mylib/option.hpp"
#include "mylib/slice.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

auto utf8_char_len(const unsigned char byte) -> std::size_t;
auto decode_utf8(const unsigned char *str, uint32_t &out)
    -> Option<std::size_t>;

struct string {
  Allocator &allocator = c_allocator;
  unsigned char *items = nullptr;
  std::size_t len = 0;
  std::size_t cap = 0;

  struct Iterator {
    const unsigned char *items;
    std::size_t len;
    std::size_t current = 0;
    std::uint32_t current_codepoint = 0;
    std::uint32_t previous_codepoint = 0;

    explicit Iterator(unsigned char *const ptr, std::size_t len);

    auto done() const -> bool;
    auto peek() const -> std::uint32_t;
    auto prev() const -> std::uint32_t;
    auto next() -> Option<std::uint32_t>;
  };

  string(const char *src, Allocator &alloc = c_allocator);
  string(const unsigned char *src, Allocator &alloc = c_allocator);
  string(std::initializer_list<unsigned char> init, Allocator &alloc = c_allocator);
  auto deinit() -> void;

  constexpr auto push(const char ch) -> void { push((unsigned char)ch); }
  constexpr auto push(const char *str) -> void { push((unsigned char *) str); }
  auto push(const unsigned char ch) -> void;
  auto push(const unsigned char *str) -> void;
  auto push(const string &other) -> void;
  auto push(const std::initializer_list<unsigned char> list) -> void;

  void reserve(std::size_t new_cap);

  auto c_str() const -> const unsigned char *;
  auto bytes() const -> Slice<unsigned char>;
  auto utf8_iter() const -> Option<string::Iterator>;

  auto begin() -> Slice<unsigned char>::Iterator;
  auto end() -> Slice<unsigned char>::Iterator;

  auto is_valid_utf8() const -> bool;
  auto operator==(const unsigned char *other) const -> bool;
  auto operator==(const string &other) const -> bool;
  auto operator[](const std::size_t index) -> unsigned char &;
  auto operator[](const std::size_t index) const -> const unsigned char &;
};

#endif // !__STRING_HPP
