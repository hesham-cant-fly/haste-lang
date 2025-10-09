#ifndef __COMMON_H
#define __COMMON_H

#include <stdlib.h>
#include <stdnoreturn.h>

template<typename Fn>
struct defer {
  Fn fn;

  [[nodiscard]] explicit(false) constexpr defer(Fn fn) noexcept : fn(fn) {}
  constexpr compl defer() { this->fn(); }
};

template<typename Fn>
defer(Fn) -> defer<Fn>;

#define DEFER_1(x, y)  x##y
#define DEFER_2(x, y)  DEFER_1(x, y)
#define DEFER_3(x)     DEFER_2(x, __COUNTER__)
#define $defer(...) defer DEFER_3(_defer_) = __VA_ARGS__

#define $unused(...) ((void)(__VA_ARGS__))
#define VERSION "0.0.1-alpha"

void to_lower_case(char *str);

#endif // !__COMMON_H
