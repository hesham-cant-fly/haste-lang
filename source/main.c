#include <errno.h>
#include <stdio.h>
#include <locale.h>
#define MY_COMMONS_IMPLEMENTATION
#define MY_ALLOCATOR_IMPL
#define MY_ARENA_ALLOCATOR_IMPL
#define MY_C_ALLOCATOR_IMPL
#define MY_TEMPORARY_ALLOCATOR_IMPL
#define MY_HASHTABLE_IMPL
#define MY_STREAM_IMPL
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

static int custom_format_span(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct span span = va_arg(args, struct span);
	if (span.start == NULL) {
		return sprint(stream, "(nil)");
	}

	if (match_modifier(&mod, '#')) {
		return sprint(stream, "{s:#*}", span.start, (int)span.len);
	}
	return sprint(stream, "{s:*}", span.start, (int)span.len);
}

static int custom_format_token(stream_t stream, struct modifier_stream mod, va_list args)
{
	struct token token = va_arg(args, struct token);
	if (match_modifier(&mod, '#')) {
		if (match_modifier(&mod, '#')) {
			return sprint(stream, "{span:#}", token_to_span(token));
		}
		return print_token(stream, token);
	}
	return sprint(stream, "{span}", token_to_span(token));
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

	define_format_specifier("span", custom_format_span);
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

	enum { PHASE_PARSE, PHASE_HOIST, PHASE_ANALYZE, PHASE_CODEGEN, PHASE_COUNT };
	struct timer timers[PHASE_COUNT] = {0};
	const char *phase_names[PHASE_COUNT] = { "parser", "hoisting", "analysis", "codegen" };

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

	timer_start(&timers[PHASE_PARSE]);
	err = parse(arena_allocator, src);
	timer_stop(&timers[PHASE_PARSE]);

	if (err) { exit_code = 1; goto cleanup; }

	timer_start(&timers[PHASE_HOIST]);
	err = hoist(c_allocator,  src);
	timer_stop(&timers[PHASE_HOIST]);
	if (err) { exit_code = 1; goto cleanup; }

	if (g_options.dump_ast) {
		char path_buf[4096];
		stream_t out = open_dump_stream(".json", path_buf, sizeof(path_buf));
		if (!out.data) { exit_code = 1; goto cleanup; }
		sprintln(out, "{ast}", get_source_file_ast(src));
		close_dump_stream(&g_options, out);
		goto cleanup;
	}

	timer_start(&timers[PHASE_ANALYZE]);
	err = analyze(analysis_alloc, arena_allocator,  src);
	timer_stop(&timers[PHASE_ANALYZE]);
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
		timer_start(&timers[PHASE_CODEGEN]);
		err = codegen(c_allocator, src,  llvm_path, llvm_to_stderr);
		timer_stop(&timers[PHASE_CODEGEN]);
		if (err) { exit_code = 1; goto cleanup; }
		if (g_options.do_measure)
			print_timing_report(timers, phase_names, PHASE_COUNT);
		goto cleanup;
	}

	timer_start(&timers[PHASE_CODEGEN]);
	err = codegen(c_allocator, src,  NULL, false);
	timer_stop(&timers[PHASE_CODEGEN]);
	if (err) { exit_code = 1; goto cleanup; }

	if (g_options.do_measure)
		print_timing_report(timers, phase_names, PHASE_COUNT);

	// Cleanup source files
	for (size_t i = 0; i < sources.len; i++) {
		destroy(sources.allocator, sources.items[i].path);
		destroy(sources.allocator, sources.items[i].content);
		hmfree(sources.allocator, sources.items[i].declarations);
	}
	marrfree(sources);

cleanup:
	arena_free(&analysis_arena);
	deinit_intern_table();
	arena_free(&arena);
	return exit_code;
}
