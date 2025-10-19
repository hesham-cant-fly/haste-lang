use core::fmt;
use std::{path::PathBuf, rc::Rc};

use colored::Colorize;

use crate::token::{Span, Token};

#[derive(Debug)]
pub enum ReportKind {
    Error,
    Warning,
    Custom(String),
}

impl fmt::Display for ReportKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ReportKind::Error => write!(f, "Error"),
            ReportKind::Warning => write!(f, "Warning"),
            ReportKind::Custom(v) => write!(f, "{}", v),
        }
    }
}

#[derive(Debug)]
pub struct Label {
    span: Span,
    msg: Option<String>,
    caret: String,
}

impl Label {
    pub fn new(span: Span) -> Self {
        Self {
            span,
            msg: None,
            caret: "^".into()
        }
    }

    pub fn with_msg<M: ToString>(mut self, msg: M) -> Self {
        self.msg = Some(msg.to_string());
        self
    }

    pub fn with_caret<M: ToString>(mut self, caret: M) -> Self {
        self.caret = caret.to_string();
        self
    }
}

#[derive(Debug)]
pub struct ReportBuilder {
    span: Span,
    file_path: String,
    file_content: Rc<String>,
    kind: ReportKind,
    msg: Option<String>,
    labels: Vec<Label>,
}

impl ReportBuilder {
    pub fn with_file_path<M: ToString>(mut self, path: M) -> Self {
        self.file_path = path.to_string();
        self
    }

    pub fn with_message<M: ToString>(mut self, msg: M) -> Self {
        self.msg = Some(msg.to_string());
        self
    }

    pub fn with_label(mut self, label: Label) -> Self {
        self.labels.push(label);
        self
    }

    pub fn print(&self) {
        let (line_num, column) = self.get_pos_index(self.span.start);

        eprintln!(
            "{}: {}: {}",
            format!("{}:{line_num}:{column}", self.file_path).underline(),
            "Error".red().italic(),
            self.msg.clone().unwrap_or("".into()).white().bold(),
        );

        let line = self.get_line_at(self.span.start);
        eprintln!("{line_num:5} | {line}");

        for label in self.labels.iter() {
            let (line_num, column) = self.get_pos_index(self.span.start);
            eprint!("      | ");
            for _ in 1..column {
                eprint!(" ");
            }
            eprintln!("{} {}", label.caret, &label.msg.clone().unwrap_or("".into()));
        }
    }

    pub fn get_line_at(&self, at: usize) -> &str {
        let bytes = self.file_content.as_bytes();
        let mut start_pos = at;
        let mut end_pos = at;

        while start_pos > 0 {
            if bytes[start_pos - 1] == b'\n' {
                break;
            }
            start_pos -= 1;
        }

        while end_pos < bytes.len() {
            if bytes[end_pos] == b'\n' {
                break;
            }
            end_pos += 1;
        }

        unsafe { str::from_utf8_unchecked(&bytes[start_pos..end_pos]) }
    }

    pub fn get_lexem(&self, token: &Token) -> &str {
        let span = &token.span;
        &self.file_content[span.start..span.end]
    }

    pub fn get_pos_index(&self, at: usize) -> (usize, usize) {
        let mut line = 1;
        let mut column = 1;
        let mut last_nl = 0;

        for (idx, ch) in self.file_content.char_indices() {
            if idx == at {
                return (line, column);
            }
            if ch == '\n' {
                line += 1;
                column = 1;
                last_nl = idx + 1;
            } else {
                column += 1;
            }
        }

        if at >= self.file_content.len() {
            (line, column.saturating_sub(1))
        } else {
            (line, column)
        }
    }

    pub fn get_pos_token(&self, token: &Token) -> (usize, usize) {
        self.get_pos_index(token.span.start)
    }
}

#[derive(Debug)]
pub struct Reporter {
    pub path: Option<PathBuf>,
    pub src: String,
}

impl Reporter {
    pub fn new(src: String, path: Option<PathBuf>) -> Self {
        Self { src, path }
    }

    pub fn build(&self, kind: ReportKind, span: Span) -> ReportBuilder {
        ReportBuilder {
            file_path: String::new(),
            // TODO: cloning here is bad
            file_content: Rc::new(self.src.clone()),
            kind, span,
            msg: None,
            labels: vec![],
        }
    }

    pub fn report_error_at(&self, message: String, at: usize) {
        let (line_num, column) = self.get_pos_index(at);
        let binding = self.path.clone().unwrap_or("(BUFFERED)".into());
        let path = binding.to_str().unwrap_or("[INVALID-UNICODE]");

        eprintln!(
            "{}: {}: {}",
            format!("{}:{line_num}:{column}", path).underline(),
            "Error".red().italic(),
            message.white().bold(),
        );

        let line = self.get_line_at(at);
        eprintln!("{line_num:5} | {line}");

        eprint!("      | ");
        for _ in 1..column {
            eprint!(" ");
        }
        eprintln!("^");
    }

    pub fn report_error_at_token(&self, message: String, at: &Token) {
        self.report_error_at(message, at.span.start);
    }

    pub fn get_line_at(&self, at: usize) -> &str {
        let bytes = self.src.as_bytes();
        let mut start_pos = at;
        let mut end_pos = at;

        while start_pos > 0 {
            if bytes[start_pos - 1] == b'\n' {
                break;
            }
            start_pos -= 1;
        }

        while end_pos < bytes.len() {
            if bytes[end_pos] == b'\n' {
                break;
            }
            end_pos += 1;
        }

        unsafe { str::from_utf8_unchecked(&bytes[start_pos..end_pos]) }
    }

    pub fn get_lexem(&self, token: &Token) -> &str {
        let span = &token.span;
        &self.src[span.start..span.end]
    }

    pub fn get_pos_index(&self, at: usize) -> (usize, usize) {
        let mut line = 1;
        let mut column = 1;
        let mut last_nl = 0;

        for (idx, ch) in self.src.char_indices() {
            if idx == at {
                return (line, column);
            }
            if ch == '\n' {
                line += 1;
                column = 1;
                last_nl = idx + 1;
            } else {
                column += 1;
            }
        }

        if at >= self.src.len() {
            (line, column.saturating_sub(1))
        } else {
            (line, column)
        }
    }

    pub fn get_pos_token(&self, token: &Token) -> (usize, usize) {
        self.get_pos_index(token.span.start)
    }
}
