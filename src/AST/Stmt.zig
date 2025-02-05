const std = @import("std");
const TokenMod = @import("../Token.zig");
const TypeMod = @import("./Type.zig");
const ExprMod = @import("./Expr.zig");
const DataStructure = @import("../DataStructure.zig");

const Token = TokenMod.Token;
const mem = std.mem;
const debug = std.debug;

const LinkedList = DataStructure.LinkedList;

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

pub const ArgNode = struct {
    start: *const Token,
    end: *const Token,
    ident: *const Token,
    tp: *const TypeMod.Type,
    default: ?*const ExprMod.Expr,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        print_identation(depth);
        debug.print("ident: ", .{});
        print_token(self.ident);

        print_identation(depth);
        debug.print("tp:\n", .{});
        self.tp.print(depth + ident_size);

        print_identation(depth);
        debug.print("default:\n", .{});
        if (self.default) |default| {
            default.print(depth + ident_size);
        } else {
            print_identation(depth + ident_size);
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
        print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        print_identation(depth);
        debug.print("visibility: {s}\n", .{self.visibility.stringfy()});

        self.stmt.print(depth);
    }

    pub fn clone(self: @This(), alloc: mem.Allocator) *Stmt {
        const res = alloc.create(Stmt) catch @panic("Out of memory");

        res.* = self;

        return res;
    }
};

pub const FuncStmt = struct {
    identifier: *const Token,
    arguments: []const ArgNode,
    return_type: ?*const TypeMod.Type = null,
    body: *Stmt,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("ItemFunc:\n", .{});
        print_identation(depth);
        debug.print("identifier: ", .{});
        print_token(self.identifier);

        print_identation(depth);
        debug.print("params:\n", .{});
        for (self.arguments, 0..) |argument, i| {
            print_identation(depth + ident_size);
            debug.print("{}:\n", .{i});
            argument.print(depth + (ident_size * 2));
        }

        print_identation(depth);
        debug.print("return_type:\n", .{});
        if (self.return_type) |return_type| {
            return_type.print(depth + ident_size);
        } else {
            print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }

        print_identation(depth);
        debug.print("body:\n", .{});
        self.body.print(depth + ident_size);
        // for (self.body, 0..) |stmt, i| {
        //     print_identation(depth + ident_size);
        //     debug.print("{}:\n", .{i});
        //     stmt.print(depth + (ident_size * 2));
        // }
    }
};

pub const LetStmt = struct {
    mut: ?*const Token,
    identifier: *const Token,
    tp: ?*const TypeMod.Type = null,
    eq: ?*const Token = null,
    init: ?*const ExprMod.Expr = null,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Let:\n", .{});

        print_identation(depth);
        debug.print("mut: ", .{});
        if (self.mut) |mut| {
            print_token(mut);
        } else {
            debug.print("NONE\n", .{});
        }

        print_identation(depth);
        debug.print("identifier: ", .{});
        print_token(self.identifier);

        print_identation(depth);
        debug.print("eq: ", .{});
        if (self.eq) |eq| {
            eq.print();
        } else {
            debug.print("NONE\n", .{});
        }

        print_identation(depth);
        debug.print("tp:\n", .{});
        if (self.tp) |tp| {
            tp.print(depth + ident_size);
        } else {
            print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }

        print_identation(depth);
        debug.print("init:\n", .{});
        if (self.init) |init| {
            init.print(depth + ident_size);
        } else {
            print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
        }
    }
};

pub const IfStmt = struct {
    condition: *ExprMod.Expr,
    consequent: *Stmt,
    alternate: ?*Stmt,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("If:\n", .{});

        print_identation(depth);
        debug.print("condition:\n", .{});
        self.condition.print(depth + ident_size);

        print_identation(depth);
        debug.print("consequent:\n", .{});
        self.consequent.print(depth + ident_size);

        if (self.alternate) |alternate| {
            print_identation(depth);
            debug.print("alternate:\n", .{});
            alternate.print(depth + ident_size);
        }
    }
};

pub const BlockStmt = struct {
    stmts: LinkedList(*Stmt),

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Block:\n", .{});
        if (self.stmts.len == 0) {
            print_identation(depth + ident_size);
            debug.print("NONE\n", .{});
            return;
        }
        var stmts_iter = self.stmts.iter();
        var i: i32 = 1;
        while (stmts_iter.next()) |stmt| : (i += 1) {
            print_identation(depth + ident_size);
            debug.print("{}:\n", .{i});
            stmt.value.print(depth + (ident_size * 2));
        }
    }
};

pub const DoStmt = struct {
    stmt: *Stmt,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("DoStmt:\n", .{});

        self.stmt.print(depth);
    }
};

pub const RetStmt = struct {
    expr: ?*const ExprMod.Expr,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("Ret: ", .{});
        if (self.expr) |expr| {
            debug.print("\n", .{});
            expr.print(depth + ident_size);
        } else {
            debug.print("NONE\n", .{});
        }
    }
};

pub const ReturnStmt = struct {
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
    Func: FuncStmt,
    Let: LetStmt,
    If: IfStmt,
    Block: BlockStmt,
    Do: DoStmt,
    Ret: RetStmt,
    Return: ReturnStmt,
    Expr: *const ExprMod.Expr,
    None,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        switch (self.*) {
            .Func => |v| v.print(depth + ident_size),
            .Let => |v| v.print(depth + ident_size),
            .If => |v| v.print(depth + ident_size),
            .Block => |v| v.print(depth + ident_size),
            .Do => |v| v.print(depth + ident_size),
            .Ret => |v| v.print(depth + ident_size),
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

pub fn create_stmt(alloc: mem.Allocator, start: *const Token, end: *const Token, visibility: Visibility, stmt: *const StmtNode) *Stmt {
    const result = alloc.create(Stmt) catch @panic("Out of memory");

    result.visibility = visibility;
    result.start = start;
    result.end = end;
    result.stmt = stmt;

    return result;
}

pub fn create_func(alloc: mem.Allocator) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Func = undefined };

    return result;
}

pub fn create_let(alloc: mem.Allocator) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Let = undefined };

    return result;
}

pub fn create_expr_stmt(alloc: mem.Allocator, expr: *const ExprMod.Expr) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Expr = expr };

    return result;
}

pub fn create_do_stmt(alloc: mem.Allocator, stmt: *Stmt) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Do = .{ .stmt = stmt } };

    return result;
}

pub fn create_ret_stmt(alloc: mem.Allocator, expr: ?*const ExprMod.Expr) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Ret = .{ .expr = expr } };

    return result;
}

pub fn create_return(alloc: mem.Allocator, expr: ?*const ExprMod.Expr) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Return = .{ .expr = expr } };

    return result;
}

pub fn create_block(alloc: mem.Allocator, stmts: LinkedList(*Stmt)) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .Block = .{ .stmts = stmts } };

    return result;
}

pub fn create_none_stmt(alloc: mem.Allocator) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .None = {} };

    return result;
}

pub fn create_if_stmt(alloc: mem.Allocator, condition: *ExprMod.Expr, consequent: *Stmt, alternate: ?*Stmt) *StmtNode {
    const result = alloc.create(StmtNode) catch @panic("Out of memory");

    result.* = .{ .If = .{ .condition = condition, .consequent = consequent, .alternate = alternate } };

    return result;
}
