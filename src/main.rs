#![allow(unused)]
use std::{fs::File, io::Read, path::Path};

use analysis::Analyzer;
use chumsky::Parser;
use chumsky::prelude::*;
use hir::hoist;
use macros::FromTuple;
use reporter::Reporter;
use token::TokenKind;

use crate::token::Token;

mod analysis;
mod ast;
mod hir;
mod parser;
mod reporter;
mod symbol;
mod token;

fn parser<'a>() -> impl Parser<'a, &'a [Token], ast::Expr> {
    macro_rules! token {
        ($pt:pat) => {
            any().filter(|t: &Token| matches!(t.kind, $pt))
        };
    }

    macro_rules! binary {
        ($left:expr, $pt:pat, $right:expr) => {
            $left.foldl(token!($pt).then($right).repeated(), |lhs, (op, rhs)| {
                ast::Expr::Binary((op.clone(), Box::new(lhs), Box::new(rhs)).into())
            })
        };
        ($left:expr, $pt:pat) => {
            binary!($left, $pt, $left)
        };
    }

    recursive(|expr| {
        let int_lit = token!(TokenKind::IntLit)
            .map(|t| ast::Expr::Int(t.clone()));

        let float_lit = token!(TokenKind::FloatLit)
            .map(|t| ast::Expr::Float(t.clone()));

        let identifier = token!(TokenKind::Identifier)
            .map(|t| ast::Expr::Identifier(t.clone()));

        let grouping = expr
            .delimited_by(token!(TokenKind::OpenParen), token!(TokenKind::CloseParen))
            .map(|t: ast::Expr| ast::Expr::Grouping(t.into()));

        let primary = choice(
            (
                int_lit,
                float_lit,
                identifier,
                grouping,
            )
        );

        let unary = token!(TokenKind::Minus | TokenKind::Plus)
            .repeated()
            .foldr(primary, |op, rhs| ast::Expr::Unary((op.clone(), rhs.into()).into()));

        let factor = binary!(unary.clone(), TokenKind::Star | TokenKind::FSlash);
        let term = binary!(factor.clone(), TokenKind::Plus | TokenKind::Minus);
        term
    })
}

fn main() {
    let path = Path::new("./main.haste");
    let mut f = File::open(path).unwrap();
    let mut src = String::new();
    f.read_to_string(&mut src).unwrap();

    let reporter = Reporter::new(src, Some(path.to_path_buf()));
    let mut tokens = Token::scan_source(&reporter).unwrap();
    tokens.pop();
    println!("{:#?}", tokens);

    let ast = parser().parse(&tokens).into_result();
    println!("{:#?}", ast);
}
