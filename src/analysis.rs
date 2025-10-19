use std::rc::Rc;

// use ariadne::{Config, Fmt, Label, Report, ReportKind, Source};

use crate::reporter::{Label, ReportKind, Reporter};
use crate::symbol::{Symbol, SymbolKind, SymbolTable};
use crate::token::Token;
use crate::{ast, hir, symbol};

pub type AnalyzerResult<T> = Result<T, ()>;

#[derive(Debug)]
pub struct Analyzer {
    pub reporter: Reporter,
    pub module: hir::Module,
    pub symbol_table: SymbolTable,
}

impl Analyzer {
    pub fn new(module: hir::Module, symbol_table: SymbolTable, reporter: Reporter) -> Self {
        Analyzer {
            module,
            reporter,
            symbol_table,
        }
    }

    pub fn analyse(&mut self) -> AnalyzerResult<()> {
        // How to get jumped by a rust dev 101
        unsafe {
            let this = self as *mut Analyzer;
            for name in (*this).module.globals.iter() {
                let g = (*this).symbol_table.global_scope();
                let decl = g.get(name).unwrap();
                if decl.visited { continue; }
                self.analyse_declaration(&decl.declaration)?;
            }
        }

        Ok(())
    }

    fn analyse_declaration(&mut self, decl: &ast::Declaration) -> AnalyzerResult<symbol::Type> {
        match decl {
            ast::Declaration::Constant(constant_decl) => self.analyse_constant(constant_decl),
            ast::Declaration::Variable(variable_decl) => self.analyse_variable(variable_decl),
        }
    }

    fn analyse_constant(&mut self, cons: &ast::ConstantDecl) -> AnalyzerResult<symbol::Type> {
        let name = self.reporter.get_lexem(&cons.identifier).to_string();

        let expected_type = match &cons.tp {
            Some(tp) => self.analyse_type(&tp)?,
            None     => symbol::Type::Auto,
        };

        let value_type = match &cons.value {
            Some(expr) => Some(self.analyse_expr(&expr)?),
            None => None,
        };

        if expected_type == symbol::Type::Auto {
            if value_type.is_none() {
                self.reporter.report_error_at_token(
                    "Can't determin the type.".into(),
                    &cons.identifier
                );
                return Err(());
            }
            return Ok(value_type.unwrap());
        }

        let value_type = value_type.unwrap();
        if !expected_type.matches(&value_type) {
            self.reporter
                .build(ReportKind::Error, cons.identifier.span.clone())
                .with_file_path(self.reporter.path.clone().unwrap().to_string_lossy())
                .with_message("Type mismatch")
                .with_label(
                    Label::new(cons.identifier.span.clone())
                        .with_msg("Expected something else.")
                )
                .print();
            return Err(());
        }

        let mut symbol = self.symbol_table.find_local_first_mut(name.clone());
        if symbol.is_none() {
            self.symbol_table.define_local(name, Symbol {
                kind: SymbolKind::Typed(value_type.clone()),
                declaration: Rc::new(ast::Declaration::Constant(cons.clone())),
                constant: true,
                visited: true,
            });
        } else {
            let symbol = symbol.unwrap();
            if !symbol.is_declared() && symbol.visited {
                self.reporter.report_error_at_token(
                    "Constant is already declared.".into(),
                    &cons.identifier
                );
                return Err(());
            }

            symbol.constant = true;
            symbol.visited = true;
            symbol.kind = SymbolKind::Typed(value_type.clone());
        }

        Ok(value_type)
    }

    fn analyse_variable(&mut self, var: &ast::VariableDecl) -> AnalyzerResult<symbol::Type> {
        let name = self.reporter.get_lexem(&var.identifier).to_string();

        let expected_type = match &var.tp {
            Some(tp) => self.analyse_type(&tp)?,
            None     => symbol::Type::Auto,
        };

        let value_type = match &var.value {
            Some(expr) => Some(self.analyse_expr(&expr)?),
            None => None,
        };

        if expected_type == symbol::Type::Auto {
            if value_type.is_none() {
                self.reporter.report_error_at_token(
                    "Can't determin the type.".into(),
                    &var.identifier
                );
                return Err(());
            }
            return Ok(value_type.unwrap());
        }

        let value_type = value_type.unwrap();
        if !expected_type.matches(&value_type) {
            self.reporter.report_error_at_token(
                "Type Mismatch.".into(),
                &var.identifier
            );
            return Err(());
        }

        let mut symbol = self.symbol_table.find_local_first_mut(name.clone());
        if symbol.is_none() {
            self.symbol_table.define_local(name, Symbol {
                kind: SymbolKind::Typed(value_type.clone()),
                declaration: Rc::new(ast::Declaration::Variable(var.clone())),
                constant: false,
                visited: true,
            });
        } else {
            let symbol = symbol.unwrap();
            if !symbol.is_declared() {
                self.reporter.report_error_at_token(
                    "Variable is already declared.".into(),
                    &var.identifier
                );
                return Err(());
            }

            symbol.constant = false;
            symbol.kind = SymbolKind::Typed(value_type.clone());
        }

        Ok(value_type)
    }

    fn analyse_type(&mut self, tp: &ast::Type) -> AnalyzerResult<symbol::Type> {
        match tp {
            ast::Type::Auto => Ok(symbol::Type::Auto),
            ast::Type::Int => Ok(symbol::Type::Int),
            ast::Type::Float => Ok(symbol::Type::Float),
        }
    }

    fn analyse_expr(&mut self, expr: &ast::Expr) -> AnalyzerResult<symbol::Type> {
        match expr {
            ast::Expr::Int(token) => Ok(symbol::Type::Int),
            ast::Expr::Float(token) => Ok(symbol::Type::Float),
            ast::Expr::Identifier(token) => self.analyse_identifier(token.clone()),
            ast::Expr::Unary(unary_expr) => todo!(),
            ast::Expr::Binary(binary_expr) => todo!(),
            ast::Expr::Grouping(expr) => todo!(),
        }
    }

    fn analyse_identifier(&mut self, token: Token) -> AnalyzerResult<symbol::Type> {
        let x = self.reporter.get_lexem(&token).to_string();

        let Some(symb) = self.symbol_table.find_local_first(x) else {
            return Err(());
        };

        match &symb.kind {
            // TODO: mark it as visited
            SymbolKind::Untyped => self.analyse_declaration(&symb.declaration.clone()),
            SymbolKind::Typed(t) => Ok(t.clone()),
        }
    }
}
