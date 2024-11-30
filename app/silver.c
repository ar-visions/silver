#include <import>

/// silver 88
/// the goal of 88 is to support class, enum, struct with operator overloads
/// import keyword to import other silver modules as well as any other source project, including
/// git repos that build rust, C or C++ [ all must export C functions ]

#define   emodel(MDL)    ({ \
    member  m = ether_lookup(mod, string(MDL), typeid(import)); \
    model mdl = m ? m->mdl : null; \
    mdl; \
})

static map   operators;
static array keywords;
static array assign;
static array compare;


static bool is_alpha(A any);
static node parse_expression(silver mod);
static node parse_primary(silver mod);


static void print_tokens(symbol label, silver mod) {
    print("[%s] tokens: %o %o %o %o %o ...", label,
        element(mod, 0), element(mod, 1),
        element(mod, 2), element(mod, 3),
        element(mod, 4), element(mod, 5));
}


typedef struct {
    OPType ops   [2];
    string method[2];
    string token [2];
} precedence;


static precedence levels[] = {
    { { OPType__mul,       OPType__div        } },
    { { OPType__add,       OPType__sub        } },
    { { OPType__and,       OPType__or         } },
    { { OPType__xor,       OPType__xor        } },
    { { OPType__right,     OPType__left       } },
    { { OPType__is,        OPType__inherits   } },
    { { OPType__compare_equal, OPType__compare_not } },
    { { OPType__value_default, OPType__cond_value } } // i find cond-value to be odd, but ?? (value_default) should work for most
};


static string op_lang_token(string name) {
    pairs(operators, i) {
        string token = i->key;
        string value = i->value;
        if (eq(name, value->chars))
            return token;
    }
    fault("invalid operator name: %o", name);
    return null;
}


static void init() {
    keywords = array_of_cstr(
        "class",  "proto",    "struct", "import", "typeof",   "schema", "is", "inherits",
        "init",   "destruct", "ref",    "const",  "volatile", "require", "no-op",
        "return", "->", "::", "...", "asm", "if", "switch",   "any", "enum", "ifdef", "else",
        "while",  "for",      "do",     "signed", "unsigned", "cast", "fn", null);
    assign = array_of_cstr(
        ":", "=", "+=", "-=", "*=", "/=", 
        "|=", "&=", "^=", ">>=", "<<=", "%=", null);
    compare = array_of_cstr("==", "!=", null);

    string add = string("add");
    operators = map_of( /// ether quite needs some operator bindings, and resultantly ONE interface to use them
        "+",        string("add"),
        "-",        string("sub"),
        "*",        string("mul"),
        "/",        string("div"),
        "||",       string("or"),
        "&&",       string("and"),
        "^",        string("xor"),
        ">>",       string("right"),
        "<<",       string("left"),
        "??",       string("value_default"),
        "?:",       string("cond_value"),
        ":",        string("assign"),
        "=",        string("assign"),
        "%=",       string("assign_mod"),
        "+=",       string("assign_add"),
        "-=",       string("assign_sub"),
        "*=",       string("assign_mul"),
        "/=",       string("assign_div"),
        "|=",       string("assign_or"),
        "&=",       string("assign_and"),
        "^=",       string("assign_xor"),
        ">>=",      string("assign_right"),
        "<<=",      string("assign_left"),
        "==",       string("compare_equal"),
        "!=",       string("compare_not"),
        "is",       string("is"),
        "inherits", string("inherits"), null);
    
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 2; j++) {
            OPType op        = level->ops[j];
            string e_name    = estr(OPType, op);

            //({ __typeof__(e_name) i = e_name; ftableI(i)->len(i, ## __VA_ARGS__); })

            string op_name   = mid(e_name, 1, len(e_name) - 1);
            string op_token  = op_lang_token(op_name);
            level->method[j] = op_name;
            level->token [j] = op_token;
        }
    }
}


path create_folder(silver mod, cstr name, cstr sub) {
    string dir = format(
        "%o/%s%s%s", mod->install, name,
            (sub && *sub) ? "/" : "",
            (sub && *sub) ? sub : "");
    path   res = cast(path, dir);
    make_dir(res);
    return res;
}


typedef struct {
    symbol lib_prefix;
    symbol exe_ext, static_ext, shared_ext; 
} exts;

exts get_exts() {
    return (exts)
#ifdef _WIN32
    { "", "exe", "lib", "dll" }
#elif defined(__APPLE__)
    { "", "",    "a",   "dylib" }
#else
    { "", "",    "a",   "so" }
#endif
    ;
}


array import_list(silver mod, bool use_text) {
    array list = new(array);
    if (use_text || next_is(mod, "[")) {
        if (!use_text)
            consume(mod);
        string word = new(string, alloc, 64);
        int neighbor_x = -1;
        int neighbor_y = -1;
        while (true) {
            token arg = next(mod);
            if (eq(arg, "]")) break;
            if (!use_text && get_type(arg) == typeid(string)) {
                push(list, arg->literal);
            } else {
                int x = arg->column;
                int y = arg->line;
                if (neighbor_x == x && neighbor_y == y) {
                    append(word, arg->chars);
                } else {
                    if (word->len)
                        push(list, word);
                    if (isa(arg->literal) == typeid(string))
                        word = new(string, chars, ((string)arg->literal)->chars);
                    else
                        word = new(string, chars, arg->chars);
                }
                neighbor_x = arg->column + strlen(arg->chars); 
                neighbor_y = arg->line;
            }
            
            if (next_is(mod, ","))
                consume(mod);
            
            if (next_is(mod, "]"))
                break;
            
            continue;
        }
        if (word->len)
            push(list, word);
        assert (next_is(mod, "]"), "expected ] after build flags");
        consume(mod);
    } else {
        string next = read_string(mod);
        push(list, next);
    }
    return list;
}

/// this must be basically the same as parsing named args; , should be optional for this
void import_read_fields(import im) {
    silver mod = im->mod;
    while (true) {
        if (next_is(mod, "]")) {
            consume(mod);
            break;
        }
        token arg_name = next(mod);
        if (get_type(arg_name) == typeid(string)) {
            im->source = array_of(typeid(string), string(arg_name->chars), null);
        } else {
            assert (is_alpha(arg_name), "expected identifier for import arg");
            bool    use_tokens  = next_is(mod, "[");
            assert (use_tokens || next_is(mod, ":"),
                "expected : after import arg (argument assignment)");
            consume(mod);
            if (eq(arg_name, "name")) {
                token token_name = next(mod);
                assert (! get_type(token_name), "expected token for import name");
                im->name = string(token_name->chars);
            } else if (eq(arg_name, "links"))    im->links      = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "includes")) im->includes   = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "products")) im->products   = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "source"))   im->source     = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "build"))    im->build_args = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "shell")) {
                token token_shell = next(mod);
                assert (get_type(token_shell), "expected shell invocation for building");
                im->shell = string(token_shell->chars);
            } else if (eq(arg_name, "defines")) {
                // none is a decent name for null.
                assert (false, "not implemented");
            } else
                assert (false, "unknown arg: %o", arg_name);

            if (next_is(mod, "]"))
                break;
        }
    }
}

/// get import keyword working to build into build-root (silver-import)
none import_init(import im) {
    silver mod = im->mod;
    im->includes = array(32);
    im->lookup_omit = true;
    assert(!next_is(mod, "import"), "unexpected import");
    //consume(mod);
    
    //token n_token = next(mod);
    bool is_inc = next_is(mod, "<");
    if (is_inc) {
        im->import_type = import_t_includes;
        consume(mod);
        im->includes = array(8);
        while (1) {
            token inc = next(mod);
            verify (is_alpha(inc), "expected alpha-identifier for header");
            push(im->includes, inc);
            include(mod, string(inc->chars));
            bool is_inc = next_is(mod, ">");
            if (is_inc) {
                consume(mod);
                break;
            }
            token comma = next(mod);
            assert (eq(comma, ","), "expected comma-separator or end-of-includes >");
        }
    } else {
        string module_name = read_alpha(mod);
        assert(is_alpha(module_name), "expected mod name identifier");
        
        if (!im->name) {
            im->name = hold(module_name);
            im->anonymous = true;
            /// we can perform the global translation here if its anonymous
        }

        assert(is_alpha(module_name), format("expected variable identifier, found %o", module_name));
        
        /// [ project fields syntax ]
        if (next_is(mod, "[")) {
            next(mod);
            token n = peek(mod);
            AType s = get_type(n);
            if (s == typeid(string)) {
                im->source = new(array);
                while (true) {
                    token    inner = next(mod);
                    string s_inner = cast(string, inner);
                    assert(get_type(inner) == typeid(string), "expected a string literal");
                    string  source = mid(s_inner, 1, len(s_inner) - 2);
                    push(im->source, source);
                    string       e = next(mod);
                    if (eq(e, ","))
                        continue;
                    assert(eq(e, "]"), "expected closing bracket");
                    break;
                }
            } else {
                import_read_fields(im);
                consume(mod);
            }
        } else if (!im->skip_process) { // should be called a kind of A-type mode
            /// load a silver module
            path rel    = form(path, "%o.cms", module_name);
            path source = absolute(rel);
            im->extern_mod = silver(
                name, module_name, source, source, install, mod->install);
        }
    }
}


string configure_debug(bool debug) {
    return debug ? string("--with-debug") : string("");
}


string cmake_debug(bool debug) {
    return format("-DCMAKE_BUILD_TYPE=%s", debug ? "Debug" : "Release");
}


string make_debug(bool debug) {
    return debug ? string("-g") : string("");
}


void import_extract_libs(import im, string build_dir) {
    exts exts = get_exts();
    path i = im->mod->install;
    if (im->products)
    each(im->products, string, link_name) {
        symbol n   = cstring(link_name);
        symbol pre = exts.lib_prefix;
        symbol ext = exts.shared_ext;
        path   lib = form(path, "%o/lib/%s%s.%s", i, pre, n, ext);
        if (!file_exists(lib)) {
            ext = exts.static_ext;
            lib = form(path, "%o/lib/%s%s.%s", i, pre, n, ext);
        }
        bool exists = file_exists(lib);
        assert (im->assemble_so || exists, "lib does not exist");
        if (exists) {
            path sym = form(path, "%o/%s%s.%s", i, pre, n, ext);
            create_symlink(lib, sym);
            im->assemble_so = false;
        }
    }
    /// combine .a into single shared library; assume it will work
    if (im->assemble_so) {
        path   dawn_build = new(path, chars, build_dir->chars);
        array  files      = ls(dawn_build, string(".a"), true);
        string all        = string("");
        each (files, path, f) {
            if (all->len)
                append(all, " ");
            append(all, f->chars);
        }
        exec("%o/bin/clang -shared -o %o/lib/lib%o.so -Wl,--whole-archive %o -Wl,--no-whole-archive",
            i, i, im->name, all);
    }
}


build_state build_project(import im, string name, string url) {
    path checkout  = create_folder(im->mod, "checkout", name->chars);
    path i         = im->mod->install;
    path build_dir = form(path, "%o/%s", checkout,
        im->mod->with_debug ?
            "silver-debug" : "silver-build");

    path cwd = path_cwd(2048);
    bool dbg = im->mod->with_debug;

    /// clone if empty
    if (is_empty(checkout)) {
        char   at[2]  = { '@', 0 };
        string f      = form(string, "%s", at);
        num    find   = index_of(url, at);
        string branch = null;
        string s_url  = url;
        if (find > -1) {
            s_url     = mid(url, 0, find);
            branch    = mid(url, find + 1, len(url) - (find + 1));
        }
        string cmd = format("git clone %o %o", s_url, checkout);
        assert (system(cmd->chars) == 0, "git clone failure");
        if (len(branch)) {
            chdir(checkout->chars);
            cmd = form(string, "git checkout %o", branch);
            assert (system(cmd->chars) == 0, "git checkout failure");
        }
        make_dir(build_dir);
    }
    /// intialize and build
    if (!is_empty(checkout)) { /// above op can add to checkout; its not an else
        chdir(checkout->chars);

        bool build_success = file_exists("%o/silver-token", build_dir);
        if (file_exists("silver-init.sh") && !build_success) {
            string cmd = format(
                "%o/silver-init.sh \"%s\"", path_type.cwd(2048), i);
            assert(system(cmd->chars) == 0, "cmd failed");
        }
    
        bool is_rust = file_exists("Cargo.toml");

        /// support for Cargo/Makefile/CMake
        if (is_rust) {
            cstr rel_or_debug = "release";
            path package = form(path, "%o/%s/%o", i, "rust", name);
            make_dir(package);
            setenv("RUSTFLAGS", "-C save-temps", 1);
            setenv("CARGO_TARGET_DIR", package->chars, 1);
            string cmd = format("cargo build -p %o --%s", name, rel_or_debug);
            assert (system(cmd->chars) == 0, "cmd failed");
            path   lib = form(path,
                "%o/%s/lib%o.so", package, rel_or_debug, name);
            path   exe = form(path,
                "%o/%s/%o_bin",   package, rel_or_debug, name);
            if (!file_exists(exe->chars))
                exe = form(path, "%o/%s/%o", package, rel_or_debug, name);
            if (file_exists(lib->chars)) {
                path sym = form(path, "%o/lib%o.so", i, name);
                im->products = array_of(typeid(string), name, null);
                create_symlink(lib, sym);
            }
            if (file_exists(exe->chars)) {
                path sym = form(path, "%o/%o", i, name);
                create_symlink(exe, sym);
            }
        } else {
            bool A_build = false;
            im->assemble_so = false;
            if (!im->skip_process && (!im->products || !len(im->products))) { // default to this when initializing
                im->products = array_of(typeid(string), name, null);
                im->assemble_so = true;
            }
            if (file_exists("Makefile")) {
                bool has_config = file_exists("configure") || file_exists("configure.ac");
                if (has_config) {
                    if (!file_exists("configure")) {
                        print("running autoreconf -i in %o", im->name);
                        exec("autoupdate ..");
                        exec("autoreconf -i ..");
                        verify(file_exists("configure"), "autoreconf run, expected configure file");
                    }
                    exec("configure %o --prefix=%o", configure_debug(dbg), im->mod->install);
                } else {
                    A_build = true;
                }
                if (!build_success) {
                    chdir(build_dir->chars);
                    verify(exec("make -f ../Makefile") == 0, "Makefile build failed for %o", im->name);
                }
            } else {
                verify (file_exists("CMakeLists.txt"),
                    "CMake required for project builds");

                string cmake_flags = string("");
                each(im->build_args, string, arg) {
                    if (cast(bool, cmake_flags))
                        append(cmake_flags, " ");
                    append(cmake_flags, arg->chars);
                }
                
                if (!build_success) {
                    string cmake = string(
                        "cmake -S . -DCMAKE_BUILD_TYPE=Release "
                        "-DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON");
                    string cmd   = format(
                        "%o -B %o -DCMAKE_INSTALL_PREFIX=%o %o", cmake, build_dir, i, cmake_flags);
                    assert (system(cmd->chars) == 0, "cmd failed");
                    chdir(build_dir->chars);
                    assert (system("make -j16 install") == 0, "install failed");
                }
            }
            import_extract_libs(im, build_dir);
            FILE*  silver_token = fopen("silver-token", "w");
            fclose(silver_token);
        }
    }

    chdir(cwd->chars);
    return build_state_built;
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


build_state import_build_source(import im) {
    bool is_debug = im->mod->with_debug; /// needs separate debugging for imports; use environment variable for this one
    string install = im->mod->install;
    each (im->cfiles, string, cfile) {
        path cwd = path_cwd(1024);
        string compile;
        if (ends_with(cfile, ".rs")) {
            // rustc integrated for static libs only in this use-case
            compile = format("rustc --crate-type=staticlib -C opt-level=%s %o/%o --out-dir %o/lib",
                is_debug ? "0" : "3", cwd, cfile, install);
        } else {
            cstr opt = is_debug ? "-g2" : "-O2";
            compile = format(
                "gcc -I%o/include %s -Wfatal-errors -Wno-write-strings -Wno-incompatible-pointer-types -fPIC -std=c99 -c %o/%o -o %o/%o.o",
                install, opt, cwd, cfile, install, cfile);
        }
        
        path   obj_path   = form(path,   "%o.o", cfile);
        string log_header = form(string, "import: %o source: %o", im->name, cfile);
        print("%s > %s", cwd, compile);
        assert (system(compile) == 0,  "%o: compilation failed",    log_header);
        assert (file_exists(obj_path), "%o: object file not found", log_header);

        if (contains_main(obj_path)) {
            im->main_symbol = format("%o_main", stem(obj_path));
            string cmd = format("objcopy --redefine-sym main=%o %o",
                im->main_symbol, obj_path);
            assert (system(cmd->chars) == 0,
                "%o: could not replace main symbol", log_header);
        }
    }
    return build_state_built;
}


void import_process_includes(import im, array includes) {
    /// having a singlar expression instead of a statement would be nice for 1 line things in silver
    /// [cast] is then possible, i believe (if we dont want cast keyword)
    /// '{using} in strings, too, so we were using the character'
    /// 
    each(includes, string, e) {
        print("e = %o", e);
    }
}


void import_process(import im) {
    silver mod = im->mod;
    if (im->skip_process) return;
    bool has_name     = (im->name     && len(im->name));
    bool has_source   = (im->source   && len(im->source));
    bool has_includes = (im->includes && len(im->includes));
    /// when not import <, or import name [ ... syntax: import name
    ///                 ^- c-include      ^- build             ^- silver
    if ( im->extern_mod ) {
        array attempt = array_of(typeid(string), string(""), string("spec/"), NULL);
        bool  exists  = false;
        each(attempt, string, pre) {
            path module_path = form(path, "%o%o.si", pre, im->name);
            if (!exists(module_path)) continue;
            im->module_path = module_path;
            print("mod-path %o", module_path);
            exists = true;
            break;
        }
        assert(exists, "path does not exist for silver mod: %o", im->name);
    } else if (has_name && has_source) {
        bool has_c  = false, has_h = false, has_rs = false,
             has_so = false, has_a = false;
        each(im->source, string, i0) {
            if (ends_with(i0, ".c"))   has_c  = true;
            if (ends_with(i0, ".h"))   has_h  = true;
            if (ends_with(i0, ".rs"))  has_rs = true;
            if (ends_with(i0, ".so"))  has_so = true;
            if (ends_with(i0, ".a"))   has_a  = true;
        }
        if (has_h)
            im->import_type = import_t_source;
        else if (has_c || has_rs) {
            im->import_type = import_t_source;
            import_build_source(im);
        } else if (has_so) {
            im->import_type = import_t_library;
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), string(""), NULL);
            each(im->source, string, lib) {
                string rem = mid(lib, 0, len(lib) - 3);
                push(im->library_exports, rem);
            }
        } else if (has_a) {
            im->import_type = import_t_library;
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), string(""), NULL);
            each(im->source, string, lib) {
                string rem = mid(lib, 0, len(lib) - 2);
                push(im->library_exports, rem);
            }
        } else {
            assert(len(im->source) == 1, "source size mismatch");
            im->import_type = import_t_project;
            build_project(im, im->name, idx(im->source, 0));
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), im->name, NULL);
        }
    }
    import_process_includes(im, im->includes);
    switch (im->import_type) {
        case import_t_source:
            if (len(im->main_symbol))
                push(mod->main_symbols, im->main_symbol);
            each(im->source, string, source) {
                // these are built as shared library only, or, a header file is included for emitting
                if (ends_with(source, ".rs") || ends_with(source, ".h"))
                    continue;
                string buf = format("%o/%s.o", mod->install, source);
                push(mod->compiled_objects, buf);
            }
        case import_t_library:
        case import_t_project:
            concat(mod->products_used, im->products);
            break;
        case import_t_includes:
            break;
        default:
            verify(false, "not handled: %i", im->import_type);
    }
}


array read_expr(silver mod) {
    mod->no_build = true; /// doesnt matter what we call it with, it wont give us back things
    /// this is so we can send our expression parser off to do its thing and we can tell where the expression ends
    /// just so we can process it later in our subproc
    int  e_start = mod->cursor;
    print_tokens("read_expr", mod);
    node is_void = parse_expression(mod);
    int  e_len   = mod->cursor - e_start;
    array body = array(e_len);
    for (int i = 0; i < e_len; i++) {
        token e = element(mod, e_start + i);
        push(body, e);
    }
    mod->no_build = false;
    return body; 
}

/// used for records and functions
array read_body(silver mod) {
    array body = array(32);
    token n    = element(mod,  0);
    token p    = element(mod, -1);
    bool  multiple  = n->line > p->line;

    while (1) {
        token k = peek(mod);
        if (!k) break;
        if (!multiple && k->line   > n->line)   break;
        if ( multiple && k->indent < n->indent) break;
        push(body, k);
        consume(mod);
    }
    return body;
}

//AType tokens_isa(tokens a) {
//    token  t = idx(a->tokens, 0);
//    return isa(t->literal);
//}

num silver_line(silver a) {
    token  t = element(a, 0);
    return t->line;
}


string silver_location(silver a) {
    token  t = element(a, 0);
    return t ? (string)location(t) : (string)format("n/a");
}


bool is_keyword(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string))
        s = any;
    else if (type == typeid(token))
        s = string(((token)any)->chars);
    
    return index_of_cstr(keywords, s->chars) >= 0;
}


bool is_alpha(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string)) {
        s = any;
    } else if (type == typeid(token)) {
        token token = any;
        s = string(token->chars);
    }
    
    if (index_of_cstr(keywords, s->chars) >= 0)
        return false;
    
    if (len(s) > 0) {
        char first = s->chars[0];
        return isalpha(first) || first == '_';
    }
    return false;
}


array parse_tokens(A input) {
    string input_string;
    AType  type = isa(input);
    path    src = null;
    if (type == typeid(path)) {
        src = input;
        input_string = read(src, typeid(string));
    } else if (type == typeid(string))
        input_string = input;
    else
        assert(false, "can only parse from path");
    
    string  special_chars   = string(".$,<>()![]/+*:=#");
    array   tokens          = array(128);
    num     line_num        = 1;
    num     length          = len(input_string);
    num     index           = 0;
    num     line_start      = 0;
    num     indent          = 0;

    /// initial indent verify
    i32 chr0 = idx(input_string, index);
    verify(!isspace(chr0) || chr0 == '\n', "initial statement off indentation");

    while (index < length) {
        i32 chr = idx(input_string, index);
        
        if (isspace(chr)) {
            if (chr == '\n') {
                line_num  += 1;
                line_start = index + 1;
                indent     = 0;
                label: /// so we may use break and continue
                chr = idx(input_string, ++index);
                if       (!isspace(chr))  continue;
                else if  (chr ==  '\n')   continue;
                else if  (chr ==   ' ')   indent += 1;
                else if  (chr ==  '\t')   indent += 4;
                else                      continue;
                goto label;
            } else {
                index += 1;
                continue;
            }
        }
        
        if (chr == '#') {
            if (index + 1 < length && idx(input_string, index + 1) == '#') {
                index += 2;
                while (index < length && !(idx(input_string, index) == '#' && index + 1 < length && idx(input_string, index + 1) == '#')) {
                    if (idx(input_string, index) == '\n')
                        line_num += 1;
                    index += 1;
                }
                index += 2;
            } else {
                while (index < length && idx(input_string, index) != '\n')
                    index += 1;
                line_num += 1;
                index += 1;
            }
            continue;
        }
        
        char sval[2] = { chr, 0 };
        if (index_of(special_chars, sval) >= 0) {
            symbol name = sval;
            bool found = false;
            symbol src = &input_string->chars[index];
            pairs (operators, i) {
                string op = i->key;
                if (strncmp(src, op->chars, op->len) == 0) {
                    name = op->chars;
                    found = true;
                    break;
                }
            }
            if (!found)
                each (keywords, string, k) {
                    if (strncmp(src, k->chars, k->len) == 0) {
                        name = k->chars;
                        break;
                    }
                }
            
            push(tokens, token(chars, name, indent, indent, source, src, line, line_num, column, index - line_start));
            index += strlen(name);
            continue;
        }

        if (chr == '"' || chr == '\'') {
            i32 quote_char = chr;
            num start      = index;
            index         += 1;
            while (index < length && idx(input_string, index) != quote_char) {
                if (idx(input_string, index)     == '\\' && index + 1 < length && 
                    idx(input_string, index + 1) == quote_char)
                    index += 2;
                else
                    index += 1;
            }
            index         += 1;
            string crop    = mid(input_string, start, index - start);
            if (crop->chars[0] == '-') {
                char ch[2] = { crop->chars[0], 0 };
                push(tokens, token(
                    chars,  ch,
                    indent, indent,
                    source, src,
                    line,   line_num,
                    column, start - line_start));
                crop = string(&crop->chars[1]);
                line_start++;
            }
            push(tokens, token(
                chars,  crop->chars,
                indent, indent,
                source, src,
                line,   line_num,
                column, start - line_start));
            continue;
        }

        num start = index;
        while (index < length) {
            i32 v = idx(input_string, index);
            char sval[2] = { v, 0 };
            if (isspace(v) || index_of(special_chars, sval) >= 0)
                break;
            index += 1;
        }
        
        string crop = mid(input_string, start, index - start);
        push(tokens, token(
            chars,  crop->chars,
            indent, indent,
            source, src,
            line,   line_num,
            column, start - line_start));
    }
    return tokens;
}


token silver_element(silver a, num rel) {
    return a->tokens->elements[clamp(a->cursor + rel, 0, a->tokens->len - 1)];
}


token silver_navigate(silver a, int count) {
    if (a->cursor <= 0)
        return null;
    a->cursor += count;
    token res = element(a, 0);
    return res;
}


token silver_prev(silver a) {
    if (a->cursor <= 0)
        return null;
    a->cursor--;
    token res = element(a, 0);
    return res;
}


token silver_next(silver a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    if (strcmp(res->chars, "simple12") == 0) {
        int test = 0;
        test++;
    }
    a->cursor++;
    return res;
}


token silver_consume(silver a) {
    return silver_next(a);
}


token silver_peek(silver a) {
    if (a->cursor == len(a->tokens))
        return null;
    return element(a, 0);
}


bool silver_next_is(silver a, symbol cs) {
    token n = element(a, 0);
    return n && strcmp(n->chars, cs) == 0;
}


bool silver_read(silver a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        a->cursor++;
        return true;
    }
    return false;
}


object silver_read_literal(silver a) {
    token  n = element(a, 0);
    if (n->literal) {
        a->cursor++;
        return n->literal;
    }
    return null;
}


string silver_read_string(silver a) {
    token  n = element(a, 0);
    if (isa(n->literal) == typeid(string)) {
        string token_s = string(n->chars);
        string result  = mid(token_s, 1, token_s->len - 2);
        a->cursor ++;
        return result;
    }
    return null;
}


object silver_read_numeric(silver a) {
    token n = element(a, 0);
    if (isa(n->literal) == typeid(f64) || isa(n->literal) == typeid(i64)) {
        a->cursor++;
        return n->literal;
    }
    return null;
}


string silver_read_assign(silver a) {
    token  n = element(a, 0);
    string k = string(n->chars);
    num assign_index = index_of(assign, k);
    bool found = assign_index >= 0;
    if (found) a->cursor ++;
    return found ? k : null;
}


string silver_read_alpha(silver a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        a->cursor ++;
        return string(n->chars);
    }
    return null;
}


string silver_read_keyword(silver a) {
    token n = element(a, 0);
    if (is_keyword(n)) {
        a->cursor ++;
        return string(n->chars);
    }
    return null;
}


object silver_read_bool(silver a) {
    token  n       = element(a, 0);
    bool   is_true = strcmp(n->chars, "true")  == 0;
    bool   is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool) a->cursor ++;
    return is_bool ? A_bool(is_true) : null;
}


typedef struct tokens_data {
    array tokens;
    num   cursor;
} *tokens_data;


void silver_push_state(silver a, array tokens, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}


void silver_pop_state(silver a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data state = (tokens_data)last(a->stack); // we should call this element or ele
    pop(a->stack);
    if(!transfer)
        a->cursor = state->cursor;
    else if (transfer && state->tokens != a->tokens) {
        /// transfer implies we up as many tokens
        /// as we traveled in what must be a subset
        a->cursor += state->cursor;
    }

    if (state->tokens != a->tokens) {
        drop(a->tokens);
        a->tokens = state->tokens;
    }
}

/// silver must be hooked into this process 
/// we store silver code on member default values
void silver_build_initializer(silver mod, member mem) {
    if (mem->initializer) {
        push_state(mod, mem->initializer, 0);
        node expr = parse_expression(mod);
        assign(mod, mem, expr, OPType__assign);
        pop_state(mod, false);
    } else {
        /// ether can memset to zero
    }
}

void silver_push_current(silver a) {
    push_state(a, a->tokens, a->cursor);
}


node parse_return(silver mod) {
    model rtype = return_type(mod);
    bool  is_v  = is_void(rtype);
    model ctx = context_model(mod, typeid(function));
    consume(mod);
    node expr   = is_v ? null : parse_expression(mod);
    log("return-type", "%o", is_v ? (object)string("void") : 
                                    (object)expr->mdl);
    return fn_return(mod, expr);
}


node parse_break(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node parse_for(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node parse_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node silver_parse_if_else(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node silver_parse_do_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


static node reverse_descent(silver mod) {
    node L = parse_primary(mod);
    mod->expr_level++;
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        bool  m = true;
        while(m) {
            m = false;
            for (int j = 0; j < 2; j++) {
                string token  = level->token [j];
                if (!read(mod, cstring(token)))
                    continue;
                OPType op     = level->ops   [j];
                string method = level->method[j];
                node R = parse_primary(mod);
                     L = ether_op     (mod, op, method, L, R);
                m      = true;
                break;
            }
        }
    }
    mod->expr_level--;
    return L;
}


static node parse_expression(silver mod) {
    //mod->expr_level++;
    node vr = reverse_descent(mod);
    //mod->expr_level--;
    return vr;
}


model read_model(silver mod) {
    push_current(mod);
    bool wrap = false;
    model mdl = null;

    /// wrapped types can be array and map
    if (read(mod, "[")) {
        wrap = true;
    }
    
    /// recursion handling
    if (wrap && next_is(mod, "[")) {
        mdl = read_model(mod);
    } else {
        string name = read_alpha(mod); // Read the type to cast to
        if (!name) {
            pop_state(mod, false);
            return null;
        }
        mdl = emodel(name->chars);
        verify(mdl, "model not found: %o", name);
    }

    verify(mdl, "failed to read model");
    pop_state(mod, true);
    return mdl;
}


member cast_method(silver mod, class class_target, model cast) {
    record rec = class_target;
    verify(isa(rec) == typeid(class), "cast target expected class");
    pairs(rec->members, i) {
        member mem = i->value;
        model fmdl = mem->mdl;
        if (isa(fmdl) != typeid(function)) continue;
        function fn = fmdl;
        if (fn->is_cast && model_cmp(fn->rtype, cast) == 0)
            return mem;
    }
    return null;
}

static function parse_fn           (silver mod, AFlag member_type, object name);
static node     parse_function_call(silver mod, member fmem);

/// parse construct from and cast to, and named args
static node parse_create(silver mod, model src) {
    object n = read_literal(mod);
    if (n) return operand(mod, n);
    bool has_content = read(mod, "[") && !read(mod, "]");
    node r = null;
    bool conv = false;

    if (has_content) {
        token k = peek(mod);
        if (is_alpha(k) && eq(element(mod, 1), ":")) {
            /// init with members [alloc performs required check]
            /// this converts operand to a map
            map args  = map(hsize, 16);
            int count = 0;
            while (!next_is(mod, "]")) {
                string name  = read_alpha(mod);
                verify(read(mod, ":"), "expected : after arg %o", name);
                node   value = parse_expression(mod);
                set(args, name, value);
                count++;
                if (next_is(mod, "]")) break;
            }
            verify(len(args) == count, "arg count mismatch");
            verify(read(mod, "]"), "expected ] after construction");
            conv = true;
            r    = args;
        } else {
            r    = parse_expression(mod);
            conv = r->mdl != src;
        }
        verify(read(mod, "]"), "expected ] after mdl-expr %o", src->name);
    } else {
        r = create(mod, src, null); // default
        conv = false;
    }
    if (conv)
        r = create(mod, src, r);
    return r;
}

/// returns a node or member and must be handled for those cases
/// module-name.public-member
/// class-instance.member-of-class.another-member

static node collection_of_things() {
    node expr;
    silver mod;
    model  mdl;
    if (is_record(expr)) {
        member expr_method = cast_method(mod, expr->mdl, mdl);
        if (expr_method) {
            // object may cast because there is a method defined with is_cast and an rtype of the cast_ident
            array args = array();
            node mcall = fn_call(mod, expr_method, args);
            return mcall;
        }
    }
    return null;
}


static node parse_construct(silver mod, member mem) {
    verify(mem && mem->is_type, "expected member type");
    verify(read(mod, "["), "expected [ after type name for construction");
    node res = null;
    /// it may be a dedicated constructor (for primitives or our own), or named args (not for primitives)
    if (instanceof(mem->mdl->src, record)) {
        AType atype = isa(mem->mdl->src);
        map args = map(hsize, 16);
        int count = 0;
        while (!next_is(mod, "]")) {
            string name  = read_alpha(mod);
            verify(read(mod, ":"), "expected : after arg %o", name);
            node   value = parse_expression(mod);
            set(args, name, value);
            count++;
            verify(read(mod, ",") || next_is(mod, "]"), "expected , or ]");
        }
        verify(len(args) == count, "arg count mismatch");
        verify(read(mod, "]"), "expected ] after construction");

        /// now we need to parse the named arguments, and perform assignment on each
        /// then we separately parse 'default values' (not now, but thats in member as a body attribute)
        res = create(mod, mem->mdl, args);
    } else {
        /// enum [ value ] <- same as below:
        /// primitive [ value ] <- not sure if we want this, as we have cast
    }
    return res;
}

static node read_resolve(silver mod) {
    object n   = read_literal(mod);
    if (n) return operand(mod, n);
    print_tokens("read-cast", mod);
    model  mdl = read_model(mod); /// null if no type specified
    token  t   = peek(mod);
    n = read_literal(mod);
    bool  has_expr = n || (mdl && eq(t, "["));

    /// type, type literal, or type[expr] handled here [cast & ctr support]
    if (mdl && has_expr)
        return create(mod, mdl, n ? (object)n : (object)parse_create(mod, mdl));

    string alpha = read_alpha(mod);
    if   (!alpha) return null;

    model  ctx      = mod->top;
    member target   = null; // member is a node with value-ref (value)
    member mem      = lookup(mod, alpha, null);
    record    rec    = instanceof(mod->top, record);
    function  fn     = instanceof(mod->top, function);
    silver    module = (fn && fn->imdl == mod) ? mod : null;
    bool parse_index = false;

    if (!mem) {
        verify(mod->expr_level == 0, "member not found: %o", alpha);
        mem = new(member, mod, mod, name, alpha, is_module, module); /// this calls LLVMBuildAlloca
        push_member(mod, mem); /// if member does not have mdl defined, then we can know its a new member
    } else if (!mem->is_type && !mem->is_func) {
        import im = instanceof(mem->mdl, import);
        if (im) {
            /// import is not a type that silver may use but a namespace into a module of types
            verify(read(mod, "."), "expected .member after module reference");
            string module_m_name = read_alpha(mod);
            /// C imports will go right into import model, where as silver imports are delegated inside
            model ns = im->extern_mod ? (model)im->extern_mod : (model)im;
            mem = get(ns->members, module_m_name);
            verify(mem, "%o not a member in module %o", module_m_name, ns->name); 
        } else {
            /// if member already registered, we may assume this is an instancing of an anonymous
            /// in the case of import, and member with the same name as type
            /// construct can allow simple pass-through to next member if model scope if its valid
            /// that is if the member behind's mdl and the models src (generic A-type) match
            verify(!mem->is_func && !mem->is_type, "member-defined %o", mem->name);
        }

        /// from record if no value
        if (!has_value(mem)) {
            AType ctx_type = isa(ctx);
            member target = lookup(mod, string("this"), null); // unique to the function in class, not the class
            verify(target, "no target found in context");
            mem = resolve(target, alpha);
            verify(mem, "failed to resolve member in context: %o", mod->top->name);
        }
        /// -> pointer keyword is safe-guard here, so we are friendlier to pointers
        bool safe = false;
        while ((safe = read(mod, "->")) || read(mod, ".")) {
            target = mem;
            push(mod, mem->mdl);
            string alpha = read_alpha(mod);
            mem = resolve(mem, alpha); /// needs an argument
            verify(alpha, "expected alpha identifier");
            pop(mod);
        }
        parse_index = !mem->is_func;
    }
    /// handle compatible indexing methods and general pointer dereference @ index
    if (parse_index && next_is(mod, "[")) {
        bool prev_l = mod->left_hand;
        mod->left_hand = false;
        record r = instanceof(mem->mdl, record);
        /// must have an indexing method, or be a reference_pointer
        verify(mem->mdl->ref == reference_pointer || r, "no indexing available for model %o/%o",
            mem->mdl->name, estr(reference, mem->mdl->ref));
        
        /// we must read the arguments given to the indexer
        consume(mod);
        array args = array(16);
        while (!next_is(mod, "]")) {
            node expr = parse_expression(mod);
            push(args, expr);
            verify(next_is(mod, "]") || next_is(mod, ","), "expected ] or , in index arguments");
            if (next_is(mod, ","))
                consume(mod);
        }
        consume(mod);
        node index_expr = null;
        if (r) {
            /// this returns a 'node' on this code path
            member indexer = compatible(mod, r, null, A_TYPE_INDEX, args); /// we need to update member model to make all function members exist in an array
            /// todo: serialize arg type names too
            verify(indexer, "%o: no suitable indexing method", r->name);
            function fn = instanceof(indexer->mdl, function);
            indexer->target_member = mem; /// todo: may not be so hot to change the schema
            index_expr = fn_call(mod, indexer, args); // needs a target
        } else {
            /// this returns a 'member' on this code path
            verify(len(args) == 1, "expected singular index value of integral type");
            index_expr = offset(mod, mem, args->elements[0]);
            /// we can handle multiple dimensions with model shape
            /// could potentially support abstract for row/col major stride
            /// an interesting idea; we need this a basic thing in our language
        }
        mod->left_hand = prev_l;
        return index_expr;
    }
    if (mem) {
        member target = null;
        /// lets 'resolve' all members
        if (mem->is_func) {
            if (mod->in_ref) {
                fault("not implemented");
                return null; /// simple stack mechanism we need to support returnin the pointer for (is this a 'load?')
            } else
                return parse_function_call(mod, mem);
        } else if (mem->is_type && next_is(mod, "[")) {
            print_tokens("parse-construct", mod);
            return parse_construct(mod, mem); /// this, is the construct
        } else if (mod->in_ref) {
            return mem; /// we will not load when ref is being requested on a member
        } else {
            return load(mod, mem); // todo: perhaps wait to AddFunction until they are used; keep the member around but do not add them until they are referenced (unless we are Exporting the import)
        }
    }
    return mem;
}


node parse_statements(silver mod);
node parse_statement(silver mod);

/// parses member args for a definition of a function
arguments parse_args(silver mod) {
    verify(read(mod, "["), "parse-args: expected [");
    array       args = new(array,     alloc, 32);
    arguments   res  = new(arguments, mod, mod, args, args);

    //print_tokens("args", mod);
    if (!next_is(mod, "]")) {
        push(mod, res); // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        int statements = 0;
        for (;;) {
            /// we expect an arg-like statement when we are in this context (arguments)
            print_tokens("parse-args", mod);

            if (next_is(mod, "...")) {
                consume(mod);
                res->is_ext = true;
                continue;
            }

            parse_statement(mod);
            statements++;

            print_tokens("after-parse-args", mod);
            if (next_is(mod, "]"))
                break;
        }
        consume(mod);
        pop(mod);
        verify(statements == len(res->members), "argument parser mismatch");
        pairs(res->members, i) {
            member arg = i->value;
            push(args, arg); // todo: make sure there is no value set on these, because we have no default at the moment (we could, of course with this)
        }
    }
    return res;
}

static node parse_function_call(silver mod, member fmem) {
    bool     expect_br = false;
    function fn        = fmem->mdl;
    bool     has_expr  = len(fn->args) == 0;
    verify(isa(fn) == typeid(function), "expected function type");
    int  model_arg_count = len(fn->args);
    
    if (has_expr && next_is(mod, "[")) {
        consume(mod);
        expect_br = true;
    } else
        verify(!has_expr, "expected [ for nested call");

    int    arg_index = 0;
    array  values    = new(array, alloc, 32);
    member last_arg  = null;
    while(arg_index < model_arg_count || fn->va_args) {
        member arg      = arg_index < len(fn->args) ? get(fn->args, arg_index) : null;
        node   expr     = parse_expression(mod);
        model  arg_mdl  = arg ? arg->mdl : null;
        if (arg_mdl && expr->mdl != arg_mdl)
            expr = convert(mod, expr, arg_mdl);
        //print("argument %i: %o", arg_index, expr->mdl->name);
        push(values, expr);
        arg_index++;
        if (next_is(mod, ",")) {
            consume(mod);
            continue;
        } else if (next_is(mod, "]")) {
            verify (arg_index >= model_arg_count, "expected %i args", model_arg_count);
            break;
        } else if (arg_index >= model_arg_count)
            break;
    }
    if (expect_br) {
        verify(next_is(mod, "]"), "expected ] end of function call");
        consume(mod);
    }
    return fn_call(mod, fmem, values);
}


static node parse_ternary(silver mod, node expr) {
    if (!read(mod, "?")) return expr;
    node expr_true  = parse_expression(mod);
    node expr_false = parse_expression(mod);
    return ether_ternary(mod, expr, expr_true, expr_false);
}


static node parse_primary(silver mod) {
    print_tokens("parse_primary", mod);

    // parenthesized expressions
    if (read(mod, "[")) {
        node expr = parse_expression(mod); // Parse the expression
        verify(read(mod, "]"), "Expected closing parenthesis");
        return parse_ternary(mod, expr);
    }

    // handle the logical NOT operator (e.g., '!')
    if (read(mod, "!") || read(mod, "not")) {
        node expr = parse_expression(mod); // Parse the following expression
        return not(mod, expr);
    }

    // bitwise NOT operator
    if (read(mod, "~")) {
        node expr = parse_expression(mod);
        return bitwise_not(mod, expr);
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    if (read(mod, "typeof")) {
        bool bracket = false;
        if (next_is(mod, "[")) {
            assert(next_is(mod, "["), "Expected '[' after 'typeof'");
            consume(mod); // Consume '['
            bracket = true;
        }
        node expr = parse_expression(mod); // Parse the type expression
        if (bracket) {
            assert(next_is(mod, "]"), "Expected ']' after type expression");
            consume(mod); // Consume ']'
        }
        return expr; // Return the type reference
    }

    // 'ref' operator (reference)
    if (read(mod, "ref")) {
        mod->in_ref = true;
        node expr = parse_expression(mod);
        mod->in_ref = false;
        return addr_of(mod, expr, null);
    }

    node n = read_resolve(mod); /// should perform offset() inside
    if  (n) return n;
    
    token  tk    = consume(mod);
    string ident = cast(string, tk);
    fault("unexpected %o in primary expression", ident);
    return null;
}


node parse_assignment(silver mod, member mem, string oper) {
    verify(!mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    mod->in_assign = mem;
    node   L       = mem;
    print_tokens("parse_assignment", mod);
    node   R       = parse_expression(mod); /// getting class2 as a struct not a pointer as it should be. we cant lose that pointer info
    if (!mem->mdl) {
        mem->is_const = eq(oper, "=");
        set_model(mem, R->mdl);
    }
    verify(contains(operators, oper), "%o not an assignment-operator");
    string op_name = get(operators, oper);
    string op_form = format("_%o", op_name);
    OPType op_val  = eval(OPType, op_form->chars);
    node   result  = op(mod, op_val, op_name, L, R);
    mod->in_assign = null;
    return result;
}


node cond_builder_ternary(silver mod, array cond_tokens, object unused) {
    push_state(mod, cond_tokens, 0);
    node cond_expr = parse_expression(mod);
    pop_state(mod, false);
    return cond_expr;
}


node expr_builder_ternary(silver mod, array expr_tokens, object unused) {
    push_state(mod, expr_tokens, 0);
    node exprs = parse_expression(mod);
    pop_state(mod, false);
    return exprs;
}


node cond_builder(silver mod, array cond_tokens, object unused) {
    push_state(mod, cond_tokens, 0);
    node cond_expr = parse_expression(mod);
    pop_state(mod, false);
    return cond_expr;
}


node expr_builder(silver mod, array expr_tokens, object unused) {
    push_state(mod, expr_tokens, 0);
    node exprs = parse_statements(mod);
    pop_state(mod, false);
    return exprs;
}

/// parses entire chain of if, [else-if, ...] [else]
node parse_if_else(silver mod) {
    bool  require_if   = true;
    array tokens_cond  = array(32);
    array tokens_block = array(32);
    while (true) {
        bool is_if  = read(mod, "if");
        verify(is_if && require_if || !require_if, "expected if");
        array cond  = is_if ? read_body(mod) : null;
        array block = read_body(mod);
        push(tokens_cond,  cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = read(mod, "else");
        require_if = false;
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    return ether_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}


node parse_do_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}

bool is_access(silver mod) {
    interface access = interface_public;
    token k = peek(mod);
    for (int m = 1; m < interface_type.member_count; m++) {
        type_member_t* enum_v = &interface_type.members[m];
        if (eq(k, enum_v->name)) {
            return true;
        }
    }
    return false;
}

bool is_model(silver mod) {
    token  k = peek(mod);
    member m = lookup(mod, k, null);
    return m && m->is_type;
}

interface read_access(silver mod) {
    interface access = interface_public;
    for (int m = 1; m < interface_type.member_count; m++) {
        type_member_t* enum_v = &interface_type.members[m];
        if (read(mod, enum_v->name)) {
            access = m;
            break;
        }
    }
    return access;
}

/// this is a parser
none schematic_with_silver(schematic e, silver mod) {
    token  k = peek(mod);
    if (is_access(k)) {
        e->access = read_access(mod);
        e->mdl    = read_model (mod);
    } else if (is_model(k)) {
        e->access = interface_public;
        e->mdl    = read_model (mod);
    } else {
        string n = read_alpha(mod);
        verify(n, "expected alpha-ident, found: %o", next(mod));
        verify(read(mod, ":"), "schematic: expected : after alpha-ident");
        e->access = read_access(mod);
        e->mdl    = read_model (mod);
    }
}

void build_record(silver mod, object arg, record rec) {
    bool   is_class = instanceof(rec, class) != null;
    symbol sname    = is_class ? "class" : "struct";
    array  body     = instanceof(rec->user, array);

    log       ("build_record", "%s %o", sname, rec->name);
    push_state(mod, body, 0);
    verify    (read(mod, "["), "expected [");
    push      (mod, rec);
    while (peek(mod)) {
        if (next_is(mod, "]")) break;
        node n = parse_statement(mod);
    }
    pop       (mod);
    verify    (read(mod, "]"), "expected ]");
    pop_state (mod, false);
}

/// called after : or before, where the user has access
member parse_model(silver mod, string n, member existing) {
    bool   is_import = next_is(mod, "import");
    bool   is_class  = next_is(mod, "class");
    bool   is_struct = next_is(mod, "struct");
    bool   is_enum   = next_is(mod, "enum");
    bool   is_alias  = next_is(mod, "alias");
    if (!is_import && !is_class && !is_struct && !is_enum && !is_alias)
        return null;

    consume(mod);
    if (!n) n = read_alpha(mod);
    verify(is_alpha(n), "expected alpha-ident, found %o", n);
    member mem = existing ? existing : member(mod, mod, name, n);

    if (is_import) {
        import im = import(
            mod, mod, skip_process, mod->prebuild);
        push(mod->imports, im);
        mem->mdl = im;
        im->name = mem->name;
    } else if (is_class || is_struct) {
        array schema = array();
        token parent_name = null;
        if (next_is(mod, "[")) {
            consume(mod);
            while (!next_is(mod, "]"))
                push(schema, schematic(mod));
            consume(mod);
        }
        verify (next_is(mod, "[") , "expected function body");
        array        body    = read_body(mod);
        subprocedure process = subproc  (mod, build_record, null);
        if (is_class)
            mem->mdl = class    (mod, mod, name, n, process, process, parent_name, parent_name);
        else
            mem->mdl = structure(mod, mod, name, n, process, process);
        process->ctx = mem->mdl;
        ((record)mem->mdl)->user = body;
    } else if (is_enum) {
        /// read optional storage model (default: i32)
        model store = null; 
        if (next_is(mod, "[")) {
            consume(mod);
            store = read_model(mod);
            verify(read(mod, "]"), "expected ] for enum-model");
        }
        mem->mdl = store ? store : emodel("i32");

        /// verify model is a primitive type
        AType atype = isa(mem->mdl->src);
        verify(atype && (atype->traits & A_TRAIT_PRIMITIVE),
            "enumerables can only be based on primitive types (i32 default)");

        /// read body, create enum and parse tokens with context
        array enum_body = read_body(mod);
        push_state(mod, enum_body, 0);

        i64   value   = 0;
        mem->mdl      = enumerable(
            mod, mod, size, mem->mdl->size, name, n, src, mem->mdl);
        
        push(mod, mem->mdl);
        while (true) {
            token e = next(mod);
            if  (!e) break;
            object v = null;
            if (read(mod, ":")) {
                object i64_obj = read_literal(mod);
                verify(isa(i64_obj) == typeid(i64), "expected numeric literal");
                v = A_primitive(atype, i64_obj);
            }
            string estr = cast  (string, e);
            member emem = member(
                mod, mod, name, estr, mdl, mem->mdl,
                is_const, true, is_decl, true);
            if (!v) v = A_primitive(atype, &value); /// this creates i8 to i64 data, using &value i64 addr
            set_value(emem, v);
            set      (mem->mdl->members, estr, emem);
            value = *((i64*)v) + 1;
        }
        pop(mod);
        pop_state(mod, false);

        print_tokens("overview", mod);
    } else {
        verify(is_alias, "unknown error");
        mem->mdl = alias(mem->mdl, mem->name, reference_pointer, null);
    }
    mem->is_type = true;
    verify(mem && (is_import || len(mem->name)),
        "name required for model: %s", isa(mem->mdl)->name);
    return mem;
}

/// such a big change to this one, tectonic drift'ing ...
node parse_statement(silver mod) {
    print_tokens("parse-statement", mod);
    token     t      = peek(mod);

    /// contextual parameters that determine how statements are processed
    record    rec         = instanceof(mod->top, record);
    function  fn          = instanceof(mod->top, function);
    arguments args        = instanceof(mod->top, arguments);
    silver    module      = 
        (mod->prebuild || (fn && fn->imdl == mod)) ? mod : null;
    object    lit         = read_literal(mod);                 verify(!lit, "unexpected literal: %o", lit);
    node      initializer = null;

    /// handle root statement expressions first
    /// yes a 'module' could be its own main with args
    /// however import would need succinct arg parsing
    /// having a singular class or struct you give to main is intuitive
    /// its an argument because functions take args; modules really dont
    if (!module) {
        if (next_is(mod, "no-op"))  return node(mdl, emodel("void"));
        if (next_is(mod, "return")) return parse_return(mod);
        if (next_is(mod, "break"))  return parse_break(mod);
        if (next_is(mod, "for"))    return parse_for(mod);
        if (next_is(mod, "while"))  return parse_while(mod);
        if (next_is(mod, "if"))     return parse_if_else(mod);
        if (next_is(mod, "do"))     return parse_do_while(mod);
    }

    mod->left_hand  = true;
    string     n      = read_alpha   (mod);
    member     mem    = null;
    string     assign = null;
    if (n)     assign = read_assign  (mod);

    
    if(module) mem    = parse_model  (mod, n, null);
    if(!mem) {
        if (!args)
            print_tokens("before-read_resolve", mod);
        mem = args ? member(mod, mod, mdl, null, name, n,
                            access, read_access(mod)) :
             (member)read_resolve(mod);
    }
    mod->left_hand = false;
    
    if (!module && !args && mem->is_func)
        return parse_function_call(mod, mem);

    print_tokens("parse-statement2", mod);
    if (!mem->is_type) {
        bool   is_require = rec && read(mod, "require");
        bool   is_static  = rec && read(mod, "static"); // could support functions too if we want similar to C.  sometimes you want it to debug
        bool   is_ref     = read(mod, "ref");
        bool   is_ctr     = mem->mdl == mod->top;
        object name       = n;
        
        if (is_ctr) {
            /// transform mem->mdl [currently top model] to construct fn
            verify (!is_static, "unexpected static for construct");
            if (next_is(mod, "[") || next_is(mod, "return")) {
                name = mem->mdl->name;
                mem->mdl  = parse_fn(mod, A_TYPE_CONSTRUCT, null);
            } else
                fault("expected function body for constructor");
        } else {
            string keyword   = read_keyword(mod);
            bool   is_cast   = keyword && eq(keyword, "cast");
            if (is_cast) {
                /// transform to cast
                verify (rec, "cast functions must be defined in records");
                verify (!is_static, "unexpected static for cast");
                print_tokens("cast", mod);
                verify (!is_static, "unexpected static keyword does not apply to cast");
                verify (!is_ref,    "unexpected ref keyword does not apply to cast");
                mem->mdl = parse_fn(mod, A_TYPE_CAST, name);
            } else if (next_is(mod, "cast")) {
                mem->mdl = parse_fn(mod, is_static ? A_TYPE_SMETHOD : A_TYPE_IMETHOD, name);
                print_tokens("after-parse", mod);
                mem->is_func = true;
            }
        }
    }

    if (mem && mem->mdl) {
        verify(!assign || mem->is_type || mem->is_func, "model mismatch");
    } else if (!args && (assign && !mem->is_type)) {
        verify (assign, "expected assignment on identifier %o, found %o", mem->name, next(mod));
        return parse_assignment(mod, mem, assign);
    }

    if (args && !mem->mdl) {
        mem->mdl = read_model(mod);
        verify(mem->mdl, "cannot read model for arg: %o", mem->name);
    }
    push_member(mod, mem);
    return mem;
}


node parse_statements(silver mod) {
    push(mod, new(statements, mod, mod));
    
    print_tokens("parse_statements", mod);

    token n         = element(mod,  0);
    token p         = element(mod, -1);
    bool  multiple  = n->line > p->line; /// should store indent level at each token, because there is no line model and would not like one!
    int   depth     = 1;
    node  vr        = null;
    ///
    while(peek(mod)) {
        token nn    = element(mod,  0);
        token pp    = element(mod, -1);
        bool  mm    = nn->line > pp->line; // overlaps with first, but subsequently protects run-away statements spawning off singular on-line expr
        if(multiple && mm) {
            depth += 1;
            push(mod, statements(mod, mod));
        }
        vr = parse_statement(mod);
        if(!multiple) break;
        token nnn   = element(mod,  0);
        token ppp   = element(mod, -1);
        bool  id_less = nnn->indent < ppp->indent; /// hold onto your butts
        if(id_less) {
            if (depth > 1)
                pop(mod);
            next(mod);
            if ((depth -= 1) == 0) break;
        }
    }
    pop(mod);
    return vr;
}


void build_function(silver mod, object arg, function fn) {
    array body = instanceof(fn->user, array);
    push_state(mod, body, 0);
    parse_statements(mod);
    pop_state(mod, false);
}


function parse_fn(silver mod, AFlag member_type, object ident) {
    model rtype = null;
    token name = instanceof(ident, token);
    verify(next_is(mod, "fn") || next_is(mod, "cast"), "expected fn/cast");
    consume(mod);

    if (!name) {
        verify(member_type == A_TYPE_CAST, "expected member type of cast if there is no name");
        print("casting to %o", rtype->name);
        name = form(token, "cast_%o", rtype->name);
    } else
        verify(member_type != A_TYPE_CAST, "unexpected name for cast");

    arguments args = null;
    if (member_type == A_TYPE_CAST) {
        verify (next_is(mod, "[") || next_is(mod, "return"), "expected body for cast");
        args = arguments();
    } else {
        
        verify (next_is(mod, "[") || next_is(mod, ">"), "expected function args [");
        
        print_tokens("parse_fn-args", mod);
        bool r0 = read(mod, ">");
        if (!r0) 
            args = parse_args(mod);
        if ( r0 || read(mod, ">"))
            rtype = read_model(mod);
        else
            rtype = emodel("generic");
    }
    
    verify(rtype, "rtype not set, void is something we may lookup");
    subprocedure process = subproc   (mod, build_function, null);
    record       rec_top = instanceof(mod->top, record) ? mod->top : null;
    function     fn      = function(
        mod,    mod,     name,   name, function_type, member_type,
        record, rec_top, rtype,  rtype,
        args,   args,    process, process,
        user,   read_body(mod)); /// user must process their payload for a function to effectively resolve
    process->ctx = fn; /// safe to set after, as we'll never start building within function; we need this context in builder
    return fn;
}


void silver_incremental_resolve(silver mod) {
    bool in_top = mod->in_top;
    mod->in_top = false;
    
    /// finalize included structs
    pairs(mod->members, i) {
        member mem = i->value;
        model base = mem->mdl->ref ? mem->mdl->src : mem->mdl;
        if (instanceof(base, record) && base->from_include)
            process_finalize(base);
    }

    /// finalize imported C functions, which use those structs perhaps literally in argument form
    pairs(mod->members, i) {
        member mem = i->value;
        model  mdl = mem->mdl;
        if (instanceof(mem->mdl, function) && mdl->from_include) {
            process_finalize(mem->mdl);
        }
    }

    /// process/finalize all remaining member models 
    /// calls process sub-procedure and poly-based finalize
    pairs(mod->members, i) {
        member mem = i->value;
        model base = mem->mdl->ref ? mem->mdl->src : mem->mdl;
        if (instanceof(base, record) && !base->from_include)
            process_finalize(base);
    }

    /// finally, process functions (last step in parsing)
    pairs(mod->members, i) {
        member mem = i->value;
        if (instanceof(mem->mdl, function))
            process_finalize(mem->mdl);
    }
    mod->in_top = in_top;
}


void silver_parse(silver mod) {
    /// im a module!
    member fn = initializer(mod);
    push(mod, fn->mdl);
    mod->in_top = true;
    map members = mod->members;

    while (peek(mod)) {
        print_tokens("module-member", mod);
        parse_statement(mod);
        incremental_resolve(mod);
    }
    mod->in_top = false;
    
    /// return from the initializer
    fn_return(mod, null);
    pop(mod);
}


build_state build_project(import im, string name, string url);


bool silver_compile(silver mod) {
    verify(exec("%o/bin/llc -filetype=obj %o.ll -o %o.o -relocation-model=pic",
        mod->install, mod->name, mod->name) == 0,
            ".ll -> .o compilation failure");

    string libs = string(alloc, 128);
    each (mod->imports, import, im) {
        if (im->links)
            each_ (im->links, string, link)
                append(libs, format("-l%o ", link)->chars);
    }
    each (mod->imports, import, im) {
        if (im->products)
            each_ (im->products, string, product)
                append(libs, format("-l%o ", product)->chars);
    }
    string cmd = format("%o/bin/clang %o.o -o %o -L %o/lib %o",
        mod->install, mod->name, mod->name, mod->install, libs);
    print("cmd: %o", cmd);
    verify(system(cmd->chars) == 0, "compilation failed"); /// add in all import objects/libraries
    print("compiled: %o", mod->name);
    return true;
}


void silver_init(silver mod) {
    verify(exists(mod->source), "source (%o) does not exist", mod->source);
    mod->mod = mod;
    mod->products_used = array();
    mod->imports = array(32);
    mod->tokens  = parse_tokens(mod->source);
    mod->stack   = array(4);

    /// build a proper stack of tokens we may parse through normal means
    /// lets just set a state variable prebuild so that import->skip
    array  import_A = array_of(
        typeid(token),
        token(chars, "import"),
        token(chars, "A"),
        null);
    push_state(mod, import_A, 0);
    mod->prebuild = true;
    member m_Atype = parse_statement(mod);
    mod->prebuild = false;

    //import Atype = import(mod, mod, name, string("A"), skip_process, true);
    import Atype = instanceof(m_Atype->mdl, import);
    pop_state  (mod, false);
    push       (mod->imports, Atype);
    build_project(Atype, string("A"), string("https://github.com/ar-visions/A"));
    include    (mod, string("A"));

    model mdl = emodel("A");
    AType mdl_type = isa(mdl); /// issue is A becomes a member

    /// alias A -> generic
    member mA      = lookup(mod, string("A"), typeid(import));
    AType  mA_type = isa(mA->mdl);
    string n       = string("generic");
    model  mdl_A   = alias(mA->mdl, n, 0, null);
    member generic = member(mod, mod, name, n, mdl, mdl_A, is_type, true);
    push_member(mod, generic);

    /// we do not want to push this function to context unless it can be gracefully
    parse(mod);

    /// enumerating objects used, libs used, etc
    /// may be called earlier when the statement of import is finished
    each (mod->imports, import, im)
        process(im);

    // its odd that we 'finalize' our function before we code it, right?
    // functions should be finalized with validation of code in mind, 
    // especially when we require it from LLVM

    //model_process_finalize(mod->fn_init);

    build(mod);
    write(mod);   /// write the intermediate LL
    compile(mod); /// compile the LL
}


int main(int argc, char **argv) {
    A_start();

    string s = string("hi");
    cstr        src = getenv("SRC");
    cstr     import = getenv("SILVER_IMPORT");
    map        args = A_args(argc, argv,
        "module",  string(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."), null);
    string mkey     = string("module");
    string name     = get(args, string("module"));
    path   n        = path(chars, name->chars);
    path   source   = absolute(n);
    silver mod      = silver(
        source,  source,
        install, get(args, string("install")),
        name,    stem(source));
    return 0;
}

define_mod   (silver, ether)
define_enum  (import_t)
define_enum  (build_state)
define_class (schematic)
define_mod   (import, model)
module_init  (init)