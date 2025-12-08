#ifndef __TYPE_H
#define __TYPE_H

#include "arena.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum TypeKind {
	TYPE_UNTYPED_INT,
	TYPE_UNTYPED_FLOAT,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_AUTO,
} TypeKind;

typedef enum PrimitiveType {
	PRIMITIVE_INT,
	PRIMITIVE_FLOAT,
	PRIMITIVE_UNTYPED_FLOAT,
	PRIMITIVE_UNTYPED_INT,
	PRIMITIVE_AUTO,
} PrimitiveType;

typedef struct Type {
	TypeKind kind;
	const char *name;
	struct Type *next;
} Type;

typedef struct TypesPool {
	Arena arena;
	Type *head;
	Type *current;
} TypesPool;

extern TypesPool g_types_pool;
extern Type* g_primitive_types_table[];

typedef enum TypeMatchResult {
	TYPE_MATCH_NONE = 1, /* ERROR */
	TYPE_MATCH_EXACT,
	TYPE_MATCH_NEED_CAST, /* ERROR as Well :P */
} TypeMatchResult;

void init_types_pool(void);
void deinit_types_pool(void);
Type* create_type(Type tp);
Type* get_primitive_type(PrimitiveType type);

void print_type(FILE* f, const Type tp);

TypeMatchResult type_matches(const Type *tp1, const Type *tp2);
bool type_is_any_number(const Type *tp);
bool type_is_any_int(const Type *tp);
bool type_is_any_float(const Type *tp);

#endif /* !__TYPE_H */
