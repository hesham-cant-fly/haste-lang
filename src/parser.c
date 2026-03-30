#include "parser.h"
#include "ast.h"
#include "common.h"
#include "converters.h"
#include "core/my_array.h"
#include "core/my_commons.h"
#include "error.h"
#include "token.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

struct Parser {
	Arena arena;
	const char* path;
	struct TokenList tokens;
	jmp_buf jmpbuf;
	size_t current;
	bool had_error;
};

enum Precedence {
	PREC_NONE,
	PREC_ASSIGNMENT, // = (unimplemented)
	PREC_TERM,		 // + -
	PREC_FACTOR,	 // * /
	PREC_POWER,		 // **
	PREC_UNARY,		 // - +
	PREC_PRIMARY
};

typedef struct AstExpr (*ParsePrefixFn)(struct Parser *restrict);
typedef struct AstExpr (*ParseInfixFn)(struct Parser *restrict, const struct AstExpr lhs);

struct ParserRule {
	ParsePrefixFn prefix;
	ParseInfixFn infix;
	enum Precedence precedence;
	bool right_assoc;
};

#define create(self__, ...) _create(self__, sizeof(__VA_ARGS__), &(__VA_ARGS__))
#define new_expr(self__, type_name__, ...) make_expr((type_name__), create((self__), (struct type_name__) { __VA_ARGS__, .base.type = 0, }))
#define new_decl(self__, type_name__, ...) make_decl((type_name__), create((self__), (struct type_name__) { __VA_ARGS__, .base.type = 0, }))

static void *_create(struct Parser *self, size_t size, const void *value);

static bool check(const struct Parser *self, enum TokenKind kind);
static bool match(struct Parser *self, enum TokenKind kind);
static struct Token consume(struct Parser *self, enum TokenKind kind, const char *fmt, ...);

static NORETURN void report_error(struct Parser *parser, struct Span span, struct Location at, const char *fmt, ...);
static NORETURN void vareport_error(struct Parser *parser, struct Span span, struct Location at, const char *fmt, va_list args);

static struct Token advance(struct Parser *self);
static struct Token previous(const struct Parser *self);
static struct Token peek(const struct Parser *self);
static struct Token peek_next(const struct Parser *self);
static struct Token peek_ahead(const struct Parser *self, ssize_t offset);
static bool ended(const struct Parser *self);

static struct AstBinaryOperator token_into_binary_operator(struct Token token);
static struct AstUnaryOperator token_into_unary_operator(struct Token token);

static struct AstDeclaration parse_declaration(struct Parser *self);

static struct AstExpr parse_precedence(struct Parser *self, enum Precedence precedence);
static struct ParserRule get_rule(enum TokenKind kind);
static struct AstExpr parse_expr(struct Parser *self);

static void syncronize_decl(struct Parser *self)
{
    size_t brace_depth = 0;

    while (!ended(self)) {
        if (peek(self).kind == TOKEN_KIND_OPEN_BRACE) {
            brace_depth++;
        } else if (peek(self).kind == TOKEN_KIND_CLOSE_BRACE) {
            if (brace_depth > 0) {
                brace_depth--;
			}
        } else if (peek(self).kind == TOKEN_KIND_SEMICOLON && brace_depth == 0) {
            advance(self);
            return;
        }

        advance(self);
    }
}


error parse_tokens(const struct TokenList tokens, const char* path, struct AstFile *out, Arena *out_arena)
{
	struct Parser parser = { 0 };
	parser.tokens = tokens;
	parser.path = path;

	out->path = path;
	struct AstDeclaration head = {0};
	struct AstDeclaration *current = &head;
	while (!ended(&parser)) {
		if (setjmp(parser.jmpbuf) == 0) {
			struct AstDeclaration declaration = parse_declaration(&parser);

			if (parser.had_error) continue;
			if (ast_is_none(head)) {
				assert(ast_is_none(*current));
				head = declaration;
			} else {
				struct AstDeclaration *decl = create(&parser, declaration);
				current->next = decl;
				current = decl;
			}
		} else {
			syncronize_decl(&parser);
			// goto err;
		}
	}

	out->declarations = head;

	if (parser.had_error) {
		arena_free(&parser.arena);
		*out = (struct AstFile){ 0 };
		*out_arena = (Arena) { 0 };
		return ERROR;
	}

	*out_arena = parser.arena;

	return OK;
}

static struct AstDeclaration parse_const_declaration(struct Parser *self)
{
	const struct Token start = previous(self);

	const struct Token name = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a name for the constant.");

	switch (peek(self).kind) {
	case TOKEN_KIND_COLON:
	case TOKEN_KIND_EQUAL:
	case TOKEN_KIND_SEMICOLON:
		break;
	default: {
		struct Token token = peek(self);
		report_error(
			self,
			token.span,
			token.location,
			"Expected `:`, `=` or `;`. got '%.*s' instead.",
			SPAN_ARG(token.span));
	} break;
	}

	struct AstExpr type = {0};
	if (match(self, TOKEN_KIND_COLON)) {
		type = parse_expr(self);
	}

	struct AstExpr value = {0};
	if (match(self, TOKEN_KIND_EQUAL)) {
		value = parse_expr(self);
	}

	const struct Token end = consume(self, TOKEN_KIND_SEMICOLON, "Expected `;` at the end of constant declaration.");
	return new_decl(
		self,
		AstConstDeclaration,
		.base.location = start.location,
		.base.span = span_conjoin(start.span, end.span),
		.name = name,
		.type = type,
		.value = value);
}

static struct AstDeclaration parse_var_declaration(struct Parser *self)
{
	const struct Token start = previous(self);

	const struct Token name = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a name for the variable.");
	struct AstExpr type = {0};
	if (match(self, TOKEN_KIND_COLON)) {
		type = parse_expr(self);
	}

	struct AstExpr value = {0};
	if (match(self, TOKEN_KIND_EQUAL)) {
		value = parse_expr(self);
	}

	const struct Token end = consume(self, TOKEN_KIND_SEMICOLON, "Expected `;` at the end of constant variable.");
	return new_decl(
		self,
		AstVarDeclaration,
		.base.location = start.location,
		.base.span = span_conjoin(start.span, end.span),
		.name = name,
		.type = type,
		.value = value);
}

static struct AstDeclaration parse_declaration(struct Parser *self)
{
	const struct Token token = advance(self);

	switch (token.kind) {
	case TOKEN_KIND_CONST: return parse_const_declaration(self);
	case TOKEN_KIND_VAR:   return parse_var_declaration(self);
	case TOKEN_KIND_FUNC:  panic("unimplemented.");
	default:
		report_error(self,
					 token.span,
					 token.location,
					 "Expected `const`, `var` or `func`. Got `%.*s` instead.",
					 SPAN_ARG(token.span));
	}
}

static struct AstExpr parse_expr(struct Parser *self)
{
	return parse_precedence(self, PREC_ASSIGNMENT);
}

static struct AstExpr parse_precedence(struct Parser *self, enum Precedence precedence)
{
	struct Token token = advance(self);
	ParsePrefixFn prefix_rule = get_rule(token.kind).prefix;
	if (prefix_rule == NULL) {
		report_error(self, token.span, token.location, "Expected an expression, got `%.*s` instead.", span_len(token.span),
			token.span.start);
	}

	struct AstExpr left = prefix_rule(self);

	struct ParserRule rule;
	for (rule = get_rule(peek(self).kind); (!ended(self)) && precedence <= rule.precedence;
		rule  = get_rule(peek(self).kind)) {
		struct Token tok = advance(self);
		ParseInfixFn infix_rule = get_rule(tok.kind).infix;
		if (infix_rule == NULL) {
			report_error(
				self,
				tok.span,
				tok.location,
				"Expected a valid operator, got `%.*s`.",
				span_len(tok.span),
				tok.span.start);
		}

		left					 = infix_rule(self, left);
	}

	return left;
}

static struct AstExpr parse_identifier(struct Parser *self)
{
	const struct Token token = previous(self);

	return new_expr(
		self,
		AstIdentifierExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_int_lit(struct Parser *self)
{
	const struct Token token = previous(self);

	int64_t value	  = 0;
	error err		  = strn_to_i64(token.span.start, span_len(token.span), 10, &value);
	if (err) {
		panic("Something bad has happened.");
	}

	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_float_lit(struct Parser *self)
{
	struct Token token	 = previous(self);

	double value = 0;
	error err	 = strn_to_double(token.span.start, span_len(token.span), &value);
	if (err) {
		panic("Something bad has happened.");
	}

	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_auto_type(struct Parser *self)
{
	struct Token token = previous(self);
	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_typeid_type(struct Parser* self)
{
	struct Token token = previous(self);
	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_int_type(struct Parser *self)
{
	struct Token token = previous(self);
	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_float_type(struct Parser *self)
{
	struct Token token = previous(self);
	return new_expr(
		self,
		AstLiteralExpr,
		.base = {
			.location = token.location,
			.span = token.span,
		},
		.token = token);
}

static struct AstExpr parse_grouping(struct Parser *self)
{
	struct Token start	  = previous(self);
	struct AstExpr child = parse_expr(self);
	struct Token end	  = consume(self, TOKEN_KIND_CLOSE_PAREN, "Expected a ')'.");
	return new_expr(
		self,
		AstGroupingExpr,
		.base = {
			.location = start.location,
			.span = span_conjoin(start.span, end.span),
		},
		.child = child);
}

static struct AstExpr parse_unary(struct Parser *self)
{
	struct Token op	= previous(self);
	struct AstExpr rhs = parse_precedence(self, PREC_UNARY);
	struct Token end	= previous(self);
	return new_expr(
		self,
		AstUnaryExpr,
		.base = {
			.location = op.location,
			.span = span_conjoin(op.span, end.span),
		},
		.op = token_into_unary_operator(op),
		.rhs = rhs);
}

static struct AstExpr parse_binary(struct Parser *self, const struct AstExpr lhs)
{
	struct Token op			  = previous(self);

	struct ParserRule rule		  = get_rule(op.kind);
	enum Precedence precedence = rule.precedence + 1;
	if (rule.right_assoc) {
		precedence = rule.precedence;
	}

	struct AstExpr rhs = parse_precedence(self, precedence);
	struct Token end	= previous(self);
	return new_expr(
		self,
		AstBinaryExpr,
		.base = {
			.location = get_ast_base(lhs).location,
			.span	  = span_conjoin(get_ast_base(lhs).span, end.span),
		},
		.op = token_into_binary_operator(op),
		.lhs = lhs,
		.rhs = rhs);
}

static struct ParserRule get_rule(enum TokenKind kind)
{
	switch (kind) {
	case TOKEN_KIND_IDENTIFIER:  return (struct ParserRule){ parse_identifier, NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_INT_LIT:     return (struct ParserRule){ parse_int_lit,    NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_FLOAT_LIT:   return (struct ParserRule){ parse_float_lit,  NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_AUTO:        return (struct ParserRule){ parse_auto_type,  NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_TYPEID:      return (struct ParserRule){ parse_typeid_type,NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_INT:         return (struct ParserRule){ parse_int_type,   NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_FLOAT:       return (struct ParserRule){ parse_float_type, NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_OPEN_PAREN:  return (struct ParserRule){ parse_grouping,   NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_PLUS:        return (struct ParserRule){ NULL,             parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_MINUS:       return (struct ParserRule){ parse_unary,      parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_STAR:        return (struct ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_FSLASH:      return (struct ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_DOUBLE_STAR: return (struct ParserRule){ NULL,             parse_binary, PREC_POWER,   true };
	default:                     return (struct ParserRule){ 0, 0, 0, 0 };
	}
}

static void *_create(struct Parser *self, size_t size, const void *value)
{
	/* if (self->had_error) { */
	/* 	return NULL; */
	/* } */

	void *result = arena_alloc(&self->arena, size);
	if (result == NULL) {
		panic("BUY MORE RAM lol");
	}
	memcpy(result, value, size);
	return result;
}

static bool check(const struct Parser *self, enum TokenKind kind)
{
	if (ended(self)) {
		return false;
	}
	return peek(self).kind == kind;
}

static bool match(struct Parser *self, enum TokenKind kind)
{
	if (check(self, kind)) {
		advance(self);
		return true;
	}
	return false;
}

static struct Token consume(struct Parser *self, enum TokenKind kind, const char *fmt, ...)
{
	struct Token token = peek(self);
	if (match(self, kind)) {
		return token;
	}

	va_list args;
	va_start(args, fmt);
	vareport_error(self, token.span, token.location, fmt, args);
}

static struct Token advance(struct Parser *self)
{
	struct Token token = peek(self);
	if (!ended(self)) {
		self->current += 1;
	}
	return token;
}

static struct Token previous(const struct Parser *self)
{
	if (self->current == 0) {
		return self->tokens.items[0];
	}
	return self->tokens.items[self->current - 1];
}

static struct Token peek(const struct Parser *self)
{
	if (ended(self)) {
		return self->tokens.items[self->current - 1];
	}
	return self->tokens.items[self->current];
}

static struct Token peek_next(const struct Parser *self)
{
	if (self->current + 1 >= self->tokens.len) {
		return peek(self);
	}
	return self->tokens.items[self->current + 1];
}

static struct Token peek_ahead(const struct Parser *self, ssize_t offset)
{
	if (self->current + offset >= self->tokens.len) {
		return peek(self);
	}
	return self->tokens.items[self->current + offset];
}

static bool ended(const struct Parser *self)
{
	return self->current >= arrlen(self->tokens);
}

static struct AstBinaryOperator token_into_binary_operator(struct Token token)
{
	struct AstBinaryOperator result = {
		.location = token.location,
		.span = token.span,
	};

	switch (token.kind) {
	case TOKEN_KIND_PLUS:        result.op = AST_BIN_ADD; break;
	case TOKEN_KIND_MINUS:       result.op = AST_BIN_SUB; break;
	case TOKEN_KIND_STAR:        result.op = AST_BIN_MUL; break;
	case TOKEN_KIND_FSLASH:      result.op = AST_BIN_DIV; break;
	case TOKEN_KIND_DOUBLE_STAR: result.op = AST_BIN_POW; break;
	default: unreachable();
	}

	return result;
}

static struct AstUnaryOperator token_into_unary_operator(struct Token token)
{
	struct AstUnaryOperator result = {
		.location = token.location,
		.span = token.span,
	};

	switch (token.kind) {
	case TOKEN_KIND_MINUS:       result.op = AST_UN_NEGATE; break;
	default: unreachable();
	}

	return result;
}

static NORETURN void report_error(
	struct Parser *parser,
	struct Span span,
	struct Location at,
	const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);
	vreport(stderr, parser->path, span, at, "Error", fmt, args);
	va_end(args);

	parser->had_error = true;

	longjmp(parser->jmpbuf, ERROR);
}

static NORETURN void vareport_error(
	struct Parser *parser,
	struct Span span,
	struct Location at,
	const char *fmt,
	va_list args) {

	vreport(stderr, parser->path, span, at, "Error", fmt, args);
	parser->had_error = true;
	longjmp(parser->jmpbuf, ERROR);
}
