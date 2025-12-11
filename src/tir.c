#include "tir.h"
#include "arena.h"
#include "core/my_array.h"

Tir init_tir(void)
{
	Tir result = {0};
	result.functions = arrinit(TirFunctionInfo);
	return result;
}

void deinit_tir(Tir* tir)
{
	for (size_t i=0; i < arrlen(tir->functions); i += 1)
	{
		TirFunctionInfo item = tir->functions[i];
		arrfree(item.instructions);
		arrfree(item.blocks);
	}
	arrfree(tir->functions);
	arena_free(&tir->operands_pool);
}

TirFunction tir_push_function(Tir* self, const TirFunctionInfo function)
{
	TirFunction result = arrlen(self->functions);
	arrpush(self->functions, function);
	return result;
}

TirValue* tir_alloc_operands(Tir* self, const size_t amount)
{
	TirValue* result = arena_alloc(
		&self->operands_pool,
		sizeof(TirValue) * amount
	);
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
