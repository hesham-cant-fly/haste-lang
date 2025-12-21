#include "common.h"
#include "hir.h"
#include "tir.h"
#include "analysis.h"
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

	char *result = malloc(len + 1);
	if (result == NULL) {
		print_errno();
		fclose(f);
		return NULL;
	}

	size_t bytes_read = fread(result, 1, len, f);
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

static void save_ast(struct ASTFile ast)
{
	char* out_path = strdup(ast.path);
	cwk_path_change_extension(ast.path, "ast", out_path, strlen(ast.path) + 1);

	FILE* f = fopen(out_path, "w");

	print_ast_fileln(f, ast);

	free(out_path);
	fclose(f);
}

static void save_hir(struct Hir hir)
{
	char* out_path = strdup(hir.path);
	cwk_path_change_extension(hir.path, "hir", out_path, strlen(hir.path) + 1);

	FILE* f = fopen(out_path, "w");

	print_hir(f, hir);

	free(out_path);
	fclose(f);
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

	struct ASTFile ast = { 0 };
	err = parse_tokens(tokens, full_path, &ast);
	if (err) {
		arrfree(tokens);
		free(src);
		free(full_path);
		return 1;
	}

#ifdef DEBUG_DEV_BUILD
	save_ast(ast);
#endif // DEBUG_DEV_BUILD

	struct Hir hir = {0};
	err = hoist_ast(ast, &hir);
	if (err) {
		arrfree(tokens);
		free(src);
		free(full_path);
		arena_free(&ast.arena);
		return 1;
	}

#ifdef DEBUG_DEV_BUILD
	save_hir(hir);
#endif // DEBUG_DEV_BUILD

	struct Tir tir = {0};
	err = analyze_hir(hir, &tir);
	if (err) {
		arrfree(tokens);
		free(src);
		free(full_path);
		arena_free(&ast.arena);
		deinit_hir(hir);
		deinit_types_pool();
		return 1;
	}

#ifdef DEBUG_DEV_BUILD
	save_tir(tir);
#endif // DEBUG_DEV_BUILD

	arrfree(tokens);
	free(src);
	free(full_path);
	arena_free(&ast.arena);
	deinit_hir(hir);
	deinit_tir(&tir);
	deinit_types_pool();
	return 0;
}
