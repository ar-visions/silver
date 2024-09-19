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

} isilver;

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

#define next_is(s) call(tokens, next_is, s)
#define next()     call(tokens, next)
#define consume()  call(tokens, consume)

LLVMTypeRef dim_type_ref(dim a);

LLVMValueRef type_dbg(type t) {
    itype* f = t->intern;
    return f->dbg;
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
            enumerate(a->args, arg) {
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
            enumerate(a->args, arg) {
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
            enumerate(a->members, member_pair) {
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

void silver_push_member_stack(silver a) {
    isilver* i = a->intern;
    map m = new(map, hsize, 16);
    push(i->member_stack, m);
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
    Tokens tokens = a->tokens;
    silver module = a->module;
    if (!next_is("["))
        return;
    consume();
    a->wrap  = call(module, get_type, str("array"));
    a->shape = new(array); /// shape is there but not given data 
    if (!next_is("]")) {
        type wdef = call(tokens, read_type, module);
        if (wdef) {
            /// must be map
            for (;;) {
                push(a->shape, wdef);
                Token n = peek(tokens);
                if (eq(n, ",")) {
                    wdef = call(tokens, read_type, module);
                    continue;
                }
                break;
            }
        } else {
            /// must be array
            for (;;) {
                i64 dim_size = 0;
                object n = call(tokens, read_numeric);
                verify(n && isa(n) == typeid(i64), "expected integer");
                consume();
                push(a->shape, A_i64(dim_size));
                Token next = peek(tokens);
                if (eq(next, ",")) {
                    consume();
                    continue;
                }
                break;
            }
        }
        Token next = peek(tokens);
        verify (eq(next, "]"), "expected ] in type usage expression");
    }
    consume();
}

void A_test() {
    num         types_len;
    A_f**       types = A_types(&types_len);

    /// iterate through types
    for (num i = 0; i < types_len; i++) {
        A_f* type = types[i];
        if (type->traits & A_TRAIT_ABSTRACT) continue;
        /// for each member of type
        for (num m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            if (mem->member_type & (A_TYPE_PROP)) {
                verify(!mem->required, "found required?");
            }
        }
    }
}

void dim_create_fn(dim a) {
    silver module  = a->module;
    Tokens tokens  = a->tokens;
    map    context = a->context;
    if (next_is("[")) {
        consume();
        map args = new(map, hsize, 8);
        while (true) {
            dim arg = new(dim, module, module, tokens, tokens, context, a->type->members);
            verify (arg, "member failed to read");
            verify (arg->name, "name not set after member recursion");
            if (next_is("]"))
                break;
            verify (next_is(","), "expected separator");
            consume();
            set(args, arg->name, arg);
        }
        consume();
        dim rtype_dim = new(dim,
            module,     module,    type,       a->type,
            depth,      a->depth,  shape,      a->shape,
            wrap,       a->wrap,   context,    context);
        type f_def = new(type,
            name,     str(a->name->chars),  module,   module,
            mdl,      model_function,       rtype,    rtype_dim,
            args,     args,                 info,     a);
        set(context, f_def->name, f_def);
        drop(a->type);
        a->type = hold(f_def);
        array body = new(array, alloc, 32);
        verify (next_is("["), "expected function body");
        int depth = 0;
        do {
            Token token = next();
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

dim dim_init(dim a) {
    a->intern      = A_struct(idim);
    idim*  intern  = a->intern;
    silver module  = a->module;
    Tokens tokens  = a->tokens;
    map    context = a->context;
    if (!a->visibility)
        a->visibility = Visibility_public;
    verify(a->context, "context required");
    if (tokens) {
        call(tokens, push_current);
        if (next_is("static")) {
            consume();
            a->is_static = true;
        }
        /// look for visibility (default is possibly provided)
        for (int i = 1; i < Visibility_type.member_count; i++) {
            type_member_t* enum_v = &Visibility_type.members[i];
            if (next_is(enum_v->name)) {
                consume();
                a->visibility = i;
                break;
            }
        }
        if (!a->is_static) {
            if (next_is("static")) {
                consume();
                a->is_static = true;
            }
        }
        Token  n = peek(tokens);
        print("dim_read: next token = %o", n);
        type def = call(tokens, read_type, module);
        if (!def) {
            print("info: could not read type at position %i", tokens->cursor);
            pop(tokens, false); // we may 'info' here
            return null;
        }
        a->type = hold(def);
        
        // may be [, or alpha-id  (its an error if its neither)
        if (next_is("["))
            idim(a, bind);

        /// members must be named
        verify(call(tokens, next_alpha), "expected identifier for member");

        Token    name = next();
        string s_name = cast(name, string);
        a->name       = hold(s_name);

        if (next_is("["))
            idim(a, create_fn);
        
        pop(tokens, true);
    }
    return a;
}

void silver_parse_top(silver a) {
    isilver* i      = a->intern;
    Tokens   tokens = i->tokens;
    while (cast(tokens, bool)) {
        if (next_is("import")) {
            Import import  = new(Import, module, a, tokens, tokens);
            push(i->imports, import);
            continue;
        } else if (next_is("class")) {
            verify (false, "not implemented");
            //EClass def = new(EClass, tokens, tokens);
            //call(a->defs, set, def->name, def);
            continue;
        } else {
            /// functions are a 'member' of the module
            /// so are classes, but we have a i->defs for the type alone
            /// so we may have class contain in a 'member' of definition type
            /// so its name could be the name of the class and the type would be the same name
            dim member = new(dim,
                module,     a,
                tokens,     tokens,
                context,    i->defs);
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

/*
typedef struct itype {
    LLVMTypeRef         type_ref;
    LLVMValueRef        value_ref;
    LLVMMetadataRef     sub_routine;
    LLVMMetadataRef     dbg;
    LLVMBasicBlockRef   entry;
} itype;

# previous reference code.. (we will not use prestatements)
def parse_statements(self, prestatements = None):
    if prestatements != None:
        block = prestatements.value
    else:
        block = []  # List to hold enode instances
    multiple = self.peek_token() == '['

    tokens, index = self.debug_tokens()
    if multiple:
        self.next_token()  # Consume '['

    depth = 1
    self.push_member_depth()
    while self.peek_token():
        t = self.peek_token()
        if multiple and t == '[':    # here, this is eating my cast <----------- 
            depth += 1
            self.push_member_depth()
            self.consume()
            continue
        global debug
        debug += 1
        n = self.parse_statement()  # Parse the next statement
        assert n is not None, 'expected statement or expression'
        block.append(n)
        if not multiple: break
        if multiple and self.peek_token() == ']':
            if depth > 1:
                self.pop_member_depth()
            self.next_token()  # Consume ']'
            depth -= 1
            if depth == 0:
                break
    self.pop_member_depth()
    # Return a combined operation of type EType_Statements
    return prestatements if prestatements else EStatements(type=None, value=block)
*/

LLVMValueRef silver_build_assignment(silver a) {
    isilver*    i = a->intern;
    Tokens tokens = i->tokens;
    
    // Assume we have tokens for the assignment: x = 5
    Token var = next();  // variable (e.g., 'x')
    Token eq  = next();  // '='
    Token val = next();  // value (e.g., '5')

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

map silver_top_members(silver a) {
    isilver* i = a->intern;
    assert (i->member_stack->len, "stack is empty");
    return i->member_stack->elements[i->member_stack->len - 1];
}

void silver_init_member(silver a, dim member) {
    map members = isilver(a, top_members);
    set(members, member->name, member);
}

void silver_build_function(silver a, type fn) {
    isilver*     i = a->intern;
    itype*       f = fn->intern;
    dim       info = fn->info;
    idim*        m = info->intern;
    
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(m->value_ref, "entry");
    LLVMPositionBuilderAtEnd(i->builder, entry);
    
    isilver(a, push_member_stack);
    /// push args, they are just members
    enumerate(fn->args, e) {
        dim arg_member = e->value;
        isilver(a, init_member, e->value);
        int test = 1;
    }
    Tokens tokens = i->tokens;
    while(peek(tokens)) {
        
    }
    isilver(a, pop_member_stack);
    

    //LLVMValueRef arg1 = LLVMGetParam(m->value_ref, 0);
    //LLVMValueRef arg2 = LLVMGetParam(m->value_ref, 1);
    //LLVMValueRef result = LLVMBuildAdd(i->builder, arg1, arg2, "result");

    LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), 22, 0); // Create a constant integer with value 1
    LLVMValueRef two = LLVMConstInt(LLVMInt64Type(), 22, 0); // Create a constant integer with value 2

    LLVMValueRef result = LLVMBuildAdd(i->builder, one, two, "result"); // Add the two constants

    LLVMBuildRet(i->builder, result);
}

void silver_init(silver a) {
    verify(a->source, "module name not set");

    a->intern       = A_struct(isilver);
    isilver*      i = a->intern;
    i->members      = new(map,   hsize, 32);
    i->member_stack = new(array, alloc, 32);
    i->imports      = new(array, alloc, 32);
    i->source_path  = call(a->source, directory);
    i->source_file  = call(a->source, filename);
    i->libraries_used = new(array);

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
    
    enumerate (i->defs, e) {
        type def = e->value;
        // for each type def with a body to build
        if (def->mdl == model_function) {
            isilver(a, build_function, def);
        } else if (def->mdl == model_class) {
            enumerate_ (def->members, m) {
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
