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

static error_t generate_c(gen_context_t ctx) {
  ((void)ctx);
  unreachable0();
  return ERROR;
}

static error_t generate_asm(gen_context_t ctx) {
  ((void)ctx);
  unreachable0();
  return ERROR;
}
