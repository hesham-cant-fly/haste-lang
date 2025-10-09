#include "parser.hpp"
#include "arena.hpp"
#include "ast.hpp"
#include "mylib/result.hpp"
#include "mylib/string.hpp"
#include "token.hpp"
#include <cstddef>
#include <cstdint>
#include <initializer_list>

using AST::Expr;
using ExprKind = AST::Expr::ExprKind;

enum class Precedence : std::uint8_t {
  None,
  Assignment, // = (unimplemented)
  Term,       // + -
  Factor,     // * /
  Power,      // **
  Unary,      // - +
  Primary
};

struct Parser;

using ParsePrefixFn = auto (*)(Parser &) -> Result<AST::Expr *, ParserError>;
using ParseInfixFn = auto (*)(Parser &, const AST::Expr *left) -> Result<AST::Expr *, ParserError>;

struct ParserRule {
  ParsePrefixFn prefix;
  ParseInfixFn infix;
  Precedence precedence;
  bool right_assoc;
};

struct Parser {
  TokenList tokens;
  string &src;
  string &path;
  ArenaAllocator allocator;
  std::size_t current = 0;

  auto check(std::initializer_list<TokenKind> kinds) -> bool {
    Token current_token = peek();
    for (auto &kind : kinds) {
      if (current_token.kind == kind) return true;
    }
    return false;
  }

  auto match(std::initializer_list<TokenKind> kinds) -> bool {
    if (check(kinds)) {
      return advance(), true;
    }
    return false;
  }

  auto peek() -> Token {
    return tokens[current];
  }

  auto advance() -> Token {
    if (ended()) {
      return tokens[tokens.len - 1];
    }
    return tokens[current++];
  }

  auto previous() -> Token {
    if (current == 0) return tokens[0];
    return tokens[current - 1];
  }

  auto ended() -> bool {
    return current >= tokens.len;
  }
};

static auto parse_expression(Parser &parser)
    -> Result<AST::Expr *, ParserError>;
static auto get_rule(TokenKind kind) -> ParserRule;
static auto parse_precedence(Parser &parser, Precedence precedence)
    -> Result<AST::Expr *, ParserError>;

auto parse_tokens(TokenList tokens, string &src, string &path)
    -> Result<AST::Module, ParserError> {
  Parser parser {
    .tokens = tokens,
    .src = src,
    .path = path,
    .allocator = ArenaAllocator(),
  };

  auto expr_res = parse_expression(parser);
  if (!expr_res) {
    parser.allocator.deinit();
    return expr_res.error();
  }

  return AST::Module{
    .arena = parser.allocator,
    .root = expr_res.unwrap(),
  };
}

static auto parse_expression(Parser &parser)
    -> Result<AST::Expr *, ParserError> {
  return parse_precedence(parser, Precedence::Assignment);
}

static auto parse_unary(Parser &parser) -> Result<AST::Expr *, ParserError> {
  Token op = parser.previous();
  auto rhs = parse_precedence(parser, Precedence::Unary);
  if (!rhs) return rhs;
  return parser.allocator.create(AST::Expr{
    .kind = AST::Expr::ExprKind::Unary,
    .unary = {
      .rhs = rhs.unwrap(),
      .op = op,
    },
  });
}

static auto parse_assignment(Parser &parser, const AST::Expr *left)
    -> Result<AST::Expr *, ParserError> {
  Token op = parser.previous();
  if (*left != ExprKind::Idetifier)
    return ParserError::InvalidToken;

  auto value_res = parse_precedence(parser, Precedence::Assignment);
  if (!value_res) return value_res;

  return parser.allocator.create(AST::Expr{
    .kind = ExprKind::Assign,
    .assign = {
      .target = left,
      .value = value_res.unwrap(),
      .op = op,
    },
  });
}

static auto parse_binary(Parser &parser, const AST::Expr *left) -> Result<AST::Expr *, ParserError> {
  Token op = parser.previous();
  ParserRule rule = get_rule(op.kind);
  Precedence precedence = (Precedence)((int)rule.precedence + 1);
  if (rule.right_assoc) precedence = rule.precedence;

  auto rhs_res = parse_precedence(parser, precedence);
  if (!rhs_res) return rhs_res;

  return parser.allocator.create(AST::Expr{
    .kind = ExprKind::Binary,
    .binary = {
      .lhs = left,
      .rhs = rhs_res.unwrap(),
      .op = op,
    },
  });
}

static auto parse_int(Parser &parser) -> Result<AST::Expr *, ParserError> {
  Token lit = parser.previous();
  return parser.allocator.create(AST::Expr{
    .kind = ExprKind::Int,
    .integer_lit = lit,
  });
}

static auto parse_float(Parser &parser) -> Result<AST::Expr *, ParserError> {
  Token lit = parser.previous();
  return parser.allocator.create(AST::Expr{
    .kind = ExprKind::Float,
    .float_lit = lit,
  });
}

static auto parse_identifier(Parser &parser)
    -> Result<AST::Expr *, ParserError> {
  Token lit = parser.previous();
  return parser.allocator.create(AST::Expr{
    .kind = ExprKind::Idetifier,
    .identifier = lit,
  });
}

static auto get_rule(TokenKind kind) -> ParserRule {
  switch (kind) {
  case TokenKind::Equal:      return { nullptr,          parse_assignment, Precedence::Assignment, true};
  case TokenKind::Plus:       return { parse_unary,      parse_binary,     Precedence::Term,       false };
  case TokenKind::Minus:      return { parse_unary,      parse_binary,     Precedence::Term,       false };
  case TokenKind::Star:       return { nullptr,          parse_binary,     Precedence::Factor,     false };
  case TokenKind::FSlash:     return { nullptr,          parse_binary,     Precedence::Factor,     false };
  case TokenKind::DoubleStar: return { nullptr,          parse_binary,     Precedence::Power,      true  };
  case TokenKind::IntLit:     return { parse_int,        nullptr,          Precedence::Primary,    false };
  case TokenKind::FloatLit:   return { parse_float,      nullptr,          Precedence::Primary,    false };
  case TokenKind::Identifier: return { parse_identifier, nullptr,          Precedence::Primary,    false };
  default:                    return { nullptr,          nullptr,          Precedence::None,       0     };
  }
}

static auto parse_precedence(Parser &parser, Precedence precedence)
    -> Result<AST::Expr*, ParserError>
{
  Token tok = parser.advance();
  auto prefix_rule = get_rule(tok.kind).prefix;
  if (!prefix_rule) return ParserError::InvalidToken;

  auto left_res = prefix_rule(parser);
  if (!left_res) return left_res;
  AST::Expr *left = left_res.unwrap();

  while (true) {
    const auto &rule = get_rule(parser.peek().kind);
    if (precedence > rule.precedence) break;

    tok = parser.advance();
    auto infix_rule = rule.infix;
    if (!infix_rule) return ParserError::InvalidToken;

    auto lhs_res = infix_rule(parser, left);
    if (!lhs_res) return lhs_res;
    left = lhs_res.unwrap();
  }

  return left;
}

