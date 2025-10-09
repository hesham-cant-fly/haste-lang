#include "token.hpp"
#include "mylib/slice.hpp"

const Slice<unsigned char> Token::get_lexem() const {
  return span.lexem;
}

void Token::print(std::FILE *stream) const {
  std::fprintf(stream,
      "(token :kind \"%s\" :lexem \"%.*s\" :line %zu :column %zu )",
      token_kind_tostr(kind), (int)span.lexem.len, span.lexem.items,
      span.line, span.column);
}

const char *token_kind_tostr(const TokenKind kind) {
  switch (kind) {
  case TokenKind::Eof: return "TOKEN_EOF";
  case TokenKind::Plus: return "TOKEN_PLUS";
  case TokenKind::Minus: return "TOKEN_MINUS";
  case TokenKind::Star: return "TOKEN_STAR";
  case TokenKind::FSlash: return "TOKEN_FSLASH";
  case TokenKind::DoubleStar: return "TOKEN_DOUBLE_STAR";
  case TokenKind::OpenParen: return "TOKEN_OPEN_PAREN";
  case TokenKind::CloseParen: return "TOKEN_CLOSE_PAREN";
  case TokenKind::Equal: return "TOKEN_EQUAL";
  case TokenKind::Colon: return "TOKEN_COLON";
  case TokenKind::Semicolon: return "TOKEN_SEMICOLON";
  case TokenKind::IntLit: return "TOKEN_INT_LIT";
  case TokenKind::FloatLit: return "TOKEN_FLOAT_LIT";
  case TokenKind::Identifier: return "TOKEN_IDENTIFIER";
  case TokenKind::Const: return "TOKEN_CONST";
  case TokenKind::Var: return "TOKEN_VAR";
  case TokenKind::Auto: return "TOKEN_AUTO";
  case TokenKind::Int: return "TOKEN_INT";
  }
}
