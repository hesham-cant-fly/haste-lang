#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Int,
    Float,
    Auto,
}

impl Type {
    pub fn matches(&self, other: &Type) -> bool {
        self == other
    }
}
