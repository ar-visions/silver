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
    assert (len(ordered), "no items");
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
    map                 imports;
    Tokens              tokens;
} isilver;

typedef struct itype {
    LLVMTypeRef         type_ref;
    LLVMValueRef        value_ref;
    LLVMMetadataRef     sub_routine;
    LLVMMetadataRef     dbg;
    LLVMBasicBlockRef   entry;
} itype;

#define isilver(I,N,...)   silver_##N(I, ## __VA_ARGS__)
#define ifunction(I,N,...) function_##N(I, ## __VA_ARGS__)
#define itype(I,N,...)     type_##N(I, ## __VA_ARGS__)

dim dim_init(dim a) {
    assert(a->type, "type (required arg) not set");
    return a;
}

LLVMTypeRef dim_type_ref(dim a) {
    itype*       f = a->type->intern;
    LLVMTypeRef  t = f->type_ref;
    for (int i = 0; i < a->depth; i++)
        t = LLVMPointerType(t, 0);
    return t;
}

LLVMValueRef type_dbg(type t) {
    itype* f = t->intern;
    return f->dbg;
}

void type_set_body(type a) {
    isilver* i = a->module->intern;
    itype*   f = a->intern;

    assert(a->mdl == model_function, "set_body must be called on a function type");

    // Create debug info for function type
    f->sub_routine = LLVMDIBuilderCreateSubroutineType(i->dbg, i->file, NULL, 0, 0);
    f->dbg = LLVMDIBuilderCreateFunction(
        i->dbg, i->compile_unit,
        a->name->chars, a->name->len,
        a->name->chars, a->name->len,
        i->file, 1, f->sub_routine, 0, 1, 1, 0, LLVMDIFlagPrototyped);

    LLVMSetSubprogram(f->value_ref, f->dbg); // Set the debug info for the function
    LLVMBasicBlockRef entry   = LLVMAppendBasicBlock(f->value_ref, "entry"); // Create a basic block to hold the function body
    LLVMBuilderRef    builder = LLVMCreateBuilder(); // Create a builder to add instructions
    LLVMPositionBuilderAtEnd(builder, entry); // Create a global string constant for "Hello, World!"
    LLVMValueRef      hello_world   = LLVMBuildGlobalStringPtr(builder, "Hello, World!", "hello_str");
    LLVMTypeRef  printf_arg_types[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    LLVMTypeRef  printf_type        = LLVMFunctionType(LLVMInt32Type(), printf_arg_types, 1, true);
    LLVMValueRef printf_func        = LLVMGetNamedFunction(i->module, "printf");
    if (!printf_func)
         printf_func = LLVMAddFunction(i->module, "printf", printf_type);

    // Call printf
    LLVMValueRef args[] = { hello_world };
    LLVMBuildCall2(builder, printf_type, printf_func, args, 1, "");
    LLVMBuildRet(builder, hello_world);
    LLVMDisposeBuilder(builder);
}

void type_init(type a) {
    assert(a->module, "module not set");
    assert(a->name,   "name not set");

    a->intern  = A_struct(itype);
    isilver* i = a->module->intern;
    itype*   f = a->intern;
    bool handled_members = false;

    switch (a->mdl) {
        case model_function: {
            assert(a->rtype,  "rtype");
            assert(a->args,   "args");
            int n_args = a->args->count;
            int index  = 0;
            LLVMTypeRef* param_types = calloc(n_args, sizeof(LLVMTypeRef));
            cstr*        param_names = calloc(n_args, sizeof(cstr));

            // create function type
            f->type_ref  = LLVMFunctionType(dim_type_ref(a->rtype), NULL, 0, false);
            f->value_ref = LLVMAddFunction(i->module, a->name, f->type_ref);

            // set arg names
            enumerate(a->args, arg) {
                string arg_name = arg->key;
                dim    arg_type = arg->value;
                AType     arg_t = isa(arg_type);
                assert(arg_t == typeid(dim), "type mismatch");
                LLVMValueRef param = LLVMGetParam(f->value_ref, index);
                LLVMSetValueName2(param, arg_name->chars, arg_name->len);
                index++;
            }

            free(param_types);
            free(param_names);
            break;
        }
        case model_bool:   f->type_ref = LLVMInt1TypeInContext  (i->llvm_context); break;
        case model_i8:     f->type_ref = LLVMInt8TypeInContext  (i->llvm_context); break;
        case model_i16:    f->type_ref = LLVMInt16TypeInContext (i->llvm_context); break;
        case model_i32:    f->type_ref = LLVMInt32TypeInContext (i->llvm_context); break;
        case model_i64:    f->type_ref = LLVMInt64TypeInContext (i->llvm_context); break;
        case model_u8:     f->type_ref = LLVMInt8TypeInContext  (i->llvm_context); break;
        case model_u16:    f->type_ref = LLVMInt16TypeInContext (i->llvm_context); break;
        case model_u32:    f->type_ref = LLVMInt32TypeInContext (i->llvm_context); break;
        case model_u64:    f->type_ref = LLVMInt64TypeInContext (i->llvm_context); break;
        case model_f32:    f->type_ref = LLVMFloatTypeInContext (i->llvm_context); break;
        case model_f64:    f->type_ref = LLVMDoubleTypeInContext(i->llvm_context); break;
        case model_void:   f->type_ref = LLVMVoidTypeInContext  (i->llvm_context); break;
        case model_typedef: {
            assert (a->origin && isa(a->origin) == typeid(dim), "origin must be a reference");
            f->type_ref = dim_type_ref(a->origin);
            if (i->dbg) {
                assert(type_dbg(a->origin), "no debug info set on origin");
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
                assert(isa(member_r) == typeid(dim), "mismatch");
                member_types[index] = dim_type_ref(member_r);
                index++;
            }
            f->type_ref = LLVMStructCreateNamed(LLVMGetGlobalContext(), a->name);
            LLVMStructSetBody(f->type_ref, member_types, index, 0);
            handled_members = true;
            break;
        }
        case model_union: {
            assert (false, "not implemented");
            break;
        }
    }
    assert (!a->members || handled_members, "members given and not processed");
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
    assert (full_path, "include path not found for %o", include);
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, full_path->chars, NULL, 0, NULL, 0, CXTranslationUnit_None);

    assert(unit, "unable to parse translation unit %o", include);
    
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

/// set_body must be called after the top pass; thats when we have the imports and top level definitions read
/// again silver will not have embedded structs, classes and enums its very inaccessible to do so
/// for internals its better to use intern on enums and classes
void silver_parse_top(silver a) {
    isilver* i      = a->intern;
    Tokens   tokens = i->tokens;
    while (cast(tokens, bool)) {
        if (next_is(tokens, "import")) {
            EImport def = new(EImport, tokens, tokens);
            assert (len(def->name) > 0, "import requires name");
            call(a->defs, set, def->name, def);
            continue;
        } else if (next_is(tokens, "class")) {
            assert (false, "not implemented");
            //EClass def = new(EClass, tokens, tokens);
            //call(a->defs, set, def->name, def);
            continue;
        } else {
            assert (false, "unexpected: %o", call(tokens, peek));
        }
    }
}

void silver_define_C99(silver a) {
    isilver* i = a->intern;
    map defs = a->defs = new(map, hsize, 64);
    
    type def = new(type, module, a, name, str("bool"), mdl, model_bool, imported, typeid(bool));
    call(defs, set, str("bool"), def);
    call(defs, set, str("i8"),   new(type, module, a, name, str("i8"),   mdl, model_i8,   imported, typeid(i8)));
    call(defs, set, str("i16"),  new(type, module, a, name, str("i16"),  mdl, model_i16,  imported, typeid(i16)));
    call(defs, set, str("i32"),  new(type, module, a, name, str("i32"),  mdl, model_i32,  imported, typeid(i32)));
    call(defs, set, str("i64"),  new(type, module, a, name, str("i64"),  mdl, model_i64,  imported, typeid(i64)));
    call(defs, set, str("u8"),   new(type, module, a, name, str("u8"),   mdl, model_u8,   imported, typeid(u8)));
    call(defs, set, str("u16"),  new(type, module, a, name, str("u16"),  mdl, model_u16,  imported, typeid(u16)));
    call(defs, set, str("u32"),  new(type, module, a, name, str("u32"),  mdl, model_u32,  imported, typeid(u32)));
    call(defs, set, str("u64"),  new(type, module, a, name, str("u64"),  mdl, model_u64,  imported, typeid(u64)));
    call(defs, set, str("void"), new(type, module, a, name, str("void"), mdl, model_void, imported, typeid(none)));

    /// test out basic type system with code generation in functions outside of classes.
    /// implement function first!
    map args = new(map);
    dim dim_i32 = new(dim, type, module_def(a, "i32"));
    call(args, set, str("arg1"), dim_i32);

    type      i32_t = module_def(a, "i32");
    dim      rtype = new(dim, type, i32_t, depth, 0);
    itype* i_i32_t = i32_t->intern;
    type      fn    = new(type,
        name,     str("func_name"), 
        module,   a,
        mdl,      model_function,
        rtype,    rtype,
        args,     args);
    call(defs, set, str("func_name"), fn);
}

bool silver_build_dependencies(silver a) {
    //global build_root
    enumerate(a->defs, e) {
        string key = e->key;
        if (!inherits(e->value, EImport))
            continue;
        EImport im = e->value;
        call(im, process);
        switch (im->import_type) {
            case ImportType_source:
                if (len(im->main_symbol))
                    call(a->main_symbols, push, im->main_symbol);
                each(im->source, string, source) {
                    // these are built as shared library only, or, a header file is included for emitting
                    if (call(source, has_suffix, ".rs") || call(source, has_suffix, ".h"))
                        continue;
                    string buf = format("%o/%s.o", a->install, source);
                    call(a->compiled_objects, push, buf);
                }
            case ImportType_library:
            case ImportType_project:
                call(a->libraries_used, concat, im->links);
                break;
            default:
                assert(false, "not handled: %i", im->import_type);
        }
    }
    return true;
}

void silver_init(silver a) {
    assert(a->source, "module name not set");

    a->intern      = A_struct(isilver);
    isilver*     i = a->intern;
    a->source_path = call(a->source, directory);
    a->source_file = call(a->source, filename);

    print("LLVM Version: %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);

    path full_path = form(path, "%o/%o", a->source_path, a->source_file);
    assert(call(full_path, exists), "source (%o) does not exist", full_path);

    i->module       = LLVMModuleCreateWithName(a->source_file->chars);
    i->llvm_context = LLVMGetModuleContext(i->module);
    i->dbg          = LLVMCreateDIBuilder(i->module);

    isilver(a, llflag, "Dwarf Version",      5);
    isilver(a, llflag, "Debug Info Version", 3);

    i->file = LLVMDIBuilderCreateFile( // create a file reference (the source file for debugging)
        i->dbg,
        cast(a->source_file, cstr), cast(a->source_file, sz),
        cast(a->source_path, cstr), cast(a->source_path, sz));
    i->compile_unit = LLVMDIBuilderCreateCompileUnit(
        i->dbg, LLVMDWARFSourceLanguageC, i->file,
        cast(a->source_file, cstr),
        cast(a->source_file, sz), 0, "", 0,
        3, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "/", 1, "", 0);

    i->tokens = new(Tokens, file, full_path);

    isilver(a, define_C99);
    isilver(a, parse_top);
    isilver(a, include, str("stdio.h"));
    // we want to include stdio and perform syntax parsing on a function
    // isolated reverse decent for just a sub-set of assignment and expressions.
    // no operators yet
    isilver(a, build_dependencies);

    // define a struct type in the custom language
    LLVMTypeRef structType = LLVMStructCreateNamed(i->llvm_context, "MyStruct");
    LLVMTypeRef elementTypes[] = {
        LLVMInt32Type(), LLVMInt32Type(),
        LLVMInt32Type(), LLVMInt32Type()
    };
    LLVMStructSetBody(structType, elementTypes, 4, 0);

    // create debug info for struct
    LLVMMetadataRef memberDebugTypes[4];
    for (int m = 0; m < 4; m++) {
        memberDebugTypes[m] = LLVMDIBuilderCreateBasicType(
            i->dbg, "int", 3, 32, 
            (LLVMDWARFTypeEncoding)0x05,
            (LLVMDIFlags)0); /// signed 0x05 (not defined somehwo) DW_ATE_signed
    }

    LLVMMetadataRef structDebugType = LLVMDIBuilderCreateStructType(
        i->dbg, i->compile_unit, "MyStruct", 8, i->file, 1, 
        128, 32, 0, NULL, 
        memberDebugTypes, 4, 0, NULL, "", 0);

    // create a function and access the struct member
    map args = new(map);
    call(args, set, str("argc"), new(dim, type, module_def(a, "i32"),  depth, 0));
    call(args, set, str("argv"), new(dim, type, module_def(a, "cstr"), depth, 1));
    dim rtype = new(dim, type, call(a->defs, get, str("i32")));
    type  fn = new(type,
        name,  "main",
        module, a,
        rtype,  rtype,
        args,   args);

    //call(fn, from_tokens) -- lets import parse_statements -> parse_expression
    LLVMBuilderRef builder = LLVMCreateBuilder();

    /// this is 'finalize' for a method, after we call parse on module, parsing all members in each class or struct
    // Create a pointer to MyStruct (simulate `self` in your custom language)
    LLVMValueRef structPtr = LLVMBuildAlloca(builder, structType, "self");

    // Create debug info for the local variable
    LLVMMetadataRef localVarDIType = LLVMDIBuilderCreatePointerType(
        i->dbg, structDebugType, 64, 0, 0, "MyStruct*", 9);
    LLVMMetadataRef localVarDbgInfo = LLVMDIBuilderCreateAutoVariable(
        i->dbg, type_dbg(fn), "self", 4, i->file, 2, localVarDIType, 1, 0, 0);
    
    LLVMDIBuilderInsertDeclareAtEnd(
        i->dbg,
        structPtr,
        localVarDbgInfo,
        LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(i->llvm_context, 2, 0, type_dbg(fn), NULL),
        LLVMGetInsertBlock(builder));

    // Set values for struct members
    symbol memberNames[]  = { "member1", "member2", "member3", "member4" };
    int    memberValues[] = { 42, 44, 46, 48 };
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(i->llvm_context);

    for (int m = 0; m < 4; m++) {
        LLVMValueRef memberPtr = LLVMBuildStructGEP2(builder, structType, structPtr, m, memberNames[m]);
        LLVMBuildStore(builder, LLVMConstInt(int32Type, memberValues[m], 0), memberPtr);

        // Create debug info for each member
        LLVMMetadataRef memberDebugInfo = LLVMDIBuilderCreateAutoVariable(
            i->dbg, type_dbg(fn), memberNames[m], strlen(memberNames[m]),
            i->file, m + 3, memberDebugTypes[m], 0, 0, 0);
        LLVMDIBuilderInsertDeclareAtEnd(i->dbg, memberPtr, memberDebugInfo,
            LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
            LLVMDIBuilderCreateDebugLocation(i->llvm_context, m + 3, 0, type_dbg(fn), NULL),
            LLVMGetInsertBlock(builder));

        isilver(a, set_line, 3 + m, 0);
    }
    isilver(a, set_line, 7, 0);

    // Return from main
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, false));
    LLVMDIBuilderFinalize(i->dbg);
    isilver(a, write);
}

define_enum(model)

define_class(type)
define_class(dim)
define_class(silver)
