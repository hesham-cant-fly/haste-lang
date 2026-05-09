#include "haste.h"

static void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsprint(serr, fmt, args);
	eprint("\n");
	Exit(1);
}

static struct span get_full_line(struct span span)
{
	struct span result = span;
	while (result.start[-1] != '\0' && result.start[-1] != '\n') {
		result.start -= 1;
	}

	while (result.start[result.len] != '\0' && result.start[result.len] != '\n') {
		result.len += 1;
	}

	return result;
}

static int get_line_number(const char *str)
{
	int line = 1;
	for (const char *current = str;
		 *current != '\0';
		 current -= 1) {
		if (*current == '\n') {
			line += 1;
		}
	}

	return line;
}

static noreturn void error_at_with_path(const char *path, const char *start, const char *fmt, va_list args)
{
	const int line_no = get_line_number(start);
	const struct span line = span_to_trimed(get_full_line(span(start, 0)));

	int indent = eprint("{s}:{d}: Error: ", path, line_no);
	eprintln("{span}", line);

	int pos = display_width(line.start, (int)((uintptr_t)start - (uintptr_t)line.start)) + indent;

	eprint("{s:w*} ^ ", "", pos - 1);

	vsprint(serr, fmt, args);
	eprint("\n");

	Exit(1);
}

noreturn void f_error_at(const source_file_id src, const char *start, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char *path = get_source_file_path(src);
	error_at_with_path(path, start, fmt, args);
	va_end(args);
}

noreturn void f_error_at_token(const source_file_id src, struct token token, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char *path = get_source_file_path(src);
	error_at_with_path(path, token.start, fmt, args);
	va_end(args);
}

noreturn void f_verror_at_token(const source_file_id src, struct token token, const char *fmt, va_list args)
{
	const char *path = get_source_file_path(src);
	error_at_with_path(path, token.start, fmt, args);
}

noreturn void error_at(const char *start, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	error_at_with_path("(?)", start, fmt, args);
	va_end(args);
}

noreturn void error_at_token(struct token token, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	error_at_with_path("(?)", token.start, fmt, args);
}

noreturn void verror_at_token(struct token token, const char *fmt, va_list args)
{
	error_at_with_path("(?)", token.start, fmt, args);
}
