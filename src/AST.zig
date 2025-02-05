const std = @import("std");

const ExprMod = @import("./AST/Expr.zig");
const StmtMod = @import("./AST/Stmt.zig");
const TypeMod = @import("./AST/Type.zig");
const PatternMod = @import("./AST/Pattern.zig");
const DataStructure = @import("./DataStructure.zig");

const mem = std.mem;
const LinkedList = DataStructure.LinkedList;

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
pub const create_tuple_expr = ExprMod.create_tuple_expr;
pub const create_unit_expr = ExprMod.create_unit_expr;

pub const UnaryExpr = ExprMod.UnaryExpr;
pub const BinaryExpr = ExprMod.BinaryExpr;
pub const LogicalBinaryExpr = ExprMod.LogicalBinaryExpr;
pub const LogicalUnaryExpr = ExprMod.LogicalUnaryExpr;
pub const AssignmentExpr = ExprMod.AssignmentExpr;
pub const InlineIfExpr = ExprMod.InlineIfExpr;
pub const CallExpr = ExprMod.CallExpr;
pub const TupleExpr = ExprMod.TupleExpr;

pub const ArgNode = StmtMod.ArgNode;

pub const Stmt = StmtMod.Stmt;
pub const StmtNode = StmtMod.StmtNode;
pub const init_stmt = StmtMod.init_stmt;
pub const create_stmt = StmtMod.create_stmt;
pub const create_func = StmtMod.create_func;
pub const create_let = StmtMod.create_let;
pub const create_expr_stmt = StmtMod.create_expr_stmt;
pub const create_do_stmt = StmtMod.create_do_stmt;
pub const create_ret_stmt = StmtMod.create_ret_stmt;
pub const create_return = StmtMod.create_return;
pub const create_block = StmtMod.create_block;
pub const create_none_stmt = StmtMod.create_none_stmt;
pub const create_if_stmt = StmtMod.create_if_stmt;

pub const FuncStmt = StmtMod.FuncStmt;
pub const LetStmt = StmtMod.LetStmt;
pub const BlockStmt = StmtMod.BlockStmt;
pub const DoStmt = StmtMod.DoStmt;
pub const RetStmt = StmtMod.RetStmt;
pub const ReturnStmt = StmtMod.ReturnStmt;
pub const IfStmt = StmtMod.IfStmt;

pub const Type = TypeMod.Type;
pub const PrimitiveType = TypeMod.PrimitiveType;
pub const TupleType = TypeMod.TupleType;
pub const ArrayType = TypeMod.ArrayType;
pub const SliceType = TypeMod.SliceType;
pub const TypeNode = TypeMod.TypeNode;
pub const create_type = TypeMod.create_type;
pub const create_primitive = TypeMod.create_primitive;
pub const create_tuple_type = TypeMod.create_tuple_type;
pub const create_auto_type = TypeMod.create_auto_type;
pub const create_unit_type = TypeMod.create_unit_type;
pub const create_id_type = TypeMod.create_id_type;
pub const create_array_type = TypeMod.create_array_type;
pub const create_slice_type = TypeMod.create_slice_type;

pub const Pattern = PatternMod.Pattern;
pub const PatternNode = PatternMod.PatternNode;
pub const RangePattern = PatternMod.RangePattern;
pub const OrPattern = PatternMod.OrPattern;
pub const TuplePattern = PatternMod.TuplePattern;
pub const init_pattern = PatternMod.init_pattern;

pub const ItemList = LinkedList(*Stmt);

pub const ProgramAST = struct { //
    items: ItemList,

    pub fn init(allocator: mem.Allocator) ProgramAST {
        return ProgramAST{ .items = ItemList.init(allocator) };
    }

    pub fn add_item(self: *@This(), item: *Stmt) !void {
        try self.items.add(item);
    }

    pub fn print(self: *const @This()) void {
        var items_iter = self.items.iter();
        while (items_iter.next()) |value| {
            value.value.print(0);
        }
    }

    pub fn deinit(self: *const @This()) void {
        self.items.deinit();
    }
};
