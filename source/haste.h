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
#include <stdint.h>

#define SAFE_COUNT(n) ((n) > 0 ? (n) : (size_t)1)
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

#define frand() ((float)rand() / (float)RAND_MAX)
#define when_fun if (not g_options.disable_fun)
#define run_at_percent(...) if ((not g_options.disable_fun) and (frand() * 100.0) <= ((__VA_ARGS__)))

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
	bool do_measure  : 1;
	bool do_dump     : 1;
	bool disable_fun : 1;
	const char *source_path;
	const char *output_path;
};

extern struct options g_options;

Error parse_arguments(const int argc, const char *argv[argc]);

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
typedef int32_t source_file_id;

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
			const char *key; // name
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
	TK_KW_INT_BITS,  // "int[0-9]+
	TK_KW_UINT_BITS, // "uint[0-9]+
	TK_KW_INT,       // "int"
	TK_KW_UINT,      // "uint"
	TK_KW_FLOAT,     // "float"
	TK_KW_USIZE,     // "usize"
	TK_KW_VOID,      // "void"
	TK_KW_AUTO,      // "auto"
	TK_KW_TYPE,      // "type"
	TK_KW_CAST,      // "cast"
	TK_KW_CONST,     // "const"
	TK_KW_VAR,       // "var"
	TK_KW_STRUCT,    // "struct"
	TK_KW_DISTINCT,  // "distinct"

	TK_SEMI_COLON,   // ";"

	TK_OPEN_BRAKET,  // "["
	TK_CLOSE_BRAKET, // "]"
	TK_OPEN_PAREN,   // "("
	TK_CLOSE_PAREN,  // ")"

	TK_OPEN_BRACE,   // "{"
	TK_CLOSE_BRACE,  // "}"

	TK_COLON,        // ":"
	TK_EQ,           // "="
	TK_COMMA,        // ","
	TK_DOT,          // "."

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
	enum token_kind kind : 8;
	const char *start;
	uint32_t len;
	uint32_t line;

	union {
		int64_t ival;
		double fval;
		const char *str;
		const char *ident;
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
// intern.c
//
struct intern_table {
	struct Allocator arena;
	struct Allocator allocator;
	size_t cap, len;
	struct intern_entry {
		uint64_t hash;
		const char *str;
		size_t len;
	} *entries;
};

//
// type pool
//
typedef uint32_t TypeID;
struct haste_type_info;
struct haste_struct_type;

struct type_pool {
	struct Allocator allocator;
	struct {
		size_t len, cap;
		struct haste_type_info **items;
	} chunks;
	uint32_t len;
};

#define STANDARD_BITWIDTH_LIMIT       128
#define HASTE_TID_RESERVED_INT_BASE   0
#define HASTE_TID_RESERVED_UINT_BASE  129
#define HASTE_TID_TOTAL_RESERVED      ( HASTE_TID_RESERVED_UINT_BASE + 129 )
#define HASTE_TID_IS_RESERVED(id)     ((id) <= HASTE_TID_TOTAL_RESERVED)

extern struct type_pool g_type_pool;

TypeID type_pool_add(struct haste_type_info type);
struct haste_type_info *type_pool_get(TypeID id);
void type_pool_set_name(TypeID id, const char *name);
struct haste_value type_get_int(uint16_t bits, bool is_signed);

//
// value.c
//
#  define VAL_NONE                  ((struct haste_value) { .kind = HASTE_VL_NONE })
#  define VAL_BAD                   ((struct haste_value) { .kind = HASTE_VL_BAD  })
#  define VAL_ZERO                  ((struct haste_value) { .kind = HASTE_VL_ZERO })
#  define VAL_UNINIT                ((struct haste_value) { .kind = HASTE_VL_UNINIT })
#  define VAL_SCALAR(tid, ...)      ((struct haste_value) { .kind = HASTE_VL_SCALAR, .type_id = (tid), __VA_ARGS__ })
#  define VAL_RUNTIME(...)          ((struct haste_value) { .kind = HASTE_VL_RUNTIME, .runtime = (__VA_ARGS__) })
#  define VAL_TYPE(...)             ((struct haste_value) { .kind = HASTE_VL_TYPE, .type_id = AS_TYPEID(ty_type), .type = (__VA_ARGS__) })
#  define VAL_OBJ(tid, p)           ((struct haste_value) { .kind = HASTE_VL_OBJ, .type_id = (tid), .obj = (struct haste_object*)(void*)(p) })

#  define TYPE_INFO(...)             ((struct haste_type_info) { __VA_ARGS__ })
#  define STRUCT_TYPE_INFO(...)      ((struct haste_struct_type_info) { .kind = HASTE_TY_STRUCT, __VA_ARGS__ })
#  define AUTO_STRUCT_TYPE_INFO(...) ((struct haste_struct_type_info) { .kind = HASTE_TY_AUTO_STRUCT, __VA_ARGS__ })

#  define OBJ_STRUCT(...)           ((struct haste_struct_object) { .base = OBJ_TYPE(HASTE_TY_STRUCT), __VA_ARGS__ })

#  define IS_NONE(...)              ((__VA_ARGS__).kind == HASTE_VL_NONE)
#  define IS_BAD(...)               ((__VA_ARGS__).kind == HASTE_VL_BAD)
#  define IS_ZERO(...)              ((__VA_ARGS__).kind == HASTE_VL_ZERO)
#  define IS_UNINIT(...)            ((__VA_ARGS__).kind == HASTE_VL_UNINIT)
#  define IS_SCALAR(...)            ((__VA_ARGS__).kind == HASTE_VL_SCALAR)
#  define IS_RUNTIME(...)           ((__VA_ARGS__).kind == HASTE_VL_RUNTIME)
#  define IS_TYPE(...)              ((__VA_ARGS__).kind == HASTE_VL_TYPE)

#  define IS_STRUCT_TYPE(...)       ((IS_TYPE(__VA_ARGS__)) and (AS_TYPE_INFO(__VA_ARGS__)->kind == HASTE_TY_STRUCT))
#  define IS_AUTO_STRUCT_TYPE(...)  ((IS_TYPE(__VA_ARGS__)) and (AS_TYPE_INFO(__VA_ARGS__)->kind == HASTE_TY_AUTO_STRUCT))

#  define IS_OBJ(...)               ((__VA_ARGS__).kind == HASTE_VL_OBJ)
#  define IS_STRUCT(...)            ((IS_OBJ(__VA_ARGS__)) and ((__VA_ARGS__).obj->kind == HASTE_OBJ_STRUCT))

#  define AS_OBJ(v)                 ((v).obj)
#  define AS_TYPEID(v)              ((v).type)
#  define AS_TYPE_INFO(v)           ((type_pool_get(AS_TYPEID(v))))
#  define AS_STRUCT_TYPE_INFO(v)    ((&AS_TYPE_INFO(v)->structure))

#  define AS_STRUCT(v)              ((OAS_STRUCT(AS_OBJ(v))))
#  define OAS_STRUCT(v)             ((struct haste_struct_object *)(v))

enum haste_value_kind {
	HASTE_VL_NONE, // unset value
	HASTE_VL_BAD,
	HASTE_VL_ZERO,
	HASTE_VL_UNINIT,
	HASTE_VL_SCALAR,
	HASTE_VL_RUNTIME,
	HASTE_VL_TYPE,
	HASTE_VL_OBJ,
};

struct haste_value {
	enum haste_value_kind kind : 7;
	bool is_explicitly_comptime : 1;
	TypeID type_id;

	union {
		int64_t integer;
		double floating;
		TypeID type;
		struct haste_ast_node *runtime;
		struct haste_object *obj;
	};
};

struct haste_object {
	enum {
		HASTE_OBJ_STRING,
		HASTE_OBJ_STRUCT,
	} kind : 8;
};

struct haste_string_object {
	struct haste_object base;
	char *data;
	size_t len;
};

struct haste_struct_field {
	const char *name;
	struct haste_value type;
	struct haste_value default_value;
	bool has_default;
};

struct haste_struct_object {
	struct haste_object base;
	struct haste_value *fields;
};

struct haste_type_info {
	TypeID pool_id;
	enum {
		HASTE_TY_ZERO,
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
		HASTE_TY_USIZE,
		HASTE_TY_UINT,
		HASTE_TY_STRUCT,
		HASTE_TY_AUTO_STRUCT,
	} kind;
	bool is_integer  : 1;
	bool is_float    : 1;
	bool is_unsigned : 1;
	bool is_string   : 1;
	bool is_untyped  : 1;

	const char *name;
	size_t size;
	size_t align;
	size_t bit_size;

	union {
		struct haste_struct_type_info {
			size_t field_count;
			struct haste_struct_field *fields;
		} structure;
	};
};

extern struct haste_value ty_zero;
extern struct haste_value ty_unknown;
extern struct haste_value ty_type;
extern struct haste_value ty_uint;
extern struct haste_value ty_int;
extern struct haste_value ty_untyped_int;
extern struct haste_value ty_float;
extern struct haste_value ty_untyped_float;
extern struct haste_value ty_auto;
extern struct haste_value ty_void;
extern struct haste_value ty_untyped_string;
extern struct haste_value ty_string;
extern struct haste_value ty_cstr;
extern struct haste_value ty_usize;

void set_up_builtins(struct Allocator allocator, struct intern_table *table);
bool type_is_builtin(struct haste_value ty);
bool is_newly_created_type(struct haste_value ty);
void reset_new_type_counter(void);

struct haste_value typeof(const struct haste_value value);

bool value_equal(struct haste_value a, struct haste_value b);

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
struct haste_value value_cast(struct Allocator alloc, const struct haste_value to, const struct haste_value value);
struct haste_value zero_for_type(struct Allocator alloc, struct haste_value to);
struct haste_value default_for_type(struct Allocator alloc, struct haste_value to);

bool type_equal(const struct haste_value t1,
                const struct haste_value t2);
bool type_can_assign(const struct haste_value assignable,
                     const struct haste_value value);
bool type_can_cast(const struct haste_value to,
                   const struct haste_value from);

struct haste_value untyped_to_typed(struct haste_value type);

bool type_is_integer(const struct haste_value t);
bool type_is_float(const struct haste_value t);
bool type_is_number(const struct haste_value t);
bool type_is_untyped(const struct haste_value t);
bool type_is_untyped_integer(const struct haste_value t);
bool type_is_untyped_number(const struct haste_value t);
bool type_is_any_string(const struct haste_value t);

bool type_equal_struct(const struct haste_type_info *t1, const struct haste_type_info *t2);
uint64_t type_hash(const struct haste_value t);
bool is_comptime_known(const struct haste_value v);

ssize_t find_named_field(const struct haste_value tp, const char *name);

bool haste_is_default_empty_string(const struct haste_object *obj);

int print_object(stream_t stream, const struct haste_object *obj, struct haste_value type);
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
		ND_ACCESS,   // access
		ND_PRIMARY,  // struct token token
		ND_GROUPING, // struct haste_ast_node *body
		ND_DISTINCT, // struct haste_ast_node *body

		ND_CAST,     // struct { ... } cast

		/* Structs */
		ND_STRUCT_TYPE,       // struct { fields... }
		ND_STRUCT_FIELD,      // name: type [= default]
		ND_STRUCT_LITERAL,    // Type{...} or .{...}
		ND_STRUCT_LIT_FIELD,  // name: value

		/* Statements */
		ND_VAR_DECL, // struct { ... } variable
	} kind : 8;
	struct haste_value type;
	struct haste_ast_node *next;
	struct token start;

	union {
		struct token token;          // ND_PRIMARY
		struct haste_ast_node *body; // ND_GROUPING, ND_DISTINCT
		struct haste_value value;    // ND_VALUE
		struct {                     // ND_BINARY, ND_UNARY
			struct haste_ast_node *lhs;
			struct haste_ast_node *rhs;
			struct token op;
		};
		struct {
			struct haste_ast_node *lhs;
			struct token rhs;
		} access;

		struct {                     // ND_CAST
			struct haste_ast_node *to;
			struct haste_ast_node *expr;
		} cast;

		struct {                     // ND_STRUCT_TYPE
			struct haste_ast_node *fields;
		} struct_type;

		struct {                     // ND_STRUCT_FIELD
			size_t name_count;
			struct token *names;
			struct haste_ast_node *type;
			struct haste_ast_node *default_value;
		} struct_field;

		struct {                     // ND_STRUCT_LITERAL
			struct haste_ast_node *type_expr;
			struct haste_ast_node *fields;
		} struct_literal;

		struct {                     // ND_STRUCT_LIT_FIELD
			struct token name;
			struct haste_ast_node *value;
		} struct_lit_field;

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

struct intern_table init_intern_table(struct Allocator allocator, struct Allocator arena);
void deinit_intern_table(struct intern_table *table);

const char *intern_str(struct intern_table *table, const char *start, size_t len);
const char *intern_token(struct intern_table *table, struct token token);
const char *intern_cstr(struct intern_table *table, const char *str);

//
// scanner.c
//
Error scan_entire_file(
	struct Allocator allocator,
	struct intern_table *table,
	const source_file_id src,
	struct token_list *out);
Error scan_entire_string(
	struct Allocator allocator,
	struct intern_table *table,
	const char *content,
	struct token_list *out);

//
// parse.c
//
Error parse_expr(
	struct Allocator allocator,
	const struct token_list tokens,
	struct haste_ast_node **out);
Error parse(
	struct Allocator allocator,
	const struct token_list tokens,
	const source_file_id src);

//
// hoisting.c
//
Error hoist(struct Allocator allocator,
			struct intern_table *table,
            const source_file_id src);

//
// analysis.c
//

Error analyze_one_node(
	struct Allocator allocator,
	struct Allocator arena_allocator,
    struct intern_table *intern_table,
	struct haste_ast_node *node,
	struct haste_value *out);
Error analyze(struct Allocator allocator,
              struct Allocator arena_allocator,
              struct intern_table *intern_table,
              const source_file_id src);
//
// codegen.c
//
Error codegen(struct Allocator allocator, const source_file_id src, struct intern_table *table, const char *output_path, bool dump_to_stderr);

#endif // !HASTE_H_
