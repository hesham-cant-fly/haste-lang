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

enum HirDeclKind {
	HIR_DECL_CONST,
	HIR_DECL_VAR,
};

enum HirVisibility {
	HIR_PUBLIC,
	HIR_PRIVATE,
};

enum HirInstructionNodeKind {
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
};

struct HirInstruction {
	struct Span span;
	struct Location location;
	enum HirInstructionNodeKind tag;
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
};

struct HirGlobal {
	bool visited;
	struct Location location;
	struct Span span;
	const char* name;
	struct HirInstructionList {
		struct HirInstruction* items;
		size_t len;
		size_t cap;
	} instructions;
	enum HirVisibility visibility;
	enum HirDeclKind kind;
	bool explicit_typing;
	bool initialized;
};

struct Hir {
	const char* path;
	Arena string_pool;
	struct HirGlobalList {
		struct HirGlobal* items;
		size_t len;
		size_t cap;
	} globals;
};

void print_hir_instruction(FILE* f, struct HirInstruction instruction);
void print_hir(FILE *f, struct Hir hir);

error hoist_ast(struct ASTFile file, struct Hir *out);
void deinit_hir(struct Hir hir);
struct HirGlobal* hir_find_global(struct HirGlobalList globals, const char* name);

#endif // !__HIR_H
