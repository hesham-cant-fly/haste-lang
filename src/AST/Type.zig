const std = @import("std");
const TokenMod = @import("../Token.zig");
const DataStructure = @import("../DataStructure.zig");

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

pub const Type = struct {
    start: *const Token,
    end: *const Token,
    tp: *const TypeNode,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        debug.print("start: ", .{});
        self.start.print();

        print_identation(depth);
        debug.print("end: ", .{});
        self.end.print();

        self.tp.print(depth);
    }
};

pub const TypeNode = union(enum) {
    Primitive: PrimitiveType,
    Tuple: TupleType,
    Id: *const Token,
    Array: ArrayType,
    Slice: SliceType,
    Auto,
    Unit,

    pub fn print(self: *const @This(), depth: u32) void {
        print_identation(depth);
        switch (self.*) {
            .Primitive => |v| debug.print("Primitive: {s}\n", .{v.stringfy()}),
            .Tuple => |v| v.print(depth + ident_size),
            .Id => |v| {
                debug.print("Id: ", .{});
                print_token(v);
            },
            .Array => |v| v.print(depth + ident_size),
            .Slice => |v| v.print(depth + ident_size),
            .Auto => debug.print("Auto\n", .{}),
            .Unit => debug.print("Unit\n", .{}),
        }
    }
};

pub const PrimitiveType = enum {
    UInt,
    Int,
    Float,
    Bool,
    Void,

    pub fn stringfy(self: @This()) []const u8 {
        return switch (self) {
            .UInt => "UInt",
            .Int => "Int",
            .Float => "Float",
            .Bool => "Bool",
            .Void => "Void",
        };
    }
};

pub const TupleType = struct {
    types: LinkedList(*Type),

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("TupleType:\n", .{});

        var types_iter = self.types.iter();
        var i: i32 = 0;
        while (types_iter.next()) |tp| : (i += 1) {
            print_identation(depth);
            debug.print("{}:\n", .{i});
            tp.value.print(depth + ident_size);
        }
    }
};

pub const ArrayType = struct {
    items_type: *Type,
    size: *const Token,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("ArrayType:\n", .{});

        print_identation(depth);
        debug.print("size: ", .{});
        print_token(self.size);

        print_identation(depth);
        debug.print("items_type:\n", .{});
        self.items_type.print(depth + ident_size);
    }
};

pub const SliceType = struct {
    items_type: *Type,

    pub fn print(self: *const @This(), depth: u32) void {
        debug.print("SliceType:\n", .{});

        print_identation(depth);
        debug.print("items_type:\n", .{});
        self.items_type.print(depth + ident_size);
    }
};

pub fn create_type(alloc: mem.Allocator, start: *const Token, end: *const Token, tp: *const TypeNode) !*Type {
    const result = try alloc.create(Type);

    result.start = start;
    result.end = end;
    result.tp = tp;

    return result;
}

pub fn create_primitive(alloc: mem.Allocator, tp: PrimitiveType) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .{ .Primitive = tp };

    return result;
}

pub fn create_tuple_type(alloc: mem.Allocator, types: LinkedList(*Type)) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .{ .Tuple = .{ .types = types } };

    return result;
}

pub fn create_auto_type(alloc: mem.Allocator) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .Auto;

    return result;
}

pub fn create_unit_type(alloc: mem.Allocator) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .Unit;

    return result;
}

pub fn create_id_type(alloc: mem.Allocator, id: *const Token) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .{ .Id = id };

    return result;
}

pub fn create_array_type(alloc: mem.Allocator, items_type: *Type, size: *const Token) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .{ .Array = .{ .items_type = items_type, .size = size } };

    return result;
}

pub fn create_slice_type(alloc: mem.Allocator, items_type: *Type) !*TypeNode {
    const result = try alloc.create(TypeNode);

    result.* = .{ .Slice = .{ .items_type = items_type } };

    return result;
}
