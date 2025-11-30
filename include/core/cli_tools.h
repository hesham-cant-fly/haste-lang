#ifndef __CLI_TOOLS_H
#define __CLI_TOOLS_H

// #define CLI_TOOLS_IMPLEMENTATION

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Subcommand {
  const char *subcommand;
  int (*handle)(int argc, char **argv);
  const char *description;
} subcommand_t;

typedef enum OptionKind {
  OPT_FLAG,
  OPT_STRING,
} option_kind_t;

typedef struct Option {
  const char *long_name;
  char shortname;
  option_kind_t kind;
  void *out;
} option_t;

void print_usage(const char *cli_name, subcommand_t *subcommands, size_t len);
int process_subcommand(int argc, char **argv, subcommand_t *subcommands, size_t len);
bool parse_options(int *argc, char ***argv, option_t *options);

#ifdef CLI_TOOLS_IMPLEMENTATION
void print_usage(const char *cli_name, subcommand_t *subcommands, size_t len) {
  printf("Usage: %s [command] [options]\n\n", cli_name);
  printf("Commands:\n");

  const size_t gap = 3;
  size_t longest_command_size = 0;

  size_t i;
  for (i = 0; i < len; ++i) {
    const subcommand_t subcommand = subcommands[i];
    size_t size = strlen(subcommand.subcommand);
    if (size > longest_command_size) {
      longest_command_size = size;
    }
  }

  for (i = 0; i < len; ++i) {
    const subcommand_t subcommand = subcommands[i];
    printf("  %-*s%s\n", (int)(longest_command_size + gap),
           subcommand.subcommand, subcommand.description);
  }
}

int process_subcommands(int argc, char **argv, subcommand_t *subcommands,
                       size_t len) {
  size_t i;
  for (i = 0; i < len; ++i) {
    const subcommand_t subcommand = subcommands[i];
    if (strcmp(argv[1], subcommand.subcommand) == 0) {
      int result = subcommand.handle(argc - 2, argv + 2);
      return result;
    }
  }

  return 1;  
}

static option_t *find_long(option_t *options, const char *name);
static option_t *find_short(option_t *options, char c);
static int set_option(option_t *o, char *val, int *i, int argc, char **argv);
static int process_long_option(char *arg, int *i, int argc, char **argv,
                               option_t *options);
static int process_short_options(char *arg, int *i, int argc, char **argv,
                                 option_t *options);

bool parse_options(int *argc, char ***argv, option_t *options) {
  int write_idx = 1;
  int i;
  for (i = 1; i < *argc; i++) {
    char *arg = (*argv)[i];

    int j;
    if (strcmp(arg, "--") == 0) { // stop parsing
      for (j = i + 1; j < *argc; j++)
        (*argv)[write_idx++] = (*argv)[j];
      break;
    } else if (strncmp(arg, "--", 2) == 0) {
      if (!process_long_option(arg, &i, *argc, *argv, options))
        return false;
    } else if (arg[0] == '-' && arg[1]) {
      if (!process_short_options(arg, &i, *argc, *argv, options))
        return false;
    } else {
      (*argv)[write_idx++] = arg; // positional
    }
  }
  *argc = write_idx;
  return true;
}

static option_t *find_long(option_t *options, const char *name) {
  option_t *o;
  for (o = options; o->long_name; o++) {
    if (strcmp(o->long_name, name) == 0)
      return o;
  }
  return NULL;
}

static option_t *find_short(option_t *options, char c) {
  option_t *o;
  for (o = options; o->long_name; o++) {
    if (o->shortname == c)
      return o;
  }
  return NULL;
}

static int set_option(option_t *o, char *val, int *i, int argc, char **argv) {
  if (o->kind == OPT_FLAG) {
    *(int *)o->out = 1;
    return 1;
  }
  if (o->kind == OPT_STRING) {
    if (val) {
      *(const char **)o->out = val;
      return 1;
    }
    if (*i + 1 < argc) {
      *(const char **)o->out = argv[++(*i)];
      return 1;
    }
    fprintf(stderr, "Option requires a value: --%s\n", o->long_name);
    return 0;
  }
  return 0;
}

static int process_long_option(char *arg, int *i, int argc, char **argv,
                               option_t *options) {
  char *opt = arg + 2;
  char *val = strchr(opt, '=');
  if (val) {
    *val = '\0';
    val++;
  }

  option_t *o = find_long(options, opt);
  if (!o) {
    fprintf(stderr, "Unknown option: --%s\n", opt);
    return 0;
  }
  return set_option(o, val, i, argc, argv);
}

static int process_long_option(char *arg, int *i, int argc, char **argv,
                               option_t *options);
static int process_short_options(char *arg, int *i, int argc, char **argv,
                                 option_t *options) {
  int j;
  for (j = 1; arg[j]; j++) {
    option_t *o = find_short(options, arg[j]);
    if (!o) {
      fprintf(stderr, "Unknown option: -%c\n", arg[j]);
      return 0;
    }
    char *val = NULL;
    if (o->kind == OPT_STRING) {
      if (arg[j + 1]) {
        val = &arg[j + 1];
        j = strlen(arg) - 1; // consume rest
      }
    }
    if (!set_option(o, val, i, argc, argv))
      return 0;
  }
  return 1;
}

#endif // CLI_TOOLS_IMPLEMENTATION

#endif // !__CLI_TOOLS_H
