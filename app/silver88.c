#include <import>

#define   emodel(MDL)    ({ \
    member  m = ether_lookup(mod, str(MDL)); \
    model mdl = m ? m->mdl : null; \
    mdl; \
})

static map   operators;
static array keywords;
static array consumables;
static array assign;
static array compare;

static bool is_alpha(A any);
static node parse_expression(silver mod);
static node parse_primary(silver mod);

static void print_tokens(symbol label, silver mod) {
    print("[%s] tokens: %o %o %o %o %o ...", label,
        element(mod->tokens, 0), element(mod->tokens, 1),
        element(mod->tokens, 2), element(mod->tokens, 3),
        element(mod->tokens, 4), element(mod->tokens, 5));
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
        "class",  "proto",    "struct", "import", "typeof", "schema", "is", "inherits",
        "init",   "destruct", "ref",    "const",  "volatile",
        "return", "<-", "asm",      "if",     "switch",
        "while",  "for",      "do",     "signed", "unsigned", "cast", null);
    consumables = array_of_cstr(
        "ref", "schema", "enum", "class", "union", "proto", "struct",
        "const", "volatile", "signed", "unsigned", null);
    assign = array_of_cstr(
        ":", "=", "+=", "-=", "*=", "/=", 
        "|=", "&=", "^=", ">>=", "<<=", "%=", null);
    compare = array_of_cstr("==", "!=", null);

    string add = str("add");
    operators = map_of( /// ether quite needs some operator bindings, and resultantly ONE interface to use them
        "+",        str("add"),
        "-",        str("sub"),
        "*",        str("mul"),
        "/",        str("div"),
        "||",       str("or"),
        "&&",       str("and"),
        "^",        str("xor"),
        ">>",       str("right"),
        "<<",       str("left"),
        "??",       str("value_default"),
        "?:",       str("cond_value"),
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
        "inherits", str("inherits"), null);
    
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 2; j++) {
            OPType op        = level->ops[j];
            string e_name    = estr(OPType, op);
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
    if (use_text || next_is(mod->tokens, "[")) {
        if (!use_text)
            consume(mod->tokens);
        string word = new(string, alloc, 64);
        int neighbor_x = -1;
        int neighbor_y = -1;
        while (true) {
            token arg = next(mod->tokens);
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
                    word = new(string, chars, arg->chars);
                }
                neighbor_x = arg->column + strlen(arg->chars); 
                neighbor_y = arg->line;
            }
            
            if (next_is(mod->tokens, ","))
                consume(mod->tokens);
            
            if (next_is(mod->tokens, "]"))
                break;
            
            continue;
        }
        if (word->len)
            push(list, word);
        assert (next_is(mod->tokens, "]"), "expected ] after build flags");
        consume(mod->tokens);
    } else {
        string next = read_string(mod->tokens);
        push(list, next);
    }
    return list;
}

/// this must be basically the same as parsing named args; , should be optional for this
void import_read_fields(import im, tokens tokens) {
    silver mod = im->mod;
    while (true) {
        if (next_is(mod->tokens, "]")) {
            consume(mod->tokens);
            break;
        }
        token arg_name = next(mod->tokens);
        if (get_type(arg_name) == typeid(string))
            im->source = array_of(typeid(string), str(arg_name), null);
        else {
            assert (is_alpha(arg_name), "expected identifier for import arg");
            bool    use_tokens  = next_is(mod->tokens, "[");
            assert (use_tokens || next_is(mod->tokens, ":"),
                "expected : after import arg (argument assignment)");
            consume(mod->tokens);
            if (eq(arg_name, "name")) {
                token token_name = next(mod->tokens);
                assert (! get_type(token_name), "expected token for import name");
                im->name = str(token_name);
            } else if (eq(arg_name, "links"))    im->links      = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "includes")) im->includes   = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "products")) im->products   = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "source"))   im->source     = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "build"))    im->build_args = import_list(im->mod, use_tokens);
              else if (eq(arg_name, "shell")) {
                token token_shell = next(mod->tokens);
                assert (get_type(token_shell), "expected shell invocation for building");
                im->shell = str(token_shell);
            } else if (eq(arg_name, "defines")) {
                // none is a decent name for null.
                assert (false, "not implemented");
            } else
                assert (false, "unknown arg: %o", arg_name);

            if (next_is(mod->tokens, "]"))
                break;
        }
    }
}

/// get import keyword working to build into build-root (silver-import)
none import_init(import im) {
    silver mod = im->mod;
    assert(isa(im->tokens) == typeid(tokens), "tokens mismatch: class is %s", isa(im->tokens)->name);
    im->includes = new(array, alloc, 32);
    tokens tokens = im->tokens;
    if (tokens) {
        assert(next_is(mod->tokens, "import"), "expected import");
        consume(mod->tokens);
        //token n_token = next(mod->tokens);
        bool is_inc = next_is(mod->tokens, "<");
        if (is_inc) {
            im->import_type = import_t_includes;
            tokens_f* type = isa(tokens);
            consume(mod->tokens);
            im->includes = new(array, alloc, 8);
            while (1) {
                token inc = next(mod->tokens);
                verify (is_alpha(inc), "expected alpha-identifier for header");
                push(im->includes, inc);
                include(mod, str(inc->chars));
                bool is_inc = next_is(mod->tokens, ">");
                if (is_inc) {
                    consume(mod->tokens);
                    break;
                }
                token comma = next(mod->tokens);
                assert (eq(comma, ","), "expected comma-separator or end-of-includes >");
            }
        } else {
            string module_name = read_alpha(mod->tokens);
            assert(is_alpha(module_name), "expected mod name identifier");
            im->name = hold(module_name);

            if (next_is(mod->tokens, "as")) {
                consume(mod->tokens);
                im->isolate_namespace = next(mod->tokens);
            }

            assert(is_alpha(module_name), format("expected variable identifier, found %o", module_name));
            
            if (next_is(mod->tokens, "[")) {
                next(mod->tokens);
                token n = peek(mod->tokens);
                AType s = get_type(n);
                if (s == typeid(string)) {
                    im->source = new(array);
                    while (true) {
                        token    inner = next(mod->tokens);
                        string s_inner = cast(string, inner);
                        assert(get_type(inner) == typeid(string), "expected a string literal");
                        string  source = mid(s_inner, 1, len(s_inner) - 2);
                        push(im->source, source);
                        string       e = next(mod->tokens);
                        if (eq(e, ","))
                            continue;
                        assert(eq(e, "]"), "expected closing bracket");
                        break;
                    }
                } else {
                    import_read_fields(im, im->tokens);
                    consume(mod->tokens);
                }
            }
        }
    }
}

string configure_debug(bool debug) {
    return debug ? str("--with-debug") : str("");
}

string cmake_debug(bool debug) {
    return format("-DCMAKE_BUILD_TYPE=%s", debug ? "Debug" : "Release");
}

string make_debug(bool debug) {
    return debug ? str("-g") : str("");
}

void import_extract_libs(import im, string build_dir) {
    exts exts = get_exts();
    path i = im->mod->install;
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
        array  files      = ls(dawn_build, str(".a"), true);
        string all        = str("");
        each (files, path, f) {
            if (all->len)
                append(all, " ");
            append(all, f->chars);
        }
        exec("%o/bin/clang -shared -o %o/lib/lib%o.so -Wl,--whole-archive %o -Wl,--no-whole-archive",
            i, i, im->name, all);
    }
}

build_state import_build_project(import im, string name, string url) {
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
            if (!im->skip_process && !len(im->products)) { // default to this when initializing
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

                string cmake_flags = str("");
                each(im->build_args, string, arg) {
                    if (cast(bool, cmake_flags))
                        append(cmake_flags, " ");
                    append(cmake_flags, arg->chars);
                }
                
                if (!build_success) {
                    string cmake = str(
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
    if (len(im->name) && !len(im->source) && len(im->includes)) {
        array attempt = array_of(typeid(string), str(""), str("spec/"), NULL);
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
    } else if (len(im->name) && len(im->source)) {
        bool has_c  = false, has_h = false, has_rs = false,
             has_so = false, has_a = false;
        each(im->source, string, i0) {
            if (ends_with(i0, str(".c")))   has_c  = true;
            if (ends_with(i0, str(".h")))   has_h  = true;
            if (ends_with(i0, str(".rs")))  has_rs = true;
            if (ends_with(i0, str(".so")))  has_so = true;
            if (ends_with(i0, str(".a")))   has_a  = true;
        }
        if (has_h)
            im->import_type = import_t_source;
        else if (has_c || has_rs) {
            im->import_type = import_t_source;
            import_build_source(im);
        } else if (has_so) {
            im->import_type = import_t_library;
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), str(""), NULL);
            each(im->source, string, lib) {
                string rem = mid(lib, 0, len(lib) - 3);
                push(im->library_exports, rem);
            }
        } else if (has_a) {
            im->import_type = import_t_library;
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), str(""), NULL);
            each(im->source, string, lib) {
                string rem = mid(lib, 0, len(lib) - 2);
                push(im->library_exports, rem);
            }
        } else {
            assert(len(im->source) == 1, "source size mismatch");
            im->import_type = import_t_project;
            import_build_project(im, im->name, idx(im->source, 0));
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

tokens read_expr(silver mod) {
    mod->no_build = true; /// doesnt matter what we call it with, it wont give us back things
    /// this is so we can send our expression parser off to do its thing and we can tell where the expression ends
    /// just so we can process it later in our subproc
    int  e_start = mod->tokens->cursor;
    node is_void = parse_expression(mod);
    int  e_len   = mod->tokens->cursor - e_start;
    array body = new(array, alloc, e_len);
    for (int i = 0; i < e_len; i++) {
        push(body, mod->tokens->tokens->elements[e_start + i]);
    }
    tokens e_body = new(tokens, cursor, 0, tokens, body);
    mod->no_build = false;
    return e_body; 
}

tokens read_body(silver mod) {
    array body = new(array, alloc, 32);
    verify (next_is(mod->tokens, "["), "expected function body");
    int depth  = 0;
    do {
        token   token = next(mod->tokens);
        verify (token, "expected end of function body ( too many ['s )");
        push   (body, token);
        if      (eq(token, "[")) depth++;
        else if (eq(token, "]")) depth--;
    } while (depth > 0);
    tokens fn_body = new(tokens, cursor, 0, tokens, body);
    return fn_body; 
}

AType tokens_isa(tokens a) {
    token  t = idx(a->tokens, 0);
    return isa(t->literal);
}

num tokens_line(tokens a) {
    token  t = idx(a->tokens, 0);
    return t->line;
}

string tokens_location(tokens a) {
    token  t = idx(a->tokens, 0);
    return t ? (string)location(t) : (string)format("n/a");
}

bool is_keyword(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string))
        s = any;
    else if (type == typeid(token))
        s = new(string, chars, ((token)any)->chars);
    
    return index_of_cstr(keywords, s->chars) >= 0;
}

bool is_alpha(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string)) {
        s = any;
    } else if (type == typeid(token)) {
        token token = any;
        s = new(string, chars, token->chars);
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
    
    string  special_chars   = str(".$,<>()![]/+*:=#");
    array   tokens          = new(array, alloc, 128);
    num     line_num        = 1;
    num     length          = len(input_string);
    num     index           = 0;
    num     line_start      = 0;

    while (index < length) {
        i32 chr = idx(input_string, index);
        
        if (isspace(chr)) {
            if (chr == '\n') {
                line_num += 1;
                line_start = index + 1;
            }
            index += 1;
            continue;
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
            if (chr == ':' && idx(input_string, index + 1) == ':')
                name = "::";
            else if (chr == '=' && idx(input_string, index + 1) == '=')
                name = "==";
            else if (chr == '<' && idx(input_string, index + 1) == '-')
                name = "<-";
            push(tokens, new(token, chars, name, source, src, line, line_num, column, index - line_start));
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
                push(tokens, new(token,
                    chars,  ch,
                    source, src,
                    line,   line_num,
                    column, start - line_start));
                crop = str(&crop->chars[1]);
                line_start++;
            }
            push(tokens, new(token,
                chars,  crop->chars,
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
        push(tokens, new(token, chars, crop->chars, source, src,
            line,   line_num,
            column, start - line_start));
    }
    return tokens;
}

none tokens_init(tokens a) {
    if (a->file)
        a->tokens = parse_tokens(a->file);
    else if (!a->tokens)
        assert (false, "file/tokens not set");
    a->stack = new(array, alloc, 4);
}

token tokens_element(tokens a, num rel) {
    return a->tokens->elements[clamp(a->cursor + rel, 0, a->tokens->len - 1)];
}

token tokens_navigate(tokens a, int count) {
    if (a->cursor <= 0)
        return null;
    a->cursor += count;
    token res = element(a, 0);
    return res;
}

token tokens_prev(tokens a) {
    if (a->cursor <= 0)
        return null;
    a->cursor--;
    token res = element(a, 0);
    return res;
}

token tokens_next(tokens a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    a->cursor++;
    return res;
}

token tokens_consume(tokens a) {
    return tokens_next(a);
}

token tokens_peek(tokens a) {
    return element(a, 0);
}

bool tokens_next_is(tokens a, symbol cs) {
    token n = element(a, 0);
    return n && strcmp(n->chars, cs) == 0;
}

bool tokens_read(tokens a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        a->cursor++;
        return true;
    }
    return false;
}

object tokens_read_literal(tokens a) {
    token  n = element(a, 0);
    if (n->literal) {
        a->cursor++;
        return n->literal;
    }
    return null;
}

string tokens_read_string(tokens a) {
    token  n = element(a, 0);
    if (isa(n->literal) == typeid(string)) {
        string token_s = str(n->chars);
        string result  = mid(token_s, 1, token_s->len - 2);
        a->cursor ++;
        return result;
    }
    return null;
}

object tokens_read_numeric(tokens a) {
    token n = element(a, 0);
    if (isa(n->literal) == typeid(f64) || isa(n->literal) == typeid(i64)) {
        a->cursor++;
        return n->literal;
    }
    return null;
}

string tokens_read_assign(tokens a) {
    token  n = element(a, 0);
    string k = str(n->chars);
    num assign_index = index_of(assign, k);
    bool found = assign_index >= 0;
    if (found) a->cursor ++;
    return found ? k : null;
}

string tokens_read_alpha(tokens a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        a->cursor ++;
        return str(n->chars);
    }
    return null;
}

string tokens_read_keyword(tokens a) {
    token n = element(a, 0);
    if (is_keyword(n)) {
        a->cursor ++;
        return str(n->chars);
    }
    return null;
}

object tokens_read_bool(tokens a) {
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

void tokens_push_state(tokens a, array tokens, num cursor) {
    tokens_data state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}

void tokens_pop_state(tokens a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data state = (tokens_data)last(a->stack); // we should call this element or ele
    pop(a->stack);
    if(!transfer)
        a->cursor = state->cursor;
    if (state->tokens != a->tokens) {
        drop(a->tokens);
        a->tokens = state->tokens;
    }
}

void tokens_push_current(tokens a) {
    push_state(a, a->tokens, a->cursor);
}

bool tokens_cast_bool(tokens a) {
    return a->cursor < len(a->tokens);
}

node parse_return(silver mod) {
    model rtype = return_type(mod);
    bool  is_v  = is_void(rtype);
    model ctx = context_model(mod, typeid(function));
    consume(mod->tokens);
    node expr   = is_v ? null : parse_expression(mod);
    log("return-type", "%o", is_v ? (object)str("void") : 
                                    (object)expr->mdl);
    return freturn(mod, expr);
}

node parse_break(silver mod) {
    consume(mod->tokens);
    node vr = null;
    return null;
}

node parse_for(silver mod) {
    consume(mod->tokens);
    node vr = null;
    return null;
}

node parse_while(silver mod) {
    consume(mod->tokens);
    node vr = null;
    return null;
}

node silver_parse_if_else(silver mod) {
    consume(mod->tokens);
    node vr = null;
    return null;
}

node silver_parse_do_while(silver mod) {
    consume(mod->tokens);
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
                if (!read(mod->tokens, cstring(token)))
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

model read_model(silver mod, bool is_ref) {
    string name = read_alpha(mod->tokens); // Read the type to cast to
    if (!name) return null;
    return emodel(name->chars);
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

/// @brief modifies incoming member for the various wrap cases we will want to 
/// serve.  its important that this be extensible
static model parse_wrap(silver mod, model mdl_src) {
    verify(read(mod->tokens, "["), "expected [");
    array shape = new(array, alloc, 32);
    while (true) {
        if (next_is(mod->tokens, "]")) break;
        node n = parse_expression(mod);
        /// check to be sure its a literal
        push(shape, n);
    }
    model mdl_wrap = model_alias(mdl_src, null, 0, shape);
    verify(read(mod->tokens, "]"), "expected ]");
    return mdl_wrap;
}

static function parse_fn(silver mod, AFlag member_type, model rtype, object name, interface access);

static node parse_function_call(silver mod, node target, member fmem);

static node read_cast(silver mod) {
    push_current(mod->tokens);
    //print_tokens("read-cast", mod);
    if (!read(mod->tokens, "[")) {
        pop_state(mod->tokens, false);
        return null;
    }
    string alpha = read_alpha(mod->tokens);
    if(!alpha) {
        pop_state(mod->tokens, false);
        return null;
    }
    member mem = lookup(mod, alpha);
    if (!mem || !mem->is_type || mem->is_func) {
        pop_state(mod->tokens, false);
        return null;
    }

    if (read(mod->tokens, ".")) {
        pop_state(mod->tokens, false);
        return null;
    }

    model mdl = mem->mdl;
    if (next_is(mod->tokens, "["))
        mdl = parse_wrap(mod, mdl);

    if (!read(mod->tokens, ",")) {
        pop_state(mod->tokens, false);
        return null;
    }

    node expr = parse_expression(mod);
    if (is_object(expr)) {
        model method = cast_method(mod, expr->mdl, mdl);
        if (method) {
            // object may cast because there is a method defined with is_cast and an rtype of the cast_ident
            array args = new(array);
            node mcall = fcall(mod, method, expr, args);
            return mcall;
        }
    }
    verify (!is_object(expr), "object %o requires a cast method for %o",
        expr->mdl->name, mdl->name);
    /// cast to basic type with convert
    verify (!read(mod->tokens, "]"), "expected ] to complete cast");
    node conv = convert(mod, expr, mdl);
    print("is_cast");
    pop_state(mod->tokens, true); /// we pop state, saving this current
    return conv;
}

/// @brief this reads entire function definition, member we lookup, new member
static member read_member(silver mod) {
    tokens tk = mod->tokens;
    push_current(mod->tokens);
    interface access = interface_undefined;
    for (int m = 1; m < interface_type.member_count; m++) {
        type_member_t* enum_v = &interface_type.members[m];
        if (read(mod->tokens, enum_v->name)) {
            access = m;
            break;
        }
    }
    bool   is_static = read(mod->tokens, "static");
    bool   is_ref    = read(mod->tokens, "ref");

    string alpha = read_alpha(mod->tokens);
    if(alpha) {
        model  ctx      = mod->top;
        member target   = null; // member is a node with value-ref (value)
        member mem      = lookup(mod, alpha);

        /// make sure this is not a constructor in context
        if (mem && mem->mdl != mod->top) {
            /// attempt to resolve target if there is a chain
            /// silver makes c->like->code a bit safer, and pointers more approachable
            bool safe = false;
            while ((safe = read(mod->tokens, "->")) || read(mod->tokens, ".")) {
                target = mem;
                push(mod, mem->mdl);
                string alpha = read_alpha(mod->tokens);
                mem = resolve(mem, alpha); /// needs an argument
                verify(alpha, "expected alpha identifier");
                pop(mod);
            }
            verify (!is_static, "unexpected static description of member");

            /// if is_ref, its an anonymous member with a mdl type +ref
            if (mem && mem->is_func) {
                pop_state(mod->tokens, true); /// we pop state, saving this current
                return mem; // parse_function_call(mod, target, mem); (we are doing this in parse_statements)
            }
            if (mem && !mem->is_type) {
                pop_state(mod->tokens, true); /// we pop state, saving this current
                return mem; /// baked into mem is read.all.members.in.series <- where series is mem (right gpt?)
            }
            
            pop_state(mod->tokens, false); /// we pop the state to the last push
            push_current(mod->tokens);
        }
    }

    token n   = peek(mod->tokens);
    model mdl = read_model(mod, is_ref);
    if (!mdl) {
        print("info: could not read type at position %o", location(mod->tokens));
        pop_state(mod->tokens, false); // we may 'info' here
        return null;
    }

    // may be [, or alpha-id  (its an error if its neither)
    if (next_is(mod->tokens, "["))
        mdl = parse_wrap(mod, mdl);

    bool   is_ctr = mdl == mod->top;
    object name = null;
    object default_value = null;
    if (is_ctr) {
        /// construct
        verify (!is_static, "unexpected static for construct");
        if (next_is(mod->tokens, "[") || next_is(mod->tokens, "return") || next_is(mod->tokens, "<-")) {
            name = mdl->name;
            mdl  = parse_fn(mod, A_TYPE_CONSTRUCT, mdl, null, access);
        } else
            fault("expected function body for constructor");
    } else {
        /// cast
        string keyword   = read_keyword(mod->tokens);
        bool   is_cast   = keyword && eq(keyword, "cast");
        if (is_cast) {
            verify (!is_static, "unexpected static for cast");
            print_tokens("cast", mod);
            verify (!is_static, "unexpected static keyword does not apply to cast");
            verify (!is_ref,    "unexpected ref keyword does not apply to cast");
            
            if (next_is(mod->tokens, "[") || next_is(mod->tokens, "return") || next_is(mod->tokens, "<-")) {
                name = format("cast_%o", mdl->name);
                mdl  = parse_fn(mod, A_TYPE_CAST, mdl, null, access);
            } else
                fault("expected function body for cast");
        } else {
            /// read member name
            name = next(mod->tokens);
            verify(is_alpha(name), "expected alpha-numeric name");

            /// convert model to function parse_fn takes in rtype and token name
            if (next_is(mod->tokens, "[")) {
                mdl = parse_fn(mod, is_static ? A_TYPE_SMETHOD : A_TYPE_IMETHOD,
                    mdl, name, access);
            } else {
                /// we may set the 'value' here so we may re-parse later for 'default values'
                /// value here is tokens
                default_value = read_expr(mod);
            }
        }
    }

    member mem = new(member,
        mod, mod, name, name, mdl, mdl, is_static, is_static,
        access, access, default_value, default_value);
    push_member(mod, mem);
    pop_state(mod->tokens, true);
    return mem;
}


arguments parse_args(silver mod) {
    verify(read(mod->tokens, "["), "parse-args: expected [");
    array       args = new(array,     alloc, 32);
    arguments   res  = new(arguments, mod, mod, args, args);
    //print_tokens("args", mod);
    if (!next_is(mod->tokens, "]")) {
        push(mod, res); // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        while (true) {
            member arg = read_member(mod);
            verify (arg,       "member failed to read");
            verify (arg->name, "member name not set");
            push   (args, arg);
            if     (next_is(mod->tokens, "]")) break;
            verify (next_is(mod->tokens, ","), "expected separator");
            consume(mod->tokens);
        }
        pop(mod);
    }
    consume(mod->tokens);
    return res;
}

node parse_statements(silver mod);

static node parse_construct(silver mod, member mem) {
    verify(mem && mem->is_type, "expected member type");
    verify(read(mod->tokens, "["), "expected [ after type name for construction");
    node res = null;
    /// it may be a dedicated constructor (for primitives or our own), or named args (not for primitives)
    if (instanceof(mem->mdl->src, record)) {
        node n = alloc(mod, mem->mdl->src, null);
        /// now we need to parse the named arguments, and perform assignment on each
        /// then we separately parse 'default values' (not now, but thats in member as a body attribute)
        res = n;
    } else {

    }
    return res;
}

static node parse_function_call(silver mod, node target, member fmem) {
    bool allow_no_paren = mod->expr_level == 1; /// remember this decision? ... var args
    bool expect_end_br  = false;
    function fn        = fmem->mdl;
    verify(isa(fn) == typeid(function), "expected function type");
    int  model_arg_count = len(fn->args);
    
    if (next_is(mod->tokens, "[")) {
        consume(mod->tokens);
        expect_end_br = true;
    } else if (!allow_no_paren)
        fault("expected [ for nested call");

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
        if (next_is(mod->tokens, ",")) {
            consume(mod->tokens);
            continue;
        } else if (next_is(mod->tokens, "]")) {
            verify (arg_index >= model_arg_count, "expected %i args", model_arg_count);
            break;
        } else if (arg_index >= model_arg_count)
            break;
    }
    if (expect_end_br) {
        verify(next_is(mod->tokens, "]"), "expected ] end of function call");
        consume(mod->tokens);
    }
    return fcall(mod, fmem, target, values);
}

static node parse_ternary(silver mod, node expr) {
    if (!read(mod->tokens, "?")) return expr;
    node expr_true  = parse_expression(mod);
    node expr_false = parse_expression(mod);
    return ether_ternary(mod, expr, expr_true, expr_false);
}

static node parse_primary(silver mod) {
    token t = peek(mod->tokens);

    // handle the logical NOT operator (e.g., '!')
    if (read(mod->tokens, "!") || read(mod->tokens, "not")) {
        node expr = parse_expression(mod); // Parse the following expression
        return not(mod, expr);
    }

    // bitwise NOT operator
    if (read(mod->tokens, "~")) {
        node expr = parse_expression(mod);
        return bitwise_not(mod, expr);
    }

    // 'typeof' operator
    if (read(mod->tokens, "typeof")) {
        bool bracket = false;
        if (next_is(mod->tokens, "[")) {
            assert(next_is(mod->tokens, "["), "Expected '[' after 'typeof'");
            consume(mod->tokens); // Consume '['
            bracket = true;
        }
        node expr = parse_expression(mod); // Parse the type expression
        if (bracket) {
            assert(next_is(mod->tokens, "]"), "Expected ']' after type expression");
            consume(mod->tokens); // Consume ']'
        }
        return expr; // Return the type reference
    }

    // 'cast' operator has a type inside
    node cast = read_cast(mod);
    if  (cast) return cast;

    // 'ref' operator (reference)
    if (read(mod->tokens, "ref")) {
        mod->in_ref = true;
        node expr = parse_expression(mod);
        mod->in_ref = false;
        fault("not impl");
        return expr;
    }

    // literal values (int, float, bool, string)
    object n = read_literal(mod->tokens);
    if (n)
        return operand(mod, n);

    // parenthesized expressions
    if (read(mod->tokens, "[")) {
        node expr = parse_expression(mod); // Parse the expression
        verify(read(mod->tokens, "]"), "Expected closing parenthesis");
        return parse_ternary(mod, expr);
    }
    
    // handle identifiers (variables or function calls)
    string ident = read_string(mod->tokens);
    member mem = lookup(mod, ident); // we need target on member
    if (mem) {
        if (mem->is_func) {
            if (mod->in_ref) {
                fault("not implemented");
                return null; /// simple stack mechanism we need to support returnin the pointer for (is this a 'load?')
            } else
                return parse_function_call(mod, mem->target, mem);
        } else if (mem->is_type) {
            return parse_construct(mod, mem); /// this, is the construct
        } else
            return load(mod, mem, null, null);
    }

    fault("unexpected %o in primary expression", ident);
    return null;
}

node parse_assignment(silver mod, member mem, string op) {
    verify(!mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    string         op_name = get(operators, op);
    member         method  = null;
    if (instanceof(mem->mdl, record))
        method = get(((record)mem->mdl)->members, op_name); /// op-name must be reserved for use in functions only
    node           res     = null;
    node           L       = mem;
    node           R       = parse_expression(mod);

    mod->in_assign = mem;

    if (method && method->mdl && instanceof(method->mdl, function)) {
        array args = array_of(null, R, null);
        res = fcall(mod, method, L, args);
    } else {
        mem->is_const = eq(op, "=");
        bool e = mem->is_const;
        if (e || eq(op, ":"))  res = assign(mod,     L, R); // LLVMBuildStore(B, R, L);
        else if (eq(op, "+=")) res = assign_add(mod, L, R); // LLVMBuildAdd  (B, R, L, "assign-add");
        else if (eq(op, "-=")) res = assign_sub(mod, L, R); // LLVMBuildSub  (B, R, L, "assign-sub");
        else if (eq(op, "*=")) res = assign_mul(mod, L, R); // LLVMBuildMul  (B, R, L, "assign-mul");
        else if (eq(op, "/=")) res = assign_div(mod, L, R); // LLVMBuildSDiv (B, R, L, "assign-div");
        else if (eq(op, "%=")) res = assign_mod(mod, L, R); // LLVMBuildSRem (B, R, L, "assign-mod");
        else if (eq(op, "|=")) res = assign_or(mod,  L, R); // LLVMBuildOr   (B, R, L, "assign-or"); 
        else if (eq(op, "&=")) res = assign_and(mod, L, R); // LLVMBuildAnd  (B, R, L, "assign-and");
        else if (eq(op, "^=")) res = assign_xor(mod, L, R); // LLVMBuildXor  (B, R, L, "assign-xor");
        else
            fault("unsupported operator: %o", op);
    }
    /// update member's value_ref (or not! it was not what we expected in LLVM)
    /// investigate
    ///mem->value = res->value;
    if (eq(op, "=")) mem->is_const = true;

    /// our grand entrance into type-inference; mdl can be null for newly created members
    /// we just need to let this null not interfere with the assignment
    if (!mem->mdl)
        mem->mdl = res->mdl;
    
    mod->in_assign = null;
    return mem;
}

node cond_builder_ternary(silver mod, tokens cond_tokens, object unused) {
    push_state(mod->tokens, cond_tokens->tokens, 0);
    node cond_expr = parse_expression(mod);
    verify(cond_tokens->cursor == len(cond_tokens->tokens), 
        "unexpected trailing expression in single expression condition");
    pop_state(mod->tokens, false);
    return cond_expr;
}

node expr_builder_ternary(silver mod, tokens expr_tokens, object unused) {
    push_state(mod->tokens, expr_tokens->tokens, 0);
    node exprs = parse_expression(mod);
    verify(expr_tokens->cursor == len(expr_tokens->tokens), 
        "error: statements not fully processed");
    pop_state(mod->tokens, false);
    return exprs;
}

node cond_builder(silver mod, tokens cond_tokens, object unused) {
    push_state(mod->tokens, cond_tokens->tokens, 0);
    node cond_expr = parse_expression(mod);
    verify(cond_tokens->cursor == len(cond_tokens->tokens), 
        "unexpected trailing expression in single expression condition");
    pop_state(mod->tokens, false);
    return cond_expr;
}

node expr_builder(silver mod, tokens expr_tokens, object unused) {
    push_state(mod->tokens, expr_tokens->tokens, 0);
    node exprs = parse_statements(mod);
    verify(expr_tokens->cursor == len(expr_tokens->tokens), 
        "error: statements not fully processed");
    pop_state(mod->tokens, false);
    return exprs;
}

/// parses entire chain of if, [else-if, ...] [else]
node parse_if_else(silver mod) {
    bool  require_if   = true;
    array tokens_cond  = new(array, alloc, 32);
    array tokens_block = new(array, alloc, 32);
    while (true) {
        bool is_if  = read(mod->tokens, "if");
        verify(is_if && require_if || !require_if, "expected if");
        tokens cond  = is_if ? read_body(mod) : null;
        tokens block = read_body(mod);
        push(tokens_cond,  cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = read(mod->tokens, "else");
        require_if = false;
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    return ether_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}

node parse_do_while(silver mod) {
    consume(mod->tokens);
    node vr = null;
    return null;
}

/// top-level parsing is fairly basic
node parse_statement(silver mod) {
    token t = peek(mod->tokens);
    if (next_is(mod->tokens, "return") || next_is(mod->tokens, "<-")) return parse_return(mod);
    if (next_is(mod->tokens, "break"))  return parse_break(mod);
    if (next_is(mod->tokens, "for"))    return parse_for(mod);
    if (next_is(mod->tokens, "while"))  return parse_while(mod);
    if (next_is(mod->tokens, "if"))     return parse_if_else(mod);
    if (next_is(mod->tokens, "do"))     return parse_do_while(mod);

    member mem    = null;
    string ident  = read_string(mod->tokens);
    if (ident) {
        mem = lookup(mod, ident);
        if (mem) {
            verify (!mem->is_type, "invalid syntax");
            if (mem->is_func)
                return parse_function_call(mod, null, mem);
        } else {
            /// now we must infer the type, start with null, and please proceed governor
            member mem = new(member, mod, mod, name, ident);
            push_member(mod, mem);
        }
        string assign = read_assign(mod->tokens);
        verify (assign, "expected assignment on identifier %o, found %o", ident, next(mod->tokens));
        return parse_assignment(mod, mem, assign);
    }
    fault ("parse-statement: unexpected %o", next(mod->tokens));
    return null;
}

node parse_statements(silver mod) {
    push(mod, new(statements, mod, mod));
    
    print_tokens("parse_statements", mod);

    bool multiple   = next_is(mod->tokens, "[");
    if  (multiple)    consume(mod->tokens);
    int  depth      = 1;
    node vr = null;
    ///
    while(cast(bool, mod->tokens)) {
        if(multiple && read(mod->tokens, "[")) {
            depth += 1;
            push(mod, new(statements, mod, mod));
        }
        vr = parse_statement(mod);
        if(!multiple) break;
        if(next_is(mod->tokens, "]")) {
            if (depth > 1)
                pop(mod);
            next(mod->tokens);
            if ((depth -= 1) == 0) break;
        }
    }
    pop(mod);
    return vr;
}

void build_function(silver mod, object arg, function fn) {
    tokens body = instanceof(fn->user, tokens);
    push_state(mod->tokens, body->tokens, 0);
    parse_statements(mod);
    pop_state(mod->tokens, false);
}

function parse_fn(silver mod, AFlag member_type, model rtype, object ident, interface access) {
    token name = instanceof(ident, token);
    if (!name) {
        verify(member_type == A_TYPE_CAST, "expected member type of cast if there is no name");
        print("casting to %o", rtype->name);
        name = new(token, chars, rtype->name->chars);
    } else
        verify(member_type != A_TYPE_CAST, "unexpected name for cast");
    
    arguments       args = null;
    if (member_type == A_TYPE_CAST) {
        verify (next_is(mod->tokens, "[") || next_is(mod->tokens, "<-") || next_is(mod->tokens, "return"), "expected body for cast");
        args = new(arguments);
    } else {
        verify (next_is(mod->tokens, "["), "expected function args [");
        args    = parse_args(mod);
    }
    subprocedure    process = subproc   (mod, build_function, null);
    record          rec_top = instanceof(mod->top, record) ? mod->top : null;
    function        fn      = new       (function,
        mod,    mod,     name,   name, function_type, member_type,
        record, rec_top, rtype,  rtype,
        args,   args,    process, process,
        access, access,  user,   read_body(mod)); /// user must process their payload for a function to effectively resolve
    process->ctx = fn; /// safe to set after, as we'll never start building within function; we need this context in builder
    return fn;
}

/// this has to be called when we have made all top members
/// when its called, the record type and 
/// create the members of the struct first, then we call record_completer manually
/// better this than create 'future' chain out of the prior, more direct control here.
void build_record(silver mod, object arg, record rec) {
    bool   is_class = instanceof(rec, class) != null;
    symbol sname    = is_class ? "class" : "struct";
    log("build_record", "%s %o", sname, rec->name);
    tokens body     = instanceof(rec->user, tokens);
    push_state(mod->tokens, body->tokens, 0);
    verify(read(mod->tokens, "["), "expected [");

    push(mod, rec);
    while (cast(bool, mod->tokens)) {
        if (next_is(mod->tokens, "]"))
            break;
        member mem = read_member(mod);
        if   (!mem) break;

        verify(get(rec->members, str(mem->name->chars)), "member not added to %o", rec->name);

        /// struct members are always public, since there are no methods
        verify(is_class || mem->access != interface_intern, 
            "only public members allowed in struct");

        /// if there is an initializer, we should emit the expression in an init function
        /// it would be called post-args, the same as A-type
        /// it needs to be smart about holding, though
        verify(len(mem->name), "member must have name in class: %o", rec->name);

        //set(rec->members, str(mem->name->chars), mem);
        //set(mod->members, mem->name, mem);

        /// functions could be built right away when reading
        /// however we may need to delay based on reference to type or member
        /// that has not been built yet.
    }
    pop(mod);

    verify(read(mod->tokens, "]"), "expected ]");
    pop_state(mod->tokens, false);
    //compile(mod, rec);
}

record parse_record(silver mod, bool is_class) {
    verify(( is_class && read(mod->tokens, "class")) || 
           (!is_class && read(mod->tokens, "struct")),
        "expected record type");
    token name  = read_alpha(mod->tokens);
    verify(name, "expected alpha-numeric class identifier");
    token parent_name = null;
    if (next_is(mod->tokens, ":")) {
        consume(mod->tokens);
        parent_name = read_alpha(mod->tokens);
        verify(parent_name, "expected alpha-numeric parent identifier");
    }
    verify (next_is(mod->tokens, "[") , "expected function body");
    tokens body = read_body(mod);

    subprocedure process = subproc(mod, build_record, null);
    record rec;
    if (is_class)
        rec = new(class,     mod, mod, name, name, process, process, parent_name, parent_name);
    else
        rec = new(structure, mod, mod, name, name, process, process);
    process->ctx = rec;
    rec->user = body;
    return rec;
}

void silver_parse(silver mod) {
    /// in top level we only need the names of classes and their blocks
    /// technically we must do this before any functions
    /// we did not need this before because we did not have functions before

    /// first pass we read classes, and aliases
    /// then we load the class members from their token states
    while (cast(bool, mod->tokens)) {
        if (next_is(mod->tokens, "import")) {
            import im = new(import, mod, mod, tokens, mod->tokens);
            push(mod->imports, im);
        } else if (next_is(mod->tokens, "enum")) {
            consume(mod->tokens);
            string ename = read_alpha(mod->tokens);
            verify (ename, "expected alpha-numeric name for enum");
            model mdl = emodel("i32");
            if (read(mod->tokens, ":")) {
                string mtype = read_alpha(mod->tokens);
                verify (mtype, "expected alpha-numeric name for model type");
                mdl = emodel(mtype->chars);
            }
            AType atype = isa(mdl->src);
            verify(atype, "enumerables can only be based on primitive types (i32 default)");
            verify(read(mod->tokens, "["), "expected [ after enum type");
            array enums = import_list(mod, true);
            verify(len(enums), "expected > 0 enums");
            map members = new(map, hsize, 16);
            i64 value = 0;
            each(enums, token, e) {
                string estr = cast(string, e);
                member mem = new(member,
                    mod, mod, name, estr, mdl, mdl, is_const, true);
                
                object v = A_primitive(atype, &value); /// this creates i8 to i64 data, using &value i64 addr
                set_value(mem, v);
                set(members, estr, mem);
                value++;
            }
            int mlen = len(members);
            enumerable e = new(enumerable, mod, mod, size, mdl->size, name, ename, members, members);
            push_model(mod, e);
        } else if (next_is(mod->tokens, "class") || next_is(mod->tokens, "struct")) {
            bool  is_class = next_is(mod->tokens, "class");
            record rec = parse_record(mod, is_class);
            /// with the same name, we effectively change the model of membership, and what inlay and ref/ptr do
            model  access_model = alias(rec, str(rec->name->chars), reference_pointer, null);
            member mem = new(member, mod, mod, name, rec->name, mdl, access_model, is_type, true);
            string key = cast(string, rec->name);
            push_member(mod, mem);
        } else {
            /// must make read-ahead mechanism for reading functions
            member mem = read_member(mod);
            string key = str(mem->name->chars);
            push_member(mod, mem);
        }
    }

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

    pairs(mod->members, i) {
        member mem = i->value;
        if (instanceof(mem->mdl, function))
            process_finalize(mem->mdl);
    }
}

build_state import_build_project(import im, string name, string url);

bool silver_compile(silver mod) {
    verify(exec("%o/bin/llc -filetype=obj %o.ll -o %o.o -relocation-model=pic",
        mod->install, mod->name, mod->name) == 0,
            ".ll -> .o compilation failure");

    string libs = new(string, alloc, 128);
    each (mod->imports, import, im) {
        each_ (im->links, string, link)
            append(libs, format("-l%o ", link)->chars);
    }
    each (mod->imports, import, im) {
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
    
    mod->imports   = new(array, alloc, 32);
    mod->products_used = new(array);
    mod->tokens    = new(tokens, file, mod->source);

    array  import_A = array_of(
        typeid(token), new(token, chars, "import"), new(token, chars, "A"), null);
    tokens tok      = new(tokens, tokens, import_A);
    push_state(mod->tokens, import_A, 0);
    import Atype    = new(import, mod, mod, tokens, tok, name, str("A"), skip_process, true);
    pop_state(mod->tokens, false);
    push(mod->imports, Atype);
    import_build_project(Atype, str("A"), str("https://github.com/ar-visions/A"));
    include(mod, str("A"));

    parse(mod);

    /// process imported
    each (mod->imports, import, im)
        process(im);

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
        "module",  str(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."), null);
    string mkey     = str("module");
    string name     = get(args, str("module"));
    path   n        = path(chars, name->chars);
    path   source   = absolute(n);
    silver mod      = silver(
        source,  source,
        install, get(args, str("install")),
        name,    stem(source));
}

define_class (tokens)
define_mod   (silver, ether)
define_enum  (import_t)
define_enum  (build_state)
define_class (import)
module_init  (init)