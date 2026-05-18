#ifndef MY_ARENA_ALLOCATOR_H_
#define MY_ARENA_ALLOCATOR_H_

// #define MY_ARENA_ALLOCATOR_IMPL

#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "my_allocator.h"
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct ArenaBlock {
	struct ArenaBlock *next;
	void *current;
	void *end;
};

struct Arena {
	struct Allocator child_allocator;
	struct ArenaBlock *begin;
	struct ArenaBlock *end;
	size_t total_size;
};

#ifndef ARENA_REGION_DEFAULT_CAPACITY
# define ARENA_REGION_DEFAULT_CAPACITY (8 * 1024)
#endif /* ARENA_REGION_DEFAULT_CAPACITY */

struct Arena Arena(struct Allocator child_allocator);
struct Arena ArenaDefault(void);
void arena_free(struct Arena *self);
void arena_print(FILE *out, const struct Arena arena);

struct Allocator arena_get_allocator(struct Arena *arena);

#ifdef MY_ARENA_ALLOCATOR_IMPL
static void *arena_allocate_virt(void *data, size_t alignment, size_t size);
static void *arena_reallocate_virt(void *data, size_t old_size, void *ptr, size_t alignment, size_t new_size);
static void arena_free_virt(void *data, size_t size, void *ptr);

static struct AllocatorInterface vtable = {
	.allocate = arena_allocate_virt,
	.reallocate = arena_reallocate_virt,
	.free = arena_free_virt,
};

struct Arena Arena(struct Allocator child_allocator)
{
	struct Arena result = (struct Arena){0};
	result.child_allocator = child_allocator;
	return result;
}

struct Arena ArenaDefault(void)
{
	return Arena(get_default_allocator());
}

void arena_free(struct Arena *self)
{
	struct ArenaBlock *current = self->begin;

	while (current != NULL) {
		struct ArenaBlock *next = current->next;
		xdestroy(self->child_allocator, self->total_size, current);
		current = next;
	}

	*self = (struct Arena){0};
}

struct Allocator arena_get_allocator(struct Arena *arena)
{
	return Allocator(arena, &vtable);
}

static struct ArenaBlock *arena_append_block(struct Arena *self, size_t min_size)
{
	size_t block_size = (ARENA_REGION_DEFAULT_CAPACITY > min_size ? ARENA_REGION_DEFAULT_CAPACITY : min_size) + sizeof(struct ArenaBlock);
	// if (min_size + sizeof(struct ArenaBlock) > block_size) {
	// 	block_size = min_size + sizeof(struct ArenaBlock);
	// }

	struct ArenaBlock* new_block = align_alloc(
		self->child_allocator,
		GET_ALIGNMENT(struct ArenaBlock),
		block_size);
	if (new_block == NULL) {
		return NULL;
	}

	new_block->current = new_block + 1;
	new_block->end = (void*)((uintptr_t)new_block + block_size);
	new_block->next = NULL;

	self->total_size += block_size;
	if (self->begin == NULL) {
		assert(self->end == NULL);
		self->begin = new_block;
		self->end = new_block;
	} else {
		self->end->next = new_block;
		self->end = new_block;
	}

	return new_block;
}

static size_t block_used_size(const struct ArenaBlock *self)
{
	return (uintptr_t)self->current - (uintptr_t)(self + 1);
}

static size_t block_available_size(const struct ArenaBlock *self)
{
	return (uintptr_t)self->end - (uintptr_t)self->current;
}

static size_t block_actual_size(const struct ArenaBlock *self)
{
	return (uintptr_t)self->end - (uintptr_t)(self + 1);
}

static struct ArenaBlock *find_owner(struct Arena *self, void *ptr)
{
	struct ArenaBlock *current = self->begin;

	while (current != NULL) {
		struct ArenaBlock *next = current->next;
		void *start = current + 1;
		if (ptr < current->end && ptr >= start) {
			return current;
		}
		current = next;
	}

	return NULL;
}
static void *arena_allocate_virt(void *data, size_t alignment, size_t size)
{
    struct Arena *self = data;

    assert((alignment & (alignment - 1)) == 0 && "Alignment must be a power of 2");

    if (self->end == NULL) {
        assert(self->begin == NULL);
        arena_append_block(self, size);
    }

    struct ArenaBlock *blk = self->end;

    uintptr_t curr = (uintptr_t)blk->current;
    uintptr_t aligned = (curr + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - curr;

    if (block_available_size(blk) < size + padding) {
        blk = arena_append_block(self, size + alignment);

        curr = (uintptr_t)blk->current;
        aligned = (curr + alignment - 1) & ~(alignment - 1);
    }

    blk->current = (void*)(aligned + size);
    return (void*)aligned;
}

#ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static void *arena_reallocate_virt(void *data, size_t old_size, void *ptr, size_t alignment, size_t new_size)
{
	(void)old_size;
	struct Arena *const self = data;

	struct ArenaBlock *const target = find_owner(self, ptr);
	if (target == NULL) return NULL; // ERROR: the pointer is nowhere to be allocated in the arena

	//    ٧٧٧٧٧٧٧ potential size
	// [--*-----|-------]
	//    ^ ptr ^
	//          |
	//          +- current
	const size_t potential_allocation_size = (uintptr_t)target->current - (uintptr_t)ptr;
	void* const result = arena_allocate_virt(self, alignment, new_size);
	memcpy(result, ptr, MIN(potential_allocation_size, new_size));

	return result;
}

static void arena_free_virt(void *data, size_t size, void *ptr)
{
	(void)size;
	((void)data);
	((void)ptr);
	// NOTE: This just doesn't make sense
}

void arena_print(FILE *out, const struct Arena arena)
{
	const size_t width = 20;
	struct ArenaBlock *current = arena.begin;
	size_t index = 0;
	char *const bar = calloc(sizeof(char), width + 1);
	if (bar == NULL) return;

	while (current != NULL) {
		struct ArenaBlock *next = current->next;
		const float persentage = (float)(block_actual_size(current) - block_available_size(current)) / (float)block_actual_size(current);

		memset(bar, ' ', width);
		for (size_t i=0; i < width * persentage; i += 1) bar[i] = '=';

		fprintf(
			out,
			"Block(%04zu): %6.2f%% [%s] %zu/%zu bytes\n",
			index++, persentage * 100.0F, bar,
			block_actual_size(current) - block_available_size(current),
			block_actual_size(current));
		current = next;
	}

	{
		double size = (double)arena.total_size;
		const char *unit = "B";

		if (size >= 1024) { size /= 1024; unit = "kB"; }
		if (size >= 1024) { size /= 1024; unit = "MB"; }
		if (size >= 1024) { size /= 1024; unit = "GB"; }

		fprintf(out, "\t> Allocated: %zu bytes (%.2f %s)\n",
				arena.total_size, size, unit);
	}

	free(bar);
}

#endif // !MY_ARENA_ALLOCATOR_IMPL

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_ARENA_ALLOCATOR_H_
