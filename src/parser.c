#include "parser.h"
#include "ast.h"
#include "common.h"
#include "converters.h"
#include "core/my_array.h"
#include "core/my_commons.h"
#include "error.h"
#include "token.h"
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
typedef struct AstExpr (*ParseInfixFn)(struct Parser *restrict, const struct AstExpr *left);

struct ParserRule {
	ParsePrefixFn prefix;
	ParseInfixFn infix;
	enum Precedence precedence;
	bool right_assoc;
};

#define create(self, ...) _create(self, sizeof(__VA_ARGS__), &(__VA_ARGS__))

static const void *_create(struct Parser *self, size_t size, const void *value);

static bool check(const struct Parser *self, enum TokenKind kind);
static bool match(struct Parser *self, enum TokenKind kind);
static struct Token consume(struct Parser *self, enum TokenKind kind, const char *fmt, ...);

static NORETURN void report_error(struct Parser *parser, struct Location at, const char *fmt, ...);
static NORETURN void vareport_error(struct Parser *parser, struct Location at, const char *fmt, va_list args);

static struct Token advance(struct Parser *self);
static struct Token previous(const struct Parser *self);
static struct Token peek(const struct Parser *self);
static struct Token peek_next(const struct Parser *self);
static struct Token peek_ahead(const struct Parser *self, ssize_t offset);
static bool ended(const struct Parser *self);
static enum AstOperator operator_from_token(const struct Token token);

static struct AstExpr parse_precedence(struct Parser *self, enum Precedence precedence);
static struct ParserRule get_rule(enum TokenKind kind);

static struct AstDecl parse_declaration(struct Parser *self);
static struct AstExpr parse_expr(struct Parser *self);

error parse_tokens(const struct TokenList tokens, const char* path, struct ASTFile *out)
{
	struct Parser parser = { 0 };
	parser.tokens = tokens;
	parser.path = path;

	/* AstDecl *declarations = arrinit(AstDecl); */
	struct AstDeclarationList declarations = { 0 };
	while (parser.current < parser.tokens.len) {
		error err			= OK;
		struct AstDecl declaration = { 0 };
		if ((err = setjmp(parser.jmpbuf)) == OK) {
			declaration = parse_declaration(&parser);
		} else {
			// TODO: Static Synchronize or assume tokens
		}

		// its a waste of memory to allocate when we already got an error
		if (!parser.had_error) {
			struct AstDeclarationListNode node = { 0 };
			node.node					= declaration;
			ast_declaration_list_append(&declarations, create(&parser, node));
		}
	}

	if (parser.had_error) {
		arena_free(&parser.arena);
		*out = (struct ASTFile){ 0 };
		return ERROR;
	}

	*out			  = (struct ASTFile){ 0 };
	out->path         = path;
	out->arena		  = parser.arena;
	out->declarations = declarations;
	return OK;
}

static struct AstDecl parse_const_declaration(struct Parser *self);
static struct AstDecl parse_var_declaration(struct Parser *self);

static struct AstDecl parse_declaration(struct Parser *self)
{
	const struct Token token = advance(self);
	switch (token.kind) {
	case TOKEN_KIND_CONST:
		return parse_const_declaration(self);
	case TOKEN_KIND_VAR:
		return parse_var_declaration(self);
	default:
		report_error(self, token.location, "Unexpected Token! Expected either `const`, or `var`.");
	}
}

static struct AstDecl parse_const_declaration(struct Parser *self)
{
	const struct Token start	   = previous(self);
	const struct Token identifier = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a Constant Name.\n");
	const struct AstExpr *type	   = NULL;
	if (match(self, TOKEN_KIND_COLON)) {
		const struct AstExpr tp = parse_expr(self);
		type			 = create(self, tp);
	}

	consume(self, TOKEN_KIND_EQUAL, "Expected `=`.");
	struct AstExpr value = parse_expr(self);
	struct Token end	  = consume(self, TOKEN_KIND_SEMICOLON, "Expected `;`.");

	return (struct AstDecl){
		.location = start.location,
		.span = span_conjoin(start.span, end.span),
		.node = {
			.tag = AST_DECL_KIND_CONSTANT,
			.as = {
				.constant = {
					.name  = identifier.span,
					.type  = type,
					.value = create(self, value),
				},
			},
		},
	};
}

static struct AstDecl parse_var_declaration(struct Parser *self)
{
	const struct Token start	   = previous(self);
	const struct Token identifier = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a Variable Name.\n");
	const struct AstExpr *type	   = NULL;
	if (match(self, TOKEN_KIND_COLON)) {
		const struct AstExpr tp = parse_expr(self);
		type			 = create(self, tp);
	}

	consume(self, TOKEN_KIND_EQUAL, "Expected `=`.");
	const struct AstExpr value = parse_expr(self);

	const struct Token end		= consume(self, TOKEN_KIND_SEMICOLON, "Expected `;`.");

	return (struct AstDecl){
		.location = start.location,
		.span	  = span_conjoin(start.span, end.span),
		.node = {
			.tag = AST_DECL_KIND_VARIABLE,
			.as = {
				.variable = {
					.name  = identifier.span,
					.type  = type,
					.value = create(self, value),
				},
			},
		},
	};
}

static struct AstExpr parse_expr(struct Parser *self)
{
	return parse_precedence(self, PREC_ASSIGNMENT);
}

static struct AstExpr parse_precedence(struct Parser *self, enum Precedence precedence)
{
	struct Token token				  = advance(self);
	ParsePrefixFn prefix_rule = get_rule(token.kind).prefix;
	if (prefix_rule == NULL) {
		report_error(self, token.location, "Expected an expression, got `%.*s` instead.", span_len(token.span),
			token.span.start);
	}

	struct AstExpr left = prefix_rule(self);

	struct ParserRule rule;
	for (rule = get_rule(peek(self).kind); (!ended(self)) && precedence <= rule.precedence;
		rule  = get_rule(peek(self).kind)) {
		struct Token tok				= advance(self);
		ParseInfixFn infix_rule = get_rule(tok.kind).infix;
		if (infix_rule == NULL) {
			report_error(
				self,
				tok.location,
				"Expected a valid operator, got `%.*s`.",
				span_len(tok.span),
				tok.span.start);
		}

		const struct AstExpr *left_expr = create(self, left);
		left					 = infix_rule(self, left_expr);
	}

	return left;
}

static struct AstExpr parse_identifier(struct Parser *self)
{
	const struct Token token = previous(self);

	return (struct AstExpr) {
		.location = token.location,
		.span = token.span,
		.node = {
			.tag = AST_EXPR_KIND_IDENTIFIER,
			.as = {
				.identifier = token.span,
			},
		},
	};
}

static struct AstExpr parse_int_lit(struct Parser *self)
{
	const struct Token token = previous(self);

	int64_t value	  = 0;
	error err		  = strn_to_i64(token.span.start, span_len(token.span), 10, &value);
	if (err) {
		panic("Something bad has happened.");
	}

	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_INT_LIT,
			.as = {
				.int_lit = value,
			},
		},
	};
}

static struct AstExpr parse_float_lit(struct Parser *self)
{
	struct Token token	 = previous(self);

	double value = 0;
	error err	 = strn_to_double(token.span.start, span_len(token.span), &value);
	if (err) {
		panic("Something bad has happened.");
	}

	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_FLOAT_LIT,
			.as = {
				.float_lit = value,
			},
		},
	};
}

static struct AstExpr parse_auto_type(struct Parser *self)
{
	struct Token token = previous(self);
	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_AUTO_TYPE,
		},
	};
}

static struct AstExpr parse_typeid_type(struct Parser* self)
{
	struct Token token = previous(self);
	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_TYPEID_TYPE,
		},
	};
}

static struct AstExpr parse_int_type(struct Parser *self)
{
	struct Token token = previous(self);
	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_INT_TYPE,
		},
	};
}

static struct AstExpr parse_float_type(struct Parser *self)
{
	struct Token token = previous(self);
	return (struct AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_FLOAT_TYPE,
		},
	};
}

static struct AstExpr parse_grouping(struct Parser *self)
{
	struct Token start	  = previous(self);
	struct AstExpr child = parse_expr(self);
	struct Token end	  = consume(self, TOKEN_KIND_CLOSE_PAREN, "Expected a ')'.");
	return (struct AstExpr){
		.location = start.location,
		.span	  = span_conjoin(start.span, end.span),
		.node = {
			.tag = AST_EXPR_KIND_GROUPING,
			.as = {
				.grouping = create(self, child),
			},
		},
	};
}

static struct AstExpr parse_unary(struct Parser *self)
{
	struct Token op	= previous(self);
	struct AstExpr rhs = parse_precedence(self, PREC_UNARY);
	struct Token end	= previous(self);
	return (struct AstExpr){
		.location = op.location,
		.span	  = span_conjoin(op.span, end.span),
		.node = {
			.tag = AST_EXPR_KIND_UNARY,
			.as = {
				.unary = {
					.op	 = operator_from_token(op),
					.rhs = create(self, rhs),
				},
			},
		},
	};
}

static struct AstExpr parse_binary(struct Parser *self, const struct AstExpr *left)
{
	struct Token op			  = previous(self);

	struct ParserRule rule		  = get_rule(op.kind);
	enum Precedence precedence = rule.precedence + 1;
	if (rule.right_assoc) {
		precedence = rule.precedence;
	}

	struct AstExpr rhs = parse_precedence(self, precedence);
	struct Token end	= previous(self);
	return (struct AstExpr){
		.location = left->location,
		.span	  = span_conjoin(left->span, end.span),
		.node = {
			.tag = AST_EXPR_KIND_BINARY,
			.as = {
				.bin = {
					.op	 = operator_from_token(op),
					.lhs = left,
					.rhs = create(self, rhs),
				},
			},
		},
	};
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
	case TOKEN_KIND_PLUS:        return (struct ParserRule){ parse_unary,      parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_MINUS:       return (struct ParserRule){ parse_unary,      parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_STAR:        return (struct ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_FSLASH:      return (struct ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_DOUBLE_STAR: return (struct ParserRule){ NULL,             parse_binary, PREC_POWER,   true };
	default:                     return (struct ParserRule){ 0, 0, 0, 0 };
	}
}

static const void *_create(struct Parser *self, size_t size, const void *value)
{
	if (self->had_error) {
		return NULL;
	}

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
	vareport_error(self, token.location, fmt, args);
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

static enum AstOperator operator_from_token(const struct Token token)
{
	switch (token.kind) {
	case TOKEN_KIND_PLUS:
		return AST_OPERATOR_PLUS;
	case TOKEN_KIND_MINUS:
		return AST_OPERATOR_MINUS;
	case TOKEN_KIND_STAR:
		return AST_OPERATOR_STAR;
	case TOKEN_KIND_FSLASH:
		return AST_OPERATOR_FSLASH;
	case TOKEN_KIND_DOUBLE_STAR:
		return AST_OPERATOR_POW;
	default:
		unreachable();
	}
}

static NORETURN void report_error(
	struct Parser *parser,
	struct Location at,
	const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);
	vreport(stderr, parser->path, at, "Error", fmt, args);
	va_end(args);

	parser->had_error = true;

	longjmp(parser->jmpbuf, ERROR);
}

static NORETURN void vareport_error(
	struct Parser *parser,
	struct Location at,
	const char *fmt,
	va_list args) {

	vreport(stderr, parser->path, at, "Error", fmt, args);
	parser->had_error = true;
	longjmp(parser->jmpbuf, ERROR);
}
