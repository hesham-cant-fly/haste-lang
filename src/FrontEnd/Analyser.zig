const std = @import("std");
const AST = @import("../AST.zig");
const SymbolMod = @import("../Symbol.zig");
const Context = @import("../Context.zig").Context;

const mem = std.mem;

const Symbol = SymbolMod.Symbol;
const SymbolTable = SymbolMod.SymbolTable;

const Analyser = struct {
    allocator: mem.Allocator,
    ast: *const AST.ProgramAST,
    content: []const u8,
    ctx: Context,

    pub fn init(allocator: mem.Allocator, content: []const u8, ast: *const AST.ProgramAST) Analyser {
        return Analyser{ //
            .allocator = allocator,
            .ast = ast,
            .content = content,
            .ctx = Context.init(allocator, content),
        };
    }

    pub fn analyse(self: *@This()) void {
        for (self.ast.items) |item| {
            self.analyse_stmt(item.stmt);
        }
    }

    fn analyse_stmt(self: *@This(), node: *const AST.StmtNode) void {
        switch (node) {
            .ItemFunc => |v| self.resolveFunction(v),
            .Let => |v| self.resolveVariable(v),
            .Block => |v| self.resolveBlock(v),
            .Return => |v| self.resolveReturn(v),
            .Expr => |v| self.resolveExprStmt(v),
            else => unreachable,
        }
    }

    fn resolveFunction(self: *@This(), func: *const AST.ItemFunc) void {
        _ = self;
        _ = func;
    }

    fn resolveArgs(self: *@This(), args: []const AST.ArgNode) void {
        _ = self;
        _ = args;
    }

    fn resolveVariable(self: *@This(), let: *const AST.Let) void {
        _ = self;
        _ = let;
    }

    fn resolveBlock(self: *@This(), block: *const AST.Block) void {
        _ = self;
        _ = block;
    }

    fn resolveReturn(self: *@This(), ret: *const AST.Return) void {
        _ = self;
        _ = ret;
    }

    fn resolveExprStmt(self: *@This(), expr: *const AST.Expr) void {
        _ = self;
        _ = expr;
    }
};
