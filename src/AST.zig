const std = @import("std");

const ExprMod = @import("./AST/Expr.zig");
const StmtMod = @import("./AST/Stmt.zig");
const TypeMod = @import("./AST/Type.zig");

const mem = std.mem;

pub const Expr = ExprMod.Expr;
pub const ExprNode = ExprMod.ExprNode;
pub const create_expr = ExprMod.create_expr;
pub const create_grouping = ExprMod.create_grouping;
pub const create_unary = ExprMod.create_unary;
pub const create_binary = ExprMod.create_binary;
pub const create_logical_binary = ExprMod.create_logical_binary;
pub const create_logical_unary = ExprMod.create_logical_unary;
pub const create_assignment = ExprMod.create_assignment;
pub const create_primary = ExprMod.create_primary;
pub const create_inline_if = ExprMod.create_inline_if;
pub const create_call = ExprMod.create_call;

pub const ArgNode = StmtMod.ArgNode;

pub const Stmt = StmtMod.Stmt;
pub const StmtNode = StmtMod.StmtNode;
pub const init_stmt = StmtMod.init_stmt;
pub const create_stmt = StmtMod.create_stmt;
pub const create_func = StmtMod.create_func;
pub const create_let = StmtMod.create_let;
pub const create_expr_stmt = StmtMod.create_expr_stmt;
pub const create_return = StmtMod.create_return;
pub const create_block = StmtMod.create_block;
pub const create_none_stmt = StmtMod.create_none_stmt;

pub const ItemFunc = StmtMod.ItemFunc;
pub const Let = StmtMod.Let;
pub const Block = StmtMod.Block;
pub const Return = StmtMod.Return;

pub const Type = TypeMod.Type;
pub const Primitive = TypeMod.Primitive;
pub const TypeNode = TypeMod.TypeNode;
pub const create_type = TypeMod.create_type;
pub const create_primitive = TypeMod.create_primitive;

pub const ItemList = std.ArrayList(Stmt);

pub const ProgramAST = struct { //
    items: std.ArrayList(Stmt),

    pub fn init(allocator: mem.Allocator) ProgramAST {
        return ProgramAST{ .items = ItemList.init(allocator) };
    }

    pub fn add_item(self: *@This(), item: Stmt) !void {
        try self.items.append(item);
    }

    pub fn print(self: *const @This()) void {
        for (self.items.items) |value| {
            value.print(0);
        }
    }

    pub fn deinit(self: *const @This()) void {
        self.items.deinit();
    }
};
