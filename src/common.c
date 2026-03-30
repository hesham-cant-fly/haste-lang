#include "common.h"
#include "error.h"
#include "token.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

error get_full_path(const char* path, char** out)
{
	*out = realpath(path, NULL);
	if (out == NULL) return ERROR;
	return OK;
}

bool operator_is_unary(enum Operator operator)
{
	if (operator == OPERATOR_NEGATE) return true;
	return false;
}

bool operator_is_binary(enum Operator operator)
{
	return !operator_is_unary(operator);
}

void report(
	FILE* f,
	const char* path,
	const struct Span span,
	const struct Location location,
	const char* kind,
	const char* fmt, ...
) {
	va_list args;
	va_start(args, fmt);
	vreport(f, path, span, location, kind, fmt, args);
	va_end(args);
}

static struct Span get_full_line(const struct Span span)
{
	char *start = span.start;
	char *end = span.end;

	while (start[-1] != '\n' && start[-1] != '\0') {
		start -= 1;
	}

	while (end[0] != '\n' && end[0] != '\0') {
		end += 1;
	}

	return (struct Span) { start, end };
}

void vreport(
	FILE* f,
	const char* path,
	const struct Span span,
	const struct Location location,
	const char* kind,
	const char* fmt,
	va_list args
) {
	fprintf(f, "%s:%u:%u: %s: ", path, location.line, location.column, kind);
	vfprintf(f, fmt, args);
	fprintf(f, "\n");

	const struct Span full_line = get_full_line(span);
	fprintf(f, " %6u | %.*s\n", location.line, SPAN_ARG(full_line));

	size_t caret_pos = (size_t)(location.column - 1);
	size_t line_len = (size_t)(full_line.end - full_line.start);
	if (caret_pos > line_len) caret_pos = line_len;
	fprintf(f, "        | %*s^\n", (int)caret_pos, "");
}
