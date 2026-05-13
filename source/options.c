#include "haste.h"
#include <string.h>
#include <stdio.h>

static int print_usage(stream_t f, const char *prog)
{
	int amount = 0;
	amount += sprintln(f, "Usage: {s} [options] [file]", prog);
	amount += sprintln(f, "Options:");
	amount += sprintln(f, "  --tokens    Dump token stream and exit");
	amount += sprintln(f, "  --ast       Dump AST after parsing/hoisting and exit");
	amount += sprintln(f, "  --sema      Dump semantic analysis result and exit");
	amount += sprintln(f, "  --llvm      Dump LLVM IR and exit");
	amount += sprintln(f, "  --dump      Write dump output to stderr instead of a file");
	amount += sprintln(f, "  -o <file>   Write dump output to <file>");
	amount += sprintln(f, "  --measure   Show timing report for each compiler phase");
	amount += sprintln(f, "  --help      Show this help message and exit");
	return amount;
}

Error parse_arguments(const int argc, const char *argv[argc], struct options *out)
{
	*out = (struct options){
		.source_path = NULL,
		.output_path = NULL,
	};

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--tokens") == 0) {
			out->dump_tokens = true;
		} else if (strcmp(argv[i], "--ast") == 0) {
			out->dump_ast = true;
		} else if (strcmp(argv[i], "--sema") == 0) {
			out->dump_sema = true;
		} else if (strcmp(argv[i], "--llvm") == 0) {
			out->dump_llvm = true;
		} else if (strcmp(argv[i], "--measure") == 0) {
			out->do_measure = true;
		} else if (strcmp(argv[i], "--dump") == 0) {
			out->do_dump = true;
		} else if (strcmp(argv[i], "-o") == 0) {
			i += 1;
			if (i >= argc) {
				eprintln("error: '-o' requires a file argument.");
				return ERROR;
			}
			out->output_path = argv[i];
		} else if (strcmp(argv[i], "--help") == 0) {
			print_usage(sout, argv[0]);
			exit(0);
		} else if (argv[i][0] == '-') {
			eprintln("error: unknown option '{s}'\n", argv[i]);
			print_usage(serr, argv[0]);
			return ERROR;
		} else {
			out->source_path = argv[i];
		}
	}

	if (out->source_path == NULL) {
		print_usage(serr, argv[0]);
		eprintln("\nerror: expected a file. provided none.");
		return ERROR;
	}

	return OK;
}
