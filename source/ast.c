#include "haste.h"
#include "my_stream.h"

static const char *HASTE_AST_NODE_KIND[] =
{
	[ND_VALUE]      = "value",

	[ND_INTEGER_LIT] = "integer_lit",
	[ND_FLOAT_LIT]   = "float_lit",
	[ND_STRING_LIT]  = "string_lit",
	[ND_IDENT]       = "ident",

	[ND_BINARY]   = "binary",
	[ND_UNARY]    = "unary",
	[ND_ACCESS]   = "access",
	[ND_INT_BITS] = "int_bits",
	[ND_UINT_BITS]= "uint_bits",
	[ND_GROUPING] = "grouping",
	[ND_DISTINCT] = "distinct",

	[ND_CAST]     = "cast",

	[ND_STRING]   = "string",
	[ND_CSTR]     = "cstr",
	[ND_INT]      = "int",
	[ND_UINT]     = "uint",
	[ND_FLOAT]    = "float",
	[ND_USIZE]    = "usize",
	[ND_VOID]     = "void",
	[ND_AUTO]     = "auto",
	[ND_TYPE]     = "type",

	[ND_STRUCT_TYPE]      = "struct_type",
	[ND_STRUCT_FIELD]     = "struct_field",
	[ND_STRUCT_LITERAL]   = "struct_literal",
	[ND_STRUCT_LIT_FIELD] = "struct_lit_field",

	[ND_VAR_DECL]   = "var_decl",
};

static int print_haste_ast_node_kind(stream_t file, const enum haste_ast_node_kind kind)
{
	return sprint(file, "{s}", HASTE_AST_NODE_KIND[kind]);
}

static int print_haste_ast_node(stream_t file, const struct haste_ast_node *node)
{
	int printed_amount = 0;
	printed_amount += sprint(file, "{");
	printed_amount += sprint(file, "\"kind\": \"");
	printed_amount += print_haste_ast_node_kind(file, node->kind);
	printed_amount += sprint(file, "\",");
	if (node->type.value.kind != 0) {
		printed_amount += sprint(file, "\"type\": \"");
		printed_amount += sprint(file, "{value}", node->type);
		printed_amount += sprint(file, "\",");
	}
	switch (node->kind) {
	case ND_VALUE:
		{
			const struct haste_ast_value *n = (const struct haste_ast_value*)node;
			printed_amount += sprint(file, "\"value\": \"");
			printed_amount += sprint(file, "{value}", n->value);
			printed_amount += sprint(file, "\"");
		}
		break;
	case ND_BINARY:
		{
			const struct haste_ast_binary *n = (const struct haste_ast_binary*)node;
			printed_amount += sprint(file, "\"op\": \"{string}\",", as_string(n->op));
			printed_amount += sprint(file, "\"lhs\": ");
			printed_amount += print_haste_ast_node(file, n->lhs);
			printed_amount += sprint(file, ",");
			printed_amount += sprint(file, "\"rhs\": ");
			printed_amount += print_haste_ast_node(file, n->rhs);
		}
		break;
	case ND_UNARY:
		{
			const struct haste_ast_unary *n = (const struct haste_ast_unary*)node;
			printed_amount += sprint(file, "\"op\": \"{string}\",", as_string(n->op));
			printed_amount += sprint(file, "\"rhs\": ");
			printed_amount += print_haste_ast_node(file, n->rhs);
		}
		break;
	case ND_ACCESS:
		{
			const struct haste_ast_access *n = (const struct haste_ast_access*)node;
			printed_amount += sprint(file, "\"lhs\": {ast},\"field\": \"{string}\"", n->lhs, n->field);
		}
		break;
	case ND_DISTINCT:
	case ND_GROUPING:
		{
			const struct haste_ast_grouping *n = (const struct haste_ast_grouping*)node;
			printed_amount += sprint(file, "\"body\": ");
			printed_amount += print_haste_ast_node(file, n->child);
		}
		break;
	case ND_CAST:
		{
			const struct haste_ast_cast *n = (const struct haste_ast_cast*)node;
			printed_amount += sprint(file, "\"to\": ");
			if (n->to != NULL) printed_amount += print_haste_ast_node(file, n->to);
			else printed_amount += sprint(file, "\"auto\"");
			printed_amount += sprint(file, ",");
			printed_amount += sprint(file, "\"expr\": ");
			printed_amount += print_haste_ast_node(file, n->expr);
		}
		break;
	case ND_VAR_DECL:
		{
			const struct haste_ast_var_decl *n = (const struct haste_ast_var_decl*)node;
			printed_amount += sprint(file, "\"name\": \"{string}\",", as_string(n->name));
			printed_amount += sprint(file, "\"type_node\": ");
			if (n->type) printed_amount += print_haste_ast_node(file, n->type);
			else printed_amount += sprint(file, "null");
			printed_amount += sprint(file, ",");
			printed_amount += sprint(file, "\"value\": ");
			if (n->value) printed_amount += print_haste_ast_node(file, n->value);
			else printed_amount += sprint(file, "null");
		}
		break;
	case ND_INTEGER_LIT:
		{
			const struct haste_ast_integer_lit *n = (const struct haste_ast_integer_lit*)node;
			printed_amount += sprint(file, "\"value\": {i64}", n->value);
		}
		break;
	case ND_FLOAT_LIT:
		{
			const struct haste_ast_float_lit *n = (const struct haste_ast_float_lit*)node;
			printed_amount += sprint(file, "\"value\": {lf}", n->value);
		}
		break;
	case ND_STRING_LIT:
		{
			const struct haste_ast_string_lit *n = (const struct haste_ast_string_lit*)node;
			printed_amount += sprint(file, "\"value\": \"{string:#}\"", n->value);
		}
		break;
	case ND_IDENT:
		{
			const struct haste_ast_ident *n = (const struct haste_ast_ident*)node;
			printed_amount += sprint(file, "\"value\": \"{string:#}\"", n->value);
		}
		break;
	case ND_INT_BITS:
		{
			const struct haste_ast_int_bits *n = (const struct haste_ast_int_bits*)node;
			printed_amount += sprint(file, "\"bits\": {u32}", n->bits);
		}
		break;
	case ND_UINT_BITS:
		{
			const struct haste_ast_uint_bits *n = (const struct haste_ast_uint_bits*)node;
			printed_amount += sprint(file, "\"bits\": {u32}", n->bits);
		}
		break;
	case ND_STRUCT_TYPE:
		{
			const struct haste_ast_struct_type *n = (const struct haste_ast_struct_type*)node;
			printed_amount += sprint(file, "\"fields\": ");
			printed_amount += print_haste_ast(file, (const struct haste_ast_node*)n->fields);
		}
		break;
	case ND_STRUCT_FIELD:
		{
			const struct haste_ast_struct_field *n = (const struct haste_ast_struct_field*)node;
			printed_amount += sprint(file, "\"names\": [");
			for (size_t i=0; i < n->name_count; i += 1) {
				printed_amount += sprint(file, "\"{string}\"", n->names[i]);
				if (i != (n->name_count - 1)) {
					printed_amount += sprint(file, ",");
				}
			}
			printed_amount += sprint(file, "],");
			printed_amount += sprint(file, "\"type\": ");
			if (n->type) printed_amount += print_haste_ast_node(file, n->type);
			else printed_amount += sprint(file, "null");
			printed_amount += sprint(file, ",");
			printed_amount += sprint(file, "\"default\": ");
			if (n->default_value) printed_amount += print_haste_ast_node(file, n->default_value);
			else printed_amount += sprint(file, "null");
		}
		break;
	case ND_STRUCT_LITERAL:
		{
			const struct haste_ast_struct_literal *n = (const struct haste_ast_struct_literal*)node;
			printed_amount += sprint(file, "\"type_expr\": ");
			if (n->type_expr) printed_amount += print_haste_ast_node(file, n->type_expr);
			else printed_amount += sprint(file, "null");
			printed_amount += sprint(file, ",");
			printed_amount += sprint(file, "\"fields\": ");
			if (n->fields == NULL) printed_amount += sprint(file, "[]");
			else printed_amount += print_haste_ast(file, (const struct haste_ast_node*)n->fields);
		}
		break;
	case ND_STRUCT_LIT_FIELD:
		{
			const struct haste_ast_struct_lit_field *n = (const struct haste_ast_struct_lit_field*)node;
			printed_amount += sprint(file, "\"name\": \"{string}\",", as_string(n->name));
			printed_amount += sprint(file, "\"value\": ");
			printed_amount += print_haste_ast_node(file, n->value);
		}
		break;
	case ND_STRING:
	case ND_CSTR:
	case ND_INT:
	case ND_UINT:
	case ND_FLOAT:
	case ND_USIZE:
	case ND_VOID:
	case ND_AUTO:
	case ND_TYPE:
		break;
	}
	printed_amount += sprint(file, "}");
	return printed_amount;
}

void *node_into_value(
	struct Allocator allocator,
	void *nd,
	struct haste_value value)
{
	struct haste_ast_value *node = nd;
	if (node == NULL) {
		node = create(allocator, struct haste_ast_value, 0);
	}

	node->base.kind = ND_VALUE;
	node->value = value;
	node->base.type = typeof_value(value);

	return node;
}

int print_haste_ast(stream_t file, const struct haste_ast_node *root)
{
	int printed_amount = 0;
	if (root->next == NULL) {
		printed_amount += print_haste_ast_node(file, root);
		return printed_amount;
	}
	printed_amount += sprint(file, "[");
	for (const struct haste_ast_node *current = root;
		 current != NULL;
		 current = current->next) {
		if (current != root) {
			printed_amount += sprint(file, ",");
		}
		printed_amount += print_haste_ast_node(file, current);
	}
	printed_amount += sprint(file, "]");
	return printed_amount;
}

bool node_is_declaration(const struct haste_ast_node *node)
{
	return node->kind == ND_VAR_DECL;
}
