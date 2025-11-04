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
    tail: Option<String>,
}

impl Label {
    pub fn new(span: Span) -> Self {
        Self {
            span,
            msg: None,
            caret: "^".into(),
            tail: Some("----".into()),
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

    pub fn with_tail<M: ToString>(mut self, tail: M) -> Self {
        self.tail = Some(tail.to_string());
        self
    }

    pub fn with_no_tail<M: ToString>(mut self, tail: M) -> Self {
        self.tail = None;
        self
    }
}

#[derive(Debug)]
pub struct ReportBuilder {
    pub span: Span,
    pub file_path: String,
    pub file_content: Rc<String>,
    pub kind: ReportKind,
    pub msg: Option<String>,
    pub labels: Vec<Label>,
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
            eprintln!(
                "{}{} {}",
                label.caret,
                label.tail.clone().unwrap_or("".to_string()),
                &label.msg.clone().unwrap_or("".into())
            );
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
