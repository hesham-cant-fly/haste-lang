const std = @import("std");
const TokenMod = @import("../../../Token.zig");
const AST = @import("../../../AST.zig");
const Meta = @import("../../../Meta.zig");
const ParserMod = @import("../../Parser.zig");
const Expression = @import("../expression.zig");
const parse_type = @import("../types.zig").parse_type;
const _return = @import("../statements/_return.zig")._return;
const _block = @import("../statements/block.zig")._block;

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

pub fn function(self: *Parser) ParserError!*Stmt {
    const start = self.previous();
    const ident = try self.consume_iden("Expected an identifier of the function name.");
    _ = try self.consume(.OpenParen, "Expected '('.");
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
        // FIXME: THIS DON'T EMIT AND ERROR TO THE USER
        return_type = try parse_type(self);
    }

    const body: *Stmt = body_block: {
        if (self.match(TokenType.Eq)) |_| {
            break :body_block try _return(self); // Direcly parses the return value
        } else if (self.match(TokenType.OpenCurlyParen)) |_| {
            break :body_block try _block(self); // Function body is just a block
        } else {
            return ParserError.UnexpectedToken;
        }
    };
    const end = self.previous();

    const node = AST.create_func(self.allocator);
    node.Func = .{ //
        .identifier = ident,
        .arguments = params.items,
        .return_type = return_type,
        .body = body,
    };
    const res = AST.create_stmt(self.allocator, start, end, .Inherited, node);
    return res;
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
