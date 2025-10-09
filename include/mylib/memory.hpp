#ifndef __MEMORY_HPP
#define __MEMORY_HPP

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>

template <typename T> T *create(std::size_t size = 1) {
  return (T *)std::malloc(sizeof(T) * size);
}

template <typename T> T *create(T value) {
  void *result = std::malloc(sizeof(T));
  assert(result && "Buy more RAM lol.");
  std::memcpy(result, (void *)&value, sizeof(T));
  return (T *)result;
}

template <typename T> void remove(T *mem) {
  std::free((void *)mem);
}

template <typename T> T *recreate(T *mem, std::size_t new_size) {
  if (mem == nullptr) return (T *)create<T>(new_size);
  return (T *)std::realloc((void *)mem, sizeof(T) * new_size);
}

#endif // __MEMORY_HPP
