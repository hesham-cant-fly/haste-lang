/** TIR, Typed IR. is a SSA form and the result
  * of the analysis phase.
  */
#ifndef __TIR_H
#define __TIR_H

#include "type.h"
#include "arena.h"
#include <stddef.h>
#include <stdint.h>

typedef size_t TirValue; // References to a TirInstruction in the current function
typedef size_t TirFunction;
typedef size_t TirBlock;
typedef size_t TirGlobal;
typedef size_t TirConst;

typedef enum TirConstKind {
	TIR_CONST_I32,
	TIR_CONST_F32,

	TIR_CONST_GLOBAL_REF, // &other_global
	TIR_CONST_ZERO,       // zero-initialized aggregate
} TirConstKind;

typedef struct TirConstInfo {
	TypeID type;
	TirConstKind kind;

	union {
		int32_t i32;
		float   f32;
		TirGlobal global_ref;
	} as;
} TirConstInfo;

#define OPERANDS_COUNT 2

typedef enum TirOpCode {
	TIR_CONST, // TirConst constant;

	TIR_ADD,
	TIR_SUB,
	TIR_MUL,
	TIR_DIV,
	TIR_POW,
} TirOpCode;

typedef struct TirInstruction {
	TypeID type;
	TirOpCode op_code;

	union {
		TirConst constant;
		TirValue operands[OPERANDS_COUNT];
	} as;
} TirInstruction;

typedef struct TirBlockInfo {
	TirValue start_instruction;
	TirValue end_instruction;
} TirBlockInfo;

typedef struct TirFunctionInfo {
	const char* name;
	TirInstruction* instructions;
	TirBlockInfo* blocks;
} TirFunctionInfo;

typedef struct TirGlobalInfo {
	const char* name;
	TirConst initializer;
	// A way to represent values
} TirGlobalInfo;

typedef struct Tir {
	TirConstInfo* constants;
	TirFunctionInfo* functions;
	TirGlobalInfo* globals;
} Tir;

Tir init_tir(void);
void deinit_tir(Tir *tir);
TirConst tir_push_constant(Tir* self, const TirConstInfo constant);
TirFunction tir_push_function(Tir* self, const TirFunctionInfo function);
TirGlobal tir_push_global(Tir* self, const TirGlobalInfo global);

TirBlock tir_block_begin(TirFunctionInfo* self);
void tir_block_end(TirFunctionInfo* self);

void print_tir_constant(FILE* f, const Tir tir, const TirConstInfo constant);
void print_tir_function(FILE* f, const Tir tir, const TirFunctionInfo function);
void print_tir_global(FILE* f, const Tir tir, const TirGlobalInfo global);
void print_tir(FILE* f, const Tir tir);

#endif // !__TIR_H
