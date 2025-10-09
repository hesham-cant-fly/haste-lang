#ifndef __RESULT_HPP
#define __RESULT_HPP

#include <cstdint>
#include <cstdlib>

template <typename T, typename E> struct Result {
  enum class State : std::uint8_t {
    Ok = 0,
    Err = 1,
  };

  State state;
  union {
    T res;
    E err;
  };

  Result(T res) : state(State::Ok), res(res) {}
  Result(E err) : state(State::Err), err(err) {}

  static inline auto Ok(T res) -> Result<T, E> {
    return {
      .state = State::Ok,
      .res = res,
    };
  }

  static inline auto Err(E err) -> Result<T, E> {
    return {
      .state = State::Err,
      .err = err,
    };
  }

  auto error() -> E& {
    if (is_err()) return err;
    std::abort();
  }

  auto unwrap() -> T& {
    if (is_ok()) return res;
    std::abort();
  }

  auto unwrap_or(T alternate) -> T & {
    if (is_ok()) return res;
    return alternate;
  }

  auto is_ok() -> bool {
    return state == State::Ok;
  }

  auto is_err() -> bool {
    return state == State::Err;
  }

  operator bool() const { return state == State::Ok; }
};

#endif // !__RESULT_HPP
