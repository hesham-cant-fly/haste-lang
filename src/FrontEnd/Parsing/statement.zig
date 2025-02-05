const std = @import("std");
const TokenMod = @import("../../Token.zig");
const AST = @import("../../AST.zig");
const Meta = @import("../../Meta.zig");
const ParserMod = @import("../Parser.zig");
const Expression = @import("./expression.zig");
const _return = @import("./statements/_return.zig")._return;
const _if = @import("./statements/_if.zig")._if;
const _do = @import("./statements/_do.zig")._do;
const _block = @import("./statements/block.zig")._block;
const _ret = @import("./statements/_ret.zig")._ret;

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

pub fn statement(self: *Parser) ParserError!*Stmt {
    _ = self.advance();
    return switch (self.previous().kind) {
        .Ret => _ret(self),
        .Return => _return(self),
        .Do => _do(self),
        .If => _if(self),
        .OpenCurlyParen => _block(self),
        else => {
            self.index -= 1;
            return statement_expr(self);
        }
    };
}

pub fn statement_expr(self: *Parser) ParserError!*Stmt {
    const start = self.peek();
    const expr = try Expression.parse_expr(self);
    const end = try self.consume(.SemiColon, "Expected `;` at the end of the expression.");

    const node = AST.create_expr_stmt(self.allocator, expr);
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
