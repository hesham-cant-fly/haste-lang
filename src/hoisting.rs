use std::cell::RefCell;
use std::rc::Rc;

use crate::ast::{Ast, ConstantDecl, DeclarationNode, ExprNode, VariableDecl};
use crate::file_source_pool::FSPool;
use crate::hir::{Hir, Instruction, Node as HirNode};
use crate::reporter::{Label, ReportKind};
use crate::token::{Span, Spanned, TokenKind};
use crate::value::HasteType;

#[derive(Debug)]
pub enum Error {
    Redeclaration,
}

pub fn hoist(fs_pool: Rc<RefCell<FSPool>>, ast: Ast) -> Result<Hir, Error> {
    let mut hir = Hir::new(ast.path);

    for declaration in ast.declarations {
        let Spanned(declaration, span) = declaration;

        use DeclarationNode as Dn;
        match declaration {
            Dn::Constant(cons) => {
                hoist_constant_declaration(&mut hir, span.clone(), &cons, Rc::clone(&fs_pool))?;
            }
            Dn::Variable(var) => {
                hoist_variable_declaration(&mut hir, span.clone(), &var, Rc::clone(&fs_pool))?;
            }
        }
    }

    Ok(hir)
}

fn hoist_constant_declaration(
    hir: &mut Hir,
    span: Span,
    decl: &ConstantDecl,
    fs_pool: Rc<RefCell<FSPool>>,
) -> Result<(), Error> {
    let begining = hir.current();
    let name = decl.identifier.lexem.clone();
    let const_decl = HirNode::ConstantDecl {
        begining,
        name: decl.identifier.clone(),
        has_type_info: decl.tp.is_some(),
    };

    if hir.is_declared(&name) {
        fs_pool
            .borrow()
            .build_report(&hir.path, ReportKind::Error, span.clone())
            .unwrap()
            .with_message(format!("A Redeclaration of '{}'.", name))
            .with_label(Label::new(span.clone()).with_msg("redeclaration right here."))
            .print();
        return Err(Error::Redeclaration);
    }

    hoist_expr(hir, &decl.value);
    if let Some(expr) = &decl.tp {
        hoist_expr(hir, &expr);
    }

    let result_pos = hir.push(Instruction::new(span, const_decl));

    hir.declare(name, result_pos).unwrap();

    Ok(())
}

fn hoist_variable_declaration(
    hir: &mut Hir,
    span: Span,
    decl: &VariableDecl,
    fs_pool: Rc<RefCell<FSPool>>,
) -> Result<(), Error> {
    let begining = hir.current();
    let name = decl.identifier.lexem.clone();
    let const_decl = HirNode::VariableDecl {
        begining,
        name: decl.identifier.clone(),
        has_type_info: decl.tp.is_some(),
    };

    if hir.is_declared(&name) {
        fs_pool
            .borrow()
            .build_report(&hir.path, ReportKind::Error, span.clone())
            .unwrap()
            .with_message(format!("A Redeclaration of '{}'.", name))
            .with_label(Label::new(span.clone()).with_msg("redeclaration right here."))
            .print();
        return Err(Error::Redeclaration);
    }

    hoist_expr(hir, &decl.value);
    if let Some(expr) = &decl.tp {
        hoist_expr(hir, &expr);
    }

    let result_pos = hir.push(Instruction::new(span, const_decl));

    hir.declare(name, result_pos).unwrap();

    Ok(())
}

fn hoist_expr(hir: &mut Hir, expr: &Spanned<ExprNode>) {
    let Spanned(expr, span) = expr;
    let span = span.clone();
    match expr {
        ExprNode::IntLit(token) => {
            hir.push(Instruction::new(
                span,
                HirNode::Integer(token.lexem.parse().unwrap()),
            ));
        }
        ExprNode::FloatLit(token) => {
            hir.push(Instruction::new(
                span,
                HirNode::Float(token.lexem.parse().unwrap()),
            ));
        }
        ExprNode::Identifier(token) => {
            hir.push(Instruction::new(span, HirNode::Identifier(token.clone())));
        }
        ExprNode::Int => {
            hir.push(Instruction::new(span, HirNode::Type(HasteType::Int)));
        }
        ExprNode::Float => {
            hir.push(Instruction::new(span, HirNode::Type(HasteType::Float)));
        }
        ExprNode::Type => {
            hir.push(Instruction::new(span, HirNode::Type(HasteType::Type)));
        }
        ExprNode::Auto => {
            hir.push(Instruction::new(span, HirNode::Type(HasteType::Auto)));
        }
        ExprNode::Grouping(spanned) => {
            hoist_expr(hir, spanned);
        }
        ExprNode::Unary(unary_expr) => {
            let op = hirnode_from_un_op(unary_expr.op.kind.clone());
            hoist_expr(hir, &unary_expr.rhs);
            hir.push(Instruction::new(span, op));
        }
        ExprNode::Binary(binary_expr) => {
            let op = hirnode_from_bin_op(binary_expr.op.kind.clone());
            _ = hoist_expr(hir, &binary_expr.lhs);
            _ = hoist_expr(hir, &binary_expr.rhs);
            hir.push(Instruction::new(span, op));
        }
    };
}

fn hirnode_from_un_op(kind: TokenKind) -> HirNode {
    match kind {
        TokenKind::Plus => HirNode::UnaryPlus,
        TokenKind::Minus => HirNode::UnaryMinus,
        _ => unreachable!(),
    }
}

fn hirnode_from_bin_op(kind: TokenKind) -> HirNode {
    match kind {
        TokenKind::Plus => HirNode::Add,
        TokenKind::Minus => HirNode::Sub,
        TokenKind::Star => HirNode::Mul,
        TokenKind::DoubleStar => HirNode::Pow,
        TokenKind::FSlash => HirNode::Div,
        _ => unreachable!(),
    }
}
