use std::collections::LinkedList;

use macros::FromTuple;

use crate::token::Token;
use crate::token::Spanned;
use crate::token::TokenKind;

#[derive(Debug)]
pub struct Ast {
    pub path: String,
    pub declarations: DeclarationList,
}

pub type ExprRef = Box<Spanned<ExprNode>>;
pub type DeclarationRef = Box<Spanned<DeclarationNode>>;
pub type DeclarationList = Vec<Spanned<DeclarationNode>>;

#[derive(Debug, Clone)]
pub enum UnOp {
    Plus,
    Negate,
}

impl TryFrom<TokenKind> for UnOp {
    type Error = ();

    fn try_from(value: TokenKind) -> Result<Self, Self::Error> {
        use TokenKind as Tk;
        match value {
            Tk::Plus => Ok(UnOp::Plus),
            Tk::Minus => Ok(UnOp::Negate),
            _ => Err(()),
        }
    }
}

impl TryFrom<Token> for UnOp {
    type Error = ();

    fn try_from(value: Token) -> Result<Self, Self::Error> {
        Self::try_from(value.kind)
    }
}

#[derive(Debug, Clone)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Pow,
}

impl TryFrom<TokenKind> for BinOp {
    type Error = ();

    fn try_from(value: TokenKind) -> Result<Self, Self::Error> {
        use TokenKind as Tk;
        match value {
            Tk::Plus => Ok(BinOp::Add),
            Tk::Minus => Ok(BinOp::Sub),
            Tk::Star => Ok(BinOp::Mul),
            Tk::FSlash => Ok(BinOp::Div),
            Tk::DoubleStar => Ok(BinOp::Pow),
            _ => Err(()),
        }
    }
}

impl TryFrom<Token> for BinOp {
    type Error = ();

    fn try_from(value: Token) -> Result<Self, Self::Error> {
        Self::try_from(value.kind)
    }
}

// Expression
#[derive(Debug, Clone)]
pub enum ExprNode {
    IntLit(i64),
    FloatLit(f64),
    Identifier(String),

    Grouping(ExprRef),
    Unary(UnaryExpr),
    Binary(BinaryExpr),

    // Type
    Int,
    Float,
    Auto,
    Type,
}

#[derive(Debug, FromTuple, Clone)]
pub struct UnaryExpr {
    pub op: UnOp,
    pub rhs: ExprRef,
}

#[derive(Debug, FromTuple, Clone)]
pub struct BinaryExpr {
    pub op: BinOp,
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
    pub identifier: String,
    pub tp: Option<Spanned<ExprNode>>,
    pub value: Spanned<ExprNode>,
}

#[derive(Debug, FromTuple, Clone)]
pub struct VariableDecl {
    pub identifier: String,
    pub tp: Option<Spanned<ExprNode>>,
    pub value: Spanned<ExprNode>,
}
