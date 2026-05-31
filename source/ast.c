#include "haste.h"
#include "my_stream.h"

static const char *HASTE_AST_NODE_KIND[] =
{
	[ND_VALUE]    = "value",

	[ND_BINARY]   = "binary",
	[ND_UNARY]    = "unary",
	[ND_ACCESS]   = "access",
	[ND_PRIMARY]  = "primary",
	[ND_GROUPING] = "grouping",
	[ND_DISTINCT] = "distinct",

	[ND_CAST]     = "cast",

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
			printed_amount += sprint(file, "\"lhs\": {ast},\"rhs\": {token:##}", n->lhs, n->rhs);
		}
		break;
	case ND_PRIMARY:
		{
			const struct haste_ast_primary *n = (const struct haste_ast_primary*)node;
			printed_amount += sprint(file, "\"token\": {token:##}", n->token);
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
				printed_amount += sprint(file, "{token:##}", n->names[i]);
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
