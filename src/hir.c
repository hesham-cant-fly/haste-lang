#include "hir.h"
#include "token.h"
#include "type.h"
#include "core/my_array.h"
#include <stdio.h>

void print_hir_instruction(FILE *f, HirInstruction instruction)
{
	switch (instruction.tag)
	{
	case HIR_NODE_END:
		fprintf(f, "end");
		break;
	case HIR_NODE_IDENTIFIER:
		fprintf(f, "ident(\"%.*s\")", SPAN_ARG(instruction.as.identifier));
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

	case HIR_NODE_START_DECLARATION:
		fprintf(
			f,
			"start_declaration(\"%.*s\", at: %%%zu)",
			SPAN_ARG(instruction.as.start_decl.name),
			instruction.as.start_decl.end
		);
		break;
	case HIR_NODE_CONSTANT_DECLARATION:
		fprintf(
			f,
			"decl_const(\n"
			"\tname: \"%.*s\",\n"
			"\tbegining: %%%zu,\n"
			"\texplicit_typing: %s,\n"
			"\tinitialized: %s,\n"
			")",
			SPAN_ARG(instruction.as.constant.name),
			instruction.as.constant.begining,
			BOOL_ARG(instruction.as.constant.explicit_typing),
			BOOL_ARG(instruction.as.variable.initialized)
		);
		break;
	case HIR_NODE_VARIABLE_DECLARATION:
		fprintf(
			f,
			"decl_var(\n"
			"\tname: \"%.*s\",\n"
			"\tbegining: %%%zu,\n"
			"\texplicit_typing: %s,\n"
			"\tinitialized: %s,\n"
			")",
			SPAN_ARG(instruction.as.variable.name),
			instruction.as.variable.begining,
			BOOL_ARG(instruction.as.variable.explicit_typing),
			BOOL_ARG(instruction.as.variable.initialized)
		);
		break;
	}
}

static void print_hoist_table(FILE* f, HirHoistEntry *hoist_table)
{
	fprintf(f, "{\n");
	for (size_t i=0; i<arrlen(hoist_table); i += 1)
	{
		HirHoistEntry entry = hoist_table[i];
		fprintf(f, "\t\"%.*s\": %%%zu,\n", SPAN_ARG(entry.key), entry.value);
	}
	fprintf(f, "}");
}

void print_hir(FILE *f, Hir hir)
{
	fprintf(f, "hoist_table: ");
	print_hoist_table(f, hir.hoist_table);
	fprintf(f, "\n");

	for (size_t i=0; i < arrlen(hir.instructions); i += 1)
	{
		HirInstruction instruction = hir.instructions[i];
		fprintf(f, "%%%010zu -> ", i);
		print_hir_instruction(f, instruction);
		fprintf(f, "\n");
	}
}

static HirInstruction hir_get(Hir* hir, const size_t at);
static size_t hir_len(Hir *hir);

static size_t hir_push_instruction(Hir *hir, const HirInstruction instruction);
static void hir_insert_instruction(Hir *hir, const size_t at, const HirInstruction instruction);
static void hoist_entry_append(HirHoistEntry* hoist_table, Span key, size_t value);

static error hoist_declaration(Hir *hir, const AstDecl declaration);
static error hoist_expr(const AstExpr *expr, Hir *hir);

error hoist_ast(ASTFile file, Hir *out)
{
	Hir hir = {0};
	hir.hoist_table = arrinit(HirHoistEntry);
	hir.instructions = arrinit(HirInstruction);

	const AstDeclarationListNode* current = file.declarations.head;
	while (current != NULL)
	{
		const AstDeclarationListNode* next = current->next;

		error err = hoist_declaration(&hir, current->node);
		if (err) return err;

		hoist_entry_append(
			hir.hoist_table,
			get_declaration_name(current->node),
			arrlen(hir.instructions) - 1
		);

		current = next;
	}

	*out = hir;
	return OK;
}

static error hoist_declaration(Hir *hir, const AstDecl declaration)
{
	const size_t begining = arrlen(hir->instructions);
	const size_t start_decl = begining;
	const AstDeclNode node = declaration.node;
	HirInstruction instruction = {0};
	instruction.span = declaration.span;
	instruction.location = declaration.location;

	switch (declaration.node.tag)
	{
	case AST_DECL_KIND_CONSTANT:
		if (node.as.constant.value != NULL)
		{
			error err = hoist_expr(node.as.constant.value, hir);
			if (err) return err;
		}
		if (node.as.constant.type != NULL)
		{
			error err = hoist_expr(node.as.constant.type, hir);
			if (err) return err;
		}

		if (hoist_entry_find(hir->hoist_table, node.as.constant.name) != NULL)
		{
			panic("");
		}

		instruction.tag = HIR_NODE_CONSTANT_DECLARATION;
		instruction.as.constant = (struct HirConstantDecl){
			.name = node.as.constant.name,
			.begining = begining,
			.explicit_typing = node.as.constant.type != NULL,
			.initialized = node.as.constant.value != NULL,
		};
		break;
	case AST_DECL_KIND_VARIABLE:
		if (node.as.variable.value != NULL)
		{
			error err = hoist_expr(node.as.variable.value, hir);
			if (err) return err;
		}
		if (node.as.variable.type != NULL)
		{
			error err = hoist_expr(node.as.variable.type, hir);
			if (err) return err;
		}

		if (hoist_entry_find(hir->hoist_table, node.as.variable.name) != NULL)
		{
			panic("");
		}

		instruction.tag = HIR_NODE_VARIABLE_DECLARATION;
		instruction.as.variable = (struct HirVariableDecl){
			.name = node.as.variable.name,
			.begining = begining,
			.explicit_typing = node.as.variable.type != NULL,
			.initialized = node.as.variable.value != NULL,
		};
		break;
	}

	{
		HirInstruction instruction = {0};
		instruction.span = declaration.span;
		instruction.location = declaration.location;
		instruction.tag = HIR_NODE_START_DECLARATION;
		instruction.as.start_decl.name = get_declaration_name(declaration);
		instruction.as.start_decl.end = hir_len(hir) + 1;

		hir_insert_instruction(hir, start_decl, instruction);
	}

	hir_push_instruction(hir, instruction);
	return OK;
}

static error hoist_expr(const AstExpr *expr, Hir *out)
{
	error err = OK;
	AstExprNode node = expr->node;
	HirInstruction instruction = {0};
	instruction.span = expr->span;
	instruction.location = expr->location;

	switch (node.tag)
	{
	case AST_EXPR_KIND_UNARY:
		err = hoist_expr(node.as.unary.rhs, out);
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
		err = hoist_expr(node.as.bin.lhs, out);
		if (err) return err;
		err = hoist_expr(node.as.bin.rhs, out);
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
		return hoist_expr(node.as.grouping, out);
	case AST_EXPR_KIND_IDENTIFIER:
		instruction.tag = HIR_NODE_IDENTIFIER;
		instruction.as.identifier = node.as.identifier;
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
	default:
		unreachable();
	}
_defer:
	hir_push_instruction(out, instruction);
	return err;
}

void deinit_hir(Hir hir)
{
	arrfree(hir.hoist_table);
	arrfree(hir.instructions);
}

HirHoistEntry* hoist_entry_find(HirHoistEntry* hoist_table, Span key)
{
	if (arrlen(hoist_table) == 0) return NULL;

	ssize_t left = 0;
	ssize_t right = arrlen(hoist_table) - 1;

    while (left <= right)
	{
        ssize_t mid = left + (right - left) / 2;
        HirHoistEntry* e = &hoist_table[mid];

        size_t min_len = span_len(key) < span_len(e->key) ? span_len(key) : span_len(e->key);
        int cmp = strncmp(e->key.start, key.start, min_len);

        if (cmp == 0)
		{
            if (span_len(e->key) == span_len(key))
			{
                return e;
            }
			else if (span_len(e->key) > span_len(key))
			{
                left = mid + 1;
            }
			else
			{
                right = mid - 1;
            }
        }
		else if (cmp > 0)
		{
            left = mid + 1;
        }
		else
		{
            right = mid - 1;
        }
    }

    return NULL;
}

static size_t hir_push_instruction(Hir *hir, HirInstruction instruction)
{
	arrpush(hir->instructions, instruction);
	return hir_len(hir) - 1;
}

static void hir_insert_instruction(Hir *hir, const size_t at, const HirInstruction instruction)
{
	const size_t len = hir_len(hir);
	if (at > len) panic("Insert Out-of-bound.");

	if (arrcap(hir->instructions) <= len + 1)
	{
		arrreserve(hir->instructions, len + 1);
	}

	arrsetlen(hir->instructions, len + 1);
	for (size_t i = len; i > at; i--) {
		hir->instructions[i] = hir_get(hir, i - 1);
	}

	hir->instructions[at] = instruction;
}

static HirInstruction hir_get(Hir* hir, const size_t at)
{
	if (at >= hir_len(hir))
	{
		panic("OUT OF BOUND: %zu >= %zu", at, hir_len(hir));
	}
	return hir->instructions[at];
}

static size_t hir_len(Hir *hir)
{
	return arrlen(hir->instructions);
}

static void hoist_entry_append(HirHoistEntry* hoist_table, Span key, size_t value)
{
	if (arrcap(hoist_table) <= arrlen(hoist_table))
	{
		arrreserve(hoist_table, arrlen(hoist_table) + 1);
	}

	ssize_t i = arrlen(hoist_table) - 1;
	for (; i >= 0; i -= 1)
	{
		HirHoistEntry e = hoist_table[i];
		size_t len = span_len(e.key);
		if (span_len(key) < len)
		{
			len = span_len(key);
		}
		if (strncmp(e.key.start, key.start, len) > 0)
		{
			break;
		}

		hoist_table[i + 1] = hoist_table[i];
	}

	arrsetlen(hoist_table, arrlen(hoist_table) + 1);
	hoist_table[i + 1] = (HirHoistEntry){
		.key = key,
		.value = value,
	};
}
