#ifndef AST_BASE_H_
#define AST_BASE_H_

#include "token.h"
#include "type.h"

struct Environment;

struct BoolList {
	bool *items;
	size_t len;
	size_t cap;
};

struct AstNodeBase {
	struct Span span;
	struct Location location;
	TypeID type;
};

struct AstNodeInterface {
	void (*print)(struct AstNodeBase *node, FILE* f, struct BoolList *is_last, int formatted);
	struct Value (*analyze)(struct AstNodeBase *node, struct Environment *env);
};

struct AstExpr {
	const struct AstNodeInterface *v_table;
	struct AstNodeBase *data;
	struct AstExpr *next;
};

#endif // !AST_BASE_H_
