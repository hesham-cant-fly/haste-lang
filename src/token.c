#include "token.h"
#include "my_string_view.h"
#include "span.h"
#include "common.h"
#include <stdio.h>

string_view_t token_to_string_view(const token_t token) {
  return span_to_string_view(token.span);
}

void print_token(const token_t token) {
  printf("{ \"kind\": \"%s\", \"line\": %zu, \"column\": %zu }", token_kind_tostr(token.kind), token.span.line, token.span.column);
}

void sprint_token(const token_t token, char *out) {
  sprintf(out, "{ \"kind\": %s, \"line\": %zu, \"column\": %zu }", token_kind_tostr(token.kind), token.span.line, token.span.column);
}

void print_token_lexem(const token_t token) {
  printf(
      "{ \"kind\": \"%s\", \"lexem\": \"%.*s\", \"line\": %zu, \"column\": %zu }",
      token_kind_tostr(token.kind), SPANArgs(token.span), token.span.line,
      token.span.column);
}

void fprint_token_lexem(FILE *stream, const token_t token) {
  fprintf(stream, "{ \"kind\": %s, \"lexem\": \"%.*s\", \"line\": %zu, \"column\": %zu }", token_kind_tostr(token.kind),
          SPANArgs(token.span), token.span.line, token.span.column);
}

void sprint_token_lexem(const token_t token, char *out) {
  sprintf(out, "{ %s, '%.*s', (line: %zu, column: %zu) }",
          token_kind_tostr(token.kind), SPANArgs(token.span), token.span.line,
          token.span.column);
}

const char *token_kind_tostr(const token_kind_t kind) {
  switch (kind) {
  case TOKEN_EOF: return "TOKEN_EOF";
  case TOKEN_PLUS: return "TOKEN_PLUS";
  case TOKEN_MINUS: return "TOKEN_MINUS";
  case TOKEN_STAR: return "TOKEN_STAR";
  case TOKEN_FSLASH: return "TOKEN_FSLASH";
  case TOKEN_DOUBLE_STAR: return "TOKEN_DOUBLE_STAR";
  case TOKEN_OPEN_PAREN: return "TOKEN_OPEN_PAREN";
  case TOKEN_CLOSE_PAREN: return "TOKEN_CLOSE_PAREN";
  case TOKEN_EQUAL: return "TOKEN_EQUAL";
  case TOKEN_COLON: return "TOKEN_COLON";
  case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
  case TOKEN_INT_LIT: return "TOKEN_INT_LIT";
  case TOKEN_FLOAT_LIT: return "TOKEN_FLOAT_LIT";
  case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
  case TOKEN_CONST: return "TOKEN_CONST";
  case TOKEN_VAR: return "TOKEN_VAR";
  case TOKEN_AUTO: return "TOKEN_AUTO";
  case TOKEN_INT: return "TOKEN_INT";
  }
  unreachable0();
}
