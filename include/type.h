#ifndef __TYPE_H
#define __TYPE_H

#include "arena.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef size_t TypeID;
#define TYPE_ID_ENUM_DEF(X)						\
	X(TYPE_AUTO, "auto")						\
	X(TYPE_INT, "int")						    \
	X(TYPE_FLOAT, "float")					    \
	X(TYPE_UNTYPED_INT, "int")				    \
	X(TYPE_UNTYPED_FLOAT, "float")					
enum {
#define X(__field_name, ...) __field_name,
	TYPE_ID_ENUM_DEF(X)
#undef X
};

typedef enum PrimitiveType {
	PRIMITIVE_INT,
	PRIMITIVE_FLOAT,
	PRIMITIVE_UNTYPED_FLOAT,
	PRIMITIVE_UNTYPED_INT,
	PRIMITIVE_AUTO,
} PrimitiveType;

typedef struct Type {
	TypeID id;
	const char *name;
} Type;

typedef struct TypesPool {
	Arena arena;
	Type** types;
} TypesPool;

extern TypesPool g_types_pool;

typedef enum TypeMatchResult {
	TYPE_MATCH_NONE = 1, /* ERROR */
	TYPE_MATCH_EXACT,
	TYPE_MATCH_NEED_CAST, /* ERROR as Well :P */
} TypeMatchResult;

void init_types_pool(void);
void deinit_types_pool(void);
TypeID create_type(Type tp);

void print_type(FILE* f, const TypeID id);

TypeMatchResult type_matches(const TypeID id1, const TypeID id2);
bool type_is_any_number(const TypeID id);
bool type_is_any_int(const TypeID id);
bool type_is_any_float(const TypeID id);

#endif /* !__TYPE_H */
