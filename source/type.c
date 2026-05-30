#include "haste.h"
#include "my_common.h"
#include <signal.h>


#define IS_UNKNOWN(type) \
	type_equal(type, ty_unknown)

#define IS_ZERO_TYPE(type) \
	type_equal(type, ty_zero)

static struct haste_string_object _default_empty_string = {
	.base = { .kind = HASTE_OBJ_STRING },
	.len = 0,
};

void setup_builtin_types(struct Allocator allocator)
{
	discard allocator;
}

struct haste_type into_type(struct haste_value value)
{
	if (IS_BAD(value) or IS_NONE(value)) {
		return (struct haste_type) { value };
	}
	ASSERT_IS_TYPE(value);
	return (struct haste_type) { value };
}

struct haste_value into_value(struct haste_type type)
{
	return type.value;
}

struct haste_value zero_for_type(struct Allocator alloc, struct haste_type to)
{
	if (IS_STRUCT_TYPE(to)) {
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(to);
		struct haste_struct_object *so = (void*)create_struct(alloc, st);
		iarreach (i, *st) {
			so->fields[i] = default_for_type(alloc, st->items[i].type);
		}
		return VAL_OBJ(AS_TYPEID(to), so);
	}
	return default_for_type(alloc, to);
}

struct haste_value default_for_type(struct Allocator alloc, struct haste_type type)
{
	if (type_is_integer(type)) return VAL_SCALAR(AS_TYPEID(type), .integer = 0);
	if (type_is_float(type))   return VAL_SCALAR(AS_TYPEID(type), .floating = 0.0f);
	if (type_equal(type, ty_cstr)) {
		return VAL_OBJ(AS_TYPEID(type), &_default_empty_string);
	}

	if (IS_STRUCT_TYPE(type)) {
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);
		struct haste_struct_object *so = (void*)create_struct(alloc, st);
		for (size_t i = 0; i < st->len; i += 1) {
			if (IS_NONE(so->fields[i])) {
				so->fields[i] = default_for_type(alloc, st->items[i].type);
			}
		}
		return VAL_OBJ(AS_TYPEID(type), so);
	}

	unreachable();
}

ssize_t find_named_field(const struct haste_type tp, const char *name)
{
	if (not IS_STRUCT_TYPE(tp) and not IS_AUTO_STRUCT_TYPE(tp)) {
		return -1;
	}

	struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(tp);
	for (size_t i = 0; i < st->len; i += 1) {
		if (strcmp(st->items[i].name, name) == 0) {
			return i;
		}
	}

	return -1;
}

struct haste_type typeof_value(const struct haste_value value)
{
	switch (value.kind) {
	case HASTE_VL_NONE:
		unreachable();
	case HASTE_VL_BAD:     return into_type(VAL_BAD);
	case HASTE_VL_ZERO:    return ty_zero;
	case HASTE_VL_UNINIT:  return ty_unknown;
	case HASTE_VL_RUNTIME: return value.runtime->type;
	case HASTE_VL_TYPE:
	case HASTE_VL_SCALAR:
	case HASTE_VL_OBJ:
		return into_type(VAL_TYPE(value.type_id));
	}
	unreachable();
}

struct haste_value make_value(struct Allocator alloc, const struct haste_type type)
{
	struct haste_type_info *type_info = AS_TYPE_INFO(type);

	if (type_info->kind == HASTE_TY_STRUCT
		or type_info->kind == HASTE_TY_AUTO_STRUCT) {
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);
		struct haste_struct_object *so = (void*)create_struct(alloc, st);
		return VAL_OBJ(AS_TYPEID(type), so);
	}

	return default_for_type(alloc, type);
}

bool type_equal(const struct haste_type v1,
                const struct haste_type v2)
{
	const struct haste_type_info *t1 = AS_TYPE_INFO(v1);
	const struct haste_type_info *t2 = AS_TYPE_INFO(v2);

	return t1->pool_id == t2->pool_id;
}

uint64_t type_hash(const struct haste_type t)
{
	const struct haste_type_info *ot = AS_TYPE_INFO(t);
	uint64_t h = ot->kind;

	if (ot->name != NULL)
		for (const char *p = ot->name; *p; p += 1)
			h = h * 31 + (unsigned char)*p;

	if (ot->kind == HASTE_TY_STRUCT) {
		const struct haste_struct_type_info *st = (const struct haste_struct_type_info *)ot;
		for (size_t i = 0; i < st->len; i += 1) {
			for (const char *p = st->items[i].name; *p; p += 1)
				h = h * 31 + (unsigned char)*p;
			h = h * 31 + (uint64_t)st->items[i].type.value.kind;
		}
	}

	return h;
}

/* bool type_can_assign(const struct haste_type assignable, */
/*                      const struct haste_type value) */
/* { */
/* 	assert(not type_is_untyped(assignable) and "the assignable shouldn't be untyped."); */

/* 	if (type_equal(assignable, value))    return true; */
/* 	if (IS_ZERO_TYPE(value))       return true; */
/* 	if (type_equal(assignable, ty_auto))  return not IS_UNKNOWN(value); */
/* 	if (type_equal(assignable, ty_int))   return IS_UNKNOWN(value) or type_is_integer(value); */
/* 	if (type_equal(assignable, ty_usize)) return IS_UNKNOWN(value) or type_is_integer(value); */
/* 	if (type_equal(assignable, ty_float)) return IS_UNKNOWN(value) or type_is_untyped_number(value); */

/* 	if (AS_TYPE_INFO(assignable)->kind == HASTE_TY_INT or AS_TYPE_INFO(assignable)->kind == HASTE_TY_UINT) */
/* 		return IS_UNKNOWN(value) or type_is_untyped_integer(value); */

/* 	if (type_is_any_string(assignable)) */
/* 		return type_is_any_string(value) or IS_UNKNOWN(value); */

/* 	if (IS_UNKNOWN(value)) return true; */

/* 	if (IS_STRUCT_TYPE(assignable) and IS_AUTO_STRUCT_TYPE(value)) { */
/* 		const struct haste_struct_type_info *ass_st = AS_STRUCT_TYPE_INFO(assignable); */
/* 		const struct haste_struct_type_info *val_st = AS_STRUCT_TYPE_INFO(value); */

/* 		// TODO: this is not a valid requirement. consider this */
/* 		// const Foo = struct{ */
/* 		//     a: int; */
/* 		//     b: int = 2; */
/* 		// }; */
/* 		// const foo = .{ */
/* 		//     a: 1, // we set whats required */
/* 		// }; */
/* 		// const bar: Foo = cast foo; */
/* 		if (val_st->len > ass_st->len) return false; */

/* 		for (size_t i=0; i < ass_st->len; i += 1) { */
/* 			if (ass_st->items[i].has_default) continue; */
/* 			ssize_t idx = find_named_field(value, ass_st->items[i].name); */
/* 			if (idx < 0) { */
/* 				return false; */
/* 			} */
/* 		} */

/* 		return true; */
/* 	} */

/* 	return false; */
/* } */

/* bool type_can_cast(const struct haste_type to, */
/*                    const struct haste_type from) */
/* { */
/* 	// if you can assign to it. you can cast to it */
/* 	// (Also this saves us some code) */
/* 	if (type_can_assign(to, from))    return true; */

/* 	if (IS_UNKNOWN(from)) return true; */
/* 	if (type_equal(to, ty_auto))      return true; */
/* 	if (type_equal(to, from))         return true; */
/* 	if (IS_ZERO_TYPE(from))    return true; */
/* 	if (type_is_number(to))           return type_is_number(from) or type_equal(from, ty_usize); */
/* 	if (type_equal(to, ty_usize))     return type_is_number(from); */
/* 	if (AS_TYPE_INFO(to)->kind == HASTE_TY_INT or AS_TYPE_INFO(to)->kind == HASTE_TY_UINT) */
/* 		return type_is_integer(from); */
/* 	if (type_is_any_string(to)) */
/* 		return type_is_any_string(from) or IS_UNKNOWN(from); */

/* 	return false; */
/* } */

bool type_is_any_string(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_string;
}

struct haste_type untyped_to_typed(struct haste_type type)
{
	if (type_equal(type, ty_untyped_int))       return ty_int;
	if (type_equal(type, ty_untyped_float))     return ty_float;
	if (type_equal(type, ty_untyped_string))    return ty_string;
	if (IS_ZERO_TYPE(type))              return ty_int;
	if (HASTE_TID_IS_RESERVED(AS_TYPEID(type))) return type;

	return type;
}

bool type_is_integer(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_integer;
}

bool type_is_float(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_float;
}

bool type_is_untyped_float(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_float and AS_TYPE_INFO(t)->is_untyped;
}

bool type_is_number(const struct haste_type t)
{
	return type_is_integer(t) or type_is_float(t);
}

bool type_is_untyped(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_untyped;
}

bool type_is_untyped_integer(const struct haste_type t)
{
	return AS_TYPE_INFO(t)->is_untyped and AS_TYPE_INFO(t)->is_integer;
}

bool type_is_untyped_number(const struct haste_type t)
{
	return type_is_number(t) and type_is_untyped(t);
}

bool haste_is_default_empty_string(const struct haste_object *obj)
{
	return obj == &_default_empty_string.base;
}
