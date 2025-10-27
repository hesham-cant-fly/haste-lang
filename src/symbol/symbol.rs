use std::collections::HashMap;
use std::rc::Rc;

use crate::ast;

use super::Type;

pub type SymbolId = String;
pub type Scope = HashMap<SymbolId, Symbol>;

#[derive(Debug, Clone, PartialEq)]
pub enum SymbolKind {
    Untyped,
    Typed(Type),
}

#[derive(Debug, Clone)]
pub struct Symbol {
    pub kind: SymbolKind,
    pub declaration: Rc<ast::DeclarationNode>,
    pub constant: bool,
    pub visited: bool,
}

impl Symbol {
    pub fn is_declared(&self) -> bool {
        self.kind == SymbolKind::Untyped
    }
}

#[derive(Debug)]
pub struct SymbolTable {
    pub symbols: Vec<Scope>,
}

impl SymbolTable {
    pub fn new() -> Self {
        let mut result = Self {
            symbols: Vec::new(),
        };
        result.symbols.push(Scope::new());
        result
    }

    pub fn begin_scope(&mut self) -> &mut Scope {
        self.symbols.push(Scope::new());
        self.symbols.last_mut().unwrap()
    }

    pub fn end_scope(&mut self) {
        self.symbols.pop().unwrap();
    }

    pub fn global_scope(&self) -> &Scope {
        self.symbols.first().unwrap()
    }

    pub fn global_scope_mut(&mut self) -> &mut Scope {
        self.symbols.first_mut().unwrap()
    }

    pub fn local_scope(&self) -> &Scope {
        self.symbols.last().unwrap()
    }

    pub fn local_scope_mut(&mut self) -> &mut Scope {
        self.symbols.last_mut().unwrap()
    }

    pub fn is_defined(&self, key: String) -> bool {
        match self.find_local_first(key) {
            Some(symbol) => symbol.kind == SymbolKind::Untyped,
            None => false,
        }
    }

    pub fn define_local(&mut self, key: String, symb: Symbol) {
        assert!(self.is_defined(key.clone()));
        let mut local = self.local_scope_mut();
        local.insert(key, symb);
    }

    pub fn find_local_first(&self, key: String) -> Option<&Symbol> {
        for scope in self.symbols.iter().rev() {
            if let Some(symbol) = scope.get(&key) {
                return Some(symbol);
            }
        }
        None
    }

    pub fn find_local_first_mut(&mut self, key: String) -> Option<&mut Symbol> {
        for scope in &mut self.symbols.iter_mut().rev() {
            if let Some(symbol) = scope.get_mut(&key) {
                return Some(symbol);
            }
        }
        None
    }
}
