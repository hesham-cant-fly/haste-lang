#include "haste.h"
#include "my_allocator.h"
#include "my_common.h"
#include "my_stream.h"
#include "my_termcolor.h"

#define IS_NODE_ANALYZED(...) (__VA_ARGS__)->type.value.kind != 0

struct scope {
	struct scope *next;
	size_t len;
	struct symbol {
		const char *key;
		bool is_constant : 1;
		bool is_explicitly_comptime : 1;
		enum symbol_kind {
			SYM_VAR,
		} kind;
		struct haste_type type;
		struct haste_value value;
		struct haste_ast_node *node;
	} *items;
};

struct symbol_set {
	size_t cap, len;
	struct symbol *items;
};

struct analyzer {
	struct Allocator allocator;
	struct Allocator arena_allocator;
	struct scope *global, *local;
	const source_file_id src;
	bool had_error;
};

/* 🗣️: Stop using macros they are bad
 * me: Macro abuse goes brrrrrrrrrrrrrrrrrrrrrrr */
#define try(name_, ...) \
	for (int _ok_ = 1, _run_ = 1; _ok_; _ok_ = 0) \
	for (struct haste_value name_ = (__VA_ARGS__); _run_; _run_ = 0) \
	if (IS_BAD(name_)) return name_; \
	else

#define catch(ok_name_, err_name_, ...) \
	struct haste_value ok_name_ = (__VA_ARGS__); \
	for (int _ok_ = 1; _ok_; _ok_ = 0) \
	if (IS_BAD(ok_name_)) \
		for (enum haste_value_error err_name_ = (ok_name_).error_code; _ok_; _ok_ = 0)

#define propagate(...) \
	do { \
		struct haste_value v_ = (__VA_ARGS__); \
		if (IS_BAD(v_)) { \
			return v_; \
		} \
	} while (0)

#define bail(self_, node_, ...) \
	(report_error(self_, node_, __VA_ARGS__), VAL_BAD)

#define inject(alloc_, node_, result_) \
	(node_) = node_into_value((alloc_), (node_), (result_))

#define with_scope(self_) \
	for (struct scope *_scope_ = begin_scope(self_); _scope_; end_scope(self_), _scope_ = NULL)

#define IS_AUTO(type) \
	type_equal(type, ty_auto)

#define fail_var_decl(self_, scope_, name_, is_constant_, node_) \
	do { \
		reset_new_type_counter(); \
		emit_error_symbol(self_, scope_, name_, is_constant_, node_); \
	} while (0)

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type);

// ── Error helpers ────────────────────────────────────────────────

static void _vreport(struct analyzer *self, struct token token, const char *kind, bool set_error, const char *restrict fmt, va_list args)
{
	if (self->src != -1) f_vreport_at_token(self->src, kind, token, fmt, args);
	else {
		eprintln(fmt, args);
	}
	if (set_error) self->had_error = true;
}

#define DEFINE_REPORT_FN(name, color, label, set_error) \
	static void _report_##name##_node(struct analyzer *self, struct haste_ast_node *node, const char *restrict fmt, ...) \
	{ va_list args; va_start(args, fmt); _vreport(self, node->start, color label, set_error, fmt, args); va_end(args); } \
	static void _report_##name##_token(struct analyzer *self, struct token token, const char *restrict fmt, ...) \
	{ va_list args; va_start(args, fmt); _vreport(self, token, color label, set_error, fmt, args); va_end(args); }

DEFINE_REPORT_FN(error,   ANSI_CODE_RED,    "Error",   true)
DEFINE_REPORT_FN(note,    ANSI_CODE_GREEN,  "Note",   false)
DEFINE_REPORT_FN(warning, ANSI_CODE_YELLOW, "Warning", false)

#define report_error(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_error_node, \
		struct token: _report_error_token)(self_, node_, __VA_ARGS__)
#define report_note(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_note_node, \
		struct token: _report_note_token)(self_, node_, __VA_ARGS__)
#define report_warning(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_warning_node, \
		struct token: _report_warning_token)(self_, node_, __VA_ARGS__)

// ── Scope management ─────────────────────────────────────────────

static struct scope *begin_scope(struct analyzer *self)
{
	struct scope *s = create(self->allocator, struct scope, .next = self->local);
	self->local = s;
	if (self->global == NULL) self->global = s;
	return s;
}

static void end_scope(struct analyzer *self)
{
	if (self->global == NULL) return;
	struct scope *scope = self->local;
	self->local = scope->next;
	hmfree(self->allocator, *scope);
	destroy(self->allocator, scope);
}

// ── Symbol table ─────────────────────────────────────────────────

#define put_local_symbol(self_, name_, ...) put_symbol(self_, (self_)->local, name_, __VA_ARGS__)
#define put_global_symbol(self_, name_, ...) put_symbol(self_, (self_)->global, name_, __VA_ARGS__)
#define put_symbol(self_, scope_, name_, ...) _put_symbol((self_), (scope_), (name_), (struct symbol) { __VA_ARGS__ })
static bool _put_symbol(struct analyzer *self, struct scope *scope, const char *name, struct symbol symbol)
{
	symbol.key = name;
	if (hmget(*scope, name)) return true;
	hmput(self->allocator, *scope, symbol);
	return false;
}

#define emit_error_symbol(self_, scope_, name_, is_constant_, node_) \
	do { \
		put_symbol(self_, scope_, name_, \
			.value = VAL_BAD, \
			.type = into_type(VAL_BAD), \
			.is_constant = is_constant_, \
			.node = node_); \
		return VAL_BAD; \
	} while (0)

static struct symbol _recursion_sentinel = { .value = VAL_BAD };

static struct symbol *find_local_first(struct analyzer *self, const char *name)
{
	leach (struct scope, scope, self->local) {
		struct symbol *s = hmget(*scope, name);
		if (s != NULL) return s;
	}

	struct haste_declaration *decl = hmget(sources.items[self->src].declarations, name);
	if (decl == NULL) return NULL;

	if (decl->analyzing) {
		report_error(self, decl->node, "Recursive declrations is not allowed");
		return &_recursion_sentinel;
	}

	decl->analyzing = true;
	analyze_node(self, decl->node, (struct haste_type){0});
	decl->analyzing = false;

	return find_local_first(self, name);
}

static void symbol_set_add(struct Allocator alloc, struct symbol_set *set, struct symbol s)
{
	arrpush(alloc, *set, s);
}

static struct symbol_set find_all_symbols(struct analyzer *self, const char *name)
{
	struct symbol_set result = {0};
	leach (struct scope, scope, self->local) {
		struct symbol *s = hmget(*scope, name);
		if (s != NULL) symbol_set_add(self->allocator, &result, *s);
	}

	struct haste_declaration *decl = hmget(sources.items[self->src].declarations, name);
	if (decl == NULL) return result;

	if (not decl->analyzing) {
		decl->analyzing = true;
		analyze_node(self, decl->node, (struct haste_type){0});
		decl->analyzing = false;

		struct symbol *s = hmget(*self->global, name);
		if (s != NULL) symbol_set_add(self->allocator, &result, *s);
	}

	return result;
}

// ── Token → value ──────────────────────────────────────────────

static struct haste_value token_to_value(struct analyzer *self, struct token token)
{
	switch (token.kind) {
	case TK_IDENT:    unimplemented();
	case TK_STR: {
		size_t len = strlen(token.str);
		struct haste_object *obj = create_string(self->arena_allocator, (char *)token.str, len);
		return VAL_OBJ(AS_TYPEID(ty_untyped_string), obj);
	}
	case TK_INT:
		if (token.ival == 0) return VAL_ZERO;
		return VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = token.ival);
	case TK_FLOAT:
		return VAL_SCALAR(AS_TYPEID(ty_untyped_float), .floating = token.fval);
	case TK_KW_INT_BITS:
	case TK_KW_UINT_BITS: {
		const bool is_signed = token.kind == TK_KW_INT_BITS;
		if (token.ival == 0)
			return bail(self, token, "Bit width must be greater than 0");
		if (token.ival > STANDARD_BITWIDTH_LIMIT) {
			run_at_percent (20.0f) {
				report_error(self, token, "Amigo! u mama is too big.");
			} else {
				report_error(self, token, "Bit width bigger than {d} is not supported", STANDARD_BITWIDTH_LIMIT);
			}
			return VAL_BAD;
		}
		return type_get_int(token.ival, is_signed);
	}
	case TK_KW_STRING: return into_value(ty_string);
	case TK_KW_CSTR:   return into_value(ty_cstr);
	case TK_KW_UINT:   return into_value(ty_uint);
	case TK_KW_INT:    return into_value(ty_int);
	case TK_KW_FLOAT:  return into_value(ty_float);
	case TK_KW_USIZE:  return into_value(ty_usize);
	case TK_KW_VOID:   return into_value(ty_void);
	case TK_KW_TYPE:   return into_value(ty_type);
	case TK_KW_AUTO:   return into_value(ty_auto);
	default:           unreachable();
	}
}

// ── Struct helpers ─────────────────────────────────────────────

static bool field_name_matches(struct haste_struct_field field, struct token name_token)
{
	return strncmp(name_token.start, field.name, name_token.len) == 0
		and field.name[name_token.len] == '\0';
}

static void inject_struct_type(struct analyzer *self, struct haste_ast_node *lit, struct haste_type field_type)
{
	if (lit->kind != ND_STRUCT_LITERAL) return;
	if (lit->struct_literal.type_expr != NULL
		and (lit->struct_literal.type_expr->kind != ND_PRIMARY
			or lit->struct_literal.type_expr->token.kind != TK_KW_AUTO))
		return;
	struct haste_ast_node *ty_node = create(self->arena_allocator,
											struct haste_ast_node, .start = lit->start);
	inject(self->arena_allocator, ty_node, into_value(field_type));
	lit->struct_literal.type_expr = ty_node;
}

// TODO: gotta refactor this one
static ssize_t find_struct_field(struct haste_struct_type_info *st, struct token name)
{
	iarreach (i, *st) {
		if (field_name_matches(st->items[i], name)) {
			return i;
		}
	}
	return -1;
}

// Returns true on error. Fills `*out` with the field info on success.
static bool read_struct_field(struct analyzer *self,
							  const char *name,
							  struct haste_ast_node *field,
							  struct haste_struct_field *out)
{
	struct haste_type field_type = ty_auto;
	if (field->struct_field.type != NULL) {
		struct haste_value tp = analyze_node(self, field->struct_field.type, (struct haste_type){0});
		if (IS_BAD(tp)) return true;
		if (not IS_TYPE(tp)) {
			report_error(self, field->struct_field.type,
						 "Expected a type for field, got '{value}' instead.", typeof_value(tp));
			out->type = into_type(VAL_BAD);
			return true;
		}
		field_type = into_type(tp);
	}

	struct haste_value default_value = {0};
	bool has_default = false;
	if (field->struct_field.default_value != NULL) {
		default_value = analyze_node(self, field->struct_field.default_value, field_type);
		if (IS_BAD(default_value)) return true;

		if (not IS_AUTO(field_type)) {
			struct haste_type default_value_type = typeof_value(default_value);
			if (not type_equal(field_type, default_value_type)) {
				struct haste_value slot = make_value(self->allocator, field_type);
				slot.is_lvalue = true;
				catch(result, err, value_assign(self->allocator, &slot, default_value)) {
					discard err;
					report_error(self, field->struct_field.default_value,
								 "Cannot set the default value of type '{value}' to '{value}'",
								 default_value_type, field_type);
					default_value = VAL_BAD;
					return true;
				}
				default_value = result;
			}
		}

		if (IS_AUTO(field_type)) {
			field_type = typeof_value(default_value);
			field_type = untyped_to_typed(field_type);
		}
		has_default = true;
	}

	if (IS_AUTO(field_type) and not has_default) {
		report_error(self, field,
					 "Cannot infer type for field '{s}' without a default value.", name);
		return true;
	}

	*out = (struct haste_struct_field){
		.name = name,
		.type = field_type,
		.default_value = default_value,
		.has_default = has_default,
	};
	return false;
}

// ── Per-node-kind analysis ───────────────────────────────────────

static struct haste_value resolve_binary_op(struct analyzer *self, struct haste_value lhs, struct haste_value rhs, struct token op);

static struct haste_value analyze_binary(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	try (lhs, analyze_node(self, node->lhs, expected_type))
	try (rhs, analyze_node(self, node->rhs, expected_type))
	try (result, resolve_binary_op(self, lhs, rhs, node->op))
	{
		inject(self->arena_allocator, node, result);
		return result;
	}

	return VAL_NONE;
}

#define BINARY_OP_CASE(kind, op_func, op_name) \
	case kind: { \
		catch(value, err, op_func(lhs, rhs)) {  \
			discard err; \
			switch (err) { \
			case ERR_INCOMPATIBLE_ARITH_TYPES: \
				report_error(self, op, \
					op_name " is not possible between a value of type '{value}' and a value of type '{value}'", \
					typeof_value(lhs), typeof_value(rhs)); \
				break; \
			case ERR_ARITH_OVERFLOW: \
				report_error(self, op, \
					op_name " is not possible because of arithmatic overflow."); \
				break; \
			case ERR_DIVISION_BY_ZERO: \
				report_error(self, op, \
					op_name " by zero is not possible."); \
				break; \
			default: unreachable(); \
			} \
			return VAL_BAD; \
		} \
		return value; \
	} break

static struct haste_value resolve_binary_op(struct analyzer *self, struct haste_value lhs, struct haste_value rhs, struct token op)
{
	switch (op.kind) {
	case TK_PLUS: {
		catch (value, err, value_add(lhs, rhs)) {
			run_at_percent (1.5) {
				report_error(self, op, "Addition is not impossible. (try harder)");
				return VAL_BAD;
			}

			switch (err) {
			case ERR_INCOMPATIBLE_ARITH_TYPES:
				report_error(self, op,
					"Addition is not possible between a value of type '{value}' and a value of type '{value}'",
					typeof_value(lhs), typeof_value(rhs));
				break;
			case ERR_ARITH_OVERFLOW:
				report_error(self, op,
					"Addition is not possible because of arithmatic overflow.");
				break;
			default: unreachable();
			}
			return VAL_BAD;
		}
		return value;
	} break;
	BINARY_OP_CASE(TK_MINUS,  value_sub, "Subtraction");
	BINARY_OP_CASE(TK_STAR,   value_mul, "Multiplication");
	BINARY_OP_CASE(TK_FSLASH, value_div, "Division");
	default: unreachable();
	}

	return VAL_NONE;
}

static struct haste_value analyze_unary(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	// TODO: This needs some refactoring
	try (value, analyze_node(self, node->rhs, expected_type)) {
		switch (node->op.kind) {
		case TK_MINUS:
			if (IS_ZERO(value)) {
				value = VAL_SCALAR(AS_TYPEID(typeof_value(value)), .integer = 0);
			} else if (IS_SCALAR(value)) {
				if (type_is_integer(typeof_value(value))) {
					value.integer = -value.integer;
				} else if (type_is_float(typeof_value(value))) {
					value.floating = -value.floating;
				} else {
					goto neg_error;
				}
			} else if (IS_BAD(value)) {
				return VAL_BAD;
			} else {
				goto neg_error;
			}
			break;
		case TK_PLUS: break;
		default: unimplemented();
		}

		inject(self->arena_allocator, node, value);
		return value;

	neg_error:
		return bail(self, node->op,
					 "Negation is not possible on '{value}'.", typeof_value(value));
	}

	return VAL_NONE;
}

static struct haste_value analyze_access(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	try (lhs_value, analyze_node(self, node->access.lhs, expected_type)) {
		catch (result, err, struct_get_field(lhs_value, node->access.rhs)) {
			discard err;
			return bail(self, node->access.rhs,
						 "Cannot access field '{token}'. no such field inside '{value}'",
						 node->access.rhs, typeof_value(lhs_value));
		}

		node->type = typeof_value(result);
		inject(self->allocator, node, result);
		return result;
	}

	return VAL_NONE;
}

static struct haste_value analyze_primary(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	discard expected_type;
	struct haste_value value = {0};
	if (node->token.kind == TK_IDENT) {
		const char *name = intern_token(node->token);
		struct symbol *symbol = find_local_first(self, name);
		if (symbol == NULL) {
			report_error(self, node, "undefined symbol '{s}'.", name);
			return VAL_UNINIT;
		}
		value = symbol->value;
		if (not symbol->is_constant) {
			value.is_lvalue = true;
		}
	} else {
		value = token_to_value(self, node->token);
	}

	node->type = typeof_value(value);
	node->kind = ND_VALUE;
	node->value = value;
	return value;
}

static struct haste_value analyze_grouping(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	struct haste_value value = analyze_node(self, node->body, expected_type);
	node->type = typeof_value(value);
	return value;
}

static struct haste_value analyze_distinct(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	try (type, analyze_node(self, node->body, expected_type)) {
		if (not IS_TYPE(type))
			return bail(self, node->body, "Expected a '{value}', got '{value}' instead.", ty_type, typeof_value(type));

		struct haste_type tp = {0};
		tp = into_type(type);

		struct haste_value result = VAL_TYPE(type_pool_add(*AS_TYPE_INFO(tp)));
		inject(self->arena_allocator, node, result);
		return result;
	}

	return VAL_NONE;
}

static struct haste_value analyze_cast(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	struct haste_type to = ty_auto;
	if (node->cast.to == NULL) {
		to = expected_type;
	} else {
		try (value, analyze_node(self, node->cast.to, (struct haste_type){0}))
		{
			if (not IS_TYPE(value))
				return bail(self, node->cast.to then node->cast.to otherwise node,
							 "expected a {value} got '{value}' instead.", ty_type, typeof_value(value));
			to = into_type(value);
		}
	}

	try (value, analyze_node(self, node->cast.expr, (struct haste_type){0}))
	{
		catch(result, err, value_cast(self->allocator, to, value))
		{
			run_at_percent (1) {
				report_error(self, node,
				             "This cat cannot purr like a {value}. "
				             "(if you don't get the easter egg its because I sometimes mispell 'cast' to 'cat')", to);
				return VAL_BAD;
			}
			switch (err) {
			case ERR_INVALID_CAST:
				report_error(
					self, node,
					"Cannot cast '{value}' to '{value}'",
					typeof_value(value), to);
				break;
			default: unreachable();
			}

			return VAL_BAD;
		}
		node->type = typeof_value(result);
		return result;
	}

	return VAL_NONE;
}

static struct haste_value analyze_var_decl(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	discard expected_type;
	const char *name = intern_token(node->variable.name);
	const bool is_constant = node->variable.is_constant;
	struct scope *target_scope = node->variable.is_global then self->global otherwise self->local;

	if (node->variable.type == NULL and node->variable.value == NULL) {
		report_error(self, node, "You need to either specify the type or the value or both.");
		fail_var_decl(self, target_scope, name, is_constant, node);
	}

	struct haste_type type = ty_auto;
	if (node->variable.type != NULL) {
		struct haste_value tp = analyze_node(self, node->variable.type, (struct haste_type){0});
		if (IS_BAD(tp))
			fail_var_decl(self, target_scope, name, is_constant, node);
		if (not IS_TYPE(tp)) {
			report_error(self, node->variable.type,
						 "Expected a {value} got '{value}' instead.", ty_type, typeof_value(tp));
			fail_var_decl(self, target_scope, name, is_constant, node);
		}
		type = into_type(tp);
	}

	struct haste_value value = VAL_UNINIT;
	if (node->variable.value != NULL) {
		struct haste_declaration *struct_decl = node->variable.value->kind == ND_STRUCT_TYPE
			then hmget(sources.items[self->src].declarations, name)
			otherwise NULL;

		if (struct_decl != NULL) struct_decl->analyzing = true;

		if (not IS_AUTO(type)) {
			inject_struct_type(self, node->variable.value, type);
		}

		value = analyze_node(self, node->variable.value, type);

		if (struct_decl != NULL) struct_decl->analyzing = false;

		if (IS_BAD(value))
			fail_var_decl(self, target_scope, name, is_constant, node);
	}

	if (IS_AUTO(type)) {
		type = typeof_value(value);
	} else if (IS_UNINIT(value))
		value = default_for_type(self->allocator, type);

	if (not type_equal(type, typeof_value(value))) {
		struct haste_type orig_type = typeof_value(value);
		struct haste_value slot = make_value(self->allocator, type);
		slot.is_lvalue = true;
		catch(result, err, value_assign(self->allocator, &slot, value)) {
			discard err;
			report_error(self, node->variable.name,
				"cannot assign a value of type '{value}' to '{value}'.", orig_type, type);
			fail_var_decl(self, target_scope, name, is_constant, node);
		}
		value = result;
	}

	if (IS_TYPE(value)) {
		if (is_newly_created_type(into_type(value))) {
			type_pool_set_name(AS_TYPEID(into_type(value)), name);
		}
	}

	const bool is_explicitly_comptime = node->variable.is_explicitly_comptime
		or (is_constant and type_equal(type, ty_type));

	if (_put_symbol(self, target_scope, name, (struct symbol){
		.type = type, .value = value,
		.is_constant = is_constant,
		.is_explicitly_comptime = is_explicitly_comptime,
		.node = node,
	})) {
		report_error(self, node->variable.name,
			"duplicate declaration of '{s}'.", name);
		reset_new_type_counter();
		return VAL_BAD;
	}

	node->type = type;
	node->variable.is_explicitly_comptime = is_explicitly_comptime;
	inject(self->arena_allocator, node->variable.value, value);

	run_at_percent (0.67) {
		if (value_equal(value, VAL_SCALAR(AS_TYPEID(ty_untyped_int), .integer = 67))) {
			report_note(self, node->variable.value,
				"THE FORBIDDEN {value} NUMBER IS NOT ALLOWED.", value);
		}
	}

	reset_new_type_counter();

	return value;
}

// ── Struct analysis ───────────────────────────────────────────────

static struct haste_struct_field *alloc_struct_items(struct Allocator alloc, size_t count)
{
	return alloc(alloc, sizeof(struct haste_struct_field) * SAFE_COUNT(count));
}

static struct haste_struct_object *alloc_struct_object(struct Allocator alloc, size_t field_count)
{
	return alloc(alloc, sizeof(struct haste_struct_object) + sizeof(struct haste_value) * SAFE_COUNT(field_count));
}

static struct haste_value analyze_struct_type(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	discard expected_type;
	struct haste_type_info type_info = { .kind = HASTE_TY_STRUCT };
	struct haste_struct_type_info *st = &type_info.structure;

	st->len = 0;
	leach (struct haste_ast_node, field, node->struct_type.fields) {
		st->len += field->struct_field.name_count;
	}

	st->items = alloc_struct_items(self->allocator, st->len);

	bool has_error = false;
	size_t i = 0;
	leach (struct haste_ast_node, field, node->struct_type.fields) {
		for (size_t j=0; j < field->struct_field.name_count; j += 1) {
			const char *name = intern_token(field->struct_field.names[j]);
			struct haste_struct_field sf = {0};

			if (read_struct_field(self, name, field, &sf)) {
				has_error = true;
			} else {
				st->items[i++] = sf;
			}
		}
	}

	if (has_error) return VAL_BAD;

	struct haste_value result = VAL_TYPE(type_pool_add(type_info));
	inject(self->arena_allocator, node, result);
	return result;
}

static struct haste_value analyze_automatic_struct_literal(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	struct haste_type_info type_info = {
		.kind = HASTE_TY_AUTO_STRUCT,
	};
	struct haste_struct_type_info *st = &type_info.structure;

	st->len = 0;
	leach (struct haste_ast_node, f, node->struct_literal.fields) {
		st->len += 1;
	}

	st->items = alloc_struct_items(self->allocator, st->len);

	/* struct haste_struct_object *so = create(self->allocator, struct haste_struct_object, */
	/* 	.base = { .kind = HASTE_OBJ_STRUCT }); */
	/* so->fields = alloc(self->allocator, sizeof(struct haste_value) * SAFE_COUNT(st->len)); */
	struct haste_struct_object *so = alloc_struct_object(self->allocator, st->len);
	so->base.kind = HASTE_OBJ_STRUCT;

	size_t i = 0;
	leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
		if (lit_field->struct_lit_field.name.kind == 0) {
			return bail(self, lit_field->struct_lit_field.value,
				"Automatic struct literals must use named fields.");
		}
		struct haste_value fv = analyze_node(self, lit_field->struct_lit_field.value, expected_type);
		if (IS_BAD(fv)) return VAL_BAD;
		st->items[i] = (struct haste_struct_field){
			.name = intern_str(
				lit_field->struct_lit_field.name.start,
				lit_field->struct_lit_field.name.len),
			.type = typeof_value(fv),
		};
		so->fields[i] = fv;
		i += 1;
	}

	struct haste_value result = VAL_OBJ(type_pool_add(type_info), so);
	inject(self->arena_allocator, node, result);
	return result;
}

static struct haste_value analyze_struct_literal(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	if (node->struct_literal.type_expr == NULL)
		return analyze_automatic_struct_literal(self, node, expected_type);

	struct haste_type struct_type = {0};
	{
		try (tp, analyze_node(self, node->struct_literal.type_expr, (struct haste_type){0}))
		{
			if (not IS_TYPE(tp)) {
				return bail(
					self, node->struct_literal.type_expr,
					"Expected a type, got '{value}'.",
					typeof_value(tp));
			}

			struct_type = into_type(tp);
		}
	}

	if (IS_AUTO(struct_type)) {
		return analyze_automatic_struct_literal(
			self, node, expected_type);
	}

	if (not IS_STRUCT_TYPE(struct_type)) {
		return bail(
			self, node->struct_literal.type_expr,
			"Expected a struct type, got '{value}'.",
			struct_type);
	}

	struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(struct_type);

	struct haste_value result = make_value(self->allocator, struct_type);

	struct haste_struct_object *so = AS_STRUCT(result);

	bool has_error = false;

	size_t positional_idx = 0;
	leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
		ssize_t idx = -1;
		if (lit_field->struct_lit_field.name.kind == 0) {
			if (positional_idx >= st->len) {
				report_error(self, lit_field->struct_lit_field.value,
					"Too many positional fields for struct '{value}'.", struct_type);
				has_error = true;
				continue;
			}
			idx = positional_idx++;
		} else {
			idx = find_struct_field(st, lit_field->struct_lit_field.name);
			if (idx < 0) {
				report_error(self, lit_field,
					"Unknown field '{token}'.", lit_field->struct_lit_field.name);
				has_error = true;
				continue;
			}
			positional_idx = idx + 1;
		}

		inject_struct_type(self, lit_field->struct_lit_field.value, st->items[idx].type);
		catch (fv, err, analyze_node(self, lit_field->struct_lit_field.value, st->items[idx].type))
		{
			discard err;
			has_error = true;
		}

		catch (_, err, struct_set_field_by_index(self->allocator, &result, idx, fv))
		{
			discard err;
			so->fields[idx] = VAL_BAD;
			report_error(self, lit_field,
						 "Cannot assign a value of type '{value}' to a value of type '{value}'",
						 typeof_value(fv), st->items[idx].type);
			has_error = true;
		}
	}

	// TODO: Probably gotta find a better way to iterate over struct's fields
	iarreach (i, *st) {
		if (IS_NONE(so->fields[i])) {
			report_error(self, node,
				"Forgot to set a value for `{s}`.", st->items[i].name);
			has_error = true;
		}
	}

	if (has_error) return VAL_BAD;

	inject(self->arena_allocator, node, result);
	return result;
}

// ── Main dispatch ────────────────────────────────────────────────

struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node, struct haste_type expected_type)
{
	if (IS_NODE_ANALYZED(node)) {
		return node->kind == ND_VALUE then node->value otherwise VAL_BAD;
	}

	switch (node->kind) {
	case ND_STRUCT_FIELD:     unreachable();
	case ND_STRUCT_LIT_FIELD: unreachable();
	case ND_VALUE:            unreachable();
	case ND_BINARY:           return analyze_binary(self, node, expected_type);
	case ND_UNARY:            return analyze_unary(self, node, expected_type);
	case ND_ACCESS:           return analyze_access(self, node, expected_type);
	case ND_PRIMARY:          return analyze_primary(self, node, expected_type);
	case ND_GROUPING:         return analyze_grouping(self, node, expected_type);
	case ND_DISTINCT:         return analyze_distinct(self, node, expected_type);
	case ND_CAST:             return analyze_cast(self, node, expected_type);
	case ND_STRUCT_TYPE:      return analyze_struct_type(self, node, expected_type);
	case ND_STRUCT_LITERAL:   return analyze_struct_literal(self, node, expected_type);
	case ND_VAR_DECL:         return analyze_var_decl(self, node, expected_type);
	default:                  unreachable();
	}
}

Error analyze_one_node(
	struct Allocator allocator,
	struct Allocator arena_allocator,
	struct haste_ast_node *node,
	struct haste_value *out)
{
	struct analyzer analyzer = {
		.allocator = allocator,
		.arena_allocator = arena_allocator,
		.src = -1,
	};
	with_scope(&analyzer) {
		*out = analyze_node(&analyzer, node, into_type(VAL_NONE));
	}
	return analyzer.had_error then ERROR otherwise OK;
}

Error analyze(struct Allocator allocator,
              struct Allocator arena_allocator,
              const source_file_id src)
{
	struct analyzer analyzer = {
		.allocator = allocator,
		.arena_allocator = arena_allocator,
		.src = src,
	};
	with_scope(&analyzer) {
		struct haste_ast_node *root = get_source_file_ast(src);
		leach (struct haste_ast_node, node, root) {
			analyze_node(&analyzer, node, (struct haste_type){0});
			reset_temporary_allocator();
		}
	}
	return analyzer.had_error then ERROR otherwise OK;
}
