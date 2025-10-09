#include "arena.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

auto ArenaAllocator::Block::start() -> void * { return (void *)(this + 1); }

auto ArenaAllocator::Block::remaining() -> std::size_t {
  return cap - occupied;
}

auto ArenaAllocator::Block::percentages() -> float {
  // std::printf("here: %zu %zu\n", occupied, cap);
  return (float)occupied / (float)cap;
}

auto ArenaAllocator::Block::current_ptr() -> void * {
  return (void *)((std::uintptr_t)start() + occupied);
}

ArenaAllocator ::ArenaAllocator(Allocator &child, std::size_t block_size)
    : child(child), default_block_size(block_size) {}

auto ArenaAllocator::make(Allocator &child, std::size_t block_size)
    -> ArenaAllocator {
  return ArenaAllocator(child, block_size);
}

auto ArenaAllocator::print() -> void {
  std::size_t index = 1;
  std::size_t allocated_memory = 0;

  constexpr std::size_t max_len = 20;

  for (Block *current = current_block; current != nullptr;
       current = current->prev, --index) {
    allocated_memory += current->cap;
    std::printf("%04.zu [", blocks_count - index);
    float percentages = current->percentages();
    std::size_t used = max_len * percentages;
    for (std::size_t i = 0; i < used; ++i) {
      std::printf("+");
    }
    for (std::size_t i = 0; i < max_len - used; ++i) {
      std::printf("-");
    }
    std::printf("] %.2f%% used (%zu/%zu)\n", percentages * 100, current->occupied, current->cap);
  }

  std::printf("    <allocated %zu bytes>\n", allocated_memory);
}

auto ArenaAllocator::create_block(std::size_t size) -> Block * {
  Block *new_block =
      (Block *)child.allocate(size + sizeof(Block), alignof(std::max_align_t));
  *new_block = Block{
      .prev = current_block,
      .cap = size,
      .occupied = 0,
  };
  current_block = new_block;
  blocks_count += 1;
  return new_block;
}

constexpr auto align_forward_uintptr(std::uintptr_t v, std::size_t alignment)
    -> std::uintptr_t {
  return (alignment == 0) ? v : ((v + (alignment - 1)) / alignment) * alignment;
}

template <typename T> auto align_forward(T *ptr, std::size_t alignment) -> T * {
  static_assert(std::is_object<T>::value || std::is_void<T>::value,
                "align_forward requires object or void pointer type");
  if (alignment == 0)
    return ptr;

  std::uintptr_t p = reinterpret_cast<std::uintptr_t>(ptr);
  std::uintptr_t aligned = align_forward_uintptr(p, alignment);
  return reinterpret_cast<T *>(aligned);
}

auto ArenaAllocator::find_block(std::size_t size, std::size_t alignment)
    -> Block * {
  for (Block *current = current_block; current != nullptr;
       current = current->prev) {
    if (current->remaining() < size)
      continue;

    std::uintptr_t ptr = (std::uintptr_t)current->current_ptr();
    std::uintptr_t aligned_ptr = align_forward_uintptr(ptr, alignment);
    std::uintptr_t adjustment = aligned_ptr - ptr;

    if (current->remaining() >= adjustment + size)
      return current;
  }

  return nullptr;
}

auto ArenaAllocator::allocate(std::size_t size, std::size_t alignment)
    -> void * {
  Block *target_block = find_block(size, alignment);
  if (target_block == nullptr) {
    std::size_t actual_size = std::max(default_block_size, size);
    target_block = create_block(actual_size);
  }
  if (target_block == nullptr) {
    return nullptr;
  }

  std::uintptr_t ptr = (std::uintptr_t)target_block->current_ptr();
  std::uintptr_t aligned_ptr = align_forward_uintptr(ptr, alignment);
  std::uintptr_t adjustment = aligned_ptr - ptr;
  target_block->occupied += size + adjustment;

  return (void *)aligned_ptr;
}

auto ArenaAllocator::reallocate(void *mem, std::size_t new_size,
                                std::size_t alignment) -> void * {
  // Arena allocators don't really support realloc; allocate + copy.
  void *new_mem = allocate(new_size, alignment);
  // NOTE: user should handle copying manually or outside.
  return new_mem;
}

auto ArenaAllocator::deallocate(void *mem) -> void {
  // No-op: freed all at once in deinit()
  (void)mem;
}

auto ArenaAllocator::deinit() -> void {
  Block *current = current_block;
  while (current != nullptr) {
    Block *prev_block = current->prev;
    child.destroy((void *)current);
    current = prev_block;
  }
  current_block = nullptr;
  blocks_count = 0;
}
