#include <import>
#include <limits.h>
#include <ports.h>

// for Michael

enode parse_statements(silver a, bool unique_members);
enode parse_statement(silver a);
void build_fn(silver a, enode fmem, callback preamble, callback postamble);
static void build_record(silver a, etype mrec);

#define au_lookup(sym) Au_lexical(a->lexical, sym)
#define elookup(sym) ({              \
    Au_t m = Au_lexical(a->lexical, sym); \
    (m ? m->user : (etype)null);          \
})

#define validate(cond, t, ...) ({                                                          \
    if (!(cond)) {                                                                         \
        formatter((Au_t)null, stderr, (Au) true, (symbol) "\n%o:%i:%i " t, a->source, \
                  silver_peek(a)->line, silver_peek(a)->column, ##__VA_ARGS__);     \
        if (level_err >= fault_level) {                                                 \
            raise(SIGTRAP);                                                             \
            exit(1);                                                                    \
        }                                                                               \
        false;                                                                          \
    } else {                                                                            \
        true;                                                                           \
    }                                                                                   \
    true;                                                                               \
})

token silver_element(silver, num);

void print_tokens(silver a, symbol label) {
    print("[%s] tokens: %o %o %o %o %o %o...", label,
          silver_element(a, 0), silver_element(a, 1), silver_element(a, 2), silver_element(a, 3),
          silver_element(a, 4), silver_element(a, 5));
}

static void print_all(silver a, symbol label, array list) {
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
#elif defined(_WIN32)
static symbol lib_pre = "";
static symbol lib_ext = ".dll";
static symbol app_ext = ".exe";
static symbol platform = "windows";
#elif defined(__APPLE__)
static symbol lib_pre = "lib";
static symbol lib_ext = ".dylib";
static symbol app_ext = "";
static symbol platform = "darwin";
#endif

static bool is_dbg(import t, string query, cstr name, bool is_remote) {
    cstr dbg = (cstr)query->chars; // getenv("DBG");
    char dbg_str[PATH_MAX];
    char name_str[PATH_MAX];
    sprintf(dbg_str, ",%s,", dbg);
    sprintf(name_str, "%s,", name_str);
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

etype read_etype(silver a, array *p_expr);

num index_of_cstr(Au a, cstr f) {
    Au_t t = isa(a);
    if (t == typeid(string))
        return index_of((string)a, f);
    if (t == typeid(array))
        return index_of((array)a, string(f));
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal)) {
        cstr v = strstr(a, f);
        return v ? (num)(v - f) : (num)-1;
    }
    fault("len not handled for type %s", t->ident);
    return 0;
}

static bool is_alpha(Au any) {
    if (!any)
        return false;
    Au_t type = isa(any);
    string s;
    if (type == typeid(string)) {
        s = any;
    } else if (type == typeid(token)) {
        token token = any;
        s = string(token->chars);
    }

    if (index_of_cstr(keywords, cstring(s)) >= 0)
        return false;

    if (len(s) > 0) {
        char first = s->chars[0];
        return isalpha(first) || first == '_';
    }
    return false;
}

string silver_read_alpha(silver a) {
    token n = silver_element(a, 0);
    if (is_alpha(n)) {
        a->cursor++;
        return string(n->chars);
    }
    return null;
}

string git_remote_info(path path, string *out_service, string *out_owner, string *out_project) {
    // run git command
    string cmd = f(string, "git -C %s remote get-url origin", path->chars);
    string remote = command_run(cmd);

    if (!remote || !remote->count)
        error("git_remote_info: failed to get remote url");

    cstr url = remote->chars;

    // strip trailing newline(s)
    for (int i = remote->count - 1; i >= 0 && (url[i] == '\n' || url[i] == '\r'); i--)
        url[i] = '\0';

    cstr domain = NULL, owner = NULL, repo = NULL;

    if (strstr(url, "://")) {
        // HTTPS form: https://github.com/owner/repo.git
        domain = strstr(url, "://");
        domain += 3; // skip "://"
    } else if (strchr(url, '@')) {
        // SSH form: git@github.com:owner/repo.git
        domain = strchr(url, '@') + 1;
    } else {
        error("git_remote_info: unrecognized URL: %s", url);
    }

    // domain ends at first ':' or '/'
    cstr domain_end = strpbrk(domain, ":/");
    if (!domain_end)
        error("git_remote_info: malformed URL");
    *domain_end = '\0';

    // next part: owner/repo
    cstr next = domain_end + 1;
    owner = next;

    cstr slash = strchr(owner, '/');
    if (!slash)
        error("git_remote_info: missing owner/repo");
    *slash = '\0';
    repo = slash + 1;

    // remove .git if present
    cstr dot = strrchr(repo, '.');
    if (dot && strcmp(dot, ".git") == 0)
        *dot = '\0';

    if (!*out_service)
        *out_service = string(domain);
    if (!*out_owner)
        *out_owner = string(owner);
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
            push(res, f);
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
        token t = tokens->origin[i];
        token prev = tokens->origin[i - 1];
        if (!current)
            current = copy(prev);

        if (prev->line == t->line && (prev->column + prev->count) == t->column) {
            concat(current, t);
            current->column += t->count;
        } else {
            push(res, current);
            current = copy(t);
        }
    }
    if (current)
        push(res, current);
    return res;
}

string model_keyword() {
    return null;
}

int main(int argc, cstrs argv) {
    Au_engage(argv);
    silver a = silver(argv);
    return 0;
}

typedef struct {
    OPType ops[2];
    string method[2];
    string token[2];
} precedence;

static precedence levels[] = {
    {{OPType__mul, OPType__div}},
    {{OPType__add, OPType__sub}},
    {{OPType__right, OPType__left}},
    {{OPType__greater, OPType__less}},
    {{OPType__greater_eq, OPType__less_eq}},
    {{OPType__equal, OPType__not_equal}},
    {{OPType__is, OPType__inherits}},
    {{OPType__xor, OPType__xor}},
    {{OPType__and, OPType__or}},
    {{OPType__bitwise_and, OPType__bitwise_or}},
    {{OPType__value_default, OPType__cond_value}} // i find cond-value to be odd, but ?? (value_default) should work for most
};

token silver_read_if(silver a, symbol cs);

static enode reverse_descent(silver a, etype expect) {
    bool cmode = a->cmode;
    enode L = silver_read_enode(a, expect); // build-arg
    token t = silver_peek(a);
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
                if (!silver_read_if(a, cstring(token)))
                    continue;
                OPType op_type = level->ops[j];
                string method = level->method[j];
                enode R = silver_read_enode(a, null);
                L = e_op(a, op_type, method, L, R);
                m = true;
                break;
            }
        }
    }
    a->expr_level--;
    return L;
}

static enode parse_expression(silver a, etype expect) {
    print_tokens(a, "parse-expr");
    enode vr = reverse_descent(a, expect);
    return vr;
}

static array parse_tokens(silver a, Au input, array output);


Au build_init_preamble(enode f, Au arg) {
    silver a = f->mod;
    etype rec = f->target_arg ? resolve((etype)f->target_arg) : (etype)a;

    members(rec->au, mem) {
        enode n = instanceof(mem->user, typeid(enode));
        if (n && n->initializer)
            build_initializer(a, n);
    }
    return null;
}

void silver_parse(silver a) {
    enode init = module_initializer(a); // publish initializer
    while (silver_peek(a)) {
        print_tokens(a, "parse");
        enode res = parse_statement(a);
        validate(res, "unexpected token found for statement: %o", silver_peek(a));
        incremental_resolve(a); // too much in the stack afterwards
    }

    build_fn(a, init, build_init_preamble, null);
    implement(init);
}

// im a module!
void silver_init(silver a) {
    bool is_once = a->build; // -b or --build [ single build, no watching! ]

    a->instances    = hold(map(hsize, 16));
    a->import_cache = hold(map(hsize, 16));

    if (!a->source) {
        fault("required argument: source-file");
    }
    if (a->source)
        a->source = absolute(a->source);
    if (!a->install) {
        cstr import = getenv("IMPORT");
        if (import)
            a->install = f(path, "%s", import);
        else {
            path exe = path_self();
            path bin = parent_dir(exe);
            path install = absolute(f(path, "%o/..", bin));
            a->install = install;
        }
    }
    a->project_path = parent_dir(a->source);

    verify(dir_exists("%o", a->install), "silver-import location not found");
    verify(len(a->source), "no source given");
    verify(file_exists("%o", a->source), "source not found: %o", a->source);

    verify(exists(a->source), "source (%o) does not exist", a->source);

    cstr _SRC = getenv("SRC");
    cstr _DBG = getenv("DBG");
    cstr _IMPORT = getenv("IMPORT");
    verify(_IMPORT, "silver requires IMPORT environment");

    a->mod = a;
    a->imports = array(32);
    a->parse_f = parse_tokens;
    a->parse_expr = parse_expression;
    a->read_etype = read_etype;
    a->src_loc = absolute(path(_SRC ? _SRC : "."));
    verify(dir_exists("%o", a->src_loc), "SRC path does not exist");

    path af = path_cwd();
    path install = path(_IMPORT);
    git_remote_info(af, &a->git_service, &a->git_owner, &a->git_project);

    bool retry = false;
    i64 mtime = modified_time(a->source);
    do {
        if (retry) {
            print("awaiting iteration: %o", a->source);
            hold_members(a);
            Au_recycle();
            mtime = path_wait_for_change(a->source, mtime, 0);
            print("rebuilding...");
            drop(a->tokens);
            drop(a->stack);
            reinit_startup(a);
        }
        retry = false;
        a->tokens = tokens(
            target, a, parser, parse_tokens, input, a->source);
        a->stack = array(4);
        a->implements = array();

        // our verify infrastructure is now production useful
        attempt() {
            string m = stem(a->source);
            path i_gen = f(path, "%o/%o.i", a->project_path, m);
            path c_file = f(path, "%o/%o.c", a->project_path, m);
            path cc_file = f(path, "%o/%o.cc", a->project_path, m);
            path files[2] = {c_file, cc_file};
            for (int i = 0; i < 2; i++)
                if (exists(files[i])) {
                    if (!a->implements)
                        a->implements = array(2);
                    push(a->implements, files[i]);
                }
            parse(a);
            build(a);

            if (len(a->implements))
                write_header(a);
        }
        on_error() {
            retry = !is_once;
        }
        finally()
    } while (retry);
}

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

static void initialize() {
    keywords = array_of_cstr(
        "class", "struct", "public", "intern",
        "import", "export", "typeid", "context",
        "is", "inherits", "ref",
        "const", "require", "no-op", "return", "->", "::", "...",
        "asm", "if", "switch", "any", "enum",
        "ifdef", "else", "while", "cast", "forge", "try", "throw", "catch", "finally",
        "for", "loop", "func", "operator", "index", "ctr",
        null);

    assign = array_of_cstr(
        "=", ":", "+=", "-=", "*=", "/=",
        "|=", "&=", "^=", "%=", ">>=", "<<=",
        null);

    compare = array_of_cstr(
        "==", "!=", "<=>", ">=", "<=", ">", "<",
        null);

    operators = map_of( // aether needs some operator bindings
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
        "=", string("assign"),
        ":", string("assign"), // dynamic behavior on this, turns into "equal" outside of parse-assignment
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
        null);

    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 2; j++) {
            OPType op = level->ops[j];
            string e_name = e_str(OPType, op);
            string op_name = mid(e_name, 1, len(e_name) - 1);
            string op_token = op_lang_token(op_name);
            level->method[j] = op_name; // replace the placeholder; assignment is outside of precedence; the camel has spoken
            level->token[j] = eq(op_name, "equal") ? string("==") : op_token;
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
    token t = a->tokens->origin[r];
    return t;
}

bool silver_next_indent(silver a) {
    token p = silver_element(a, -1);
    token n = silver_element(a, 0);
    return p && n->indent > p->indent;
}

#define next_is(a, ...) silver_next_is_eq(a, __VA_ARGS__, null)

static bool silver_next_is_eq(silver a, symbol first, ...) {
    va_list args;
    va_start(args, first);
    int i = 0;
    symbol cs = first;
    while (cs) {
        token n = silver_element(a, i);
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

token silver_next(silver a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = silver_element(a, 0);
    a->cursor++;
    return res;
}

token silver_consume(silver a) {
    return silver_next(a);
}

static array read_within(silver a) {
    array body = array(32);
    token n = silver_element(a, 0);
    bool proceed = a->expr_level == 0 ? true : eq(n, "[");
    if (!proceed)
        return null;

    bool bracket = eq(n, "[");
    silver_consume(a);
    int depth = bracket == true; // inner expr signals depth 1, and a bracket does too.  we need both togaether sometimes, as in inner expression that has parens
    for (;;) {
        token inner = silver_next(a);
        if (!inner)
            break;
        if (eq(inner, "]"))
            depth--;
        if (eq(inner, "["))
            depth++;
        if (depth > 0) {
            push(body, inner);
            continue;
        }
        break;
    }
    return body;
}

token silver_peek(silver a) {
    if (a->cursor == len(a->tokens))
        return null;
    return silver_element(a, 0);
}

static array read_body(silver a) {
    array body = array(32);
    token n = silver_element(a, 0);
    if (!n)
        return null;
    token p = silver_element(a, -1);
    bool mult = n->line > p->line;

    while (1) {
        token k = silver_peek(a);
        if (!k)
            break;
        if (!mult && k->line > n->line)
            break;
        if (mult && k->indent <= p->indent)
            break;
        push(body, k);
        silver_consume(a);
    }
    return body;
}

num silver_current_line(silver a) {
    token t = silver_element(a, 0);
    return t->line;
}

string silver_location(silver a) {
    token t = silver_element(a, 0);
    return t ? (string)location(t) : (string)form(string, "n/a");
}

token silver_navigate(silver a, int count) {
    if (a->cursor <= 0)
        return null;
    a->cursor += count;
    token res = silver_element(a, 0);
    return res;
}

token silver_prev(silver a) {
    if (a->cursor <= 0)
        return null;
    a->cursor--;
    token res = silver_element(a, 0);
    return res;
}

static bool is_keyword(Au any) {
    Au_t type = isa(any);
    string s;
    if (type == typeid(string))
        s = any;
    else if (type == typeid(token))
        s = string(((token)any)->chars);

    return index_of_cstr(keywords, cstring(s)) >= 0;
}

string silver_read_keyword(silver a) {
    token n = silver_element(a, 0);
    if (n && is_keyword(n)) {
        a->cursor++;
        return string(n->chars);
    }
    return null;
}

string silver_peek_keyword(silver a) {
    token n = silver_element(a, 0);
    return (n && is_keyword(n)) ? string(n->chars) : null;
}

bool silver_next_is_alpha(silver a) {
    if (silver_peek_keyword(a))
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
        string f = get(m, s);
        if (f) {
            last_match = f;
            i++;
        } else
            break;
    }
    return last_match;
}

static array parse_tokens(silver a, Au input, array output) {
    string input_string;
    Au_t type = isa(input);
    path src = null;
    if (type == typeid(path)) {
        src = input;
        input_string = load(src, typeid(string), null); // this was read before, but we 'load' files; way less conflict wit posix
    } else if (type == typeid(string))
        input_string = input;
    else
        assert(false, "can only parse from path");

    a->source_raw = hold(input_string);

    list symbols = list();
    string special = string(".$,<>()![]/+*:=#");
    i32 special_ln = len(special);
    for (int i = 0; i < special_ln; i++)
        push(symbols, string((i32)special->chars[i]));
    each(keywords, string, kw) push(symbols, kw);
    each(assign, string, a) push(symbols, a);
    each(compare, string, a) push(symbols, a);
    pairs(operators, i) push(symbols, i->key);
    sort(symbols, sfn);
    map mapping = map(hsize, 32);
    for (item i = symbols->first; i; i = i->next) {
        string sym = i->value;
        set(mapping, sym, sym);
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

        if (!a->cmode && chr == '#') {
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
            push(tokens, t);
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
                push(tokens, token(
                                 chars, ch,
                                 indent, indent,
                                 source, src,
                                 line, line_num,
                                 column, start - line_start));
                crop = string(&crop->chars[1]);
                line_start++;
            }
            push(tokens, token(
                             chars, crop->chars,
                             indent, indent,
                             source, src,
                             line, line_num,
                             column, start - line_start));
            continue;
        }

        num start = index;
        bool is_dim = false;
        bool last_dash = false;
        while (index < length) {
            i32 v = idx(input_string, index);
            char sval[2] = {v, 0};
            bool is_dash = v == '-';
            is_dim = (num_start && v == 'x');
            if (isspace(v) || index_of(special, sval) >= 0 || is_dim) {
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
        push(tokens, token(
                         chars, crop->chars,
                         indent, indent,
                         source, src,
                         line, line_num,
                         column, start - line_start));
        if (is_dim) {
            push(tokens, token(
                             chars, "x",
                             indent, indent,
                             source, src,
                             line, line_num,
                             column, index - line_start));
            index++;
        }
    }
    return tokens;
}

token silver_read_if(silver a, symbol cs) {
    token n = silver_element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        a->cursor++;
        return n;
    }
    return null;
}

Au silver_read_literal(silver a, Au_t of_type) {
    token n = silver_element(a, 0);
    if (n && n->literal) {
        a->cursor++;
        return n->literal;
    }
    return null;
}

string silver_read_string(silver a) {
    token n = silver_element(a, 0);
    if (n && isa(n->literal) == typeid(string)) {
        string token_s = string(n->chars);
        string result = mid(token_s, 1, token_s->count - 2);
        a->cursor++;
        return result;
    }
    return null;
}

Au silver_read_numeric(silver a) {
    token n = silver_element(a, 0);
    if (n && (isa(n->literal) == typeid(f64) || isa(n->literal) == typeid(i64))) {
        a->cursor++;
        return n->literal;
    }
    return null;
}

static etype next_is_class(silver a, bool read_token) {
    token t = silver_peek(a);
    if (!t)
        return null;
    if (eq(t, "class")) {
        if (read_token)
            silver_consume(a);
        return elookup("Au");
    }

    etype f = elookup(t->chars);
    if (is_class(f)) {
        if (read_token)
            silver_consume(a);
        return f;
    }
    return null;
}

string silver_peek_def(silver a) {
    token n = silver_element(a, 0);
    etype t = top_scope(a)->user;
    etype rec = is_rec(t) ? t : null;
    if (!rec && next_is_class(a, false))
        return string(n->chars);

    if (n && is_keyword(n))
        if (eq(n, "import") || eq(n, "func") || eq(n, "cast") ||
            eq(n, "class") || eq(n, "enum") || eq(n, "struct"))
            return string(n->chars);
    return null;
}

Au silver_read_bool(silver a) {
    token n = silver_element(a, 0);
    if (!n)
        return null;
    bool is_true = strcmp(n->chars, "true") == 0;
    bool is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool)
        a->cursor++;
    return is_bool ? _bool(is_true) : null;
}

// this merely reads the various assignment operators if set, and gives a constant bool
string silver_read_assign(silver a, ARef assign_type, ARef is_const) {
    token n = silver_element(a, 0);
    if (!n)
        return null;
    string k = string(n->chars);
    num assign_index = index_of(assign, k);
    bool found = assign_index >= 0;

    if (is_const)
        *(bool *)is_const = false;
    if (assign_type)
        *(OPType *)assign_type = OPType__undefined;

    if (found) {
        if (eq(k, ":")) {
            if (is_const)
                *(bool *)is_const = true;
            if (assign_type)
                *(OPType *)assign_type = OPType__assign;
        } else {
            if (is_const)
                *(bool *)is_const = false;
            if (assign_type)
                *(OPType *)assign_type = OPType__assign + assign_index;
        }
        a->cursor++;
    }
    return found ? k : null;
}

string silver_read_alpha_any(silver a) {
    token n = silver_element(a, 0);
    if (n && isalpha(n->chars[0])) {
        a->cursor++;
        return string(n->chars);
    }
    return null;
}

string silver_peek_alpha(silver a) {
    token n = silver_element(a, 0);
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

enode parse_fn(silver a, string ident, u8 member_type, u32 traits, OPType assign_enum);

enode silver_read_enode(silver a, etype mdl_expect) {
    print_tokens(a, "read-node");
    bool      cmode     = a->cmode;
    array     expr      = null;
    token     peek      = peek(a);
    bool      is_expr0  = !a->cmode && a->expr_level == 0;
    interface access    = is_expr0 ? read_enum(a, interface_undefined, typeid(interface)) : 0;
    bool      is_static = is_expr0 && read_if(a, "static") != null;
    string    kw        = is_expr0 ? peek_keyword(a) : null;
    bool      is_oper   = kw && eq(kw, "operator");
    bool      is_index  = kw && eq(kw, "index");
    bool      is_cast   = kw && eq(kw, "cast");
    bool      is_fn     = kw && eq(kw, "func");
    etype     rec_ctx   = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    Au_t      top       = top_scope(a);
    etype     rec_top   = (!a->cmode && is_rec(top)) ? top : null;
    enode     f         = !a->cmode ? context_func(a) : null;
    silver    module    = !a->cmode && (top->is_namespace) ? a : null;
    enode     mem       = null;
    // todo: lost notion of static vs imethod here, reintroduce
    AFlag     mtype     = (is_fn && !is_static) ? AU_MEMBER_FUNC : (!is_static && is_cast) ? AU_MEMBER_CAST : AU_MEMBER_FUNC;

    if (is_expr0 && !a->cmode && (module || rec_ctx) && peek_def(a)) {
        mem = read_def(a);
        if (!mem) {
            if (is_cast || is_fn) consume(a);
            token alpha = !is_cast ? read_alpha(a) : null;
            enode rmem = alpha ? elookup(alpha->chars) : null;
            if (rmem && rmem->au->context == module->au) {
                validate(!is_cast && is_fn, "invalid constructor for module; use func keyword");
                mtype = AU_MEMBER_CONSTRUCT;
            } else if (rec_ctx && rmem && canonical(rmem) == canonical(rec_ctx)) {
                validate(!is_cast && !is_fn, "invalid constructor for class; use class-name[] [ no func, not static ]");
                mtype = AU_MEMBER_CONSTRUCT;
            }

            bool has_args = is_cast || next_is(a, "[");
            validate(has_args, "expected func args for %o", alpha);

            mem = parse_fn(a, alpha, mtype,
                is_static ? AU_TRAIT_STATIC : 0, OPType__undefined);
            mem->au->access_type = access;
        }
        return mem;
    }

    if (peek && is_alpha(peek)) {
        etype m = elookup(peek->chars);
        if (m && isa(m) != typeid(macro)) {
            etype mdl_found = read_etype(a, &expr);
            if (mdl_found) {
                enode res = typed_expr(a, mdl_found, expr);
                return e_create(a, mdl_expect, res); // we need a 'meta_expect' here
            }
        }
    }

    Au lit = read_literal(a, null);
    if (lit) {
        a->expr_level++;
        enode res = e_operand(a, lit, mdl_expect);
        a->expr_level--;
        return e_create(a, mdl_expect, res);
    }
    
    if (!cmode && next_is(a, "$", "(")) {
        consume(a);
        consume(a);
        fault("shell syntax not implemented for 88");
    }

    if (!cmode && read_if(a, "null"))
        return e_null(a, mdl_expect);

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
                    enode res = e_create(a, inner, parse_expression(a, inner));
                    return e_create(a, mdl_expect, res);
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
        token t = read_if(a, ")");
        if (t) a->parens_depth--;
        return e_create(a, mdl_expect, 
            parse_ternary(a, expr, mdl_expect));
    }

    if (!cmode && next_is(a, "[")) {
        validate(mdl_expect, "expected model name before [");
        array expr = read_within(a);
        // we need to get mdl from argument
        enode r = typed_expr(a, mdl_expect, expr);
        return r;
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read_if(a, "!") || (!cmode && read_if(a, "not"))) {
        enode expr = parse_expression(a, null); // Parse the following expression
        return e_create(a,
            mdl_expect, e_not(a, expr));
    }

    // bitwise NOT operator
    else if (read_if(a, "~")) {
        enode expr = parse_expression(a, null);
        return e_create(a,
            mdl_expect, e_bitwise_not(a, expr));
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    else if (!cmode && read_if(a, "typeid")) {
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
            mdl_expect, expr); // Return the type reference
    }

    // 'ref' operator (reference)
    else if (!cmode && read_if(a, "ref")) {
        a->in_ref = true;
        enode expr = parse_expression(a, null);
        a->in_ref = false;
        return e_create(a,
            mdl_expect, e_addr_of(a, expr, null));
    }

    // we may only support a limited set of C functionality for #define macros
    if (cmode) return null;

    push_current(a);
    
    string    alpha     = null;
    int       depth     = 0;
    
    bool skip_member_check = false;
    bool avoid_model_lookup = a->read_etype_abort;
    if (!skip_member_check && module) {
        string alpha = peek_alpha(a);
        if (alpha && !a->read_etype_abort) {

            Au_t tt = Au_global();
            Au_t v0 = a->lexical->origin[0];
            Au_t v1 = a->lexical->origin[1];
            Au_t v2 = a->lexical->origin[2];

            Au_t bb = Au_lexical(a->lexical, "main");

            Au_t aa = Au_find_member(Au_global(), "main", AU_MEMBER_TYPE);
            aa = aa;

            enode m = elookup(alpha->chars); // silly read as string here in int [ silly.len ]
            etype mdl = resolve(m);
            if (m && m->au->member_type == AU_MEMBER_TYPE && is_class(mdl))
                skip_member_check = true;
        }
        a->read_etype_abort = false; // state var used for this purpose with the singular call to read-model above
    }

    for (;!skip_member_check;) {
        bool first = !mem;
        if (first && (is_fn || is_cast)) consume(a);
        
        alpha = read_alpha(a);
        if (!alpha) {
            validate(mem == null, "expected alpha ident after .");
            break;
        }

        /// Namespace resolution (only on first iteration)
        bool ns_found = false;
        if (first && !avoid_model_lookup) {
            members (a->au, im) {
                if (!im->is_namespace || im->is_nameless) continue;
                if (im->ident && eq(alpha, im->ident)) {
                    string module_name = alpha;
                    validate(read_if(a, "."), "expected . after module-name: %o", alpha);
                    alpha = read_alpha(a);
                    validate(alpha, "expected alpha-ident after module-name: %o", module_name);
                    mem = elookup(alpha->chars);
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
            if (first) {

                // try implicit 'this' access in instance methods
                if (!mem && f && f->target_arg) {
                    Au_t field = Au_find_member(resolve(f->target_arg)->au, alpha->chars, 0);
                    if (field) {
                        mem = access(f->target_arg, alpha);
                    } else {
                        mem = elookup(alpha->chars);
                    }
                } else if (!f || !f->target_arg) {
                    mem = elookup(alpha->chars);
                }
                
                // Still not found? Create new member (module-level definition)
                if (!mem) {
                    validate(a->expr_level == 0, "member not found: %o", alpha);
                    mem = enode(mod, a, name, alpha, member, null);
                    if (is_static) mem->au->traits |= AU_TRAIT_STATIC;
                    mem->au->access_type = access;
                }
            } else {
                // Subsequent iterations - access from previous member
                validate(mem && mem->au, "cannot resolve from null member");
                
                // Load previous member to traverse into it
                enode loaded = e_load(a, mem, null);
                mem = access(loaded, alpha);
            }

            /// Handle macros and function calls
            if (instanceof(mem, typeid(macro)) || instanceof(mem, typeid(etype))) {
                mem = parse_member_expr(a, mem);
            }
        }
        avoid_model_lookup = false;

        /// Check if there's more chaining
        if (!read_if(a, ".")) {
            // End of chain - final load if needed
            if (!a->left_hand) {
                mem = e_load(a, mem, null);
            }
            break;
        }
        
        // More chaining - push context for next iteration
        validate(!is_func(mem), "cannot resolve into function");
        if (mem->au && !module) {
            push_scope(a, mem);
            depth++;
        }
    }


    /// restore namespace after resolving emember
    for (int i = 0; i < depth; i++)
        pop_scope(a);

    pop_tokens(a, true);

    if (a->expr_level == 0) {
        OPType assign_enum  = OPType__undefined;
        bool   assign_const = false;
        string assign_type  = read_assign  (a, &assign_enum, &assign_const);
        if (f) {
            validate(mem, "expected emember");
            validate(assign_enum == OPType__assign, "expected : operator in eargs");
            validate(!mem->au, "duplicate member exists in eargs");
            etype t = read_etype(a, &expr);
            if (mem->au->src != t) {
                if (mem->au->src) drop(mem->au->src);
                mem->au->src = t ? hold(t->au) : null;
            }
            validate(expr == null, "unexpected assignment in args");
            validate(mem->au, "cannot read model for arg: %o", mem->au);
        }
        else if ((rec_top || module) && assign_type) {
            token t = peek(a);

            // this is where member types are read
            mem->au = read_etype(a, &expr); // given VkInstance intern instance2 (should only read 1 token)
            validate(mem->au, "expected type after ':' in %o, found %o",
                (rec_top ? (etype)rec_top : (etype)module)->au, peek(a));
            mem->initializer = hold(expr);

            // lets read meta types here, at the record member level

        } else if (mem && assign_type) {
            a->left_hand = false;
            a->expr_level++;
            validate(mem, "member expected before assignment operator");
            expr = parse_assignment(a, mem, assign_type);
            a->expr_level--;
        } else if (mem && !assign_type && !mem->au) {
            validate (false, "expected assignment after '%o'", alpha);
        }
    }

    // we register when creating
    //if (mem && isa(mem) == typeid(emember) && !mem->membership && mem->mdl && mem->name && len(mem->name)) {
        //push(a, a->userspace);
    //    register_member(a, mem, false); // was true
        //pop(a);
    //}

    // this only happens when in a function
    return f ? e_create(a, mdl_expect, mem) : null;
}

enode parse_statement(silver a) {
    enode f = context_func(a);
    verify(!f || !f->au->is_mod_init, "unexpected init function");

    print_tokens(a, "parse-statement");
    a->last_return = null;
    a->expr_level = 0;

    if (f) {
        if (next_is(a, "no-op"))
            return e_noop(a, null);
        if (next_is(a, "return")) {
            a->expr_level++;
            a->last_return = parse_return(a);
            return a->last_return;
        }
        if (next_is(a, "break"))
            return parse_break(a);
        if (next_is(a, "for"))
            return parse_for(a);
        if (next_is(a, "loop"))
            return parse_loop_while(a);
        if (next_is(a, "if"))
            return parse_if_else(a);
        if (next_is(a, "ifdef"))
            return parse_ifdef_else(a);
        if (next_is(a, "loop"))
            return parse_loop_while(a);
    } else {
        if (next_is(a, "ifdef"))
            return parse_ifdef_else(a);
    }
    a->left_hand = true;

    // lets read ahead to get the type here, so read-node has less scope to cover
    enode e = read_enode(a, null); /// at module level, supports keywords
    a->left_hand = false;
    return e;
}

enode parse_statements(silver a, bool unique_members) {
    if (unique_members)
        push_scope(a, new(statements, mod, a));

    enode vr = null;
    while (silver_peek(a)) {
        print_tokens(a, "parse_statements");
        vr = parse_statement(a);
    }
    if (unique_members)
        pop_scope(a);
    return vr;
}

void silver_incremental_resolve(silver a) {
    members(a->au, mem) {
        if (is_func(mem) && mem->user != a->fn_init && !mem->user->user_built) {
            build_fn(a, mem, null, null);
        }
    }
    members(a->au, mem) {
        etype rec = (etype)is_rec(mem);
        if (mem->is_system)
            continue;
        if (rec && !rec->parsing && !rec->user_built)
            build_record(a, rec);
    }
}

enode parse_fn(silver a, string ident, u8 member_type, u32 traits, OPType assign_enum) {
    etype rtype = null;
    string name = instanceof(ident, typeid(token));
    array body = null;
    if (!name)
        name = instanceof(ident, typeid(string));
    bool is_cast = member_type == AU_MEMBER_CAST;
    //etype    rec_ctx = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);

    if (is_cast) {
        rtype = read_etype(a, &body);
    } else if (!name) {
        name = silver_read_alpha(a);
        validate(isa(name) == typeid(string), "expected alpha-identifier");
    }

    validate((is_cast || next_is(a, "[")) || next_is(a, "->"), "expected function args [");

    Au_t au = Au_register(top_scope(a), ident, AU_MEMBER_FUNC, traits);

    if (!name) {
        validate(member_type == AU_MEMBER_CAST, "with no name, expected cast");
        name = form(string, "cast_%o", rtype);
    }

    etype rec_ctx = context_class(a);
    if (!rec_ctx)
        rec_ctx = context_struct(a);
    string fname = rec_ctx ? f(string, "%o_%o", rec_ctx, name) : (string)name;
    au->alt = rec_ctx ? strdup(fname->chars) : null;

    if (!next_is(a, "]")) {
        push_scope(a, au);
        // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        int statements = 0;
        for (;;) {
            // we expect an arg-like statement when we are in this context (eargs)
            if (next_is(a, "...")) {
                // finalize this design (it should be a bit more c than .net)
                silver_consume(a);
                au->is_vargs = true;
                continue;
            }
            int count0 = au->args.count;
            print_tokens(a, "parse-args");
            parse_statement(a);
            int count1 = au->args.count;
            verify(count0 == count1 - 1, "arg could not parse");
            statements++;
            if (next_is(a, "]"))
                break;
        }
        silver_consume(a);
        pop_scope(a);
        // we do not 'show' the instance arg; i do wonder if we add it first?
        validate(statements == au->args.count, "argument parser mismatch");
    } else {
        silver_consume(a);
    }

    bool has_ret = silver_read_if(a, "->") != null;
    rtype = has_ret ? read_etype(a, &body) : elookup("none");
    verify(rtype, "expected return type after ->, found %o", silver_peek(a));

    // check if using ai model
    codegen cgen = null;
    if (silver_read_if(a, "using")) {
        verify(!body, "body already defined in type[expr]");
        token codegen_name = silver_read_alpha(a);
        verify(codegen_name, "expected codegen-identifier after 'using'");
        cgen = get(a->codegens, codegen_name);
        verify(cgen, "codegen identifier not found: %o", codegen_name);
    }

    validate(rtype, "rtype not set, void is something we may lookup");

    return enode(
        mod, a, au, au, body, (body && len(body)) ? body : read_body(a),
        cgen, cgen);
}

etype pointer(aether, Au);

static etype model_adj(silver a, etype mdl) {
    while (a->cmode && silver_read_if(a, "*"))
        mdl = pointer(a, mdl);
    return mdl;
}

static etype read_named_model(silver a) {
    etype mdl = null;
    push_current(a);

    bool any = silver_read_if(a, "any") != null;
    if (any)
        return elookup("Au");

    string alpha = silver_read_alpha(a);
    if (a && !next_is(a, ".")) {
        mdl = elookup(alpha->chars);
    }
    pop_tokens(a, mdl != null); /// save if we are returning a model
    return mdl;
}

array read_meta(silver a) {
    array res = null;
    if (silver_read_if(a, "<")) {
        bool first = true;
        while (!next_is(a, ">")) {
            if (!res)
                res = array(alloc, 32, assorted, true);
            if (!first)
                verify(silver_read_if(a, ","), "expected ',' seperator between models");
            Au n = silver_read_numeric(a);
            bool f = true;
            shape s = null;
            if (n) {
                s = shape();
                while (n) {
                    verify(isa(n) == typeid(i64),
                           "expected numeric type i64, found %s", isa(n)->ident);
                    i64 value = *(i64 *)n;
                    shape_push(s, value);
                    if (!silver_read_if(a, "x"))
                        break;
                    n = silver_read_numeric(a);
                }
                push(res, s);
            } else {
                etype mdl = read_named_model(a);
                verify(mdl, "expected model name, found %o", silver_peek(a));
                push(res, mdl);
            }
            first = false;
        }
        silver_consume(a);
    }
    return res;
}

etype read_etype(silver a, array *p_expr) {
    Au_t mdl = null;
    bool body_set = false;
    bool type_only = false;
    etype type = null;
    array expr = null;
    array meta = null;

    a->read_etype_abort = false;

    push_current(a);

    if (!a->cmode) {
        Au lit = silver_read_literal(a, null);
        if (lit) {
            a->cursor--;
            // we support var:1  or var:2.2 or var:'this is a test with {var2}'
            if (isa(lit) == typeid(string)) {
                expr = a(token("string"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("string");
            } else if (isa(lit) == typeid(i64)) {
                expr = a(token("i64"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("i64");
            } else if (isa(lit) == typeid(u64)) {
                expr = a(token("u64"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("u64");
            } else if (isa(lit) == typeid(i32)) {
                expr = a(token("i32"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("i32");
            } else if (isa(lit) == typeid(u32)) {
                expr = a(token("u32"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("u32");
            } else if (isa(lit) == typeid(f32)) {
                expr = a(token("f32"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("f32");
            } else if (isa(lit) == typeid(f64)) {
                expr = a(token("f64"), token("["), silver_element(a, 0), token("]"));
                mdl = au_lookup("f64");
            } else {
                verify(false, "implement literal %s in read_model", isa(lit)->ident);
            }
            a->cursor++;
        }
    }

    bool explicit_sign = !mdl && silver_read_if(a, "signed") != null;
    bool explicit_un = !mdl && !explicit_sign && silver_read_if(a, "unsigned") != null;
    etype prim_mdl = null;

    if (!mdl && !explicit_un) {
        if (silver_read_if(a, "char"))
            prim_mdl = elookup("i8");
        else if (silver_read_if(a, "short"))
            prim_mdl = elookup("i16");
        else if (silver_read_if(a, "int"))
            prim_mdl = elookup("i32");
        else if (silver_read_if(a, "long"))
            prim_mdl = silver_read_if(a, "long") ? elookup("i64") : elookup("i32");
        else if (explicit_sign)
            prim_mdl = elookup("i32");
        if (prim_mdl)
            prim_mdl = model_adj(a, prim_mdl);

        mdl = prim_mdl ? prim_mdl : read_named_model(a);

        if (!a->cmode && meta && next_is(a, "<")) {
            meta = read_meta(a);
        }

        if (!a->cmode && next_is(a, "[")) {
            body_set = true;
            expr = read_within(a);
        }

    } else if (!mdl && explicit_un) {
        if (silver_read_if(a, "char"))
            prim_mdl = elookup("u8");
        if (silver_read_if(a, "short"))
            prim_mdl = elookup("u16");
        if (silver_read_if(a, "int"))
            prim_mdl = elookup("u32");
        if (silver_read_if(a, "long"))
            prim_mdl = silver_read_if(a, "long") ? elookup("u64") : elookup("u32");

        prim_mdl = model_adj(a, prim_mdl ? prim_mdl : elookup("u32"));
    }

    // abort if there is assignment following isolated type-like token
    if (!expr && a->expr_level == 0 && silver_read_assign(a, null, null)) {
        a->read_etype_abort = true;
        mdl = null;
    }

    if (p_expr)
        *p_expr = expr;

    etype t = etype(mod, a, au, mdl, meta, meta);

    pop_tokens(a, mdl != null); // if we read a model, we transfer token state
    return t;
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, Au_t f, array query) {
    fault("must subclass codegen for usable code generation");
    return null;
}

// design-time for dictation
array read_dictation(silver a, array input) {
    // we want to read through [ 'tokens', image[ 'file.png' ] ]
    // also 'token here' 'and here' as two messages
    array result = array();

    push_tokens(a, input, 0);
    while (silver_read_if(a, "[")) {
        array content = array();
        while (silver_peek(a) && !next_is(a, "]")) {
            if (silver_read_if(a, "file")) {
                verify(silver_read_if(a, "["), "expected [ after file");
                string file = silver_read_literal(a, typeid(string));
                verify(file, "expected 'path' of file in resources");
                path share = path_share_path();
                path fpath = f(path, "%o/%o", share, file);
                verify(exists(fpath), "path does not exist: %o", fpath);
                verify(silver_read_if(a, "]"), "expected ] after file [ literal string path... ] ");
                push(content, fpath); // we need to bring in the image/media api
            } else {
                string msg = silver_read_literal(a, typeid(string));
                verify(msg, "expected 'text' message");
                push(content, msg);
            }
            silver_read_if(a, ","); // optional for arrays of 1 dimension
        }
        verify(len(content), "expected more than one message entry");
        verify(silver_read_if(a, "]"), "expected ] after message");

        push(result, content);
    }
    verify(len(result), "expected dictation message");
    pop_tokens(a, false);
    return result;
}

array chatgpt_generate_fn(chatgpt gpt, Au_t f, array query) {
    silver a = f->user->mod;
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
        Au_t mem = f->args.origin[i];
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
        "role", string("system"),
        "content", a->source_raw);

    // now we need a silver document with reasonable how-to
    // this can be fetched from resource, as its meant for both human and AI learning
    path docs = path_share_path();
    path test_sf = f(path, "%o/docs/test.ag", docs);
    string test_content = load(test_sf, typeid(string), null);
    map sys_howto = m(
        "role", string("system"),
        "content", test_content);

    array messages = a(sys_intro, sys_module, sys_howto);

    // now we have 1 line of dictation: ['this is text describing an image', image[ 'file.png' ] ]
    // for each dictation message, there is a response from the server which we also include as assistant
    // it must error if there are missing responses from the servea
    array dictation = read_dictation(a, f->user->body);

    each(dictation, array, msg) {
        array content = array();
        each(msg, Au, info) {
            map item;
            if (instanceof(typeid(path), info)) {
                string mime_type = mime((path)info);
                string b64 = base64((path)info);
                map m_url = m("url", f(string, "data:%o;base64,%o", mime_type, b64)); // data:image/png;base64,
                item = m("type", "image_url", "image_url", m_url);
            } else if (instanceof(typeid(string), info)) {
                item = m("type", "text", "text", info);
            } else {
                fault("unknown type in dictation: %s", isa(info)->ident);
            }
            push(content, item);
        }
        map user_dictation = m(
            "role", string("user"),
            "content", content);

        push(messages, user_dictation);
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
                concat(cmd, t);
            } else
                token_line = t->line;
        } else if (cmd) {
            token_line = -1;
            push(res, cmd);
            cmd = null;
        }
    }
    if (cmd) {
        push(res, cmd);
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
            concat(config, t);
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
            concat(env, t);
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
            concat(libs, t);
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

static none checkout(import im, path uri, string commit, array prebuild, array postbuild, string conf, string env) {
    silver a = im->mod;
    path install = a->install;
    string s = cast(string, uri);
    num sl = rindex_of(s, "/");
    validate(sl >= 0, "invalid uri");
    string name = mid(s, sl + 1, len(s) - sl - 1);
    path project_f = f(path, "%o/checkout/%o", install, name);
    bool debug = false;
    string config = interpolate(conf, a);

    validate(command_exists("git"), "git required for import feature");

    // we need to check if its full hash
    validate(len(commit) == 40 || is_branchy(commit),
             "commit-id must be a full SHA-1 hash or a branch name (short-hand does not work for depth=1 checkouts)");

    // checkout or symlink to src
    if (!dir_exists("%o", project_f)) {
        path src_path = f(path, "%o/%o", a->src_loc, name);
        if (dir_exists("%o", src_path)) {
            project_f = src_path;
            vexec("ln -s %o/%o %o", a->src_loc, name, name);
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

    // we build to another folder, not inside the source, or checkout
    path build_f = f(path, "%o/%s/%o", install, debug ? "debug" : "build", name);
    path rust_f = f(path, "%o/Cargo.toml", project_f);
    path meson_f = f(path, "%o/meson.build", project_f);
    path cmake_f = f(path, "%o/CMakeLists.txt", project_f);
    path silver_f = f(path, "%o/build.ag", project_f);
    path gn_f = f(path, "%o/BUILD.gn", project_f);
    bool is_rust = file_exists("%o", rust_f);
    bool is_meson = file_exists("%o", meson_f);
    bool is_cmake = file_exists("%o", cmake_f);
    bool is_gn = file_exists("%o", gn_f);
    bool is_silver = file_exists("%o", silver_f);
    path token = f(path, "%o/silver-token", build_f);

    if (file_exists("%o", token)) {
        string s = load(token, typeid(string), null);
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
            string icmd = interpolate(cmd, a);
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
        silver sf = silver(source, silver_f);
        validate(sf, "silver module compilation failed: %o", silver_f);
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
            string icmd = interpolate(cmd, a);
            command_exec((command)icmd);
        }
        cd(cw);
    }

    save(token, config, null);
}

// build with optional bc path; if no bc path we use the project file system
none silver_build(silver a) {
    path ll = null, bc = null;
    emit(a, &ll, &bc);
    verify(bc != null, "compilation failed");

    int error_code = 0;
    path install = a->install;

    // simplified process for .bc case
    string name = stem(bc);
    path cwd = path_cwd();
    verify(exec("%o/bin/llc -filetype=obj %o.ll -o %o.o -relocation-model=pic",
                install, name, name) == 0,
           ".ll -> .o compilation failed");
    string libs, cflags;

    // create libs, and describe in reverse order from import
    libs = string("");
    array rlibs = reverse(a->libs);
    each(rlibs, string, lib_name) {
        if (len(libs))
            append(libs, " ");
        concat(libs, f(string, "-l%o", lib_name));
    }

    // set cflags
#ifndef NDEBUG
    cflags = string("-fsanitize=address"); // import keyword should publish to these
#else
    cflags = string("");
#endif

    // link
    verify(exec("%o/bin/clang %s%o.o -o %o -L%o/lib -Wl,--allow-multiple-definition %o %o",
                install, a->is_library ? "-shared " : "", name, name, install, libs, cflags) == 0,
           "link failed");
}

bool silver_next_is_neighbor(silver a) {
    token b = silver_element(a, -1);
    token c = silver_element(a, 0);
    return b->column + b->count == c->column;
}

string expect_alpha(silver a) {
    token t = silver_next(a);
    verify(t && isalpha(*t->chars), "expected alpha identifier");
    return string(t->chars);
}

path is_module_dir(silver a, string ident) {
    path dir = f(path, "%o/%o", a->project_path, ident);
    if (dir_exists("%o", dir))
        return dir;
    return null;
}

// when we load silver files, we should look for and bind corresponding .c files that have implementation
// this is useful for implementing in C or other languages
path module_exists(silver a, array idents) {
    string to_path = join(idents, "/");
    path sf = f(path, "%o/%o.ag", a->project_path, to_path);
    return file_exists("%o", sf) ? sf : null;
}

enode silver_parse_ternary(silver a, enode expr, etype mdl_expect) {
    if (!silver_read_if(a, "?"))
        return expr;
    enode expr_true = parse_expression(a, mdl_expect);
    enode expr_false = parse_expression(a, mdl_expect);
    return e_ternary(a, expr, expr_true, expr_false);
}

// these are for public, intern, etc; Au-Type enums, not someting the user defines in silver context
i32 read_enum(silver a, i32 def, Au_t etype) {
    for (int m = 1; m < etype->members.count; m++) {
        Au_t enum_v = etype->members.origin[m];
        if (silver_read_if(a, enum_v->ident))
            return *(i32 *)enum_v->ptr; // should support typed enums; the ptr is a mere Au-object
    }
    return def;
}

static bool peek_fields(silver a);

static bool class_inherits(etype cl, etype of_cl);

map parse_map(silver a, etype mdl_schema);

static enode typed_expr(silver a, enode mdl, array expr) {
    push_tokens(a, expr ? (tokens)expr : a->tokens, expr ? 0 : a->cursor);
    
    // function calls
    if (is_func(mdl)) {
        enode f = mdl;
        array   m      = &f->au->args;
        int     ln     = m->count, i = 0;
        array   values = array(alloc, 32);
        enode   target = null;
        i32     offset = 0;

        if (f->target_arg) {
            verify(mdl->target, "expected target for method call");
            push(values, mdl->target);
            offset = 1;
        }

        while (i + offset < ln || f->au->is_vargs) {
            etype   arg  = array_get(m, i + offset);
            enode   expr = parse_expression(a, arg); // self contained for '{interp}' to cstr!
            verify(expr, "invalid expression");
            if (arg && canonical(expr) != canonical(arg))
                expr = e_create(a, arg, expr);
            
            push(values, expr);
            
            if (read_if(a, ","))
                continue;
            
            validate(len(values) >= ln, "expected %i args for function %o", ln, f);
            break;
        }
        return e_fn_call(a, f, values);
    }
    
    // this is only suitable if reading a literal constitutes the token stack
    // for example:  i32 100
    Au  n = read_literal(a, null);
    if (n && a->cursor == len(a->tokens)) {
        pop_tokens(a, expr ? false : true);
        return e_operand(a, n, mdl);
    } else if (n) {
        // reset if we read something
        pop_tokens(a, expr ? false : true);
        push_tokens(a, expr ? (tokens)expr : a->tokens, expr ? 0 : a->cursor);
    }
    bool    has_content = !!expr && len(expr); //read_if(mod, "[") && !read(mod, "]");
    enode   r           = null;
    bool    conv        = false;
    bool    has_init    = peek_fields(a);

    a->expr_level++;
    if (!has_content) {
        r = e_create(a, mdl, null); // default
        conv = false;
    } else if (class_inherits(mdl, elookup("array"))) {
        array nodes = array(64);
        etype element_type = mdl->au->src;
        shape sh = mdl->au->shape;
        validate(sh, "expected shape on array");
        int shape_len = shape_total(sh);
        int top_stride = sh->count ? sh->data[sh->count - 1] : 0;
        validate((!top_stride && shape_len == 0) || (top_stride && shape_len),
            "unknown stride information");  
        int num_index = 0; /// shape_len is 0 on [ int 2x2 : 1 0, 2 2 ]

        while (peek(a)) {
            token n = peek(a);
            enode  e = parse_expression(a, element_type);
            e = e_create(a, element_type, e);
            push(nodes, e);
            num_index++;
            if (top_stride && (num_index % top_stride == 0)) {
                validate(read_if(a, ",") || !peek(a),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    top_stride);
            }
        }
        r = e_create(a, mdl, nodes);
    } else if (has_init || class_inherits(mdl, elookup("map"))) {
        validate(has_init, "invalid initialization of map model");
        conv = true;
        r    = parse_map(a, mdl);
    } else {
        /// this is a conversion operation
        r = parse_expression(a, mdl);
        conv = canonical(r) != canonical(mdl);
        //validate(read_if(a, "]"), "expected ] after mdl-expr %o", src->name);
    }
    a->expr_level--;
    if (conv)
        r = e_create(a, mdl, r);
    pop_tokens(a, expr ? false : true);
    return r;
}

silver silver_with_path(silver a, path module_path) {
    a->source = hold(module_path);
    return a;
}

enode import_parse(silver a) {
    validate(next_is(a, "import"), "expected import keyword");
    silver_consume(a);

    int from = a->cursor;
    codegen cg = null;
    string namespace = null;
    array includes = array(32);
    array module_paths = array(32);
    array module_names = array(32);
    path local_mod = null;

    token t = silver_peek(a);
    Au_t is_codegen = null;
    token commit = null;
    string uri = null;

    if (t && isalpha(t->chars[0])) {
        bool cont = false;
        string service = a->git_service;
        string user = a->git_owner;
        string project = null;
        string aa = expect_alpha(a); // value of t
        string bb = silver_read_if(a, ":") ? expect_alpha(a) : null;
        string cc = bb && silver_read_if(a, ":") ? expect_alpha(a) : null;
        array mpath = null;
        Au_t f = Au_find_type((cstr)aa->chars, null);

        if (f && inherits(f, typeid(codegen)))
            is_codegen = f;
        else if (next_is(a, ".")) {
            while (silver_read_if(a, ".")) {
                if (!mpath) {
                    mpath = array(alloc, 32);
                    push(mpath, cc ? cc : bb ? bb
                                      : aa   ? aa
                                             : (string)null);
                }
                string ident = silver_read_alpha(a);
                push(mpath, ident);
            }
        } else {
            mpath = array(alloc, 32);
            string f = cc ? cc : bb ? bb
                             : aa   ? aa
                                    : (string)null;
            if (index_of(f, ".") >= 0) {
                array sp = split(f, ".");
                array sh = shift(sp);
                push(mpath, sh);
            }
        }

        if (!is_codegen) {
            // read commit if given
            if (silver_read_if(a, "/"))
                commit = next(a);

            if (aa && !bb && !commit) {
                local_mod = module_exists(a, mpath);
                if (!local_mod) {
                    // push entire directory
                    string j = join(mpath, "/");
                    verify(dir_exists("%o", j), "module/directory not found: %o", j);
                    path f = f(path, "%o", j);
                    array dir = ls(f, string("*.ag"), false);
                    verify(len(dir), "no modules in directory %o", f);
                    each(dir, path, m) {
                        push(module_paths, m);
                    }
                } else
                    push(module_paths, local_mod);
            } else if (aa && !bb) {
                verify(!local_mod, "unexpected import chain containing different methodologies");

                // contains commit, so logically cannot be a singular module
                // for commit # it must be a project for now, this is not a
                // constraint that we directly need to mitigate, but it may
                // be a version difference
                project = aa;
                verify(!mpath || len(mpath) == 0, "unexpected path to module (expected 1st arg as project)");
            } else if (aa && !cc) {
                verify(!local_mod, "unexpected import chain containing different methodologies");
                user = aa;
                project = bb;
                //verify(!mpath || len(mpath) == 0, "unexpected path to module (expected 2nd arg as project)");
            } else {
                verify(!local_mod, "unexpected import chain containing different methodologies");
                user = aa;
                project = bb;
            }

            string path_str = string();
            if (len(mpath)) {
                string str_mpath = join(mpath, "/") ? cc : string("");
                path_str = len(str_mpath) ? f(string, "blob/%o/%o", commit, str_mpath) : string("");
            }

            verify(project || local_mod, "could not decipher module references from import statement");

            if (local_mod) {
                uri = null;
                cont = silver_read_if(a, ",") != null;
                verify(!cont, "comma not yet supported in import (func needs restructuring to create multiple imoprts in enode)");
            } else
                uri = f(string, "https://%o/%o/%o%s%o", service, user, project,
                        cast(bool, path_str) ? "/" : "", path_str);
        }
    }

    // includes for this import
    if (silver_read_if(a, "<")) {
        for (;;) {
            string f = read_alpha_any(a);
            validate(f, "expected include");

            // we may read: something/is-a.cool\file.hh.h
            while (next_is_neighbor(a) && (!next_is(a, ",") && !next_is(a, ">")))
                concat(f, next(a));

            push(includes, f);

            if (!silver_read_if(a, ",")) {
                token n = silver_read_if(a, ">");
                validate(n, "expected '>' after include, list, of, headers");
                break;
            }
        }
    }

    array c = read_body(a);
    array all_config = compact_tokens(c);
    map props = map();

    // this invokes import by git; a local repo may be possible but not very usable
    // arguments / config not stored / used after this
    if (next_is(a, "[") || next_indent(a)) {
        array b = read_body(a);
        int index = 0;
        while (index < len(b)) {
            verify(index - len(b) >= 3, "expected prop: value for codegen object");
            token prop_name = b->origin[index++];
            token col = b->origin[index++];
            token prop_value = b->origin[index++];
            // this will not work for reading {fields}
            // trouble is the merging of software with build config, and props we set in module.

            verify(eq(col, ":"), "expected prop: value for codegen object");
            set(props, string(prop_name->chars), string(prop_value->chars));
        }
    }

    silver external = null;

    if (uri) {
        checkout(a, uri, commit,
                 import_build_commands(all_config, ">"),
                 import_build_commands(all_config, ">>"),
                 import_config(all_config),
                 import_env(all_config));
        each(all_config, string, t) if (starts_with(t, "-l"))
            push(a->libs, mid(t, 2, len(t) - 2));
    } else if (local_mod)
        external = silver(local_mod);
    else if (is_codegen) {
        cg = construct_with(is_codegen, props, null);
    }

    if (next_is(a, "as")) {
        silver_consume(a);
        namespace = hold(silver_read_alpha(a));
        validate(namespace, "expected alpha-numeric %s",
                 is_codegen ? "alias" : "namespace");
    } else if (is_codegen) {
        namespace = hold(string(is_codegen->ident));
    }

    // hash. for cache.  keep cache warm
    int to = a->cursor;
    array tokens = array(alloc, to - from + 1);
    for (int i = from; i < to; i++) {
        token t = a->tokens->origin[i];
        push(tokens, t);
    }

    import mdl = get(a->import_cache, tokens);
    bool has_cache = mdl != null;

    if (!has_cache) {
        mdl = import(
            mod, a,
            codegen, cg,
            external, external,
            tokens, tokens);

        set(a->import_cache, tokens, mdl);
    }

    // member registration for 'import'
    // needs to be enode
    enode mem = enode(
        mod,  a,
        name, namespace,
        au,   mdl->au);
    
    if (!has_cache && !is_codegen) {
        push_scope(a, mdl->au);
        mdl->include_paths = array();

        // include each, collecting the clang instance for which we will invoke macros through
        each(includes, string, inc) {
            aclang_cc instance;
            path i = include(a, inc, namespace, &instance);
            push(mdl->include_paths, i);
            set(a->instances, i, instance);
        }

        each(module_paths, path, m) {
            import_Au(a, m);
        }
    }

    mdl->au->is_closed = true;
    mdl->module_paths = hold(module_paths);

    if (is_codegen) {
        string name = namespace ? (string)namespace : string(is_codegen->ident);
        set(a->codegens, name, mdl->codegen);
    }

    return mem;
}

// works for class inits, and module inits
void silver_build_initializer(silver a, enode t) {
    if (t && t->au && t->au->member_type == AU_MEMBER_PROP && t->body) {
        // only override for module members, not class members
        bool   is_module_mem = t->au->context == a->au;
        tokens override      = is_module_mem ? get(a->props, string(t->au->ident)) : null;
        tokens expr          = override ? override : t->body;
        if (!instanceof(t->body, typeid(enode))) {
            array post_const = parse_const(a, t->body);
            int level = a->expr_level;
            a->expr_level++;
            expr = typed_expr(a, t, post_const); // we have tokens for the name pushed to the stack
            a->expr_level = level;
        }
        enode ctx = context_func(a);
        enode L = (!is_module_mem && ctx) ? 
            access(ctx->target_arg, string(t->au->ident)) : (enode)t;
        e_assign(a, L, expr, OPType__assign);
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
    string u = copy(s);
    do {
        int i = index_of(u, "-");
        if (i == -1)
            break;
        ((cstr)u->chars)[i] = '_';
    } while (1);
    return u;
}

static string method_def(enode emem) {
    return f(string,
             "#define %o(I,...) ({{ __typeof__(I) _i_ = I; ftableI(_i_)->ft.%o(_i_, ## __VA_ARGS__); }})",
             cname(emem->au->ident));
}

static string type_name(Au a) {
    Au_t au = au_arg(a);
    if (au && au->member_type == AU_MEMBER_PROP) {
        au = au->src;
    }
    return au ? string(au->alt ? au->alt : au->ident) : null;
}

void silver_write_header(silver a) {
    string m = stem(a->source);
    path inc_path = f(path, "%o/include", a->install);
    path module_dir = f(path, "%o/%o", inc_path, m);
    path module_path = f(path, "%o/%o/%o", inc_path, m, m);
    path intern_path = f(path, "%o/%o/intern", inc_path, m);
    path public_path = f(path, "%o/%o/public", inc_path, m);
    path method_path = f(path, "%o/%o/methods", inc_path, m); // lets store in the install path
    string NAME = uccase(string(a->au->ident));

    verify(make_dir(module_dir), "could not make dir %o", module_dir);

    // we still need to parse aliases where we subclass
    // LA

    FILE *module_f = fopen(cstring(module_path), "wb");
    FILE *intern_f = fopen(cstring(intern_path), "wb");
    FILE *public_f = fopen(cstring(public_path), "wb");
    FILE *method_f = fopen(cstring(method_path), "wb");

    // write intern header
    fputs(fmt("#ifndef _%o_INTERN_\n", NAME)->chars, intern_f);
    fputs(fmt("#define _%o_INTERN_\n", NAME)->chars, intern_f);
    members(a->au, m) {
        if (is_class(m)) {
            string n = cname(m->ident);
            fputs(fmt("#undef %o_intern\n", n)->chars, intern_f);
            fputs(fmt("#define %o_intern(AA,YY,...) AA##_schema(AA,YY, __VA_ARGS__)\n", n)->chars, intern_f);
        }
    }
    fputs(fmt("#endif\n")->chars, intern_f);

    // write public header
    fputs(fmt("#ifndef _%o_PUBLIC_\n", NAME)->chars, public_f);
    fputs(fmt("#define _%o_PUBLIC_\n", NAME)->chars, public_f);
    members(a->au, m) {
        etype mdl = m->user;
        if (is_class(m)) {
            string n = type_name(m);
            fputs(fmt("#ifndef %o_intern\n", n)->chars, public_f);
            fputs(fmt("#define %o_intern(AA,YY,...) AA##_schema(AA,YY##_EXTERN, __VA_ARGS__)\n", n)->chars, public_f);
            fputs(fmt("#endif\n")->chars, public_f);
        }
    }
    fputs(fmt("#endif\n")->chars, public_f);

    fputs(fmt("#ifndef _%o_\n", NAME)->chars, module_f);
    fputs(fmt("#define _%o_\n", NAME)->chars, module_f);

    // forward declare all classes
    members(a->au, m) {
        if (is_class(m))
            fputs(fmt("forward(%o)\n", cname(type_name(m)))->chars, module_f);
    }

    // write class schemas
    members(a->au, m) {
        if (is_class(m)) {
            string n = cname(type_name(m));
            array acl = etype_class_list(m->user);
            string f = fmt("#define %o_schema(A,B,...)\\\n", n);
            fputs(f->chars, module_f);
            members(m, mi) {
                string mn = cname(string(mi->ident));
                if (is_func(mi)) {
                    enode f = mi->user;
                    string args = string();
                    args(mi, arg) {
                        enode a = arg->user;
                        if (len(args))
                            append(args, ", ");
                        concat(args, cname(cast(string, a)));
                    }
                    string i = f->target_arg ? string("i") : string("s");
                    string rtype = cname(cast(string, f->au->rtype->user));
                    fputs(fmt("M(A,B, %o,method,%o)\\\n", i, rtype, mn, args)->chars, module_f);
                } else {
                    string i = !mi->is_static ? string("i") : string("s");
                    bool has_meta = mi->meta.count > 0;
                    string meta = string();
                    args(mi, m) {
                        string n = type_name(m);
                        if (meta->count)
                            append(meta, ", ");
                        concat(meta, n);
                    }
                    string prop_type = cname(type_name(mi));
                    fputs(fmt("M(A,B, %o,prop,%o,%o%s%o)\\\n",
                              i, prop_type, mn,
                              has_meta ? "," : "", meta)
                              ->chars,
                          module_f);
                }
                fputs(fmt("%o\n", method_def(mi->user))->chars, module_f);
            }
            int count = len(acl) - 1;
            string extra = string("");
            if (count > 1)
                extra = fmt("_%i", count);
            string classes = string();
            etype Au_cl = elookup("Au");
            each(acl, etype, c) {
                if (c == Au_cl) break;
                string s = cast(string, c);
                if (classes->count)
                    append(classes, ",");
                concat(classes, s);
            }
            fputs(fmt("\ndeclare_class%o(%o)\n", extra, classes), module_f);
        }
    }
    fputs(fmt("#endif\n")->chars, module_f);

    // write methods
    fputs(fmt("#ifndef _%o_METHODS_\n", NAME)->chars, public_f);
    fputs(fmt("#define _%o_METHODS_\n", NAME)->chars, public_f);
    members(a->au, m) {
        if (is_class(m)) {
            members(m, mi) {
                if (is_func(mi))
                    fputs(fmt("%o\n", method_def(mi->user))->chars, method_f);
            }
        }
    }
    fputs(fmt("#endif\n")->chars, method_f);

    fclose(module_f);
    fclose(intern_f);
    fclose(public_f);
}

void print_token_array(silver a, array tokens) {
    string res = string();
    for (int i = 0, ln = len(tokens); i < ln; i++) {
        token t = tokens->origin[i];
        append(res, t->chars);
        append(res, " ");
    }
    print("tokens = %o", res);
}

i32 read_enum(silver a, i32 def, Au_t etype);

static enode typed_expr(silver a, enode src, array expr);

void build_fn(silver a, enode f, callback preamble, callback postamble) {
    if (f->user_built || (!f->body && !preamble && !postamble))
        return;

    // finalize first: this prepares args and gives us a value for our function
    // otherwise we could not call recursively
    implement(f);
    f->user_built = true;

    if (f->target_arg)
        push_scope(a, f->target_arg);
    a->last_return = null;
    push_scope(a, f);

    // before the preamble we handle guard
    if (preamble)
        preamble(f, null);
    array after_const = parse_const(a, f->body);
    if (f->cgen) {
        // generate code with cgen delegate imported (todo)
        array gen = generate_fn(f->cgen, f, f->body);
    } else {
        push_tokens(a, after_const, 0);
        print_tokens(a, "build-fn-statements");
        parse_statements(a, true);
        pop_tokens(a, false);
    }
    if (postamble)
        postamble(f, null);
    pop_scope(a);
    if (f->target_arg)
        pop_scope(a);
}

static void build_record(silver a, etype mrec) {
    etype rec = resolve(mrec);
    if (rec->user_built) return;
    rec->user_built = true;
    rec->parsing = true;
    verify(rec->au->is_class || rec->au->is_struct, "not a record");
    print_token_array(a, rec->body);

    array body = rec->body ? (array)rec->body : array();
    push_tokens(a, body, 0);
    push_scope(a, mrec);
    while (silver_peek(a)) {
        print_tokens(a, "parse-statement");
        parse_statement(a); // must not 'build' code here, and should not (just initializer type[expr] for members)
    }
    pop_scope(a);
    pop_tokens(a, false);
    rec->parsing = false;

    if (rec->au->is_class) {
        push_scope(a, mrec);

        // if no init, create one (attach preamble for our property inits)
        Au_t m_init = Au_find_member(rec->au, "init", AU_MEMBER_FUNC);
        if (!m_init) {
            m_init = function(a,
                string("init"), array(), AU_MEMBER_FUNC,
                AU_TRAIT_IMETHOD | AU_TRAIT_OVERRIDE, 0)->au;
        }
        pop_scope(a);

        build_fn(a, m_init->user, build_init_preamble, null); // we may need to

        // build remaining functions
        members(rec->au, m) {
            enode n = instanceof(m->user, typeid(enode));
            if (is_func(n)) build_fn(a, n, null, null);
        }
    }
}

array silver_parse_const(silver a, array tokens) {
    array res = array(32);
    push_tokens(a, tokens, 0);
    while (silver_peek(a)) {
        token t = next(a);
        if (eq(t, "const")) {
            validate(false, "implement const");
        } else {
            push(res, t);
        }
    }
    pop_tokens(a, false);
    return res;
}

enode parse_return(silver a) {
    etype rtype = return_type(a);
    bool  is_v  = is_void(rtype);
    enode ctx   = context_func(a);
    silver_consume(a);
    enode expr  = is_v ? null : parse_expression(a, rtype);
    Au_log("return-type", "%o", is_v ? (Au)string("none") : (Au)expr);
    e_fn_return(a, expr);
    return e_noop(a, expr);
}

enode parse_break(silver a) {
    silver_consume(a);
    catcher cat = context_model(a, typeid(catcher));
    verify(cat, "expected cats");
    return e_break(a, cat);
}

enode silver_parse_do_while(silver a) {
    silver_consume(a);
    enode vr = null;
    return null;
}

// read-expression does not pass in 'expected' models, because 100% of the time we run conversion when they differ
// the idea is to know what model is returning from deeper calls
static array read_expression(silver a, etype *mdl_res, bool *is_const) {
    array exprs = array(32);
    int s = a->cursor;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    enode n = reverse_descent(a, null);
    if (mdl_res)
        *mdl_res = n;
    a->no_build = false;
    int e = a->cursor;
    for (int i = s; i < e; i++) {
        push(exprs, a->tokens->origin[i]);
    }
    *is_const = a->is_const_op;
    return exprs;
}

static enode parse_fn_call(silver, etype, enode);

map parse_map(silver a, etype mdl_schema) {
    map args = map(hsize, 16, assorted, true);

    // we need e_create to handle this as well; since its given a map of fields and a is_ref struct it knows to make an alloc
    if (is_ptr(mdl_schema) && is_struct(mdl_schema->au->src))
        mdl_schema = resolve(mdl_schema);

    while (silver_peek(a)) {
        string name = silver_read_alpha(a);
        validate(silver_read_if(a, ":"), "expected : after arg %o", name);
        etype mdl_expect = null;
        if (mdl_schema) {
            Au_t m = Au_find_member(mdl_schema->au, name->chars, AU_MEMBER_PROP);
            validate(m, "member %o not found on model %o", name, mdl_schema);
            mdl_expect = m->user;
        }
        enode value = parse_expression(a, mdl_expect);
        validate(!contains(args, name), "duplicate initialization of %o", name);
        set(args, name, value);
    }
    return args;
}

static bool class_inherits(etype cl, etype of_cl) {
    silver a = cl->mod;
    etype aa = canonical(of_cl);
    while (cl && cl != aa) {
        if (!cl->au->context || cl->au->context == cl->au)
            break;
        cl = cl->au->context->user;
    }
    return cl && cl == aa;
}

static bool peek_fields(silver a) {
    token t0 = silver_element(a, 0);
    token t1 = silver_element(a, 1);
    if (t0 && is_alpha(t0) && t1 && eq(t1, ":"))
        return true;
    return false;
}

array read_arg(array tokens, int start, int *next_read);

enode silver_parse_member_expr(silver a, enode mem) {
    push_current(a);
    int indexable = !is_func(mem);
    bool is_macro = mem && instanceof(mem, typeid(macro));

    /// handle compatible indexing methods and general pointer dereference @ index
    if (indexable && next_is(a, "[")) {
        etype r = is_rec(mem);
        /// must have an indexing method, or be a reference_pointer
        validate(is_ptr(mem) || r, "no indexing available for model %o",
                 mem->name);

        /// we must read the arguments given to the indexer
        silver_consume(a);
        array args = array(16);
        if (r && mem->target)
            push(args, mem->target);
        while (!next_is(a, "]")) {
            enode expr = parse_expression(a, null);
            push(args, expr);
            validate(next_is(a, "]") || next_is(a, ","), "expected ] or , in index arguments");
            if (next_is(a, ","))
                silver_consume(a);
        }
        silver_consume(a);
        enode index_expr = null;
        if (r) {
            enode indexer = compatible(a, r, null, AU_MEMBER_INDEX, args); /// we need to update emember model to make all function members exist in an array
            validate(indexer, "%o: no suitable indexing method", r);
            index_expr = e_fn_call(a, indexer, args); // needs a target
        } else {
            index_expr = e_offset(a, mem, args);
        }
        pop_tokens(a, true);
        return index_expr;
    } else if (is_macro) {
        bool is_functional = next_is(a, "(");
        array args = array(alloc, 32);
        if (is_functional) {
            silver_consume(a);
            while (!next_is(a, ")")) {
                int next;
                array arg = read_arg(a->tokens, a->cursor, &next);
                validate(arg, "macro expansion failed");
                if (arg)
                    push(args, arg);
                a->cursor = next;
            }
            validate(silver_read_if(a, ")"), "expected parenthesis to end macro func call");
        }
        token cur = silver_peek(a);

        // read arguments, and call macro
        macro m = mem;
        array tokens = macro_expand(m, is_functional ? args : null, null);
        push_tokens(a, tokens, 0); // set these tokens as the parser state, run parse, then return the statement
        enode res = parse_expression(a, null);
        pop_tokens(a, false);
        pop_tokens(a, true);
        return res;

    } else if (mem) {
        if (is_func(mem) || is_type(mem)) {
            array expr = read_within(a);
            mem = typed_expr(a, mem, expr); // this, is the construct
        }
    }
    pop_tokens(a, mem != null);
    return mem;
}

enode silver_parse_assignment(silver a, enode mem, string oper) {
    validate(isa(mem) == typeid(enode) || !mem->au->is_const,
        "mem %o is a constant", mem->name);
    
    enode L = mem;
    enode R = parse_expression(a, mem); /// getting class2 as a struct not a pointer as it should be. we cant lose that pointer info
    if (!mem->au) {
        mem->au = hold(R->au);
        mem->au->is_const = eq(oper, "=");
        if (mem->literal)
            drop(mem->literal);
        mem->literal = hold(R->literal);
    } else {
        verify(!eq(oper, "="), "cannot perform constant assign after initial assignment");
    }
    validate(contains(operators, oper), "%o not an assignment-operator");
    string op_name = get   (operators, oper);
    string op_form = form  (string, "_%o", op_name);
    OPType op_val  = e_val (OPType, cstring(op_form));
    enode  result  = e_op  (a, op_val, op_name, L, R);
    return result;
}

enode cond_builder(silver a, array cond_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, cond_tokens, 0);
    enode cond_expr = parse_expression(a, elookup("bool"));
    pop_tokens(a, false);
    a->expr_level--;
    return cond_expr;
}

// singular statement (not used)
enode statement_builder(silver a, array expr_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    push_tokens(a, expr_tokens, 0);
    enode expr = parse_statement(a);
    pop_tokens(a, false);
    a->expr_level = level;
    return expr;
}

enode block_builder(silver a, array block_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    push_tokens(a, block_tokens, 0);
    last = parse_statements(a, false);
    pop_tokens(a, false);
    a->expr_level = level;
    return last;
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_builder(silver a, array expr_groups, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    each(expr_groups, array, expr_tokens) {
        push_tokens(a, expr_tokens, 0);
        last = parse_statement(a);
        pop_tokens(a, false);
    }
    a->expr_level = level;
    return last;
}

enode exprs_builder(silver a, array expr_groups, Au unused) {
    a->expr_level++; // make sure we are not at 0
    enode last = null;
    each(expr_groups, array, expr_tokens) {
        push_tokens(a, expr_tokens, 0);
        last = parse_expression(a, null);
        pop_tokens(a, false);
    }
    a->expr_level--;
    return last;
}

enode expr_builder(silver a, array expr_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, expr_tokens, 0);
    enode exprs = parse_expression(a, null);
    pop_tokens(a, false);
    a->expr_level--;
    return exprs;
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
        bool is_if = silver_read_if(a, "ifdef") != null;
        validate(is_if && require_if || !require_if, "expected if");
        a->expr_level++;
        enode n_cond = is_if ? parse_expression(a, null
        ) : null;
        a->expr_level--;
        array block = read_body(a);
        if (n_cond) {
            Au const_v = n_cond->literal;
            if (!one_truth && const_v && cast(bool, const_v)) { // we need to make sure that we do not follow anymore in this block!
                push_tokens(a, block, 0);                     // are we doing that?
                statements = parse_statements(a, false);
                pop_tokens(a, false);
                one_truth = true; /// we passed the condition, so we cannot enter in other blocks.
            }
        } else if (!one_truth) {
            validate(!is_if, "if statement incorrect");
            push_tokens(a, block, 0);
            statements = parse_statements(a, false); /// make sure parse-statements does not use its own members
            pop_tokens(a, false);
            expect_last = true;
        }
        if (!is_if)
            break;
        bool next_else = silver_read_if(a, "else") != null;
        if (!next_else)
            break;
        require_if = false;
    }

    verify(a->expr_level == 0, "unexpected expression level after ifdef");
    return statements ? statements : enode(mod, a, au, null);
}

/// parses entire chain of if, [else-if, ...] [else]
// if the cond is a constant evaluation then we do not build the condition in with LLVM build, but omit the blocks that are not used
// and only proceed
enode parse_if_else(silver a) {
    bool require_if = true;
    array tokens_cond = array(32);
    array tokens_block = array(32);
    while (true) {
        bool is_if = silver_read_if(a, "if") != null;
        validate(is_if && require_if || !require_if, "expected if");
        array cond = is_if ? read_within(a) : array();
        array block = read_body(a);
        verify(block, "expected body");
        push(tokens_cond, cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = silver_read_if(a, "else") != null;
        require_if = false;
    }
    subprocedure build_cond = subproc(a, cond_builder, null);
    subprocedure build_expr = subproc(a, block_builder, null);
    return e_if_else(a, tokens_cond, tokens_block, build_cond, build_expr);
}

enode parse_switch(silver a) {

    validate(silver_read_if(a, "switch") != null, "expected switch");
    enode e_expr = parse_expression(a, null);
    map cases = map(hsize, 16);
    array expr_def = null;
    bool all_const = is_prim(e_expr);

    while (true) {
        if (silver_read_if(a, "case")) {
            bool is_const = false;
            etype mdl_read = null;
            array meta_read = null;
            array value = read_expression(a, &mdl_read, &is_const);
            all_const &= is_const && (canonical(mdl_read) == canonical(e_expr));
            array body = read_body(a);
            //value->deep_compare = true; // tell array when its given a compare to not just do silver_element equation but effectively deep compare (i will do this)
            set(cases, value, body);
            continue;
        } else if (silver_read_if(a, "default")) {
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

// improve this to use read_expression (todo: read_expression needs to be able to keep the stack read)
array read_expression_groups(silver a) {
    array result = array(8);
    array current = array(32);
    int level = 0;

    token f = silver_peek(a);
    if (!eq(f, "["))
        return null;

    silver_consume(a);

    while (true) {
        token t = silver_consume(a);
        if (!t)
            break;

        // adjust nesting
        if (eq(t, "["))
            level++;
        else if (eq(t, "]")) {
            level--;
            if (level < 0)
                break;
        }

        // comma at base-level → end group
        if (eq(t, ",") && level == 0) {
            push(result, current);
            current = array(32);
            continue;
        }

        // normal token inside current expression
        push(current, t);
    }

    // Push final group if not empty
    if (len(current) > 0)
        push(result, current);

    return result;
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_push_builder(silver a, array expr_groups, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    statements s_members = statements(mod, a);
    push_scope(a, s_members);
    each(expr_groups, array, expr_tokens) {
        push_tokens(a, expr_tokens, 0);
        last = parse_statement(a);
        pop_tokens(a, false);
    }
    a->expr_level = level;
    return last;
}

enode parse_for(silver a) {
    validate(silver_read_if(a, "for") != null, "expected for");

    array groups = read_expression_groups(a);
    validate(groups != null, "expected [ init , cond , step [ , step-b , step-c , ...] ]");
    validate(len(groups) == 3, "for expects exactly 3 expressions");

    array init_exprs = groups->origin[0];
    array cond_expr = groups->origin[1];
    array step_exprs = groups->origin[2];

    verify(isa(init_exprs) == typeid(array), "expected array for init exprs");
    verify(isa(cond_expr) == typeid(array), "expected group of cond expr");
    cond_expr = cond_expr->count ? cond_expr->origin[0] : null;
    verify(isa(cond_expr) == typeid(array), "expected array for inner cond expr");

    array body = read_body(a);
    verify(body, "expected for-body");

    // drop remaining statements before return, so we encapsulate the members to this transaction.
    subprocedure build_init = subproc(a, statements_push_builder, null);
    subprocedure build_cond = subproc(a, cond_builder, null);
    subprocedure build_body = subproc(a, expr_builder, null);
    subprocedure build_step = subproc(a, exprs_builder, null);

    enode res = e_for(
        a,
        init_exprs, cond_expr, step_exprs,
        body,
        build_init, build_cond, build_body, build_step);
    pop_scope(a);
    return res;
}

enode parse_loop_while(silver a) {
    bool is_loop = silver_read_if(a, "loop") != null;
    validate(is_loop, "expected loop");
    array cond = read_within(a);
    array block = read_body(a);
    verify(block, "expected body");
    bool is_loop_while = silver_read_if(a, "while") != null;
    if (is_loop_while) {
        verify(!cond, "condition given above conflicts with while below");
        cond = read_within(a);
    }
    subprocedure build_cond = subproc(a, cond_builder, null);
    subprocedure build_expr = subproc(a, expr_builder, null);
    return e_loop(a, cond, block, build_cond, build_expr, is_loop_while);
}

bool is_model(silver a) {
    token k = silver_peek(a);
    enode m = elookup(k->chars);
    return m && is_type(m);
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
    token t = silver_peek(a);
    if (!t)
        return null;
    if (!isalpha(t->chars[0]))
        return null;
    Au_t f = Au_find_type((cstr)t->chars, null);
    if (f && inherits(f, typeid(etype)) && (*fn = Au_find_member(f, "parse", AU_MEMBER_FUNC)))
        return f;
    return null;
}

/// called after : or before, where the user has access
etype silver_read_def(silver a) {
    Au_t  parse_fn  = null;
    Au_t  is_type   = next_is_keyword(a, &parse_fn);
    etype class_base = !is_type ? next_is_class(a, false) : null;
    bool  is_struct = next_is(a, "struct");
    bool  is_enum   = next_is(a, "enum");

    if (!is_type && !class_base && !is_struct && !is_enum)
        return null;

    if (is_type) {
        struct _etype* (*parser)(silver) = parse_fn->value;
        validate(parser, "expected parse fn on member");
        return parser(a);
    }

    silver_consume(a);
    string n = silver_read_alpha(a);
    validate(n, "expected alpha-numeric identity, found %o", next(a));

    etype mtop = top_scope(a)->user;
    enode mem = null; // = emember(mod, a, name, n, context, mtop);
    etype mdl = null;
    array meta = null;

    if (class_base || is_struct) {
        validate(mtop->au->is_namespace,
            "expected record definition at module level");
        array schema = array();
        meta = (class_base && next_is(a, "<")) ? read_meta(a) : null;

        mdl = record(a, class_base, n,
            is_struct ? AU_TRAIT_STRUCT : AU_TRAIT_CLASS, meta);
        mdl->body = read_body(a);

    } else if (is_enum) {
        etype store = null, suffix = null;
        if (silver_read_if(a, ":")) {
            store  = instanceof(read_etype(a, null), typeid(etype));
            validate(store, "invalid storage type");
        } else
            store = elookup("i32");
        
        array enum_body = read_body(a);
        print_all(a, "enum-tokens", enum_body);
        validate(len(enum_body), "expected body for enum %o", n);

        Au_t enum_au = Au_register(top_scope(a),
            n->chars, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
        enum_au->src = store->au;
        mdl = etype(mod, a, au, enum_au);
        push_tokens(a, enum_body, 0);
        push_scope(a, mdl);

        validate(enum_au->src->is_integral,
                 "enumeration can only be based on integral types (i32 default)");

        i64 value = 0;
        while (true) {
            token e = next(a);
            if  (!e) break;
            Au    v = null;

            if (silver_read_if(a, ":")) {
                enode n = read_enode(a, store);
                verify(n && n->literal && ((Au_t)isa(n->literal))->is_integral,
                    "expected integral literal");
                v = n->literal;
                u8 sp[64];
                memcpy(sp, n->literal, isa(n->literal)->abi_size / 8);
                value = *(i64*)sp; // primitives would need a minimum allocation size in this case
            } else
                v = primitive(store->au, &value); /// this creates i8 to i64 data, using &value i64 addr
            
            Au_t enum_v         = Au_register_enum_value(enum_au, e->chars, v);
            enum_v->src         = enum_au->src;
            enum_v->value       = hold(v);
            enum_v->member_type = AU_MEMBER_ENUMV;
            enum_v->is_const    = true;
            
            enode enum_node = enode(
                mod, a, name, e, au, enum_v, literal, v);

            implement(enum_node); // this should create the value similarly to emember_set_value
            value += 1;
        }
        pop_scope(a);
        pop_tokens(a, false);
    } else {
        fault("unknown error");
    }

    validate(mdl && len(n),
             "name required for model: %s", isa(mdl)->ident);

    return mdl;
}

define_class(chatgpt, codegen)
define_class(silver, aether)
define_class(import, etype)

initializer(initialize)