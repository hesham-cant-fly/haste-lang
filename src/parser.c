#include "parser.h"
#include "ast.h"
#include "common.h"
#include "error.h"
#include "my_array.h"
#include "my_string_view.h"
#include "span.h"
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
  ast_expr_pool_t expr_pool;
  jmp_buf jmpbuf;
  size_t current;
  bool had_error;
} parser_t;

typedef enum Precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,  // = (unimplemented)
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_POWER,       // **
  PREC_UNARY,       // - +
  PREC_PRIMARY
} precedence_t;

typedef ast_expr_ref_t (*parse_fn_prefix_t)(parser_t *restrict);
typedef ast_expr_ref_t (*parse_fn_infix_t)(parser_t *restrict, const ast_expr_ref_t left);

typedef struct ParserRule {
  parse_fn_prefix_t prefix;
  parse_fn_infix_t infix;
  precedence_t precedence;
  bool right_assoc;
} parser_rule_t;

#define match(_parser, ...) _match((_parser) __VA_OPT__(, ) __VA_ARGS__, 0)

static ast_expr_ref_t add_expr(parser_t *restrict parser, const ast_expr_t expr);

static bool check(const parser_t *restrict parser, const token_kind_t kind);
static bool _match(parser_t *restrict parser, ...);
static token_t peek(const parser_t *restrict parser);
static token_t previous(const parser_t *restrict parser);
static token_t advance(parser_t *restrict parser);
static bool ended(const parser_t *restrict parser);

static NORETURN void report_error(parser_t *restrict parser, const span_t at,
                                   const char *restrict fmt, ...);

static parser_rule_t get_rule(token_kind_t kind);
static ast_expr_ref_t parse_precedence(parser_t *restrict parser, precedence_t precedence);

static ast_expr_ref_t parse_assignment(parser_t *restrict parser, const ast_expr_ref_t left);
static ast_expr_ref_t parse_binary(parser_t *restrict parser,
                               const ast_expr_ref_t left);
static ast_expr_ref_t parse_unary(parser_t *restrict parser);
static ast_expr_ref_t parse_int(parser_t *restrict parser);
static ast_expr_ref_t parse_float(parser_t *restrict parser);
static ast_expr_ref_t parse_identifier(parser_t *restrict parser);
static ast_expr_ref_t parse_expr(parser_t *restrict parser);

error_t parse_tokens(const token_t *restrict tokens, const char *path,
                     const char *src, ast_module_t *out) {
  parser_t parser = {
      .path = path,
      .tokens = tokens,
      .src = src,
      .current = 0,
      .expr_pool = new_ast_expr_pool(),
      .had_error = false,
  };

  if (setjmp(parser.jmpbuf) == 0) {
    ast_expr_ref_t expr = parse_expr(&parser);
    /* fprint_ast_expr(stdout, expr, parser.src); */
    fprint_ast_expr(stdout, parser.expr_pool, expr, parser.src);
  }
  *out = (ast_module_t){
      .expr_pool = parser.expr_pool,
      .src = parser.src,
  };

  return !parser.had_error;
}

static ast_expr_ref_t parse_expr(parser_t *restrict parser) {
  return parse_precedence(parser, PREC_ASSIGNMENT);
}

static ast_expr_ref_t parse_precedence(parser_t *restrict parser, precedence_t precedence) {
  token_t tok = advance(parser);
  parse_fn_prefix_t prefix_rule = get_rule(tok.kind).prefix;
  if (prefix_rule == NULL) {
    report_error(parser, tok.span, "Expected an exprssion, got `%.*s`.",
                 SVArgs(span_to_string_view(tok.span, parser->src)));
  }

  ast_expr_ref_t left = prefix_rule(parser);

  parser_rule_t rule;
  while (precedence <= (rule = get_rule(peek(parser).kind)).precedence) {
    token_t tok = advance(parser);
    parse_fn_infix_t infix_rule = get_rule(tok.kind).infix;
    if (infix_rule == NULL) {
      report_error(parser, tok.span, "Expected a valid operator, got `%.*s`.",
                   SVArgs(span_to_string_view(tok.span, parser->src)));
    }
    left = infix_rule(parser, left);
  }

  return left;
}

static parser_rule_t get_rule(token_kind_t kind) {
  switch (kind) {
  case TOKEN_EQUAL:       return (parser_rule_t){ NULL,             parse_assignment, PREC_ASSIGNMENT, true  };
  case TOKEN_PLUS:        return (parser_rule_t){ parse_unary,      parse_binary,     PREC_TERM,       false };
  case TOKEN_MINUS:       return (parser_rule_t){ parse_unary,      parse_binary,     PREC_TERM,       false };
  case TOKEN_STAR:        return (parser_rule_t){ NULL,             parse_binary,     PREC_FACTOR,     false };
  case TOKEN_FSLASH:      return (parser_rule_t){ NULL,             parse_binary,     PREC_FACTOR,     false };
  case TOKEN_DOUBLE_STAR: return (parser_rule_t){ NULL,             parse_binary,     PREC_POWER,      true  };
  case TOKEN_INT_LIT:     return (parser_rule_t){ parse_int,        NULL,             PREC_PRIMARY,    false };
  case TOKEN_FLOAT_LIT:   return (parser_rule_t){ parse_float,      NULL,             PREC_PRIMARY,    false };
  case TOKEN_IDENTIFIER:  return (parser_rule_t){ parse_identifier, NULL,             PREC_PRIMARY,    false };
  default:                return (parser_rule_t){ NULL,             NULL,             PREC_NONE,       0     };
  }
}

static ast_expr_ref_t parse_assignment(parser_t *restrict parser,
                                       const ast_expr_ref_t left) {
  const ast_expr_t left_expr = get_ast_expr(parser->expr_pool, left);
  if (left_expr.tag != AST_EXPR_IDENTIFIER_LIT) report_error(parser, previous(parser).span, "Invalid assignment target.");

  const ast_expr_ref_t right = parse_precedence(parser, PREC_ASSIGNMENT);

  return add_expr(parser,
                  make_ast_assignment_expr({.target = left, .value = right}));
}

static ast_expr_ref_t parse_unary(parser_t *restrict parser) {
  token_t op = previous(parser);
  ast_expr_ref_t right = parse_precedence(parser, PREC_UNARY);
  return add_expr(parser, make_ast_unary_expr({.op = op, .rhs = right}));
}

static ast_expr_ref_t parse_binary(parser_t *restrict parser,
                               const ast_expr_ref_t left) {
  token_t op = previous(parser);

  parser_rule_t rule = get_rule(op.kind);
  precedence_t precedence = rule.precedence + 1;
  if (rule.right_assoc) precedence = rule.precedence;

  ast_expr_ref_t right = parse_precedence(parser, precedence);
  return add_expr(parser,
                  make_ast_binary_expr({.op = op, .lhs = left, .rhs = right}));
}

static ast_expr_ref_t parse_int(parser_t *restrict parser) {
  token_t value = previous(parser);
  return add_expr(parser, make_ast_int_lit_expr(value));
}

static ast_expr_ref_t parse_float(parser_t *restrict parser) {
  token_t value = previous(parser);
  return add_expr(parser, make_ast_float_lit_expr(value));
}

static ast_expr_ref_t parse_identifier(parser_t *restrict parser) {
  token_t value = previous(parser);
  return add_expr(parser, make_ast_identifier_lit_expr(value));
}

static ast_expr_ref_t add_expr(parser_t *restrict parser, const ast_expr_t expr) {
  return ast_expr_pool_add(parser->expr_pool, expr);
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
}
