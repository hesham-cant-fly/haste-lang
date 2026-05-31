#include "haste.h"
#include "my_allocator.h"
#include "my_array.h"
#include "my_stream.h"
#include <stddef.h>
#include <string.h>

struct parser {
	struct Allocator allocator;
	struct token_stream stream;
	// source_file_id src;
	struct token previous;
	bool has_error;
};

enum precedence {
	PREC_NONE,
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
	PREC_ACCESS,     // .
	PREC_UNARY,      // - + cast
	PREC_PRIMARY    
};

typedef struct haste_ast_node *(*ParsePrefixFn)(struct parser *);
typedef struct haste_ast_node *(*ParseInfixFn)(struct parser *, struct haste_ast_node *lhs);

struct parser_rule {
	ParsePrefixFn prefix;
	ParseInfixFn infix;
	enum precedence precedence;
	bool right_assoc;
};

#define create_node(self_, T_, ...) \
	(void*)_create_node((self_), &(T_) { __VA_ARGS__ }, sizeof(T_))

inline static struct haste_ast_node *_create_node(struct parser *self, void *value, size_t size)
{
	void *result = alloc(self->allocator, size);
	memcpy(result, value, size);
	return result;
}

static struct parser_rule get_rule(struct token token);
static struct haste_ast_node *expr(struct parser *self);
static struct haste_ast_node *parse_precedence(struct parser *self, enum precedence prec);

static struct token peek(struct parser *self);

static bool ended(struct parser *self)
{
	return peek(self).kind == TK_EOF or token_stream_ended(&self->stream);
}

static struct token peek(struct parser *self)
{
	return token_stream_peek(&self->stream);
}

static struct token peek_next(struct parser *self)
{
	/* if (self->current + 1 >= self->tokens.len) return get_eof(self); */
	/* return self->tokens.items[self->current + 1]; */
	return token_stream_peek_next(&self->stream);
}

static struct token advance(struct parser *self)
{
	if (ended(self)) return (struct token){0};
	self->previous = peek(self);
	token_stream_advance(&self->stream);
	return self->previous;
}

static struct token previous(const struct parser *self)
{
	return self->previous;
}

static void report_error_at(struct parser *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token("Error", token, fmt, args);
	va_end(args);
	self->has_error = true;
	exit(1);
}

static void report_error(struct parser *self, const char *restrict fmt, ...)
{
	struct token token = peek(self);
	va_list args; va_start(args, fmt);
	f_vreport_at_token("Error", token, fmt, args);
	va_end(args);
	self->has_error = true;
	exit(1);
}

static void vreport_error(struct parser *self, const char *restrict fmt, va_list args)
{
	struct token token = peek(self);
	f_vreport_at_token("Error", token, fmt, args);
	self->has_error = true;
	exit(1);
}

#define check(_self, ...) _check((_self), __VA_ARGS__, (enum token_kind){0})
static bool _check(struct parser *self, ...)
{
	if (ended(self)) return false;
	va_list args;
	va_start(args, self);

	for (;;) {
		const enum token_kind kind = va_arg(args, int);
		if (kind == 0) break;
		if (peek(self).kind == kind) {
			va_end(args);
			return true;
		}
	}

	va_end(args);

	return false;
}

#define match(_self, ...) _match((_self), __VA_ARGS__, (enum token_kind){0})
static bool _match(struct parser *self, ...)
{
	va_list args;
	va_start(args, self);
	for (;;) {
		enum token_kind kind = va_arg(args, enum token_kind);
		if (kind == 0) break;
		if (check(self, kind)) {
			va_end(args);
			advance(self);
			return true;
		}
	}
	va_end(args);

	return false;
}

static struct token consume(struct parser *self, enum token_kind kind, const char *fmt, ...)
{
	if (match(self, kind))
	{
		return previous(self);
	}

	va_list args;
	va_start(args, fmt);
	vreport_error(self, fmt, args);
	va_end(args);
	exit(1);
}

struct haste_ast_node *binary(struct parser *self, struct haste_ast_node *lhs)
{
	struct location start = lhs->location;
	struct token op = previous(self);

	struct parser_rule rule = get_rule(op);
	enum precedence precedence = rule.precedence + 1;
	if (rule.right_assoc) {
		precedence = rule.precedence;
	}

	struct haste_ast_node *rhs = parse_precedence(self, precedence);
	struct location end = as_location(previous(self));
	return (void*) create_node(
		self,
		struct haste_ast_binary,
		.base.kind = ND_BINARY,
		.base.location = location_conjoin(start, end),
		.lhs = lhs,
		.rhs = rhs,
		.op = op);
}

struct haste_ast_node *field_access(struct parser *self, struct haste_ast_node *lhs)
{
	struct location start = lhs->location;
	struct token token = peek(self);
	if (not _match(self, TK_IDENT)) {
		report_error(self, "Expected a name. got '{token}' instead.", token);
	}

	struct location end = as_location(token);
	return create_node(self,
		struct haste_ast_access,
		.base.kind = ND_ACCESS,
		.base.location = location_conjoin(start, end),
		.lhs = lhs,
		.rhs = token);
}

static struct haste_ast_node *unary(struct parser *self)
{
	struct location start = as_location(previous(self));
	struct token op	= previous(self);
	struct haste_ast_node *rhs = parse_precedence(self, PREC_UNARY);
	struct location end = as_location(previous(self));
	return create_node(
		self,
		struct haste_ast_unary,
		.base.kind = ND_UNARY,
		.base.location = location_conjoin(start, end),
		.op = op,
		.rhs = rhs);
}

static struct haste_ast_node *cast(struct parser *self)
{
	struct location start = as_location(previous(self));
	struct haste_ast_node *to = NULL;
	if (match(self, TK_OPEN_BRAKET)) {
		if (not match(self, TK_CLOSE_BRAKET)) {
			to = expr(self);
			consume(self, TK_CLOSE_BRAKET, "Expected closing braket ']'.");
		}
	}

	struct haste_ast_node *child_expr = parse_precedence(self, PREC_UNARY);
	return create_node(
		self,
		struct haste_ast_cast,
		.base.kind = ND_CAST,
		.base.location = location_conjoin(start, child_expr->location),
		.to = to,
		.expr = child_expr);
}

static struct haste_ast_node *distinct(struct parser *self)
{
	struct location start = as_location(previous(self));
	struct haste_ast_node *child = parse_precedence(self, PREC_UNARY);
	struct location end = as_location(previous(self));
	return create_node(
		self,
		struct haste_ast_distinct,
		.base.kind = ND_DISTINCT,
		.base.location = location_conjoin(start, end),
		.child = child);
}

static struct haste_ast_node *primary(struct parser *self)
{
	struct token lit = previous(self);
	return create_node(
		self,
		struct haste_ast_primary,
		.base.kind = ND_PRIMARY,
		.base.location = as_location(lit),
		.token = lit);
}

static struct haste_ast_node *grouping(struct parser *self)
{
	struct location start = as_location(previous(self));
	struct haste_ast_node *child = expr(self);
	struct location end = as_location(consume(self, TK_CLOSE_PAREN, "Expected ')', got '{token}' instead.", peek(self)));
	return create_node(
		self,
		struct haste_ast_grouping,
		.base.kind = ND_GROUPING,
		.base.location = location_conjoin(start, end),
		.child = child);
}

// ── Struct parsing ────────────────────────────────────────────────

static struct haste_ast_node *struct_type_prefix(struct parser *self)
{
	const struct location start = as_location(previous(self));
	consume(self, TK_OPEN_BRACE, "Expected '{' after 'struct'.");

	struct haste_ast_struct_field head = {0};
	struct haste_ast_struct_field *current = &head;
	while (not check(self, TK_CLOSE_BRACE) and not ended(self)) {
		const struct location start = as_location(peek(self));
		struct token_list names = {0};
		arrpush(self->allocator, names, consume(self, TK_IDENT, "Expected field name."));
		while (match(self, TK_COMMA)) {
			arrpush(self->allocator, names, consume(self, TK_IDENT, "Expected field name."));
		}

		consume(self, TK_COLON, "Expected ':' after field name.");
		struct haste_ast_node *type = NULL;
		struct haste_ast_node *default_value = NULL;
		if (match(self, TK_EQ)) {
			default_value = expr(self);
		} else {
			type = expr(self);
			if (match(self, TK_EQ)) default_value = expr(self);
		}
		const struct location end = as_location(consume(self, TK_SEMI_COLON, "Expected ';' after field declaration."));

		current->next = create_node(
			self,
			struct haste_ast_struct_field,
			.base.location = location_conjoin(start, end),
			.name_count = names.len,
			.names = names.items,
			.type = type,
			.default_value = default_value);
		current = current->next;
	}

	const struct location end = as_location(consume(self, TK_CLOSE_BRACE, "Expected '}' after struct fields."));
	return create_node(
		self,
		struct haste_ast_struct_type,
		.base.kind = ND_STRUCT_TYPE,
		.base.location = location_conjoin(start, end),
		.fields = (void*)head.next);
}

static struct haste_ast_node *struct_literal_infix(struct parser *self, struct haste_ast_node *type_expr)
{
	struct location start = type_expr != NULL then type_expr->location otherwise as_location(previous(self));
	struct haste_ast_struct_lit_field head = {0};
	struct haste_ast_struct_lit_field *current = &head;

	while (not check(self, TK_CLOSE_BRACE) and not ended(self)) {
		const struct location start = as_location(peek(self));
		struct token name = {0};
		struct haste_ast_node *value = NULL;

		if (peek(self).kind == TK_IDENT and peek_next(self).kind == TK_COLON) {
			name = consume(self, TK_IDENT, "Expected field name.");
			consume(self, TK_COLON, "Expected ':' after field name.");
			value = expr(self);
		} else {
			value = expr(self);
		}

		if (check(self, TK_COMMA)) advance(self);
		else if (not check(self, TK_CLOSE_BRACE))
			consume(self, TK_COMMA, "Expected ',' or '}' after field value.");

		const struct location end = as_location(previous(self));
		current->next = create_node(
			self,
			struct haste_ast_struct_lit_field,
			.base.kind = ND_STRUCT_LIT_FIELD,
			.base.location = location_conjoin(start, end),
			.name = name,
			.value = value);
		current = current->next;
	}

	const struct location end = as_location(consume(self, TK_CLOSE_BRACE, "Expected '}' after struct literal fields."));
	return create_node(
		self,
		struct haste_ast_struct_literal,
		.base.kind = ND_STRUCT_LITERAL,
		.base.location = location_conjoin(start, end),
		.type_expr = type_expr,
		.fields = (void*)head.next);
}

static struct haste_ast_node *auto_struct_prefix(struct parser *self)
{
	consume(self, TK_OPEN_BRACE, "Expected '{' after '.'.");
	return struct_literal_infix(self, NULL);
}

struct parser_rule get_rule_from_kind(enum token_kind kind)
{
	switch (kind) {
	case TK_OPEN_PAREN:   return (struct parser_rule){ grouping,           NULL,                 PREC_PRIMARY, false };
	case TK_PLUS:         return (struct parser_rule){ unary,              binary,               PREC_TERM,    false };
	case TK_MINUS:        return (struct parser_rule){ unary,              binary,               PREC_TERM,    false };
	case TK_STAR:         return (struct parser_rule){ NULL,               binary,               PREC_FACTOR,  false };
	case TK_FSLASH:       return (struct parser_rule){ NULL,               binary,               PREC_FACTOR,  false };
	case TK_INT:          return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_FLOAT:        return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_STR:          return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_IDENT:        return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_DISTINCT:  return (struct parser_rule){ distinct,           NULL,                 PREC_UNARY,   false };
	case TK_KW_STRING:    return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_CSTR:      return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_INT:       return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_UINT:      return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_INT_BITS:  return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_UINT_BITS: return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_FLOAT:     return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_VOID:      return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_AUTO:      return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_TYPE:      return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_STRUCT:    return (struct parser_rule){ struct_type_prefix, NULL,                 PREC_PRIMARY, false };
	case TK_KW_USIZE:     return (struct parser_rule){ primary,            NULL,                 PREC_PRIMARY, false };
	case TK_KW_CAST:      return (struct parser_rule){ cast,               NULL,                 PREC_UNARY,   false };
	case TK_OPEN_BRACE:   return (struct parser_rule){ NULL,               struct_literal_infix, PREC_PRIMARY, false };
	case TK_DOT:          return (struct parser_rule){ auto_struct_prefix, field_access,         PREC_PRIMARY, false };
	default:              return (struct parser_rule){ NULL,               NULL,                 PREC_NONE,    false };
	}
}

static struct parser_rule get_rule(struct token token)
{
	return get_rule_from_kind(token.kind);
}

static struct haste_ast_node *parse_precedence(struct parser *self, enum precedence precedence)
{
	if (ended(self)) {
		report_error(self, "Incomplete expression.");
	}

	struct token token = advance(self);
	ParsePrefixFn prefix_rule = get_rule(token).prefix;
	if (prefix_rule == NULL) {
		report_error_at(
			self,
			token,
			"Expected an expression, got `{token}` instead.",
			token);
	}

	struct haste_ast_node *left = prefix_rule(self);

	struct parser_rule rule;
	for (rule = get_rule(peek(self));
		 (not ended(self)) and precedence <= rule.precedence;
		 rule  = get_rule(peek(self))) {
		struct token tok = advance(self);
		ParseInfixFn infix_rule = get_rule(tok).infix;
		if (infix_rule == NULL) {
			run_at_percent (3) {
				struct string str = as_string(left->location);
				if (strncmp(str.chars, "cat", left->location.len) == 0) {
					report_error_at(self, tok,
									"meow! sorry, but purrs of a cat not gonna write useful software :(.", tok);
				} else {
					report_error_at(self, tok, "`{token}` is not a valid operator.", tok);
				}
			} else {
				report_error_at(self, tok, "`{token}` is not a valid operator.", tok);
			}
		}

		left = infix_rule(self, left);
		if (left == NULL) return NULL;
	}

	return left;
}

static struct haste_ast_node *expr(struct parser *self)
{
	return parse_precedence(self, PREC_TERM);
}

static struct haste_ast_node *variable_decl(struct parser *self, bool is_constant)
{
	const struct location start = as_location(previous(self));
	struct token name = consume(self, TK_IDENT, "expected a variable name.");

	struct haste_ast_node *type = NULL;
	struct haste_ast_node *value = NULL;
	if (match(self, TK_COLON)) {
		if (match(self, TK_EQ)) {
			value = expr(self);
		} else {
			type = expr(self);
		}
	}

	if (value == NULL and match(self, TK_EQ)) value = expr(self);

	const struct location end = as_location(consume(self, TK_SEMI_COLON, "Expected ';' at the end of the variable declaration."));
	return create_node(
		self,
		struct haste_ast_var_decl,
		.base.kind = ND_VAR_DECL,
		.base.location = location_conjoin(start, end),
		.is_constant = is_constant,
		.name        = name,
		.type        = type,
		.value       = value);
}

static struct haste_ast_node *decl(struct parser *self, const bool error_on_unexpected)
{
	struct token token = peek(self);
	if (match(self, TK_KW_CONST, TK_KW_VAR)) {
		struct haste_ast_var_decl *node = (void*)variable_decl(self, token.kind == TK_KW_CONST);
		node->is_global = true;
		return (void*)node;
	}

	if (not error_on_unexpected) return NULL;

	if (ended(self)) {
		report_error_at(self, token,
			"Unexpected end of file. expected 'const', 'var' or 'func'.");
	}

	return NULL;
}

Error parse(struct Allocator allocator, const source_file_id src)
{
	struct parser parser = {
		.allocator = allocator,
		.stream = token_stream(src),
	};

	struct haste_ast_node head = {0};
	struct haste_ast_node *current = &head;
	while (not ended(&parser)) {
		struct haste_ast_node *node = decl(&parser, true);
		current->next = node;
		current = current->next;
	}

	sources.items[src].root = head.next;

	return OK;
}
