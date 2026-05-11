#include "haste.h"

static void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsprint(serr, fmt, args);
	eprint("\n");
	Exit(1);
}

static struct span get_full_line(const char *content, struct span span)
{
	struct span result = span;

	while (result.start > content && result.start[-1] != '\n') {
		result.start -= 1;
	}

	while (result.start[result.len] != '\0' && result.start[result.len] != '\n') {
		result.len += 1;
	}

	return result;
}

static int get_line_number(const char *content, const char *pos)
{
	int line = 1;
	for (const char *cur = pos; cur > content; cur -= 1) {
		if (*cur == '\n') {
			line += 1;
		}
	}

	return line;
}

static noreturn void error_at_with_src(source_file_id src, const char *start, const char *fmt, va_list args)
{
	const char *content = get_source_file_content(src);
	const int line_no = get_line_number(content, start);
	const struct span line = span_to_trimed(get_full_line(content, span(start, 0)));

	const char *path = get_source_file_path(src);
	int indent = eprint("{s}:{d}: Error: ", path, line_no);
	eprintln("{span}", line);

	int pos = display_width(line.start, (int)((uintptr_t)start - (uintptr_t)line.start), src) + indent;

	eprint("{s:w*} ^ ", "", pos - 1);

	vsprint(serr, fmt, args);
	eprint("\n");

	Exit(1);
}

noreturn void f_error_at(const source_file_id src, const char *start, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	error_at_with_src(src, start, fmt, args);
	va_end(args);
}

noreturn void f_error_at_token(const source_file_id src, struct token token, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	error_at_with_src(src, token.start, fmt, args);
	va_end(args);
}

noreturn void f_verror_at_token(const source_file_id src, struct token token, const char *fmt, va_list args)
{
	error_at_with_src(src, token.start, fmt, args);
}


