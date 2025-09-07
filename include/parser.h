#ifndef __PARSER_H
#define __PARSER_H

#include "arena.h"
#include "error.h"
#include "token.h"
#include "ast.h"
#include <stddef.h>

error_t parse_tokens(const token_t *restrict tokens, Arena *restrict arena,
                     ast_module_t *out);

#endif // !__PARSER_H
