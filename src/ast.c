#include "ast.h"
#include "analyzer.h"
#include "core/my_array.h"
#include "core/my_commons.h"
#include "token.h"
#include "type.h"
#include <stdio.h>

static void print_custom(const char *msg, FILE *f, struct BoolList *is_last);
static void print_none(FILE *f, struct BoolList *is_last);
static void write_tree_prefix(FILE *f, struct BoolList *is_last);

static void ast_binary_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);
static void ast_unary_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);
static void ast_grouping_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);
static void ast_identifier_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);
static void ast_literal_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);
static void ast_value_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted);

static void ast_const_declaration_print(struct AstNodeBase *decl, FILE *f, struct BoolList *is_last, int formatted);
static void ast_var_declaration_print(struct AstNodeBase *decl, FILE *f, struct BoolList *is_last, int formatted);

struct AstNodeInterface AstBinaryExpr       = {ast_binary_expr_print,
											   analyze_binary_expr};
struct AstNodeInterface AstUnaryExpr        = {ast_unary_expr_print,
											   analyze_unary_expr};
struct AstNodeInterface AstGroupingExpr     = {ast_grouping_expr_print,
											   analyze_grouping_expr};
struct AstNodeInterface AstIdentifierExpr   = {ast_identifier_expr_print,
											   analyze_identifier_expr};
struct AstNodeInterface AstLiteralExpr      = {ast_literal_expr_print,
											   analyze_literal_expr};
struct AstNodeInterface AstValueExpr        = {ast_value_expr_print,
										       NULL};

struct AstNodeInterface AstConstDeclaration = {ast_const_declaration_print,
											   analyze_const_decl};
struct AstNodeInterface AstVarDeclaration   = {ast_var_declaration_print,
											   analyze_var_decl};

void expr_print(struct AstExpr expr, FILE *f)
{
	struct BoolList is_last = {0};
	expr.v_table->print(expr.data, f, &is_last, 0);
	fprintf(f, "\n");
	arrfree(is_last);
}

void declaration_print(struct AstDeclaration decl, FILE* f)
{
	struct BoolList is_last = {0};
	decl.v_table->print(decl.data, f, &is_last, 0);
	fprintf(f, "\n");
	arrfree(is_last);
}

void ast_file_print(struct AstFile file, FILE *f)
{
	struct AstDeclaration *current = &file.declarations;
	while (current != NULL) {
		if (ast_is_none(*current)) break;
		struct AstDeclaration *next = current->next;
		declaration_print(*current, f);
		current = next;
	}
}

struct Value analyze_ast_expr(struct AstExpr expr,
							  struct Environment *env)
{
	return expr.v_table->analyze(expr.data, env);
}

struct Value analyze_ast_declaration(struct AstDeclaration decl,
							 struct Environment *env)
{
	return decl.v_table->analyze(decl.data, env);
}

error analyze_ast_file(struct AstFile file, struct Environment *env)
{
	struct AstDeclaration *current = &file.declarations;
	while (current != NULL && !ast_is_none(*current)) {
		struct AstDeclaration *next = current->next;
		struct Value result = analyze_ast_declaration(*current, env);
		if (is_poisoned(result)) {
			// At the moment I will just return on the first poisoned global delcaration.
			// I can make it continue if I want to later.
			return ERROR;
		}
		current = next;
	}

	return OK;
}

#define fmt(expr__) fmt_or(expr__, "None()")
#define fmt_or(expr__, msg__) \
	do { \
		formatted += 1; \
		arrpush(*is_last, false); \
		if (!ast_is_none((expr__))) (expr__).v_table->print((expr__).data, f, is_last, 0); \
		else print_custom((msg__), f, is_last); \
		fprintf(f, "\n"); \
	} while (0)
#define fmt_last(expr__) fmt_last_or(expr__, "None()")
#define fmt_last_or(expr__, msg__) \
	do { \
		if (formatted > 0) is_last->items[is_last->len - 1] = true; \
		else arrpush(*is_last, true); \
		if (!ast_is_none((expr__))) (expr__).v_table->print((expr__).data, f, is_last, 0); \
		else print_custom((msg__), f, is_last); \
		if (is_last->len != 0) arrpop(*is_last); \
	} while (0)

static void ast_binary_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstBinaryExpr *self = (struct AstBinaryExpr *)expr;

	write_tree_prefix(f, is_last);
	fprintf(f, "BinaryExpr(%.*s) %u:%u ", SPAN_ARG(self->op.span), LOCATION_ARG(self->base.location));
	print_typeln(f, expr->type);
	fmt(self->lhs);
	fmt_last(self->rhs);
}

static void ast_unary_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstUnaryExpr *self = (struct AstUnaryExpr *)expr;
	write_tree_prefix(f, is_last);
	fprintf(f, "UnaryExpr(%.*s) %u:%u ", SPAN_ARG(self->op.span), LOCATION_ARG(self->base.location));
	print_typeln(f, expr->type);
	fmt_last(self->rhs);
}

static void ast_grouping_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstGroupingExpr *self = (struct AstGroupingExpr *)expr;
	write_tree_prefix(f, is_last);
	fprintf(f, "GroupingExpr() %u:%u ", LOCATION_ARG(self->base.location));
	print_typeln(f, expr->type);
	fmt_last(self->child);
}

static void ast_identifier_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstIdentifierExpr *self = (struct AstIdentifierExpr *)expr;
	unused(formatted);

	write_tree_prefix(f, is_last);
	fprintf(f, "IdentifieExpr(`%.*s`) %u:%u ", SPAN_ARG(self->token.span), LOCATION_ARG(self->base.location));
	print_type(f, expr->type);
}

static void ast_literal_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstLiteralExpr *self = (struct AstLiteralExpr *)expr;
	unused(formatted);

	write_tree_prefix(f, is_last);
	fprintf(f, "LiteralExpr(%.*s) %u:%u ", SPAN_ARG(self->token.span), LOCATION_ARG(self->base.location));
	print_type(f, expr->type);
}

static void ast_value_expr_print(struct AstNodeBase *expr, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstValueExpr *self = (struct AstValueExpr*)expr;
	unused(formatted);

	write_tree_prefix(f, is_last);
	print_value(f, self->value);
}

static void ast_const_declaration_print(struct AstNodeBase *decl, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstConstDeclaration *self = (struct AstConstDeclaration *)decl;

	write_tree_prefix(f, is_last);
	fprintf(f, "ConstDeclaration(`%.*s`) %u:%u ", SPAN_ARG(self->name.span), LOCATION_ARG(self->base.location));
	print_typeln(f, decl->type);
	fmt_or(self->type, "Untyped()");
	fmt_last_or(self->value, "Uninit()");
}

static void ast_var_declaration_print(struct AstNodeBase *decl, FILE *f, struct BoolList *is_last, int formatted)
{
	struct AstVarDeclaration *self = (struct AstVarDeclaration *)decl;

	write_tree_prefix(f, is_last);
	fprintf(f, "VarDeclaration(`%.*s`) %u:%u ", SPAN_ARG(self->name.span), LOCATION_ARG(self->base.location));
	print_typeln(f, decl->type);
	fmt_or(self->type, "Untyped()");
	fmt_last_or(self->value, "Uninit()");
}

static void print_custom(const char *msg, FILE *f, struct BoolList *is_last)
{
	write_tree_prefix(f, is_last);
	fprintf(f, "%s", msg);
}

static void print_none(FILE *f, struct BoolList *is_last)
{
	print_custom("None()", f, is_last);
}

static void write_tree_prefix(
	FILE *f,
	struct BoolList *is_last)
{
	for (size_t i = 0; i + 1 < is_last->len; i += 1) {
		fprintf(f, "\t");
		if (!is_last->items[i]) {
			fprintf(f, "│");
		}
	}

	if (is_last->len != 0) {
		fprintf(f,
			"\t%s─",
			is_last->items[is_last->len - 1] ? "└" : "├"
		);
	}
}
