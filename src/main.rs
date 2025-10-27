#![allow(unused)]
use std::{fs::File, io::Read, path::Path};

use chumsky::Parser;
use hoisting::hoist;
use macros::FromTuple;
use reporter::Reporter;
use token::TokenKind;

use crate::token::Token;

mod ast;
mod hir;
mod parser;
mod reporter;
mod symbol;
mod token;
mod hoisting;

fn main() {
    let path = Path::new("./main.haste");
    let mut f = File::open(path).unwrap();
    let mut src = String::new();
    f.read_to_string(&mut src).unwrap();

    let reporter = Reporter::new(src, Some(path.to_path_buf()));
    let mut tokens = Token::scan_source(&reporter).unwrap();
    tokens.pop();
    // println!("{:#?}", tokens);

    let ast = parser::parser().parse(&tokens).into_result().unwrap();
    // println!("{:#?}", ast);
    let hir = hoist(ast::Ast { declarations: ast });
    println!("{:#?}", hir);
}
