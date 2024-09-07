#include <silver>
#include <tokens>

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>



item first_key_value(map ordered) {
    assert (len(ordered), "no items");
    return ((map_item)ordered->refs->first->val)->value;
}

string exe_ext() {
    #ifdef _WIN32
        return str("exe");
    #endif
    return str("");
}

string shared_ext() {
#ifdef _WIN32
    return str("dll");
#elif defined(__APPLE__)
    return str("dylib");
#else
    return str("so");
#endif
}

string static_ext() {
#ifdef _WIN32
    return str("lib");
#else
    return str("a");
#endif
}

string lib_prefix() {
#ifdef _WIN32
    return str("");
#else
    return str("lib");
#endif
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
    string              source_file;
    string              source_path;
    array               tokens;
    bool                debug;
} isilver;

typedef struct ifunction {
    LLVMTypeRef         fn_type;
    LLVMValueRef        fn;
    LLVMMetadataRef     sub_routine;
    LLVMMetadataRef     dbg;
    LLVMBasicBlockRef   entry;
} ifunction;

#define icall(I,N,...) silver_##N(I, ## __VA_ARGS__)

/// reference is a good wrapper for AType, it helps distinguish its usage in silver's design-time
/// flatten out references on construction
reference reference_with_AType(reference a, AType type, num refs, silver module) {
    a->type     = type;
    a->module   = module;
    reference r = a;
    while (r) {
        A f = A_fields(r);
        if (f->type) { /// if the object has a type then its a reference
            a->refs += r->refs;
            r        = r->type;
        } else {
            if (r != a) a->type = r;
            break;
        }
    }
    a->refs += refs;
    return a;
}

LLVMTypeRef reference_llvm(reference a) {
    LLVMTypeRef t = null;
    AType      at = a->type;
         if (at == typeid(bool)) t = LLVMInt1Type();
    else if (at == typeid(i8))   t = LLVMInt8Type();
    else if (at == typeid(i16))  t = LLVMInt16Type();
    else if (at == typeid(i32))  t = LLVMInt32Type();
    else if (at == typeid(i64))  t = LLVMInt64Type();
    else if (at == typeid(u8))   t = LLVMInt8Type();
    else if (at == typeid(u16))  t = LLVMInt16Type();
    else if (at == typeid(u32))  t = LLVMInt32Type();
    else if (at == typeid(u64))  t = LLVMInt64Type();
    else if (at == typeid(f32))  t = LLVMFloatType();
    else if (at == typeid(f64))  t = LLVMDoubleType();
    else {
        ///
        assert(false, "not implemented");
    }
    for (int i = 0; i < a->refs; i++)
        t = LLVMPointerType(t, 0);
    return t;
}

function function_with_cstr(function a, cstr name, silver module, reference rtype, map args) {
    isilver*     i = module->intern;
    ifunction*   f = a->intern; 
    sz         len = strlen(name);
    f->fn_type     = LLVMFunctionType(reference_llvm(rtype), NULL, 0, false);
    f->fn          = LLVMAddFunction(i->module, name, f->fn_type);
    f->sub_routine = LLVMDIBuilderCreateSubroutineType(i->dbg, i->file, NULL, 0, 0);
    f->dbg         = LLVMDIBuilderCreateFunction(
        i->dbg, i->compile_unit, name, len, name, len,
        i->file, 1, f->sub_routine, 0, 1, 1, 0, LLVMDIFlagPrototyped);
    LLVMSetSubprogram(f->fn, f->dbg);
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

LLVMValueRef function_dbg(function fn) {
    ifunction* f = fn->intern;
    return f->dbg;
}

#define EVisibility_schema(X,Y) \
    enum_value(X,Y, undefined) \
    enum_value(X,Y, intern) \
    enum_value(X,Y, public)
declare_enum(EVisibility)
define_enum(EVisibility)

#define EImportType_schema(X,Y) \
    enum_value(X,Y, none) \
    enum_value(X,Y, source) \
    enum_value(X,Y, library) \
    enum_value(X,Y, project)
declare_enum(EImportType)
define_enum(EImportType)

#define BuildState_schema(X,Y) \
    enum_value(X,Y, none) \
    enum_value(X,Y, built)
declare_enum(BuildState)
define_enum(BuildState)

#define EImport_schema(X,Y,Z) \
    i_intern(X,Y,Z, silver, module) \
    i_intern(X,Y,Z, string, name) \
    i_intern(X,Y,Z, string, source) \
    i_intern(X,Y,Z, bool,   imported) \
    i_intern(X,Y,Z, array,  includes) \
    i_intern(X,Y,Z, array,  cfiles) \
    i_intern(X,Y,Z, array,  links) \
    i_intern(X,Y,Z, array,  build_args) \
    i_intern(X,Y,Z, EImportType, import_type) \
    i_intern(X,Y,Z, array, library_exports) \
    i_intern(X,Y,Z, string, visibility) \
    i_intern(X,Y,Z, string, main_symbol)
declare_class(EImport)


path create_folder(silver module, cstr name, cstr sub) {
    isilver* i = module->intern;
    string dir = format("%o/%s%s%s", i->source_path, name, sub ? "/" : "", sub ? sub : "");
    path   res = cast(dir, path);
    call(res, make_dir);
    return res;
}

BuildState EImport_build_project(EImport a, string name, string url) {
    path checkout = create_folder(a->module, "checkouts", name->chars);
    path i        = create_folder(a->module, "install", null);
    path b        = form(path, "%o/%s", checkout, "silver-build");

    /// clone if empty
    if (call(checkout, is_empty)) {
        char   at[2]  = { '@', 0 };
        string f      = form(string, "%s", at);
        num    find   = call(url, index_of, at);
        string branch = null;
        string s_url  = url;
        if (find > -1) {
            s_url  = call(url, mid, 0, find);
            branch = call(url, mid, find + 1, len(url) - (find + 1));
        }
        string cmd = format("git clone %o %o", s_url, checkout);
        assert (system(cmd->chars) == 0, "git clone failure");
        if (len(branch)) {
            chdir(checkout->chars);
            cmd = form(string, "git checkout %o", branch);
            assert (system(cmd->chars) == 0, "git checkout failure");
        }
        call(b, make_dir);
    }
    /// intialize and build
    if (!call(checkout, is_empty)) { /// above op can add to checkout; its not an else
        chdir(checkout->chars);

        bool build_success = file_exists("%o/silver-token", b);
        if (file_exists("silver-init.sh") && !build_success) {
            string cmd = format("%s/silver-init.sh \"%s\"", path_type.cwd(2048), i);
            assert(system(cmd->chars) == 0, "cmd failed");
        }
    
        bool is_rust = file_exists("Cargo.toml");
        ///
        if (is_rust) {
            cstr rel_or_debug = "release";
            path package = form(path, "%o/%s/%o", i, "rust", name);
            call(package, make_dir);
            ///
            setenv("RUSTFLAGS", "-C save-temps", 1);
            setenv("CARGO_TARGET_DIR", package->chars, 1);
            string cmd = format("cargo build -p %o --%s", name, rel_or_debug);

            assert (system(cmd->chars) == 0, "cmd failed");
            path   lib = form(path, "%o/%s/lib%o.so", package, rel_or_debug, name);
            path   exe = form(path, "%o/%s/%o_bin",   package, rel_or_debug, name);
            if (!file_exists(exe->chars))
                exe = form(path, "%o/%s/%o", package, rel_or_debug, name);
            if (file_exists(lib->chars)) {
                path sym = form(path, "%o/lib%o.so", i, name);
                a->links = array_of(typeid(string), name, null);
                create_symlink(lib, sym);
            }
            if (file_exists(exe->chars)) {
                path sym = form(path, "%o/%o", i, name);
                create_symlink(exe, sym);
            }
        }   
        else {
            assert (file_exists("CMakeLists.txt"), "CMake required for project builds");

            string cmake_flags = str("");
            each(array, a->build_args, string, arg) {
                if (cast(cmake_flags, bool))
                    call(cmake_flags, append, " ");
                call(cmake_flags, append, arg->chars);
            }
            if (!build_success) {
                string cmd = format("cmake -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -B %o -DCMAKE_INSTALL_PREFIX=%o %o", b, i, cmake_flags);
                assert (system(cmd->chars) == 0, "cmd failed");
                chdir(b->chars);
                assert (system("make -j16 install") == 0, "install failed");
            }
            
            if (!a->links) a->links = array_of(typeid(string), name, null);
            each(array, a->links, string, name) {
                string pre = lib_prefix();
                string ext = shared_ext();
                path   lib = form(path, "%o/lib/%o%o.%o", i, pre, name, ext);
                if (!file_exists(lib)) {
                    pre = lib_prefix();
                    ext = static_ext();
                    lib = form(path, "%o/lib/%o%o.%o", i, pre, name, ext);
                }
                assert (file_exists(lib), "lib does not exist");
                path sym = form(path, "%o/%o%o.%o", i, pre, name, ext);
                create_symlink(lib, sym);
            }

            FILE*  silver_token = fopen("silver-token", "w");
            fclose(silver_token);
        }
    }
    return BuildState_built;
}

bool contains_main(path obj_file) {
    string cmd = format("nm %o", obj_file);
    FILE *fp = popen(cmd->chars, "r");
    assert(fp, "failure to open %o", obj_file);
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, " T main") != NULL) {
            pclose(fp);
            return true;
        }
    }

    pclose(fp);
    return false;
}

BuildState EImport_build_source(EImport a) {
    isilver* i = a->module->intern;
    bool is_debug = i->debug;
    string build_root = i->source_path;
    each (array, a->cfiles, string, cfile) {
        path cwd = M(path, cwd, 1024);
        string compile;
        if (call(cfile, has_suffix, ".rs")) {
            // rustc integrated for static libs only in this use-case
            compile = format("rustc --crate-type=staticlib -C opt-level=%s %o/%o --out-dir %o",
                is_debug ? "0" : "3", cwd, cfile, build_root);
        } else {
            cstr opt = is_debug ? "-g2" : "-O2";
            compile = format("gcc -I%o/install/include %s -Wfatal-errors -Wno-write-strings -Wno-incompatible-pointer-types -fPIC -std=c99 -c %o/%o -o %o/%o.o",
                build_root, opt, cwd, cfile, build_root, cfile);
        }
        
        path   obj_path   = form(path,   "%o.o", cfile);
        string log_header = form(string, "import: %o source: %o", a->name, cfile);
        print("%s > %s", cwd, compile);
        assert (system(compile) == 0,  "%o: compilation failed",    log_header);
        assert (file_exists(obj_path), "%o: object file not found", log_header);

        if (contains_main(obj_path)) {
            a->main_symbol = format("%o_main", call(obj_path, stem));
            string cmd = format("objcopy --redefine-sym main=%o %o", a->main_symbol, obj_path);
            assert (system(cmd->chars) == 0, "%o: could not replace main symbol", log_header);
        }

    return BuildState_built;
}

bool EImport_process(EImport a) {
    if (len(a->name) && !len(a->source) && len(a->includes)) {
        attempt = ['','spec/']
        exists = False;
        for ia in range(len(attempt)):
            pre = attempt[ia]
            si_path = Path('%s%s.si' % (pre, a->name))
            if not si_path.exists():
                continue
            a->module_path = si_path
            print('module %s' % si_path)
            a->module = si_path
            exists = True
            break
        
        assert exists, 'path does not exist for silver module: %s' % a->name
    } else if (len(a->name) && len(a->source)) {                        
        has_c  = false;
        has_h  = false;
        has_rs = false;
        has_so = false;
        has_a  = false;
        each(array, a->source, string, i0) {
            if i0.endswith('.c'):
                has_c = True
                break
            if i0.endswith('.h'):
                has_h = True
                break
            if i0.endswith('.rs'):
                has_rs = True
                break
            if i0.endswith('.so'):
                has_so = True
                break
            if i0.endswith('.a'):
                has_a = True
                break
        
        if has_h:
            a->import_type = EImport.source # we must do 1 at a time, currently
        elif has_c || has_rs:
            assert os.chdir(a->relative_path) == 0
            # build this single module, linking all we have imported prior
            a->import_type = EImport.source
            a->build_source()
        elif has_so:
            assert os.chdir(a->relative_path) == 0
            # build this single module, linking all we have imported prior
            a->import_type = EImport.library
            if not a->library_exports:
                a->library_exports = []
            for i in a->source:
                lib = a->source[i]
                rem = lib.substr(0, len(lib) - 3)
                a->library_exports.append(rem)
        elif has_a:
            assert(os.chdir(a->relative_path) == 0);
            # build this single module, linking all we have imported prior
            a->import_type = EImport.library
            if not a->library_exports:
                a->library_exports = []
            for i in a->source:
                lib = a->source[i]
                rem = lib.substr(0, len(lib) - 2)
                a->library_exports.append(rem)
        } else {
            # source[0] is the url, we need to register the libraries we have
            # for now I am making a name relationship here that applies to about 80% of libs
            assert len(a->source) == 1, ''
            a->import_type = EImport.project
            a->build_project(a->name, a->source[0])
            if not a->library_exports:
                a->library_exports = []
            # only do this if it exists
            a->library_exports.append(a->name)
        }
    }
    // parse all headers with libclang
    module.process_includes(a->includes)
}


















void silver_parse_top(silver a) {
    isilver* i      = a->intern;
    Tokens   tokens = i->tokens;
    while (cast(tokens, bool)) {
        if (next_is(tokens, "import")) {
            /// like before, we support 'import' keyword first!
            /// add to a->defs
            import im = new(import);
            call(a->defs, set, def->name, def);
            continue;
        }
    }
}

void silver_init(silver a) {
    isilver* i = a->intern;

    i->debug = true;
    print("LLVM Version: %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);

    path full_path = form(path, "%o/%o", i->source_path, i->source_file);
    assert(call(full_path, exists));

    i->module       = LLVMModuleCreateWithName(i->source_file->chars);
    i->llvm_context = LLVMGetModuleContext(i->module);
    i->dbg          = LLVMCreateDIBuilder(i->module);

    icall(a, llflag, "Dwarf Version",      5);
    icall(a, llflag, "Debug Info Version", 3);

    i->file = LLVMDIBuilderCreateFile( // create a file reference (the source file for debugging)
        i->dbg,
        cast(i->source_file, cstr), cast(i->source_file, sz),
        cast(i->source_path, cstr), cast(i->source_path, sz));
    
    i->compile_unit = LLVMDIBuilderCreateCompileUnit(
        i->dbg, LLVMDWARFSourceLanguageC, i->file,
        cast(i->source_file, cstr),
        cast(i->source_file, sz), 0, "", 0,
        3, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "/", 1, "", 0);

    i->tokens = parse_tokens(full_path);
    /// create definitions with tokens inside for further processing
    /// top is just about getting the map to tokens
    icall(a, parse_top);
    
    // define a struct type in the custom language
    LLVMTypeRef structType = LLVMStructCreateNamed(i->llvm_context, "MyStruct");
    LLVMTypeRef elementTypes[] = { LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type() }; // Member: int member
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
    call(args, set, str("argc"), ref(a, typeid(i32),  0));
    call(args, set, str("argv"), ref(a, typeid(cstr), 1));
    function  fn = ctr(function, cstr, "main", a, ref(a, typeid(i32), 0), args);

    //call(fn, from_tokens) -- lets import parse_statements -> parse_expression
    LLVMBuilderRef builder = LLVMCreateBuilder();

    /// this is 'finalize' for a method, after we call parse on module, parsing all members in each class or struct
    // Create a pointer to MyStruct (simulate `self` in your custom language)
    LLVMValueRef structPtr = LLVMBuildAlloca(builder, structType, "self");

    // Create debug info for the local variable
    LLVMMetadataRef localVarDIType = LLVMDIBuilderCreatePointerType(
        i->dbg, structDebugType, 64, 0, 0, "MyStruct*", 9);
    LLVMMetadataRef localVarDbgInfo = LLVMDIBuilderCreateAutoVariable(
        i->dbg, function_dbg(fn), "self", 4, i->file, 2, localVarDIType, 1, 0, 0);
    
    LLVMDIBuilderInsertDeclareAtEnd(
        i->dbg,
        structPtr,
        localVarDbgInfo,
        LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(i->llvm_context, 2, 0, function_dbg(fn), NULL),
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
            i->dbg, function_dbg(fn), memberNames[m], strlen(memberNames[m]),
            i->file, m + 3, memberDebugTypes[m], 0, 0, 0);
        LLVMDIBuilderInsertDeclareAtEnd(i->dbg, memberPtr, memberDebugInfo,
            LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
            LLVMDIBuilderCreateDebugLocation(i->llvm_context, m + 3, 0, function_dbg(fn), NULL),
            LLVMGetInsertBlock(builder));

        icall(a, set_line, 3 + m, 0);
    }
    icall(a, set_line, 7, 0);

    // Return from main
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, false));
    LLVMDIBuilderFinalize(i->dbg);
    icall(a, write);
}

silver silver_with_string(silver a, string module) {
    a->intern      = A_struct(silver);
    isilver*     i = a->intern;
    i->source_path = str("/home/kalen/src/silver/silver-build");
    i->source_file = format("%o.c", module);
    return a;
}

define_class(reference)
define_class(function)
define_class(silver)
