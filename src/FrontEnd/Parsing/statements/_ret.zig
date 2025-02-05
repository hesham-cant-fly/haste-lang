const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");

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

pub fn _ret(self: *Parser) ParserError!*AST.Stmt {
    const start = self.previous();
    const node = AST.create_ret_stmt(self.allocator, null);
    if (self.match(TokenType.SemiColon)) |_| {
        const end = try self.consume(.SemiColon, "Expected `;` at the end of 'return'.");
        const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
        return res;
    }

    const expr = try Expression.parse_expr(self);
    node.Ret.expr = expr;

    const end = try self.consume(.SemiColon, "Expected `;` at the end of 'return'.");
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
