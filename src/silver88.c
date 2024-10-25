#define import_intern intern(import)
#define silver_intern intern(silver)
#define tokens_intern intern(tokens)
#include <silver>
#include <ether>


#define   mcall(M,...)   call(mod, M, ## __VA_ARGS__)
#define   emodel(MDL)    ({ \
    member  m = ether_lookup(mod, str(MDL)); \
    model mdl = m ? m->mdl : null; \
    mdl; \
})

#define   icall(M,...)   import_ ## M(im, ## __VA_ARGS__)

#undef  peek
#define tok(...)     call(mod->tokens, __VA_ARGS__)

static map   operators;
static array keywords;
static array consumables;
static map   assign;
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
    { { OPType__compare_equal, OPType__compare_not } }
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
        "return", "asm",      "if",     "switch",
        "while",  "for",      "do",     "signed", "unsigned", "cast", null);
    consumables = array_of_cstr(
        "ref", "schema", "enum", "class", "union", "proto", "struct",
        "const", "volatile", "signed", "unsigned", null);
    assign = array_of_cstr(
        ":", "=", "+=", "-=", "*=", "/=", 
        "|=", "&=", "^=", ">>=", "<<=", "%=", null);
    compare = array_of_cstr("==", "!=", null);
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
        "%o/%s%s%s", mod->source, name, sub ? "/" : "", sub ? sub : "");
    path   res = cast(dir, path);
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

array import_list(import im, tokens tokens) {
    array list = new(array);
    silver mod = im->mod;
    if (tok(next_is, "[")) {
        tok(consume);
        while (true) {
            token arg = tok(next);
            if (eq(arg, "]")) break;
            assert (call(arg, get_type) == typeid(string), "expected build-arg in string literal");
            A l = arg->literal; /// must be set in token_init
            push(list, l);
            if (tok(next_is, ",")) {
                tok(consume);
                continue;
            }
            break;
        }
        assert (tok(next_is, "]"), "expected ] after build flags");
        tok(consume);
    } else {
        string next = tok(read_string);
        push(list, next);
    }
    return list;
}

void import_read_fields(import im, tokens tokens) {
    silver mod = im->mod;
    while (true) {
        if (tok(next_is, "]")) {
            tok(consume);
            break;
        }
        token arg_name = tok(next);
        if (call(arg_name, get_type) == typeid(string))
            im->source = array_of(typeid(string), str(arg_name), null);
        else {
            assert (is_alpha(arg_name), "expected identifier for import arg");
            assert (tok(next_is, ":"), "expected : after import arg (argument assignment)");
            tok(consume);
            if (eq(arg_name, "name")) {
                token token_name = tok(next);
                assert (! call(token_name, get_type), "expected token for import name");
                im->name = str(token_name);
            } else if (eq(arg_name, "links"))    im->links      = icall(list, tokens);
              else if (eq(arg_name, "includes")) im->includes   = icall(list, tokens);
              else if (eq(arg_name, "source"))   im->source     = icall(list, tokens);
              else if (eq(arg_name, "build"))    im->build_args = icall(list, tokens);
              else if (eq(arg_name, "shell")) {
                token token_shell = tok(next);
                assert (call(token_shell, get_type), "expected shell invocation for building");
                im->shell = str(token_shell);
            } else if (eq(arg_name, "defines")) {
                // none is a decent name for null.
                assert (false, "not implemented");
            } else
                assert (false, "unknown arg: %o", arg_name);

            if (tok(next_is, ","))
                tok(next);
            else {
                assert (tok(next_is, "]"), "expected comma or ] after arg %o", arg_name);
                break;
            }
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
        assert(tok(next_is, "import"), "expected import");
        tok(consume);
        //token n_token = tok(next);
        bool is_inc = tok(next_is, "<");
        if (is_inc) {
            im->import_type = import_t_includes;
            tokens_f* type = isa(tokens);
            tok(consume);
            im->includes = new(array, alloc, 8);
            while (1) {
                token inc = tok(next);
                assert (is_alpha(inc), "expected alpha-identifier for header");
                push(im->includes, inc);
                call(mod, include, str(inc->chars));
                bool is_inc = tok(next_is, ">");
                if (is_inc) {
                    tok(consume);
                    break;
                }
                token comma = tok(next);
                assert (eq(comma, ","), "expected comma-separator or end-of-includes >");
            }
        } else {
            token t_next = tok(next);
            string module_name = cast(t_next, string);
            im->name = hold(module_name);
            assert(is_alpha(module_name), "expected mod name identifier");

            if (tok(next_is, "as")) {
                tok(consume);
                im->isolate_namespace = tok(next);
            }

            assert(is_alpha(module_name), format("expected variable identifier, found %o", module_name));
            
            if (tok(next_is, "[")) {
                tok(next);
                token n = tok(peek);
                AType s = call(n, get_type);
                if (s == typeid(string)) {
                    im->source = new(array);
                    while (true) {
                        token    inner = tok(next);
                        string s_inner = cast(inner, string);
                        assert(call(inner, get_type) == typeid(string), "expected a string literal");
                        string  source = mid(s_inner, 1, len(s_inner) - 2);
                        push(im->source, source);
                        string       e = tok(next);
                        if (eq(e, ","))
                            continue;
                        assert(eq(e, "]"), "expected closing bracket");
                        break;
                    }
                } else {
                    icall(read_fields, im->tokens);
                    tok(consume);
                }
            }
        }
    }
}

build_state import_build_project(import im, string name, string url) {
    path checkout = create_folder(im->mod, "checkouts", name->chars);
    path i        = create_folder(im->mod, im->mod->with_debug ? "debug" : "install", null);
    path b        = form(path, "%o/%s", checkout, "silver-build");

    path cwd = path_cwd(2048);

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
        make_dir(b);
    }
    /// intialize and build
    if (!is_empty(checkout)) { /// above op can add to checkout; its not an else
        chdir(checkout->chars);

        bool build_success = file_exists("%o/silver-token", b);
        if (file_exists("silver-init.sh") && !build_success) {
            string cmd = format(
                "%o/silver-init.sh \"%s\"", path_type.cwd(2048), i);
            assert(system(cmd->chars) == 0, "cmd failed");
        }
    
        bool is_rust = file_exists("Cargo.toml");
        ///
        if (is_rust) {
            cstr rel_or_debug = "release";
            path package = form(path, "%o/%s/%o", i, "rust", name);
            make_dir(package);
            ///
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
                im->links = array_of(typeid(string), name, null);
                create_symlink(lib, sym);
            }
            if (file_exists(exe->chars)) {
                path sym = form(path, "%o/%o", i, name);
                create_symlink(exe, sym);
            }
        }   
        else {
            assert (file_exists("CMakeLists.txt"),
                "CMake required for project builds");

            string cmake_flags = str("");
            each(im->build_args, string, arg) {
                if (cast(cmake_flags, bool))
                    append(cmake_flags, " ");
                append(cmake_flags, arg->chars);
            }

            bool assemble_so = false;
            if (!len(im->links)) { // default to this when initializing
                im->links = array_of(typeid(string), name, null);
                assemble_so = true;
            }
            exts exts = get_exts();
            if (!build_success) {
                string cmake = str(
                    "cmake -S . -DCMAKE_BUILD_TYPE=Release "
                    "-DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON");
                string cmd   = format(
                    "%o -B %o -DCMAKE_INSTALL_PREFIX=%o %o", cmake, b, i, cmake_flags);
                assert (system(cmd->chars) == 0, "cmd failed");
                chdir(b->chars);
                assert (system("make -j16 install") == 0, "install failed");

                each(im->links, string, link_name) {
                    symbol n   = cs(link_name);
                    symbol pre = exts.lib_prefix;
                    symbol ext = exts.shared_ext;
                    path   lib = form(path, "%o/lib/%s%s.%s", i, pre, n, ext);
                    if (!file_exists(lib)) {
                        ext = exts.static_ext;
                        lib = form(path, "%o/lib/%s%s.%s", i, pre, n, ext);
                    }
                    bool exists = file_exists(lib);
                    assert (assemble_so || exists, "lib does not exist");
                    if (exists) {
                        path sym = form(path, "%o/%s%s.%s", i, pre, n, ext);
                        create_symlink(lib, sym);
                        assemble_so = false;
                    }
                }
                /// combine .a into single shared library; assume it will work
                if (assemble_so) {
                    path   dawn_build = new(path, chars, b->chars);
                    array  files      = ls(dawn_build, str(".a"), true);
                    string all        = str("");
                    each (files, path, f) {
                        if (all->len)
                            append(all, " ");
                        append(all, f->chars);
                    }
                    string cmd = format(
                        "gcc -shared -o %o/lib%o.so -Wl,--whole-archive %o -Wl,--no-whole-archive",
                        i, name, all);
                    system(cmd->chars);
                }
                FILE*  silver_token = fopen("silver-token", "w");
                fclose(silver_token);
            }
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
    bool is_debug = im->mod->with_debug;
    string install = im->mod->install;
    each (im->cfiles, string, cfile) {
        path cwd = path_cwd(1024);
        string compile;
        if (call(cfile, has_suffix, ".rs")) {
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
            if (call(i0, has_suffix, str(".c")))   has_c  = true;
            if (call(i0, has_suffix, str(".h")))   has_h  = true;
            if (call(i0, has_suffix, str(".rs")))  has_rs = true;
            if (call(i0, has_suffix, str(".so")))  has_so = true;
            if (call(i0, has_suffix, str(".a")))   has_a  = true;
        }
        if (has_h)
            im->import_type = import_t_source;
        else if (has_c || has_rs) {
            im->import_type = import_t_source;
            icall(build_source);
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
            icall(build_project, im->name, idx(im->source, 0));
            if (!im->library_exports)
                 im->library_exports = array_of(typeid(string), im->name, NULL);
        }
    }
    icall(process_includes, im->includes);
    switch (im->import_type) {
        case import_t_source:
            if (len(im->main_symbol))
                push(mod->main_symbols, im->main_symbol);
            each(im->source, string, source) {
                // these are built as shared library only, or, a header file is included for emitting
                if (call(source, has_suffix, ".rs") || call(source, has_suffix, ".h"))
                    continue;
                string buf = format("%o/%s.o", mod->install, source);
                push(mod->compiled_objects, buf);
            }
        case import_t_library:
        case import_t_project:
            concat(mod->libs_used, im->links);
            break;
        case import_t_includes:
            break;
        default:
            verify(false, "not handled: %i", im->import_type);
    }
}

tokens read_body(silver mod) {
    array body = new(array, alloc, 32);
    verify (tok(next_is, "["), "expected function body");
    int depth  = 0;
    do {
        token   token = tok(next);
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
    return t ? (string)call(t, location) : (string)format("n/a");
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
            if (chr == ':' && idx(input_string, index + 1) == ':') {
                token t = new(token, chars, "::", source, src, line, line_num, column, 0);
                push(tokens, t);
                index += 2;
            } else if (chr == '=' && idx(input_string, index + 1) == '=') {
                push(tokens, new(token, chars, "==", source, src, line, line_num, column, 0));
                index += 2;
            } else {
                push(tokens, new(token, chars, sval, source, src, line, line_num, column, 0));
                index += 1;
            }
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

bool tokens_symbol(tokens a, symbol cs) {
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
    string m = get(assign, k);
    if (m) a->cursor ++;
    return k;
}

string tokens_read_alpha(tokens a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
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
}

void tokens_push_current(tokens a) {
    call(a, push_state, a->tokens, a->cursor);
}

bool tokens_cast_bool(tokens a) {
    return a->cursor < len(a->tokens);
}

node parse_return(silver mod) {
    model rtype = mcall(return_type);
    bool  is_v  = is_void(rtype);
    model ctx = mcall(context_model, typeid(function));
    tok (consume);
    node expr   = is_v ? null : parse_expression(mod);
    return mcall(freturn, expr);
}

node parse_break(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

node parse_for(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

node parse_while(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

node silver_parse_if_else(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

node silver_parse_do_while(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

static node reverse_descent(silver mod) {
    node L = parse_primary(mod);
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        bool  m = true;
        while(m) {
            m = false;
            for (int j = 0; j < 2; j++) {
                string token  = level->token [j];
                if (!tok(symbol, cs(token)))
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
    return L;
}

static node parse_expression(silver mod) {
    mod->expr_level++;
    node vr = reverse_descent(mod);
    mod->expr_level--;
    return vr;
}

/// @brief completely inadequate
model read_model(silver mod, bool is_ref) {
    print_tokens("read-model", mod);
    string name = tok(read_alpha); // Read the type to cast to
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
    verify(tok(symbol, "["), "expected [");
    array shape = new(array, alloc, 32);
    while (true) {
        if (tok(next_is, "]")) break;
        node n = parse_expression(mod);
        /// check to be sure its a literal
        push(shape, n);
    }
    model mdl_wrap = model_alias(mdl_src, null, 0, shape);
    verify(tok(symbol, "]"), "expected ]");
    return mdl_wrap;
}

static function parse_fn(silver mod, model rtype, token name, interface access);

static node parse_function_call(silver mod, node target, member fmem);

static model read_cast(silver mod) {
    tok(push_current);
    print_tokens("read-cast", mod);
    if (!tok(symbol, "[")) {
        tok(pop_state, false);
        return null;
    }
    string alpha = tok(read_alpha);
    if(!alpha) {
        tok(pop_state, false);
        return null;
    }
    member mem = lookup(mod, alpha);
    if (!mem || !mem->is_type || mem->is_func) {
        tok(pop_state, false);
        return null;
    }

    if (tok(symbol, ".")) {
        tok(pop_state, false);
        return null;
    }

    model mdl = mem->mdl;
    if (tok(next_is, "["))
        mdl = parse_wrap(mod, mdl);

    if (!tok(symbol, "]")) {
        fault("expected ] after cast"); /// we should be able to error here unless it leaks into a statement block
        tok(pop_state, false);
        return null;
    }
    print("is_cast");
    tok(pop_state, true); /// we pop state, saving this current
    return mdl; // parse_function_call(mod, target, mem); (we are doing this in parse_statements)
}

/// @brief this reads entire function definition, member we lookup, new member
static member read_member(silver mod) {
    /// we use alloc to obtain a value-ref by means of allocation inside a function body.
    /// if we are not there, then set false in that case.
    /// in that case, the value_ref should be set by the user to R-type or ARG-type
    tokens tk = mod->tokens;
    tok(push_current);
    print_tokens("read-member", mod);
    string alpha = tok(read_alpha);
    if(alpha) {
        model  ctx      = mod->top;
        member target   = null; // member is a node with value-ref (value)
        member mem      = lookup(mod, alpha);
        
        /// attempt to resolve target if there is a chain
        /// silver makes c->like->code a bit safer, and pointers more approachable
        bool safe = false;
        while ((safe = tok(symbol, "->")) || tok(symbol, ".")) {
            target = mem;
            push(mod, mem->mdl);
            string alpha = tok(read_alpha);
            mem = resolve(mem, alpha); /// needs an argument
            verify(alpha, "expected alpha identifier");
            pop(mod);
        }
        
        if (mem && mem->is_func) {
            tok(pop_state, true); /// we pop state, saving this current
            return mem; // parse_function_call(mod, target, mem); (we are doing this in parse_statements)
        }
        if (mem && !mem->is_type) {
            tok(pop_state, true); /// we pop state, saving this current
            return mem; /// baked into mem is read.all.members.in.series <- where series is mem (right gpt?)
        }
        tok(pop_state, false); /// we pop the state to the last push
        tok(push_current);
        /// now we will proceed here for declaration of members
    }
    interface access = interface_undefined;
    for (int m = 1; m < interface_type.member_count; m++) {
        type_member_t* enum_v = &interface_type.members[m];
        if (tok(symbol, enum_v->name)) {
            access = m;
            break;
        }
    }
    bool  is_static = tok(symbol, "static");
    bool  is_ref    = tok(symbol, "ref");
    token n         = tok(peek);
    model mdl       = read_model(mod, is_ref);
    if (!mdl) {
        print("info: could not read type at position %o", tok(location));
        tok(pop_state, false); // we may 'info' here
        return null;
    }
    
    // may be [, or alpha-id  (its an error if its neither)
    if (tok(next_is, "["))
        mdl = parse_wrap(mod, mdl);

    /// read member name
    print_tokens("read-member-name", mod);
    token name = tok(next);
    verify(is_alpha(name), "expected alpha-numeric name");

    /// convert model to function parse_fn takes in rtype and token name
    if (tok(next_is, "[")) {
        mdl = parse_fn(mod, mdl, name, access);
    }
 
    member mem = new(member, mod, mod,
        name, name, mdl, mdl,
        is_static, is_static, access, access);
    mcall(push_member, mem);
    tok(pop_state, true);
    return mem;
}


arguments parse_args(silver mod) {
    verify(tok(symbol, "["), "parse-args: expected [");
    array       args = new(array,     alloc, 32);
    arguments   res  = new(arguments, mod, mod, args, args);
    print_tokens("args", mod);
    if (!tok(next_is, "]")) {
        push(mod, res); // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        while (true) {
            print_tokens("arg", mod);
            member arg = read_member(mod);
            verify (arg,       "member failed to read");
            verify (arg->name, "member name not set");
            push   (args, arg);
            if     (tok(next_is, "]")) break;
            verify (tok(next_is, ","), "expected separator");
            tok    (consume);
        }
        pop(mod);
    }
    tok(consume);
    return res;
}

node parse_statements(silver mod);

/// any-kind-of-object.function[ ... 
///                            ^-- we are here, and the target is given::
/// any-kind-of-object (member; with a pointer-value and mdl-type)
/// so target is a node -- lets refrain from using target across types...
static node parse_function_call(silver mod, node target, member fmem) {
    bool allow_no_paren = mod->expr_level == 1; /// remember this decision? ... var args
    bool expect_end_br  = false;
    function fn        = fmem->mdl;
    verify(isa(fn) == typeid(function), "expected function type");
    int  model_arg_count = len(fn->args);
    
    if (tok(next_is, "[")) {
        tok(consume);
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
            expr = mcall(convert, expr, arg_mdl);
        print("argument %i: %o", arg_index, expr->mdl->name);

        push(values, expr);
        arg_index++;
        if (tok(next_is, ",")) {
            tok(consume);
            continue;
        } else if (tok(next_is, "]")) {
            verify (arg_index >= model_arg_count, "expected %i args", model_arg_count);
            break;
        } else if (arg_index >= model_arg_count)
            break;
    }
    if (expect_end_br) {
        verify(tok(next_is, "]"), "expected ] end of function call");
        tok(consume);
    }
    return mcall(fcall, fmem, target, values);
}

static node parse_primary(silver mod) {
    token t = tok(peek);

    // handle the logical NOT operator (e.g., '!')
    if (tok(symbol, "!") || tok(symbol, "not")) {
        node expr = parse_expression(mod); // Parse the following expression
        return mcall(not, expr);
    }

    // bitwise NOT operator
    if (tok(symbol, "~")) {
        node expr = parse_expression(mod);
        return mcall(bitwise_not, expr);
    }

    // 'typeof' operator
    if (tok(symbol, "typeof")) {
        bool bracket = false;
        if (tok(next_is, "[")) {
            assert(tok(next_is, "["), "Expected '[' after 'typeof'");
            tok(consume); // Consume '['
            bracket = true;
        }
        node expr = parse_expression(mod); // Parse the type expression
        if (bracket) {
            assert(tok(next_is, "]"), "Expected ']' after type expression");
            tok(consume); // Consume ']'
        }
        return expr; // Return the type reference
    }

    // 'cast' operator has a type inside
    model cast_mdl = read_cast(mod);
    if (cast_mdl) {
        node expr = parse_expression(mod); // Parse the expression to cast
        if (is_object(expr)) {
            model method = cast_method(mod, expr->mdl, cast_mdl);
            if (method) {
                // object may cast because there is a method defined with is_cast and an rtype of the cast_ident
                array args = new(array);
                return mcall(fcall, method, expr, args);
            }
        }
        verify (!is_object(expr), "object %o requires a cast method for %o",
            expr->mdl->name, cast_mdl->name);
        /// cast to basic type with convert
        return mcall(convert, expr, cast_mdl);
    }

    // 'ref' operator (reference)
    if (tok(symbol, "ref")) {
        mod->in_ref = true;
        node expr = parse_expression(mod);
        mod->in_ref = false;
        fault("not impl");
        return expr;
    }

    // literal values (int, float, bool, string)
    object n = tok(read_literal);
    if (n)
        return mcall(literal, n);

    // parenthesized expressions
    if (tok(symbol, "[")) {
        node expr = parse_expression(mod); // Parse the expression
        verify(tok(symbol, "]"), "Expected closing parenthesis");
        return expr;
    }
    
    // handle identifiers (variables or function calls)
    member mem = read_member(mod); // we need target on member
    if (mem) {
        if (mem->is_func) {
            if (mod->in_ref) {
                fault("not implemented");
                return null; /// simple stack mechanism we need to support returnin the pointer for (is this a 'load?')
            } else {
                return parse_function_call(mod, mem->target, mem);
            }
        } else
            return mcall(load, mem, null, null);
    }

    fault("unexpected token %o in primary expression", tok(peek));
    return null;
}

node parse_assignment(silver mod, member mem, string op) {
    verify(!mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    string         op_name = get(operators, op);
    member         method = null;
    if (instanceof(mem->mdl, record))
        method = get(((record)mem->mdl)->members, op_name); /// op-name must be reserved for use in functions only
    node           res     = null;
    node           L       = mem;
    node           R       = parse_expression(mod);

    if (method && method->mdl && instanceof(method->mdl, function)) {
        array args = array_of(null, R, null);
        res = mcall(fcall, method, L, args);
    } else {
        mem->is_const = eq(op, "=");
        bool e = mem->is_const;
        if (e || eq(op, ":"))  res = mcall(assign,     L, R); // LLVMBuildStore(B, R, L);
        else if (eq(op, "+=")) res = mcall(assign_add, L, R); // LLVMBuildAdd  (B, R, L, "assign-add");
        else if (eq(op, "-=")) res = mcall(assign_sub, L, R); // LLVMBuildSub  (B, R, L, "assign-sub");
        else if (eq(op, "*=")) res = mcall(assign_mul, L, R); // LLVMBuildMul  (B, R, L, "assign-mul");
        else if (eq(op, "/=")) res = mcall(assign_div, L, R); // LLVMBuildSDiv (B, R, L, "assign-div");
        else if (eq(op, "%=")) res = mcall(assign_mod, L, R); // LLVMBuildSRem (B, R, L, "assign-mod");
        else if (eq(op, "|=")) res = mcall(assign_or,  L, R); // LLVMBuildOr   (B, R, L, "assign-or"); 
        else if (eq(op, "&=")) res = mcall(assign_and, L, R); // LLVMBuildAnd  (B, R, L, "assign-and");
        else if (eq(op, "^=")) res = mcall(assign_xor, L, R); // LLVMBuildXor  (B, R, L, "assign-xor");
        else fault("unsupported operator: %o", op);
    }
    /// update member's value_ref (or not! it was not what we expected in LLVM)
    /// investigate
    ///mem->value = res->value;
    if (eq(op, "=")) mem->is_const = true;
    return mem;
}

/*

ether defines this model:

/// ether knows nothing of tokens, but we are giving the elements back to it for
/// processing, to call ether back with more operations
#define if_context_schema(X,Y) \
    i_prop(X,Y, public,     object,       user_cond) \
    i_prop(X,Y, public,     node,         user_cond_result) \
    i_prop(X,Y, required,   object,       user_expr) \
    i_prop(X,Y, public,     node,         user_expr_result) \
#ifndef op_intern
#define op_intern
#endif
declare_class(if_context)

*/

node cond_builder(silver mod, tokens cond_tokens, object unused) {
    tok(push_state, cond_tokens->tokens, 0);
    node cond_expr = parse_expression(mod);
    verify(cond_tokens->cursor == len(cond_tokens->tokens), 
        "unexpected trailing expression in single expression condition");
    tok(pop_state, false);
    return cond_expr;
}

node expr_builder(silver mod, tokens expr_tokens, object unused) {
    tok(push_state, expr_tokens->tokens, 0);
    node exprs = parse_statements(mod);
    verify(expr_tokens->cursor == len(expr_tokens->tokens), 
        "error: statements not fully processed");
    tok(pop_state, false);
    return exprs;
}

/// parses entire chain of if, [else-if, ...] [else]
node parse_if_else(silver mod) {
    bool  require_if   = true;
    array tokens_cond  = new(array, alloc, 32);
    array tokens_block = new(array, alloc, 32);
    while (true) {
        bool is_if  = tok(symbol, "if");
        verify(is_if && require_if || !require_if, "expected if");
        tokens cond  = is_if ? read_body(mod) : null;
        tokens block = read_body(mod);
        push(tokens_cond,  cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = tok(symbol, "else");
        require_if = false;
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    /// we call this way to manage the LLVM-IR control-flow
    /// while it would be nicest to parse and place in blocks after, LLVM does not work that way
    /// we must read-ahead
    /// the technical issue with this one is how to parse non [ block ] single expressions
    /// seems like a lot of parallel code -- perhaps we may perform this in parallel and obtain the tokens
    /// ether could be 'muted' or something weird like that
    /// that is we must read-ahead to know where expressions end, it can be a: [method-call(a,b) + 2 / 2] ? 1 : 2 / 2
    /// i think ether must be made to be silent and return placeholder node for this
    /// we must be careful not to register member changes such as assignment (not allowed in this level anyway!)
    /// this is ONLY for single-expression when we dont see a [.. if we see one of those we need not do this
    return ether_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}

node parse_do_while(silver mod) {
    tok(consume);
    node vr = null;
    return null;
}

node parse_statement(silver mod) {
    token t = tok(peek);
    if (tok(next_is, "return")) return parse_return(mod);
    if (tok(next_is, "break"))  return parse_break(mod);
    if (tok(next_is, "for"))    return parse_for(mod);
    if (tok(next_is, "while"))  return parse_while(mod);
    if (tok(next_is, "if"))     return parse_if_else(mod);
    if (tok(next_is, "do"))     return parse_do_while(mod);

    member mem  = read_member(mod); // we store target on member
    if (mem) {
        print("%s member: %o %o", (mem->is_assigned || mem->is_func) ?
            "existing" : "new", mem->mdl->name, mem->name);
        /// mem (multi instanced, we arent looking it up here...)
        /// to do that we need to be in member mode
        /// todo: must support member chain with . ... code is in EIdent silver44
        if (instanceof(mem->mdl, function)) {
            return parse_function_call(mod, null, mem);
        } else {
            string assign = tok(read_assign);
            if    (assign) return parse_assignment(mod, mem, assign);
        }
    }
    fault ("implement"); /// implement as we need them
    return null;
}

node parse_statements(silver mod) {
    push(mod, new(statements, mod, mod));

    print_tokens("statements", mod);
    model is_cast = read_cast(mod);
    verify (!is_cast, "unexpected cast at statements level");

    bool multiple   = tok(next_is, "[");
    if  (multiple)    tok(consume);
    int  depth      = 1;
    node vr = null;
    ///
    while(tok(cast_bool)) {
        if(multiple && tok(symbol, "[")) {
            depth += 1;
            push(mod, new(statements, mod, mod));
        }
        print("next statement origin: %o", tok(peek));
        vr = parse_statement(mod);
        if(!multiple) break;
        if(tok(next_is, "]")) {
            if (depth > 1)
                pop(mod);
            tok(next);
            if ((depth -= 1) == 0) break;
        }
    }
    pop(mod);
    return vr;
}

void build_function(silver mod, object arg, function fn) {
    tokens body = instanceof(fn->user, tokens);
    if (!call(fn, has_scope)) {
        int test = 1;
        test++;
    }
    //call(fn, finalize);
    print("building function: %o", fn->name);
    tok(push_state, body->tokens, 0);
    print_tokens("body-tokens", mod);
    parse_statements(mod);
    tok(pop_state, false);
}

function parse_fn(silver mod, model rtype, token name, interface access) {
    verify (tok(next_is, "["), "expected function args");
    array           m       = new       (array, alloc, 32);
    arguments       args    = parse_args(mod);
    subprocedure    process = subproc   (mod, build_function, null);
    record          rec_top = instanceof(mod->top, record) ? mod->top : null;
    function        fn      = new       (function,
        mod,    mod,     name,   name,
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
    bool   is_class = inherits(rec, class);
    symbol sname    = is_class ? "class" : "struct";
    print("building %s: %o", sname, rec->name);
    tokens body     = instanceof(rec->user, tokens);
    tok(push_state, body->tokens, 0);
    verify(tok(symbol, "["), "expected [");

    push(mod, rec);
    while (tok(cast_bool)) {
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

    verify(tok(symbol, "]"), "expected ]");
    tok(pop_state, false);
    //mcall(compile, rec);
}

record parse_record(silver mod, bool is_class) {
    verify(( is_class && tok(symbol, "class")) || 
           (!is_class && tok(symbol, "struct")),
        "expected record type");
    token name  = tok(read_alpha);
    verify(name, "expected alpha-numeric class identifier");
    token parent_name = null;
    if (tok(next_is, ":")) {
        tok(consume);
        parent_name = tok(read_alpha);
        verify(parent_name, "expected alpha-numeric parent identifier");
    }
    verify (tok(next_is, "[") , "expected function body");
    tokens body = read_body(mod);

    subprocedure process = subproc(mod, build_record, null);
    record rec;
    if (is_class)
        rec = new(class,     mod, mod, name, name, process, process);
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
    while (tok(cast_bool)) {
        if (tok(next_is, "import")) {
            import im = new(import, mod, mod, tokens, mod->tokens);
            push(mod->imports, im);
        } else if (tok(next_is, "class") || tok(next_is, "struct")) {
            bool  is_class = tok(next_is, "class");
            record rec = parse_record(mod, is_class);
            /// with the same name, we effectively change the model of membership, and what inlay and ref/ptr do
            model  access_model = call(rec, alias, str(rec->name->chars), reference_pointer, null);
            member mem = new(member, mod, mod, name, rec->name, mdl, access_model, is_type, true);
            string key = cast(rec->name, string);
            mcall(push_member, mem);
        } else {
            /// must make read-ahead mechanism for reading functions
            print_tokens("top-member", mod);
            member mem = read_member(mod);
            string key = str(mem->name->chars);
            mcall(push_member, mem);
        }
    }

    /// finalize imported C functions first
    pairs(mod->members, i) {
        member mem = i->value;
        model  mdl = mem->mdl;
        if (instanceof(mem->mdl, function) && mdl->from_include) {
            call(mem->mdl, process_finalize);
        }
    }

    /// process/finalize all remaining member models 
    /// calls process sub-procedure and poly-based finalize
    pairs(mod->members, i) {
        member mem = i->value;
        model base = mem->mdl->ref ? mem->mdl->src : mem->mdl;
        if (instanceof(base, record))
            call(base, process_finalize);
    }

    pairs(mod->members, i) {
        member mem = i->value;
        if (instanceof(mem->mdl, function))
            call(mem->mdl, process_finalize);
    }
}

void silver_init(silver mod) {
    verify(exists(mod->source), "source (%o) does not exist", mod->source);
    
    mod->imports   = new(array, alloc, 32);
    mod->libs_used = new(array);
    mod->tokens    = new(tokens, file, mod->source);

    parse(mod);
    build(mod);
    write(mod);
}

int main(int argc, char **argv) {
    A_start();

    AF         pool = allocate(AF);
    cstr        src = getenv("SRC");
    cstr     import = getenv("SILVER_IMPORT");
    map    defaults = map_of(
        "module",  str(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."),
        null);

    string ikey     = str("install");
    string mkey     = str("module");
    map    args     = A_args(argc, argv, defaults, mkey);
    print("args = %o", args);

    string name     = get(args, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);
    silver mod      = new(silver,
        source,  source,
        install, get(args, ikey),
        name,    stem(source));
    drop(pool);
}

define_class (tokens)
define_mod   (silver, ether)
define_enum  (import_t)
define_enum  (build_state)
define_class (import)
module_init  (init)