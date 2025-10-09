#ifndef __TOKEN_H
#define __TOKEN_H

#include "mylib/dynamic-array.hpp"
#include "mylib/slice.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class TokenKind : std::uint8_t {
  Eof = 1,

  Plus,        // +
  Minus,       // -
  Star,        // *
  FSlash,      // /
  DoubleStar, // **
  OpenParen,  // (
  CloseParen, // )

  Equal,     // =
  Colon,     // :
  Semicolon, // ;

  IntLit,
  FloatLit, // eg. 1.2, .1
  Identifier,

  Const,
  Var,
  Auto,
  Int,
};

struct Span {
  std::size_t line;
  std::size_t column;
  Slice<unsigned char> lexem;
};

struct Token {
  Span span;
  TokenKind kind;

  const Slice<unsigned char> get_lexem() const;
  void print(std::FILE *stream = stdout) const;
};

using TokenList = ArrayList<Token>;

const char *token_kind_tostr(const TokenKind kind);

#endif // !__TOKEN_H
