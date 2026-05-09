#include "haste.h"

struct scanner {
	struct Allocator allocator;
	struct token_list tokens;
	source_file_id src;
	const char *start, *current;
	uint32_t line, current_line;
	bool has_error, ended;
};

static void populate_token_value(struct token *tok) {
	char buf[64]; 
	size_t copy_len = tok->len < 63 ? tok->len : 63;
	memcpy(buf, tok->start, copy_len);
	buf[copy_len] = '\0';

    switch (tok->kind) {
	case TK_INT:
		tok->ival = strtoll(buf, NULL, 10);
		break;
		
	case TK_FLOAT:
		tok->fval = strtold(buf, NULL);
		break;
		
	default:
		break;
    }
}

static bool ended(const struct scanner *self)
{
	return self->ended;
}

static uint32_t peek(struct scanner *self)
{
	if (ended(self)) return '\0';
	const uint32_t result = decode_utf8(NULL, self->current);
	if (result == '\0') self->ended = true;
	return result;
}

static uint32_t advance(struct scanner *self)
{
	if (ended(self)) return '\0';
	const uint32_t result = decode_utf8(&self->current, self->current);
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
	if (check(self, ch))
	{
		return advance(self);
	}
	return '\0';
}

static bool matches(struct scanner *self, char *str)
{
	const size_t str_len = strlen(str);
	const bool result = strncmp(self->current, str, str_len) == 0;
	if (!result) return false;

	for (size_t i=0; i<str_len; i+=1) {
		advance(self);
	}

	return true;
}

static bool matches_any(struct scanner *self, char *str)
{
	const size_t str_len = strlen(str);
	for (size_t i=0; i < str_len; i += 1)
	{
		if (advance_if_eq(self, str[i]))
		{
			return true;
		}
	}
	return false;
}

static void advance_until(struct scanner *self, uint32_t ch)
{
	while (!check(self, ch))
	{
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
	populate_token_value(result);
	return result;
}

static void scan_lexem(struct scanner *self)
{
	const uint32_t ch = peek(self);

	// skip single line comment
	if (matches(self, "//")) {
		advance_until(self, '\n');
		return;
	}

	// skip block comment
	if (matches(self, "/*")) {
		char *q = strstr(self->current, "*/");
		if (!q) f_error_at(self->src, self->current, "unclosed block comment");
		self->current = q + 2;
		return;
	}

	// numbers (float and integers)
	if (matches_any(self, "0123456789")) {
		advance_all(self, "0123456789");
		const bool is_float = advance_if_eq(self, '.');
		if (is_float) advance_all(self, "0123456789");
		add_token(self, is_float ? TK_FLOAT : TK_INT);
		return;
	}

	const struct { const char *str; enum token_kind kind; } keywords[] = {
		{"int", TK_KW_INT},
		{"float", TK_KW_FLOAT},
		{"void", TK_KW_VOID},
		{"auto", TK_KW_AUTO},
		{"type", TK_KW_TYPE},
		{"cast", TK_KW_CAST},
		{"const", TK_KW_CONST},
		{"var", TK_KW_VAR},
	};
	for (size_t i=0; i<sizeof(keywords)/sizeof(keywords[0]); i += 1) {
		if (matches(self, (char*)keywords[i].str)) {
			add_token(self, keywords[i].kind);
			return;
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

	if (advance_if_eq(self, ';')) {
		add_token(self, TK_SEMI_COLON);
		return;
	}

	if (advance_if_eq(self, '[')) {
		add_token(self, TK_OPEN_BRAKET);
		return;
	}

	if (advance_if_eq(self, ']')) {
		add_token(self, TK_CLOSE_BRAKET);
		return;
	}

	if (advance_if_eq(self, '(')) {
		add_token(self, TK_OPEN_PAREN);
		return;
	}

	if (advance_if_eq(self, ')')) {
		add_token(self, TK_CLOSE_PAREN);
		return;
	}

	if (advance_if_eq(self, ':')) {
		add_token(self, TK_COLON);
		return;
	}

	if (advance_if_eq(self, '=')) {
		add_token(self, TK_EQ);
		return;
	}

	if (advance_if_eq(self, '+')) {
		add_token(self, TK_PLUS);
		return;
	}

	if (advance_if_eq(self, '-')) {
		add_token(self, TK_MINUS);
		return;
	}

	if (advance_if_eq(self, '*')) {
		add_token(self, TK_STAR);
		return;
	}

	if (advance_if_eq(self, '/')) {
		add_token(self, TK_FSLASH);
		return;
	}

	if (ch == '\0') return;

	f_error_at(self->src, self->current, "invalid character: '%lc'", ch);
}

static void start_scanning(struct scanner *self)
{
	while (!ended(self)) {
		advance_all(self, " \n\t\r");

		self->line = self->current_line;
		self->start = self->current;
		scan_lexem(self);
	}
}

Error scan_entire_file(
	struct Allocator allocator,
	const source_file_id src,
	struct token_list *out)
{
	const char *content = get_source_file_content(src);
	struct scanner scanner = {
		.allocator = allocator,
		.tokens = {0},
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
