#ifndef MY_ARRAY_H_
#define MY_ARRAY_H_

#include <stdlib.h>
#include <assert.h>

/* Usage:
  struct NumberList {
	size_t cap;
	size_t len;
	int* items;
  };

  struct NumberList numbers = {0};
  arrpush(numbers, 1);
  arrpush(numbers, 2);
  arrpush(numbers, 3);

  iarreach(numbers, i) {
    printf("%d\n", numbers.items[i]);
  }

  arrfree(numbers);
 */

#ifndef MY_ARRAY_INITIAL_CAP
# define MY_ARRAY_INITIAL_CAP 10
#endif // !MY_ARRAY_INITIAL_CAP
#ifndef MY_ARRAY_GROW_FACTOR
# define MY_ARRAY_GROW_FACTOR 2
#endif // !MY_ARRAY_GROW_FACTOR

#define arrsize(...) (sizeof(*(__VA_ARGS__).items)) * (__VA_ARGS__).cap
#define arrfree(allocator_, array__) \
	do { \
		xdestroy(allocator_, arrsize(array__), (array__).items); \
		(array__).len = 0; \
		(array__).cap = 0; \
	} while (0)

#define arrpush(allocator_, array__, item__) \
	do { \
		if ((array__).len >= (array__).cap) { \
			arrgrow((allocator_), (array__)); \
		} \
		(array__).items[(array__).len++] = (item__);	\
	} while (0)

#define arrgrow(allocator_, array__) \
	do { \
		const size_t new_cap__ = (array__).cap == 0 ? (MY_ARRAY_INITIAL_CAP) : (array__).cap * (MY_ARRAY_GROW_FACTOR); \
		(array__).items = xrecreate((allocator_), arrsize(array__), (new_cap__ * sizeof(*(array__).items)), (array__).items); \
		(array__).cap = new_cap__; \
	} while (0)

#define arrinsert(allocator_, array__, item__, pos__) \
	do { \
		size_t len__ = (array__).len; \
		arrreserve((allocator_), (array__), len__ + 1); \
		assert((pos__) <= len__); \
		for (size_t i__ = len__; i__ >= (pos__) ; i__ -= 1) { \
			(array__).items[i__] = (array__).items[i__ - 1]; \
		} \
		(array__).items[(pos__)] = (item__); \
		(array__).len += 1; \
	} while (0)

#define arrpop(array__) \
	do { \
		if ((array__).len == 0) break; \
		(array__).len -= 1; \
	} while (0)

#define arrreserve(allocator_, array__, min_cap__) \
	do { \
		if ((array__).cap >= (min_cap__)) {	\
			break; \
		} \
		size_t new_cap__ = (array__).cap ? (array__).cap : MY_ARRAY_INITIAL_CAP; \
		while (new_cap__ < (min_cap__)) { \
			new_cap__ *= (MY_ARRAY_GROW_FACTOR);			  \
		} \
		(array__).items = xrecreate((allocator_), arrsize(array__), MY_DEFAULT_ALIGNMENT,  new_cap__ * sizeof(*(array__).items), (array__).items); \
		(array__).cap = new_cap__; \
	} while (0)

#define marrlen(array__) ((array__).len)
#define marrcap(array__) ((array__).len)
#define marrget(array__, index__) ((array__).items[index__])

#endif // !MY_ARRAY_H_
