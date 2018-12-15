//! This crate provides bindings for the [ExprTk](http://www.partow.net/programming/exprtk/index.html)
//! library.
//!
//! For an overview of the data structures [see the ExprTk main page](http://www.partow.net/programming/exprtk/index.html).
//! While `exprtk-sys` maps most functions of the library to Rust, the high level bindings
//! were considerably simplified. Each [Expression](struct.Expression.html) owns a
//! [SymbolTable](struct.SymbolTable.html), they cannot be shared between different instances,
//! and multiple symbol tables per expression are not possible.
//! Variables are owned by the `SymbolTable` instance. [add_variable()](exprtk/struct.SymbolTable.html#method.add_variable)
//! returns an `usize`, which is a _variable ID_. This ID can be used to later modify the value
//! Using [set_value()](exprtk/struct.SymbolTable.html#method.set_value). The same is true for
//! string and vector variables.
//!
//! There may be a more idiomatic way to represent the whole API in Rust, but it seems difficult to
//! me to integrate with Rust's concepts of lifetimes and mutable/immutable borrowing.
//! Suggestions are of course welcome.
//!
//! Since there is no guarantee that `double` is always `f64`, the `c_double` type is used all
//! over the library. Other precisions are currently not supported.
//!
//! # Examples:
//!
//! This code corresponds to the [example 1](http://www.partow.net/programming/exprtk/index.html#simpleexample01)
//! in the ExprTk documentation:
//!
//! ```no_run
//! use exprtk_rs::*;
//!
//! let expression_string = "clamp(-1.0,sin(2 * pi * x) + cos(x / 2 * pi),+1.0)";
//!
//! let mut symbol_table = SymbolTable::new();
//! symbol_table.add_constants();
//! let var_id = symbol_table.add_variable("x", 0.).unwrap().unwrap();
//!
//! let mut expression = Expression::new(expression_string, symbol_table).unwrap();
//!
//! // this value is a reference to a std::cell::Cell that can be changed
//! let value = expression.symbols().value(var_id).unwrap();
//! value.set(-5.);
//!
//! while value.get() <= 5. {
//!     let y = expression.value();
//!     println!("{}\t{}", value.get(), y);
//!     value.set(value.get() + 0.001);
//! }
//! ```
//!
//! # Unknown variables
//!
//! Unknown variables encountered in an expression can be automatically added to the symbol table.
//! The function `Expression::parse_vars` will return a `Vec` containing the newly added variable
//! names and their variable IDs.
//! This works only for regular variables, not for strings or vectors.
//!
//! ```
//! use exprtk_rs::*;
//!
//! let expr_string = "a*x^2 + b*x + c";
//!
//! let (mut expr, unknown_vars) = Expression::parse_vars(expr_string, SymbolTable::new()).unwrap();
//!
//! assert_eq!(
//!     unknown_vars,
//!     vec![("a".to_string(), 0), ("x".to_string(), 1), ("b".to_string(), 2), ("c".to_string(), 3)]
//! );
//!
//! // modify the values
//! expr.symbols().value(0).unwrap().set(2.); // a
//! expr.symbols().value(2).unwrap().set(3.); // b
//! expr.symbols().value(3).unwrap().set(1.); // c
//! expr.symbols().value(1).unwrap().set(5.); // x
//!
//! assert_eq!(expr.value(), 66.);
//! ```
//!
//! # Strings
//!
//! String variables are not UTF-8 encoded like in Rust, but byte strings. They are
//! still called 'string' variables in the API.
//!
//! ```
//! use exprtk_rs::*;
//!
//! let mut symbol_table = SymbolTable::new();
//! let s1_id = symbol_table.add_stringvar("s1", b"Hello").unwrap().unwrap();
//! let s2_id = symbol_table.add_stringvar("s2",  b"world!").unwrap().unwrap();
//!
//! // concatenation
//! let mut expr = Expression::new("s1 + ' ' + s2 == 'Hello world!'", symbol_table).unwrap();
//! // a boolean `true` is represented by `1`
//! assert_eq!(expr.value(), 1.);
//!
//! // Modifying a string
//! expr.symbols_mut().set_string(s1_id, b"What a");
//! assert_eq!(expr.value(), 0.);
//! ```
//!
//! # Functions
//!
//! There is currently the possibility to add functions/closures with up to four scalar arguments.
//! Example:
//!
//! ```
//! use exprtk_rs::*;
//!
//! let mut symbol_table = SymbolTable::new();
//! symbol_table.add_func2("add", |x, y| x + y);
//! symbol_table.add_variable("x", 1.).unwrap();
//!
//! let mut expr = Expression::new("add(x, 1)", symbol_table).unwrap();
//! assert_eq!(expr.value(), 2.);
//! ```

#[macro_use] extern crate enum_primitive;
extern crate exprtk_sys;
extern crate libc;

pub use libc::c_double;
pub use exprtk::*;
pub use error::*;

mod exprtk;
mod error;

#[cfg(test)]
mod tests;
