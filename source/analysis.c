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
		struct haste_value type;
		struct haste_value value;
		struct haste_ast_node *node;
	} *items;
};

struct analyzer {
	struct Allocator allocator;
	struct Allocator arena_allocator;
	struct intern_table *intern_table;
	struct scope *global, *local;
	const source_file_id src;
	bool had_error;
};

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node);

// ── Error helpers ────────────────────────────────────────────────

#define report_error(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_error_node, \
		struct token: _report_error_token)(self_, node_, __VA_ARGS__)
static void _report_error_node(struct analyzer *self, struct haste_ast_node *node, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_RED "Error", node->start, fmt, args);
	va_end(args);
	self->had_error = true;
}

static void _report_error_token(struct analyzer *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_RED "Error", token, fmt, args);
	va_end(args);
	self->had_error = true;
}

#define report_note(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_note_node, \
		struct token: _report_note_token)(self_, node_, __VA_ARGS__)
static void _report_note_node(struct analyzer *self, struct haste_ast_node *node, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_GREEN "Note", node->start, fmt, args);
	va_end(args);
}

static void _report_note_token(struct analyzer *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_GREEN "Note", token, fmt, args);
	va_end(args);
}

#define report_warning(self_, node_, ...) \
	_Generic((node_), \
		struct haste_ast_node *: _report_warning_node, \
		struct token: _report_warning_token)(self_, node_, __VA_ARGS__)
static void _report_warning_node(struct analyzer *self, struct haste_ast_node *node, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_YELLOW "Warning", node->start, fmt, args);
	va_end(args);
}

static void _report_warning_token(struct analyzer *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_YELLOW "Warning", token, fmt, args);
	va_end(args);
}

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
		return NULL;
	}

	decl->analyzing = true;
	analyze_node(self, decl->node);
	decl->analyzing = false;

	return find_local_first(self, name);
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

// ── Per-node-kind analysis ───────────────────────────────────────

static struct haste_value analyze_binary(struct analyzer *self, struct haste_ast_node *node)
{
	struct haste_value lhs = analyze_node(self, node->lhs);
	struct haste_value rhs = analyze_node(self, node->rhs);
	struct haste_value value = {0};

	if (IS_BAD(lhs)) return lhs;
	if (IS_BAD(rhs)) return rhs;

	switch (node->op.kind) {
	case TK_PLUS:
		value = value_add(lhs, rhs);
		if (IS_BAD(value)) {
			run_at_percent (1.5) {
				report_error(self, node->op, "Addition is not impossible. (try harder)");
			} else {
				report_error(self, node->op,
					"Addition is not possible between '{value}' and '{value}'",
					typeof(lhs), typeof(rhs));
			}
			return VAL_BAD;
		}
		break;
	case TK_MINUS:
		value = value_sub(lhs, rhs);
		if (IS_BAD(value)) {
			report_error(self, node->op,
				"Subtraction is not possible between '{value}' and '{value}'",
				typeof(lhs), typeof(rhs));
			return VAL_BAD;
		}
		break;
	case TK_STAR:
		value = value_mul(lhs, rhs);
		if (IS_BAD(value)) {
			report_error(self, node->op,
				"Multiplication is not possible between '{value}' and '{value}'",
				typeof(lhs), typeof(rhs));
			return VAL_BAD;
		}
		break;
	case TK_FSLASH:
		value = value_div(lhs, rhs);
		if (IS_BAD(value)) {
			report_error(self, node->op,
				"Division is not possible between '{value}' and '{value}'",
				typeof(lhs), typeof(rhs));
			return VAL_BAD;
		}
		break;
	default: unreachable();
	}

	node->type = typeof(value);
	node_transform(node, ND_VALUE, value = value);
	return value;
}

static struct haste_value analyze_unary(struct analyzer *self, struct haste_ast_node *node)
{
	struct haste_value value = analyze_node(self, node->rhs);
	node->type = typeof(value);
	return value;
}

static struct haste_value analyze_primary(struct analyzer *self, struct haste_ast_node *node)
{
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

static struct haste_value analyze_grouping(struct analyzer *self, struct haste_ast_node *node)
{
	struct haste_value value = analyze_node(self, node->body);
	node->type = typeof(value);
	return value;
}

static struct haste_value analyze_cast(struct analyzer *self, struct haste_ast_node *node)
{
	if (node->cast.to == NULL) unimplemented();

	struct haste_value to = analyze_node(self, node->cast.to);
	if (not type_equal(typeof(to), ty_type)) {
		report_error(self, node->cast.to,
			"expected a {value} got '{value}' instead.", ty_type, typeof(to));
		return VAL_BAD;
	}

	struct haste_value value = analyze_node(self, node->cast.expr);
	if (not type_can_cast(to, typeof(value))) {
		report_error(self, node,
			"Cannot cast '{value}' to '{value}'", typeof(value), to);
		return VAL_BAD;
	}

	struct haste_value result = value_cast(self->arena_allocator, to, value);
	node->type = typeof(result);
	return result;
}

static struct haste_value analyze_var_decl(struct analyzer *self, struct haste_ast_node *node)
{
	const char *name = intern_token(self->intern_table, node->variable.name);
	const bool is_constant = node->variable.is_constant;
	struct scope *target_scope = node->variable.is_global then self->global otherwise self->local;

	if (node->variable.type == NULL and node->variable.value == NULL) {
		report_error(self, node, "You need to either specify the type or the value or both.");
		put_symbol(self, target_scope, name,
			.type = VAL_BAD,
			.value = VAL_BAD,
			.is_constant = is_constant,
			.node = node);
		return VAL_BAD;
	}

	struct haste_value type = ty_auto;
	if (node->variable.type != NULL) {
		type = analyze_node(self, node->variable.type);
	}

	if (not type_equal(typeof(type), ty_type)) {
		report_error(self, node->variable.type,
			"Expected a {value} got '{value}' instead.", ty_auto, typeof(type));
		put_symbol(self, target_scope, name,
			.value = VAL_BAD,
			.type = VAL_BAD,
			.is_constant = is_constant,
			.node = node);
		return VAL_BAD;
	}

	struct haste_value value = VAL_UNINIT;
	if (node->variable.value != NULL) {
		// For anonymous struct literals (.{...}) without a type expression,
		// inject the type annotation so analyze_struct_literal can use it
		if (node->variable.value->kind == ND_STRUCT_LITERAL
		    && node->variable.value->struct_literal.type_expr == NULL
		    && !type_equal(type, ty_auto)) {
			struct haste_ast_node *ty_node = create(self->arena_allocator,
				struct haste_ast_node, .type = typeof(type));
			ty_node = node_into_value(self->arena_allocator, ty_node, type);
			node->variable.value->struct_literal.type_expr = ty_node;
		}
		value = analyze_node(self, node->variable.value);
		if (IS_BAD(value)) {
			put_symbol(self, target_scope, name,
				.value = VAL_BAD,
				.type = VAL_BAD,
				.is_constant = is_constant,
				.node = node);
			return VAL_BAD;
		}
	}

	if (type_equal(type, ty_auto)) {
		type = typeof(value);
	} else if (not type_can_assign(type, typeof(value))) {
		report_error(self, node->variable.name,
			"cannot assign a value of type '{value}' to '{value}'.", typeof(value), type);
		put_symbol(self, target_scope, name,
			.value = VAL_BAD,
			.type = VAL_BAD,
			.is_constant = is_constant,
			.node = node);
		return VAL_BAD;
	} else if (IS_UNINIT(value)) {
		value = default_for_type(self->arena_allocator, type);
	}

	if (not type_equal(type, typeof(value))) {
		value = value_cast(self->arena_allocator, type, value);
	}

	const bool is_explicitly_comptime = node->variable.is_explicitly_comptime
		or (is_constant and type_equal(type, ty_type));

	put_symbol(self, target_scope, name,
		.type = type,
		.value = value,
		.is_constant = is_constant,
		.is_explicitly_comptime = is_explicitly_comptime,
		.node = node);

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

static struct haste_value analyze_struct_type(struct analyzer *self, struct haste_ast_node *node)
{
	struct haste_struct_type *st = create(self->arena_allocator, struct haste_struct_type,
		.base = { .base = { .kind = HASTE_OBJ_TYPE }, .kind = HASTE_TY_STRUCT });

	size_t count = 0;
	leach (struct haste_ast_node, field, node->struct_type.fields)
		count += 1;
	st->field_count = count;
	st->fields = alloc(self->arena_allocator, sizeof(struct haste_struct_field) * count);

	size_t i = 0;
	bool got_an_error = false;
	leach (struct haste_ast_node, field, node->struct_type.fields) {
		const char *name = intern_str(self->intern_table, field->struct_field.name.start, field->struct_field.name.len);
		struct haste_value field_type = analyze_node(self, field->struct_field.type);
		if (IS_BAD(field_type) or not type_equal(typeof(field_type), ty_type)) {
			if (not IS_BAD(field_type)) {
				report_error(self, field->struct_field.type,
					"expected a type for field, got '{value}' instead.", typeof(field_type));
			}
			got_an_error = true;
			continue;
		}
		struct haste_value default_value = {0};
		bool has_default = false;
		if (field->struct_field.default_value != NULL) {
			default_value = analyze_node(self, field->struct_field.default_value);
			if (type_can_assign(field_type, typeof(default_value))) {
				default_value = value_cast(self->arena_allocator, field_type, default_value);
			} else {
				report_error(self, field->struct_field.default_value,
					"Cannot set the default value of type '{value}' to '{value}'",
					typeof(default_value), field_type);
				default_value = VAL_BAD;
			}
			has_default = true;
		}
		st->fields[i] = (struct haste_struct_field){
			.name = name,
			.type = field_type,
			.default_value = default_value,
			.has_default = has_default,
		};
		i += 1;
	}

	struct haste_value result = {
		.kind = HASTE_VL_OBJ,
		.type = AS_TYPE(ty_type),
		.obj = &st->base.base,
	};
	if (got_an_error) {
		result = VAL_BAD;
		result.type = AS_TYPE(ty_type);
	}
	node = node_into_value(self->arena_allocator, node, result);
	return result;
}

static struct haste_value analyze_struct_literal(struct analyzer *self, struct haste_ast_node *node)
{
	struct haste_value struct_type = {0};

	if (node->struct_literal.type_expr != NULL) {
		struct_type = analyze_node(self, node->struct_literal.type_expr);
	}
	if (node->struct_literal.type_expr == NULL or type_equal(struct_type, ty_auto)) {
		// Anonymous struct literal — create a struct type from the fields
		size_t field_count = 0;
		leach (struct haste_ast_node, f, node->struct_literal.fields)
			field_count += 1;

		struct haste_struct_type *st = create(self->arena_allocator, struct haste_struct_type,
			.base = { .base = { .kind = HASTE_OBJ_TYPE }, .kind = HASTE_TY_STRUCT });
		st->field_count = field_count;
		size_t fc = field_count > 0 ? field_count : 1;
		st->fields = alloc(self->arena_allocator, sizeof(struct haste_struct_field) * fc);

		struct haste_struct_object *so = create(self->arena_allocator, struct haste_struct_object,
			.base = { .kind = HASTE_OBJ_STRUCT });
		so->fields = alloc(self->arena_allocator, sizeof(struct haste_value) * fc);

		size_t i = 0;
		leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
			const char *name = intern_str(self->intern_table,
				lit_field->struct_lit_field.name.start,
				lit_field->struct_lit_field.name.len);
			struct haste_value fv = analyze_node(self, lit_field->struct_lit_field.value);
			if (IS_BAD(fv)) return VAL_BAD;
			st->fields[i] = (struct haste_struct_field){
				.name = name,
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

	// Initialize all fields with their defaults first
	for (size_t i = 0; i < st->field_count; i += 1) {
		if (st->fields[i].has_default) {
			so->fields[i] = st->fields[i].default_value;
			if (not type_equal(typeof(so->fields[i]), st->fields[i].type))
				so->fields[i] = value_cast(self->arena_allocator, st->fields[i].type, so->fields[i]);
		} else {
			so->fields[i] = VAL_NONE;
		}
	}

	bool has_error = false;

	// Override with provided fields
	leach (struct haste_ast_node, lit_field, node->struct_literal.fields) {
		bool found = false;
		for (size_t i = 0; i < st->field_count; i += 1) {
			if (strncmp(lit_field->struct_lit_field.name.start, st->fields[i].name,
						lit_field->struct_lit_field.name.len) == 0
				and st->fields[i].name[lit_field->struct_lit_field.name.len] == '\0') {
				if (lit_field->struct_lit_field.value->kind == ND_STRUCT_LITERAL) {
					struct haste_ast_node *lit = lit_field->struct_lit_field.value;
					if (lit->struct_literal.type_expr == NULL
					    or (lit->struct_literal.type_expr->kind == ND_PRIMARY
					        and lit->struct_literal.type_expr->token.kind == TK_KW_AUTO)) {
						struct haste_ast_node *ty_node = create(self->arena_allocator,
							struct haste_ast_node, .type = typeof(st->fields[i].type));
						ty_node = node_into_value(self->arena_allocator, ty_node, st->fields[i].type);
						lit->struct_literal.type_expr = ty_node;
					}
				}
				struct haste_value fv = analyze_node(self, lit_field->struct_lit_field.value);
				if (not IS_BAD(fv)) {
					if (type_can_assign(st->fields[i].type, typeof(fv))) {
						fv = value_cast(self->arena_allocator, st->fields[i].type, fv);
					} else {
						report_error(self, lit_field,
							"Cannot assign a value of type '{value}' to a value of type '{value}'",
							typeof(fv), st->fields[i].type);
						has_error = true;
					}
					so->fields[i] = fv;
				} else {
					has_error = true;
				}
				found = true;
				break;
			}
		}
		if (not found) {
			report_error(self, lit_field,
				"Unknown field '{token}'.", lit_field->struct_lit_field.name);
			has_error = true;
		}
	}

	for (size_t i = 0; i < st->field_count; i += 1) {
		struct haste_struct_field field = st->fields[i];
		struct haste_value field_value = so->fields[i];
		if (IS_NONE(field_value)) {
			report_error(self, node,
				"Forgot to set a value for `{s}`.", field.name);
			has_error = true;
		}
	}

	struct haste_value result = {
		.kind = HASTE_VL_OBJ,
		.type = AS_TYPE(struct_type),
		.obj = &so->base,
	};
	if (has_error) {
		result = VAL_BAD;
		result.type = AS_TYPE(struct_type);
	}

	node->type = typeof(result);
	node_transform(node, ND_VALUE, value = result);
	return result;
}

// ── Main dispatch ────────────────────────────────────────────────

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node)
{
	if (IS_NODE_ANALYZED(node)) {
		return node->kind == ND_VALUE then node->value otherwise VAL_BAD;
	}

	switch (node->kind) {
	case ND_STRUCT_FIELD:     unreachable();
	case ND_STRUCT_LIT_FIELD: unreachable();
	case ND_VALUE:            unreachable();
	case ND_BINARY:           return analyze_binary(self, node);
	case ND_UNARY:            return analyze_unary(self, node);
	case ND_PRIMARY:          return analyze_primary(self, node);
	case ND_GROUPING:         return analyze_grouping(self, node);
	case ND_CAST:             return analyze_cast(self, node);
	case ND_STRUCT_TYPE:      return analyze_struct_type(self, node);
	case ND_STRUCT_LITERAL:   return analyze_struct_literal(self, node);
	case ND_VAR_DECL:         return analyze_var_decl(self, node);
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
		analyze_node(&analyzer, node);
		reset_temporary_allocator();
	}

	end_scope(&analyzer);
	return analyzer.had_error then ERROR otherwise OK;
}
