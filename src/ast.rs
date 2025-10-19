use std::collections::LinkedList;

use macros::FromTuple;

use crate::token::Token;

#[derive(Debug)]
pub struct ASTFile {
    pub declarations: DeclarationList,
}

pub type ExprRef = Box<Expr>;
pub type TypeRef = Box<Type>;
pub type DeclarationRef = Box<Declaration>;
pub type DeclarationList = Vec<Declaration>;

// Expression
#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    Int        (Token),
    Float      (Token),
    Identifier (Token),
    Unary      (UnaryExpr),
    Binary     (BinaryExpr),
    Grouping   (ExprRef),
}

#[derive(Debug, FromTuple, Clone, PartialEq)]
pub struct UnaryExpr {
    pub op: Token,
    pub rhs: ExprRef,
}

#[derive(Debug, FromTuple, Clone, PartialEq)]
pub struct BinaryExpr {
    pub op:  Token,
    pub lhs: ExprRef,
    pub rhs: ExprRef,
}

// Type
#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Auto,
    Int,
    Float,
    // TODO: Allow Expression
}

// Declaration
#[derive(Debug, Clone, PartialEq)]
pub enum Declaration {
    Constant(ConstantDecl),
    Variable(VariableDecl),
}

#[derive(Debug, FromTuple, Clone, PartialEq)]
pub struct ConstantDecl {
    pub identifier: Token,
    pub tp: Option<Type>,
    pub value: Option<Expr>,
}

#[derive(Debug, FromTuple, Clone, PartialEq)]
pub struct VariableDecl {
    pub identifier: Token,
    pub tp: Option<Type>,
    pub value: Option<Expr>,
}
