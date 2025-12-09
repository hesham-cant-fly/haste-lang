#ifndef __MY_COMMONS_H
#define __MY_COMMONS_H

/* #define MY_COMMONS_IMPLEMENTATION */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <signal.h>

#ifdef RELEASE
# define NORETURN
#endif
#ifndef NORETURN
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define NORETURN _Noreturn
# elif defined(__GNUC__) || defined(__clang__)
#  define NORETURN __attribute__((noreturn))
# elif defined(_MSC_VER)
#  define NORETURN __declspec(noreturn)
# else
#  define NORETURN
# endif
#endif

#define BOOL_ARG(__expr) (__expr) ? "true" : "false"

#define unused(...) ((void)(__VA_ARGS__))
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)
#define unreachable() __unreachable(__FILE__, __LINE__)

NORETURN void __panic(const char *file, int line, const char *fmt, ...);
NORETURN void __unreachable(const char *file, int line);

#ifdef MY_COMMONS_IMPLEMENTATION
NORETURN void __panic(const char *file, int line, const char *fmt, ...)
{
	fprintf(stderr, "%s:%d: Error: Panic: ", file, line);

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, ".\n");

	raise(SIGSEGV);
	exit(69);
}

NORETURN void __unreachable(const char *file, int line) {
  fprintf(stderr, "%s:%d: Error: reached an unreachable.\n", file, line);
  exit(1);
}
#endif /* MY_COMMONS_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__MY_COMMONS_H */
