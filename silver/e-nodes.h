#include <A>

/*
typedef struct ENode*                   ENode;
typedef struct EIdent*                  EIdent;
typedef struct EModule*                 EModule;
typedef struct EMember*                 EMember;
typedef struct EMetaMember*             EMetaMember;
typedef struct EMethod*                 EMethod;
typedef struct EClass*                  EClass;
typedef struct EProp*                   EProp;
typedef struct EStruct*                 EStruct;
typedef struct EOperator*               EOperator;
typedef struct ECompareEquals*          ECompareEquals;
typedef struct ECompareNotEquals*       ECompareNotEquals;
typedef struct EAdd*                    EAdd;
typedef struct ESub*                    ESub;
typedef struct EMul*                    EMul;
typedef struct EDiv*                    EDiv;
typedef struct EOr*                     EOr;
typedef struct EAnd*                    EAnd;
typedef struct EXor*                    EXor;
typedef struct EIs*                     EIs;
typedef struct EInherits*               EInherits;
typedef struct EImport*                 EImport;
typedef struct EStatements*             EStatements;
typedef struct EModel*                  EModel;
typedef struct EDeclaration*            EDeclaration;
typedef struct EConstruct*              EConstruct;
typedef struct EExplicitCast*           EExplicitCast;
typedef struct EPrimitive*              EPrimitive;
typedef struct EUndefined*              EUndefined;
typedef struct EParenthesis*            EParenthesis;
typedef struct ELogicalNot*             ELogicalNot;
typedef struct EBitwiseNot*             EBitwiseNot;
typedef struct ERef*                    ERef;
typedef struct ERefCast*                ERefCast;
typedef struct EIndex*                  EIndex;
typedef struct EAssign*                 EAssign;
typedef struct EAssignAdd*              EAssignAdd;
typedef struct EAssignSub*              EAssignSub;
typedef struct EAssignMul*              EAssignMul;
typedef struct EAssignDiv*              EAssignDiv;
typedef struct EAssignOr*               EAssignOr;
typedef struct EAssignAnd*              EAssignAnd;
typedef struct EAssignXor*              EAssignXor;
typedef struct EAssignShiftR*           EAssignShiftR;
typedef struct EAssignShiftL*           EAssignShiftL;
typedef struct EAssignMod*              EAssignMod;
typedef struct EIf*                     EIf;
typedef struct EFor*                    EFor;
typedef struct EWhile*                  EWhile;
typedef struct EDoWhile*                EDoWhile;
typedef struct EBreak*                  EBreak;
typedef struct ELiteralReal*            ELiteralReal;
typedef struct ELiteralInt*             ELiteralInt;
typedef struct ELiteralStr*             ELiteralStr;
typedef struct ELiteralStrInterp*       ELiteralStrInterp;
typedef struct ELiteralBool*            ELiteralBool;
typedef struct ESubProc*                ESubProc;
typedef struct ERuntimeType*            ERuntimeType;
typedef struct EMethodCall*             EMethodCall;
typedef struct EMethodReturn*           EMethodReturn;
typedef struct Token*                   Token;

#define Token_schema(X,Y,Z) \
    i_public(   X,Y,Z,  string,    value) \
    i_public(   X,Y,Z,  num,       col) \
    i_public(   X,Y,Z,  num,       line) \
    i_override_cast(X,Y,Z, string) \
    i_override_m(X,Y,Z, num,       compare) \
    i_construct(X,Y,Z,  cstr,      num, num) \
declare_class(Token)


#define EContext_schema(X,Y,Z) \
    i_public(   X,Y,Z,  EModule,   module) \
    i_public(   X,Y,Z,  EMember,   method) \
    i_public(   X,Y,Z,  array,     states) \
    i_public(   X,Y,Z,  bool,    raw_primitives) \
    i_public(   X,Y,Z,  map,       values) \
    i_public(   X,Y,Z,  num,       indent_level) \
    i_method(   X,Y,Z,  string,    indent) \
    i_method(   X,Y,Z,  none,      set_value, A, A) \
    i_method(   X,Y,Z,  A,         get_value, A) \
    i_method(   X,Y,Z,  none,      push, string) \
    i_method(   X,Y,Z,  none,      pop) \
    i_method(   X,Y,Z,  string,    top_state)
declare_class(EContext)


#define ENode_schema(X,Y,Z) \
    i_public(   X,Y,Z,  AType,     type) \
    i_public(   X,Y,Z,  num,       id) \
    i_method(   X,Y,Z,  bool,    equals, X) \
    i_method(   X,Y,Z,  bool,    to_bool) \
    i_method(   X,Y,Z,  string,    emit, A) \
    i_override_cast(X,Y,Z,  string) \
    i_construct(X,Y,Z,  AType)
declare_class(ENode)

#define EIdent_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(  X,Y,Z, hashmap,     decorators)            \
    i_public(  X,Y,Z, array,       list)                  \
    i_public(  X,Y,Z, string,      ident)                 \
    i_public(  X,Y,Z, string,      initial)               \
    i_public(  X,Y,Z, EModule,     module)                \
    i_public(  X,Y,Z, bool,      is_fp)                 \
    i_public(  X,Y,Z, AType,       kind)                  \
    i_public(  X,Y,Z, EMember,     base)                  \
    i_public(  X,Y,Z, hashmap,     meta_types)            \
    i_public(  X,Y,Z, hashmap,     args)                  \
    i_public(  X,Y,Z, array,       members)               \
    i_public(  X,Y,Z, bool,      ref_keyword)           \
    i_public(  X,Y,Z, EMember,     conforms)              \
    i_public(  X,Y,Z, EMetaMember, meta_member)           \
    i_method(  X,Y,Z, EMember,     get_target)            \
    i_method(  X,Y,Z, EMember,     member_lookup, X, string) \
    i_method(  X,Y,Z, X,           ref_type, num)         \
    i_method(  X,Y,Z, AType,       get_def)               \
    i_method(  X,Y,Z, string,      get_c99_members, string) \
    i_method(  X,Y,Z, AType,       get_base)              \
    i_method(  X,Y,Z, string,      get_name)              \
    i_override_m(X,Y,Z, string,    emit) \
    i_method(  X,Y,Z, num,         ref_total)             \
    i_method(  X,Y,Z, num,         ref)                   \
    i_method(  X,Y,Z, none,        updated)               \
    i_override_cast(X,Y,Z, string) \
    i_construct(X,Y,Z, A, AType)
declare_mod(EIdent, ENode)

#define EModule_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(  X,Y,Z, path,        path)                  \
    i_public(  X,Y,Z, string,      name)                  \
    i_public(  X,Y,Z, array,       tokens)                \
    i_public(  X,Y,Z, array,       include_paths)         \
    i_public(  X,Y,Z, hashmap,     clang_cache)           \
    i_public(  X,Y,Z, hashmap,     include_defs)          \
    i_public(  X,Y,Z, hashmap,     parent_modules)        \
    i_public(  X,Y,Z, hashmap,     defs)                  \
    i_public(  X,Y,Z, hashmap,     type_cache)            \
    i_public(  X,Y,Z, bool,      finished)              \
    i_public(  X,Y,Z, num,         recur)                 \
    i_public(  X,Y,Z, hashmap,     clang_defs)            \
    i_public(  X,Y,Z, num,         expr_level)            \
    i_public(  X,Y,Z, num,         index)                 \
    i_public(  X,Y,Z, array,       token_bank)            \
    i_public(  X,Y,Z, array,       libraries_used)        \
    i_public(  X,Y,Z, array,       compiled_objects)      \
    i_public(  X,Y,Z, array,       main_symbols)          \
    i_public(  X,Y,Z, array,       context_state)         \
    i_public(  X,Y,Z, EMember,     current_def)           \
    i_public(  X,Y,Z, array,       member_stack)          \
    i_public(  X,Y,Z, hashmap,     meta_users)            \
    i_method(  X,Y,Z, none,        register_meta_user, EIdent) \
    i_method(  X,Y,Z, bool,      eof)                   \
    i_method(  X,Y,Z, array,       read_tokens, string)   \
    i_method(  X,Y,Z, AType,       find_clang_def, A)     \
    i_method(  X,Y,Z, AType,       find_def, A)           \
    i_method(  X,Y,Z, string,      header_emit, string, string) \
    i_method(  X,Y,Z, none,        emit_includes, A, string) \
    i_override_m(X,Y,Z, string,    emit) \
    i_method(  X,Y,Z, none,        initialize)            \
    i_method(  X,Y,Z, none,        process_includes, array) \
    i_method(  X,Y,Z, none,        build)                 \
    i_method(  X,Y,Z, none,        build_dependencies)    \
    i_method(  X,Y,Z, AType,       get_base_type, AType)  \
    i_method(  X,Y,Z, EIdent,      type_identity, A)      \
    i_method(  X,Y,Z, hashmap,     function_info, A)      \
    i_method(  X,Y,Z, hashmap,     struct_info, A)        \
    i_method(  X,Y,Z, hashmap,     union_info, A)         \
    i_method(  X,Y,Z, hashmap,     enum_info, A)          \
    i_method(  X,Y,Z, bool,      has_function_pointer, A) \
    i_method(  X,Y,Z, hashmap,     typedef_info, A)       \
    i_method(  X,Y,Z, A,           edef_for, A)           \
    i_method(  X,Y,Z, none,        parse_header, string)  \
    i_method(  X,Y,Z, none,        push_token_state, array, num) \
    i_method(  X,Y,Z, none,        transfer_token_state)  \
    i_method(  X,Y,Z, none,        pop_token_state)       \
    i_method(  X,Y,Z, Token,       next_token)            \
    i_method(  X,Y,Z, Token,       prev_token)            \
    i_method(  X,Y,Z, A,           debug_tokens)          \
    i_method(  X,Y,Z, Token,       peek_token, num)       \
    i_method(  X,Y,Z, none,        parse_method, A, A)    \
    i_method(  X,Y,Z, none,        assertion, bool, string) \
    i_method(  X,Y,Z, EMethod,     is_method, EIdent)     \
    i_method(  X,Y,Z, AType,       is_defined, array)     \
    i_method(  X,Y,Z, EMember,     lookup_stack_member, string) \
    i_method(  X,Y,Z, array,       lookup_all_stack_members, string) \
    i_method(  X,Y,Z, A,           resolve_member, array) \
    i_method(  X,Y,Z, ENode,       parse_expression)      \
    i_method(  X,Y,Z, none,        consume, A)            \
    i_method(  X,Y,Z, EModel,      model, AType)          \
    i_method(  X,Y,Z, bool,      is_primitive, EIdent)  \
    i_method(  X,Y,Z, EIdent,      preferred_type, A, A)  \
    i_method(  X,Y,Z, ENode,       parse_operator, A, string, string, AType, AType) \
    i_method(  X,Y,Z, ENode,       parse_add)             \
    i_method(  X,Y,Z, ENode,       parse_mult)            \
    i_method(  X,Y,Z, ENode,       parse_is)              \
    i_method(  X,Y,Z, ENode,       parse_eq)              \
    i_method(  X,Y,Z, AType,       is_bool, Token)        \
    i_method(  X,Y,Z, AType,       is_numeric, Token)     \
    i_method(  X,Y,Z, AType,       is_string, Token)      \
    i_method(  X,Y,Z, EIdent,      type_of, A)            \
    i_method(  X,Y,Z, none,        push_context_state, A) \
    i_method(  X,Y,Z, none,        pop_context_state)     \
    i_method(  X,Y,Z, string,      top_context_state)     \
    i_method(  X,Y,Z, ENode,       parse_sub_proc)        \
    i_method(  X,Y,Z, ENode,       parse_primary)         \
    i_method(  X,Y,Z, none,        reset_member_depth)    \
    i_method(  X,Y,Z, none,        push_member_depth)     \
    i_method(  X,Y,Z, hashmap,     pop_member_depth)      \
    i_method(  X,Y,Z, none,        push_return_type, EIdent) \
    i_method(  X,Y,Z, none,        push_member, EMember, EMember) \
    i_method(  X,Y,Z, array,       casts, A)              \
    i_method(  X,Y,Z, array,       constructs, A)         \
    i_method(  X,Y,Z, A,           castable, EIdent, EIdent) \
    i_method(  X,Y,Z, EMethod,     constructable, EIdent, EIdent) \
    i_method(  X,Y,Z, bool,      convertible, EIdent, EIdent) \
    i_method(  X,Y,Z, ENode,       convert_enode, ENode, EIdent) \
    i_method(  X,Y,Z, array,       convert_args, EMethod, array) \
    i_method(  X,Y,Z, ENode,       parse_while, Token)    \
    i_method(  X,Y,Z, ENode,       parse_do_while, Token) \
    i_method(  X,Y,Z, ENode,       parse_if_else, Token)  \
    i_method(  X,Y,Z, ENode,       parse_for, Token)      \
    i_method(  X,Y,Z, ENode,       parse_break, Token)    \
    i_method(  X,Y,Z, ENode,       parse_return, Token)   \
    i_method(  X,Y,Z, A,           parse_anonymous_ref, EIdent) \
    i_method(  X,Y,Z, ENode,       parse_statement)       \
    i_method(  X,Y,Z, EStatements, parse_statements, EStatements) \
    i_method(  X,Y,Z, array,       parse_call_args, EMethod, bool, bool) \
    i_method(  X,Y,Z, A,           parse_defined_args, bool, bool) \
    i_method(  X,Y,Z, EMethod,     finish_method, A, Token, string, bool, string) \
    i_method(  X,Y,Z, none,        finish_class, A)       \
    i_method(  X,Y,Z, bool,      next_is, string)       \
    i_method(  X,Y,Z, string,      convert_literal, Token) \
    i_method(  X,Y,Z, array,       import_list, A, string) \
    i_method(  X,Y,Z, none,        parse_import_fields, A) \
    i_method(  X,Y,Z, EImport,     parse_import, string)  \
    i_method(  X,Y,Z, EClass,      parse_class, string, hashmap) \
    i_method(  X,Y,Z, ENode,       translate, ENode, EMethod) \
    i_method(  X,Y,Z, hashmap,     parse_meta_model)      \
    i_method(  X,Y,Z, none,        parse, array)          \
    i_override_cast(X,Y,Z, string) \
    i_construct(X,Y,Z, path)
declare_mod(EModule, ENode)


#define EMember_schema(X,Y,Z) \
    ENode_schema(X,Y,Z) \
    i_public(  X,Y,Z, string,      name)                  \
    i_public(  X,Y,Z, EModule,     module)                \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_public(  X,Y,Z, EClass,      parent)                \
    i_public(  X,Y,Z, string,      access)                \
    i_public(  X,Y,Z, bool,      imported)              \
    i_public(  X,Y,Z, bool,      emitted)               \
    i_public(  X,Y,Z, hashmap,     members)               \
    i_public(  X,Y,Z, hashmap,     args)                  \
    i_public(  X,Y,Z, hashmap,     context_args)          \
    i_public(  X,Y,Z, hashmap,     meta_types)            \
    i_public(  X,Y,Z, hashmap,     meta_model)            \
    i_public(  X,Y,Z, bool,      is_static)             \
    i_public(  X,Y,Z, string,      visibility)            \
    i_override_m(X,Y,Z, string,    emit) \
    i_override_cast(X,Y,Z, string) \
    i_construct(X,Y,Z, string, EIdent, EModule)
declare_mod(EMember, ENode)


#define EMetaMember_schema(X,Y,Z) \
    EMember_schema(X,Y,Z)                                 \
    i_public(  X,Y,Z, EIdent,      conforms)              \
    i_public(  X,Y,Z, EIdent,      index)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, string, EIdent, EModule, EIdent, EIdent)
declare_mod(EMetaMember, ENode)

// todo: cleanup constructors; lets somehow make named arguments if we can lol
// ok gpt

#define EMethod_schema(X,Y,Z) \
    EMember_schema(X,Y,Z)                                 \
    i_public(  X,Y,Z, string,      method_type)           \
    i_public(  X,Y,Z, bool,        type_expressed)        \
    i_public(  X,Y,Z, EIdent,      body)                  \
    i_public(  X,Y,Z, EStatements, statements)            \
    i_public(  X,Y,Z, ENode,       code)                  \
    i_public(  X,Y,Z, bool,        auto)                  \
    i_public(  X,Y,Z, hashmap,     context)               \
    i_construct(X,Y,Z, string, EIdent, EModule, string)
declare_mod(EMethod, EMember)


#define EStruct_schema(X,Y,Z) \
    EMember_schema(X,Y,Z)                                 \
    i_method(  X,Y,Z, none,        emit_header, A, bool) \
    i_method(  X,Y,Z, none,        emit_source_decl, A)   \
    i_method(  X,Y,Z, none,        emit_source, A)        \
    i_construct(X,Y,Z, string, EModule)
declare_mod(EStruct, EMember)


#define EClass_schema(X,Y,Z) \
    EMember_schema(X,Y,Z)                                 \
    i_public(  X,Y,Z, EModel,      model)                 \
    i_public(  X,Y,Z, EClass,      inherits)              \
    i_public(  X,Y,Z, EIdent,      block_tokens)          \
    i_method(  X,Y,Z, none,        print)                 \
    i_method(  X,Y,Z, none,        emit_header, A)        \
    i_method(  X,Y,Z, none,        output_methods, A, A, bool) \
    i_method(  X,Y,Z, none,        emit_source_decl, A)   \
    i_method(  X,Y,Z, none,        emit_source, A)        \
    i_construct(X,Y,Z, string, EModule, string)
declare_mod(EClass, EMember)


#define EProp_schema(X,Y,Z) \
    EMember_schema(X,Y,Z)                                 \
    i_public(  X,Y,Z, bool,      is_prop)               \
    i_construct(X,Y,Z, string, EIdent, EModule)
declare_mod(EProp, EMember)


#define EOperator_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       left)                  \
    i_public(  X,Y,Z, ENode,       right)                 \
    i_public(  X,Y,Z, string,      op)                    \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode, string)
declare_mod(EOperator, ENode)


#define ECompareEquals_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(ECompareEquals, EOperator)


#define ECompareNotEquals_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(ECompareNotEquals, EOperator)


#define EAdd_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EAdd, EOperator)


#define ESub_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(ESub, EOperator)


#define EMul_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EMul, EOperator)


#define EDiv_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EDiv, EOperator)


#define EOr_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EOr, EOperator)


#define EAnd_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EAnd, EOperator)


#define EXor_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)
declare_mod(EXor, EOperator)


#define EIs_schema(X,Y,Z) \
    EOperator_schema(X,Y,Z)                               \
    i_method(  X,Y,Z, string,      template)              \
    i_override_m(X,Y,Z, string,    emit)
declare_mod(EIs, EOperator)


#define EInherits_schema(X,Y,Z) \
    EIs_schema(X,Y,Z)                                     \
    i_method(  X,Y,Z, string,      template)
declare_mod(EInherits, EIs)


#define EImport_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, string,      name)                  \
    i_public(  X,Y,Z, string,      source)                \
    i_public(  X,Y,Z, array,       includes)              \
    i_public(  X,Y,Z, array,       cfiles)                \
    i_public(  X,Y,Z, array,       links)                 \
    i_public(  X,Y,Z, bool,      imported)              \
    i_public(  X,Y,Z, array,       build_args)            \
    i_public(  X,Y,Z, num,         import_type)           \
    i_public(  X,Y,Z, array,       library_exports)       \
    i_public(  X,Y,Z, string,      visibility)            \
    i_public(  X,Y,Z, string,      main_symbol)           \
    i_method(  X,Y,Z, none,        emit_header, A)        \
    i_method(  X,Y,Z, none,        emit_source, A)        \
    i_method(  X,Y,Z, none,        emit_source_decl, A)   \
    i_construct(X,Y,Z, string, string, array)
declare_mod(EImport, ENode)


#define EStatements_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, array,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, array)
declare_mod(EStatements, ENode)


#define EModel_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, string,      name)                  \
    i_public(  X,Y,Z, num,         size)                  \
    i_public(  X,Y,Z, bool,      integral)              \
    i_public(  X,Y,Z, bool,      realistic)             \
    i_public(  X,Y,Z, AType,       type)                  \
    i_construct(X,Y,Z, string, num, bool, bool, AType)
declare_mod(EModel, ENode)


#define EDeclaration_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, EMember,     target)                \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, EMember)
declare_mod(EDeclaration, ENode)


#define EConstruct_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, EMethod,     method)                \
    i_public(  X,Y,Z, array,       args)                  \
    i_public(  X,Y,Z, EMetaMember, meta_member)           \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, EMethod, array)
declare_mod(EConstruct, ENode)


#define EExplicitCast_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(EExplicitCast, ENode)


#define EPrimitive_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(EPrimitive, ENode)


#define EUndefined_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)
declare_mod(EUndefined, ENode)


#define EParenthesis_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       enode)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(EParenthesis, ENode)


#define ELogicalNot_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       enode)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(ELogicalNot, ENode)


#define EBitwiseNot_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       enode)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(EBitwiseNot, ENode)


#define ERef_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(ERef, ENode)


#define ERefCast_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_public(  X,Y,Z, ENode,       index)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode)
declare_mod(ERefCast, ENode)


#define EIndex_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       target)                \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode)
declare_mod(EIndex, ENode)


#define EAssign_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       target)                \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_public(  X,Y,Z, ENode,       index)                 \
    i_public(  X,Y,Z, bool,      declare)               \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode, ENode, bool)
declare_mod(EAssign, ENode)


#define EIf_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       condition)             \
    i_public(  X,Y,Z, EStatements, body)                  \
    i_public(  X,Y,Z, ENode,       else_body)             \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, EStatements, ENode)
declare_mod(EIf, ENode)


#define EFor_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       init)                  \
    i_public(  X,Y,Z, ENode,       condition)             \
    i_public(  X,Y,Z, ENode,       update)                \
    i_public(  X,Y,Z, ENode,       body)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode, ENode, ENode)
declare_mod(EFor, ENode)


#define EWhile_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       condition)             \
    i_public(  X,Y,Z, ENode,       body)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode)
declare_mod(EWhile, ENode)


#define EDoWhile_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       condition)             \
    i_public(  X,Y,Z, ENode,       body)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode)
declare_mod(EDoWhile, ENode)


#define EBreak_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent)
declare_mod(EBreak, ENode)


#define ELiteralReal_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, f64,         value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, f64)
declare_mod(ELiteralReal, ENode)


#define ELiteralInt_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, i64,         value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, i64)
declare_mod(ELiteralInt, ENode)


#define ELiteralStr_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, string,      value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, string)
declare_mod(ELiteralStr, ENode)


#define ELiteralStrInterp_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, string,      value)                 \
    i_public(  X,Y,Z, A,           args)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, string, A)
declare_mod(ELiteralStrInterp, ENode)


#define ELiteralBool_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, bool,      value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, bool)
declare_mod(ELiteralBool, ENode)


#define ESubProc_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, EMember,     target)                \
    i_public(  X,Y,Z, EMember,     method)                \
    i_public(  X,Y,Z, EIdent,      context_type)          \
    i_public(  X,Y,Z, array,       context_args)          \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, EMember, EMember, EIdent, array)
declare_mod(ESubProc, ENode)


#define ERuntimeType_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent)
declare_mod(ERuntimeType, ENode)


#define EMethodCall_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       target)                \
    i_public(  X,Y,Z, ENode,       method)                \
    i_public(  X,Y,Z, array,       args)                  \
    i_public(  X,Y,Z, array,       arg_temp_members)      \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode, ENode, array)
declare_mod(EMethodCall, ENode)


#define EMethodReturn_schema(X,Y,Z) \
    ENode_schema(X,Y,Z)                                   \
    i_public(  X,Y,Z, EIdent,      type)                  \
    i_public(  X,Y,Z, ENode,       value)                 \
    i_override_m(X,Y,Z, string,    emit) \
    i_construct(X,Y,Z, EIdent, ENode)
declare_mod(EMethodReturn, ENode)


#define EAssignAdd_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignAdd, EAssign)


#define EAssignSub_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignSub, EAssign)


#define EAssignMul_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignMul, EAssign)


#define EAssignDiv_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignDiv, EAssign)


#define EAssignOr_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignOr, EAssign)


#define EAssignAnd_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignAnd, EAssign)


#define EAssignXor_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignXor, EAssign)


#define EAssignShiftR_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignShiftR, EAssign)


#define EAssignShiftL_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignShiftL, EAssign)


#define EAssignMod_schema(X,Y,Z) \
    EAssign_schema(X,Y,Z)
declare_mod(EAssignMod, EAssign)

*/