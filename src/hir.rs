use crate::token::{Span, Token};

use std::collections::HashMap;

/// Haste IR (HIR), a Stack Machine Code (SMC) IR
/// that represents the entire program. designed
/// to simplify the CTFE and analysis.
#[derive(Debug, Default)]
pub struct Hir {
    pub ht: HoistTable,
    pub instructions: Vec<Instruction>,
}

impl Hir {
    pub fn new() -> Self {
        Self {
            ..Default::default()
        }
    }

    pub fn len(&self) -> usize {
        self.instructions.len()
    }

    pub fn current(&self) -> usize {
        let len = self.len();
        if len == 0 {
            return 0;
        }
        len - 1
    }

    pub fn push(&mut self, instruction: Instruction) -> usize {
        self.instructions.push(instruction);
        self.instructions.len() - 1
    }

    pub fn is_declared(&self, name: &String) -> bool {
        self.ht.contains_key(name)
    }

    pub fn declare(&mut self, name: String, at: usize) -> Option<usize> {
        if self.is_declared(&name) {
            return None;
        }
        self.ht.insert(name, at.clone());
        Some(at)
    }
}

pub type HoistTable = HashMap<String, usize>;

#[derive(Debug)]
pub struct Instruction {
    pub span: Span,
    pub node: Node,
}

impl Instruction {
    pub fn new(span: Span, node: Node) -> Self {
        Self {
            span, node,
        }
    }
}

#[derive(Debug)]
pub enum PrimitiveType {
    Int,
    Float,
    Auto,
}

#[derive(Debug)]
pub enum Node {
    End,

    // Constants
    Identifier(Token),
    Integer(i64),
    Float(f64),
    Type(PrimitiveType),

    // Arithmatics
    UnaryMinus, // - <something>
    UnaryPlus, // + <something>

    Add, // <something> + <something>
    Sub, // <something> - <something>
    Mul, // <something> * <something>
    Div, // <something> / <something>
    Pow, // <something> ** <something>

    // Declarations
    ConstantDecl {
        name: Token,
        begining: usize, // used for hoisting
        has_initializer: bool,
        has_type_info: bool,
    },
    VariableDecl {
        name: Token,
        begining: usize, // used for hoisting
        has_initializer: bool,
        has_type_info: bool,
    },
}
