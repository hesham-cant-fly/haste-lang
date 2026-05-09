#include <errno.h>
#include <stdio.h>
#include <locale.h>
#define MY_ALLOCATOR_IMPL
#define MY_ARENA_ALLOCATOR_IMPL
#define MY_C_ALLOCATOR_IMPL
#define MY_TEMPORARY_ALLOCATOR_IMPL
#define MY_HASHTABLE_IMPL
#define MY_STREAM_IMPL
#include "haste.h"

static void print_errno(void)
{
	fprintf(stderr, " * [%d] %s\n", errno, strerror(errno));
}

static int custom_format_span(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct span span = va_arg(args, struct span);
	if (match_modifier(&mod, '#')) {
		return sprint(stream, "{s:#*}", span.start, (int)span.len);
	}
	return sprint(stream, "{s:*}", span.start, (int)span.len);
}

static int custom_format_token(stream_t stream, struct modifier_stream mod, va_list args)
{
	struct token token = va_arg(args, struct token);
	if (match_modifier(&mod, '#')) {
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
	struct haste_object *object = va_arg(args, struct haste_object*);
	return print_object(stream, object);
}

static int custom_format_ast(stream_t stream, struct modifier_stream mod, va_list args)
{
	discard mod;
	struct haste_ast_node *node = va_arg(args, struct haste_ast_node *);
	return print_haste_ast(stream, node);
}

int main(int argc, char **argv)
{
	bool dump_tokens = false;
	bool dump_ast = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--tokens") == 0) dump_tokens = true;
		else if (strcmp(argv[i], "--ast") == 0) dump_ast = true;
	}

	setup_io_stream();

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
	struct Allocator allocator = arena_get_allocator(&arena);

	const source_file_id src = obtain_source_file_id(NULL, "./main.haste");
	struct token_list tokens = {0};
	Error err = scan_entire_file(c_allocator, src, &tokens);
	if (err) {
		arena_free(&arena);
		return 1;
	}

	if (dump_tokens) {
		arreach (struct token, tok, tokens) {
			println("{token:#}", tok);
		}
		arrfree(c_allocator, tokens);
		arena_free(&arena);
		return 0;
	}

	err = parse(allocator, tokens, src);
	if (err) {
		arena_free(&arena);
		arrfree(c_allocator, tokens);
		return 1;
	}

	arrfree(c_allocator, tokens);

	err = hoist(c_allocator, allocator, src);
	if (err) {
		arena_free(&arena);
		return 1;
	}

	if (dump_ast) {
		println("{ast}", get_source_file_ast(src));
		arena_free(&arena);
		return 0;
	}

	err = analyze(c_allocator, allocator, src);
	if (err) {
		arena_free(&arena);
		return 1;
	}
	err = codegen(c_allocator, src);
	if (err) {
		arena_free(&arena);
		return 1;
	}

	arena_free(&arena);
	return 0;
}
