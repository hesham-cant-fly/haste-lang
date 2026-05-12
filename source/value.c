#include "haste.h"
#include <signal.h>

#define ASSERT_IS_TYPE(...) \
	do { \
		assert(IS_TYPE(__VA_ARGS__) and "It should be a type. maybe you forgot to use `typeof()`?"); \
	} while (0)

static struct haste_object_type _ty_zero_data           = OBJ_TYPE(.kind = HASTE_TY_ZERO);
static struct haste_object_type _ty_unknown_data        = OBJ_TYPE(.kind = HASTE_TY_UNKNOWN);
static struct haste_object_type _ty_type_data           = OBJ_TYPE(.kind = HASTE_TY_TYPE,       .size = 8,  .align = 8);
static struct haste_object_type _ty_int_data            = OBJ_TYPE(.kind = HASTE_TY_INT,        .size = 4,  .align = 4);
static struct haste_object_type _ty_untyped_int_data    = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_INT,.size = 4,  .align = 4);
static struct haste_object_type _ty_float_data          = OBJ_TYPE(.kind = HASTE_TY_FLOAT,      .size = 4,  .align = 4);
static struct haste_object_type _ty_untyped_float_data  = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_FLOAT,.size=4,  .align=4);
static struct haste_object_type _ty_auto_data           = OBJ_TYPE(.kind = HASTE_TY_AUTO);
static struct haste_object_type _ty_void_data           = OBJ_TYPE(.kind = HASTE_TY_VOID);
static struct haste_object_type _ty_untyped_string_data = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_STRING, .size = 8, .align = 8);
static struct haste_object_type _ty_string_data         = OBJ_TYPE(.kind = HASTE_TY_STRING); // TODO:
static struct haste_object_type _ty_cstr_data           = OBJ_TYPE(.kind = HASTE_TY_CSTR, .size = 8, .align = 4);

struct haste_value ty_zero           = VAL_OBJ(&_ty_type_data, &_ty_zero_data);
struct haste_value ty_unknown        = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_unknown_data };
struct haste_value ty_type           = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_type_data };
struct haste_value ty_int            = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_int_data };
struct haste_value ty_untyped_int    = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_untyped_int_data };
struct haste_value ty_float          = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_float_data };
struct haste_value ty_untyped_float  = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_untyped_float_data };
struct haste_value ty_auto           = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_auto_data };
struct haste_value ty_void           = { .kind = HASTE_VL_OBJ, .type = &_ty_type_data, .obj = (struct haste_object*)&_ty_void_data };
struct haste_value ty_untyped_string = VAL_OBJ(&_ty_type_data, &_ty_untyped_string_data);
struct haste_value ty_string         = VAL_OBJ(&_ty_type_data, &_ty_string_data);
struct haste_value ty_cstr           = VAL_OBJ(&_ty_type_data, &_ty_cstr_data);

enum arith_op {
	ARITH_ADD,
	ARITH_SUB,
	ARITH_MUL,
	ARITH_DIV,
};

static bool value_is_any_int(struct haste_value v)
{
	return IS_SCALAR(v) and (v.type == &_ty_int_data or v.type == &_ty_untyped_int_data);
}

static bool value_is_any_float(struct haste_value v)
{
	return IS_SCALAR(v) and (v.type == &_ty_float_data or v.type == &_ty_untyped_float_data);
}

static struct haste_value arith_float(enum arith_op op, struct haste_value lhs, struct haste_value rhs)
{
	double a = value_is_any_float(lhs) then lhs.floating otherwise (double)lhs.integer;
	double b = value_is_any_float(rhs) then rhs.floating otherwise (double)rhs.integer;
	double res = 0.0;

	switch (op) {
	case ARITH_ADD: res = a + b; break;
	case ARITH_SUB: res = a - b; break;
	case ARITH_MUL: res = a * b; break;
	case ARITH_DIV:
		if (b == 0.0) return VAL_BAD;
		res = a / b;
		break;
	}

	if (lhs.type == &_ty_float_data or rhs.type == &_ty_float_data)
		return VAL_SCALAR(AS_TYPE(ty_float), .floating = res);

	return VAL_SCALAR(AS_TYPE(ty_untyped_float), .floating = res);
}

static struct haste_value arith_int(enum arith_op op, struct haste_value lhs, struct haste_value rhs)
{
	int64_t a = lhs.integer;
	int64_t b = rhs.integer;
	int64_t res = 0;

	switch (op) {
	case ARITH_ADD: res = a + b; break;
	case ARITH_SUB: res = a - b; break;
	case ARITH_MUL: res = a * b; break;
	case ARITH_DIV:
		if (b == 0) return VAL_BAD;
		res = a / b;
		break;
	}

	if (lhs.type == &_ty_int_data or rhs.type == &_ty_int_data)
		return VAL_SCALAR(AS_TYPE(ty_int), .integer = res);

	return VAL_SCALAR(AS_TYPE(ty_untyped_int), .integer = res);
}

static struct haste_value value_do_arith(
	const enum arith_op op,
	const struct haste_value lhs,
	const struct haste_value rhs)
{
	if (not (value_is_any_int(lhs) or value_is_any_float(lhs))) return VAL_BAD;
	if (not (value_is_any_int(rhs) or value_is_any_float(rhs))) return VAL_BAD;
	if (not type_equal(typeof(lhs), typeof(rhs))
	    and not type_is_untyped(typeof(lhs))
	    and not type_is_untyped(typeof(rhs)))
		return VAL_BAD;

	if (value_is_any_float(lhs) or value_is_any_float(rhs))
		return arith_float(op, lhs, rhs);

	return arith_int(op, lhs, rhs);
}

struct haste_value value_add(const struct haste_value lhs, const struct haste_value rhs)
{
	return value_do_arith(ARITH_ADD, lhs, rhs);
}

struct haste_value value_sub(const struct haste_value lhs, const struct haste_value rhs)
{
	return value_do_arith(ARITH_SUB, lhs, rhs);
}

struct haste_value value_mul(const struct haste_value lhs, const struct haste_value rhs)
{
	return value_do_arith(ARITH_MUL, lhs, rhs);
}

struct haste_value value_div(const struct haste_value lhs, const struct haste_value rhs)
{
	return value_do_arith(ARITH_DIV, lhs, rhs);
}

typedef struct {
	int64_t as_int;
	double as_float;
	bool is_float;
} RawNumber;

static RawNumber extract_raw(struct haste_value value)
{
	if (value_is_any_int(value))   return (RawNumber){ .as_int = value.integer,           .as_float = (double)value.integer, };
	if (value_is_any_float(value)) return (RawNumber){ .as_int = (int64_t)value.floating, .as_float = value.floating, };
	unreachable();
}

static struct haste_value construct_from_raw(struct haste_value to, RawNumber raw)
{
	if (type_equal(to, ty_int))
		return VAL_SCALAR(AS_TYPE(ty_int), .integer = raw.as_int);

	if (type_equal(to, ty_float))
		return VAL_SCALAR(AS_TYPE(ty_float), .floating = raw.as_float);

	unreachable();
}

static struct haste_string_object _default_empty_string = {
	.base = { .kind = HASTE_OBJ_STRING },
	.data = "",
	.len = 0,
};

struct haste_value zero_for_type(struct Allocator alloc, struct haste_value to)
{
	if (IS_STRUCT_TYPE(to)) {
		struct haste_struct_type *st = AS_STRUCT_TYPE(to);
		struct haste_struct_object *so = create(alloc, struct haste_struct_object,
			.base.kind = HASTE_OBJ_STRUCT);
		so->fields = alloc(alloc, sizeof(struct haste_value) * st->field_count);
		for (size_t i=0; i < st->field_count; i += 1) {
			so->fields[i] = default_for_type(alloc, st->fields[i].type);
		}
		return VAL_OBJ(AS_TYPE(to), so);
	}
	return default_for_type(alloc, to);
}

struct haste_value default_for_type(struct Allocator alloc, struct haste_value to)
{
	if (type_equal(to, ty_int))   return VAL_SCALAR(AS_TYPE(ty_int), .integer = 0);
	if (type_equal(to, ty_float)) return VAL_SCALAR(AS_TYPE(ty_float), .floating = 0.0f);
	if (type_equal(to, ty_string))
		return (struct haste_value){ .kind = HASTE_VL_OBJ, .type = AS_TYPE(ty_string), .obj = &_default_empty_string.base };
	if (type_equal(to, ty_cstr))
		return (struct haste_value){ .kind = HASTE_VL_OBJ, .type = AS_TYPE(ty_cstr), .obj = &_default_empty_string.base };
	if (IS_STRUCT_TYPE(to)) {
		struct haste_struct_type *st = AS_STRUCT_TYPE(to);
		struct haste_struct_object *so = create(alloc, struct haste_struct_object,
			.base = { .kind = HASTE_OBJ_STRUCT });
		so->fields = alloc(alloc, sizeof(struct haste_value) * st->field_count);
		for (size_t i = 0; i < st->field_count; i += 1) {
			if (st->fields[i].has_default) {
				so->fields[i] = st->fields[i].default_value;
			} else
				so->fields[i] = default_for_type(alloc, st->fields[i].type);
		}
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type = AS_TYPE(to),
			.obj = &so->base,
		};
	}
	unreachable();
}

struct haste_value value_cast(struct Allocator alloc, const struct haste_value to, const struct haste_value value)
{
	ASSERT_IS_TYPE(to);
	const struct haste_value value_type = typeof(value);

	if (type_is_untyped(to))               unreachable();
	if (type_equal(to, ty_auto))           unreachable();
	if (IS_RUNTIME(value))                 unimplemented();
	if (IS_BAD(value))                     return VAL_BAD;
	if (value_equal(value, VAL_UNINIT))    return default_for_type(alloc, to);
	if (not type_can_cast(to, value_type)) return VAL_BAD;
	if (type_equal(to, value_type))        return value;
	if (IS_ZERO(value))      return zero_for_type(alloc, to);

	if ((type_equal(to, ty_string) or type_equal(to, ty_cstr))
		and IS_OBJ(value) and value.obj->kind == HASTE_OBJ_STRING) {
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type = AS_TYPE(to),
			.obj = value.obj,
		};
	}

	return construct_from_raw(to, extract_raw(value));
}

struct haste_value typeof(const struct haste_value value)
{
	switch (value.kind) {
	case HASTE_VL_NONE:    unreachable();
	case HASTE_VL_BAD:     return VAL_BAD;
	case HASTE_VL_ZERO:    return ty_zero;
	case HASTE_VL_UNINIT:  return ty_unknown;
	case HASTE_VL_RUNTIME: return value.runtime->type;
	case HASTE_VL_SCALAR:
	case HASTE_VL_OBJ:
		return VAL_OBJ(&_ty_type_data, (struct haste_object*)value.type);
	}
	unreachable();
}

bool object_equal(struct haste_object *a, struct haste_object *b)
{
	if (a == b) return true;
	if (a->kind != b->kind) return false;
	switch (a->kind) {
	case HASTE_OBJ_TYPE:
		return type_equal(VAL_OBJ(&_ty_type_data, a), VAL_OBJ(&_ty_type_data, b));
	case HASTE_OBJ_STRING: {
		struct haste_string_object *sa = (struct haste_string_object*)a;
		struct haste_string_object *sb = (struct haste_string_object*)b;
		return sa->len == sb->len and memcmp(sa->data, sb->data, sa->len) == 0;
	}
	case HASTE_OBJ_STRUCT:
		return a == b;
	}
	return false;
}

bool value_equal(struct haste_value a, struct haste_value b)
{
	switch (a.kind) {
	case HASTE_VL_NONE:
	case HASTE_VL_BAD:           return false;
	case HASTE_VL_UNINIT:        return b.kind == HASTE_VL_UNINIT;
	case HASTE_VL_ZERO:
		return b.kind == HASTE_VL_ZERO or value_equal(b, VAL_SCALAR(AS_TYPE(ty_untyped_int), 0));
	case HASTE_VL_SCALAR:
		if (not IS_SCALAR(b)) return false;
		if (value_is_any_float(a) or value_is_any_float(b)) {
			double fa = value_is_any_float(a) then a.floating otherwise (double)a.integer;
			double fb = value_is_any_float(b) then b.floating otherwise (double)b.integer;
			return fa == fb;
		}
		return a.integer == b.integer;
	case HASTE_VL_RUNTIME:
		return false;
	case HASTE_VL_OBJ:
		if (not IS_OBJ(b)) return false;
		return object_equal(a.obj, b.obj);
	}
}

bool type_equal(const struct haste_value v1,
                const struct haste_value v2)
{
	ASSERT_IS_TYPE(v1);
	ASSERT_IS_TYPE(v2);

	const struct haste_object_type *t1 = AS_TYPE(v1);
	const struct haste_object_type *t2 = AS_TYPE(v2);

	if (t1 == t2) return true;
	if (t1->kind != t2->kind) return false;

	if (t1->kind == HASTE_TY_STRUCT) {
		const struct haste_struct_type *s1 = (const struct haste_struct_type *)t1;
		const struct haste_struct_type *s2 = (const struct haste_struct_type *)t2;
		if (s1->field_count != s2->field_count) return false;
		for (size_t i = 0; i < s1->field_count; i++) {
			if (s1->fields[i].name != s2->fields[i].name) return false;
			if (!type_equal(s1->fields[i].type, s2->fields[i].type)) return false;
		}
		return true;
	}

	return t1->size == t2->size and t1->align == t2->align;
}

bool type_can_assign(const struct haste_value assignable,
                     const struct haste_value value)
{
	ASSERT_IS_TYPE(assignable);
	ASSERT_IS_TYPE(value);
	assert(not type_is_untyped(assignable) and "the assignable shouldn't be untyped.");

	if (type_equal(assignable, value))    return true;
	if (type_equal(assignable, ty_auto))  return not type_equal(value, ty_unknown);
	if (type_equal(value, ty_zero))       return true;
	if (type_equal(assignable, ty_int))   return type_equal(value, ty_unknown) or type_is_integer(value);
	if (type_equal(assignable, ty_float)) return type_equal(value, ty_unknown) or type_is_untyped_number(value);
	if (type_equal(assignable, ty_string))
		return type_equal(value, ty_string) or type_equal(value, ty_untyped_string) or type_equal(value, ty_unknown);
	if (type_equal(assignable, ty_cstr))
		return type_equal(value, ty_cstr) or type_equal(value, ty_untyped_string) or type_equal(value, ty_unknown);

	if (type_equal(value, ty_unknown)) return true;

	return false;
}

bool type_can_cast(const struct haste_value to,
                   const struct haste_value from)
{
	ASSERT_IS_TYPE(to);
	ASSERT_IS_TYPE(from);

	if (type_equal(from, ty_unknown)) return true;
	if (type_equal(to, ty_auto))      return true;
	if (type_equal(to, from))         return true;
	if (type_equal(from, ty_zero))    return true;
	if (type_is_number(to))           return type_is_number(from);
	if (type_equal(to, ty_string) or type_equal(to, ty_cstr) or type_equal(to, ty_untyped_string))
		return type_equal(from, ty_string) or type_equal(from, ty_cstr) or type_equal(from, ty_untyped_string) or type_equal(from, ty_unknown);

	return false;
}

bool type_is_integer(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object *obj = AS_OBJ(t);
	return type_equal(t, ty_int)
		or type_equal(t, ty_untyped_int)
		or obj->kind == HASTE_TY_INT
		or obj->kind == HASTE_TY_UNTYPED_INT;
}

bool type_is_float(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object *obj = AS_OBJ(t);
	return type_equal(t, ty_float)
		or type_equal(t, ty_untyped_float)
		or obj->kind == HASTE_TY_FLOAT
		or obj->kind == HASTE_TY_UNTYPED_FLOAT;
}

bool type_is_number(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	return type_is_integer(t) or type_is_float(t);
}

bool type_is_untyped(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object_type *obj = AS_TYPE(t);
	return obj->kind == HASTE_TY_UNTYPED_INT
		or obj->kind == HASTE_TY_UNTYPED_FLOAT
		or obj->kind == HASTE_TY_UNTYPED_STRING;
}

bool type_is_untyped_number(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	return type_is_number(t) and type_is_untyped(t);
}

int print_object(stream_t stream, const struct haste_object *obj, const struct haste_object *type)
{
	int printed_amount = 0;
	switch (obj->kind) {
	case HASTE_OBJ_TYPE: {
		struct haste_object_type *type = OAS_TYPE(obj);
		switch (type->kind) {
		case HASTE_TY_UNKNOWN:
			printed_amount += sprint(stream, "unknown");
			break;
		case HASTE_TY_TYPE:
			printed_amount += sprint(stream, "type");
			break;
		case HASTE_TY_ZERO:
			printed_amount += sprint(stream, "zero");
			break;
		case HASTE_TY_INT:
			printed_amount += sprint(stream, "int");
			break;
		case HASTE_TY_UNTYPED_INT:
			printed_amount += sprint(stream, "untyped_int");
			break;
		case HASTE_TY_FLOAT:
			printed_amount += sprint(stream, "float");
			break;
		case HASTE_TY_UNTYPED_FLOAT:
			printed_amount += sprint(stream, "untyped_float");
			break;
		case HASTE_TY_AUTO:
			printed_amount += sprint(stream, "auto");
			break;
		case HASTE_TY_VOID:
			printed_amount += sprint(stream, "void");
			break;
		case HASTE_TY_UNTYPED_STRING:
			printed_amount += sprint(stream, "untyped_string");
			break;
		case HASTE_TY_STRING:
			printed_amount += sprint(stream, "string");
			break;
		case HASTE_TY_CSTR:
			printed_amount += sprint(stream, "cstr");
			break;
		case HASTE_TY_STRUCT:
			printed_amount += sprint(stream, "struct");
			break;
		}
	} break;
	case HASTE_OBJ_STRING: {
		struct haste_string_object *s = (struct haste_string_object*)obj;
		printed_amount += sprint(stream, "{s:*}", s->data, (int)s->len);
	} break;
	case HASTE_OBJ_STRUCT: {
		printed_amount += sprint(stream, "struct {"); // TODO: Make it prints struct's name
		struct haste_struct_object *so = OAS_STRUCT(obj);
		struct haste_struct_type *st = OAS_STRUCT_TYPE(type);
		for (size_t i=0; i<st->field_count; i += 1) {
			struct haste_struct_field field = st->fields[i];
			struct haste_value field_value = so->fields[i];
			printed_amount += sprint(stream, "{s}: {value},", field.name, field_value);
		}
		printed_amount += sprint(stream, "}");
	} break;
	}
	return printed_amount;
}

int print_value(stream_t stream, const struct haste_value value)
{
	int printed_amount = 0;

	switch (value.kind) {
	case HASTE_VL_NONE:
		printed_amount += sprint(stream, "NONE");
		break;
	case HASTE_VL_BAD:
		printed_amount += sprint(stream, "BAD");
		break;
	case HASTE_VL_ZERO:
		printed_amount += sprint(stream, "ZERO");
		break;
	case HASTE_VL_UNINIT:
		printed_amount += sprint(stream, "UNINIT");
		break;
	case HASTE_VL_SCALAR:
		if (value_is_any_float(value))
			printed_amount += sprint(stream, "{lf}", value.floating);
		else
			printed_amount += sprint(stream, "{i64}", value.integer);
		break;
	case HASTE_VL_RUNTIME:
		printed_amount += print_haste_ast(stream, value.runtime);
		break;
	case HASTE_VL_OBJ:
		printed_amount += sprint(stream, "{obj}", value);
		break;
	}

	return printed_amount;
}
