#ifndef __MY_PRINTER_H
#define __MY_PRINTER_H
/* #define MY_PRINTER_IMPLEMENTATION */

#include <stdio.h>
#include "core/my_commons.h"

#ifndef CONCATE
#define CONCATE(__x, __y) __x##__y
#endif // !CONCATE

#ifndef IDENTATION
#define IDENTATION 4
#endif // !IDENTATION

/* Primitives */
#define PRINT_STRING_indent(...) PRINT_STRING(__VA_ARGS__)
#define PRINT_STRING(__file, __value, ...) fprintf((__file), "\"%s\"", (__value))

#define PRINT_INT_indent(...) PRINT_INT(__VA_ARGS__)
#define PRINT_INT(__file, __value, ...) fprintf((__file), "%d", (__value))

#define PRINT_LONG_DOUBLE_indent(...) PRINT_LONG_DOUBLE(__VA_ARGS__)
#define PRINT_LONG_DOUBLE(__file, __value, ...) fprintf((__file), "%LF", (__value))

#define PRINT_DOUBLE_indent(...) PRINT_DOUBLE(__VA_ARGS__)
#define PRINT_DOUBLE(__file, __value, ...) fprintf((__file), "%lf", (__value))

#define PRINT_SIZE_T_indent(...) PRINT_SIZE_T(__VA_ARGS__)
#define PRINT_SIZE_T(__file, __value, ...) fprintf((__file), "%zu", (__value))

#define PRINT_SSIZE_T_indent(...) PRINT_SSIZE_T(__VA_ARGS__)
#define PRINT_SSIZE_T(__file, __value, ...) fprintf((__file), "%zi", (__value))

#define PRINT_UINT32_T_indent(...) PRINT_UINT32_T(__VA_ARGS__)
#define PRINT_UINT32_T(__file, __value, ...) fprintf((__file), "%u", (__value))

#define PRINT_PTR_indent(...) PRINT_PTR(__VA_ARGS__)
#define PRINT_PTR(__file, __value, ...) fprintf((__file), "%p", (__value))

#define PRINT_NONE_indent(...) PRINT_NONE(__VA_ARGS__)
#define PRINT_NONE(__file, ...) fprintf((__file), "{ }")

#define GEN_PRINTER_DEF(__type, __fn_name)                                     \
  void __fn_name(FILE *f, const __type v);                                     \
  void CONCATE(__fn_name, _ptr)(FILE * f, const __type *v);                    \
  void CONCATE(__fn_name, _ptr_indent)(FILE * f, const __type *v, int i);      \
  void CONCATE(__fn_name, _indent)(FILE * f, const __type v, int i);           \
  void CONCATE(__fn_name, ln)(FILE * f, const __type v)

#define GEN_COSTOM_PRINTER_IMPL_START(__type, __fn_name)                       \
  void __fn_name(FILE *f, const __type v) { CONCATE(__fn_name, _indent)(f, v, IDENTATION); } \
  void CONCATE(__fn_name, _ptr)(FILE *f, const __type *v) { __fn_name(f, *v); } \
  void CONCATE(__fn_name, _ptr_indent)(FILE *f, const __type *v, int i) {        \
    CONCATE(__fn_name, _indent)(f, *v, i);                              \
  }                                                                            \
  void CONCATE(__fn_name, ln)(FILE *f, const __type v) {                       \
    __fn_name(f, v);                                                           \
    fprintf(f, "\n");                                                          \
  }                                                                            \
  void CONCATE(__fn_name, _indent)(FILE *f, const __type v, int i) {    \
    unused(i);

#define GEN_COSTOM_PRINTER_IMPL_END(__type, __fn_name) }

/* Structs */
#define X_STRUCT(__type, __name, ...) __type __name;
#define GEN_STRUCT_PRINT_IMPL(__type, __fn_name, M)                            \
  void __fn_name(FILE *f, const __type v) {                                    \
    CONCATE(__fn_name, _indent)(f, v, IDENTATION);                             \
  }                                                                            \
  void CONCATE(__fn_name, _ptr)(FILE * f, const __type *v) {                   \
    __fn_name(f, *v);                                                          \
  }                                                                            \
  void CONCATE(__fn_name, _ptr_indent)(FILE * f, const __type *v, int i) {     \
    CONCATE(__fn_name, _indent)(f, *v, i);                                     \
  }                                                                            \
  void CONCATE(__fn_name, _indent)(FILE * f, const __type v, int i) {          \
    fprintf(f, "(%s) {\n", #__type);                                           \
    M(__STRUCT_PRINT);                                                         \
    __print_indent(f, i > IDENTATION ? i - IDENTATION : 0);                    \
    fprintf(f, "}");                                                           \
  }                                                                            \
  void CONCATE(__fn_name, ln)(FILE * f, const __type v) {                      \
    __fn_name(f, v);                                                           \
    fprintf(f, "\n");                                                          \
  }

#define __STRUCT_PRINT(__type, __name, __print_fn)                             \
  do {                                                                         \
    __print_indent(f, i);                                                      \
    fprintf(f, ".%s = ", #__name);                                             \
    CONCATE(__print_fn, _indent)(f, (v).__name, i + IDENTATION);                        \
    fprintf(f, ",\n");                                                         \
  } while (0);

/* Tagged Union */
#define X_UNION_TAG(__tag, ...) X_ENUM(__tag)
#define X_TAGGED_UNION(__tag, __type, __name, ...) __type __name;

#define GEN_TAGGED_UNION_TAG_PRINT_IMPL(__type, __fn_name, M)                  \
  void __fn_name(FILE *f, const __type v) {                                    \
    CONCATE(__fn_name, _indent)(f, v, 0);                                      \
  }                                                                            \
  void CONCATE(__fn_name, _ptr)(FILE * f, const __type *v) {                   \
    __fn_name(f, *v);                                                          \
  }                                                                            \
  void CONCATE(__fn_name, _ptr_indent)(FILE * f, const __type *v, int i) {     \
    CONCATE(__fn_name, _indent)(f, *v, i);                                     \
  }                                                                            \
  void CONCATE(__fn_name, _indent)(FILE * f, const __type v, int i) {          \
    unused(i);                                                                 \
    switch (v) {                                                               \
      M(__UNION_TAG_PRINT)                                                     \
    default:                                                                   \
      fprintf(f, "%s.%d", #__type, v);                                         \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  void CONCATE(__fn_name, ln)(FILE * f, const __type v) {                      \
    __fn_name(f, v);                                                           \
    fprintf(f, "\n");                                                          \
  }

#define __UNION_TAG_PRINT(__tag, ...)                                          \
  case __tag:                                                                  \
    fprintf(f, ".%s", #__tag);                                                 \
    break;

#define GEN_TAGGED_UNION_PRINT_IMPL(__type, __fn_name, M)                      \
  void __fn_name(FILE *f, const __type v) {                                    \
    CONCATE(__fn_name, _indent)(f, v, 0);                                      \
  }                                                                            \
  void CONCATE(__fn_name, _ptr)(FILE * f, const __type *v) {                   \
    __fn_name(f, *v);                                                          \
  }                                                                            \
  void CONCATE(__fn_name, _ptr_indent)(FILE * f, const __type *v, int i) {     \
    CONCATE(__fn_name, _indent)(f, *v, i);                                     \
  }                                                                            \
  void CONCATE(__fn_name, _indent)(FILE * f, const __type v, int i) {          \
    fprintf(f, "(%s) {\n", #__type);                                           \
    switch (v.tag) {                                                           \
      M(__TAGGED_UNION_PRINT)                                                  \
    default:                                                                   \
      fprintf(stderr, "Wait WTF???? %d\n", v.tag);                             \
      break;                                                                   \
    }                                                                          \
    __print_indent(f, i > IDENTATION ? i - IDENTATION : 0);                    \
    fprintf(f, "}");                                                           \
  }                                                                            \
  void CONCATE(__fn_name, ln)(FILE * f, const __type v) {                      \
    __fn_name(f, v);                                                           \
    fprintf(f, "\n");                                                          \
  }

#define __TAGGED_UNION_PRINT(__tag, __type, __name, __print)                   \
  case __tag:                                                                  \
    do {                                                                       \
      __print_indent(f, i);                                                    \
      fprintf(f, ".%s = ", #__name);                                           \
      CONCATE(__print, _indent)(f, ((v).as).__name, i + IDENTATION);    \
      fprintf(f, ",\n");                                                       \
    } while (0);                                                               \
    break;

/* Enums */
/* Example: */
/*
#define TOKEN_KIND_ENUM_DEF(M)                                                 \
  M(TOKEN_KIND_INT_LIT)                                                        \
  M(TOKEN_KIND_INT_FLOAT)                                                      \
  M(TOKEN_KIND_PLUS)                                                           \
  M(TOKEN_KIND_MINUS)                                                          \
  M(TOKEN_KIND_STAR)                                                           \
  M(TOKEN_KIND_FSLASH)                                                         \
  M(TOKEN_KIND_DOUBLE_STAR)

typedef enum TokenKind {
  TOKEN_KIND_ENUM_DEF(X_ENUM)
} TokenKind;

GEN_ENUM_PRINT_DEF(TokenKind, print_token_kind);
GEN_ENUM_PRINT_IMPL(TokenKind, print_token_kind, TOKEN_KIND_ENUM_DEF);
*/
#define X_ENUM(__kind) __kind,
#define GEN_ENUM_PRINT_IMPL(__type, __fn_name, M)                              \
  void __fn_name(FILE *f, const __type v) {                                    \
    CONCATE(__fn_name, _indent)(f, v, 0);                                      \
  }                                                                            \
  void CONCATE(__fn_name, _ptr)(FILE * f, const __type *v) {                   \
    __fn_name(f, *v);                                                          \
  }                                                                            \
  void CONCATE(__fn_name, _ptr_indent)(FILE * f, const __type *v, int i) {     \
    CONCATE(__fn_name, _indent)(f, *v, i);                                     \
  }                                                                            \
  void CONCATE(__fn_name, _indent)(FILE * f, const __type v, int i) {          \
    unused(i);                                                                 \
    switch (v) {                                                               \
      M(__ENUM_PRINT)                                                          \
    default:                                                                   \
      fprintf(f, "%s.%d", #__type, v);                                         \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  void CONCATE(__fn_name, ln)(FILE * f, const __type v) {                      \
    __fn_name(f, v);                                                           \
    fprintf(f, "\n");                                                          \
  }

#define __ENUM_PRINT(__kind)                                                   \
  case __kind:                                                                 \
    fprintf(f, ".%s", #__kind);                                                \
    break;

void __print_indent(FILE *f, int ident);

#ifdef MY_PRINTER_IMPLEMENTATION
void __print_indent(FILE *f, int ident) {
  int i;
  for (i = 0; i < ident; ++i) {
    fprintf(f, " ");
  }
}
#endif /* MY_PRINTER_IMPLEMENTATION */

#endif /* !__MY_PRINTER_H */
