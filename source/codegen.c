#include "haste.h"
#include "llvm-c/Core.h"

struct type_map_entry {
	struct haste_object_type *haste_type;
	LLVMTypeRef llvm_type;
};

struct codegen_context {
	LLVMContextRef llvm_ctx;
	LLVMBuilderRef builder;
	LLVMModuleRef module;
	size_t struct_type_count;
	struct type_map_entry struct_types[64];
};

static void context_deinit(struct codegen_context *ctx)
{
	LLVMDisposeBuilder(ctx->builder);
	LLVMDisposeModule(ctx->module);
	LLVMContextDispose(ctx->llvm_ctx);
	*ctx = (struct codegen_context){0};
}

// ── LLVM type helpers ─────────────────────────────────────────────

static LLVMTypeRef t_i8(struct codegen_context *ctx)
{
	return LLVMInt8TypeInContext(ctx->llvm_ctx);
}

static LLVMTypeRef t_i32(struct codegen_context *ctx)
{
	return LLVMInt32TypeInContext(ctx->llvm_ctx);
}

static LLVMTypeRef t_i64(struct codegen_context *ctx)
{
	return LLVMInt64TypeInContext(ctx->llvm_ctx);
}

static LLVMTypeRef t_f32(struct codegen_context *ctx)
{
	return LLVMFloatTypeInContext(ctx->llvm_ctx);
}

static LLVMTypeRef t_void(struct codegen_context *ctx)
{
	return LLVMVoidTypeInContext(ctx->llvm_ctx);
}

static LLVMTypeRef t_i8ptr(struct codegen_context *ctx)
{
	return LLVMPointerType(t_i8(ctx), 0);
}

// ── Haste type → LLVM type ────────────────────────────────────────

static LLVMTypeRef llvm_type(struct codegen_context *ctx, struct haste_value type)
{
	assert(IS_TYPE(type));

	if (type_equal(type, ty_int) or type_equal(type, ty_untyped_int))
		return t_i32(ctx);

	if (type_equal(type, ty_float) or type_equal(type, ty_untyped_float))
		return t_f32(ctx);

	if (type_equal(type, ty_void))
		return t_void(ctx);

	if (type_equal(type, ty_string)) {
		LLVMTypeRef members[] = { t_i8ptr(ctx), t_i64(ctx) };
		return LLVMStructTypeInContext(ctx->llvm_ctx, members, 2, false);
	}

	if (type_equal(type, ty_untyped_string) or type_equal(type, ty_cstr))
		return t_i8ptr(ctx);

	if (IS_STRUCT_TYPE(type)) {
		struct haste_struct_type *st = (struct haste_struct_type*)AS_TYPE(type);

		// Check cache
		for (size_t i = 0; i < ctx->struct_type_count; i += 1) {
			if (ctx->struct_types[i].haste_type == AS_TYPE(type))
				return ctx->struct_types[i].llvm_type;
		}

		// Create named struct
		char *name = tsprint("struct.type.{z}", ctx->struct_type_count);
		LLVMTypeRef llvm_st = LLVMStructCreateNamed(ctx->llvm_ctx, name);

		ctx->struct_types[ctx->struct_type_count].haste_type = AS_TYPE(type);
		ctx->struct_types[ctx->struct_type_count].llvm_type = llvm_st;
		ctx->struct_type_count += 1;

		// Set body
		LLVMTypeRef members[st->field_count > 0 ? st->field_count : 1];
		for (size_t i = 0; i < st->field_count; i += 1) {
			members[i] = llvm_type(ctx, st->fields[i].type);
		}
		LLVMStructSetBody(llvm_st, members, (unsigned)st->field_count, false);
		return llvm_st;
	}

	unreachable();
}

// ── String globals ────────────────────────────────────────────────

static uint64_t string_global_counter = 0;

static LLVMValueRef emit_string_global(struct codegen_context *ctx,
                                       const char *data, uint64_t len)
{
	char *name = tsprint(".str.{lu}", string_global_counter++);

	LLVMValueRef global = LLVMAddGlobal(ctx->module,
		LLVMArrayType(t_i8(ctx), len + 1), name);
	LLVMSetInitializer(global,
		LLVMConstStringInContext(ctx->llvm_ctx, data, (unsigned)len, false));
	LLVMSetGlobalConstant(global, true);
	LLVMSetLinkage(global, LLVMPrivateLinkage);
	LLVMSetUnnamedAddress(global, LLVMGlobalUnnamedAddr);
	return global;
}

// ── Haste value → LLVM value ──────────────────────────────────────

static LLVMValueRef llvm_value(struct codegen_context *ctx, struct haste_value value)
{
	assert(not IS_TYPE(value));

	switch (value.kind) {
	case HASTE_VL_SCALAR: {
		if (value.type->kind == HASTE_TY_INT or value.type->kind == HASTE_TY_UNTYPED_INT)
			return LLVMConstInt(t_i32(ctx), value.integer, true);
		return LLVMConstReal(t_f32(ctx), value.floating);
	}
	case HASTE_VL_OBJ: {
		if (value.obj->kind == HASTE_OBJ_STRING) {
			struct haste_string_object *s = (struct haste_string_object*)value.obj;
			LLVMValueRef global = emit_string_global(ctx, s->data, s->len);

			if (type_equal(typeof(value), ty_string)) {
				LLVMValueRef fields[] = {
					LLVMConstBitCast(global, t_i8ptr(ctx)),
					LLVMConstInt(t_i64(ctx), s->len, false),
				};
				return LLVMConstStructInContext(ctx->llvm_ctx, fields, 2, false);
			}

			return LLVMConstBitCast(global, t_i8ptr(ctx));
		}

		if (value.obj->kind == HASTE_OBJ_STRUCT) {
			LLVMTypeRef llvm_st = llvm_type(ctx, typeof(value));
			struct haste_struct_object *so = (struct haste_struct_object*)value.obj;
			struct haste_struct_type *st = (struct haste_struct_type*)AS_TYPE(typeof(value));
			LLVMValueRef members[st->field_count > 0 ? st->field_count : 1];
			for (size_t i = 0; i < st->field_count; i += 1) {
				members[i] = llvm_value(ctx, so->fields[i]);
			}
			return LLVMConstNamedStruct(llvm_st, members, (unsigned)st->field_count);
		}

		unreachable();
	}
	default: unreachable();
	}
}

// ── Expression codegen ────────────────────────────────────────────

static LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VALUE:
		return llvm_value(ctx, node->value);
	default: unimplemented();
	}
}

// ── Global declaration codegen ────────────────────────────────────

static Error codegen_global_node(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VAR_DECL: {
		if (node->variable.is_explicitly_comptime) return OK;

		LLVMTypeRef type = llvm_type(ctx, node->type);
		const char *name = tsprint("{token}", node->variable.name);
		LLVMValueRef global = LLVMAddGlobal(ctx->module, type, name);

		LLVMValueRef init = node->variable.value != NULL
			then codegen_expr(ctx, node->variable.value)
			otherwise LLVMConstNull(type);
		LLVMSetInitializer(global, init);
		LLVMSetGlobalConstant(global, node->variable.is_constant);
	} break;
	default: unreachable();
	}

	reset_temporary_allocator();
	return OK;
}

// ── Entry point ──────────────────────────────────────────────────

Error codegen(struct Allocator allocator, const source_file_id src)
{
	discard allocator;
	const char *path = get_source_file_path(src);

	LLVMContextRef llvm_ctx = LLVMContextCreate();
	LLVMBuilderRef builder = LLVMCreateBuilder();
	LLVMModuleRef module = LLVMModuleCreateWithNameInContext(path, llvm_ctx);

	struct codegen_context ctx = {
		.llvm_ctx = llvm_ctx,
		.builder = builder,
		.module = module,
	};

	leach (struct haste_ast_node, node, get_source_file_ast(src)) {
		codegen_global_node(&ctx, node);
	}

	LLVMDumpModule(ctx.module);
	context_deinit(&ctx);
	return OK;
}
