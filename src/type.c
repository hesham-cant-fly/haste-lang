#include "type.h"
#include "arena.h"
#include "core/my_commons.h"
#include <stdio.h>
#include <string.h>


TypesPool g_types_pool = {0};
Type* g_primitive_types_table[] = {
	[PRIMITIVE_INT] = NULL,
	[PRIMITIVE_FLOAT] = NULL,
	[PRIMITIVE_UNTYPED_FLOAT] = NULL,
	[PRIMITIVE_UNTYPED_INT] = NULL,
	[PRIMITIVE_AUTO] = NULL,
};

void init_types_pool(void)
{
	Type tp = {0};

	tp.kind = TYPE_INT;
	tp.name = "int";
	g_primitive_types_table[PRIMITIVE_INT] = create_type(tp);

	tp.kind = TYPE_FLOAT;
	tp.name = "float";
	g_primitive_types_table[PRIMITIVE_FLOAT] = create_type(tp);

	tp.kind = TYPE_UNTYPED_FLOAT;
	tp.name = "untyped_float";
	g_primitive_types_table[PRIMITIVE_UNTYPED_FLOAT] = create_type(tp);

	tp.kind = TYPE_UNTYPED_INT;
	tp.name = "untyped_int";
	g_primitive_types_table[PRIMITIVE_UNTYPED_INT] = create_type(tp);

	tp.kind = TYPE_AUTO;
	tp.name = "auto";
	g_primitive_types_table[PRIMITIVE_AUTO] = create_type(tp);
}

void deinit_types_pool(void)
{
	arena_free(&g_types_pool.arena);
	g_types_pool = (TypesPool) {0};
}

Type* create_type(Type tp)
{
	Type *new_type = arena_alloc(&g_types_pool.arena, sizeof(tp));
	memcpy(new_type, &tp, sizeof(tp));
	if (g_types_pool.head == NULL && g_types_pool.current == NULL)
	{
		g_types_pool.head = new_type;
		g_types_pool.current = new_type;
	}

	g_types_pool.current->next = new_type;
	g_types_pool.current = new_type;
}

Type *get_primitive_type(PrimitiveType type)
{
	return g_primitive_types_table[type];
}

void print_type(FILE *f, const Type tp)
{
	fprintf(f, "%s", tp.name);
}

TypeMatchResult type_matches(const Type *tp1, const Type *tp2)
{
	if (tp1 == tp2)
	{
		return TYPE_MATCH_EXACT;
	}

	if (tp2->kind == TYPE_AUTO) unreachable();
	if (tp1->kind == TYPE_AUTO)
	{
		return TYPE_MATCH_EXACT;
	}

	switch (tp1->kind)
	{
	case TYPE_UNTYPED_INT:
		if (type_is_any_int(tp2)) return TYPE_MATCH_EXACT;
		if (type_is_any_float(tp2)) return TYPE_MATCH_EXACT;
		return TYPE_MATCH_NONE;
	case TYPE_UNTYPED_FLOAT:
		if (type_is_any_float(tp2)) return TYPE_MATCH_EXACT;
		if (type_is_any_int(tp2)) return TYPE_MATCH_NEED_CAST;
		return TYPE_MATCH_NONE;
	case TYPE_INT:
		if (type_is_any_int(tp2)) return TYPE_MATCH_EXACT;
		if (type_is_any_float(tp2)) return TYPE_MATCH_NEED_CAST;
		return TYPE_MATCH_NONE;
	case TYPE_FLOAT:
		if (type_is_any_int(tp2)) return TYPE_MATCH_NEED_CAST;
		if (type_is_any_float(tp2)) return TYPE_MATCH_EXACT;
		return TYPE_MATCH_NONE;
	default:
		return TYPE_MATCH_NONE;
	}
}

bool type_is_any_number(const Type *tp)
{
	return type_is_any_int(tp) || type_is_any_float(tp);
}

bool type_is_any_int(const Type *tp)
{
	switch (tp->kind)
	{
	case TYPE_UNTYPED_INT:
	case TYPE_INT:
		return true;
	default:
		return false;
	}
}

bool type_is_any_float(const Type *tp)
{
	switch (tp->kind)
	{
	case TYPE_UNTYPED_FLOAT:
	case TYPE_FLOAT:
		return true;
	default:
		return false;
	}
}
