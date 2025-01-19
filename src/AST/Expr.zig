const std = @import("std");
const DataStructure = @import("../DataStructure.zig");
const TokenMod = @import("../Token.zig");
const Context = @import("../Context.zig");

const LinkedList = DataStructure.LinkedList;
const Token = TokenMod.Token;
const mem = std.mem;
const debug = std.debug;

const ident_size: comptime_int = 2;

pub const Expr = struct { //
    start: *const Token,
    end: *const Token,
    expr: *ExprNode,

    pub fn print(self: *const @This(), depth: u32) void {
        ExprNode.print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        ExprNode.print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        self.expr.print(depth);
    }
};

pub const ExprNode = union(enum) { //
    Grouping: *Expr,
    Primary: *const Token,

    Unary: struct { //
        op: *const Token,
        right: *Expr,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("Unary:\n", .{});
            print_identation(depth);
            debug.print("op: ", .{});
            print_token(self.op);

            print_identation(depth);
            debug.print("right:\n", .{});
            self.right.print(depth + ident_size);
        }
    },
    Binary: struct { //
        left: *Expr,
        op: *const Token,
        right: *Expr,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("Binary:\n", .{});
            print_identation(depth);
            debug.print("op: ", .{});
            print_token(self.op);

            print_identation(depth);
            debug.print("left:\n", .{});
            self.left.print(depth + ident_size);

            print_identation(depth);
            debug.print("right:\n", .{});
            self.right.print(depth + ident_size);
        }
    },
    LogicalBinary: struct { //
        left: *Expr,
        op: *const Token,
        right: *Expr,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("LogicalBinary:\n", .{});
            print_identation(depth);
            debug.print("op: ", .{});
            print_token(self.op);

            print_identation(depth);
            debug.print("left:\n", .{});
            self.left.print(depth + ident_size);

            print_identation(depth);
            debug.print("right:\n", .{});
            self.right.print(depth + ident_size);
        }
    },
    LogicalUnary: struct { //
        op: *const Token,
        right: *Expr,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("LogicalUnary:\n", .{});
            print_identation(depth);
            debug.print("op: ", .{});
            print_token(self.op);

            print_identation(depth);
            debug.print("right:\n", .{});
            self.right.print(depth + ident_size);
        }
    },
    Assignment: struct { //
        left: *Expr,
        op: *const Token,
        right: *Expr,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("Assignment:\n", .{});
            print_identation(depth);
            debug.print("op: ", .{});
            print_token(self.op);

            print_identation(depth);
            debug.print("left:\n", .{});
            self.left.print(depth + ident_size);

            print_identation(depth);
            debug.print("right:\n", .{});
            self.right.print(depth + ident_size);
        }
    },

    InlineIf: struct {
        condition: *Expr,
        consequent: *Expr,
        alternate: *Expr,

        if_token: *const Token,
        then_token: *const Token,
        else_token: *const Token,

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("InlineIf:\n", .{});

            print_identation(depth);
            debug.print("if_token: ", .{});
            self.if_token.print();

            print_identation(depth);
            debug.print("then_token: ", .{});
            self.then_token.print();

            print_identation(depth);
            debug.print("else_token: ", .{});
            self.else_token.print();

            print_identation(depth);
            debug.print("condition:\n", .{});
            self.condition.print(depth + ident_size);

            print_identation(depth);
            debug.print("consequent:\n", .{});
            self.consequent.print(depth + ident_size);

            print_identation(depth);
            debug.print("alternate:\n", .{});
            self.alternate.print(depth + ident_size);
        }
    },

    Call: struct {
        callee: *Expr,
        paren: *const Token,
        args: LinkedList(*Expr),

        pub fn print(self: *const @This(), depth: u32) void {
            debug.print("Call:\n", .{});

            print_identation(depth);
            debug.print("paren: ", .{});
            self.paren.print();

            print_identation(depth);
            debug.print("callee:\n", .{});
            self.callee.print(depth + ident_size);

            print_identation(depth);
            debug.print("args:\n", .{});
            if (self.args.len == 0) {
                print_identation(depth + ident_size);
                debug.print("NONE\n", .{});
                return;
            }
            var iter = self.args.iter();
            var i: i32 = 0;
            while (iter.next()) |arg| : (i += 1) {
                print_identation(depth + ident_size);
                debug.print("{}:\n", .{i});
                arg.value.print(depth + (ident_size * 2));
            }
        }
    },

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
            .Unary => |v| v.print(depth + ident_size),
            .Binary => |v| v.print(depth + ident_size),
            .LogicalBinary => |v| v.print(depth + ident_size),
            .LogicalUnary => |v| v.print(depth + ident_size),
            .Assignment => |v| v.print(depth + ident_size),
            .InlineIf => |v| v.print(depth + ident_size),
            .Call => |v| v.print(depth + ident_size),
            .Grouping => |v| {
                debug.print("Grouping:\n", .{});
                v.print(depth + ident_size);
            },
            .Primary => |v| {
                debug.print("Primary: ", .{});
                print_token(v);
            },
        }
    }
};

pub fn create_expr(allocator: mem.Allocator, start: *const Token, end: *const Token, expr: *ExprNode) !*Expr {
    const result = try allocator.create(Expr);

    result.start = start;
    result.end = end;
    result.expr = expr;

    return result;
}

pub fn create_grouping(allocator: mem.Allocator, expr: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .Grouping = expr };

    return result;
}

pub fn create_unary(allocator: mem.Allocator, op: *const Token, right: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);
    result.* = .{ .Unary = .{ .op = op, .right = right } };

    return result;
}

pub fn create_binary(allocator: mem.Allocator, left: *Expr, op: *const Token, right: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .Binary = .{ .left = left, .op = op, .right = right } };

    return result;
}

pub fn create_logical_binary(allocator: mem.Allocator, left: *Expr, op: *const Token, right: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .LogicalBinary = .{ .left = left, .op = op, .right = right } };

    return result;
}

pub fn create_logical_unary(allocator: mem.Allocator, op: *const Token, right: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .LogicalUnary = .{ .op = op, .right = right } };

    return result;
}

pub fn create_assignment(allocator: mem.Allocator, left: *Expr, op: *const Token, right: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .Assignment = .{ .left = left, .op = op, .right = right } };

    return result;
}

pub fn create_primary(allocator: mem.Allocator, value: *const Token) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .Primary = value };

    return result;
}

pub fn create_inline_if(allocator: mem.Allocator, condition: *Expr, consequent: *Expr, alternate: *Expr) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .InlineIf = .{ .condition = condition, .consequent = consequent, .alternate = alternate, .if_token = undefined, .then_token = undefined, .else_token = undefined } };

    return result;
}

pub fn create_call(allocator: mem.Allocator, callee: *Expr, paren: *const Token, args: LinkedList(*Expr)) !*ExprNode {
    const result = try allocator.create(ExprNode);

    result.* = .{ .Call = .{ .callee = callee, .paren = paren, .args = args } };

    return result;
}
