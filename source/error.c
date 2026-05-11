#include "haste.h"
#include "my_termcolor.h"

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

static int get_column_number(source_file_id src, const char *content, const char *pos)
{
	const char *line_start = pos;
	while (line_start > content && line_start[-1] != '\n') {
		line_start -= 1;
	}
	return display_width(line_start, (int)(pos - line_start), src) + 1;
}

static void report_at_with_src(source_file_id src, const char *kind, const char *start, const char *fmt, va_list args)
{
	const char *content = get_source_file_content(src);
	const int line_no = get_line_number(content, start);
	const int column_no = get_column_number(src, content, start);
	const struct span line = span_to_trimed(get_full_line(content, span(start, 0)));

	const char *path = get_source_file_path(src);
	eprint(ANSI_CODE_BOLD ANSI_CODE_UNDERLINE "{s}:{d}:{d}" ANSI_CODE_RESET ": " ANSI_CODE_BOLD "{s}: " ANSI_CODE_RESET, path, line_no, column_no, kind);
	vsprint(serr, fmt, args);
	eprint("\n");

	int indent = eprint("{d:w5} | ", line_no);
	eprintln("{span}", line);
	int pos = display_width(line.start, (int)((uintptr_t)start - (uintptr_t)line.start), src) + indent;

	eprint("{s:w*}^ ", "", pos, pos);
	eprint("\n");
}

void f_report_at(const source_file_id src, const char *kind, const char *start, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	report_at_with_src(src, kind, start, fmt, args);
	va_end(args);
}

void f_vreport_at(const source_file_id src, const char *kind, const char *start, const char *fmt, va_list args)
{
	report_at_with_src(src, kind, start, fmt, args);
}

void f_report_at_token(const source_file_id src, const char *kind, struct token token, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	report_at_with_src(src, kind, token.start, fmt, args);
	va_end(args);
}

void f_vreport_at_token(const source_file_id src, const char *kind, struct token token, const char *fmt, va_list args)
{
	report_at_with_src(src, kind, token.start, fmt, args);
}
