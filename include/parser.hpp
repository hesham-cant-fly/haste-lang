#ifndef __PARSER_H
#define __PARSER_H

#include "ast.hpp"
#include "mylib/result.hpp"
#include "mylib/string.hpp"
#include "token.hpp"

enum class ParserError {
  InvalidToken,
};

auto parse_tokens(TokenList tokens, string &src,
                  string &path) -> Result<AST::Module, ParserError>;

#endif // !__PARSER_H
