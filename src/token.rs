use std::path::{Path, PathBuf};

use colored::Colorize;
use logos::Logos;

use crate::reporter::{self, Reporter};

#[repr(u8)]
#[derive(Logos, Clone, Debug, PartialEq, Default)]
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
    #[token("auto")]                      Auto,
    #[token("int")]                       Int,
    #[token("float")]                     Float,

    #[regex("(?&digit)+")]                IntLit,
    #[regex(r"(?&digit)+\.(?&digit)+")]   FloatLit,
    #[regex(r"(?&symbol)(?&symbolnum)*")] Identifier,

    #[default]
    EOF,
}

#[derive(Debug, Clone, PartialEq, Default)]
pub struct Token {
    pub span: Span,
    pub kind: TokenKind,
}

impl Token {
    pub fn new(kind: TokenKind, span: Span) -> Token {
        Token { kind, span }
    }

    pub fn scan_source(reporter: &Reporter) -> Result<Vec<Token>, ()> {
        let mut lxr = TokenKind::lexer(&reporter.src);
        let mut tokens = Vec::<Token>::new();
        let mut has_error = false;

        while let Some(token) = lxr.next() {
            let span = Span::from(lxr.span());
            let lexem = lxr.slice();
            let Ok(kind) = token else {
                has_error = true;
                reporter.report_error_at(
                    format!("Invalid token '{}'", lexem),
                    span.start,
                );
                continue;
            };
            tokens.push(Token::new(kind, lxr.span().into()));
        }

        tokens.push(Token::new(TokenKind::EOF, Default::default()));

        if has_error {
            Err(())
        } else {
            Ok(tokens)
        }
    }
}

#[derive(Debug, Clone, PartialEq, Default)]
pub struct Span {
    pub start: usize,
    pub end: usize,
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
