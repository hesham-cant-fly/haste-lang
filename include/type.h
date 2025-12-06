#ifndef __TYPE_H
#define __TYPE_H

#include "arena.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum TypeKind {
	TYPE_UNTYPED_INT,
	TYPE_UNTYPED_FLOAT,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_AUTO,
} TypeKind;

typedef struct Type {
	TypeKind kind;
	struct Type *next;
} Type;

typedef struct TypesPool {
	Arena arena;
	Type *head;
	Type *current;
} TypesPool;

typedef enum TypeMatchResult {
	TYPE_MATCH_NONE = 1, /* ERROR */
	TYPE_MATCH_EXACT,
	TYPE_MATCH_NEED_CAST, /* ERROR as Well :P */
} TypeMatchResult;

void type_pool_add_type(TypesPool *pool, Type tp);

TypeMatchResult type_matches(const Type *tp1, const Type *tp2);
bool type_is_any_number(const Type *tp);
bool type_is_any_int(const Type *tp);
bool type_is_any_float(const Type *tp);

#endif /* !__TYPE_H */
