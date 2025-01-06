const std = @import("std");
const TokenMod = @import("../Token.zig");
const TypeMod = @import("./Type.zig");
const ExprMod = @import("./Expr.zig");

const Token = TokenMod.Token;
const mem = std.mem;
const debug = std.debug;

const ident_size: comptime_int = 2;

pub const ArgNode = struct {
    start: *const Token,
    end: *const Token,
    ident: *const Token,
    tp: *const TypeMod.Type,
    default: ?*const ExprMod.Expr,

    pub fn print(self: *const @This(), depth: u32) void {
        StmtNode.print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        StmtNode.print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        StmtNode.print_identation(depth);
        debug.print("ident: ", .{});
        StmtNode.print_token(self.ident);

        StmtNode.print_identation(depth);
        debug.print("tp:\n", .{});
        self.tp.print(depth + ident_size);

        StmtNode.print_identation(depth);
        debug.print("default:\n", .{});
        if (self.default) |default| {
            default.print(depth + ident_size);
        } else {
            StmtNode.print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }
    }
};

pub const Visibility = enum { //
    Public,
    Private,
    Inherited,

    pub fn stringfy(self: @This()) []const u8 {
        return switch (self) {
            .Public => "public",
            .Private => "private",
            .Inherited => "inherited",
        };
    }
};

pub const Stmt = struct {
    visibility: Visibility = .Private,
    start: *const Token,
    end: *const Token,
    stmt: *const StmtNode,

    pub fn print(self: *const @This(), depth: u32) void {
        StmtNode.print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        StmtNode.print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        StmtNode.print_identation(depth);
        debug.print("visibility: {s}\n", .{self.visibility.stringfy()});

        self.stmt.print(depth);
    }
};

pub const ItemFunc = struct {
    identifier: *const Token,
    arguments: []const ArgNode,
    return_type: ?*const TypeMod.Type = null,
    body: Stmt,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("ItemFunc:\n", .{});
        StmtNode.print_identation(depth);
        debug.print("identifier: ", .{});
        StmtNode.print_token(self.identifier);

        StmtNode.print_identation(depth);
        debug.print("params:\n", .{});
        for (self.arguments, 0..) |argument, i| {
            StmtNode.print_identation(depth + ident_size);
            debug.print("{}:\n", .{i});
            argument.print(depth + (ident_size * 2));
        }

        StmtNode.print_identation(depth);
        debug.print("return_type:\n", .{});
        if (self.return_type) |return_type| {
            return_type.print(depth + ident_size);
        } else {
            StmtNode.print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }

        StmtNode.print_identation(depth);
        debug.print("body:\n", .{});
        self.body.print(depth + ident_size);
        // for (self.body, 0..) |stmt, i| {
        //     print_identation(depth + ident_size);
        //     debug.print("{}:\n", .{i});
        //     stmt.print(depth + (ident_size * 2));
        // }
    }
};

const Let = struct {
    mut: ?*const Token,
    identifier: *const Token,
    tp: ?*const TypeMod.Type = null,
    eq: ?*const Token = null,
    init: ?*const ExprMod.Expr = null,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Let:\n", .{});

        StmtNode.print_identation(depth);
        debug.print("mut: ", .{});
        if (self.mut) |mut| {
            StmtNode.print_token(mut);
        } else {
            debug.print("NONE\n", .{});
        }

        StmtNode.print_identation(depth);
        debug.print("identifier: ", .{});
        StmtNode.print_token(self.identifier);

        StmtNode.print_identation(depth);
        debug.print("eq: ", .{});
        if (self.eq) |eq| {
            eq.print();
        } else {
            debug.print("NONE\n", .{});
        }

        StmtNode.print_identation(depth);
        debug.print("tp:\n", .{});
        if (self.tp) |tp| {
            tp.print(depth + ident_size);
        } else {
            StmtNode.print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }

        StmtNode.print_identation(depth);
        debug.print("init:\n", .{});
        if (self.init) |init| {
            init.print(depth + ident_size);
        } else {
            StmtNode.print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }
    }
};

const Block = struct {
    stmts: []const Stmt,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Block:\n", .{});
        if (self.stmts.len == 0) {
            StmtNode.print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
            return;
        }
        for (self.stmts, 0..) |stmt, i| {
            StmtNode.print_identation(depth + ident_size);
            debug.print("{}:\n", .{i});
            stmt.print(depth + (ident_size * 2));
        }
    }
};

const Return = struct {
    expr: ?*const ExprMod.Expr,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Return: ", .{});
        if (self.expr) |expr| {
            debug.print("\n", .{});
            expr.print(depth + ident_size);
        } else {
            debug.print("NONE\n", .{});
        }
    }
};

pub const StmtNode = union(enum) {
    ItemFunc: ItemFunc,
    Let: Let,
    Block: Block,
    Return: Return,
    Expr: *const ExprMod.Expr,
    None,

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

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        switch (self.*) {
            .ItemFunc => |v| v.print(depth + ident_size),
            .Let => |v| v.print(depth + ident_size),
            .Block => |v| v.print(depth + ident_size),
            .Return => |v| v.print(depth + ident_size),
            .None => |_| debug.print("None.\n", .{}),
            .Expr => |v| {
                debug.print("Expr:\n", .{});
                v.print(depth + ident_size);
            },
        }
    }
};

pub fn init_stmt(start: *const Token, end: *const Token, visibility: Visibility, stmt: *const StmtNode) Stmt {
    return Stmt{
        .visibility = visibility,
        .start = start,
        .end = end,
        .stmt = stmt,
    };
}

pub fn create_stmt(alloc: mem.Allocator, start: *const Token, end: *const Token, visibility: Visibility, stmt: *const StmtNode) !*Stmt {
    const result = try alloc.create(Stmt);

    result.visibility = visibility;
    result.start = start;
    result.end = end;
    result.stmt = stmt;

    return result;
}

pub fn create_func(alloc: mem.Allocator) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .ItemFunc = undefined };

    return result;
}

pub fn create_let(alloc: mem.Allocator) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .Let = undefined };

    return result;
}

pub fn create_expr_stmt(alloc: mem.Allocator, expr: *const ExprMod.Expr) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .Expr = expr };

    return result;
}

pub fn create_return(alloc: mem.Allocator, expr: ?*const ExprMod.Expr) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .Return = .{ .expr = expr } };

    return result;
}

pub fn create_block(alloc: mem.Allocator, stmts: []const Stmt) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .Block = .{ .stmts = stmts } };

    return result;
}

pub fn create_none_stmt(alloc: mem.Allocator) !*StmtNode {
    const result = try alloc.create(StmtNode);

    result.* = .{ .None = {} };

    return result;
}
