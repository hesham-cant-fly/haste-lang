#ifndef HASTE_H_
#define HASTE_H_

#include "cwalk.h"
#include "my_allocator.h"
#include "my_common.h"
#include "my_c_allocator.h"
#include "my_arena_allocator.h"
#include "my_temporary_allocator.h"
#include "my_hashtable.h"
#include "my_array.h"
#include "my_managed_array.h"
#include "my_stream.h"
#include <stdarg.h>
#include <stdnoreturn.h>

#ifdef DEBUG
#  define Break() __asm__("int3")
#else
#  define Break() do {} while (0)
#endif
#define Exit(...) \
	do { \
		Break(); \
		exit(__VA_ARGS__); \
	} while (0)

#define crash() \
	do { \
		fprintf(stderr, "%s:%d: Error: program crashed.\n", __FILE__, __LINE__);\
		Break(); \
		exit(1); \
	} while (0)

typedef enum Error {
	OK = 0,
	ERROR = 1,
} Error;

//
// options.c
//
struct options {
	bool dump_tokens : 1;
	bool dump_ast    : 1;
	bool dump_sema   : 1;
	bool dump_llvm   : 1;
	const char *source_path;
};

Error parse_arguments(const int argc, const char *argv[argc], struct options *out);

//
// span.c
//
struct span {
	const char *start;
	uint32_t len;
};

#define span(...) ((struct span) { __VA_ARGS__ })
#define SPAN_FMT(...) ((int)(__VA_ARGS__).len), (__VA_ARGS__).start

struct span span_to_trimed(struct span span);

//
// source.c
//
typedef uint32_t source_file_id;

enum source_file_type {
	SRC_HASTE,   // .haste
	SRC_UNKNOWN,
};

struct source_file {
	char *path;
	char *content;
	size_t len;
	enum source_file_type type;
	struct haste_ast_node *root; // NULL by default

	struct haste_declarations {
		size_t len;
		struct haste_declaration {
			char *key; // name
			struct haste_ast_node *node;
			bool analyzing : 1;
		} *items;
	} declarations;
};

struct source_file_list {
	struct Allocator allocator;
	size_t cap, len;
	struct source_file *items;
};

extern struct source_file_list sources;

/**
  * @brief Obtain the current working directory
  */
char* get_current_working_directory(void);

/**
  * @brief returns the absolute path of a relative path.
  */
char *get_absolute_path(struct Allocator allocator, const char* relative_path);

/**
  * @brief reads entire file and returns c string of its content
  */
char *read_entire_file(struct Allocator allocator, const char *path);

/**
  * @brief based on `path`. it will determen the file type of the file.
  * @brief look at `enum source_file_type` to see the possible results
  */
enum source_file_type get_file_type(const char *path);

/**
  * @brief given base (which is cwd if its null) and a relative path.
  * @brief it will read the entire file's content. addes it to the `sources` global
  * @brief and return an id (index) to it
  * @param base if NULL. it will be defaulted to "./"
  * @param path a relative path to the file
  */
source_file_id obtain_source_file_id(const char *base, const char *path);

/**
  * @brief given an id. it will return the `struct source_file`
  * @brief if the `id` is invalid. it will crash the program.
  */
struct source_file get_source_file(const source_file_id id);

/**
  * @brief given an id. it will return its path
  */
const char *get_source_file_path(const source_file_id id);

/** @brief given an id. it will return its len
  */
size_t get_source_file_len(const source_file_id id);

/** @brief given an id. it will return its content as a c string.
  */
const char *get_source_file_content(const source_file_id id);

/** @brief given an id. it will return its content's end
  */
const char *get_source_file_end(const source_file_id id);

/**
  * @brief given an id. it will return which type is that file
  */
enum source_file_type get_source_file_type(const source_file_id id);

/**
  * @brief given an id. it will return the ast that it got parsed into.
  */
struct haste_ast_node *get_source_file_ast(const source_file_id id);

/**
  * @brief given an id. it will return the declrations it have.
  */
struct haste_declarations get_source_file_declarations(const source_file_id id);

//
// token.c
//

enum token_kind {
	TK_IDENT = 1,    // names
	TK_KW_STRING,    // "string"
	TK_KW_CSTR,      // "cstr"
	TK_KW_INT,       // "int"
	TK_KW_FLOAT,     // "float"
	TK_KW_VOID,      // "void"
	TK_KW_AUTO,      // "auto"
	TK_KW_TYPE,      // "type"
	TK_KW_CAST,      // "cast"
	TK_KW_CONST,     // "const"
	TK_KW_VAR,       // "var"

	TK_SEMI_COLON,   // ";"

	TK_OPEN_BRAKET,  // "["
	TK_CLOSE_BRAKET, // "]"
	TK_OPEN_PAREN,   // "("
	TK_CLOSE_PAREN,  // ")"

	TK_COLON,        // ":"
	TK_EQ,           // "="

	TK_PLUS,         // "+"
	TK_MINUS,        // "-"
	TK_STAR,         // "*"
	TK_FSLASH,       // "/"

	TK_INT,          // integer
	TK_FLOAT,        // float
	TK_STR,          // string
	TK_EOF,          // end of file
};

struct token {
	enum token_kind kind;
	const char *start;
	uint32_t len;
	uint32_t line;

	union {
		int64_t ival;
		double fval;
		char *str;
	};
};

#define token(kind_, start_, end_, ...) \
	(struct token) { \
		.kind = (kind_), \
		.start = (start_), \
		.len = (uint32_t)((uintptr_t)(end_) - (uintptr_t)(start_)), \
		__VA_ARGS__ \
	}

#define TOKEN_FMT(...) SPAN_FMT(token_to_span(__VA_ARGS__))

struct token_list {
	size_t len;
	size_t cap;
	struct token *items;
};

struct span token_to_span(struct token token);
int print_token(stream_t stream, struct token token);

//
// unicode.c
//
int encode_utf8(char *out, uint32_t c);
uint32_t decode_utf8(const char **new_pos, const char *p, source_file_id src);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
int display_width(const char *p, int len, source_file_id src);

//
// value.c
//
#  define VAL_BAD                ((struct haste_value) { .kind = HASTE_VL_BAD })
#  define VAL_UNINIT             ((struct haste_value) { .kind = HASTE_VL_UNINIT })
#  define VAL_SCALAR(t, ...)     ((struct haste_value) { .kind = HASTE_VL_SCALAR, .type = (t), __VA_ARGS__ })
#  define VAL_RUNTIME(...)       ((struct haste_value) { .kind = HASTE_VL_RUNTIME, .runtime = (__VA_ARGS__) })
#  define VAL_OBJ(t, p)             ((struct haste_value) { .kind = HASTE_VL_OBJ, .type = (t), .obj = (struct haste_object*)(void*)(p) })

#  define OBJ_TYPE(...)          ((struct haste_object_type) { .base = { .kind = HASTE_OBJ_TYPE, }, __VA_ARGS__ })

#  define IS_BAD(...)            ((__VA_ARGS__).kind == HASTE_VL_BAD)
#  define IS_UNINIT(...)         ((__VA_ARGS__).kind == HASTE_VL_UNINIT)
#  define IS_SCALAR(...)         ((__VA_ARGS__).kind == HASTE_VL_SCALAR)
#  define IS_RUNTIME(...)        ((__VA_ARGS__).kind == HASTE_VL_RUNTIME)
#  define IS_OBJ(...)            ((__VA_ARGS__).kind == HASTE_VL_OBJ)
#  define IS_TYPE(...)           ((IS_OBJ(__VA_ARGS__) and ((__VA_ARGS__).obj->kind == HASTE_OBJ_TYPE)))

#  define AS_OBJ(v)              ((v).obj)
#  define AS_TYPE(v)             ((OAS_TYPE(AS_OBJ(v))))
#  define OAS_TYPE(v)            ((struct haste_object_type *)(v))

struct haste_object_type;

struct haste_value {
	enum haste_value_kind {
		HASTE_VL_BAD,
		HASTE_VL_UNINIT,
		HASTE_VL_SCALAR,
		HASTE_VL_RUNTIME,
		HASTE_VL_OBJ,
	} kind;
	struct haste_object_type *type;
	bool is_explicitly_comptime : 1;

	union {
		int64_t integer;
		double floating;
		struct haste_ast_node *runtime;
		struct haste_object *obj;
	};
};

struct haste_object {
	enum {
		HASTE_OBJ_TYPE,
		HASTE_OBJ_STRING,
	} kind;
};

struct haste_string_object {
	struct haste_object base;
	char *data;
	size_t len;
};

struct haste_object_type {
	struct haste_object base;
	enum {
		HASTE_TY_UNKNOWN,
		HASTE_TY_TYPE,
		HASTE_TY_INT,
		HASTE_TY_UNTYPED_INT,
		HASTE_TY_FLOAT,
		HASTE_TY_UNTYPED_FLOAT,
		HASTE_TY_AUTO,
		HASTE_TY_VOID,
		HASTE_TY_UNTYPED_STRING,
		HASTE_TY_STRING,
		HASTE_TY_CSTR,
	} kind;
	size_t size;
	size_t align;
};

extern struct haste_value ty_unknown;
extern struct haste_value ty_type;
extern struct haste_value ty_int;
extern struct haste_value ty_untyped_int;
extern struct haste_value ty_float;
extern struct haste_value ty_untyped_float;
extern struct haste_value ty_auto;
extern struct haste_value ty_void;
extern struct haste_value ty_untyped_string;
extern struct haste_value ty_string;
extern struct haste_value ty_cstr;

struct haste_value typeof(const struct haste_value value);

bool value_equal(struct haste_value a, struct haste_value b);

// bool type_can_add(const struct haste_value lhs, const struct haste_value rhs);
// bool type_can_sub(const struct haste_value lhs, const struct haste_value rhs);
// bool type_can_mul(const struct haste_value lhs, const struct haste_value rhs);
// bool type_can_div(const struct haste_value lhs, const struct haste_value rhs);

struct haste_value value_add(const struct haste_value lhs, const struct haste_value rhs);
struct haste_value value_sub(const struct haste_value lhs, const struct haste_value rhs);
struct haste_value value_mul(const struct haste_value lhs, const struct haste_value rhs);
struct haste_value value_div(const struct haste_value lhs, const struct haste_value rhs);

/** @brief These are the allowed cast:
  * typeof(value) -> to
  * untyped_int   -> int, float
  * int           -> float
  * untyped_float -> float, int
  * float         -> int
  * @param to has to be a type. otherwise it will crash (see `ASSERT_IS_TYPE')
  *           its an untyped type. this function will crash
  *           if its `ty_auto'. this function will crash
  * @param value can be any value
  * @returns `value' casted to the type `to'. upon failing it will return `VAL_BAD'
  */
struct haste_value value_cast(const struct haste_value to, const struct haste_value value);

bool type_equal(const struct haste_value t1,
                const struct haste_value t2);
bool type_can_assign(const struct haste_value assignable,
                     const struct haste_value value);
bool type_can_cast(const struct haste_value to,
                   const struct haste_value from);

bool type_is_integer(const struct haste_value t);
bool type_is_float(const struct haste_value t);
bool type_is_number(const struct haste_value t);
bool type_is_untyped(const struct haste_value t);
bool type_is_untyped_number(const struct haste_value t);

int print_object(stream_t stream, const struct haste_object *obj);
int print_value(stream_t stream, const struct haste_value value);

//
// ast.c
//
struct haste_ast_node {
	enum haste_ast_node_kind {
		/* Generated during analysis */
		ND_VALUE,    // value

		/* Expresion */
		ND_BINARY,   // lhs, rhs, op
		ND_UNARY,    // rhs, op
		ND_PRIMARY,  // struct token token
		ND_GROUPING, // struct haste_ast_node *body

		ND_CAST,     // struct { ... } cast

		/* Statements */
		ND_VAR_DECL, // struct { ... } variable
	} kind;
	struct haste_value type;
	struct haste_ast_node *next;
	struct token start;

	union {
		struct token token;          // ND_PRIMARY
		struct haste_ast_node *body; // ND_GROUPING
		struct haste_value value;    // ND_VALUE
		struct {                     // ND_BINARY, ND_UNARY
			struct haste_ast_node *lhs;
			struct haste_ast_node *rhs;
			struct token op;
		};

		struct {                     // ND_CAST
			struct haste_ast_node *to;
			struct haste_ast_node *expr;
		} cast;

		struct {                     // ND_VAR_DECL
			bool is_constant : 1;
			bool is_explicitly_comptime : 1;
			bool is_global : 1;
			struct token name;
			struct haste_ast_node *type;
			struct haste_ast_node *value;
		} variable;
	};
};

#define node_transform(node_, kind_, ...) \
	do { \
		(node_)->kind = (kind_); \
		(node_) ->__VA_ARGS__; \
	} while (0)

struct haste_ast_node * node_into_value(
	struct Allocator allocator,
	struct haste_ast_node *node,
	struct haste_value value);
int print_haste_ast(stream_t file, const struct haste_ast_node *root);
bool node_is_declaration(const struct haste_ast_node *node);

//
// error.c
//
void f_report_at(const source_file_id src, const char *kind, const char *start, const char *fmt, ...);
void f_vreport_at(const source_file_id src, const char *kind, const char *start, const char *fmt, va_list args);
void f_report_at_token(const source_file_id src, const char *kind, struct token token, const char *fmt, ...);
void f_vreport_at_token(const source_file_id src, const char *kind, struct token token, const char *fmt, va_list args);

//
// scanner.c
//
Error scan_entire_file(
	struct Allocator allocator,
	const source_file_id src,
	struct token_list *out);

//
// parse.c
//
Error parse(
	struct Allocator allocator,
	const struct token_list tokens,
	const source_file_id src);

//
// hoisting.c
//
Error hoist(struct Allocator allocator,
            struct Allocator arena_allocator,
            const source_file_id src);

//
// analysis.c
//
Error analyze(struct Allocator allocator, struct Allocator arena_allocator, const source_file_id src);

//
// codegen.c
//
Error codegen(struct Allocator allocator, const source_file_id src);

#endif // !HASTE_H_
