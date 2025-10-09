#ifndef __LEXER_H
#define __LEXER_H

#include "mylib/result.hpp"
#include "mylib/string.hpp"
#include "token.hpp"
#include <stddef.h>

enum class LexerError {
  InvalidUTF8,
  InvalidToken,
};

auto scan_source(const string &src, const string &path)
    -> Result<TokenList, LexerError>;

#endif // !__LEXER_H
