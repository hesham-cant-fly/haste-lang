#include "type.h"
#include "arena.h"
#include "core/my_commons.h"

void type_pool_add_type(TypesPool *self, Type tp)
{
	Type *new_type = arena_alloc(&self->arena, sizeof(tp));
	*new_type	   = tp;
	if (self->head == NULL && self->current == NULL)
	{
		self->head	  = new_type;
		self->current = new_type;
	}

	self->current->next = new_type;
	self->current		= new_type;
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
