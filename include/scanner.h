#ifndef __SCANNER_H
#define __SCANNER_H

#include "token.h"
#include "error.h"

error scan_entire_cstr(const char *content, const char* path, struct TokenList *out);

#endif // !__SCANNER_H
