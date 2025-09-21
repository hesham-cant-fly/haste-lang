#ifndef __CODEGEN_H
#define __CODEGEN_H

#include "ast.h"
#include "common.h"
#include "error.h"
#include "my_string.h"
#include <stdint.h>

defenum(codegen_target_t, uint8_t,
        {
            CODEGEN_TARGET_C,
            CODEGEN_TARGET_ASM,
        });

error_t generate(const ast_module_t mod, const codegen_target_t target,
                 String *out);

#endif // !__CODEGEN_H
