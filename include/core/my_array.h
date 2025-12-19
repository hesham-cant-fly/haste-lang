#ifndef MY_ARRAY_H_
#define MY_ARRAY_H_

#include <stdlib.h>
#include <assert.h>

#ifndef INITIAL_CAP
# define INITIAL_CAP 256
#endif // !INITIAL_CAP 
#ifndef GROW_FACTOR
# define GROW_FACTOR 2
#endif // !GROW_FACTOR

#define arrfree(array__) \
	do { \
		free((array__).items); \
		(array__).len = 0; \
		(array__).cap = 0; \
	} while (0)

#define arrpush(array__, item__) \
	do { \
		if ((array__).len >= (array__).cap) { \
			const size_t new_cap__ = (array__).cap == 0 ? (INITIAL_CAP) : (array__).cap * (GROW_FACTOR); \
			(array__).cap = new_cap__; \
			(array__).items = realloc((array__).items, (array__).cap * sizeof(*(array__).items)); \
		} \
		(array__).items[(array__).len++] = (item__);	\
	} while (0)

#define arrinsert(array__, item__, pos__) \
	do { \
		size_t len__ = (array__).len; \
		arrreserve((array__), len__ + 1); \
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

#define arrreserve(array__, min_cap__) \
	do { \
		if ((array__).cap >= (min_cap__)) {	\
			break; \
		} \
		size_t new_cap__ = (array__).cap ? (array__).cap : INITIAL_CAP; \
		while (new_cap__ < (min_cap__)) { \
			new_cap__ *= (GROW_FACTOR);			  \
		} \
		(array__).items = realloc((array__).items, new_cap__ * sizeof(*(array__).items)); \
		(array__).cap = new_cap__; \
	} while (0)

#define arrlen(array__) ((array__).len)
#define arrcap(array__) ((array__).len)
#define arrget(array__, index__) ((array__).items[index__])

#define arreach(array__, index__) \
	for (size_t index__ = 0; (index__) < (array__).len; (index__) += 1)

#define arrprint(array__, fmt__) \
	do { \
		arreach((array__), i__) {	\
			printf((fmt__), (array__).items[i__]);	\
		} \
	} while (0)

#endif // !MY_ARRAY_H_
