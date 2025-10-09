#ifndef __AST_H
#define __AST_H

#include "arena.h"
#include "common.h"
#include "my_optional.h"
#include "token.h"
#include <stdint.h>

#define AST_EXPR_KIND_LIST                                                     \
  X(AST_EXPR_FLOAT_LIT, "expr-float", print_expr_float_lit)                    \
  X(AST_EXPR_INT_LIT, "expr-int", print_expr_int_lit)                          \
  X(AST_EXPR_IDENTIFIER_LIT, "expr-identifier", print_expr_identifier_lit)     \
  X(AST_EXPR_BINARY, "expr-binary", print_expr_binary)                         \
  X(AST_EXPR_ASSIGNMENT, "expr-assignment", print_expr_assignment)             \
  X(AST_EXPR_UNARY, "expr-unary", print_expr_unary)

#define AST_TYPE_LIST                                                          \
  X(AST_TYPE_AUTO, "type-auto", print_type_auto)                               \
  X(AST_TYPE_INT, "type-int", print_type_int)

#define AST_STMT_KIND_LIST                                                     \
  X(AST_STMT_VARIABLE, "stmt-variable", print_stmt_variable)                   \
  X(AST_STMT_CONSTANT, "stmt-constant", print_stmt_constant)                   \
  X(AST_STMT_EXPR, "stmt-expr", print_stmt_expr)

//--------- EXPRESSIONS ---------//
#define X(enum_name, ...) enum_name,
defenum(ast_expr_kind_t, uint8_t, {AST_EXPR_KIND_LIST});
#undef X

typedef uint32_t ast_expr_ref_t;

typedef struct ASTExprAssignment {
  ast_expr_ref_t target;
  ast_expr_ref_t value;
} ast_expr_assignment_t;

typedef struct ASTExprBinary {
  ast_expr_ref_t lhs;
  ast_expr_ref_t rhs;
  token_t op;
} ast_expr_binary_t;

typedef struct ASTExprUnary {
  ast_expr_ref_t rhs;
  token_t op;
} ast_expr_unary_t;

typedef struct ASTExprBlock {
  ast_expr_ref_t value;
  ast_expr_ref_t next;
} ast_expr_block_t;

typedef struct ASTExpr {
  ast_expr_kind_t tag;
  union {
    token_t float_lit;
    token_t int_lit;
    token_t identifier;
    ast_expr_block_t block;
    ast_expr_binary_t binary;
    ast_expr_assignment_t assignment;
    ast_expr_unary_t unary;
  };
} ast_expr_t;

typedef struct ASTExprPool {
  ast_expr_t *data;
} ast_expr_pool_t;

/***** TYPES *****/

#define X(enum_name, ...) enum_name,
defenum(ast_type_kind_t, uint8_t, {AST_TYPE_LIST});
#undef X

typedef uint32_t ast_type_ref_t;

typedef struct ASTType {
  ast_type_kind_t tag;
} ast_type_t;

typedef struct ASTTypePool {
  ast_type_t *data;
} ast_type_pool_t;

//--------- STATEMENTS ---------//
#define X(enum_field, ...) enum_field,
defenum(ast_stmt_kind_t, uint8_t, {AST_STMT_KIND_LIST});
#undef X
typedef uint32_t ast_stmt_ref_t;

typedef struct OptionalASTExpr {
  ast_expr_ref_t value;
  bool has_value;
} optional_ast_expr_t;

typedef struct OptionalASTType {
  ast_type_ref_t value;
  bool has_value;
} optional_ast_type_t;

typedef struct ASTStmtVariable {
  token_t target;
  optional(ast_type_t) type;
  optional(ast_expr_t) value;
} ast_stmt_variable_t;

typedef struct ASTStmtConstant {
  token_t target;
  optional(ast_type_t) type;
  optional(ast_expr_t) value;
} ast_stmt_constant_t;

typedef struct ASTStmt {
  ast_stmt_kind_t tag;
  union {
    ast_stmt_variable_t variable;
    ast_stmt_constant_t constant;
    ast_expr_ref_t expr;
  };
} ast_stmt_t;

typedef struct ASTStmtPool {
  ast_stmt_t *data;
} ast_stmt_pool_t;

typedef struct ASTModule {
  const char *src;
  ast_expr_pool_t expr_pool;
  ast_type_pool_t type_pool;
  ast_stmt_pool_t stmt_pool;
  ast_stmt_ref_t start;
  ast_stmt_ref_t end;
} ast_module_t;

//--------- METHODS ---------//
ast_expr_pool_t new_ast_expr_pool(void);
ast_type_pool_t new_ast_type_pool(void);
ast_stmt_pool_t new_ast_stmt_pool(void);

ast_module_t new_ast_module(void);
void free_ast_module(ast_module_t mod);
ast_expr_ref_t ast_module_add_expr(ast_module_t mod, const ast_expr_t expr);
ast_type_ref_t ast_module_add_type(ast_module_t mod, const ast_type_t type);
ast_stmt_ref_t ast_module_add_stmt(ast_module_t mod, const ast_stmt_t stmt);

void print_ast_module(ast_module_t mod);
void print_ast_expr(ast_module_t pool, ast_expr_ref_t ref);
void print_ast_type(ast_module_t mod, ast_type_ref_t ref);
void print_ast_stmt(ast_module_t mod, ast_stmt_ref_t ref);

#define get_ast_expr(_mod, _ref) (_mod).expr_pool.data[(_ref)]
#define get_ast_type(_mod, _ref) (_mod).type_pool.data[(_ref)]
#define get_ast_stmt(_mod, _ref) (_mod).stmt_pool.data[(_ref)]

#define make_ast_int_lit_expr(...) (ast_expr_t) {.tag = AST_EXPR_INT_LIT, .int_lit = __VA_ARGS__ }
#define make_ast_identifier_lit_expr(...) (ast_expr_t) { .tag = AST_EXPR_IDENTIFIER_LIT, .identifier = __VA_ARGS__ }
#define make_ast_float_lit_expr(...) (ast_expr_t) { .tag = AST_EXPR_FLOAT_LIT, .float_lit = __VA_ARGS__ }
#define make_ast_binary_expr(...) (ast_expr_t) { .tag = AST_EXPR_BINARY, .binary = __VA_ARGS__ }
#define make_ast_assignment_expr(...) (ast_expr_t) { .tag = AST_EXPR_ASSIGNMENT, .assignment = __VA_ARGS__ }
#define make_ast_unary_expr(...) (ast_expr_t) { .tag = AST_EXPR_UNARY, .unary = __VA_ARGS__ }

#define make_ast_stmt_variable(...) (ast_stmt_t) { .tag = AST_STMT_VARIABLE, .variable = __VA_ARGS__, }
#define make_ast_stmt_constant(...) (ast_stmt_t) { .tag = AST_STMT_CONSTANT, .constant = __VA_ARGS__, }
#define make_ast_stmt_expr(...) (ast_stmt_t) { .tag = AST_STMT_EXPR, .expr = __VA_ARGS__, }

#endif // !__AST_H
