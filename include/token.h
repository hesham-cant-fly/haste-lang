#ifndef __TOKEN_H
#define __TOKEN_H

#include "common.h"
#include "my_string_view.h"
#include "span.h"
#include <stdint.h>
#include <stdio.h>

defenum(token_kind_t, uint8_t,
        {
            TOKEN_EOF = 1,

            TOKEN_PLUS,        // +
            TOKEN_MINUS,       // -
            TOKEN_STAR,        // *
            TOKEN_FSLASH,      // /
            TOKEN_DOUBLE_STAR, // **
            TOKEN_OPEN_PAREN,  // (
            TOKEN_CLOSE_PAREN, // )

            TOKEN_EQUAL, // =
            TOKEN_COLON, // :
            TOKEN_SEMICOLON, // ;

            TOKEN_INT_LIT,
            TOKEN_FLOAT_LIT, // eg. 1.2, .1
            TOKEN_IDENTIFIER,

            TOKEN_CONST,
            TOKEN_VAR,
            TOKEN_AUTO,
        });

typedef struct Token {
  span_t span;
  token_kind_t kind;
} token_t;

string_view_t token_to_string_view(const token_t token, const char *src);
void print_token(const token_t token);
void sprint_token(const token_t token, char *out);
void print_token_lexem(const token_t token, const char *src);
void fprint_token_lexem(FILE *stream, const token_t token, const char *src);
void sprint_token_lexem(const token_t token, const char *src, char *out);
const char *token_kind_tostr(const token_kind_t kind);

#endif // !__TOKEN_H
