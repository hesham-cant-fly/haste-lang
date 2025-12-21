#include "common.h"
#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

error get_full_path(const char* path, char** out)
{
	*out = realpath(path, NULL);
	if (out == NULL) return ERROR;
	return OK;
}

void report(
	FILE* f,
	const char* path,
	const struct Location location,
	const char* kind,
	const char* fmt, ...
) {
	va_list args;
	va_start(args, fmt);
	vreport(f, path, location, kind, fmt, args);
	va_end(args);
}

void vreport(
	FILE* f,
	const char* path,
	const struct Location location,
	const char* kind,
	const char* fmt,
	va_list args
) {
	fprintf(f, "%s:%u:%u: %s: ", path, location.line, location.column, kind);
	vfprintf(f, fmt, args);
	fprintf(f, "\n");
}
