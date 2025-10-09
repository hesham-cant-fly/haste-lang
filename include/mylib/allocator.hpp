#ifndef __ALLOCATOR_HPP
#define __ALLOCATOR_HPP

#include "mylib/slice.hpp"
#include <cstddef>
#include <cstdio>

struct Allocator {
  template <typename T> T *create() {
    return (T *)allocate(sizeof(T), alignof(T));
  }

  template <typename T> T *create(T value) {
    auto result = (T *)allocate(sizeof(T), alignof(T));
    *result = value;
    return result;
  }

  template <typename T> Slice<T> alloc(std::size_t count = 1) {
    auto result = (T *)allocate(sizeof(T) * count, alignof(T));
    return Slice(result, count);
  }

  template <typename T> void destroy(T *ptr) {
    deallocate((void *)ptr);
  }

  template <typename T> Slice<T> realloc(Slice<T> mem, std::size_t new_size) {
    auto new_mem = (T *)reallocate((void *)mem.items, sizeof(T) * new_size, alignof(T));
    return Slice(new_mem, new_size);
  }
  
  template <typename T> Slice<T> realloc(T *mem, std::size_t new_size) {
    auto new_mem = (T *)reallocate((void *)mem, sizeof(T) * new_size, alignof(T));
    return Slice(new_mem, new_size);
  }

  virtual void *allocate(std::size_t size, std::size_t alignment) = 0;
  virtual void *reallocate(void *mem, std::size_t new_size, std::size_t alignment) = 0;
  virtual void deallocate(void *mem) = 0;
  virtual void deinit() {}
};

struct CAllocator :public Allocator {
  void *allocate(std::size_t size, std::size_t alignment) override;
  void *reallocate(void *mem, std::size_t new_size, std::size_t alignment) override;
  void deallocate(void *mem) override;
};

extern CAllocator c_allocator;

#endif // !__ALLOCATOR_HPP
