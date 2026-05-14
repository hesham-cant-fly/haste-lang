#include "haste.h"
#include "llvm-c/Core.h"
#include <signal.h>

struct type_map_entry {
	struct haste_object_type *haste_type;
	LLVMTypeRef llvm_type;
};

struct codegen_context {
	LLVMContextRef llvm_ctx;
	LLVMBuilderRef builder;
	LLVMModuleRef module;
	struct Allocator allocator;
	struct intern_table *table;
	struct { size_t cap, len; struct type_map_entry *items; } struct_types;
};

static void context_deinit(struct codegen_context *ctx)
{
	LLVMDisposeBuilder(ctx->builder);
	LLVMDisposeModule(ctx->module);
	LLVMContextDispose(ctx->llvm_ctx);
	arrfree(ctx->allocator, ctx->struct_types);
	*ctx = (struct codegen_context){0};
}

// ── LLVM type helpers ─────────────────────────────────────────────

#define DEFINE_LLVM_TYPE_FN(name, llvm_call) \
	static LLVMTypeRef t_##name(struct codegen_context *ctx) { return llvm_call; }

DEFINE_LLVM_TYPE_FN(i8,   LLVMInt8TypeInContext(ctx->llvm_ctx))
DEFINE_LLVM_TYPE_FN(i32,  LLVMInt32TypeInContext(ctx->llvm_ctx))
DEFINE_LLVM_TYPE_FN(i64,  LLVMInt64TypeInContext(ctx->llvm_ctx))
DEFINE_LLVM_TYPE_FN(f32,  LLVMFloatTypeInContext(ctx->llvm_ctx))
DEFINE_LLVM_TYPE_FN(void, LLVMVoidTypeInContext(ctx->llvm_ctx))

static LLVMTypeRef t_i8ptr(struct codegen_context *ctx)
{
	return LLVMPointerType(t_i8(ctx), 0);
}

// ── Haste type → LLVM type ────────────────────────────────────────

static LLVMTypeRef llvm_type(struct codegen_context *ctx, struct haste_value type)
{
	assert(IS_TYPE(type));

	if (type_equal(type, ty_int) or type_equal(type, ty_untyped_int) or type_equal(type, ty_zero))
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

	if (IS_STRUCT_TYPE(type) or IS_AUTO_STRUCT_TYPE(type)) {
		struct haste_struct_type *st = (struct haste_struct_type*)AS_TYPE(type);

		// Check cache
		for (size_t i = 0; i < ctx->struct_types.len; i += 1) {
			if (ctx->struct_types.items[i].haste_type == AS_TYPE(type))
				return ctx->struct_types.items[i].llvm_type;
		}

		// Create named struct
		char *name = tsprint("struct.type.{s}.{z}", st->base.name then st->base.name otherwise "auto", ctx->struct_types.len);
		LLVMTypeRef llvm_st = LLVMStructCreateNamed(ctx->llvm_ctx, name);

		arrpush(ctx->allocator, ctx->struct_types, ((struct type_map_entry){
			.haste_type = AS_TYPE(type),
			.llvm_type = llvm_st,
		}));

		// Set body
		LLVMTypeRef members[SAFE_COUNT(st->field_count)];
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
	case HASTE_VL_ZERO: {
		return LLVMConstInt(t_i32(ctx), 0, true);
	}
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
			LLVMValueRef members[SAFE_COUNT(st->field_count)];
			for (size_t i = 0; i < st->field_count; i += 1) {
				members[i] = llvm_value(ctx, so->fields[i]);
			}
			return LLVMConstNamedStruct(llvm_st, members, (unsigned)st->field_count);
		}

		unreachable();
	}
	default:
		unreachable();
	}
}

// ── Expression codegen ────────────────────────────────────────────

static LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node);

static LLVMValueRef codegen_cast(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	LLVMValueRef val = codegen_expr(ctx, node->cast.expr);
	LLVMTypeRef target_type = llvm_type(ctx, node->type);
	LLVMTypeRef src_type = LLVMTypeOf(val);

	if (LLVMGetTypeKind(src_type) == LLVMGetTypeKind(target_type))
		return val;

	if (LLVMGetTypeKind(src_type) == LLVMIntegerTypeKind
		and LLVMGetTypeKind(target_type) == LLVMFloatTypeKind)
		return LLVMBuildSIToFP(ctx->builder, val, target_type, "cast");

	if (LLVMGetTypeKind(src_type) == LLVMFloatTypeKind
		and LLVMGetTypeKind(target_type) == LLVMIntegerTypeKind)
		return LLVMBuildFPToSI(ctx->builder, val, target_type, "cast");

	if (LLVMGetTypeKind(src_type) == LLVMIntegerTypeKind
		and LLVMGetTypeKind(target_type) == LLVMIntegerTypeKind)
		return LLVMBuildIntCast2(ctx->builder, val, target_type, true, "cast");

	if (LLVMGetTypeKind(src_type) == LLVMPointerTypeKind
		and LLVMGetTypeKind(target_type) == LLVMPointerTypeKind)
		return LLVMBuildBitCast(ctx->builder, val, target_type, "cast");

	unreachable();
}

static LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VALUE:
		return llvm_value(ctx, node->value);
	case ND_CAST:
		return codegen_cast(ctx, node);
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
		const char *name = intern_token(ctx->table, node->variable.name);
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

Error codegen(struct Allocator allocator, const source_file_id src, struct intern_table *table, const char *output_path, bool dump_to_stderr)
{
	const char *path = get_source_file_path(src);

	LLVMContextRef llvm_ctx = LLVMContextCreate();
	LLVMBuilderRef builder = LLVMCreateBuilder();
	LLVMModuleRef module = LLVMModuleCreateWithNameInContext(path, llvm_ctx);

	struct codegen_context ctx = {
		.llvm_ctx = llvm_ctx,
		.builder = builder,
		.module = module,
		.allocator = allocator,
		.table = table,
	};

	leach (struct haste_ast_node, node, get_source_file_ast(src)) {
		codegen_global_node(&ctx, node);
	}

	if (dump_to_stderr) {
		LLVMDumpModule(ctx.module);
	} else if (output_path) {
		char *err_msg = NULL;
		if (LLVMPrintModuleToFile(ctx.module, output_path, &err_msg)) {
			fprintf(stderr, "error: failed to write LLVM IR to '%s': %s\n", output_path, err_msg);
			LLVMDisposeMessage(err_msg);
			context_deinit(&ctx);
			return ERROR;
		}
	}
	context_deinit(&ctx);
	return OK;
}
