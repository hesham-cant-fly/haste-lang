use std::{
    cell::RefCell, collections::HashMap, fs::File, io::{self, Read}, rc::Rc
};

use crate::{
    reporter::{ReportBuilder, ReportKind},
    token::Span,
};

#[derive(Debug, Default)]
pub struct FSPool {
    files: HashMap<String, Rc<String>>,
}

impl FSPool {
    pub fn new() -> Rc<RefCell<Self>> {
        Rc::new(Self {
            ..Default::default()
        }.into())
    }

    pub fn load_file(&mut self, path: impl Into<String>) -> io::Result<Rc<String>> {
        let path: String = path.into();
        if let Some(src) = self.get(&path) {
            return Ok(Rc::clone(&src));
        }

        let mut f = File::open(&path)?;
        let mut src = String::new();
        f.read_to_string(&mut src)?;

        let src = Rc::new(src);
        self.files.insert(path, Rc::clone(&src));

        Ok(Rc::clone(&src))
    }

    pub fn get(&self, path: &str) -> Option<Rc<String>> {
        let Some(rc) = self.files.get(path) else {
            return None;
        };
        Some(Rc::clone(rc))
    }

    pub fn build_report(
        &self,
        path: &str,
        kind: ReportKind,
        span: Span,
    ) -> Option<ReportBuilder> {
        let src = self.get(&path)?;
        Some(ReportBuilder {
            file_path: path.to_string(),
            file_content: src,
            kind,
            span,
            msg: None,
            labels: vec![],
        })
    }
}
