const std = @import("std");
const ReporterMod = @import("./Reporter.zig");
const ArrayList = std.ArrayList;

const StringView = @import("./DataStructure.zig").StringView;

const Report = ReporterMod.Report;

pub const Type = enum(i8) {
    EOF = -1,

    SemiColon, // ';'
    Colon, // ':'
    Coma, // ','

    // Operators (I guess??)
    OpenParen, // '('
    CloseParen, // ')'
    OpenSquareParen, // '['
    CloseSquareParen, // ']'
    OpenCurlyParen, // '{'
    CloseCurlyParen, // '}'

    // Arithmetic operators
    Minus, // '-'
    Plus, // '+'
    Star, // '*'
    DoubleStar, // '**'
    FSlash, // '/'
    BSlash, // '\'
    PercentSign, // '%'

    // Comparation
    EqEq, // '==',
    NotEq, // '!='
    LessThan, // '<'
    LessThanEq, // '<='
    GreaterThan, // '>'
    GreaterThanEq, // '>='

    Eq, // '='
    Dot, // '.'
    DoubleDot, // '..'

    // Literals
    StringLit, // "[any]"
    FloatLit, // [0-9]*.[0-9]*
    IntLit, // [0-9]*

    // Keywords
    Ident, // ([a-zA-Z]|_|$)([a-zA-Z]|$|_|[0-9])*
    SpecialIdent, // $"[any]*"
    MacroUse, // @[ident]

    Uint,
    Int,
    Float,
    Bool,
    Void,
    Auto,

    True, // 'true'
    False, // 'false'

    // Logical Operator
    Not, // 'not'
    And, // 'and'
    Or, // 'or'

    // Declarations and deffinitions
    Public,
    Private,
    NotFunc, // '!func'
    Func, // 'func'
    Let, // 'let'
    Mut, // 'mut'

    // Statements
    If, // 'if'
    NotIf, // '!if'
    Then, // 'then'
    Else, // 'else'

    Do,
    Return,
    Ret,
};

pub const Token = struct { //
    kind: Type,
    lexem: StringView,
    line: usize,
    column: usize,
    start: usize,
    end: usize,

    pub fn create(kind: Type, lexem: StringView, line: usize, column: usize, start: usize, end: usize) Token {
        return Token{ .kind = kind, .lexem = lexem, .line = line, .column = column, .start = start, .end = end };
    }

    pub fn print(self: *const @This()) void {
        std.debug.print("Token(\"{s}\", {}, line: {}, column: {})\n", .{ self.lexem.data, self.kind, self.line, self.column });
    }

    pub inline fn get_index(self: *const @This()) usize {
        return self.start;
    }

    pub inline fn get_len(self: *const @This()) usize {
        return self.end - self.start;
    }

    pub fn make_report(self: *const @This(), kind: ReporterMod.ReportType, msg: []const u8) Report {
        var res = Report.init(self.line, self.column, self.get_index(), msg);
        res.kind = kind;
        res.len = self.get_len();
        return res;
    }
};

pub const TokenList = ArrayList(Token);
