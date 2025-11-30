#include "ast.h"
#include "core/my_array.h"
#include "error.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"

#include <locale.h>
#include <stddef.h>
#include <stdint.h>

int main(void) {
    setlocale(LC_ALL, "C.UTF-8");
    const char *src = "int + float + auto";
    Token *tokens = NULL;
    error err = scan_entire_cstr(src, &tokens);
    if (err) return 1;

    ASTFile ast = {0};
    err = parse_tokens(tokens, &ast);
    if (err) {
        arrfree(tokens);
        return 1;
    }

    print_ast_fileln(stdout, ast);

    arrfree(tokens);
    arena_free(&ast.arena);
    return 0;
}
