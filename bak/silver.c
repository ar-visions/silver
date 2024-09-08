#include <silver.h>

static array    keywords;
static module_t root; /// this we populate in code and not from the data, since we are the versioned compiler
static bool     is_debug;
static path     build_root;

static path install_dir() {
    char install_dir[256];
    snprintf(install_dir, sizeof(install_dir), "%s/install", build_root->chars);
    path i = construct(path, cstr, install_dir, -1);
    M(path, make_dir, i);
    return i;
}

static bool has_suffix(cstr str, cstr suffix) {
    int len  = strlen(str);
    int slen = strlen(suffix);
    return strcmp(&str[len - slen], suffix) == 0;
}

/// what directory are we in?  do we have relative access now?  yes
static BuildState import_t_build_source(import_t import, string name, array cfiles) {
    path install = install_dir();
    each(array, cfiles, string, cfile) {
        char compile[1024];
        path cwd = path_type.cwd(1024);
        if (has_suffix(cfile->chars, ".rs")) {
            /// rustc integrated for static libs only in this use-case
            snprintf(compile, sizeof(compile),
                "rustc --crate-type=staticlib -C opt-level=%s %s/%s --out-dir %s",
                is_debug ? "0" : "3",
                cwd->chars, cfile->chars,
                build_root->chars);
            /// needs to know the product of the build here
        } else {
            cstr opt = is_debug ? "-g2" : "-O2";
            snprintf(compile, sizeof(compile),
                "gcc -I%s/include %s -Wfatal-errors -Wno-write-strings -Wno-incompatible-pointer-types -fPIC -std=c99 -c %s/%s -o %s/%s.o",
                build_root->chars,
                opt,
                cwd->chars, cfile->chars,
                build_root->chars, cfile->chars);
        }

        printf("%s > %s\n", cwd->chars, compile);
        assert(system(compile) == 0);
        drop(cwd);
    }
    drop(install);
    return BuildState_built;
}

static void create_symlink(const char *target, const char *linkpath) {
    // Remove the existing symbolic link if it exists
    if (unlink(linkpath) == -1)
        perror("Error deleting previous symlink (if it existed)");

    // Create a new symbolic link
    if (symlink(target, linkpath) == -1)
        perror("Error creating symlink");
    else
        printf("Symlink created successfully: %s -> %s\n", linkpath, target);
}

/// we checkout, [generate-and] build here,
static BuildState import_t_build_project(import_t import, string name, string url) {

    char checkout_dir[128];
    snprintf(checkout_dir, sizeof(checkout_dir),
        "%s/checkouts/%s", build_root->chars, name->chars);
    path s = construct(path, cstr, checkout_dir, -1);
    M(path, make_dir, s);

    /// our install folder contains all libs we've built in the 
    /// build folder along with their resources they install
    path i = install_dir();

    /// build directory
    char build_dir[256];
    snprintf(build_dir, sizeof(build_dir), "%s/silver-build", checkout_dir);
    path b = construct(path, cstr, build_dir, -1);
    
    /// clone the project
    char cmd[1024];
    if (M(path, is_empty, s)) {
        cstr find = strstr(url->chars, "@");
        cstr branch = null;
        char s_url[256];
        if (find > url->chars) {
            sz dist = (sz)find - (sz)url->chars;
            memcpy(s_url, url->chars, dist);
            s_url[dist] = 0;
            branch = find + 1;
        } else
            strcpy(s_url, url->chars);
        
        snprintf(cmd, sizeof(cmd), "git clone %s %s", s_url, checkout_dir);
        assertion(null, system(cmd) == 0, "git clone failure");
        /// checkout a branch
        if (branch) {
            assert(chdir(checkout_dir) == 0);
            snprintf(cmd, sizeof(cmd), "git checkout %s", branch);
            assertion(null, system(cmd) == 0, "git checkout failure");
        }

        /// create build directory
        M(path, make_dir, b);
    }
    if (!M(path, is_empty, s)) {
        /// for now we depend on a CMakeList.txt
        /// can also support meson and the others and simply switch based on an order
        /// theres a basic enumerable we may define, associated to import
        assert(chdir(s->chars) == 0);
        bool is_rust = file_exists("Cargo.toml");
        if (is_rust) {
            char env[1024];
            cstr rel_or_debug = "release";
            char package_dir[1024];
            snprintf(package_dir, sizeof(package_dir), "%s/rust/%s", i->chars, name->chars);
            path package = construct(path, cstr, package_dir, -1);
            //M(path, make_dir, package);

            snprintf(env, sizeof(env), "RUSTFLAGS=-C save-temps");
            putenv(env);
            snprintf(env, sizeof(env), "CARGO_TARGET_DIR=%s", package->chars);
            putenv(env);
            snprintf(cmd, sizeof(cmd), "cargo build -p %s --%s", name->chars, rel_or_debug);
            assert(system(cmd) == 0);

            /// now we must copy the lib to the actual lib folder
            char lib[2048];
            snprintf(lib, sizeof(lib), "%s/%s/lib%s.so", package_dir, rel_or_debug, name->chars);
            
            char exe[2048];
            snprintf(exe, sizeof(exe), "%s/%s/%s_bin", package_dir, rel_or_debug, name->chars);
            if (!file_exists(exe))
                snprintf(exe, sizeof(exe), "%s/%s/%s", package_dir, rel_or_debug, name->chars);
            
            
            /// create symlink for lib
            if (file_exists(lib)) {
                char sym[2048];
                snprintf(sym, sizeof(sym), "%s/lib%s.so", i->chars, name->chars);
                create_symlink(lib, sym);
            }

            /// create symlink for exec
            /// (we probably want a flat install dir for silver-build)
            if (file_exists(exe)) {
                char sym[2048];
                snprintf(sym, sizeof(sym), "%s/%s", i->chars, name->chars);
                create_symlink(exe, sym);
            }
            drop(package);
            /// cargo build -p with_winit
        } else {
            assertion(null, file_exists("CMakeLists.txt"), "CMake required for project builds");
            
            snprintf(cmd, sizeof(cmd), "cmake -S . -DCMAKE_BUILD_TYPE=Release -DDBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -B %s -DCMAKE_INSTALL_PREFIX=%s", b->chars, i->chars);
            printf("> %s\n", cmd);
            assert(system(cmd) == 0);
            
            /// make and install into our install directory (inside build-root)
            assert(chdir(b->chars) == 0);
            snprintf(cmd, sizeof(cmd), "make install");
            printf("> %s\n", cmd);
            assert(system(cmd) == 0);

            /// todo: support multiple targets (name will be the first i suppose if we keep that)
            /// for each lib target (also app if we are packaging that too)
            char lib[2048];
            snprintf(lib, sizeof(lib), "%s/lib/lib%s.so", i->chars, name->chars);
            char* ext = "so";
            if (!file_exists(lib)) {
                snprintf(lib, sizeof(lib), "%s/lib/lib%s.a", i->chars, name->chars);
                ext = "a";
            }
            assert(file_exists(lib));
            
            char sym[2048];
            snprintf(sym, sizeof(sym), "%s/lib%s.%s", i->chars, name->chars, ext);
            create_symlink(lib, sym);
        }
    }
    drop(i);
    return BuildState_built;
}

static cstr ident_cast_cstr(ident token) {
    return (token && token->value) ? token->value->chars : null;
}

static EType ident_is_alpha(ident a) {
    char* t = a->value->chars;
    if (isalpha(*t) && call(keywords, index_of, a->value) == -1)
        return EType_AlphaIdent;
    return EType_Undefined;
}

static EType ident_is_string(ident a) {
    char* t = a->value->chars;
    return t[0] == '"' ? EType_LiteralStr : t[0] == '\'' ? EType_LiteralStrInterp : EType_Undefined;
}

static EType ident_is_numeric(ident a) {
    char* t = a->value->chars; /// even a null string can get valid pointer; a pointer to the length which is set to 0 is a null string
    return (t[0] >= '0' && t[0] <= '9') ? (strchr(t, '.') ? 
        EType_LiteralReal : EType_LiteralInt) : EType_Undefined;
}

static num ident_compare(ident a, ident b) {
    return M(string, compare, a->value, b->value);
}

static u64 ident_hash(ident a) {
    if (!a->h) {
        u64 h = OFFSET_BASIS;
            h *= FNV_PRIME;
            h ^= M(string, hash, a->value);
        a->h = h;
    }
    return a->h;
}

static array ident_split_members(ident a, cstr sp) {
    if (a->members_cache)
        drop(a->members_cache);
    a->members_cache = M(string, split, a->value, sp);
    return a->members_cache;
}

ident ident_with_cstr(ident a, cstr token, num sz) {
    a->value = construct(string, cstr, token, strlen(token));
    return a;
}

bool enode_cast_bool(enode a) {
    return a && a->etype > EType_Undefined;
}

enode enode_create_operation(EType etype, array operands) { // array references ?
    enode a = new(enode);
    a->etype = etype;
    a->operands = hold(operands);
    //a->references = hold(references);
    return a;
}

enode enode_create_value(EType etype, A value) {
    enode a = new(enode);
    a->etype = etype;
    a->value = hold(value);
    return null;
}

enode enode_method_call(EType etype, ident method, array args) {
    return null;
}

A enode_lookup(array stack, ident id, bool top_only) {
    return null;
}

string enode_string_interpolate(A input, array stack) {
    return null;
}

A enode_exec(enode op, array stack) {
    return null;
}

bool enode_boolean(enode a) {
    return a->etype != EType_Undefined;
}

cstr chars(string s) { return s ? s->chars : null; }

static ident parse_token(cstr start, num len, path fname, int line_num) {
    while (start[len - 1] == '\t' || start[len - 1] == ' ')
        len--;
    string all = construct(string, cstr, start, len);
    ident res = construct(ident, cstr, chars(all), -1);
    res->fname = fname;
    res->line_num = line_num; /// mr b: im a token!  line-num can be used for breakpoints (need the file path too)
    return res;
}

static void ws(char **cur) {
    while (**cur == ' ' || **cur == '\t') {
        ++(*cur);
    }
}

void assertion(Parser parser, bool is_true, cstr message, ...) {
    if (!is_true) {
        char buffer[1024];
        va_list args;
        va_start(args, message);
        vsprintf(buffer, message, args);
        va_end(args);
        printf("%s\n", buffer);
        exit(-1);
    }
}

ident next(Parser parser) {
    return M(Parser, next, parser);
}

ident next_n(Parser parser, num rel) {
    return M(Parser, relative, parser, rel);
}

bool next_is(Parser parser, cstr token) {
    ident id = M(Parser, next, parser);
    return strcmp(id->value->chars, token) == 0;
}

bool ident_is(ident i, cstr str) {
    return strcmp(i->value->chars, str) == 0;
}

bool pop_is(Parser parser, cstr token) {
    ident id = M(Parser, pop, parser);
    return strcmp(id->value->chars, token) == 0;
}

ident pop(Parser parser) {
    return M(Parser, pop, parser);
}

EType is_string(ident i) {
    return M(ident, is_string, i);
}

EType is_numeric(ident i) {
    return M(ident, is_numeric, i);
}

void consume(Parser parser) {
    M(Parser, consume, parser);
}

EType is_alpha(ident i) {
    return M(ident, is_alpha, i);
}

string ident_string(ident i) {
    return i->value;
}

bool ident_equals(ident a, ident b) {
    return strcmp(a->value->chars, b->value->chars) == 0;
}

bool file_exists(cstr filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true; // File exists
    }
    return false; // File does not exist
}

static silver_t forge_type(module_t module, array type_tokens);

define_t define_t_with_Parser(define_t import, Parser parser) {
    assert(false);
    return null;
}

none define_t_read_members(define_t a) { }

none define_t_resolve_members(define_t a) { } /// this should output c99

import_t import_t_with_Parser(import_t import, Parser parser) {
    assertion(parser, pop_is(parser, "import"), "expected import");
    if (next_is(parser, "[")) {
        pop(parser);
        for (;;) {
            if (next_is(parser, "]")) {
                pop(parser);
                break;
            }
            ident arg_name = pop(parser);
            if (is_string(arg_name)) {
                import->source = new(array);
                M(array, push, import->source, ident_string(arg_name));
            } else {
                assertion(parser, is_alpha(arg_name), "expected identifier for import arg");
                assertion(parser, pop_is(parser, ":"), "expected : after import arg (argument assignment)");
                if (ident_is(arg_name, "name")) {
                    ident token_name = pop(parser);
                    assertion(parser, !is_string(token_name), "expected token for import name");
                    import->name = ident_string(token_name);
                } else if (ident_is(arg_name, "links")) {
                    assertion(parser, pop_is(parser, "["), "expected array for library links");
                    for (;;) {
                        ident link = pop(parser);
                        
                        if (ident_is(link, "]")) break;
                        assertion(parser, is_string(link), "expected library link string");

                        call(import->links, push, ident_string(link));
                        if (next_is(parser, ",")) {
                            pop(parser);
                            continue;
                        } else {
                            assertion(parser, pop_is(parser, "]"), "expected ] in includes");
                            break;
                        }
                    }
                } else if (ident_is(arg_name, "includes")) {
                    assertion(parser, pop_is(parser, "["), "expected array for includes");
                    for (;;) {
                        ident include = pop(parser);
                        if (ident_is(include, "]")) break;
                        assertion(parser, is_string(include), "expected include string");
                        call(import->includes, push, ident_string(include));
                        if (next_is(parser, ",")) {
                            pop(parser);
                            continue;
                        } else {
                            assertion(parser, pop_is(parser, "]"), "expected ] in includes");
                            break;
                        }
                    }
                } else if (ident_is(arg_name, "source")) {
                    ident token_source = pop(parser);
                    assertion(parser, is_string(token_source), "expected quoted url for import source");
                    import->source = new(array);
                    M(array, push, import->source, ident_string(token_source));
                } else if (ident_is(arg_name, "shell")) {
                    ident token_shell = pop(parser);
                    assertion(parser, is_string(token_shell), "expected shell invocation for building");
                    import->shell = ident_string(token_shell);
                } else if (ident_is(arg_name, "defines")) {
                    // none is a decent name for null.
                    assertion(parser, false, "not implemented");
                } else {
                    assertion(parser, false, "unknown arg: %s", arg_name->value->chars);
                }

                if (next_is(parser, ","))
                    pop(parser);
                else {
                    assertion(parser, pop_is(parser, "]"), "expected ] after end of args, with comma inbetween");
                    break;
                }
            }
        }
    } else {
        ident module_name = pop(parser);
        ident as = next(parser);
        if (ident_is(as, "as")) {
            consume(parser);
            import->isolate_namespace = ident_string(pop(parser)); /// inlay overrides this -- it would have to; modules decide if their types are that important
        }
        /// if its a location to a .c file (source) or url (project)
        //assertion(parser.is_string(mod), "expected type identifier, found {0}", { type });
        assertion(parser, is_alpha(module_name), "expected variable identifier, found %s", module_name->value->chars);
        import->name = hold(module_name->value);

        /// No Interpolation on this string!
        if (ident_is(as, "[")) {
            pop(parser);
            import->source = new(array);
            for (;;) {
                ident inner = pop(parser);
                assert(is_string(inner));
                string source = construct(string, cstr, &inner->value->chars[1], inner->value->len - 2);
                M(array, push, import->source, source);
                ident e = pop(parser);
                if (ident_is(e, ","))
                    continue;
                assert(ident_is(e, "]"));
                break;
            }
        }

    }
    return import;
}

struct_t struct_t_with_Parser(struct_t a, Parser parser) {
    assert(false);
    return null;
}

/// class_t with AType for storage; this is for primitive registration
class_t class_t_with_AType(class_t a, AType atype) {
    a->membership    = EMembership_normal;
    a->keyword       = construct(string, cstr, "class", -1);
    a->atype         = atype;
    a->name          = construct(string, cstr, atype->name, -1);
    return a;
}

/// this constructor overrides define_t
/// even when you dont override constructor, you are still given the type of class you new' with
class_t class_t_with_Parser(
        class_t cl, Parser parser) {
    cl->membership    = parser->membership;
    cl->keyword       = hold(parser->keyword);
    cl->members       = new(array);
    cl->tokens        = new(array);
    cl->meta_symbols  = hold(parser->meta_symbols);
    /// parse class members
    ident ikeyword    = construct(ident, cstr, chars(cl->keyword), -1);
    ident inext       = call(parser, pop);
    bool  cmp         = call(inext, compare, ikeyword);
    assertion(parser, cmp == 0, "expected %s", cl->keyword->chars);
    assertion(parser, is_alpha(next(parser)), "expected class identifier");
    ident iname = pop(parser);
    cl->name = ident_string(iname);
    if (next_is(parser, ":")) {
        consume(parser);
        cl->from = ident_string(pop(parser));
    }
    ident start = pop(parser);
    assertion(parser, ident_is(start, "["), "expected beginning of class");
    
    /// do not process contents yet
    M(array, push, cl->tokens, hold(start));
    int level = 1;
    for (;;) {
        ident t = pop(parser);
        if (!t->value) {
            assertion(parser, false, "expected ]");
            break;
        }
        M(array, push, cl->tokens, hold(t));
        if (ident_is(t, "[")) {
            level++;
        } else if (ident_is(t, "]")) {
            level--;
            if (level == 0) {
                break;
            }
        }
    }
    return cl;
}

/// we read members once other identies are registered
/// we need a read, then resolve per member
/// resolve will infact create C99 code

none class_t_read_members(class_t cl) {
    Parser parser = construct(Parser, array, cl->tokens, cl->module->module_name, cl->module);
    /// we parse the tokens afterwards with another Parser instance
    assertion(parser, ident_is(pop(parser), "["), "expected [ after class");
    for (;;) {
        ident t = next(parser);
        if (!t || ident_is(t, "]"))
            break;
        member_def mlast = null;
        call(cl->members, push, M(Parser, read_member, parser, cl, mlast));
        mlast = call(cl->members, last);
        for (;;) {
            if (!next_is(parser, ","))
                break;
            pop(parser);
            call(cl->members, push, M(Parser, read_member, parser, cl, mlast));
            mlast = call(cl->members, last);
        }
    }
    ident n = pop(parser);
    assertion(parser, ident_is(n, "]"), "expected end of class");
    drop(parser);
}

///
none class_t_resolve_members(class_t cl) {

}

/// take these args and shove them in Parser state
enum_t enum_t_with_Parser(enum_t a, Parser parser) {
    ident token_name = pop(parser);
    assertion(parser, is_alpha(token_name),
        "expected qualified name for enum, found {0}",
        token_name);
    a->name = token_name;
    assertion(parser, ident_is(pop(parser), "["), "expected [ in enum statement");
    i64  prev_value = 0;
    for (;;) {
        ident symbol = pop(parser);
        if (ident_is(symbol, "]"))
            break;
        assertion(parser, is_alpha(symbol),
            "expected identifier in enum, found %s", symbol->value->chars);
        ident peek = next(parser);
        if (ident_is(peek, ":")) {
            pop(parser);
            enode enum_expr = M(Parser, parse_expression, parser);
            A enum_value = M(enode, exec, enum_expr, null);
            assertion(parser, typeid(enum_value)->traits & A_TRAIT_INTEGRAL,
                "expected integer value for enum symbol %s, found %i", symbol->value->chars, *(i32*)enum_value);
            prev_value = *(i32*)enum_value;
            assertion(parser, prev_value >= INT32_MIN && prev_value <= INT32_MAX,
                "integer out of range in enum %s for symbol %s (%i)",
                    a->name->chars, symbol->value->chars, (i32)prev_value);
        } else {
            prev_value += 1;
        }
        A f = construct(item, symbol, symbol->value->chars, A_i32(prev_value));
        operator(a->symbols, assign_add, f);
        drop(f);

    }
    return a;
}

/// parse tokens from string, referenced Parser in C++
static array parse_tokens(string input, path fname) {
    string        sp         = construct(string, cstr, "$,<>()![]/+*:\"\'#", -1); /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
    char          until      = 0; /// either ) for $(script) ", ', f or i
    //num           len        = input->len;
    char*         origin     = input->chars;
    char*         start      = 0;
    char*         cur        = origin - 1;
    int           line_num   = 1;
    bool          new_line   = true;
    bool          token_type = false;
    bool          found_null = false;
    bool          multi_comment = false;
    array         tokens     = new(array);
    ///
    while (*(++cur)) {
        bool is_ws = false;
        if (!until) {
            if (new_line)
                new_line = false;
            /// ws does not work with new lines
            if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
                is_ws = true;
                ws(&cur);
            }
        }
        if (!*cur) break;
        bool add_str = false;
        char *rel = cur;
        if (*cur == '#') { // comment
            if (cur[1] == '#')
                multi_comment = !multi_comment;
            while (*cur && *cur != '\n')
                cur++;
            found_null = !*cur;
            new_line = true;
            until = 0; // requires further processing if not 0
        }
        if (until) {
            if (*cur == until && *(cur - 1) != '/') {
                add_str = true;
                until = 0;
                cur++;
                rel = cur;
            }
        }// else if (cur[0] == ':' && cur[1] == ':') { /// :: is a single token
        //    rel = ++cur;
        //}
        if (!until && !multi_comment) {
            char ch[2] = { cur[0], 0 };
            int type = M(string, index_of, sp, ch);
            new_line |= *cur == '\n';

            if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                ident token = parse_token(start, (size_t)(rel - start), fname, line_num);
                M(array, push, tokens, token);
                if (!add_str) {
                    if (*cur == '$' && *(cur + 1) == '(') // shell
                        until = ')';
                    else if (*cur == '"') // double-quote
                        until = '"';
                    else if (*cur == '\'') // single-quote
                        until = '\'';
                }
                if (new_line) {
                    start = null;
                } else {
                    ws(&cur);
                    start = cur;
                    if (start[0] == ':' && start[1] == ':') /// double :: is a single token
                        cur++;
                    token_type = (type >= 0);
                }
            }
            else if (!start && !new_line) {
                start = cur;
                token_type = (type >= 0);
            }
        }
        if (new_line) {
            until = 0;
            line_num++;
            if (found_null)
                break;
        }
    }
    if (start && (cur - start)) {
        ident token = parse_token(start, cur - start, fname, line_num);
        M(array, push, tokens, token);
    }
    drop(sp);
    return tokens;
}

static Parser  Parser_with_array(Parser a, array tokens, path fname, module_t module) {
    a->fname = hold(fname);
    a->tokens = hold(tokens);
    a->module = module; // weak-ref
    return a;
}

static ident  Parser_token_at(Parser a, num r) {
    return a->tokens->elements[a->cur + r];
}

static ident  Parser_next(Parser a) {
    return M(Parser, token_at, a, 0);
}

static ident  Parser_pop(Parser a) {
    if (a->cur < a->tokens->len)
        a->cur++;
    return a->tokens->elements[a->cur - 1];
}

static num    Parser_consume(Parser a) {
    M(Parser, pop, a);
    return a->cur;
}

static EType  Parser_expect(Parser a, ident token, array tokens) {
    return M(array, index_of, tokens, token);
}

static ident  Parser_relative(Parser a, num pos) {
    return a->tokens->elements[a->cur + pos];
}

static array assign;

static EType  Parser_is_assign(Parser a, ident token) {
    num id = M(array, index_of, assign, token);
    return (id >= 0) ? EType_Assign : EType_Undefined;
}

static enode  Parser_parse_statements(Parser parser) {
    array block = new(array); /// array of enode
    bool multiple = ident_is(next(parser), "[");
    if (multiple) pop(parser);
    /// references here (was bind-vars)
    // make sure we need this for c99 only
    //references refs;
    //call(back_stack, push, vars);
    while (next(parser)) {
        enode n = call(parser, parse_statement);
        assertion(parser, cast(n, bool), "expected statement or expression");
        call(block, push, n);
        if (!multiple)
            break;
        else if (ident_is(next(parser), "]"))
            break;
    }
    //call(back_stack, pop);
    return enode_type.create_operation(EType_Statements, block);
}

static enode  Parser_parse_add(Parser parser) {
    enode left = call(parser, parse_mult);
    while (ident_is(next(parser), "+") || ident_is(next(parser), "/")) {
        EType etype = ident_is(next(parser), "+") ? EType_Add : EType_Sub;
        pop(parser);
        enode right = call(parser, parse_mult);
        array operands = array_type.of_objects(null, left, right, null);
        left = enode_type.create_operation(etype, operands);
    }
    return left;
}

/// silver_t = handle
static handle Parser_parse_type(Parser parser) {
    array type_tokens = call(parser, read_type);
    return forge_type(parser->module, type_tokens);
}

static enode  Parser_parse_expression(Parser parser) {
    return Parser_parse_add(parser);
}

static enode  Parser_parse_statement(Parser parser) {
    ident t0 = next(parser);
    if (is_alpha(t0)) {
        pop(parser);
        silver_t type   = call(parser, parse_type);
        ident    t1     = next(parser);
        EType    assign = call(parser, is_assign, t1); // Type var: 2  when t0 == Type, this must be true!
        if (assign) {
            pop(parser);
            pop(parser);
            array operands = array_type.of_objects(null, t0, call(parser, parse_expression), null);
            return enode_type.create_operation(assign, operands);
        } else if (ident_is(t1, "[")) {
            /// method call / array lookup
            /// determine if its a method
            bool is_static;
            //type = lookup_type(t0, is_static);
            //vspaces
        }
        return call(parser, parse_expression);
    } else if (ident_is(t0, "return")) {
        pop(parser);
        enode result = call(parser, parse_expression);
        array operands = array_type.of_objects(null, result, null);
        call(operands, push, result);
        return enode_type.create_operation(EType_MethodReturn, operands);
    } else if (ident_is(t0, "break")) {
        pop(parser);
        enode levels;
        if (ident_is(next(parser), "[")) {
            pop(parser);
            levels = call(parser, parse_expression);
            assertion(parser, ident_is(pop(parser), "]"), "expected ] after break[expression...");
        }
        array operands = array_type.of_objects(null, levels, null);
        return enode_type.create_operation(EType_Break, operands);
    } else if (ident_is(t0, "for")) {
        pop(parser);
        assertion(parser, ident_is(next(parser), "["), "expected condition expression '['"); pop(parser);
        enode statement = call(parser, parse_statements);
        assertion(parser, ident_is(next(parser), ";"), "expected ;"); pop(parser);
        //bind_stack->push(statement->vars);
        enode condition = call(parser, parse_expression);
        assertion(parser, ident_is(next(parser), ";"), "expected ;"); pop(parser);
        enode post_iteration = call(parser, parse_expression);
        assertion(parser, ident_is(next(parser), "]"), "expected ]"); pop(parser);
        enode for_block = call(parser, parse_statements);
        //bind_stack->pop(); /// the vspace is manually pushed above, and thus remains for the parsing of these
        array operands = array_type.of_objects(null, statement, condition, post_iteration, for_block, null);
        enode for_statement = enode_type.create_operation(EType_For, operands);
        return for_statement;
    } else if (ident_is(t0, "while")) {
        pop(parser);
        assertion(parser, ident_is(next(parser), "["), "expected condition expression '['"); pop(parser);
        enode condition  = call(parser, parse_expression);
        assertion(parser, ident_is(next(parser), "]"), "expected condition expression ']'"); pop(parser);
        enode statements = call(parser, parse_statements);
        array operands = array_type.of_objects(null, condition, statements, null);
        return enode_type.create_operation(EType_While, operands);
    } else if (ident_is(t0, "if")) {
        pop(parser);
        assertion(parser, ident_is(next(parser), "["), "expected condition expression '['"); pop(parser);
        enode condition  = call(parser, parse_expression);
        assertion(parser, ident_is(next(parser), "]"), "expected condition expression ']'"); pop(parser);
        enode statements = call(parser, parse_statements);
        enode else_statements;
        bool else_if = false;
        if (ident_is(next(parser), "else")) { /// if there is no 'if' following this, then there may be no other else's following
            pop(parser);
            else_if = ident_is(next(parser), "if");
            else_statements = call(parser, parse_statements);
            assertion(parser, !else_if && ident_is(next(parser), "else"), "else proceeding else");
        }
        array operands = array_type.of_objects(null, condition, statements, else_statements, null);
        return enode_type.create_operation(EType_If, operands);
    } else if (ident_is(t0, "do")) {
        pop(parser);
        enode statements = call(parser, parse_statements);
        assertion(parser, ident_is(next(parser), "while"), "expected while");                pop(parser);
        assertion(parser, ident_is(next(parser), "["), "expected condition expression '['"); pop(parser);
        enode condition  = call(parser, parse_expression);
        assertion(parser, ident_is(next(parser), "]"), "expected condition expression '['");
        pop(parser);
        array operands = array_type.of_objects(null, condition, statements, null);
        return enode_type.create_operation(EType_DoWhile, operands);
    } else {
        return call(parser, parse_expression);
    }
}

static i64    Parser_parse_numeric(Parser parser, ident token) {
    return 0;
}

static EType  Parser_is_var(Parser parser, ident token) {
    char t = token->value->chars[0];
    if (isalpha(t) && call(keywords, index_of, token) == -1) {
        /// lookup against variable table; declare if in isolation
        return EType_Var;
        /// so types are included in var
        /// a token can variably be a type or a member here, or member there
    }
    return EType_Undefined;
}

static enode  Parser_parse_mult(Parser parser) {
    enode left = call(parser, parse_primary);
    while (ident_is(next(parser), "*") || ident_is(next(parser), "/")) {
        EType etype = ident_is(next(parser), "*") ? EType_Mul : EType_Div;
        pop(parser);
        enode right = call(parser, parse_primary);
        left = enode_type.create_operation(etype, array_type.of_objects(null, left, right, null));
    }
    return left;
}

static enode  Parser_parse_primary(Parser parser) {
    ident id = next(parser);
    printf("parse_primary: %s\n", id->value->chars);
    EType n = is_numeric(id);
    if (n) {
        ident f = next(parser);
        cstr cs = cast(f, cstr);
        bool is_int = n == EType_LiteralInt;
        pop(parser);
        A i64_value = i64_type.with_cstr((A)valloc(i64, 1), cs, -1);
        A prim = is_int ? construct(i64, cstr, cs, -1) : construct(f64, cstr, cs, -1);
        return enode_type.create_value(n, prim);
    }
    EType s = is_string(id);
    if (s) {
        ident f = next(parser);
        cstr cs = cast(f, cstr);
        pop(parser);
        AType t = typeof(string);
        string v = construct(string, cstr, cs, -1);
        string str_literal = v;
        assert(str_literal->len >= 2);
        str_literal = call(str_literal, mid, 1, str_literal->len - 2);
        return enode_type.create_value(s, str_literal);
    }
    /// parse_primary is called upon all types being read, for this module
    /// so we may know if the token is a type or not
    /// if its a 
    ident after = next_n(parser, 1);
    EType i = call(parser, is_var, id); /// its a variable or method (same thing; methods consume optional args)
    if (i && !ident_is(after, "[")) {
        pop(parser); /// for something like a lambda we cannot
        return enode_type.create_value(i, id); /// the entire Ident is value
    }

    /// make logging built into the exec of methods; methods would tag themselves
    /// members have 'tags', and that needs to be in C
    /// tags, again are types; so loggable is an abstract type

    if (ident_is(next(parser), "[")) {
        array emember_path = call(id, split_members, ".");
        assertion(parser, i == EType_MethodCall, "expected method call");
        pop(parser);
        array enode_args;
        for (;;) {
            enode op = call(parser, parse_expression); // do not read the , [verify this]
            call(enode_args, push, op);
            if (ident_is(next(parser), ","))
                pop(parser);
            else
                break;
        }
        assertion(parser, ident_is(next(parser), "]"), "expected ] after method invocation");
        pop(parser);
        enode method_call = enode_type.method_call(emember_path, enode_args);
        return method_call;
    } else {
        assertion(parser, i != EType_MethodCall, "not implemented");
    }
    return null;
}

array Parser_parse_raw_block(Parser parser) {
    if (!ident_is(next(parser), "["))
        return array_type.of_objects(null, pop(parser), null);
    assertion(parser, ident_is(next(parser), "["), "expected beginning of block [");
    array res = new(array);
    operator(res, assign_add, pop(parser));
    int level = 1;
    for (;;) {
        if (ident_is(next(parser), "[")) {
            level++;
        } else if (ident_is(next(parser), "]")) {
            level--;
        }
        operator(res, assign_add, pop(parser));
        if (level == 0)
            break;
    }
    return res;
}

/// parse members from a block
array Parser_parse_args(Parser parser, A object) {
    array result = new(array);
    assertion(parser, ident_is(pop(parser), "["), "expected [ for arguments");
    /// parse all symbols at level 0 (levels increased by [, decreased by ]) until ,

    /// # [ int arg, int arg2[int, string] ]
    /// # args look like this, here we have a lambda as a 2nd arg
    /// 
    while (next(parser) && !ident_is(next(parser), "]")) {
        member_def def = M(Parser, read_member, parser, object, null); /// we do not allow type-context in args but it may be ok to try in v2
        call(result, push, def);
        ident n = next(parser);
        assertion(parser, ident_is(n, "]") || ident_is(n, ","), ", or ] in arguments");
        if (ident_is(n, "]"))
            break;
        pop(parser);
    }
    assertion(parser, ident_is(pop(parser), "]"), "expected end of args ]");
    return result;
}

/// run this only when we have read all module entries and are ready to resolve
array Parser_read_type(Parser parser) {
    array res = new(array);
    // see if we can do this without a ref; 
    // object could handle things of this nature
    //if (ident_is(next(parser), "ref"))
    //    call(res, push, pop(parser));
    
    for (;;) {
        ident t_id = pop(parser);
        assertion(parser, is_alpha(t_id), "expected type identifier");
        call(res, push, t_id);

        if (ident_is(next(parser), "::")) {
            call(res, push, pop(parser));
            continue;
        }
        break;
    }
    return res;
};

void print_tokens(array tokens) {
    for (int i = 0; i < tokens->len; i++) {
        printf("%s\n", ((ident)tokens->elements[i])->value->chars);
    }
}

/// we need to change this a bit, i want it to add the tokens for all parsing to be done after we 'load' the other types
/// array of ident
string tokens_string(array tokens) {
    string res = construct(string, sz, 32);
    each(array, tokens, ident, token) {
        //if (res->len)
        //    M(res, append, " ");
        M(string, append, res, token->value->chars);
    }
    return res;
}

member_def Parser_read_member(Parser parser, A obj, member_def peer) {
    //struct_t st = null;
    class_t  cl = null;
    string  this_name;
    A       obj_type = obj ? fields(obj) : null;

    bool is_intern = false;
    if (next_is(parser, "intern")) {
        pop(parser);
        is_intern = true;
    }
    bool is_public = false;
    if (next_is(parser, "public")) {
        pop(parser);
        is_public = true;
    }
    bool is_static = false;
    if (next_is(parser, "static")) {
        pop(parser);
        is_static = true;
    }

    if (obj) {
        if (obj_type->type == typeof(struct_t))
            this_name = ((struct_t)obj)->name;
        else if (obj_type->type == typeof(class_t))
            this_name = ((class_t)obj)->name;
    }
    member_def result  = new(member_def);
    bool       is_ctr  = false;
    //bool       is_init = false;
    bool       no_args = false;
    ident      ntop    = next(parser);
    
    /// is constructor
    if (!peer && ident_is(ntop, this_name->chars)) {
        assertion(parser, obj_type->type == typeof(class_t),
            "expected class when defining constructor");
        result->name = hold(pop(parser));
        is_ctr = true;
    /// is init method [this is where the type is]
    } else if (ident_is(ntop, "init")) {
        no_args = true;
        /// int[] [ ...code ... ]
        /// int[] method[...args...] [...code...]
        /// [read-type][check-token for method or cast]

        result->name = hold(ntop);
        result->value = M(Parser, parse_raw_block, parser);
    /// is method with args
    } else if (ident_is(ntop, "cast")) {
        no_args = true;
        pop(parser);
        result->type_tokens = call(parser, read_type);
        string type = tokens_string(result->type_tokens);
        result->name = format("cast_%s", type->chars); /// cannot support unsupported :: at the moment, will replace later
        assertion(parser, result->type_tokens->len && call((ident)result->type_tokens->elements[0], is_alpha), "expected type for casting");
        //result->value = call(parser, parse_raw_block);
        result->member_type == MemberType_Cast;
    } else {
        if (peer) {
            result->type_tokens = peer->type_tokens;
        } else {
            no_args = true;
            ident n = next(parser);
            result->type_tokens = M(Parser, read_type, parser);

            string type = tokens_string(result->type_tokens);
            result->name = ident_string(pop(parser));
            printf("member: type: %s name: %s\n", type->chars, result->name->chars);
            if (ident_is(next(parser), ":")) {
                pop(parser);
                result->value = M(Parser, parse_raw_block, parser);
            } else if (ident_is(next(parser), "[")) {
                no_args = false;
            }
            drop(type);
        }

        if (!result->name) {
            assertion(parser, is_alpha(next(parser)),
                "%s:%d: expected identifier for member, found %s", 
                next(parser)->fname->chars, next(parser)->line_num, next(parser)->value->chars);
            result->name = pop(parser);
        }
    }
    ident n = next(parser);
    assertion(parser, (ident_is(n, "[") && is_ctr) || !is_ctr, "invalid syntax for constructor; expected [args]");
    
    /// this only happens for methods, or should...
    /// members with name:[initializer] is handled above (block parsed for processing later)
    if (ident_is(n, "[")) {
        if (!no_args)
            result->args = M(Parser, parse_args, parser, obj);
        int line_num_def = n->line_num;
        if (is_ctr) {
            if (ident_is(next(parser), ":")) {
                pop(parser);
                ident class_name = pop(parser);
                assertion(parser,
                    ident_is(class_name, cl->from->chars) ||
                    ident_is(class_name, this_name->chars), "invalid constructor base call");
                result->base_class = class_name; /// should be assertion checked above
                result->base_forward = M(Parser, parse_raw_block, parser);
            }
            result->member_type = MemberType_Constructor;
            ident n = next(parser);
            if (ident_is(n, "[")) {
                result->value = M(Parser, parse_raw_block, parser);
            } else {
                call(result->value, push, construct(ident, cstr, "[", -1)); /// with ctrs of name, the #2 and on args can be optional.  this is a decent standard.  we would select the first defined to match
                assert(obj);
                assert((obj_type->type == typeof(class_t)));
                class_t cl = ((class_t)obj);
                
                each(array, result->args, member_def, arg) {
                    bool found = false;
                    each(array, cl->members, member_def, m) {
                        if (m->name == arg->name) {
                            found = true;
                            break;
                        }
                    }
                    assertion(parser, found, "arg cannot be found in membership");
                    operator(result->value, assign_add, construct(ident, cstr, chars(arg->name), -1));
                    operator(result->value, assign_add, construct(ident, cstr, ":", -1));
                    call(result->value, push, construct(ident, cstr, arg->name->chars, -1));
                }
                call(result->value, push, construct(ident, cstr, "]", -1));
            }
            /// the automatic constructor we'll for-each for the args
        } else {
            ident next_token = next(parser);
            if (!ident_is(next_token, "return") && (ident_is(next_token, ":") || !ident_is(next_token, "["))) {
                assert(result->member_type != MemberType_Cast);
                result->member_type = MemberType_Lambda;
                if (ident_is(next_token, ":"))
                    pop(parser); /// lambda is being assigned, we need to set a state var
            } else if (!result->member_type) {
                result->member_type = MemberType_Method;
            }
            ident n = next(parser);
            if ((result->member_type == MemberType_Method    || 
                 result->member_type == MemberType_Cast      ||
                 result->member_type == MemberType_Operator) || ident_is(n, "[")) {
                // needs to be able to handle trivial methods if we want this supported
                if (ident_is(n, "return")) {
                    assertion(parser, n->line_num == line_num_def, "single line return must be on the same line as the method definition");
                    for (;;) {
                        if (n->line_num == next(parser)->line_num) {
                            call(result->value, push, pop(parser));
                        } else 
                            break;
                    }
                } else {
                    assertion(parser, ident_is(n, "["), "expected [method code block], found %s", n->value->chars);
                    result->value = M(Parser, parse_raw_block, parser);
                }
            }
        }
    } else if (ident_is(n, ":")) {
        pop(parser);
        result->value = M(Parser, parse_raw_block, parser);
    } else {
        // not assigning variable
    }

    result->intern    = is_intern;
    result->is_static = is_static;
    result->is_public = is_public;
    return result;
}

#define token_cstr(T)   T->value->chars

void push_define(module_t m, Parser parser, ident keyword, define_t mm) {
    for (num i = 0; i < m->defines->len; i++) {
        define_t def = m->defines->elements[i];
        if (strcmp(mm->name->chars, def->name->chars) == 0) {
            assertion(parser, false, "duplicate identifier for %s: %s",
                keyword->value->chars, mm->name->chars);
        }
    }
    //A obj = fields(mm);
    M(array, push, m->defines, hold(mm)); /// anything 'new' gets a base ref count, so anything pushed should be held if not new
    mm->module = (module_t)hold(m);
    if (strcmp(mm->name->chars, "app") == 0)
        m->app = (class_t)hold(mm);
};


#include <stdio.h>

int contains_main(cstr obj_file) {
    char command[256];
    snprintf(command, sizeof(command), "nm %s", obj_file);

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, " T main") != NULL) {
            pclose(fp);
            return 1; // Found main
        }
    }

    pclose(fp);
    return 0; // No main found
}

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
#define OS_MAC
#elif defined(__linux__) || defined(__unix__)
#define OS_UNIX
#else
#define OS_UNKNOWN
#endif

cstr exe_ext() {
    #ifdef OS_WINDOWS
        return "exe";
    #else
        return "";
    #endif
}

cstr shared_ext() {
    #ifdef OS_WINDOWS
        return "dll";
    #elif defined(OS_MAC)
        return "dylib";
    #elif defined(OS_UNIX)
        return "so";
    #else
        return "";
    #endif
}

/// split this up!  30 lines each at most lol


void define_t_added(define_t def) { }

void import_t_added(import_t import) {
    /// load silver module if name is specified without source
    if (import->name && !import->source) {
        array attempt = new(array);
        M(array, push_symbols, attempt, "", "spec/", null);
        bool exists = false;
        for (int ia = 0; ia < attempt->len; ia++) {
            string pre = attempt->elements[ia];
            char buf[1024];
            sprintf(buf, "%s%s.si", pre->chars, import->name->chars);
            path si_path = construct(path, cstr, buf, -1);

            //console.log("si_path = {0}", { si_path });
            if (!M(path, exists, si_path))
                continue;
            import->module_path = si_path;
            printf("module %s", si_path->chars);
            import->module = construct(module_t, path, si_path);
            exists = true;
            break;
        }
        assertion(null, exists, "path does not exist for silver module: %s", import->name->chars);
    } else if (import->name && import->source) { 
        /// this can be simpler but i am experimenting with different modes of import
        bool has_c  = false;
        bool has_rs = false;
        bool has_so = false;
        bool has_a  = false;
        for (int i = 0; i < import->source->len; i++) {
            string i0 = import->source->elements[i];
            assert(fields(i0)->type == typeof(string));
            if (has_suffix(i0->chars, ".c")) {
                has_c = true;
                break;
            }
            if (has_suffix(i0->chars, ".rs")) {
                has_rs = true;
                break;
            }
            if (has_suffix(i0->chars, ".so")) {
                has_so = true;
                break;
            }
            if (has_suffix(i0->chars, ".a")) {
                has_a = true;
                break;
            }
        }
        if (has_c || has_rs) {
            assert(chdir(import->relative_path->chars) == 0);
            /// build this single module, linking all we have imported prior
            import->import_type = ImportType_source;
            M(import_t, build_source, import, import->name, import->source);
        } else if (has_so) {
            assert(chdir(import->relative_path->chars) == 0);
            /// build this single module, linking all we have imported prior
            import->import_type = ImportType_library;
            if (!import->library_exports)
                import->library_exports = new(array);
            each (array, import->source, string, lib) {
                string rem = M(string, mid, lib, 0, lib->len - 3);
                M(array, push, import->library_exports, hold(rem));
            }
        } else if (has_a) {
            assert(chdir(import->relative_path->chars) == 0);
            /// build this single module, linking all we have imported prior
            import->import_type = ImportType_library;
            if (!import->library_exports)
                import->library_exports = new(array);
            each (array, import->source, string, lib) {
                string rem = M(string, mid, lib, 0, lib->len - 2);
                M(array, push, import->library_exports, hold(rem));
            }
        } else {
            /// source[0] is the url, we need to register the libraries we have
            /// for now I am making a name relationship here that applies to about 80% of libs
            assert(import->source->len == 1);
            import->import_type = ImportType_project;
            M(import_t, build_project, import, import->name, import->source->elements[0]);

            if (!import->library_exports)
                import->library_exports = new(array);
            /// only do this if it exists
            M(array, push, import->library_exports, import->name);
        }
    }
}

// its an interesting idea to release objects based on enumeration so we do not need to have code for that
// it would mean registration of interns too purely for the release of data
// its trivial to know if its a primitive or not, so lets do that


module_t module_t_with_path(module_t m, path fname) {
    path rel = M(path, directory, fname);
    string contents = M(path, read, fname, typeof(string));
    assert(chdir(rel->chars) == 0);
    m->module_name = hold(fname);
    m->tokens = parse_tokens(contents, fname);
    m->imports = new(array);
    m->defines = new(array);
    m->cache   = new(hashmap);
    drop(contents);
    Parser parser = construct(Parser, array, m->tokens, fname, m);
    int   imports  = 0;
    array meta_symbols = null;
    ///
    for (;;) {
        ident token = call(parser, next);
        if (!token)
            break;
        if (ident_is(token, "intern")) {
            pop(parser);
            token  = pop(parser);
            parser->membership = EMembership_internal;
        } else {
            parser->membership = EMembership_normal;
        }
        drop(parser->keyword);
        parser->keyword = ident_string(token);
        if (ident_is(token, "meta")) {
            pop(parser);
            assertion(parser, ident_is(next(parser), "["), "expected [ after meta");
            array raw = call(parser, parse_raw_block);
            assertion(parser, raw->len >= 3, "meta args cannot be empty");
            meta_symbols = new(array);
            for (int i = 1; i < raw->len - 1; i++)
                call(meta_symbols, push, hold(raw->elements[i]));
            drop(raw);
        } else if (ident_is(token, "import")) {
            assertion(parser, parser->membership == EMembership_normal, 
                "intern keyword not applicable to import");
            parser->membership = EMembership_normal;
            parser->keyword = null;
            import_t import = construct(import_t, Parser, parser);
            import->relative_path = hold(rel);
            imports++;
            push_define(m, parser, token, import);
            M(array, push, m->imports, import);
            M(define_t, added, import);
        } else if (ident_is(token, "enum")) {
            assert(false);
        } else if (ident_is(token, "class")) {
            parser->meta_symbols = hold(meta_symbols);
            class_t cl = construct(class_t, Parser, parser);
            push_define(m, parser, token, cl);
            drop(cl);
            drop(meta_symbols);
            meta_symbols = null;
        } else if (ident_is(token, "proto")) {
            assert(false);
        } else if (ident_is(token, "struct")) {
            assert(false);
        } else {
            assert(false);
        }
    }

    /// step 1. read members in all definitions in this module
    /// this means their name and type which we can be aware of, but not how to initialize it if its in our own module
    each(array, m->defines, define_t, def) {
        //AType type = typeid(def);
        call(def, read_members); 
    }

    /// step 2. resolve members to their 
    each(array, m->defines, define_t, def) {
        //AType type = typeid(def);
        /// set identity on types specified from R-Type and any args
        call(def, resolve_members);
    }

    string libraries_used   = new(string);
    string compiled_objects = new(string);
    bool   has_main         = false;

    each(array, m->defines, define_t, def) {
        AType type = typeid(def);
        if (type == typeof(import_t)) {
            import_t import = def;
            
            switch (import->import_type) {
                case ImportType_source:
                    each(array, import->source, string, source) {
                        /// these are built as shared library only 
                        if (has_suffix(source->chars, ".rs"))
                            continue;
                        if (compiled_objects->len)
                            M(string, append, compiled_objects, " ");
                        char buf[64];
                        sprintf(buf, "%s/%s.o", build_root->chars, source->chars);
                        if (!has_main && contains_main(buf))
                            has_main = true;
                        M(string, append, compiled_objects, buf);
                    }
                    break;
                case ImportType_library:
                case ImportType_project:
                    each(array, import->library_exports, string, lib) {
                        if (libraries_used->len)
                            M(string, append, libraries_used, " ");
                        M(string, append, libraries_used, "-l ");
                        M(string, append, libraries_used, lib->chars);
                    }
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    }

    assert(chdir(rel->chars) == 0);
    if (compiled_objects->len) {
        char link[2048];
        path cwd = path_type.cwd(1024);
        path out = M(path, change_ext, m->module_name, has_main ? exe_ext() : shared_ext());
        string out_stem = M(path, stem, out);
        path install = install_dir();
        /// we need to have a lib paths array
        snprintf(link, sizeof(link),
            "gcc %s -L %s %s %s -o %s/%s",
                has_main ? "" : "-shared",
                install->chars,
                compiled_objects->chars, libraries_used->chars,
                build_root->chars, out_stem->chars);
        printf("%s > %s\n", cwd->chars, link);
        assert(system(link) == 0);
        drop(install);
        drop(cwd);
    }
    return m;
}

static string pull_sp(array sp, int* remain, int* cursor) {
    if (*cursor >= sp->len) return null;
    (*remain)--;
    return sp->elements[(*cursor)++];
}

static silver_t resolve(array sp, module_t module_instance, string* key_name, int* remain, int* cursor) {
    *key_name = new(string);
    /// pull the type requested, and the template args for it at depth
    string class_name = pull_sp(sp, remain, cursor); /// resolve from template args, needs 
    class_t class_def = call(module_instance, find_class, class_name);
    assertion(null, class_def != null, "class not found: %s", class_name->chars);
    assertion(null, class_def->meta_symbols->len <= *remain, "template args mismatch");
    call((*key_name), append, class_name->chars);

    array meta_types = new(array);
    for (int i = 0; i < class_def->meta_symbols->len; i++) {
        string k;
        silver_t type_from_symbol = resolve(sp, module_instance, &k, remain, cursor);
        assert(type_from_symbol); /// grabbing context would be nice if the security warranted it
        assertion(null, k->len, "failed to resolve meta symbol");
        operator(meta_types, assign_add, type_from_symbol);
        call(*key_name, append, "::");
        call(*key_name, append, k->chars);
    }
    if (class_def->module)
        if (call(class_def->module->cache, contains, key_name))
            return call(class_def->module->cache, get, key_name);
    
    /// translation means applying template, then creating enode operations for the methods and initializers
    silver_t res = new(silver_t);
    res->tokens  = parse_tokens(key_name, module_instance->module_name);
    res->def     = construct(meta_instance, define_t, class_def, meta_types);
    call(class_def->module->cache, set, key_name, res);
    return res;
};

static meta_instance meta_instance_with_define_t(meta_instance a, define_t def, array meta_types) {
    a->def = hold(def);
    a->meta_types = hold(meta_types);
    return a;
}

/// call this within the enode translation; all referenced types must be forged
/// resolving type involves designing, so we may choose to forge a type that wont forge at all.  thats just a null
static silver_t forge_type(module_t module, array type_tokens) {
    /// need array or map feature on this type, 
    /// which must reference a type thats found
    array sp = construct(array, sz, type_tokens->len);
    if (type_tokens->len == 1) {
        operator(sp, assign_add, hold(type_tokens->elements[0])); /// single class reference (no templating)
    } else {
        assert(type_tokens->len & 1); /// cannot be an even number of tokens
        for (int i = 0; i < type_tokens->len - 1; i += 2) {
            ident i0 = type_tokens->elements[i + 0];
            ident i1 = type_tokens->elements[i + 1];
            assert(call(i0, is_alpha));
            assert(ident_is(i1, "::"));
            operator(sp, assign_add, i0);
        }
    }
    int cursor = 0;
    int remain = sp->len;

}

module_t module_t_find_module(module_t a, string name) {
    each(array, a->imports, import_t, e) {
        if (e->name == name)
            return e->module;
    }
    return null;
}

/// find_implement is going to be called with a as the source parser module
/// objects can be nullable, thats default for array but not for members
A module_t_find_implement(module_t a, ident iname) {
    //module_t m    = a;
    array   sp      = call(iname->value, split, ".");
    int     n_len   = sp->len;
    string  ns      = n_len ? sp->elements[0] : null;
    string  name    = sp->elements[(n_len > 1) ? 1 : 0];
    each(array, a->imports, import_t, e) {
        if ((!ns || e->isolate_namespace == ns) && e->module)
            each(array, e->module->defines, define_t, mm) {
                if (mm->name == name)
                    return mm;
            }
    }
    each(array, a->defines, define_t, mm) {
        if (mm->name == name)
            return mm;
    }
    return null;
}

A module_t_find_class(module_t a, ident name) {
    A impl = M(module_t, find_implement, a, name);
    if (typeid(impl) == typeof(class_t))
        return (class_t)impl;
    return null;
}

A module_t_find_struct(module_t a, ident name) {
    A impl = M(module_t, find_implement, a, name);
    if (typeid(impl) == typeof(struct_t))
        return (struct_t)impl;
    return null;
}


class_t class_t_translate(class_t cl) {

    if (!cl->is_translated) {
        each (array, cl->members, member_def, m) {
            if (m->member_type == MemberType_Method) {
                assertion(null, m->value->len, "method %s has no value", m->name->chars);
                Parser parser = construct(Parser, array, m->value, cl->module->module_name, cl->module);
                printf("translating method %s\n", m->name->chars);
                /// needs to have arguments in statements
                m->translation = M(Parser, parse_statements, parser); /// the parser must know if the tokens are translatable
            }
        }
        cl->is_translated = true;
    }
}

void graph(module_t module) {
    if (module->translated)
        return;
    
    module->translated = true;
    each (array, module->imports, import_t, import)
        if (import->module)
            graph(import->module);

    each (array, module->defines, define_t, def)
        if (typeid(def) == typeof(class_t))
            M(class_t, translate, def);
}

void module_t_graph(module_t module) {
    each (array, module->imports, import_t, import)
        if (import->module)
            graph(import->module);
    graph(module);
}

void module_t_c99(module_t m) {
    int test = 0;
    test++;
}

void module_t_run(module_t m) {
}

cstr silver_t_cast_cstr(silver_t a) {
    static string dragon;
    drop(dragon);
    dragon = tokens_string(a->tokens);
    return dragon->chars;
}

u64 silver_t_hash(silver_t a) {
    return call(a->tokens, hash);
}

define_class(silver_t)
define_class(Parser)
define_class(ident)
define_class(enode)
define_class(define_t)
define_class(module_t)
define_class(member_def)

define_mod(class_t,  define_t)
define_mod(enum_t,   define_t)
define_mod(import_t, define_t)
define_mod(struct_t, define_t)
define_mod(var_t,    define_t)

define_class(meta_instance)

declare_alias(array, array_i32)
 define_alias(array, array_i32, i32)

 define_alias(array, array_ident, ident)

int main(int argc, char **argv) {
    A_finish_types();

    i64 i64_v = 22;
    i32*    v = construct(i32, i64, i64_v);

    /// create silver root module; reflect types registered
    root = new(module_t);
    root->defines = new(array);
    num n_types;
    A_f** types = A_types(&n_types);

    for (int i = 0; i < n_types; i++) {
        A_f *type = &types[i];
        if (!(type->traits & A_TRAIT_PRIMITIVE)) continue;
        class_t prim_class  = construct(class_t, AType, type);
        call(root->defines, push, prim_class);
    }

    keywords = new(array);
    M(array, push_symbols, keywords, 
        "class",  "proto",  "struct", "cast",
        "import", "return", "asm", "if",
        "switch", "while",  "for", "do", null);

    assign = new(array);
    M(array, push_symbols, assign, 
        ":", "+=", "-=", "*=", "/=", "|=",
        "&=", "^=", ">>=", "<<=", "%=", null);

    /// if -b is not specified, the build directory is cwd
    /// silver module.si [-b build-root]
    is_debug = false;
    build_root = path_type.cwd(1024);

    //chdir("spec");
    if (argc > 1) {
        cstr arg = argv[1];
        path module_path = construct(path, cstr, arg, -1);
        path module_path_abs = call(module_path, absolute);

        module_t wgpu = construct(module_t, path, module_path_abs);
        call(wgpu, graph);

        class_t cl_def = call(wgpu, find_class, "app");

        assert(cl_def);
        call(wgpu, c99);
        drop(wgpu);
    } else {
        printf("required argument: path/to/silver-module.si\n");
    }
    return 0;
}