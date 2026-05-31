#define MY_COMMONS_IMPLEMENTATION
#define MY_ALLOCATOR_IMPL
#define MY_ARENA_ALLOCATOR_IMPL
#define MY_C_ALLOCATOR_IMPL
#define MY_TEMPORARY_ALLOCATOR_IMPL
#define MY_HASHTABLE_IMPL
#define MY_STREAM_IMPL
#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include "haste.h"
#include "my_timing.h"
#include "cwalk.h"

static stream_t open_dump_stream(const char *ext, char *path_buf, size_t path_buf_size)
{
	if (g_options.do_dump)
		return serr;

	const char *path = g_options.output_path;
	if (!path) {
		cwk_path_change_extension(g_options.source_path, ext, path_buf, path_buf_size);
		path = path_buf;
	}

	stream_t out = sopen(path, "w");
	if (!out.data) {
		fprintf(stderr, "error: failed to open '%s' for writing\n", path);
		return out;
	}
	return out;
}

static void close_dump_stream(const struct options *opts, stream_t out)
{
	if (!opts->do_dump)
		sclose(out);
}

static void print_errno(void)
{
	fprintf(stderr, " * [%d] %s\n", errno, strerror(errno));
}

static int custom_format_string(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct string string = va_arg(args, struct string);
	if (string.chars == NULL) {
		return sprint(stream, "(nil)");
	}

	if (match_modifier(&mod, '#')) {
		return sprint(stream, "{s:#*}", string.chars, (int)string.len);
	}
	return sprint(stream, "{s:*}", string.chars, (int)string.len);
}

static int custom_format_token(stream_t stream, struct modifier_stream mod, va_list args)
{
	struct token token = va_arg(args, struct token);
	if (match_modifier(&mod, '#')) {
		if (match_modifier(&mod, '#')) {
			return sprint(stream, "{string:#}", as_string(token));
		}
		return print_token(stream, token);
	}
	return sprint(stream, "{string}", as_string(token));
}

static int custom_format_value(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct haste_value value = va_arg(args, struct haste_value);
	return print_value(stream, value);
}

static int custom_format_object(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct haste_value object = va_arg(args, struct haste_value);
	return print_object(stream, object.obj, typeof_value(object));
}

static int custom_format_ast(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct haste_ast_node *node = va_arg(args, struct haste_ast_node *);
	return print_haste_ast(stream, node);
}

int main(int argc, char *argv[argc])
{
	int exit_code = 0;
	setup_io_stream();

	Error err = parse_arguments(argc, (const char **)argv);
	if (err) return 1;

	define_format_specifier("string", custom_format_string);
	define_format_specifier("token", custom_format_token);
	define_format_specifier("value", custom_format_value);
	define_format_specifier("obj", custom_format_object);
	define_format_specifier("ast", custom_format_ast);

	srand(time(NULL));
	setlocale(LC_ALL, "C.UTF-8");

	struct Allocator c_allocator = get_c_allocator();
	set_default_allocator(c_allocator);

	sources.allocator = c_allocator;

	struct Arena arena = ArenaDefault();
	struct Allocator arena_allocator = arena_get_allocator(&arena);

	init_intern_table(c_allocator, arena_allocator);
	setup_builtins(c_allocator);

	// Sub-arena for analysis allocations (struct types, objects, strings)
	struct Arena analysis_arena = Arena(c_allocator);
	struct Allocator analysis_alloc = arena_get_allocator(&analysis_arena);

	struct timer_list timers = {
		.allocator = get_default_allocator(),
	};

	const source_file_id src = obtain_source_file_id(NULL, g_options.source_path);

	if (g_options.dump_tokens) {
		char path_buf[4096];
		stream_t out = open_dump_stream(".tokens", path_buf, sizeof(path_buf));
		struct token_stream tokens = token_stream(src);
		while (not token_stream_ended(&tokens)) {
			sprintln(out, "{token:#}", token_stream_advance(&tokens));
		}
		close_dump_stream(&g_options, out);
		goto cleanup;
	}

	timer_start(&timers, "parser");
	err = parse(arena_allocator, src);
	timer_stop(&timers, allocated);

	if (err) { exit_code = 1; goto cleanup; }
	if (g_options.only_parse) {
		exit_code = 0;
		goto cleanup;
	}

	if (g_options.dump_ast) {
		char path_buf[4096];
		stream_t out = open_dump_stream(".json", path_buf, sizeof(path_buf));
		if (!out.data) { exit_code = 1; goto cleanup; }
		sprintln(out, "{ast}", get_source_file_ast(src));
		close_dump_stream(&g_options, out);
		goto cleanup;
	}

	timer_start(&timers, "analysis");
	err = analyze(analysis_alloc, arena_allocator,  src);
	timer_stop(&timers, allocated);
	if (err) { exit_code = 1; goto cleanup; }

	if (g_options.dump_sema) {
		char path_buf[4096];
		stream_t out = open_dump_stream(".json", path_buf, sizeof(path_buf));
		if (!out.data) { exit_code = 1; goto cleanup; }
		sprintln(out, "{ast}", get_source_file_ast(src));
		close_dump_stream(&g_options, out);
		goto cleanup;
	}

	if (g_options.dump_llvm) {
		const char *llvm_path = NULL;
		bool llvm_to_stderr = false;
		char llvm_path_buf[4096];
		if (g_options.do_dump) {
			llvm_to_stderr = true;
		} else {
			llvm_path = g_options.output_path;
			if (!llvm_path) {
				cwk_path_change_extension(g_options.source_path, ".ll", llvm_path_buf, sizeof(llvm_path_buf));
				llvm_path = llvm_path_buf;
			}
		}
		timer_start(&timers, "codegen");
		err = codegen(c_allocator, src,  llvm_path, llvm_to_stderr);
		timer_stop(&timers, allocated);
		if (err) { exit_code = 1; goto cleanup; }
		goto cleanup;
	}

	timer_start(&timers, "codegen");
	err = codegen(c_allocator, src,  NULL, false);
	timer_stop(&timers, allocated);
	if (err) { exit_code = 1; goto cleanup; }

	// Cleanup source files
	for (size_t i = 0; i < sources.len; i++) {
		struct source_file item = sources.items[i];
		xdestroy(sources.allocator, strlen(item.path), item.path);
		xdestroy(sources.allocator, strlen(item.content), item.content);
	}
	marrfree(sources);

cleanup:
	if (g_options.do_measure and exit_code == 0) {
		print_timing_report(timers);
	}
	marrfree(timers);

	arena_free(&analysis_arena);
	deinit_intern_table();
	arena_free(&arena);
	return exit_code;
}
