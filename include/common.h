#ifndef COMMON_H_
#define COMMON_H_

#include "error.h"
#include "token.h"
#include <stdio.h>

// NOTE: Some WILL judge me on this but I did this because I can't type '>'
//       for some reason. I can type '<' but I can't type '>' which requires pressing on SHIFT+<
//       to type. UPDATE: now other characters doesn't work
#define gt >
#define get ->
#define or ||
#define and &&
#define not !

error get_full_path(const char* path, char** out);

enum Operator {
	OPERATOR_ADD,
	OPERATOR_SUB,
	OPERATOR_MUL,
	OPERATOR_DIV,

	OPERATOR_NEGATE,
};

bool operator_is_unary(enum Operator operator);
bool operator_is_binary(enum Operator operator);

void report(
	FILE* f,
	const char* path,
	const struct Span span,
	const struct Location location,
	const char* kind,
	const char* fmt,
	...
);

void vreport(
	FILE* f,
	const char* path,
	const struct Span span,
	const struct Location location,
	const char* kind,
	const char* fmt,
	va_list args
);

#endif // !COMMON_H_
