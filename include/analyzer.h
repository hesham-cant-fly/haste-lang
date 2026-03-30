#ifndef ANALYZER_H_
#define ANALYZER_H_

#include "token.h"
#include "type.h"
#include "ast.base.h"
#include <stddef.h>
#include <stdint.h>

struct AstBinaryOperator;
struct AstUnaryOperator;

#define LOAD_FACTOR_NUM 85
#define LOAD_FACTOR_DEN 100

enum SymbolState {
	SYMBOL_HOISTED,
	SYMBOL_DECLARED,
	SYMBOL_DEFINED,
};

struct Value {
	enum ValueKind {
		VALUE_POISONED = 0, // error
		VALUE_UNKNOWN,      // runtime value
		VALUE_INT,          // integer
		VALUE_FLT32,        // float
		VALUE_FLT64,        // double
		VALUE_TYPEID,       // typeid
	} kind;
	TypeID type;
	struct Location location;
	struct Span span;
	union {
		int32_t integer;
		float float32;
		double float64;
		TypeID type;
		struct AstExpr unknown; // runtime value
	} as;
};

#define VALUE(...) ((struct Value) { __VA_ARGS__ })
#define is_poisoned(value__) ((value__).kind == VALUE_POISONED)

struct Symbol {
	struct Span name;
	uint64_t hash;
	struct Value value;
	enum SymbolState state;
	bool is_constant;
	bool assigned;
	bool is_param;
};

#define get_symbol_type(symbol__) ((symbol__).value.type)

struct SymbolTable {
	size_t len;
	size_t cap;
	struct Symbol *items;
};

struct Scope {
	struct Scope *parent;
	struct Scope *child;
	bool is_function; /* Is Function Scope */
	struct SymbolTable symbols;
};

struct Environment {
	Arena *allocator;
	const char *path;
	struct Scope *global;
	struct Scope *current;
	size_t errors_count;
	size_t warnings_count;
	bool had_error;
};

void init_environment(struct Environment *env, const char *path, Arena *allocator);
void deinit_environment(struct Environment *env);

/* value */
void print_value(FILE *f, struct Value value);
struct AstExpr value_into_ast_expr(Arena *allocator, struct Value value);
struct Value value_from_ast_expr(struct AstExpr expr);
struct Value value_binary_op(struct Environment *env,
							 struct AstBinaryOperator op,
							 struct Value value_a,
							 struct Value value_b);

float value_into_float32(struct Value value);
double value_into_float64(struct Value value);
int64_t value_into_int64(struct Value value);

bool value_is_comptime(const struct Value value);
bool value_is_numiric(const struct Value value);
bool value_is_typed(const struct Value value);

/* Expr */
struct Value analyze_binary_expr(struct AstNodeBase *node, struct Environment *env);
struct Value analyze_unary_expr(struct AstNodeBase *node, struct Environment *env);
struct Value analyze_grouping_expr(struct AstNodeBase *node, struct Environment *env);
struct Value analyze_identifier_expr(struct AstNodeBase *node, struct Environment *env);
struct Value analyze_literal_expr(struct AstNodeBase *node, struct Environment *env);

/* Declarations */
struct Value analyze_const_decl(struct AstNodeBase *node, struct Environment *env);
struct Value analyze_var_decl(struct AstNodeBase *node, struct Environment *env);

#endif /* !ANALYZER_H_ */
