const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");
const parse_type = @import("../types.zig").parse_type;
const _return = @import("../statements/_return.zig")._return;
const block = @import("../statements/block.zig").block;

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

pub fn function(self: *Parser) ParserError!*StmtNode {
    const ident = try self.consume_iden("Expected an identifier of the function name.");
    _ = try self.consume(.OpenParen, "");
    var params = std.ArrayList(AST.ArgNode).init(self.allocator);
    if (!self.check(.CloseParen)) {
        while (true) {
            const param = try parse_param(self);
            params.append(param) catch return ParserError.BadAllocation;
            if (self.match(TokenType.Coma) == null) break;
        }
    }
    _ = try self.consume(.CloseParen, "Expected ')' as closing of function params.");
    params.shrinkAndFree(params.items.len);

    var return_type: ?*const AST.Type = null;
    if (self.match(TokenType.Colon)) |_| {
        return_type = try parse_type(self);
    }

    const body_start = self.peek();
    var body: Stmt = undefined;
    if (self.match(TokenType.Eq)) |_| {
        body.stmt = try _return(self); // Direcly parses the return value
    } else if (self.match(TokenType.OpenCurlyParen)) |_| {
        body.stmt = try block(self); // Function body is just a block
    } else {
        return ParserError.UnexpectedToken;
    }

    body.start = body_start;
    body.end = self.previous();

    const stmt = AST.create_func(self.allocator) catch return ParserError.BadAllocation;
    stmt.ItemFunc = .{ //
        .identifier = ident,
        .arguments = params.items,
        .return_type = return_type,
        .body = body,
    };

    return stmt;
}

fn parse_param(self: *Parser) ParserError!AST.ArgNode {
    const start = self.peek();
    const ident = try self.consume_iden("Expected an identifier for the name of the param.");
    _ = try self.consume(.Colon, "Expected ':' after the param name.");

    const tp = try parse_type(self);
    var default: ?*const AST.Expr = null;
    if (self.match(TokenType.Eq)) |_| {
        default = try Expression.parse_expr(self);
    }

    const end = self.previous();
    return AST.ArgNode{ //
        .start = start,
        .end = end,
        .ident = ident,
        .tp = tp,
        .default = default,
    };
}
