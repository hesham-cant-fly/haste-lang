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

static struct string get_full_line(const char *content, struct string span)
{
	struct string result = span;

	while (result.chars > content and result.chars[-1] != '\n') {
		result.chars -= 1;
	}

	while (result.chars[result.len] != '\0' and result.chars[result.len] != '\n') {
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
	while (line_start > content and line_start[-1] != '\n') {
		line_start -= 1;
	}
	return display_width(line_start, (int)(pos - line_start), src) + 1;
}

static void report_at_with_src(source_file_id src, const char *kind, const char *start, const char *fmt, va_list args)
{
	const char *content = get_source_file_content(src);
	const int line_no = get_line_number(content, start);
	const int column_no = get_column_number(src, content, start);
	const struct string line = string_to_trimed(get_full_line(content, string(start, 0)));

	const char *path = get_source_file_path(src);
	eprint(ANSI_CODE_BOLD ANSI_CODE_UNDERLINE "{s}:{d}:{d}" ANSI_CODE_RESET ": " ANSI_CODE_BOLD "{s}: " ANSI_CODE_RESET, path, line_no, column_no, kind);
	vsprint(serr, fmt, args);
	eprint("\n");

	int indent = eprint("{d:w5} | ", line_no);
	eprintln("{string}", line);
	int pos = display_width(line.chars, (int)((uintptr_t)start - (uintptr_t)line.chars), src) + indent;

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

void f_report_at_token(const char *kind, struct token token, const char *fmt, ...)
{
	const char *content = get_source_file_content(token.src);
	va_list args;
	va_start(args, fmt);
	report_at_with_src(token.src, kind, content + token.start, fmt, args);
	va_end(args);
}

void f_vreport_at_token(const char *kind, struct token token, const char *fmt, va_list args)
{
	const char *content = get_source_file_content(token.src);
	report_at_with_src(token.src, kind, content + token.start, fmt, args);
}

void f_report_at_location(const char *kind, struct location location, const char *fmt, ...)
{
	const char *content = get_source_file_content(location.src);
	va_list args;
	va_start(args, fmt);
	report_at_with_src(location.src, kind, content + location.start, fmt, args);
	va_end(args);
}

void f_vreport_at_location(const char *kind, struct location location, const char *fmt, va_list args)
{
	const char *content = get_source_file_content(location.src);
	report_at_with_src(location.src, kind, content + location.start, fmt, args);
}
