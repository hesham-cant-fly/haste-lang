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
		print_type(f, *instruction.as.type);
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
			"  name: \"%.*s\",\n"
			"  begining: %%%zu,\n"
			"  explicit_typing: %s,\n"
			"  initialized: %s,\n"
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
			"  name: \"%.*s\",\n"
			"  begining: %%%zu,\n"
			"  explicit_typing: %s,\n"
			"  initialized: %s,\n"
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
		fprintf(f, "  \"%.*s\": %%%zu,\n", SPAN_ARG(entry.key), entry.value);
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

static size_t hir_push_instruction(Hir *hir, HirInstruction instruction);
static void hoist_entry_append(HirHoistEntry* hoist_table, Span key, size_t value);
static error hoist_expr(const AstExpr *expr, Hir *out);

error hoist_ast(ASTFile file, Hir *out)
{
	Hir result = {0};
	result.hoist_table = arrinit(HirHoistEntry);
	result.instructions = arrinit(HirInstruction);

	const AstDeclarationListNode* current = file.declarations.head;
	while (current != NULL)
	{
		const size_t begining = arrlen(result.instructions);
		const AstDeclarationListNode* next = current->next;
		const AstDecl declaration = current->node;
		const AstDeclNode node = declaration.node;
		HirInstruction instruction = {0};
		instruction.span = declaration.span;
		instruction.location = declaration.location;
		size_t start_decl = 0;
		{
			HirInstruction instruction = {0};
			instruction.span = declaration.span;
			instruction.location = declaration.location;
			instruction.tag = HIR_NODE_START_DECLARATION;
			start_decl = hir_push_instruction(&result, instruction);
		}

		switch (declaration.node.tag)
		{
		case AST_DECL_KIND_CONSTANT: {
			if (node.as.constant.value != NULL)
			{
				error err = hoist_expr(node.as.constant.value, &result);
				if (err) return err;
			}
			if (node.as.constant.type != NULL)
			{
				error err = hoist_expr(node.as.constant.type, &result);
				if (err) return err;
			}

			if (hoist_entry_find(result.hoist_table, node.as.constant.name) != NULL)
			{
				panic("");
			}
			result.instructions[start_decl].as.start_decl.name = node.as.constant.name;
			instruction.tag = HIR_NODE_CONSTANT_DECLARATION;
			instruction.as.constant = (struct HirConstantDecl){
				.name = node.as.constant.name,
				.begining = begining,
				.explicit_typing = node.as.constant.type != NULL,
				.initialized = node.as.constant.value != NULL,
			};

			hoist_entry_append(
				result.hoist_table,
				node.as.constant.name,
				arrlen(result.instructions) - 1
			);
		} break;
		case AST_DECL_KIND_VARIABLE:
			if (node.as.variable.value != NULL)
			{
				error err = hoist_expr(node.as.variable.value, &result);
				if (err) return err;
			}
			if (node.as.variable.type != NULL)
			{
				error err = hoist_expr(node.as.variable.type, &result);
				if (err) return err;
			}

			if (hoist_entry_find(result.hoist_table, node.as.variable.name) != NULL)
			{
				panic("");
			}

			result.instructions[start_decl].as.start_decl.name = node.as.variable.name;
			instruction.tag = HIR_NODE_VARIABLE_DECLARATION;
			instruction.as.variable = (struct HirVariableDecl){
				.name = node.as.variable.name,
				.begining = begining,
				.explicit_typing = node.as.variable.type != NULL,
				.initialized = node.as.variable.value != NULL,
			};

			hoist_entry_append(
				result.hoist_table,
				node.as.variable.name,
				arrlen(result.instructions) - 1
			);
			break;
		}

		result.instructions[start_decl].as.start_decl.end = arrlen(result.instructions);

		hir_push_instruction(&result, instruction);
	   
		current = next;
	}

	*out = result;
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
		instruction.as.type = get_primitive_type(PRIMITIVE_AUTO);
		goto _defer;
	case AST_EXPR_KIND_FLOAT_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = get_primitive_type(PRIMITIVE_FLOAT);
		goto _defer;
	case AST_EXPR_KIND_INT_TYPE:
		instruction.tag = HIR_NODE_TYPE;
		instruction.as.type = get_primitive_type(PRIMITIVE_INT);
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
	return arrlen(hir->instructions) - 1;
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
