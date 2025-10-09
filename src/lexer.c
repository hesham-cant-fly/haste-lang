#include "lexer.h"
#include "common.h"
#include "my_array.h"
#include "span.h"
#include "token.h"
#include "utf8.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Lexer {
  size_t line;
  size_t column;
  size_t current_line;
  size_t current_column;
  size_t start;
  size_t current;
  utf8_iter_t iter;
  token_t *tokens;
  const char *src;
  const char *path;
  bool had_error;
} lexer_t;

static const struct {
  const char *lexem;
  const token_kind_t kind;
} keywords[] = {
  { "const", TOKEN_CONST },
  { "var", TOKEN_VAR },
  { "auto", TOKEN_AUTO },
  { "int", TOKEN_INT },
  { NULL, 0 },
};

static bool check(const lexer_t *lxr, uint32_t ch);
static bool match(lexer_t *lxr, uint32_t ch);
static uint32_t peek(const lexer_t *lxr);
static uint32_t advance(lexer_t *lxr);

static void scan_lexem(lexer_t *lxr);
static void scan_number(lexer_t *lxr);
static void scan_float(lexer_t *lxr);
static void scan_identifier(lexer_t *lxr);

static span_t get_span(const lexer_t *lxr);
static void add_token(lexer_t *lxr, const token_kind_t kind);
static bool ended(const lexer_t *lxr);
static bool is_numiric(const uint32_t ch);
static bool is_alpha(const uint32_t ch);
static bool is_alphanum(const uint32_t ch);

static void report_error(lexer_t *lxr, const char *restrict fmt, ...);

token_t *scan_source(const char *src, const char *path) {
  lexer_t lxr = {
      .start = 0,
      .current = 0,
      .line = 1,
      .current_line = 1,
      .column = 1,
      .current_column = 1,
      .src = src,
      .had_error = false,
      .tokens = arrinit(token_t),
      .path = path,
  };
  if (lxr.tokens == NULL) return NULL;
  if (!utf8_iter_init(&lxr.iter, src)) {
    fprintf(stderr, "Invalid UTF-8\n");
    return NULL;
  }

  while (!ended(&lxr)) {
    lxr.line = lxr.current_line;
    lxr.column = lxr.current_column;
    lxr.start = lxr.current;

    scan_lexem(&lxr);
  }

  if (lxr.had_error) {
    arrfree(lxr.tokens);
    lxr.tokens = NULL;
  } else {
    add_token(&lxr, TOKEN_EOF);
  }
  return lxr.tokens;
}

static void scan_lexem(lexer_t *lxr) {
  const uint32_t ch = advance(lxr);

  switch (ch) {
  case '+': add_token(lxr, TOKEN_PLUS); break;
  case '-': add_token(lxr, TOKEN_MINUS); break;
  case '*': match(lxr, '*') ? add_token(lxr, TOKEN_DOUBLE_STAR) : add_token(lxr, TOKEN_STAR); break;
  case '/':
    if (match(lxr, '/')) {
      while (peek(lxr) != '\n') advance(lxr);
      break;
    }
    add_token(lxr, TOKEN_FSLASH);
    break;
  case '(': add_token(lxr, TOKEN_OPEN_PAREN); break;
  case ')': add_token(lxr, TOKEN_CLOSE_PAREN); break;
  case '.':
    if (is_numiric(peek(lxr))) {
      scan_float(lxr);
      return;
    }
    goto err;
  case '=': add_token(lxr, TOKEN_EQUAL); break;
  case ':': add_token(lxr, TOKEN_COLON); break;
  case ';': add_token(lxr, TOKEN_SEMICOLON); break;

  case '\n':
    lxr->current_line += 1;
    lxr->current_column = 1;
  case ' ':
  case '\t':
  case '\r':
    break;

  default:
    if (is_numiric(ch)) {
      scan_number(lxr);
      return;
    }
    if (is_alpha(ch)) {
      scan_identifier(lxr);
      return;
    }
    err:
      report_error(lxr, "Unexpected character `%lc`.", ch);
  }
}

static void scan_number(lexer_t *lxr) {
  while (is_numiric(peek(lxr))) {
    advance(lxr);
  }

  if (match(lxr, '.')) {
    scan_float(lxr);
    return;
  }

  add_token(lxr, TOKEN_INT_LIT);
}

static void scan_float(lexer_t *lxr) {
  while (is_numiric(peek(lxr))) {
    advance(lxr);
  }
  add_token(lxr, TOKEN_FLOAT_LIT);
}

static void scan_identifier(lexer_t *lxr) {
  while (is_alphanum(peek(lxr))) {
    advance(lxr);
  }

  // NOTE: "What if you had a 1000 keyword?" why would I have this
  //       amount of keywords in the first place?
  for (size_t i = 0; keywords[i].lexem != NULL; ++i) {
    span_t span = get_span(lxr);
    const char *lexem = span.start;
    const size_t len = strlen(keywords[i].lexem);
    if (strncmp(lexem, keywords[i].lexem, len) == 0) {
      add_token(lxr, keywords[i].kind);
      return;
    }
  }

  add_token(lxr, TOKEN_IDENTIFIER);
}

static bool check(const lexer_t *lxr, uint32_t ch) {
  if (ended(lxr)) return false;
  return peek(lxr) == ch;
}

static bool match(lexer_t *lxr, uint32_t ch) {
  if (check(lxr, ch)) {
    advance(lxr);
    return true;
  }
  return false;
}

static uint32_t peek(const lexer_t *lxr) {
  return utf8_peek(lxr->iter);
}

static uint32_t advance(lexer_t *lxr) {
  if (ended(lxr)) return 0x0;
  int len = utf8_next(&lxr->iter);
  if (len == -1) unreachable0();
  lxr->current += len;
  lxr->current_column += 1;
  return utf8_prev(lxr->iter);
}

static span_t get_span(const lexer_t *lxr) {
  return (span_t) {
    .line = lxr->line,
    .column = lxr->column,
    .start = (char *)lxr->src + lxr->start,
    .len = lxr->current - lxr->start,
  };
}

static void add_token(lexer_t *lxr, const token_kind_t kind) {
  if (lxr->had_error) // no need to add push new tokens at this case
    return;
  token_t token = {
    .kind = kind,
    .span = get_span(lxr),
  };
  arrpush(lxr->tokens, token);
}

static bool ended(const lexer_t *lxr) {
  return lxr->src[lxr->current] == 0x0;
}

static bool is_numiric(const uint32_t ch) {
  return ch >= '0' && ch <= '9';
}

static bool is_alpha(const uint32_t ch) {
  if (ch >= 'a' && ch <= 'z') return true;
  if (ch >= 'A' && ch <= 'Z') return true;
  return ch == '_' || ch == '$';
}

static bool is_alphanum(const uint32_t ch) {
  return is_numiric(ch) || is_alpha(ch);
}

static void report_error(lexer_t *lxr, const char *restrict fmt, ...) {
  lxr->had_error = true;
  fprintf(stderr, "%s:%zu:%zu: Error: ", lxr->path, lxr->current_line, lxr->current_column);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  printf("\n");
}
