#include "haste.h"
#include "llvm-c/Core.h"

struct codegen_context {
	LLVMContextRef llvm_ctx;
	LLVMBuilderRef builder;
	LLVMModuleRef module;
};

static void context_deinit(struct codegen_context *ctx)
{
	LLVMDisposeBuilder(ctx->builder);
	LLVMDisposeModule(ctx->module);
	LLVMContextDispose(ctx->llvm_ctx);
	*ctx = (struct codegen_context){0};
}

static LLVMTypeRef value_to_llvm_type(struct codegen_context *ctx, struct haste_value value)
{
	assert(IS_TYPE(value));

	if (type_equal(value, ty_int)
		or type_equal(value, ty_untyped_int)) {
		return LLVMInt32TypeInContext(ctx->llvm_ctx);
	}

	if (type_equal(value, ty_float)
		or type_equal(value, ty_untyped_float)) {
		return LLVMFloatTypeInContext(ctx->llvm_ctx);
	}

	if (type_equal(value, ty_void)) {
		return LLVMVoidTypeInContext(ctx->llvm_ctx);
	}

	unreachable();
}

static LLVMValueRef value_to_llvm_value(struct codegen_context *ctx, struct haste_value value)
{
	assert(not IS_TYPE(value));

	switch (value.kind) {
	case HASTE_VL_INT:
	case HASTE_VL_UNTYPED_INT:
		return LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_ctx), value.integer, true);
	case HASTE_VL_FLOAT:
	case HASTE_VL_UNTYPED_FLOAT:
		return LLVMConstReal(LLVMFloatTypeInContext(ctx->llvm_ctx), value.floating);
	default: unreachable();
	}
}

static const char *span_to_tstr(struct span span)
{
	return nclone_string(get_temporary_allocator(), span.start, span.len);
}

LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VALUE:
		return value_to_llvm_value(ctx, node->value);
	default: unimplemented();
	}
}

Error codegen_global_node(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VAR_DECL: {
		if (node->variable.is_explicitly_comptime) return OK;
		LLVMTypeRef type = value_to_llvm_type(ctx, node->type);
		const char *name = tsprint("{token}", node->variable.name);
		LLVMValueRef global = LLVMAddGlobal(ctx->module, type, name);
		LLVMValueRef value = {0};
		if (node->variable.value != NULL) {
			value = codegen_expr(ctx, node->variable.value);
		} else {
			value = LLVMConstNull(type);
		}
		LLVMSetInitializer(global, value);
		LLVMSetGlobalConstant(global, node->variable.is_constant);
	} break;
	default: unreachable();
	}

	reset_temporary_allocator();
	return OK;
}

Error codegen(struct Allocator allocator, const source_file_id src)
{
	discard allocator;
	const char *source_file_path = get_source_file_path(src);
	struct haste_ast_node *root = get_source_file_ast(src);

	LLVMContextRef llvm_ctx = LLVMContextCreate();
	LLVMBuilderRef builder = LLVMCreateBuilder();
	LLVMModuleRef module = LLVMModuleCreateWithNameInContext(source_file_path, llvm_ctx);
	struct codegen_context ctx = {
		.llvm_ctx = llvm_ctx,
		.builder = builder,
		.module = module,
	};

	leach (struct haste_ast_node, node, root) {
		codegen_global_node(&ctx, node);
	}

	LLVMDumpModule(ctx.module);

	context_deinit(&ctx);
	return OK;
}
