use std::rc::Rc;

use crate::ast;
use crate::reporter::Reporter;
use crate::symbol::{Symbol, SymbolId, SymbolKind, SymbolTable};

#[derive(Debug)]
pub struct Module {
    pub globals: Vec<SymbolId>,
}

pub fn hoist(file: ast::ASTFile, reporter: &Reporter) -> (Module, SymbolTable) {
    let mut symbol_table = SymbolTable::new();
    let mut global_scope = symbol_table.global_scope_mut();
    let mut module = Module {globals: vec![]};

    for ref declaration in file.declarations {
        let mut constant = true;
        let name: String = match declaration {
            ast::Declaration::Constant(c) => reporter.get_lexem(&c.identifier).into(),
            ast::Declaration::Variable(v) => {
                constant = false;
                reporter.get_lexem(&v.identifier).into()
            },
        };

        global_scope.insert(name.clone(), Symbol {
            constant,
            kind: SymbolKind::Untyped,
            declaration: Rc::new(declaration.clone()),
            visited: false,
        });

        module.globals.push(name);
    }

    (module, symbol_table)
}
