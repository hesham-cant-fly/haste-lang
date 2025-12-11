/** TIR, Typed IR. is a SSA form and the result
  * of the analysis phase.
  */
#ifndef __TIR_H
#define __TIR_H

#include "type.h"
#include "config.h"
#include "arena.h"
#include <stddef.h>
#include <stdint.h>

typedef size_t TirValue; // References to a TirInstruction in the current function
typedef size_t TirFunction;
typedef size_t TirBlock;

typedef enum TirOpCode {
	TIR_LIT_I32,   // int32_t lit_i32
	TIR_LIT_FLOAT, // float lit_float

	TIR_ADD_I32,
	TIR_SUB_I32,
	TIR_MUL_I32,
	TIR_DIV_I32,
	TIR_POW_I32,
} TirOpCode;

typedef struct TirInstruction {
	TypeID type;
	TirOpCode op_code;
	size_t operands_count;
	TirValue* operands;

	union {
		int32_t lit_i32;
		float lit_float;
	} value;
} TirInstruction;

typedef struct TirBlockInfo {
	TirValue start_instruction;
	TirValue end_instruction;
} TirBlockInfo;

typedef struct TirFunctionInfo {
	const char* name;
	// dynamic array of instructions
	TirInstruction* instructions;
	// dynamic array of blocks
	TirBlockInfo* blocks;
} TirFunctionInfo;

typedef struct Tir {
	// dynamic array of functions
	TirFunctionInfo *functions;
	Arena operands_pool;
} Tir;

Tir init_tir(void);
void deinit_tir(Tir *tir);
TirFunction tir_push_function(Tir *self, const TirFunctionInfo function);
TirValue* tir_alloc_operands(Tir* self, const size_t amount);

TirBlock tir_block_begin(TirFunctionInfo* self);
void tir_block_end(TirFunctionInfo* self);

#endif // !__TIR_H
