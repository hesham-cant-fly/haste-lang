#include "mylib/allocator.hpp"
#include <cstdio>
#include <cstdlib>

CAllocator c_allocator = CAllocator{};

void *CAllocator::allocate(std::size_t size, std::size_t alignment) {
  return malloc(size);
}

void *CAllocator::reallocate(void *mem, std::size_t new_size, std::size_t alignment) {
  return std::realloc(mem, new_size);
}

void CAllocator::deallocate(void *mem) {
  std::free(mem);
}
