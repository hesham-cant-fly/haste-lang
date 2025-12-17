#include "analysis.h"
#include "common.h"
#include "core/my_commons.h"
#include "hir.h"
#include "my_math.h"
#include "tir.h"
#include "token.h"
#include "type.h"
#include "error.h"
#include "core/my_array.h"
#include <assert.h>
#include <linux/limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_MAX_LEN 256

enum HasteValueKind {
	VALUE_KIND_INT,      // int32_t integer;
	VALUE_KIND_FLOAT,    // float floating;
	VALUE_KIND_SYMBOL,   // const char* symbol;
	VALUE_KIND_TYPE,     // TypeID type;
	VALUE_KIND_UNKNOWN,  // TirValue tir_value;
};

struct HasteValue {
	TypeID type;
	enum HasteValueKind tag;
	union {
		int64_t integer;
		double float_literal;
		const char* symbol;
		TypeID type;
		TirValue tir_value;
	} as;
};

static TypeID type_of_value(struct HasteValue self)
{
	return self.type;
}

enum SymbolKind {
	SYMBOL_CONSTANT,
	SYMBOL_VARIABLE,
};

enum SymbolVisibility {
	SYMBOL_PUBLIC,
	SYMBOL_PRIVATE,
};

struct Symbol {
	const char* name;
	enum SymbolKind kind;
	enum SymbolVisibility visibility;
	bool defined;
	struct HasteValue value;
};

static TypeID type_of_symbol(struct Symbol self)
{
	return self.value.type;
}

struct AnalysisStack {
	struct HasteValue *values;
	size_t sp;
};

static struct AnalysisStack init_stack(void);
static void deinit_stack(struct AnalysisStack* self);
static void push_value(struct AnalysisStack* self, const struct HasteValue value);
static struct HasteValue pop_value(struct AnalysisStack* self);

typedef struct Symbol* SymbolTable;

/** @return -1 if not found */
static int32_t find_symbol(const SymbolTable self, const char* name);
static error declare_symbol(
	SymbolTable table,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility);
static error define_symbol(SymbolTable self, const char* name, const struct HasteValue value);

struct Scope {
	SymbolTable symbols;
	struct Scope* parent;
	struct Scope* child;
};

typedef size_t CheckPoint;

struct CheckPointInfo {
	size_t ip;
};

struct Analyzer {
	Tir tir;
	Hir hir;
	const char* path;
	size_t ip;
	struct AnalysisStack stack;
	struct CheckPointInfo *check_points;
	struct Scope global_scope;
	struct Scope* current_scope;
	bool had_error;
};

static struct Analyzer init_analyzer(Hir hir);
static void deinit_analyzer(struct Analyzer* self);
static void analyze_range(struct Analyzer* self, const size_t start, const size_t end);

static CheckPoint start_check_point(struct Analyzer* self);
static void end_check_point(struct Analyzer* self, const CheckPoint check_point);

static void push(struct Analyzer* self, struct HasteValue value);
static struct HasteValue pop(struct Analyzer* self);

static struct HasteValue value_of(struct Analyzer* self, const struct HasteValue value);
static TirConst tirrify_the_value(struct Analyzer* self, struct HasteValue value);
static void tirrify_the_symbol(struct Analyzer* self, struct Symbol* symbol);

static struct Symbol* find_local_first(struct Analyzer* self, const char* name);
static struct Symbol* find_local_only(struct Analyzer* self, const char* name);
static struct Symbol* find_global_first(struct Analyzer* self, const char* name); 
static struct Symbol* find_global_only(struct Analyzer* self, const char* name); 
static error declare_local(
	struct Analyzer* self,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility);
static error define_local(
	struct Analyzer* self,
	const char* name,
	const struct HasteValue value);
static error declare_global(
	struct Analyzer* self,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility);
static error define_global(
	struct Analyzer* self,
	const char* name,
	const struct HasteValue value);
static bool is_defined(struct Analyzer* self, const char* name);
static bool is_declared(struct Analyzer* self, const char* name);

static NORETURN void report_error(struct Analyzer* self, Location location, const char* fmt, ...);

static struct AnalysisStack init_stack(void)
{
	return (struct AnalysisStack) {
		.values = malloc(sizeof(struct HasteValue) * STACK_MAX_LEN),
		.sp = 0,
	};
}

static void deinit_stack(struct AnalysisStack *self)
{
	free(self->values);
}

static void push_value(struct AnalysisStack* self, const struct HasteValue value)
{
	assert(self->sp < STACK_MAX_LEN);

	self->sp += 1;
	self->values[self->sp - 1] = value;
}

static struct HasteValue pop_value(struct AnalysisStack* self)
{
	assert(self->sp > 0);

	self->sp -= 1;
	struct HasteValue value = self->values[self->sp];
	self->values[self->sp] = (struct HasteValue) {0};
	return value;
}

static int32_t find_symbol(const SymbolTable self, const char* name)
{
	const size_t len = arrlen(self);
	if (len == 0) return -1;

	int32_t left = 0;
	int32_t right = len - 1;

	while (left <= right) {
		const int32_t mid = left + (right - left) / 2;
		const struct Symbol symbol = self[mid];
		const int cmp = strcmp(symbol.name, name);

		if (cmp == 0)
			return mid;
		else if (cmp > 0)
			left = mid + 1;
		else
			right = mid - 1;
	}

	return -1;
}

static error declare_symbol(
	SymbolTable table,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility)
{
	const size_t len = arrlen(table);
	arrreserve(table, len + 1);

	ssize_t i = len - 1;
	for (; i >= 0; i -= 1) {
		const struct Symbol symbol = table[i];
		const int cmp_res = strcmp(symbol.name, name);
		if (cmp_res == 0) return ERROR;
		if (cmp_res > 0)  break;

		table[i + 1] = symbol;
	}

	arrsetlen(table, len + 1);
	table[i + 1] = (struct Symbol) {
		.name = name,
		.defined = false,
		.kind = kind,
		.visibility = visibility,
		.value = {0},
	};

	return OK;
}

static error define_symbol(SymbolTable self, const char* name, const struct HasteValue value)
{
	const int32_t index = find_symbol(self, name);
	if (index == -1) return ERROR;

	struct Symbol* const symbol = &self[index];
	if (symbol->defined) return ERROR;

	symbol->defined = true;
	symbol->value = value;

	return OK;
}

static struct Analyzer init_analyzer(Hir hir)
{
	struct Analyzer result = {0};
	result.path = hir.path;
	result.hir = hir;
	result.tir = init_tir(hir.path);
	result.stack = init_stack();
	result.check_points = arrinit(struct CheckPointInfo);
	result.global_scope = (struct Scope){
		.symbols = arrinit(struct Symbol),
		.parent = NULL,
		.child = NULL,
	};

	return result;
}

static void deinit_analyzer_ok(struct Analyzer* self)
{
	deinit_stack(&self->stack);
	arrfree(self->check_points);
	arrfree(self->global_scope.symbols);
}

static void deinit_analyzer_err(struct Analyzer* self)
{
	deinit_analyzer_ok(self);
	deinit_tir(&self->tir);
}

static CheckPoint start_check_point(struct Analyzer* self)
{
	struct CheckPointInfo check_point = {
		.ip = self->ip,
	};
	arrpush(self->check_points, check_point);
	return arrlen(self->check_points) - 1;
}

static void end_check_point(struct Analyzer* self, const CheckPoint check_point)
{
	const size_t top = arrlen(self->check_points) - 1;
	assert(check_point <= top);

	const struct CheckPointInfo info = self->check_points[check_point];
	self->ip = info.ip;

	arrsetlen(self->check_points, check_point);
}

static void push(struct Analyzer* self, struct HasteValue value)
{
	push_value(&self->stack, value);
}

static struct HasteValue pop(struct Analyzer* self)
{
	return pop_value(&self->stack);
}

static struct HasteValue value_of(struct Analyzer* self, const struct HasteValue value)
{
	switch (value.tag) {
		case VALUE_KIND_INT:
		case VALUE_KIND_FLOAT:
		case VALUE_KIND_TYPE:
		case VALUE_KIND_UNKNOWN:
			return value;
		case VALUE_KIND_SYMBOL: {
			const struct Symbol* symbol = find_local_first(self, value.as.symbol);
			return value_of(self, symbol->value);
		}
	}
}

static TirConst tirrify_the_value(struct Analyzer* self, struct HasteValue value)
{
	struct TirConstInfo constant_info = {0};
	constant_info.type = type_of_value(value);
	switch (value.tag) {
		case VALUE_KIND_INT:
			constant_info.kind = TIR_CONST_I32;
			constant_info.as.i32 = value.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			constant_info.kind = TIR_CONST_F32;
			constant_info.as.f32 = value.as.float_literal;
			break;
		case VALUE_KIND_TYPE:
			constant_info.kind = TIR_CONST_TYPE;
			constant_info.as.type = value.as.type;
			break;
		case VALUE_KIND_SYMBOL:
			unreachable();
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	const TirConst constant = tir_push_constant(&self->tir, constant_info);
	return constant;
}

static void tirrify_the_symbol(struct Analyzer* self, struct Symbol* symbol)
{
	const struct HasteValue value = symbol->value;
	TirConstInfo constant_info = {0};
	constant_info.type = type_of_symbol(*symbol);
	switch (value.tag) {
		case VALUE_KIND_INT:
			constant_info.kind = TIR_CONST_I32;
			constant_info.as.i32 = value.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			constant_info.kind = TIR_CONST_F32;
			constant_info.as.f32 = value.as.float_literal;
			break;
		case VALUE_KIND_TYPE:
			constant_info.kind = TIR_CONST_TYPE;
			constant_info.as.type = value.as.type;
			break;
		case VALUE_KIND_SYMBOL:
			unreachable();
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	const TirConst constant = tir_push_constant(&self->tir, constant_info);
	const TirGlobalInfo global = {
		.name = symbol->name,
		.initializer = constant,
		.is_constant = symbol->kind == SYMBOL_CONSTANT,
	};

	tir_push_global(&self->tir, global);
}

static struct Symbol* find_local_first(struct Analyzer* self, const char* name)
{
	struct Scope* current = self->current_scope;

	while (current != NULL) {
		struct Scope* const parent = current->parent;
		const int index = find_symbol(current->symbols, name);
		if (index != -1) {
			return &current->symbols[index];
		}

		current = parent;
	}

	return NULL;
}

static struct Symbol* find_local_only(struct Analyzer* self, const char* name)
{
	struct Scope* current = self->current_scope;

	const int index = find_symbol(current->symbols, name);
	if (index != -1) return NULL;

	return &current->symbols[index];
}

static struct Symbol* find_global_first(struct Analyzer* self, const char* name)
{
	struct Scope* current = &self->global_scope;

	while (current != NULL) {
		struct Scope* const child = current->child;
		const int index = find_symbol(current->symbols, name);
		if (index != -1) {
			return &current->symbols[index];
		}

		current = child;
	}

	return NULL;
}

static struct Symbol* find_global_only(struct Analyzer* self, const char* name)
{
	struct Scope current = self->global_scope;

	const int index = find_symbol(current.symbols, name);
	if (index == -1) return NULL;

	return &current.symbols[index];
}

static error declare_local(
	struct Analyzer* self,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility)
{
	struct Scope* current = self->current_scope;
	const error err = declare_symbol(current->symbols, name, kind, visibility);
	return err;
}

static error define_local(struct Analyzer* self, const char* name, const struct HasteValue value)
{
	struct Scope* current = self->current_scope;
	const error err = define_symbol(current->symbols, name, value);
	return err;
}

static error declare_global(
	struct Analyzer* self,
	const char* name,
	const enum SymbolKind kind,
	const enum SymbolVisibility visibility)
{
	struct Scope current = self->global_scope;
	const error err = declare_symbol(current.symbols, name, kind, visibility);
	return err;
}

static error define_global(struct Analyzer* self, const char* name, const struct HasteValue value)
{
	struct Scope current = self->global_scope;
	const  error err = define_symbol(current.symbols, name, value);
	return err;
}

static bool is_defined(struct Analyzer* self, const char* name)
{
	struct Symbol* symbol = find_local_first(self, name);
	if (symbol == NULL) {
		return false;
	}
	return symbol->defined;
}

static bool is_declared(struct Analyzer* self, const char* name)
{
	struct Symbol* symbol = find_local_first(self, name);
	return symbol != NULL;
}

static NORETURN void report_error(
	struct Analyzer* self,
	Location location,
	const char* fmt, ...
) {
	va_list args;
	va_start(args, fmt);
	vreport(stderr, self->path, location, "Error", fmt, args);
	va_end(args);
	self->had_error = true;
	// TODO:
	panic("");
}

static void analyze_instruction(struct Analyzer* self, const HirInstruction instruction);

static void analyze_global(struct Analyzer* self, const struct HirGlobal global)
{
	if (is_declared(self, global.name)) {
		report_error(self, global.location, "'%s' is already defined!", global.name);
		return;
	}

	enum SymbolVisibility visibility = SYMBOL_PUBLIC;
	if (global.visibility == HIR_PRIVATE) visibility = SYMBOL_PRIVATE;

	enum SymbolKind symbol_kind = SYMBOL_CONSTANT;
	
	switch (global.kind) {
		case HIR_DECL_VAR:
			symbol_kind = SYMBOL_VARIABLE;
		case HIR_DECL_CONST: {
			error err = declare_global(self, global.name, symbol_kind, visibility);
			if (err) panic("TODO:");

			const size_t len = arrlen(global.instructions);
			for (size_t i=0; i < len; i += 1) {
				const struct HirInstruction instruction = global.instructions[i];
				analyze_instruction(self, instruction);
			}

			TypeID type = TYPE_AUTO;
			if (global.explicit_typing) {
				const struct HasteValue type_value = pop(self);
				const struct HasteValue actual_type = value_of(self, type_value);
				if (actual_type.tag != VALUE_KIND_TYPE) {
					report_error(
						self, 
						global.location,
						"No known type has been provided.");
				}
				type = actual_type.as.type;
			}

			struct HasteValue value = {0};
			if (global.initialized) {
				const struct HasteValue avalue = pop(self);
				const struct HasteValue actual_value = value_of(self, avalue);
				const TypeID value_type = type_of_value(actual_value);
				const enum TypeMatchResult match_result = type_matches(type, value_type);
				if (match_result != TYPE_MATCH_EXACT) {
					report_error(
						self,
						global.location,
						"Type mismatch.");
				}

				value = actual_value;
			}

			const TirConst tir_constant = tirrify_the_value(self, value);
			const struct TirGlobalInfo tir_global_info = {
				.name = global.name,
				.is_constant = global.kind == HIR_DECL_CONST,
				.initializer = tir_constant,
			};

			tir_push_global(&self->tir, tir_global_info);

			err = define_global(self, global.name, value);
			if (err) panic("TODO:");

			break;
		}
	}
}

static void analyze_identifier(struct Analyzer* self, const HirInstruction instruction)
{
	const char* identifier = instruction.as.identifier;
	const struct Symbol* symbol = find_local_first(self, identifier);
	if (symbol == NULL) {
		struct HirGlobal* global = hir_find_global(self->hir.globals, identifier);
		if (global == NULL) {
			report_error(
				self,
				instruction.location,
				"Undefined Symbol `%s`",
				identifier);
		}

		analyze_global(self, *global);
		global->visited = true;

		symbol = find_global_only(self, identifier);
	}

	if (!symbol->defined) {
		report_error(
			self,
			instruction.location,
			"Dependency loop has been detected in '%s'",
			identifier);
	}

	const struct HasteValue value = value_of(self, symbol->value);
	
	push(self, value);
}


static void analyze_integer(struct Analyzer* self, const HirInstruction instruction)
{
	const int64_t integer = instruction.as.integer;
	const struct HasteValue value = {
		.type = TYPE_UNTYPED_INT,
		.tag = VALUE_KIND_INT,
		.as = {
			.integer = integer,
		},
	};

	push(self, value);
}

static void analyze_float(struct Analyzer* self, const HirInstruction instruction)
{
	const double floating = instruction.as.floating_point;
	const struct HasteValue value = {
		.type = TYPE_UNTYPED_FLOAT,
		.tag = VALUE_KIND_FLOAT,
		.as = {
			.float_literal = floating,
		},
	};

	push(self, value);
}

static void analyze_type(struct Analyzer* self, const HirInstruction instruction)
{
	const TypeID type = instruction.as.type;
	const struct HasteValue value = {
		.type = TYPE_TYPEID,
		.tag = VALUE_KIND_TYPE,
		.as = {
			.type = type,
		},
	};

	push(self, value);
}

static void analyze_unary_plus(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue value = pop(self);

	if (!type_is_numiric(value.type))
		panic("Can't do a unary operator on a non numiric value.");

	switch (value.tag) {
		case VALUE_KIND_INT:
		case VALUE_KIND_FLOAT:
			break;
		case VALUE_KIND_SYMBOL:
		case VALUE_KIND_TYPE:
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	push(self, value);
}

static void analyze_unary_minus(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue value = pop(self);

	if (!type_is_numiric(value.type)) {
		panic("Can't do a unary operator on a non numiric value.");
	}

	struct HasteValue result = value;
	switch (value.tag) {
		case VALUE_KIND_INT:
			result.as.integer = -result.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			result.as.float_literal = -result.as.float_literal;
			break;
		case VALUE_KIND_SYMBOL:
		case VALUE_KIND_TYPE:
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	push(self, result);
}

static error check_binary_op(struct Analyzer* self, struct HasteValue a, struct HasteValue b)
{
	unused(self);
	const TypeID a_type = type_of_value(a);
	const TypeID b_type = type_of_value(b);

	if (!(type_is_numiric(a_type) && type_is_numiric(b_type))) {
		panic("Cannot do binary operation on a non numiric type.");
		return ERROR;
	}

	const TypeMatchResult match = type_matches(a_type, b_type);
	if (match != TYPE_MATCH_EXACT) {
		panic("Type mismatch.");
		return ERROR;
	}

	return OK;
}

static struct HasteValue add(struct Analyzer* self, const struct HasteValue a, const struct HasteValue b)
{
	unused(self);
	struct HasteValue result = a;
	switch (b.tag) {
		case VALUE_KIND_INT:
			result.as.integer += b.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			result.as.float_literal += b.as.float_literal;
			break;
		case VALUE_KIND_SYMBOL:
			unreachable();
		case VALUE_KIND_TYPE:
			unreachable();
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	return result;
}

static struct HasteValue subtract(struct Analyzer* self, const struct HasteValue a, const struct HasteValue b)
{
	unused(self);
	struct HasteValue result = a;
	switch (b.tag) {
		case VALUE_KIND_INT:
			result.as.integer -= b.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			result.as.float_literal -= b.as.float_literal;
			break;
		case VALUE_KIND_SYMBOL:
			unreachable();
		case VALUE_KIND_TYPE:
			unreachable();
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}

	return result;
}

static struct HasteValue multiply(struct Analyzer* self, const struct HasteValue a, const struct HasteValue b)
{
	unused(self);
	struct HasteValue result = a;
	switch (b.tag) {
		case VALUE_KIND_INT:
			result.as.integer *= b.as.integer;
			break;
		case VALUE_KIND_FLOAT:
			result.as.float_literal *= b.as.float_literal;
			break;
		case VALUE_KIND_SYMBOL:
		case VALUE_KIND_TYPE:
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}
	return result;
}

static struct HasteValue divide(struct Analyzer* self, const struct HasteValue a, const struct HasteValue b)
{
	unused(self);
	struct HasteValue result = a;
	switch (b.tag) {
	case VALUE_KIND_INT:
		result.as.integer /= b.as.integer;
		break;
	case VALUE_KIND_FLOAT:
		result.as.float_literal /= b.as.float_literal;
		break;
	case VALUE_KIND_SYMBOL:
	case VALUE_KIND_TYPE:
	case VALUE_KIND_UNKNOWN:
		unreachable();
	}
	return result;
}

static struct HasteValue power(struct Analyzer* self, const struct HasteValue a, const struct HasteValue b)
{
	unused(self);
	struct HasteValue result = a;
	switch (b.tag) {
		case VALUE_KIND_INT:
			result.as.integer = powi64(result.as.integer, b.as.integer);
			break;
		case VALUE_KIND_FLOAT:
			result.as.float_literal = pow(result.as.float_literal, b.as.float_literal);
			break;
		case VALUE_KIND_SYMBOL:
		case VALUE_KIND_TYPE:
		case VALUE_KIND_UNKNOWN:
			unreachable();
	}
	return result;
}

static void analyze_binary_add(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue b = pop(self);
	const struct HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const struct HasteValue a_value = value_of(self, a);
	const struct HasteValue b_value = value_of(self, b);
	const struct HasteValue result = add(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_sub(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue b = pop(self);
	const struct HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const struct HasteValue a_value = value_of(self, a);
	const struct HasteValue b_value = value_of(self, b);
	const struct HasteValue result = subtract(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_mul(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue b = pop(self);
	const struct HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const struct HasteValue a_value = value_of(self, a);
	const struct HasteValue b_value = value_of(self, b);
	const struct HasteValue result = multiply(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_div(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue b = pop(self);
	const struct HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const struct HasteValue a_value = value_of(self, a);
	const struct HasteValue b_value = value_of(self, b);
	const struct HasteValue result = divide(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_pow(struct Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const struct HasteValue b = pop(self);
	const struct HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const struct HasteValue a_value = value_of(self, a);
	const struct HasteValue b_value = value_of(self, b);
	const struct HasteValue result = power(self, a_value, b_value);

	push(self, result);
}

static void analyze_constant_declaration(struct Analyzer* self, const HirInstruction instruction)
{
	const HirConstantDecl constant = instruction.as.constant;

	if (!(constant.explicit_typing || constant.initialized)) {
		panic("no value, no initializatio. how tf I'm gonna tell the type?!");
	}

	TypeID type = TYPE_AUTO;
	if (constant.explicit_typing) {
		const struct HasteValue type_value = pop(self);
		const struct HasteValue actual_value = value_of(self, type_value);
		if (actual_value.tag != VALUE_KIND_TYPE) {
			panic("Not a type!");
		}

		type = actual_value.as.type;
	}

	if (constant.initialized) {
		const struct HasteValue value = pop(self);
		const TypeID value_type = type_of_value(value);
		const TypeMatchResult match_result = type_matches(type, value_type);
		if (match_result != TYPE_MATCH_EXACT) {
			panic("Type mismatch.");
		}

		const error err = define_local(self, constant.name, value);
		if (err) panic("IDK");
		tirrify_the_symbol(self, find_local_first(self, constant.name));
	}
}

static void analyze_variable_declaration(struct Analyzer* self, const HirInstruction instruction)
{
	const HirVariableDecl variable = instruction.as.variable;

	if (!variable.explicit_typing && !variable.initialized) {
		panic("no value, no initializatio. how tf I'm gonna tell the tell?!");
	}

	TypeID type = {0};
	if (variable.explicit_typing) {
		const struct HasteValue type_value = pop(self);
		if (type_value.tag != VALUE_KIND_TYPE) {
			panic("Not a type!");
		}

		type = type_value.as.type;
	}

	if (variable.initialized) {
		const struct HasteValue value = pop(self);
		const TypeID value_type = type_of_value(value);
		const TypeMatchResult match_result = type_matches(type, value_type);
		if (match_result != TYPE_MATCH_EXACT) {
			panic("Type mismatch.");
		}

		const error err = define_local(self, variable.name, value);
		if (err) panic("IDK");
		tirrify_the_symbol(self, find_local_first(self, variable.name));
	}
}

static void analyze_instruction(struct Analyzer* self, const HirInstruction instruction)
{
	switch (instruction.tag) {
		case HIR_NODE_END:
			return;
		case HIR_NODE_IDENTIFIER:
			analyze_identifier(self, instruction);
			break;
		case HIR_NODE_INTEGER:
			analyze_integer(self, instruction);
			break;
		case HIR_NODE_FLOAT:
			analyze_float(self, instruction);
			break;
		case HIR_NODE_TYPE:
			analyze_type(self, instruction);
			break;
		case HIR_NODE_UNARY_PLUS:
			analyze_unary_plus(self, instruction);
			break;
		case HIR_NODE_UNARY_MINUS:
			analyze_unary_minus(self, instruction);
			break;
		case HIR_NODE_ADD:
			analyze_binary_add(self, instruction);
			break;
		case HIR_NODE_SUB:
			analyze_binary_sub(self, instruction);
			break;
		case HIR_NODE_MUL:
			analyze_binary_mul(self, instruction);
			break;
		case HIR_NODE_DIV:
			analyze_binary_div(self, instruction);
			break;
		case HIR_NODE_POW:
			analyze_binary_pow(self, instruction);
			break;
		case HIR_NODE_CONSTANT_DECLARATION:
			analyze_constant_declaration(self, instruction);
			break;
		case HIR_NODE_VARIABLE_DECLARATION:
			analyze_variable_declaration(self, instruction);
			break;
	}
}

static void start_analyzing(struct Analyzer* self)
{
	const size_t len = arrlen(self->hir.globals);
	for (size_t i=0; i < len; i += 1) {
		HirGlobal* global = &self->hir.globals[i];
		if (global->visited) continue;
		analyze_global(self, *global);
		global->visited = true;
	}
	return;
}

error analyze_hir(Hir hir, Tir* out)
{
	struct Analyzer analyzer = init_analyzer(hir);
	analyzer.current_scope = &analyzer.global_scope;

	start_analyzing(&analyzer);

	if (analyzer.had_error) {
		deinit_analyzer_err(&analyzer);
		return ERROR;
	}

	*out = analyzer.tir;
	deinit_analyzer_ok(&analyzer);
	return OK;
}
