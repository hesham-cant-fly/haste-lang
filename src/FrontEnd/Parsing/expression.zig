const std = @import("std");
const DataStructure = @import("../../DataStructure.zig");
const TokenMod = @import("../../Token.zig");
const AST = @import("../../AST.zig");
const Meta = @import("../../Meta.zig");
const ParserMod = @import("../Parser.zig");

const Parser = ParserMod.Parser;
const ParserError = ParserMod.ParserError;

const Token = TokenMod.Token;
const TokenType = TokenMod.Type;
const TokenList = TokenMod.TokenList;

const Expr = AST.Expr;
const ExprNode = AST.ExprNode;

const Stmt = AST.Stmt;
const StmtNode = AST.StmtNode;

const LinkedList = DataStructure.LinkedList;

const mem = std.mem;
const debug = std.debug;

pub fn parse_expr(self: *Parser) ParserError!*Expr {
    return parse_inline_if(self);
}

fn parse_inline_if(self: *Parser) ParserError!*Expr {
    if (self.match(TokenType.If) == null) return parse_assignment(self);
    const start = self.previous();
    const condition = try parse_expr(self);

    const then_token = try self.consume(.Then, "Expected 'then'.");
    const consequent = try parse_expr(self);

    const else_token = try self.consume(.Else, "Expected 'else'.");
    const alternate = try parse_expr(self);

    const end = self.previous();
    const tmp = AST.create_inline_if(self.allocator, condition, consequent, alternate) catch return ParserError.BadAllocation;
    tmp.InlineIf.if_token = start;
    tmp.InlineIf.then_token = then_token;
    tmp.InlineIf.else_token = else_token;

    const result = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

    return result;
}

fn parse_assignment(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_or(self);

    while (self.match(TokenType.Eq)) |op| {
        const right = try parse_expr(self);
        const tmp = AST.create_assignment(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_or(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_and(self);

    while (self.match(TokenType.Or)) |op| {
        const right = try parse_and(self);
        const tmp = AST.create_logical_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_and(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_relational(self);

    while (self.match(TokenType.And)) |op| {
        const right = try parse_relational(self);
        const tmp = AST.create_logical_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_relational(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_addition(self);

    while (self.match(.{ .EqEq, .NotEq, .LessThan, .LessThanEq, .GreaterThan, .GreaterThanEq })) |op| {
        const right = try parse_addition(self);
        const tmp = AST.create_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_addition(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_factor(self);

    while (self.match(.{ .Plus, .Minus })) |op| {
        const right = try parse_factor(self);
        const tmp = AST.create_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_factor(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_power(self);

    while (self.match(.{ .Star, .FSlash, .PercentSign })) |op| {
        const right = try parse_power(self);
        const tmp = AST.create_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_power(self: *Parser) ParserError!*Expr {
    var start = self.peek();
    var left = try parse_unary(self);

    while (self.match(TokenType.DoubleStar)) |op| {
        const right = try parse_unary(self);
        const tmp = AST.create_binary(self.allocator, left, op, right) catch return ParserError.BadAllocation;
        const end = self.previous();
        left = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;

        start = self.peek();
    }

    return left;
}

fn parse_unary(self: *Parser) ParserError!*Expr {
    const start = self.peek();
    while (self.match(.{ .Not, .Minus })) |op| {
        const right = try parse_unary(self);
        var tmp: *ExprNode = undefined;
        const end = self.previous();

        if (op.kind == .Not) {
            tmp = AST.create_logical_unary(self.allocator, op, right) catch return ParserError.BadAllocation;
        } else {
            tmp = AST.create_unary(self.allocator, op, right) catch return ParserError.BadAllocation;
        }

        const res = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;
        return res;
    }

    return parse_call(self);
}

fn parse_call(self: *Parser) ParserError!*Expr {
    const start = self.peek();
    var expr = try parse_primary(self);
    expr.start = start;

    while (true) {
        if (self.match(TokenType.OpenParen)) |_| {
            const tmp = try finish_call(self, expr);
            expr = AST.create_expr(self.allocator, start, self.previous(), tmp) catch return ParserError.BadAllocation;
        } else {
            break;
        }
    }

    return expr;
}

fn finish_call(self: *Parser, callee: *Expr) ParserError!*ExprNode {
    const paren = self.previous();
    var arguments = LinkedList(*Expr).init(self.allocator);
    if (!self.check(.CloseParen)) {
        while (true) {
            if (arguments.len > 225) {
                return ParserError.TooMuchArguments;
            }

            const ex = try parse_expr(self);
            arguments.add(ex) catch return ParserError.UnexpectedToken;
            if (self.match(TokenType.Coma) == null) break;
        }
    }

    // _ = try self.consume(.CloseParen, "Expected ')' after arguments.");

    _ = self.consume(.CloseParen, "Expected ')' after arguments.") catch |err| {
        // std.debug.panic("here\n.", .{});
        return err;
    };

    return AST.create_call(self.allocator, callee, paren, arguments) catch ParserError.BadAllocation;
}

fn parse_primary(self: *Parser) ParserError!*Expr {
    const start = self.peek();
    if (self.match(.{ .True, .False, .Ident, .SpecialIdent, .StringLit, .FloatLit, .IntLit })) |token| {
        const tmp = AST.create_primary(self.allocator, token) catch return ParserError.BadAllocation;
        const end = self.previous();
        const res = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;
        return res;
    }

    if (self.match(TokenType.OpenParen)) |_| {
        if (self.match(TokenType.CloseParen)) |end| {
            const node = AST.create_unit_expr(self.allocator) catch return ParserError.BadAllocation;
            const res = AST.create_expr(self.allocator, start, end, node) catch return ParserError.BadAllocation;
            return res;
        }
        const expr: *Expr = try parse_expr(self);
        if (self.match(TokenType.Coma)) |_| {
            return try parse_tuple(self, start, expr);
        }
        const end = try self.consume(.CloseParen, "Expected ')' as closing.");
        const tmp = AST.create_grouping(self.allocator, expr) catch return ParserError.BadAllocation;
        const res = AST.create_expr(self.allocator, start, end, tmp) catch return ParserError.BadAllocation;
        return res;
    }

    if (self.match(TokenType.If)) |_| {
        self.index -= 1;
        return parse_inline_if(self);
    }

    return ParserError.ExpectedExpression;
}

fn parse_tuple(self: *Parser, start: *const Token, first_expr: *Expr) ParserError!*Expr {
    var tuple_values = LinkedList(*Expr).init(self.allocator);

    tuple_values.add(first_expr) catch return ParserError.BadAllocation;

    while (true) {
        tuple_values.add(try parse_expr(self)) catch return ParserError.BadAllocation;
        if (self.match(TokenType.Coma) == null) break;
    }
    const end = try self.consume(.CloseParen, "Expected a ')' as closing for the tuple.");
    const result_tuple = AST.create_tuple_expr(self.allocator, tuple_values) catch return ParserError.BadAllocation;
    const result = AST.create_expr(self.allocator, start, end, result_tuple) catch return ParserError.BadAllocation;
    return result;
}
