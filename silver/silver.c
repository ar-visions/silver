#include <silver>

#define isilver(M,...) silver_##M(mod,    ## __VA_ARGS__)
#define   itype(M,...)   type_##M(ty,     ## __VA_ARGS__)
#define    idim(M,...)    dim_##M(member, ## __VA_ARGS__)

#undef  peek
#define tokens(...)     call(mod->tokens, __VA_ARGS__) /// no need to pass around the redundant tokens args when the class supports a stack

LLVMMetadataRef dbg_info(LLVMDIBuilderRef D, const char *typeName, uint64_t sizeInBits) {
    LLVMMetadataRef primitiveMeta = LLVMDIBuilderCreateBasicType(
        D,                             // Debug info builder
        typeName,                      // Name of the primitive type (e.g., "int32", "float64")
        strlen(typeName),              // Length of the name
        sizeInBits,                    // Size in bits (e.g., 32 for int, 64 for double)
        typeName[0] == 'f' ? 0x04 : 0x05, // switching based on f float or u/i int (on primitives)
        0
    );
    return primitiveMeta;
}

LLVMTypeRef dim_type_ref(dim member);

LLVMMetadataRef type_dbg(type f) { return f->meta_ref; }

string type_ref_key(LLVMTypeRef type_ref) {
    return format("%p", type_ref);
}

void type_associate(type ty, LLVMTypeRef type_ref) {
    silver mod = ty->mod;
    string key = type_ref_key(type_ref);
    assert (!contains(mod->type_refs, key), "already associated");
    set(mod->type_refs, key, ty);
}

dim type_find_member(type ty, string key) {
    if (!ty->members) return null;
    return get(ty->members, key);
}

sz type_size_of(type ty) {
    return LLVMSizeOfTypeInBits(ty->mod->target_data, ty->type_ref);
}

LLVMMetadataRef dim_meta_ref(dim member) {
    silver mod = member->mod;
    if (member->meta_ref)
        return member->meta_ref;
    LLVMMetadataRef t = member->type->meta_ref;
    int ptr_size = LLVMPointerSize(mod->target_data) * 8;
    for (int ii = 0; ii < member->depth; ii++) {
        t = LLVMDIBuilderCreatePointerType(
            mod->dbg,       // Debug info builder
            t,              // Base type (which will become pointer)
            ptr_size,       // Pointer size in bits (change for your target)
            0,              // Pointer alignment (use 0 to let LLVM infer)
            0,              // address space (default)
            NULL,           // Optional type name (can be NULL for anonymous)
            0               // Optional length of the name
        );
    }
    member->meta_ref = t;
    return t;
}

LLVMTypeRef dim_type_ref(dim member) {
    LLVMTypeRef t = member->type->type_ref;
    if (member->depth > 0 && t == LLVMVoidType())
        t = LLVMInt8Type();
    
    static int debug = 0; debug++;
    if (debug == 48)
        debug++;
    print("type string = %s", LLVMPrintTypeToString(t));

    for (int i = 0; i < member->depth; i++)
        t = LLVMPointerType(t, 0);
    
    return t;
}

LLVMValueRef dim_value_ref(dim member) {
    assert(member->value_ref, "value_ref not set when read\n");
    return member->value_ref;
}

void type_init(type ty) {
    silver mod = ty->mod;

    verify(ty->mod, "mod not set");
    verify(ty->name,   "name not set");

    ty->members = new(map, hsize, 8);
    bool handled_members = false;

    switch (ty->mdl) {
        case model_class:
            verify(false, "not implemented");
            break;
        
        case model_function: {
            print("making function for %o", ty->name);

            //if (!eq(ty->name, "main"))
            //    break;

            verify(ty->rtype,  "rtype");
            verify(ty->args,   "args");
            bool             is_main   = eq(ty->name, "main");
            int              n_args    = ty->args->count;
            dim              info      = ty->info;
            int              arg_index = 0;
            LLVMMetadataRef* arg_meta  = calloc(1 + n_args, sizeof(LLVMMetadataRef));
            LLVMTypeRef*     arg_types = calloc(n_args, sizeof(LLVMTypeRef));
            cstr*            arg_names = calloc(n_args, sizeof(cstr));
            

            pairs(ty->args, e) {
                dim arg = e->value;
                LLVMTypeRef       tr = dim_type_ref(arg);
                LLVMMetadataRef meta = dim_meta_ref(arg);
                arg_types[arg_index] = tr;
                arg_meta[1 + arg_index]  = meta;
                arg_index++;
            }

            LLVMTypeRef rtype     = dim_type_ref(ty->rtype);
            ty->type_ref          = LLVMFunctionType(rtype, arg_types, arg_index, ty->va_args);
            LLVMValueRef existing = LLVMGetNamedFunction(ty->mod->module_ref, ty->name->chars);
            
            verify (!existing, "parallel creation of function: %o", ty->name);
            verify (info,      "anonymous member not set on type for function");
            
            if (existing) {
                info->value_ref = existing;
                print  ("existing function found: %o", ty->name);
            } else {
                verify (!existing, "found existing function for %o", ty->name);
                print  ("return type: %s", LLVMPrintTypeToString(rtype));
                for (int i = 0; i < arg_index; i++)
                    print("arg %d type: %s", i, LLVMPrintTypeToString(arg_types[i]));

                info->value_ref = LLVMAddFunction(ty->mod->module_ref, ty->name->chars, ty->type_ref);
                LLVMValueRef function = info->value_ref;

                LLVMSetLinkage(function, info->visibility == Visibility_public
                    ? LLVMExternalLinkage : LLVMInternalLinkage);

                // set arg names
                arg_index = 0;
                pairs(ty->args, arg) {
                    string arg_name = arg->key;
                    dim    arg_type = arg->value;
                    AType     arg_t = isa(arg_type);

                    verify(arg_t == typeid(dim), "type mismatch");
                    //LLVMValueRef param = LLVMGetParam(function, arg_index);
                    //LLVMSetValueName2(param, arg_name->chars, arg_name->len);
                    arg_index++;
                }

                LLVMMetadataRef  rmeta = dim_meta_ref(ty->rtype);
                arg_meta[0]            = rmeta;
                const char *symbolName = strdup(LLVMGetValueName(function));
                size_t   symbolNameLen = strlen(symbolName);

                if (!ty->from_include) {
                    ty->entry_ref = LLVMAppendBasicBlockInContext(
                        mod->context, info->value_ref, "entry");

                    ty->sub_ref  = LLVMDIBuilderCreateSubroutineType(
                        mod->dbg, mod->file, arg_meta, 1 + arg_index, LLVMDIFlagZero);
                    
                    ty->fn_ref   = LLVMDIBuilderCreateFunction(
                        mod->dbg, mod->compile_unit,
                        strdup("main"), 4,
                        strdup("main"), 4, mod->file,
                        1, ty->sub_ref, false, !ty->from_include, 1, 0, false);

                    LLVMSetSubprogram(function, ty->fn_ref);
                }
            }
            //free(arg_meta);
            //free(arg_types);
            //free(arg_names);
            break;
        }
        case model_bool:   ty->type_ref = LLVMInt1Type  (); break;
        case model_i8:     ty->type_ref = LLVMInt8Type  (); break;
        case model_i16:    ty->type_ref = LLVMInt16Type (); break;
        case model_i32:    ty->type_ref = LLVMInt32Type (); break;
        case model_i64: {
            ty->type_ref = LLVMInt64Type ();
            print("ty->type_ref = %p", ty->type_ref);
            break;
        }
        case model_u8:     ty->type_ref = LLVMInt8Type  (); break;
        case model_u16:    ty->type_ref = LLVMInt16Type (); break;
        case model_u32:    ty->type_ref = LLVMInt32Type (); break;
        case model_u64:    ty->type_ref = LLVMInt64Type (); break;
        case model_f32:    ty->type_ref = LLVMFloatType (); break;
        case model_f64:    ty->type_ref = LLVMDoubleType(); break;
        case model_void:   ty->type_ref = LLVMVoidType  (); break;
        case model_cstr:   ty->type_ref = LLVMPointerType(LLVMInt8Type(), 0); break;
        case model_typedef: {
            verify (ty->origin && isa(ty->origin) == typeid(dim), "origin must be ty reference");
            ty->type_ref = dim_type_ref(ty->origin);
            if (mod->dbg) {
                dim origin = ty->origin;
                verify(origin, "origin not resolved");
                LLVMMetadataRef orig_ref = type_dbg(origin->type);
                if (orig_ref) {
                    ty->meta_ref = LLVMDIBuilderCreateTypedef(
                        mod->dbg, orig_ref, ty->name->chars, len(ty->name),
                        mod->file, ty->token ? ty->token->line : 0, mod->scope, LLVMDIFlagZero);
                } else
                    print("no debug info for type %o", ty->name);
            }
            break;
        }
        case model_struct: {
            LLVMTypeRef* member_types = calloc(len(ty->members), sizeof(LLVMTypeRef));
            int index = 0;
            pairs(ty->members, member_pair) {
                dim member_r = member_pair->value;
                verify(isa(member_r) == typeid(dim), "mismatch");
                member_types[index] = dim_type_ref(member_r);
                index++;
            }
            ty->type_ref = LLVMStructCreateNamed(LLVMGetGlobalContext(), ty->name);
            LLVMStructSetBody(ty->type_ref, member_types, index, 0);
            handled_members = true;
            break;
        }
        case model_union: {
            verify (false, "not implemented");
            break;
        }
        type_associate(ty, ty->type_ref);
    }

    if (ty->info && !ty->info->type)
        ty->info->type = ty;

    /// create debug info for primitives
    if (ty->mdl >= model_bool && ty->mdl <= model_f64) {
        if (ty->mdl == model_i64) {
            debug();
        }
        ty->meta_ref = dbg_info(mod->dbg, ty->name->chars, type_size_of(ty));
    }

    verify (!count(ty->members) || handled_members, "members given and not processed");
}

dim cx_to_dim(silver mod, CXType cxType, symbol name, bool arg_rules) {
    string    t = null;
    int   depth = 0;
    CXType base = cxType;
    array shape = new(array, alloc, 32);

    while (base.kind == CXType_Pointer || base.kind == CXType_ConstantArray || base.kind == CXType_IncompleteArray) {
        if (base.kind == CXType_Pointer) {
            base = clang_getPointeeType(base);
            depth++;
        } else {
            if (!arg_rules) {
                sz size = clang_getArraySize(base);
                push(shape, A_sz(size));
            } else {
                depth++;
            }
            base = clang_getArrayElementType(base);
        }
    }

    switch (base.kind) {
        case CXType_Void:   t = str("void"); break;
        case CXType_Char_S: t = str("i8");   break;
        case CXType_Char_U: t = str("u8");   break;
        case CXType_SChar:  t = str("i8");   break;
        case CXType_UChar:  t = str("u8");   break;
        case CXType_Char16: t = str("i16");  break;
        case CXType_Char32: t = str("i32");  break;
        case CXType_Bool:   t = str("bool"); break;
        case CXType_UShort: t = str("u16");  break;
        case CXType_Short:  t = str("i16");  break;
        case CXType_UInt:   t = str("u32");  break;
        case CXType_Int:    t = str("i32");  break;
        case CXType_ULong:  t = str("u64");  break;
        case CXType_Long:   t = str("i64");  break;
        case CXType_Float:  t = str("f32");  break;
        case CXType_Double: t = str("f64");  break;
        case CXType_Record: {
            CXString n = clang_getTypeSpelling(base);
            t = str(clang_getCString(n));
            if (!get(mod->defs, t)) {
                 set(mod->defs, t,
                    new(type, mod, mod,
                        mdl,    model_struct,
                        name,   t));
            }
            clang_disposeString(n);
            break;
        }
        default:
            t = str("void");
    }
    
    if (t && !get(mod->defs, t)) {
        print("no def?");
    }
    verify (t, "type not set");
    return new(dim,
        type,    get(mod->defs, t),
        mod,  mod,
        depth,   depth,
        context, mod->defs,
        name,    name ? str(name) : null,
        shape,   shape);
}

enum CXChildVisitResult visit_member(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    type   type_def = (dim)client_data;
    silver mod   = type_def->mod;
    if (clang_getCursorKind(cursor) == CXCursor_FieldDecl) {
        CXType   field_type    = clang_getCursorType(cursor);
        CXString field_name    = clang_getCursorSpelling(cursor);
        CXString field_ts      = clang_getTypeSpelling(field_type);
        symbol   field_type_cs = clang_getCString(field_ts);
        symbol   field_name_cs = clang_getCString(field_name);
        dim      field_dim     = cx_to_dim(mod, field_type, (cstr)field_name_cs, false);
        set(type_def->members, str(field_name_cs), field_dim);
        clang_disposeString(field_name);
        clang_disposeString(field_ts);
    }
    
    return CXChildVisit_Recurse;
}

map silver_top_members(silver mod);

enum CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    silver mod = (silver)client_data;
    CXString  fcx = clang_getCursorSpelling(cursor);
    symbol     cs = clang_getCString(fcx);
    string   name = str(cs);
    type      def = null;
    enum CXCursorKind k = clang_getCursorKind(cursor);

    type  current = get(mod->defs, name);
    if (!current)
    switch (k) {
        case CXCursor_FunctionDecl: {
            
            CXType   cx_rtype = clang_getCursorResultType(cursor);
            dim      rtype    = cx_to_dim(mod, cx_rtype, null, true);
            int      n_args   = clang_Cursor_getNumArguments(cursor);
            map      args     = new(map);

            static int seq = 0;
            seq++;
            if (!eq(name, "printf"))
                return CXChildVisit_Recurse;
            
            for (int i = 0; i < n_args; i++) {
                CXCursor arg_cursor = clang_Cursor_getArgument(cursor, i);
                CXString pcx        = clang_getCursorSpelling (arg_cursor);
                symbol   arg_name   = clang_getCString        (pcx);
                CXType   arg_cxtype = clang_getCursorType     (arg_cursor);
                CXString pcx_type   = clang_getTypeSpelling   (arg_cxtype);
                symbol   arg_type   = clang_getCString        (pcx_type);
                dim      arg        = cx_to_dim(mod, arg_cxtype, arg_name, true);
                set(args, arg->name, arg);
                clang_disposeString(pcx);
            }
            dim member = new(dim, mod, mod, name, name, context, mod->defs);
            bool is_var = clang_Cursor_isVariadic(cursor);
            if (eq(name, "printf"))
                assert(is_var, "expected var arg");
            def = new(type,
                mod,            mod,
                from_include,   mod->current_include,
                info,           member,
                name,           name,
                va_args,        is_var,
                mdl,            model_function,
                rtype,          rtype,
                args,           args);
            //member->type = def;
            break;
        }
        case CXCursor_StructDecl: {
            def = new(type,
                mod,     mod,
                from_include, mod->current_include,
                name,       name,
                mdl,        model_struct,
                members,    new(map));
            clang_visitChildren(cursor, visit_member, def);
            break;
        }
        case CXCursor_TypedefDecl: {
            CXType  underlying_type = clang_getTypedefDeclUnderlyingType(cursor);
            while (underlying_type.kind == CXType_Typedef) {
                CXCursor u = clang_getTypeDeclaration(underlying_type);
                underlying_type = clang_getTypedefDeclUnderlyingType(u);
            }
            CXString underlying_type_name = clang_getTypeSpelling(underlying_type);
            const char *type_name = clang_getCString(underlying_type_name);

            dim typedef_dim = underlying_type.kind ? cx_to_dim(mod, underlying_type, null, false) : null; 
            if (typedef_dim && !typedef_dim->type) {
                print("test");
            }
            def = new(type,
                mod, mod,
                from_include, mod->current_include,
                name,   name,
                mdl,    model_typedef,
                origin, typedef_dim);
            break;
        }
        default:
            break;
    }
    if (def) {
        assert (def, "no type defintion made");
        set    (mod->defs, name, def);
        if (def->info) {
            map base = silver_top_members(mod);
            set(base, name, def->info);
            assert(mod->member_stack->len == 1, "invalid import");
            print  ("imported: %s", cs);
        }
    }
    clang_disposeString(fcx);
    return CXChildVisit_Recurse;
}

path silver_source_path(silver mod) {
    return mod->source_path;
}

type silver_get_type(silver mod, string key) {
    return get(mod->defs, key);
}

dim silver_get_member(silver mod, string key) {
    return get(mod->members, key);
}

map silver_top_members(silver mod) {
    assert (mod->member_stack->len, "stack is empty");
    return mod->member_stack->elements[mod->member_stack->len - 1];
}

/// lookup value ref for member in stack
dim silver_member_stack_lookup(silver mod, string name) {
    for (int m = mod->member_stack->len - 1; m >= 0; m--) {
        map members = mod->member_stack->elements[m];
        dim f = get(members, name);
        if (f) return f;
    }
    return null;
}

map silver_push_member_stack(silver mod) {
    map members = new(map, hsize, 16);
    push(mod->member_stack, members);
    return members;
}

void silver_pop_member_stack(silver mod) {
    pop(mod->member_stack);
}

/// return a map of defs found by their name (we can isolate the namespace this way by having separate maps)
map silver_include(silver mod, string include) {
    string   install   = format("%o/include", mod->install);
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
    mod->current_include = include;
    //clang_visitChildren(cursor, visit, (CXClientData)mod);
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    mod->current_include = null;
    return 0;
}

void silver_set_line(silver mod, i32 line, i32 column) {
    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        mod->context, line, column, mod->scope, null);
    LLVMSetCurrentDebugLocation2(mod->dbg, loc);
}

void silver_llflag(silver mod, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(mod->context), ival, 0));
    LLVMAddModuleFlag(
        mod->module_ref, LLVMModuleFlagBehaviorError, flag, strlen(flag), v);
}

void silver_write(silver mod) {
    cstr err = NULL;
    if (LLVMVerifyModule(mod->module_ref, LLVMPrintMessageAction, &err))
        fault("Error verifying mod: %s", err);
    else
        print("mod verified");

    if (!LLVMPrintModuleToFile(mod->module_ref, "a.ll", &err))
        print("generated IR");
    else
        fault("LLVMPrintModuleToFile failed");

    symbol bc = "a.bc";
    if (LLVMWriteBitcodeToFile(mod->module_ref, bc) != 0)
        fault("LLVMWriteBitcodeToFile failed");
    else
        print("bitcode written to %s", bc);
}

void silver_destructor(silver mod) {
    LLVMDisposeBuilder(mod->builder);
    LLVMDisposeDIBuilder(mod->dbg);
    LLVMDisposeModule(mod->module_ref);
}

/// debug function for printing tokens, for context
void silver_tokens(silver mod) {
    print("tokens: %o %o %o %o %o ...", 
        read(mod->tokens, 0), read(mod->tokens, 1),
        read(mod->tokens, 2), read(mod->tokens, 3),
        read(mod->tokens, 4), read(mod->tokens, 5));
}

/// this reads a num var
/// --------------------
/// num[2] name
/// num[2] name [ num arg1, ... num[] extra ] expr | [ statements ]
/// map[string, object] name   # good idea to always define that the value is object, and no default 'map' usage?
/// array[meta][string, object] name   #
/// --------------------

void dim_bind(dim member) {
    silver mod = member->mod;
    if (!tokens(next_is, "["))
        return;
    tokens(consume);
    member->wrap  = call(mod, get_type, str("array"));
    member->shape = new(array); /// shape is there but not given data 
    if (!tokens(next_is, "]")) {
        type wdef = tokens(read_type, mod);
        if (wdef) {
            /// must be map
            for (;;) {
                push(member->shape, wdef);
                Token n = tokens(peek);
                if (eq(n, ",")) {
                    wdef = tokens(read_type, mod);
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
                push(member->shape, A_i64(dim_size));
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

void dim_allocate(dim member, silver mod, bool create_debug) {
    LLVMTypeRef tr = dim_type_ref(member);
    member->value_ref   = LLVMBuildAlloca(mod->builder, tr, "alloc-dim");
    /*member->meta_ref    = LLVMDIBuilderCreateAutoVariable(
        mod->dbg,
        mod->scope,
        member->name->chars,
        member->name->len,
        mod->file,
        2,
        member->type->meta_ref,
        true, 0, 0);*/


    LLVMBasicBlockRef block = LLVMGetInsertBlock(mod->builder);
    LLVMValueRef      head  = LLVMGetFirstInstruction(block);

    verify (block && head, "LLVM is a block head");
    verify (head == member->value_ref, "LLVM is a block head");
    // Now insert this variable into the IR, associating it with the debug metadata
    /*LLVMDIBuilderInsertDeclareBefore(
        mod->dbg,
        member->value_ref,
        member->meta_ref,
        LLVMDIBuilderCreateExpression(mod->dbg, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(mod->llvm_context, member->line, 0, mod->scope, NULL),  // Debug location
        head
    );*/
}

void dim_create_fn(dim member) {
    silver mod = member->mod;
    map    members = member->context;
    verify (tokens(next_is, "["), "expected function args");
    tokens (consume);
    map args = new(map, hsize, 8);
    int arg_index = 0;
    
    silver_tokens(mod);

    if (!tokens(next_is, "]"))
        while (true) {
            dim arg = dim_read(member->mod, member->type->members, false);
            verify (arg,       "member failed to read");
            verify (arg->name, "member name not set");
            if     (tokens(next_is, "]")) break;
            verify (tokens(next_is, ","), "expected separator");
            tokens (consume);
            set    (args, arg->name, arg);
            arg_index++;
        }
    num line = tokens(line);
    tokens(consume);
    dim rtype_dim = new(dim,
        mod,        member->mod,    type,       member->type,   line, line,
        depth,      member->depth,  shape,      member->shape,
        wrap,       member->wrap,   context,    members);
    type f_def = new(type,
        name,     str(member->name->chars),  mod,   member->mod,
        mdl,      model_function,       rtype,    rtype_dim,
        args,     args,                 info,     member);
    //assert(count(members, f_def->name) == 0, "duplicate member: %o", f_def->name);
    set (members, f_def->name, f_def);
    drop(member->type);
    member->type    = hold(f_def);
    array body = new(array, alloc, 32);
    verify (tokens(next_is, "["), "expected function body");
    int depth = 0;
    do {
        Token   token = tokens(next);
        verify (token, "expected end of function body ( too many ['s )");
        push   (body, token);
        if      (eq(token, "[")) depth++;
        else if (eq(token, "]")) depth--;
    } while (depth > 0);
    member->type->body = new(Tokens, cursor, 0, tokens, body);
}

void dim_init(dim member) {
    if (!member->visibility) member->visibility = Visibility_public;
}

bool dim_type_is(dim member, type t) {
    type tt = member->type;
    while (tt) {
        if (t == tt) return true;
        if (!tt->origin) break;
        tt = ((dim)tt->origin)->type;
    }
    return false;
}

dim dim_read(silver mod, map context, bool alloc) {
    /// we use alloc to obtain a value-ref by means of allocation inside a function body.
    /// if we are not there, then set false in that case.
    /// in that case, the value_ref should be set by the user to R-type or ARG-type
    tokens(push_current);
    dim member = null;

    print("dim_read:");
    silver_tokens(mod);

    string alpha = tokens(next_alpha);
    if(alpha) {
        member = isilver(member_stack_lookup, alpha);
        if (member) {
            member->cached = true;
            return member;
        } else {
            tokens(prev);
        }
    }
    member = new(dim, mod, mod, context, context);
    
    if (tokens(next_is, "static")) {
        tokens(consume);
        member->is_static = true;
    }
    if (tokens(next_is, "ref")) {
        tokens(consume);
        member->depth = 1;
    }
    /// look for visibility (default is possibly provided)
    for (int m = 1; m < Visibility_type.member_count; m++) {
        type_member_t* enum_v = &Visibility_type.members[m];
        if (tokens(next_is, enum_v->name)) {
            tokens(consume);
            member->visibility = m;
            break;
        }
    }
    if (!member->is_static) {
        if (tokens(next_is, "static")) {
            tokens(consume);
            member->is_static = true;
        }
    }
    Token  n = tokens(peek);
    type def = tokens(read_type, mod);
    if (!def) {
        print("info: could not read type at position %o", tokens(location));
        tokens(pop_state, false); // we may 'info' here
        return null;
    }
    member->type = hold(def);

    silver_tokens(mod);
    
    // may be [, or alpha-id  (its an error if its neither)
    if (tokens(next_is, "["))
        idim(bind);

    /// members must be named
    silver_tokens(mod);
    string name = tokens(next_alpha);
    verify(name, "expected identifier for member");
    member->name     = hold(name);

    if (tokens(next_is, "["))
        idim(create_fn);
    else if (alloc)
        idim(allocate, mod, true); /// would be nice to get member context from state.. or hey, just set 1 variable and retrieve one variable.  put it in silver intern.

    tokens(pop_state, true);
    map top = isilver(top_members);
    set(top, name, member);
    return member;
}

void silver_parse_top(silver mod) {
    while (tokens(cast_bool)) {
        if (tokens(next_is, "import")) {
            Import import  = new(Import, mod, mod, tokens, mod->tokens);
            push(mod->imports, import);
            continue;
        } else if (tokens(next_is, "class")) {
            verify (false, "not implemented");
            //EClass def = new(EClass, tokens, tokens);
            //call(mod->defs, set, def->name, def);
            continue;
        } else {
            /// functions are a 'member' of the mod
            /// so are classes, but we have a a->defs for the type alone
            /// so we may have class contain in a 'member' of definition type
            /// so its name could be the name of the class and the type would be the same name
            //break;
            silver_tokens(mod);
            dim member = dim_read(mod, mod->defs, false);
            string key = member->name ? member->name : (string)format("$m%i", count(mod->defs));
            set(mod->members, key, member);
        }
    }
}

void silver_define_C99(silver mod) {
    map      defs = mod->defs = new(map, hsize, 64);
    set(defs, str("bool"),    new(type, mod, mod, name, str("bool"),   mdl, model_bool, imported, typeid(bool)));
    set(defs, str("i8"),      new(type, mod, mod, name, str("i8"),     mdl, model_i8,   imported, typeid(i8)));
    set(defs, str("i16"),     new(type, mod, mod, name, str("i16"),    mdl, model_i16,  imported, typeid(i16)));
    set(defs, str("i32"),     new(type, mod, mod, name, str("i32"),    mdl, model_i32,  imported, typeid(i32)));
    set(defs, str("i64"),     new(type, mod, mod, name, str("i64"),    mdl, model_i64,  imported, typeid(i64)));
    set(defs, str("u8"),      new(type, mod, mod, name, str("u8"),     mdl, model_u8,   imported, typeid(u8)));
    set(defs, str("u16"),     new(type, mod, mod, name, str("u16"),    mdl, model_u16,  imported, typeid(u16)));
    set(defs, str("u32"),     new(type, mod, mod, name, str("u32"),    mdl, model_u32,  imported, typeid(u32)));
    set(defs, str("u64"),     new(type, mod, mod, name, str("u64"),    mdl, model_u64,  imported, typeid(u64)));
    set(defs, str("f32"),     new(type, mod, mod, name, str("f32"),    mdl, model_f32,  imported, typeid(f32)));
    set(defs, str("f64"),     new(type, mod, mod, name, str("f64"),    mdl, model_f64,  imported, typeid(f64)));
    set(defs, str("void"),    new(type, mod, mod, name, str("void"),   mdl, model_void, imported, typeid(none)));
    set(defs, str("symbol"),  new(type, mod, mod, name, str("symbol"), mdl, model_cstr, imported, typeid(symbol)));
    set(defs, str("cstr"),    new(type, mod, mod, name, str("cstr"),   mdl, model_cstr, imported, typeid(cstr)));
    set(defs, str("int"),     get(defs, str("i64")));
    set(defs, str("uint"),    get(defs, str("u64")));
}

bool silver_build_dependencies(silver mod) {
    each(mod->imports, Import, im) {
        process(im);
        switch (im->import_type) {
            case ImportType_source:
                if (len(im->main_symbol))
                    push(mod->main_symbols, im->main_symbol);
                each(im->source, string, source) {
                    // these are built as shared library only, or, a header file is included for emitting
                    if (call(source, has_suffix, ".rs") || call(source, has_suffix, ".h"))
                        continue;
                    string buf = format("%o/%s.o", mod->install, source);
                    push(mod->compiled_objects, buf);
                }
            case ImportType_library:
            case ImportType_project:
                concat(mod->libraries_used, im->links);
                break;
            case ImportType_includes:
                break;
            default:
                verify(false, "not handled: %i", im->import_type);
        }
    }
    return true;
}

LLVMValueRef silver_build_statements(silver mod) {
    LLVMValueRef result = null;
    return result;
}

LLVMValueRef silver_parse_expression(silver mod);

LLVMValueRef silver_parse_return(silver mod) {
    dim  rtype   = silver_member_stack_lookup(mod, str("#rtype")); // stored when we start building function
    type t_void  = get(mod->defs, str("void"));
    bool is_void = dim_type_is(rtype, t_void);
    tokens (consume);
    return (is_void) ?
        LLVMBuildRetVoid(mod->builder) :
        LLVMBuildRet    (mod->builder, isilver(parse_expression));
}

LLVMValueRef silver_parse_break(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_for(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_while(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_if_else(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_do_while(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

type silver_type_from_llvm(silver mod, LLVMTypeRef type_ref) {
    string key = type_ref_key(type_ref);
    type   res = get(mod->type_refs, key);
    assert(res, "expected associated type");
    return res;
}

type preferred_type(silver mod, type t0, type t1) {
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
typedef LLVMValueRef(*parse_fn)(silver, symbol, symbol, builder_fn, builder_fn);

#define resolve_type(llvm_type_ref) isilver(type_from_llvm, llvm_type_ref);

LLVMValueRef parse_ops(
        silver mod, parse_fn descent, symbol op0, symbol op1, builder_fn b0, builder_fn b1) {
    LLVMValueRef left = descent(mod, op0, op1, b0, b1);
    while(tokens(next_is, op0) || tokens(next_is, op1)) {
        tokens(consume);
        LLVMValueRef right = descent(mod, op0, op1, b0, b1);
        bool         use0 = tokens(next_is, op0);
        symbol    op_code = use0 ? op0 : op1;
        builder_fn    bfn = use0 ? b0  : b1;

        assert (contains(mod->operators, str(op_code)), "op (%s) not registered", op_code);
        string  op_name   = get(mod->operators, str(op_code));
        type    left_type = resolve_type(left);
        dim     method    = call(left_type, find_member, op_name);
        type    r_left    = resolve_type(LLVMTypeOf(left));
        type    r_right   = resolve_type(LLVMTypeOf(right));

        if (method) { /// and convertible(r_right, method.args[0].type):
            LLVMValueRef args[2] = { left, right };
            left = LLVMBuildCall2(mod->builder, method->type->type_ref, dim_value_ref(method), args, 2, "operator");
        } else {
            type type_out = preferred_type(mod, r_left, r_right); /// should work for primitives however we must handle more in each
            left          = bfn(mod, type_out, r_left, r_right, left, right); 
        }
    }
    return left;
}

LLVMValueRef op_is(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert(member->type->mdl == model_bool, "inherits operator must return a boolean type");
    assert(LLVMGetTypeKind(LLVMTypeOf(v0))  == LLVMFunctionTypeKind &&
           LLVMGetTypeKind(LLVMTypeOf(v1)) == LLVMFunctionTypeKind, 
           "is operator expects function type or initializer");
    bool equals = t0 == t1;
    return LLVMConstInt(LLVMInt1Type(), equals, 0);
}

LLVMValueRef op_inherits(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert(member->type->mdl == model_bool, "inherits operator must return a boolean type");
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

LLVMValueRef op_add(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    return LLVMBuildAdd(mod->builder, v0, v1, "add");
}

LLVMValueRef op_sub(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    return LLVMBuildSub(mod->builder, v0, v1, "add");
}

LLVMValueRef op_mul(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert (false, "op_mul: implement more design here");
    return null;
}

LLVMValueRef op_div(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert (false, "op_div: implement more design here");
    return null;
}

LLVMValueRef op_eq(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(mod->builder, LLVMIntEQ,   v0, v1, "eq-i");
    else if (f0) return LLVMBuildFCmp(mod->builder, LLVMRealOEQ, v0, v1, "eq-f");
    else {
        assert (false, "op_eq: implement more design here");
        return null;
    }
}

LLVMValueRef op_not_eq(silver mod, dim member, type t0, type t1, LLVMValueRef v0,  LLVMValueRef v1) {
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(mod->builder, LLVMIntNE,   v0, v1, "not-eq-i");
    else if (f0) return LLVMBuildFCmp(mod->builder, LLVMRealONE, v0, v1, "not-eq-f");
    else {
        assert (false, "op_not_eq: implement more design here");
        return null;
    }
}

LLVMValueRef silver_parse_primary(silver mod);

LLVMValueRef silver_parse_eq  (silver mod) { return parse_ops(mod, silver_parse_primary, "==", "!=",         op_eq,  op_not_eq); }
LLVMValueRef silver_parse_is  (silver mod) { return parse_ops(mod, silver_parse_eq,      "is", "inherits",   op_is,  op_is);     }
LLVMValueRef silver_parse_mult(silver mod) { return parse_ops(mod, silver_parse_is,      "*",  "/",          op_is,  op_is);     }
LLVMValueRef silver_parse_add (silver mod) { return parse_ops(mod, silver_parse_mult,    "+",  "-",          op_add, op_sub);    }

LLVMValueRef silver_parse_expression(silver mod) {
    mod->expr_level++;
    LLVMValueRef vr = isilver(parse_add);
    mod->expr_level--;
    return vr;
}

LLVMValueRef silver_parse_primary(silver mod) {
    Token t = tokens(peek);

    // handle the logical NOT operator (e.g., '!')
    if (tokens(next_is, "!") || tokens(next_is, "not")) {
        tokens(consume); // Consume '!' or 'not'
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the following expression
        return LLVMBuildNot(mod->builder, expr, "logical_not");
    }

    // bitwise NOT operator
    if (tokens(next_is, "~")) {
        tokens(consume); // Consume '~'
        LLVMValueRef expr = silver_parse_expression(mod);
        return LLVMBuildNot(mod->builder, expr, "bitwise_not");
    }

    // 'typeof' operator
    if (tokens(next_is, "typeof")) {
        tokens(consume); // Consume 'typeof'
        assert(tokens(next_is, "["), "Expected '[' after 'typeof'");
        tokens(consume); // Consume '['
        LLVMValueRef type_ref = silver_parse_expression(mod); // Parse the type expression
        assert(tokens(next_is, "]"), "Expected ']' after type expression");
        tokens(consume); // Consume ']'
        return type_ref; // Return the type reference
    }

    // 'cast' operator
    if (tokens(next_is, "cast")) {
        tokens(consume); // Consume 'cast'
        dim cast_ident = tokens(read_type, mod); // Read the type to cast to
        assert(tokens(next_is, "["), "Expected '[' for cast");
        tokens(consume); // Consume '['
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the expression to cast
        tokens(consume); // Consume ']'
        if (cast_ident) {
            return LLVMBuildCast(mod->builder, LLVMBitCast, cast_ident, expr, "explicit_cast");
        }
    }

    // 'ref' operator (reference)
    if (tokens(next_is, "ref")) {
        tokens(consume); // Consume 'ref'
        LLVMValueRef expr = silver_parse_expression(mod);
        return LLVMBuildLoad2(mod->builder, LLVMTypeOf(expr), expr, "ref_expr"); // Build the reference
    }

    // numeric constants
    object v_num = tokens(next_numeric);
    if (v_num) {
        AType num_type = isa(v_num);
        if (num_type == typeid(i8))  return LLVMConstInt( LLVMInt8Type(),  *( i8*)v_num, 0);
        if (num_type == typeid(i16)) return LLVMConstInt(LLVMInt16Type(),  *(i16*)v_num, 0);
        if (num_type == typeid(i32)) return LLVMConstInt(LLVMInt32Type(),  *(i32*)v_num, 0);
        if (num_type == typeid(i64)) return LLVMConstInt(LLVMInt64Type(),  *(i64*)v_num, 0);
        if (num_type == typeid(u8))  return LLVMConstInt( LLVMInt8Type(),  *( u8*)v_num, 0);
        if (num_type == typeid(u16)) return LLVMConstInt(LLVMInt16Type(),  *(u16*)v_num, 0);
        if (num_type == typeid(u32)) return LLVMConstInt(LLVMInt32Type(),  *(u32*)v_num, 0);
        if (num_type == typeid(u64)) return LLVMConstInt(LLVMInt64Type(),  *(u64*)v_num, 0);
        if (num_type == typeid(f32)) return LLVMConstInt(LLVMFloatType(),  *(f32*)v_num, 0);
        if (num_type == typeid(f64)) return LLVMConstInt(LLVMDoubleType(), *(f64*)v_num, 0);
        assert (false, "numeric literal not handling primitive: %s", num_type->name);
    }

    // strings
    string str_token = tokens(next_string);
    if (str_token)
        return LLVMBuildGlobalStringPtr(mod->builder, str_token->chars, "str");

    // boolean values
    object v_bool = tokens(next_bool);
    if (v_bool)
        return LLVMConstInt(LLVMInt1Type(), *(bool*)v_bool, 0);

    // parenthesized expressions
    if (tokens(next_is, "[")) {
        tokens(consume);
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the expression
        assert(tokens(next_is, "]"), "Expected closing parenthesis");
        tokens(consume);
        return expr;
    }

    // handle identifiers (variables or function calls)
    string ident = tokens(next_alpha);
    if (ident) {
        dim member = silver_member_stack_lookup(mod, ident); // Look up variable
        // if its a primitive, we will want to get its value unless we are referencing (which we handle differently above!)
        LLVMValueRef vr = dim_value_ref(member);
        if (member->type->mdl >= model_bool && member->type->mdl <= model_f64) {
            LLVMTypeRef type = LLVMTypeOf(vr);
            print ("vr type = %s", LLVMPrintTypeToString(LLVMTypeOf(vr)));
            verify (LLVMGetTypeKind(type) == LLVMPointerTypeKind, "expected member address");
            vr = LLVMBuildLoad2(mod->builder, member->type->type_ref, vr, "load-member");
        }
        return vr;
    }

    string literal = tokens(next_string);

    fault("unexpected token %o in primary expression", tokens(peek));
    return null;
}

LLVMValueRef silver_parse_assignment(silver mod, dim member, string op) {
    verify(!member->cached || !member->is_const, "member %o is a constant", member->name);
    string         op_name = Token_op_name(op);
    dim            method  = call(member->type, find_member, op_name);
    LLVMValueRef   res     = null;
    LLVMBuilderRef B       = mod->builder;
    LLVMValueRef   L       = member->value_ref;
    LLVMValueRef   R       = isilver(parse_expression);

    if (method) {
        LLVMValueRef args[2] = { L, R };
        res = LLVMBuildCall2(B, dim_type_ref(method), dim_value_ref(method), args, 2, "assign");
    } else {
        member->is_const = eq(op, "=");
        bool e = member->is_const;
        if (e || eq(op, ":"))  res = LLVMBuildStore(B, R, L);
        else if (eq(op, "+=")) res = LLVMBuildAdd  (B, R, L, "assign-add");
        else if (eq(op, "-=")) res = LLVMBuildSub  (B, R, L, "assign-sub");
        else if (eq(op, "*=")) res = LLVMBuildMul  (B, R, L, "assign-mul");
        else if (eq(op, "/=")) res = LLVMBuildSDiv (B, R, L, "assign-div");
        else if (eq(op, "%=")) res = LLVMBuildSRem (B, R, L, "assign-mod");
        else if (eq(op, "|=")) res = LLVMBuildOr   (B, R, L, "assign-or"); 
        else if (eq(op, "&=")) res = LLVMBuildAnd  (B, R, L, "assign-and");
        else if (eq(op, "^=")) res = LLVMBuildXor  (B, R, L, "assign-xor");
        else fault("unsupported operator: %o", op);
    }
    /// update member's value_ref
    //member->value_ref = res;
    if (eq(op, "=")) member->is_const = true;
    return member->value_ref;
}

LLVMValueRef silver_parse_function_call(silver mod, dim fn) {
    bool allow_no_paren = mod->expr_level == 1; /// remember this decision? ... var args
    bool expect_end_br  = false;
    type def            = fn->type;
    assert (def, "no definition found for function member %o", fn->name);
    int  arg_count      = count(def->args);
    
    if (tokens(next_is, "[")) {
        tokens(consume);
        expect_end_br = true;
    } else if (!allow_no_paren)
        fault("expected [ for nested call");

    LLVMValueRef vr_args[16];
    vector v_args = new(vector, alloc, 32, type, typeid(LLVMValueRef));
    int arg_index = 0;
    LLVMValueRef values[32];

    LLVMTypeRef fn_type = dim_type_ref(fn);
    
    dim last_arg = null;
    while(arg_index < arg_count || def->va_args) {
        dim          arg      = arg_index < fn->type->args->count ? idx_1(fn->type->args, sz, arg_index) : null;
        LLVMValueRef expr     = isilver(parse_expression);
        LLVMTypeRef  arg_type = arg ? dim_type_ref(arg) : null;
        LLVMTypeRef  e_type   = LLVMTypeOf(expr); /// this should be 'someting' member

        if (LLVMGetTypeKind(e_type) == LLVMPointerTypeKind) {
            expr = LLVMBuildLoad2(mod->builder, arg ? dim_type_ref(arg) : LLVMPointerType(LLVMInt8Type(), 0), expr, "load-arg");
        }

        if (arg_type && e_type != arg_type)
            expr = LLVMBuildBitCast(mod->builder, expr, arg_type, "bitcast");
        
        print("argument %i: %s", arg_index, LLVMPrintValueToString(expr));

        //push(v_args, &expr);
        values[arg_index] = expr;
        arg_index++;
        if (tokens(next_is, ",")) {
            tokens(consume);
            continue;
        } else if (tokens(next_is, "]")) {
            verify (arg_index >= arg_count, "expected %i args", arg_count);
            break;
        } else {
            if (arg_index >= arg_count)
                break;
        }
    }
    if (expect_end_br) {
        verify(tokens(next_is, "]"), "expected ] end of function call");
        tokens(consume);
    }
    return LLVMBuildCall2(mod->builder, dim_type_ref(fn), dim_value_ref(fn), values, arg_index, "fn-call");
}

LLVMValueRef silver_parse_statement(silver mod) {
    Token t = tokens(peek);
    if (tokens(next_is, "return")) return isilver(parse_return);
    if (tokens(next_is, "break"))  return isilver(parse_break);
    if (tokens(next_is, "for"))    return isilver(parse_for);
    if (tokens(next_is, "while"))  return isilver(parse_while);
    if (tokens(next_is, "if"))     return isilver(parse_if_else);
    if (tokens(next_is, "do"))     return isilver(parse_do_while);

    map members = isilver(top_members);
    dim member  = dim_read(mod, members, true);
    if (member) {
        print("%s member: %o %o", member->cached ? "existing" : "new", member->type->name, member->name);
        if (member->type->mdl == model_function) {
            if      (member->depth == 1) return dim_value_ref(member);
            else if (member->depth == 0) return isilver(parse_function_call, member);
            fault ("invalid operation");
        } else {
            string assign = tokens(next_assign);
            if    (assign) return isilver(parse_assignment, member, assign);
        }
    }
    fault ("implement"); /// implement as we need them
    return null;
}

LLVMValueRef silver_parse_statements(silver mod) {
    map  members    = isilver(push_member_stack);
    bool multiple   = tokens(next_is, "[");
    if  (multiple)    tokens(consume);
    int  depth      = 1;
    LLVMValueRef vr = null;
    ///
    while(tokens(cast_bool)) {
        if(multiple && tokens(next_is, "[")) {
            depth += 1;
            isilver(push_member_stack);
            tokens(consume);
        }
        print("next statement origin: %o", tokens(peek));
        vr = isilver(parse_statement);
        if(!multiple) break;
        if(tokens(next_is, "]")) {
            if (depth > 1)
                isilver(pop_member_stack);
            tokens(next);
            if ((depth -= 1) == 0) break;
        }
    }
    isilver(pop_member_stack);
    return vr;
}

static void silver_scope_push(silver mod, LLVMMetadataRef meta_ref) {
    push    (mod->scope_stack, meta_ref);
    mod->scope = meta_ref;
}

static void silver_scope_pop(silver mod) {
    pop     (mod->scope_stack);
    mod->scope = len(mod->scope_stack) ? last(mod->scope_stack) : null;
}

static void silver_build_function(silver mod, type fn) {
    isilver (scope_push, fn->fn_ref);
    mod->scope = fn->fn_ref;
    tokens  (push_state, fn->body->tokens, fn->body->cursor);
    LLVMPositionBuilderAtEnd(mod->builder, fn->entry_ref);
    map      members = isilver(push_member_stack);
    concat  (members, fn->args);
    set     (members, str("#rtype"), (dim)fn->rtype);
    //LLVMBuildRetVoid(mod->builder);
    isilver (parse_statements);
    isilver (pop_member_stack);
    tokens  (pop_state, false);
    isilver (scope_pop);
}

static map operators;

static void init() {
    operators    = map_of(
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
}

module_init(init);

static void silver_init(silver mod) {
    verify(mod->source, "mod name not set");

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    
    mod->scope_stack  = new(array, alloc, 32, unmanaged, true);
    mod->member_stack = new(array, alloc, 32);
    mod->members      = isilver(push_member_stack); /// our base members is the first stack item that we keep after building
    mod->imports      = new(array, alloc, 32);
    mod->source_path  = call(mod->source, directory);
    mod->source_file  = call(mod->source, filename);
    mod->libraries_used = new(array);
    mod->type_refs    = new(map, hsize, 64);
    mod->operators    = map_of(
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

    path  full_path = form(path, "%o/%o", mod->source_path, mod->source_file);
    verify(call(full_path, exists), "source (%o) does not exist", full_path);

    mod->module_ref    = LLVMModuleCreateWithName(mod->source_file->chars);
    mod->context       = LLVMGetModuleContext(mod->module_ref);
    mod->dbg           = LLVMCreateDIBuilder(mod->module_ref);
    mod->builder       = LLVMCreateBuilderInContext(mod->context);
    mod->target_triple = LLVMGetDefaultTargetTriple();
    cstr error = NULL;
    if (LLVMGetTargetFromTriple(mod->target_triple, &mod->target, &error))
        fault("error: %s", error);

    mod->target_machine = LLVMCreateTargetMachine(
        mod->target, mod->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    mod->target_data = LLVMCreateTargetDataLayout(mod->target_machine);
    isilver(llflag, "Dwarf Version",      5);
    isilver(llflag, "Debug Info Version", 3);

    mod->file = LLVMDIBuilderCreateFile( // create mod file reference (the source file for debugging)
        mod->dbg,
        cast(mod->source_file, cstr), cast(mod->source_file, sz),
        cast(mod->source_path, cstr), cast(mod->source_path, sz));
    
    mod->compile_unit = LLVMDIBuilderCreateCompileUnit(
        mod->dbg, LLVMDWARFSourceLanguageC, mod->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);

    mod->tokens = new(Tokens, file, full_path);
    isilver(define_C99);
    isilver(parse_top);
    isilver(include, str("stdio.h"));
    isilver(build_dependencies);
    
    /// build into functions and class methods
    map base_members = isilver(top_members);
    pairs (mod->defs, e) {
        type def = e->value;
        /// create members out of included functions and external data (may make sense to do this directly)
        if (def->from_include && def->mdl == model_function)
            set(base_members, def->name, def->info);
    }

    
    pairs (mod->defs, e) {
        type def = e->value;
        // for each type def with a body to build
        if (def->mdl == model_function && !def->from_include) {
            isilver(build_function, def);
        } else if (def->mdl == model_class) {
            pairs_ (def->members, e) {
                dim member = e->value;
                if (member->type->mdl == model_function)
                    isilver(build_function, member->type);
            }
        }
    }
    isilver(pop_member_stack);

    LLVMDIBuilderFinalize(mod->dbg);
    
    write(mod); /// write mod we just read in; if we can get away with bulk LLVM code it may be alright to stay direct
}

define_enum(model)
define_enum(Visibility)

define_class(type)
define_class(dim)
define_class(silver)

define_primitive(LLVMValueRef, raw, 0)