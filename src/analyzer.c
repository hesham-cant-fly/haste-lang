#include "analyzer.h"
#include "ast.h"
#include "common.h"
#include "converters.h"
#include "core/my_commons.h"
#include "error.h"
#include "token.h"
#include "type.h"
#include "core/my_array.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define is_used(symbol__) ((symbol__).name.start != NULL)

static struct Value poisoned_value = {0};

static uint64_t hash_span(const struct Span s)
{
	const unsigned char *p = (const unsigned char *)s.start;
	size_t len = (size_t)(s.end - s.start);

	uint64_t h = 1469598103934665603ull;
	for (size_t i = 0; i < len; i++) {
		h ^= p[i];
		h *= 1099511628211ull;
	}
	return h;
}

static bool span_eq(const struct Span a, const struct Span b)
{
	size_t len = (size_t)(a.end - a.start);
	return len == (size_t)(b.end - b.start) && memcmp(a.start, b.start, len) == 0;
}

static int clzll(uint64_t x)
{
	int n = 0;
	while (!(x & (1ull << 63))) {
		n++;
		x <<= 1;
	}
	return n;
}

static void symboltable_init(struct SymbolTable *m, size_t cap)
{
	if (cap < 16) cap = 16;
	cap = 1ull << (64 - clzll(cap - 1)); // power of two

	m->len = 0;
	m->cap = cap;
	m->items = calloc(cap, sizeof(*m->items));
}

static struct Symbol *symboltable_put(struct SymbolTable *m, struct Symbol sym);
static void symboltable_grow(struct SymbolTable *m)
{
	struct Symbol *old = m->items;
	const size_t old_cap = m->cap;

	symboltable_init(m, old_cap * 2);

	for (size_t i = 0; i < old_cap; i++) {
		if (is_used(old[i])) {
			symboltable_put(m, old[i]);
		}
	}

	free(old);
}

static struct Symbol *symboltable_get(struct SymbolTable *m, const struct Span key)
{
	if (!m->len) return NULL;

	uint64_t hash = hash_span(key);
	size_t mask = m->cap - 1;
	size_t idx = hash & mask;
	size_t dist = 0;

	for (;;) {
		struct Symbol *symbol = &m->items[idx];
		if (!is_used(*symbol)) return NULL;

		size_t edist = (idx - (symbol->hash & mask)) & mask;
		if (edist < dist) return NULL;

		if (symbol->hash == hash && span_eq(symbol->name, key))
			return symbol;

		idx = (idx + 1) & mask;
		dist++;
	}
}

static struct Symbol *symboltable_put(struct SymbolTable *m, struct Symbol sym)
{
	if ((m->len + 1) * LOAD_FACTOR_DEN > m->cap * LOAD_FACTOR_NUM)
		symboltable_grow(m);

	uint64_t hash = hash_span(sym.name);
	size_t mask = m->cap - 1;
	size_t idx = hash & mask;
	size_t dist = 0;

	for (;;) {
		struct Symbol *e = &m->items[idx];

		if (!is_used(*e)) {
			*e = sym;
			e->hash = hash;
			m->len++;
			return e;
		}

		if (e->hash == hash && span_eq(e->name, sym.name)) {
			*e = sym;
			return e;
		}

		const size_t edist = (idx - (e->hash & mask)) & mask;
		if (edist < dist) {
			// Robin Hood swap
			struct Symbol tmp = *e;
			*e = sym;
			e->hash = hash;

			hash = tmp.hash;
			sym  = tmp;
			dist = edist;
		}

		idx = (idx + 1) & mask;
		dist++;
	}
}

static struct Scope *begin_scope(struct Environment *self)
{
	struct Scope *scope = malloc(sizeof(struct Scope));
	*scope = (struct Scope){0};

	if (self->global == NULL) {
		assert(self->current == NULL);
		self->global = scope;
		self->current = scope;
		return scope;
	}

	scope->parent = self->current;
	self->current->child = scope;
	self->current = scope;
	return scope;
}

static void end_scope(struct Environment *self)
{
	struct Scope *current = NULL;
	current->parent->child = NULL;
	self->current = current->parent;
	arrfree(current->symbols);
	free(current);
}

static void declare_local(struct Environment *self, struct Span name, struct Value value, bool is_constant, bool is_param)
{
	assert(self->current != NULL);

	struct Symbol symbol = {
		.name = name,
		.value = value,
		.is_constant = is_constant,
		.is_param = is_param,
		.state = SYMBOL_DECLARED,
	};

	symboltable_put(&self->current->symbols, symbol);
}

static void define_local(struct Environment *self, struct Span name, struct Value value)
{
	assert(self->current != NULL);

	struct Symbol *symbol = symboltable_get(&self->current->symbols, name);
	symbol->state = SYMBOL_DEFINED;
	symbol->value = value;
}

static struct Symbol *find_local_first(struct Environment env, const struct Span name)
{
	struct Scope *current = env.current;
	if (current == NULL) {
		return NULL;
	}

	while (current != NULL) {
		struct Scope *parent = current->parent;
		struct Symbol *symbol = symboltable_get(&current->symbols, name);
		if (symbol != NULL) {
			return symbol;
		}

		if (current->is_function) break;
		current = parent;
	}

	struct Symbol *symbol = symboltable_get(&env.global->symbols, name);
	return symbol;
}

static bool is_defined(struct Environment env, const struct Span name)
{
	struct Symbol *symbol = find_local_first(env, name);
	return symbol != NULL;
}

static void report_error(
	struct Environment *env,
	struct Span span,
	struct Location at,
	const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);
	vreport(stderr, env->path, span, at, "Error", fmt, args);
	va_end(args);

	env->had_error = true;
	env->errors_count += 1;
}

static void report_warning( 
	struct Environment *env,
	struct Span span,
	struct Location at,
	const char *fmt, ...) {

	va_list args;
	va_start(args, fmt);
	vreport(stderr, env->path, span, at, "Warning", fmt, args);
	va_end(args);

	env->had_error = true;
	env->warnings_count += 1;
}

void init_environment(struct Environment *env, const char *path, Arena *allocator)
{
	*env = (struct Environment){0};
	env->path = path;
	env->allocator = allocator;
	begin_scope(env);
}

void deinit_environment(struct Environment *env)
{
	struct Scope *current = env->global;
	while (current != NULL) {
		struct Scope *child = current->child;
		arrfree(current->symbols);
		free(current);
		current = child;
	}

	*env = (struct Environment) {0};
}

void print_value(FILE *f, struct Value value)
{
	if (!value_is_comptime(value)) {
		expr_print(value.as.unknown, f);
		return;
	}

	switch (value.kind) {
	case VALUE_INT:
		fprintf(f, "%d", value.as.integer);
		break;
	case VALUE_FLT32:
		fprintf(f, "%f", value.as.float32);
		break;
	case VALUE_FLT64:
		fprintf(f, "%lf", value.as.float64);
		break;
	case VALUE_TYPEID:
		print_type(f, value.as.type);
		break;
	default:
		unreachable();
	}
}

struct AstExpr value_into_ast_expr(Arena *allocator, struct Value value)
{
	if (value.kind == VALUE_UNKNOWN) {
		return value.as.unknown;
	}

	struct AstValueExpr *value_expr = arena_alloc(allocator, sizeof(struct AstValueExpr));
	*value_expr = (struct AstValueExpr){0};
	value_expr->base.type = value.type;
	value_expr->value = value;
	return make_expr(AstValueExpr, &value_expr->base);
}

struct Value value_from_ast_expr(struct AstExpr expr)
{
	if (expr.v_table == &AstValueExpr) {
		return ((struct AstValueExpr*)expr.data)->value;
	}

	return (struct Value) {
		.kind = VALUE_UNKNOWN,
		.location = expr.data->location,
		.type = expr.data get type,
		.span = expr.data->span,
		.as.unknown = expr,
	};
}

struct Value value_binary_op(struct Environment *env,
							 struct AstBinaryOperator op,
							 struct Value value_a,
							 struct Value value_b)
{
#define X(dist_, a_, b_) \
	switch (op.op) { \
	case AST_BIN_ADD: (dist_) = (a_) + (b_); break; \
	case AST_BIN_SUB: (dist_) = (a_) - (b_); break; \
	case AST_BIN_MUL: (dist_) = (a_) * (b_); break; \
	case AST_BIN_DIV: (dist_) = (a_) / (b_); break; \
	case AST_BIN_POW: unreachable(); \
	}

	if (!value_is_numiric(value_a)) {
		report_error(env,
					 value_a.span,
					 value_a.location,
					 "Value is not a number.");
		return poisoned_value;
	}
	if (!value_is_numiric(value_b)) {
		report_error(env,
					 value_b.span,
					 value_b.location,
					 "Value is not a number.");
		return poisoned_value;
	}
	
	if (value_is_comptime(value_a) and value_is_comptime(value_b)) {
		struct Value res = {
			.location = value_a.location,
			.span = span_conjoin(value_a.span, value_b.span),
		};

		if (value_a.kind == VALUE_FLT32 or value_b.kind == VALUE_FLT32) {
			res.kind = VALUE_FLT32;
			res.type = value_a.type;
			X(res.as.float32,
			  value_into_float32(value_a),
			  value_into_float32(value_b));
			return res;
		}
		
		if (value_a.kind == VALUE_FLT64 or value_b.kind == VALUE_FLT64) {
			res.kind = VALUE_FLT64;
			res.type = value_a.type;
			X(res.as.float64,
			  value_into_float64(value_a),
			  value_into_float64(value_b));
			return res;
		}

		if (value_a.kind == VALUE_INT or value_b.kind == VALUE_INT) {
			res.kind = VALUE_INT;
			res.type = value_a.type;
			X(res.as.integer,
			  value_into_int64(value_a),
			  value_into_int64(value_b));
			return res;
		}

		unreachable();
	}
#undef X

	struct AstBinaryExpr *expr = arena_alloc(env->allocator, sizeof(struct AstBinaryExpr));
	expr->base.location = value_a.location;
	expr->base.span = span_conjoin(value_a.span, value_b.span);
	expr->base.type = value_a.type;
	expr->op = op;
	expr->lhs = value_into_ast_expr(env->allocator, value_a);
	expr->rhs = value_into_ast_expr(env->allocator, value_b);

	return value_from_ast_expr(make_expr(AstBinaryExpr, &expr->base));
}

float value_into_float32(struct Value value)
{
	switch (value.kind) {
	case VALUE_INT:   return (float) value.as.integer;
	case VALUE_FLT32: return value.as.float32;
	case VALUE_FLT64: return (float) value.as.float64;
	default:          unreachable();
	}
}

double value_into_float64(struct Value value)
{
	switch (value.kind) {
	case VALUE_INT:   return (double) value.as.integer;
	case VALUE_FLT32: return (double) value.as.float32;
	case VALUE_FLT64: return value.as.float64;
	default:          unreachable();
	}
}

int64_t value_into_int64(struct Value value)
{
	switch (value.kind) {
	case VALUE_INT:   return value.as.integer;
	case VALUE_FLT32: return (int64_t) value.as.float32;
	case VALUE_FLT64: return (int64_t) value.as.float64;
	default:          unreachable();
	}
}

bool value_is_comptime(const struct Value value)
{
	return value.kind != VALUE_UNKNOWN;
}

bool value_is_numiric(const struct Value value)
{
	const enum ValueKind kind = value.kind;
	const TypeID type = value.type;
	if (kind == VALUE_INT) return true;
	if (kind == VALUE_FLT32) return true;
	if (kind == VALUE_FLT64) return true;
	return type_is_numiric(type);
}

bool value_is_typed(const struct Value value)
{
	return type_is_untyped(value.type);
}

/* Expr */
struct Value analyze_binary_expr(struct AstNodeBase *node, struct Environment *env)
{
	struct AstBinaryExpr *self = (void*)node;
	struct Value lhs = analyze_ast_expr(self->lhs, env);
	struct Value rhs = analyze_ast_expr(self->rhs, env);

	if (is_poisoned(rhs) || is_poisoned(lhs)) {
		node->type = TYPE_POISONED;
		return poisoned_value;
	}

	const struct AstBinaryOperator op = self get op;
	const struct Value result = value_binary_op(env, op, lhs, rhs);
	if (is_poisoned(result)) {
		node->type = TYPE_POISONED;
		return poisoned_value;
	}

	node->type = result.type;
	return result;
}

struct Value analyze_unary_expr(struct AstNodeBase *node, struct Environment *env)
{
	struct AstUnaryExpr *self = (void*)node;
	struct Value rhs = analyze_ast_expr(self->rhs, env);

	if (is_poisoned(rhs)) {
		node->type = TYPE_POISONED;
		return poisoned_value;
	}

	if (!type_is_numiric(rhs.type)) {
		report_error(env,
					 self->rhs.data->span,
					 self->rhs.data->location,
					 "The right operand expected to be a numeric type.");
		node->type = TYPE_POISONED;
		return poisoned_value;
	}

	node->type = rhs.type;
	return rhs;
}

struct Value analyze_grouping_expr(struct AstNodeBase *node, struct Environment *env)
{
	struct AstGroupingExpr *self = (void*)node;

	struct Value result = analyze_ast_expr(self->child, env);
	node->type = result.type;
	return result;
}

struct Value analyze_identifier_expr(struct AstNodeBase *node, struct Environment *env)
{
	struct AstIdentifierExpr *self = (void*)node;
	struct Span name = self->token.span;

	struct Symbol *symbol = find_local_first(*env, name);
	if (symbol == NULL) {
		report_error(env,
					 name,
					 self->token.location,
					 "Undefined Symbol: `%.*s`.", SPAN_ARG(name));
		node->type = TYPE_POISONED;
		return poisoned_value;
	}

	TypeID type = get_symbol_type(*symbol);
	node->type = type;
	return symbol->value;
}

struct Value analyze_literal_expr(struct AstNodeBase *node, struct Environment *env)
{
	unused(env);
	struct AstLiteralExpr *self = (void*)node;
	struct Token token = self->token;
	switch (token.kind) {
	case TOKEN_KIND_TYPEID:
		node->type = TYPE_TYPEID;
		return VALUE(.kind = VALUE_TYPEID,
					 .type = TYPE_TYPEID,
					 .as.type = TYPE_TYPEID);
	case TOKEN_KIND_INT:
		node->type = TYPE_TYPEID;
		return VALUE(.kind = VALUE_TYPEID,
					 .type = TYPE_TYPEID,
					 .as.type = TYPE_INT);
	case TOKEN_KIND_FLOAT:
		node->type = TYPE_TYPEID;
		return VALUE(.kind = VALUE_FLT32,
					 .type = TYPE_TYPEID,
					 .as.type = TYPE_FLOAT);
	case TOKEN_KIND_AUTO:
		node->type = TYPE_TYPEID;
		return VALUE(.kind = VALUE_TYPEID,
					 .type = TYPE_TYPEID,
					 .as.type = TYPE_AUTO);
	case TOKEN_KIND_INT_LIT: {
		node->type = TYPE_UNTYPED_INT;
		int64_t value = 0;
		const error err = strn_to_i64(token.span.start, span_len(token.span), 10, &value);
		if (err) panic("Unreachable.");
		return VALUE(.kind = VALUE_INT,
					 .type = TYPE_UNTYPED_INT,
					 .as = { .integer = value });
	}
	case TOKEN_KIND_FLOAT_LIT: {
		node->type = TYPE_UNTYPED_FLOAT;
		double value = 0.f;
		const error err = strn_to_double(token.span.start, span_len(token.span), &value);
		if (err) panic("Unreachable.");
		return VALUE(.kind = VALUE_FLT32,
					 .type = TYPE_UNTYPED_FLOAT,
					 .as.float32 = value);
	}
	case TOKEN_KIND_IDENTIFIER:
		panic("OK wtf?!");
	default:
		panic("Unreachable: %d\n", token.kind);
	}
}

/* Declarations */
struct Value analyze_const_decl(struct AstNodeBase *node, struct Environment *env)
{
	struct AstConstDeclaration *self = (void*)node;

	if (is_defined(*env, self->name.span)) {
		report_error(env,
					 self get name.span,
					 self get name.location,
					 "`%.*s` is already declared.", SPAN_ARG(self get name.span));
		return poisoned_value;
	}

	struct Value excepected_type = VALUE(.type = TYPE_TYPEID, .as.type = TYPE_AUTO);
	if (!ast_is_none(self->type)) {
		excepected_type = analyze_ast_expr(self->type, env);
		if (is_poisoned(excepected_type)) return poisoned_value;
		if (excepected_type.type != TYPE_TYPEID) {
			node->type = TYPE_POISONED;
			report_error(env,
						 self->type.data->span,
						 self->type.data->location,
						 "Expected a type. got `%s` instead.",
						 type_str(excepected_type.type));
			return poisoned_value;
		}
	}

	declare_local(env, self->name.span, VALUE(.type = excepected_type.as.type), true, false);
	struct Value value = {0};
	if (!ast_is_none(self->value)) {
		value = analyze_ast_expr(self->value, env);
		if (is_poisoned(value)) {
			node->type = TYPE_POISONED;
			return poisoned_value;
		}

		const enum TypeMatchResult match = type_matches(excepected_type.as.type, value.type);
		switch (match) {
		case TYPE_MATCH_NONE:
			node->type = TYPE_POISONED;
			report_error(env,
						 self->value.data->span,
						 self->value.data->location,
						 "the type `%s` is not compatible with `%s`.",
						 type_str(value.type), type_str(excepected_type.as.type));
			return poisoned_value;
		case TYPE_MATCH_EXACT:
			break;
		case TYPE_MATCH_NEED_CAST:
			node->type = TYPE_POISONED;
			report_error(env,
						 self->value.data->span,
						 self->value.data->location,
						 "These types are not compatible. however, casting `%s` to `%s` is possible.",
						 type_str(value.type), type_str(excepected_type.as.type));
			return poisoned_value;
		}
	}

	if (excepected_type.as.type != TYPE_AUTO) {
		value.type = excepected_type.as.type;
	}
	define_local(env, self->name.span, value);

	node->type = value.type;
	self->value = value_into_ast_expr(env->allocator, value);
	return value;
}

struct Value analyze_var_decl(struct AstNodeBase *node, struct Environment *env)
{
	unused(node);
	unused(env);
	panic("Unimplmented.");
}
