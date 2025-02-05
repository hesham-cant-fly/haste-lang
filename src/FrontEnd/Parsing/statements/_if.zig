const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");
const _block = @import("./block.zig")._block;
const _do = @import("./_do.zig")._do;

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

pub fn _if(self: *Parser) ParserError!*Stmt {
    const start = self.previous();
    const condition = try Expression.parse_expr(self);
    const consequent: *Stmt = consequent_block: {
        if (self.match(TokenType.OpenCurlyParen)) |_| {
            break :consequent_block try _block(self);
        } else if (self.match(TokenType.Do)) |_| {
            break :consequent_block try _do(self);
        } else {
            self.report_error(self.peek(), "Expected either `do` or `{` after if's condition.");
            return ParserError.UnexpectedToken;
        }
    };
    const alternate: ?*Stmt = alternate_block: {
        if (self.match(TokenType.Else)) |_| {
            if (self.match(TokenType.If)) |_| {
                break :alternate_block try _if(self);
            } else if (self.match(TokenType.OpenCurlyParen)) |_| {
                break :alternate_block try _block(self);
            } else if (self.match(TokenType.Do)) |_| {
                break :alternate_block try _do(self);
            } else {
                self.report_error(self.peek(), "Expected either `if`, `do` or `{` after else clause.");
                return ParserError.UnexpectedToken;
            }
        }
        break :alternate_block null;
    };
    const end = self.previous();

    const node = AST.create_if_stmt(self.allocator, condition, consequent, alternate);
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
}
