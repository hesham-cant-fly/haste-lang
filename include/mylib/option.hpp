#ifndef __OPTION_HPP
#define __OPTION_HPP

#include <cassert>
#include <cstdlib>

struct NoneOption {};
static constexpr NoneOption null = {};

template <typename T>
struct Option {
  bool has_value = false;
  union {
    T value;
  };

  Option() {}
  Option(T value) : value(value), has_value(true) {}
  Option(NoneOption) : has_value(false) {}

  auto unwrap() -> T& {
    if (has_value) return value;
    std::abort();
  }

  auto unwrap() const -> T& {
    if (has_value) return value;
    std::abort();
  }

  auto unwrap_or(T alternate) -> T& {
    if (has_value) return value;
    return alternate;
  }

  auto unwrap_or(T alternate) const -> T& {
    if (has_value) return value;
    return alternate;
  }

  operator bool() const { return has_value; }
};

#endif // !__OPTION_HPP
