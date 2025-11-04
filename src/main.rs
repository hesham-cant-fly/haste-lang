#![allow(unused)]
use std::{
    fs::File,
    io::{Read, Write},
    path::Path, rc::Rc,
};

use file_source_pool::FSPool;
// use hoisting::hoist;
use macros::FromTuple;
use token::TokenKind;

use crate::token::Token;

mod analyzer;
mod ast;
mod file_source_pool;
mod hir;
// mod hoisting;
mod parser;
mod reporter;
mod token;
mod value;

// fn main() {
//     let mut module = qbe::Module::new();

//     let mut main_func = qbe::Function::new(
//         qbe::Linkage::public(),
//         "\"main\"",
//         vec![],
//         None
//     );
 
//     let mut main_func_start_block = main_func.add_block("start");
 
//     main_func_start_block.add_instr(qbe::Instr::Ret(None));
 
//     module.add_function(main_func);
 
//     println!("{}", module);
// }

fn main() {
    let path = "./main.haste";
    let fs_pool = FSPool::new();

    let mut tokens = Token::scan_source(Rc::clone(&fs_pool), path).unwrap();

    let ast = parser::parse(&tokens);
    println!("{:#?}", ast);

    // let hir = hoist(
    //     Rc::clone(&fs_pool),
    //     ast::Ast { path: "./main.haste".to_string(), declarations: ast },
    // ).unwrap();
    // let mut f = File::create("./main.hir").unwrap();
    // writeln!(f, "{}", hir).unwrap();
}
