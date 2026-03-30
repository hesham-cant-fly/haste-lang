#include "analyzer.h"
#include "common.h"
#include "tir.h"
#include "ast.h"
#include "core/my_array.h"
#include "error.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"
#include "type.h"
#include "cwalk.h"

#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_errno(void)
{
	fprintf(stderr, " * [%d] %s\n", errno, strerror(errno));
}

static char *read_entire_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		print_errno();
		return NULL;
	}

	if (fseek(f, 0, SEEK_END) == -1) {
		print_errno();
		fclose(f);
		return NULL;
	}

	int32_t len = ftell(f);
	if (len == -1) {
		print_errno();
		fclose(f);
		return NULL;
	}

	rewind(f);

	char *result = malloc(len + 2);
	if (result == NULL) {
		print_errno();
		fclose(f);
		return NULL;
	}
	result[0] = 0x0;

	size_t bytes_read = fread(result + 1, 1, len, f);
	if (bytes_read != (size_t)len) {
		print_errno();
		free(result);
		fclose(f);
		return NULL;
	}
	result[len] = 0x0;

	fclose(f);
	return result;
}

static void save_ast(struct AstFile ast)
{
	ast_file_print(ast, stdout);
	/* char* out_path = strdup(ast.path); */
	/* cwk_path_change_extension(ast.path, "ast", out_path, strlen(ast.path) + 1); */

	/* FILE* f = fopen(out_path, "w"); */

	/* print_ast_fileln(f, ast); */

	/* free(out_path); */
	/* fclose(f); */
}

static void save_tir(struct Tir tir)
{
	char* out_path = strdup(tir.path);
	cwk_path_change_extension(tir.path, "tir", out_path, strlen(tir.path) + 1);

	FILE* f = fopen(out_path, "w");
	print_tir(f, tir);

	free(out_path);
	fclose(f);
}

int main(void)
{
	setlocale(LC_ALL, "C.UTF-8");
	init_types_pool();

	char* full_path = NULL;
	error err = get_full_path("./main.haste", &full_path);
	if (err) {
		printf("FILE doesn't exits.");
		return 1;
	}

	char *src = read_entire_file(full_path);
	if (src == NULL) {
		free(full_path);
		return 1;
	}

	struct TokenList tokens = {0};
	err = scan_entire_cstr(src, full_path, &tokens);
	if (err) {
		free(src);
		free(full_path);
		return 1;
	}

	Arena arena = {0};
	struct AstFile ast = { 0 };
	err = parse_tokens(tokens, full_path, &ast, &arena);
	if (err) {
		arrfree(tokens);
		free(src);
		free(full_path);
		fprintf(stderr, "Parsing Error.");
		return 1;
	}

#ifdef DEBUG_DEV_BUILD
	save_ast(ast);
#endif /* DEBUG_DEV_BUILD */

	struct Environment env = {0};
	init_environment(&env, full_path, &arena);

	err = analyze_ast_file(ast, &env);
	if (err) {
		deinit_environment(&env);
		arrfree(tokens);
		free(src);
		free(full_path);
		arena_free(&arena);
		deinit_types_pool();
		fprintf(stderr, "Analysis Error.");
		return 1;
	}

#ifdef DEBUG_DEV_BUILD
	save_ast(ast);
#endif /* DEBUG_DEV_BUILD */

	deinit_environment(&env);
	arrfree(tokens);
	free(src);
	free(full_path);
	arena_free(&arena);
	deinit_types_pool();
	return 0;
}
