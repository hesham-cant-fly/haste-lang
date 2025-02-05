const std = @import("std");
const TokenMod = @import("../../Token.zig");
const AST = @import("../../AST.zig");
const Meta = @import("../../Meta.zig");
const ParserMod = @import("../Parser.zig");
const Statemet = @import("./statement.zig");
const function = @import("./declarations/function.zig").function;
const let = @import("./declarations/let.zig").let;

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

pub fn static_declaration(self: *Parser) ParserError!*Stmt { //
    if (self.match(TokenType.Func)) |_| {
        return function(self);
    }

    self.has_error = true;
    return ParserError.UnexpectedStaticDeclarationToken;
}

pub fn declaration(self: *Parser) ParserError!*Stmt {
    if (self.match(TokenType.Let)) |_| {
        return let(self);
    } else if (self.match(TokenType.Func)) |_| {
        return function(self);
    }

    return Statemet.statement(self) catch {
        self.has_error = true;
        self.synchronize();
        const node = AST.create_none_stmt(self.allocator);
        const res = AST.create_stmt(self.allocator, undefined, undefined, .Inherited, node);
        return res;
    };
}
