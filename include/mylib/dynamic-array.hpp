#ifndef __DYNAMIC_ARRAY_HPP
#define __DYNAMIC_ARRAY_HPP

#include "mylib/allocator.hpp"
#include "mylib/slice.hpp"
#include "mylib/traits.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

template <typename T, bool own = true, std::size_t default_cap = 0>
struct ArrayList {
  Allocator &allocator = c_allocator;
  T *items = nullptr;
  std::size_t len = 0;
  std::size_t cap = default_cap;

  explicit ArrayList(Allocator &allocator) : allocator(allocator) {
    ArrayList();
  }
  explicit ArrayList() {
    if (cap > 0) {
      items = allocator.alloc<T>(cap);
      assert(items && "Buy more RAM lol.");
    }
  }
  explicit ArrayList(std::initializer_list<T> init) { push(init); }
  explicit ArrayList(std::initializer_list<T> init, Allocator &allocator) : allocator(allocator) {
    push(init);
  }

  void deinit() {
    if constexpr (Deinit<T> && own) {
      for (std::size_t i = 0; i < len; ++i) {
        items[i].deinit();
      }
    }
    allocator.destroy(items);
  }

  void push(T value) {
    if (len >= cap) {
      std::size_t new_cap = cap == 0 ? 1 : cap * 2;
      reserve(new_cap);
    }
    items[len++] = value;
  }

  void push(std::initializer_list<T> value) {
    if (len + value.size() >= cap) {
      std::size_t new_cap = cap == 0 ? 1 : cap * 2;
      if (new_cap < value.size()) {
        new_cap += value.size();
      }
      reserve(new_cap);
    }
    for (auto &v : value)
      items[len++] = v;
  }

  void pop() {
    assert(len > 0 && "Out Of Bound");
    if constexpr (Deinit<T> && own) {
      items[len - 1].deinit();
    }
    len -= 1;
  }

  void reserve(std::size_t new_cap) {
    items = allocator.realloc(items, new_cap);
    assert(items && "Buy more RAM lol.");
    cap = new_cap;
  }

  Slice<T> slice(std::size_t start) const {
    assert(start <= len && "Out Of Bound.");
    return Slice(items + start, len - start);
  }

  Slice<T> slice(std::size_t start, std::size_t end) const {
    assert(end >= start && "Out Of Bound.");
    assert(start <= len && "Out Of Bound.");
    assert(end <= len && "Out Of Bound.");
    return Slice(items + start, end - start);
  }

  typename Slice<T>::Iterator begin() { return slice(0).begin(); }
  typename Slice<T>::Iterator end() { return slice(0).end(); }

  typename Slice<T>::Iterator begin() const { return slice(0).begin(); }
  typename Slice<T>::Iterator end() const { return slice(0).end(); }

  const T &operator[](std::size_t index) const noexcept {
    assert(index < len && "Out Of Bound.");
    return items[index];
  }

  T &operator[](std::size_t index) noexcept {
    assert(index < len && "Out Of Bound.");
    return items[index];
  }
};

#endif // !__DYNAMIC_ARRAY_HPP
