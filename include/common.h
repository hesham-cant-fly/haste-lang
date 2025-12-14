#ifndef COMMON_H_
#define COMMON_H_

#include "error.h"
#include "token.h"
#include <stdio.h>

error get_full_path(const char* path, char** out);

void report(
	FILE* f,
	const char* path,
	const Location location,
	const char* kind,
	const char* fmt,
	...
);

void vreport(
	FILE* f,
	const char* path,
	const Location location,
	const char* kind,
	const char* fmt,
	va_list args
);

#endif // !COMMON_H_
