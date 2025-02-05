const std = @import("std");
const ReporterMod = @import("../Reporter.zig");
const TokenMod = @import("../Token.zig");
const AST = @import("../AST.zig");
const Meta = @import("../Meta.zig");
const Declaration = @import("./Parsing/declaration.zig");
const Expression = @import("./Parsing/expression.zig");
const Statement = @import("./Parsing/statement.zig");

const mem = std.mem;
const debug = std.debug;

const Token = TokenMod.Token;
const TokenType = TokenMod.Type;
const TokenList = TokenMod.TokenList;

const Expr = AST.Expr;
const ExprNode = AST.ExprNode;

const Stmt = AST.Stmt;
const StmtNode = AST.StmtNode;

pub const ParserError = error{
    HasError,

    ExpectedSemiColon,
    UnexpectedStaticDeclarationToken, //
    ExpectedExpression,
    TooMuchArguments,
    UnexpectedToken,
    BadAllocation,
};

pub const Parser = struct {
    stdout: std.fs.File,
    allocator: mem.Allocator,
    content: []const u8,
    tokens: []const Token,
    program: *AST.ProgramAST,
    index: usize = 0,
    has_error: bool = false,

    pub fn init(allocator: mem.Allocator, stdout: std.fs.File, content: []const u8, tokens: []const Token, program: *AST.ProgramAST) Parser {
        return Parser{ //
            .allocator = allocator,
            .stdout = stdout,
            .content = content,
            .tokens = tokens,
            .program = program,
        };
    }

    pub fn parse(self: *Parser) ParserError!void {
        while (!self.is_at_end()) {
            const item = Declaration.static_declaration(self) catch |err| {
                try self.report(err);
                self.static_synchronize();
                self.has_error = true;
                continue;
            };

            self.program.add_item(item) catch return ParserError.BadAllocation;
        }

        if (self.has_error) {
            return ParserError.HasError;
        }
    }

    pub fn static_synchronize(self: *Parser) void {
        _ = self.advance();
        while (!self.is_at_end()) {
            switch (self.peek().kind) {
                .Func, .NotFunc => return,
                else => {},
            }
            _ = self.advance();
        }
    }

    pub fn synchronize(self: *Parser) void {
        _ = self.advance();
        while (!self.is_at_end()) : (_ = self.advance()) {
            if (self.previous().kind == .SemiColon) {
                return;
            }
            switch (self.peek().kind) {
                .Func, .NotFunc, .Let, .OpenCurlyParen, .If, .NotIf, .Do => return,
                else => {},
            }
        }
    }

    pub fn match(self: *Parser, comptime kinds: anytype) ?*const TokenMod.Token {
        const kinds_type = @TypeOf(kinds);
        if (comptime kinds_type == TokenMod.Type) {
            const kind = @as(TokenType, kinds);
            if (self.check(kind)) {
                return self.advance();
            }
            return null;
        } else {
            var entries: [kinds.len]TokenType = undefined;
            inline for (kinds, 0..) |value, i| {
                const v = @as(TokenType, value);
                entries[i] = v;
            }
            for (entries) |value| {
                if (self.check(value)) {
                    return self.advance();
                }
            }
            return null;
        }
    }

    pub fn report(self: *const Parser, err: ParserError) !void {
        switch (err) {
            ParserError.BadAllocation => return ParserError.BadAllocation,

            ParserError.ExpectedExpression => self.report_error(self.peek(), "Expected expression."),
            ParserError.UnexpectedStaticDeclarationToken => self.report_error(self.peek(), "Expected `func`, `!func`."),
            // ParserError.ExpectedSemiColon => self.report_error(self.peek(), "Expected semicolon `;`."),

            else => {},
        }
    }

    pub fn report_error(self: *const Parser, token: *const Token, msg: []const u8) void {
        const rep = token.make_report(.SyntaxError, msg);

        ReporterMod.reporter.report(self.stdout, self.content, .SyntaxError, "Unexpected Token", &[_]ReporterMod.Report{rep});
    }

    pub fn check(self: *const Parser, kind: TokenType) bool {
        if (self.is_at_end()) return false;
        return self.peek().kind == kind;
    }

    pub fn is_at_end(self: *const Parser) bool {
        return self.tokens[self.index].kind == TokenType.EOF;
    }

    pub fn consume_iden(self: *Parser, message: []const u8) !*const Token {
        if (!(self.check(.Ident) or self.check(.SpecialIdent))) {
            self.report_error(self.peek(), message);
            return ParserError.UnexpectedToken;
        }
        return self.advance();
    }

    pub fn consume_semi_colon(self: *Parser, message: []const u8) !*const Token {
        if (!self.check(.SemiColon)) {
            self.report_error(self.peek(), message);
            return ParserError.ExpectedSemiColon;
        }
        return self.advance();
    }

    pub fn consume(self: *Parser, kind: TokenType, message: []const u8) !*const Token {
        if (!self.check(kind)) {
            self.report_error(self.peek(), message);
            return ParserError.UnexpectedToken;
        }
        return self.advance();
    }

    pub fn advance(self: *Parser) *const Token {
        if (!self.is_at_end()) self.index += 1;
        return &self.tokens[self.index - 1];
    }

    pub fn peek(self: *const Parser) *const Token {
        return &self.tokens[self.index];
    }

    pub fn previous(self: *const Parser) *const Token {
        return &self.tokens[self.index - 1];
    }
};
