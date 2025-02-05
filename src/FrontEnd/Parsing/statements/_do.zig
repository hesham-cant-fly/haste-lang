const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");
const statement = @import("../statement.zig");

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

pub fn _do(self: *Parser) ParserError!*Stmt {
    const start = self.previous();
    const stmt = try statement.statement(self);
    const end = self.previous();

    const node = AST.create_do_stmt(self.allocator, stmt.clone(self.allocator));
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
