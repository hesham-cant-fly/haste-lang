#ifndef MY_ALLOCATOR_H_
#define MY_ALLOCATOR_H_

// #define MY_ALLOCATOR_IMPL

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>
#include <string.h>
#include "my_assert.h"

struct Allocator {
	struct AllocatorInterface *vtable;
	void *data;
};

struct AllocatorInterface {
	void *(*allocate)(void *self, size_t alignment, size_t size);
	void *(*reallocate)(void *self, size_t old_size, void *ptr, size_t alignment, size_t new_size);
	void (*free)(void *self, size_t size, void *ptr);
};

union max_align_t_ {
	void *p;
	long l;
	double d;
	long double ld;
};

extern size_t allocated;

#define MY_DEFAULT_ALIGNMENT \
	(sizeof(union max_align_t_))

// #define GET_ALIGNMENT(...) _Alignof(__VA_ARGS__)
#define GET_ALIGNMENT(...) MY_DEFAULT_ALIGNMENT

#define create(allocator_, T_, ...)            (allocator_alloc_with_value((allocator_), GET_ALIGNMENT(T_), sizeof((T_){ __VA_ARGS__ }), &((T_){ __VA_ARGS__ })))
#define alloc(allocator_, size_)               allocator_alloc((allocator_), MY_DEFAULT_ALIGNMENT, (size_))
#define align_alloc(allocator_, align_, size_) allocator_alloc((allocator_), align_, (size_))
#define destroy(allocator_, ...)               allocator_free((allocator_), 0, (__VA_ARGS__))
#define recreate(allocator_, new_size_, ...)   allocator_realloc((allocator_), 0, (__VA_ARGS__), MY_DEFAULT_ALIGNMENT, (new_size_))

#define xdestroy(allocator_, size_, ...)      allocator_free((allocator_), size_, (__VA_ARGS__))
#define xrecreate(allocator_, old_size_, new_size_, ...) allocator_realloc((allocator_), old_size_, (__VA_ARGS__), MY_DEFAULT_ALIGNMENT, (new_size_))

#define new(T_, ...)          create((default_allocator_), T_, __VA_ARGS__)
#define make(size_)   alloc((default_allocator_), (size_))
#define aligned_make(align_, size_)   alloc((default_allocator_), (align_), (size_))
#define delete(...)           destroy((default_allocator_), (__VA_ARGS__))
#define renew(new_size_, ...) recreate((default_allocator_), (new_size_), __VA_ARGS__)

#define default_allocator default_allocator_

extern struct Allocator default_allocator_;

void set_default_allocator(struct Allocator allocator);
struct Allocator get_default_allocator(void);

struct Allocator Allocator(
	void *data,
	struct AllocatorInterface *vtable);
void *allocator_alloc(
	struct Allocator allocator,
	size_t alignment,
	size_t size);
void *allocator_alloc_with_value(
	struct Allocator allocator,
	size_t alignment,
	size_t size,
	void *value);
void *allocator_realloc(
	struct Allocator allocator,
	size_t old_size,
	void *ptr,
	size_t alignment,
	size_t new_size);
void allocator_free(struct Allocator allocator, size_t size, void *ptr);

void *clone_memory(struct Allocator allocator, const void *ptr, const size_t len);
char *clone_string(struct Allocator allocator, const char *str);
char *nclone_string(struct Allocator allocator, const char *str, const size_t len);

#ifdef MY_ALLOCATOR_IMPL
size_t allocated = 0;
struct Allocator default_allocator_ = {0};

void set_default_allocator(struct Allocator allocator)
{
	default_allocator_ = allocator;
}

struct Allocator get_default_allocator(void)
{
	return default_allocator_;
}

struct Allocator Allocator(
	void *data,
	struct AllocatorInterface *vtable)
{
	return (struct Allocator) {
		.data = data,
		.vtable = vtable,
	};
}

void *allocator_alloc_with_value(
	struct Allocator allocator,
	size_t alignment,
	size_t size,
	void *value)
{
	void *result = allocator_alloc(allocator, alignment, size);
	if (result == NULL) return NULL;

	memcpy(result, value, size);

	return result;
}

void *allocator_alloc(
	struct Allocator allocator,
	size_t alignment,
	size_t size)
{
	assert(allocator.vtable);
	allocated += size;
	return allocator.vtable->allocate(allocator.data, alignment, size);
}

void *allocator_realloc(
	struct Allocator allocator,
	size_t old_size,
	void *ptr,
	size_t alignment,
	size_t new_size)
{
	assert(allocator.vtable);
	allocated += new_size - old_size;
	return allocator.vtable->reallocate(allocator.data, old_size, ptr, alignment, new_size);
}

void allocator_free(
	struct Allocator allocator,
	size_t size,
	void *ptr)
{
	assert(allocator.vtable);
	allocated -= size;
	allocator.vtable->free(allocator.data, size, ptr);
}

void *clone_memory(struct Allocator allocator, const void *ptr, const size_t len)
{
	void *result = alloc(allocator, len);
	memcpy(result, ptr, len);
	return result;
}

char *clone_string(struct Allocator allocator, const char *str)
{
	const size_t len = strlen(str);
	return nclone_string(allocator, str, len);
}

char *nclone_string(struct Allocator allocator, const char *str, const size_t len)
{
	char *result = alloc(allocator, len + 1);
	memcpy(result, str, len);
	result[len] = '\0';
	return result;
}
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_ALLOCATOR_H_
