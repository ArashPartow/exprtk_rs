#include "exprtk.hpp"


// helpers

char *string_to_cstr(const std::string &s) {
  char *pc = new char[s.size() + 1];
  std::strcpy(pc, s.c_str());
  return pc;
}

struct cstr_list {
  size_t size;
  char **elements;
};

cstr_list *strings_to_cstr_list(const std::vector<std::string> &v) {
  cstr_list *out = new cstr_list;
  out->size = v.size();
  out->elements = new char *[out->size];

  for (size_t i = 0; i < out->size; i++) {
    out->elements[i] = string_to_cstr(v[i]);
  }

  return out;
}

extern "C" void free_rust_cstring(char * s);

// for resolving unknown variables
template <typename T>
struct symbol_resolver : exprtk::parser<T>::unknown_symbol_resolver
{
  typedef typename exprtk::parser<T>::unknown_symbol_resolver usr_t;
  char * (*callback)(const char *, void *);
  void *user_data;

  symbol_resolver(char * (*cb)(const char *, void *), void *d)
  : usr_t(exprtk::parser<T>::unknown_symbol_resolver::e_usrmode_extended)
  {
    callback = cb;
    user_data = d;
  }

  virtual bool process(const std::string& unknown_symbol,
                       exprtk::symbol_table<T>& symbol_table,
                       std::string&        error_message)
  {

    // in bindings, only one symbol table is allowed per expression
    // -> simplify things by ignoring this parameter
    (void)symbol_table;

    char * msg = (*callback)(unknown_symbol.c_str(), user_data);

    if (msg != NULL) {
      error_message = std::string(msg);
      free_rust_cstring(msg);
      return false;
    }

    return true;
  }
};

// Functions (not sure if there is a way to avoid repetition)

template <typename T> struct var1_func : public exprtk::ifunction<T> {
  T (*cb)(void *, T);
  void *user_data;
  var1_func(T (*c)(void *, T), void *d) : exprtk::ifunction<T>(1) {
    cb = c;
    user_data = d;
  }
  T operator()(const T &a) { return cb(user_data, a); }
};

template <typename T> struct var2_func : public exprtk::ifunction<T> {
  T (*cb)(void *, T, T);
  void *user_data;
  var2_func(T (*c)(void *, T, T), void *d) : exprtk::ifunction<T>(2) {
    cb = c;
    user_data = d;
  }
  T operator()(const T &a, const T &b) { return cb(user_data, a, b); }
};

template <typename T> struct var3_func : public exprtk::ifunction<T> {
  T (*cb)(void *, T, T, T);
  void *user_data;
  var3_func(T (*c)(void *, T, T, T), void *d) : exprtk::ifunction<T>(3) {
    cb = c;
    user_data = d;
  }
  T operator()(const T &a, const T &b, const T &c) {
    return cb(user_data, a, b, c);
  }
};

template <typename T> struct var4_func : public exprtk::ifunction<T> {
  T (*cb)(void *, T, T, T, T);
  void *user_data;
  var4_func(T (*c)(void *, T, T, T, T), void *d) : exprtk::ifunction<T>(4) {
    cb = c;
    user_data = d;
  }
  T operator()(const T &a, const T &b, const T &c, const T &d) {
    return cb(user_data, a, b, c, d);
  }
};


// these methods don't depend on a specific precision

extern "C" {
typedef exprtk::parser<double> Parser;
typedef symbol_resolver<double> UnknownSymbolResolver;
typedef exprtk::symbol_table<double> SymbolTable;
typedef exprtk::expression<double> Expression;

// Parser

Parser *parser_new() { return new Parser; }

void parser_destroy(Parser *p) { delete p; }

bool parser_compile(Parser *p, const char *s, Expression *e) {
  return p->compile((const std::string &)s, *e);
}

bool parser_compile_resolve(Parser *p, const char *s, Expression *e,
    char * (*cb)(const char *, void *), void *user_data) {

  UnknownSymbolResolver resolver(cb, user_data);

  p->enable_unknown_symbol_resolver(&resolver);

  bool ok = p->compile((const std::string &)s, *e);

  p->disable_unknown_symbol_resolver();

  return ok;
}

struct parser_err {
  bool is_err;
  int mode;
  const char *token_type;
  const char *token_value;
  // const char* mode;
  const char *diagnostic;
  const char *error_line;
  size_t line_no;
  size_t column_no;
};

parser_err *parser_error(Parser *p) {
  // TODO: it seems p->get_error(0) creates a copy of the error (why?)
  // therefore we have to heap allocate the output
  parser_err *out = new parser_err;
  if (p->error_count() > 0) {
    out->is_err = true;
    exprtk::parser_error::type err = p->get_error(0);
    out->mode = err.mode;
    out->token_type =
        string_to_cstr(exprtk::lexer::token::to_str(err.token.type));
    out->token_value = string_to_cstr(err.token.value);
    out->diagnostic = string_to_cstr(err.diagnostic);
    out->error_line = string_to_cstr(err.error_line);
    out->line_no = err.line_no;
    out->column_no = err.column_no;
  }
  return out;
}

void parser_error_free(parser_err *e) {
  delete[] e->token_type;
  delete[] e->token_value;
  delete[] e->diagnostic;
  delete[] e->error_line;
  delete e;
}

// String values: Rust cannot deal with C++ strings by itself

std::string *cpp_string_create(const char *s, size_t len) {
  return new std::string(s, len);
}

void cpp_string_set(std::string *s, const char *replacement, size_t len) {
  s->assign(replacement, len);
}

const char *cpp_string_get(const std::string *s) { return s->c_str(); }

void cpp_string_free(std::string *s) { delete s; }

// Symbol table

SymbolTable *symbol_table_new() { return new SymbolTable; }

void symbol_table_destroy(SymbolTable *t) { delete t; }

bool symbol_table_add_variable(SymbolTable *t, char *variable_name,
                               double *value, bool is_constant = false) {
  return t->add_variable(std::string(variable_name), *value, is_constant);
}

bool symbol_table_create_variable(SymbolTable *t, char *variable_name,
                                  const double value) {
  return t->create_variable(std::string(variable_name), value);
}

bool symbol_table_add_constant(SymbolTable *t, char *variable_name,
                               const double value) {
  return t->add_constant(std::string(variable_name), value);
}

bool symbol_table_add_stringvar(SymbolTable *t, char *variable_name,
                                std::string *string, bool is_const) {
  return t->add_stringvar(std::string(variable_name), *string, is_const);
}

bool symbol_table_create_stringvar(SymbolTable *t, char *variable_name,
                                   char *string) {
  return t->create_stringvar(std::string(variable_name), std::string(string));
}

bool symbol_table_add_vector(SymbolTable *t, char *name, double *vec,
                             const size_t len) {
  return t->add_vector(std::string(name), vec, len);
}

bool symbol_table_remove_variable(SymbolTable *t, char *name) {
  return t->remove_variable(std::string(name), true);
}

bool symbol_table_remove_stringvar(SymbolTable *t, char *name) {
  return t->remove_stringvar(std::string(name));
}

bool symbol_table_remove_vector(SymbolTable *t, char *name) {
  return t->remove_vector(std::string(name));
}

void symbol_table_clear_variables(SymbolTable *t) { t->clear_variables(true); }

void symbol_table_clear_strings(SymbolTable *t) { t->clear_strings(); }

void symbol_table_clear_vectors(SymbolTable *t) { t->clear_vectors(); }

void symbol_table_clear_local_constants(SymbolTable *t) {
  t->clear_local_constants();
}

void symbol_table_clear_functions(SymbolTable *t) { t->clear_functions(); }

double *symbol_table_variable_ref(SymbolTable *t, char *variable_name) {
  return &t->variable_ref(std::string(variable_name));
}

std::string *symbol_table_stringvar_ref(SymbolTable *t, char *variable_name) {
  return &t->stringvar_ref(std::string(variable_name));
}

const double *symbol_table_vector_ptr(SymbolTable *t, char *variable_name) {
  exprtk::symbol_table<double>::vector_holder_ptr v =
      t->get_vector(std::string(variable_name));
  if (v != NULL) {
    return (double *)v->data();
  } else {
    return NULL;
  }
}

size_t symbol_table_variable_count(SymbolTable *t) {
  return t->variable_count();
}

size_t symbol_table_stringvar_count(SymbolTable *t) {
  return t->stringvar_count();
}

size_t symbol_table_vector_count(SymbolTable *t) { return t->vector_count(); }

size_t symbol_table_function_count(SymbolTable *t) {
  return t->function_count();
}

bool symbol_table_add_constants(SymbolTable *t) { return t->add_constants(); }

bool symbol_table_add_pi(SymbolTable *t) { return t->add_pi(); }

bool symbol_table_add_epsilon(SymbolTable *t) { return t->add_epsilon(); }

bool symbol_table_add_infinity(SymbolTable *t) { return t->add_infinity(); }

bool symbol_table_is_constant_node(SymbolTable *t, char *name) {
  return t->is_constant_node(std::string(name));
}

bool symbol_table_is_constant_string(SymbolTable *t, char *name) {
  return t->is_constant_string(std::string(name));
}

cstr_list *symbol_table_get_variable_list(SymbolTable *t) {
  std::vector<std::string> vlist;
  t->get_variable_list(vlist);
  return strings_to_cstr_list(vlist);
}

cstr_list *symbol_table_get_stringvar_list(SymbolTable *t) {
  std::vector<std::string> slist;
  t->get_stringvar_list(slist);
  return strings_to_cstr_list(slist);
}

cstr_list *symbol_table_get_vector_list(SymbolTable *t) {
  std::vector<std::string> vlist;
  t->get_vector_list(vlist);
  return strings_to_cstr_list(vlist);
}

void string_array_free(cstr_list *c) {
  int n = c->size;
  for (int i = 0; i < n; i++) {
    delete[] c->elements[i];
  }
  delete[] c->elements;
  delete c;
}

bool symbol_table_symbol_exists(SymbolTable *t, char *variable_name) {
  return t->symbol_exists(std::string(variable_name));
}

bool symbol_table_valid(SymbolTable *t) { return t->valid(); }

void symbol_table_load_from(SymbolTable *t, const SymbolTable *other) {
  t->load_from(*other);
}

// functions

struct func_result {
  bool res;
  void *fn_pointer;
};

#define FUNC_DEF(ADD_NAME, FREE_NAME, STRUCT, ...)                             \
  func_result ADD_NAME(SymbolTable *t, char *name,                             \
                       double (*cb)(void *, __VA_ARGS__), void *user_data) {   \
    STRUCT<double> *f = new STRUCT<double>(cb, user_data);                     \
    func_result out;                                                           \
    out.res = t->add_function(std::string(name), *f);                          \
    out.fn_pointer = (void *)f;                                                \
    return out;                                                                \
  }                                                                            \
                                                                               \
  void FREE_NAME(var1_func<double> *f) { delete f; }

FUNC_DEF(symbol_table_add_func1, symbol_table_free_func1, var1_func, double);
FUNC_DEF(symbol_table_add_func2, symbol_table_free_func2, var2_func, double,
         double);
FUNC_DEF(symbol_table_add_func3, symbol_table_free_func3, var3_func, double,
         double, double);
FUNC_DEF(symbol_table_add_func4, symbol_table_free_func4, var4_func, double,
         double, double, double);

// Expression

Expression *expression_new() { return new Expression; }

void expression_destroy(Expression *e) { delete e; }

void expression_register_symbol_table(Expression *e, SymbolTable *t) {
  e->register_symbol_table(*t);
}

double expression_value(Expression *e) { return e->value(); }
}
