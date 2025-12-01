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

typedef struct ASTFile ASTFile;

typedef struct AstExpr AstExpr;
typedef struct AstUnaryExpr AstUnaryExpr;
typedef struct AstBinaryExpr AstBinaryExpr;
typedef struct AstExprNode AstExprNode;

typedef struct AstDecl AstDecl;
typedef struct AstVariableDecl AstVariableDecl;
typedef struct AstConstantDecl AstConstantDecl;
typedef struct AstDeclNode AstDeclNode;

/* Expressions */
typedef enum AstOperator {
#define AST_OPERATOR_ENUM_DEF(X)                                        \
    X(AST_OPERATOR_PLUS)                                                \
    X(AST_OPERATOR_MINUS)                                               \
    X(AST_OPERATOR_STAR)                                                \
    X(AST_OPERATOR_FSLASH)

    AST_OPERATOR_ENUM_DEF(X_ENUM)
} AstOperator;

struct AstUnaryExpr {
#define UNARY_EXPR_STRUCT_DEF(X)                                        \
    X(AstOperator, op, print_ast_operator)                              \
    X(const AstExpr *, rhs, print_ast_expr_ptr)

    UNARY_EXPR_STRUCT_DEF(X_STRUCT)
};

struct AstBinaryExpr {
#define BINARY_EXPR_STRUCT_DEF(X)                                       \
    X(AstOperator, op, print_ast_operator)                              \
    X(const AstExpr *, lhs, print_ast_expr_ptr)                         \
    X(const AstExpr *, rhs, print_ast_expr_ptr)

    BINARY_EXPR_STRUCT_DEF(X_STRUCT)
};

typedef enum AstExprKind {
#define EXPR_NODE_TAGGED_UNION_DEF(X)                                            \
    X(AST_EXPR_KIND_UNARY, AstUnaryExpr, unary, print_ast_unary_expr)            \
    X(AST_EXPR_KIND_BINARY, AstBinaryExpr, bin, print_ast_binary_expr)           \
    X(AST_EXPR_KIND_GROUPING, const AstExpr *, grouping, print_ast_expr_ptr)     \
    X(AST_EXPR_KIND_INT_LIT, int64_t, int_lit, PRINT_SSIZE_T)                    \
    X(AST_EXPR_KIND_FLOAT_LIT, double, float_lit, PRINT_DOUBLE)                  \
    X(AST_EXPR_KIND_AUTO_TYPE, uint8_t, auto_type, PRINT_NONE)                   \
    X(AST_EXPR_KIND_FLOAT_TYPE, uint8_t, float_type, PRINT_NONE)                 \
    X(AST_EXPR_KIND_INT_TYPE, uint8_t, int_type, PRINT_NONE)

    EXPR_NODE_TAGGED_UNION_DEF(X_UNION_TAG)
} AstExprKind;

struct AstExprNode {
    AstExprKind tag;
    union {
        EXPR_NODE_TAGGED_UNION_DEF(X_TAGGED_UNION)
    } as;
};

struct AstExpr {
#define EXPR_STRUCT_DEF(X)                                                     \
    X(Span, span, __PRINT_SPAN)                                                \
    X(Location, location, __PRINT_LOCATION)                                    \
    X(AstExprNode, node, print_ast_expr_node)

    EXPR_STRUCT_DEF(X_STRUCT)
};

/* Declarations */
struct AstVariableDecl {
#define AST_VARIABLE_DECL_STRUCT_DEF(X)                                        \
  X(Span, name, my_print_span)                                                 \
  X(const AstExpr *, type, print_ast_expr_ptr)                                 \
  X(const AstExpr *, value, print_ast_expr_ptr)

  AST_VARIABLE_DECL_STRUCT_DEF(X_STRUCT)
};

struct AstConstantDecl {
#define AST_CONSTANT_DECL_STRUCT_DEF(X)                                        \
  X(Span, name, my_print_span)                                                 \
  X(const AstExpr *, type, print_ast_expr_ptr)                                 \
  X(const AstExpr *, value, print_ast_expr_ptr)

  AST_CONSTANT_DECL_STRUCT_DEF(X_STRUCT)
};

typedef enum AstDeclKind {
#define AST_DECL_NODE_TAGGED_UNION_DEF(X)                                         \
    X(AST_DECL_KIND_VARIABLE, AstVariableDecl, variable, print_ast_variable_decl) \
    X(AST_DECL_KIND_CONSTANT, AstConstantDecl, constant, print_ast_constant_decl)

    AST_DECL_NODE_TAGGED_UNION_DEF(X_UNION_TAG)
} AstDeclKind;

struct AstDeclNode {
    AstDeclKind tag;
    union {
        AST_DECL_NODE_TAGGED_UNION_DEF(X_TAGGED_UNION)
    } as;
};

struct AstDecl {
#define AST_DECL_STRUCT_DEF(X)                                                 \
    X(Span, span, __PRINT_SPAN)                                                \
    X(Location, location, __PRINT_LOCATION)                                    \
    X(AstDeclNode, node, print_ast_decl_node)

    AST_DECL_STRUCT_DEF(X_STRUCT)
};

/* Lists */
typedef struct AstDeclarationListNode {
    AstDecl node;
    struct AstDeclarationListNode *next;
} AstDeclarationListNode;

typedef struct AstDeclarationList {
    const AstDeclarationListNode *head;
    AstDeclarationListNode *end;
} AstDeclarationList;

struct ASTFile {
#define AST_FILE_STRUCT_DEF(X)                                                 \
    X(Arena, arena, PRINT_NONE)                                                \
    X(AstDeclarationList, declarations, print_declaration_list)

    AST_FILE_STRUCT_DEF(X_STRUCT)
};

void ast_declaration_list_append(AstDeclarationList *list, const AstDeclarationListNode *node);

/* Printers */
GEN_PRINTER_DEF(AstDeclarationList, print_declaration_list);
GEN_PRINTER_DEF(ASTFile, print_ast_file);

GEN_PRINTER_DEF(AstOperator, print_ast_operator);
GEN_PRINTER_DEF(AstExprKind, print_ast_expr_kind);
GEN_PRINTER_DEF(AstExprNode, print_ast_expr_node);
GEN_PRINTER_DEF(AstExpr, print_ast_expr);
GEN_PRINTER_DEF(AstUnaryExpr, print_ast_unary_expr);
GEN_PRINTER_DEF(AstBinaryExpr, print_ast_binary_expr);

GEN_PRINTER_DEF(AstDecl, print_ast_decl);
GEN_PRINTER_DEF(AstDeclKind, print_ast_decl_kind);
GEN_PRINTER_DEF(AstDeclNode, print_ast_decl_node);
GEN_PRINTER_DEF(AstVariableDecl, print_ast_variable_decl);
GEN_PRINTER_DEF(AstConstantDecl, print_ast_constant_decl);

#endif // !__AST_H
