const std = @import("std");

const mem = std.mem;

pub const SymbolType = struct {
    mut: bool = false,
    tp: *Type,

    pub fn init(allocator: mem.Allocator) !SymbolType {
        const tp = try allocator.create(Type);

        tp.* = UnsetType.init();
        return SymbolType{ .tp = tp };
    }

    pub fn stringfy(self: *const SymbolType) []const u8 {
        return self.tp.stringfy();
    }
    pub fn make(allocator: mem.Allocator) !SymbolType {
        const res = try allocator.create(SymbolType);
        res.* = SymbolType.init(allocator);
        return res;
    }
};

pub const Type = union(enum) {
    TypeError: TypeError,
    UnsetType: UnsetType,
    NativeType: NativeType,
    CallableType: CallableType,
    // InterfaceType: InterfaceType,
    // StructType: StructType,
    // EnumType: EnumType,
    // ArrayType: ArrayType,

    fn stringfy(self: Type) []const u8 {
        return switch (self) {
            .TypeError => self.TypeError.stringfy(),
            .UnsetType => self.UnsetType.stringfy(),
            .NativeType => self.NativeType.stringfy(),
            .CallableType => self.CallableType.stringfy(),
            // .InterfaceType => self.InterfaceType.stringfy(),
            // .StructType => self.StructType.stringfy(),
            // .EnumType => self.EnumType.stringfy(),
            // .ArrayType => self.ArrayType.stringfy(),
        };
    }

    fn getSize(self: Type) usize {
        return switch (self) {
            .TypeError => self.TypeError.getSize(),
            .UnsetType => self.UnsetType.getSize(),
            .NativeType => self.NativeType.getSize(),
            .CallableType => self.CallableType.getSize(),
            // .InterfaceType => self.InterfaceType.getSize(),
            // .StructType => self.StructType.getSize(),
            // .EnumType => self.EnumType.getSize(),
            // .ArrayType => self.ArrayType.getSize(),
        };
    }
};

pub const TypeError = struct {
    msg: []const u8,

    fn init(msg: []const u8) Type {
        return Type{ .TypeError = TypeError{ .msg = msg } };
    }

    fn stringfy(self: *const TypeError) []const u8 {
        return self.msg;
    }

    fn getSize(self: *const TypeError) usize {
        _ = self;
        return 0;
    }
};

pub const UnsetType = struct {
    fn init() Type {
        return Type{ .UnsetType = UnsetType{} };
    }

    fn stringfy(self: UnsetType) []const u8 {
        _ = self;
        return "unset";
    }

    fn getSize(self: UnsetType) usize {
        _ = self;
        return 0;
    }
};

pub const NativeType = struct {
    const Kind = enum {
        Unknown,
        Undefined,
        Void,
        Number,
        Int,
        UInt,
        Float,
        Bool,
        String,
        Char,
    };

    kind: Kind,

    fn init(kind: Kind) Type {
        return Type{ .NativeType = NativeType{ .kind = kind } };
    }

    fn stringfy(self: *const NativeType) []const u8 {
        return switch (self.kind) {
            .Unknown => "unknown",
            .Undefined => "undefined",
            .Void => "void",
            .Number => "number",
            .Int => "int",
            .UInt => "uint",
            .Float => "float",
            .Bool => "bool",
            .String => "string",
            .Char => "char",
        };
    }

    fn getSize(self: *const NativeType) usize {
        return switch (self.kind) {
            .Int, .UInt => 4,
            .Float => 4,
            .Bool, .Char => 1,
            else => 0,
        };
    }
};

pub const CallableType = struct {
    pub const Arg = struct {
        name: []const u8,
        type: Type,
        hasDefault: bool,
        isMut: bool,
    };

    arguments: std.ArrayList(Arg),
    returnType: Type,

    pub fn init(allocator: *std.mem.Allocator, returnType: Type) !Type {
        return Type{
            .CallableType = CallableType{
                .arguments = try std.ArrayList(Arg).init(allocator),
                .returnType = returnType,
            },
        };
    }

    pub fn stringfy(self: *const CallableType) []const u8 {
        _ = self;
        return "callable";
    }

    pub fn getSize(self: *const CallableType) usize {
        _ = self;
        return 0;
    }
};

// Define InterfaceType, StructType, EnumType, ArrayType similarly with relevant fields and methods.
