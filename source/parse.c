#include "haste.h"

struct parser {
	struct Allocator allocator;
	struct token_list tokens;
	source_file_id src;
	size_t current;
	bool has_error;
};

enum precedence {
	PREC_NONE,
	PREC_ASSIGNMENT, // = (unimplemented)
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
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

static struct parser_rule get_rule(struct token token);
static struct haste_ast_node *expr(struct parser *self);
static struct haste_ast_node *parse_precedence(struct parser *self, enum precedence prec);

static struct token peek(const struct parser *self);

static bool ended(const struct parser *self)
{
	return peek(self).kind == TK_EOF || self->current >= self->tokens.len;
}

static struct token get_eof(const struct parser *self)
{
	return self->tokens.items[self->tokens.len - 1];
}

static struct token get_pre_eof(const struct parser *self)
{
	return self->tokens.items[self->tokens.len - 2];
}

static struct token peek(const struct parser *self)
{
	return self->tokens.items[self->current];
}

static struct token advance(struct parser *self)
{
	if (ended(self)) return (struct token){0};
	struct token token = peek(self);
	self->current += 1;
	return token;
}

static struct token previous(const struct parser *self)
{
	return self->tokens.items[self->current - 1];
}

static void report_error_at(struct parser *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, "Error", token, fmt, args);
	va_end(args);
	self->has_error = true;
	exit(1);
}

static void report_error(struct parser *self, const char *restrict fmt, ...)
{
	struct token token = ended(self) then get_pre_eof(self) otherwise peek(self);
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, "Error", token, fmt, args);
	va_end(args);
	self->has_error = true;
	exit(1);
}

static void vreport_error(struct parser *self, const char *restrict fmt, va_list args)
{
	struct token token = peek(self);
	f_vreport_at_token(self->src, "Error", token, fmt, args);
	self->has_error = true;
	exit(1);
}

#define check(_self, ...) _check((_self), __VA_ARGS__, (enum token_kind){0})
static bool _check(const struct parser *self, ...)
{
	if (ended(self)) return false;
	va_list args;
	va_start(args, self);

	for (;;) {
		const enum token_kind kind = va_arg(args, enum token_kind);
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
	exit(0);
}

struct haste_ast_node *binary(struct parser *self, struct haste_ast_node *lhs)
{
	struct token op = previous(self);

	struct parser_rule rule = get_rule(op);
	enum precedence precedence = rule.precedence + 1;
	if (rule.right_assoc) {
		precedence = rule.precedence;
	}

	struct haste_ast_node *rhs = parse_precedence(self, precedence);
	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_BINARY,
		.start = lhs->start,
		.lhs = lhs,
		.rhs = rhs,
		.op = op);
}

static struct haste_ast_node *unary(struct parser *self)
{
	struct token start = previous(self);
	struct token op	= previous(self);
	struct haste_ast_node *rhs = parse_precedence(self, PREC_UNARY);
	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_UNARY,
		.start = start,
		.op = op,
		.rhs = rhs);
}

static struct haste_ast_node *cast(struct parser *self)
{
	struct token start = previous(self);
	struct haste_ast_node *to = NULL;
	if (match(self, TK_OPEN_BRAKET)) {
		to = expr(self);
		consume(self, TK_CLOSE_BRAKET, "Expected closing braket ']'.");
	}

	struct haste_ast_node *child_expr = parse_precedence(self, PREC_UNARY);
	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_CAST,
		.start = start,
		.cast = {
			.to = to,
			.expr = child_expr
		});
}

static struct haste_ast_node *primary(struct parser *self)
{
	struct token lit = previous(self);
	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_PRIMARY,
		.start = lit,
		.token = lit);
}

static struct haste_ast_node *grouping(struct parser *self)
{
	struct token start = previous(self);
	struct haste_ast_node *child = expr(self);
	consume(self, TK_CLOSE_PAREN, "Expected ')', got '%.*s' instead.", TOKEN_FMT(peek(self)));
	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_GROUPING,
		.start = start,
		.body = child);
}

struct parser_rule get_rule_from_kind(enum token_kind kind)
{
	switch (kind) {
	case TK_OPEN_PAREN: return (struct parser_rule){ grouping, NULL,   PREC_PRIMARY, false };
	case TK_PLUS:       return (struct parser_rule){ unary,    binary, PREC_TERM,    false };
	case TK_MINUS:      return (struct parser_rule){ unary,    binary, PREC_TERM,    false };
	case TK_STAR:       return (struct parser_rule){ NULL,     binary, PREC_FACTOR,  false };
	case TK_FSLASH:     return (struct parser_rule){ NULL,     binary, PREC_FACTOR,  false };
	case TK_INT:        return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_FLOAT:      return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_IDENT:      return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_INT:     return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_FLOAT:   return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_VOID:    return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_AUTO:    return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_TYPE:    return (struct parser_rule){ primary,  NULL,   PREC_PRIMARY, false };
	case TK_KW_CAST:    return (struct parser_rule){ cast,     NULL,   PREC_UNARY,   false };
	default:            return (struct parser_rule){ NULL,     NULL,   PREC_NONE,    false };
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
			report_error_at(self, token, "`{token}` is not a valid operator.", token);
		}

		left = infix_rule(self, left);
	}

	return left;
}

static struct haste_ast_node *expr(struct parser *self)
{
	return parse_precedence(self, PREC_ASSIGNMENT);
}

static struct haste_ast_node *variable_decl(struct parser *self, bool is_constant)
{
	struct token start = previous(self);
	struct token name = consume(self, TK_IDENT, "expected a variable name.");

	struct haste_ast_node *type = NULL;
	if (match(self, TK_COLON)) type = expr(self);

	struct haste_ast_node *value = NULL;
	if (match(self, TK_EQ)) value = expr(self);

	consume(self, TK_SEMI_COLON, "Expected ';' at the end of the variable declaration.");

	return create(
		self->allocator,
		struct haste_ast_node,
		.kind = ND_VAR_DECL,
		.start = start,
		.variable = {
			.is_constant = is_constant,
			.name        = name,
			.type        = type,
			.value       = value,
		});
}

static struct haste_ast_node *decl(struct parser *self, const bool error_on_unexpected)
{
	struct token token = peek(self);
	if (match(self, TK_KW_CONST, TK_KW_VAR)) {
		struct haste_ast_node *node = variable_decl(self, token.kind == TK_KW_CONST);
		node->variable.is_global = true;
		return node;
	}

	if (!error_on_unexpected) return NULL;
	report_error_at(
		self,
		token,
		"Unexpected token: '{token}'. "
		"expected either 'const', 'var' or 'func'.",
		token);
	exit(0);
}

Error parse(struct Allocator allocator, const struct token_list tokens, const source_file_id src)
{
	struct parser parser = {
		.allocator = allocator,
		.src = src,
		.tokens = tokens,
	};

	struct haste_ast_node head = {0};
	struct haste_ast_node *current = &head;
	while (!ended(&parser)) {
		struct haste_ast_node *node = decl(&parser, true);
		current->next = node;
		current = current->next;
	}

	sources.items[src].root = head.next;

	return OK;
}
