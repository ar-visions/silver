#include <import>
#include <limits.h>
#include <execinfo.h>
#include <ports.h>

// designed for Audrey and Rebecca

enode parse_statements(silver a, bool unique_members);
enode parse_statement(silver a);
void build_fn(silver a, efunc fmem, callback preamble, callback postamble);
static void build_record(silver a, etype mrec);

// used in more primitive cases
#define au_lookup(sym) lexical(a->lexical, sym)

#define elookup(sym) ({ \
    Au_t m = lexical(a->lexical, sym); \
    m ? u(etype, m) : null; \
})

#define validate(cond, t, ...) ({ \
    if (!(cond)) { \
        string s = (string)formatter((Au_t)null, stderr, (Au) true, (symbol) "\n%o:%i:%i " t, a->module_file, \
                  peek(a)->line, peek(a)->column, ##__VA_ARGS__); \
        if (level_err >= fault_level) { \
            halt(s); \
        } \
        false; \
    } else { \
        true; \
    } \
})

token silver_element(silver, num);

void print_tokens(silver a, symbol label) {
    print("[%s] %o %o %o %o %o %o...", label,
          element(a, 0), element(a, 1), element(a, 2), element(a, 3),
          element(a, 4), element(a, 5));
}

void print_token_state(silver a, symbol label) {
    print("[%s] %o %o %o %o %o %o...", label,
          element(a, 0), element(a, 1), element(a, 2), element(a, 3),
          element(a, 4), element(a, 5));
}

static void print_all(silver a, symbol label, array list) {
    print("[%s] tokens", label);
    each(list, token, t)
        put("%o ", t);
    put("\n");
}

static void print_all_tokens(silver a, symbol label, array list) {
    print("[%s] tokens", label);
    each(list, token, t)
        put("%o ", t);
    put("\n");
}

static map operators;
static array keywords;
static array assign;
static array compare;

#if defined(__x86_64__) || defined(_M_X64)
static symbol arch = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
static symbol arch = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
static symbol arch = "arm64";
#elif defined(__arm__) || defined(_M_ARM)
static symbol arch = "arm32";
#endif

#if defined(__linux__)
static symbol lib_pre = "lib";
static symbol lib_ext = ".so";
static symbol app_ext = "";
static symbol platform = "linux";
static symbol shared   = "-shared";
#elif defined(_WIN32)
static symbol lib_pre = "";
static symbol lib_ext = ".dll";
static symbol app_ext = ".exe";
static symbol platform = "windows";
static symbol shared   = "-shared";
#elif defined(__APPLE__)
static symbol lib_pre = "lib";
static symbol lib_ext = ".dylib";
static symbol app_ext = "";
static symbol platform = "darwin";
static symbol shared   = "-dynamiclib";
#endif

#define next_is(a, ...) silver_next_is_eq(a, __VA_ARGS__, null)


static bool is_dbg(import t, string query, cstr name, bool is_remote) {
    cstr dbg = (cstr)query->chars; // getenv("DBG");
    char dbg_str[PATH_MAX];
    char name_str[PATH_MAX];
    sprintf(dbg_str, ",%s,", dbg);
    sprintf(name_str, "%s,", name);
    int name_len = strlen(name);
    int dbg_len = strlen(dbg_str);
    int has_astrick = 0;

    for (int i = 0, ln = strlen(dbg_str); i < ln; i++) {
        if (dbg_str[i] == '*') {
            if (dbg_str[i + 1] == '*')
                has_astrick = 2;
            else if (has_astrick == 0)
                has_astrick = 1;
        }
        if (strncmp(&dbg_str[i], name, name_len) == 0) {
            if (i == 0 || dbg_str[i - 1] != '-')
                return true;
            else
                return false;
        }
    }
    bool is_local = !is_remote;
    return has_astrick > 1 ||
           (has_astrick && has_astrick == (int)is_local);
}

etype read_etype(silver a, array*);

num index_of_cstr(Au a, cstr f) {
    Au_t t = isa(a);
    if (t == typeid(string))
        return index_of((string)a, f);
    if (t == typeid(array))
        return index_of((array)a, (Au)string(f));
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal)) {
        cstr v = strstr((cstr)a, f);
        return v ? (num)(v - f) : (num)-1;
    }
    fault("len not handled for type %s", t->ident);
    return 0;
}

#define is_alpha(any) _is_alpha((Au)any)
static bool _is_alpha(Au any) {
    if (!any)
        return false;
    Au_t type = isa(any);
    string s;
    if (type == typeid(string)) {
        s = (string)any;
    } else if (type == typeid(token)) {
        token t = (token)any;
        s = string(t->chars);
    }
    if (index_of_cstr((Au)keywords, cstring(s)) >= 0)
        return false;
    if (len(s) > 0) {
        char first = s->chars[0];
        return isalpha(first) || first == '_';
    }
    return false;
}

string silver_read_alpha(silver a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        next(a);
        return string(n->chars);
    }
    return null;
}

bool silver_is_cmode(silver a) {
    token n = element(a, 0);
    return (n && n->cmode);
}

string git_remote_info(path path, string *out_service, string *out_owner, string *out_project) {
    // run git command
    string cmd = f(string, "git -C %s remote get-url origin", path->chars);
    string remote = command_run((command)cmd);

    verify (remote && remote->count, "silver modules must originate in git repository");

    cstr url = remote->chars;

    // strip trailing newline(s)
    for (int i = remote->count - 1; i >= 0 && (url[i] == '\n' || url[i] == '\r'); i--)
        url[i] = '\0';

    cstr domain = NULL, owner = NULL, repo = NULL;

    if (strstr(url, "://")) {
        // HTTPS form: https://github.com/owner/repo.git
        domain = strstr(url, "://") + 3;

        cstr at = strchr(domain, '@');
        if (at && at < strpbrk(domain, ":/"))
            domain = at + 1;

    } else if (strchr(url, '@')) {
        // SSH form: git@github.com:owner/repo.git
        domain = strchr(url, '@') + 1;
    } else {
        fault("git_remote_info: unrecognized URL: %s", url);
    }

    // domain ends at first ':' or '/'
    cstr domain_end = strpbrk(domain, ":/");
    if (!domain_end)
        fault("git_remote_info: malformed URL");
    *domain_end = '\0';

    // next part: owner/repo
    cstr next = domain_end + 1;
    owner = next;

    cstr slash = strchr(owner, '/');
    if (!slash)
        fault("git_remote_info: missing owner/repo");
    *slash = '\0';
    repo = slash + 1;

    // remove .git if present
    cstr dot = strrchr(repo, '.');
    if (dot && strcmp(dot, ".git") == 0)
        *dot = '\0';

    if (!*out_service)
         *out_service = string(domain);
    if (!*out_owner)
         *out_owner   = string(owner);
    if (!*out_project)
         *out_project = string(repo);

    return remote; // optional if you want to keep full URL
}

none sync_tokens(import t, path build_path, string name) {
    path t0 = form(path, "%o/import-token", build_path);
    path t1 = form(path, "%o/tokens/%o", t->mod->install, name);
    struct stat build_token, installed_token;
    /// create token pair (build & install) to indicate no errors during config/build/install
    cstr both[2] = {cstring(t0), cstring(t1)};
    for (int i = 0; i < 2; i++) {
        FILE *ftoken = fopen(both[i], "wb");
        fwrite("im-a-token", 10, 1, ftoken);
        fclose(ftoken);
    }
    int istat_build = stat(cstring(t0), &build_token);
    int istat_install = stat(cstring(t1), &installed_token);
    struct utimbuf times;
    times.actime = build_token.st_atime;  // access time
    times.modtime = build_token.st_mtime; // modification time
    utime(cstring(t1), &times);
}

string serialize_environment(map environment, bool b_export);

static array headers(path dir) {
    array all = ls(dir, null, false);
    array res = array();
    each(all, path, f) {
        string e = ext(f);
        if (len(e) == 0 || cmp(e, ".h") == 0)
            push(res, (Au)f);
    }
    drop(all);
    return res;
}

static int filename_index(array files, path f) {
    string fname = filename(f);
    int index = 0;
    each(files, path, p) {
        string n = filename(p);
        if (compare(n, fname) == 0)
            return index;
        index++;
    }
    return -1;
}

static bool sync_symlink(path src, path dst) {
    return false;
}

static bool is_checkout(path a) {
    path par = parent_dir(a);
    string st = stem(par);
    if (eq(st, "checkout")) {
        return true;
    }
    return false;
}

array compact_tokens(array tokens) {
    if (len(tokens) <= 1)
        return tokens;

    array res = array(32);
    int ln = len(tokens);
    token prev = null;
    token current = null;

    for (int i = 1; i < ln; i++) {
        token t = (token)tokens->origin[i];
        token prev = (token)tokens->origin[i - 1];
        if (!current)
            current = copy(prev);

        if (prev->line == t->line && (prev->column + prev->count) == t->column) {
            concat(current, (string)t);
            current->column += t->count;
        } else {
            push(res, (Au)current);
            current = copy(t);
        }
    }
    if (current)
        push(res, (Au)current);
    return res;
}

string model_keyword() {
    return null;
}

int main(int argc, cstrs argv) {
    engage(argv);
    silver a = silver(argv);
    return 0;
}

typedef struct {
    OPType ops[2];
    string method[2];
    string token[2];
} precedence;

static precedence levels[] = {
    {{OPType__mul,              OPType__div}},
    {{OPType__add,              OPType__sub}},
    {{OPType__right,            OPType__left}},
    {{OPType__greater,          OPType__less}},
    {{OPType__greater_eq,       OPType__less_eq}},
    {{OPType__equal,            OPType__not_equal}},
    {{OPType__is,               OPType__inherits}},
    {{OPType__xor,              OPType__xor}},
    {{OPType__and,              OPType__or}},
    {{OPType__bitwise_and,      OPType__bitwise_or}},
    {{OPType__value_default,    OPType__cond_value}} // i find cond-value to be odd, but ?? (value_default) should work for most
};

token silver_read_if(silver a, symbol cs);

enode parse_object(silver a, etype mdl_schema, bool in_expr);

static bool silver_next_is_eq(silver a, symbol first, ...);

static enode reverse_descent(silver a, etype expect);

static enode parse_expression(silver a, etype expect) {
    // handle array and map types here at this level, and cue in other types through a protocol if possible
    // our calls below this do not have any idea to use the comma in expression syntax
    if (expect && (is_rec(expect) || inherits(expect->au, typeid(collective))) && next_is(a, "["))
        return parse_object(a, expect, false);
    
    return reverse_descent(a, expect);
}

static enode reverse_descent(silver a, etype expect) {
    bool cmode = is_cmode(a);
    enode L = read_enode(a, expect, false); // build-arg
    token t = peek(a);
    validate(cmode || L, "unexpected '%o'", t);
    if (!L)
        return null;
    a->expr_level++;

    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        bool m = true;
        while (m) {
            m = false;
            for (int j = 0; j < 2; j++) {
                string token = level->token[j];
                if (!read_if(a, cstring(token)))
                    continue;
                OPType op_type = level->ops[j];
                string method  = level->method[j];
                
                if (op_type == OPType__and || op_type == OPType__or) {
                    
                    //L = e_short_circuit(a, op_type, L);

                    if (op_type == OPType__or && read_if(a, "return")) {
                        etype rtype = return_type(a);
                        enode fallback = peek(a) ? parse_expression(a, rtype) : null;
                        verify(fallback || is_void(return_type(a)), "expected expression after return");
                        enode cond = e_not(a, L);
                        enode ret_node = fallback ? e_create(a, rtype, (Au)fallback) : null;
                        e_cond_return(a, cond, (Au)ret_node);
                        // L survives as the expression result for the next line
                    } else {
                        L = e_short_circuit(a, op_type, L);
                    }

                } else {
                    enode R = read_enode(a, null, false);
                    L = e_op(a, op_type, method, (Au)L, (Au)R);
                }
                m = true;
                break;
            }
        }
    }
    a->expr_level--;
    return L;
}

static array parse_tokens(silver a, Au input, array output);


Au build_init_preamble(enode f, Au arg) {
    silver a = (silver)f->mod;
    etype  rec = f->target ? resolve((etype)f->target) : (etype)a;

    members(rec->au, mem) {
        enode n = u(enode, mem);
        if (n && n->initializer)
            build_user_initializer(a, (etype)n);
    }
    return null;
}

#if defined(__APPLE__)
    #define SILVER_IS_MAC     1
    #define SILVER_IS_LINUX   0
    #define SILVER_IS_WINDOWS 0
    #define SILVER_IS_EMBEDED 0
#elif defined(_WIN32)
    #define SILVER_IS_MAC     0
    #define SILVER_IS_LINUX   0
    #define SILVER_IS_WINDOWS 1
    #define SILVER_IS_EMBEDED 0
#elif defined(__linux__)
    #define SILVER_IS_MAC     0
    #define SILVER_IS_LINUX   1
    #define SILVER_IS_WINDOWS 0
    #define SILVER_IS_EMBEDED 0
#else
    #error "unsupported platform"
#endif

#ifdef SILVER_SDK
    #define SILVER_IS_EMBEDDED 1
#else
    #define SILVER_IS_EMBEDDED 0
#endif

void implement_type_id(etype);

void silver_parse(silver a) {
    efunc init = module_initializer(a);

    Au_t m_mac = def_member(a->au, "mac",     typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_lin = def_member(a->au, "linux",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_win = def_member(a->au, "windows", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);

    set(a->registry, (Au)m_mac, (Au)hold(e_operand(a, _bool(SILVER_IS_MAC),     etypeid(bool))));
    set(a->registry, (Au)m_lin, (Au)hold(e_operand(a, _bool(SILVER_IS_LINUX),   etypeid(bool))));
    set(a->registry, (Au)m_win, (Au)hold(e_operand(a, _bool(SILVER_IS_WINDOWS), etypeid(bool))));
    
    while (peek(a)) {
        validate(parse_statement(a), "unexpected token found for statement: %o", peek(a));
        incremental_resolve(a);
    }

    // when done parsing, we are able to create a module schema (type_id definition) and the evar instance for the type_id (module_m with info/type)
    implement_type_id((etype)a);
    build_fn(a, init, build_init_preamble, null);
}

none aether_test_write(aether a);


// who throws away a Perfectly Good Watch?
i64 silver_watch(silver mod, path a, i64 last_mod, i64 millis) {
    int    fd = inotify_init1(IN_NONBLOCK);
    int    wd = inotify_add_watch(fd, a->chars, IN_MODIFY | IN_CLOSE_WRITE);
    char   buf[4096];
    struct stat st;

    while (1) {
        i64 ark_time = 0;
        each (mod->artifacts, path, ark) {
            i64 n = modified_time(ark);
            if (!ark_time || n > ark_time)
                ark_time = n;
        }
        i64 m = modified_time(a);
        if ((m > last_mod || ark_time > last_mod) && m != 0) {
            if (m > last_mod)
                last_mod = m;
            if (ark_time > last_mod)
                last_mod = ark_time;
            break;
        }
        // drain any pending events (old ones)
        read(fd, buf, sizeof(buf));

        // block until something *new* arrives
        int ln = read(fd, buf, sizeof(buf));
        if (ln > 0) continue;
        usleep(100000); // 100 ms safety
    }

    inotify_rm_watch(fd, wd);
    #undef close
    close(fd);
    return last_mod;
}

static path is_git_project(silver a) {
    // must be repo path: a->project_path
    // if so, return a->project_path
    // walk up from project_path to find the git repo root
    path p = parent_dir(a->module_path);
    path git_dir = f(path, "%o/.git", p);
    if (dir_exists("%o", git_dir) || file_exists("%o", git_dir))
        return p;
        
    return null;
}

// im a module!
void silver_init(silver a) {
    hold(a);

    bool is_once = a->build || a->is_external;

    if (a->version) {
        printf("silver 0.8.8\n");
        printf("Copyright (C) 2021 AR Visions\n");
        printf("This is free software; see the source for LICENSE.  There is NO\n");
        printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
        return;
    }

    // this is a means by which we cache configurations of our module, 
    // and prevent re-compilation when the date of the source is less than the product with name
    string defs_hash;
    if (len(a->defs)) {
        u64 hash = hash(a->defs);

        // lets get the first 6
        defs_hash = f(string, "%llx", hash);
        defs_hash = mid(defs_hash, 0, 6);
    } else
        defs_hash = string("");

    a->build_dir    = f(path, "%o/%s", a->install, a->debug ? "debug" : "release");
    a->product_link = f(path, "%o/%o.product", a->build_dir, a->name);
    a->defs_hash    = defs_hash;
    //a->import_cache = map();
    a->artifacts    = array(32);
    a->artifacts_path = f(path, "%o/%o.artifacts", a->build_dir, a->name);

    verify(a->module, "required argument: module (path/to/module)");

    a->module_path = parent_dir(a->module);
    a->module_file  = f(path, "%o/%o.ag",
        a->module, stem(a->module));

    // 1ms resolution time comparison (it could be nano-second based)
    bool update_product = true;
    u64  module_file_m  = modified_time(a->module_file);
    verify(module_file_m, "module file not found: %o", a->module_file);

    // check if we are the main project of this repository
    a->project_path = is_git_project(a);

    if (file_exists("%o", a->product_link) &&
            modified_time(a->product_link) > module_file_m) {

        if (file_exists("%o", a->artifacts_path)) {
            fdata f = fdata(read, true, src, a->artifacts_path);
            u64 newest = 0;
            while (true) {
                string art = (string)file_read(f, typeid(string));
                if (!art) break;
                path artifact = path(art);
                u64  m = modified_time(artifact);
                if  (m > newest) newest = m;
            }
            if (newest && newest < module_file_m)
                update_product = false;
        } else
            update_product = false; // it has no build artifacts, so we only use the date on itself 
    }

    verify(dir_exists("%o", a->install), "silver-import location not found");
    verify(len(a->module), "no source given");
    verify(file_exists("%o", a->module_file), "module-source not found: %o", a->module_file);

    verify(exists(a->module), "source (%o) does not exist", a->module);

    cstr _SRC    = getenv("SRC");
    cstr _DBG    = getenv("DBG");
    cstr _IMPORT = getenv("IMPORT");
    verify(_IMPORT, "silver requires IMPORT environment");

    a->mod          = (aether)a;
    a->imports      = array(32);
    a->parse_f      = parse_tokens;
    a->parse_expr   = parse_expression;
    //a->parse_enode  = silver_read_enode;
    //a->reverse_descent = reverse_descent;
    a->read_etype   = read_etype;
    a->src_loc      = absolute(path(_SRC ? _SRC : "."));
    verify(dir_exists("%o", a->src_loc), "SRC path does not exist");

    // should only get its parent if its a file
    path af         = a->module ? directory(a->module) : path_cwd();
    path install    = path(_IMPORT);
    git_remote_info(af, &a->git_service, &a->git_owner, &a->git_project);

    bool retry = false;
    i64 mtime = current_time();// modified_time(a->module);
    hold_members(a);
    
    do {
        if (retry) {
            print("awaiting iteration: %o", a->module);
            
            auto_free();
            mtime = silver_watch(a, a->module, mtime, 0); // it was easiest to fork path's implementation and add arks
            print("rebuilding...");
            drop(a->tokens);
            drop(a->stack);
            // clear(a->artifacts); [ its best to accumulate with dupe-checking -- otherwise design stage fail can result in less hooks on your workflow]
            reinit_startup(a);
            a->imports = array();
        }
        retry = false;
        a->cursor = 0;
        a->tokens = hold(tokens(
            target, (Au)a, parser, parse_tokens, input, (Au)a->module_file));
        print_all(a, "all", (array)a->tokens);
        a->stack = hold(array(4));
        a->implements = hold(array());


        // our verify infrastructure is now production useful
        attempt() {
            string m = stem(a->module);
            path i_gen = f(path, "%o/%o.i", a->module_path, m);
            path c_file = f(path, "%o/%o.c", a->module_path, m);
            path cc_file = f(path, "%o/%o.cc", a->module_path, m);
            path files[2] = {c_file, cc_file};
            for (int i = 0; i < 2; i++)
                if (exists(files[i])) {
                    if (!a->implements)
                        a->implements = array(2);
                    push(a->implements, (Au)files[i]);
                }
            
            string bp = a->breakpoint;
            Au info = head(bp);
            parse(a);
            build(a);
        }
        on_error() {
            retry = !is_once;
            a->error = true;
        }
        finally()
    } while (!a->is_external && retry); // externals do not watch (your watcher must invoke everything)
                                        // they handle their own exceptions
}

static string op_lang_token(string name) {
    pairs(operators, i) {
        string token = (string)i->key;
        string value = (string)i->value;
        if (eq(name, value->chars))
            return token;
    }
    fault("invalid operator name: %o", name);
    return null;
}

static void silver_module() {
    keywords = hold(array_of_cstr(
        "class", "struct", "public", "intern",
        "import", "export", "typeid", "context",
        "is", "inherits", "ref", "of",
        "const", "require", "no-op", "return", "->", "::", "...", "<>",
        "asm", "if", "switch", "any", "enum",
        "ifdef", "else", "while", "cast", "forge", "try", "throw", "catch", "finally",
        "for", "loop", "func", "operator", "index", "construct",
        null));

    assign = hold(array_of_cstr(
        ":", "=", "+=", "-=", "*=", "/=",
        "|=", "&=", "^=", "%=", ">>=", "<<=",
        null));

    compare = hold(array_of_cstr(
        "==", "!=", "<=>", ">=", "<=", ">", "<",
        null));

    operators = hold(map_of( // aether needs some operator bindings
        "+", string("add"),
        "-", string("sub"),
        "*", string("mul"),
        "/", string("div"),
        "||", string("or"),
        "&&", string("and"),
        "|", string("bitwise_or"),
        "&", string("bitwise_and"),
        "^", string("xor"),
        ">>", string("right"),
        "<<", string("left"),
        ">=", string("greater_eq"),
        ">", string("greater"),
        "<=", string("less_eq"),
        "<", string("less"),
        "??", string("value_default"),
        "?:", string("cond_value"),
        ":", string("bind"), // dynamic behavior on this, turns into "equal" outside of parse-assignment
        "=", string("assign"),
        "%=", string("assign_mod"),
        "+=", string("assign_add"),
        "-=", string("assign_sub"),
        "*=", string("assign_mul"),
        "/=", string("assign_div"),
        "|=", string("assign_or"),
        "&=", string("assign_and"),
        "^=", string("assign_xor"),
        ">>=", string("assign_right"),
        "<<=", string("assign_left"),
        "->", string("resolve_member"),
        "==", string("compare"),
        "~ ", string("equal"), // placeholder impossible match, just so we have the enum ordering
        "!=", string("not_equal"),
        "is", string("is"),
        "inherits", string("inherits"),
        null));

    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 2; j++) {
            OPType op = level->ops[j];
            string e_name = e_str(OPType, op);
            string op_name = mid(e_name, 1, len(e_name) - 1);
            string op_token = op_lang_token(op_name);
            level->method[j] = hold(op_name); // replace the placeholder; assignment is outside of precedence; the camel has spoken
            level->token[j] = eq(op_name, "equal") ? hold(string("==")) : hold(op_token);
        }
    }
}

typedef struct {
    symbol lib_prefix;
    symbol exe_ext, static_ext, shared_ext;
} exts;

exts get_exts() {
    return (exts)
#ifdef _WIN32
        {"", "exe", "lib", "dll"}
#elif defined(__APPLE__)
        {"", "", "a", "dylib"}
#else
        {"", "", "a", "so"}
#endif
    ;
}

token silver_element(silver a, num rel) {
    int r = a->cursor + rel;
    if (r < 0 || r > a->tokens->count - 1)
        return null;
    token t = (token)a->tokens->origin[r];
    return t;
}

bool silver_next_indent(silver a) {
    token p = element(a, -1);
    token n = element(a, 0);
    return p && n->indent > p->indent;
}

static bool silver_next_is_eq(silver a, symbol first, ...) {
    va_list args;
    va_start(args, first);
    int i = 0;
    symbol cs = first;
    while (cs) {
        token n = element(a, i);
        if (!n || strcmp(n->chars, cs) != 0) {
            va_end(args);
            return false;
        }
        cs = va_arg(args, symbol);
        i++;
    }
    va_end(args);
    return true;
}

bool dbg_addr_to_line(void *addr,
        const char **file,
        int *line,
        const char **func);

#define breakpoint(...) ({ \
    print(__VA_ARGS__); \
    raise(SIGTRAP); \
})


token silver_next(silver a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    if ( !a->clipping && a->breakpoint && eq(a->breakpoint, res->chars)) {
        print("token-breakpoint: %o", a->breakpoint);
    }
    a->cursor++;

    if (false && !a->clipping) {
        void *bt[8];
        backtrace(bt, 8);
        char **syms = backtrace_symbols(bt, 8);
        int ii = 2;
        for (int i = 2; i < 8; i++) {
            char* f = syms[i] ? strstr(syms[i], "(") : null;
            if  (!f) continue;
            // skip inlines and utility functons
            if (strncmp(f, "(+",                     2) == 0) continue;
            if (strncmp(f, "(silver_read_if+",      16) == 0) continue;
            if (strncmp(f, "(silver_read_alpha+",   19) == 0) continue;
            if (strncmp(f, "(silver_read_literal+", 21) == 0) continue;
            if (strncmp(f, "(silver_consume+",      16) == 0) continue;
            ii = i;
            break;
        }

        char* f = syms[ii] ? strstr(syms[ii], "(") : null;
        string ff = null;
        if (f) {
            ff = string(f);
            ff = mid(ff, 1, index_of(ff, "+") - 1);
        }
        if (ff && len(ff)) {

            symbol file = null;
            symbol func = null;
            int    line = 0;

            bool has_dbg = dbg_addr_to_line(bt[ii], &file, &line, &func);
            if (!has_dbg)
                return null;
            
            string buf = f(string, "%24s -- %12s:%-6i %o %o %o %o %o %o...", func, file, line,
                element(a, 0), element(a, 1), element(a, 2), element(a, 3),
                element(a, 4), element(a, 5));
            puts(buf->chars);
        }
    }

    return res;
}

token silver_consume(silver a) {
    return next(a);
}

static array read_within(silver a) {
    //return read_expression(a, null, null);

    array body = array(32);
    token n = element(a, 0);
    bool proceed = a->expr_level == 0 ? true : eq(n, "[");
    if (!proceed)
        return null;

    bool bracket = eq(n, "[");
    consume(a);
    int depth = bracket == true; // inner expr signals depth 1, and a bracket does too.  we need both togaether sometimes, as in inner expression that has parens
    for (;;) {
        token inner = next(a);
        if (!inner)
            break;
        if (eq(inner, "]"))
            depth--;
        if (eq(inner, "["))
            depth++;
        if (depth > 0) {
            push(body, (Au)inner);
            continue;
        }
        break;
    }
    return body;
}

token silver_peek(silver a) {
    if (a->cursor == len(a->tokens))
        return null;
    return element(a, 0);
}


static array read_body(silver a) {
    a->clipping = true;
    array body = array(32);
    token n = element(a, 0);
    if (!n)
        return null;
    token p = element(a, -1);
    bool mult = n->line > p->line;

    while (1) {
        token k = peek(a);
        if (!k)
            break;
        if (!mult && k->line > n->line)
            break;
        if (mult && k->indent <= p->indent)
            break;
        push(body, (Au)k);
        consume(a);
    }
    a->clipping = false;
    return body;
}

evar read_evar(silver a) {
    string name = read_alpha(a);
    if   (!name) return null;
    etype  mem  = elookup(name->chars);
    Au_t info = isa(mem);
    evar   node = instanceof(mem, evar);
    return node;
}

enode parse_asm(silver a, etype rtype) {
    //validate(read_if(a, "asm") != null, "expected asm");

    array input_nodes  = array(alloc, 8);
    array input_tokens = read_within(a);
    if (input_tokens) {
        push_tokens(a, (tokens)input_tokens, 0);
        bool expect_comma = false;
        while (peek(a)) {
            validate (!expect_comma || read_if(a, ","), "expected comma");
            evar node = read_evar(a);
            validate (node, "expected input var, found %o", peek(a));
            push     (input_nodes, (Au)node);
            expect_comma = true;
        }
        pop_tokens(a, false);
    }

    //etype rtype = null;
    //if (read_if(a, "->"))
    //    rtype = read_etype(a, null);

    array body = read_body(a);
    verify(body, "expected asm body");

    string return_name = null;
    for (int i = len(body) - 1; i >= 0; i--) {
        token t = (token)get(body, i);
        if (eq(t, "return") && i + 1 < len(body)) {
            return_name = string(((token)get(body, i + 1))->chars);
            remove(body, i + 1);
            remove(body, i);
            break;
        }
    }
    validate(!rtype || return_name,
        "asm with output requires return <name>");

    return e_asm(a, body, input_nodes, rtype, return_name);
}

// inline initializers can have expression continuations  aclass[something:1].something
// ones that are made with indentation do not   aclass
// however we usually have the option to do both (expr-level-0)
static array read_expression(silver a, etype *mdl_res, bool *is_const);
static array read_enode_tokens(silver a);

static array read_initializer(silver a) {
    array body = array(32);
    token n    = element(a,  0);
    token p    = element(a, -1);
    if  (!n || eq(n, "sub") || eq(n, "asm")) return null;

    print_tokens(a, "read_initializer");

    if (eq(n, "[") && (n->line == p->line || (n->line > p->line && n->indent == p->indent))) {
        consume(a);
        int depth = 1; // inner expr signals depth 1, and a bracket does too.  we need both together sometimes, as in inner expression that has parens
        push(body, (Au)token("["));
        for (;;) {
            token inner = next(a);
            if (!inner)
                break;
            if (eq(inner, "]"))
                depth--;
            if (eq(inner, "["))
                depth++;
            if (depth > 0) {
                push(body, (Au)inner);
                continue;
            }
            break;
        }
        push(body, (Au)token("]"));
        return body;
    }

    else if (n->indent > p->indent && n->line > p->line) {
        array res = read_body(a);
        array r = array(alloc, res->count + 2);
        push(r, (Au)token(chars, "["));
        concat(r, res);
        push(r, (Au)token(chars, "]"));
        return r;
    }

    return n->line == p->line ? read_enode_tokens(a) : null;
}

num silver_current_line(silver a) {
    token t = element(a, 0);
    return t->line;
}

string silver_location(silver a) {
    token t = element(a, 0);
    return t ? (string)location(t) : (string)form(string, "n/a");
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

static bool is_keyword(Au any) {
    Au_t type = isa(any);
    string s;
    if (type == typeid(string))
        s = (string)any;
    else if (type == typeid(token))
        s = string(((token)any)->chars);

    return index_of_cstr((Au)keywords, cstring(s)) >= 0;
}

string silver_read_keyword(silver a) {
    token n = element(a, 0);
    if (n && is_keyword((Au)n)) {
        next(a);
        return string(n->chars);
    }
    return null;
}

string silver_peek_keyword(silver a) {
    token n = element(a, 0);
    return (n && is_keyword((Au)n)) ? string(n->chars) : null;
}

bool silver_next_is_alpha(silver a) {
    if (peek_keyword(a))
        return false;
    token n = silver_element(a, 0);
    return is_alpha(n) ? true : false;
}

static int sfn(string a, string b) {
    int diff = len(a) - len(b);
    return diff;
}

static string scan_map(map m, string source, int index) {
    string s = string(alloc, 32);
    string last_match = null;
    int i = index;
    while (1) {
        if (i >= len(source))
            break;
        string a = mid(source, i, 1);
        append(s, a->chars);
        string f = (string)get(m, (Au)s);
        if (f) {
            last_match = f;
            i++;
        } else
            break;
    }
    return last_match;
}

static shape parse_shape(string str, string* str_res, i64* index) {
    int ln = len(str);
    bool single = false;
    int index_stop = *index;
    bool explicit = false;
    for (int i = *index; i < ln; i++) {
        i32 chr = idx(str, i);
        bool start = (i == *index);
        if ((chr == '-' && start) || ((chr == 'x' && !start) || (chr >= '0' && chr <= '9'))) {
            single |= (chr >= '0' && chr <= '9');
            explicit |= chr == 'x';
            index_stop = i;
            continue;
        }
        if (chr == '.')
            return null;
        break;
    }
    if (single) {
        int ln = index_stop - *index + 1;
        string sh = mid(str, *index, ln);
        array dims = split(sh, "x");
        shape res = shape(explicit, explicit);
        *index += ln;
        each(dims, string, s) {
            string tr = trim(s);
            i64 idim = integer_value(tr);
            shape_push(res, idim);
        }
        *str_res = sh;
        return res;
    }
    return null;
}

static array parse_tokens(silver a, Au input, array output) {
    string input_string;
    Au_t type = isa(input);
    path src = null;
    if (type == typeid(path)) {
        src = (path)input;
        input_string = (string)load(src, typeid(string), null); // this was read before, but we 'load' files; way less conflict wit posix
    } else if (type == typeid(string))
        input_string = (string)input;
    else
        assert(false, "can only parse from path");

    a->source_raw = (string)hold(input_string);

    list symbols = list();
    string special = string(".{}$,<>()![]/+*:=#");
    i32 special_ln = len(special);
    for (int i = 0; i < special_ln; i++)
        push(symbols, (Au)string((i32)special->chars[i]));
    each(keywords, string, kw) push(symbols, (Au)kw);
    each(assign, string, a) push(symbols, (Au)a);
    each(compare, string, a) push(symbols, (Au)a);
    pairs(operators, i) push(symbols, i->key);
    sort(symbols, (ARef)sfn);
    map mapping = map(hsize, 32);
    for (item i = symbols->first; i; i = i->next) {
        string sym = (string)i->value;
        set(mapping, (Au)sym, (Au)sym);
    }

    array tokens = output;
    verify(tokens, "no output set");

    num line_num = 1;
    num length = len(input_string);
    num index = 0;
    num line_start = 0;
    num indent = 0;
    bool num_start = 0;

    i32 chr0 = idx(input_string, index);
    validate(!isspace(chr0) || chr0 == '\n', "initial statement off indentation");

    while (index < length) {
        i32 chr = idx(input_string, index);

        if (isspace(chr)) {
            if (chr == '\n') {
                line_num += 1;
                line_start = index + 1;
                indent = 0;
            label: /// so we may use break and continue
                chr = idx(input_string, ++index);
                if (!isspace(chr))
                    continue;
                else if (chr == '\n')
                    continue;
                else if (chr == ' ')
                    indent += 1;
                else if (chr == '\t')
                    indent += 4;
                else
                    continue;
                goto label;
            } else {
                index += 1;
                continue;
            }
        }

        num_start = isdigit(chr) > 0;

        // comments
        if (!a->cmode && chr == '#') {
            if (index + 1 < length && idx(input_string, index + 1) == '#') {
                // multi-line
                index += 2;
                while (index < length && !(idx(input_string, index) == '#' && index + 1 < length && idx(input_string, index + 1) == '#')) {
                    if (idx(input_string, index) == '\n')
                        line_num += 1;
                    index += 1;
                }
                index += 2;
            } else {
                // single-line
                while (index < length && idx(input_string, index) != '\n')
                    index += 1;
            }
            continue;
        }

        string name = scan_map(mapping, input_string, index);
        if (name) {
            // we could merge these more generically
            if (a->cmode && len(name) == 1 && strncmp(&input_string->chars[index], "##", 2) == 0) {
                name = string("##");
            }
            token t = token(
                chars, (cstr)name->chars,
                indent, indent,
                source, src,
                line, line_num,
                column, index - line_start);
            push(tokens, (Au)t);
            index += len(name);
            continue;
        }

        if (chr == '"' || chr == '\'') {
            i32 quote_char = chr;
            num start = index;
            index += 1;
            while (index < length && idx(input_string, index) != quote_char) {
                if (idx(input_string, index) == '\\' && index + 1 < length &&
                    idx(input_string, index + 1) == quote_char)
                    index += 2;
                else
                    index += 1;
            }
            index += 1;
            string crop = mid(input_string, start, index - start);
            if (crop->chars[0] == '-') {
                char ch[2] = {crop->chars[0], 0};
                push(tokens, (Au)token(
                                 chars, ch,
                                 indent, indent,
                                 source, src,
                                 line, line_num,
                                 column, start - line_start));
                crop = string(&crop->chars[1]);
                line_start++;
            }
            token l = (token)last_element(tokens);
            if (l && chr == '\'' && isa(l->literal) == typeid(string)) {
                string s  = mid((string)l->literal, 0, len((string)l->literal) - 1);
                string s2 = mid(l, 1, len(l) - 2);
                concat(s, s2);
                drop(l->literal);
                l->literal = hold((Au)s);
            } else if (l && chr == '\"' && isa(l->literal) == typeid(const_string)) {
                string s  = mid((string)l->literal, 0, len((string)l->literal) - 1);
                string s2 = mid(l, 1, len(l) - 2);
                concat(s, s2);
                drop(l->literal);
                l->literal = hold((Au)s);
            } else
                push(tokens, (Au)token(
                                chars, crop->chars,
                                indent, indent,
                                source, src,
                                line, line_num,
                                column, start - line_start));
            continue;
        }

        num start = index;
        bool last_dash = false;
        i32 st_char = idx(input_string, start);
        bool start_numeric = st_char == '-' || (st_char >= '0' && st_char <= '9');
        string shape_str = null;
        //printf("tokens: %s\n", mid(input_string, index, 4)->chars);
        shape shape_literal = start_numeric ? parse_shape(input_string, &shape_str, &index) : null;
        if (shape_literal) {
            push(tokens, (Au)token(
                    chars,   shape_str->chars,
                    indent,  indent,
                    source,  src,
                    line,    line_num,
                    literal, (Au)shape_literal,
                    column,  start - line_start));
            continue;
        }

        int seps = 0;
        while (index < length) {
            i32 v = idx(input_string, index);
            bool is_sep = v == '.';
            if (is_sep) seps++;
            bool cont_numeric = (is_sep && seps <= 1) || (v >= '0' && v <= '9');
            char sval[2] = {v, 0};
            bool is_dash = v == '-';
            // R-hand types, L is for members only
            int imatch = index_of(special, sval);
            if (!start_numeric || !cont_numeric)
                if (isspace(v) || index_of(special, sval) >= 0) {
                    if (last_dash && (index - start) > 1) {
                        i32 vb = idx(input_string, index - 2); // allow the -- sequence, disallow - at end of tokens
                        index -= vb != '-';
                    }
                    break;
                }
            index += 1;
            last_dash = is_dash;
        }

        string crop = mid(input_string, start, index - start);
        push(tokens, (Au)token(
                         chars, crop->chars,
                         indent, indent,
                         source, src,
                         line, line_num,
                         column, start - line_start));
    }
    return tokens;
}

token silver_read_if(silver a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        next(a);
        return n;
    }
    return null;
}

Au silver_read_literal(silver a, Au_t of_type) {
    token n = element(a, 0);
    if (!n) return null;
    Au res = get_literal(n, of_type);
    if (res) {
        next(a);
        return res;
    }
    return null;
}

string silver_read_string(silver a) {
    token n = element(a, 0);
    if (n && instanceof(n->literal, string)) {
        string token_s = string(n->chars);
        string result = mid(token_s, 1, token_s->count - 2);
        next(a);
        return result;
    }
    return null;
}

Au silver_read_numeric(silver a) {
    token n = element(a, 0);
    Au_t au = n ? isa(n->literal) : null;
    if (au == typeid(f64) || au == typeid(i64) || au == typeid(shape)) {
        shape sh = instanceof(n->literal, shape);
        Au res = n->literal;
        if (sh && sh->count == 1) {
            res = _i64(sh->data[0]);
        }
        next(a);
        return res;
    }
    return null;
}

static etype next_is_class(silver a, bool read_token) {
    token t = peek(a);
    if (!t)
        return null;
    if (eq(t, "class")) {
        if (read_token)
            consume(a);
        return etypeid(Au);
    }
    
    etype f = elookup(t->chars);
    if (is_class(f)) {
        if (read_token)
            consume(a);
        return f;
    }
    return null;
}

string silver_peek_etype(silver a) { // we need a peek etype
    token n = element(a, 0);
    return null;
}

string silver_peek_def(silver a) { // we need a peek etype
    token n = element(a, 0);
    Au_t top = top_scope(a);
    etype t = u(etype, top);
    etype rec = is_rec(t) ? t : null;
    if (!rec && next_is_class(a, false))
        return string(n->chars);

    if (n && is_keyword((Au)n))
        if (eq(n, "import") || eq(n, "export") || eq(n, "func") || eq(n, "cast") ||
            eq(n, "class") || eq(n, "enum") || eq(n, "struct"))
            return string(n->chars);
    return null;
}

Au silver_read_bool(silver a) {
    token n = element(a, 0);
    if (!n)
        return null;
    bool is_true = strcmp(n->chars, "true") == 0;
    bool is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool)
        next(a);
    return is_bool ? _bool(is_true) : null;
}

OPType silver_read_operator(silver a) {
    token n = element(a, 0);
    if (!n)
        return OPType__undefined;
    string found = (string)get(operators, (Au)n);
    if (found) {
        consume(a);
        return evalue(typeid(OPType), found->chars);
    }
    return OPType__undefined;
}

string silver_read_alpha_any(silver a) {
    token n = element(a, 0);
    if (n && isalpha(n->chars[0])) {
        next(a);
        return string(n->chars);
    }
    return null;
}

string silver_peek_alpha(silver a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        return string(n->chars);
    }
    return null;
}

enode parse_return(silver a);
enode parse_break(silver a);
enode parse_for(silver a);
enode parse_loop_while(silver a);
enode parse_if_else(silver a);
enode parse_ifdef_else(silver a);
static enode typed_expr(silver mod, enode n, array expr);
i32 read_enum(silver a, i32 def, Au_t etype);
efunc parse_func(silver a, Au_t mem, enum AU_MEMBER member_type, u64 traits, OPType assign_enum);
etype etype_resolve(etype t);
enode enode_value(enode mem);

bool is_loaded(Au n) {
    Au_t i = isa(n);
    if (i == typeid(etype)) return false;
    enode node = (enode)n;
    return node->loaded;
}

etype evar_type(evar a);

bool in_context(Au_t au, Au_t ctx) {
    while (au) {
        if (au->context == ctx) return true;
        if (ctx == ctx->context) break;
        ctx = ctx->context;
    }
    return false;
}

string read_alpha_macrofilter(silver a, bool is_decl) {
    enode res = null;
    string n = read_alpha(a);
    if (!n) return null;

    push_current(a);
    /*
    mem: Au_t sub for (int i = len(lex) - 1; i >= 0; i--) {
        Au_t au = (Au_t)lex->origin[i];
        while (au) {
            if (au->member_type == AU_MEMBER_TYPE || au->member_type == AU_MEMBER_FUNC)
                for (int ii = 0; ii < au->meta.count; ii++) {
                    Au_t m = (Au_t)au->meta.origin[ii];
                    if (m->ident && strcmp(m->ident, f) == 0)
                        return m;
                }
            for (int ii = 0; ii < au->members.count; ii++) {
                Au_t m = (Au_t)au->members.origin[ii];
                if (m->ident && strcmp(m->ident, f) == 0)
                    return m;
            }
            Au_t au_isa = isa(au);
            if (!is_class((Au)au)) break;
            if (au->context == au) break;
            au = au->context;
        }
    }*/

    bool next_is_paren = next_is(a, "(");
    Au_t mem = null;
    bool use_name;
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t au = (Au_t)a->lexical->origin[i];
        while (au) {
            if (au->member_type == AU_MEMBER_TYPE || au->member_type == AU_MEMBER_FUNC)
                for (int ii = 0; ii < au->meta.count; ii++) {
                    Au_t m = (Au_t)au->meta.origin[ii];
                    if (m->ident && strcmp(m->ident, n->chars) == 0) {
                        mem = m;
                        goto mem_set;
                    }
                }
            for (int ii = 0; ii < au->members.count; ii++) {
                Au_t m = (Au_t)au->members.origin[ii];
                if (m->ident && strcmp(m->ident, n->chars) == 0) {
                    bool allow = true;
                    if (au->member_type == AU_MEMBER_MACRO) {
                        macro mac = u(macro, m);
                        verify(mac, "unresolved macro: %o", m);
                        allow = !mac->params || next_is_paren;
                    }
                    if (allow) {
                        mem = m;
                        goto mem_set;
                    }
                }
            }
            Au_t au_isa = isa(au);
            if (!is_class((Au)au)) break;
            if (au->context == au) break;
            au = au->context;
        }
    }
    mem_set:
    use_name = (is_decl || mem != null);
    pop_tokens(a, use_name);
    return use_name ? n : null;
}

enode silver_parse_member(silver a, ARef assign_type, Au_t in_decl) {
    OPType assign_enum = OPType__undefined;
    Au_t   top     = top_scope(a);
    etype  rec_top = context_record(a);
    silver module  =  !is_cmode(a) && (top->is_namespace) ? a : null;
    enode  f       =  !is_cmode(a) ? context_func(a) : null;
    bool   in_rec  = rec_top && rec_top->au == top;

    if (assign_type) *(OPType*)assign_type = OPType__undefined;

    push_current(a);

    enode  mem                = null;
    string alpha              = null;
    int    depth              = 0;
    bool   skip_member_check  = false;

    if (module) {
        string alpha = peek_alpha(a);
        if (alpha) {
            enode m = (enode)elookup(alpha->chars); // silly read as string here in int [ silly.len ]
            etype mdl = m ? resolve(m) : null;
            if (mdl && (mdl->au->member_type == AU_MEMBER_TYPE || mdl->au->member_type == AU_MEMBER_MODULE))
                skip_member_check = true;
                // might replace type[...] -> i32[array]
                // specifying a type gives us more types from an open concept
        }
    }

    for (;!skip_member_check;) {
        bool first = !mem;

        alpha = read_alpha_macrofilter(a, in_decl != null);
        if (alpha && eq(alpha, "size")) {
            alpha = alpha;
        }
        if (!alpha) {
            validate(mem == null, "expected alpha-ident after .");
            break;
        }

        /// Namespace resolution (only on first iteration)
        bool ns_found = false;
        if (first) {
            members (a->au, im) {
                if (!im->is_namespace || im->is_nameless) continue;
                if (im->ident && eq(alpha, im->ident)) {
                    string module_name = alpha;
                    validate(read_if(a, "."), "expected . after module-name: %o", alpha);
                    alpha = read_alpha(a);
                    validate(alpha, "expected alpha-ident after module-name: %o", module_name);
                    mem = (enode)elookup(alpha->chars);
                    if (mem && mem->au->is_namespace) {
                        validate(mem, "%o not found in module-name: %o", alpha, module_name);
                        ns_found = true;
                    } else {
                        // lexically overridden
                        mem = null;
                        break;
                    }
                }
            }
        }

        /// Lookup or resolve member
        if (!ns_found) {
            // we may only define our own members within our own space
            if (first) {

                if (eq(alpha, "super")) {
                    validate(rec_top, "super only valid in class context");
                    mem = (enode)enode(mod, (aether)a, au, rec_top->au->context, avoid_ftable, true, target, f->target); // with this node, we must not use the function table associated to the target
                }
                else if (!in_rec) {
                    // try implicit 'this' access in instance methods
                    if (!mem && f && f->target) {
                        //mem = (enode)elookup(alpha->chars);
                        mem = (enode)elookup(alpha->chars);
                        if (mem && !mem->au->is_static) {
                            etype ftarg = etype_resolve((etype)f->target);
                            if (ftarg && in_context(mem->au, ftarg->au)) {
                                mem = access(f->target, alpha);
                            }
                        }

                    } else if (!f || !f->target) {
                        //mem = (enode)elookup(alpha->chars);
                        mem = (enode)elookup(alpha->chars);
                        Au minfo = head(mem);
                        minfo = minfo;
                    }
                }
                
                if (!mem) {
                    token tm1 = element(a, -2); // sorry for the mess (coin-flip)
                    verify(next_is(a, ":") || (tm1 && index_of(keywords, (Au)tm1) >= 0), "unknown identifier %o", alpha);
                    validate(!find_member(top, alpha->chars, 0, false), "duplicate member: %o", alpha);
                    Au_t m = def_member(top, alpha->chars, null, AU_MEMBER_DECL, 0); // this is promoted to different sorts of members based on syntax
                    if (eq(alpha, "test2_func")) {
                        alpha = alpha;
                    }
                    mem = (enode)edecl(mod, (aether)a, au, m, meta, null);
                    break;
                }
                    
            } else if (instanceof(mem, enode) && !is_loaded((Au)mem)) {
                // Subsequent iterations - access from previous member
                verify(mem && mem->au, "cannot resolve from null member");
                
                // Load previous member to traverse into it
                enode prop = e_load(a, mem, null);
                mem = access(prop, alpha);
            } else {
                Au info = head(mem);
                mem = access(mem, alpha);
            }

            Au_t mem_type = isa(mem);
            if (!in_decl && (next_is(a, "[") || instanceof(mem, macro) || is_func((Au)mem) || inherits(mem->au->src, typeid(lambda)))) {
                print_tokens(a, "parsing member expr");
                mem = parse_member_expr(a, mem);
            }
        }

        // check if there's more chaining
        bool br = read_if(a, ".") == null;
        if (br) {
            if (a->in_ref) {
                break;
            }
            // final load if needed [ assign_type, when set, indicates a L-hand side parse ]
            if (instanceof(mem, enode) && !is_loaded((Au)mem) && !assign_type) {
                Au_t isa_type = isa(mem);
                mem = enode_value(mem);
            }
            break;
        }
        
        // More chaining - push context for next iteration
        validate(!is_func((Au)mem), "cannot resolve into function");
        if (mem->au && !module) {
            push_scope(a, (Au)mem);
            depth++;
        }
    }

    /// restore namespace after resolving emember
    for (int i = 0; i < depth; i++)
        pop_scope(a);

    bool save_tokens = true;
    if (isa(mem) == typeid(etype)) {
        mem = null;
        save_tokens = false;
    }
    pop_tokens(a, save_tokens);

    if (assign_type && mem) {
        token k = element(a, 0);
        if  (!k) return mem;
        num assign_index = index_of(assign, (Au)k);
        if (assign_index >= 0) {
            next(a);
            *(OPType*)assign_type = eq(k, ":") ? OPType__bind : (OPType__bind + assign_index);
        }
    }
    return mem;
}

etype pointer(aether, Au);

enode block_builder(silver a, array block_tokens, Au unused);

enode parse_sub(silver a, etype rtype) {
    array body = read_body(a);
    validate(body, "expected sub body");
    
    // e_sub in aether creates merge block, catcher, 
    // parses body via builder callback,
    // builds phi from all return points
    subprocedure build_body = subproc(a, block_builder, null);
    enode res = e_subroutine(a, rtype, body, build_body);
    return res;
}

enode silver_read_enode(silver a, etype mdl_expect, bool from_ref) { sequencer
    print_tokens(a, "read-node");
    bool      cmode     = is_cmode(a);
    array     expr      = null;
    token     peek      = peek(a);
    bool      is_expr0  = !cmode && a->expr_level == 0;
    bool      is_static = is_expr0 && read_if(a, "static") != null;
    string    kw        = is_expr0 ? peek_keyword(a) : null;
    etype     rec_ctx   = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    Au_t      top       = top_scope(a);
    etype     rec_top   = (!cmode && is_rec(top)) ? u(etype, top) : null;
    enode     f         = !cmode ? context_func(a) : null;
    silver    module    = !cmode && (top->is_namespace) ? a : null;
    enode     mem       = null;

    int slen = len(a->stack);

    // handle typed operations, converting to our expected model (if no difference, it passes through)
    if (a->expr_level > 0 && peek && is_alpha(peek)) {
        etype mdl_found = read_etype(a, null);
        if (mdl_found) {
            if (a->in_ref) mdl_found = pointer((aether)a, (Au)mdl_found);
            array expr = read_initializer(a);
            enode res0 = null;
            if (!len(expr) && read_if(a, "sub")) {
                res0 = e_create(a, mdl_expect, (Au)parse_sub(a, mdl_found)); 
            } else if (!len(expr) && read_if(a, "asm")) {
                res0 = e_create(a, mdl_expect, (Au)parse_asm(a, mdl_found));
            } else {
                push_tokens(a, (tokens)expr, 0);
                res0 = read_enode(a, mdl_found, false);
                pop_tokens(a, false);
            }
            enode conv = e_create(a, mdl_expect, (Au)res0);
            return conv;
        }
    }

    shape sh = (shape)read_literal(a, typeid(shape));
    if (sh && (sh->count == 1 || sh->explicit)) {
        enode op;
        if (mdl_expect == typeid(shape)) 
            op = e_operand(a, (Au)sh, etypeid(shape));
        else
            op = e_operand(a, _i64(sh->data[0]), mdl_expect);
        
        return op; // otherwise interpreted as an i64
    }

    Au lit = read_literal(a, null);
    if (lit) {
        a->expr_level++;
        enode res = e_operand(a, lit, mdl_expect);
        a->expr_level--;
        return e_create(a, mdl_expect, (Au)res);
    }
    
    if (!cmode && next_is(a, "$", "(")) {
        consume(a);
        consume(a);
        fault("shell syntax not implemented for 88");
    }

    if (read_if(a, "sizeof")) {
        bool read_br = (cmode && read_if(a, "(")) || (!cmode && read_if(a, "["));
        etype mdl = read_etype(a, null);
        
        if (read_br)
            verify((cmode && read_if(a, ")")) || (!cmode && read_if(a, "]")), "expected closing-bracket");
        
        return e_operand(a, _i64(mdl->au->typesize), mdl_expect);
    }

    if (!cmode && read_if(a, "typeof")) {
        bool read_br = read_if(a, "[") != null;
        etype mdl = read_etype(a, null);
        
        if (read_br)
            verify(read_if(a, "]"), "expected closing-bracket after typeof");
        
        return e_create(a, (etype)mdl_expect, (Au)e_typeid(a, mdl));
    }    

    if (!cmode && read_if(a, "new")) {
        etype mdl = read_etype(a, null);
        enode sz  = null;
        if (read_if(a, "[")) {
            sz = parse_expression(a, etypeid(shape));
            validate(read_if(a, "]"), "expected closing-bracket after new Type [");
        }
        return e_create(a,
            (etype)pointer((aether)a, (Au)mdl->au),
            (Au)e_vector(a, mdl, sz));
    }

    if (!cmode && read_if(a, "null"))
        return e_null(a, mdl_expect);

    if (cmode && read_if(a, "*")) {
        enode n = read_enode(a, null, false);
        return deref(n);
    }

    // parenthesized expressions
    if (next_is(a, "(")) {
        consume(a);
        // support C-style cast here, only in cmode (macro definitions)
        if (cmode) {
            push_current(a);
            array meta = null;
            etype inner = read_etype(a, null);
            if (inner) {
                if (next_is(a, ")")) {
                    consume(a);
                    pop_tokens(a, true);
                    enode res = e_create(a, inner, (Au)parse_expression(a, inner));
                    return e_create(a, mdl_expect, (Au)res);
                } else {
                    pop_tokens(a, false);
                    a->expr_level = 0;
                    return null;
                }
            }
            pop_tokens(a, false);
        }
        a->parens_depth++;
        enode expr = parse_expression(a, null); // Parse the expression
        verify(read_if(a, ")"), "expected ) after expression");
        a->parens_depth--;
        return e_create(a, mdl_expect, (Au)
            parse_ternary(a, (enode)expr, (etype)mdl_expect));
    }

    if (!cmode && next_is(a, "[")) {
        validate(mdl_expect, "expected model name before [");
        array expr = read_within(a);
        // we need to get mdl from argument
        enode r = typed_expr(a, (enode)mdl_expect, (array)expr);
        return r;
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read_if(a, "!") || (!cmode && read_if(a, "not"))) {
        validate(!from_ref, "unexpected not after ref");
        token t = peek(a);
        enode expr = read_enode(a, null, false); // Parse the following expression
        return e_create(a,
            mdl_expect, (Au)e_not(a, expr));
    }

    // bitwise NOT operator
    else if (read_if(a, "~")) {
        validate(!from_ref, "unexpected ~ after ref");
        enode expr = read_enode(a, null, false);
        return e_create(a,
            mdl_expect, (Au)e_bitwise_not(a, expr));
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    else if (!cmode && read_if(a, "typeid")) { // todo: merge with isa, eventually implement with a const expression in silver implementation
        validate(!from_ref, "unexpected typeid after ref");
        bool bracket = false;
        if (next_is(a, "[")) {
            consume(a); // Consume '['
            bracket = true;
        }
        enode expr = parse_expression(a, null); // Parse the type expression
        if (bracket) {
            assert(next_is(a, "]"), "Expected ']' after type expression");
            consume(a); // Consume ']'
        }
        return e_create(a,
            mdl_expect, (Au)expr); // Return the type reference
    }

    // 'ref' operator (reference)
    // we only allow one reference depth, for multiple we would resort to using defined types
    // this is to make code cleaner, with more explicit definition
    // we must use in-ref state only at neighboring calls
    else if (!cmode && read_if(a, "ref")) {
        validate(!from_ref, "unexpected double-ref (use type definitions)");
        a->in_ref = true;
        enode expr = read_enode(a, null, true);
        a->in_ref = false;

        if (next_is(a, "["))
            expr = parse_member_expr(a, expr);

        return e_create(a, mdl_expect, (Au)expr);
    }

    // we may only support a limited set of C functionality for #define macros
    mem = parse_member(a, null, null); // we never parse assignment here

    if (!mem && cmode) return null;
    validate(!instanceof(mem, edecl), "unexpected declaration of member %s", mem->au->ident);

    return (f && mdl_expect) ? e_create(a, mdl_expect, (Au)mem) : (enode)mem;
}

enode parse_switch(silver a);


// this shouldnt change the read cursor, its for reading types from c macros
etype etype_infer(silver a) {
    push_current(a);
    etype t = read_etype(a, null);
    pop_tokens(a, false);
    if (!t) {
        push_current(a);
        Au lit = read_literal(a, null);
        if (lit) {
            if      (isa(lit) == typeid(string)) t = etypeid(string);
            else if (isa(lit) == typeid(i64))    t = etypeid(i64);
            else if (isa(lit) == typeid(u64))    t = etypeid(u64);
            else if (isa(lit) == typeid(i32))    t = etypeid(i32);
            else if (isa(lit) == typeid(u32))    t = etypeid(u32);
            else if (isa(lit) == typeid(f32))    t = etypeid(f32);
            else if (isa(lit) == typeid(f64))    t = etypeid(f64);
            else
                fault("implement literal %s in infer_etype", isa(lit)->ident);
        }
        pop_tokens(a, false);
    }
    return t;
}

static tokens map_initializer(silver a, string field, tokens def_tokens) {
    if (!a->defs || !contains(a->defs, (Au)field)) return def_tokens;
    tokens value = (tokens)get(a->defs, (Au)field);
    return value;
}

enode parse_statement(silver a)
{
    enode f = context_func(a);
    verify(!f || !f->au->is_mod_init, "unexpected init function");

    print_tokens(a, "parse-statement");

    a->last_return      = null;
    a->expr_level       = 0;

    Au_t      top       = top_scope(a);
    silver    module    = is_module(top) ? a : null;

    if (module && !next_is(a, "func") && peek_def(a))
        return (enode)read_def(a);

    // standard statements first
    if (f) {
        if (next_is(a, "no-op")) {
            consume(a);
            return e_noop(a, null);
        }
        
        if (next_is(a, "return")) return parse_return (a);
        if (next_is(a, "break"))  return parse_break  (a);
        if (next_is(a, "for"))    return parse_for    (a);
        if (next_is(a, "if"))     return parse_if_else(a);
        if (next_is(a, "switch")) return parse_switch (a);
    }
    
    if (next_is(a, "ifdef")) return parse_ifdef_else(a);

    interface access    = read_enum(a, interface_undefined, typeid(interface));
    bool      is_static = read_if(a, "static") != null;

    bool      is_lambda = !f ?
        read_if(a, "lambda")     != null : false;
    bool      is_func   = !f && !is_lambda ?
        read_if(a, "func")       != null : false;
    bool      is_cast   = !f && !is_static && !(is_func|is_lambda) ?
        read_if(a, "cast")       != null : false;
    bool      is_oper   = !f && !is_static && !(is_func|is_lambda) && !is_cast ?
        read_if(a, "operator")   != null : false;
    bool      is_ctr    = !f && !is_static && !(is_func|is_lambda) && !is_cast && !is_oper ?
        read_if(a, "construct")  != null : false;
    bool      is_idx    = !f && !is_static && !(is_func|is_lambda) && !is_cast && !is_oper && !is_ctr ?
        read_if(a, "index")      != null : false;

    if (is_func)
        is_func = is_func;

    OPType assign_enum = OPType__undefined;
    
    enode mem = (!is_cast && !is_oper && !is_idx && !is_ctr) ?
        parse_member(a, (ARef)&assign_enum,
            is_func ? typeid(efunc) : ((access || f) ? typeid(evar) : null)) : null;
    Au_t mem_info = isa(mem);
    validate(!is_func || !instanceof(mem, efunc), "redefinition of %o", mem);

    validate(!(is_lambda|is_func) || mem,
        "expected member identifier to follow function or lambda");

    validate(!is_static || mem,
        "expected member identifier to follow static");

    validate(mem || (!mem || access == interface_undefined),
        "expected member-name after access '%o'", estring(typeid(interface), access));

    // if no access then full access
    if (!access) access = interface_public;
    
    push_current(a);

    OPType op_type = is_oper ? read_operator(a) : OPType__undefined;
    enode  e       = null;
    
    if (mem!=null || is_ctr || is_idx || is_oper || is_lambda || is_func || is_cast)
    {
        validate(!is_oper || op_type != OPType__undefined, "operator required");
        
        // check if this is a nested, static member (we need to back off and read_enode can handle this)
        Au_t top_type = isa(u(etype, top));

        etype      rec_top    = is_rec(top) ? u(etype, top) : null;
        statements in_code    = context_code(a);
        bool       is_const   = mem && mem->au->is_const;

        if (is_func|is_lambda|is_ctr|is_idx|is_cast|is_oper) {
            if (module || rec_top) {
                u64 traits = (is_static ? AU_TRAIT_STATIC : 
                             (is_lambda ? 0 : (rec_top ? AU_TRAIT_IMETHOD : 0))) | 
                             (is_lambda ? AU_TRAIT_LAMBDA : 0);
                Au_t au = mem ? mem->au : null;
                enum AU_MEMBER ftype = is_lambda ? AU_MEMBER_FUNC      :
                    is_ctr    ? AU_MEMBER_CONSTRUCT : is_cast ?
                                AU_MEMBER_CAST      : is_idx  ?
                                AU_MEMBER_INDEX     : AU_MEMBER_FUNC;

                Au_t top = top_scope(a);
                aether top_user;

                if (!au)
                     au = def(top_scope(a), null, AU_MEMBER_DECL, 0);
                
                set(a->registry, (Au)au, (Au)null);
                e = (enode)parse_func(a, au, // for cast, we read the rtype first; for others, its parsed after ->
                    ftype,
                    traits, op_type);
                e->au->access_type = (u8)access;
                efunc fn = (efunc)get(a->registry, (Au)au);
                verify(fn && fn == e && fn->au == au, "unexpected registration state");
                //rm(a->registry, (Au)au);
                //set(a->registry, (Au)au, (Au)hold(e));
            }

        }
        else if (rec_top || module) {
            bool  is_inline = true;
            bool is_f = is_idx|is_ctr|is_lambda|is_func|is_cast;
            verify(assign_enum == OPType__bind || is_f, "invalid member syntax, expected member:type[ initializer ]");
            enode rtype = (mem && mem->au->member_type == AU_MEMBER_DECL) && !is_f ?
                (enode)read_etype(a, null) : null;
            array expr = read_initializer(a); // we allow inline only for assignment operations
            //verify(!assign_enum || expr, "expected expression after %i", assign_enum);

            mem->au->member_type = AU_MEMBER_VAR;
            mem->au->src = rtype->au;
            mem->au->is_static = is_static;
            Au f = get(a->registry, (Au)mem->au);
            Au_t info = isa(f);
            //rm(a->registry, (Au)mem->au);
            Au_t au = mem->au;
            set(a->registry, (Au)au, null);
            Au f2 = get(a->registry, (Au)au);
            Au_t info2 = isa(f2);
            mem = (enode)evar(mod, (aether)a, au, au, loaded, false, meta, rtype->meta, initializer,
                (tokens)map_initializer(a, string(au->ident), (tokens)expr));
            e = (enode)mem;

            efunc fn = (efunc)get(a->registry, (Au)e->au);
            verify(fn && fn == e && fn->au == mem->au, "unexpected registration state");

            //au_register(e->au, (etype)e);
            if (is_static || module) {
                if (is_static && rec_top) {
                    verify(rec_top, "invalid use of static (must be a class member, not a global item -- use intern for module-interns)");
                    mem->au->alt = (cstr)cstr_copy((cstr)((string)(f(string, "%o_%o", rec_top, e))->chars));
                }
                etype_implement((etype)e);
            }

        } else if (assign_enum) {
            validate(mem, "expected member");

            a->left_hand = false;
            a->expr_level++;
            mem->au->is_const = module != null;
            e = parse_assignment(a, (enode)mem, assign_enum, mem->au->is_const);
            a->expr_level--;
            a->left_hand = true;

        } else {
            // default
            validate(!assign_enum, "unexpected assignment");
        }
        
    } else {
        validate(!is_cast, "expected type after cast keyword");
    }

    pop_tokens(a, e != null); // if its a type, we consume the tokens, otherwise we let read_enode handle it

    if (!mem && !e && peek(a)) {
        a->left_hand = true;
        e = parse_expression(a, null); /// at module level, supports keywords
        a->left_hand = false;
    }
    return e;
}

enode parse_statements(silver a, bool unique_members) {
    if (unique_members)
        push_scope(a, (Au)new(statements, mod, (aether)a, au, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0)));

    enode vr = null;
    while (peek(a)) {
        vr = parse_statement(a);
    }
    if (unique_members)
        pop_scope(a);
    return vr;
}

void silver_incremental_resolve(silver a) {
    members(a->au, mem) {
        etype e = u(etype, mem);
        if (is_func((Au)mem) && !mem->is_system && e != a->fn_init && !e->user_built) {
            Au_t e_isa = isa(e);
            build_fn(a, (efunc)e, null, null);
        }
    }
    members(a->au, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && !rec->parsing && !rec->user_built) {
            build_record(a, rec);
        }
    }
}


ARef lltype(Au a);

Au_t alloc_arg(Au_t context, symbol ident, Au_t arg);

int method_id(Au_t ctx, Au_t avoid) {
    int count = 0;
    while (ctx) {
        members(ctx, m) {
            if (m->is_imethod && !m->is_override && m != avoid)
                count++;
        }
        if (ctx->context == ctx)
            break;
        ctx = ctx->context;
    }
    return count;
}

efunc parse_func(silver a, Au_t mem, enum AU_MEMBER member_type, u64 traits, OPType assign_enum) {
    etype  rtype   = null;
    string name    = string(mem->ident);
    bool   is_cast = member_type == AU_MEMBER_CAST;
    //etype rec_ctx = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    etype rec_ctx = context_class(a);

    validate(read_if(a, "["), "expected function args [");
    Au_t au = mem; //def(top_scope(a), ident ? ident->chars : null, AU_MEMBER_FUNC, traits);
    verify(mem->member_type == AU_MEMBER_DECL, "unknown declaration found in parsing function");
    au->member_type = member_type;
    au->index = rec_ctx ? method_id(rec_ctx->au, au) : 0;
    au->traits = traits;
    au->module = a->au;

    if (is_cast) {
        rtype = read_etype(a, null);
        name = form(string, "cast_%o", rtype);
    }
    
    if (!rec_ctx)
        rec_ctx = context_struct(a);
    else {
        au->is_override = find_member(
            rec_ctx->au->context, name->chars, member_type, true) != null;
    }
    // fill out args in function model
    bool is_instance = (traits & AU_TRAIT_IMETHOD) != 0 || (member_type == AU_MEMBER_CAST);
    if (is_instance) {
        Au_t top    = top_scope(a);
        Au_t rec    = is_rec(top);
        verify(rec, "cannot parse IMETHOD without record in scope");
        Au_t au_arg = alloc_arg(au, "a", is_struct(rec) ? pointer((aether)a, (Au)rec)->au : rec);
        au_arg->is_target = true;
        array_qpush((array)&au->args, (Au)au_arg);
    }

    bool is_lambda = (traits & AU_TRAIT_LAMBDA) != 0;
    bool in_context = false;
    
    // create model entries for the args (enodes created on func init)
    push_scope(a, (Au)au);
    bool first = true;
    Au_t target = null;
    for (;;) {
        if (read_if(a, "]"))
            break;
        
        bool skip = false;

        if (!first && is_lambda && read_if(a, "::")) {
            skip = true;
            in_context = true;
        }
        validate(skip || first || read_if(a, ","), "expected comma separator between arguments");

        push_current(a);
        etype peek_type = read_etype(a, null);
        pop_tokens(a, peek_type != null);
        
        string n  = peek_type ? null : read_alpha(a); // optional
        array  ar = in_context ? (array)&au->members : (array)&au->args;

        validate(peek_type || read_if(a, ":"), "expected seperator between name and type");
        etype  t = peek_type ? peek_type : read_etype(a, null); // we need to avoid the literal check in here!

        if (member_type == AU_MEMBER_CONSTRUCT && !len(name))
            name = form(string, "with_%o", t);
        else if (member_type == AU_MEMBER_CAST && !len(name))
            name = form(string, "cast_%o", t);
        
        if (!t) {
            read_etype(a, null);
            t = t;
        }
        verify(t, "expected alpha-numeric identity for type or name");

        Au_t   au_arg = alloc_arg(au, n ? n->chars : null, t->au);
        array_qpush(ar, (Au)au_arg);
        if (first)
            first = false;
    }
    pop_scope(a);

    au->ident = cstr_copy(name->chars); // free other instance
    //au->rtype = rtype->au;

    string fname = rec_ctx ? f(string, "%o_%o", rec_ctx, name) : (string)name;
    au->alt = rec_ctx ? cstr_copy(fname->chars) : null;

    if (member_type != AU_MEMBER_CAST && read_if(a, "->")) {
        validate(!rtype, "unexpected -> after cast type");
        rtype = read_etype(a, null);
    } else if (!rtype) {
        // validate it is not a cast
        rtype = etypeid(none);
    }

    au->rtype = rtype->au;

    bool is_using = read_if(a, "using") != null;
    codegen cgen = null;

    // check if using generative model
    if (is_using) {
        token codegen_name = (token)read_alpha(a);
        verify(codegen_name, "expected codegen-identifier after 'using'");
        cgen = (codegen)get(a->codegens, (Au)codegen_name);
        verify(cgen, "codegen identifier not found: %o", codegen_name);
    }

    bool is_init    = rec_ctx && eq(name, "init");
    bool is_dealloc = rec_ctx && eq(name, "dealloc");

    array b = (array)read_body(a);
    // all instances of func enode need special handling to bind the unique user space to it; or, we could make efunc

    efunc func = efunc(
        mod,    (aether)a,
        au,     au,
        body,   (tokens)b,
        remote_code, !is_using && !len(b),
        has_code,    len(b) || is_init || is_dealloc || cgen,
        cgen,   cgen,
        used,   true,
        target, null);
    
    print_all(a, "body", b);
    return func;
}

static etype model_adj(silver a, etype mdl) {
    while (a->cmode && read_if(a, "*"))
        mdl = pointer((aether)a, (Au)mdl);
    return mdl;
}

static etype read_named_model(silver a) {
    etype mdl = null;
    push_current(a);

    bool any = read_if(a, "any") != null; // this should be a primitive type, with a trait for any
    if (any) {
        pop_tokens(a, true);
        return etypeid(Au);
    }

    string alpha = read_alpha(a);
    if (alpha && !next_is(a, ".")) {
        mdl = elookup(alpha->chars);
        if (instanceof(mdl, evar)) {
            pop_tokens(a, false);
            return null;
        }
    }
    pop_tokens(a, mdl != null); /// save if we are returning a model
    return mdl;
}

static shape read_shape(silver a) {
    shape s = (shape)read_literal(a, typeid(shape));
    if (!s) {
        i64* i = (i64*)read_literal(a, typeid(i64));
        if (i) {
            i64* cp = (i64*)calloc(1, sizeof(i64));
            *cp = *i;
            s = shape_from(1, cp);
        }
    } else {
        s = s;
    }
    return s;
}

etype read_etype(silver a, array* p_expr) {
    etype mdl   = null;
    array expr  = null;
    array meta  = null;
    array types = array();
    array sizes = array();
    
    token f = peek(a);
    if (!f || f->literal) return null;

    /*
    if  ((!f || f->literal || !is_alpha(f)) && !next_is(a, "ref")) {
        if (f->literal) {
            Au_t id = isa(f->literal);
            if (id == typeid(shape)) {
                shape s = (shape)f->literal;
                if (s->explicit || s->count > 1)
                    return etypeid(shape);
                return etypeid(i64);
            }
            return u(etype, id);
        }
        return null;
    }*/

    push_current(a);
    bool is_ref = read_if(a, "ref") != null;
    bool  explicit_sign = !mdl && read_if(a, "signed") != null;
    bool  explicit_un   = !mdl && !explicit_sign && read_if(a, "unsigned") != null;
    etype prim_mdl      = null;

    if (!mdl && !explicit_un) {
        if      (read_if(a, "char"))  prim_mdl = etypeid(i8);
        else if (read_if(a, "short")) prim_mdl = etypeid(i16);
        else if (read_if(a, "int"))   prim_mdl = etypeid(i32);
        else if (read_if(a, "float")) prim_mdl = etypeid(f32);
        else if (read_if(a, "double")) prim_mdl = etypeid(f64);
        else if (read_if(a, "half"))  prim_mdl = etypeid(bf16);
        else if (read_if(a, "object")) prim_mdl = etypeid(Au);
        else if (read_if(a, "long"))  prim_mdl = read_if(a, "long")?
            etypeid(i64) : etypeid(i32);
        else if (explicit_sign)
            prim_mdl = etypeid(i32);
        
        if   (prim_mdl)  prim_mdl = model_adj(a, prim_mdl);
        mdl = prim_mdl ? prim_mdl : read_named_model(a);

        if (mdl && mdl->au->is_meta)
            mdl = pointer((aether)a, (Au)mdl->au->src);
        
        if (mdl && mdl->au->member_type != AU_MEMBER_TYPE && !mdl->au->is_meta) {
            pop_tokens(a, false);
            validate(!is_ref, "expected valid type after ref");
            return null;
        }

        // is_class(mdl) && mdl->au->meta.count

        if (mdl) {
            // silver is about introducing more syntax only when you require complex containment
            // this applies to methods with no args at method level 0 and [ args ] at > 0
            // array map<string[int]> is the same exact way here
            bool has_depth_meta = read_if(a, "<") != null;
            //validate(mdl->au->meta.count == 0 || a->etype_level == 0 || has_depth_meta, "expected <meta> containment at depth");
            bool read_meta =  mdl->au->meta.count && is_class(mdl);
            Au_t meta0_src = (has_depth_meta || a->etype_level == 0) && read_meta ? 
                au_arg_type(mdl->au->meta.origin[0]) : null;
            array meta_args = null;

            // read shape literal, or type 
            // (to generalize types offering different literals 
            //  we could express a trait bit take_literally or something)
            if (meta0_src ==  typeid(shape)) {
                shape    s =  read_shape(a);
                validate(s && isa(s) == typeid(shape), "expected shape description, found %o", peek(a));
                meta_args  =  a(s);
            } else if (meta0_src) {
                a->etype_level++;
                etype t = read_etype(a, null);
                validate(t, "expected meta type, found %o", peek(a));
                meta_args = a(t);
                a->etype_level--;
            }
            int rem = mdl->au->meta.count - 1;
            if (rem > 0 && read_meta && (has_depth_meta || a->etype_level == 0) ) {
                // second arg and on are joined within [ ]
                validate(read_if(a, "["),
                    "expected [ after first meta type %o", first_element(meta_args));
                
                for (int i = 0; i < rem; i++) {
                    Au_t meta_src =  au_arg_type(mdl->au->meta.origin[1 + i]);
                    if  (meta_src == typeid(shape)) {
                        token t = peek(a);
                        Au_t t_isa = isa(t->literal);
                        shape    s =  read_shape(a);
                        if (s) {
                            validate(s, "expected shape description, found %o", peek(a));
                            push(meta_args, (Au)s);
                        }
                    } else {
                        a->etype_level++;
                        etype imdl = read_etype(a, null);
                        a->etype_level--;
                        if (!imdl && meta_src == typeid(none))
                            break;
                        validate(imdl, "expected type, found %o", peek(a));
                        push(meta_args, (Au)imdl);
                    }
                    if (i >= (rem - 1))
                        break;

                    if (read_if(a, ","))
                        continue;

                    Au_t meta_src_next = au_arg_type(array_get((array)&mdl->au->meta, 1 + i + 1));
                    validate(meta_src_next == typeid(none),
                        "expected comma after meta type %o", last_element(meta_args));
                    break;
                }
                validate(read_if(a, "]"),
                    "expected [ after last meta type %o", last_element(meta_args));
            }
            
            // we read this at depth, so ajoined vector of types don't get unreadable when depth is to be displayed
            validate(!has_depth_meta || read_if(a, ">"), "expected <meta> containment at depth");
            
            // sorry for the mess [/flicks-coin]
            mdl  = meta_args ? etype(mod, (aether)a, au, mdl->au, meta, meta_args) : mdl;
            meta = null; // dont reconstruct our mdl again with more
        }

    } else if (!mdl && explicit_un) {
        if (read_if(a, "char"))  prim_mdl = etypeid(u8);
        if (read_if(a, "short")) prim_mdl = etypeid(u16);
        if (read_if(a, "int"))   prim_mdl = etypeid(u32);
        if (read_if(a, "long"))  prim_mdl = read_if(a, "long")? 
            etypeid(u64) : etypeid(u32);

        mdl = model_adj(a, prim_mdl ? prim_mdl : etypeid(u32));
    }

    if (mdl && mdl->au->member_type != AU_MEMBER_TYPE && !mdl->au->is_meta)
        mdl = null;

    etype t = (mdl && meta) ? etype(mod, (aether)a, au, mdl->au, meta, meta) : mdl;
    if (is_ref)
        t = pointer((aether)a, (Au)t);

    //if (p_expr) {
    //    *p_expr = read_initializer(a);
    //}
    pop_tokens(a, mdl != null); // if we read a model, we transfer token state
    return t;
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, efunc f, array query) {
    fault("must subclass codegen for usable code generation");
    return null;
}

// design-time for dictation
array read_dictation(silver a, array input) {
    // we want to read through [ 'tokens', image[ 'file.png' ] ]
    // also 'token here' 'and here' as two messages
    array result = array();

    push_tokens(a, (tokens)input, 0);
    while (read_if(a, "[")) {
        array content = array();
        while (peek(a) && !next_is(a, "]")) {
            if (read_if(a, "file")) {
                verify(read_if(a, "["), "expected [ after file");
                string file = (string)read_literal(a, typeid(string));
                verify(file, "expected 'path' of file in resources");
                path share = path_share_path();
                path fpath = f(path, "%o/%o", share, file);
                verify(exists(fpath), "path does not exist: %o", fpath);
                verify(read_if(a, "]"), "expected ] after file [ literal string path... ] ");
                push(content, (Au)fpath); // we need to bring in the image/media api
            } else {
                string msg = (string)read_literal(a, typeid(string));
                verify(msg, "expected 'text' message");
                push(content, (Au)msg);
            }
            read_if(a, ","); // optional for arrays of 1 dimension
        }
        verify(len(content), "expected more than one message entry");
        verify(read_if(a, "]"), "expected ] after message");

        push(result, (Au)content);
    }
    verify(len(result), "expected dictation message");
    pop_tokens(a, false);
    return result;
}

array chatgpt_generate_fn(chatgpt gpt, Au_t f, array query) {
    silver a = (silver)u(efunc, f)->mod;
    efunc fn = u(efunc, f);    
    array res = array(alloc, 32);

    // we need to construct the query for chatgpt from our query tokens
    // as well as the preamble system context
    // we have simple strings
    string key = f(string, "%s", getenv("CHATGPT"));
    verify(len(key),
           "chatgpt requires an api key stored in environment variable CHATGPT");

    map headers = m(
        "Authorization", f(string, "Bearer %o", key));

    uri addr = uri("POST https://api.openai.com/v1/chat/completions");
    sock chatgpt = sock(addr);
    bool connected = connect_to(chatgpt);
    verify(connected, "failed to connect to chatgpt (online access required for remote codegen)");
    string str_args = string();
    for (int i = 0; i < f->args.count; i++) {
        Au_t mem = (Au_t)f->args.origin[i];
        if (len(str_args))
            append(str_args, ",");
        concat(str_args, f(string, "%o: %o", mem, mem->type));
    }
    string signature = f(string, "func %o[%o] -> %o", f, str_args, f->rtype);

    // main system message
    map sys_intro = m(
        "role", string("system"),
        "content", f(string, "this is silver compiler, and your job is to write the code for inside of method: %o, "
                             "no [ braces ] containing it, just the inner method code; next we will provide entire module "
                             "source, so you know context and other components available",
                     signature));

    // include our module source code
    map sys_module = m(
        "role", (Au)string("system"),
        "content", (Au)a->source_raw);

    // now we need a silver document with reasonable how-to
    // this can be fetched from resource, as its meant for both human and AI learning
    path docs = path_share_path();
    path test_sf = f(path, "%o/docs/test.ag", docs);
    string test_content = (string)load(test_sf, typeid(string), null);
    map sys_howto = m(
        "role", string("system"),
        "content", test_content);

    array messages = a(sys_intro, sys_module, sys_howto);

    // now we have 1 line of dictation: ['this is text describing an image', image[ 'file.png' ] ]
    // for each dictation message, there is a response from the server which we also include as assistant
    // it must error if there are missing responses from the servea
    array dictation = read_dictation(a, (array)fn->body);

    each(dictation, array, msg) {
        array content = array();
        each(msg, Au, info) {
            map item;
            if (instanceof(info, path)) {
                string mime_type = mime((path)info);
                string b64 = base64((path)info);
                map m_url = m("url", f(string, "data:%o;base64,%o", mime_type, b64)); // data:image/png;base64,
                item = m("type", "image_url", "image_url", m_url);
            } else if (instanceof(info, string)) {
                item = m("type", "text", "text", info);
            } else {
                fault("unknown type in dictation: %s", isa(info)->ident);
            }
            push(content, (Au)item);
        }
        map user_dictation = m(
            "role", string("user"),
            "content", content);

        push(messages, (Au)user_dictation);
        path test_sf = f(path, "%o/docs/test.ag", docs);
    }

    map user = m(
        "role", string("user"),
        "content", string("write a function that adds the args a and b"));

    hold(messages);
    map body = m("model", string("gpt-5"), "messages", messages);

    return res;
}

static array import_build_commands(array input, symbol sym) {
    array res = array(alloc, 32);
    int token_line = -1;
    string cmd = null;

    each(input, token, t) {
        bool is_cmd = eq(t, sym);
        if (is_cmd || (t->line == token_line)) {
            if (!is_cmd) {
                if (!cmd)
                    cmd = string(alloc, 32);
                if (len(cmd))
                    append(cmd, " ");
                concat(cmd, (string)t);
            } else
                token_line = t->line;
        } else if (cmd) {
            token_line = -1;
            push(res, (Au)cmd);
            cmd = null;
        }
    }
    if (cmd) {
        push(res, (Au)cmd);
        cmd = null;
    }
    return res;
}

string import_config(array input) {
    string config = string(alloc, 128);
    int token_line = -1;
    each(input, token, t) {
        if (starts_with(t, ">")) {
            token_line = t->line;
        } else if (token_line >= 0 && t->line != token_line) {
            token_line = -1;
        }
        if (token_line == -1 && !starts_with(t, "-l")) {
            if (len(config))
                append(config, " ");
            concat(config, (string)t);
        }
    }
    return config;
}

string import_env(array input) {
    string env = string(alloc, 128);
    each(input, string, t) {
        if (isalpha(t->chars[0]) && index_of(t, "=") >= 0) {
            if (len(env))
                append(env, " ");
            concat(env, (string)t);
        }
    }
    return env;
}

string import_libs(array input) {
    string libs = string(alloc, 128);
    each(input, string, t) {
        if (starts_with(t, "-l")) {
            if (len(libs))
                append(libs, " ");
            concat(libs, (string)t);
        }
    }
    return libs;
}

static bool command_exists(cstr cmd) {
    char buf[256];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

static bool is_branchy(string n) {
    i32 ln = len(n);
    if (ln == 7) {
        for (int i = 0; i < ln; i++) {
            char l = tolower(n->chars[i]);
            if ((l >= 'a' && l <= 'f') || (l >= '0' && l <= '9'))
                continue;

            return true;
        }
        return false;
    }
    return true;
}

string command_run(command cmd);

static none checkout(silver a, path uri, string commit, array prebuild, array postbuild, string conf, string env) {
    path    install     = a->install;
    string  s           = cast(string, uri);
    num     sl          = rindex_of(s, "/");
    validate(sl >= 0, "invalid uri");
    string  name        = mid(s, sl + 1, len(s) - sl - 1);
    path    project_f   = f(path, "%o/checkout/%o", a->root_path, name);
    bool    debug       = false;
    string  config      = interpolate(conf, (Au)a);

    validate(command_exists("git"), "git required for import feature");

    // checkout or symlink to src
    if (!dir_exists("%o", project_f)) {
        path src_path = f(path, "%o/%o", a->src_loc, name);
        if (dir_exists("%o", src_path)) {
            vexec("symlink", "ln -s %o %o", src_path, project_f);
            project_f = src_path;
        } else {
            // we need to check if its full hash
            bool is_short = len(commit) == 7 && !is_branchy(commit);

            if (is_short) {
                vexec("remote", "git clone %o %o", uri, project_f); // FULL CHECKOUTS with short
                vexec("checkout", "git -C %o checkout %o", project_f, commit);
            } else {
                vexec("init", "git init %o", project_f);
                vexec("remote", "git -C %o remote add origin %o", project_f, uri);
                if (!commit) {
                    command c = f(command, "git remote show origin");
                    string res = run(c);
                    verify(starts_with(res, "HEAD branch: "), "unexpected result for git remote show origin");
                    commit = mid(res, 13, len(res) - 13);
                }
                vexec("fetch", "git -C %o fetch origin %o", project_f, commit);
                vexec("checkout", "git -C %o reset --hard FETCH_HEAD", project_f);
            }
        }
    }

    // we build to another folder, not inside the source, or checkout
    path build_f    = f(path, "%o/%s/%o", install, debug ? "debug" : "build", name);
    path rust_f     = f(path, "%o/Cargo.toml", project_f);
    path meson_f    = f(path, "%o/meson.build", project_f);
    path cmake_f    = f(path, "%o/CMakeLists.txt", project_f);
    path silver_f   = f(path, "%o/%o/%o.ag", project_f, name, name);
    path gn_f       = f(path, "%o/BUILD.gn", project_f);
    bool is_rust    = file_exists("%o", rust_f);
    bool is_meson   = file_exists("%o", meson_f);
    bool is_cmake   = file_exists("%o", cmake_f);
    bool is_gn      = file_exists("%o", gn_f);
    bool is_silver  = file_exists("%o", silver_f);
    path token      = f(path, "%o/silver-token", build_f);

    if (file_exists("%o", token)) {
        string s = (string)load(token, typeid(string), null);
        if (s && eq(s, config->chars))
            return; // we may want to return cached / built / error, etc
    }

    // the only reliable way of rebuilding on reconfig is to have a new build-folder
    remove_dir(build_f);
    make_dir(build_f);

    // this is the only place we 'cd' anywhere, where there are serial shell commands
    // however we go right back to where we were after
    if (prebuild && len(prebuild)) {
        path cw = path_cwd();
        cd(project_f);
        each(prebuild, string, cmd) {
            string icmd = interpolate(cmd, (Au)a);
            command_exec((command)icmd);
        }
        cd(cw);
    }

    if (is_cmake) { // build for cmake
        cstr build = debug ? "Debug" : "Release";
        string opt = a->isysroot ? f(string, "-DCMAKE_OSX_SYSROOT=%o", a->isysroot) : string("");

        vexec("configure",
              "%o cmake -B %o -S %o %o -DCMAKE_INSTALL_PREFIX=%o -DCMAKE_BUILD_TYPE=%s %o",
              env, build_f, project_f, opt, install, build, config);

        vexec("build", "%o cmake --build %o -j16", env, build_f);
        vexec("install", "%o cmake --install %o", env, build_f);
    } else if (is_meson) { // build for meson
        cstr build = debug ? "debug" : "release";

        vexec("setup",
              "%o meson setup %o --prefix=%o --buildtype=%s %o",
              env, build_f, install, build, config);

        vexec("compile", "%o meson compile -C %o", env, build_f);
        vexec("install", "%o meson install -C %o", env, build_f);
    } else if (is_gn) {
        cstr is_debug = debug ? "true" : "false";
        vexec("gen", "gn gen %o --args='is_debug=%s is_official_build=true %o'", build_f, is_debug, config);
        vexec("ninja", "ninja -C %o -j8", build_f);
    } else if (is_rust) { // todo: copy bin/lib after
        vexec("rust", "cargo build --%s --manifest-path %o/Cargo.toml --target-dir %o",
              debug ? "debug" : "release", project_f, build_f);
    } else if (is_silver) { // build for Au-type projects
        silver sf = silver(module, silver_f, breakpoint, a->breakpoint, debug, a->debug,
            is_external, a->is_external ? a->is_external : a);
        validate(sf, "silver module compilation failed: %o", silver_f);
        drop(sf);
    } else {
        /// build for automake
        if (file_exists("%o/autogen.sh", project_f) ||
            file_exists("%o/configure.ac", project_f) ||
            file_exists("%o/configure", project_f) ||
            file_exists("%o/config", project_f)) {

            // fix common race condition with autotools
            if (!file_exists("%o/ltmain.sh", project_f))
                verify(exec("libtoolize --install --copy --force") == 0, "libtoolize");

            // common preference on these repos
            if (file_exists("%o/autogen.sh", project_f))
                verify(exec("(cd %o && bash autogen.sh)", project_f) == 0, "autogen");

            // generate configuration scripts if available
            else if (!file_exists("%o/configure", project_f) && file_exists("%o/configure.ac", project_f)) {
                verify(exec("autoupdate --verbose --force --output=%o/configure.ac %o/configure.ac",
                            project_f, project_f) == 0,
                       "autoupdate");
                verify(exec("autoreconf -i %o",
                            project_f) == 0,
                       "autoreconf");
            }

            // prefer pre/generated script configure, fallback to config
            path configure = file_exists("%o/configure", project_f) ? f(path, "./configure") : f(path, "./config");

            if (file_exists("%o/%o", project_f, configure)) {
                verify(exec("%o (cd %o && %o%s --prefix=%o %o)",
                            env,
                            project_f,
                            configure,
                            debug ? " --enable-debug" : "",
                            install,
                            config) == 0,
                       "config script %o", configure);
            }
        }

        path Makefile = f(path, "%o/Makefile", project_f);
        if (file_exists("%o", Makefile))
            verify(exec("%o (cd %o && make PREFIX=%o -f %o install)", env, project_f, install, Makefile) == 0, "make");
    }

    if (postbuild && len(postbuild)) {
        path cw = path_cwd();
        cd(build_f);
        each(postbuild, string, cmd) {
            string icmd = interpolate(cmd, (Au)a);
            command_exec((command)icmd);
        }
        cd(cw);
    }

    save(token, (Au)config, null);
}

// build with optional bc path; if no bc path we use the project file system
none silver_build(silver a) {
    path ll = null, bc = null;
    emit(a, (ARef)&ll, (ARef)&bc);
    verify(bc != null, "compilation failed");

    //bool   is_debug   = a->debug;
    int    error_code = 0;
    path   install    = a->install;
    //string name       = stem(bc);
    path   cwd        = path_cwd();
    string libs       = string("");
    array  lib_paths  = array();

    path product    = f(path, "%o/%s%o%s%o%s",
        a->build_dir, a->is_library ? lib_pre : "", a->name,
        a->defs_hash ? "-" : "",
        a->defs_hash,
        a->is_library ? lib_ext : "");
    
    if (a->product)        drop(a->product);

    a->product        = hold(product);

    verify(exec("%o/bin/llc -filetype=obj %o/%o.ll -o %o/%o.o -relocation-model=pic",
                install, a->build_dir, a->name, a->build_dir, a->name) == 0,
           ".ll -> .o compilation failed");

#ifndef NDEBUG
    string cflags = string("");
    //cflags = string("-fsanitize=address"); // import keyword should publish to these
#else
    cflags = string("");
#endif

    if (len(a->implements))
        write_header(a);

    // now we can unload the imported externs

    // create libs, and describe in reverse order from import
    pairs(a->libs, i) {
        string name = (string)i->key;
        push(lib_paths, (Au)name);
    }

    array rlibs = reverse(lib_paths);
    each(rlibs, string, lib_name) {
        if (len(libs))
            append(libs, " ");
        if (file_exists("%o", lib_name))
            concat(libs, lib_name);
        else
            concat(libs, f(string, "-l%o", lib_name));
    }

    // compile implementation in c/cc, and select for linking
    string objs = string();

    each(a->implements, path, i) {
        string i_name   = f(string, "%o.o", filename(i));
        string ext      = ext(i);
        cstr   compiler = eq(ext, ".cc") ? "clang++" : "clang";
        
        // compile .c/.cc to .o
        verify(exec("%o/bin/%s -c %o -o %o -I%o/include -I%o/include/Au",
            install, compiler, i, i_name, install, install) == 0,
            "failed to compile %o", i);
        
        // accumulate object files for linking
        if (len(objs)) append(objs, " ");
        concat(objs, i_name);
    }

    // link - include the implementation objects
    string isysroot = a->isysroot ? f(string, "-isysroot %o ", a->isysroot) : string("");
    verify(exec("%o/bin/clang %s %o %o/%o.o %o -o %o -L%o -L%o/lib %o %o",
        install, a->is_library ? shared : "", isysroot, a->build_dir, a->name, objs,
        a->product,
        a->build_dir,
        install, libs, cflags) == 0,
        "link failed");
    
    if (file_exists("%o", a->product_link))
        unlink(a->product_link->chars);
    
    verify(create_symlink(a->product, a->product_link),
        "could not create product symlink from %o", a->product);


    // write out each ark we find
    fdata ar = fdata(write, true, src, a->artifacts_path);
    each(a->artifacts, path, ark)
        file_write(ar, (Au)string(ark->chars));

    file_close(ar);
}

bool silver_next_is_neighbor(silver a) {
    token b = element(a, -1);
    token c = element(a, 0);
    return b->column + b->count == c->column;
}

string expect_alpha(silver a) {
    token t = next(a);
    verify(t && isalpha(*t->chars), "expected alpha identifier");
    return string(t->chars);
}

// when we load silver files, we should look for and bind corresponding .c files that have implementation
// this is useful for implementing in C or other languages
path module_exists(silver a, array idents, bool binary_finary, bool* is_bin) {
    verify(len(idents), "invalid module 'path");

    path to_path = cast(path, join(idents, "/"));
    path sf = absolute(f(path, "%o/../%o/%o.ag", a->module, stem(to_path), stem(to_path)));
    if (file_exists("%o", sf)) {
        *is_bin = false;
        return sf;
    }

    if (binary_finary && len(idents) == 1) {
        path sf = f(path, "%o/lib/lib%o.so", a->install, idents->origin[0]);
        //path sf2 = f(path, "%o/%o.ag", a->project_path, to_path);
        if (file_exists("%o", sf)) {
            *is_bin = true;
            return sf;
        }
    }

    *is_bin = false;
    return null;
}

enode silver_parse_ternary(silver a, enode expr, etype mdl_expect) {
    if (!read_if(a, "?")) {
        if (!read_if(a, "??"))
            return expr;
        enode expr_true = parse_expression(a, mdl_expect);
        return e_ternary(a, expr, expr_true, null);
    }
    enode expr_true = parse_expression(a, mdl_expect);
    verify(read_if(a, ":"), "expected : after expression");
    enode expr_false = parse_expression(a, mdl_expect);
    return e_ternary(a, expr, expr_true, expr_false);
}

// these are for public, intern, etc; Au-Type enums, not someting the user defines in silver context
i32 read_enum(silver a, i32 def, Au_t etype) {
    for (int m = 1; m < etype->members.count; m++) {
        Au_t enum_v = (Au_t)etype->members.origin[m];
        if (read_if(a, enum_v->ident))
            return *(i32 *)enum_v->value; // should support typed enums; the ptr is a mere Au-object
    }
    return def;
}

static bool peek_fields(silver a);

static bool class_inherits(etype cl, etype of_cl);

enode parse_object(silver a, etype mdl_schema, bool in_expr);

etype evar_type(evar a);

int user_arg_count(efunc f) {
    bool is_lambda_call = inherits(f->au, typeid(lambda));

    if (is_lambda_call) {
        verify(len(f->meta) > 0, "expected return-type for lambda instance");
        return len(f->meta) - 1;
    }

    if (f->au->member_type == AU_MEMBER_FUNC) {
        if (f->au->is_imethod) return f->au->args.count - 1;
        return f->au->args.count;
    }
    if (f->au->member_type == AU_MEMBER_CAST) {
        return 0;
    }
    if (f->au->member_type == AU_MEMBER_OPERATOR) {
        return f->au->args.count - 1;
    }
    if (f->au->member_type == AU_MEMBER_INDEX) {
        return 1;
    }
    return 0;
}

static enode parse_lambda_call(silver a, efunc mem) {
    // mem is the lambda instance (evar with lambda type)
    etype lambda_type = evar_type((evar)mem);
    array  meta       = mem->meta;
    
    verify(meta && len(meta) >= 1, "lambda requires meta with return type");
    
    // meta[0] is return type, meta[1..n] are arg types
    etype rtype = (etype)meta->origin[0];
    int n_args = len(meta) - 1;
    
    // Parse the bracket if needed
    bool br = false;
    validate(n_args == 0 || (br = read_if(a, "[") != null) || a->expr_level == 0,
        "expected bracket for lambda call");
    
    a->expr_level++;
    
    // Build array of arg values: user args + context at end
    array call_values = array(alloc, 32);
    
    // Parse each user arg
    for (int i = 0; i < n_args; i++) {
        etype arg_type = (etype)meta->origin[i + 1];  // skip return type
        enode arg_expr = parse_expression(a, arg_type);
        verify(arg_expr, "invalid lambda argument");
        push(call_values, (Au)arg_expr);
        
        if (i < n_args - 1)
            read_if(a, ",");  // optional comma
    }
    
    if (br)
        validate(read_if(a, "]"), "expected ] after lambda args");
    
    a->expr_level--;

    return lambda_fcall(a, mem, call_values);
}

enode efunc_fptr(efunc f);

static enode parse_func_call(silver a, efunc f) { sequencer
    push_current(a);
    validate(is_func((Au)f) || is_func_ptr((Au)f), "expected function got %o", f);

    bool read_br = false;
    bool cmode = false;
    if (is_func_ptr(f))
        read_br = read_if(a, "[") != null;
    else if (is_cmode(a)) {
        cmode = true;
        read_br = read_if(a, "(") != null;
        if (!read_br) {
            pop_tokens(a, true);
            return efunc_fptr(f);
        }
    }
    else validate(a->expr_level == 0 || (user_arg_count(f) == 0 || (read_br = read_if(a, "[") != null)),
            "expected call-bracket [ at expression depth past statement level");
    
    a->expr_level++;
    Au_t    fmdl   = au_arg_type((Au)f);
    efunc   fn     = (efunc)(is_func_ptr(f) ? u(enode, f->au) : u(enode, fmdl));
    array   m      = (array)&fmdl->args;
    int     ln     = m->count, i = 0;
    array   values = array(alloc, 32, assorted, true);
    enode   target = null;
    i32     offset = 0;

    if (is_func((Au)f) && f->target) {
        push(values, (Au)f->target);
        offset = 1;
    }

    while (i + offset < ln || fn->au->is_vargs) {
        Au_t   src  = (Au_t)au_arg_type(array_get(m, i + offset));
        etype  typ  = canonical(u(etype, src));
        token t2 = element(a, 0);
        enode  expr = parse_expression(a, typ); // self contained for '{interp}' to cstr!
        verify(expr, "invalid expression");
        push(values, (Au)expr);
        i++;

        if (read_if(a, ","))
            continue;

        verify(len(values) >= ln, "expected %i args for function %o (%i)", ln, f, seq);
        break;
    }

    a->expr_level--;
    validate(!read_br || read_if(a, cmode ? ")" : "]"), "expected ] after call");
    pop_tokens(a, true);
    return e_fn_call(a, fn, values);
}

static enode typed_expr(silver a, enode f, array expr) {
    push_tokens(a, expr ? (tokens)expr : a->tokens, expr ? 0 : a->cursor);
    
    // function calls
    efunc f_decl = u(efunc, f->au);
    if (f_decl) {
        array   m      = (array)&f_decl->au->args;
        int     ln     = m->count, i = 0;
        array   values = array(alloc, 32, assorted, true);
        enode   target = null;
        i32     offset = 0;

        if (f->target) {
            verify(f->target, "expected target for method call");
            push(values, (Au)f->target);
            offset = 1;
            verify(f_decl->target, "no target specified on target %o", f_decl);
        }

        while (i + offset < ln || f_decl->au->is_vargs) {
            Au_t   arg  = (Au_t)array_get(m, i + offset);
            etype  typ  = canonical(u(etype, arg));
            enode  expr = parse_expression(a, typ); // self contained for '{interp}' to cstr!
            verify(expr, "invalid expression");
            push(values, (Au)expr);
            
            if (read_if(a, ","))
                continue;
            
            verify(len(values) >= ln, "expected %i args for function %o", ln, f);
            break;
        }

        pop_tokens(a, expr ? false : true);
        return e_fn_call(a, f_decl, values);
    }
    
    // this is only suitable if reading a literal constitutes the token stack
    // for example:  i32 100
    Au  n = read_literal(a, null);
    if (n && a->cursor == len(a->tokens)) {
        pop_tokens(a, expr ? false : true);
        return e_operand(a, n, (etype)f);
    } else if (n) {
        // reset if we read something
        pop_tokens(a, expr ? false : true);
        push_tokens(a, expr ? (tokens)expr : a->tokens, expr ? 0 : a->cursor);
    }
    bool    has_content = !!expr && len(expr); //read_if(mod, "[") && !read(mod, "]");
    enode   r           = null;
    bool    conv        = false;

    a->expr_level++;
    if (!has_content) {
        r = e_create(a, (etype)f, null); // default
        conv = false;
    } else if (class_inherits((etype)f, etypeid(array))) {
        array nodes         = array(64);
        etype element_type  = u(etype, f->au->src);
        shape sh            = f->au->shape;
        validate(sh, "expected shape on array");
        int   shape_len     = shape_total(sh);
        int   top_stride    = sh->count ? sh->data[sh->count - 1] : 0;
        validate((!top_stride && shape_len == 0) || (top_stride && shape_len),
            "unknown stride information");  
        int   num_index     = 0; /// shape_len is 0 on [ int 2x2 : 1 0, 2 2 ]

        while (peek(a)) {
            token n = peek(a);
            enode e = parse_expression(a, element_type);
            e = e_create(a, element_type, (Au)e);
            push(nodes, (Au)e);
            num_index++;
            if (top_stride && (num_index % top_stride == 0)) {
                validate(read_if(a, ",") || !peek(a),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    top_stride);
            }
        }
        r = e_create(a, (etype)f, (Au)nodes);
    } else if (peek_fields(a) || class_inherits((etype)f, etypeid(map))) {
        conv = false; // parse map will attempt to go direct
        r    = (enode)parse_object(a, (etype)evar_type((evar)f), true);
    } else {
        /// this is a conversion operation
        r = (enode)parse_expression(a, (etype)f);
        conv = canonical(r) != canonical(f);
        //validate(read_if(a, "]"), "expected ] after f-expr %o", src->name);
    }
    a->expr_level--;
    if (conv)
        r = e_create(a, (etype)f, (Au)r);
    //if (expr && a->cursor != len(a->tokens) - 1) {
    //    validate(false, "unexpected %o after expression", peek(a));
    //}
    pop_tokens(a, expr ? false : true);
    return r;
}

// still have not decided if we want to allow instance override in construct; its certainly a viable caching mechanism
// its certainly a way to control for duplicates, etc
silver silver_with_path(silver a, path module_path) {
    string e = ext(module_path);
    a->module = eq(e, "ag") ? parent_dir(module_path) : module_path;
    return a;
}

token read_compacted(silver a) {
    token  f = next(a);
    if (!f) return null;
    string r = string(f->chars);
    int len = r->count;
    int start_col = f->column;

    for (;;) {
        token n = peek(a);
        if (!n || n->column != (start_col + len) || n->line != f->line)
            break;
        concat(r, (string)n);
        consume(a);
        f = n;
        len += n->count;
    }
    return token(chars, r->chars, source, f->source, line, f->line, column, start_col + len);
}

Au parse_field(silver a, etype key_type) {
    Au k = null;
    if (read_if(a, "{")) {
        // todo: this must be const-controlled for import configuration
        // also, that config must effectively hash-id the builds (4 or 6 base-16 is fine)
        k = (Au)parse_expression(a, key_type);
        validate(read_if(a, "}"), "expected }");
    } else {
        string name = (string)read_alpha(a);
        validate(name, "expected member identifier");
        k = (Au)const_string(chars, name->chars);
    }
    return k;
}

enode parse_export(silver a) {
    sequencer;
    validate(read_if(a, "export"), "expected export keyword");
    token version = read_compacted(a);
    verify(a->project_path, "expected silver invocation into main project module");
    return e_noop(a, null);
}

enode parse_import(silver a) {
    sequencer;

    validate(next_is(a, "import"), "expected import keyword");
    consume(a);

    int     from         = a->cursor;
    codegen cg           = null;
    string  namespace    = null;
    path    lib_path     = null;
    path    module_source = null;
    bool    is_binary    = false;
    token   t            = peek(a);
    Au_t    is_codegen   = null;
    token   commit       = null;
    string  uri          = null;
    string  module_lib   = null;
    array   mpath        = null;
    string  single       = null;
    Au_t    mod          = null;
    string  aa           = null;
    string  bb           = read_if(a, ":") ? expect_alpha(a) : null;
    string  cc           = bb && read_if(a, ":") ? expect_alpha(a) : null;
    string  service      = null;
    string  user         = null;
    string  project      = null;

    if (t && isalpha(t->chars[0])) {
        bool   cont     = false;
        service  = a->git_service;
        user     = a->git_owner;
        project  = null;
        aa       = expect_alpha(a); // value of t
        bb       = read_if(a, ":") ? expect_alpha(a) : null;
        cc       = bb && read_if(a, ":") ? expect_alpha(a) : null;

        mod = find_module((cstr)aa->chars);
        Au_t f = mod ? f : find_type((cstr)aa->chars, null);

        if (mod) {
            if (!mpath) mpath = array(alloc, 32);
            push(mpath, (Au)string(mod->ident));
        }
        else if (f && inherits(f, typeid(codegen)))
            is_codegen = f;
        else if (next_is(a, ".")) {
            while (read_if(a, ".")) {
                if (!mpath) {
                    mpath = array(alloc, 32);
                    push(mpath, (Au)(cc ? cc : bb ? bb
                                      : aa   ? aa
                                             : (string)null));
                }
                string ident = read_alpha(a);
                push(mpath, (Au)ident);
            }
        } else {
            mpath = array(alloc, 32);
            string f = cc ? cc : bb ? bb
                             : aa   ? aa
                                    : (string)null;
            if (index_of(f, ".") >= 0) {
                array sp = split(f, ".");
                array sh = shift(sp);
                push(mpath, (Au)sh);
            } else if (f) {
                push(mpath, (Au)f);
                single = f;
            }
        }

        // read commit if given
        if (read_if(a, "/"))
            commit = read_compacted(a);
    }

    array includes = array(32);

    // determine includes, uri, and config
    // includes for this import
    if (!is_codegen && read_if(a, "<")) {
        for (;;) {
            string f = read_alpha_any(a);
            validate(f, "expected include");

            // we may read: something/is-a.cool\file.hh.h
            while (next_is_neighbor(a) && (!next_is(a, ",") && !next_is(a, ">")))
                concat(f, (string)next(a));

            push(includes, (Au)f);

            if (!read_if(a, ",")) {
                token n = read_if(a, ">");
                validate(n, "expected '>' after include, list, of, headers");
                break;
            }
        }
    }

    array b = read_body(a);
    map defs = len(b) ? map() : null;
    bool is_fields = false;
    if (defs) {
        push_tokens(a, (tokens)b, 0);
        if (peek_fields(a)) {
            is_fields = true;
            Au k = parse_field(a, null);
            verify(read_if(a, ":"), "expected : after field");
            bool is_const = false;
            array tokens = read_expression(a, null, &is_const);
            token token0 = (token)get(tokens, 0);
            validate(tokens, "expected expression");
            set(defs, (Au)k, (Au)tokens);
        }
        pop_tokens(a, false);
    }
    array all_config = is_fields ? array() : compact_tokens(b);
    map props = map();

    // this invokes import by git; a local repo may be possible but not very usable
    // arguments / config not stored / used after this
    // run with these, or compile with these?
    // its obviously a major nightmare to control namespace for configuration of module
    // it would need to follow the same cache rules below
    // loading at runtime does seem a better idea
    if (next_is(a, "[")) {
        verify(!mod, "run-time module imported -- configuration cannot be applied");
        array b = read_body(a);
        int index = 0;
        while (index < len(b)) {
            verify(index - len(b) >= 3, "expected prop: value for codegen object");
            token prop_name  = (token)b->origin[index++];
            token col        = (token)b->origin[index++];
            token prop_value = (token)b->origin[index++];

            verify(eq(col, ":"), "expected prop: value for codegen object");
            set(props, (Au)string(prop_name->chars), (Au)string(prop_value->chars));
        }
    }
    
    string external_name = null;
    path   external_product = null;

    if (read_if(a, "from")) {
        uri = hold(read_alpha(a)); // todo: compact neighboring tokens with https:// and git://
        validate(uri, "expected uri");
    }

    if (!is_codegen && aa && !bb && !commit) {
        path m = module_exists(a, mpath, true, &is_binary); // useful to resolve in either case

        if (!is_binary && m) {
            module_source = hold(m);
        } else if (is_binary && m) {
            lib_path = hold(m);
        }
        
        // if the module is built into our run-time already, we support this
        if (mod) {
            set(a->libs, string(mod->ident), (Au)_bool(true));

        } else if (!module_source && !lib_path)
            fault("could not find module %o", mpath);
        
    } else if (aa && !bb) {
        project     = aa;
    } else {
        user        = aa;
        project     = bb;
    }

    if (project && !lib_path && !module_source) {
        string path_str = string();
        if (len(mpath)) {
            string str_mpath = join(mpath, "/") ? cc : string("");
            path_str = len(str_mpath) ? f(string, "blob/%o/%o", commit, str_mpath) : string("");
        }
        uri = f(string, "https://%o/%o/%o%s%o", service, user, project,
                cast(bool, path_str) ? "/" : "", path_str);
    }

    if (uri) {
        checkout(a, path(uri->chars), (string)commit,
                 import_build_commands(all_config, ">"),
                 import_build_commands(all_config, ">>"),
                 import_config(all_config),
                 import_env(all_config));
        each(all_config, string, t)
            if (starts_with(t, "-l"))
                set(a->libs, (Au)mid(t, 2, len(t) - 2), (Au)_bool(true));
    } else if (module_source) {
        path module = parent_dir(module_source);
        verify(compare(stem(module_source), stem(module)) == 0, "silver expects identical module stem");

        // we should turn on object tracking here, as to trace which objects are still in memory after we drop
        // the simple ternary statement allows us to give an og silver
        // og silver is keeper of artifacts
        silver og = a->is_external ? a->is_external : a;
        silver external = silver(module, module, breakpoint, a->breakpoint,
            is_external, og, debug, a->debug, defs, defs);
        
        // these should be the only two objects remaining.
        external_name    = hold(external->name); 
        external_product = hold(external->product);

        if (external_product) {
            if (index_of(og->artifacts, (Au)external_product) < 0) {
                push(og->artifacts, (Au)external_product);
                printf("adding product artifact\n");
            }
        }

        if (external->module_file) {
            if (index_of(og->artifacts, (Au)external->module_file) < 0) {
                push(og->artifacts, (Au)external->module_file); // throw it all in the [artifacts] gumbo
                printf("adding module_file artifact\n");
            }
        }
        validate (!external->error, "error importing silver module %o", external);

        drop(external);
        drop(external); // this is to compensate for the initial hold in silver_init [ quirk for build in init ]

        set(a->libs, (Au)string(external_product->chars), (Au)_bool(true));

    }
    else if (is_codegen) {
        cg = (codegen)construct_with(is_codegen, (Au)props, null);
    }

    if (next_is(a, "as")) {
        consume(a);
        namespace = hold(read_alpha(a));
        validate(namespace, "expected alpha-numeric %s",
                 is_codegen ? "alias" : "namespace");
    } else if (is_codegen) {
        namespace = hold(string(is_codegen->ident));
    }

    // hash. for cache.  keep cache warm
    int to = a->cursor;
    array tokens = array(alloc, to - from + 1);
    for (int i = from; i < to; i++) {
        token t = (token)a->tokens->origin[i];
        push(tokens, (Au)t);
    }

    import mdl = import(
        mod, (aether)a,
        codegen, cg,
        external_name, external_name,
        external_product, external_product,
        tokens, tokens);

    push(a->imports, (Au)mdl);
    a->current_import = (etype)mdl;

    mdl->au->alt = namespace ? cstr_copy(namespace->chars) : null;
    
    if (len(includes)) {
        push_scope(a, (Au)mdl->au);
        mdl->include_paths = array();

        // include each, collecting the clang instance for which we will invoke macros through
        each(includes, string, inc) {
            path i = include(a, (Au)inc, namespace);
            push(mdl->include_paths, (Au)i);
        }
    }

    Au_t top22 = top_scope(a);

    // loads the actual library here -- DO NOT integrate external->au module; we load it direct with our own
    // and let the runtime register itself
    // this is so we may be Au-centric, and language agnostic
    if (!is_codegen && (mdl->external_name || mod || lib_path)) {
        import_Au(a,
            mdl->external_name,
            mdl->external_product ? (Au)mdl->external_product : mod ? (Au)mod : (Au)lib_path);
    }

    Au_t top2244 = top_scope(a);
    
    mdl->au->is_closed = true;
    mdl->lib_path = hold(lib_path);
    mdl->module_source = hold(module_source);
    a->current_import = null;

    if (is_codegen) {
        string name = namespace ? (string)namespace : string(is_codegen->ident);
        set(a->codegens, (Au)name, (Au)mdl->codegen);
    }

    Au_t top2 = top_scope(a);

    return (enode)mdl;
}

// works for class inits, and module inits
void silver_build_user_initializer(silver a, enode t) {
    if (t && t->au && t->au->member_type == AU_MEMBER_VAR && t->initializer) {
        // only override for module members, not class members
        bool   is_module_mem = t->au->context == a->au;
        Au     expr          = (Au)t->initializer;
        if (!instanceof(t->initializer, enode)) {
            array post_const = parse_const(a, (array)t->initializer);
            int level = a->expr_level;
            a->expr_level++;
            push_tokens(a, (tokens)post_const, 0);
            // we store the meta field on the var entry, not the var's src type
            etype meta_arg0 = (etype)array_get(t->meta, 0);
            shape meta_arg1 = (shape)array_get(t->meta, 1);
            etype recombine = etype(mod, (aether)a, meta, t->meta, au, t->au->src);
            expr = (Au)parse_expression(a, (etype)recombine); // we have tokens for the name pushed to the stack
            pop_tokens(a, false);
            a->expr_level = level;
        }
        enode ctx = context_func(a);
        enode L;
        
        if (!is_module_mem && ctx) {
            evar ar = (evar)u(evar, (Au_t)ctx->au->args.origin[0]);
            Au_t au_type = isa(ar);
            L = access((enode)ar, string(t->au->ident));
        } else
            L = (enode)t;
        
        e_assign(a, L, (Au)expr, OPType__assign);
    }
}

i64 path_wait_for_change(path, i64, i64);

static string uccase(string s) {
    string u = ucase(s);
    do {
        int i = index_of(u, "-");
        if (i == -1)
            break;
        ((cstr)u->chars)[i] = '_';
    } while (1);
    return u;
}

static string cname(string s) {
    string u = string(chars, s->chars);
    do {
        int i = index_of(u, "-");
        if (i == -1)
            break;
        ((cstr)u->chars)[i] = '_';
    } while (1);
    return u;
}

static string method_def(enode emem) {
    string name = cname(string(emem->au->ident));
    return f(string,
             "#ifndef %o\n\t#define %o(I,...) ({{ __typeof__(I) _i_ = I; ftableI(_i_)->ft.%o(_i_, ## __VA_ARGS__); }})\n#endif\n",
             name, name, name);
}

static string type_name(Au a) {
    Au_t au = au_arg(a);
    if (au && au->member_type == AU_MEMBER_VAR) {
        au = au->src;
    }
    return au ? string(au->alt ? au->alt : au->ident) : null;
}

void silver_write_header(silver a) {
    string m           = string(a->au->ident);
    path   inc_path    = f(path, "%o/include",    a->install);
    path   module_dir  = f(path, "%o/%o",         inc_path, m);
    path   module_path = f(path, "%o/%o/%o",      inc_path, m, m);
    path   import_path = f(path, "%o/%o/import",  inc_path, m);
    path   init_path   = f(path, "%o/%o/init",    inc_path, m);
    path   intern_path = f(path, "%o/%o/intern",  inc_path, m);
    path   public_path = f(path, "%o/%o/public",  inc_path, m);
    path   method_path = f(path, "%o/%o/methods", inc_path, m); // lets store in the install path
    string NAME        = uccase(m);

    verify(make_dir(module_dir), "could not make dir %o", module_dir);

    // we still need to parse aliases where we subclass
    // LA

    FILE *import_f = fopen(cstring(import_path), "wb");
    FILE *module_f = fopen(cstring(module_path), "wb");
    FILE *intern_f = fopen(cstring(intern_path), "wb");
    FILE *  init_f = fopen(cstring(init_path),   "wb");
    FILE *public_f = fopen(cstring(public_path), "wb");
    FILE *method_f = fopen(cstring(method_path), "wb");

    #undef  line
    #undef  write
    #define write(f,s,...) fputs(fmt(s     __VA_OPT__(,) __VA_ARGS__)->chars, f)
    #define line(f,s,...)  fputs(fmt(s"\n" __VA_OPT__(,) __VA_ARGS__)->chars, f)
    

    // write intern header
    line(intern_f, "#ifndef _%o_INTERN_", NAME);
    line(intern_f, "#define _%o_INTERN_", NAME);
    members(a->au, m) {
        if (is_class(m)) {
            string n = cname(string(m->ident));
            line(intern_f,
                "#undef %o_intern", n);
            line(intern_f,
                "#define %o_intern(A,B,...) A##_schema(A,B, __VA_ARGS__)", n);
        }
    }
    line(intern_f, "#include <%o/%o>", m, m);
    line(intern_f, "#endif");

    // write public header
    line(public_f, "#ifndef _%o_PUBLIC_", NAME);
    line(public_f, "#define _%o_PUBLIC_", NAME);
    members(a->au, m) {
        etype mdl = u(etype, m);
        if (is_class(m)) {
            string n = type_name((Au)m);
            line(public_f, "#ifndef %o_intern", n);
            line(public_f, "#define %o_intern(A,B,...) A##_schema(A,B##_EXTERN, __VA_ARGS__)", n);
            line(public_f, "#endif");
        }
    }
    line(public_f, "#endif");

    // write init header
    line(init_f, "#ifndef _%o_INTERN_", NAME);
    line(init_f, "#define _%o_INTERN_", NAME);
    members(a->au, m) {
        if (is_class(m)) {
            string n = cname(string(m->ident));
            line(init_f,
                "#undef %o_intern", n);
            line(init_f,
                "#define %o_intern(A,B,...) A##_schema(A,B, __VA_ARGS__)", n);
        }
    }
    line(init_f, "#include <%o/%o>", m, m);
    line(init_f, "#endif");


    // write module-name header
    line(module_f, "#ifndef _%o_",     NAME);
    line(module_f, "#define _%o_\n",   NAME);

    // forward declare all classes
    members(a->au, m) {
        if (is_class(m))
            line(module_f, "forward(%o)", cname(type_name((Au)m)));
    }

    // write class schemas
    members(a->au, m) {
        if (is_class(m)) {
            string n   = cname(type_name((Au)m));
            array  acl = etype_class_list(u(etype, m));

            write(module_f, "#define %o_schema(A,B,...)", n);
            members(m, mi) {
                line(module_f, "\\", n);
                string mn = cname(string(mi->ident));
                string access_type = estring(typeid(interface), mi->access_type ? mi->access_type : interface_public);

                if (is_func((Au)mi)) {
                    enode f = u(enode, mi);
                    string args = string();
                    string i = f->target ? string("i") : string("s");
                    bool first = true;
                    arg_types(mi, arg) {
                        if (eq(i, "i") && first) {
                            first = false;
                            continue;
                        }
                        first = false;
                        enode aa = u(enode, arg);
                        if (len(args))
                            append(args, ",");
                        concat(args, cname(cast(string, aa)));
                    }
                    
                    string rtype = cname(cast(string, u(etype, f->au->rtype)));
                    bool show_comma = f->target && mi->args.count > 1 ||
                                     !f->target && mi->args.count > 0;
                    if (mi->is_override)
                        write(module_f, "M(A,B, %o,override,method,%o)", i, mn);
                    else
                        write(module_f, "M(A,B, %o,method,%o,%o,%o%s%o)",
                            i, access_type, rtype, mn, show_comma ? "," : "", args);
                } else {
                    string i        = !mi->is_static ? string("i") : string("s");
                    string meta     = string();
                    arg_list(mi, m) {
                        string n = type_name((Au)m);
                        if (meta->count)
                            append(meta, ", ");
                        concat(meta, n);
                    }
                    bool   has_meta = mi->meta.count > 0;
                    string prop_type   = cname(type_name((Au)mi));
                    write(module_f, "M(A,B, %o,prop,%o,%o,%o%s%o)",
                        i, access_type, prop_type, mn, has_meta ? "," : "", meta);
                }
            }
            line(module_f, "\n");


            int count = len(acl) - 1;
            string extra = string("");
            if (count > 1)
                extra = fmt("_%i", count);
            string classes = string();
            etype Au_cl = etypeid(Au);
            acl = reverse(acl);
            each(acl, etype, c) {
                if (c == Au_cl) break;
                string s = cast(string, c);
                if (classes->count)
                    append(classes, ",");
                concat(classes, s);
            }
            line(module_f, "declare_class%o(%o)\n", extra, classes);
        }
    }
    line(module_f, "#endif");

    // write methods
    line(method_f, "#ifndef _%o_METHODS_", NAME);
    line(method_f, "#define _%o_METHODS_", NAME);
    members(a->au, m) {
        if (is_class(m)) {
            members(m, mi) {
                efunc fn = u(efunc, mi);
                if   (fn)  line(method_f, "%o", method_def((enode)fn));
            }
        }
    }
    line(method_f, "#endif");
    fclose(method_f);


    // write import header
    line(import_f, "#ifndef _%o_IMPORT_",   NAME);
    line(import_f, "#define _%o_IMPORT_\n", NAME);

    each(a->imports, import, im) {
        each(im->include_paths, path, i)
            line(import_f, "#include <%o>", i);
    }

    line(import_f, "#include <Au/public>");
    each(a->imports, import, im) {
        if (im->external_name)
            line(import_f, "#include <%o/public>", im->external_name);
    }
    line(import_f, "#include <Au/Au>");
    each(a->imports, import, im) {
        if (im->external_name)
            line(import_f, "#include <%o/%o>", im->external_name, im->external_name);
    }
    line(import_f, "#include <%o/intern>",  a->name);
    line(import_f, "#include <%o/%o>",      a->name, a->name);
    line(import_f, "#include <%o/methods>", a->name);
    line(import_f, "#undef init");
    line(import_f, "#undef dealloc");
    line(import_f, "#include <Au/init>");
    each(a->imports, import, im) {
        if (im->external_name)
            line(import_f, "#include <%o/init>", im->external_name);
    }
    line(import_f, "#include <Au/methods>");
    each(a->imports, import, im) {
        if (im->external_name)
            line(import_f, "#include <%o/methods>", im->external_name);
    }
    line(import_f, "#include <%o/init>", a->name);
    line(import_f, "#endif");

    fclose(import_f);
    fclose(module_f);
    fclose(intern_f);
    fclose(public_f);
}

void print_token_array(silver a, array tokens) {
    string res = string();
    for (int i = 0, ln = len(tokens); i < ln; i++) {
        token t = (token)tokens->origin[i];
        append(res, t->chars);
        append(res, " ");
    }
    print("tokens = %o", res);
}

i32 read_enum(silver a, i32 def, Au_t etype);

static enode typed_expr(silver a, enode src, array expr);

none push_lambda_members(aether a, efunc f);

void build_fn(silver a, efunc f, callback preamble, callback postamble) {
    if (f->user_built)
        return;

    bool user_has_code = len(f->body) || f->cgen;

    f->user_built = true;

    // if there is no code, then this is an external c function; implement must do this
    implement(f);

    if (f->has_code) {
        if (f->target)
            push_scope(a, (Au)f->target);

        // reasonable convention for silver's debugging facility
        // if this is a standard for IDE, then we can rely on this to improve productivity
        Au_t au_calls = def(f->au, "sequence", AU_MEMBER_VAR, AU_TRAIT_STATIC);
        au_calls->src = etypeid(i64)->au;
        evar e_calls = evar(mod, (aether)a,
            au, au_calls);
        
        a->last_return = null;
        push_scope(a, (Au)f);

        if (is_lambda((Au)f))
            push_lambda_members((aether)a, f);

        // we need to initialize the schemas first, then we can actually perform user-based inits
        if (f->au->is_mod_init)
            build_module_initializer(a, (enode)f);

        // before the preamble we handle guard
        if (preamble)
            preamble((Au)f, null);

        if (f->remote_func) {
            // the user may implement their own init/dealloc inbetween pre-amble
            // we init our own too but its name is changed on init to facilitate
            array call_args = array(alloc, 32);
            if (f->target)
                push(call_args, (Au)f->target);
            arg_list(f->au, arg) {
                push(call_args, (Au)u(evar, arg));
            }
            e_fn_call(a, f->remote_func, call_args);
        } else if (f->cgen) {
            array gen = generate_fn(f->cgen, f, (array)f->body);
        } else {
            array source_tokens = parse_const(a, (array)f->body);
            push_tokens(a, (tokens)source_tokens, 0);
            parse_statements(a, true);
            pop_tokens(a, false);
        }

        if (postamble)
            postamble((Au)f, null);
        
        validate(f->au->has_return || (!f->au->rtype || is_void(u(etype, f->au->rtype))),
            "expected return statement in %o", f);
        
        if (is_lambda((Au)f))
            pop_scope(a);

        if (!f->au->has_return)
            e_fn_return(a, null);
        
        pop_scope(a);
        if (f->target)
            pop_scope(a);
    }
}

static void build_record(silver a, etype mrec) {
    etype rec = resolve(mrec);
    if (rec->user_built) return;
    rec->user_built = true;
    rec->parsing = true;
    verify(rec->au->is_class || rec->au->is_struct, "not a record");
    print_token_array(a, (array)rec->body);

    array body = rec->body ? (array)rec->body : array();
    push_tokens(a, (tokens)body, 0);
    push_scope(a, (Au)mrec);
    int index = 0;
    while (peek(a)) {
        enode n = parse_statement(a);
        if (instanceof(n, evar)) {
            evar mem = (evar)n;
            if (mem->au->member_type == AU_MEMBER_VAR) {
                verify(mem->au->index == 0, "unexpected member-index");
                mem->au->index = index++;
            }
        }
    }
    pop_tokens(a, false);   
    rec->parsing = false;
    
    etype_implement(mrec);

    // the functions we write after may access the type-id
    create_type_members(a, a->au);

    // if this is a class, we create one, then built init with a preamble that initializes our properties
    // this is called from Au_initialize
    if (rec->au->is_class) {
        // if no init, create one (attach preamble for our property inits)
        Au_t m_init = find_member(rec->au, "init", AU_MEMBER_FUNC, false);
        if (!m_init) {
            efunc f = function(a, (etype)rec, 
                string("init"), etypeid(none), a(rec), AU_MEMBER_FUNC,
                AU_TRAIT_IMETHOD | AU_TRAIT_OVERRIDE, 0);
            f->has_code = true;
            f->au->alt = cstr_copy(((string)f(string, "%o_init", mrec))->chars);
            etype_implement((etype)f);
            m_init = f->au;
        }
        build_fn(a, u(efunc, m_init), build_init_preamble, null); // we may need to

        // build remaining functions
        members(rec->au, m) {
            efunc n = u(efunc, m);
            if (n) build_fn(a, n, null, null);
        }
    }
    pop_scope(a);
}

// we want to save const for a version 1.00, not 0.88
array silver_parse_const(silver a, array tt) {
    array res = array(32);
    push_tokens(a, (tokens)tt, 0);
    a->clipping = true;
    while (peek(a)) {
        token t = next(a);
        if (eq(t, "const")) {
            validate(false, "implement const");
        } else {
            push(res, (Au)t);
        }
    }
    a->clipping = false;
    pop_tokens(a, false);
    return res;
}

enode parse_return(silver a) {
    etype rtype = return_type(a);
    bool  is_v  = is_void(rtype);
    enode ctx   = context_func(a);
    consume(a);
    a->expr_level++;
    enode expr  = is_v ? null : parse_expression(a, rtype);
    a->expr_level--;
    Au_log("return-type", "%o", is_v ? (Au)string("none") : (Au)expr);

    Au_t au_top = top_scope(a);
    //catcher cat = u(catcher, au_top);
    verify (!au_top->has_return, "return already built at statement level");

    bool is_sub = e_fn_return(a, (Au)expr);
    if (!is_sub) {
        au_top->has_return = true;
        if (ctx) ctx->au->has_return = true;
        a->last_return = e_noop(a, (etype)expr);
        return a->last_return;
    } else {
        return e_noop(a, (etype)expr);
    }
}

catcher context_catcher(silver);

enode parse_break(silver a) {
    consume(a);
    catcher cat = context_catcher(a);
    verify(cat, "expected cats");
    return e_break(a, cat);
}

// read-expression does not pass in 'expected' models, because 100% of the time we run conversion when they differ
// the idea is to know what model is returning from deeper calls
static array read_expression(silver a, etype *mdl_res, bool *is_const) {
    array exprs = array(32);
    int s = a->cursor;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    enode n = parse_expression(a, null);
    if (mdl_res)
        *mdl_res = (etype)n;
    a->no_build = false;
    int e = a->cursor;
    for (int i = s; i < e; i++) {
        push(exprs, (Au)a->tokens->origin[i]);
    }
    *is_const = a->is_const_op;
    return exprs;
}

static array read_enode_tokens(silver a) {
    array exprs = array(32);
    int s = a->cursor;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    enode n = read_enode(a, null, false);
    a->no_build = false;
    int e = a->cursor;
    for (int i = s; i < e; i++) {
        push(exprs, (Au)a->tokens->origin[i]);
    }
    return exprs;
}

static enode parse_func_call(silver, efunc);

// this will have to adapt to parsing into a map, or parsing into a real type
// for real types, we cannot use the string as its redundant and can be reduced by the user
//

bool is_map(etype);

etype prop_value_at(etype aa, i64 index) {
    aether a = aa->mod;
    i64 prop = 0;
    members(aa->au, m) {
        if (m->member_type == AU_MEMBER_VAR && m->is_iprop) {
            if (index == prop) {
                return u(etype, m->src);
            }
            prop++;
        }
    }
    return null;
}

// this must parse into map or array, then hand to e_create
// in the case where the entire value is given at once, we can back out and perform a direct e_create
// with that result.. we will do this on the first item.  we merely check for the ] afterwards.
// as such we might want to lazy load the imap or iarray (intermediates that we give to e_create)
enode parse_object(silver a, etype mdl, bool within_expr) {
    print_tokens(a, "parse-object");
    validate(within_expr || read_if(a, "["), "expected [");

    bool is_fields = peek_fields(a) || inherits(mdl->au, typeid(map));
    bool is_mdl_map = mdl->au == typeid(map);
    bool is_mdl_collective = inherits(mdl->au, typeid(collective));
    bool was_ptr = false;

    validate(!is_mdl_map || is_fields, "expected fields for map");

    if (is_ptr(mdl) && is_struct(mdl->au->src)) {
        was_ptr = true;
        mdl = resolve(mdl);
    }

    etype key = is_mdl_map ? (etype)array_get((array)mdl->meta, 0) : null;
    etype val = is_mdl_map ? (etype)array_get((array)mdl->meta, 1) : null;

    if (!key) key = etypeid(string);
    if (!val) val = etypeid(Au);

    // Lazy-initialized containers
    map   imap   = null;
    array iarray = null;
    int   iter   = 0;
    shape s      = is_mdl_collective ? instanceof(array_get(mdl->meta, 1), shape) : null;
    int shape_stride = (s && s->count > 1) ? s->data[s->count - 1] : 0;
    
    while (peek(a)) {
        if (!peek(a) || next_is(a, "]"))
            break;

        Au    k = null;
        token t = peek(a);
        bool  is_literal = instanceof(t->literal, string) != null;
        bool  is_enode_key = false;

        bool auto_bind = is_fields && read_if(a, ":");
        // -- KEY --
        if (is_fields && read_if(a, "{")) {
            k = (Au)parse_field(a, key);
            validate(read_if(a, "}"), "expected }");
            is_enode_key = true;
        } else if (!is_fields && is_mdl_collective) {
            // we are parsing individual scalar value f64 -> vec2f
            etype e = (etype)array_get(mdl->meta, 0);
            k = (Au)parse_expression(a, e);
        } else if (!is_fields && !is_mdl_map) {
            etype e = prop_value_at(mdl, iter);
            validate(e, "cannot find prop for %o at index %i", mdl, iter);
            k = (Au)parse_expression(a, e);
        } else if (!is_mdl_map) {
            //k = (Au)parse_field(a, key); 
            string name = (string)read_alpha(a);
            validate(name, "expected member identifier");
            k = (Au)const_string(chars, name->chars);
        } else {
            token t = peek(a);
            string name = (string)read_literal(a, typeid(string));
            validate(is_literal, "expected literal string");
            k = (Au)const_string(chars, name->chars);
        }

        // -- Handle literal short case --
        if (iter == 0 && next_is(a, "]")) {
            // single element, return e_create(k)
            return e_create(a, mdl, k);
        }

        // -- VALUE --
        Au v = null;
        if (is_fields) {
            validate(auto_bind || read_if(a, ":"), "expected : after key %o", t);
            if (auto_bind) prev(a);
            v = (Au)parse_expression(a, null);
        }

        // -- Lazy allocate --
        if (!imap && is_fields)
            imap   = map(assorted, true);
        else if (!iarray && !is_fields)
            iarray = array(alloc, 32, assorted, true);
        
        // -- Insert --
        if (is_fields) {
            validate(v, "expected value after key %o", k);
            validate(!get(imap, k), "duplicate key %o", k);
            set(imap, k, v); // k's are both strings and enode -- this is so we can eval into both map, struct and class props
        } else {
            push(iarray, k);
        }

        token comma = read_if(a, ",");
        if (shape_stride != 0) {
            verify(( comma && ((iter + 1) % shape_stride == 0)) ||
                   (!comma || ((iter + 1) % shape_stride != 0)), "check array commas compared to stride");
        }

        iter++;
    }

    // Now create from intermediate container
    if (imap) return e_create(a, mdl, (Au)imap);

    // validation check
    if (iarray && mdl->au == typeid(array)) {
        shape dims = instanceof(array_get(mdl->meta, 1), shape);
        int max_items = dims ? shape_total(dims) : -1;
        verify(max_items == -1 || len(iarray) <= max_items,
            "too many elements (total array size: %i, user provides %i)", max_items, len(iarray));
    }

    validate(within_expr || read_if(a, "]"), "expected ]");

    // a default is made if we give a []; if iarray is provided, e_create will iterate through members
    return e_create(a, mdl, (Au)iarray);
}


static bool class_inherits(etype cl, etype of_cl) {
    silver a = (silver)cl->mod;
    etype aa = canonical(of_cl);
    while (cl && cl != aa) {
        if (!cl->au->context || cl->au->context == cl->au)
            break;
        cl = u(etype, cl->au->context);
    }
    return cl && cl == aa;
}

static bool peek_fields(silver a) {
    token t0 = element(a, 0);
    token t1 = element(a, 1);
    if (t0 && eq(t0, ":")) return true; // auto-bind
    if (t0 && is_alpha((Au)t0) && t1 && eq(t1, ":"))
        return true;
    return false;
}

array read_arg(array tokens, int start, int *next_read);

none copy_lambda_info(enode mem, enode lambda_fn);

enode parse_create_lambda(silver a, enode mem) {
    validate(read_if(a, "["), "expected [ context ] after lambda");

    // mem is the lambda function definition
    enode   lambda_f = (enode)evar_type((evar)mem);
    array   ctx_mem  = (array)&lambda_f->au->members;  // context members after ::
    int     ctx_ln   = ctx_mem->count;
    array   ctx      = array(alloc, ctx_ln);
    
    Au_t lt = isa(lambda_f);

    // Parse context references - these become pointers in the context struct
    for (int i = 0; i < ctx_ln; i++) {
        Au_t  ctx_arg  = (Au_t)array_get(ctx_mem, i);
        enode ctx_expr = parse_expression(a, u(etype, ctx_arg->src));
        
        validate(ctx_expr, "expected context variable for %s", ctx_arg->ident);

        // Take address of the expression to store in context struct
        push(ctx, (Au)ctx_expr);
        
        if (i < ctx_ln - 1)
            validate(read_if(a, ","), "expected comma between context values");
    }
    
    validate(read_if(a, "]"), "expected ] after lambda context");
    
    // Create lambda instance: packages function pointer + context struct
    copy_lambda_info(mem, lambda_f);

    return e_create(a, (etype)mem, (Au)ctx);
}

enode eshape_from_indices(aether a, array indices);

enode silver_parse_member_expr(silver a, enode mem) {
    push_current(a);

    macro is_macro = instanceof(mem, macro);
    bool is_lambda_call = inherits(mem->au, typeid(lambda));
    int indexable = !is_func((Au)mem) && !is_func_ptr((Au)mem) && !is_macro && !is_lambda_call;

    /// handle compatible indexing methods / lambda / and general pointer dereference @ index
    if (indexable && next_is(a, "[")) {
        Au_t au_rec = is_rec((Au)mem);
        etype r = u(etype, au_rec);
        /// must have an indexing method, or be a reference_pointer
        validate(is_ptr((Au)mem) || r, "no indexing available for model %s",
                 mem->au->ident);

        /// we must read the arguments given to the indexer
        consume(a);
        array args = array(16);
        if (r && mem->target)
            push(args, (Au)mem->target);
        enode first_index = null;
        while (!next_is(a, "]")) {
            enode expr = parse_expression(a, null);
            if (!first_index && expr)
                first_index = expr;
            push(args, (Au)expr);
            validate(next_is(a, "]") || next_is(a, ","), "expected ] or , in index arguments");
            if (next_is(a, ","))
                consume(a);
        }
        validate(next_is(a, "]"), "expected ] after index expression");
        consume(a);
        enode index_expr = null;
        if (r) {
            Au_t idx = find_member(r->au, null, AU_MEMBER_INDEX, true);
            validate(idx, "index method not found on %o", mem);
            enode eshape = eshape_from_indices((aether)a, args);
            index_expr = e_fn_call(a, (efunc)u(efunc, idx), a(mem, eshape));
        } else {
            if (len(args) > 1) {
                enode eshape = eshape_from_indices((aether)a, args);
                index_expr = e_offset(a, mem, (Au)eshape);
            } else
                index_expr = e_offset(a, mem, (Au)first_index);
        }
        pop_tokens(a, true);
        return index_expr;
    } else if (is_macro) {

        // function-like macro without ( — not an invocation, skip expansion (allowing namespace for other members)
        //if (is_macro->params && !next_is(a, "(")) {
        //    pop_tokens(a, true);
        //    return mem;
        //}

        verify(!is_macro->params || read_if(a, "("), "expected ( for functional macros");
        array args = is_macro->params ? array(alloc, 32) : null;
        macro mac  = (macro)mem;

        // read arguments
        if (is_macro->params) {
            while (peek(a) && !next_is(a, ")")) {
                int next;
                array arg = read_arg((array)a->tokens, a->cursor, &next);
                validate(arg, "macro expansion failed");
                if (arg)
                    push(args, (Au)arg);
                a->cursor = next;
                if (!read_if(a, ","))
                    break;
            }
            validate(read_if(a, ")"), "expected parenthesis to end macro func call");
        }

        print_all_tokens(a, "macro", is_macro->def);

        // expand macro
        token f = (token)first_element((array)mac->def);
        array exp = macro_expand(mac, args);
        print_all_tokens(a, "expanded", exp);

        push_tokens(a, (tokens)exp, 0);
        bool cmode = a->cmode;
        a->cmode = true;
        mem = parse_expression(a, null);
        a->cmode = cmode;
        pop_tokens(a, false);

    } else if (mem) {
        if (is_func((Au)mem) || is_func_ptr((Au)mem) || is_lambda_call) {
            if (!is_lambda_call && is_lambda((Au)mem))
                mem = parse_create_lambda(a, mem);
            else if (is_lambda_call)
                mem = parse_lambda_call( a, (efunc)mem);
            else
                mem = parse_func_call(a, (efunc)mem);
        
        } else if (is_type((Au)mem)) {
            array expr = read_within(a);
            inspect(mem);
            mem = typed_expr(a, mem, expr); // this, is the construct
        }
    }
    pop_tokens(a, mem != null);
    return mem;          
}

etype etype_of(enode mem) {
    aether a = mem->mod;
    return (mem->au->member_type == AU_MEMBER_VAR) ?
                (etype)u(etype, mem->au->src) :
           (mem->au->member_type == AU_MEMBER_DECL) ?
                (etype)null : (etype)mem;
}


enode silver_parse_assignment(silver a, enode mem, OPType op_val, bool is_const) {
    validate(isa(mem) == typeid(enode) || !mem->au->is_const,
        "mem %s is a constant", mem->au->ident);
    
    etype t = (etype)etype_of(mem);
    enode R = parse_expression(a, t); 

    // Handle Promotion and Inference for AU_MEMBER_DECL
    if (mem->au->member_type == AU_MEMBER_DECL) {

        // Promote the member to a variable
        Au_t ctx = top_scope(a);
        mem->au->context = ctx;
        mem->au->member_type = AU_MEMBER_VAR;
        mem->au->src = R->au;
        mem->au->is_const = is_const;
        rm(a->registry, (Au)mem->au);

        mem = (enode)evar(mod, (aether)a, au, mem->au, loaded, true, meta, R->meta);

        // Register and allocate the variable in the backend
        etype_implement((etype)mem);
    }

    enode result = e_assign(a, mem, (Au)R, op_val);
    mem->au->is_assigned = true;
    return mem;
}

enode expr_builder(silver a, array cond_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    enode cond_expr = parse_expression(a, null);
    validate(a->cursor == len(cond_tokens), "expected condition expression, found remaining: %o", peek(a));
    pop_tokens(a, false);
    a->expr_level--;
    return cond_expr;
}

enode cond_builder(silver a, array cond_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    enode cond_expr = parse_expression(a, etypeid(bool));
    validate(a->cursor == len(cond_tokens), "expected condition expression, found remaining: %o", peek(a));
    pop_tokens(a, false);
    a->expr_level--;
    return cond_expr;
}

// singular statement (not used)
enode statement_builder(silver a, array expr_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    push_tokens(a, (tokens)expr_tokens, 0);
    enode expr = parse_statement(a);
    pop_tokens(a, false);
    a->expr_level = level;
    return expr;
}

enode block_builder(silver a, array block_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    push_tokens(a, (tokens)block_tokens, 0);
    print_all_tokens(a, "block_builder", block_tokens);
    last = parse_statements(a, true);
    pop_tokens(a, false);
    a->expr_level = level;
    return last;
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_builder(silver a, array expr_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    push_tokens(a, (tokens)expr_tokens, 0);
    bool first = true;
    while (peek(a)) {
        validate(first || read_if(a, ","), "expected comma between statements");
        last = parse_statement(a);
        first = false;
    }
    a->expr_level = level;
    pop_tokens(a, false);
    return last;
}

enode exprs_builder(silver a, array expr_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    enode last = null;
    push_tokens(a, (tokens)expr_tokens, 0);
    bool first = true;
    while (peek(a)) {
        validate(first || read_if(a, ","), "expected comma between statements");
        last = parse_statement(a);
        first = false;
    }
    pop_tokens(a, false);
    a->expr_level--;
    return last;
}

// we must have separate if/ifdef, since ifdef does not push the variable scope (it cannot by design)
// being adaptive to this based on constant status is too ambiguous

// this needs to perform const expressions (test this; also rename to something like if-const)
enode parse_ifdef_else(silver a) {
    bool require_if = true;
    bool one_truth = false;
    bool expect_last = false;
    enode statements = null;

    verify(a->expr_level == 0, "unexpected expression level at ifdef");

    while (true) {
        validate(!expect_last, "continuation after else");
        bool is_if = read_if(a, "ifdef") != null;
        validate(is_if && require_if || !require_if, "expected if");
        a->expr_level++;
        enode n_cond = is_if ? parse_expression(a, null
        ) : null;
        a->expr_level--;
        array block = read_body(a);
        if (n_cond) {
            Au const_v = n_cond->literal;
            if (!one_truth && const_v && cast(bool, const_v)) { // we need to make sure that we do not follow anymore in this block!
                push_tokens(a, (tokens)block, 0);                     // are we doing that?
                statements = parse_statements(a, true);
                pop_tokens(a, false);
                one_truth = true; /// we passed the condition, so we cannot enter in other blocks.
            }
        } else if (!one_truth) {
            validate(!is_if, "if statement incorrect");
            push_tokens(a, (tokens)block, 0);
            statements = parse_statements(a, true); /// make sure parse-statements does not use its own members
            pop_tokens(a, false);
            expect_last = true;
        }
        if (!is_if)
            break;
        bool next_else = read_if(a, "else") != null;
        if (!next_else)
            break;
        require_if = false;
    }

    verify(a->expr_level == 0, "unexpected expression level after ifdef");
    return statements ? statements : enode(mod, (aether)a, au, null);
}

/// parses entire chain of if, [else-if, ...] [else]
// if the cond is a constant evaluation then we do not build the condition in with LLVM build, but omit the blocks that are not used
// and only proceed
enode parse_if_else(silver a) {
    validate(read_if(a, "if") != null, "expected if");

    array tokens_cond  = array(32);
    array tokens_block = array(32);

    // first if
    array cond  = read_within(a);
    verify(cond, "expected [condition] after if");
    array block = read_body(a);
    verify(block, "expected body");
    push(tokens_cond,  (Au)cond);
    push(tokens_block, (Au)block);

    // chain of el [cond] / el
    while (read_if(a, "el")) {
        array cond  = read_within(a); // null when no [...] → final else
        array block = read_body(a);
        verify(block, "expected body after el");
        push(tokens_cond,  (Au)(cond ? cond : array()));
        push(tokens_block, (Au)block);
        if (!cond)
            break; // bare el is terminal
    }

    subprocedure build_cond = subproc(a, cond_builder, null);
    subprocedure build_expr = subproc(a, block_builder, null);
    return e_if_else(a, tokens_cond, tokens_block, build_cond, build_expr);
}

enode parse_switch(silver a) {

    validate(read_if(a, "switch") != null, "expected switch");
    enode e_expr = parse_expression(a, null);
    map cases = map(hsize, 16);
    array expr_def = null;
    bool all_const = is_prim((Au)e_expr);

    while (true) {
        if (read_if(a, "case")) {
            bool is_const = false;
            etype mdl_read = null;
            array meta_read = null;
            array value = read_expression(a, &mdl_read, &is_const);
            all_const &= is_const && (canonical(mdl_read) == canonical(e_expr));
            array body = read_body(a);
            //value->deep_compare = true; // tell array when its given a compare to not just do silver_element equation but effectively deep compare (i will do this)
            set(cases, (Au)value, (Au)body);
            continue;
        } else if (read_if(a, "default")) {
            expr_def = read_body(a);
            continue;
        } else
            break;
    }

    subprocedure build_expr = subproc(a, expr_builder, null);
    subprocedure build_body = subproc(a, statements_builder, null);
    if (all_const)
        return e_native_switch(a, e_expr, cases, expr_def, build_expr, build_body);
    else
        return e_switch(a, e_expr, cases, expr_def, build_expr, build_body);
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_push_builder(silver a, array expr_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    statements s_members = statements(mod, (aether)a, au, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_scope(a, (Au)s_members);
    push_tokens(a, (tokens)expr_tokens, 0);
    bool first = true;
    while (peek(a)) {
        validate(first || read_if(a, ","), "expected comma between init expressions");
        last = parse_statement(a);
        first = false;
    }
    pop_tokens(a, false);
    a->expr_level = level;
    return last;
}

enode parse_for(silver a) {
    validate(read_if(a, "for") != null, "expected for");

    array all = read_within(a); // null if no [...] after for
    array init_exprs = array(alloc, 32);
    array cond_exprs = array(alloc, 32);
    array step_exprs = array(alloc, 32);
    bool  do_while   = false;

    if (all) {
        // split on :: to figure out which form
        int level = 0;
        array cur = init_exprs;
        each(all, token, t) {
            if (eq(t, "::")) {
                level++;
                if (level == 1)
                    cur = cond_exprs;
                else if (level == 2)
                    cur = step_exprs;
                else
                    fault("too many :: in for (max 2)");
            } else {
                push(cur, (Au)t);
            }
        }
        if (level == 0) {
            // for [cond] — no ::, so init_exprs actually holds the condition
            cond_exprs = init_exprs;
            init_exprs = array(alloc, 32);
        }
    }

    array body = read_body(a);
    verify(body, "expected for-body");

    if (!all) {
        // for \n body \n while [cond]  — do-while form
        validate(read_if(a, "while") != null,
            "for without [...] requires while [cond] after body");
        array while_tokens = read_within(a);
        verify(while_tokens, "expected [cond] after while");
        cond_exprs = while_tokens;
        do_while   = true;
    }

    subprocedure build_init = subproc(a, statements_push_builder, null);
    subprocedure build_cond = subproc(a, cond_builder, null);
    subprocedure build_step = subproc(a, exprs_builder, null);
    subprocedure build_body = subproc(a, exprs_builder, null);

    enode res = e_for(a,
        init_exprs, cond_exprs, body, step_exprs,
        build_init, build_cond, build_body, build_step,
        do_while);
    pop_scope(a);
    return res;
}

bool is_model(silver a) {
    token k = peek(a);
    enode m = (enode)elookup(k->chars);
    return m && is_type((Au)m);
}

path module_path(silver a, string name) {
    cstr exts[] = {"sf", "sr"};
    path res = null;
    path cw = path_cwd();
    for (int i = 0; i < 2; i++) {
        path r = f(path, "%o/%o.%s", cw, name, exts[i]);
        if (file_exists("%o", res)) {
            res = r;
            break;
        }
    }
    return res;
}

#undef find_member

Au_t next_is_keyword(silver a, Au_t *fn) {
    token t = peek(a);
    if (!t)
        return null;
    if (!isalpha(t->chars[0]))
        return null;
    Au_t f = find_type((cstr)t->chars, null);
    string kw = f(string, "parse_%o", t);
    if (f && inherits(f, typeid(etype)) && (*fn = find_member(f, kw->chars, AU_MEMBER_FUNC, false)))
        return f;
    return null;
}

/// called after : or before, where the user has access
etype silver_read_def(silver a) {
    Au_t  parse_fn  = null;
    Au_t  is_type   = next_is_keyword(a, &parse_fn);
    etype is_class  = !is_type ? next_is_class(a, false) : null;
    bool  is_struct = next_is(a, "struct");
    bool  is_enum   = next_is(a, "enum");

    if (!is_type && !is_class && !is_struct && !is_enum)
        return null;

    if (is_type) {
        struct _etype* (*parser)(silver) = (void*)parse_fn->value;
        validate(parser, "expected parse fn on member");
        return parser(a);
    }

    consume(a);
    string n = read_alpha(a);
    validate(n, "expected alpha-numeric identity, found %o", next(a));

    Au_t top = top_scope(a);
    etype mtop = u(etype, top);
    enode mem  = null; // = emember(mod, (aether)a, name, n, context, mtop);
    etype mdl  = null;
    array meta = null;

    if (eq(n, "export")) {
        int test2 = 2;
        test2    += 2;
    }
    else if (is_class || is_struct) {
        validate(is_module(mtop),
            "expected record definition at module level");
        array schema = array();

        // would be nice to use meta here
        // however, meta must be both setting default type info at slot and dictating their use
        // its difficult to really achieve this with one type
        // currently we have 'any'
        /*
        if (read_if(a, "[")) {
            is_class = read_etype(a, null);
            validate(is_class, "expected class, found %o", peek(a));
            validate(read_if(a, "]"), "expected ] after base type");
        }
        */

        mdl = record(a, (etype)a, is_class, n,
            is_struct ? AU_TRAIT_STRUCT : AU_TRAIT_CLASS);

        // read meta args (some of these will permute the type, and others are run-time)
        if (read_if(a, "<")) {
            bool first = true;
            int index = 0;
            while (!read_if(a, ">")) {
                if (!first)
                    verify(read_if(a, ","), "expected ',' seperator between models");
                string n = read_alpha(a);
                verify(n, "expected identifier");
                verify(read_if(a, ":"), "expected : after %o", n);
                etype type = read_etype(a, null);
                verify(type, "expected model after :");
                bool  f   = true;
                shape s   = null;
                Au_t  m   = def_member(top, n->chars, type->au, AU_MEMBER_VAR, AU_TRAIT_META);
                set(a->registry, (Au)m, (Au)hold(emeta(mod, (aether)a, au, m, meta_index, index)));
                array_qpush((array)&mdl->au->meta, (Au)m);
                first = false;
            }
        }

        mdl->body = (tokens)read_body(a);
        print_all(a, "class", (array)mdl->body);

    } else if (is_enum) {
        etype store = null, suffix = null;
        bool expect_bracket = false;

        if (read_if(a, "[")) {
            store  = instanceof(read_etype(a, null), etype);
            validate(store, "invalid storage type");
            validate(read_if(a, "]"), "expected ] after storage type");
        } else
            store = etypeid(i32);
        
        array enum_body = read_body(a);
        print_all(a, "enum-tokens", enum_body);
        validate(len(enum_body), "expected body for enum %o", n);

        Au_t enum_au = def(top_scope(a),
            n->chars, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
        enum_au->src = store->au;
        mdl = etype(mod, (aether)a, au, enum_au);

        push_tokens(a, (tokens)enum_body, 0);
        push_scope(a, (Au)mdl);
        validate(enum_au->src->is_integral,
                 "enumeration can only be based on integral types (i32 default)");
        i64 value = 0;
        while (true) {
            token e = next(a);
            if  (!e) break;
            Au    v = null;

            bool is_explicit = read_if(a, ":") != null;

            if (is_explicit) {
                enode n = parse_expression(a, store);
                Au lit = n ? literal_value(n,
                    isa(n->literal) == typeid(shape) ? typeid(i64) : isa(n->literal)) : null;
                verify(n && ((Au_t)isa(lit))->is_integral,
                    "expected integral literal");
                v = lit;
                u8 sp[64];
                memcpy(sp, lit, ((Au_t)isa(lit))->abi_size / 8);
                value = *(i64*)sp; // primitives would need a minimum allocation size in this case
            } else
                v = primitive(store->au, &value); /// this creates i8 to i64 data, using &value i64 addr
            
            Au_t enum_v         = def_enum_value(enum_au, e->chars, v);
            enum_v->src         = enum_au->src;
            enum_v->value       = (object)hold(v);
            enum_v->member_type = AU_MEMBER_ENUMV;
            enum_v->is_const    = true;
            
            enode enum_node = enode(
                mod, (aether)a, au, enum_v, literal, v);
            set(a->registry, (Au)enum_v, (Au)hold(enum_node));
            implement(enum_node); // this should create the value similarly to emember_set_value
            value += 1;
        }
        pop_scope(a);
        pop_tokens(a, false);

        // this should create type data for the enum type
        create_type_members(a, a->au);

    } else {
        fault("unknown error");
    }

    validate(mdl && len(n),
             "name required for model: %s", isa(mdl)->ident);

    return mdl;
}

define_class(chatgpt, codegen)
define_class(silver, aether)
define_class(import, enamespace)

initializer(silver_module)