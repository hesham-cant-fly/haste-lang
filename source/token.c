#include "haste.h"

static const char *TOKEN_KIND_LIT[] =
{
	[0]               = "unknown",
	[TK_IDENT]        = "ident",
	[TK_KW_STRING]    = "kw_string",   // "string"
	[TK_KW_CSTR]      = "kw_cstr",     // "cstr"
	[TK_KW_INT]       = "kw_int",      // "int"
	[TK_KW_FLOAT]     = "kw_float",    // "float"
	[TK_KW_VOID]      = "kw_void",     // "void"
	[TK_KW_AUTO]      = "kw_auto",     // "auto"
	[TK_KW_TYPE]      = "kw_type",     // "type"
	[TK_KW_CAST]      = "kw_cast",     // "cast"
	[TK_KW_CONST]     = "kw_const",    // "const"
	[TK_KW_VAR]       = "kw_var",      // "var"

	[TK_SEMI_COLON]   = "semi_colon",  // ";"

	[TK_OPEN_BRAKET]  = "[",           // "["
	[TK_CLOSE_BRAKET] = "]",           // "]"
	[TK_OPEN_PAREN]   = "open_paren",  // "("
	[TK_CLOSE_PAREN]  = "close_paren", // ")"

	[TK_COLON]        = "colon",       // ":"
	[TK_EQ]           = "eq",          // "="

	[TK_PLUS]         = "plus",        // "+"
	[TK_MINUS]        = "minus",       // "-"
	[TK_STAR]         = "star",        // "*"
	[TK_FSLASH]       = "fslash",      // "/"

	[TK_INT]          = "int",
	[TK_FLOAT]        = "float",
	[TK_STR]          = "str",
	[TK_EOF]          = "eof",
};

struct span token_to_span(struct token token)
{
	return span(token.start, token.len);
}

int print_token(stream_t f, struct token token)
{
	return sprint(f, "Token({s}, {span:#})", TOKEN_KIND_LIT[token.kind], token_to_span(token));

}
