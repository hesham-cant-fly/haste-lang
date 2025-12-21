#ifndef __TYPE_H
#define __TYPE_H

#include "arena.h"
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef size_t TypeID;
#define TYPE_ID_ENUM_DEF(X)						\
	X(TYPE_AUTO, "auto")						\
	X(TYPE_TYPEID, "typeid")					\
	X(TYPE_INT, "int")						    \
	X(TYPE_FLOAT, "float")					    \
	X(TYPE_UNTYPED_INT, "int")				    \
	X(TYPE_UNTYPED_FLOAT, "float")					
enum {
#define X(__field_name, ...) __field_name,
	TYPE_ID_ENUM_DEF(X)
#undef X
};

enum PrimitiveType {
	PRIMITIVE_INT,
	PRIMITIVE_FLOAT,
	PRIMITIVE_UNTYPED_FLOAT,
	PRIMITIVE_UNTYPED_INT,
	PRIMITIVE_AUTO,
};

struct Type {
	TypeID id;
	const char *name;
};

struct TypesPool {
	struct TypeList {
		struct Type *items;
		size_t len;
		size_t cap;
	} types;
};

extern struct TypesPool g_types_pool;

enum TypeMatchResult {
	TYPE_MATCH_NONE = 1, /* ERROR */
	TYPE_MATCH_EXACT,
	TYPE_MATCH_NEED_CAST, /* ERROR as Well :P */
};

void init_types_pool(void);
void deinit_types_pool(void);
TypeID create_type(struct Type tp);

void print_type(FILE* f, const TypeID id);

enum TypeMatchResult type_matches(const TypeID id1, const TypeID id2);
bool type_is_numiric(const TypeID id);
bool type_is_any_int(const TypeID id);
bool type_is_any_float(const TypeID id);

#endif /* !__TYPE_H */
