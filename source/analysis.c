#include "haste.h"
#include "my_termcolor.h"

#define IS_NODE_ANALYZED(...) (__VA_ARGS__)->type.kind != 0

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
		struct haste_value type;
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
	struct intern_table *intern_table;
	struct scope *global, *local;
	const source_file_id src;
	bool had_error;
};

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type);

// ── Error helpers ────────────────────────────────────────────────

static void _vreport(struct analyzer *self, struct token token, const char *kind, bool set_error, const char *restrict fmt, va_list args)
{
	f_vreport_at_token(self->src, kind, token, fmt, args);
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
			.type = VAL_BAD, \
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
	analyze_node(self, decl->node, (struct haste_value){0});
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
		analyze_node(self, decl->node, (struct haste_value){0});
		decl->analyzing = false;

		struct symbol *s = hmget(*self->global, name);
		if (s != NULL) symbol_set_add(self->allocator, &result, *s);
	}

	return result;
}

// ── Token → value ──────────────────────────────────────────────

static struct haste_string_object *make_string_obj(struct Allocator allocator, char *data, size_t len)
{
	struct haste_string_object *obj = create(
		allocator, struct haste_string_object,
		.base = { .kind = HASTE_OBJ_STRING },
		.data = data,
		.len = len);
	return obj;
}

static struct haste_value token_to_value(struct token token, struct Allocator arena)
{
	switch (token.kind) {
	case TK_IDENT:    unimplemented();
	case TK_STR: {
		size_t len = strlen(token.str);
		struct haste_string_object *obj = make_string_obj(arena, (char *)token.str, len);
		return (struct haste_value){
			.kind = HASTE_VL_OBJ,
			.type = AS_TYPE(ty_untyped_string),
			.obj = &obj->base,
		};
	}
	case TK_INT:
		if (token.ival == 0) return VAL_ZERO;
		return VAL_SCALAR(AS_TYPE(ty_untyped_int), .integer = token.ival);
	case TK_FLOAT:
		return VAL_SCALAR(AS_TYPE(ty_untyped_float), .floating = token.fval);
	case TK_KW_STRING: return ty_string;
	case TK_KW_CSTR:   return ty_cstr;
	case TK_KW_INT:    return ty_int;
	case TK_KW_FLOAT:  return ty_float;
	case TK_KW_VOID:   return ty_void;
	case TK_KW_TYPE:   return ty_type;
	case TK_KW_AUTO:   return ty_auto;
	default:           unreachable();
	}
}

// ── Struct helpers ─────────────────────────────────────────────

static bool field_name_matches(struct haste_struct_field field, struct token name_token)
{
	return strncmp(name_token.start, field.name, name_token.len) == 0
		and field.name[name_token.len] == '\0';
}

static void inject_struct_type(struct analyzer *self, struct haste_ast_node *lit, struct haste_value field_type)
{
	if (lit->kind != ND_STRUCT_LITERAL) return;
	if (lit->struct_literal.type_expr != NULL
		and (lit->struct_literal.type_expr->kind != ND_PRIMARY
			or lit->struct_literal.type_expr->token.kind != TK_KW_AUTO))
		return;
	struct haste_ast_node *ty_node = create(self->arena_allocator,
		struct haste_ast_node, .type = typeof(field_type));
	ty_node = node_into_value(self->arena_allocator, ty_node, field_type);
	lit->struct_literal.type_expr = ty_node;
}

static ssize_t find_struct_field(struct haste_struct_type *st, struct token name)
{
	for (size_t i = 0; i < st->field_count; i += 1)
		if (field_name_matches(st->fields[i], name))
			return i;
	return -1;
}

// Returns true on error. Fills `*out` with the field info on success.
static bool read_struct_field(struct analyzer *self,
	struct haste_ast_node *field, struct haste_struct_field *out)
{
	const char *name = intern_token(self->intern_table, field->struct_field.name);

	struct haste_value field_type = ty_auto;
	if (field->struct_field.type != NULL)
		field_type = analyze_node(self, field->struct_field.type, (struct haste_value){0});

	if (IS_BAD(field_type) or not type_equal(typeof(field_type), ty_type)) {
		if (not IS_BAD(field_type))
			report_error(self, field->struct_field.type,
				"expected a type for field, got '{value}' instead.", typeof(field_type));
		out->type = VAL_BAD;
		return true;
	}

	struct haste_value default_value = {0};
	bool has_default = false;
	if (field->struct_field.default_value != NULL) {
		default_value = analyze_node(self, field->struct_field.default_value, field_type);
		if (type_can_assign(field_type, typeof(default_value))) {
			default_value = value_cast(self->arena_allocator, field_type, default_value);
			if (type_equal(field_type, ty_auto)) {
				field_type = typeof(default_value);
				field_type = untyped_to_typed(field_type);
			}
		} else {
			report_error(self, field->struct_field.default_value,
				"Cannot set the default value of type '{value}' to '{value}'",
				typeof(default_value), field_type);
			default_value = VAL_BAD;
			return true;
		}
		has_default = true;
	}

	if (type_equal(field_type, ty_auto) and not has_default) {
		report_error(self, field->struct_field.name,
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

static struct haste_value analyze_binary(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	struct haste_value lhs = analyze_node(self, node->lhs, expected_type);
	struct haste_value rhs = analyze_node(self, node->rhs, expected_type);
	struct haste_value value = {0};

	if (IS_BAD(lhs)) return lhs;
	if (IS_BAD(rhs)) return rhs;

	value = resolve_binary_op(self, lhs, rhs, node->op);

	if (IS_BAD(value)) return VAL_BAD;

	node->type = typeof(value);
	node_transform(node, ND_VALUE, value = value);
	return value;
}

#define BINARY_OP_CASE(kind, op_func, op_name) \
	case kind: \
		value = op_func(lhs, rhs); \
		if (IS_BAD(value)) { \
			report_error(self, op, \
				op_name " is not possible between '{value}' and '{value}'", \
				typeof(lhs), typeof(rhs)); \
			return VAL_BAD; \
		} \
		break;

static struct haste_value resolve_binary_op(struct analyzer *self, struct haste_value lhs, struct haste_value rhs, struct token op)
{
	struct haste_value value = {0};

	switch (op.kind) {
	case TK_PLUS:
		value = value_add(lhs, rhs);
		if (IS_BAD(value)) {
			run_at_percent (1.5) {
				report_error(self, op, "Addition is not impossible. (try harder)");
			} else {
				report_error(self, op,
					"Addition is not possible between '{value}' and '{value}'",
					typeof(lhs), typeof(rhs));
			}
			return VAL_BAD;
		}
		break;
	BINARY_OP_CASE(TK_MINUS,  value_sub, "Subtraction")
	BINARY_OP_CASE(TK_STAR,   value_mul, "Multiplication")
	BINARY_OP_CASE(TK_FSLASH, value_div, "Division")
	default: unreachable();
	}

	return value;
}

static struct haste_value analyze_unary(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	struct haste_value value = analyze_node(self, node->rhs, expected_type);

	switch (node->op.kind) {
	case TK_MINUS:
		if (IS_ZERO(value)) {
			value = VAL_SCALAR(AS_TYPE(typeof(value)), .integer = 0);
		} else if (IS_SCALAR(value)) {
			if (type_is_integer(typeof(value)))
				value.integer = -value.integer;
			else if (type_is_float(typeof(value)))
				value.floating = -value.floating;
			else
				goto neg_error;
		} else if (IS_BAD(value)) {
			return VAL_BAD;
		} else {
			goto neg_error;
		}
		break;
	default:
		break;
	}

	node->type = typeof(value);
	node->kind = ND_VALUE;
	node->value = value;
	return value;

neg_error:
	report_error(self, node->op,
		"Negation is not possible on '{value}'.", typeof(value));
	return VAL_BAD;
}

static struct haste_value analyze_primary(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	discard expected_type;
	struct haste_value value = {0};
	if (node->token.kind == TK_IDENT) {
		const char *name = intern_token(self->intern_table, node->token);
		struct symbol *symbol = find_local_first(self, name);
		if (symbol == NULL) {
			report_error(self, node, "undefined symbol '{s}'.", name);
			return VAL_UNINIT;
		}
		value = symbol->value;
	} else {
		value = token_to_value(node->token, self->arena_allocator);
	}

	node->type = typeof(value);
	node->kind = ND_VALUE;
	node->value = value;
	return value;
}

static struct haste_value analyze_grouping(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	struct haste_value value = analyze_node(self, node->body, expected_type);
	node->type = typeof(value);
	return value;
}

static struct haste_value analyze_cast(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	struct haste_value to = ty_auto;
	if (node->cast.to == NULL) {
		to = expected_type;
	} else {
		to = analyze_node(self, node->cast.to, (struct haste_value){0});
	}

	if (not type_equal(typeof(to), ty_type)) {
		report_error(self, node->cast.to then node->cast.to otherwise node,
			"expected a {value} got '{value}' instead.", ty_type, typeof(to));
		return VAL_BAD;
	}

	struct haste_value value = analyze_node(self, node->cast.expr, (struct haste_value){0});
	if (not type_can_cast(to, typeof(value))) {
		report_error(self, node,
			"Cannot cast '{value}' to '{value}'", typeof(value), to);
		return VAL_BAD;
	}

	struct haste_value result = value_cast(self->arena_allocator, to, value);
	node->type = typeof(result);
	return result;
}

static struct haste_value analyze_var_decl(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	discard expected_type;
	const char *name = intern_token(self->intern_table, node->variable.name);
	const bool is_constant = node->variable.is_constant;
	struct scope *target_scope = node->variable.is_global then self->global otherwise self->local;

	if (node->variable.type == NULL and node->variable.value == NULL) {
		report_error(self, node, "You need to either specify the type or the value or both.");
		emit_error_symbol(self, target_scope, name, is_constant, node);
	}

	struct haste_value type = ty_auto;
	if (node->variable.type != NULL) {
		type = analyze_node(self, node->variable.type, (struct haste_value){0});
		if (IS_BAD(type))
			emit_error_symbol(self, target_scope, name, is_constant, node);
	}

	if (not type_equal(typeof(type), ty_type)) {
		report_error(self, node->variable.type,
			"Expected a {value} got '{value}' instead.", ty_auto, typeof(type));
		emit_error_symbol(self, target_scope, name, is_constant, node);
	}

	struct haste_value value = VAL_UNINIT;
	if (node->variable.value != NULL) {
		struct haste_declaration *struct_decl = node->variable.value->kind == ND_STRUCT_TYPE
			then hmget(sources.items[self->src].declarations, name)
			otherwise NULL;

		if (struct_decl != NULL) struct_decl->analyzing = true;

		if (not type_equal(type, ty_auto))
			inject_struct_type(self, node->variable.value, type);

		value = analyze_node(self, node->variable.value, type);

		if (struct_decl != NULL) struct_decl->analyzing = false;

		if (IS_BAD(value))
			emit_error_symbol(self, target_scope, name, is_constant, node);
	}

	if (type_equal(type, ty_auto))
		type = typeof(value);
	else if (not type_can_assign(type, typeof(value))) {
		report_error(self, node->variable.name,
			"cannot assign a value of type '{value}' to '{value}'.", typeof(value), type);
		emit_error_symbol(self, target_scope, name, is_constant, node);
	} else if (IS_UNINIT(value))
		value = default_for_type(self->arena_allocator, type);

	if (not type_equal(type, typeof(value)))
		value = value_cast(self->arena_allocator, type, value);

	if (IS_STRUCT_TYPE(value))
		AS_TYPE(value)->name = name;

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
		return VAL_BAD;
	}

	node->type = type;
	node->variable.is_explicitly_comptime = is_explicitly_comptime;
	node->variable.value = node_into_value(self->arena_allocator, node->variable.value, value);

	run_at_percent (0.67) {
		if (value_equal(value, VAL_SCALAR(AS_TYPE(ty_untyped_int), .integer = 67))) {
			report_note(self, node->variable.value,
				"THE FORBIDDEN {value} NUMBER IS NOT ALLOWED.", value);
		}
	}

	return value;
}

// ── Struct analysis ───────────────────────────────────────────────

static struct haste_value analyze_struct_type(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	discard expected_type;
	struct haste_struct_type *st = create(self->arena_allocator, struct haste_struct_type,
		.base = { .base = { .kind = HASTE_OBJ_TYPE }, .kind = HASTE_TY_STRUCT });

	st->field_count = 0;
	leach (struct haste_ast_node, field, node->struct_type.fields)
		st->field_count += 1;

	st->fields = alloc(self->arena_allocator,
		sizeof(struct haste_struct_field) * SAFE_COUNT(st->field_count));

	bool has_error = false;
	size_t i = 0;
	leach (struct haste_ast_node, field, node->struct_type.fields) {
		struct haste_struct_field sf;
		if (read_struct_field(self, field, &sf)) {
			has_error = true;
		} else {
			st->fields[i++] = sf;
		}
	}

	if (has_error) return VAL_BAD;

	struct haste_value result = {
		.kind = HASTE_VL_OBJ,
		.type = AS_TYPE(ty_type),
		.obj = &st->base.base,
	};
	node = node_into_value(self->arena_allocator, node, result);
	return result;
}

static struct haste_value analyze_automatic_struct_literal(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	struct haste_struct_type *st = create(self->arena_allocator, struct haste_struct_type,
		.base = { .base.kind = HASTE_OBJ_TYPE, .kind = HASTE_TY_AUTO_STRUCT });

	st->field_count = 0;
	leach (struct haste_ast_node, f, node->struct_literal.fields)
		st->field_count += 1;

	const size_t alloc_size = SAFE_COUNT(st->field_count);
	st->fields = alloc(self->arena_allocator, sizeof(struct haste_struct_field) * alloc_size);

	struct haste_struct_object *so = create(self->arena_allocator, struct haste_struct_object,
		.base = { .kind = HASTE_OBJ_STRUCT });
	so->fields = alloc(self->arena_allocator, sizeof(struct haste_value) * alloc_size);

	size_t i = 0;
	leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
		struct haste_value fv = analyze_node(self, lit_field->struct_lit_field.value, expected_type);
		if (IS_BAD(fv)) return VAL_BAD;
		st->fields[i] = (struct haste_struct_field){
			.name = intern_str(self->intern_table,
				lit_field->struct_lit_field.name.start,
				lit_field->struct_lit_field.name.len),
			.type = typeof(fv),
		};
		so->fields[i] = fv;
		i += 1;
	}

	struct haste_value result = {
		.kind = HASTE_VL_OBJ,
		.type = &st->base,
		.obj = &so->base,
	};
	node->type = typeof(result);
	node_transform(node, ND_VALUE, value = result);
	return result;
}

static struct haste_value analyze_struct_literal(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
{
	if (node->struct_literal.type_expr == NULL)
		return analyze_automatic_struct_literal(self, node, expected_type);

	struct haste_value struct_type = analyze_node(self, node->struct_literal.type_expr, (struct haste_value){0});
	if (type_equal(struct_type, ty_auto))
		return analyze_automatic_struct_literal(self, node, expected_type);

	if (not type_equal(typeof(struct_type), ty_type)) {
		report_error(self, node->struct_literal.type_expr,
			"Expected a type, got '{value}'.", typeof(struct_type));
		return VAL_BAD;
	}

	if (not IS_STRUCT_TYPE(struct_type)) {
		report_error(self, node->struct_literal.type_expr,
			"Expected a struct type, got '{value}'.", struct_type);
		return VAL_BAD;
	}

	struct haste_struct_type *st = AS_STRUCT_TYPE(struct_type);

	struct haste_struct_object *so = create(self->arena_allocator, struct haste_struct_object,
		.base = { .kind = HASTE_OBJ_STRUCT });
	so->fields = alloc(self->arena_allocator, sizeof(struct haste_value) * st->field_count);

	bool has_error = false;

	for (size_t i = 0; i < st->field_count; i += 1) {
		if (st->fields[i].has_default) {
			so->fields[i] = st->fields[i].default_value;
			if (not type_equal(typeof(so->fields[i]), st->fields[i].type))
				so->fields[i] = value_cast(self->arena_allocator, st->fields[i].type, so->fields[i]);
		} else {
			so->fields[i] = VAL_NONE;
		}
	}

	leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
		ssize_t idx = find_struct_field(st, lit_field->struct_lit_field.name);
		if (idx < 0) {
			report_error(self, lit_field,
				"Unknown field '{token}'.", lit_field->struct_lit_field.name);
			has_error = true;
			continue;
		}

		inject_struct_type(self, lit_field->struct_lit_field.value, st->fields[idx].type);
		struct haste_value fv = analyze_node(self, lit_field->struct_lit_field.value, st->fields[idx].type);

		if (IS_BAD(fv)) {
			has_error = true;
		} else if (not type_can_assign(st->fields[idx].type, typeof(fv))) {
			report_error(self, lit_field,
				"Cannot assign a value of type '{value}' to a value of type '{value}'",
				typeof(fv), st->fields[idx].type);
			has_error = true;
		} else {
			fv = value_cast(self->arena_allocator, st->fields[idx].type, fv);
			so->fields[idx] = fv;
		}
	}

	for (size_t i = 0; i < st->field_count; i += 1) {
		if (IS_NONE(so->fields[i])) {
			report_error(self, node,
				"Forgot to set a value for `{s}`.", st->fields[i].name);
			has_error = true;
		}
	}

	if (has_error) return VAL_BAD;

	struct haste_value result = {
		.kind = HASTE_VL_OBJ,
		.type = AS_TYPE(struct_type),
		.obj = &so->base,
	};
	node->type = typeof(result);
	node_transform(node, ND_VALUE, value = result);
	return result;
}

// ── Main dispatch ────────────────────────────────────────────────

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node, struct haste_value expected_type)
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
	case ND_PRIMARY:          return analyze_primary(self, node, expected_type);
	case ND_GROUPING:         return analyze_grouping(self, node, expected_type);
	case ND_CAST:             return analyze_cast(self, node, expected_type);
	case ND_STRUCT_TYPE:      return analyze_struct_type(self, node, expected_type);
	case ND_STRUCT_LITERAL:   return analyze_struct_literal(self, node, expected_type);
	case ND_VAR_DECL:         return analyze_var_decl(self, node, expected_type);
	}
}

Error analyze(struct Allocator allocator,
              struct Allocator arena_allocator,
              struct intern_table *intern_table,
              const source_file_id src)
{
	struct analyzer analyzer = {
		.allocator = allocator,
		.arena_allocator = arena_allocator,
		.intern_table = intern_table,
		.src = src,
	};
	begin_scope(&analyzer);

	struct haste_ast_node *root = get_source_file_ast(src);
	leach (struct haste_ast_node, node, root) {
		analyze_node(&analyzer, node, (struct haste_value){0});
		reset_temporary_allocator();
	}

	end_scope(&analyzer);
	return analyzer.had_error then ERROR otherwise OK;
}
