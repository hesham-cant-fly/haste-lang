#ifndef __TYPE_H
#define __TYPE_H

#include "arena.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#define TYPES_BUCKET_CAPACITY 256
#define USE_DYNAMIC_ARRAY 0

typedef uint32_t TypeID;
#define TYPE_ID_ENUM_DEF(X)	\
	X(TYPE_NO_TYPE, "__no_type__") \
	X(TYPE_POISONED, "poisoned") \
	X(TYPE_AUTO, "auto") \
	X(TYPE_VOID, "void") \
	X(TYPE_TYPEID, "typeid") \
	X(TYPE_INT, "int") \
	X(TYPE_FLOAT, "float") \
	X(TYPE_UNTYPED_INT, "untyped_int") \
	X(TYPE_UNTYPED_FLOAT, "untyped_float")					
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

struct TypeInfo {
	TypeID id;
	const char *name;
};

struct TypesBucket {
	struct TypesBucket *next;
	size_t len;
	struct TypeInfo items[TYPES_BUCKET_CAPACITY];
};

struct TypesPool {
#if USE_DYNAMIC_ARRAY
	struct TypeList {
		struct TypeInfo *items;
		size_t len;
		size_t cap;
	} types;
#else
	size_t total_count;
	size_t buckets_count;
	struct TypesBucket *head;
	struct TypesBucket *current;
#endif
};

extern struct TypesPool g_types_pool;

enum TypeMatchResult {
	TYPE_MATCH_NONE = 1, /* ERROR */
	TYPE_MATCH_EXACT,
	TYPE_MATCH_NEED_CAST, /* ERROR as Well :P */
};

void init_types_pool(void);
void deinit_types_pool(void);
TypeID create_type(struct TypeInfo tp);

const char* type_str(const TypeID id);
void print_typeln(FILE *f, const TypeID id);
void print_type(FILE* f, const TypeID id);

enum TypeMatchResult type_matches(const TypeID id1, const TypeID id2);
bool type_is_numiric(const TypeID id);
bool type_is_any_int(const TypeID id);
bool type_is_any_float(const TypeID id);
bool type_is_untyped(const TypeID id);

#endif /* !__TYPE_H */
