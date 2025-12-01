#include "scanner.h"
#include "core/my_commons.h"
#include "error.h"
#include "token.h"
#include "utf-8.h"
#include "core/my_array.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Scanner {
    Token *tokens;
    utf8_iter iter;
    size_t start;
    Location current_location;
    bool had_error;
    bool ended;
} Scanner;

typedef struct KeywordEntry {
    const char *lexem;
    TokenKind kind;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    { "int", TOKEN_KIND_INT },
    { "float", TOKEN_KIND_FLOAT },
    { "auto", TOKEN_KIND_AUTO },
    { "const", TOKEN_KIND_CONST },
    { "var", TOKEN_KIND_VAR },
    { 0, 0 },
};

static uint32_t peek(const Scanner *self);
static uint32_t advance(Scanner *self);
static bool check(const Scanner *self, uint32_t ch);
static bool match(Scanner *self, uint32_t ch);
static bool ended(const Scanner *self);

static void skip_whitespaces(Scanner *self);

static void scan_lexem(Scanner *self);
static void add_token(Scanner *self, TokenKind kind, Location start_location);
static Span get_span(Scanner *self);

error scan_entire_cstr(const char *content, Token **out) {
    Scanner scanner = {0};
    utf8_init(&scanner.iter, content);
    advance(&scanner); // initial advance
    scanner.current_location = (Location) {
        .column = 1,
        .line = 1,
    };

    scanner.tokens = arrinit(Token);
    if (scanner.tokens == NULL) panic("Buy more RAM lol");

    while (!ended(&scanner)) {
        skip_whitespaces(&scanner);
        if (ended(&scanner)) break;
        scanner.start = scanner.iter.position;
        scan_lexem(&scanner);
    }

    if (scanner.had_error) {
        arrfree(scanner.tokens);
        *out = NULL;
        return ERROR;
    }
    *out = scanner.tokens;
    return OK;
}

static void skip_whitespaces(Scanner *self) {
    while (!ended(self)) {
        uint32_t ch = peek(self);
        switch (ch) {
        case '\n':
            self->current_location.line += 1;
            self->current_location.column = 0;
        case ' ':
        case '\0':
        case '\t':
        case '\v':
        case '\r':
            ch = advance(self);
            break;
        default:
            return;
        }
    }
}

static bool is_identifier_starter(uint32_t ch) {
    if (isalpha(ch)) return true;
    return ch == '_' || ch == '$';
}

static bool is_symbol(uint32_t ch) {
    return is_identifier_starter(ch) || isdigit(ch);
}

static void scan_number(Scanner *self, const Location location);
static void scan_identifier(Scanner *self, const Location location);

static void scan_lexem(Scanner *self) {
    const Location location = self->current_location;
    const uint32_t ch = advance(self);
    switch (ch) {
    case '+': add_token(self, TOKEN_KIND_PLUS, location); break;
    case '-': add_token(self, TOKEN_KIND_MINUS, location); break;
    case '*':
        if (match(self, '*')) {
            add_token(self, TOKEN_KIND_DOUBLE_STAR, location);
        } else {
            add_token(self, TOKEN_KIND_STAR, location);
        }
        break;
    case '/': add_token(self, TOKEN_KIND_FSLASH, location); break;
    case '(': add_token(self, TOKEN_KIND_OPEN_PAREN, location); break;
    case ')': add_token(self, TOKEN_KIND_CLOSE_PAREN, location); break;
    case ':': add_token(self, TOKEN_KIND_COLON, location); break;
    case ';': add_token(self, TOKEN_KIND_SEMICOLON, location); break;
    case '=': add_token(self, TOKEN_KIND_EQUAL, location); break;
    default:
        if (isdigit(ch)) {
            scan_number(self, location);
        } else if (is_identifier_starter(ch)) {
            scan_identifier(self, location);
        } else {
            self->had_error = true;
            fprintf(stderr, "huh: '%lc'\n", ch);
        }
        break;
    }
}

static void scan_number(Scanner *self, const Location location) {
    while (!ended(self)) {
        const uint32_t ch = peek(self);
        if (!isdigit(ch)) {
            break;
        }
        advance(self);
    }

    bool is_float = false;
    if (match(self, '.')) {
        is_float = true;
        while (!ended(self)) {
            uint32_t ch = peek(self);
            if (!isdigit(ch)) {
                break;
            }
            advance(self);
        }
    }

    if (is_float) {
        add_token(self, TOKEN_KIND_FLOAT_LIT, location);
    } else {
        add_token(self, TOKEN_KIND_INT_LIT, location);
    }
}

static void scan_identifier(Scanner *self, const Location location) {
    while (!ended(self)) {
        const uint32_t ch = peek(self);
        if (!is_symbol(ch)) {
            break;
        }
        advance(self);
    }

    TokenKind kind = TOKEN_KIND_IDENTIFIER;
    const Span span = get_span(self);
    const size_t lexem_len = span_len(span);
    for (size_t i = 0; keywords[i].lexem != NULL; i += 1) {
        const KeywordEntry entry = keywords[i];
        const size_t len = strlen(entry.lexem);
        if (len != lexem_len) continue;
        if (strncmp(entry.lexem, span.start, len) == 0) {
            kind = entry.kind;
            break;
        }
    }

    add_token(self, kind, location);  
}

static void add_token(Scanner *self, TokenKind kind, Location start_location) {
    if (self->had_error) return; // No need for new allocations
    Span span = get_span(self);
    Token token = {
        .kind = kind,
        .span = span,
        .location = start_location,
    };
    arrpush(self->tokens, token);
}

static Span get_span(Scanner *self) {
    return (Span) {
        .start = (char*)self->iter.ptr + self->start,
        .end = (char*)self->iter.ptr + self->iter.position,
    };
}

static uint32_t peek(const Scanner *self) {
    return self->iter.codepoint;
}

static uint32_t advance(Scanner *self) {
    uint32_t prev = self->iter.codepoint;
    if (!ended(self)) {
        self->ended = !utf8_next(&self->iter);
        self->current_location.column += 1;
    }
    return prev;
}

static bool check(const Scanner *self, uint32_t ch) {
    if (ended(self)) {
        return false;
    }
    return peek(self) == ch;
}

static bool match(Scanner *self, uint32_t ch) {
    if (check(self, ch)) {
        advance(self);
        return true;
    }
    return false;
}

static bool ended(const Scanner *self) {
    return self->ended;
}
