/** HIR, stands for Haste Intermediant Representation. is an Untyped
  * Stack Machine Code designed to make the analysis and compile time
  * execution Easy and simple.
  */
#ifndef __HIR_H
#define __HIR_H

#include "type.h"
#include "token.h"
#include "ast.h"
#include "error.h"
#include <stddef.h>
#include <stdbool.h>

typedef enum HirDeclKind {
	HIR_DECL_CONST,
	HIR_DECL_VAR,
} HirDeclKind;

typedef enum HirVisibility {
	HIR_PUBLIC,
	HIR_PRIVATE,
} HirVisibility;

typedef enum HirInstructionNodeKind {
	HIR_NODE_END,                  // Halt | EOF
	HIR_NODE_IDENTIFIER,           // Span indentifier
	HIR_NODE_INTEGER,              // int64_t integer
	HIR_NODE_FLOAT,                // double floating_point
	HIR_NODE_TYPE,                 // Type* type

	HIR_NODE_UNARY_PLUS,           // + <something>
	HIR_NODE_UNARY_MINUS,          // - <something>

	HIR_NODE_ADD,                  // <something> +  <something>
	HIR_NODE_SUB,                  // <something> -  <something>
	HIR_NODE_MUL,                  // <something> *  <something>
	HIR_NODE_DIV,                  // <something> /  <something>
	HIR_NODE_POW,                  // <something> ** <something>

	HIR_NODE_CONSTANT_DECLARATION, // HirConstant constant
	HIR_NODE_VARIABLE_DECLARATION, // HirVariable variable
} HirInstructionNodeKind;

typedef struct HirInstruction {
	Span span;
	Location location;
	HirInstructionNodeKind tag;
	union {
		const char* identifier;
		int64_t integer;
		double floating_point;
		TypeID type;

		struct HirConstantDecl {
			const char* name;
			bool explicit_typing;
			bool initialized;
		} constant;

		struct HirVariableDecl {
			const char* name;
			bool explicit_typing;
			bool initialized;
		} variable;
	} as;
} HirInstruction;

typedef struct HirStartDecl HirStartDecl;
typedef struct HirConstantDecl HirConstantDecl;
typedef struct HirVariableDecl HirVariableDecl;

typedef struct HirGlobal {
	Location location;
	Span span;
	const char* name;
	HirVisibility visibility;
	HirDeclKind kind;
	bool explicit_typing;
	bool initialized;
	HirInstruction* instructions;
} HirGlobal;

typedef struct Hir {
	const char* path;
	Arena string_pool;
	HirGlobal* globals;
} Hir;

void print_hir_instruction(FILE* f, HirInstruction instruction);
void print_hir(FILE *f, Hir hir);

error hoist_ast(ASTFile file, Hir *out);
void deinit_hir(Hir hir);
HirGlobal* hir_find_global(HirGlobal* globals, const char* name);

#endif // !__HIR_H
