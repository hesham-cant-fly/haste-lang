#include "ast.h"
#include "common.h"
#include "my_array.h"
#include "my_optional.h"
#include "span.h"
#include "token.h"

typedef void (*ast_expr_printer_t)(ast_module_t mod, ast_expr_ref_t expr);
typedef void (*ast_type_printer_t)(ast_module_t mod, ast_type_ref_t expr);
typedef void (*ast_stmt_printer_t)(ast_module_t mod, ast_stmt_ref_t expr);
static const char *ast_expr_kind_str(ast_expr_kind_t kind);
static const char *ast_type_kind_str(ast_type_kind_t kind);
static const char *ast_stmt_kind_str(ast_stmt_kind_t kind);

#define X(enum_name, json_name, handler) static void handler(ast_module_t mod, ast_expr_ref_t ref);
  AST_EXPR_KIND_LIST
#undef X
#define X(enum_name, json_name, handler) static void handler(ast_module_t mod, ast_stmt_ref_t ref);
  AST_TYPE_LIST
#undef X
#define X(enum_name, json_name, handler) static void handler(ast_module_t mod, ast_stmt_ref_t ref);
  AST_STMT_KIND_LIST
#undef X

#define X(enum_name, _, handler) [enum_name] = handler,
static const ast_stmt_printer_t stmt_printers[] = {AST_STMT_KIND_LIST};
static const ast_type_printer_t type_printers[] = {AST_TYPE_LIST};
static const ast_expr_printer_t expr_printers[] = {AST_EXPR_KIND_LIST};
#undef X

void print_ast_module(ast_module_t mod) {
  printf("{");
  printf("\"start\": %u, \"end\": %u, ", mod.start, mod.end);
  printf("\"nodes\": [");
  for (ast_stmt_ref_t i = mod.start; i < mod.end; ++i) {
    print_ast_stmt(mod, i);
    if (i != mod.end - 1) {
      printf(",");
    }
  }
  printf("]");
  printf("}");
}

void print_ast_stmt(ast_module_t mod, ast_stmt_ref_t ref) {
  const ast_stmt_t stmt = get_ast_stmt(mod, ref);
  printf("{");
  printf("\"id\": %u,", ref);
  printf("\"%s\": ", ast_stmt_kind_str(stmt.tag));
  stmt_printers[stmt.tag](mod, ref);
  printf("}");
}

void print_ast_type(ast_module_t mod, ast_type_ref_t ref) {
  const ast_type_t type = get_ast_type(mod, ref);
  printf("{");
  printf("\"id\": %u,", ref);
  printf("\"%s\": ", ast_type_kind_str(type.tag));
  type_printers[type.tag](mod, ref);
  printf("}");
}

static void print_type_auto(ast_module_t mod, ast_stmt_ref_t ref) {
  unused(mod);
  unused(ref);
  printf("\"auto\"");
}

static void print_type_int(ast_module_t mod, ast_stmt_ref_t ref) {
  unused(mod);
  unused(ref);
  printf("\"int\"");
}

static void print_stmt_variable(ast_module_t mod, ast_stmt_ref_t ref) {
  const ast_stmt_t stmt = get_ast_stmt(mod, ref);
  const ast_stmt_variable_t vr = stmt.variable;
  printf("{");
  printf("\"target\": ");
  print_token_lexem(vr.target);
  printf(", \"type\": ");
  if (is_some(vr.type)) {
    print_ast_type(mod, vr.type.value);
  } else {
    printf("undefined");
  }
  printf(", \"value\": ");
  if (is_some(vr.value)) {
    print_ast_expr(mod, vr.value.value);
  } else {
    printf("undefined");
  }
  printf("}");
}

static void print_stmt_constant(ast_module_t mod, ast_stmt_ref_t ref) {
  const ast_stmt_t stmt = get_ast_stmt(mod, ref);
  const ast_stmt_constant_t cons = stmt.constant;
  printf("{");
  printf("\"target\": ");
  print_token_lexem(cons.target);
  printf(", \"type\": ");
  if (is_some(cons.type)) {
    print_ast_type(mod, cons.type.value);
  } else {
    printf("undefined");
  }
  printf(", \"value\": ");
  if (is_some(cons.value)) {
    print_ast_expr(mod, cons.value.value);
  } else {
    printf("undefined");
  }
  printf("}");
}

static void print_stmt_expr(ast_module_t mod, ast_stmt_ref_t ref) {
  print_ast_expr(mod, ref);
}

void print_ast_expr(ast_module_t mod, ast_expr_ref_t ref) {
  printf("{");
  printf("\"id\": %u,", ref);
  const ast_expr_t expr = get_ast_expr(mod, ref);
  printf("\"%s\": ", ast_expr_kind_str(expr.tag));
  if (expr_printers[expr.tag]) {
    expr_printers[expr.tag](mod, ref);
  } else {
    unreachable0();
  }
  printf("}");
}

static void print_token_json(token_t token) {
  printf(
      "\"kind\": \"%s\", \"lexem\": \"%.*s\", \"line\": %zu, \"column\": %zu",
      token_kind_tostr(token.kind), SPANArgs(token.span), token.span.line,
      token.span.column);
}

static void print_expr_float_lit(ast_module_t mod, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(mod, ref);
  print_token(expr.float_lit);
}

static void print_expr_int_lit(ast_module_t mod, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(mod, ref);
  print_token(expr.int_lit);
}

static void print_expr_identifier_lit(ast_module_t mod, ast_expr_ref_t ref) {
  ast_expr_t expr = get_ast_expr(mod, ref);
  print_token_json(expr.identifier);
}

static void print_expr_binary(ast_module_t mod, ast_expr_ref_t ref) {
  printf("{");
  ast_expr_t expr = get_ast_expr(mod, ref);
  ast_expr_binary_t binary = expr.binary;
  printf("\"op\": ");
  print_token_lexem(binary.op);
  printf(", \"lhs\": ");
  print_ast_expr(mod, binary.lhs);
  printf(", \"rhs\": ");
  print_ast_expr(mod, binary.rhs);
  printf("}");
}

static void print_expr_assignment(ast_module_t mod, ast_expr_ref_t ref) {
  printf("{");
  ast_expr_t expr = get_ast_expr(mod, ref);
  ast_expr_assignment_t assignment = expr.assignment;
  printf("\"target\": ");
  print_ast_expr(mod, assignment.target);
  printf(", \"value\": ");
  print_ast_expr(mod, assignment.value);
  printf("}");
}

static void print_expr_unary(ast_module_t mod, ast_expr_ref_t ref) {
  printf("{");
  ast_expr_t expr = get_ast_expr(mod, ref);
  ast_expr_unary_t unary = expr.unary;
  printf("\"op\": ");
  print_token_lexem(unary.op);
  printf(", \"rhs\": ");
  print_ast_expr(mod, unary.rhs);
  printf("}");
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
  return (ast_module_t){
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

#define X(enum_name, json_name, ...)                                           \
  case enum_name:                                                              \
    return json_name;
static const char *ast_expr_kind_str(ast_expr_kind_t kind) {
  switch (kind) { AST_EXPR_KIND_LIST }
  unreachable0();
}

static const char *ast_type_kind_str(ast_type_kind_t kind) {
  switch (kind) { AST_TYPE_LIST }
  unreachable0();
}

static const char *ast_stmt_kind_str(ast_stmt_kind_t kind) {
  switch (kind) { AST_STMT_KIND_LIST }
  unreachable0();
}
#undef X
