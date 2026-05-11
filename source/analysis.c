#include "haste.h"
#include "my_termcolor.h"

#define IS_NODE_ANALYZED(...) (__VA_ARGS__)->type.kind != 0
#define NODE_IS_CAST(...)     (__VA_ARGS__)->kind == ND_CAST

struct scope {
	struct scope *next;
	size_t len;
	struct symbol {
		char *key;
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
	struct scope *global, *local;
	const source_file_id src;
	bool had_error;
};

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node);

static struct scope *begin_scope(struct analyzer *self)
{
	struct scope *new_scope = create(self->allocator, struct scope, .next = self->local);
	self->local = new_scope;
	if (self->global == NULL) self->global = self->local;
	return new_scope;
}

static void end_scope(struct analyzer *self)
{
	if (self->global == NULL) return;
	struct scope *scope = self->local;
	self->local = scope->next;
	hmfree(self->allocator, *scope);
	destroy(self->allocator, scope);
}

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
	exit(1);
}

static void _report_error_token(struct analyzer *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_RED "Error", token, fmt, args);
	va_end(args);
	self->had_error = true;
	exit(1);
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
	self->had_error = true;
	exit(1);
}

static void _report_note_token(struct analyzer *self, struct token token, const char *restrict fmt, ...)
{
	va_list args; va_start(args, fmt);
	f_vreport_at_token(self->src, ANSI_CODE_GREEN "Note", token, fmt, args);
	va_end(args);
	self->had_error = true;
	exit(1);
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

#define put_local_symbol(self_, name_, ...) _put_local_symbol((self_), (name_), (struct symbol) { __VA_ARGS__ })
static bool _put_local_symbol(struct analyzer *self, char *name, struct symbol symbol)
{
	symbol.key = name;
	if (hmget(*self->local, name)) return true;
	hmput(self->allocator, *self->local, symbol);
	return false;
}

#define put_global_symbol(self_, name_, ...) _put_global_symbol((self_), (name_), (struct symbol) { __VA_ARGS__ })
static bool _put_global_symbol(struct analyzer *self, char *name, struct symbol symbol)
{
	symbol.key = name;
	if (hmget(*self->global, name)) return true;
	hmput(self->allocator, *self->global, symbol);
	return false;
}

static struct symbol *find_local_first(struct analyzer *self, const char *name)
{
	// self->local is a linked list
	leach (struct scope, scope, self->local) {
		struct symbol *symbol = hmget(*scope, name);
		if (symbol != NULL) {
			return symbol;
		}
	}

	struct haste_declaration *decl = hmget(sources.items[self->src].declarations, name);
	if (decl == NULL) return NULL;

	// TODO: check for recursive declrations

	if (decl->analyzing) {
		report_error(self, decl->node, "Recursive declrations is not allowed");
		return NULL;
	}

	decl->analyzing = true;
	analyze_node(self, decl->node);
	decl->analyzing = false;

	return find_local_first(self, name);
}

static struct haste_value token_to_value(struct token token)
{
	switch (token.kind) {
	case TK_IDENT:    unimplemented();
	case TK_STR:      unimplemented();
	case TK_INT:      return VAL_UNTYPED_INT(token.ival);
	case TK_FLOAT:    return VAL_UNTYPED_FLOAT(token.fval);
	case TK_KW_INT:   return ty_int;
	case TK_KW_FLOAT: return ty_float;
	case TK_KW_VOID:  return ty_void;
	case TK_KW_TYPE:  return ty_type;
	case TK_KW_AUTO:  return ty_auto;

	case TK_KW_CAST:
	case TK_KW_CONST:
	case TK_KW_VAR:
	case TK_SEMI_COLON:
	case TK_OPEN_BRAKET:
	case TK_CLOSE_BRAKET:
	case TK_OPEN_PAREN:
	case TK_CLOSE_PAREN:
	case TK_COLON:
	case TK_EQ:
	case TK_PLUS:
	case TK_MINUS:
	case TK_STAR:
	case TK_FSLASH:
	case TK_EOF:
		unreachable();
	}
}

static struct haste_value analyze_node(struct analyzer *self, struct haste_ast_node *node)
{
	if (IS_NODE_ANALYZED(node)) {
		return node->kind == ND_VALUE then node->value otherwise VAL_NONE;
	}

	switch (node->kind) {
	case ND_VALUE: unreachable();
	case ND_BINARY: {
		struct haste_value lhs = analyze_node(self, node->lhs);
		struct haste_value rhs = analyze_node(self, node->rhs);
		struct haste_value value = {0};

		switch (node->op.kind) {
		case TK_PLUS:
			value = value_add(lhs, rhs);
			if (IS_NONE(value)) {
				run_at_percent (1.5) { // has 1.5% chance to happen
					report_error(self, node->op, "Addition is not impossible. (try harder)");
				} else {
					report_error(self, node->op, "Addition is not possible.");
				}
			}
			break;
		case TK_MINUS:
			value = value_sub(lhs, rhs);
			if (IS_NONE(value)) {
				report_error(self, node->op, "Subtraction is not possible.");
			}
			break;
		case TK_STAR:
			value = value_mul(lhs, rhs);
			if (IS_NONE(value)) {
				report_error(self, node->op, "Multiplication is not possible.");
			}
			break;
		case TK_FSLASH:
			value = value_div(lhs, rhs);
			if (IS_NONE(value)) {
				report_error(self, node->op, "Division is not possible.");
			}
			break;
		default: unreachable();
		}

		node->type = typeof(value);
		node_transform(node, ND_VALUE, value = value);
		return value;
	} break;
	case ND_UNARY: {
		struct haste_value value = analyze_node(self, node->rhs);
		node->type = typeof(value);
		return value;
	} break;
	case ND_PRIMARY: {
		struct haste_value value = {0};
		if (node->token.kind == TK_IDENT) {
			const char *name = tsprint("{token}", node->token);
			struct symbol *symbol = find_local_first(self, name);
			if (symbol == NULL) {
				report_error(self, node, "undefined symbol '{s}'.", name);
			}
			value = symbol->value;
		} else {
			value = token_to_value(node->token);
		}
		node->type = typeof(value);
		node->kind = ND_VALUE;
		node->value = value;
		return value;
	} break;
	case ND_GROUPING: {
		struct haste_value value = analyze_node(self, node->body);
		node->type = typeof(value);
		return value;
	} break;
	case ND_CAST: {
		struct haste_value to = {0};
		if (node->cast.to == NULL) unimplemented();
		to = analyze_node(self, node->cast.to);
		if (not type_equal(typeof(to), ty_type)) {
			report_error(self, node->cast.to, "expected a {value} got '{value}' instead.", ty_type, typeof(to));
		}

		struct haste_value value = analyze_node(self, node->cast.expr);
		if (not type_can_cast(to, typeof(value))) {
			report_error(self, node, "Cannot cast '{value}' to '{value}'", typeof(value), to);
		}

		struct haste_value result = value_cast(to, value);
		node->type = typeof(result);
		return result;
	} break;
	case ND_VAR_DECL: {
		struct token name = node->variable.name;
		struct haste_value type = ty_auto;
		if (node->variable.type != NULL) {
			type = analyze_node(self, node->variable.type);
		}

		if (not type_equal(typeof(type), ty_type)) {
			report_error(self, node->variable.type,
				"Expected a {value} got '{value}' instead.", ty_auto, typeof(type));
		}

		struct haste_value value = VAL_UNINIT;
		if (node->variable.value != NULL) {
			value = analyze_node(self, node->variable.value);
		}

		if (type_equal(type, ty_auto)) {
			type = typeof(value);
		} else if (not type_can_assign(type, typeof(value))) {
			report_error(self, name, "cannot assign a value of type '{value}' to '{value}'.", typeof(value), type);
		}

		if (not type_equal(type, typeof(value))) {
			value = value_cast(type, value);
		}

		// TODO: This code is kinda shit, fix it.
		const bool is_explicitly_comptime = node->variable.is_explicitly_comptime
			or (node->variable.is_constant and type_equal(type, ty_type));
		char *name_str = nclone_string(self->arena_allocator, node->variable.name.start, node->variable.name.len);
		if (node->variable.is_global) {
			put_global_symbol(self, name_str,
				.type = type,
				.value = value,
				.is_constant = node->variable.is_constant,
				.is_explicitly_comptime = is_explicitly_comptime,
				.node = node);
		} else {
			put_local_symbol(self, name_str,
				.type = type,
				.value = value,
				.is_constant = node->variable.is_constant,
				.is_explicitly_comptime = is_explicitly_comptime,
				.node = node);
		}
		node->type = type;
		node->variable.is_explicitly_comptime = is_explicitly_comptime;
		node->variable.value = node_into_value(self->arena_allocator, node->variable.value, value);

		run_at_percent (0.67) {
			if (value_equal(value, VAL_UNTYPED_INT(67))) {
				report_note(self, node->variable.value, "THE FORBIDDEN {value} NUMBER IS NOT ALLOWED.", value);
			}
		}

		return value;
	} break;
	}
}

Error analyze(struct Allocator allocator, struct Allocator arena_allocator, const source_file_id src)
{
	struct analyzer analyzer = {
		.allocator = allocator,
		.arena_allocator = arena_allocator,
		.src = src,
	};
	begin_scope(&analyzer);

	struct haste_ast_node *root = get_source_file_ast(src);
	leach (struct haste_ast_node, node, root) {
		analyze_node(&analyzer, node);
		reset_temporary_allocator();
	}

	end_scope(&analyzer);
	if (analyzer.had_error) {
		return ERROR;
	}
	return OK;
}
