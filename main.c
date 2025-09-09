#include "cli_tools.h"
#include "common.h"
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

static int help_handler(int argc, char **argv);
static int version_handler(int argc, char **argv);

subcommand_t subcommands[] = {
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
