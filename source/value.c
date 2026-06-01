#include "haste.h"
#include "my_allocator.h"
#include "my_c_allocator.h"
#include "my_common.h"
#include "my_stream.h"
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define TY_POOL_CHUNK 256
#define ty_pool_get(pool, i) ((pool).chunks.items[(i) / TY_POOL_CHUNK][(i) % TY_POOL_CHUNK])

static struct haste_type_info *ensure_reserved_type(TypeID id)
{
	if (id > HASTE_TID_TOTAL_RESERVED) return NULL;

	struct haste_type_info *slot = type_pool_get(id);

	if (slot->pool_id == id) return slot;

	bool is_signed = id < HASTE_TID_RESERVED_UINT_BASE;
	uint16_t bits = is_signed
		? (uint16_t)(id - HASTE_TID_RESERVED_INT_BASE)
		: (uint16_t)(id - HASTE_TID_RESERVED_UINT_BASE);
	size_t bytes = (bits + 7) / 8;

	struct haste_type_info type = {0};
	memset(&type, 0, sizeof(type));
	type = TYPE_INFO(
		.pool_id = id,
		.kind = is_signed ? HASTE_TY_INT : HASTE_TY_UINT,
		.is_integer = true,
		.is_unsigned = !is_signed,
		.is_untyped = false,
		.bit_size = bits,
		.size = bytes,
		.align = bytes < 8 ? bytes : 8,
	);

	memcpy(slot, &type, sizeof(struct haste_type_info));
	return slot;
}

struct type_pool g_type_pool = {0};

static void type_pool_grow(void)
{
	const size_t chunk_size = sizeof(struct haste_type_info) * TY_POOL_CHUNK;
	void *chunk = alloc(g_type_pool.allocator, sizeof(struct haste_type_info) * TY_POOL_CHUNK);
	memset(chunk, 0, chunk_size);

	arrpush(g_type_pool.allocator, g_type_pool.chunks, chunk);
}

TypeID type_pool_add(struct haste_type_info type)
{
	if (g_type_pool.len >= g_type_pool.chunks.len * TY_POOL_CHUNK) {
		type_pool_grow();
	}
	TypeID id = g_type_pool.len++;
	struct haste_type_info *slot = &ty_pool_get(g_type_pool, id);

	*slot = type;

	slot->pool_id = id;
	return id;
}

struct haste_type_info *type_pool_get(TypeID id)
{
	assert(id < g_type_pool.len);
	return &ty_pool_get(g_type_pool, id);
}

void type_pool_set_name(TypeID id, const char *name)
{
	if (HASTE_TID_IS_RESERVED(id)) return;
	assert(id < g_type_pool.len);
	ty_pool_get(g_type_pool, id).name = name;
}

struct haste_type ty_int            = {0};
struct haste_type ty_uint           = {0};
struct haste_type ty_zero           = {0};
struct haste_type ty_unknown        = {0};
struct haste_type ty_type           = {0};
struct haste_type ty_untyped_int    = {0};
struct haste_type ty_float          = {0};
struct haste_type ty_untyped_float  = {0};
struct haste_type ty_auto           = {0};
struct haste_type ty_void           = {0};
struct haste_type ty_untyped_string = {0};
struct haste_type ty_string         = {0};
struct haste_type ty_cstr           = {0};
struct haste_type ty_usize          = {0};

struct haste_value type_get_int(uint16_t bits, bool is_signed)
{
	TypeID base = is_signed ? HASTE_TID_RESERVED_INT_BASE : HASTE_TID_RESERVED_UINT_BASE;
	ensure_reserved_type(base + bits);
	return VAL_TYPE(base + bits);
}

static uint32_t _builtin_end = 0;
static uint32_t _new_type_therhold = 0;

void setup_builtins(struct Allocator allocator)
{
	g_type_pool.allocator = allocator;
	while (g_type_pool.chunks.len * TY_POOL_CHUNK <= HASTE_TID_TOTAL_RESERVED) {
		type_pool_grow();
	}

	g_type_pool.len = (uint32_t)HASTE_TID_TOTAL_RESERVED + 1;

	TypeID tid_type = type_pool_add(TYPE_INFO(.kind = HASTE_TY_TYPE, .size = 8, .align = 8, .name = "type"));
	ty_type = into_type((struct haste_value) {
		.kind = HASTE_VL_TYPE,
		.type_id = tid_type,
		.type = tid_type});

#define REGISTER_BUILTIN(val_, ...) \
	do { \
		TypeID tid = type_pool_add((struct haste_type_info) { __VA_ARGS__ });	\
		(val_) = into_type(VAL_TYPE(tid)); \
	} while (0)

	REGISTER_BUILTIN(ty_zero,           
					 .kind = HASTE_TY_ZERO,                                  
					 .name = "zero");
	REGISTER_BUILTIN(ty_unknown,        
					 .kind = HASTE_TY_UNKNOWN,                               
					 .name = "uninit");
	REGISTER_BUILTIN(ty_untyped_int,    
					 .kind = HASTE_TY_UNTYPED_INT,   
					 .size = 4, 
					 .align = 4,  
					 .name = "untyped_int",        
					 .is_integer = true, 
					 .is_untyped = true);
	REGISTER_BUILTIN(ty_float,          
					 .kind = HASTE_TY_FLOAT,         
					 .size = 4, 
					 .align = 4,  
					 .name = "float",              
					 .is_float = true);
	REGISTER_BUILTIN(ty_untyped_float,  
					 .kind = HASTE_TY_UNTYPED_FLOAT, 
					 .size = 4, 
					 .align = 4,  
					 .name = "untyped_float",      
					 .is_float = true, 
					 .is_untyped = true);
	REGISTER_BUILTIN(ty_auto,           
					 .kind = HASTE_TY_AUTO,                                  
					 .name = "auto");
	REGISTER_BUILTIN(ty_void,           
					 .kind = HASTE_TY_VOID,                                  
					 .name = "void");
	REGISTER_BUILTIN(ty_untyped_string, 
					 .kind = HASTE_TY_UNTYPED_STRING, 
					 .size = 8, 
					 .align = 8, 
					 .name = "untyped_string",     
					 .is_string = true, 
					 .is_untyped = true);
	REGISTER_BUILTIN(ty_cstr,           
					 .kind = HASTE_TY_CSTR, 
					 .size = 8, 
					 .align = 4,           
					 .name = "cstr",               
					 .is_string = true);
	REGISTER_BUILTIN(ty_usize,          
					 .kind = HASTE_TY_USIZE, 
					 .size = 8, 
					 .align = 8,          
					 .name = "usize",
					 .is_integer = true, .is_unsigned = true);

	{
		struct haste_struct_field *string_fields = alloc(
			g_type_pool.allocator,
			sizeof(struct haste_struct_field) * 2);
		string_fields[0] = (struct haste_struct_field){
			.name = "ptr",
			.type = ty_cstr,
		};
		string_fields[1] = (struct haste_struct_field){
			.name = "len",
			.type = ty_usize,
		};
		const auto string_type_info = TYPE_INFO(
			.kind = HASTE_TY_STRUCT,
			.structure = {
				.len = 2,
				.items = string_fields,
			});
		const TypeID string_id = type_pool_add(string_type_info);
		ty_string = into_type(VAL_TYPE(string_id));
		AS_TYPE_INFO(ty_string)->name = "string";
		AS_TYPE_INFO(ty_string)->is_string = true;
	}

	{
		ty_int = into_type(VAL_TYPE(ensure_reserved_type(32)->pool_id));
		AS_TYPE_INFO(ty_int)->name = "int";
	}

	{
		ty_uint = into_type(VAL_TYPE(ensure_reserved_type(HASTE_TID_RESERVED_UINT_BASE + 32)->pool_id));
		AS_TYPE_INFO(ty_uint)->name = "uint";
	}

	_builtin_end = g_type_pool.len - 1;
	_new_type_therhold = _builtin_end;
}

bool type_is_builtin(struct haste_type ty)
{
	return AS_TYPEID(ty) <= _builtin_end;
}

bool is_newly_created_type(struct haste_type ty)
{
	return AS_TYPEID(ty) >= _new_type_therhold;
}

void reset_new_type_counter(void)
{
	_new_type_therhold = g_type_pool.len - 1;
}

enum arith_op {
	ARITH_ADD,
	ARITH_SUB,
	ARITH_MUL,
	ARITH_DIV,
};

#define value_is_any_int(v) \
	(IS_SCALAR(v) and type_is_integer(typeof_value(v)))

#define value_is_any_float(v) \
	(IS_SCALAR(v) and type_is_float(typeof_value(v)))

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
		if (b == 0.0) return VAL_BAD_ERROR(ERR_DIVISION_BY_ZERO);
		res = a / b;
		break;
	}

	/* if (typeof_value(lhs).value.kind == HASTE_TY_FLOAT or typeof_value(rhs).value.kind == HASTE_TY_UNTYPED_FLOAT) */
	if (type_is_float(typeof_value(lhs)) or type_is_untyped_float(typeof_value(rhs)))
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
		if ((b > 0 && a > INT64_MAX - b) or (b < 0 && a < INT64_MIN - b)) {
			return VAL_BAD_ERROR(ERR_ARITH_OVERFLOW);
		}
		res = a + b;
		break;
	case ARITH_SUB:
		if ((b > 0 && a < INT64_MIN + b) or (b < 0 && a > INT64_MAX + b)) {
			return VAL_BAD_ERROR(ERR_ARITH_OVERFLOW);
		}
		res = a - b;
		break;
	case ARITH_MUL:
		if (a != 0 and b != 0) {
			if ((a > 0 and b > 0 and a > INT64_MAX / b)
			    or (a > 0 and b < 0 and b < INT64_MIN / a)
			    or (a < 0 and b > 0 and a < INT64_MIN / b)
			    or (a < 0 and b < 0 and a < INT64_MAX / b)) {
				return VAL_BAD_ERROR(ERR_ARITH_OVERFLOW);
			}
		}
		res = a * b;
		break;
	case ARITH_DIV:
		if (b == 0) {
			return VAL_BAD_ERROR(ERR_DIVISION_BY_ZERO);
		}
		if (a == INT64_MIN and b == -1) {
			return VAL_BAD_ERROR(ERR_ARITH_OVERFLOW);
		}
		res = a / b;
		break;
	}

	if (type_equal(typeof_value(lhs), typeof_value(rhs))) {
		return VAL_SCALAR(lhs.type_id, .integer = res);
	}

	return VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = res);
}

static struct haste_value value_do_arith(
	const enum arith_op op,
	struct haste_value lhs,
	struct haste_value rhs)
{
	if (IS_ZERO(lhs)) lhs = VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 0);
	if (IS_ZERO(rhs)) rhs = VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 0);

	if (not (value_is_any_int(lhs) or value_is_any_float(lhs))) {
		return VAL_BAD_ERROR(ERR_INCOMPATIBLE_ARITH_TYPES);
	}
	if (not (value_is_any_int(rhs) or value_is_any_float(rhs))) {
		return VAL_BAD_ERROR(ERR_INCOMPATIBLE_ARITH_TYPES);
	}
	if (not type_equal(typeof_value(lhs), typeof_value(rhs))
	    and not type_is_untyped(typeof_value(lhs))
	    and not type_is_untyped(typeof_value(rhs))) {
		return VAL_BAD_ERROR(ERR_INCOMPATIBLE_ARITH_TYPES);
	}

	if (value_is_any_float(lhs) or value_is_any_float(rhs))
		return arith_float(op, lhs, rhs);

	return arith_int(op, lhs, rhs);
}

#define DEFINE_ARITH(name, op) \
	struct haste_value name(const struct haste_value lhs, const struct haste_value rhs) \
	{ return value_do_arith(op, lhs, rhs); }

DEFINE_ARITH(value_add, ARITH_ADD)
DEFINE_ARITH(value_sub, ARITH_SUB)
DEFINE_ARITH(value_mul, ARITH_MUL)
DEFINE_ARITH(value_div, ARITH_DIV)

struct haste_value value_implicit_cast(struct Allocator alloc, const struct haste_type to, const struct haste_value value)
{
	struct haste_type from = typeof_value(value);

	if (type_equal(to, from)) {
		return value;
	}

	if (type_is_untyped_integer(from) and type_is_number(to)) {
		if (type_is_float(to)) {
			return VAL_SCALAR(AS_TYPEID(to), .floating = (double)value.integer);
		}
		return VAL_SCALAR(AS_TYPEID(to), .integer = value.integer);
	}

	if (type_is_untyped_float(from) and type_is_float(to)) {
		return VAL_SCALAR(AS_TYPEID(to), .floating = value.floating);
	}

	if (type_equal(from, ty_untyped_string)) {
		if (type_equal(to, ty_string)) {
			struct haste_string_object *s = (struct haste_string_object*)value.obj;
			struct haste_value so = make_value(alloc, to);
			struct_set_field(alloc, &so, (size_t)0,
				VAL_OBJ(AS_TYPEID(ty_cstr), value.obj));
			struct_set_field(alloc, &so, (size_t)1,
				VAL_SCALAR(AS_TYPEID(ty_usize), .integer = (int64_t)s->len));
			return so;
		}
		if (type_equal(to, ty_cstr)) {
			return VAL_OBJ(AS_TYPEID(to), value.obj);
		}
	}

	return VAL_BAD_ERROR(ERR_INVALID_IMPLICIT_CAST);
}

typedef struct {
	int64_t as_int;
	double as_float;
	bool is_float;
} RawNumber;

static RawNumber extract_raw(struct haste_value value)
{
	if (type_is_integer(typeof_value(value)))   return (RawNumber){ .as_int = value.integer,           .as_float = (double)value.integer, };
	if (type_is_float(typeof_value(value))) return (RawNumber){ .as_int = (int64_t)value.floating, .as_float = value.floating, };
	unreachable();
}

static struct haste_value construct_from_raw(struct haste_type to, RawNumber raw)
{
	if (type_is_integer(to))
		return VAL_SCALAR(AS_TYPEID(to), .integer = raw.as_int);

	if (type_is_float(to))
		return VAL_SCALAR(AS_TYPEID(to), .floating = raw.as_float);

	unreachable();
}

struct haste_value value_cast(
	struct Allocator alloc,
	const struct haste_type to,
	const struct haste_value value)
{
	const struct haste_type value_type = typeof_value(value);

	if (IS_BAD(value))                     return VAL_BAD;
	if (IS_BAD(into_value(to)))            return VAL_BAD;

	if (type_is_untyped(to))               unreachable();
	if (IS_RUNTIME(value))                 unimplemented();

	if (type_equal(to, ty_auto))           return value;
	if (type_equal(to, value_type))        return value;

	{
		struct haste_value implicit = value_implicit_cast(alloc, to, value);
		if (not IS_BAD(implicit)) return implicit;
	}

	if (value_equal(value, VAL_UNINIT))    return default_for_type(alloc, to);
	if (IS_ZERO(value))                    return zero_for_type(alloc, to);

	if (type_equal(to, ty_string) and IS_OBJ(value) and value.obj->kind == HASTE_OBJ_STRING) {
		struct haste_string_object *s = (struct haste_string_object*)value.obj;
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(to);
		struct haste_value so = make_value(alloc, to);
		struct_set_field(alloc, &so, (size_t)0, value_cast(alloc, st->items[0].type, value));
		struct_set_field(alloc, &so, (size_t)1, VAL_SCALAR(AS_TYPEID(ty_usize), .integer = (int64_t)s->len));
		return so;
	}

	if (type_equal(to, ty_cstr) and IS_STRUCT(value)) {
		struct haste_struct_object *so = AS_STRUCT(value);
		ssize_t idx = find_named_field(typeof_value(value), "ptr");
		assert(idx >= 0);
		return so->fields[idx];
	}

	if (type_is_any_string(to) and IS_OBJ(value) and value.obj->kind == HASTE_OBJ_STRING) {
		return VAL_OBJ(AS_TYPEID(to), value.obj);
	}

	if (IS_STRUCT_TYPE(to) and IS_AUTO_STRUCT_TYPE(value_type)) {
		const struct haste_struct_type_info *to_st = AS_STRUCT_TYPE_INFO(to);
		const struct haste_struct_type_info *val_st = AS_STRUCT_TYPE_INFO(value_type);

		const struct haste_struct_object *val_so = AS_STRUCT(value);

		struct haste_value result = default_for_type(alloc, to);
		struct haste_struct_object *so = AS_STRUCT(result);

		for (size_t i=0; i < to_st->len; i += 1) {
			struct haste_struct_field ass_field = to_st->items[i];

			for (size_t j=0; j < val_st->len; j += 1) {
				struct haste_struct_field val_field = val_st->items[j];
				if (strcmp(ass_field.name, val_field.name) == 0) {
					so->fields[i] = value_cast(alloc, ass_field.type, val_so->fields[j]);
					break;
				}
			}
		}

		return result;
	}

	if (not type_is_number(to) or not type_is_number(value_type)) {
		return VAL_BAD_ERROR(ERR_INVALID_CAST);
	}
	return construct_from_raw(to, extract_raw(value));
}

struct haste_value value_assign(struct Allocator alloc, struct haste_value *lvalue, struct haste_value rvalue)
{
	if (not lvalue->is_lvalue)
		return VAL_BAD_ERROR(ERR_INVALID_ASSIGNMET);

	struct haste_type lhs_type = typeof_value(*lvalue);

	struct haste_value result = value_implicit_cast(alloc, lhs_type, rvalue);
	if (IS_BAD(result)) {
		if (IS_ZERO(rvalue)) {
			result = zero_for_type(alloc, lhs_type);
		} else if (IS_UNINIT(rvalue)) {
			result = default_for_type(alloc, lhs_type);
		} else if (IS_STRUCT_TYPE(lhs_type) and IS_AUTO_STRUCT_TYPE(typeof_value(rvalue))) {
			result = value_cast(alloc, lhs_type, rvalue);
		}
	}
	if (IS_BAD(result)) return VAL_BAD_ERROR(ERR_INVALID_ASSIGNMET);

	if (IS_STRUCT(*lvalue) and IS_STRUCT(result)) {
		struct haste_struct_object *lso = AS_STRUCT(*lvalue);
		struct haste_struct_object *rso = AS_STRUCT(result);
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(lhs_type);
		iarreach (i, *st) {
			lso->fields[i] = rso->fields[i];
		}
	} else {
		*lvalue = result;
	}

	result.is_lvalue = false;
	return result;
}

struct haste_value value_coerce(struct Allocator alloc, const struct haste_type to, const struct haste_value value)
{
	struct haste_value slot = make_value(alloc, to);
	slot.is_lvalue = true;
	return value_assign(alloc, &slot, value);
}

struct haste_object *create_struct(struct Allocator alloc, struct haste_struct_type_info *st)
{
	assert(st != NULL);

	struct haste_struct_object *so = alloc(
		alloc,
		sizeof(struct haste_struct_object) +
		sizeof(struct haste_value) *
		SAFE_COUNT(st->len));
	so->base.kind = HASTE_OBJ_STRUCT;
	memset(so->fields, 0, sizeof(struct haste_value) * st->len);

	iarreach (i, *st) {
		struct haste_struct_field field = st->items[i];
		if (field.has_default) {
			so->fields[i] = field.default_value;
			if (not type_equal(typeof_value(field.default_value), field.type)) {
				so->fields[i] = value_cast(alloc, field.type, field.default_value);
			}
		} else {
			so->fields[i] = VAL_NONE;
		}
	}

	return (void*)so;
}

struct haste_object *create_string(struct Allocator alloc, const char *str, size_t len)
{
	struct haste_string_object *so = alloc(
		alloc,
		sizeof(struct haste_string_object) +
		sizeof(char) * len);
	so->base.kind = HASTE_OBJ_STRING;
	so->len = len;
	memcpy(so->data, str, so->len);
	return (void*)so;
}

bool object_equal(struct haste_object *a, struct haste_object *b)
{
	if (a == b) return true;
	if (a->kind != b->kind) return false;
	switch (a->kind) {
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
	case HASTE_VL_TYPE:
		if (not IS_TYPE(b)) {
			return false;
		}
		return type_equal(into_type(a), into_type(b));
	case HASTE_VL_OBJ:
		if (not IS_OBJ(b)) return false;
		return object_equal(a.obj, b.obj);
	}
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

bool struct_has_field_name(const struct haste_value value, const char *name)
{
	return find_named_field(typeof_value(value), name) > 0;
}

struct haste_value struct_get_field_by_name(const struct haste_value value,
											const char *name)
{
	if (not IS_STRUCT(value)) {
		return VAL_BAD_ERROR(ERR_NOT_A_STRUCT);
	}

	struct haste_type type = typeof_value(value);
	const ssize_t idx = find_named_field(type, name);
	if (idx < 0) {
		return VAL_BAD_ERROR(ERR_FIELD_DOESNT_EXIST);
	}

	return struct_get_field_by_index(value, idx);
}

struct haste_value struct_get_field_by_index(const struct haste_value value,
											 const size_t idx)
{
	if (not IS_STRUCT(value)) {
		return VAL_BAD_ERROR(ERR_NOT_A_STRUCT);
	}

	struct haste_type type = typeof_value(value);
	struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);
	if (idx >= st->len) {
		return VAL_BAD_ERROR(ERR_FIELD_DOESNT_EXIST);
	}

	struct haste_struct_object *so = AS_STRUCT(value);

	return so->fields[idx];
}

// TODO: Should I do type checking here?
struct haste_value struct_set_field_by_name(struct Allocator allocator,
											struct haste_value *value,
											const char *name,
											const struct haste_value new_value)
{
	if (not IS_STRUCT(*value)) {
		return VAL_BAD_ERROR(ERR_NOT_A_STRUCT);
	}

	struct haste_type type = typeof_value(*value);
	const ssize_t idx = find_named_field(type, name);
	if (idx < 0) {
		return VAL_BAD_ERROR(ERR_FIELD_DOESNT_EXIST);
	}

	return struct_set_field_by_index(allocator, value, idx, new_value);
}

struct haste_value struct_set_field_by_index(struct Allocator allocator,
											 struct haste_value *value,
											 const size_t idx,
											 const struct haste_value new_value)
{
	if (not IS_STRUCT(*value)) {
		return VAL_BAD_ERROR(ERR_NOT_A_STRUCT);
	}

	struct haste_type type = typeof_value(*value);
	struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);
	if (idx >= st->len) {
		return VAL_BAD_ERROR(ERR_FIELD_DOESNT_EXIST);
	}

	struct haste_struct_object *so = AS_STRUCT(*value);
	struct haste_value casted = value_cast(allocator, st->items[idx].type, new_value);
	if (IS_BAD(casted)) {
		return VAL_BAD_ERROR(ERR_INVALID_ASSIGNMET);
	}
	so->fields[idx] = casted;
	return *value;
}

int print_object(stream_t stream, const struct haste_object *obj, struct haste_type type)
{
	int printed_amount = 0;
	switch (obj->kind) {
	case HASTE_OBJ_STRING: {
		struct haste_string_object *s = (struct haste_string_object*)obj;
		printed_amount += sprint(stream, "{s:*}", s->data, (int)s->len);
	} break;
	case HASTE_OBJ_STRUCT: {
		struct haste_struct_object *so = OAS_STRUCT(obj);
		struct haste_type_info *type_info = AS_TYPE_INFO(type);
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);
		printed_amount += sprint(stream, "{s} {", type_info->name then type_info->name otherwise "auto");
		for (size_t i=0; i<st->len; i += 1) {
			struct haste_struct_field field = st->items[i];
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
	case HASTE_VL_TYPE: {
		struct haste_type_info *type = AS_TYPE_INFO(into_type(value));
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
	case HASTE_VL_OBJ:
		printed_amount += sprint(stream, "{obj}", value);
		break;
	}

	return printed_amount;
}
