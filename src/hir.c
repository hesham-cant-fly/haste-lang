#include "hir.h"
#include "ast.h"
#include "core/my_commons.h"
#include "token.h"
#include "type.h"
#include "core/my_array.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

void print_hir_instruction(FILE *f, struct HirInstruction instruction)
{
	switch (instruction.tag)
	{
	case HIR_NODE_END:
		fprintf(f, "end");
		break;
	case HIR_NODE_IDENTIFIER:
		fprintf(f, "ident(\"%s\")", instruction.as.identifier);
		break;
	case HIR_NODE_INTEGER:
		fprintf(f, "int(%ld)", instruction.as.integer);
		break;
	case HIR_NODE_FLOAT:
		fprintf(f, "float(%lf)", instruction.as.floating_point);
		break;
	case HIR_NODE_TYPE:
		fprintf(f, "type(");
		print_type(f, instruction.as.type);
		fprintf(f, ")");
		break;

	case HIR_NODE_UNARY_PLUS:
		fprintf(f, "unary_op(add)");
		break;
	case HIR_NODE_UNARY_MINUS:
		fprintf(f, "unary_op(sub)");
		break;

	case HIR_NODE_ADD:
		fprintf(f, "binary_op(add)");
		break;
	case HIR_NODE_SUB:
		fprintf(f, "binary_op(sub)");
		break;
	case HIR_NODE_MUL:
		fprintf(f, "binary_op(mul)");
		break;
	case HIR_NODE_DIV:
		fprintf(f, "binary_op(div)");
		break;
	case HIR_NODE_POW:
		fprintf(f, "binary_op(pow)");
		break;

	case HIR_NODE_CONSTANT_DECLARATION:
		fprintf(
			f,
			"decl_const(\n"
			"\tname: \"%s\",\n"
			"\texplicit_typing: %s,\n"
			"\tinitialized: %s,\n"
			")",
			instruction.as.constant.name,
			BOOL_ARG(instruction.as.constant.explicit_typing),
			BOOL_ARG(instruction.as.variable.initialized)
		);
		break;
	case HIR_NODE_VARIABLE_DECLARATION:
		fprintf(
			f,
			"decl_var(\n"
			"\tname: \"%s\",\n"
			"\texplicit_typing: %s,\n"
			"\tinitialized: %s,\n"
			")",
			instruction.as.variable.name,
			BOOL_ARG(instruction.as.variable.explicit_typing),
			BOOL_ARG(instruction.as.variable.initialized)
		);
		break;
	}
}

void print_hir_global(FILE* f, struct HirGlobal global)
{
	fprintf(f, "[");
	switch (global.visibility)
	{
	case HIR_PUBLIC:
		fprintf(f, "public");
		break;
	case HIR_PRIVATE:
		fprintf(f, "private");
		break;
	}
	fprintf(f, ",");
	fprintf(f, "type=%d", global.explicit_typing);
	fprintf(f, ",");
	fprintf(f, "init=%d", global.initialized);
	fprintf(f, "]\n");
	switch (global.kind)
	{
	case HIR_DECL_CONST:
		fprintf(f, "const ");
		break;
	case HIR_DECL_VAR:
		fprintf(f, "var ");
		break;
	}
	fprintf(f, "%s {\n", global.name);

	for (size_t i=0; i < global.instructions.len; i+=1)
	{
		fprintf(f, "\t%%%05zu -> ", i);
		print_hir_instruction(f, global.instructions.items[i]);
		fprintf(f, "\n");
	}

	fprintf(f, "}\n");
}

void print_hir(FILE *f, struct Hir hir)
{
	for (size_t i=0; i < hir.globals.len; i += 1)
	{
		struct HirGlobal global = hir.globals.items[i];
		print_hir_global(f, global);
	}
}

static struct HirInstruction hir_get(struct Hir* hir, const size_t at);
static size_t hir_len(struct Hir *hir);

static size_t push_instruction(struct HirGlobal* global, struct HirInstruction instruction);
static void append_global(struct Hir* hir, const struct HirGlobal global);

static const char* hoist_span(struct Hir* hir, const struct Span span);
static error hoist_global_declaration(struct Hir* self, const struct AstDecl declaration);
static error hoist_expr(struct HirInstructionList* self, struct Hir* hir, const struct AstExpr *expr);

error hoist_ast(struct ASTFile file, struct Hir *out)
{
	struct Hir hir = {0};
	hir.path = file.path;

	const struct AstDeclarationListNode* current = file.declarations.head;
	while (current != NULL)
	{
		const struct AstDeclarationListNode* next = current->next;

		error err = hoist_global_declaration(&hir, current->node);
		if (err) return err;

		current = next;
	}

	*out = hir;
	return OK;
}

static const char* hoist_span(struct Hir* hir, const struct Span span)
{
	const size_t len = span_len(span);
	char* result = arena_alloc(&hir->string_pool, len + 1);

	memcpy(result, span.start, len);
	result[len] = 0x0;

	return result;
}

static error hoist_global_declaration(struct Hir* self, const struct AstDecl declaration)
{
	const struct AstDeclNode node = declaration.node;

	struct HirGlobal global = {0};
	global.kind = HIR_DECL_CONST;
	global.visibility = HIR_PUBLIC;

	switch (declaration.node.tag)
	{
	case AST_DECL_KIND_CONSTANT:
		global.kind = HIR_DECL_CONST;
		if (node.as.constant.value != NULL)
		{
			hoist_expr(&global.instructions, self, node.as.constant.value);
			global.initialized = true;
		}
		if (node.as.constant.type != NULL)
		{
			hoist_expr(&global.instructions, self, node.as.constant.type);
			global.explicit_typing = true;
		}

		global.name = hoist_span(self, node.as.constant.name);
		break;
	case AST_DECL_KIND_VARIABLE:
		global.kind = HIR_DECL_VAR;
		if (node.as.variable.value != NULL)
		{
			hoist_expr(&global.instructions, self, node.as.variable.value);
			global.initialized = true;
		}
		if (node.as.variable.type != NULL)
		{
			hoist_expr(&global.instructions, self, node.as.variable.type);
			global.explicit_typing = true;
		}

		global.name = hoist_span(self, node.as.variable.name);
		break;
	}

	append_global(self, global);

	return OK;
}

static error hoist_expr(struct HirInstructionList* self, struct Hir* hir, const struct AstExpr *expr)
{
	error err = OK;
	struct AstExprNode node = expr->node;
	struct HirInstruction instruction = {0};
	instruction.span = expr->span;
	instruction.location = expr->location;

	switch (node.tag)
	{
	case AST_EXPR_KIND_UNARY:
		err = hoist_expr(self, hir, node.as.unary.rhs);
		if (err) return err;
		switch (node.as.unary.op)
		{
		case AST_OPERATOR_PLUS:
			instruction.tag = HIR_NODE_UNARY_PLUS;
			break;
		case AST_OPERATOR_MINUS:
			instruction.tag = HIR_NODE_UNARY_MINUS;
			break;
		default:
			unreachable();
		}
		goto _defer;
	case AST_EXPR_KIND_BINARY:
		err = hoist_expr(self, hir, node.as.bin.lhs);
		if (err) return err;
		err = hoist_expr(self, hir, node.as.bin.rhs);
		if (err) return err;

		switch (node.as.bin.op)
		{
		case AST_OPERATOR_PLUS:
			instruction.tag = HIR_NODE_ADD;
			break;
		case AST_OPERATOR_MINUS:
			instruction.tag = HIR_NODE_SUB;
			break;
		case AST_OPERATOR_STAR:
			instruction.tag = HIR_NODE_MUL;
			break;
		case AST_OPERATOR_FSLASH:
			instruction.tag = HIR_NODE_DIV;
			break;
		case AST_OPERATOR_POW:
			instruction.tag = HIR_NODE_POW;
			break;
		default: unreachable();
		}
		goto _defer;
	case AST_EXPR_KIND_GROUPING:
		return hoist_expr(self, hir, node.as.grouping);
	case AST_EXPR_KIND_IDENTIFIER:
		instruction.tag = HIR_NODE_IDENTIFIER;
		instruction.as.identifier = hoist_span(hir, node.as.identifier);
		goto _defer;
	case AST_EXPR_KIND_INT_LIT:
		instruction.tag = HIR_NODE_INTEGER;
		instruction.as.integer = node.as.int_lit;
		goto _defer;
	case AST_EXPR_KIND_FLOAT_LIT:
		instruction.tag = HIR_NODE_FLOAT;
		instruction.as.floating_point = node.as.float_lit;
		goto _defer;
	case AST_EXPR_KIND_AUTO_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = TYPE_AUTO;
		goto _defer;
	case AST_EXPR_KIND_FLOAT_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = TYPE_FLOAT;
		goto _defer;
	case AST_EXPR_KIND_INT_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = TYPE_INT;
		goto _defer;
	case AST_EXPR_KIND_TYPEID_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = TYPE_TYPEID;
		goto _defer;
	}
_defer:
	arrpush(*self, instruction);
	return err;
}

void deinit_hir(struct Hir hir)
{
	arena_free(&hir.string_pool);
	arreach(hir.globals, i) {
		arrfree(hir.globals.items[i].instructions);
	}
	arrfree(hir.globals);
}

static size_t hir_push_instruction(struct HirGlobal* global, struct HirInstruction instruction)
{
	arrpush(global->instructions, instruction);
	return global->instructions.len - 1;
}

struct HirGlobal* hir_find_global(struct HirGlobalList globals, const char* name)
{
	if (globals.len == 0) return NULL;

	ssize_t left = 0;
	ssize_t right = globals.len - 1;

	while (left <= right) {
		ssize_t mid = left + (right - left) / 2;
		struct HirGlobal* e = &globals.items[mid];

		const int cmp = strcmp(e->name, name);

		if (cmp == 0)
			return e;
		else if (cmp > 0)
			left = mid + 1;
		else
			right = mid - 1;
	}

	return NULL;
}

static void append_global(struct Hir* self, const struct HirGlobal global)
{
	arrreserve(self->globals, self->globals.len + 1);

	ssize_t i = self->globals.len - 1;
	for (; i >= 0; i -= 1) {
		struct HirGlobal e = self->globals.items[i];
		const int cmp = strcmp(e.name, global.name);
		if (cmp > 0) break;

		self->globals.items[i + 1] = e;
	}

	self->globals.len += 1;
	self->globals.items[i + 1] = global;
}
