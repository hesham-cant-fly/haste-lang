#include "haste.h"
#include "my_common.h"
#include <assert.h>
#include <signal.h>

#define ASSERT_IS_TYPE(...) \
	do { \
		assert(IS_TYPE(__VA_ARGS__) and "It should be a type. maybe you forgot to use `typeof()`?"); \
	} while (0)



#define TY_POOL_CHUNK 256
#define ty_pool_get(pool, i) ((pool).chunks.items[(i) / TY_POOL_CHUNK][(i) % TY_POOL_CHUNK])

static struct haste_object_type *ensure_reserved_type(TypeID id)
{
	if (id > HASTE_TID_TOTAL_RESERVED) return NULL;

	bool is_signed = id < HASTE_TID_RESERVED_UINT_BASE;
	uint16_t bits = is_signed
		? (uint16_t)(id - HASTE_TID_RESERVED_INT_BASE)
		: (uint16_t)(id - HASTE_TID_RESERVED_UINT_BASE);
	size_t bytes = (bits + 7) / 8;

	struct haste_object_type type = OBJ_TYPE(
		.pool_id = id,
		.kind = is_signed ? HASTE_TY_INT : HASTE_TY_UINT,
		.bit_size = bits,
		.size = bytes,
		.align = bytes < 8 ? bytes : 8,
	);

	*type_pool_get(id) = type;
	return type_pool_get(id);
}

struct type_pool g_type_pool = {0};

static void type_pool_grow(void)
{
	void *chunk = alloc(g_type_pool.allocator, sizeof(struct haste_struct_type) * TY_POOL_CHUNK);
	arrpush(g_type_pool.allocator, g_type_pool.chunks, chunk);
}

TypeID type_pool_add(struct haste_object_type *type)
{
	if (g_type_pool.len >= g_type_pool.chunks.len * TY_POOL_CHUNK) {
		type_pool_grow();
	}
	TypeID id = (TypeID)(g_type_pool.len++);
	struct haste_struct_type *slot = &ty_pool_get(g_type_pool, id);

	if (type->kind == HASTE_TY_STRUCT or type->kind == HASTE_TY_AUTO_STRUCT) {
		*slot = *(struct haste_struct_type *)type;
	} else {
		memset(slot, 0, sizeof(*slot));
		slot->base = *type;
	}

	slot->base.pool_id = id;
	type->pool_id = id;
	return id;
}

struct haste_object_type *type_pool_get(TypeID id)
{
	assert(id < g_type_pool.len);
	return &ty_pool_get(g_type_pool, id).base;
}

void type_pool_set_name(TypeID id, const char *name)
{
	if (HASTE_TID_IS_RESERVED(id)) return;
	assert(id < g_type_pool.len);
	ty_pool_get(g_type_pool, id).base.name = name;
}

static struct haste_object_type _ty_zero_data           = OBJ_TYPE(.kind = HASTE_TY_ZERO,                                  .name = "zero");
static struct haste_object_type _ty_unknown_data        = OBJ_TYPE(.kind = HASTE_TY_UNKNOWN,                               .name = "uninit");
static struct haste_object_type _ty_type_data           = OBJ_TYPE(.kind = HASTE_TY_TYPE,          .size = 8, .align = 8,  .name = "type");
static struct haste_object_type _ty_untyped_int_data    = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_INT,   .size = 4, .align = 4,  .name = "untyped_int");
static struct haste_object_type _ty_float_data          = OBJ_TYPE(.kind = HASTE_TY_FLOAT,         .size = 4, .align = 4,  .name = "float");
static struct haste_object_type _ty_untyped_float_data  = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_FLOAT, .size = 4, .align = 4,  .name = "untyped_float");
static struct haste_object_type _ty_auto_data           = OBJ_TYPE(.kind = HASTE_TY_AUTO,                                  .name = "auto");
static struct haste_object_type _ty_void_data           = OBJ_TYPE(.kind = HASTE_TY_VOID,                                  .name = "void");
static struct haste_object_type _ty_untyped_string_data = OBJ_TYPE(.kind = HASTE_TY_UNTYPED_STRING, .size = 8, .align = 8, .name = "untyped_string");
static struct haste_object_type _ty_cstr_data           = OBJ_TYPE(.kind = HASTE_TY_CSTR, .size = 8, .align = 4,           .name = "cstr");
static struct haste_object_type _ty_usize_data          = OBJ_TYPE(.kind = HASTE_TY_USIZE, .size = 8, .align = 8,          .name = "usize");

struct haste_value ty_int            = {0};
struct haste_value ty_uint           = {0};
struct haste_value ty_zero           = {0};
struct haste_value ty_unknown        = {0};
struct haste_value ty_type           = {0};
struct haste_value ty_untyped_int    = {0};
struct haste_value ty_float          = {0};
struct haste_value ty_untyped_float  = {0};
struct haste_value ty_auto           = {0};
struct haste_value ty_void           = {0};
struct haste_value ty_untyped_string = {0};
struct haste_value ty_string         = {0};
struct haste_value ty_cstr           = {0};
struct haste_value ty_usize          = {0};

static Error eval(struct Allocator allocator, struct intern_table *table, const char *input, struct haste_value *out)
{
	Error err = 0;

	struct token_list tokens = {0};
	err = scan_entire_string(allocator, table, input, &tokens);
	if (err) return ERROR;

	struct Arena arena = Arena(allocator);
	struct Allocator arena_allocator = arena_get_allocator(&arena);

	struct haste_ast_node *node = {0};
	err = parse_expr(arena_allocator, tokens, &node);
	if (err) {
		arrfree(allocator, tokens);
		arena_free(&arena);
		return ERROR;
	}

	err = analyze_one_node(allocator, arena_allocator, table, node, out);
	if (err) {
		arrfree(allocator, tokens);
		arena_free(&arena);
		return ERROR;
	}

	arrfree(allocator, tokens);
	arena_free(&arena);
	return OK;
}

struct haste_value type_get_int(uint16_t bits, bool is_signed)
{
	TypeID base = is_signed ? HASTE_TID_RESERVED_INT_BASE : HASTE_TID_RESERVED_UINT_BASE;
	return VAL_OBJ(AS_TYPEID(ty_type), ensure_reserved_type(base + bits));
}

void set_up_builtins(struct Allocator allocator, struct intern_table *table)
{
	g_type_pool.allocator = allocator;
	while (g_type_pool.chunks.len * TY_POOL_CHUNK <= HASTE_TID_TOTAL_RESERVED) {
		type_pool_grow();
	}

	g_type_pool.len = HASTE_TID_TOTAL_RESERVED + 1;

	TypeID tid_type = type_pool_add(&_ty_type_data);
	ty_type = VAL_OBJ(tid_type, type_pool_get(tid_type));

#define REGISTER_BUILTIN(val, data) \
	do { \
		TypeID tid = type_pool_add(&(data)); \
		(val) = VAL_OBJ(AS_TYPEID(ty_type), type_pool_get(tid)); \
	} while (0)

	REGISTER_BUILTIN(ty_zero, _ty_zero_data);
	REGISTER_BUILTIN(ty_unknown, _ty_unknown_data);
	REGISTER_BUILTIN(ty_untyped_int, _ty_untyped_int_data);
	REGISTER_BUILTIN(ty_float, _ty_float_data);
	REGISTER_BUILTIN(ty_untyped_float, _ty_untyped_float_data);
	REGISTER_BUILTIN(ty_auto, _ty_auto_data);
	REGISTER_BUILTIN(ty_void, _ty_void_data);
	REGISTER_BUILTIN(ty_untyped_string, _ty_untyped_string_data);
	REGISTER_BUILTIN(ty_cstr, _ty_cstr_data);
	REGISTER_BUILTIN(ty_usize, _ty_usize_data);

	Error err = eval(
		allocator,
		table,
		"struct { ptr: cstr; len: usize; }",
		&ty_string);
	if (err) panic("Cannot Initialize a built-in string.");
	AS_TYPE(ty_string)->name = "string";

	err = eval(
		allocator,
		table,
		"int32",
		&ty_int);
	if (err) panic("Cannot Initialize the int type");
	AS_TYPE(ty_int)->name = "int";

	err = eval(
		allocator,
		table,
		"uint32",
		&ty_uint);
	if (err) panic("Cannot Initialize the uint type");
	AS_TYPE(ty_uint)->name = "uint";
}

enum arith_op {
	ARITH_ADD,
	ARITH_SUB,
	ARITH_MUL,
	ARITH_DIV,
};

static bool value_is_any_int(struct haste_value v)
{
	const struct haste_value tp = typeof(v);
	return IS_SCALAR(v) and type_is_integer(tp);
}

static bool value_is_any_float(struct haste_value v)
{
	const struct haste_value tp = typeof(v);
	return IS_SCALAR(v) and type_is_float(tp);
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

	if (typeof(lhs).kind == HASTE_TY_FLOAT or typeof(rhs).kind == HASTE_TY_UNTYPED_FLOAT)
		return VAL_SCALAR(AS_TYPEID(ty_float), .floating = res);

	return VAL_SCALAR(AS_TYPEID(ty_untyped_float), .floating = res);
}

static struct haste_value arith_int(enum arith_op op, struct haste_value lhs, struct haste_value rhs)
{
	int64_t a = lhs.integer;
	int64_t b = rhs.integer;
	int64_t res = 0;

	switch (op) {
	case ARITH_ADD:
		if ((b > 0 && a > INT64_MAX - b) or (b < 0 && a < INT64_MIN - b))
			return VAL_BAD;
		res = a + b;
		break;
	case ARITH_SUB:
		if ((b > 0 && a < INT64_MIN + b) or (b < 0 && a > INT64_MAX + b))
			return VAL_BAD;
		res = a - b;
		break;
	case ARITH_MUL:
		if (a != 0 and b != 0) {
			if ((a > 0 and b > 0 and a > INT64_MAX / b)
			    or (a > 0 and b < 0 and b < INT64_MIN / a)
			    or (a < 0 and b > 0 and a < INT64_MIN / b)
			    or (a < 0 and b < 0 and a < INT64_MAX / b))
				return VAL_BAD;
		}
		res = a * b;
		break;
	case ARITH_DIV:
		if (b == 0 or (a == INT64_MIN and b == -1)) return VAL_BAD;
		res = a / b;
		break;
	}

	if (type_equal(typeof(lhs), typeof(rhs)))
		return VAL_SCALAR(lhs.type_id, .integer = res);

	return VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = res);
}

static struct haste_value value_do_arith(
	const enum arith_op op,
	struct haste_value lhs,
	struct haste_value rhs)
{
	if (IS_ZERO(lhs)) lhs = VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 0);
	if (IS_ZERO(rhs)) rhs = VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 0);

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
	ASSERT_IS_TYPE(to);

	if (type_is_integer(to))
		return VAL_SCALAR(AS_TYPEID(to), .integer = raw.as_int);

	if (type_is_float(to))
		return VAL_SCALAR(AS_TYPEID(to), .floating = raw.as_float);

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
		return VAL_OBJ(AS_TYPE(to)->pool_id, so);
	}
	return default_for_type(alloc, to);
}

struct haste_value default_for_type(struct Allocator alloc, struct haste_value type)
{
	ASSERT_IS_TYPE(type);

	if (type_is_integer(type)) return VAL_SCALAR(AS_TYPEID(type), .integer = 0);
	if (type_is_float(type))   return VAL_SCALAR(AS_TYPEID(type), .floating = 0.0f);
	if (type_equal(type, ty_cstr))
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type_id = AS_TYPEID(type),
			.obj = &_default_empty_string.base
		};
	if (IS_STRUCT_TYPE(type)) {
		struct haste_struct_type *st = AS_STRUCT_TYPE(type);
		struct haste_struct_object *so = create(alloc, struct haste_struct_object,
			.base = { .kind = HASTE_OBJ_STRUCT });
		so->fields = alloc(alloc, sizeof(struct haste_value) * st->field_count);
		for (size_t i = 0; i < st->field_count; i += 1) {
			if (st->fields[i].has_default) {
				so->fields[i] = st->fields[i].default_value;
			} else {
				so->fields[i] = default_for_type(alloc, st->fields[i].type);
			}
		}
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type_id = AS_TYPE(type)->pool_id,
			.obj = &so->base,
		};
	}
	raise(SIGSEGV);
	unreachable();
}

struct haste_value value_cast(struct Allocator alloc, const struct haste_value to, const struct haste_value value)
{
	ASSERT_IS_TYPE(to);
	const struct haste_value value_type = typeof(value);

	if (type_is_untyped(to))               unreachable();
	if (IS_RUNTIME(value))                 unimplemented();

	if (type_equal(to, ty_auto))           return value;
	if (IS_BAD(value))                     return VAL_BAD;
	if (not type_can_cast(to, value_type)) return VAL_BAD;
	if (type_equal(to, value_type))        return value;

	if (value_equal(value, VAL_UNINIT))    return default_for_type(alloc, to);
	if (IS_ZERO(value))                    return zero_for_type(alloc, to);

	if (type_equal(to, ty_string) and IS_OBJ(value) and value.obj->kind == HASTE_OBJ_STRING) {
		struct haste_string_object *s = (struct haste_string_object*)value.obj;
		struct haste_struct_type *st = AS_STRUCT_TYPE(to);
		struct haste_struct_object *so = create(alloc, struct haste_struct_object,
			.base = { .kind = HASTE_OBJ_STRUCT });
		so->fields = alloc(alloc, sizeof(struct haste_value) * st->field_count);
		so->fields[0] = value_cast(alloc, st->fields[0].type, value);
		so->fields[1] = VAL_SCALAR(AS_TYPEID(ty_usize), .integer = (int64_t)s->len);
		return VAL_OBJ(AS_TYPE(to)->pool_id, so);
	}

	if (type_equal(to, ty_cstr) and IS_STRUCT(value)) {
		struct haste_struct_object *so = AS_STRUCT(value);
		ssize_t idx = find_named_field(AS_STRUCT_TYPE(typeof(value)), "ptr");
		assert(idx >= 0);
		return so->fields[idx];
	}

	if (type_is_any_string(to) and IS_OBJ(value) and value.obj->kind == HASTE_OBJ_STRING) {
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type_id = AS_TYPE(to)->pool_id,
			.obj = value.obj,
		};
	}

	if (IS_STRUCT_TYPE(to) and IS_AUTO_STRUCT_TYPE(value_type)) {
		const struct haste_struct_type *to_st = AS_STRUCT_TYPE(to);
		const struct haste_struct_type *val_st = AS_STRUCT_TYPE(value_type);

		const struct haste_struct_object *val_so = AS_STRUCT(value);

		struct haste_value result = default_for_type(alloc, to);
		struct haste_struct_object *so = AS_STRUCT(result);

		for (size_t i=0; i < to_st->field_count; i += 1) {
			struct haste_struct_field ass_field = to_st->fields[i];

			for (size_t j=0; j < val_st->field_count; j += 1) {
				struct haste_struct_field val_field = val_st->fields[j];
				if (strcmp(ass_field.name, val_field.name) == 0) {
					so->fields[i] = value_cast(alloc, ass_field.type, val_so->fields[j]);
					break;
				}
			}
		}

		return result;
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
		return VAL_OBJ(AS_TYPEID(ty_type), type_pool_get(value.type_id));
	}
	unreachable();
}

bool object_equal(struct haste_object *a, struct haste_object *b)
{
	if (a == b) return true;
	if (a->kind != b->kind) return false;
	switch (a->kind) {
	case HASTE_OBJ_TYPE:
		return type_equal(VAL_OBJ(AS_TYPEID(ty_type), a), VAL_OBJ(AS_TYPEID(ty_type), b));
	case HASTE_OBJ_STRING: {
		struct haste_string_object *sa = (struct haste_string_object*)a;
		struct haste_string_object *sb = (struct haste_string_object*)b;
		return sa->len == sb->len and memcmp(sa->data, sb->data, sa->len) == 0;
	}
	case HASTE_OBJ_STRUCT:
		return false;
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
		return b.kind == HASTE_VL_ZERO or value_equal(b, VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 0));
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

	return t1->pool_id == t2->pool_id;
}

    


uint64_t type_hash(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object_type *ot = AS_TYPE(t);
	uint64_t h = ot->kind;

	if (ot->name != NULL)
		for (const char *p = ot->name; *p; p += 1)
			h = h * 31 + (unsigned char)*p;

	if (ot->kind == HASTE_TY_STRUCT) {
		const struct haste_struct_type *st = (const struct haste_struct_type *)ot;
		for (size_t i = 0; i < st->field_count; i += 1) {
			for (const char *p = st->fields[i].name; *p; p += 1)
				h = h * 31 + (unsigned char)*p;
			h = h * 31 + (uint64_t)st->fields[i].type.kind;
		}
	}

	return h;
}

bool is_comptime_known(const struct haste_value v)
{
	switch (v.kind) {
	case HASTE_VL_SCALAR:
	case HASTE_VL_ZERO:
	case HASTE_VL_OBJ:
		return true;
	default:
		return false;
	}
}

bool type_can_assign(const struct haste_value assignable,
                     const struct haste_value value)
{
	ASSERT_IS_TYPE(assignable);
	ASSERT_IS_TYPE(value);
	assert(not type_is_untyped(assignable) and "the assignable shouldn't be untyped.");

	if (type_equal(assignable, value))    return true;
	if (type_equal(value, ty_zero))       return true;
	if (type_equal(assignable, ty_auto))  return not type_equal(value, ty_unknown);
	if (type_equal(assignable, ty_int))   return type_equal(value, ty_unknown) or type_is_integer(value);
	if (type_equal(assignable, ty_usize)) return type_equal(value, ty_unknown) or type_is_integer(value);
	if (type_equal(assignable, ty_float)) return type_equal(value, ty_unknown) or type_is_untyped_number(value);
	if (AS_TYPE(assignable)->kind == HASTE_TY_INT or AS_TYPE(assignable)->kind == HASTE_TY_UINT)
		return type_equal(value, ty_unknown) or type_is_integer(value);
	if (type_is_any_string(assignable))
		return type_is_any_string(value) or type_equal(value, ty_unknown);

	if (type_equal(value, ty_unknown)) return true;

	if (IS_STRUCT_TYPE(assignable) and IS_AUTO_STRUCT_TYPE(value)) {
		const struct haste_struct_type *ass_st = AS_STRUCT_TYPE(assignable);
		const struct haste_struct_type *val_st = AS_STRUCT_TYPE(value);

		if (val_st->field_count > ass_st->field_count) return false;

		for (size_t i=0; i < ass_st->field_count; i += 1) {
			if (ass_st->fields[i].has_default) continue;
			if (find_named_field(val_st, ass_st->fields[i].name) < 0) return false;
		}

		return true;
	}

	return false;
}

bool type_can_cast(const struct haste_value to,
                   const struct haste_value from)
{
	ASSERT_IS_TYPE(to);
	ASSERT_IS_TYPE(from);

	// if you can assign to it. you can cast to it
	// (Also this saves us some code)
	if (type_can_assign(to, from))    return true;

	if (type_equal(from, ty_unknown)) return true;
	if (type_equal(to, ty_auto))      return true;
	if (type_equal(to, from))         return true;
	if (type_equal(from, ty_zero))    return true;
	if (type_is_number(to))           return type_is_number(from) or type_equal(from, ty_usize);
	if (type_equal(to, ty_usize))      return type_is_number(from);
	if (AS_TYPE(to)->kind == HASTE_TY_INT or AS_TYPE(to)->kind == HASTE_TY_UINT)
		return type_is_integer(from);
	if (type_is_any_string(to))
		return type_is_any_string(from) or type_equal(from, ty_unknown);

	return false;
}

ssize_t find_named_field(const struct haste_struct_type *st, const char *name)
{
	for (size_t i = 0; i < st->field_count; i += 1)
		if (strcmp(st->fields[i].name, name) == 0)
			return i;
	return -1;
}

bool type_is_any_string(const struct haste_value t)
{
	return type_equal(t, ty_string) or type_equal(t, ty_cstr) or type_equal(t, ty_untyped_string);
}

struct haste_value untyped_to_typed(struct haste_value type)
{
	ASSERT_IS_TYPE(type);

	if (type_equal(type, ty_untyped_int))    return ty_int;
	if (type_equal(type, ty_untyped_float))  return ty_float;
	if (type_equal(type, ty_untyped_string)) return ty_string;
	if (type_equal(type, ty_zero))           return ty_int;
	if (HASTE_TID_IS_RESERVED(AS_TYPE(type)->pool_id)) return type;

	return type;
}

bool type_is_integer(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object_type *obj = AS_TYPE(t);
	return type_equal(t, ty_int)
		or type_equal(t, ty_untyped_int)
		or type_equal(t, ty_usize)
		or obj->kind == HASTE_TY_INT
		or obj->kind == HASTE_TY_UINT
		or obj->kind == HASTE_TY_UNTYPED_INT;
}

bool type_is_float(const struct haste_value t)
{
	ASSERT_IS_TYPE(t);

	const struct haste_object_type *obj = AS_TYPE(t);
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

bool haste_is_default_empty_string(const struct haste_object *obj)
{
	return obj == &_default_empty_string.base;
}

int print_object(stream_t stream, const struct haste_object *obj, const struct haste_object *type)
{
	int printed_amount = 0;
	switch (obj->kind) {
	case HASTE_OBJ_TYPE: {
		struct haste_object_type *type = OAS_TYPE(obj);
		if (type->name) {
			printed_amount += sprint(stream, "{s}", type->name);
		} else if (type->kind == HASTE_TY_INT or type->kind == HASTE_TY_UINT) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%sint%zu", type->kind == HASTE_TY_UINT ? "u" : "", type->bit_size);
			printed_amount += sprint(stream, "{s}", buf);
		} else {
			printed_amount += sprint(stream, "auto_struct");
		}
	} break;
	case HASTE_OBJ_STRING: {
		struct haste_string_object *s = (struct haste_string_object*)obj;
		printed_amount += sprint(stream, "{s:*}", s->data, (int)s->len);
	} break;
	case HASTE_OBJ_STRUCT: {
		struct haste_struct_object *so = OAS_STRUCT(obj);
		struct haste_struct_type *st = OAS_STRUCT_TYPE(type);
		printed_amount += sprint(stream, "{s} {", st->base.name then st->base.name otherwise "auto");
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
