#ifndef MY_ASSERT_H_
# define MY_ASSERT_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef assert
# undef assert
#endif

#define assert(...) \
	do { \
		if (!(__VA_ARGS__)) { \
			fprintf(stderr, "%s:%d: Assertion Failed: %s\n", __FILE__, __LINE__, #__VA_ARGS__);\
			abort(); \
		} \
	} while (0)

#endif // !MY_ASSERT_H_
