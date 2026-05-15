#ifndef MY_MANAGED_CHUNKED_ARRAY_H_
#define MY_MANAGED_CHUNKED_ARRAY_H_

#include <stdlib.h>
#include <assert.h>
#include "my_allocator.h"

/*
  Chunked dynamic array with embedded allocator.
  Instead of reallocating a single contiguous block,
  we allocate fixed-size chunks and link them.

  Usage:
    struct NumberList {
      struct Allocator allocator;
      size_t len, chunk_count;
      int **chunks;
    };

    struct NumberList numbers = marrinit(struct NumberList, some_allocator);
    marrpush(numbers, 1);

    for (size_t i = 0; i < numbers.len; i++)
      printf("%d\n", marrget(numbers, i));

    marrfree(numbers);
*/

#ifndef MY_MANAGED_CHUNK
# define MY_MANAGED_CHUNK 256
#endif

#define marrinit(T_, allocator_) (T_) { .allocator = (allocator_) }

#define marrget(array_, i_) \
	((array_).chunks[(i_) / MY_MANAGED_CHUNK][(i_) % MY_MANAGED_CHUNK])

#define marrfree(array_) \
	do { \
		for (size_t _i_ = 0; _i_ < (array_).chunk_count; _i_++) \
			xdestroy((array_).allocator, sizeof(**(array_).chunks) * MY_MANAGED_CHUNK, (array_).chunks[_i_]); \
		xdestroy((array_).allocator, (array_).chunk_count * sizeof(*(array_).chunks), (array_).chunks); \
		(array_).len = 0; \
		(array_).chunk_count = 0; \
	} while (0)

#define marrpush(array_, item_) \
	do { \
		if ((array_).len >= (array_).chunk_count * MY_MANAGED_CHUNK) { \
			marrgrow((array_)); \
		} \
		size_t _idx_ = (array_).len; \
		marrget((array_), _idx_) = (item_); \
		(array_).len = _idx_ + 1; \
	} while (0)

#define marrgrow(array_) \
	do { \
		size_t _old_cc_ = (array_).chunk_count; \
		size_t _new_cc_ = _old_cc_ == 0 ? 1 : _old_cc_ * 2; \
		(array_).chunks = recreate((array_).allocator, \
			_new_cc_ * sizeof(*(array_).chunks), \
			(array_).chunks); \
		for (size_t _i_ = _old_cc_; _i_ < _new_cc_; _i_++) \
			(array_).chunks[_i_] = alloc((array_).allocator, sizeof(**(array_).chunks) * MY_MANAGED_CHUNK); \
		(array_).chunk_count = _new_cc_; \
	} while (0)

#define marrreserve(array_, min_cap_) \
	do { \
		size_t _need_ = ((min_cap_) + MY_MANAGED_CHUNK - 1) / MY_MANAGED_CHUNK; \
		while ((array_).chunk_count < _need_) { \
			marrgrow((array_)); \
		} \
	} while (0)

#define marrinsert(array_, item_, pos_) \
	do { \
		size_t len_ = (array_).len; \
		marrreserve((array_), len_ + 1); \
		assert((pos_) <= len_); \
		for (size_t i_ = len_; i_ > (pos_); i_--) { \
			marrget((array_), i_) = marrget((array_), i_ - 1); \
		} \
		marrget((array_), (pos_)) = (item_); \
		(array_).len += 1; \
	} while (0)

#define marrpop(array_) \
	do { \
		if ((array_).len == 0) break; \
		(array_).len -= 1; \
	} while (0)

#endif // !MY_MANAGED_CHUNKED_ARRAY_H_
