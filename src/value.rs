use core::fmt;

#[derive(Debug)]
pub enum HasteValue {
    UntypedInt(i64),
    UntypedFloat(f64),
    Int(i64),
    Float(f64),
    Type(HasteType),
}

impl HasteValue {
    pub fn get_type(&self) -> HasteType {
        match self {
            HasteValue::UntypedInt(_) => HasteType::UntypedInt,
            HasteValue::UntypedFloat(_) => HasteType::UntypedFloat,
            HasteValue::Int(_) => HasteType::Int,
            HasteValue::Float(_) => HasteType::Float,
            HasteValue::Type(_) => HasteType::Type,
        }
    }
}

#[derive(Debug, Clone)]
pub enum HasteType {
    UntypedInt,
    UntypedFloat,
    Int,
    Float,
    Auto,
    Type,
}

impl fmt::Display for HasteType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HasteType::UntypedInt => write!(f, "untyped_int"),
            HasteType::UntypedFloat => write!(f, "untyped_float"),
            HasteType::Int => write!(f, "int"),
            HasteType::Float => write!(f, "float"),
            HasteType::Auto => write!(f, "auto"),
            HasteType::Type => write!(f, "type"),
        }
    }
}
