const std = @import("std");
const SymbolMod = @import("./Symbol.zig");
const TypeMod = @import("./Type.zig");
const Token = @import("./Token.zig").Token;

const mem = std.mem;

const Symbol = SymbolMod.Symbol;
const SymbolTable = SymbolMod.SymbolTable;

pub const Context = struct {
    allocator: mem.Allocator,
    current_function: ?*Symbol = null,
    symbol_table: SymbolTable,

    source_content: []const u8,
    error_count: u32 = 0,
    warning_count: u32 = 0,

    pub fn init(allocator: mem.Allocator, source_content: []const u8) Context {
        return Context{ .allocator = allocator, .source_content = source_content, .symbol_table = SymbolTable.init(allocator) };
    }

    pub fn deinit(self: *Context) void {
        self.current_function = null;
        self.symbol_table.deinit();
    }

    pub fn is_declared(self: *const Context, key: *const Token) bool {
        return self.symbol_table.look_up(key) != null;
    }

    pub fn is_defined(self: *const Context, key: *const Token) bool {
        const symbol = self.symbol_table.look_up(key) orelse return false;
        return symbol.defined;
    }

    pub fn is_in_function(self: *const Context) bool {
        return self.current_function != null;
    }

    pub fn has_error(self: *const Context) bool {
        return self.error_count != 0;
    }

    pub fn has_warning(self: *const Context) bool {
        return self.warning_count != 0;
    }

    pub inline fn declare(self: *Context, ident: *const Token) !void {
        return self.symbol_table.declare(ident);
    }

    pub inline fn define(self: *Context, ident: *const Token, tp: TypeMod.Type, mut: bool) !void {
        return self.symbol_table.define(ident, tp, mut);
    }

    pub inline fn look_up(self: *const Context, key: *const Token) ?*Symbol {
        return self.symbol_table.look_up(key);
    }

    pub inline fn scope_begin(self: Context) !void {
        return self.symbol_table.scope_begin();
    }

    // TODO: Unused variables warnings
    pub inline fn scope_end(self: Context) !void {
        return self.symbol_table.scope_end();
    }
};
