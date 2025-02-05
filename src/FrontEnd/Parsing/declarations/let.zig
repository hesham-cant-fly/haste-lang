const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");
const parse_type = @import("../types.zig").parse_type;

const Parser = ParserMod.Parser;
const ParserError = ParserMod.ParserError;

const Token = TokenMod.Token;
const TokenType = TokenMod.Type;
const TokenList = TokenMod.TokenList;

const Expr = AST.Expr;
const ExprNode = AST.ExprNode;

const Stmt = AST.Stmt;
const StmtNode = AST.StmtNode;

const mem = std.mem;
const debug = std.debug;

pub fn let(self: *Parser) ParserError!*Stmt {
    const start = self.previous();
    const mut = self.match(TokenType.Mut);
    const ident = try self.consume_iden(if (mut) |_| "Expected variable name." else "Expected `mut` or the variable name.");
    var tp: ?*const AST.Type = null;
    if (self.match(TokenType.Colon)) |_| {
        tp = try parse_type(self);
    }
    var eq: ?*const Token = null;
    var expr: ?*const Expr = null;
    if (self.match(TokenType.Eq)) |token| {
        eq = token;
        expr = try Expression.parse_expr(self);
    }

    const end = try self.consume_semi_colon("Expected `;` at the end of 'let' statement.");

    const node = AST.create_let(self.allocator);
    node.Let = .{ .mut = mut, .identifier = ident, .tp = tp, .eq = eq, .init = expr };
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
