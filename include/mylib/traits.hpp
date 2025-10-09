#ifndef __TRAITS_HPP
#define __TRAITS_HPP

template<typename T>
concept Deinit = requires (T arg) {
  { arg.deinit() };
};

#endif // !__TRAITS_HPP
