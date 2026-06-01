#include "haste.h"
#include "my_common.h"
#include "my_stream.h"
#include "llvm-c/Core.h"
#include <assert.h>
#include <llvm-c/Types.h>

struct type_map_entry {
	TypeID haste_type;
	LLVMTypeRef llvm_type;
};

struct local_entry {
	const char *name;
	LLVMValueRef value;
	LLVMTypeRef elem_type;
};

struct codegen_context {
	LLVMContextRef llvm_ctx;
	LLVMBuilderRef builder;
	LLVMModuleRef module;
	struct Allocator allocator;
	struct { size_t cap, len; struct type_map_entry *items; } struct_types;
	LLVMValueRef current_func;
	struct { size_t cap, len; struct local_entry *items; } locals;
};

static LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node);
static LLVMValueRef codegen_stmt(struct codegen_context *ctx, const struct haste_ast_node *node);

static void context_deinit(struct codegen_context *ctx)
{
	LLVMDisposeBuilder(ctx->builder);
	LLVMDisposeModule(ctx->module);
	LLVMContextDispose(ctx->llvm_ctx);
	arrfree(ctx->allocator, ctx->struct_types);
	arrfree(ctx->allocator, ctx->locals);
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

		for (size_t i = 0; i < ctx->struct_types.len; i += 1) {
			if (ctx->struct_types.items[i].haste_type == AS_TYPEID(type)) {
				return ctx->struct_types.items[i].llvm_type;
			}
		}

		char *name = tsprint("struct.type.{s}.{z}", type_info->name then type_info->name otherwise "auto", ctx->struct_types.len);
		LLVMTypeRef llvm_st = LLVMStructCreateNamed(ctx->llvm_ctx, name);

		arrpush(ctx->allocator, ctx->struct_types, ((struct type_map_entry){
			.haste_type = AS_TYPEID(type),
			.llvm_type = llvm_st,
		}));

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

// ── Local variable management ─────────────────────────────────────

static struct local_entry push_local(struct codegen_context *ctx, const char *name, LLVMValueRef value, LLVMTypeRef elem_type)
{
	arrpush(ctx->allocator, ctx->locals, ((struct local_entry){ .name = name, .value = value, .elem_type = elem_type }));
	return ctx->locals.items[ctx->locals.len - 1];
}

static struct local_entry find_local_entry(
	struct codegen_context *ctx,
	const char *name)
{
	for (size_t i = ctx->locals.len; i > 0; i--) {
		if (strcmp(ctx->locals.items[i - 1].name, name) == 0)
			return ctx->locals.items[i - 1];
	}
	return (struct local_entry){0};
}

static LLVMValueRef find_local(
	struct codegen_context *ctx, const char *name)
{
	for (size_t i = ctx->locals.len; i > 0; i--) {
		if (strcmp(ctx->locals.items[i - 1].name, name) == 0)
			return ctx->locals.items[i - 1].value;
	}
	return NULL;
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
	case HASTE_VL_RUNTIME:
		return codegen_expr(ctx, value.runtime);
	case HASTE_VL_NONE:
	case HASTE_VL_BAD:
	case HASTE_VL_UNINIT:
	case HASTE_VL_TYPE:
		unreachable();
	}
}

// ── Expression codegen ────────────────────────────────────────────

static LLVMValueRef codegen_cast(struct codegen_context *ctx, const struct haste_ast_cast *node)
{
	LLVMValueRef val = codegen_expr(ctx, node->expr);
	LLVMTypeRef target_type = llvm_type(ctx, node->base.type);
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

static LLVMValueRef codegen_value(struct codegen_context *ctx, const struct haste_ast_value *node)
{
	if (is_comptime_known(node->value)) {
		return llvm_value(ctx, node->value);
	}

	if (IS_RUNTIME(node->value)) {
		return codegen_expr(ctx, node->value.runtime);
	}
	unreachable();
}

static LLVMValueRef codegen_lvalue(struct codegen_context *ctx, const struct haste_ast_node *node);

static LLVMValueRef codegen_ident(struct codegen_context *ctx, const struct haste_ast_ident *node)
{
	LLVMValueRef ptr = codegen_lvalue(ctx, &node->base);
	return LLVMBuildLoad2(ctx->builder, llvm_type(ctx, node->base.type), ptr, node->value.chars);
}

static LLVMValueRef codegen_binary(struct codegen_context *ctx, const struct haste_ast_binary *node)
{
	LLVMValueRef lhs = codegen_expr(ctx, node->lhs);
	LLVMValueRef rhs = codegen_expr(ctx, node->rhs);

	switch (node->op) {
	case TK_PLUS:   return LLVMBuildAdd  (ctx->builder, lhs, rhs, "addtmp");
	case TK_MINUS:  return LLVMBuildSub  (ctx->builder, lhs, rhs, "subtmp");
	case TK_STAR:   return LLVMBuildMul  (ctx->builder, lhs, rhs, "multmp");
	case TK_FSLASH: return LLVMBuildSDiv (ctx->builder, lhs, rhs, "divtmp");
	default: unreachable();
	}
}

static LLVMValueRef codegen_unary(struct codegen_context *ctx, const struct haste_ast_unary *node)
{
	LLVMValueRef rhs = codegen_expr(ctx, node->rhs);
	switch (node->op) {
	case TK_MINUS: return LLVMBuildNeg(ctx->builder, rhs, "negtmp");
	case TK_PLUS:  return rhs;
	default: unreachable();
	}
}

static LLVMValueRef codegen_access(struct codegen_context *ctx, const struct haste_ast_access *node)
{
	LLVMValueRef ptr = codegen_lvalue(ctx, &node->base);
	return LLVMBuildLoad2(ctx->builder, llvm_type(ctx, node->base.type), ptr, node->field.chars);
}

static LLVMValueRef codegen_lvalue(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_IDENT: {
		const struct haste_ast_ident *ident = (const void*)node;
		struct local_entry local = find_local_entry(ctx, ident->value.chars);
		if (local.name != NULL) return local.value;
		LLVMValueRef global = LLVMGetNamedGlobal(ctx->module, ident->value.chars);
		if (global != NULL) return global;
		unreachable();
	}
	case ND_ACCESS: {
		const struct haste_ast_access *access = (const void*)node;
		LLVMValueRef ptr = codegen_lvalue(ctx, access->lhs);
		LLVMTypeRef struct_type = llvm_type(ctx, access->lhs->type);
		return LLVMBuildStructGEP2(ctx->builder, struct_type, ptr, (unsigned)access->field_index, access->field.chars);
	}
	default:
		unreachable();
	}
}

static LLVMValueRef codegen_func_call(struct codegen_context *ctx, const struct haste_ast_func_call *node)
{
	const char *fn_name = "";
	if (node->callee->kind == ND_IDENT) {
		fn_name = ((const struct haste_ast_ident*)node->callee)->value.chars;
	}

	LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, fn_name);
	if (fn == NULL) {
		fprintf(stderr, "error: function '%s' not found in module\n", fn_name);
		return LLVMConstInt(t_i32(ctx), 0, true);
	}

	// Count args
	size_t arg_count = 0;
	for (const struct haste_ast_func_call_arg *a = node->args; a; a = a->next)
		arg_count++;

	LLVMValueRef args[arg_count > 0 ? arg_count : 1];
	size_t i = 0;
	for (const struct haste_ast_func_call_arg *a = node->args; a; a = a->next) {
		args[i++] = codegen_expr(ctx, a->value);
	}

	return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fn), fn, args, (unsigned)arg_count, "calltmp");
}

static LLVMValueRef codegen_block(struct codegen_context *ctx, const struct haste_ast_block *node)
{
	LLVMValueRef last_val = NULL;
	if (node->stmts != NULL) {
		leach (struct haste_ast_node, stmt, node->stmts) {
			last_val = codegen_stmt(ctx, stmt);
		}
	}
	return last_val;
}

static LLVMValueRef codegen_return(struct codegen_context *ctx, const struct haste_ast_return *node)
{
	if (node->value != NULL) {
		const LLVMValueRef val = codegen_expr(ctx, node->value);
		return LLVMBuildRet(ctx->builder, val);
	}
	return LLVMBuildRetVoid(ctx->builder);
}

static LLVMValueRef codegen_expr(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VALUE:     return codegen_value    (ctx, (void*)node);
	case ND_CAST:      return codegen_cast     (ctx, (void*)node);
	case ND_GROUPING:  return codegen_expr     (ctx, ((const struct haste_ast_grouping*)node)->child);
	case ND_IDENT:     return codegen_ident    (ctx, (void*)node);
	case ND_BINARY:    return codegen_binary   (ctx, (void*)node);
	case ND_UNARY:     return codegen_unary    (ctx, (void*)node);
	case ND_ACCESS:    return codegen_access   (ctx, (void*)node);
	case ND_FUNC_CALL: return codegen_func_call(ctx, (void*)node);
	case ND_BLOCK:     return codegen_block    (ctx, (void*)node);
	case ND_RETURN:    return codegen_return   (ctx, (void*)node);
	default: unimplemented();
	}
}

static LLVMValueRef codegen_var(
	struct codegen_context *ctx,
	const struct haste_ast_var_decl *node,
	bool is_global);

static LLVMValueRef codegen_stmt(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_FUNC_DECL: unimplemented();
	case ND_VAR_DECL:  return codegen_var(ctx, (void*)node, false);
	default:           return codegen_expr(ctx, node);
	}
}

// ── Global declaration codegen ────────────────────────────────────

static LLVMValueRef codegen_var(struct codegen_context *ctx, const struct haste_ast_var_decl *node, bool is_global)
{
	if (node->is_explicitly_comptime) return 0;

	const char *name = node->name.chars;
	LLVMTypeRef type = llvm_type(ctx, node->base.type);
	LLVMValueRef init = node->value != NULL
		then codegen_expr(ctx, node->value)
		otherwise LLVMConstNull(type);

	LLVMValueRef symbol = {0};
	if (is_global) {
		symbol = LLVMAddGlobal(ctx->module, type, name);
		LLVMSetInitializer(symbol, init);
		LLVMSetGlobalConstant(symbol, node->is_constant);
	} else {
		LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, type, name);
		LLVMBuildStore(ctx->builder, init, alloca);
		struct local_entry local = push_local(ctx, name, alloca, type);
		symbol = local.value;
	}

	return symbol;
}

static LLVMValueRef codegen_func_decl(struct codegen_context *ctx, const struct haste_ast_func_decl *node)
{
	// Build function type: ret_type(param_types...)
	LLVMTypeRef return_type = llvm_type(ctx, node->base.type);

	// Count params
	size_t param_count = 0;
	leach (struct haste_ast_func_param, p, node->params) {
		param_count += p->name_count;
	}

	LLVMTypeRef param_types[param_count > 0 ? param_count : 1];
	size_t idx = 0;
	leach (struct haste_ast_func_param, p, node->params) {
		struct haste_type param_type = {0};
		if (p->type != NULL and p->type->kind == ND_VALUE) {
			struct haste_ast_value *val_node = (struct haste_ast_value*)p->type;
			param_type = into_type(VAL_TYPE(val_node->value.type));
		}
		for (size_t i = 0; i < p->name_count; i++) {
			param_types[idx++] = llvm_type(ctx, param_type);
		}
	}

	LLVMTypeRef fn_type = LLVMFunctionType(return_type, param_types, (unsigned)param_count, false);
	const char *name = node->name.chars;
	LLVMValueRef fn = LLVMAddFunction(ctx->module, name, fn_type);

	// Create entry basic block
	LLVMBasicBlockRef entry = LLVMAppendBasicBlock(fn, "entry");
	LLVMPositionBuilderAtEnd(ctx->builder, entry);

	// Save current function/restore on exit
	LLVMValueRef prev_func = ctx->current_func;
	ctx->current_func = fn;

	// Store params in locals (alloca + store)
	LLVMValueRef llvm_params = LLVMGetParam(fn, 0); // just for typing
	discard llvm_params;
	idx = 0;
	leach (struct haste_ast_func_param, p, node->params) {
		struct haste_type param_type = {0};
		if (p->type != NULL and p->type->kind == ND_VALUE) {
			struct haste_ast_value *val_node = (struct haste_ast_value*)p->type;
			param_type = into_type(VAL_TYPE(val_node->value.type));
		} else if (p->type != NULL) {
			param_type = p->type->type;
		}
		LLVMTypeRef llvm_param_type = llvm_type(ctx, param_type);
		for (size_t i = 0; i < p->name_count; i++) {
			const char *pname = p->names[i].chars;
			LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, llvm_param_type, pname);
			LLVMValueRef param_val = LLVMGetParam(fn, (unsigned)idx);
			LLVMBuildStore(ctx->builder, param_val, alloca);
			push_local(ctx, pname, alloca, llvm_param_type);
			idx++;
		}
	}

	// Generate body
	LLVMValueRef body_val = NULL;
	if (node->body != NULL) {
		body_val = codegen_expr(ctx, node->body);
	} else {
		LLVMBuildRetVoid(ctx->builder);
	}

	// If the function's last instruction is not a terminator, add default return
	LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->builder);
	if (current_block != NULL and LLVMGetBasicBlockTerminator(current_block) == NULL) {
		if (body_val != NULL) {
			LLVMBuildRet(ctx->builder, body_val);
		} else if (LLVMGetReturnType(fn_type) == t_void(ctx)) {
			LLVMBuildRetVoid(ctx->builder);
		} else {
			LLVMBuildRet(ctx->builder, LLVMConstNull(return_type));
		}
	}

	ctx->current_func = prev_func;
	return fn;
}

static Error codegen_global_node(struct codegen_context *ctx, const struct haste_ast_node *node)
{
	switch (node->kind) {
	case ND_VAR_DECL:
		codegen_var(ctx, (void*)node, true);
		break;
	case ND_FUNC_DECL:
		codegen_func_decl(ctx, (void*)node);
		break;
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
