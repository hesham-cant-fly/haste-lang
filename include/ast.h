#ifndef __AST_H
#define __AST_H

#include "arena.h"
#include "token.h"
#include <stdint.h>

typedef enum ASTExprKind : uint8_t {
  AST_EXPR_FLOAT_LIT = 0,
  AST_EXPR_INT_LIT,
  AST_EXPR_BINARY,
} ast_expr_kind_t;

typedef struct ASTExprBinary {
  struct ASTExpr *lhs;
  struct ASTExpr *rhs;
  token_t op;
} ast_expr_binary_t;

typedef struct ASTExpr {
  union {
    token_t float_lit;
    token_t int_lit;
    ast_expr_binary_t binary;
  } as;
  ast_expr_kind_t tag;
} ast_expr_t;

typedef struct ASTModule {
  Arena *arena;
  const char *src;
  ast_expr_t *root;
} ast_module_t;

void fprint_ast_expr(FILE *stream, const ast_expr_t *expr, const char *src);

#define make_ast_int_lit_expr(__value)                                         \
  (ast_expr_t) {                                                               \
    .tag = AST_EXPR_INT_LIT, .as = {.int_lit = __value }                       \
  }
#define make_ast_float_lit_expr(...)                                           \
  (ast_expr_t) {                                                               \
    .tag = AST_EXPR_FLOAT_LIT, .as = {.float_lit = __VA_ARGS__ }               \
  }
#define make_ast_binary_expr(...)                                              \
  (ast_expr_t) {                                                               \
    .tag = AST_EXPR_BINARY, .as = {.binary = __VA_ARGS__ }                     \
  }

#endif // !__AST_H
