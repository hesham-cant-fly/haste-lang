use proc_macro::TokenStream;
use quote::quote;
use syn::{Data, DeriveInput};

fn generate_from_trait(ast: DeriveInput) -> TokenStream {
    let Data::Struct(struct_data) = ast.data else {
        panic!("Only supports structs.");
    };

    let struct_name = ast.ident;
    let fields_names: Vec<syn::Ident> = (&struct_data.fields)
        .into_iter()
        .map(|f| {
            f.ident.clone().unwrap()
        })
        .collect();
    let fields_types: Vec<syn::Type> = (&struct_data.fields)
        .into_iter()
        .map(|t| t.ty.clone())
        .collect();

    quote! {
        impl From<(#(#fields_types),*)> for #struct_name {
            fn from(value: (#(#fields_types),*)) -> Self {
                let (#(#fields_names),*) = value;
                Self {
                    #(#fields_names),*
                }
            }
        }
    }.into()
}

#[proc_macro_derive(FromTuple)]
pub fn from_tuple_derive(input: TokenStream) -> TokenStream {
    let ast = syn::parse(input).unwrap();

    generate_from_trait(ast)
}
