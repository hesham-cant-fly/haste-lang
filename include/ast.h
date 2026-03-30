#ifndef __AST_H
#define __AST_H

#include "analyzer.h"
#include "ast.base.h"
#include "error.h"
#include "token.h"
#include "arena.h"
#include "type.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define make_expr(type_name__, ...) \
	(struct AstExpr) { \
		.v_table = &(type_name__), \
		.data = (__VA_ARGS__), \
		.next = NULL, \
	}
#define make_decl(type_name__, ...) \
	(struct AstDeclaration) { \
		.v_table = &(type_name__), \
		.data = (__VA_ARGS__), \
		.next = NULL, \
	}
#define ast_is_none(node__) ((node__).v_table == NULL)

#define get_ast_base(node__) (*((struct AstNodeBase*)(node__).data))
#define get_location(node__) ((get_ast_base(node__)).location)
#define get_span(node__) ((get_ast_base(node__)).span)

/* Expressions */
struct AstBinaryOperator {
	struct Location location;
	struct Span span;
	enum AstBinaryOperationKind {
		AST_BIN_ADD,
		AST_BIN_SUB,
		AST_BIN_MUL,
		AST_BIN_DIV,
		AST_BIN_POW,
	} op;
};

extern struct AstNodeInterface AstBinaryExpr;
struct AstBinaryExpr {
	struct AstNodeBase base;

	struct AstBinaryOperator op;
	struct AstExpr lhs;
	struct AstExpr rhs;
};

struct AstUnaryOperator {
	struct Location location;
	struct Span span;
	enum {
		AST_UN_NEGATE,
	} op;
};

extern struct AstNodeInterface AstUnaryExpr;
struct AstUnaryExpr {
	struct AstNodeBase base;

	struct AstUnaryOperator op;
	struct AstExpr rhs;
};

extern struct AstNodeInterface AstGroupingExpr;
struct AstGroupingExpr {
	struct AstNodeBase base;

	struct AstExpr child;
};

extern struct AstNodeInterface AstIdentifierExpr;
struct AstIdentifierExpr {
	struct AstNodeBase base;

	struct Token token;
};

extern struct AstNodeInterface AstLiteralExpr;
struct AstLiteralExpr {
	struct AstNodeBase base;

	struct Token token;
};

extern struct AstNodeInterface AstValueExpr;
struct AstValueExpr {
	struct AstNodeBase base;

	struct Value value;
};

/* Declarations */
struct AstDeclaration {
	struct AstNodeInterface *v_table;
	struct AstNodeBase *data;
	struct AstDeclaration *next;
};

extern struct AstNodeInterface AstConstDeclaration;
struct AstConstDeclaration {
	struct AstNodeBase base;

	struct Token name;
	struct AstExpr type;
	struct AstExpr value;
};

extern struct AstNodeInterface AstVarDeclaration;
struct AstVarDeclaration {
	struct AstNodeBase base;

	struct Token name;
	struct AstExpr type;
	struct AstExpr value;
};

struct AstFile {
	const char *path;
	struct AstDeclaration declarations;
};

void expr_print(struct AstExpr expr, FILE* f);
void declaration_print(struct AstDeclaration decl, FILE* f);
void ast_file_print(struct AstFile file, FILE *f);

struct Value analyze_ast_expr(struct AstExpr expr,
							  struct Environment *env);
struct Value analyze_ast_declaration(struct AstDeclaration decl,
									 struct Environment *env);
error analyze_ast_file(struct AstFile file, struct Environment *env);

#endif // !__AST_H
