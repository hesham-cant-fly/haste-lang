#include "tir.h"
#include "arena.h"
#include "core/my_array.h"
#include "core/my_commons.h"
#include "type.h"
#include <stddef.h>
#include <stdio.h>

Tir init_tir(void)
{
	Tir result = {0};
	result.constants = arrinit(TirConstInfo);
	result.functions = arrinit(TirFunctionInfo);
	result.globals = arrinit(TirGlobalInfo);
	return result;
}

void deinit_tir(Tir* tir)
{
	for (size_t i=0; i < arrlen(tir->constants); i += 1)
	{
		TirConstInfo constant = tir->constants[i];
		unused(constant);
	}
	arrfree(tir->constants);

	for (size_t i=0; i < arrlen(tir->globals); i += 1)
	{
		TirGlobalInfo global = tir->globals[i];
		unused(global);
	}
	arrfree(tir->globals);

	for (size_t i=0; i < arrlen(tir->functions); i += 1)
	{
		TirFunctionInfo function = tir->functions[i];
		arrfree(function.instructions);
		arrfree(function.blocks);
	}
	arrfree(tir->functions);
}

TirConst tir_push_constant(Tir* self, const TirConstInfo constant)
{
	TirFunction result = arrlen(self->constants);
	arrpush(self->constants, constant);
	return result;
}

TirFunction tir_push_function(Tir* self, const TirFunctionInfo function)
{
	TirFunction result = arrlen(self->functions);
	arrpush(self->functions, function);
	return result;
}

TirGlobal tir_push_global(Tir* self, const TirGlobalInfo global)
{
	TirGlobal result = arrlen(self->globals);
	arrpush(self->globals, global);
	return result;
}

TirValue tir_push_instruction(TirFunctionInfo* self, TirInstruction instruction)
{
	TirValue result = arrlen(self->instructions);
	arrpush(self->instructions, instruction);
	return result;
}

TirBlock tir_block_begin(TirFunctionInfo* self)
{
	size_t instructions_len = arrlen(self->instructions);
	TirBlock result = arrlen(self->blocks);
	if (result != 0)
	{
		self->blocks[result - 1].end_instruction = instructions_len == 0 ? 0 : instructions_len - 1;
	}

	TirBlockInfo new_block = {0};
	new_block.start_instruction = instructions_len;
	arrpush(self->blocks, new_block);

	return result;
}

void tir_block_end(TirFunctionInfo* self)
{
	size_t instructions_len = arrlen(self->instructions);
	self->blocks[arrlen(self->blocks) - 1].end_instruction = instructions_len == 0 ? 0 : instructions_len - 1;
}

void print_tir_constant(FILE* f, const Tir tir, const TirConstInfo constant)
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
		fprintf(f, "%s", tir.globals[constant.as.global_ref].name);
		break;
	case TIR_CONST_TYPE:
		print_type(f, constant.as.type);
		break;
	case TIR_CONST_ZERO:
		fprintf(f, "---");
		break;
	}
}

void print_tir_function(FILE* f, const Tir tir, const TirFunctionInfo function)
{
	unused(function);
	unused(tir);
	fprintf(f, "Unimplemented.\n");
}

void print_tir_global(FILE* f, const Tir tir, const TirGlobalInfo global)
{
	const TirConstInfo constant = tir.constants[global.initializer];
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

void print_tir(FILE* f, const Tir tir)
{
	const size_t globals_len = arrlen(tir.globals);
	for (size_t i=0; i<globals_len; i += 1)
	{
		const TirGlobalInfo global = tir.globals[i];
		print_tir_global(f, tir, global);
	}

	const size_t functions_len = arrlen(tir.functions);
	for (size_t i=0; i<functions_len; i += 1)
	{
		const TirFunctionInfo function = tir.functions[i];
		print_tir_function(f, tir, function);
	}
}

