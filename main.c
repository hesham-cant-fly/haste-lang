#include "ast.h"
#include "core/my_array.h"
#include "error.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"

#include <errno.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static void print_errno(void)
{
	fprintf(stderr, " * [%d] %s\n", errno, strerror(errno));
}

static char *read_entire_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
	{
		print_errno();
		return NULL;
	}

	if (fseek(f, 0, SEEK_END) == -1)
	{
		print_errno();
		fclose(f);
		return NULL;
	}

	int32_t len = ftell(f);
	if (len == -1)
	{
		print_errno();
		fclose(f);
		return NULL;
	}

	rewind(f);

	char *result = malloc(len + 1);
	if (result == NULL)
	{
		print_errno();
		fclose(f);
		return NULL;
	}

	size_t bytes_read = fread(result, 1, len, f);
	if (bytes_read != (size_t)len)
	{
		print_errno();
		free(result);
		fclose(f);
		return NULL;
	}
	result[len] = 0x0;

	fclose(f);
	return result;
}

int main(void)
{
	setlocale(LC_ALL, "C.UTF-8");
	char *src = read_entire_file("./main.haste");
	if (src == NULL)
	{
		return 1;
	}
	Token *tokens = NULL;
	error err	  = scan_entire_cstr(src, &tokens);
	if (err)
	{
		free(src);
		return 1;
	}

	ASTFile ast = { 0 };
	err			= parse_tokens(tokens, &ast);
	if (err)
	{
		arrfree(tokens);
		free(src);
		return 1;
	}

	print_ast_fileln(stdout, ast);

	arrfree(tokens);
	free(src);
	arena_free(&ast.arena);
	return 0;
}
