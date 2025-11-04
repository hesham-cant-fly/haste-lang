use std::ops;
use std::process::Output;

use crate::ast;
use crate::token::Span;
use crate::token::Spanned;
use crate::token::Token;
use crate::token::TokenKind;

#[repr(u8)]
#[non_exhaustive]
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Debug)]
pub enum Precedence {
    None,
    Term,    // + -
    Factor,  // * /
    Power,   // **
    Unary,   // - +
    Primary, // literals, identifiers, grouping
}

impl Precedence {
    pub fn lowest() -> Self {
        Precedence::None
    }
}

impl ops::Add for Precedence {
    type Output = Precedence;

    fn add(self, rhs: Self) -> Self::Output {
        unsafe { std::mem::transmute((self as u8) + (rhs as u8)) }
    }
}

type PrefixFn<'a> = fn(&mut Parser<'a>, Span) -> ParserResult<Spanned<ast::ExprNode>>;
type InfixFn<'a> =
    fn(&mut Parser<'a>, Span, Spanned<ast::ExprNode>) -> ParserResult<Spanned<ast::ExprNode>>;

#[derive(Debug)]
pub struct Rule<'a> {
    pub prefix: Option<PrefixFn<'a>>,
    pub infix: Option<InfixFn<'a>>,
    pub precedence: Precedence,
    pub right_assoc: bool,
}

type ParserResult<T> = Result<T, ()>;

#[derive(Debug)]
pub struct Parser<'a> {
    pub tokens: &'a [Token],
    pub current: usize,
}

impl<'a> Parser<'a> {
    pub fn new(tokens: &'a [Token]) -> Self {
        Self { tokens, current: 0 }
    }

    pub fn expr(&mut self) -> ParserResult<Spanned<ast::ExprNode>> {
        self.parse_precedence(Precedence::lowest())
    }

    pub fn parse_precedence(&mut self, prec: Precedence) -> ParserResult<Spanned<ast::ExprNode>> {
        let token = self.advance();
        let rule = Self::get_rule(&token.kind).ok_or(())?;
        let prefix_fn = rule.prefix.ok_or(())?;

        let start = token.span;
        let mut lhs = prefix_fn(self, start.clone())?;

        while !self.ended() {
            let token = self.peek();
            let rule = Self::get_rule(&token.kind).ok_or(())?;
            if rule.precedence <= prec {
                break;
            }

            self.advance();

            let Some(infix_fn) = rule.infix else {
                break;
            };

            lhs = infix_fn(self, start.clone(), lhs)?;
        }

        Ok(lhs)
    }

    pub fn get_rule(kind: &TokenKind) -> Option<Rule<'a>> {
        macro_rules! rule {
            (None, $infix:expr, $prec:expr, $assoc:expr) => {
                Some(Rule {
                    prefix: None,
                    infix: Some($infix),
                    precedence: $prec,
                    right_assoc: $assoc,
                })
            };

            ($prefix:expr, None, $prec:expr, $assoc:expr) => {
                Some(Rule {
                    prefix: Some($prefix),
                    infix: None,
                    precedence: $prec,
                    right_assoc: $assoc,
                })
            };

            ($prefix:expr, $infix:expr, $prec:expr, $assoc:expr) => {
                Some(Rule {
                    prefix: Some($prefix),
                    infix: Some($infix),
                    precedence: $prec,
                    right_assoc: $assoc,
                })
            };
        }

        use TokenKind as Tk;
        match kind {
            Tk::IntLit => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::FloatLit => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::Identifier => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::Auto => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::Int => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::Float => rule!(Self::literal, None, Precedence::Primary, false),
            Tk::Type => rule!(Self::literal, None, Precedence::Primary, false),

            Tk::Plus => rule!(Self::unary, Self::binary, Precedence::Term, false),
            Tk::Minus => rule!(Self::unary, Self::binary, Precedence::Term, false),
            Tk::Star => rule!(None, Self::binary, Precedence::Factor, false),
            Tk::FSlash => rule!(None, Self::binary, Precedence::Factor, false),
            Tk::DoubleStar => rule!(None, Self::binary, Precedence::Power, true),

            _ => None,
        }
    }

    pub fn literal(&mut self, start: Span) -> ParserResult<Spanned<ast::ExprNode>> {
        use TokenKind as Tk;

        let token = self.previous();
        let expr = match token.kind {
            Tk::IntLit => ast::ExprNode::IntLit(token.lexem.parse().unwrap()),
            Tk::FloatLit => ast::ExprNode::FloatLit(token.lexem.parse().unwrap()),
            Tk::Identifier => ast::ExprNode::Identifier(token.lexem.clone()),
            Tk::Auto => ast::ExprNode::Auto,
            Tk::Int => ast::ExprNode::Int,
            Tk::Float => ast::ExprNode::Float,
            Tk::Type => ast::ExprNode::Type,

            _ => return Err(()),
        };

        Ok(Spanned(expr, start))
    }

    pub fn unary(&mut self, start: Span) -> ParserResult<Spanned<ast::ExprNode>> {
        let op = self.previous();
        let rhs = self.parse_precedence(Precedence::Unary)?;
        let end = self.previous().span;

        Ok(Spanned(
            ast::ExprNode::Unary((op.try_into().unwrap(), Box::new(rhs)).into()),
            start.conjoin(&end),
        ))
    }

    pub fn binary(
        &mut self,
        start: Span,
        lhs: Spanned<ast::ExprNode>,
    ) -> ParserResult<Spanned<ast::ExprNode>> {
        let op = self.previous();
        let rule = Self::get_rule(&op.kind).ok_or(())?;

        let precedence = if rule.right_assoc {
            rule.precedence
        } else {
            rule.precedence + unsafe { std::mem::transmute(1 as u8) }
        };

        let rhs = self.parse_precedence(precedence)?;
        let end = self.previous().span;

        Ok(Spanned(
            ast::ExprNode::Binary((op.try_into().unwrap(), Box::new(lhs), Box::new(rhs)).into()),
            start.conjoin(&end),
        ))
    }

    pub fn peek(&self) -> Token {
        self.tokens[self.current].clone()
    }

    pub fn previous(&self) -> Token {
        self.tokens[self.current - 1].clone()
    }

    pub fn advance(&mut self) -> Token {
        if self.ended() {
            return self.peek();
        }
        self.current += 1;
        self.previous()
    }

    pub fn ended(&self) -> bool {
        self.current >= self.tokens.len()
    }
}

pub fn parse(tokens: &[Token]) -> ParserResult<Spanned<ast::ExprNode>> {
    let mut parser = Parser::new(tokens);

    parser.expr()
}
