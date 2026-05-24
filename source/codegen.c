#include "haste.h"
#include "my_common.h"
#include "my_stream.h"
#include "llvm-c/Core.h"

struct type_map_entry {
	TypeID haste_type;
	LLVMTypeRef llvm_type;
};

struct codegen_context {
	LLVMContextRef llvm_ctx;
	LLVMBuilderRef builder;
	LLVMModuleRef module;
	struct Allocator allocator;
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

static LLVMTypeRef llvm_type(struct codegen_context *ctx, struct haste_type type)
{
	struct haste_type_info *tp = AS_TYPE_INFO(type);

	if (tp->kind == HASTE_TY_UNTYPED_INT or tp->kind == HASTE_TY_ZERO) return t_i32(ctx);
	if (tp->kind == HASTE_TY_USIZE) return t_i64(ctx);
	if (tp->kind == HASTE_TY_FLOAT or tp->kind == HASTE_TY_UNTYPED_FLOAT) return t_f32(ctx);
	if (tp->kind == HASTE_TY_VOID) return t_void(ctx);
	if (tp->kind == HASTE_TY_UNTYPED_STRING or tp->kind == HASTE_TY_CSTR or tp->kind == HASTE_TY_STRING) return t_i8ptr(ctx);

	if (tp->kind == HASTE_TY_INT or tp->kind == HASTE_TY_UINT) {
		return LLVMIntTypeInContext(ctx->llvm_ctx, tp->bit_size);
	}

	if (tp->kind == HASTE_TY_STRUCT or tp->kind == HASTE_TY_AUTO_STRUCT) {
		struct haste_type_info *type_info = AS_TYPE_INFO(type);
		struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(type);

		// Check cache (compare by pool_id, not pointer, since typeof() returns pool copies)
		for (size_t i = 0; i < ctx->struct_types.len; i += 1) {
			if (ctx->struct_types.items[i].haste_type == AS_TYPEID(type)) {
				return ctx->struct_types.items[i].llvm_type;
			}
		}

		// Create named struct
		char *name = tsprint("struct.type.{s}.{z}", type_info->name then type_info->name otherwise "auto", ctx->struct_types.len);
		LLVMTypeRef llvm_st = LLVMStructCreateNamed(ctx->llvm_ctx, name);

		arrpush(ctx->allocator, ctx->struct_types, ((struct type_map_entry){
			.haste_type = AS_TYPEID(type),
			.llvm_type = llvm_st,
		}));

		// Set body
		LLVMTypeRef members[SAFE_COUNT(st->len)];
		iarreach (i, *st) {
			members[i] = llvm_type(ctx, st->items[i].type);
		}
		LLVMStructSetBody(llvm_st, members, (unsigned)st->len, false);
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
		int k = type_pool_get(value.type_id)->kind;
		if (k == HASTE_TY_USIZE)
			return LLVMConstInt(t_i64(ctx), value.integer, false);
		if (k == HASTE_TY_INT or k == HASTE_TY_UNTYPED_INT or k == HASTE_TY_UINT) {
			LLVMTypeRef int_type = llvm_type(ctx, typeof_value(value));
			return LLVMConstInt(int_type, value.integer, k != HASTE_TY_UINT);
		}
		if (k == HASTE_TY_FLOAT or k == HASTE_TY_UNTYPED_FLOAT)
			return LLVMConstReal(t_f32(ctx), value.floating);
		unreachable();
	}
	case HASTE_VL_OBJ: {
		if (value.obj->kind == HASTE_OBJ_STRING) {
			struct haste_string_object *s = (struct haste_string_object*)value.obj;
			LLVMValueRef global = emit_string_global(ctx, s->data, s->len);
			return LLVMConstBitCast(global, t_i8ptr(ctx));
		}

		if (value.obj->kind == HASTE_OBJ_STRUCT) {
			LLVMTypeRef llvm_st = llvm_type(ctx, typeof_value(value));
			struct haste_struct_object *so = (struct haste_struct_object*)value.obj;
			struct haste_struct_type_info *st = AS_STRUCT_TYPE_INFO(typeof_value(value));
			LLVMValueRef members[SAFE_COUNT(st->len)];
			iarreach (i, *st) {
				members[i] = llvm_value(ctx, so->fields[i]);
			}
			return LLVMConstNamedStruct(llvm_st, members, (unsigned)st->len);
		}

		unreachable();
	}
	/* default: */
	/* 	unreachable(); */
	case HASTE_VL_NONE:
		unreachable();
	case HASTE_VL_BAD:
		unreachable();
	case HASTE_VL_UNINIT:
		unreachable();
	case HASTE_VL_RUNTIME:
		unreachable();
	case HASTE_VL_TYPE:
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
		const char *name = intern_token(node->variable.name);
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

Error codegen(
	struct Allocator allocator,
	const source_file_id src,
	const char *output_path,
	bool dump_to_stderr)
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
