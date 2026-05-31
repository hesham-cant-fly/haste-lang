#ifndef MY_C_ALLOCATOR_H_
#define MY_C_ALLOCATOR_H_

// #define MY_C_ALLOCATOR_IMPL

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "my_allocator.h"

struct Allocator get_c_allocator(void);

#ifdef MY_C_ALLOCATOR_IMPL
#include <stdlib.h>

static void *c_alloc_virt(void *self, size_t alignment, size_t size);
static void *c_realloc_virt(void *self, size_t old_size, void *ptr, size_t alignment, size_t size);
static void  c_free_virt(void *self, size_t size, void *ptr);

static struct AllocatorInterface c_allocator_vtable = {
	.allocate = c_alloc_virt,
	.reallocate = c_realloc_virt,
	.free = c_free_virt,
};

struct Allocator get_c_allocator(void)
{
	return (struct Allocator) {
		.vtable = &c_allocator_vtable,
		.data = NULL,
	};
}

static void *c_alloc_virt(void *self, size_t alignment, size_t size)
{
	((void)self);
	((void)alignment);
	allocated += size;
	return malloc(size);
}

static void *c_realloc_virt(void *self, size_t old_size, void *ptr, size_t alignment, size_t size)
{
	((void)self);
	(void)old_size;
	((void)alignment);
	allocated += size - old_size;
	return realloc(ptr, size);
}

static void c_free_virt(void *self, size_t size, void *ptr)
{
	((void)self);
	allocated -= size;
	free(ptr);
}

#endif // MY_C_ALLOCATOR_IMPL

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_C_ALLOCATOR_H_
