#include "haste.h"
#include "my_termcolor.h"

struct scanner {
	struct Allocator allocator;
	struct intern_table *table;
	struct token_list tokens;
	source_file_id src;
	const char *start, *current;
	uint32_t line, current_line;
	bool has_error, ended;
};

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

static void populate_token_value(struct scanner *self, struct token *tok) {
    switch (tok->kind) {
	case TK_INT: {
		char *buf = tsprint("{s:*}", tok->start, (int)tok->len);
		tok->ival = strtoll(buf, NULL, 10);
	} break;
	case TK_FLOAT: {
		char *buf = tsprint("{s:*}", tok->start, (int)tok->len);
		tok->fval = strtold(buf, NULL);
	} break;
	case TK_IDENT: {
		tok->ident = intern_str(self->table, tok->start, tok->len);
	} break;
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
	const size_t remaining = get_source_file_end(self->src) - self->current;
	const size_t str_len = strlen(str);
	if (str_len > remaining) return false;

	if (strncmp(self->current, str, str_len) != 0)
		return false;

	for (size_t i=0; i<str_len; i+=1) {
		advance(self);
	}

	return true;
}

static bool matches_any(struct scanner *self, char *str)
{
	const size_t str_len = strlen(str);
	for (size_t i=0; i < str_len; i += 1) {
		if (advance_if_eq(self, str[i])) {
			return true;
		}
	}
	return false;
}

static void advance_until(struct scanner *self, uint32_t ch)
{
	while (not check(self, ch)) {
		advance(self);
	}
}

static void advance_all(struct scanner *self, char *str)
{
	while (matches_any(self, str));
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
	f_vreport_at(self->src, ANSI_CODE_RED "Error", at, fmt, args);
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
	if (matches_any(self, "0123456789")) {
		advance_all(self, "0123456789");
		const bool is_float = advance_if_eq(self, '.');
		if (is_float) advance_all(self, "0123456789");
		add_token(self, is_float then TK_FLOAT otherwise TK_INT);
		return;
	}

	// NOTE: No need for an optimization
	const struct { const char *str; enum token_kind kind; } keywords[] = {
		{"string", TK_KW_STRING},
		{"cstr",   TK_KW_CSTR},
		{"int",    TK_KW_INT},
		{"float",  TK_KW_FLOAT},
		{"void",   TK_KW_VOID},
		{"auto",   TK_KW_AUTO},
		{"type",   TK_KW_TYPE},
		{"cast",   TK_KW_CAST},
		{"const",  TK_KW_CONST},
		{"var",    TK_KW_VAR},
		{"struct", TK_KW_STRUCT},
	};
	for (size_t i=0; i<sizeof(keywords)/sizeof(keywords[0]); i += 1) {
		const char *saved_current = self->current;
		const bool saved_ended = self->ended;
		if (matches(self, (char*)keywords[i].str)) {
			const uint32_t next = decode_utf8(NULL, self->current, self->src);
			if (not is_ident2(next)) {
				add_token(self, keywords[i].kind);
				return;
			}
			self->current = saved_current;
			self->ended = saved_ended;
		}
	}

	if (matches_any(self, "abcdefghijklmnopqrstuvwxyz"
	                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                      "$_")) {
		advance_all(self, "abcdefghijklmnopqrstuvwxyz"
	                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						  "0123456789"
	                      "$_");
		add_token(self, TK_IDENT);
		return;
	}

	{
		static const struct { char ch; enum token_kind kind; } single_char_tokens[] = {
			{';', TK_SEMI_COLON},
			{'[', TK_OPEN_BRAKET},
			{']', TK_CLOSE_BRAKET},
			{'(', TK_OPEN_PAREN},
			{')', TK_CLOSE_PAREN},
			{':', TK_COLON},
			{'=', TK_EQ},
			{'+', TK_PLUS},
			{'-', TK_MINUS},
			{'*', TK_STAR},
			{'/', TK_FSLASH},
			{'{', TK_OPEN_BRACE},
			{'}', TK_CLOSE_BRACE},
			{',', TK_COMMA},
			{'.', TK_DOT},
		};
		for (size_t i = 0; i < sizeof(single_char_tokens)/sizeof(single_char_tokens[0]); i += 1) {
			if (advance_if_eq(self, single_char_tokens[i].ch)) {
				add_token(self, single_char_tokens[i].kind);
				return;
			}
		}
	}

	if (advance_if_eq(self, '"')) {
		self->start = self->current - 1;
		scan_string(self);
		add_token(self, TK_STR);
		return;
	}

	const char *pos = self->current;
	const uint32_t ch = advance(self);
	if (ch == '\0') return;

	report_error(self, pos, "invalid character: '{lc}'", ch);
}

static void start_scanning(struct scanner *self)
{
	while (not ended(self)) {
		advance_all(self, " \n\t\r");

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
		.line = 1,
		.current_line = 1,
	};

	start_scanning(&scanner);
	if (scanner.has_error) {
		arrfree(scanner.allocator, scanner.tokens);
		return ERROR;
	}

	struct token *token = add_token(&scanner, TK_EOF);
	token->len = 0;

	*out = scanner.tokens;

	return OK;
}
