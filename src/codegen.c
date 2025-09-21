#include "codegen.h"
#include "ast.h"
#include "error.h"
#include "common.h"
#include "my_string.h"
#include "my_string_view.h"
#include "token.h"

typedef struct GenContext {
  const ast_module_t module;
  String *out;
} gen_context_t;

static error_t generate_c(gen_context_t ctx);
static error_t generate_asm(gen_context_t ctx);

error_t generate(const ast_module_t mod, const codegen_target_t target,
                 String *out) {
  gen_context_t ctx = {
    .module = mod,
    .out = out,
  };
  switch (target) {
  case CODEGEN_TARGET_C:
    return generate_c(ctx);
  case CODEGEN_TARGET_ASM:
    return generate_asm(ctx);
  }
  unreachable0();
  return OK;
}

static error_t generate_c_from_expr(gen_context_t ctx, ast_expr_t *expr);
static error_t generate_c_from_float_lit(gen_context_t ctx, token_t tok);
static error_t generate_c_from_int_lit(gen_context_t ctx, token_t tok);
static error_t generate_c_from_binary_expr(gen_context_t ctx, ast_expr_binary_t bin);

static error_t generate_c(gen_context_t ctx) {
  string_push_cstr(ctx.out, "#include <stdio.h>\n"
                            "#include <math.h>\n\n"
                            "int main(void) {\n"
                            "  double x = ");
  // error_t ok = generate_c_from_expr(ctx, ctx.module.root);
  // if (!ok) return ERROR;
  string_push_cstr(ctx.out, ";\n  printf(\"%lf\\n\", x);\n"
                            "  return x;\n"
                            "}\n");
  return OK;
}

static error_t generate_asm(gen_context_t ctx) {
  ((void)ctx);
  unreachable0();
  return ERROR;
}

static error_t generate_c_from_expr(gen_context_t ctx, ast_expr_t *expr) {
  error_t ok = OK;
  string_push_char(ctx.out, '(');
  switch (expr->tag) {
  case AST_EXPR_FLOAT_LIT: ok = generate_c_from_float_lit(ctx, expr->as.float_lit); break;
  case AST_EXPR_INT_LIT: ok = generate_c_from_int_lit(ctx, expr->as.int_lit); break;
  case AST_EXPR_BINARY: ok = generate_c_from_binary_expr(ctx, expr->as.binary); break;
  default: unreachable0();
  }
  if (!ok) return ERROR;
  string_push_char(ctx.out, ')');
  return OK;
}

static error_t generate_c_from_float_lit(gen_context_t ctx, token_t tok) {
  string_pushf(ctx.out, "%.*s", SVArgs(token_to_string_view(tok, ctx.module.src)));
  return OK;
}

static error_t generate_c_from_int_lit(gen_context_t ctx, token_t tok) {
  string_pushf(ctx.out, "%.*s", SVArgs(token_to_string_view(tok, ctx.module.src)));
  return OK;
}

static error_t generate_c_from_binary_expr(gen_context_t ctx,
                                           ast_expr_binary_t bin) {
  if (bin.op.kind == TOKEN_DOUBLE_STAR) {
    string_push_cstr(ctx.out, "pow(");
    error_t ok = generate_c_from_expr(ctx, bin.lhs);
    if (!ok)
      return ERROR;
    string_push_cstr(ctx.out, ",");
    ok = generate_c_from_expr(ctx, bin.rhs);
    if (!ok)
      return ERROR;
    string_push_cstr(ctx.out, ")");
    return OK;
  }
  error_t ok = generate_c_from_expr(ctx, bin.lhs);
  if (!ok)
    return ERROR;
  switch (bin.op.kind) {
  case TOKEN_PLUS: string_push_cstr(ctx.out, "+"); break;
  case TOKEN_MINUS: string_push_cstr(ctx.out, "-"); break;
  case TOKEN_STAR: string_push_cstr(ctx.out, "*"); break;
  case TOKEN_FSLASH: string_push_cstr(ctx.out, "/"); break;
  default: unreachable0();
  }
  ok = generate_c_from_expr(ctx, bin.rhs);
  if (!ok)
    return ERROR;
  return OK;
}
