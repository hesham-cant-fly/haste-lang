use core::fmt;

use crate::{token::Span, value::{HasteType, HasteValue}};

#[derive(Debug, Clone)]
pub struct Symbol {
    pub name: String,
    pub span: Span,
    pub value: HasteType,
    pub constant: bool,
}
