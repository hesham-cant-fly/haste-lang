#include "haste.h"

static const char *HASTE_AST_NODE_KIND[] =
{
	[ND_VALUE]    = "value",

	[ND_BINARY]   = "binary",
	[ND_UNARY]    = "unary",
	[ND_PRIMARY]  = "primary",
	[ND_GROUPING] = "grouping",

	[ND_CAST]     = "cast",

	[ND_STRUCT_TYPE]      = "struct_type",
	[ND_STRUCT_FIELD]     = "struct_field",
	[ND_STRUCT_LITERAL]   = "struct_literal",
	[ND_STRUCT_LIT_FIELD] = "struct_lit_field",

	[ND_VAR_DECL] = "var_decl",
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
	if (node->type.kind != 0) {
		printed_amount += sprint(file, "\"type\": \"");
		printed_amount += sprint(file, "{value}", node->type);
		printed_amount += sprint(file, "\",");
	}
	switch (node->kind) {
	case ND_VALUE:
		printed_amount += sprint(file, "\"value\": \"");
		printed_amount += sprint(file, "{value}", node->value);
		printed_amount += sprint(file, "\"");
		break;
	case ND_BINARY:
		printed_amount += sprint(file, "\"op\": \"{span}\",", token_to_span(node->op));
		printed_amount += sprint(file, "\"lhs\": ");
		printed_amount += print_haste_ast_node(file, node->lhs);
		printed_amount += sprint(file, ",");
		printed_amount += sprint(file, "\"rhs\": ");
		printed_amount += print_haste_ast_node(file, node->rhs);
		break;
	case ND_UNARY:
		printed_amount += sprint(file, "\"op\": \"{span}\",", token_to_span(node->op));
		printed_amount += sprint(file, "\"rhs\": ");
		printed_amount += print_haste_ast_node(file, node->rhs);
		break;
	case ND_PRIMARY:
		printed_amount += sprint(file, "\"token\": \"{span}\"", token_to_span(node->token));
		break;
	case ND_GROUPING:
		printed_amount += sprint(file, "\"body\": ");
		printed_amount += print_haste_ast_node(file, node->body);
		break;
	case ND_CAST:
		printed_amount += sprint(file, "\"to\": ");

		if (node->cast.to != NULL) printed_amount += print_haste_ast_node(file, node->cast.to);
		else printed_amount += sprint(file, "\"auto\"");
		printed_amount += sprint(file, ",");
		printed_amount += sprint(file, "\"expr\": ");
		printed_amount += print_haste_ast_node(file, node->cast.expr);
		break;
	case ND_VAR_DECL:
		printed_amount += sprint(file, "\"name\": \"{span}\",", token_to_span(node->variable.name));
		printed_amount += sprint(file, "\"type_node\": ");
		if (node->variable.type) printed_amount += print_haste_ast_node(file, node->variable.type);
		else printed_amount += sprint(file, "null");
		printed_amount += sprint(file, ",");
		printed_amount += sprint(file, "\"value\": ");
		if (node->variable.value) printed_amount += print_haste_ast_node(file, node->variable.value);
		else printed_amount += sprint(file, "null");
		break;
	case ND_STRUCT_TYPE:
		printed_amount += sprint(file, "\"fields\": ");
		printed_amount += print_haste_ast(file, node->struct_type.fields);
		break;
	case ND_STRUCT_FIELD:
		printed_amount += sprint(file, "\"name\": \"{span}\",", token_to_span(node->struct_field.name));
		printed_amount += sprint(file, "\"type\": ");
		if (node->struct_field.type) printed_amount += print_haste_ast_node(file, node->struct_field.type);
		else printed_amount += sprint(file, "null");
		printed_amount += sprint(file, ",");
		printed_amount += sprint(file, "\"default\": ");
		if (node->struct_field.default_value) printed_amount += print_haste_ast_node(file, node->struct_field.default_value);
		else printed_amount += sprint(file, "null");
		break;
	case ND_STRUCT_LITERAL:
		printed_amount += sprint(file, "\"type_expr\": ");
		if (node->struct_literal.type_expr) printed_amount += print_haste_ast_node(file, node->struct_literal.type_expr);
		else printed_amount += sprint(file, "null");
		printed_amount += sprint(file, ",");
		printed_amount += sprint(file, "\"fields\": ");
		printed_amount += print_haste_ast(file, node->struct_literal.fields);
		break;
	case ND_STRUCT_LIT_FIELD:
		printed_amount += sprint(file, "\"name\": \"{span}\",", token_to_span(node->struct_lit_field.name));
		printed_amount += sprint(file, "\"value\": ");
		printed_amount += print_haste_ast_node(file, node->struct_lit_field.value);
		break;
	}
	printed_amount += sprint(file, "}");
	return printed_amount;
}

struct haste_ast_node *node_into_value(
	struct Allocator allocator,
	struct haste_ast_node *node,
	struct haste_value value)
{
	if (node == NULL) {
		node = create(allocator, struct haste_ast_node, 0);
	}

	node->kind = ND_VALUE;
	node->value = value;
	node->type = typeof(value);

	return node;
}

int print_haste_ast(stream_t file, const struct haste_ast_node *root)
{
	int printed_amount = 0;
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
