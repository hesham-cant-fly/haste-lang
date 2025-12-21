#ifndef __AST_H
#define __AST_H

#include "token.h"
#include "arena.h"
#include "core/my_printer.h"
#include <stdint.h>
#include <stdbool.h>

#define __PRINT_SPAN my_print_span
#define __PRINT_LOCATION PRINT_NONE
// #define __PRINT_SPAN print_span
// #define __PRINT_LOCATION print_location 

/* Expressions */
enum AstOperator {
#define AST_OPERATOR_ENUM_DEF(X)                                        \
    X(AST_OPERATOR_PLUS)                                                \
    X(AST_OPERATOR_MINUS)                                               \
	X(AST_OPERATOR_STAR)                                                \
	X(AST_OPERATOR_FSLASH)												\
	X(AST_OPERATOR_POW)

    AST_OPERATOR_ENUM_DEF(X_ENUM)
};

struct AstUnaryExpr {
#define UNARY_EXPR_STRUCT_DEF(X)                                               \
  X(const struct AstExpr *, rhs, print_ast_expr_ptr)                                  \
  X(enum AstOperator, op, print_ast_operator)

  UNARY_EXPR_STRUCT_DEF(X_STRUCT)
};

struct AstBinaryExpr {
#define BINARY_EXPR_STRUCT_DEF(X)                                       \
    X(enum AstOperator, op, print_ast_operator)                              \
    X(const struct AstExpr *, lhs, print_ast_expr_ptr)                         \
    X(const struct AstExpr *, rhs, print_ast_expr_ptr)

    BINARY_EXPR_STRUCT_DEF(X_STRUCT)
};

enum AstExprKind {
#define EXPR_NODE_TAGGED_UNION_DEF(X)									\
	X(AST_EXPR_KIND_UNARY,      struct AstUnaryExpr,    unary, print_ast_unary_expr) \
	X(AST_EXPR_KIND_BINARY,     struct AstBinaryExpr,   bin, print_ast_binary_expr) \
	X(AST_EXPR_KIND_GROUPING,   const struct AstExpr*,  grouping, print_ast_expr_ptr) \
	X(AST_EXPR_KIND_IDENTIFIER, struct Span,            identifier, print_span) \
	X(AST_EXPR_KIND_INT_LIT,    int64_t,         int_lit, PRINT_SSIZE_T) \
	X(AST_EXPR_KIND_FLOAT_LIT,  double,          float_lit, PRINT_DOUBLE) \
	X(AST_EXPR_KIND_AUTO_TYPE,  uint8_t,         auto_type, PRINT_NONE)	\
	X(AST_EXPR_KIND_FLOAT_TYPE, uint8_t,         float_type, PRINT_NONE) \
    X(AST_EXPR_KIND_TYPEID_TYPE, uint8_t,        typeid_type, PRINT_NONE) \
    X(AST_EXPR_KIND_INT_TYPE,   uint8_t,         int_type, PRINT_NONE)

    EXPR_NODE_TAGGED_UNION_DEF(X_UNION_TAG)
};

struct AstExprNode {
    enum AstExprKind tag;
    union {
        EXPR_NODE_TAGGED_UNION_DEF(X_TAGGED_UNION)
    } as;
};

struct AstExpr {
#define EXPR_STRUCT_DEF(X)                                                     \
    X(struct Span, span, __PRINT_SPAN)                                                \
    X(struct Location, location, __PRINT_LOCATION)                                    \
    X(struct AstExprNode, node, print_ast_expr_node)

    EXPR_STRUCT_DEF(X_STRUCT)
};

/* Declarations */
struct AstVariableDecl {
#define AST_VARIABLE_DECL_STRUCT_DEF(X)                                        \
  X(struct Span, name, my_print_span)                                                 \
  X(const struct AstExpr *, type, print_ast_expr_ptr)                                 \
  X(const struct AstExpr *, value, print_ast_expr_ptr)

  AST_VARIABLE_DECL_STRUCT_DEF(X_STRUCT)
};

struct AstConstantDecl {
#define AST_CONSTANT_DECL_STRUCT_DEF(X)                                        \
  X(struct Span, name, my_print_span)                                                 \
  X(const struct AstExpr *, type, print_ast_expr_ptr)                                 \
  X(const struct AstExpr *, value, print_ast_expr_ptr)

  AST_CONSTANT_DECL_STRUCT_DEF(X_STRUCT)
};

enum AstDeclKind {
#define AST_DECL_NODE_TAGGED_UNION_DEF(X)                                         \
    X(AST_DECL_KIND_VARIABLE, struct AstVariableDecl, variable, print_ast_variable_decl) \
    X(AST_DECL_KIND_CONSTANT, struct AstConstantDecl, constant, print_ast_constant_decl)

    AST_DECL_NODE_TAGGED_UNION_DEF(X_UNION_TAG)
};

struct AstDeclNode {
    enum AstDeclKind tag;
    union {
        AST_DECL_NODE_TAGGED_UNION_DEF(X_TAGGED_UNION)
    } as;
};

struct AstDecl {
#define AST_DECL_STRUCT_DEF(X)                                                 \
    X(struct Span, span, __PRINT_SPAN)                                                \
    X(struct Location, location, __PRINT_LOCATION)                                    \
    X(struct AstDeclNode, node, print_ast_decl_node)

    AST_DECL_STRUCT_DEF(X_STRUCT)
};

/* Lists */
struct AstDeclarationListNode {
    struct AstDecl node;
    struct AstDeclarationListNode *next;
};

struct AstDeclarationList {
    const struct AstDeclarationListNode *head;
    struct AstDeclarationListNode *end;
};

struct ASTFile {
#define AST_FILE_STRUCT_DEF(X)                                                 \
    X(Arena, arena, PRINT_NONE)                                                \
	X(const char*, path, PRINT_STRING)                                         \
    X(struct AstDeclarationList, declarations, print_declaration_list)

    AST_FILE_STRUCT_DEF(X_STRUCT)
};

void ast_declaration_list_append(struct AstDeclarationList *list, const struct AstDeclarationListNode *node);

/* Other Methods */
struct Span get_declaration_name(const struct AstDecl declaration);

/* Printers */
GEN_PRINTER_DEF(struct AstDeclarationList, print_declaration_list);
GEN_PRINTER_DEF(struct ASTFile, print_ast_file);

GEN_PRINTER_DEF(enum AstOperator, print_ast_operator);
GEN_PRINTER_DEF(enum AstExprKind, print_ast_expr_kind);
GEN_PRINTER_DEF(struct AstExprNode, print_ast_expr_node);
GEN_PRINTER_DEF(struct AstExpr, print_ast_expr);
GEN_PRINTER_DEF(struct AstUnaryExpr, print_ast_unary_expr);
GEN_PRINTER_DEF(struct AstBinaryExpr, print_ast_binary_expr);

GEN_PRINTER_DEF(struct AstDecl, print_ast_decl);
GEN_PRINTER_DEF(enum AstDeclKind, print_ast_decl_kind);
GEN_PRINTER_DEF(struct AstDeclNode, print_ast_decl_node);
GEN_PRINTER_DEF(struct AstVariableDecl, print_ast_variable_decl);
GEN_PRINTER_DEF(struct AstConstantDecl, print_ast_constant_decl);

#endif // !__AST_H
