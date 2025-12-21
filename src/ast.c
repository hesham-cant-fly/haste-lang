#include "ast.h"
#include "core/my_printer.h"
#include <stdio.h>

void ast_declaration_list_append(struct AstDeclarationList *list, const struct AstDeclarationListNode *node)
{
	if (list->head == NULL && list->end == NULL)
	{
		list->head = node;
		list->end = (struct AstDeclarationListNode*)node;
		return;
	}
	list->end->next = (struct AstDeclarationListNode*)node;
	list->end = (struct AstDeclarationListNode*)node;
}


struct Span get_declaration_name(const struct AstDecl declaration)
{
#define X(__tag, _, __name, ...)				\
	case __tag:									\
		return node.as.__name.name;	   

	struct AstDeclNode node = declaration.node;
	switch (node.tag)
	{
		AST_DECL_NODE_TAGGED_UNION_DEF(X)
	}
#undef X
}

GEN_COSTOM_PRINTER_IMPL_START(struct AstDeclarationList, print_declaration_list)
{
	if (v.head == NULL)
	{
		fprintf(f, "[vide]");
		return;
	}
	fprintf(f, "[\n");
	size_t j = 0;
	const struct AstDeclarationListNode *current = v.head;

	while (current != NULL)
	{
		struct AstDeclarationListNode *next = current->next;
		__print_indent(f, i);
		fprintf(f, "[%zu] => ", j);
		print_ast_decl_indent(f, current->node, i + IDENTATION);
		fprintf(f, ",\n");
		current = next;
		j += 1;
	}
	__print_indent(f, i > 0 ? i - IDENTATION : i);
	fprintf(f, "]");
}
GEN_COSTOM_PRINTER_IMPL_END(struct AstDeclarationList, print_declaration_list)

GEN_STRUCT_PRINT_IMPL(struct ASTFile, print_ast_file, AST_FILE_STRUCT_DEF)

GEN_ENUM_PRINT_IMPL(enum AstOperator, print_ast_operator, AST_OPERATOR_ENUM_DEF)
GEN_TAGGED_UNION_TAG_PRINT_IMPL(enum AstExprKind, print_ast_expr_kind, EXPR_NODE_TAGGED_UNION_DEF)
GEN_TAGGED_UNION_PRINT_IMPL(struct AstExprNode, print_ast_expr_node, EXPR_NODE_TAGGED_UNION_DEF)

GEN_STRUCT_PRINT_IMPL(struct AstExpr, print_ast_expr, EXPR_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct AstUnaryExpr, print_ast_unary_expr, UNARY_EXPR_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct AstBinaryExpr, print_ast_binary_expr, BINARY_EXPR_STRUCT_DEF)

GEN_TAGGED_UNION_TAG_PRINT_IMPL(enum AstDeclKind, print_ast_decl_kind, AST_DECL_NODE_TAGGED_UNION_DEF)
GEN_TAGGED_UNION_PRINT_IMPL(struct AstDeclNode, print_ast_decl_node, AST_DECL_NODE_TAGGED_UNION_DEF)

GEN_STRUCT_PRINT_IMPL(struct AstDecl, print_ast_decl, AST_DECL_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct AstVariableDecl, print_ast_variable_decl, AST_VARIABLE_DECL_STRUCT_DEF)
GEN_STRUCT_PRINT_IMPL(struct AstConstantDecl, print_ast_constant_decl, AST_CONSTANT_DECL_STRUCT_DEF)
