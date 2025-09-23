#include "ast.h"
#include "common.h"
#include "my_array.h"
#include "token.h"
#include <stdio.h>

#define comma() fprintf(stream, ",")
#define obj_start() fprintf(stream, "{")
#define obj_end() fprintf(stream, "}")
#define json_string(_value) fprintf(stream, "\"%s\"", (_value))
#define json_uint32(_value) fprintf(stream, "%u", (_value))
#define json_token(_token)                                                     \
  do {                                                                         \
    fprint_token_lexem(stream, (_token), src);                                 \
  } while (0)
#define json_object(...)                                                       \
  do {                                                                         \
    obj_start();                                                               \
    __VA_ARGS__;                                                               \
    obj_end();                                                                 \
  } while (0)
#define json_kv(_key, _value)                                                  \
  do {                                                                         \
    fprintf(stream, "\"%s\": ", _key);                                         \
    _value;                                                                    \
    comma();                                                                   \
  } while (0)

static const char *ast_expr_kind_str(ast_expr_kind_t kind);

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

// TODO: Implement those
// void fprint_ast_type(FILE *stream, const ast_stmt_pool_t stmt_pool,
//                      const ast_stmt_ref_t stmt_ref, const char *src) {
// }
// 
// void fprint_ast_stmt(FILE *stream, const ast_stmt_pool_t stmt_pool,
//                      const ast_stmt_ref_t stmt_ref, const char *src) {
//   ast_stmt_t stmt = get_ast_stmt(stmt_pool, stmt_ref);
// }

void fprint_ast_expr(FILE *stream, const ast_expr_pool_t expr_pool,
                     const ast_expr_ref_t expr_ref, const char *src) {
  ast_expr_t expr = get_ast_expr(expr_pool, expr_ref);
  json_object(json_kv("id", json_uint32(expr_ref));
              json_kv("kind", json_string(ast_expr_kind_str(expr.tag)));
              json_kv("node", json_object(switch (expr.tag) {
                        case AST_EXPR_FLOAT_LIT:
                          json_kv("float", json_token(expr.float_lit));
                          break;
                        case AST_EXPR_INT_LIT:
                          json_kv("int", json_token(expr.int_lit));
                          break;
                        case AST_EXPR_IDENTIFIER_LIT:
                          json_kv("identifier",
                                  json_token(expr.identifier));
                          break;
                        case AST_EXPR_BINARY:
                          json_kv("op", json_token(expr.binary.op));
                          json_kv("lhs", fprint_ast_expr(stream, expr_pool, expr.binary.lhs, src));
                          json_kv("rhs", fprint_ast_expr(stream, expr_pool, expr.binary.rhs, src));
                          break;
                        case AST_EXPR_ASSIGNMENT:
                          json_kv("target", fprint_ast_expr(stream, expr_pool, expr.assignment.target, src));
                          json_kv("value", fprint_ast_expr(stream, expr_pool, expr.assignment.value, src));
                          break;
                        case AST_EXPR_UNARY:
                          json_kv("op", json_token(expr.unary.op));
                          json_kv("rhs", fprint_ast_expr(stream, expr_pool, expr.unary.rhs, src));
                          break;
                        default:
                          unreachable0();
                      })));
}

static const char *ast_expr_kind_str(ast_expr_kind_t kind) {
  switch (kind) {
  case AST_EXPR_FLOAT_LIT: return "AST_EXPR_FLOAT_LIT";
  case AST_EXPR_INT_LIT: return "AST_EXPR_INT_LIT";
  case AST_EXPR_IDENTIFIER_LIT: return "AST_EXPR_IDENTIFIER_LIT";
  case AST_EXPR_BINARY: return "AST_EXPR_BINARY";
  case AST_EXPR_ASSIGNMENT: return "AST_EXPR_ASSIGNMENT";
  case AST_EXPR_UNARY: return "AST_EXPR_UNARY";
  case AST_EXPR_BLOCK: return "AST_EXPR_BLOCK";
  }
  unreachable0();
}
