#ifndef MY_THIN_ARRAY_H_
#define MY_THIN_ARRAY_H_

// #define MY_ARRAY_IMPL

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct ArrayListHeader {
  size_t len;
  size_t cap;
} ArrayListHeader;

#ifndef INITIAL_CAP
# define INITIAL_CAP 10
#endif
#ifndef GROW_FACTOR
# define GROW_FACTOR 2
#endif

#define thinarrinit(T) (T *)(create_array(sizeof(T)))
#define thinarrfree(arr) (assert((arr) != NULL), free(arrheader((arr))))
#define thinarrheader(arr) (assert((arr) != NULL), (ArrayListHeader *)(((char *)(arr)) - sizeof(ArrayListHeader)))
#define thinarrlen(arr) (((arr) != NULL) ? arrheader((arr))->len : 0)
#define thinarrsetlen(arr, new_len) (assert((arr) != NULL), (arrheader((arr))->len = new_len))
#define thinarrcap(arr) (assert((arr) != NULL), arrheader((arr))->cap)
#define thinarrpush(arr, ...) (assert((arr) != NULL), array_push((void **)&(arr), sizeof(*(arr))), arr[arrheader((arr))->len++] = (__VA_ARGS__))
#define thinarrpop(arr) (assert((arr) != NULL), assert(arrheader((arr))->len != 0), (arr)[arrheader((arr))->len--])
#define thinarrreserve(__arr, __new_cap) (assert((__arr) != NULL), array_reserve((void **)&(__arr), sizeof(*(__arr)), (__new_cap)))

void *create_array(size_t element_size)
#ifdef MY_ARRAY_IMPL
{
    ArrayListHeader *res =
        malloc(sizeof(ArrayListHeader) + (element_size * INITIAL_CAP));
    if (res == NULL) {
        return NULL;
    }
    memset(res, 0, sizeof(ArrayListHeader) + (element_size * INITIAL_CAP));
    res->len = 0;
    res->cap = INITIAL_CAP;
    return res + 1;
}
#else
    ;
#endif

void array_grow(void **arr, size_t element_size)
#ifdef MY_ARRAY_IMPL
{
    ArrayListHeader *header = arrheader(*arr);
    header->cap *= GROW_FACTOR;
    ArrayListHeader *new_header =
        realloc(header, sizeof(ArrayListHeader) + element_size * header->cap);
    *arr = new_header + 1;
}
#else
    ;
#endif

void array_reserve(void **arr, size_t element_size, size_t new_cap)
#ifdef MY_ARRAY_IMPL
{
	if (arrcap(*arr) < new_cap)
	{
		while (arrcap(*arr) < new_cap)
		{
			array_grow(arr, element_size);
		}
	}
}
#else
;
#endif

void array_push(void **arr, size_t element_size)
#ifdef MY_ARRAY_IMPL
{
    ArrayListHeader *header = arrheader(*arr);
    if (header->len >= header->cap) {
        array_grow(arr, element_size);
    }
}
#else
    ;
#endif

#endif // !MY_THIN_ARRAY_H_
