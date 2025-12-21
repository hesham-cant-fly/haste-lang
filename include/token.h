#ifndef __TOKEN_H
#define __TOKEN_H

#include "core/my_printer.h"
#include <stdint.h>

enum TokenKind {
#define TOKEN_KIND_ENUM_DEF(X)                                                 \
	X(TOKEN_KIND_TYPEID)                                                       \
    X(TOKEN_KIND_INT)                                                          \
    X(TOKEN_KIND_FLOAT)                                                        \
    X(TOKEN_KIND_AUTO)                                                         \
    X(TOKEN_KIND_CONST)                                                        \
    X(TOKEN_KIND_VAR)                                                          \
    X(TOKEN_KIND_IDENTIFIER)                                                   \
    X(TOKEN_KIND_INT_LIT)                                                      \
    X(TOKEN_KIND_FLOAT_LIT)                                                    \
    X(TOKEN_KIND_COLON)                                                        \
    X(TOKEN_KIND_SEMICOLON)                                                    \
    X(TOKEN_KIND_EQUAL)                                                        \
    X(TOKEN_KIND_OPEN_PAREN)                                                   \
    X(TOKEN_KIND_CLOSE_PAREN)                                                  \
    X(TOKEN_KIND_PLUS)                                                         \
    X(TOKEN_KIND_MINUS)                                                        \
    X(TOKEN_KIND_STAR)                                                         \
    X(TOKEN_KIND_FSLASH)                                                       \
    X(TOKEN_KIND_DOUBLE_STAR)

    TOKEN_KIND_ENUM_DEF(X_ENUM)
};

struct Span {
#define SPAN_STRUCT_DEF(X)                                                     \
    X(char *, start, PRINT_PTR)                                                \
    X(char *, end, PRINT_PTR)

    SPAN_STRUCT_DEF(X_STRUCT)
};

struct Location {
#define LOCATION_STRUCT_DEF(X)                                                 \
    X(uint32_t, line, PRINT_UINT32_T)                                          \
    X(uint32_t, column, PRINT_UINT32_T)

    LOCATION_STRUCT_DEF(X_STRUCT)
};

struct Token {
#define TOKEN_STRUCT_DEF(M)                                                    \
    M(struct Span, span, my_print_span)                                               \
    M(struct Location, location, print_location)                                      \
    M(enum TokenKind, kind, print_token_kind)

    TOKEN_STRUCT_DEF(X_STRUCT)
};

struct TokenList {
	struct Token* items;
	size_t len;
	size_t cap;
};

#define SPAN_ARG(__span) (int)span_len(__span), (__span).start
size_t span_len(struct Span self);
struct Span span_conjoin(struct Span a, struct Span b);

GEN_PRINTER_DEF(enum TokenKind, print_token_kind);
GEN_PRINTER_DEF(struct Span, print_span);
GEN_PRINTER_DEF(struct Span, my_print_span);
GEN_PRINTER_DEF(struct Token, print_token);
GEN_PRINTER_DEF(struct Location, print_location);

#endif /* !__TOKEN_H */
