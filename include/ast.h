#ifndef __AST_H
#define __AST_H

#include "arena.h"
#include "common.h"
#include "token.h"
#include <stdint.h>

defenum(ast_expr_kind_t, uint8_t,
        {
            AST_EXPR_FLOAT_LIT = 0,
            AST_EXPR_INT_LIT,
            AST_EXPR_IDENTIFIER_LIT,
            AST_EXPR_BINARY,
            AST_EXPR_BLOCK,
        });

typedef struct ASTExprBinary {
  struct ASTExpr *lhs;
  struct ASTExpr *rhs;
  token_t op;
} ast_expr_binary_t;

typedef struct ASTExprBlock {
  struct ASTStmt *value;
  struct ASTExprBlock *next;
} ast_expr_block_t;

typedef struct ASTExpr {
  union {
    token_t float_lit;
    token_t int_lit;
    token_t identifier;
    ast_expr_block_t block;
    ast_expr_binary_t binary;
  } as;
  ast_expr_kind_t tag;
} ast_expr_t;

defenum(ast_stmt_kind_t, uint8_t,
        {
            AST_STMT_VAR_DECL = 0,
            AST_STMT_CONST_DECL = 0,
            AST_STMT_EXPR,
        });

typedef struct ASTStmtVarDecl {
  token_t name;
  ast_expr_t *type;
  ast_expr_t *value;
} ast_stmt_var_decl_t;

typedef struct ASTStmtConstDecl {
  token_t name;
  ast_expr_t *type;
  ast_expr_t *value;
} ast_stmt_const_decl_t;

typedef struct ASTStmt {
  union {
    ast_stmt_var_decl_t var;
    ast_stmt_const_decl_t cons;
    ast_expr_t expr;
  } as;
  ast_stmt_kind_t tag;
} ast_stmt_t;

typedef struct ASTModule {
  Arena *arena;
  const char *src;
  ast_expr_block_t *root;
} ast_module_t;

void fprint_ast_expr(FILE *stream, const ast_expr_t *expr, const char *src);

#define make_ast_int_lit_expr(__value) (ast_expr_t) {.tag = AST_EXPR_INT_LIT, .as = {.int_lit = __value }}
#define make_ast_float_lit_expr(...) (ast_expr_t) { .tag = AST_EXPR_FLOAT_LIT, .as = {.float_lit = __VA_ARGS__ }}
#define make_ast_binary_expr(...) (ast_expr_t) { .tag = AST_EXPR_BINARY, .as = {.binary = __VA_ARGS__ }}

#endif // !__AST_H
