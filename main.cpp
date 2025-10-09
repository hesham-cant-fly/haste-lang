#include "ast.hpp"
#include "common.hpp"
#include "lexer.hpp"
#include "mylib/result.hpp"
#include "mylib/string.hpp"
#include "parser.hpp"
#include "token.hpp"
#include <clocale>
#include <cstdio>

int main() {
  std::setlocale(LC_CTYPE, "");

  // string src = "value = 1 ** 2 ** 3 ** 4";
  string src = "1 + 1";
  $defer ([&] { src.deinit(); });

  string path = "N/A";
  $defer ([&] { path.deinit(); });

  auto scan_res = scan_source(src, path);
  if (!scan_res) return 1;

  TokenList tokens = scan_res.unwrap();
  $defer ([&] { tokens.deinit(); });

  Result<AST::Module, ParserError> parser_res = parse_tokens(tokens, src, path);
  if (!parser_res) return 1;

  AST::Module mod = parser_res.unwrap();
  $defer ([&] { mod.deinit(); });

  FILE *f = fopen("./ast.clj", "w");
  if (!f) return 0;
  $defer ([&] { fclose(f); });

  AST::print(mod, f);

  return 0;
}

// #include "common.hpp"
// #include "cli_tools.h"
// #include <expected>
// #include <string>
// #include <cstdio>

// std::expected<std::string, std::string> read_file(std::string &path) {
//   FILE *f = std::fopen(path.c_str(), "r");
//   if (f == NULL) return std::unexpected("Cannot open file: `" + path + "` for
//   reading."); defer (std::fclose(f));

//   std::fseek(f, 0, SEEK_END);
//   size_t size = std::ftell(f);
//   std::rewind(f);

//   std::string result{};
//   result.reserve(size);
//   std::fread((void *)result.c_str(), sizeof(char), size, f);
//   return result;
// }

// std::expected<void, std::string> write_to_file(std::string &path, std::string
// &content) {
//   FILE *f = std::fopen(path.c_str(), "w");
//   if (f == NULL) return std::unexpected("Cannot open file: `" + path + "` for
//   writing."); defer (std::fclose(f)); std::fprintf(f, "%s", content.c_str());
//   return {};
// }

// static int compile_handler(int argc, char **argv);
// static int help_handler(int argc, char **argv);
// static int version_handler(int argc, char **argv);

// subcommand_t subcommands[] = {
//   {"c", compile_handler, "Compile a haste file"},

//   {"help", help_handler, "Prints this and then exits."},
//   {"version", version_handler, "Prints the version of the compiler."},
// };
// const size_t subcommands_len = sizeof(subcommands) / sizeof(subcommand_t);

// static int help_handler(int argc, char **argv) {
//   unused(argc);
//   print_usage(argv[-2], subcommands, subcommands_len);
//   return EXIT_STATUS_SUCCESS;
// }

// static int version_handler(int argc, char **argv) {
//   unused(argc);
//   unused(argv);
//   printf("%s\n", VERSION);
//   return EXIT_STATUS_SUCCESS;
// }

// static int compile_handler(int argc, char **argv) {
//   char *out = "out.c";
//   bool verbose = false;
//   option_t options[] = {
//     { "output", 'o', OPT_STRING, &out },
//     { "verbose", 'v', OPT_FLAG, &verbose },
//     { NULL, 0, (option_kind_t)0, NULL },
//   };

//   if (!parse_options(&argc, &argv, options)) return
//   EXIT_STATUS_OTHER_FAILURE;

//   if (argc == 0) {
//     std::fprintf(stderr, "Expected a file as an argument.\n");
//     return EXIT_STATUS_OTHER_FAILURE;
//   }

//   const char *path = argv[0];
//   return EXIT_STATUS_SUCCESS;
// }

// int main(int argc, char **argv) {
//   setlocale(LC_CTYPE, "C.UTF-8");

//   if (argc < 2) {
//     print_usage(argv[0], subcommands, subcommands_len);
//     return EXIT_FAILURE;
//   }

//   int status = process_subcommands(argc, argv, subcommands, subcommands_len);
//   if (status == EXIT_STATUS_SUBCOMMAND_NOT_FOUND) {
//     print_usage(argv[0], subcommands, subcommands_len);
//   }

//   return status;
// }
