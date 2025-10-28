use crate::token::{Span, Token};

use core::fmt;
use std::collections::HashMap;

/// Haste IR (HIR), a Stack Machine Code (SMC) IR
/// that represents the entire program. designed
/// to simplify the CTFE and analysis.
#[derive(Debug, Default)]
pub struct Hir {
    pub ht: HoistTable,
    pub instructions: Vec<Instruction>,
}

impl fmt::Display for Hir {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "hoist-table: {{")?;
        {
            let mut v: Vec<(String, usize)> = self.ht.clone().into_iter().collect();
            v.sort_by(|(_, a), (_, b)| a.cmp(b));
            for (name, index) in v.into_iter() {
                writeln!(f, "  \"{}\": %{},", name, index)?;
            }
        }
        writeln!(f, "}}")?;

        for (index, instruction) in (&self.instructions).iter().enumerate() {
            writeln!(f, "%{:08} -> {}", index, instruction)?;
        }

        fmt::Result::Ok(())
    }
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
        self.len()
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

impl fmt::Display for Instruction {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} : {}", self.node, self.span)
    }
}

impl Instruction {
    pub fn new(span: Span, node: Node) -> Self {
        Self { span, node }
    }
}

#[derive(Debug)]
pub enum PrimitiveType {
    Int,
    Float,
    Auto,
}

impl fmt::Display for PrimitiveType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PrimitiveType::Int => write!(f, "int"),
            PrimitiveType::Float => write!(f, "float"),
            PrimitiveType::Auto => write!(f, "auto"),
        }
    }
}

#[derive(Debug)]
pub enum Node {
    // Constants
    Identifier(Token),
    Integer(i64),
    Float(f64),
    Type(PrimitiveType),

    // Arithmatics
    UnaryMinus, // - <something>
    UnaryPlus,  // + <something>

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

impl fmt::Display for Node {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Node::Identifier(token) => write!(f, "ident(\"{}\")", token.lexem),
            Node::Integer(i) => write!(f, "int({})", i),
            Node::Float(fl) => write!(f, "float({})", fl),
            Node::Type(primitive_type) => write!(f, "type({})", primitive_type),
            Node::UnaryMinus => write!(f, "un_op(negate)"),
            Node::UnaryPlus => write!(f, "un_op(plusify)"),
            Node::Add => write!(f, "bin_op(add)"),
            Node::Sub => write!(f, "bin_op(sub)"),
            Node::Mul => write!(f, "bin_op(mul)"),
            Node::Div => write!(f, "bin_op(div)"),
            Node::Pow => write!(f, "bin_op(pow)"),
            Node::ConstantDecl {
                name,
                begining,
                has_initializer,
                has_type_info,
            } => write!(f, "decl_const(
  name: \"{}\",
  begining: {},
  has_init: {},
  has_type: {}
)", name.lexem, begining, has_initializer, has_type_info),
            Node::VariableDecl {
                name,
                begining,
                has_initializer,
                has_type_info,
            } => write!(f, "decl_var(
  name: \"{}\",
  begining: {},
  has_init: {},
  has_type: {}
)", name.lexem, begining, has_initializer, has_type_info),
        }
    }
}
