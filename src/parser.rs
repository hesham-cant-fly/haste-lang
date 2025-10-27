use chumsky::Parser;
use chumsky::prelude::*;

use crate::ast;
use crate::ast::Spanned;
use crate::token::Token;
use crate::token::TokenKind;

macro_rules! token {
    ($pt:pat) => {
        any().filter(|t: &Token| matches!(t.kind, $pt))
    };
}

macro_rules! binary {
    ($left:expr, $pt:pat, $right:expr) => {
        $left.foldl_with(token!($pt).then($right).repeated(), |lhs, (op, rhs), extra| {
            let tokens: &[Token] = extra.slice();
            let fst = tokens.first().unwrap();
            let lst = tokens.last().unwrap();
            let span = fst.span.conjoin(&lst.span);

            ast::Spanned(
                ast::ExprNode::Binary((op.clone(), Box::new(lhs), Box::new(rhs)).into()),
                span,
            )
        })
    };

    ($left:expr, $pt:pat) => {
        binary!($left, $pt, $left)
    };
}

pub fn parser<'a>() -> impl Parser<'a, &'a [Token], ast::DeclarationList> {
    let expr = recursive(|expr| {
        let int_lit = token!(TokenKind::IntLit).map(|t| ast::ExprNode::IntLit(t.clone()));
        let float_lit = token!(TokenKind::FloatLit).map(|t| ast::ExprNode::FloatLit(t.clone()));
        let identifier = token!(TokenKind::Identifier).map(|t| ast::ExprNode::Identifier(t.clone()));
        let int_tp = token!(TokenKind::Int).map(|_| ast::ExprNode::Int);
        let float_tp = token!(TokenKind::Float).map(|_| ast::ExprNode::Float);
        let auto_tp = token!(TokenKind::Auto).map(|_| ast::ExprNode::Auto);

        let grouping = expr
            .delimited_by(token!(TokenKind::OpenParen), token!(TokenKind::CloseParen))
            .map(|t: ast::Expr| ast::ExprNode::Grouping(t.into()));

        let primary = choice((
            int_lit, float_lit, identifier, int_tp, float_tp, auto_tp, grouping,
        ))
            .map_with(|ast_node, extra| {
                let tokens: &[Token] = extra.slice();
                let fst = tokens.first().unwrap();
                let lst = tokens.last().unwrap();
                let span = fst.span.conjoin(&lst.span);

                ast::Spanned(ast_node, span)
            });

        let unary = token!(TokenKind::Minus | TokenKind::Plus)
            .repeated()
            .foldr_with(primary, |op, rhs, extra| {
                let tokens: &[Token] = extra.slice();
                let fst = tokens.first().unwrap();
                let lst = tokens.last().unwrap();
                let span = fst.span.conjoin(&lst.span);

                ast::Spanned(
                    ast::ExprNode::Unary((op.clone(), rhs.into()).into()),
                    span,
                )
            });

        let factor = binary!(unary.clone(), TokenKind::Star | TokenKind::FSlash);
        let term = binary!(factor.clone(), TokenKind::Plus | TokenKind::Minus);
        term
    });

    let constant = token!(TokenKind::Const)
        .ignore_then(token!(TokenKind::Identifier))
        .then(token!(TokenKind::Colon).ignore_then(expr.clone()).or_not())
        .then(token!(TokenKind::Equal).ignore_then(expr.clone()).or_not())
        .then_ignore(token!(TokenKind::Semicolon))
        .map(|((name, tp), value)| ast::DeclarationNode::Constant((name, tp, value).into()));

    let variable = token!(TokenKind::Var)
        .ignore_then(token!(TokenKind::Identifier))
        .then(token!(TokenKind::Colon).ignore_then(expr.clone()).or_not())
        .then(token!(TokenKind::Equal).ignore_then(expr.clone()).or_not())
        .then_ignore(token!(TokenKind::Semicolon))
        .map(|((name, tp), value)| ast::DeclarationNode::Variable((name, tp, value).into()));

    let declaration = choice((constant, variable))
        .map_with(|ast_node, extra| {
            let tokens: &[Token] = extra.slice();
            let fst = tokens.first().unwrap();
            let lst = tokens.last().unwrap();
            let span = fst.span.conjoin(&lst.span);

            ast::Spanned(ast_node, span)
        });

    declaration.repeated().collect()
}
