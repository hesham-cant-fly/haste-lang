#include "token.h"
#include "core/my_printer.h"

size_t span_len(Span self) {
    return self.end - self.start;
}

Span span_conjoin(Span a, Span b) {
    return (Span){
        .start = a.start,
        .end = b.end,
    };
}

GEN_ENUM_PRINT_IMPL(TokenKind, print_token_kind, TOKEN_KIND_ENUM_DEF)
GEN_STRUCT_PRINT_IMPL(Span, print_span, SPAN_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(Token, print_token, TOKEN_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(Location, print_location, LOCATION_STRUCT_DEF)

GEN_COSTOM_PRINTER_IMPL_START(Span, my_print_span)
    int len = span_len(v);
    fprintf(f, "\"%.*s\"", len, v.start);
GEN_COSTOM_PRINTER_IMPL_END(Span, my_print_span)
