use crate::token::{Span, TokenKind};
use crate::hir::{
    Instruction,
    PrimitiveType,
    Node as HirNode,
    Hir,
};
use crate::ast::{
    Ast, ConstantDecl, DeclarationNode, Expr, ExprNode, Spanned, VariableDecl
};

pub fn hoist(ast: Ast) -> Hir {
    let mut hir = Hir::new();

    for declaration in ast.declarations {
        let Spanned(declaration, span) = declaration;

        use DeclarationNode as Dn;
        match declaration {
            Dn::Constant(cons) => hoist_constant_declaration(&mut hir, span.clone(), &cons),
            Dn::Variable(var) => hoist_variable_declaration(&mut hir, span.clone(), &var),
        };
    }

    hir
}

fn hoist_constant_declaration(hir: &mut Hir, span: Span, decl: &ConstantDecl) -> usize {
    let begining = hir.current();
    let name = decl.identifier.lexem.clone();
    let const_decl = HirNode::ConstantDecl {
        begining,
        name: decl.identifier.clone(),
        has_initializer: decl.value.is_some(),
        has_type_info: decl.tp.is_some(),
    };

    if hir.is_declared(&name) {
        todo!()
    }

    if let Some(expr) = &decl.tp {
        hoist_expr(hir, &expr);
    }
    if let Some(expr) = &decl.value {
        hoist_expr(hir, &expr);
    }

    let result_pos = hir.push(
        Instruction::new(
            span,
            const_decl,
        )
    );

    hir.declare(name, result_pos).unwrap();

    result_pos
}

fn hoist_variable_declaration(hir: &mut Hir, span: Span, decl: &VariableDecl) -> usize {
    let begining = hir.current();
    let name = decl.identifier.lexem.clone();
    let const_decl = HirNode::VariableDecl {
        begining,
        name: decl.identifier.clone(),
        has_initializer: decl.value.is_some(),
        has_type_info: decl.tp.is_some(),
    };

    if hir.is_declared(&name) {
        todo!()
    }

    if let Some(expr) = &decl.tp {
        hoist_expr(hir, &expr);
    }
    if let Some(expr) = &decl.value {
        hoist_expr(hir, &expr);
    }

    let result_pos = hir.push(
        Instruction::new(
            span,
            const_decl,
        )
    );

    hir.declare(name, result_pos).unwrap();

    result_pos
}

fn hoist_expr(hir: &mut Hir, expr: &Expr) -> usize {
    let Spanned(expr, span) = expr;
    let span = span.clone();
    match expr {
        ExprNode::IntLit(token) => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Integer(
                        token.lexem.parse().unwrap()
                    )
                )
            )
        }
        ExprNode::FloatLit(token) => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Float(
                        token.lexem.parse().unwrap()
                    )
                )
            )
        },
        ExprNode::Identifier(token) => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Identifier(
                        token.clone()
                    )
                )
            )
        },
        ExprNode::Int => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Type(PrimitiveType::Int)
                )
            )
        },
        ExprNode::Float => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Type(PrimitiveType::Float)
                )
            )
        },
        ExprNode::Auto => {
            hir.push(
                Instruction::new(
                    span,
                    HirNode::Type(PrimitiveType::Auto)
                )
            )
        },
        ExprNode::Grouping(spanned) => {
            hoist_expr(
                hir,
                spanned,
            )
        },
        ExprNode::Unary(unary_expr) => {
            let op = hirnode_from_un_op(unary_expr.op.kind.clone());
            _ = hoist_expr(hir, &unary_expr.rhs);
            hir.push(
                Instruction::new(
                    span,
                    op,
                )
            )
        },
        ExprNode::Binary(binary_expr) => {
            let op = hirnode_from_bin_op(binary_expr.op.kind.clone());
            _ = hoist_expr(hir, &binary_expr.lhs);
            _ = hoist_expr(hir, &binary_expr.rhs);
            hir.push(
                Instruction::new(
                    span,
                    op,
                )
            )
        },
    }
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
