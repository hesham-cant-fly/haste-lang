/**
 * @file my_commons.h
 * @brief Common utility macros and functions.
 *
 * Provides `panic`, `unreachable`, `unimplemented`, and iteration
 * macros for arrays and linked lists.
 *
 * Define `MY_COMMONS_IMPLEMENTATION` in exactly one translation unit
 * to generate the implementation.
 */

#ifndef __MY_COMMONS_H
#define __MY_COMMONS_H

// #define MY_COMMONS_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdnoreturn.h>
#include <stdio.h>

/** @def discard
 *  @brief Suppress unused-variable warnings. */
#define discard (void)
/** @def panic(fmt)
 *  @brief Print file:line and formatted message, then exit with code 69. */
#define panic(__fmt) __panic(__FILE__, __LINE__, (__fmt))
/** @def unreachable()
 *  @brief Print file:line and abort — marks code paths that must never execute. */
#define unreachable() __unreachable(__FILE__, __LINE__)

/**
 * @brief Internal panic function (use the `panic` macro instead).
 * @param file  Source file name.
 * @param line  Source line number.
 * @param fmt   Printf-like format string.
 * @param ...   Format arguments.
 */
void __panic(const char *file, int line, const char *restrict fmt, ...);
/**
 * @brief Internal unreachable handler (use the `unreachable` macro instead).
 */
noreturn void __unreachable(const char *file, int line);

/** @def unimplemented()
 *  @brief Print file:line and exit — marks a stub that is not yet implemented. */
#define unimplemented() \
	do { \
		fprintf(stderr, "%s:%d: Error: this is not implemented yet.\n", __FILE__, __LINE__);\
		exit(1); \
	} while (0)

#define then ?
#define otherwise :

#define or ||
#define and &&
#define not !

/** @def forange(T, name, list)
 *  @brief Iterate from `start_`, incrementing `n_` by 1. until `n_` < `...` is false.
 *  @param n_      Loop variable name (a size_t integer)
 *  @param start_  where it should start.
 *  @param ...     the end. */
#ifndef forange
#  define forange(n_, start_, ...) \
	for (size_t n_ = (start_); (n_) < (__VA_ARGS__); (n_) += 1)
#endif // !forange

/** @def leach(T, name, list)
 *  @brief Iterate over a linked list by following `->next` pointers.
 *  @param T     The node type.
 *  @param name  Loop variable name (a pointer to `T`).
 *  @param ...   Pointer to the head node. */
#define leach(T_, name_, ...) \
	for (T_ *name_ = (__VA_ARGS__); (name_) != NULL; (name_) = (name_)->next)

/** @def iarreach(index, array)
 *  @brief Iterate over an array-like struct by index.
 *  The struct must have a `.len` field of type `size_t`.
 *  @param index  Loop variable name (`size_t`).
 *  @param array  The array struct. */
#ifndef iarreach
#  define iarreach(index__, array__) \
	for (size_t index__ = 0; (index__) < (array__).len; (index__) += 1)
#endif // !iarreach
/** @def arreach(type, var, array)
 *  @brief Iterate over an array-like struct by value.
 *  The struct must have `.items` and `.len` fields.
 *  At each iteration `var` is a copy of the current element.
 *  @param type   Element type.
 *  @param var    Loop variable name.
 *  @param array  The array struct. */
#ifndef arreach
#  define arreach(type_, var_, arr_) \
	for (type_ *var_##_ptr_ = (arr_).items, \
	           *var_##_tmp_ = (void*)1, \
               *var_##_end_ = (arr_).items + (arr_).len; \
         var_##_ptr_ < var_##_end_; \
         var_##_ptr_ += 1, var_##_tmp_ = (void*)1) \
        for (type_ var_ = *var_##_ptr_; var_##_tmp_; var_##_tmp_ = NULL)
#endif // !arreach

#ifdef MY_COMMONS_IMPLEMENTATION
void __panic(const char *file, int line, const char *restrict fmt, ...) {
  fprintf(stderr, "%s:%d: Error: Panic: %s.\n", file, line, fmt);
  exit(69);
}

void __unreachable(const char *file, int line) {
  fprintf(stderr, "%s:%d: Error: reached an unreachable.\n", file, line);
  exit(1);
}
#endif // MY_COMMONS_IMPLEMENTATION

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__MY_COMMONS_H/
