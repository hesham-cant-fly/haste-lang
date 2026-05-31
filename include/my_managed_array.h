#ifndef MY_MANAGED_ARRAY_H_
#define MY_MANAGED_ARRAY_H_

#include <stdlib.h>
#include <assert.h>
#include "my_allocator.h"

/* Usage:
  struct NumberList {
    struct Allocator allocator;
	size_t cap;
	size_t len;
	int* items;
  };

  struct NumberList numbers = {0};
  marrpush(numbers, 1);
  marrpush(numbers, 2);
  marrpush(numbers, 3);

  arreach(numbers, i) {
    printf("%d\n", numbers.items[i]);
  }

  marrfree(numbers);
 */

#ifndef INITIAL_CAP
# define INITIAL_CAP 256
#endif // !INITIAL_CAP 
#ifndef GROW_FACTOR
# define GROW_FACTOR 2
#endif // !GROW_FACTOR

#define marrinit(T_, allocator_) (T_) { .allocator = (allocator_) }

#define marrsize(...) (sizeof(*(__VA_ARGS__).items)) * (__VA_ARGS__).cap
#define marrfree(array_) \
	do { \
		xdestroy((array_).allocator, marrsize(array_), (array_).items); \
		(array_).len = 0; \
		(array_).cap = 0; \
	} while (0)

#define marrpush(array_, item_) \
	do { \
		if ((array_).len >= (array_).cap) { \
			marrgrow((array_)); \
		} \
		(array_).items[(array_).len++] = (item_);	\
	} while (0)

#define marrgrow(array_) \
	do { \
		size_t old_cap_ = (array_).cap; \
		const size_t new_cap_ = old_cap_ == 0 ? (INITIAL_CAP) : old_cap_ * (GROW_FACTOR); \
		(array_).cap = new_cap_; \
		(array_).items = xrecreate((array_).allocator, old_cap_ * sizeof(*(array_).items), new_cap_ * sizeof(*(array_).items), (array_).items); \
	} while (0)

#define marrinsert(array_, item_, pos_) \
	do { \
		size_t len_ = (array_).len; \
		marrreserve((array_), len_ + 1); \
		assert((pos_) <= len_); \
		for (size_t i_ = len_; i_ >= (pos_) ; i_ -= 1) { \
			(array_).items[i_] = (array_).items[i_ - 1]; \
		} \
		(array_).items[(pos_)] = (item_); \
		(array_).len += 1; \
	} while (0)

#define marrpop(array_) \
	do { \
		if ((array_).len == 0) break; \
		(array_).len -= 1; \
	} while (0)

#define marrreserve(array_, min_cap_) \
	do { \
		if ((array_).cap >= (min_cap_)) {	\
			break; \
		} \
		size_t new_cap_ = (array_).cap ? (array_).cap : INITIAL_CAP; \
		while (new_cap_ < (min_cap_)) { \
			new_cap_ *= (GROW_FACTOR);			  \
		} \
		(array_).items = xrecreate((array_).allocator, (array_).cap * sizeof(*(array_).items), new_cap_ * sizeof(*(array_).items), (array_).items); \
		(array_).cap = new_cap_; \
	} while (0)

#define arrlen(array_) ((array_).len)
#define arrcap(array_) ((array_).len)
#define arrget(array_, index_) ((array_).items[index_])

// #define arreach(array_, index_) \
// 	for (size_t index_ = 0; (index_) < (array_).len; (index_) += 1)

#define arrprint(array_, fmt_) \
	do { \
		arreach((array_), i_) {	\
			printf((fmt_), (array_).items[i_]);	\
		} \
	} while (0)

#endif // !MY_MANAGED_ARRAY_H_
