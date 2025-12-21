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

enum TirConstKind {
	TIR_CONST_I32,
	TIR_CONST_F32,
	TIR_CONST_TYPE,

	TIR_CONST_GLOBAL_REF, // &other_global
	TIR_CONST_ZERO,       // zero-initialized aggregate
};

struct TirConstInfo {
	TypeID type;
	enum TirConstKind kind;

	union {
		int32_t   i32;
		float     f32;
		TypeID    type;
		TirGlobal global_ref;
	} as;
};

#define OPERANDS_COUNT 2

enum TirOpCode {
	TIR_CONST, // TirConst constant;

	TIR_ADD,
	TIR_SUB,
	TIR_MUL,
	TIR_DIV,
	TIR_POW,
};

struct TirInstruction {
	TypeID type;
	enum TirOpCode op_code;

	union {
		TirConst constant;
		TirValue operands[OPERANDS_COUNT];
	} as;
};

struct TirBlockInfo {
	TirValue start_instruction;
	TirValue end_instruction;
};

struct TirFunctionInfo {
	const char* name;
	struct TirInstructionList {
		struct TirInstruction* items;
		size_t len;
		size_t cap;
	} instructions;
	struct TirBlockList {
		struct TirBlockInfo* items;
		size_t len;
		size_t cap;
	} blocks;
};

struct TirGlobalInfo {
	const char* name;
	bool is_constant;
	TirConst initializer;
	// A way to represent values
};

struct Tir {
	const char* path;
	struct TirConstList {
		struct TirConstInfo* items;
		size_t len;
		size_t cap;
	} constants;
	struct TirFunctionList {
		struct TirFunctionInfo* items;
		size_t len;
		size_t cap;
	} functions;
	struct TirGlobalList {
		struct TirGlobalInfo* items;
		size_t len;
		size_t cap;
	} globals;
};

struct Tir init_tir(const char* path);
void deinit_tir(struct Tir *tir);
TirConst tir_push_constant(struct Tir* self, const struct TirConstInfo constant);
TirFunction tir_push_function(struct Tir* self, const struct TirFunctionInfo function);
TirGlobal tir_push_global(struct Tir* self, const struct TirGlobalInfo global);

TirBlock tir_block_begin(struct TirFunctionInfo* self);
void tir_block_end(struct TirFunctionInfo* self);

void print_tir_constant(FILE* f, const struct Tir tir, const struct TirConstInfo constant);
void print_tir_function(FILE* f, const struct Tir tir, const struct TirFunctionInfo function);
void print_tir_global(FILE* f, const struct Tir tir, const struct TirGlobalInfo global);
void print_tir(FILE* f, const struct Tir tir);

#endif // !__TIR_H
