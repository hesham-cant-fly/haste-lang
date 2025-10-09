#ifndef __SLICE_HPP
#define __SLICE_HPP

#include <cassert>
#include <cstddef>

template <typename T> struct Slice {
  struct Iterator {
    using Value = T;
    T *ptr;

    explicit Iterator(T *ptr) : ptr(ptr) {}

    bool operator!=(Iterator &other) { return ptr != other.ptr; }

    Iterator operator++() {
      Iterator result(ptr++);
      return result;
    }

    Value &operator*() { return *ptr; }
  };

  T *items;
  std::size_t len;

  explicit Slice(T *items, std::size_t len) : items(items), len(len) {}

  Slice<T> slice(std::size_t start) const {
    assert(start <= len && "Out Of Bound.");
    return Slice(items + start, len - start);
  }

  Slice<T> slice(std::size_t start, std::size_t end) const {
    assert(start <= len && "Out Of Bound.");
    assert(end <= len && "Out Of Bound.");
    return Slice(items + start, end - start);
  }

  Iterator begin() { return Iterator(items); }
  Iterator end() { return Iterator(items + len); }

  Iterator begin() const { return Iterator(items); }
  Iterator end() const { return Iterator(items + len); }

  operator T*() {
    return items;
  }

  const T &operator[](std::size_t index) const noexcept {
    assert(index < len && "Out Of Bound.");
    return items[index];
  }

  T &operator[](std::size_t index) noexcept {
    assert(index < len && "Out Of Bound.");
    return items[index];
  }
};

#endif // !__SLICE_HPP
