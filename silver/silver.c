#include <silver>
#include <tokens>
#include <import>

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <clang-c/Index.h>

item first_key_value(map ordered) {
    verify (len(ordered), "no items");
    return (pair)ordered->first->value;
}

typedef struct isilver {
    LLVMModuleRef       module;
    LLVMContextRef      llvm_context;
    LLVMBuilderRef      builder;
    LLVMMetadataRef     file;
    LLVMMetadataRef     compile_unit;
    LLVMDIBuilderRef    dbg;
    LLVMMetadataRef     scope;
    array               imports;        // processed imports
    Tokens              tokens;         // tokens state with cursor and stack
    array               member_stack;   // working member stack used as we parse
    map                 defs;           // the types we describe in here
    map                 members;        // module members in order
    string              source_file;
    path                source_path;
    array               main_symbols;
    array               compiled_objects;
    array               libraries_used;
    map                 include;
    map                 type_refs;
    map                 operators;
    int                 expr_level;
} isilver;

#define builder_ref(module) ((struct isilver*)a->intern)->builder;
#define     dbg_ref(module) ((struct isilver*)a->intern)->dbg;
#define   scope_ref(module) ((struct isilver*)a->intern)->scope;
#define context_ref(module) ((struct isilver*)a->intern)->context;

typedef struct itype {
    LLVMTypeRef         type_ref;
    //LLVMValueRef      value_ref; -- in dim (member space is always there for functions!)
    LLVMMetadataRef     sub_routine;
    LLVMMetadataRef     dbg;
    LLVMBasicBlockRef   entry;
} itype;

typedef struct idim {
    LLVMValueRef        value_ref;
} idim;



#define isilver(I,N,...)     silver_##N(I, ## __VA_ARGS__)
#define ifunction(I,N,...) function_##N(I, ## __VA_ARGS__)
#define itype(I,N,...)         type_##N(I, ## __VA_ARGS__)
#define idim(I,N,...)           dim_##N(I, ## __VA_ARGS__)

#undef  peek
#define tokens(...)     call(i->tokens, __VA_ARGS__) /// no need to pass around the same tokens arg when the class supports a stack

LLVMTypeRef dim_type_ref(dim a);

LLVMValueRef type_dbg(type t) {
    itype* f = t->intern;
    return f->dbg;
}

string type_ref_key(LLVMTypeRef type_ref) {
    return format("%p", type_ref);
}

void type_associate(type a, LLVMTypeRef type_ref) {
    isilver* i = a->module->intern;
    string key = type_ref_key(type_ref);
    assert (!call(i->type_refs, contains, key), "already associated");
    set(i->type_refs, key, a);
}

dim type_find_member(type a, string key) {
    if (!a->members) return null;
    return get(a->members, key);
}

void type_init(type a) {
    verify(a->module, "module not set");
    verify(a->name,   "name not set");

    a->intern  = A_struct(itype);
    a->members = new(map, hsize, 8);
    isilver* i = a->module->intern;
    itype*   f = a->intern;
    dim   info = a->info;
    idim*    m = info ? info->intern : null;
    bool handled_members = false;

    switch (a->mdl) {
        case model_class:
            verify(false, "not implemented");
            break;
        
        case model_function: {
            verify(a->rtype,  "rtype");
            verify(a->args,   "args");
            int n_args = a->args->count;
            int arg_index = 0;
            LLVMTypeRef* arg_types = calloc(n_args, sizeof(LLVMTypeRef));
            cstr*        arg_names = calloc(n_args, sizeof(cstr));
            
            print("making function for %o", a->name);
            pairs(a->args, arg) {
                LLVMTypeRef ref = dim_type_ref(idx_1(a->args, sz, (sz)arg_index));
                arg_types[arg_index] = ref;
                arg_index++;
            }

            LLVMTypeRef return_ref = dim_type_ref(a->rtype);
            f->type_ref  = LLVMFunctionType(return_ref, arg_types, arg_index, false);
            m->value_ref = LLVMAddFunction(i->module, a->name->chars, f->type_ref);
            LLVMSetLinkage(m->value_ref, info->visibility == Visibility_public
                ? LLVMExternalLinkage : LLVMInternalLinkage);

            // set arg names
            arg_index = 0;
            pairs(a->args, arg) {
                string arg_name = arg->key;
                dim    arg_type = arg->value;
                AType     arg_t = isa(arg_type);
                verify(arg_t == typeid(dim), "type mismatch");
                LLVMValueRef param = LLVMGetParam(m->value_ref, arg_index);
                LLVMSetValueName2(param, arg_name->chars, arg_name->len);
                arg_index++;
            }

            free(arg_types);
            free(arg_names);
            break;
        }
        case model_bool:   f->type_ref = LLVMInt1Type  (); break;
        case model_i8:     f->type_ref = LLVMInt8Type  (); break;
        case model_i16:    f->type_ref = LLVMInt16Type (); break;
        case model_i32:    f->type_ref = LLVMInt32Type (); break;
        case model_i64: {
            f->type_ref = LLVMInt64Type ();
            print("f->type_ref = %p", f->type_ref);
            break;
        }
        case model_u8:     f->type_ref = LLVMInt8Type  (); break;
        case model_u16:    f->type_ref = LLVMInt16Type (); break;
        case model_u32:    f->type_ref = LLVMInt32Type (); break;
        case model_u64:    f->type_ref = LLVMInt64Type (); break;
        case model_f32:    f->type_ref = LLVMFloatType (); break;
        case model_f64:    f->type_ref = LLVMDoubleType(); break;
        case model_void:   f->type_ref = LLVMVoidType  (); break;
        case model_cstr:   f->type_ref = LLVMPointerType(LLVMInt8Type(), 0); break;
        case model_typedef: {
            verify (a->origin && isa(a->origin) == typeid(dim), "origin must be a reference");
            f->type_ref = dim_type_ref(a->origin);
            if (i->dbg) {
                verify(type_dbg(a->origin), "no debug info set on origin");
                f->dbg = LLVMDIBuilderCreateTypedef(
                    i->dbg, type_dbg(a->origin), a->name->chars, len(a->name),
                    i->file, a->token ? a->token->line : 0, i->scope, LLVMDIFlagZero);
            }
            break;
        }
        case model_struct: {
            LLVMTypeRef* member_types = calloc(len(a->members), sizeof(LLVMTypeRef));
            int index = 0;
            pairs(a->members, member_pair) {
                dim member_r = member_pair->value;
                verify(isa(member_r) == typeid(dim), "mismatch");
                member_types[index] = dim_type_ref(member_r);
                index++;
            }
            f->type_ref = LLVMStructCreateNamed(LLVMGetGlobalContext(), a->name);
            LLVMStructSetBody(f->type_ref, member_types, index, 0);
            handled_members = true;
            break;
        }
        case model_union: {
            verify (false, "not implemented");
            break;
        }
        itype(a, associate, f->type_ref);
    }
    verify (!call(a->members, count) || handled_members, "members given and not processed");
}


// function to process declarations
enum CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl) {
        CXString funcName = clang_getCursorSpelling(cursor);
        printf("function: %s\n", clang_getCString(funcName));


        clang_disposeString(funcName);
    }
    return CXChildVisit_Recurse;
}

path silver_source_path(silver a) {
    isilver* i = a->intern;
    return i->source_path;
}

type silver_get_type(silver a, string key) {
    isilver* i = a->intern;
    return get(i->defs, key);
}

dim silver_get_member(silver a, string key) {
    isilver* i = a->intern;
    return get(i->members, key);
}

map silver_top_members(silver a) {
    isilver* i = a->intern;
    assert (i->member_stack->len, "stack is empty");
    return i->member_stack->elements[i->member_stack->len - 1];
}

/// lookup value ref for member in stack
LLVMValueRef silver_member_stack_lookup(silver a, string name) {
    isilver* i = a->intern;
    for (int m = i->member_stack->len - 1; m >= 0; m--) {
        map members = i->member_stack->elements[m];
        object f = get(members, name);
        if    (f) return f;
    }
    return null;
}

map silver_push_member_stack(silver a) {
    isilver* i = a->intern;
    map members = new(map, hsize, 16);
    push(i->member_stack, members);
    return members;
}

void silver_pop_member_stack(silver a) {
    isilver* i = a->intern;
    pop(i->member_stack);
}

/// return a map of defs found by their name (we can isolate the namespace this way by having separate maps)
map silver_include(silver a, string include) {
    isilver* i         = a->intern;
    string   install   = format("%o/include", a->install);
    path     full_path = null;
    symbol   ipaths[]  = {
        install->chars,
        "/usr/include"
    };
    for (int i = 0; i < sizeof(ipaths) / sizeof(symbol); i++) {
        path r = form(path, "%s/%o", ipaths[i], include);
        if (call(r, exists)) {
            full_path = r;
            break;
        }
    }
    verify (full_path, "include path not found for %o", include);
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, full_path->chars, NULL, 0, NULL, 0, CXTranslationUnit_None);

    verify(unit, "unable to parse translation unit %o", include);
    
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, visit, NULL);
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    // Here you'd use LLVMAddFunction for each function extracted

    return 0;
}

void silver_set_line(silver a, i32 line, i32 column) {
    isilver* i = a->intern;
    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        i->llvm_context, line, column, i->scope, null);
    LLVMSetCurrentDebugLocation2(i->dbg, loc);
}

void silver_llflag(silver a, symbol flag, i32 ival) {
    isilver*        i = a->intern;
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(i->llvm_context), ival, 0));
    LLVMAddModuleFlag(
        i->module, LLVMModuleFlagBehaviorError, flag, strlen(flag), v);
}

void silver_write(silver a) {
    isilver* i = a->intern;
    cstr err = NULL;
    if (LLVMVerifyModule(i->module, LLVMPrintMessageAction, &err))
        fault("Error verifying module: %s", err);
    else
        print("module verified");

    if (!LLVMPrintModuleToFile(i->module, "a.ll", &err))
        print("generated IR");
    else
        fault("LLVMPrintModuleToFile failed");

    symbol bc = "a.bc";
    if (LLVMWriteBitcodeToFile(i->module, bc) != 0)
        fault("LLVMWriteBitcodeToFile failed");
    else
        print("bitcode written to %s", bc);
}

void silver_destructor(silver a) {
    isilver* i = a->intern;
    LLVMDisposeBuilder(i->builder);
    LLVMDisposeDIBuilder(i->dbg);
    LLVMDisposeModule(i->module);
}

/// this reads a num var
/// --------------------
/// num[2] name
/// num[2] name [ num arg1, ... num[] extra ] expr | [ statements ]
/// map[string, object] name   # good idea to always define that the value is object, and no default 'map' usage?
/// array[meta][string, object] name   #
/// --------------------

LLVMTypeRef dim_type_ref(dim a) {
    itype*       f = a->type->intern;
    LLVMTypeRef  t = f->type_ref;
    for (int i = 0; i < a->depth; i++)
        t = LLVMPointerType(t, 0);
    return t;
}

LLVMValueRef dim_value_ref(dim a) {
    idim* m = a->intern;
    assert(m->value_ref, "value_ref not set when read\n");
    return m->value_ref;
}

void dim_bind(dim a) {
    silver module = a->module;
    isilver* i = module->intern;
    if (!tokens(next_is, "["))
        return;
    tokens(consume);
    a->wrap  = call(module, get_type, str("array"));
    a->shape = new(array); /// shape is there but not given data 
    if (!tokens(next_is, "]")) {
        type wdef = tokens(read_type, module);
        if (wdef) {
            /// must be map
            for (;;) {
                push(a->shape, wdef);
                Token n = tokens(peek);
                if (eq(n, ",")) {
                    wdef = tokens(read_type, module);
                    continue;
                }
                break;
            }
        } else {
            /// must be array
            for (;;) {
                i64 dim_size = 0;
                object n = tokens(read_numeric);
                verify(n && isa(n) == typeid(i64), "expected integer");
                tokens(consume);
                push(a->shape, A_i64(dim_size));
                Token next = tokens(peek);
                if (eq(next, ",")) {
                    tokens(consume);
                    continue;
                }
                break;
            }
        }
        verify (tokens(next_is, "]"), "expected ] in type usage expression");
    }
    tokens(consume);
}

void dim_create_fn(dim a) {
    silver module  = a->module;
    map    members = a->context;
    isilver*  i    = module->intern;
    if (tokens(next_is, "[")) {
        tokens(consume);
        map args = new(map, hsize, 8);
        while (true) {
            dim arg = dim_parse(module, a->type->members);
            verify (arg, "member failed to read");
            verify (arg->name, "name not set after member recursion");
            if (tokens(next_is, "]"))
                break;
            verify (tokens(next_is, ","), "expected separator");
            tokens(consume);
            set(args, arg->name, arg);
        }
        tokens(consume);
        dim rtype_dim = new(dim,
            module,     module,    type,       a->type,
            depth,      a->depth,  shape,      a->shape,
            wrap,       a->wrap,   context,    members);
        type f_def = new(type,
            name,     str(a->name->chars),  module,   module,
            mdl,      model_function,       rtype,    rtype_dim,
            args,     args,                 info,     a);
        //assert(count(members, f_def->name) == 0, "duplicate member: %o", f_def->name);
        set(members, f_def->name, f_def);
        drop(a->type);
        a->type = hold(f_def);
        array body = new(array, alloc, 32);
        verify (tokens(next_is, "["), "expected function body");
        int depth = 0;
        do {
            Token token = tokens(next);
            verify (token, "expected end of function body ( too many ['s )");
            push(body, token);
            if (eq(token, "["))
                depth++;
            else if (eq(token, "]"))
                depth--;
        } while (depth > 0);
        a->type->body = new(Tokens, cursor, 0, tokens, body);
    }
}

void dim_init(dim a) {
    a->intern      = A_struct(idim);
    idim*  intern  = a->intern;
    silver module  = a->module;
    map    context = a->context;
    isilver* i     = module->intern;
    if (!a->visibility)
        a->visibility = Visibility_public;
}

/// this now looks up from stack, to find a member we already used
/// this should be easier to resolve code than we did in python
dim dim_parse(silver module, map context) {
    isilver* i = module->intern;
    tokens(push_current);
    string alpha;
    if((alpha = tokens(next_alpha))) {
        if (alpha) {
            /// find in stack
            dim cached = isilver(module, member_stack_lookup, alpha);
            verify (cached, "unknown identifier: %o", alpha);
            cached->cached = true;
            return cached;
        }
    }
    dim      a = new(dim, module, module, context, context);
    
    if (tokens(next_is, "static")) {
        tokens(consume);
        a->is_static = true;
    }
    /// look for visibility (default is possibly provided)
    for (int m = 1; m < Visibility_type.member_count; m++) {
        type_member_t* enum_v = &Visibility_type.members[m];
        if (tokens(next_is, enum_v->name)) {
            tokens(consume);
            a->visibility = m;
            break;
        }
    }
    if (!a->is_static) {
        if (tokens(next_is, "static")) {
            tokens(consume);
            a->is_static = true;
        }
    }
    Token  n = tokens(peek);
    print("dim_read: next token = %o", n);
    type def = tokens(read_type, module);
    if (!def) {
        print("info: could not read type at position %o", tokens(location));
        tokens(pop_state, false); // we may 'info' here
        return null;
    }
    a->type = hold(def);
    
    // may be [, or alpha-id  (its an error if its neither)
    if (tokens(next_is, "["))
        idim(a, bind);

    /// members must be named
    verify(tokens(next_alpha), "expected identifier for member");

    Token    name = tokens(next);
    string s_name = cast(name, string);
    a->name       = hold(s_name);

    if (tokens(next_is, "["))
        idim(a, create_fn);
    
    tokens(pop_state, true);
    return a;
}

void silver_parse_top(silver a) {
    isilver* i      = a->intern;
    Tokens   tokens = i->tokens;
    while (cast(tokens, bool)) {
        if (tokens(next_is, "import")) {
            Import import  = new(Import, module, a, tokens, tokens);
            push(i->imports, import);
            continue;
        } else if (tokens(next_is, "class")) {
            verify (false, "not implemented");
            //EClass def = new(EClass, tokens, tokens);
            //call(a->defs, set, def->name, def);
            continue;
        } else {
            /// functions are a 'member' of the module
            /// so are classes, but we have a i->defs for the type alone
            /// so we may have class contain in a 'member' of definition type
            /// so its name could be the name of the class and the type would be the same name
            dim member = dim_parse(a, i->defs);
            string key = member->name ? member->name : (string)format("$m%i", call(i->defs, count));
            set(i->members, key, member);
        }
    }
}

void silver_define_C99(silver a) {
    isilver* i    = a->intern;
    map      defs = i->defs = new(map, hsize, 64);
    
    set(defs, str("bool"),    new(type, module, a, name, str("bool"), mdl, model_bool, imported, typeid(bool)));
    set(defs, str("i8"),      new(type, module, a, name, str("i8"),   mdl, model_i8,   imported, typeid(i8)));
    set(defs, str("i16"),     new(type, module, a, name, str("i16"),  mdl, model_i16,  imported, typeid(i16)));
    set(defs, str("i32"),     new(type, module, a, name, str("i32"),  mdl, model_i32,  imported, typeid(i32)));
    set(defs, str("i64"),     new(type, module, a, name, str("i64"),  mdl, model_i64,  imported, typeid(i64)));
    set(defs, str("u8"),      new(type, module, a, name, str("u8"),   mdl, model_u8,   imported, typeid(u8)));
    set(defs, str("u16"),     new(type, module, a, name, str("u16"),  mdl, model_u16,  imported, typeid(u16)));
    set(defs, str("u32"),     new(type, module, a, name, str("u32"),  mdl, model_u32,  imported, typeid(u32)));
    set(defs, str("u64"),     new(type, module, a, name, str("u64"),  mdl, model_u64,  imported, typeid(u64)));
    set(defs, str("void"),    new(type, module, a, name, str("void"), mdl, model_void, imported, typeid(none)));
    set(defs, str("symbol"),  new(type, module, a, name, str("symbol"), mdl, model_cstr, imported, typeid(symbol)));
    set(defs, str("cstr"),    new(type, module, a, name, str("cstr"),   mdl, model_cstr, imported, typeid(cstr)));
    set(defs, str("int"),     get(defs, str("i64")));
    set(defs, str("uint"),    get(defs, str("u64")));
}

bool silver_build_dependencies(silver a) {
    isilver* i = a->intern;
    each(i->imports, Import, im) {
        process(im);
        switch (im->import_type) {
            case ImportType_source:
                if (len(im->main_symbol))
                    push(i->main_symbols, im->main_symbol);
                each(im->source, string, source) {
                    // these are built as shared library only, or, a header file is included for emitting
                    if (call(source, has_suffix, ".rs") || call(source, has_suffix, ".h"))
                        continue;
                    string buf = format("%o/%s.o", a->install, source);
                    push(i->compiled_objects, buf);
                }
            case ImportType_library:
            case ImportType_project:
                concat(i->libraries_used, im->links);
                break;
            case ImportType_includes:
                break;
            default:
                verify(false, "not handled: %i", im->import_type);
        }
    }
    return true;
}

LLVMValueRef silver_build_assignment(silver a) {
    isilver*    i = a->intern;
    Tokens tokens = i->tokens;
    
    // Assume we have tokens for the assignment: x = 5
    Token var = tokens(next);  // variable (e.g., 'x')
    Token eq  = tokens(next);  // '='
    Token val = tokens(next);  // value (e.g., '5')

    // Example LLVM IR for a variable assignment
    string s_var = cast(var, string);
    LLVMValueRef lhs = isilver(a, member_stack_lookup, s_var);  // Get the LHS (variable)
    assert(lhs, "member lookup failed for var: %o", var);
    LLVMValueRef rhs = LLVMConstInt(LLVMInt32Type(), atoi(val->chars), 0);  // Convert value to LLVM constant
    
    // Build the store instruction: store i32 5, i32* %x
    return LLVMBuildStore(i->builder, rhs, lhs);
}

LLVMValueRef silver_build_statements(silver a) {
    LLVMValueRef result = null;
    return result;
}

LLVMValueRef silver_parse_return(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_break(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_for(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_while(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_if_else(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_do_while(silver a) {
    isilver* i = a->intern;
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

type silver_type_from_llvm(silver a, LLVMTypeRef type_ref) {
    isilver* i = a->intern;
    string key = type_ref_key(type_ref);
    type   res = get(i->type_refs, key);
    assert(res, "expected associated type");
    return res;
}

type preferred_type(silver a, type t0, type t1) {
    if (t0 == t1) return t0;
    bool f0 = t0->mdl == model_f32 || t0->mdl == model_f64;
    bool f1 = t1->mdl == model_f32 || t1->mdl == model_f64;
    if (f0) {
        if (f1)
            return (t1->mdl == model_f64) ? t1 : t0;
        return t0;
    }
    if (f1)
        return t1;
    if (t0->mdl > t1->mdl)
        return t0;
    return t1;
}

typedef LLVMValueRef(*builder_fn)(silver, type, type, type, LLVMValueRef, LLVMValueRef);
typedef LLVMValueRef(*parse_fn)(silver, cstr, cstr, builder_fn, builder_fn);

#define resolve_type(llvm_type_ref) isilver(a, type_from_llvm, llvm_type_ref);

LLVMValueRef parse_ops(
        silver a, parse_fn descent, symbol op0, symbol op1, builder_fn b0, builder_fn b1) {
    isilver* i = a->intern;
    LLVMValueRef left = descent(a, op0, op1, b0, b1);
    while(tokens(next_is, op0) || tokens(next_is, op1)) {
        tokens(consume);
        LLVMValueRef right = descent(a, op0, op1, b0, b1);
        bool         use0 = tokens(next_is, op0);
        symbol    op_code = use0 ? op0 : op1;
        builder_fn    bfn = use0 ? b0  : b1;

        assert (call(i->operators, contains, str(op_code)), "op (%s) not registered", op_code);
        string  op_name   = get(i->operators, str(op_code));
        type    left_type = resolve_type(left);
        dim     method    = itype(left_type, find_member, op_name);

        if (method) {
            assert (false, "not implemented: overload for %o", method->name);
        }
        /*
        if (method) {
            assert len(method->type->args) == 1, 'operators must take in 1 argument'
            if self.convertible(right.type, method.args[0].type):
                return EMethodCall(type=method.type, target=left,
                    method=method, args=[self.convert_enode(right, method.args[0].type)])


        }
        */
        /// fix this (we will want to use dim, or, merge refs into type (unlikely!)
        /// we may return an inline instance of type with meta args and such to use, thats dim.
        /// its more than just a definition!  its not a 'new type' when you have int* const**
        ///
        type r_left   = resolve_type(LLVMTypeOf(left));
        type r_right  = resolve_type(LLVMTypeOf(right));
        type type_out = preferred_type(a, r_left, r_right); /// should work for primitives however we must handle more in each
        left          = bfn(a, type_out, r_left, r_right, left, right); 
    }
    return left;
}

LLVMValueRef op_is(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    assert(m->type->mdl == model_bool, "inherits operator must return a boolean type");
    assert(LLVMGetTypeKind(LLVMTypeOf(v0))  == LLVMFunctionTypeKind &&
           LLVMGetTypeKind(LLVMTypeOf(v1)) == LLVMFunctionTypeKind, 
           "is operator expects function type or initializer");
    bool equals = t0 == t1;
    return LLVMConstInt(LLVMInt1Type(), equals, 0);
}

LLVMValueRef op_inherits(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    assert(m->type->mdl == model_bool, "inherits operator must return a boolean type");
    assert(LLVMGetTypeKind(LLVMTypeOf(v0))  == LLVMFunctionTypeKind &&
           LLVMGetTypeKind(LLVMTypeOf(v1)) == LLVMFunctionTypeKind, 
           "is operator expects function type or initializer");
    bool      equals = t0 == t1;
    LLVMValueRef yes = LLVMConstInt(LLVMInt1Type(), 1, 0);
    LLVMValueRef no  = LLVMConstInt(LLVMInt1Type(), 0, 0);
    type cur = t0;
    while (cur) {
        if (cur == t1)
            return yes;
        cur = cur->origin;
    }
    return no;
}

LLVMValueRef op_add(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    return LLVMBuildAdd(i->builder, v0, v1, "add");
}

LLVMValueRef op_sub(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    return LLVMBuildSub(i->builder, v0, v1, "add");
}

LLVMValueRef op_mul(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    assert (false, "op_mul: implement more design here");
    return null;
}

LLVMValueRef op_div(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    assert (false, "op_div: implement more design here");
    return null;
}

LLVMValueRef op_eq(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(i->builder, LLVMIntEQ,   v0, v1, "eq-i");
    else if (f0) return LLVMBuildFCmp(i->builder, LLVMRealOEQ, v0, v1, "eq-f");
    else {
        assert (false, "op_eq: implement more design here");
        return null;
    }
}

LLVMValueRef op_not_eq(silver a, dim m, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    isilver* i = a->intern;
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(i->builder, LLVMIntNE,   v0, v1, "not-eq-i");
    else if (f0) return LLVMBuildFCmp(i->builder, LLVMRealONE, v0, v1, "not-eq-f");
    else {
        assert (false, "op_not_eq: implement more design here");
        return null;
    }
}

LLVMValueRef silver_parse_primary(silver a);

LLVMValueRef silver_parse_eq  (silver a) { return parse_ops(a, silver_parse_primary, "==", "!=",         op_eq,  op_not_eq); }
LLVMValueRef silver_parse_is  (silver a) { return parse_ops(a, silver_parse_eq,      "is", "inherits",   op_is,  op_is);     }
LLVMValueRef silver_parse_mult(silver a) { return parse_ops(a, silver_parse_is,      "*",  "/",          op_is,  op_is);     }
LLVMValueRef silver_parse_add (silver a) { return parse_ops(a, silver_parse_mult,    "+",  "-",          op_add, op_sub);    }

LLVMValueRef silver_parse_expression(silver a) {
    isilver* i = a->intern;
    ///
    i->expr_level++;
    LLVMValueRef vr = isilver(a, parse_add);
    i->expr_level--;
    return vr;
}

LLVMValueRef silver_parse_primary(silver a) {
    isilver* i = a->intern;

    // handle the logical NOT operator (e.g., '!')
    if (tokens(next_is, "!") || tokens(next_is, "not")) {
        tokens(consume); // Consume '!' or 'not'
        LLVMValueRef expr = silver_parse_expression(a); // Parse the following expression
        return LLVMBuildNot(i->builder, expr, "logical_not");
    }

    // bitwise NOT operator
    if (tokens(next_is, "~")) {
        tokens(consume); // Consume '~'
        LLVMValueRef expr = silver_parse_expression(a);
        return LLVMBuildNot(i->builder, expr, "bitwise_not");
    }

    // 'typeof' operator
    if (tokens(next_is, "typeof")) {
        tokens(consume); // Consume 'typeof'
        assert(tokens(next_is, "["), "Expected '[' after 'typeof'");
        tokens(consume); // Consume '['
        LLVMValueRef type_ref = silver_parse_expression(a); // Parse the type expression
        assert(tokens(next_is, "]"), "Expected ']' after type expression");
        tokens(consume); // Consume ']'
        return type_ref; // Return the type reference
    }

    // 'cast' operator
    if (tokens(next_is, "cast")) {
        tokens(consume); // Consume 'cast'
        dim cast_ident = tokens(read_type, a); // Read the type to cast to
        assert(tokens(next_is, "["), "Expected '[' for cast");
        tokens(consume); // Consume '['
        LLVMValueRef expr = silver_parse_expression(a); // Parse the expression to cast
        tokens(consume); // Consume ']'
        if (cast_ident) {
            return LLVMBuildCast(i->builder, LLVMBitCast, cast_ident, expr, "explicit_cast");
        }
    }

    // 'ref' operator (reference)
    if (tokens(next_is, "ref")) {
        tokens(consume); // Consume 'ref'
        LLVMValueRef expr = silver_parse_expression(a);
        return LLVMBuildLoad(i->builder, expr, "ref_expr"); // Build the reference
    }

    // numeric constants
    object v_num = tokens(next_numeric);
    if (v_num) {
        tokens(consume); // Consume the numeric token
        if (isa(v_num) == typeid(i8))  return LLVMConstInt( LLVMInt8Type(),  *( i8*)v_num, 0);
        if (isa(v_num) == typeid(i16)) return LLVMConstInt(LLVMInt16Type(),  *(i16*)v_num, 0);
        if (isa(v_num) == typeid(i32)) return LLVMConstInt(LLVMInt32Type(),  *(i32*)v_num, 0);
        if (isa(v_num) == typeid(i64)) return LLVMConstInt(LLVMInt64Type(),  *(i64*)v_num, 0);
        if (isa(v_num) == typeid(u8))  return LLVMConstInt( LLVMInt8Type(),  *( u8*)v_num, 0);
        if (isa(v_num) == typeid(u16)) return LLVMConstInt(LLVMInt16Type(),  *(u16*)v_num, 0);
        if (isa(v_num) == typeid(u32)) return LLVMConstInt(LLVMInt32Type(),  *(u32*)v_num, 0);
        if (isa(v_num) == typeid(u64)) return LLVMConstInt(LLVMInt64Type(),  *(u64*)v_num, 0);
        if (isa(v_num) == typeid(f32)) return LLVMConstInt(LLVMFloatType(),  *(f32*)v_num, 0);
        if (isa(v_num) == typeid(f64)) return LLVMConstInt(LLVMDoubleType(), *(f64*)v_num, 0);
        assert (false, "numeric literal not handling primitive: %s", isa(v_num)->name);
    }

    // strings
    Token str_token = tokens(next_string);
    if (str_token) {
        tokens(consume);
        return LLVMBuildGlobalString(i->builder, str_token->chars, "str");
    }

    // boolean values
    object v_bool = tokens(next_bool);
    if (v_bool) {
        tokens(consume);
        if (isa(v_bool) == typeid(bool)) return LLVMConstInt(LLVMInt1Type(), *(bool*)v_bool, 0);
        return LLVMConstInt(LLVMInt1Type(), tokens(next_is, "true"), 0);
    }

    // parenthesized expressions
    if (tokens(next_is, "[")) {
        tokens(consume);
        LLVMValueRef expr = silver_parse_expression(a); // Parse the expression
        assert(tokens(next_is, "]"), "Expected closing parenthesis");
        tokens(consume);
        return expr;
    }

    // handle identifiers (variables or function calls)
    Token ident = tokens(next_alpha);
    if (ident) {
        tokens(consume); // Consume the identifier token
        LLVMValueRef var = silver_member_stack_lookup(a, ident->chars); // Look up variable
        assert(var, "Variable %s not found", ident->chars);
        return var;
    }

    fault("unexpected token %o in primary expression", tokens(peek));
    return null;
}

LLVMValueRef silver_parse_statement(silver a) {
    isilver* i = a->intern;
    Token t = tokens(peek);
    if (tokens(next_is, "return")) return isilver(a, parse_return);
    if (tokens(next_is, "break"))  return isilver(a, parse_break);
    if (tokens(next_is, "for"))    return isilver(a, parse_for);
    if (tokens(next_is, "while"))  return isilver(a, parse_while);
    if (tokens(next_is, "if"))     return isilver(a, parse_if_else);
    if (tokens(next_is, "do"))     return isilver(a, parse_do_while);

    map members = isilver(a, top_members);
    dim member  = dim_parse(a, members);
    if (member->cached) {
        /// we can know if member was found in stack
        /// it would not be a 'new' instance in that case
        /// this was a critical juncture for A-type because we were going to 
        /// allow init to modify or nullify the object.  not a good idea though!
        /// imagine if any 'new' could do this?
        /// we also cleaned up the way statics are used in A-type
        /// there was no macro convention to use them
        print("found a cached member: %o", member->name);
    } else {
        print("new member: %o of type %o", member->name, member->type->name);
    }
    /// if the member is cached we may obtain the same value that we have in memory, this brings ambiguity to 'new' though
    /// still, we may convert this to dim(parse, ...
    if (cast(member, bool)) {
        if (member->type->mdl == model_function) {
            /// expect [
        }
    }
    LLVMValueRef vr = null;
    return vr;
}

LLVMValueRef silver_parse_statements(silver a) {
    isilver* i = a->intern;
    /// 
    map  members    = isilver(a, push_member_stack);
    bool multiple   = tokens(next_is, "[");
    if  (multiple)    tokens(consume);
    int  depth      = 1;
    LLVMValueRef vr = null;
    ///
    while(tokens(cast_bool)) {
        if(multiple && tokens(next_is, "[")) {
            depth += 1;
            isilver(a, push_member_stack);
            tokens(consume);
        }
        vr = isilver(a, parse_statement);
        if(!multiple) break;
        if(tokens(next_is, "]")) {
            if (depth > 1)
                isilver(a, pop_member_stack);
            tokens(next);
            if ((depth -= 1) == 0) break;
        }
    }
    isilver(a, pop_member_stack);
    return vr;
}

static void silver_build_function(silver a, type fn) {
    isilver*     i = a->intern;
    itype*       f = fn->intern;
    dim       info = fn->info;
    idim*        m = info->intern;

    tokens(push_state, fn->body->tokens, fn->body->cursor);
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(m->value_ref, "entry");
    LLVMPositionBuilderAtEnd(i->builder, entry);
    
    /// push args, they are constant members (we need 'const' flag on them when we read from args)
    map members = isilver(a, push_member_stack);
    concat(members, fn->args);
    
    /// iterate through tokens to parse statements
    isilver(a, parse_statements);
    isilver(a, pop_member_stack);
    
    //LLVMValueRef arg1 = LLVMGetParam(m->value_ref, 0);
    //LLVMValueRef arg2 = LLVMGetParam(m->value_ref, 1);
    //LLVMValueRef result = LLVMBuildAdd(i->builder, arg1, arg2, "result");
    LLVMValueRef one    = LLVMConstInt(LLVMInt64Type(), 22, 0); // Create a constant integer with value 1
    LLVMValueRef two    = LLVMConstInt(LLVMInt64Type(), 22, 0); // Create a constant integer with value 2
    LLVMValueRef result = LLVMBuildAdd(i->builder, one, two, "result"); // Add the two constants
    LLVMBuildRet(i->builder, result);

    tokens(pop_state, false);
}

static void silver_init(silver a) {
    verify(a->source, "module name not set");

    a->intern       = A_struct(isilver);
    isilver*      i = a->intern;
    i->member_stack = new(array, alloc, 32);
    i->members      = isilver(a, push_member_stack); /// our base members is the first stack item that we keep after building
    i->imports      = new(array, alloc, 32);
    i->source_path  = call(a->source, directory);
    i->source_file  = call(a->source, filename);
    i->libraries_used = new(array);
    i->type_refs    = new(map, hsize, 64);
    i->operators    = map_of(
        "+",        str("add"),
        "-",        str("sub"),
        "*",        str("mul"),
        "/",        str("div"),
        "||",       str("or"),
        "&&",       str("and"),
        "^",        str("xor"),
        ">>",       str("right"),
        "<<",       str("left"),
        ":",        str("assign"),
        "=",        str("assign"),
        "+=",       str("assign_add"),
        "-=",       str("assign_sub"),
        "*=",       str("assign_mul"),
        "/=",       str("assign_div"),
        "|=",       str("assign_or"),
        "&=",       str("assign_and"),
        "^=",       str("assign_xor"),
        ">>=",      str("assign_right"),
        "<<=",      str("assign_left"),
        "==",       str("compare_equal"),
        "!=",       str("compare_not"),
        "%=",       str("mod_assign"),
        "is",       str("is"),
        "inherits", str("inherits"), null
    );

    print("LLVM Version: %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);

    path  full_path = form(path, "%o/%o", i->source_path, i->source_file);
    verify(call(full_path, exists), "source (%o) does not exist", full_path);

    i->module       = LLVMModuleCreateWithName(i->source_file->chars);
    i->llvm_context = LLVMGetModuleContext(i->module);
    i->dbg          = LLVMCreateDIBuilder(i->module);
    i->builder      = LLVMCreateBuilder();

    isilver(a, llflag, "Dwarf Version",      5);
    isilver(a, llflag, "Debug Info Version", 3);

    i->file = LLVMDIBuilderCreateFile( // create a file reference (the source file for debugging)
        i->dbg,
        cast(i->source_file, cstr), cast(i->source_file, sz),
        cast(i->source_path, cstr), cast(i->source_path, sz));
    i->compile_unit = LLVMDIBuilderCreateCompileUnit(
        i->dbg, LLVMDWARFSourceLanguageC, i->file,
        cast(i->source_file, cstr),
        cast(i->source_file, sz), 0, "", 0,
        3, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "/", 1, "", 0);

    i->tokens = new(Tokens, file, full_path);

    isilver(a, define_C99);
    isilver(a, parse_top);
    isilver(a, include, str("stdio.h"));
    isilver(a, build_dependencies);
    
    pairs (i->defs, e) {
        type def = e->value;
        // for each type def with a body to build
        if (def->mdl == model_function) {
            isilver(a, build_function, def);
        } else if (def->mdl == model_class) {
            pairs_ (def->members, m) {
                dim member = m->value;
                if (member->type->mdl == model_function)
                    isilver(a, build_function, member->type);
            }
        }
    }

    isilver(a, write); /// write module we just read in; if we can get away with bulk LLVM code it may be alright to stay direct
}

define_enum(model)
define_enum(Visibility)

define_class(type)
define_class(dim)
define_class(silver)
