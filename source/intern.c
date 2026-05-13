#include "haste.h"

#define INTERN_DEFAULT_CAP 256

struct intern_table init_intern_table(struct Allocator allocator, struct Allocator arena)
{
	struct intern_table table = {
		.arena = arena,
		.allocator = allocator,
		.cap = INTERN_DEFAULT_CAP,
		.entries = alloc(allocator, sizeof(struct intern_entry) * INTERN_DEFAULT_CAP),
	};
	memset(table.entries, 0, sizeof(struct intern_entry) * INTERN_DEFAULT_CAP);
	return table;
}

void deinit_intern_table(struct intern_table *table)
{
	destroy(table->allocator, table->entries);
	*table = (struct intern_table){0};
}

static uint64_t hash_bytes(const char *s, size_t len)
{
	uint64_t h = 1469598103934665603ULL;
	for (size_t i = 0; i < len; i++) {
		h ^= (unsigned char)s[i];
		h *= 1099511628211ULL;
	}
	return h;
}

static void grow_and_rehash(struct intern_table *table)
{
	const size_t new_cap = table->cap * 2;
	struct intern_entry *old_entries = table->entries;

	table->entries = alloc(table->allocator, sizeof(struct intern_entry) * new_cap);
	memset(table->entries, 0, sizeof(struct intern_entry) * new_cap);

	for (size_t i = 0; i < table->cap; i++) {
		struct intern_entry *old = &old_entries[i];
		if (old->str == NULL) continue;

		size_t idx = old->hash & (new_cap - 1);
		while (table->entries[idx].str != NULL)
			idx = (idx + 1) & (new_cap - 1);
		table->entries[idx] = *old;
	}

	table->cap = new_cap;
	destroy(table->allocator, old_entries);
}

const char *intern_str(struct intern_table *table, const char *start, size_t len)
{
	uint64_t h = hash_bytes(start, len);

	if (table->len >= (table->cap * 7) / 10) {
		grow_and_rehash(table);
	}

	size_t idx = h & (table->cap - 1);

	while (true) {
		struct intern_entry *e = &table->entries[idx];
		if (e->str == NULL) {
			char *copy = alloc(table->arena, len + 1);
			memcpy(copy, start, len);
			copy[len] = '\0';
			e->hash = h;
			e->str  = copy;
			e->len  = len;
			table->len++;
			// println("INTERN_PUT: {s:#*}", start, (int)len);
			return copy;
		}
		if (e->hash == h && e->len == len &&
		    memcmp(e->str, start, len) == 0) {
			// println("INTERN_GET: {s:#*}", start, (int)len);
			return e->str;
		}

		idx = (idx + 1) & (table->cap - 1);
	}
}

const char *intern_token(struct intern_table *table, struct token token)
{
	switch (token.kind) {
	case TK_STR: return intern_cstr(table, token.str);
	case TK_IDENT:
		// return intern_str(table, token.start, token.len);
		return intern_cstr(table, token.ident);
	default:
		break;
	}
	return intern_str(table, token.start, token.len);
}

const char *intern_cstr(struct intern_table *table, const char *str)
{
	return intern_str(table, str, strlen(str));
}
