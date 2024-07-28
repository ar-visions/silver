#ifndef SILVER
#define SILVER
#include <A.h>

bool file_exists(cstr filename);
typedef struct Parser* Parser;
void assertion(Parser parser, bool is_true, cstr message, ...);

typedef struct module_t* module_t;
typedef struct define_t* define_t;

#define silver_t_meta(X,Y,Z) \
    i_intern(X,Y,Z, array,     tokens) \
    i_intern(X,Y,Z, define_t,  def) \
    i_cast(X,Y,Z, cstr) \
    i_override_m(X,Y,Z, u64, hash)
declare_class(silver_t)

#define EType_meta(X,Y) \
    enum_value(X,Y, Undefined) \
    enum_value(X,Y, Statements) \
    enum_value(X,Y, Assign) \
    enum_value(X,Y, AssignAdd) \
    enum_value(X,Y, AssignSub) \
    enum_value(X,Y, AssignMul) \
    enum_value(X,Y, AssignDiv) \
    enum_value(X,Y, AssignOr) \
    enum_value(X,Y, AssignAnd) \
    enum_value(X,Y, AssignXor) \
    enum_value(X,Y, AssignShiftR) \
    enum_value(X,Y, AssignShiftL) \
    enum_value(X,Y, AssignMod) \
    enum_value(X,Y, If) \
    enum_value(X,Y, For) \
    enum_value(X,Y, While) \
    enum_value(X,Y, DoWhile) \
    enum_value(X,Y, Break) \
    enum_value(X,Y, LiteralReal) \
    enum_value(X,Y, LiteralInt) \
    enum_value(X,Y, LiteralStr) \
    enum_value(X,Y, LiteralStrInterp) \
    enum_value(X,Y, Array) \
    enum_value(X,Y, AlphaIdent) \
    enum_value(X,Y, Var) \
    enum_value(X,Y, Add) \
    enum_value(X,Y, Sub) \
    enum_value(X,Y, Mul) \
    enum_value(X,Y, Div) \
    enum_value(X,Y, Or) \
    enum_value(X,Y, And) \
    enum_value(X,Y, Xor) \
    enum_value(X,Y, MethodCall) \
    enum_value(X,Y, MethodReturn)
declare_enum(EType)
define_enum(EType)

#define ident_meta(X,Y,Z) \
    i_intern(X,Y,Z, string,    value) \
    i_intern(X,Y,Z, path,      fname) \
    i_intern(X,Y,Z, array,     members_cache) \
    i_intern(X,Y,Z, int,       line_num) \
    i_intern(X,Y,Z, u64,       h) \
    i_method(X,Y,Z, array,     split_members, cstr) \
    i_method(X,Y,Z, EType,     is_numeric) \
    i_method(X,Y,Z, EType,     is_string) \
    i_method(X,Y,Z, EType,     is_alpha) \
    i_cast(X,Y,Z, cstr) \
    i_override_ctr(X,Y,Z, cstr) \
    i_override_m(X,Y,Z, u64, hash) \
    i_override_m(X,Y,Z, num, compare)
declare_class(ident)

declare_alias(array, array_ident)

#define enode_meta(X,Y,Z) \
    i_intern  (X,Y,Z, EType,      etype) \
    i_intern  (X,Y,Z, A,          value) \
    i_intern  (X,Y,Z, array,      operands) \
    i_intern  (X,Y,Z, array,      references) \
    s_method(X,Y,Z, enode,      create_operation,   EType, array) \
    s_method(X,Y,Z, enode,      create_value,       EType, A) \
    s_method(X,Y,Z, enode,      method_call,        ident, array) \
    s_method(X,Y,Z, A,          lookup,             array, ident, bool) \
    s_method(X,Y,Z, string,     string_interpolate, A, array) \
    i_method(X,Y,Z, A,          exec,               array) \
    i_override_cast(X,Y,Z, bool)
declare_class(enode)

#define MemberType_meta(X,Y) \
    enum_value(X,Y, Undefined) \
    enum_value(X,Y, Variable) \
    enum_value(X,Y, Lambda) \
    enum_value(X,Y, Method) \
    enum_value(X,Y, Cast) \
    enum_value(X,Y, Operator) \
    enum_value(X,Y, Constructor)
declare_enum(MemberType)
define_enum(MemberType)

/// everything a member can use; it could be expressed with unions in a more succinct way
#define member_def_meta(X,Y,Z) \
    i_intern(X,Y,Z,  bool,            is_template) \
    i_intern(X,Y,Z,  bool,            intern) \
    i_intern(X,Y,Z,  bool,            is_static) \
    i_intern(X,Y,Z,  bool,            is_public) \
    i_intern(X,Y,Z,  fn_t,            resolve) \
    i_intern(X,Y,Z,  MemberType,      member_type) \
    i_intern(X,Y,Z,  string,          name) \
    i_intern(X,Y,Z,  array,           args) \
    i_intern(X,Y,Z,  silver_t,        type) \
    i_intern(X,Y,Z,  string,          base_class) \
    i_intern(X,Y,Z,  array,           type_tokens) \
    i_intern(X,Y,Z,  array,           group_tokens) \
    i_intern(X,Y,Z,  array,           value) \
    i_intern(X,Y,Z,  array,           base_forward) \
    i_intern(X,Y,Z,  bool,            is_ctr) \
    i_intern(X,Y,Z,  enode,           translation)
declare_class(member_def)

#define EMembership_meta(X,Y) \
    enum_value(X,Y, normal) \
    enum_value(X,Y, internal)
declare_enum(EMembership)
define_enum(EMembership)

#define Parser_meta(X,Y,Z) \
    i_intern(X,Y,Z, array,  tokens) \
    i_intern(X,Y,Z, string, fname) \
    i_intern(X,Y,Z, module_t, module) \
    i_intern(X,Y,Z, num,    cur) \
    i_intern(X,Y,Z, EMembership, membership) \
    i_intern(X,Y,Z, array,  meta_symbols) \
    i_intern(X,Y,Z, string, keyword) \
    i_method(X,Y,Z, ident,  token_at,          num) \
    i_method(X,Y,Z, ident,  next) \
    i_method(X,Y,Z, ident,  pop) \
    i_method(X,Y,Z, num,    consume) \
    i_method(X,Y,Z, array,  parse_args, A) \
    i_method(X,Y,Z, EType,  expect,            ident, array) \
    i_method(X,Y,Z, ident,  relative,          num) \
    i_method(X,Y,Z, EType,  is_assign,         ident) \
    i_method(X,Y,Z, member_def, read_member,     A, member_def) \
    i_method(X,Y,Z, handle, parse_type) \
    i_method(X,Y,Z, enode,  parse_statements) \
    i_method(X,Y,Z, enode,  parse_expression) \
    i_method(X,Y,Z, array,  parse_raw_block) \
    i_method(X,Y,Z, enode,  parse_statement) \
    i_method(X,Y,Z, i64,    parse_numeric,     ident) \
    i_method(X,Y,Z, EType,  is_var,            ident) \
    i_method(X,Y,Z, enode,  parse_add) \
    i_method(X,Y,Z, enode,  parse_mult) \
    i_method(X,Y,Z, enode,  parse_primary) \
    i_method(X,Y,Z, array,  read_type) \
    i_construct(X,Y,Z,      array, path, module_t)
declare_class(Parser)

#define module_t_meta(X,Y,Z) \
    i_intern(X,Y,Z, array,  tokens) \
    i_intern(X,Y,Z, hashmap, cache) \
    i_intern(X,Y,Z, path,   module_name) \
    i_intern(X,Y,Z, array,  imports) \
    i_intern(X,Y,Z, array,  types) \
    i_intern(X,Y,Z, handle, app) \
    i_intern(X,Y,Z, array,  defines) \
    i_intern(X,Y,Z, bool,   translated) \
    i_construct(X,Y,Z,      path) \
    i_method(X,Y,Z, A, find_implement, ident) \
    i_method(X,Y,Z, A, find_class,     ident) \
    i_method(X,Y,Z, A, find_struct,    ident) \
    i_method(X,Y,Z, none,   graph) \
    i_method(X,Y,Z, none,   c99) \
    i_method(X,Y,Z, none,   run)
declare_class(module_t)

#define define_t_meta(X,Y,Z) \
    i_intern(X,Y,Z,    A_f*,           atype) \
    i_intern(X,Y,Z,    array,          tokens) \
    i_intern(X,Y,Z,    array,          meta_symbols) \
    i_intern(X,Y,Z,    string,         name) \
    i_intern(X,Y,Z,    string,         keyword) \
    i_intern(X,Y,Z,    EMembership,    membership) \
    i_intern(X,Y,Z,    module_t,       module) \
    i_intern(X,Y,Z,    bool,           is_translated) \
    i_method(X,Y,Z,  none, added) \
    i_method(X,Y,Z,  none, read_members) \
    i_method(X,Y,Z,  none, resolve_members) \
    i_construct(X,Y,Z, Parser)
declare_class(define_t)

/// dont need this if we are utilizing our own types
/// its trivial to know if we created the type with silver, or not
#define class_model_meta(X,Y) \
    enum_value(X,Y, allocated) \
    enum_value(X,Y, boolean_32) \
    enum_value(X,Y, unsigned_8) \
    enum_value(X,Y, unsigned_16) \
    enum_value(X,Y, unsigned_32) \
    enum_value(X,Y, unsigned_64) \
    enum_value(X,Y, signed_8) \
    enum_value(X,Y, signed_16) \
    enum_value(X,Y, signed_32) \
    enum_value(X,Y, signed_64) \
    enum_value(X,Y, real_32) \
    enum_value(X,Y, real_64) \
    enum_value(X,Y, real_128)
declare_enum(class_model)
define_enum(class_model)

/// projects build libs, we agree agree on that
/// source builds objects, then makes shared module at end with those objects and exported libraries
#define ImportType_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, project) \
    enum_value(X,Y, source) \
    enum_value(X,Y, library)
declare_enum(ImportType)
define_enum(ImportType)

#define struct_t_meta(X,Y,Z)  define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    array,  members) \
    i_override_ctr(X,Y,Z,      Parser)
declare_mod(struct_t, define_t)

#define var_t_meta(X,Y,Z)  define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    A, member)
declare_mod(var_t, define_t)

#define meta_instance_meta(X,Y,Z)  define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    array,  meta_types) \
    i_intern(X,Y,Z,    define_t, def) \
    i_construct(X,Y,Z, define_t, array)
declare_class(meta_instance)

#define class_t_meta(X,Y,Z)  define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    class_model, model) \
    i_intern(X,Y,Z,    string, from) \
    i_intern(X,Y,Z,    array,  members) \
    i_intern(X,Y,Z,    array,  friends) \
    i_method(X,Y,Z,    class_t, translate) \
    i_override_m(X,Y,Z, none, read_members) \
    i_override_m(X,Y,Z, none, resolve_members) \
    i_override_ctr(X,Y,Z,      Parser) \
    i_construct(X,Y,Z,         AType)
declare_mod(class_t, define_t)

#define enum_t_meta(X,Y,Z) define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    array,  symbols) \
    i_override_ctr(X,Y,Z,      Parser)
declare_mod(enum_t, define_t)

#define BuildState_meta(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, unbuilt) \
    enum_value(X,Y, built)
declare_enum(BuildState)
define_enum(BuildState)

#define import_t_meta(X,Y,Z)  define_t_meta(define_t,Y,Z) \
    i_intern(X,Y,Z,    ImportType, import_type) \
    i_intern(X,Y,Z,    string, import_name) \
    i_intern(X,Y,Z,    array,  library_exports) \
    i_intern(X,Y,Z,    array,  source) \
    i_intern(X,Y,Z,    string, shell) \
    i_intern(X,Y,Z,    array,  links) \
    i_intern(X,Y,Z,    array,  includes) \
    i_intern(X,Y,Z,    array,  defines) \
    i_intern(X,Y,Z,    string, isolate_namespace) \
    i_intern(X,Y,Z,    string, module_path) \
    i_intern(X,Y,Z,    path,   relative_path) \
    i_method(X,Y,Z,    BuildState, build_source,  string, array) \
    i_method(X,Y,Z,    BuildState, build_project, string, string) \
    i_override_m(X,Y,Z, none, added) \
    i_override_ctr(X,Y,Z,      Parser)
declare_mod(import_t, define_t)

#endif