#ifndef __ARRAY_H
#define __ARRAY_H

// #define MY_ARRAY_IMPL

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ArrayListHeader {
  size_t len;
  size_t cap;
} ArrayListHeader;

#ifndef INITIAL_CAP
#define INITIAL_CAP 20
#endif
#ifndef GROW_FACTOR
#define GROW_FACTOR 2
#endif

#define arrinit(T) (T *)(create_array(sizeof(T)))
#define arrfree(arr) (((arr) != NULL) ? (free(arrheader(arr)), 0) : 0)
#define arrheader(arr)                                                         \
  (((arr) != NULL)                                                             \
       ? (ArrayListHeader *)(((uint8_t *)(arr)) - sizeof(ArrayListHeader))     \
       : (0))
#define arrlen(arr) (((arr) != NULL) ? arrheader(arr)->len : 0)
#define arrsetlen(arr, new_len)                                                \
  (((arr) != NULL) ? (arrheader(arr)->len = new_len) : 0)
#define arrcap(arr) (((arr) != NULL) ? arrheader(arr)->cap : 0)
#define arrpush(arr, el)                                                       \
  (((arr) != NULL) ? (array_push((void **)&arr, sizeof(typeof(*arr))),         \
                      arr[arrheader(arr)->len++] = el)                         \
                   : el)
#define arrpop(arr)                                                            \
  (((arr) != NULL) ? arr[arrheader(arr)->len == 0 ? 0 : arrheader(arr)->len--] \
                   : (typeof(*arr))0)
#define arrpopsome(arr, x)                                                     \
  (((arr) != NULL) ? (((x) <= arrlen(arr)) ? (arrheader(arr)->len -= x) : 0)   \
                   : 0)

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

#endif // __ARRAY_H
