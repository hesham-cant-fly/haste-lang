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

typedef struct Parser {
	Arena arena;
	const Token *tokens;
	jmp_buf jmpbuf;
	size_t current;
	bool had_error;
} Parser;

typedef enum Precedence {
	PREC_NONE,
	PREC_ASSIGNMENT, // = (unimplemented)
	PREC_TERM,		 // + -
	PREC_FACTOR,	 // * /
	PREC_POWER,		 // **
	PREC_UNARY,		 // - +
	PREC_PRIMARY
} Precedence;

typedef AstExpr (*ParsePrefixFn)(Parser *restrict);
typedef AstExpr (*ParseInfixFn)(Parser *restrict, const AstExpr *left);

typedef struct ParserRule {
	ParsePrefixFn prefix;
	ParseInfixFn infix;
	Precedence precedence;
	bool right_assoc;
} ParserRule;

#define create(self, ...) _create(self, sizeof(__VA_ARGS__), &(__VA_ARGS__))

static const void *_create(Parser *self, size_t size, const void *value);

static bool check(const Parser *self, TokenKind kind);
static bool match(Parser *self, TokenKind kind);
static Token consume(Parser *self, TokenKind kind, const char *fmt, ...);

static NORETURN void report_error(Parser *parser, Location at, const char *fmt, ...);
static NORETURN void vareport_error(Parser *parser, Location at, const char *fmt, va_list args);

static Token advance(Parser *self);
static Token previous(const Parser *self);
static Token peek(const Parser *self);
static Token peek_next(const Parser *self);
static Token peek_ahead(const Parser *self, ssize_t offset);
static bool ended(const Parser *self);
static AstOperator operator_from_token(const Token token);

static AstExpr parse_precedence(Parser *self, Precedence precedence);
static ParserRule get_rule(TokenKind kind);

static AstDecl parse_declaration(Parser *self);
static AstExpr parse_expr(Parser *self);

error parse_tokens(const Token *tokens, ASTFile *out)
{
	Parser parser = { 0 };
	parser.tokens = tokens;

	/* AstDecl *declarations = arrinit(AstDecl); */
	AstDeclarationList declarations = { 0 };
	while (parser.current < arrlen(parser.tokens))
	{
		error err			= OK;
		AstDecl declaration = { 0 };
		if ((err = setjmp(parser.jmpbuf)) == OK)
		{
			declaration = parse_declaration(&parser);
		}
		else
		{
			// TODO: Static Synchronize or assume tokens
		}

		// its a waste of memory to allocate when we already got an error
		if (!parser.had_error)
		{
			AstDeclarationListNode node = { 0 };
			node.node					= declaration;
			ast_declaration_list_append(&declarations, create(&parser, node));
		}
	}

	if (parser.had_error)
	{
		arena_free(&parser.arena);
		*out = (ASTFile){ 0 };
		return ERROR;
	}

	*out			  = (ASTFile){ 0 };
	out->arena		  = parser.arena;
	out->declarations = declarations;
	return OK;
}

static AstDecl parse_const_declaration(Parser *self);
static AstDecl parse_var_declaration(Parser *self);

static AstDecl parse_declaration(Parser *self)
{
	const Token token = advance(self);
	switch (token.kind)
	{
	case TOKEN_KIND_CONST:
		return parse_const_declaration(self);
	case TOKEN_KIND_VAR:
		return parse_var_declaration(self);
	default:
		report_error(self, token.location, "Unexpected Token! Expected either `const`, or `var`.");
	}
}

static AstDecl parse_const_declaration(Parser *self)
{
	const Token start	   = previous(self);
	const Token identifier = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a Constant Name.\n");
	const AstExpr *type	   = NULL;
	if (match(self, TOKEN_KIND_COLON))
	{
		const AstExpr tp = parse_expr(self);
		type			 = create(self, tp);
	}

	consume(self, TOKEN_KIND_EQUAL, "Expected `=`.");
	AstExpr value = parse_expr(self);
	Token end	  = consume(self, TOKEN_KIND_SEMICOLON, "Expected `;`.");

	return (AstDecl){
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

static AstDecl parse_var_declaration(Parser *self)
{
	const Token start	   = previous(self);
	const Token identifier = consume(self, TOKEN_KIND_IDENTIFIER, "Expected a Variable Name.\n");
	const AstExpr *type	   = NULL;
	if (match(self, TOKEN_KIND_COLON))
	{
		const AstExpr tp = parse_expr(self);
		type			 = create(self, tp);
	}

	consume(self, TOKEN_KIND_EQUAL, "Expected `=`.");
	const AstExpr value = parse_expr(self);

	const Token end		= consume(self, TOKEN_KIND_SEMICOLON, "Expected `;`.");

	return (AstDecl){
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

static AstExpr parse_expr(Parser *self)
{
	return parse_precedence(self, PREC_ASSIGNMENT);
}

static AstExpr parse_precedence(Parser *self, Precedence precedence)
{
	Token token				  = advance(self);
	ParsePrefixFn prefix_rule = get_rule(token.kind).prefix;
	if (prefix_rule == NULL)
	{
		report_error(self, token.location, "Expected an expression, got `%.*s` instead.", span_len(token.span),
			token.span.start);
	}

	AstExpr left = prefix_rule(self);

	ParserRule rule;
	for (rule = get_rule(peek(self).kind); (!ended(self)) && precedence <= rule.precedence;
		rule  = get_rule(peek(self).kind))
	{
		Token tok				= advance(self);
		ParseInfixFn infix_rule = get_rule(tok.kind).infix;
		if (infix_rule == NULL)
		{
			report_error(
				self, tok.location, "Expected a valid operator, got `%.*s`.", span_len(tok.span), tok.span.start);
		}

		const AstExpr *left_expr = create(self, left);
		left					 = infix_rule(self, left_expr);
	}

	return left;
}

static AstExpr parse_identifier(Parser *self)
{
	const Token token = previous(self);

	return (AstExpr) {
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

static AstExpr parse_int_lit(Parser *self)
{
	const Token token = previous(self);

	int64_t value	  = 0;
	error err		  = strn_to_i64(token.span.start, span_len(token.span), 10, &value);
	if (err)
	{
		panic("Something bad has happened.");
	}

	return (AstExpr){
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

static AstExpr parse_float_lit(Parser *self)
{
	Token token	 = previous(self);

	double value = 0;
	error err	 = strn_to_double(token.span.start, span_len(token.span), &value);
	if (err)
	{
		panic("Something bad has happened.");
	}

	return (AstExpr){
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

static AstExpr parse_auto_type(Parser *self)
{
	Token token = previous(self);
	return (AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_AUTO_TYPE,
		},
	};
}

static AstExpr parse_typeid_type(Parser* self)
{
	Token token = previous(self);
	return (AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_TYPEID_TYPE,
		},
	};
}

static AstExpr parse_int_type(Parser *self)
{
	Token token = previous(self);
	return (AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_INT_TYPE,
		},
	};
}

static AstExpr parse_float_type(Parser *self)
{
	Token token = previous(self);
	return (AstExpr){
		.location = token.location,
		.span	  = token.span,
		.node = {
			.tag = AST_EXPR_KIND_FLOAT_TYPE,
		},
	};
}

static AstExpr parse_grouping(Parser *self)
{
	Token start	  = previous(self);
	AstExpr child = parse_expr(self);
	Token end	  = consume(self, TOKEN_KIND_CLOSE_PAREN, "Expected a ')'.");
	return (AstExpr){
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

static AstExpr parse_unary(Parser *self)
{
	Token op	= previous(self);
	AstExpr rhs = parse_precedence(self, PREC_UNARY);
	Token end	= previous(self);
	return (AstExpr){
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

static AstExpr parse_binary(Parser *self, const AstExpr *left)
{
	Token op			  = previous(self);

	ParserRule rule		  = get_rule(op.kind);
	Precedence precedence = rule.precedence + 1;
	if (rule.right_assoc)
	{
		precedence = rule.precedence;
	}

	AstExpr rhs = parse_precedence(self, precedence);
	Token end	= previous(self);
	return (AstExpr){
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

static ParserRule get_rule(TokenKind kind)
{
	switch (kind)
	{
	case TOKEN_KIND_IDENTIFIER:  return (ParserRule){ parse_identifier, NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_INT_LIT:     return (ParserRule){ parse_int_lit,    NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_FLOAT_LIT:   return (ParserRule){ parse_float_lit,  NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_AUTO:        return (ParserRule){ parse_auto_type,  NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_TYPEID:      return (ParserRule){ parse_typeid_type,NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_INT:         return (ParserRule){ parse_int_type,   NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_FLOAT:       return (ParserRule){ parse_float_type, NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_OPEN_PAREN:  return (ParserRule){ parse_grouping,   NULL,         PREC_PRIMARY, false };
	case TOKEN_KIND_PLUS:        return (ParserRule){ parse_unary,      parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_MINUS:       return (ParserRule){ parse_unary,      parse_binary, PREC_TERM,    false };
	case TOKEN_KIND_STAR:        return (ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_FSLASH:      return (ParserRule){ NULL,             parse_binary, PREC_FACTOR,  false };
	case TOKEN_KIND_DOUBLE_STAR: return (ParserRule){ NULL,             parse_binary, PREC_POWER,   true };
	default:
		return (ParserRule){ 0, 0, 0, 0 };
	}
}

static const void *_create(Parser *self, size_t size, const void *value)
{
	if (self->had_error)
		return NULL;
	void *result = arena_alloc(&self->arena, size);
	if (result == NULL)
	{
		panic("BUY MORE RAM lol");
	}
	memcpy(result, value, size);
	return result;
}

static bool check(const Parser *self, TokenKind kind)
{
	if (ended(self))
	{
		return false;
	}
	return peek(self).kind == kind;
}

static bool match(Parser *self, TokenKind kind)
{
	if (check(self, kind))
	{
		advance(self);
		return true;
	}
	return false;
}

static Token consume(Parser *self, TokenKind kind, const char *fmt, ...)
{
	Token token = peek(self);
	if (match(self, kind))
	{
		return token;
	}

	va_list args;
	va_start(args, fmt);
	vareport_error(self, token.location, fmt, args);
}

static Token advance(Parser *self)
{
	Token token = peek(self);
	if (!ended(self))
	{
		self->current += 1;
	}
	return token;
}

static Token previous(const Parser *self)
{
	if (self->current == 0)
	{
		return self->tokens[0];
	}
	return self->tokens[self->current - 1];
}

static Token peek(const Parser *self)
{
	if (ended(self))
	{
		return self->tokens[self->current - 1];
	}
	return self->tokens[self->current];
}

static Token peek_next(const Parser *self)
{
	if (self->current + 1 >= arrlen(self->tokens))
	{
		return peek(self);
	}
	return self->tokens[self->current + 1];
}

static Token peek_ahead(const Parser *self, ssize_t offset)
{
	if (self->current + offset >= arrlen(self->tokens))
	{
		return peek(self);
	}
	return self->tokens[self->current + offset];
}

static bool ended(const Parser *self)
{
	return self->current >= arrlen(self->tokens);
}

static AstOperator operator_from_token(const Token token)
{
	switch (token.kind)
	{
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
	Parser *parser,
	Location at,
	const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);
	vreport(stderr, "(BUF)", at, "Error", fmt, args);
	va_end(args);

	parser->had_error = true;

	longjmp(parser->jmpbuf, ERROR);
}

static NORETURN void vareport_error(
	Parser *parser,
	Location at,
	const char *fmt,
	va_list args) {

	vreport(stderr, "(BUF)", at, "Error", fmt, args);
	parser->had_error = true;
	longjmp(parser->jmpbuf, ERROR);
}
