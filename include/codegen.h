#ifndef __CODEGEN_H
#define __CODEGEN_H

#include "ast.h"
#include "error.h"
#include "my_string.h"
#include <stdint.h>

typedef enum CodegenTarget {
  CODEGEN_TARGET_C,
  CODEGEN_TARGET_ASM,
} codegen_target_t;

error_t generate(const ast_module_t mod, const codegen_target_t target,
                 String *out);

#endif // !__CODEGEN_H
