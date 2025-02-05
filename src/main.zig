const std = @import("std");
const Compiler = @import("./Compiler.zig").Compiler;
const ReporterMod = @import("./Reporter.zig");

const unicode = std.unicode;

pub fn main() !void {
    const stdout = std.io.getStdOut();

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    var arena = std.heap.ArenaAllocator.init(allocator);
    const alloc = arena.allocator();
    const cwd = try std.fs.path.join(allocator, &[_][]const u8{ try std.fs.realpathAlloc(alloc, "."), "main.haste" });
    var compiler = Compiler.init(alloc, stdout);
    defer {
        allocator.free(cwd);
        compiler.deinit();
        arena.deinit();
        const deinit_status = gpa.deinit();
        if (deinit_status == .leak)
            @panic("Leak detected.");
    }

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
