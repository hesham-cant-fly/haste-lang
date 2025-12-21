#include "tir.h"
#include "arena.h"
#include "core/my_array.h"
#include "core/my_commons.h"
#include "type.h"
#include <stddef.h>
#include <stdio.h>

struct Tir init_tir(const char* path)
{
	struct Tir result = {0};
	result.path = path;
	return result;
}

void deinit_tir(struct Tir* tir)
{
	for (size_t i=0; i < tir->constants.len; i += 1)
	{
		struct TirConstInfo constant = tir->constants.items[i];
		unused(constant);
	}
	arrfree(tir->constants);

	for (size_t i=0; i < arrlen(tir->globals); i += 1)
	{
		struct TirGlobalInfo global = tir->globals.items[i];
		unused(global);
	}
	arrfree(tir->globals);

	for (size_t i=0; i < arrlen(tir->functions); i += 1)
	{
		struct TirFunctionInfo function = tir->functions.items[i];
		arrfree(function.instructions);
		arrfree(function.blocks);
	}
	arrfree(tir->functions);
}

TirConst tir_push_constant(struct Tir* self, const struct TirConstInfo constant)
{
	TirFunction result = arrlen(self->constants);
	arrpush(self->constants, constant);
	return result;
}

TirFunction tir_push_function(struct Tir* self, const struct TirFunctionInfo function)
{
	TirFunction result = arrlen(self->functions);
	arrpush(self->functions, function);
	return result;
}

TirGlobal tir_push_global(struct Tir* self, const struct TirGlobalInfo global)
{
	TirGlobal result = arrlen(self->globals);
	arrpush(self->globals, global);
	return result;
}

TirValue tir_push_instruction(struct TirFunctionInfo* self, struct TirInstruction instruction)
{
	TirValue result = arrlen(self->instructions);
	arrpush(self->instructions, instruction);
	return result;
}

TirBlock tir_block_begin(struct TirFunctionInfo* self)
{
	size_t instructions_len = arrlen(self->instructions);
	TirBlock result = arrlen(self->blocks);
	if (result != 0) {
		self->blocks.items[result - 1].end_instruction = instructions_len == 0 ? 0 : instructions_len - 1;
	}

	struct TirBlockInfo new_block = {0};
	new_block.start_instruction = instructions_len;
	arrpush(self->blocks, new_block);

	return result;
}

void tir_block_end(struct TirFunctionInfo* self)
{
	size_t instructions_len = arrlen(self->instructions);
	self->blocks.items[arrlen(self->blocks) - 1].end_instruction = instructions_len == 0 ? 0 : instructions_len - 1;
}

void print_tir_constant(FILE* f, const struct Tir tir, const struct TirConstInfo constant)
{
	print_type(f, constant.type);
	fprintf(f, " ");
	switch (constant.kind)
	{
	case TIR_CONST_I32:
		fprintf(f, "%d", constant.as.i32);
		break;
	case TIR_CONST_F32:
		fprintf(f, "%f", constant.as.f32);
		break;
	case TIR_CONST_GLOBAL_REF:
		fprintf(f, "%s", tir.globals.items[constant.as.global_ref].name);
		break;
	case TIR_CONST_TYPE:
		print_type(f, constant.as.type);
		break;
	case TIR_CONST_ZERO:
		fprintf(f, "---");
		break;
	}
}

void print_tir_function(FILE* f, const struct Tir tir, const struct TirFunctionInfo function)
{
	unused(function);
	unused(tir);
	fprintf(f, "Unimplemented.\n");
}

void print_tir_global(FILE* f, const struct Tir tir, const struct TirGlobalInfo global)
{
	const struct TirConstInfo constant = tir.constants.items[global.initializer];
	fprintf(
		f,
		"define %s $\"%s\": ",
		global.is_constant ? "const" : "var",
		global.name
	);
	print_type(f, constant.type);
	fprintf(f, " = ");
	print_tir_constant(f, tir, constant);
	fprintf(f, ";\n");
}

void print_tir(FILE* f, const struct Tir tir)
{
	const size_t globals_len = arrlen(tir.globals);
	for (size_t i=0; i<globals_len; i += 1)
	{
		const struct TirGlobalInfo global = tir.globals.items[i];
		print_tir_global(f, tir, global);
	}

	const size_t functions_len = arrlen(tir.functions);
	for (size_t i=0; i<functions_len; i += 1)
	{
		const struct TirFunctionInfo function = tir.functions.items[i];
		print_tir_function(f, tir, function);
	}
}

