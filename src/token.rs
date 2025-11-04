use core::fmt;
use std::cell::RefCell;
use std::cmp;
use std::path::{Path, PathBuf};
use std::rc::Rc;

use colored::Colorize;
use logos::Logos;

use crate::file_source_pool::FSPool;
use crate::reporter::{Label, ReportKind};

#[repr(u8)]
#[derive(Logos, Clone, Debug, PartialEq)]
#[logos(skip r"[ \t\n\f]+")]
#[logos(skip r"\/\/.*\n")]
#[logos(subpattern alpha = r"[a-zA-Z]")]
#[logos(subpattern digit = r"[0-9]")]
#[logos(subpattern alphanum = r"(?&alpha)|(?&digit)")]
#[logos(subpattern symbol = r"(?&alpha)|\$|_")]
#[logos(subpattern symbolnum = r"(?&symbol)|(?&digit)")]
pub enum TokenKind {
    #[token("+")]                         Plus,
    #[token("-")]                         Minus,
    #[token("*")]                         Star,
    #[token("**")]                        DoubleStar,
    #[token("/")]                         FSlash,
    #[token("(")]                         OpenParen,
    #[token(")")]                         CloseParen,
    #[token(":")]                         Colon,
    #[token("=")]                         Equal,
    #[token(";")]                         Semicolon,

    #[token("var")]                       Var,
    #[token("const")]                     Const,
    #[token("type")]                      Type,
    #[token("auto")]                      Auto,
    #[token("int")]                       Int,
    #[token("float")]                     Float,

    #[regex("(?&digit)+")]                IntLit,
    #[regex(r"(?&digit)+\.(?&digit)+")]   FloatLit,
    #[regex(r"(?&symbol)(?&symbolnum)*")] Identifier,
}

#[derive(Debug)]
pub enum Error {
    CannotReadFile,
    InvalidToken,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Token {
    pub span: Span,
    pub kind: TokenKind,
    pub lexem: String,
}

impl Token {
    pub fn new(kind: TokenKind, span: Span, lexem: String) -> Token {
        Token { kind, span, lexem }
    }

    pub fn scan_source(fs_pool: Rc<RefCell<FSPool>>, path: &str) -> Result<Vec<Token>, Error> {
        let src = fs_pool.borrow_mut().load_file(path).map_err(|_| Error::CannotReadFile)?;
        let mut lxr = TokenKind::lexer(&src);
        let mut tokens = Vec::<Token>::new();
        let mut has_error = false;

        while let Some(token) = lxr.next() {
            let span = Span::from(lxr.span());
            let lexem = lxr.slice();
            let Ok(kind) = token else {
                has_error = true;
                fs_pool.borrow().build_report(path, ReportKind::Error, span.clone())
                    .unwrap()
                    .with_message(format!("Invalid character: '{}'", lexem))
                    .with_label(
                        Label::new(span)
                            .with_caret("^")
                            .with_msg("Remove this.")
                    )
                    .print();
                continue;
            };
            tokens.push(Token::new(kind, lxr.span().into(), lexem.into()));
        }

        if has_error {
            Err(Error::InvalidToken)
        } else {
            Ok(tokens)
        }
    }
}

#[derive(Debug, Clone)]
pub struct Spanned<T>(pub T, pub Span);

#[derive(Debug, Clone, PartialEq, Default)]
pub struct Span {
    pub start: usize,
    pub end: usize,
}

impl Span {
    pub fn conjoin(&self, other: &Span) -> Self {
        let start = cmp::min(self.start, other.start);
        let end = cmp::max(self.end, other.end);

        Self { start, end }
    }
}

impl fmt::Display for Span {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "start: {}, end: {}", self.start, self.end)
    }
}

impl Into<core::ops::Range<usize>> for Span {
    fn into(self) -> core::ops::Range<usize> {
        self.start..self.end
    }
}

impl From<core::ops::Range<usize>> for Span {
    fn from(value: core::ops::Range<usize>) -> Self {
        Span {
            start: value.start,
            end: value.end
        }
    }
}
