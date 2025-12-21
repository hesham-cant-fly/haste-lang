#include "type.h"
#include "arena.h"
#include "core/my_commons.h"
#include "core/my_array.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct TypesPool g_types_pool = {0};

#define get_type(id__) g_types_pool.types.items[(id__)]
#define ASSERT_TYPE_ID(id__) \
	do { \
		const struct Type tp = get_type((id__)); \
		assert((id__) == tp.id); \
	} while (0)

void init_types_pool(void)
{
	g_types_pool.types = (struct TypeList) {0};

#define X(_, __name)							\
	{											\
		struct Type tp = {0};							\
		tp.name = (__name);						\
		create_type(tp);						\
	}
	TYPE_ID_ENUM_DEF(X)
#undef X
}

void deinit_types_pool(void)
{
	arrfree(g_types_pool.types);
	g_types_pool = (struct TypesPool) {0};
}

TypeID create_type(struct Type tp)
{
	tp.id = g_types_pool.types.len;
	arrpush(g_types_pool.types, tp);

	return tp.id;
}

void print_type(FILE *f, const TypeID id)
{
	const struct Type tp = get_type(id);
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

	switch (id1)
	{
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
		if (type_is_any_int(id2)) return TYPE_MATCH_NEED_CAST;
		if (type_is_any_float(id2)) return TYPE_MATCH_EXACT;
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
