const std = @import("std");
const DataStructure = @import("./DataStructure.zig");
const TypeMod = @import("./Type.zig");
const TokenMod = @import("./Token.zig");

const mem = std.mem;

const Stack = DataStructure.Stack;
const Token = TokenMod.Token;

pub const Symbol = struct {
    tp: TypeMod.SymbolType,
    is_function: bool = false,
    assigned: bool = false,
    defined: bool = false,
    mut: bool = false,
    uses: u32 = 0,

    pub fn init(tp: TypeMod.SymbolType) !Symbol {
        return Symbol{ .tp = tp };
    }

    pub fn print(self: *const Symbol) void {
        std.log.debug("Symbol(defined: {} mut:{} uses:{} tp:{s})", .{ self.defined, self.mut, self.uses, self.tp.stringfy() });
    }

    pub fn is_used(self: *const Symbol) bool {
        return self.uses != 1;
    }
};

pub const SymbolTable = struct {
    const Scope = std.AutoHashMap(TokenMod.Token, Symbol);

    allocator: mem.Allocator,
    scopes: Stack(Scope),

    pub fn init(allocator: mem.Allocator) SymbolTable {
        return SymbolTable{ .allocator = allocator, .scopes = Stack(Scope).init(allocator) };
    }

    pub fn deinit(self: *const SymbolTable) void {
        self.scopes.deinit();
    }

    pub fn declare(self: *SymbolTable, ident: *const Token) !void {
        const current = try self.current_scope();
        const tp = try TypeMod.SymbolType.init(self.allocator);
        current.put(ident.*, try Symbol.init(tp));
    }

    // TODO: mutability bound to the type
    pub fn define(self: *SymbolTable, ident: *const Token, tp: TypeMod.Type, mut: bool) !void {
        const current = try self.current_scope();
        const the_symbol = current.getPtr(ident) orelse return error.UndeclaredSymbol;
        the_symbol.defined = true;
        the_symbol.tp.tp.* = tp;
        the_symbol.mut = mut;
    }

    pub fn look_up(self: *const SymbolTable, key: *const TokenMod.Token) ?*Symbol {
        for (self.scopes.len() - 1..0) |i| {
            const current = try self.scopes.at(i);
            return current.getPtr(key.*) orelse continue;
        }
        return null;
    }

    pub fn scope_begin(self: *SymbolTable) !void {
        try self.scopes.push(Scope.init(self.allocator));
    }

    pub fn scope_end(self: *SymbolTable) !void {
        try self.scopes.weak_pop();
    }

    pub fn current_scope(self: *const SymbolTable) !*Scope {
        return self.scopes.peek();
    }
};
