#ifndef __AST_HPP
#define __AST_HPP

#include "arena.hpp"
#include "mylib/linked-list.hpp"
#include "mylib/option.hpp"
#include "token.hpp"
#include <cassert>
#include <cstdio>

namespace AST {

// #define $X(enum_field_name, union_field_name, field_type)

#define $AST_EXPR_KIND_LIST                                                    \
  $X(Assign, assign, Assign)                                                   \
  $X(Binary, binary, Binary)                                                   \
  $X(Unary, unary, Unary)                                                      \
  $X(Int, integer_lit, Token)                                                  \
  $X(Float, float_lit, Token)                                                  \
  $X(Idetifier, identifier, Token)

#define $AST_TYPE_KIND_LIST                                                    \
  $X(Int, int_type, Token)                                                     \
  $X(Auto, auot_type, Token)

#define $AST_STMT_KIND_LIST \
  $X(Variable, variable, Variable) \
  $X(Constant, constant, Constant) \
  $X(Expr, expr, const Expr *)

struct Expr;
struct Type;
struct Stmt;

struct Expr {
#define $X(field_name, ...) field_name,
  enum class ExprKind : std::uint8_t {
    $AST_EXPR_KIND_LIST
  };
#undef $X

  struct Assign {
    const Expr *target;
    const Expr * value;
    Token op;
  };

  struct Binary {
    const Expr *lhs;
    const Expr *rhs;
    Token op;
  };

  struct Unary {
    const Expr *rhs;
    Token op;
  };

  ExprKind kind;
#define $X(_, field_name, type_name, ...) type_name field_name;
  union {
    $AST_EXPR_KIND_LIST
  };
#undef $X

  auto is(const ExprKind kind) const -> bool;
  auto operator==(const ExprKind &other) const -> bool;
};

struct Type {
#define $X(enum_field_name, ...) enum_field_name,
  enum class TypeKind : std::uint8_t {
    $AST_TYPE_KIND_LIST
  };
#undef $X

  TypeKind kind;
#define $X(_, field_name, type_name, ...) type_name field_name;
  union {
    $AST_TYPE_KIND_LIST
  };
#undef $X

  auto is(const TypeKind kind) const -> bool;
  auto operator==(const TypeKind &other) const -> bool;
};

struct Stmt {
#define $X(enum_field_name, ...) enum_field_name,
  enum class StmtKind : std::uint8_t {
    $AST_STMT_KIND_LIST
  };
#undef $X

  struct Variable {
    Token name;
    Option<const Type *> type = null;
    Option<const Expr *> value = null;
  };

  struct Constant {
    Token name;
    Option<const Type *> type = null;
    Option<const Expr *> value = null;
  };

  StmtKind kind;
#define $X(_, field_name, type_name, ...) type_name field_name;
  union {
    $AST_STMT_KIND_LIST
  };
#undef $X

  auto is(const StmtKind kind) const -> bool;
  auto operator==(const StmtKind &other) const -> bool;
};

using StmtList = LinkedList<Stmt>;

struct Module {
  ArenaAllocator arena;
  Expr *root;

  auto deinit() -> void;
};

auto print(const Token token, std::FILE *stream = stdout) -> void;

auto print(const Expr expr, std::FILE *stream = stdout) -> void;
auto print(const Expr *expr, std::FILE *stream = stdout) -> void;
auto print(const Expr::ExprKind kind, std::FILE *stream = stdout) -> void;
auto print(const Expr::Assign assign, std::FILE *stream = stdout) -> void;
auto print(const Expr::Binary bin, std::FILE *stream = stdout) -> void;
auto print(const Expr::Unary unary, std::FILE *stream = stdout) -> void;

auto print(const Type type, std::FILE *stream = stdout) -> void;
auto print(const Type *type, std::FILE *stream = stdout) -> void;
auto print(const Type::TypeKind kind, std::FILE *stream = stdout) -> void;

auto print(const Stmt stmt, std::FILE *stream = stdout) -> void;
auto print(const Stmt::StmtKind kind, std::FILE *stream = stdout) -> void;
auto print(const Stmt::Variable variable, std::FILE *stream = stdout) -> void;
auto print(const Stmt::Constant variable, std::FILE *stream = stdout) -> void;

auto print(const Module mod, std::FILE *stream = stdout) -> void;

} // namespace AST

#endif // !__AST_HPP
