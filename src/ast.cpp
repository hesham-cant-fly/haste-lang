#include "ast.hpp"
#include "common.hpp"
#include "token.hpp"
#include <cstdio>

namespace AST {

auto Expr::is(ExprKind kind) const -> bool { return this->kind == kind; }

auto Expr::operator==(const ExprKind &other) const -> bool { return is(other); }

auto Module::deinit() -> void {
  arena.deinit();
  root = nullptr;
}

auto print(const Token token, std::FILE *stream) -> void {
  token.print(stream);
}

auto print(const Expr::ExprKind kind, std::FILE *stream) -> void {
  using ExprKind = Expr::ExprKind;
#define $X(enum_field_name, union_field_name, ...)                             \
  case ExprKind::enum_field_name:                                              \
    std::fprintf(stream, #union_field_name);                                   \
    break;
  switch (kind) {
    $AST_EXPR_KIND_LIST
  }
#undef $X
}

auto print(const Expr expr, std::FILE *stream) -> void {
  using ExprKind = Expr::ExprKind;

  std::fprintf(stream, "(expr ");
  $defer([&] { std::fprintf(stream, ")"); });

#define $X(enum_field_name, union_field_name, ...)                             \
  case ExprKind::enum_field_name:                                              \
    print(expr.union_field_name, stream);                                      \
    break;
  switch (expr.kind) {
    $AST_EXPR_KIND_LIST
  }
#undef $X
}

auto print(const Expr *expr, std::FILE *stream) -> void {
  print(*expr, stream);
}

auto print(const Expr::Assign assign, std::FILE *stream) -> void {
  std::fprintf(stream, "(assign ");
  $defer([&] { std::fprintf(stream, ")"); });

  std::fprintf(stream, ":op ");
  print(assign.op, stream);

  std::fprintf(stream, ":target ");
  print(*assign.target, stream);

  std::fprintf(stream, ":value ");
  print(*assign.value, stream);
}

auto print(const Expr::Binary bin, std::FILE *stream) -> void {
  std::fprintf(stream, "(binary ");
  $defer([&] { std::fprintf(stream, ")"); });

  std::fprintf(stream, ":op ");
  print(bin.op, stream);

  std::fprintf(stream, " :lhs ");
  print(*bin.lhs, stream);

  std::fprintf(stream, " :rhs ");
  print(*bin.rhs, stream);
}

auto print(const Expr::Unary unary, std::FILE *stream) -> void {
  std::fprintf(stream, "(unary ");
  $defer([&] { std::fprintf(stream, ")"); });

  std::fprintf(stream, ":op ");
  print(unary.op, stream);


  std::fprintf(stream, " :rhs ");
  print(*unary.rhs, stream);
}

auto print(const Type type, std::FILE *stream) -> void {
  using TypeKind = Type::TypeKind;

  std::fprintf(stream, "(type ");
  $defer([&] { std::fprintf(stream, ")"); });

#define $X(enum_field_name, union_field_name, ...)                             \
  case TypeKind::enum_field_name:                                              \
    print(type.union_field_name, stream);                                   \
    break;
  switch (type.kind) {
    $AST_TYPE_KIND_LIST
  }
#undef $X
}

auto print(const Type *type, std::FILE *stream) -> void {
  print(*type);
}

auto print(const Type::TypeKind kind, std::FILE *stream) -> void {
  using TypeKind = Type::TypeKind;
#define $X(enum_field_name, union_field_name, ...)                             \
  case TypeKind::enum_field_name:                                              \
    std::fprintf(stream, #union_field_name);                                   \
    break;
  switch (kind) {
    $AST_TYPE_KIND_LIST
  }
#undef $X
}

auto print(const Stmt stmt, std::FILE *stream) -> void {
  using StmtKind = Stmt::StmtKind;

  std::fprintf(stream, "(stmt ");
  $defer([&] { std::fprintf(stream, ")"); });

#define $X(enum_field_name, union_field_name, ...)                             \
  case StmtKind::enum_field_name:                                              \
    print(stmt.union_field_name, stream);                                      \
    break;
  switch (stmt.kind) {
    $AST_STMT_KIND_LIST
  }
#undef $X
}

auto print(const Stmt::StmtKind kind, std::FILE *stream) -> void {
  using StmtKind = Stmt::StmtKind;
#define $X(enum_field_name, union_field_name, ...)                             \
  case StmtKind::enum_field_name:                                              \
    std::fprintf(stream, #union_field_name);                                   \
    break;
  switch (kind) {
    $AST_STMT_KIND_LIST
  }
#undef $X
}

auto print(const Stmt::Variable variable, std::FILE *stream) -> void {
  std::fprintf(stream, "(variable ");
  $defer ([&] { std::fprintf(stream, ")"); });

  std::fprintf(stream, ":name ");
  print(variable.name, stream);

  std::fprintf(stream, ":type ");
  if (variable.type) {
    print(*variable.type.unwrap(), stream);
  } else {
    std::fprintf(stream, "nil ");
  }

  std::fprintf(stream, ":value ");
  if (variable.value) {
    print(*variable.value.unwrap(), stream);
  } else {
    std::fprintf(stream, "nil ");
  }
}

auto print(const Stmt::Constant constant, std::FILE *stream) -> void {
  std::fprintf(stream, "(constant ");
  $defer ([&] { std::fprintf(stream, ")"); });

  std::fprintf(stream, ":name ");
  print(constant.name, stream);

  std::fprintf(stream, ":type ");
  if (constant.type) {
    print(*constant.type.unwrap(), stream);
  } else {
    std::fprintf(stream, "nil ");
  }

  std::fprintf(stream, ":value ");
  if (constant.value) {
    print(*constant.value.unwrap(), stream);
  } else {
    std::fprintf(stream, "nil ");
  }
}

auto print(Module mod, std::FILE *stream) -> void { print(*mod.root, stream); }

} // namespace AST
