#ifndef MY_CHUNKED_ARRAY_H_
#define MY_CHUNKED_ARRAY_H_

#include <stdlib.h>
#include <assert.h>

/*
  Chunked dynamic array. Instead of reallocating a single contiguous block,
  we allocate fixed-size chunks and link them. Existing elements never move.

  Usage:
    struct NumberList {
      size_t len;
      size_t chunk_count;
      int **chunks;
    };

    struct NumberList numbers = {0};
    arrpush(alloc, numbers, 1);
    arrpush(alloc, numbers, 2);

    for (size_t i = 0; i < numbers.len; i++)
      printf("%d\n", arrget(numbers, i));

    arrfree(alloc, numbers);
*/

#ifndef MY_ARRAY_CHUNK
# define MY_ARRAY_CHUNK 256
#endif

#define arrget(array__, i__) \
	((array__).chunks[(i__) / MY_ARRAY_CHUNK][(i__) % MY_ARRAY_CHUNK])

#define arrfree(allocator_, array__) \
	do { \
		for (size_t _i__ = 0; _i__ < (array__).chunk_count; _i__++) \
			xdestroy(allocator_, sizeof(**(array__).chunks) * MY_ARRAY_CHUNK, (array__).chunks[_i__]); \
		xdestroy(allocator_, (array__).chunk_count * sizeof(*(array__).chunks), (array__).chunks); \
		(array__).len = 0; \
		(array__).chunk_count = 0; \
	} while (0)

#define arrpush(allocator_, array__, item__) \
	do { \
		if ((array__).len >= (array__).chunk_count * MY_ARRAY_CHUNK) { \
			arrgrow((allocator_), (array__)); \
		} \
		size_t _idx__ = (array__).len; \
		arrget((array__), _idx__) = (item__); \
		(array__).len = _idx__ + 1; \
	} while (0)

#define arrgrow(allocator_, array__) \
	do { \
		size_t _old_cc__ = (array__).chunk_count; \
		size_t _new_cc__ = _old_cc__ == 0 ? 1 : _old_cc__ * 2; \
		(array__).chunks = xrecreate((allocator_), \
			_old_cc__ * sizeof(*(array__).chunks), \
			_new_cc__ * sizeof(*(array__).chunks), \
			(array__).chunks); \
		for (size_t _i__ = _old_cc__; _i__ < _new_cc__; _i__++) \
			(array__).chunks[_i__] = alloc((allocator_), sizeof(**(array__).chunks) * MY_ARRAY_CHUNK); \
		(array__).chunk_count = _new_cc__; \
	} while (0)

#define arrreserve(allocator_, array__, min_cap__) \
	do { \
		size_t _need__ = ((min_cap__) + MY_ARRAY_CHUNK - 1) / MY_ARRAY_CHUNK; \
		while ((array__).chunk_count < _need__) { \
			arrgrow((allocator_), (array__)); \
		} \
	} while (0)

#define arrinsert(allocator_, array__, item__, pos__) \
	do { \
		size_t len__ = (array__).len; \
		arrreserve((allocator_), (array__), len__ + 1); \
		assert((pos__) <= len__); \
		for (size_t i__ = len__; i__ > (pos__); i__--) { \
			arrget((array__), i__) = arrget((array__), i__ - 1); \
		} \
		arrget((array__), (pos__)) = (item__); \
		(array__).len += 1; \
	} while (0)

#define arrpop(array__) \
	do { \
		if ((array__).len == 0) break; \
		(array__).len -= 1; \
	} while (0)

#endif // !MY_CHUNKED_ARRAY_H_
