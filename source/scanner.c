#include "haste.h"
#include "my_stream.h"
#include "my_termcolor.h"
#include <string.h>

struct scanner {
	struct Allocator allocator;
	struct intern_table *table;
	struct token_list tokens;
	source_file_id src;
	const char *start, *current, *end;
	uint32_t line, current_line;
	bool has_error, ended;
};

static bool is_digit(uint32_t c) { return c >= '0' and c <= '9'; }

static const char *decode_string(struct scanner *self, const char *start, size_t len)
{
	char *chars = alloc(self->allocator, len + 1);
	size_t j = 0;
	for (size_t i = 0; i < len; i += 1) {
		if (start[i] == '\\' and i + 1 < len) {
			i += 1;
			switch (start[i]) {
			case 'n':  chars[j++] = '\n'; break;
			case 't':  chars[j++] = '\t'; break;
			case 'r':  chars[j++] = '\r'; break;
			case '0':  chars[j++] = '\0'; break;
			case '\\': chars[j++] = '\\'; break;
			case '"':  chars[j++] = '"';  break;
			default:   chars[j++] = start[i]; break;
			}
		} else {
			chars[j++] = start[i];
		}
	}
	chars[j] = '\0';

	const char *result = intern_cstr(self->table, chars);
	destroy(self->allocator, chars);
	return result;
}

static void populate_token_value(struct scanner *self, struct token *tok)
{
    switch (tok->kind) {
	case TK_INT: {
		char *buf = tsprint("{s:*}", tok->start, (int)tok->len);
		tok->ival = strtoll(buf, NULL, 10);
	} break;
	case TK_FLOAT: {
		char *buf = tsprint("{s:*}", tok->start, (int)tok->len);
		tok->fval = strtold(buf, NULL);
	} break;
	case TK_IDENT:
		if (tok->len > 3 and strncmp(tok->start, "int", 3) == 0) {
			const char *rest = tok->start + 3;
			size_t rest_len = tok->len - 3;
			bool all_digits = true;
			for (size_t i = 0; i < rest_len; i++)
				if (not is_digit((unsigned char)rest[i])) { all_digits = false; break; }
			if (all_digits) {
				tok->kind = TK_KW_INT_BITS;
				tok->ival = strtoll(rest, NULL, 10);
				break;
			}
		} else if (tok->len > 4 and strncmp(tok->start, "uint", 4) == 0) {
			const char *rest = tok->start + 4;
			size_t rest_len = tok->len - 4;
			bool all_digits = true;
			for (size_t i = 0; i < rest_len; i++)
				if (not is_digit((unsigned char)rest[i])) { all_digits = false; break; }
			if (all_digits) {
				tok->kind = TK_KW_UINT_BITS;
				tok->ival = strtoll(rest, NULL, 10);
				break;
			}
		}
		tok->ident = intern_str(self->table, tok->start, tok->len);
		break;
	case TK_STR: {
		tok->str = decode_string(self, tok->start + 1, tok->len - 2);
	} break;
	default:
		break;
    }

	reset_temporary_allocator();
}

static bool ended(const struct scanner *self)
{
	return self->ended;
}

static uint32_t peek(struct scanner *self)
{
	if (ended(self)) return '\0';
	const uint32_t result = decode_utf8(NULL, self->current, self->src);
	if (result == '\0') self->ended = true;
	return result;
}

static uint32_t advance(struct scanner *self)
{
	if (ended(self)) return '\0';
	const uint32_t result = decode_utf8(&self->current, self->current, self->src);
	if (result == '\0') self->ended = true;
	if (result == '\n') self->current_line += 1;
	return result;
}

static bool check(struct scanner *self, uint32_t ch)
{
	if (ended(self)) return false;
	return peek(self) == ch;
}

static uint32_t advance_if_eq(struct scanner *self, uint32_t ch)
{
	if (check(self, ch)) {
		return advance(self);
	}
	return '\0';
}

static bool matches(struct scanner *self, char *str)
{
	const size_t remaining = self->end - self->current;
	const size_t str_len = strlen(str);
	if (str_len > remaining) return false;

	if (strncmp(self->current, str, str_len) != 0)
		return false;

	for (size_t i=0; i<str_len; i+=1) {
		advance(self);
	}

	return true;
}

static void advance_until(struct scanner *self, uint32_t ch)
{
	while (not check(self, ch)) {
		advance(self);
	}
}

static struct token *add_token(struct scanner *self, enum token_kind kind)
{
	arrpush(
		self->allocator,
		self->tokens,
		token(
			kind,
			self->start,
			self->current,
			.line = self->line));
	struct token *result = &self->tokens.items[self->tokens.len - 1];
	populate_token_value(self, result);
	return result;
}

static void report_error(struct scanner *self, const char *at, const char *restrict const fmt, ...)
{
	va_list args; va_start(args, fmt);
	if (self->src >= 0) f_vreport_at(self->src, ANSI_CODE_RED "Error", at, fmt, args);
	va_end(args);
	self->has_error = true;
}

static void report_note(struct scanner *self, const char *at, const char *restrict const fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at(self->src, ANSI_CODE_GREEN "Note", at, fmt, args);
	va_end(args);
}

static void scan_string(struct scanner *self)
{
	while (not ended(self)) {
		uint32_t ch = peek(self);
		if (ch == '"') {
			advance(self);
			return;
		}
		if (ch == '\\') {
			advance(self);
			if (not ended(self)) advance(self);
			continue;
		}
		advance(self);
	}
	report_error(self, self->start, "unterminated string literal");
}

static void skip_whitespace(struct scanner *self)
{
	while (not ended(self)) {
		uint32_t c = peek(self);
		if (c == ' ' or c == '\t' or c == '\r') {
			advance(self);
		} else if (c == '\n') {
			advance(self);
		} else {
			break;
		}
	}
}

static void scan_identifiers(struct scanner *self)
{
	const struct { const char *str; enum token_kind kind; } keywords[] = {
		{"string", TK_KW_STRING},
		{"cstr",   TK_KW_CSTR},
		{"uint",   TK_KW_UINT},
		{"int",    TK_KW_INT},
		{"float",  TK_KW_FLOAT},
		{"void",   TK_KW_VOID},
		{"auto",   TK_KW_AUTO},
		{"type",   TK_KW_TYPE},
		{"cast",   TK_KW_CAST},
		{"const",  TK_KW_CONST},
		{"var",    TK_KW_VAR},
		{"struct", TK_KW_STRUCT},
		{"usize",  TK_KW_USIZE},
		{0}
	};

	const char *start = self->start;
	const char *current = self->current;
	advance(self);
	while (not ended(self) and is_ident2(peek(self))) {
		advance(self);
		current = self->current;
	}

	const size_t lexem_len = (uintptr_t)current - (uintptr_t)start;
	for (size_t i=0; keywords[i].str; i+=1) {
		size_t len = strlen(keywords[i].str);
		if (lexem_len != len) continue;
		if (strncmp(start, keywords[i].str, len) == 0) {
			add_token(self, keywords[i].kind);
			return;
		}
	}

	add_token(self, TK_IDENT);
}

static void scan_lexem(struct scanner *self)
{
	// skip single line comment
	if (matches(self, "//")) {
		advance_until(self, '\n');
		return;
	}

	// skip block comment
	if (matches(self, "/*")) {
		const char *begining = self->current - 2;
		while (not ended(self)) {
			if (matches(self, "*/")) {
				return;
			}
			advance(self);
		}
		report_error(self, self->current - 1, "unclosed block comment");
		report_note(self, begining, "it opened right here");
		return;
	}

	// numbers (float and integers)
	if (not ended(self) and is_digit(peek(self))) {
		advance(self);
		while (not ended(self) and is_digit(peek(self)))
			advance(self);
		const bool is_float = not ended(self) and peek(self) == '.';
		if (is_float) {
			advance(self);
			while (not ended(self) and is_digit(peek(self)))
				advance(self);
		}
		add_token(self, is_float then TK_FLOAT otherwise TK_INT);
		return;
	}

	// identifiers
	if (not ended(self) and is_ident1(peek(self))) {
		scan_identifiers(self);
		return;
	}

	// single-character tokens
	if (not ended(self)) {
		switch (peek(self)) {
		case ';': advance(self); add_token(self, TK_SEMI_COLON); return;
		case '[': advance(self); add_token(self, TK_OPEN_BRAKET); return;
		case ']': advance(self); add_token(self, TK_CLOSE_BRAKET); return;
		case '(': advance(self); add_token(self, TK_OPEN_PAREN); return;
		case ')': advance(self); add_token(self, TK_CLOSE_PAREN); return;
		case ':': advance(self); add_token(self, TK_COLON); return;
		case '=': advance(self); add_token(self, TK_EQ); return;
		case '+': advance(self); add_token(self, TK_PLUS); return;
		case '-': advance(self); add_token(self, TK_MINUS); return;
		case '*': advance(self); add_token(self, TK_STAR); return;
		case '/': advance(self); add_token(self, TK_FSLASH); return;
		case '{': advance(self); add_token(self, TK_OPEN_BRACE); return;
		case '}': advance(self); add_token(self, TK_CLOSE_BRACE); return;
		case ',': advance(self); add_token(self, TK_COMMA); return;
		case '.': advance(self); add_token(self, TK_DOT); return;
		case '"':
			self->start = self->current;
			advance(self);
			scan_string(self);
			add_token(self, TK_STR);
			return;
		}
	}

	const char *pos = self->current;
	const uint32_t ch = advance(self);
	if (ch == '\0') return;

	report_error(self, pos, "invalid character: '{lc}'", ch);
}

static void start_scanning(struct scanner *self)
{
	while (not ended(self)) {
		skip_whitespace(self);

		self->line = self->current_line;
		self->start = self->current;
		scan_lexem(self);
	}
}

Error scan_entire_file(
	struct Allocator allocator,
	struct intern_table *table,
	const source_file_id src,
	struct token_list *out)
{
	const char *content = get_source_file_content(src);
	struct scanner scanner = {
		.allocator = allocator,
		.tokens = {0},
		.table = table,
		.src = src,
		.start = content,
		.current = content,
		.end = get_source_file_end(src),
		.line = 1,
		.current_line = 1,
	};

	start_scanning(&scanner);
	if (scanner.has_error) {
		arrfree(scanner.allocator, scanner.tokens);
		return ERROR;
	}

	scanner.start = scanner.current;
	struct token *token = add_token(&scanner, TK_EOF);
	token->len = 0;

	*out = scanner.tokens;

	return OK;
}

Error scan_entire_string(
	struct Allocator allocator,
	struct intern_table *table,
	const char *content,
	struct token_list *out)
{
	struct scanner scanner = {
		.allocator = allocator,
		.tokens = {0},
		.src = -1,
		.table = table,
		.start = content,
		.current = content,
		.end = content + strlen(content),
		.line = 1,
		.current_line = 1,
	};

	start_scanning(&scanner);
	if (scanner.has_error) {
		arrfree(allocator, scanner.tokens);
		return ERROR;
	}

	scanner.start = scanner.current;
	struct token *tok = add_token(&scanner, TK_EOF);
	tok->len = 0;

	*out = scanner.tokens;

	return OK;
}
