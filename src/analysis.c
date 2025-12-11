#include "analysis.h"
#include "hir.h"
#include "tir.h"
#include "type.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define STACK_MAX_LEN 256

typedef struct Symbol {
	const char* name;
	TirValue tir_value;
	TypeID type;
} Symbol;

typedef enum HasteValueKind {
	VALUE_KIND_INT,
	VALUE_KIND_FLOAT,
	VALUE_KIND_SYMBOL,
	VALUE_KIND_UNKNOWN,
} HasteValueKind;

typedef struct HasteValue {
	TirValue tir_value;
	TypeID type;
	HasteValueKind tag;
	union {
		int64_t integer;
		double float_literal;
		const char* symbol_name;
	} as;
} HasteValue;

typedef struct AnalysisStack {
	HasteValue *values;
	size_t sp;
} AnalysisStack;

static AnalysisStack init_stack(void);
static void deinit_stack(AnalysisStack* self);
static void push_value(AnalysisStack* self, const HasteValue value);
static HasteValue pop_value(AnalysisStack* self);

typedef Symbol* SymbolTable;

typedef struct Analyzer {
	SymbolTable symbol_table;
	AnalysisStack stack;
	Hir hir;
	size_t current_instruction;
} Analyzer;

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
