#include "lexer.hpp"
#include "mylib/result.hpp"
#include "mylib/slice.hpp"
#include "mylib/string.hpp"
#include "token.hpp"
#include <cstddef>
#include <cstdio>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
  const char *lexem;
  const TokenKind kind;
} keywords[] = {
    {"const", TokenKind::Const}, {"var", TokenKind::Var},
    {"auto", TokenKind::Auto},   {"int", TokenKind::Int},
    {NULL, (TokenKind)0},
};

struct Lexer {
  size_t line = 1;
  size_t column = 0;
  size_t current_line = 1;
  size_t current_column = 0;
  size_t start = 0;
  string::Iterator iter;
  TokenList tokens;
  const string &src;
  const string &path;
  bool had_error = false;

  static auto make(const string &src, const string &path) -> Result<Lexer, LexerError> {
    auto i = src.utf8_iter();
    if (!i) return LexerError::InvalidUTF8;
    auto iter = i.unwrap();
    iter.len += 1; // Telling the Iterator to not ignore the NUL character.
    return Lexer{
      .iter = iter,
      .tokens = TokenList(),
      .src = src,
      .path = path,
    };
  }

  auto current() const -> std::size_t {
    return iter.current;
  }

  auto scan_lexem() -> void {
    const uint32_t ch = advance();

    switch (ch) {
    case '+': add_token(TokenKind::Plus); break;
    case '-': add_token(TokenKind::Minus); break;
    case '(': add_token(TokenKind::OpenParen); break;
    case ')': add_token(TokenKind::CloseParen); break;
    case '=': add_token(TokenKind::Equal); break;
    case ':': add_token(TokenKind::Colon); break;
    case ';': add_token(TokenKind::Semicolon); break;
    case '*':
      if (match('*'))
        add_token(TokenKind::DoubleStar);
      else
        add_token(TokenKind::Star);
      break;
    case '/':
      if (match('/')) {
        while (peek() != '\n') advance();
        break;
      }
      add_token(TokenKind::FSlash);
      break;
    case '.':
      if (is_numiric(peek())) {
        scan_float();
        return;
      }
      goto err;

    case '\n':
      current_line += 1;
      current_column = 1;
    case ' ':
    case '\t':
    case '\r':
      break;

    default:
      if (is_numiric(ch)) {
        scan_number();
        return;
      }
      if (is_alpha(ch)) {
        scan_identifier();
        return;
      }
    err:
      report_error("Unexpected character `%lc`.", ch);
    }
  }

  auto scan_number() -> void {
    while (is_numiric(peek())) {
      advance();
    }

    if (match('.')) {
      scan_float();
      return;
    }

    add_token(TokenKind::IntLit);
  }

  auto scan_float() -> void {
    while (is_numiric(peek())) {
      advance();
    }
    add_token(TokenKind::FloatLit);
  }

  auto scan_identifier() -> void {
    while (is_alphanum(peek())) {
      advance();
    }

    // NOTE: "What if you had a 1000 keyword?" why would I have this
    //       amount of keywords in the first place?
    for (size_t i = 0; keywords[i].lexem != NULL; ++i) {
      auto span = get_span();
      const unsigned char *lexem = span.lexem.items;
      const size_t len = strlen(keywords[i].lexem);
      if (strncmp((char *)lexem, keywords[i].lexem, len) == 0) {
        add_token( keywords[i].kind);
        return;
      }
    }

    add_token(TokenKind::Identifier);
  }

  auto check(uint32_t ch) const -> bool {
    if (ended()) return false;
    return peek() == ch;
  }

  auto match(uint32_t ch) -> bool {
    if (check(ch)) {
      advance();
      return true;
    }
    return false;
  }

  auto peek() const -> std::uint32_t {
    return iter.peek();
  }

  auto advance() -> std::uint32_t {
    if (ended()) return 0x0;
    auto len_res = iter.next();
    current_column += 1;
    return iter.prev();
  }

  auto get_span() const -> Span {
    return Span{
        .line = line,
        .column = column,
        .lexem = src.bytes().slice(start - 1, current() - 1),
    };
  }

  auto add_token(const TokenKind kind) -> void {
    if (had_error) return;
    tokens.push(Token{
        .span = get_span(),
        .kind = kind,
    });
  }

  auto ended() const -> bool {
    return iter.done();
  }

  static auto is_numiric(uint32_t ch) -> bool { return ch >= '0' && ch <= '9'; }

  static auto is_alpha(uint32_t ch) -> bool {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' ||
           ch == '$';
  }

  static auto is_alphanum(uint32_t ch) -> bool {
    return is_numiric(ch) || is_alpha(ch);
  }

  auto report_error(const char *fmt, ...) -> void {
    had_error = true;
    std::fprintf(stderr, "%s:%zu:%zu: Error: ", path.c_str(), current_line,
            current_column);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
  }
};

auto scan_source(const string &src, const string &path)
  -> Result<TokenList, LexerError> {
  auto lxr_res = Lexer::make(src, path);
  if (!lxr_res) return lxr_res.error();

  auto lxr = lxr_res.unwrap();
  lxr.advance();

  while (!lxr.ended()) {
    lxr.line = lxr.current_line;
    lxr.column = lxr.current_column;
    lxr.start = lxr.current();

    lxr.scan_lexem();
  }

  if (lxr.had_error) {
    lxr.tokens.deinit();
    return LexerError::InvalidToken;
  } else {
    lxr.add_token(TokenKind::Eof);
  }
  return lxr.tokens;
}
