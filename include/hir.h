#ifndef __HIR_H
#define __HIR_H

#include "token.h"

typedef enum HirInstructionNodeKind {
	HIR_NODE_END,                  // Halt
	HIR_NODE_IDENTIFIER,           // Token
	HIR_NODE_INTEGER,              // int64_t
	HIR_NODE_FLOAT,                // double
	HIR_NODE_TYPE,                 // Type *

	HIR_NODE_UNARY_PLUS,           // + <something>
	HIR_NODE_UNARY_MINUS,          // - <something>

	HIR_NODE_ADD,                  // <something> +  <something>
	HIR_NODE_SUB,                  // <something> -  <something>
	HIR_NODE_MUL,                  // <something> *  <something>
	HIR_NODE_DIV,                  // <something> /  <something>
	HIR_NODE_POW,                  // <something> ** <something>

	HIR_NODE_CONSTANT_DECLARATION, // HirConstant
	HIR_NODE_VARIABLE_DECLARATION, // HirVariable
} HirInstructionNodeKind;

typedef struct HirInstructionNode {
} HirInstructionNode;

typedef struct HirInstruction {
	Span span;
	Location location;
	HirInstructionNode node;
} HirInstruction;

typedef struct Hir {
	HirInstruction instructions;
} Hir;

#endif // !__HIR_H
