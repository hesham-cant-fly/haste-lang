#include "analysis.h"
#include "core/my_commons.h"
#include "hir.h"
#include "my_math.h"
#include "tir.h"
#include "type.h"
#include "error.h"
#include "core/my_array.h"
#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STACK_MAX_LEN 256

typedef enum HasteValueKind {
	VALUE_KIND_INT,      // int32_t integer;
	VALUE_KIND_FLOAT,    // float floating;
	VALUE_KIND_SYMBOL,   // const char* symbol;
	VALUE_KIND_TYPE,     // TypeID type;
	VALUE_KIND_UNKNOWN,  // TirValue tir_value;
} HasteValueKind;

typedef struct HasteValue {
	TypeID type;
	HasteValueKind tag;
	union {
		int64_t integer;
		double float_literal;
		const char* symbol;
		TypeID type;
		TirValue tir_value;
	} as;
} HasteValue;

static TypeID type_of_value(HasteValue self)
{
	return self.type;
}

typedef enum SymbolKind {
	SYMBOL_CONSTANT,
	SYMBOL_VARIABLE,
} SymbolKind;

typedef enum SymbolVisibility {
	SYMBOL_PUBLIC,
	SYMBOL_PRIVATE,
} SymbolVisibility;

typedef struct Symbol {
	const char* name;
	SymbolKind kind;
	SymbolVisibility visibility;
	bool defined;
	HasteValue value;
} Symbol;

static TypeID type_of_symbol(Symbol self)
{
	return self.value.type;
}

typedef struct AnalysisStack {
	HasteValue *values;
	size_t sp;
} AnalysisStack;

static AnalysisStack init_stack(void);
static void deinit_stack(AnalysisStack* self);
static void push_value(AnalysisStack* self, const HasteValue value);
static HasteValue pop_value(AnalysisStack* self);

typedef Symbol* SymbolTable;

/** @return -1 if not found */
static int32_t find_symbol(const SymbolTable self, const char* name);
static error declare_symbol(SymbolTable table, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility);
static error define_symbol(SymbolTable self, const char* name, const HasteValue value);

typedef struct Scope {
	SymbolTable symbols;
	struct Scope* parent;
	struct Scope* child;
} Scope;

typedef struct Analyzer {
	Tir tir;
	Hir hir;
	size_t current_instruction;
	AnalysisStack stack;
	Scope global_scope;
	Scope* current_scope;
	bool had_error;
} Analyzer;

static Analyzer init_analyzer(Hir hir);
static void deinit_analyzer(Analyzer* self);

static size_t hir_len(const Analyzer* self);
static HirInstruction peek(const Analyzer* self);
static HirInstruction advance(Analyzer* self);
static bool ended(const Analyzer* self);

static void push(Analyzer* self, HasteValue value);
static HasteValue pop(Analyzer* self);

static HasteValue value_of(Analyzer* self, const HasteValue value);
static void tirrify_the_symbol(Analyzer* self, Symbol* symbol);

static Symbol* find_local_first(Analyzer* self, const char* name);
static Symbol* find_local_only(Analyzer* self, const char* name);
static Symbol* find_global_first(Analyzer* self, const char* name); 
static Symbol* find_global_only(Analyzer* self, const char* name); 
static error declare_local(Analyzer* self, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility);
static error define_local(Analyzer* self, const char* name, const HasteValue value);
static error declare_global(Analyzer* self, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility);
static error define_global(Analyzer* self, const char* name, const HasteValue value);
static bool is_defined(Analyzer* self, const char* name);
static bool is_declared(Analyzer* self, const char* name);

static AnalysisStack init_stack(void)
{
	return (AnalysisStack) {
		.values = malloc(sizeof(HasteValue) * STACK_MAX_LEN),
		.sp = 0,
	};
}

static void deinit_stack(AnalysisStack *self)
{
	free(self->values);
}

static void push_value(AnalysisStack* self, const HasteValue value)
{
	assert(self->sp < STACK_MAX_LEN);

	self->sp += 1;
	self->values[self->sp - 1] = value;
}

static HasteValue pop_value(AnalysisStack* self)
{
	assert(self->sp > 0);

	self->sp -= 1;
	HasteValue value = self->values[self->sp];
	self->values[self->sp] = (HasteValue) {0};
	return value;
}

static int32_t find_symbol(const SymbolTable self, const char* name)
{
	const size_t len = arrlen(self);
	if (len == 0) return -1;

	int32_t left = 0;
	int32_t right = len - 1;

	while (left <= right)
	{
		const int32_t mid = left + (right - left) / 2;
		const Symbol symbol = self[mid];
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

static error declare_symbol(SymbolTable table, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility)
{
	const size_t len = arrlen(table);
	arrreserve(table, len + 1);

	ssize_t i = len - 1;
	for (; i >= 0; i -= 1)
	{
		const Symbol symbol = table[i];
		const int cmp_res = strcmp(symbol.name, name);
		if (cmp_res == 0) return ERROR;
		if (cmp_res > 0)  break;

		table[i + 1] = symbol;
	}

	arrsetlen(table, len + 1);
	table[i + 1] = (Symbol) {
		.name = name,
		.defined = false,
		.kind = kind,
		.visibility = visibility,
		.value = value,
	};

	return OK;
}

static error define_symbol(SymbolTable self, const char* name, const HasteValue value)
{
	const int32_t index = find_symbol(self, name);
	if (index == -1) return ERROR;

	Symbol* const symbol = &self[index];
	if (symbol->defined) return ERROR;

	symbol->defined = true;
	symbol->value = value;

	return OK;
}

static Analyzer init_analyzer(Hir hir)
{
	Analyzer result = {0};
	result.hir = hir;
	result.tir = init_tir();
	result.stack = init_stack();
	result.global_scope = (Scope){
		.symbols = arrinit(Symbol),
		.parent = NULL,
		.child = NULL,
	};

	return result;
}

static void deinit_analyzer_ok(Analyzer* self)
{
	deinit_stack(&self->stack);
	arrfree(self->global_scope.symbols);
}

static void deinit_analyzer_err(Analyzer* self)
{
	deinit_analyzer_ok(self);
	deinit_tir(&self->tir);
}

static size_t hir_len(const Analyzer* self)
{
	return arrlen(self->hir.instructions);
}

static HirInstruction peek(const Analyzer* self)
{
	assert(arrlen(self->hir.instructions) > self->current_instruction);
	return self->hir.instructions[self->current_instruction];
}

static HirInstruction advance(Analyzer* self)
{
	if (ended(self))
	{
		return peek(self);
	}
	self->current_instruction += 1;
	return self->hir.instructions[self->current_instruction - 1];
}

static bool ended(const Analyzer* self)
{
	return hir_len(self) <= self->current_instruction;
}

static void push(Analyzer* self, HasteValue value)
{
	push_value(&self->stack, value);
}

static HasteValue pop(Analyzer* self)
{
	return pop_value(&self->stack);
}

static HasteValue value_of(Analyzer* self, const HasteValue value)
{
	switch (value.tag)
	{
	case VALUE_KIND_INT:
	case VALUE_KIND_FLOAT:
	case VALUE_KIND_TYPE:
	case VALUE_KIND_UNKNOWN:
		return value;
	case VALUE_KIND_SYMBOL: {
		const Symbol* symbol = find_local_first(self, value.as.symbol);
		return value_of(self, symbol->value);
	}
	}
}

static void tirrify_the_symbol(Analyzer* self, Symbol* symbol)
{
	const HasteValue value = symbol->value;
	TirConstInfo constant_info = {0};
	constant_info.type = type_of_symbol(*symbol);
	switch (value.tag)
	{
	case VALUE_KIND_INT:
		constant_info.kind = TIR_CONST_I32;
		constant_info.as.i32 = value.as.integer;
		break;
	case VALUE_KIND_FLOAT:
		constant_info.kind = TIR_CONST_F32;
		constant_info.as.f32 = value.as.float_literal;
		break;
	case VALUE_KIND_SYMBOL: {
		unreachable();
	} break;
	case VALUE_KIND_TYPE:
		unreachable();
	case VALUE_KIND_UNKNOWN:
		unreachable();
	}

	const TirConst constant = tir_push_constant(&self->tir, constant_info);
	const TirGlobalInfo global = {
		.name = symbol->name,
		.initializer = constant,
	};

	tir_push_global(&self->tir, global);
}

static Symbol* find_local_first(Analyzer* self, const char* name)
{
	Scope* current = self->current_scope;

	while (current != NULL)
	{
		Scope* const parent = current->parent;
		const int index = find_symbol(current->symbols, name);
		if (index != -1)
		{
			return &current->symbols[index];
		}

		current = parent;
	}

	return NULL;
}

static Symbol* find_local_only(Analyzer* self, const char* name)
{
	Scope* current = self->current_scope;

	const int index = find_symbol(current->symbols, name);
	if (index != -1) return NULL;

	return &current->symbols[index];
}

static Symbol* find_global_first(Analyzer* self, const char* name)
{
	Scope* current = &self->global_scope;

	while (current != NULL)
	{
		Scope* const child = current->child;
		const int index = find_symbol(current->symbols, name);
		if (index != -1)
		{
			return &current->symbols[index];
		}

		current = child;
	}

	return NULL;
}

static Symbol* find_global_only(Analyzer* self, const char* name)
{
	Scope current = self->global_scope;

	const int index = find_symbol(current.symbols, name);
	if (index != -1) return NULL;

	return &current.symbols[index];
}

static error declare_local(Analyzer* self, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility)
{
	Scope* current = self->current_scope;
	const error err = declare_symbol(current->symbols, name, value, kind, visibility);
	return err;
}

static error define_local(Analyzer* self, const char* name, const HasteValue value)
{
	Scope* current = self->current_scope;
	const error err = define_symbol(current->symbols, name, value);
	return err;
}

static error declare_global(Analyzer* self, const char* name, const HasteValue value, const SymbolKind kind, const SymbolVisibility visibility)
{
	Scope current = self->global_scope;
	const error err = declare_symbol(current.symbols, name, value, kind, visibility);
	return err;
}

static error define_global(Analyzer* self, const char* name, const HasteValue value)
{
	Scope* current = self->current_scope;
	const error err = define_symbol(current->symbols, name, value);
	return err;
}

static bool is_defined(Analyzer* self, const char* name)
{
	Symbol* symbol = find_local_first(self, name);
	return symbol->defined;
}

static bool is_declared(Analyzer* self, const char* name)
{
	Symbol* symbol = find_local_first(self, name);
	return !symbol->defined;
}

static void analyze_identifier(Analyzer* self, const HirInstruction instruction)
{
	const char* identifier = instruction.as.identifier;
	const Symbol* symbol = find_local_first(self, identifier);
	if (!symbol->defined)
		panic("Dependency loop detected!");

	const HasteValue value = value_of(self, symbol->value);
	
	push(self, value);
}


static void analyze_integer(Analyzer* self, const HirInstruction instruction)
{
	const int64_t integer = instruction.as.integer;
	const HasteValue value = {
		.type = TYPE_UNTYPED_INT,
		.tag = VALUE_KIND_INT,
		.as = {
			.integer = integer,
		},
	};

	push(self, value);
}

static void analyze_float(Analyzer* self, const HirInstruction instruction)
{
	const double floating = instruction.as.floating_point;
	const HasteValue value = {
		.type = TYPE_UNTYPED_FLOAT,
		.tag = VALUE_KIND_FLOAT,
		.as = {
			.float_literal = floating,
		},
	};

	push(self, value);
}

static void analyze_type(Analyzer* self, const HirInstruction instruction)
{
	const TypeID type = instruction.as.type;
	const HasteValue value = {
		.type = TYPE_TYPE,
		.tag = VALUE_KIND_TYPE,
		.as = {
			.type = type,
		},
	};

	push(self, value);
}

static void analyze_unary_plus(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue value = pop(self);

	if (!type_is_numiric(value.type))
		panic("Can't do a unary operator on a non numiric value.");

	switch (value.tag)
	{
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

static void analyze_unary_minus(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue value = pop(self);

	if (!type_is_numiric(value.type))
		panic("Can't do a unary operator on a non numiric value.");

	HasteValue result = value;
	switch (value.tag)
	{
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

static error check_binary_op(Analyzer* self, HasteValue a, HasteValue b)
{
	unused(self);
	const TypeID a_type = type_of_value(a);
	const TypeID b_type = type_of_value(b);

	if (!(type_is_numiric(a_type) && type_is_numiric(b_type)))
	{
		panic("Cannot do binary operation on a non numiric type.");
		return ERROR;
	}

	const TypeMatchResult match = type_matches(a_type, b_type);
	if (match != TYPE_MATCH_EXACT)
	{
		panic("Type mismatch.");
		return ERROR;
	}

	// assert(a.tag == b.tag);

	return OK;
}

static HasteValue add(Analyzer* self, const HasteValue a, const HasteValue b)
{
	unused(self);
	HasteValue result = a;
	switch (b.tag)
	{
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

static HasteValue subtract(Analyzer* self, const HasteValue a, const HasteValue b)
{
	unused(self);
	HasteValue result = a;
	switch (b.tag)
	{
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

static HasteValue multiply(Analyzer* self, const HasteValue a, const HasteValue b)
{
	unused(self);
	HasteValue result = a;
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

static HasteValue divide(Analyzer* self, const HasteValue a, const HasteValue b)
{
	unused(self);
	HasteValue result = a;
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

static HasteValue power(Analyzer* self, const HasteValue a, const HasteValue b)
{
	unused(self);
	HasteValue result = a;
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

static void analyze_binary_add(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue b = pop(self);
	const HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const HasteValue a_value = value_of(self, a);
	const HasteValue b_value = value_of(self, b);
	const HasteValue result = add(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_sub(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue b = pop(self);
	const HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const HasteValue a_value = value_of(self, a);
	const HasteValue b_value = value_of(self, b);
	const HasteValue result = subtract(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_mul(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue b = pop(self);
	const HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const HasteValue a_value = value_of(self, a);
	const HasteValue b_value = value_of(self, b);
	const HasteValue result = multiply(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_div(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue b = pop(self);
	const HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const HasteValue a_value = value_of(self, a);
	const HasteValue b_value = value_of(self, b);
	const HasteValue result = divide(self, a_value, b_value);

	push(self, result);
}

static void analyze_binary_pow(Analyzer* self, const HirInstruction instruction)
{
	unused(instruction);
	const HasteValue b = pop(self);
	const HasteValue a = pop(self);

	check_binary_op(self, a, b);

	const HasteValue a_value = value_of(self, a);
	const HasteValue b_value = value_of(self, b);
	const HasteValue result = power(self, a_value, b_value);

	push(self, result);
}

static void analyze_start_declaration(Analyzer* self, const HirInstruction instruction)
{
	const HirStartDecl decl = instruction.as.start_decl;
	SymbolKind kind = SYMBOL_CONSTANT;
	switch (decl.kind) {
	case HIR_DECL_CONST:
		kind = SYMBOL_CONSTANT;
		break;
	case HIR_DECL_VAR:
		kind = SYMBOL_VARIABLE;
		break;
	}

	SymbolVisibility visibility = SYMBOL_PRIVATE;
	switch (decl.visibility) {
	case HIR_PUBLIC:
		visibility = SYMBOL_PRIVATE;
		break;
	case HIR_PRIVATE:
		visibility = SYMBOL_PUBLIC;
		break;
	case HIR_SCOPED:
		break;
	}

	error err = declare_local(self, decl.name, (HasteValue) {0}, kind, visibility);
	if (err) panic("Dependency loop detected!");
}

static void analyze_constant_declaration(Analyzer* self, const HirInstruction instruction)
{
	const HirConstantDecl constant = instruction.as.constant;

	if (!(constant.explicit_typing || constant.initialized))
		panic("no value, no initializatio. how tf I'm gonna tell the type?!");

	TypeID type = {0};
	if (constant.explicit_typing)
	{
		const HasteValue type_value = pop(self);
		if (type_value.tag != VALUE_KIND_TYPE)
			panic("Not a type!");
		type = type_value.as.type;
	}

	if (constant.initialized)
	{
		const HasteValue value = pop(self);
		const TypeID value_type = type_of_value(value);
		const TypeMatchResult match_result = type_matches(type, value_type);
		if (match_result != TYPE_MATCH_EXACT)
			panic("Type mismatch.");

		const error err = define_local(self, constant.name, value);
		if (err) panic("IDK");
		tirrify_the_symbol(self, find_local_first(self, constant.name));
	}
}

static void analyze_variable_declaration(Analyzer* self, const HirInstruction instruction)
{
	const HirVariableDecl variable = instruction.as.variable;

	if (variable.explicit_typing && variable.initialized)
		panic("no value, no initializatio. how tf I'm gonna tell the tell?!");

	TypeID type = {0};
	if (variable.explicit_typing)
	{
		const HasteValue type_value = pop(self);
		if (type_value.tag != VALUE_KIND_TYPE)
			panic("Not a type!");
		type = type_value.as.type;
	}

	if (variable.initialized)
	{
		const HasteValue value = pop(self);
		const TypeID value_type = type_of_value(value);
		const TypeMatchResult match_result = type_matches(type, value_type);
		if (match_result != TYPE_MATCH_EXACT)
			panic("Type mismatch.");

		const error err = define_local(self, variable.name, value);
		if (err) panic("IDK");
		tirrify_the_symbol(self, find_local_first(self, variable.name));
	}
}

static void start_analyzing(Analyzer* self)
{
	while (!ended(self))
	{
		const HirInstruction instruction = advance(self);
		switch (instruction.tag)
		{
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
		case HIR_NODE_START_DECLARATION:
			analyze_start_declaration(self, instruction);
			break;
		case HIR_NODE_CONSTANT_DECLARATION:
			analyze_constant_declaration(self, instruction);
			break;
		case HIR_NODE_VARIABLE_DECLARATION:
			analyze_variable_declaration(self, instruction);
			break;
		}
	}
	return;
}

error analyze_hir(Hir hir, Tir* out)
{
	Analyzer analyzer = init_analyzer(hir);
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
