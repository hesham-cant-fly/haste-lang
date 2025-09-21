#include "parser.h"
#include "ast.h"
#include "common.h"
#include "error.h"
#include "my_array.h"
#include "token.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Parser {
  const char *path;
  const char *src;
  const token_t *tokens;
  Arena *arena;
  jmp_buf jmpbuf;
  size_t current;
  bool had_error;
} parser_t;

#define create(_parser, _expr) parser_alloc((_parser), sizeof((_expr)), (void*)(&(_expr)))
static void *parser_alloc(parser_t *restrict parser, const size_t size, const void *any);
static bool check(const parser_t *restrict parser, const token_kind_t kind);
#define match(_parser, ...) _match((_parser) __VA_OPT__(, ) __VA_ARGS__, 0)
static bool _match(parser_t *restrict parser, ...);
static token_t peek(const parser_t *restrict parser);
static token_t previous(const parser_t *restrict parser);
static token_t advance(parser_t *restrict parser);
static bool ended(const parser_t *restrict parser);

static NORETURN void report_error(parser_t *restrict parser, const span_t at,
                                   const char *restrict fmt, ...);

static ast_expr_t *parse_expr(parser_t *restrict parser);

error_t parse_tokens(const token_t *restrict tokens, Arena *restrict arena,
                     const char *path, const char *src, ast_module_t *out) {
  parser_t parser = {
      .path = path,
      .tokens = tokens,
      .src = src,
      .current = 0,
      .had_error = false,
      .arena = arena,
  };

  if (setjmp(parser.jmpbuf) == 0) {
    ast_expr_t *expr = parse_expr(&parser);
    fprint_ast_expr(stdout, expr, parser.src);
    *out = (ast_module_t) {
      .arena = parser.arena,
    };
  } else {
    memset(out, 0, sizeof(ast_module_t));
  }

  return !parser.had_error;
}

static ast_expr_t *parse_term(parser_t *restrict parser);
static ast_expr_t *parse_factor(parser_t *restrict parser);
static ast_expr_t *parse_power(parser_t *restrict parser);
static ast_expr_t *parse_primary(parser_t *restrict parser);

static ast_expr_t *parse_expr(parser_t *restrict parser) {
  return parse_term(parser);
}

static ast_expr_t *parse_term(parser_t *restrict parser) {
  ast_expr_t *lhs = parse_factor(parser);

  while (match(parser, TOKEN_PLUS, TOKEN_MINUS)) {
    token_t op = previous(parser);
    ast_expr_t *rhs = parse_factor(parser);
    lhs = create(parser,
                 make_ast_binary_expr({.op = op, .lhs = lhs, .rhs = rhs}));
  }

  return lhs;
}

static ast_expr_t *parse_factor(parser_t *restrict parser) {
  ast_expr_t *lhs = parse_power(parser);

  while (match(parser, TOKEN_STAR, TOKEN_FSLASH)) {
    token_t op = previous(parser);
    ast_expr_t *rhs = parse_power(parser);
    lhs = create(parser,
                 make_ast_binary_expr({.op = op, .lhs = lhs, .rhs = rhs}));
  }

  return lhs;  
}

static ast_expr_t *parse_power(parser_t *restrict parser) {
  ast_expr_t *lhs = parse_primary(parser);

  while (match(parser, TOKEN_DOUBLE_STAR)) {
    token_t op = previous(parser);
    ast_expr_t *rhs = parse_power(parser);

    lhs = create(parser,
                 make_ast_binary_expr({.op = op, .lhs = lhs, .rhs = rhs}));
  }

  return lhs;
}

static ast_expr_t *parse_primary(parser_t *restrict parser) {
  token_t token = peek(parser);
  if (match(parser, TOKEN_INT_LIT))
    return create(parser, make_ast_int_lit_expr(token));
  if (match(parser, TOKEN_FLOAT_LIT))
    return create(parser, make_ast_float_lit_expr(token));
  if (match(parser, TOKEN_IDENTIFIER))
    return create(parser, make_ast_identifier_lit_expr(token));

  report_error(parser, token.span, "Expected an expression.");
  exit(69);
}

static void *parser_alloc(parser_t *restrict parser, const size_t size, const void *any) {
  void *result = arena_alloc(parser->arena, size);
  memcpy(result, any, size);
  return result;
}

static bool check(const parser_t *restrict parser, const token_kind_t kind) {
  if (ended(parser)) return false;
  return peek(parser).kind == kind;
}

static bool _match(parser_t *restrict parser, ...) {
  va_list args;
  va_start(args, parser);

  token_kind_t kind = (uint8_t)va_arg(args, int);
  while (kind != 0) {
    if (check(parser, kind)) {
      advance(parser);
      return true;
    }
    kind = (uint8_t)va_arg(args, int);
  }

  va_end(args);
  return false;
}

static token_t peek(const parser_t *restrict parser) {
  return parser->tokens[parser->current];
}

static token_t previous(const parser_t *restrict parser) {
  return parser->tokens[parser->current - 1];
}

static token_t advance(parser_t *restrict parser) {
  if (!ended(parser)) {
    parser->current += 1;
    return previous(parser);
  }
  return peek(parser);
}

static bool ended(const parser_t *restrict parser) {
  return parser->current >= arrlen(parser->tokens);
}

static NORETURN void report_error(parser_t *restrict parser, const span_t at,
                                   const char *restrict fmt, ...) {
  fprintf(stderr, "%s:%zu:%zu: Error: ", parser->path, at.line, at.column);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");

  parser->had_error = true;

  longjmp(parser->jmpbuf, 1);
  exit(69);
}
