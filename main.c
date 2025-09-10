#include "ast.h"
#include "cli_tools.h"
#include "codegen.h"
#include "common.h"
#include "error.h"
#include "lexer.h"
#include "my_string.h"
#include "parser.h"
#include "token.h"
#include "my_array.h"
#include <assert.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

error_t write_to_file(const char *restrict path, const char *restrict content) {
  FILE *f = fopen(path, "w");
  if (f == NULL) return ERROR;
  fprintf(f, "%s", content);
  fclose(f);
  return OK;
}

static int compile_handler(int argc, char **argv);
static int help_handler(int argc, char **argv);
static int version_handler(int argc, char **argv);

subcommand_t subcommands[] = {
  {"c", compile_handler, "Compile a haste file"},

  {"help", help_handler, "Prints this and then exits."},
  {"version", version_handler, "Prints the version of the compiler."},
};
const size_t subcommands_len = sizeof(subcommands) / sizeof(subcommand_t);

static int help_handler(int argc, char **argv) {
  unused(argc);
  print_usage(argv[-2], subcommands, subcommands_len);
  return EXIT_STATUS_SUCCESS;
}

static int version_handler(int argc, char **argv) {
  unused(argc);
  unused(argv);
  printf("%s\n", VERSION);
  return EXIT_STATUS_SUCCESS;
}

static int compile_handler(int argc, char **argv) {
  char *out = "out.c";
  bool verbose = false;
  option_t options[] = {
    { "output", 'o', OPT_STRING, &out },
    { "verbose", 'v', OPT_FLAG, &verbose },
    { NULL, 0, 0, NULL },
  };

  if (!parse_options(&argc, &argv, options))
    return EXIT_STATUS_OTHER_FAILURE;

  if (argc == 0) {
    fprintf(stderr, "Expected a file as an argument.\n");
    return EXIT_STATUS_OTHER_FAILURE;
  }

  char *content = read_file(argv[0]);
  if (content == NULL) {
    fprintf(stderr, "Cannot read `%s`.", argv[0]);
    return EXIT_STATUS_OTHER_FAILURE;
  }

  token_t *tokens = scan_source(content, argv[0]);
  if (tokens == NULL) {
    free(content);
    return EXIT_STATUS_OTHER_FAILURE;
  }

  ast_module_t mod = { .src = content };
  Arena arena = {0};

  error_t ok = parse_tokens(tokens, &arena, &mod);
  if (!ok) {
    free(content);
    arrfree(tokens);
    arena_free(&arena);
    return EXIT_STATUS_OTHER_FAILURE;
  }
  mod.src = content;

  // fprint_ast_expr(stderr, mod.root, mod.src);

  String output_stream = string_new(100);
  codegen_target_t target = CODEGEN_TARGET_C;
  ok = generate(mod, target, &output_stream);
  if (!ok) {
    free(content);
    arrfree(tokens);
    string_delete(&output_stream);
    arena_free(&arena);
    return EXIT_STATUS_OTHER_FAILURE;
  }

  ok = write_to_file(out, output_stream.data);
  if (!ok) {
    fprintf(stderr, "Cannot write to `%s`\n", out);
    free(content);
    arrfree(tokens);
    string_delete(&output_stream);
    arena_free(&arena);
    return EXIT_STATUS_OTHER_FAILURE;
  }

  free(content);
  arrfree(tokens);
  string_delete(&output_stream);
  arena_free(&arena);
  return EXIT_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
  setlocale(LC_CTYPE, "C.UTF-8");

  if (argc < 2) {
    print_usage(argv[0], subcommands, subcommands_len);
    return EXIT_FAILURE;
  }

  int status = process_subcommands(argc, argv, subcommands, subcommands_len);
  if (status == EXIT_STATUS_SUBCOMMAND_NOT_FOUND) {
    print_usage(argv[0], subcommands, subcommands_len);
  }

  return status;
}
