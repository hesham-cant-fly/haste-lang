use std::collections::LinkedList;

use macros::FromTuple;

use crate::token::{Span, Token};

#[derive(Debug)]
pub struct Ast {
    pub declarations: DeclarationList,
}

#[derive(Debug, Clone)]
pub struct Spanned<T>(pub T, pub Span);

pub type ExprRef = Box<Expr>;
pub type DeclarationRef = Box<Declaration>;
pub type DeclarationList = Vec<Declaration>;

pub type Expr = Spanned<ExprNode>;
pub type Declaration = Spanned<DeclarationNode>;

// Expression
#[derive(Debug, Clone)]
pub enum ExprNode {
    IntLit(Token),
    FloatLit(Token),
    Identifier(Token),

    Grouping(ExprRef),
    Unary(UnaryExpr),
    Binary(BinaryExpr),

    // Type
    Int,
    Float,
    Auto,
}

#[derive(Debug, FromTuple, Clone)]
pub struct UnaryExpr {
    pub op: Token,
    pub rhs: ExprRef,
}

#[derive(Debug, FromTuple, Clone)]
pub struct BinaryExpr {
    pub op: Token,
    pub lhs: ExprRef,
    pub rhs: ExprRef,
}

// Declaration
#[derive(Debug, Clone)]
pub enum DeclarationNode {
    Constant(ConstantDecl),
    Variable(VariableDecl),
}

#[derive(Debug, FromTuple, Clone)]
pub struct ConstantDecl {
    pub identifier: Token,
    pub tp: Option<Expr>,
    pub value: Option<Expr>,
}

#[derive(Debug, FromTuple, Clone)]
pub struct VariableDecl {
    pub identifier: Token,
    pub tp: Option<Expr>,
    pub value: Option<Expr>,
}
