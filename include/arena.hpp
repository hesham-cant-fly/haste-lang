#ifndef __ARENA_HPP
#define __ARENA_HPP

#include "mylib/allocator.hpp"
#include <cstddef>

struct ArenaAllocator : public Allocator {
  struct Block {
    Block *prev = nullptr;
    std::size_t cap;
    std::size_t occupied;

    auto start() -> void*;
    auto remaining() -> std::size_t;
    auto percentages() -> float;
    auto current_ptr() -> void*;
  };

  Allocator &child = c_allocator;
  std::size_t default_block_size;
  std::size_t blocks_count = 0;
  Block *current_block = nullptr;

  ArenaAllocator(Allocator &child = c_allocator, std::size_t block_size = 1024);

  static auto make(Allocator &child = c_allocator, std::size_t block_size = 1024) -> ArenaAllocator;

  auto print() -> void;

  auto create_block(std::size_t size) -> Block*;
  auto find_block(std::size_t size, std::size_t alignment) -> Block*;

  auto allocate(std::size_t size, std::size_t alignment) -> void * override;
  auto reallocate(void *mem, std::size_t new_size, std::size_t alignment)
      -> void * override;
  auto deallocate(void *mem) -> void override;
  auto deinit() -> void override;
};

#endif // !__ARENA_HPP
