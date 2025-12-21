#include "token.h"
#include "core/my_printer.h"

size_t span_len(struct Span self)
{
    return self.end - self.start;
}

struct Span span_conjoin(struct Span a, struct Span b)
{
    return (struct Span){
        .start = a.start,
        .end = b.end,
    };
}

GEN_ENUM_PRINT_IMPL(enum TokenKind, print_token_kind, TOKEN_KIND_ENUM_DEF)
GEN_STRUCT_PRINT_IMPL(struct Span, print_span, SPAN_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct Token, print_token, TOKEN_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct Location, print_location, LOCATION_STRUCT_DEF)

GEN_COSTOM_PRINTER_IMPL_START(struct Span, my_print_span)
    int len = span_len(v);
    fprintf(f, "\"%.*s\"", len, v.start);
GEN_COSTOM_PRINTER_IMPL_END(struct Span, my_print_span)
