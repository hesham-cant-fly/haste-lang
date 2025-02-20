const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Declaration = @import("../declaration.zig");

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

pub fn _block(self: *Parser) ParserError!*Stmt {
    const start = self.previous();
    var stmts = AST.ItemList.init(self.allocator);
    while (!self.check(TokenType.CloseCurlyParen)) {
        stmts.add(try Declaration.declaration(self)) catch return ParserError.BadAllocation;
    }
    const end = try self.consume(.CloseCurlyParen, "Expected `}` at the end of the block.");

    const node = AST.create_block(self.allocator, stmts);
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
