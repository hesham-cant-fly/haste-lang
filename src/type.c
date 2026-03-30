#include "type.h"
#include "arena.h"
#include "core/my_commons.h"
#include <stdlib.h>
#if USE_DYNAMIC_ARRAY
# include "core/my_array.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct TypesPool g_types_pool = {0};

// #define get_type(id__) g_types_pool.types.items[(id__)]
#define ASSERT_TYPE_ID(id__) \
	do { \
		const struct TypeInfo tp = get_type_info((id__)); \
		assert((id__) == tp.id); \
	} while (0)

void init_types_pool(void)
{
	g_types_pool = (struct TypesPool){0};

#define X(_, name__) \
	{ \
		struct TypeInfo tp = {0}; \
		tp.name = (name__);	\
		create_type(tp); \
	}
	TYPE_ID_ENUM_DEF(X)
#undef X
}

#if !USE_DYNAMIC_ARRAY
static void add_bucket(void)
{
    struct TypesBucket *bucket = malloc(sizeof(struct TypesBucket));
    bucket->len = 0;
    bucket->next = NULL;

    if (g_types_pool.head == NULL) {
        assert(g_types_pool.current == NULL);

        g_types_pool.head = bucket;
        g_types_pool.current = bucket;
        g_types_pool.buckets_count += 1;
        return;
    }

    g_types_pool.current->next = bucket;
    g_types_pool.current = bucket;
    g_types_pool.buckets_count += 1;
}
#endif

static struct TypeInfo get_type_info(TypeID id)
{
#if USE_DYNAMIC_ARRAY
	assert(id < g_types_pool.types.len);

	return g_types_pool.types.items[id];
#else
    assert(id < g_types_pool.total_count);

    size_t bucket_count = id / TYPES_BUCKET_CAPACITY;
    struct TypesBucket *bucket = g_types_pool.head;
    for (size_t i=0; i < bucket_count; i += 1) {
        bucket = bucket->next;
    }

    assert(bucket != NULL);

    return bucket->items[id - (bucket_count * TYPES_BUCKET_CAPACITY)];
#endif
}

void deinit_types_pool(void)
{
#if USE_DYNAMIC_ARRAY
	arrfree(g_types_pool.types);
#else
    if (g_types_pool.total_count == 0) {
        return;
    }

    assert(g_types_pool.head != NULL);
    assert(g_types_pool.current != NULL);

    struct TypesBucket *current = g_types_pool.head;
    while (current != NULL) {
        struct TypesBucket *next = current->next;
        free(current);
        current = next;
    }
#endif

	g_types_pool = (struct TypesPool) {0};
}

TypeID create_type(struct TypeInfo tp)
{
#if USE_DYNAMIC_ARRAY
	tp.id = g_types_pool.types.len;
	arrpush(g_types_pool.types, tp);
#else
    if (g_types_pool.current == NULL) add_bucket();
    if (g_types_pool.current->len == TYPES_BUCKET_CAPACITY) add_bucket();

	tp.id = g_types_pool.total_count;
    g_types_pool.current->items[g_types_pool.current->len] = tp;
    g_types_pool.current->len += 1;
    g_types_pool.total_count += 1;
#endif

	return tp.id;
}

const char* type_str(const TypeID id)
{
	const struct TypeInfo tp = get_type_info(id);
	return tp.name;
}

void print_typeln(FILE *f, const TypeID id)
{
	print_type(f, id);
	fprintf(f, "\n");
}

void print_type(FILE *f, const TypeID id)
{
	const struct TypeInfo tp = get_type_info(id);
	assert(id == tp.id);

	fprintf(f, "%s", tp.name);
}

enum TypeMatchResult type_matches(const TypeID id1, const TypeID id2)
{
	ASSERT_TYPE_ID(id1);
	ASSERT_TYPE_ID(id2);

	if (id1 == id2) return TYPE_MATCH_EXACT;

	if (id1 == TYPE_AUTO) return TYPE_MATCH_EXACT;
	if (id2 == TYPE_AUTO) unreachable();

	switch (id1) {
	case TYPE_UNTYPED_INT:
		if (type_is_any_int(id2)) return TYPE_MATCH_EXACT;
		if (type_is_any_float(id2)) return TYPE_MATCH_EXACT;
		return TYPE_MATCH_NONE;
	case TYPE_UNTYPED_FLOAT:
		if (type_is_any_float(id2)) return TYPE_MATCH_EXACT;
		if (type_is_any_int(id2)) return TYPE_MATCH_NEED_CAST;
		return TYPE_MATCH_NONE;
	case TYPE_INT:
		if (type_is_any_int(id2)) return TYPE_MATCH_EXACT;
		if (type_is_any_float(id2)) return TYPE_MATCH_NEED_CAST;
		return TYPE_MATCH_NONE;
	case TYPE_FLOAT:
		if (id2 == TYPE_UNTYPED_INT) return TYPE_MATCH_EXACT;
		if (type_is_any_float(id2)) return TYPE_MATCH_EXACT;
		if (type_is_any_int(id2)) return TYPE_MATCH_NEED_CAST;
		return TYPE_MATCH_NONE;
	default:
		return TYPE_MATCH_NONE;
	}
}

bool type_is_numiric(const TypeID id)
{
	ASSERT_TYPE_ID(id);

	return type_is_any_int(id) || type_is_any_float(id);
}

bool type_is_any_int(const TypeID id)
{
	ASSERT_TYPE_ID(id);

	switch (id)
	{
	case TYPE_UNTYPED_INT:
	case TYPE_INT:
		return true;
	default:
		return false;
	}
}

bool type_is_any_float(const TypeID id)
{
	ASSERT_TYPE_ID(id);

	switch (id)
	{
	case TYPE_UNTYPED_FLOAT:
	case TYPE_FLOAT:
		return true;
	default:
		return false;
	}
}

bool type_is_untyped(const TypeID id)
{
	ASSERT_TYPE_ID(id);

	if (id == TYPE_UNTYPED_INT) return true;
	if (id == TYPE_UNTYPED_FLOAT) return true;
	return false;
}
