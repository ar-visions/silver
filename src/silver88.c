#define import_intern intern(import)
#define silver_intern intern(silver)
#define tokens_intern intern(tokens)
#include <silver>
#include <ether>
#define   icall(M,...)   import_ ## M(im, ## __VA_ARGS__)
#define  string(...)     new(string, ## __VA_ARGS__) 
#define isilver(M,...)   silver_##M(mod,    ## __VA_ARGS__)
#undef  peek
#define tok(...)     call(mod->tokens, __VA_ARGS__)

static array operators;
static array keywords;
static array consumables;
static map   assign;
static array compare;

bool is_alpha(A any);

static void silver_print_tokens(silver mod) {
    print("tokens: %o %o %o %o %o ...", 
        element(mod->tokens, 0), element(mod->tokens, 1),
        element(mod->tokens, 2), element(mod->tokens, 3),
        element(mod->tokens, 4), element(mod->tokens, 5));
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
        "inherits", str("inherits"), null
    );
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
    path i        = create_folder(im->mod, im->mod->debug ? "debug" : "install", null);
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
    bool is_debug = im->mod->debug;
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
            concat(mod->libraries_used, im->links);
            break;
        case import_t_includes:
            break;
        default:
            verify(false, "not handled: %i", im->import_type);
    }
}

AType tokens_isa(tokens a) {
    token  t = idx(a->tokens, 0);
    return isa(t->literal);
}

num tokens_line(tokens a) {
    token  t = idx(a->tokens, 0);
    return t->line;
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

/// tokens
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
    return a->tokens->elements[clamp(a->cursor + rel, 0, a->tokens->len)];
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



void silver_init(silver mod) {
    mod->name = stem(mod->source);
    ether e = new(ether, source, mod->source, lang, str("silver"), name, mod->name);
    /// ether interface:
    ///     node operations which create the llvm at call time
    ///     push/pop for members (with functions creating their own)

    /// silvers mission: parse tokens, reverse descent, manage calls to ether
}

void silver_write(silver mod) {
    write(mod->e);
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
    map    args     = A_args(argc, argv, defaults, ikey); print("args = %o", args);
    path   install  = get(args, ikey);
    string mkey     = str("module");
    string name     = get(args, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);

    verify (exists(source), "source %o does not exist", n);
    silver mod = new(silver, source, source, install, install);
    write(mod);

    drop(pool);
}


define_class(tokens)
define_class(silver)
define_enum(import_t)
define_enum(build_state)
define_class(import)

module_init(init)