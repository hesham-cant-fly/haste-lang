#include "ast.h"
#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "my_array.h"
#include "my_string.h"
#include "parser.h"
#include "token.h"
#include <assert.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) return NULL;

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  rewind(f);

  char *result = malloc(size + 1);
  if (result == NULL) {
    fclose(f);
    return NULL;
  }

  fread(result, sizeof(char), size, f);
  result[size] = '\0';
  return result;
}

int main() {
  setlocale(LC_CTYPE, "C.UTF-8");

  char *src = read_file("./main.haste");

  token_t *tokens = scan_source(src, NULL);
  if (tokens == NULL) return EXIT_FAILURE;

  ast_module_t ast;
  Arena arena = {0};
  error_t ok = parse_tokens(tokens, &arena, &ast);
  if (!ok) {
    fprintf(stderr, "Cannot parse the input\n");
    arena_free(&arena);
    arrfree(tokens);
    free(src);
    return EXIT_FAILURE;
  }
  ast.src = src;

  {
    FILE *f = fopen("out.json", "w");
    if (f == NULL) {
      arena_free(&arena);
      arrfree(tokens);
      free(src);
      return EXIT_FAILURE;
    }
    fprint_ast_expr(f, ast.root, src);
    fclose(f);
  }

  String out = string_new(100);
  ok = generate(ast, CODEGEN_TARGET_C, &out);
  if (!ok) {
    fprintf(stderr, "Cannot generate the code\n");
    string_delete(&out);
    arena_free(&arena);
    arrfree(tokens);
    free(src);
    return EXIT_FAILURE;
  }

  {
    FILE *f = fopen("out.c", "w");
    if (f == NULL) {
      string_delete(&out);
      arena_free(&arena);
      arrfree(tokens);
      free(src);
      return EXIT_FAILURE;
    }
    fprintf(f, "%s", out.data);
    fclose(f);
  }

  string_delete(&out);
  arena_free(&arena);
  arrfree(tokens);
  free(src);
  return EXIT_SUCCESS;
}
