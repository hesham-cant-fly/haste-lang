use std::mem::transmute_copy;

use crate::reporter::Reporter;
use crate::token::{Token, TokenKind};
use crate::ast::{self, ASTFile};

pub fn parse_tokens<'a>(token_stream: Vec<Token>, reporter: &'a Reporter) -> ParserResult<ASTFile> {
    let declarations = Parser::new(token_stream, reporter).parse()?;
    Ok(ASTFile { declarations })
}

#[repr(u8)]
#[derive(Debug, PartialOrd, Ord, PartialEq, Eq)]
enum Precedence {
    None = 0,
    Term,
    Factor,
    Power,
    Unary,
    Primary,
}

impl Precedence {
    fn lowest() -> Self { Precedence::Term }
}

impl Into<u8> for Precedence {
    fn into(self) -> u8 { self as u8 }
}

impl From<u8> for Precedence {
    fn from(value: u8) -> Self {
        unsafe { transmute_copy(&value) }
    }
}

type PrefixFn<'a> = fn(&mut Parser<'a>) -> ParserResult<ast::Expr>;
type InfixFn<'a> = fn(&mut Parser<'a>, ast::Expr) -> ParserResult<ast::Expr>;

#[derive(Debug)]
struct Rule<'a> {
    pub prefix: Option<PrefixFn<'a>>,
    pub infix: Option<InfixFn<'a>>,
    pub prec: Precedence,
    pub right_assoc: bool,
}

macro_rules! make_rule {
    ($pre:expr, $inf:expr, $prec:expr, $aso:expr) => {
        Rule {
            prefix: $pre,
            infix: $inf,
            prec: $prec,
            right_assoc: $aso,
        }
    };
}

#[derive(Debug)]
struct Parser<'a> {
    pub tokens: Vec<Token>,
    pub reporter: &'a Reporter,
    pub current: usize,
    pub has_error: bool,
}

type ParserResult<T> = Result<T, ()>;

impl<'a> Parser<'a> {
    pub fn new(tokens: Vec<Token>, reporter: &'a Reporter) -> Self {
        Parser {
            tokens,
            reporter,
            current: 0,
            has_error: false,
        }
    }

    pub fn create<T>(&mut self, val: T) -> Box<T> {
        Box::new(val)
    }

    fn parse(&mut self) -> ParserResult<ast::DeclarationList> {
        let mut declarations = ast::DeclarationList::new();
        while !self.eof() {
            let Ok(decl) = self.parse_declaration() else {
                self.sync();
                continue;
            };
            declarations.push(decl);
        }

        if !self.has_error {
            Ok(declarations)
        } else {
            Err(())
        }
    }

    fn sync(&mut self) {
        loop {
            match self.peek().kind {
                TokenKind::Semicolon => {
                    _ = self.advance();
                    break;
                }
                TokenKind::Const
                | TokenKind::Var => break,
                _ => _ = self.advance(),
            }
        }
    }

    // Type
    fn parse_type(&mut self) -> ParserResult<ast::Type> {
        if self.eof() {
            return self.report_error("Early EOF.".into(), &self.previous());
        }

        use ast::Type;
        let token = self.advance();

        match token.kind {
            TokenKind::Auto => Ok(Type::Auto),
            TokenKind::Int => Ok(Type::Int),
            TokenKind::Float => Ok(Type::Float),
            _ => self.report_error(
                "Expected a type.".into(), &token
            ),
        }
    }

    // Declaration
    fn parse_declaration(&mut self) -> ParserResult<ast::Declaration> {
        if self.eof() {
            return self.report_error("Early EOF (more likely to be a bug from the compiler itself)".into(), &self.peek());
        }

        let token = self.advance();
        match token.kind {
            TokenKind::Const => self.parse_constant(),
            TokenKind::Var => self.parse_var(),
            _ => self.report_error(
                "Invalid declaration, expected `const` or `var`.".into(),
                &token
            ),
        }
    }

    fn parse_constant(&mut self) -> ParserResult<ast::Declaration> {
        let identifier = self.consume(TokenKind::Identifier, "Expected a name.".into())?;
        let tp: Option<ast::Type> = if self.matches(TokenKind::Colon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        let value: Option<ast::Expr> = if self.matches(TokenKind::Equal) {
            Some(self.parse_expr()?)
        } else {
            None
        };

        self.consume(TokenKind::Semicolon, "Expected `;`".into())?;
        Ok(
            ast::Declaration::Constant(
                ast::ConstantDecl {
                    identifier,
                    tp,
                    value
                }
            )
        )
    }

    fn parse_var(&mut self) -> ParserResult<ast::Declaration> {
        let identifier = self.consume(TokenKind::Identifier, "Expected a name.".into())?;
        let tp: Option<ast::Type> = if self.matches(TokenKind::Colon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        let value: Option<ast::Expr> = if self.matches(TokenKind::Equal) {
            Some(self.parse_expr()?)
        } else {
            None
        };

        self.consume(TokenKind::Semicolon, "Expected `;`".into());
        Ok(
            ast::Declaration::Variable(
                ast::VariableDecl {
                    identifier,
                    tp,
                    value
                }
            )
        )
    }

    // Expression
    fn parse_expr(&mut self) -> ParserResult<ast::Expr> {
        self.parse_precedence(Precedence::lowest())
    }

    fn parse_precedence(&mut self, prec: Precedence) -> ParserResult<ast::Expr> {
        if self.eof() {
            return self.report_error(
                format!("Incomplete expression. (Early EOF)"), &self.previous()
            );
        }

        let token = self.advance();
        let Some(prefix_rule) = Self::get_rule(token.kind.clone()).prefix else {
            return self.report_error(
                format!("Invalid prefix token: {}.", self.reporter.get_lexem(&token)),
                &token,
            )
        };

        let mut lhs = prefix_rule(self)?;

        while !self.ended() {
            let token = self.peek();
            let rule = Self::get_rule(token.kind.clone());
            if prec > rule.prec {
                break;
            }

            _ = self.advance();
            let Some(infix_rule) = rule.infix else {
                return self.report_error(
                    format!("Invalid infix token: {}.", self.reporter.get_lexem(&token)),
                    &token,
                )
            };

            lhs = infix_rule(self, lhs)?;
        }

        Ok(lhs)
    }

    fn get_rule(kind: TokenKind) -> Rule<'a> {
        match kind {
            TokenKind::Plus       => make_rule!(Some(Self::parse_unary),      Some(Self::parse_binary), Precedence::Term,    false),
            TokenKind::Minus      => make_rule!(Some(Self::parse_unary),      Some(Self::parse_binary), Precedence::Term,    false),
            TokenKind::Star       => make_rule!(None,                         Some(Self::parse_binary), Precedence::Factor,  false),
            TokenKind::FSlash     => make_rule!(None,                         Some(Self::parse_binary), Precedence::Factor,  false),
            TokenKind::DoubleStar => make_rule!(None,                         Some(Self::parse_binary), Precedence::Power,   true ),
            TokenKind::OpenParen  => make_rule!(Some(Self::parse_grouping),   None,                     Precedence::None,    false),
            TokenKind::IntLit     => make_rule!(Some(Self::parse_int),        None,                     Precedence::None,    false),
            TokenKind::FloatLit   => make_rule!(Some(Self::parse_float),      None,                     Precedence::None,    false),
            TokenKind::Identifier => make_rule!(Some(Self::parse_identifier), None,                     Precedence::None,    false),
            _                     => make_rule!(None,                         None,                     Precedence::None,    false),
        }
    }

    fn parse_identifier(&mut self) -> ParserResult<ast::Expr> {
        let token = self.previous();
        Ok(ast::Expr::Identifier(token))
    }

    fn parse_float(&mut self) -> ParserResult<ast::Expr> {
        let token = self.previous();
        Ok(ast::Expr::Float(token))
    }

    fn parse_int(&mut self) -> ParserResult<ast::Expr> {
        let token = self.previous();
        Ok(ast::Expr::Int(token))
    }

    fn parse_grouping(&mut self) -> ParserResult<ast::Expr> {
        let expr = self.parse_expr()?;
        self.consume(
            TokenKind::CloseParen,
            "Expected ')'.".into(),
        )?;
        Ok(
            ast::Expr::Grouping(
                self.create(expr)
            )
        )
    }

    fn parse_unary(&mut self) -> ParserResult<ast::Expr> {
        let op = self.previous();
        if self.eof() {
            return self.report_error("Expected and expression, found EOF.".into(), &op);
        }
        let rhs = self.parse_precedence(Precedence::Unary)?;
        Ok(
            ast::Expr::Unary(
                ast::UnaryExpr {
                    op,
                    rhs: self.create(rhs),
                }
            )
        )
    }

    fn parse_binary(&mut self, lhs: ast::Expr) -> ParserResult<ast::Expr> {
        let op = self.previous();
        let rule = Self::get_rule(op.kind.clone());
        let precedence = rule.prec as u8 + if rule.right_assoc { 1 } else { 0 };

        let rhs = self.parse_precedence(precedence.into())?;
        Ok(
            ast::Expr::Binary(
                ast::BinaryExpr{
                    op,
                    lhs: self.create(lhs),
                    rhs: self.create(rhs),
                }
            )
        )
    }

    fn peek(&self) -> Token { self.tokens[self.current].clone() }
    fn previous(&self) -> Token { self.tokens[self.current - 1].clone() }
    fn advance(&mut self) -> Token {
        if !self.ended() {
            self.current += 1;
        }
        self.previous()
    }

    fn check(&self, kind: TokenKind) -> bool {
        if self.ended() {
            return false;
        }
        self.peek().kind == kind
    }

    fn matches(&mut self, kind: TokenKind) -> bool {
        if self.check(kind) {
            _ = self.advance();
            return true;
        }
        return false;
    }

    fn ended(&self) -> bool {
        self.current >= self.tokens.len()
    }

    fn eof(&self) -> bool {
        self.peek().kind == TokenKind::EOF
    }

    fn consume(&mut self, kind: TokenKind, message: String) -> ParserResult<Token> {
        if self.matches(kind) {
            return Ok(self.previous());
        }
        let token = if self.eof() {
            self.previous()
        } else {
            self.peek()
        };
        self.report_error(message, &token)
    }

    fn report_error<T>(&mut self, message: String, at: &Token) -> ParserResult<T> {
        self.has_error = true;
        self.reporter.report_error_at_token(message, at);
        Err(())
    }
}
