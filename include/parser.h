#ifndef __PARSER_H
#define __PARSER_H

#include "ast.h"
#include "error.h"

error parse_tokens(const struct TokenList tokens,
				   const char* path,
				   struct AstFile *out,
				   Arena *out_arena);

#endif // !__PARSER_H
