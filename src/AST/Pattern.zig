const std = @import("std");
const DataStructure = @import("../DataStructure.zig");
const TokenMod = @import("../Token.zig");
const ExprMod = @import("./Expr.zig");

const LinkedList = DataStructure.LinkedList;
const Token = TokenMod.Token;
const mem = std.mem;
const debug = std.debug;

const ident_size: comptime_int = 2;

fn get_identation(depth: u32) []const u8 {
    var ident: [100]u8 = undefined;
    @memset(&ident, ' ');
    return ident[0..depth];
}

fn print_identation(depth: u32) void {
    debug.print("{s}", .{get_identation(depth)});
}

fn print_token(token: *const Token) void {
    std.debug.print("`{s}`\n", .{token.lexem.data});
}

pub const Pattern = struct {
    start: *const Token,
    end: *const Token,
    pattern: *PatternNode,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        self.pattern.print(depth);
    }
};

pub fn init_pattern(start: *const Token, end: *const Token, pattern: *PatternNode) Pattern {
    return Pattern{ .start = start, .end = end, .pattern = pattern };
}

pub const PatternNode = union(enum) {
    Literal: *const Token,
    Variable: *const Token,
    Range: RangePattern,
    Or: OrPattern,
    Tuple: TuplePattern,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        switch (self.*) {
            .Literal => |v| {
                debug.print("Literal: ", .{});
                print_token(v);
            },
            .Variable => |v| {
                debug.print("Variable: ", .{});
                print_token(v);
            },
            .Range => |v| v.print(depth + ident_size),
            .Or => |v| v.print(depth + ident_size),
            .Tuple => |v| v.print(depth + ident_size),
        }
    }
};

pub const RangePattern = struct {
    from: *const ExprMod.Expr,
    to: *const ExprMod.Expr,
    equal: bool = false,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("RangePattern:\n", .{});

        print_identation(depth);
        debug.print("Equal: {}\n", .{self.equalfrom});

        print_identation(depth);
        debug.print("From:\n", .{});
        self.from.print(depth + ident_size);

        print_identation(depth);
        debug.print("To:\n", .{});
        self.to.print(depth + ident_size);
    }
};

pub const OrPattern = struct {
    branches: LinkedList(Pattern),

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("OrPattern:\n", .{});

        var branches_iter = self.branches.iter();
        var i: i32 = 0;
        while (branches_iter.next()) |branch| : (i += 1) {
            print_identation(depth + ident_size);
            debug.print("{}:\n", .{i});
            branch.value.print(depth + ident_size);
        }
    }
};

pub const TuplePattern = struct {
    patterns: LinkedList(Pattern),

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("TuplePattern:\n", .{});

        var patterns_iter = self.patterns.iter();
        var i: i32 = 0;
        while (patterns_iter.next()) |pattern| : (i += 1) {
            print_identation(depth);
            debug.print("{}:\n", .{i});
            pattern.value.print(depth + ident_size);
        }
    }
};
