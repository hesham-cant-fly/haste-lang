#include "parser.h"
#include "ast.h"
#include "common.h"
#include "error.h"
#include "my_array.h"
#include "my_optional.h"
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
  ast_module_t module;
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
static ast_type_ref_t add_type(parser_t *restrict parser, const ast_type_t type);
static ast_stmt_ref_t add_stmt(parser_t *restrict parser, const ast_stmt_t stmt);
static ast_expr_t get_expr(parser_t *restrict parser, const ast_expr_ref_t id);
static ast_type_t get_type(parser_t *restrict parser, const ast_type_ref_t id);
static ast_stmt_t get_stmt(parser_t *restrict parser, const ast_stmt_ref_t id);

static bool check(const parser_t *restrict parser, const token_kind_t kind);
static bool _match(parser_t *restrict parser, ...);
static token_t consume(parser_t *restrict parser, const token_kind_t kind, const char *restrict fmt, ...);
static token_t peek(const parser_t *restrict parser);
static token_t previous(const parser_t *restrict parser);
static token_t advance(parser_t *restrict parser);
static bool ended(const parser_t *restrict parser);

static NORETURN void report_error(parser_t *restrict parser, const span_t at,
                                   const char *restrict fmt, ...);
static NORETURN void vareport_error(parser_t *restrict parser, const span_t at,
                                    const char *restrict fmt, va_list args);

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

static ast_type_ref_t parse_type(parser_t *restrict parser);

static ast_stmt_ref_t parse_stmt(parser_t *restrict parser);
static ast_stmt_ref_t parse_constant(parser_t *restrict parser);
static ast_stmt_ref_t parse_variable(parser_t *restrict parser);

error_t parse_tokens(const token_t *restrict tokens, const char *path,
                     const char *src, ast_module_t *out) {
  parser_t parser = {
      .path = path,
      .tokens = tokens,
      .src = src,
      .current = 0,
      .module = new_ast_module(),
      .had_error = false,
  };

  optional_uint32_t mod_root = none(optional_uint32_t);

  if (setjmp(parser.jmpbuf) == 0) {
    ast_expr_ref_t ref = parse_expr(&parser);
    print_expr(parser.module.expr_pool, ref);
  }
  // while (!ended(&parser)) {
  //   if (setjmp(parser.jmpbuf) == 0) {
  //     ast_stmt_ref_t stmt = parse_stmt(&parser);
  //     if (is_none(mod_root))
  //       mod_root = some(optional_uint32_t, stmt);
  //   } else {
  //     // TODO: Sync
  //     unreachable0();
  //   }
  // }
  *out = parser.module;
  out->src = parser.src;
  out->root = is_some(mod_root) ? mod_root.value : 0;

  return !parser.had_error;
}

static ast_type_ref_t parse_type(parser_t *restrict parser) {
  token_t tok = advance(parser);
  switch (tok.kind) {
  case TOKEN_AUTO: return add_type(parser, (ast_type_t) { .tag = AST_TYPE_AUTO });
  case TOKEN_INT: return add_type(parser, (ast_type_t) { .tag = AST_TYPE_INT });
  default: panic("%s", token_kind_tostr(tok.kind)); break;
  }
}

static ast_stmt_ref_t parse_stmt(parser_t *restrict parser) {
  token_t tok = advance(parser);
  switch (tok.kind) {
  case TOKEN_VAR: return parse_variable(parser);
  case TOKEN_CONST: return parse_constant(parser);
  default: panic("%s", token_kind_tostr(tok.kind)); break;
  }
}

static ast_stmt_ref_t parse_constant(parser_t *restrict parser) {
  token_t identifier =
      consume(parser, TOKEN_IDENTIFIER, "Expected a variable name.");
  optional_ast_type_t type = none(optional_ast_type_t);
  optional_ast_expr_t value = none(optional_ast_expr_t);
  if (match(parser, TOKEN_COLON)) {
    ast_type_ref_t type_ref = parse_type(parser);
    type = some(optional_ast_type_t, type_ref);
  }
  if (match(parser, TOKEN_EQUAL)) {
    ast_expr_ref_t expr = parse_expr(parser);
    value = some(optional_ast_expr_t, expr);
  }

  return add_stmt(parser,
                  make_ast_stmt_constant(
                      {.target = identifier, .type = type, .value = value}));
}

static ast_stmt_ref_t parse_variable(parser_t *restrict parser) {
  token_t identifier =
      consume(parser, TOKEN_IDENTIFIER, "Expected a variable name.");
  optional_ast_type_t type = none(optional_ast_type_t);
  optional_ast_expr_t value = none(optional_ast_expr_t);
  if (match(parser, TOKEN_COLON)) {
    ast_type_ref_t type_ref = parse_type(parser);
    type = some(optional_ast_type_t, type_ref);
  }
  if (match(parser, TOKEN_EQUAL)) {
    ast_expr_ref_t expr = parse_expr(parser);
    value = some(optional_ast_expr_t, expr);
  }

  return add_stmt(parser,
                  make_ast_stmt_variable(
                      {.target = identifier, .type = type, .value = value}));
}

static ast_expr_ref_t parse_expr(parser_t *restrict parser) {
  return parse_precedence(parser, PREC_ASSIGNMENT);
}

static ast_expr_ref_t parse_precedence(parser_t *restrict parser, precedence_t precedence) {
  token_t tok = advance(parser);
  parse_fn_prefix_t prefix_rule = get_rule(tok.kind).prefix;
  if (prefix_rule == NULL) {
    report_error(parser, tok.span, "Expected an exprssion, got `%.*s`.",
                 SPANArgs(tok.span));
  }

  ast_expr_ref_t left = prefix_rule(parser);

  parser_rule_t rule;
  while (precedence <= (rule = get_rule(peek(parser).kind)).precedence) {
    token_t tok = advance(parser);
    parse_fn_infix_t infix_rule = get_rule(tok.kind).infix;
    if (infix_rule == NULL) {
      report_error(parser, tok.span, "Expected a valid operator, got `%.*s`.",
                   SPANArgs(tok.span));
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
  const ast_expr_t left_expr = get_expr(parser, left);
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
  return ast_module_add_expr(parser->module, expr);
}

static ast_type_ref_t add_type(parser_t *restrict parser,
                               const ast_type_t type) {
  return ast_module_add_type(parser->module, type);
}

static ast_stmt_ref_t add_stmt(parser_t *restrict parser,
                               const ast_stmt_t stmt) {
  return ast_module_add_stmt(parser->module, stmt);
}

static ast_expr_t get_expr(parser_t *restrict parser, const ast_expr_ref_t id) {
  return parser->module.expr_pool.data[id];
}

static ast_type_t get_type(parser_t *restrict parser, const ast_type_ref_t id) {
  return parser->module.type_pool.data[id];
}

static ast_stmt_t get_stmt(parser_t *restrict parser, const ast_stmt_ref_t id) {
  return parser->module.stmt_pool.data[id];
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

static token_t consume(parser_t *restrict parser, const token_kind_t kind,
                       const char *restrict fmt, ...) {
  token_t token = peek(parser);
  if (match(parser, kind))
    return token;

  va_list args;
  va_start(args, fmt);
  vareport_error(parser, token.span, fmt, args);
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

static NORETURN void vareport_error(parser_t *restrict parser, const span_t at,
                                   const char *restrict fmt, va_list args) {
  fprintf(stderr, "%s:%zu:%zu: Error: ", parser->path, at.line, at.column);

  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");

  parser->had_error = true;

  longjmp(parser->jmpbuf, 1);
}
