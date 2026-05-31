#ifndef MY_TEMPORARY_ALLOCATOR_H_
#define MY_TEMPORARY_ALLOCATOR_H_

// #define MY_TEMPORARY_ALLOCATOR_IMPL

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "my_allocator.h"
#include "my_stream.h"

/** NOTE: THIS IS OPTIONAL!!
  *       only use it when you want to increase the buffer size.
  *       also when using the function don't forget to call
  *       `free_temporary_allocator` at the end of the program!
  *       this will use the global default allocator.
  */
void setup_temporary_allocator(size_t size);
void free_temporary_allocator(void);
struct Allocator get_temporary_allocator(void);
void reset_temporary_allocator(void);

char *tsprint(const char *fmt, ...);
char *tsprintf(const char *fmt, ...);

#define tcreate(T_, ...)            (create(get_temporary_allocator(), T_, __VA_ARGS__))
#define talloc(size_)               (alloc(get_temporary_allocator(), size_))
#define talign_alloc(align_, size_) (align_alloc(get_temporary_allocator(), align_, size_))
#define trecreate(new_size_, ...)   (txrecreate(0, new_size_, __VA_ARGS__))

#define txdestroy(size_, ...)                 (xdestroy(get_temporary_allocator(), size_, __VA_ARGS__))
#define txrecreate(old_size_, new_size_, ...) (xrecreate(get_temporary_allocator(), old_size_, new_size_, __VA_ARGS__))

/**********************************/
/******** IMPLEMENTATION **********/
/**********************************/

# ifdef MY_TEMPORARY_ALLOCATOR_IMPL
# include <stdalign.h>
# include <stdint.h>
# include <stdlib.h>
# include <stdarg.h>
# include <threads.h>

# define TEMPORARY_ALLOCATOR_DEFAULT_CAP 8192
# define is_using_heap() (global_temporary.buffer != global_temporary_buffer)

struct temporary_allocator {
	size_t used;
	size_t cap;
	unsigned char *buffer;
};

static unsigned char global_temporary_buffer[TEMPORARY_ALLOCATOR_DEFAULT_CAP] = {0};
thread_local static struct temporary_allocator global_temporary = {
	.cap = TEMPORARY_ALLOCATOR_DEFAULT_CAP,
	.used = 0,
	.buffer = global_temporary_buffer,
};

static void *temp_allocate_virt(void *self, size_t alignment, size_t size);
static void *temp_reallocate_virt(void *self, size_t old_size, void *ptr, size_t alignment, size_t new_size);
static void temp_free_virt(void *self, size_t size, void *ptr);

static struct AllocatorInterface temporary_allocator_vtable = {
	.allocate = temp_allocate_virt,
	.reallocate = temp_reallocate_virt,
	.free = temp_free_virt,
};

void setup_temporary_allocator(size_t size)
{
	if (is_using_heap()) {
		global_temporary.buffer = xrenew(global_temporary.cap, size, global_temporary.buffer);
		return;
	}

	global_temporary.buffer = make(size);
}

void free_temporary_allocator(void)
{
	if (is_using_heap()) {
		xdelete(global_temporary.cap, global_temporary.buffer);
	}

	global_temporary.cap = TEMPORARY_ALLOCATOR_DEFAULT_CAP;
	global_temporary.used = 0;
	global_temporary.buffer = global_temporary_buffer;
}

void reset_temporary_allocator(void)
{
	global_temporary.used = 0;
}

struct Allocator get_temporary_allocator(void)
{
	return Allocator(&global_temporary, &temporary_allocator_vtable);
}

/* NOTE: I did not use AI for this function. I just
 *       thought that its a good idea to put comments
 *       on some lines of code as an easy explaination
 *       for my future self. and for anybody that is reading
 *       the source code implementation.
 */
char *tsprintf(const char *fmt, ...)
{
	va_list args = {0};
	va_start(args, fmt);

	// Calculate the aproximate length of the formated text.
	const size_t len = vsnprintf(NULL, 0, fmt, args);

	// reset args list
	va_end(args);
	va_start(args, fmt);

	char *result = talign_alloc(alignof(char), sizeof(char) * (len + 1));
	if (result == NULL) return NULL;

	// actually format :D
	vsnprintf(result, len + 1, fmt, args);
	va_end(args);

	result[len] = '\0';
	return result;
}

char *tsprint(const char *fmt, ...)
{
	va_list args = {0};
	va_start(args, fmt);
	const size_t len = vsnsprint(NULL, 0, fmt, args);
	va_end(args);

	char *result = talign_alloc(alignof(char), sizeof(char) * (len + 1));
	if (result == NULL) return NULL;

	va_start(args, fmt);
	vsnsprint(result, len + 1, fmt, args);
	va_end(args);
	return result;
}

static void *temp_allocate_virt(void *data, size_t alignment, size_t size)
{
	struct temporary_allocator *const self = data;

	uintptr_t curr = (uintptr_t)(self->buffer + self->used);
	uintptr_t aligned = (curr + alignment - 1) & ~(alignment - 1);
	size_t padding = aligned - curr;

	self->used += size + padding;
	if (self->used >= self->cap) {
		fprintf(stderr, "FATAL ERROR: not enough temporary space.\n");
		__asm__("int3");
		return NULL;
	}

	return (void*)aligned;
}

#ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static void *temp_reallocate_virt(void *data, size_t old_size, void *ptr, size_t alignment, size_t new_size)
{
	(void)old_size;
	struct temporary_allocator *const self = data;

	const size_t potential_allocation_size = (uintptr_t)(self->buffer + self->used) - (uintptr_t)ptr;
	void* const result = temp_allocate_virt(self, alignment, new_size);
	memcpy(result, ptr, MIN(potential_allocation_size, new_size));

	return result;
}

static void temp_free_virt(void *data, size_t size, void *ptr)
{
	struct temporary_allocator *const self = data;
	(void)self;
	(void)size;
	(void)ptr;
}

# endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !MY_TEMPORARY_ALLOCATOR_H_
