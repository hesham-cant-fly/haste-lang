#ifndef __PARSER_H
#define __PARSER_H

#include "ast.h"
#include "error.h"

error parse_tokens(const Token *tokens, const char* path, ASTFile *out);

#endif // !__PARSER_H
