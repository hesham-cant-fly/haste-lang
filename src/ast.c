#include "ast.h"
#include "common.h"
#include "my_array.h"
#include "span.h"
#include "token.h"

typedef void (*ast_expr_printer_t)(ast_expr_pool_t pool, ast_expr_ref_t expr);
static const char *ast_expr_kind_str(ast_expr_kind_t kind);

#define X(enum_name, json_name, handler) static void handler(ast_expr_pool_t pool, ast_expr_ref_t expr);
  AST_EXPR_KIND_LIST
#undef X

static const ast_expr_printer_t printers[] = {
#define X(enum_name, json_name, handler) [enum_name] = handler,
  AST_EXPR_KIND_LIST
#undef X
};

void print_expr(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  printf("{");
  printf("\"id\": %u,", ref);
  const ast_expr_t expr = get_ast_expr(pool, ref);
  printf("\"%s\": {", ast_expr_kind_str(expr.tag));
  if (printers[expr.tag]) {
    printers[expr.tag](pool, ref);
  } else {
    unreachable0();
  }
  printf("}");
  printf("}");
}

static void print_token_json(token_t token) {
  printf("\"kind\": \"%s\", \"lexem\": \"%.*s\", \"line\": %zu, \"column\": %zu",
         token_kind_tostr(token.kind), SPANArgs(token.span), token.span.line, token.span.column);
}

static void print_expr_float_lit(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  print_token_json(expr.float_lit);
}

static void print_expr_int_lit(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  print_token_json(expr.int_lit);
}

static void print_expr_identifier_lit(ast_expr_pool_t pool,
                                      ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  print_token_json(expr.identifier);
}

static void print_expr_binary(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  ast_expr_binary_t binary = expr.binary;
  printf("\"op\": ");
  print_token_lexem(binary.op);
  printf(", \"lhs\": ");
  print_expr(pool, binary.lhs);
  printf(", \"rhs\": ");
  print_expr(pool, binary.rhs);
}

static void print_expr_assignment(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  ast_expr_assignment_t assignment = expr.assignment;
  printf("\"target\": ");
  print_expr(pool, assignment.target);
  printf(", \"value\": ");
  print_expr(pool, assignment.value);
}

static void print_expr_unary(ast_expr_pool_t pool, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(pool, ref);
  ast_expr_unary_t unary = expr.unary;
  printf("\"op\": ");
  print_token_lexem(unary.op);
  printf(", \"rhs\": ");
  print_expr(pool, unary.rhs);
}

ast_expr_pool_t new_ast_expr_pool(void) {
  return (ast_expr_pool_t){.data = arrinit(ast_expr_t)};
}

ast_type_pool_t new_ast_type_pool(void) {
  return (ast_type_pool_t){.data = arrinit(ast_type_t)};
}

ast_stmt_pool_t new_ast_stmt_pool(void) {
  return (ast_stmt_pool_t){.data = arrinit(ast_stmt_t)};
}

ast_module_t new_ast_module(void) {
  return (ast_module_t) {
    .src = NULL,
    .expr_pool = new_ast_expr_pool(),
    .type_pool = new_ast_type_pool(),
    .stmt_pool = new_ast_stmt_pool(),
  };
}

void free_ast_module(ast_module_t mod) {
  arrfree(mod.expr_pool.data);
  arrfree(mod.type_pool.data);
  arrfree(mod.stmt_pool.data);
}

ast_expr_ref_t ast_module_add_expr(ast_module_t mod, const ast_expr_t expr) {
  arrpush(mod.expr_pool.data, expr);
  return arrlen(mod.expr_pool.data) - 1;
}

ast_type_ref_t ast_module_add_type(ast_module_t mod, const ast_type_t type) {
  arrpush(mod.type_pool.data, type);
  return arrlen(mod.type_pool.data) - 1;
}

ast_stmt_ref_t ast_module_add_stmt(ast_module_t mod, const ast_stmt_t stmt) {
  arrpush(mod.stmt_pool.data, stmt);
  return arrlen(mod.stmt_pool.data) - 1;
}

static const char *ast_expr_kind_str(ast_expr_kind_t kind) {
  switch (kind) {
#define X(enum_name, json_name, ...) case enum_name: return json_name;
    AST_EXPR_KIND_LIST
#undef X
  }
  unreachable0();
}
