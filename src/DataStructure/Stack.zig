const std = @import("std");

const mem = std.mem;

pub const StackError = error{ OutOfMemory, StackUnderFlow, IndexOutOfBound };

pub fn Stack(comptime T: type) type {
    return struct {
        allocator: mem.Allocator,
        items: []T,
        count: usize = 0,
        i: usize = 0,
        cap: usize = 5,

        pub fn init(allocator: mem.Allocator) @This() {
            return @This(){ .allocator = allocator, .items = allocator.alloc(T, 5) };
        }

        pub fn deinit(self: *const @This()) void {
            if (self.cap != 0) {
                self.allocator.free(self.items);
            }
        }

        pub fn at(self: *@This(), index: usize) StackError!*T {
            if (index >= self.len()) {
                return StackError.IndexOutOfBound;
            }

            return &self.items[index];
        }
        pub fn weak_push(self: *@This(), item: T) StackError!void {
            if (self.count >= self.cap) {
                self.cap *= 2;
                self.items = self.allocator.realloc(self.items, self.cap) catch return StackError.OutOfMemory;
            }
            self.items[self.count] = item;
            self.count += 1;
        }

        pub fn weak_pop(self: *@This()) StackError!T {
            if (self.count == 0) {
                return StackError.StackUnderFlow;
            }
            self.count -= 1;
            return self.items[self.count];
        }

        pub fn push(self: *@This(), item: T) StackError!void {
            if (self.count >= self.cap) {
                self.cap *= 2;
                self.items = self.allocator.realloc(self.items, self.cap) catch return StackError.OutOfMemory;
            }
            self.items[self.count] = item;
            self.count += 1;
            self.i = self.count;
        }

        pub fn pop(self: *@This()) StackError!T {
            if (self.is_empty()) {
                return StackError.StackUnderFlow;
            }
            self.count -= 1;
            self.i = self.count;
            return self.items[self.count];
        }

        pub fn is_empty(self: *const @This()) bool {
            return self.count == 0;
        }

        pub fn len(self: *const @This()) usize {
            return self.i;
        }

        pub fn peek(self: *const @This()) StackError!*T {
            if (self.i == 0) {
                return StackError.StackUnderFlow;
            }
            return &self.items[self.i - 1];
        }

        pub fn peek_at(self: *const @This(), ahead: usize) StackError!*T {
            const index = self.i -| ahead;
            if (index == 0) {
                return StackError.StackUnderFlow;
            }
            return &self.items[index];
        }
    };
}
