const std = @import("std");
const Compiler = @import("./Compiler.zig").Compiler;
const ReporterMod = @import("./Reporter.zig");

const unicode = std.unicode;

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();

    const alloc = arena.allocator();

    const stdout = std.io.getStdOut();

    var compiler = Compiler.init(allocator, alloc, stdout);
    defer compiler.deinit();

    const cwd = try std.fs.path.join(allocator, &[_][]const u8{ try std.fs.realpathAlloc(alloc, "."), "main.haste" });
    // std.log.info("{s}", .{cwd});
    compiler.compile(cwd) catch {
        compiler.deinit();
        std.process.exit(2);
    };
}

test {
    const stdout = std.io.getStdOut();
    _ = stdout;
}
