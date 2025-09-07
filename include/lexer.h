#ifndef __LEXER_H
#define __LEXER_H

#include "token.h"
#include <stddef.h>

token_t *scan_source(const char *src, const char *path);

#endif // !__LEXER_H
