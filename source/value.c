#include "haste.h"
#include <signal.h>

#define ASSERT_IS_TYPE(...) \
	do { \
		assert(IS_TYPE(__VA_ARGS__) and "It should be a type. maybe you forgot to use `typeof()`?"); \
	} while (0)

struct haste_value ty_unknown = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_UNKNOWN));
struct haste_value ty_type = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_TYPE,
	.size = 8,
	.align = 8));
struct haste_value ty_int = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_INT,
	.size = 4,
	.align = 4));
struct haste_value ty_untyped_int = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_UNTYPED_INT,
	.size = 4,
	.align = 4));
struct haste_value ty_float = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_FLOAT,
	.size = 4,
	.align = 4));
struct haste_value ty_untyped_float = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_UNTYPED_FLOAT,
	.size = 4,
	.align = 4));
struct haste_value ty_auto = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_AUTO));
struct haste_value ty_void = VAL_OBJ(&OBJ_TYPE(
	.kind = HASTE_TY_VOID));

enum arith_op {
	ARITH_ADD,
	ARITH_SUB,
	ARITH_MUL,
	ARITH_DIV,
};

static bool value_is_any_int(struct haste_value v)
{
	return v.kind == HASTE_VL_INT or v.kind == HASTE_VL_UNTYPED_INT;
}

static bool value_is_any_float(struct haste_value v)
{
	return v.kind == HASTE_VL_FLOAT or v.kind == HASTE_VL_UNTYPED_FLOAT;
}

static struct haste_value value_do_arith(
	const enum arith_op op,
	const struct haste_value lhs,
	const struct haste_value rhs)
{
	/* reject invalid */
	if (not (value_is_any_int(lhs) or value_is_any_float(lhs))) return VAL_NONE;
	if (not (value_is_any_int(rhs) or value_is_any_float(rhs))) return VAL_NONE;

	const bool use_float =
		value_is_any_float(lhs) or value_is_any_float(rhs);

	if (use_float) {
		double a = value_is_any_float(lhs) then lhs.floating otherwise (double)lhs.integer;
		double b = value_is_any_float(rhs) then rhs.floating otherwise (double)rhs.integer;
		double res = 0.0;

		switch (op) {
		case ARITH_ADD: res = a + b; break;
		case ARITH_SUB: res = a - b; break;
		case ARITH_MUL: res = a * b; break;
		case ARITH_DIV:
			if (b == 0.0) return VAL_NONE;
			res = a / b;
			break;
		}

		/* result typing rules */
		if (lhs.kind == HASTE_VL_FLOAT or rhs.kind == HASTE_VL_FLOAT)
			return VAL_FLOAT(res);

		return VAL_UNTYPED_FLOAT(res);
	}

	int64_t a = lhs.integer;
	int64_t b = rhs.integer;
	int64_t res = 0;

	switch (op) {
	case ARITH_ADD: res = a + b; break;
	case ARITH_SUB: res = a - b; break;
	case ARITH_MUL: res = a * b; break;
	case ARITH_DIV:
		if (b == 0) return VAL_NONE;
		res = a / b;
		break;
	}

	/* preserve typed vs untyped */
	if (lhs.kind == HASTE_VL_INT or rhs.kind == HASTE_VL_INT)
		return VAL_INT(res);

	return VAL_UNTYPED_INT(res);
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

struct haste_value value_cast(const struct haste_value to, const struct haste_value value)
{
	ASSERT_IS_TYPE(to);

	if (type_is_untyped(to)) crash();
	if (type_equal(to, ty_auto)) crash();

	struct haste_value value_type = typeof(value);
	if (not type_can_cast(to, value_type)) return VAL_NONE;
	if (type_equal(to, value_type)) return value;
	if (IS_RUNTIME(value)) unimplemented();

	/* uninit -> any */
	if (value_equal(value, VAL_UNINIT)) {
		if (type_equal(to, ty_int)) {
			return VAL_INT(0);
		}
		if (type_equal(to, ty_float)) {
			return VAL_FLOAT(0.0f);
		}
		unreachable();
	}

	/* untyped_int -> int, float */
	if (type_equal(value_type, ty_untyped_int)) {
		if (type_equal(to, ty_int)) {
			return VAL_INT(value.integer);
		}
		if (type_equal(to, ty_float)) {
			return VAL_FLOAT((float)value.integer);
		}
		unreachable();
	}

	/* int -> float */
	if (type_equal(value_type, ty_int)) {
		if (type_equal(to, ty_float)) {
			return VAL_FLOAT((float)value.integer);
		}
		unreachable();
	}


	/* untyped_float -> float, int */
	if (type_equal(value_type, ty_untyped_float)) {
		if (type_equal(to, ty_float)) {
			return VAL_FLOAT((float)value.floating);
		}
		if (type_equal(to, ty_int)) {
			return VAL_INT((int64_t)value.floating);
		}
		unreachable();
	}

	/* float -> int */
	if (type_equal(value_type, ty_float)) {
		if (type_equal(to, ty_int)) {
			return VAL_INT((int64_t)value.floating);
		}
		unreachable();
	}

	unreachable();
}

struct haste_value typeof_object(const struct haste_object *obj)
{
	switch (obj->kind) {
	case HASTE_OBJ_TYPE:
		return ty_type;
	}
}

struct haste_value typeof(const struct haste_value value)
{
	switch (value.kind) {
	case HASTE_VL_NONE:
		raise(SIGSEGV); // ragebait ASAN
		unreachable();
	case HASTE_VL_UNINIT:        return ty_unknown;
	case HASTE_VL_INT:           return ty_int;
	case HASTE_VL_UNTYPED_INT:   return ty_untyped_int;
	case HASTE_VL_FLOAT:         return ty_float;
	case HASTE_VL_UNTYPED_FLOAT: return ty_untyped_float;
	case HASTE_VL_RUNTIME:       return value.runtime->type;
	case HASTE_VL_OBJ:           return typeof_object(value.obj);
	}
}

bool object_equal(struct haste_object *a, struct haste_object *b)
{
	if (a == b) return true;
	if (a->kind != b->kind) return false;
	switch (a->kind) {
	case HASTE_OBJ_TYPE:
		return type_equal(VAL_OBJ(a), VAL_OBJ(b));
	}
	return false;
}

bool value_equal(struct haste_value a, struct haste_value b)
{
	switch (a.kind) {
	case HASTE_VL_NONE:          return false;
	case HASTE_VL_UNINIT:        return b.kind == HASTE_VL_UNINIT;
	case HASTE_VL_INT:
	case HASTE_VL_UNTYPED_INT:
		if (b.kind != HASTE_VL_INT and b.kind != HASTE_VL_UNTYPED_INT) return false;
		return a.integer == b.integer;
	case HASTE_VL_FLOAT:
	case HASTE_VL_UNTYPED_FLOAT:
		if (b.kind == HASTE_VL_UNTYPED_INT)
			return a.floating == (double)b.integer;
		if (b.kind != HASTE_VL_FLOAT and b.kind != HASTE_VL_UNTYPED_FLOAT)
			return false;
		return a.floating == b.floating;
	case HASTE_VL_RUNTIME:
		return false;
	case HASTE_VL_OBJ:
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
	if (type_equal(assignable, ty_int))   return type_equal(value, ty_unknown) or type_is_integer(value);
	if (type_equal(assignable, ty_float)) return type_equal(value, ty_unknown) or type_is_untyped_number(value);

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
	if (type_is_number(to))           return type_is_number(from);

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
		or obj->kind == HASTE_TY_UNTYPED_FLOAT;
}

bool type_is_untyped_number(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	return type_is_number(t) and type_is_untyped(t);
}

int print_object(stream_t stream, const struct haste_object *obj)
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
		}
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
	case HASTE_VL_UNINIT:
		printed_amount += sprint(stream, "UNINIT");
		break;
	case HASTE_VL_INT:
	case HASTE_VL_UNTYPED_INT:
		printed_amount += sprint(stream, "{i64}", value.integer);
		break;
	case HASTE_VL_FLOAT:
	case HASTE_VL_UNTYPED_FLOAT:
		printed_amount += sprint(stream, "{lf}", value.floating);
		break;
	case HASTE_VL_RUNTIME:
		printed_amount += print_haste_ast(stream, value.runtime);
		break;
	case HASTE_VL_OBJ:
		printed_amount += sprint(stream, "{obj}", value.obj);
		break;
	}

	return printed_amount;
}
