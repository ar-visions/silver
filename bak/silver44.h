#include <A>

#include <errno.h>

typedef struct EModule*     EModule;
typedef struct EMember*     EMember;
typedef struct EMetaMember* EMetaMember;
typedef struct EContext*    EContext;
typedef struct EClass*      EClass;
typedef struct EStatements* EStatements;

#define Token_schema(X,Y,Z) \
    i_public(X,Y,Z, string,         value) \
    i_public(X,Y,Z, path,           file) \
    i_public(X,Y,Z, num,            line) \
    i_construct(X,Y,Z, cstr, path, num)
declare_class(Token)

#define ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, i32,            id) \
    i_method(X,Y,Z, string,         emit,             EContext) \
    i_method(X,Y,Z, none,           emit_header,      handle) \
    i_method(X,Y,Z, none,           emit_source,      handle) \
    i_method(X,Y,Z, none,           emit_source_decl, handle) \
    i_override_cast(X,Y,Z,      string)
declare_class(ENode)

#define EModel_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, string,         name) \
    i_public(X,Y,Z, i32,            size) \
    i_public(X,Y,Z, bool,           integral) \
    i_public(X,Y,Z, bool,           realistic) \
    i_public(X,Y,Z, AType,          type)
declare_class_2(EModel, ENode)

#define EMeta_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, map,            args)
declare_class_2(EMeta, ENode)

#define EIdent_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, map,            decorators) \
    i_public(X,Y,Z, array,          list) \
    i_public(X,Y,Z, string,         ident) \
    i_public(X,Y,Z, string,         initial) \
    i_public(X,Y,Z, EModule,        module) \
    i_public(X,Y,Z, bool,           is_fp) \
    i_public(X,Y,Z, object,         kind) \
    i_public(X,Y,Z, EMember,        base) \
    i_public(X,Y,Z, map,            meta_types) \
    i_public(X,Y,Z, map,            args) \
    i_public(X,Y,Z, array,          members) \
    i_public(X,Y,Z, bool,           ref_keyword) \
    i_public(X,Y,Z, EMember,        conforms) \
    i_public(X,Y,Z, EMetaMember,    meta_member) \
    i_method(X,Y,Z, EIdent,         parse, EModule, array, num) \
    i_method(X,Y,Z, EIdent,         peek, EModule) \
    i_method(X,Y,Z, EMember,        get_target) \
    i_method(X,Y,Z, EMember,        member_lookup, EIdent, string) \
    i_method(X,Y,Z, EIdent,         ref_type, num) \
    i_method(X,Y,Z, EMember,        get_def) \
    i_method(X,Y,Z, string,         get_c99_members, string) \
    i_method(X,Y,Z, EMember,        get_base) \
    i_method(X,Y,Z, string,         get_name) \
    i_method(X,Y,Z, num,            ref_total) \
    i_method(X,Y,Z, num,            ref) \
    i_method(X,Y,Z, none,           updated) \
    i_override_m(X,Y,Z, string,     emit) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EIdent, ENode)

#define EMember_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, string,         name) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, EModule,        module) \
    i_public(X,Y,Z, ENode,          value) \
    i_public(X,Y,Z, EClass,         parent) \
    i_public(X,Y,Z, string,         access) \
    i_public(X,Y,Z, bool,           imported) \
    i_public(X,Y,Z, bool,           emitted) \
    i_public(X,Y,Z, map,            members) \
    i_public(X,Y,Z, map,            args) \
    i_public(X,Y,Z, map,            context_args) \
    i_public(X,Y,Z, map,            meta_types) \
    i_public(X,Y,Z, map,            meta_model) \
    i_public(X,Y,Z, bool,           is_static) \
    i_public(X,Y,Z, string,         visibility) \
    i_override_m(X,Y,Z, string,     emit) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EMember, ENode)

#define EMetaMember_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         conforms) \
    i_public(X,Y,Z, EIdent,         index) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EMetaMember, EMember)

#define EMethod_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_public(X,Y,Z, string,         method_type) \
    i_public(X,Y,Z, bool,           type_expressed) \
    i_public(X,Y,Z, EIdent,         body) \
    i_public(X,Y,Z, EStatements,    statements) \
    i_public(X,Y,Z, ENode,          code) \
    i_public(X,Y,Z, bool,           automatic) \
    i_public(X,Y,Z, map,            context)
declare_class_2(EMethod, EMember)

#define EClass_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_public(X,Y,Z, EModel,         model) \
    i_public(X,Y,Z, EClass,         inherits) \
    i_public(X,Y,Z, EIdent,         block_tokens) \
    i_method(X,Y,Z, none,           print) \
    i_method(X,Y,Z, none,           output_methods, handle, EClass, bool) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EClass, EMember)

#define EModule_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, path,           path) \
    i_public(X,Y,Z, string,         name) \
    i_public(X,Y,Z, array,          tokens) \
    i_public(X,Y,Z, array,          include_paths) \
    i_public(X,Y,Z, map,            clang_cache) \
    i_public(X,Y,Z, map,            include_defs) \
    i_public(X,Y,Z, map,            parent_modules) \
    i_public(X,Y,Z, map,            defs) \
    i_public(X,Y,Z, map,            type_cache) \
    i_public(X,Y,Z, bool,           finished) \
    i_public(X,Y,Z, i32,            recur) \
    i_public(X,Y,Z, map,            clang_defs) \
    i_public(X,Y,Z, i32,            expr_level) \
    i_public(X,Y,Z, i32,            index) \
    i_public(X,Y,Z, array,          token_bank) \
    i_public(X,Y,Z, array,          libraries_used) \
    i_public(X,Y,Z, array,          compiled_objects) \
    i_public(X,Y,Z, array,          main_symbols) \
    i_public(X,Y,Z, array,          context_state) \
    i_public(X,Y,Z, EMember,        current_def) \
    i_public(X,Y,Z, array,          member_stack) \
    i_public(X,Y,Z, map,            meta_users) \
    i_method(X,Y,Z, none,           register_meta_user, EIdent) \
    i_method(X,Y,Z, bool,           eof) \
    i_method(X,Y,Z, array,          read_tokens, string) \
    i_method(X,Y,Z, object,         find_clang_def, object) \
    i_method(X,Y,Z, object,         find_def, object) \
    i_method(X,Y,Z, string,         header_emit, string, string) \
    i_method(X,Y,Z, none,           initialize) \
    i_method(X,Y,Z, none,           process_includes, array) \
    i_method(X,Y,Z, none,           build) \
    i_method(X,Y,Z, none,           build_dependencies) \
    i_method(X,Y,Z, object,         get_base_type, object) \
    i_method(X,Y,Z, EIdent,         type_identity, object) \
    i_method(X,Y,Z, map,            function_info, object) \
    i_method(X,Y,Z, map,            struct_info, object) \
    i_method(X,Y,Z, map,            union_info, object) \
    i_method(X,Y,Z, map,            enum_info, object) \
    i_method(X,Y,Z, bool,           has_function_pointer, object) \
    i_method(X,Y,Z, map,            typedef_info, object) \
    i_method(X,Y,Z, object,         edef_for, object) \
    i_method(X,Y,Z, map,            parse_header, string) \
    i_method(X,Y,Z, none,           push_token_state, array, num) \
    i_method(X,Y,Z, none,           transfer_token_state) \
    i_method(X,Y,Z, none,           pop_token_state) \
    i_method(X,Y,Z, Token,          next_token) \
    i_method(X,Y,Z, Token,          prev_token) \
    i_method(X,Y,Z, object,         debug_tokens) \
    i_method(X,Y,Z, Token,          peek_token, num) \
    i_method(X,Y,Z, none,           parse_method, EClass, EMethod) \
    i_method(X,Y,Z, none,           assertion, bool, string) \
    i_method(X,Y,Z, EMethod,        is_method, EIdent) \
    i_method(X,Y,Z, object,         is_defined, array) \
    i_method(X,Y,Z, EMember,        lookup_stack_member, string) \
    i_method(X,Y,Z, array,          lookup_all_stack_members, string) \
    i_method(X,Y,Z, object,         resolve_member, array) \
    i_method(X,Y,Z, ENode,          parse_expression) \
    i_method(X,Y,Z, none,           consume, object) \
    i_method(X,Y,Z, EModel,         model, object) \
    i_method(X,Y,Z, bool,           is_primitive, EIdent) \
    i_method(X,Y,Z, EIdent,         preferred_type, object, object) \
    i_method(X,Y,Z, ENode,          parse_operator, object, string, string, object, object) \
    i_method(X,Y,Z, ENode,          parse_add) \
    i_method(X,Y,Z, ENode,          parse_mult) \
    i_method(X,Y,Z, ENode,          parse_is) \
    i_method(X,Y,Z, ENode,          parse_eq) \
    i_method(X,Y,Z, object,         is_bool, Token) \
    i_method(X,Y,Z, object,         is_numeric, Token) \
    i_method(X,Y,Z, object,         is_string, Token) \
    i_method(X,Y,Z, EIdent,         type_of, object) \
    i_method(X,Y,Z, none,           push_context_state, object) \
    i_method(X,Y,Z, none,           pop_context_state) \
    i_method(X,Y,Z, string,         top_context_state) \
    i_method(X,Y,Z, ESubProc,       parse_sub_proc) \
    i_method(X,Y,Z, ENode,          parse_primary) \
    i_method(X,Y,Z, none,           reset_member_depth) \
    i_method(X,Y,Z, none,           push_member_depth) \
    i_method(X,Y,Z, map,            pop_member_depth) \
    i_method(X,Y,Z, none,           push_return_type, EIdent) \
    i_method(X,Y,Z, none,           push_member, EMember, EMember) \
    i_method(X,Y,Z, array,          casts, object) \
    i_method(X,Y,Z, array,          constructs, object) \
    i_method(X,Y,Z, object,         castable, EIdent, EIdent) \
    i_method(X,Y,Z, object,         constructable, EIdent, EIdent) \
    i_method(X,Y,Z, bool,           convertible, object, object) \
    i_method(X,Y,Z, ENode,          convert_enode, ENode, EIdent) \
    i_method(X,Y,Z, array,          convert_args, EMethod, array) \
    i_method(X,Y,Z, EWhile,         parse_while, Token) \
    i_method(X,Y,Z, EDoWhile,       parse_do_while, Token) \
    i_method(X,Y,Z, EIf,            parse_if_else, Token) \
    i_method(X,Y,Z, EFor,           parse_for, Token) \
    i_method(X,Y,Z, EBreak,         parse_break, Token) \
    i_method(X,Y,Z, EMethodReturn,  parse_return, Token) \
    i_method(X,Y,Z, object,         parse_anonymous_ref, EIdent) \
    i_method(X,Y,Z, ENode,          parse_statement) \
    i_method(X,Y,Z, EStatements,    parse_statements, EStatements) \
    i_method(X,Y,Z, array,          parse_call_args, EMethod, bool, bool) \
    i_method(X,Y,Z, object,         parse_defined_args, bool, bool) \
    i_method(X,Y,Z, EMethod,        finish_method, EClass, Token, string, bool, string) \
    i_method(X,Y,Z, none,           finish_class, EClass) \
    i_method(X,Y,Z, bool,           next_is, object) \
    i_method(X,Y,Z, string,         convert_literal, Token) \
    i_method(X,Y,Z, array,          import_list, EImport, string) \
    i_method(X,Y,Z, none,           parse_import_fields, EImport) \
    i_method(X,Y,Z, EImport,        parse_import, string) \
    i_method(X,Y,Z, EClass,         parse_class, string, map) \
    i_method(X,Y,Z, ENode,          translate, EClass, EMethod) \
    i_method(X,Y,Z, map,            parse_meta_model) \
    i_method(X,Y,Z, none,           parse, array) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EModule, ENode)

#define EStruct_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EStruct, EMember)

#define EUnion_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EUnion, EMember)

#define EEnum_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EEnum, EMember)

#define EAlias_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         to) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EAlias, EMember)

#define EImport_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, string,         name) \
    i_public(X,Y,Z, string,         source) \
    i_public(X,Y,Z, array,          includes) \
    i_public(X,Y,Z, array,          cfiles) \
    i_public(X,Y,Z, array,          links) \
    i_public(X,Y,Z, bool,           imported) \
    i_public(X,Y,Z, array,          build_args) \
    i_public(X,Y,Z, i32,            import_type) \
    i_public(X,Y,Z, array,          library_exports) \
    i_public(X,Y,Z, string,         visibility) \
    i_public(X,Y,Z, string,         main_symbol) \
    i_method(X,Y,Z, bool,           file_exists, string) \
    i_method(X,Y,Z, BuildState,     build_project, string, string) \
    i_method(X,Y,Z, BuildState,     build_source) \
    i_method(X,Y,Z, none,           process, EModule) \
    i_override_m(X,Y,Z, none,       emit_header) \
    i_override_m(X,Y,Z, none,       emit_source) \
    i_override_m(X,Y,Z, none,       emit_source_decl)
declare_class_2(EImport, ENode)

#define EConstruct_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, EMethod,        method) \
    i_public(X,Y,Z, array,          args) \
    i_public(X,Y,Z, EMetaMember,    meta_member) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EConstruct, ENode)

#define EExplicitCast_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EExplicitCast, ENode)

#define EPrimitive_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EPrimitive, ENode)

#define EProp_schema(X,Y,Z) \
    EMember_schema(X,Y,Z) \
    i_public(X,Y,Z, bool,           is_prop)
declare_class_2(EProp, EMember)

#define ERef_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ERef, ENode)

#define ERefCast_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          value) \
    i_public(X,Y,Z, ENode,          index) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ERefCast, ENode)

#define EIndex_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          target) \
    i_public(X,Y,Z, ENode,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EIndex, ENode)

#define EAssign_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          target) \
    i_public(X,Y,Z, ENode,          value) \
    i_public(X,Y,Z, ENode,          index) \
    i_public(X,Y,Z, bool,           declare) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EAssign, ENode)

#define EIf_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          condition) \
    i_public(X,Y,Z, EStatements,    body) \
    i_public(X,Y,Z, ENode,          else_body) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EIf, ENode)

#define EFor_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          init) \
    i_public(X,Y,Z, ENode,          condition) \
    i_public(X,Y,Z, ENode,          update) \
    i_public(X,Y,Z, ENode,          body) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EFor, ENode)

#define EWhile_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          condition) \
    i_public(X,Y,Z, ENode,          body) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EWhile, ENode)

#define EDoWhile_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          condition) \
    i_public(X,Y,Z, ENode,          body) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EDoWhile, ENode)

#define EBreak_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EBreak, ENode)

#define ELiteralReal_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, f64,            value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ELiteralReal, ENode)

#define ELiteralInt_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, i64,            value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ELiteralInt, ENode)

#define ELiteralStr_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, string,         value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ELiteralStr, ENode)

#define ELiteralStrInterp_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, string,         value) \
    i_public(X,Y,Z, object,         args) \
    i_method(X,Y,Z, string,         emit, EContext)
declare_class_2(ELiteralStrInterp, ENode)

#define ELiteralBool_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, bool,           value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ELiteralBool, ENode)

#define ESubProc_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, EMember,        target) \
    i_public(X,Y,Z, EMember,        method) \
    i_public(X,Y,Z, EIdent,         context_type) \
    i_public(X,Y,Z, array,          context_args) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ESubProc, ENode)

#define EOperator_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          left) \
    i_public(X,Y,Z, ENode,          right) \
    i_public(X,Y,Z, string,         op) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EOperator, ENode)

#define EIs_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z) \
    i_method(X,Y,Z, string,         template) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EIs, EOperator)

#define EInherits_schema(X,Y,Z) \
    EIs_schema(X,Y,Z) \
    i_method(X,Y,Z, string,         template)
declare_class_2(EInherits, EIs)

#define ERuntimeType_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ERuntimeType, ENode)

#define EMethodCall_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          target) \
    i_public(X,Y,Z, ENode,          method) \
    i_public(X,Y,Z, array,          args) \
    i_public(X,Y,Z, array,          arg_temp_members) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EMethodCall, ENode)

#define EMethodReturn_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EMethodReturn, ENode)

#define EStatements_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, array,          value) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EStatements, ENode)

#define EContext_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EModule,        module) \
    i_public(X,Y,Z, EMember,        method) \
    i_public(X,Y,Z, array,          states) \
    i_public(X,Y,Z, bool,           raw_primitives) \
    i_public(X,Y,Z, map,            values) \
    i_public(X,Y,Z, i32,            indent_level) \
    i_method(X,Y,Z, string,         indent) \
    i_method(X,Y,Z, none,           increase_indent) \
    i_method(X,Y,Z, none,           decrease_indent) \
    i_method(X,Y,Z, none,           set_value, string, A) \
    i_method(X,Y,Z, A,              get_value, string) \
    i_method(X,Y,Z, none,           push, string) \
    i_method(X,Y,Z, string,         pop) \
    i_method(X,Y,Z, string,         top_state)
declare_class_2(EContext, ENode)

#define EDeclaration_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, EMember,        target) \
    i_method(X,Y,Z, string,         emit, EContext)
declare_class_2(EDeclaration, ENode)

#define EUndefined_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)
declare_class_2(EUndefined, ENode)

#define EParenthesis_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          enode) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EParenthesis, ENode)

#define ELogicalNot_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          enode) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(ELogicalNot, ENode)

#define EBitwiseNot_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, EIdent,         type) \
    i_public(X,Y,Z, ENode,          enode) \
    i_override_m(X,Y,Z, string,     emit)
declare_class_2(EBitwiseNot, ENode)

#define ECompareEquals_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(ECompareEquals, EOperator)

#define ECompareNotEquals_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(ECompareNotEquals, EOperator)

#define EAdd_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EAdd, EOperator)

#define ESub_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(ESub, EOperator)

#define EMul_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EMul, EOperator)

#define EDiv_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EDiv, EOperator)

#define EOr_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EOr, EOperator)

#define EAnd_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EAnd, EOperator)

#define EXor_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_class_2(EXor, EOperator)

#define EAssignAdd_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignAdd, EAssign)

#define EAssignSub_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignSub, EAssign)

#define EAssignMul_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignMul, EAssign)

#define EAssignDiv_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignDiv, EAssign)

#define EAssignOr_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignOr, EAssign)

#define EAssignAnd_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignAnd, EAssign)

#define EAssignXor_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignXor, EAssign)

#define EAssignShiftR_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignShiftR, EAssign)

#define EAssignShiftL_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignShiftL, EAssign)

#define EAssignMod_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_class_2(EAssignMod, EAssign)

#define BuildState_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(X,Y,Z, i32,            none) \
    i_public(X,Y,Z, i32,            built)
declare_class_2(BuildState, ENode)




static string EContext_indent(EContext self) {
    string indent = ctr(string, cstr, "", 0);
    for (i32 i = 0; i < self->indent_level; i++) {
        call(indent, append, "\t");
    }
    return indent;
}

static none EContext_increase_indent(EContext self) {
    self->indent_level++;
}

static none EContext_decrease_indent(EContext self) {
    if (self->indent_level > 0) {
        self->indent_level--;
    }
}

static none EContext_set_value(EContext self, string key, A value) {
    call(self->values, set, key, value);
}

static A EContext_get_value(EContext self, string key) {
    return call(self->values, get, key);
}

static none EContext_push(EContext self, string state) {
    call(self->states, push, state);
}

static string EContext_pop(EContext self) {
    return call(self->states, pop);
}

static string EContext_top_state(EContext self) {
    if (call(self->states, count) > 0) {
        return call(self->states, last);
    } else {
        return ctr(string, cstr, "", 0);
    }
}