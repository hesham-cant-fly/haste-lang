const std = @import("std");
const TokenMod = @import("../../Token.zig");
const AST = @import("../../AST.zig");
const DataStructure = @import("../../DataStructure.zig");
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

pub fn parse_type(self: *Parser) ParserError!*AST.Type {
    return try parse_array_type(self);
}

fn parse_array_type(self: *Parser) ParserError!*AST.Type {
    if (self.match(TokenType.OpenSquareParen)) |start| {
        if (self.match(TokenType.CloseSquareParen)) |_| { // Slice type
            const items_type = try parse_type(self);
            const end = self.previous();
            const node = AST.create_slice_type(self.allocator, items_type) catch return ParserError.BadAllocation;
            const res = AST.create_type(self.allocator, start, end, node) catch return ParserError.BadAllocation;
            return res;
        }
        const size = self.match(.{ .Auto, .IntLit }) orelse return ParserError.UnexpectedToken;
        _ = try self.consume(.CloseSquareParen, "Expected a closing Square brackets ']'.");
        const items_type = try parse_type(self);
        const end = self.previous();
        const node = AST.create_array_type(self.allocator, items_type, size) catch return ParserError.BadAllocation;
        const res = AST.create_type(self.allocator, start, end, node) catch return ParserError.BadAllocation;
        return res;
    }
    return parse_primary_type(self);
}

fn parse_primary_type(self: *Parser) ParserError!*AST.Type {
    const start = self.peek();
    if (self.match(TokenType.Auto)) |_| {
        const node = AST.create_auto_type(self.allocator) catch return ParserError.BadAllocation;
        const res = AST.create_type(self.allocator, start, self.previous(), node) catch return ParserError.BadAllocation;
        return res;
    }
    if (self.match(TokenType.Ident)) |id| {
        const node = AST.create_id_type(self.allocator, id) catch return ParserError.BadAllocation;
        const res = AST.create_type(self.allocator, start, self.previous(), node) catch return ParserError.BadAllocation;
        return res;
    }
    if (self.match(TokenType.OpenParen)) |_| {
        if (self.match(TokenType.CloseParen)) |end| {
            const node = AST.create_unit_type(self.allocator) catch return ParserError.BadAllocation;
            const res = AST.create_type(self.allocator, start, end, node) catch return ParserError.BadAllocation;
            return res;
        }
        return parse_tuple_type(self);
    }
    return parse_premitive_type(self);
}

fn parse_tuple_type(self: *Parser) ParserError!*AST.Type {
    const start = self.previous();
    var types = LinkedList(*AST.Type).init(self.allocator);
    while (true) {
        types.add(try parse_type(self)) catch return ParserError.BadAllocation;
        if (self.match(TokenType.Coma) == null) break;
    }
    const end = try self.consume(.CloseParen, "Expected a ')' as closing to a tuple type.");

    const node = AST.create_tuple_type(self.allocator, types) catch return ParserError.BadAllocation;
    const res = AST.create_type(self.allocator, start, end, node) catch return ParserError.BadAllocation;
    return res;
}

fn parse_premitive_type(self: *Parser) ParserError!*AST.Type {
    const start = self.peek();
    const node = AST.create_primitive(self.allocator, switch (self.peek().kind) {
        .Uint => AST.PrimitiveType.UInt,
        .Int => AST.PrimitiveType.Int,
        .Float => AST.PrimitiveType.Float,
        .Bool => AST.PrimitiveType.Bool,
        .Void => AST.PrimitiveType.Void,

        else => return ParserError.UnexpectedToken,
    }) catch return ParserError.BadAllocation;
    const end = self.advance();

    const res = AST.create_type(self.allocator, start, end, node) catch return ParserError.BadAllocation;
    return res;
}
