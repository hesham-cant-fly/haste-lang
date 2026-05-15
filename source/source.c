#include "haste.h"

#ifdef _WIN32
#  include <direct.h>
#  define GETCWD _getcwd
#else
#  include <unistd.h>
#  define GETCWD getcwd
#endif

struct source_file_list sources = {0};

char* get_current_working_directory(void)
{
	struct Allocator allocator = get_default_allocator();

	static char *cwd = NULL;
	if (cwd != NULL) return cwd;
#ifdef _WIN32
	char* buffer = alloc(allocator, FILENAME_MAX);
	if (buffer == NULL) return NULL;
	if (getcwd(buffer, FILENAME_MAX)) {
		cwd = buffer;
		return buffer;
	}
	destroy(allocator, buffer);
	return NULL;
#else
	long path_max = pathconf("/", _PC_PATH_MAX);
	if (path_max == -1) path_max = 4096;
	char *buffer = alloc(allocator, path_max + 1);
	if (buffer == NULL) return NULL;
	if (getcwd(buffer, path_max)) {
		cwd = buffer;
		return buffer;
	}
	destroy(allocator, buffer);
	return NULL;
#endif
}

char* get_absolute_path(struct Allocator allocator, const char* relative_path)
{
#ifdef _WIN32
	DWORD len = GetFullPathNameA(relative_path, 0, NULL, NULL);
	if (len == 0) return NULL;

	char* buffer = alloc(allocator, sizeof(char) * len + 1);
	if (buffer == NULL) return NULL;

	if (GetFullPathNameA(relative_path, len, buffer, NULL) == 0) {
		destroy(allocator, buffer);
		return NULL;
	}
	return buffer;
#else
	long path_max = pathconf(relative_path, _PC_PATH_MAX);
	if (path_max == -1) path_max = 4096;
	char *buffer = alloc(allocator, path_max + 1);
    return realpath(relative_path, buffer);
#endif
}

char *read_entire_file(struct Allocator allocator, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		eprintln("Couldn't open '{s}'.", path);
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	const size_t size = ftell(f);
	rewind(f);

	char *result = alloc(allocator, (size + 1) * sizeof(char));
	const size_t readed = fread(result, 1, size, f);
	if (readed != size) {
		eprintln("failed reading '{s}'.", path);
		exit(1);
	}

	result[size] = '\0';
	return result;
}

enum source_file_type get_file_type(const char *path)
{
	const char *extension = NULL;
	size_t tmp;
	if (not cwk_path_get_extension(path, &extension, &tmp)) {
		return SRC_UNKNOWN;
	}

	if (strcmp(extension, ".haste") == 0) {
		return SRC_HASTE;
	} else {
		return SRC_UNKNOWN;
	}
}

source_file_id obtain_source_file_id(const char *base, const char *path)
{
	struct Allocator allocator = sources.allocator;
	if (base == NULL) {
		base = "./";
	}

	// cwk_path_get_absolute
	size_t path_len = cwk_path_join(base, path, NULL, 0);
	char *full_path = alloc(allocator, sizeof(char) * (path_len + 1));
	cwk_path_join(base, path, full_path, path_len + 1);

	char *content = read_entire_file(allocator, path);

	struct source_file source = {
		.path = full_path,
		.content = content,
		.len = strlen(content),
		.type = get_file_type(full_path),
	};

	marrpush(sources, source);
	return sources.len - 1;
}

struct source_file get_source_file(const source_file_id id)
{
	assert(id < (int32_t)sources.len);
	return sources.items[id];
}

const char *get_source_file_path(const source_file_id id)
{
	return get_source_file(id).path;
}

size_t get_source_file_len(const source_file_id id)
{
	return get_source_file(id).len;
}

const char *get_source_file_content(const source_file_id id)
{
	return get_source_file(id).content;
}

const char *get_source_file_end(const source_file_id id)
{
	return get_source_file_content(id) + get_source_file_len(id);
}


enum source_file_type get_source_file_type(const source_file_id id)
{
	return get_source_file(id).type;
}

struct haste_ast_node *get_source_file_ast(const source_file_id id)
{
	return get_source_file(id).root;
}

struct haste_declarations get_source_file_declarations(const source_file_id id)
{
	return get_source_file(id).declarations;
}
