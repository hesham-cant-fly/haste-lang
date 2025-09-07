#include "ast.h"
#include "common.h"
#include "token.h"
#include <stdio.h>

#define comma() fprintf(stream, ",")
#define obj_start() fprintf(stream, "{")
#define obj_end() fprintf(stream, "}")
#define json_string(_value) fprintf(stream, "\"%s\"", (_value))
#define json_token(_token)                                                     \
  do {                                                                         \
    fprintf(stream, "\"");                                                     \
    fprint_token_lexem(stream, (_token), src);                                 \
    fprintf(stream, "\"");                                                     \
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

void fprint_ast_expr(FILE *stream, const ast_expr_t *expr, const char *src) {
  json_object(json_kv("kind", json_string(ast_expr_kind_str(expr->tag)));
              json_kv("node", json_object(switch (expr->tag) {
                        case AST_EXPR_FLOAT_LIT:
                          json_kv("float", json_token(expr->as.float_lit));
                          break;
                        case AST_EXPR_INT_LIT:
                          json_kv("int", json_token(expr->as.int_lit));
                          break;
                        case AST_EXPR_BINARY:
                          json_kv("op", json_token(expr->as.binary.op));
                          json_kv("lhs", fprint_ast_expr(stream, expr->as.binary.lhs, src));
                          json_kv("rhs", fprint_ast_expr(stream, expr->as.binary.rhs, src));
                          break;
                        default:
                          unreachable0();
                      })));
}

static const char *ast_expr_kind_str(ast_expr_kind_t kind) {
  switch (kind) {
  case AST_EXPR_FLOAT_LIT: return "AST_EXPR_FLOAT_LIT";
  case AST_EXPR_INT_LIT: return "AST_EXPR_INT_LIT";
  case AST_EXPR_BINARY: return "AST_EXPR_BINARY";
  }
  unreachable0();
}
