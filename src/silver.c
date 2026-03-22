#include <import>
#include <limits.h>
#include <execinfo.h>
#include <ports.h>


// designed for Audrey and Rebecca
// with syntax that makes programming enjoyable
// to express intent clearly in modular development, easily targeting all platforms

etype etype_prep(silver, Au_t);
enode parse_statements(silver a);
enode parse_statement(silver a);
void build_fn(silver a, efunc fmem, callback preamble, callback postamble);
bool is_explicit_ref(enode);
enode enode_ref(aether, enode, etype);
etype evar_type(evar a);

enode parse_return(silver a);
enode parse_break(silver a);
enode parse_continue(silver a);
enode parse_expect(silver a);
enode parse_for(silver a);
enode parse_loop_while(silver a);
enode parse_if_else(silver a);
enode parse_ifdef_else(silver a);
static enode typed_expr(silver mod, enode n, array expr);
i32 read_enum(silver a, i32 def, Au_t etype);
efunc parse_func(silver, Au_t, enum AU_MEMBER, u64, OPType, string);
etype etype_resolve(etype t);
enode enode_value(enode mem, bool force);

static void build_record(silver a, etype mrec);
static void build_record_parse(silver a, etype mrec);
static void build_record_implement(silver a, etype mrec);
static void build_record_functions(silver a, etype mrec);

// used in more primitive cases
#define au_lookup(sym) lexical(a->lexical, sym)

#define elookup(sym) ({ \
    (etype)rlookup((aether)a, string(sym)); \
})

token aether_peek_safe(silver);

#undef error
#define error(t, ...) ({ \
    struct _token* pk = aether_peek_safe(a); \
    string s; \
    if (pk->line == 0) { \
        s = (string)formatter( \
            (Au_t)null, false, stderr, (Au) true, seq, \
            (symbol)"\n%s:%i\n" t, __FILE__, __LINE__ \
            __VA_OPT__(,) __VA_ARGS__); \
        if (level_err >= fault_level) { \
            halt(s, pk); \
        } \
    } else { \
        s = (string)formatter( \
            (Au_t)null, false, stderr, (Au) true, seq, \
            (symbol) "\n%s:%i%o :: %o:%i:%i\n" t, \
            __FILE__, __LINE__, seq ? f(string, "@%i", seq) : string(""), a->module_file, \
            pk->line, pk->column __VA_OPT__(,) __VA_ARGS__); \
        if (level_err >= fault_level) { \
            halt(s, aether_peek_safe(a)); \
        } \
    } \
    false; \
})

#define log_tokens(t, ...) ({ \
    string s = (string)formatter((Au_t)null, false, stderr, (Au) true, seq, (symbol) "\n%s: %s:%i@%i, %o:%i:%i\n            " t, a->name->chars, __FILE__, __LINE__, seq, a->module_file, \
                peek(a)->line, peek(a)->column, ##__VA_ARGS__); \
})

#define validate(cond, t, ...) ({ \
    if (!(cond)) { \
        error(t __VA_OPT__(,) __VA_ARGS__); \
        raise(SIGTRAP); \
    } else { \
        true; \
    } \
})

#define breakpoint(tok, t, ...) ({ \
    string s = (string)formatter((Au_t)null, false, stderr, (Au) true, seq, (symbol) "\n%s: %s:%i@%i, %o:%i:%i\n            " t, a->name->chars, __FILE__, __LINE__, seq, a->module_file, \
                tok->line, tok->column, ##__VA_ARGS__); \
    raise(SIGTRAP); \
})

// inline initializers can have expression continuations  aclass[something:1].something
// ones that are made with indentation do not   aclass
// however we usually have the option to do both (expr-level-0)
static array read_expression(silver a, etype *mdl_res, bool *is_const);
static array read_enode_tokens(silver a);

token silver_element(silver, num);


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


string symbol_name(Au obj);

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

none print_tokens(silver a, int seq) {
    log_tokens("%o %o %o %o %o %o %o %o - %i",
        element(a, 0), element(a, 1), element(a, 2),
        element(a, 3), element(a, 4), element(a, 5), element(a, 6), element(a, 7), seq);
}

void print_all(array tokens) {
    if (!tokens) { fprintf(stderr, "(null tokens)\n"); return; }
    fprintf(stderr, "--- tokens (%i) ---\n", (int)len(tokens));
    each(tokens, token, t) {
        fprintf(stderr, "%s ", t->chars);
    }
    fprintf(stderr, "\n");
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
    string remote = command_run((command)cmd, false);

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

// orbiter could build silver in this way from .c
// importing 
#ifndef BUILD_LIBRARY
int main(int argc, cstrs argv) {
    engage(argv);
    silver a = silver(argv);
    return 0;
}
#endif


typedef struct {
    OPType ops[3];
    string method[3];
    string token[3];
} precedence;

static precedence levels[] = {
    {{OPType__bitwise_and,      OPType__bitwise_or}},
    {{OPType__and,              OPType__or}},
    {{OPType__xor,              OPType__xor}},
    {{OPType__equal,            OPType__not_equal,  OPType__compare}},
    {{OPType__greater,          OPType__less}},
    {{OPType__greater_eq,       OPType__less_eq}},
    {{OPType__right,            OPType__left}},
    {{OPType__add,              OPType__sub}},
    {{OPType__mul,              OPType__div,        OPType__mod}},  // same precedence
    {{OPType__is,               OPType__inherits}},
};

token silver_read_if(silver a, symbol cs);

enode parse_object(silver a, etype mdl_schema, bool in_expr);
static bool peek_fields(silver a);

static bool silver_next_is_eq(silver a, symbol first, ...);

static enode reverse_descent(silver a, etype expect);

static bool is_loaded(Au n) {
    Au_t i = isa(n);
    if (i == typeid(etype)) return false;
    enode node = (enode)n;
    return node->loaded;
}

static inline enode expr_load(enode result, bool load) {
    if (load && result && !is_loaded((Au)result))
        result = enode_value(result, false);
    return result;
}

static enode parse_expression(silver a, etype expect, bool hint, bool load) { sequencer
    if (seq == 323)
        seq = seq;
    if (is_rec(expect) && next_is(a, "[")) {
        // collections go straight to parse_object — [ ] is always element data
        if (inherits(expect->au, typeid(collective))) {
            enode res = parse_object(a, expect, false);
            return expr_load(res, load);
        }

        push_current(a);

        consume(a);
        token pk = peek(a);
        bool is_default = eq(pk, "]");
        bool is_field = peek_fields(a);
        prev(a);
        if (!is_field && !is_default) {
            bool no_build = a->no_build;
            a->no_build   = true;
            enode unbias  = reverse_descent(a, expect);
            a->no_build   = no_build;
        }
        token l = !is_field ? element(a, -1) : null;
        pop_tokens(a, false);

        if (is_default || is_field || !eq(l, "]")) // field parser
            return expr_load(parse_object(a, expect, false), load);
    }

    enode unbias = reverse_descent(a, hint ? expect : null);
    return expr_load(e_create(a, expect, (Au)unbias), load); // parse assignment needs to expect a deref'd type, or, we call it loaded:false,
}

enode e_short_circuit_pair(silver a, OPType combine, enode L, enode R);

static bool is_multi_expression(silver a, OPType match_op) {
    if (match_op != OPType__equal && match_op != OPType__not_equal)
        return false;
    if (!next_is(a, "("))
        return false;
    
    push_current(a);
    consume(a);
    bool is_const = false;
    etype mdl = null;
    array expr = read_expression(a, &mdl, &is_const);
    bool positive = next_is(a, ",")  || 
                    next_is(a, "...") ||
                    next_is(a, "..<");
    pop_tokens(a, false);    
    return positive;
}

static enode reverse_descent(silver a, etype expect) { sequencer
    bool cmode = is_cmode(a);
    int num_levels = sizeof(levels) / sizeof(precedence);
    
    if (seq == 164)
        seq = seq;

    //print_tokens(a, seq);
    enode L = read_enode(a, expect, false, true);
    token t = peek(a);
    if (!L) {
        L = L;
    }
    if (!cmode && !L) {
        etype l = t ? elookup(t->chars) : null;
        error("unexpected %s'%o'", l->au->member_type == AU_MEMBER_TYPE ? "type " : "", t);
    }
    if (!L)
        return null;
    
    // Iterative precedence climbing without recursion
    // We use a stack to handle higher-precedence right-hand operands
    // Stack holds: pending left operands and their operator info
    #define MAX_DEPTH 64
    enode   lhs_stack[MAX_DEPTH];
    OPType  op_stack[MAX_DEPTH];
    string  method_stack[MAX_DEPTH];
    int     prec_stack[MAX_DEPTH];
    int     sp = 0;
    
    for (;;) {
        // find which precedence level the next token matches
        int     match_level = -1;
        int     match_j     = -1;
        OPType  match_op;
        string  match_method;
        string  match_tok;
        
        for (int i = num_levels - 1; i >= 0; i--) {
            precedence *prec = &levels[i];
            for (int j = 0; j < 3; j++) {
                string tok = prec->token[j];
                if (tok && next_is(a, cstring(tok))) {
                    match_level  = i;
                    match_j      = j;
                    match_op     = prec->ops[j];
                    match_method = prec->method[j];
                    match_tok    = tok;
                    goto found;
                }
            }
        }
        
    found:
        if (match_level < 0) {
            // no operator found — reduce everything on the stack
            while (sp > 0) {
                sp--;
                L = e_op(a, op_stack[sp], method_stack[sp],
                         (Au)lhs_stack[sp], (Au)L);
            }
            return L;
        }
        
        // reduce any stacked operators that are same or tighter precedence
        // (left-associative: same level reduces left-to-right)
        while (sp > 0 && prec_stack[sp - 1] >= match_level) {
            sp--;
            L = e_op(a, op_stack[sp], method_stack[sp],
                     (Au)lhs_stack[sp], (Au)L);
        }
        
        // consume the token
        read_if(a, cstring(match_tok));
        
        // handle special operators (and/or, is/inherits) inline
        if (match_op == OPType__and || match_op == OPType__or) {
            if (match_op == OPType__or && read_if(a, "return")) {
                etype rtype = return_type(a);
                enode fallback = peek(a) ? parse_expression(a, rtype, false, true) : null;
                verify(fallback || is_void(return_type(a)),
                       "expected expression after return");
                enode cond = e_not(a, L);
                enode ret_node = fallback ? e_create(a, rtype, (Au)fallback) : null;
                e_cond_return(a, cond, (Au)ret_node);
                // reduce remaining stack
                while (sp > 0) {
                    sp--;
                    L = e_op(a, op_stack[sp], method_stack[sp],
                             (Au)lhs_stack[sp], (Au)L);
                }
                return L;
            } else {
                L = e_short_circuit(a, match_op, L);
                continue;
            }
        } else if (match_op == OPType__is || match_op == OPType__inherits) {
            etype type = read_etype(a, null);
            verify(type, "expected type, got %o", peek(a));
            enode type_L = e_typeid(a, (etype)L);
            enode type_R = e_typeid(a, (etype)type);
            if (match_op == OPType__inherits) {
                Au_t f_inherits = find_member(typeid(Au), "inherits",
                                              AU_MEMBER_FUNC, 0, false);
                L = e_fn_call(a, u(efunc, f_inherits), a(type_L, type_R));
            } else {
                L = e_cmp_op(a, OPType__equal, type_L, type_R);
            }
            continue;
        }
        
        // ---------------------------------------------------------------
        // Hat operand: b == (1, 2, 3)    -> b == 1 || b == 2 || b == 3
        //              b != (1, 2, 3)    -> b != 1 && b != 2 && b != 3
        // Range hat:   b == (2...10)     -> b >= 2 && b <= 10  (inclusive)
        //              b == (2..<10)     -> b >= 2 && b < 10   (exclusive end)
        //              b != (2...10)     -> b < 2  || b > 10   (outside range)
        //              b != (2..<10)     -> b < 2  || b >= 10  (outside range)
        // Short-circuits: for ==, stops on first true  (||)
        //                 for !=, stops on first false (&&)
        // ---------------------------------------------------------------
        bool is_multi = is_multi_expression(a, match_op);
        if (is_multi) {
            consume(a);
            OPType combine = (match_op == OPType__not_equal) ? OPType__and : OPType__or;
            
            // read first operand
            enode R0 = parse_expression(a, null, false, true);
            
            // check for range syntax
            bool is_range_inclusive = read_if(a, "...") != null;
            bool is_range_exclusive = !is_range_inclusive && read_if(a, "..<") != null;
            
            if (is_range_inclusive || is_range_exclusive) {
                enode R1 = parse_expression(a, null, false, true);
                validate(read_if(a, ")"), "expected ) after range");
                
                enode lo_cmp, hi_cmp;
                if (match_op == OPType__not_equal) {
                    // b != (2...10) -> b < 2 || b > 10
                    lo_cmp = e_op(a, OPType__less,
                        null, (Au)L, (Au)R0);
                    hi_cmp = e_op(a, is_range_inclusive ? OPType__greater : OPType__greater_eq,
                        null, (Au)L, (Au)R1);
                    L = e_short_circuit_pair(a, OPType__or, lo_cmp, hi_cmp);
                } else {
                    // b == (2...10) -> b >= 2 && b <= 10
                    lo_cmp = e_op(a, OPType__greater_eq,
                        null, (Au)L, (Au)R0);
                    hi_cmp = e_op(a, is_range_inclusive ? OPType__less_eq : OPType__less,
                        null, (Au)L, (Au)R1);
                    L = e_short_circuit_pair(a, OPType__and, lo_cmp, hi_cmp);
                }
                continue;
            }
            
            if (next_is(a, ",")) {
                // regular hat: comma-separated values
                enode cmp = e_op(a, match_op, match_method, (Au)L, (Au)R0);
                
                while (read_if(a, ",")) {
                    enode Rn      = parse_expression(a, null, false, true);
                    enode next_cmp = e_op(a, match_op, match_method, (Au)L, (Au)Rn);
                    cmp = e_short_circuit_pair(a, combine, cmp, next_cmp);
                }
                
                validate(read_if(a, ")"), "expected ) after hat operand list");
                L = cmp;
                continue;
            }
            
            // not a hat — just a parenthesized expression: b == (expr)
            // R0 is already parsed, just consume ) and do normal op
            validate(read_if(a, ")"), "expected )");
            L = e_op(a, match_op, match_method, (Au)L, (Au)R0);
            continue;
        }
        
        // regular binary op: push L and the op onto the stack,
        // then read the next atom as the new L
        verify(sp < MAX_DEPTH, "expression too deep");
        lhs_stack[sp]    = L;
        op_stack[sp]     = match_op;
        method_stack[sp]  = match_method;
        prec_stack[sp]    = match_level;
        sp++;
        
        L = read_enode(a, (match_op == OPType__equal || match_op == OPType__not_equal) ?
            canonical(L) : null, false, true);
    }
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
void etype_register(aether, Au, Au, bool);

void finalize_coverage(silver);

void silver_parse(silver a) {
    efunc init = module_initializer(a);

    // determine target arch/os — use --platform if set, otherwise host
    symbol target_arch = arch;
    bool   target_mac  = SILVER_IS_MAC;
    bool   target_lin  = SILVER_IS_LINUX;
    bool   target_win  = SILVER_IS_WINDOWS;

    if (a->platform && len(a->platform)) {
        string p = a->platform;
        // derive OS from platform name
        target_mac = strstr(p->chars, "apple")   != NULL || strstr(p->chars, "ios")     != NULL;
        target_lin = strstr(p->chars, "linux")   != NULL || strstr(p->chars, "jetson")  != NULL ||
                     strstr(p->chars, "android") != NULL;
        target_win = strstr(p->chars, "windows") != NULL;
        // derive arch from platform name
        if      (strstr(p->chars, "x86_64") || strstr(p->chars, "x86-64"))  target_arch = "x86_64";
        else if (strstr(p->chars, "x86")    || strstr(p->chars, "i686"))    target_arch = "x86";
        else if (strstr(p->chars, "arm64")  || strstr(p->chars, "aarch64")
              || strstr(p->chars, "jetson") || strstr(p->chars, "ios"))     target_arch = "arm64";
        else if (strstr(p->chars, "arm32")  || strstr(p->chars, "armv7"))   target_arch = "arm32";
        else if (strstr(p->chars, "mips"))                                   target_arch = "mips";
        else if (strstr(p->chars, "riscv"))                                  target_arch = "riscv64";
    }

    Au_t m_mac   = def_member(a->au, "apple",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_lin   = def_member(a->au, "linux",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_win   = def_member(a->au, "windows", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_x86   = def_member(a->au, "x86_64",  typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_arm64 = def_member(a->au, "arm64",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);

    etype_register((aether)a, (Au)m_mac,   (Au)hold(e_operand(a, _bool(target_mac),                        etypeid(bool))), false);
    etype_register((aether)a, (Au)m_lin,   (Au)hold(e_operand(a, _bool(target_lin),                        etypeid(bool))), false);
    etype_register((aether)a, (Au)m_win,   (Au)hold(e_operand(a, _bool(target_win),                        etypeid(bool))), false);
    etype_register((aether)a, (Au)m_x86,   (Au)hold(e_operand(a, _bool(strcmp(target_arch, "x86_64") == 0), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_arm64, (Au)hold(e_operand(a, _bool(strcmp(target_arch, "arm64")  == 0), etypeid(bool))), false);

    while (peek(a)) {
        validate(parse_statement(a), "unexpected token found for statement: %o", peek(a));
        incremental_resolve(a);
    }

    /// phase 1: parse all record bodies so every class/struct name and member is registered
    members(a->au, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && !rec->parsing && !rec->user_built)
            build_record_parse(a, rec);
    }

    /// phase 2: implement all LLVM types (records and free functions)
    members(a->au, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && rec->user_built)
            build_record_implement(a, rec);
    }
    members(a->au, mem) {
        etype e = u(etype, mem);
        if (is_func((Au)mem) && !mem->is_system && e != a->fn_init && !e->user_built)
            implement(e, false);
    }

    /// phase 3: build all functions (all LLVM types now complete)
    members(a->au, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && rec->user_built)
            build_record_functions(a, rec);
    }
    members(a->au, mem) {
        etype e = u(etype, mem);
        if (is_func((Au)mem) && !mem->is_system && e != a->fn_init && !e->user_built)
            build_fn(a, (efunc)e, null, null);
    }

    // when done parsing, we are able to create a module schema (type_id definition) and the evar instance for the type_id (module_m with info/type)
    implement_type_id((etype)a);

    // explicit call to finalize the coverage globals
    // we need to not emit during init
    finalize_coverage(a);

    a->building_initializer = true;
    build_fn(a, init, build_init_preamble, null);
    a->building_initializer = false;
}

none aether_test_write(aether a);


// who throws away a Perfectly Good Watch?
#ifdef __linux__
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
#else
i64 silver_watch(silver mod, path a, i64 last_mod, i64 millis) {
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
        usleep(100000); // 100 ms poll
    }
    return last_mod;
}
#endif

// not sure what this does on windows without a repo -- probably freezes everything.
static path is_git_project(silver a) {

    // must be repo path: a->project_path
    // if so, return a->project_path
    // walk up from project_path to find the git repo root
    path dir = parent_dir(a->module_path);
    while (len(dir) != 1 && !dir_exists("%o/.git", dir))
        dir = parent_dir(dir);
    
    return len(dir) > 1 ? dir : null;
}

static void exporter(silver a) {
    if (a->is_external || !len(a->exports))
        return;
    
    // after successful build on main module, apply all export tags at once
    pairs(a->exports, i) {
        string  module      = (string)i->key;
        exports exp         = (exports)i->value;

        string mod_file = cast(string, exp->module_file);
        string rel_mod = mid(mod_file, exp->project_path->count + 1, len(exp->project_path) - exp->project_path->count);
        string  tag         = f(string, "%o-%o", i->key, exp->version);
        string  cmd         = f(string, "git rev-parse %o:%o", tag, rel_mod);
        string  rev_parse   = command_run((command)cmd, false);
        string  hash_cmd    = f(string, "git hash-object %o", exp->module_file);
        string  hash        = command_run((command)hash_cmd, false);

        if (compare(hash, rev_parse) != 0)
            vexec(true, "git-tag", "git -C %o tag -f %o", a->project_path, tag);
    }
}


void llvm_reinit(silver);
void aether_reinit_startup(aether);
void emit_debug_loc(aether, u32, u32);

// im a module!
static void write_target_cmake(path sdk_path, cstr system_name, cstr processor,
                               cstr triple, cstr sysroot, path clang_bin) {
    path   cmake_path = f(path, "%o/target.cmake", sdk_path);
    string content    = f(string,
        "# Auto-generated by Silver bootstrap\n"
        "# Toolchain for %s (%s)\n\n"
        "set(CMAKE_SYSTEM_NAME %s)\n"
        "set(CMAKE_SYSTEM_PROCESSOR %s)\n\n"
        "get_filename_component(TARGET_DIR \"${CMAKE_CURRENT_LIST_FILE}\" PATH)\n"
        "set(CMAKE_C_COMPILER   \"%o/clang\" CACHE STRING \"\")\n"
        "set(CMAKE_CXX_COMPILER \"%o/clang++\" CACHE STRING \"\")\n"
        "set(CMAKE_LINKER       \"%o/ld.lld\" CACHE STRING \"\")\n"
        "set(CMAKE_SYSROOT      \"%s\" CACHE STRING \"\")\n\n"
        "set(CMAKE_C_FLAGS   \"--target=%s -fPIC\" CACHE STRING \"\")\n"
        "set(CMAKE_CXX_FLAGS \"--target=%s -fPIC -stdlib=libc++\" CACHE STRING \"\")\n"
        "set(CMAKE_EXE_LINKER_FLAGS    \"-fuse-ld=lld\" CACHE STRING \"\")\n"
        "set(CMAKE_SHARED_LINKER_FLAGS \"-fuse-ld=lld\" CACHE STRING \"\")\n\n"
        "set(CMAKE_FIND_ROOT_PATH \"${CMAKE_SYSROOT}\")\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n\n"
        "set(SILVER_TARGET_NAME \"%s\")\n"
        "set(SILVER_TARGET_TRIPLE \"%s\")\n",
        triple, system_name,
        system_name, processor,
        clang_bin, clang_bin, clang_bin, sysroot,
        triple, triple,
        triple, triple);
    fdata fd = fdata(write, true, src, cmake_path);
    file_write(fd, (Au)content);
}

static void prepare_record_cb(Au a_au, Au t_au) {
    silver a = (silver)a_au;
    etype  t = (etype)t_au;
    build_record_parse(a, t);
}

void silver_init(silver a) {
    hold(a);

    bool is_once = a->build || a->is_external;

    if (a->version) {
        printf("silver 0.88\n");
        printf("Copyright (C) 2017 Kalen Novis White\n");
        printf("This is free software; see the source for LICENSE.  There is NO\n");
        printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
        return;
    }

    // platform dispatch: build docker prefix for non-native targets
    // all architecture-specific exec calls get this prepended
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
        path   silver_root = absolute(path(SILVER));
        string image = f(string, "silver-platform-%o", a->platform);
        path   plat  = f(path,   "%o/platform/%o", silver_root, a->platform);
        path   df    = f(path,   "%o/Dockerfile", plat);

        // detect if we need sg docker for permission
        bool use_sg = exec(false, "docker info >/dev/null 2>&1") != 0
                   && exec(false, "sg docker -c 'docker info >/dev/null 2>&1'") == 0;
        string dkpre = use_sg ? string("sg docker -c '") : string("");
        string dkpost = use_sg ? string("'") : string("");

        // build docker image if needed
        if (exec(false, "%odocker image inspect %o >/dev/null 2>&1%o", dkpre, image, dkpost) != 0) {
            verify(file_exists("%o", df), "no Dockerfile for platform: %o", a->platform);
            verify(exec(a->verbose,
                "%odocker build -t %o -f %o %o%o", dkpre, image, df, silver_root, dkpost) == 0,
                "docker build failed for platform: %o", a->platform);
        }

        // prefix for all build commands
        // mount checkout (shared source) and build_dir (output) but NOT the
        // full install dir — the toolchain lives inside the image
        a->docker = f(string,
            "%odocker run --rm "
            "-v %o/checkout:%o/checkout "
            "-v %o:%o "
            "%o%o",
            dkpre,
            a->root_path, a->root_path,
            a->build_dir, a->build_dir,
            image, dkpost);
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

#if defined(__SANITIZE_ADDRESS__) || defined(__has_feature) && __has_feature(address_sanitizer)
    a->asan         = true;
#endif
    a->exports      = map(hsize, 16);
    a->build_dir    = f(path, "%o/%s", a->install, a->debug ? "debug" : "release");
    a->product_link = f(path, "%o/%o.product", a->build_dir, a->name);
    a->defs_expect  = map(hsize, 4);
    a->defs_used    = map(hsize, 4);
    a->defs_hash    = defs_hash;
    //a->import_cache = map();
    a->artifacts    = array(32);
    a->artifacts_path = f(path, "%o/%o.artifacts", a->build_dir, a->name);
    a->resources    = array(32);

    verify(a->module && len(a->module), "required argument: module (path/to/module)");

    path cwd = path_cwd();
    // aether_init already resolves module to absolute path
    // accept dir or .ag file
    bool retry_path = false;
    if (cmp(ext(a->module), "ag") == 0) {
        a->module_file = hold(absolute(a->module));
        a->module      = parent_dir(a->module_file);
    } else {
        string m_stem  = stem(a->module);
        a->module      = absolute(a->module);
        a->module_file = f(path, "%o/%o.ag", a->module, m_stem);
        retry_path = true;
    }

    a->module_path = hold(a->module);
    u64  module_file_m  = modified_time(a->module_file);

    // see if we are specifying the module by its name alone, while inside the module folder
    // in that case we validate its parent folder to be the same name
    if (!module_file_m && retry_path && file_exists("%o.ag", a->module)) {
        a->module_file = absolute(f(path, "%o.ag", a->module));
        a->module      = parent_dir(a->module_file);
        drop(a->module_path);
        a->module_path = hold(a->module);
        module_file_m  = modified_time(a->module_file);
    }

    aether_reinit_startup((aether)a);

    // discover resource folders within module directory and register on root instance
    {
        silver og = a->is_external ? a->is_external : a;
        DIR *dir = opendir(a->module_path->chars);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (entry->d_type != DT_DIR)  continue;
                path res = form(path, "%o/%s", a->module_path, entry->d_name);
                if (index_of(og->resources, (Au)res) < 0)
                    push(og->resources, (Au)hold(res));
            }
            closedir(dir);
        }
    }

    // 1ms resolution time comparison (it could be nano-second based)
    bool update_product = true;

    verify(module_file_m, "module file not found: %o", a->module_file);
    push(a->include_paths, (Au)a->module); // add include folder just for our module (this was in aether's init, interfering with our filter logic)

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
    //cstr _IMPORT = getenv("IMPORT");
    //if (!_IMPORT) _IMPORT = cstr_copy(path_cwd()->chars);
    verify(dir_exists("%s", SILVER), "silver environment moved; please re-build for secure builds");
    cstr _SILVER = cstr_copy(absolute(path(SILVER))->chars);

    a->mod          = (aether)a;
    a->imports      = array(32);
    a->parse_f        = parse_tokens;
    a->parse_expr     = parse_expression;
    //a->parse_enode  = silver_read_enode;
    //a->reverse_descent = reverse_descent;
    a->read_etype     = read_etype;
    a->prepare_record = (callback)prepare_record_cb;
    a->src_loc      = absolute(path(_SRC ? _SRC : "."));
    verify(dir_exists("%o", a->src_loc), "SRC path does not exist");

    // should only get its parent if its a file
    path af         = a->module ? directory(a->module) : path_cwd();
    path install    = (a->platform && len(a->platform) && cmp(a->platform, "native") != 0)
                    ? f(path, "%s/platform/%o", _SILVER, a->platform)
                    : f(path, "%s/install",     _SILVER);
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

            Au_t mod2 = (Au_t)0x7ffff7eb7c30;
            if (mod2) {
                mod2 = mod2;
            }

            parse(a);

            Au_t mod4 = (Au_t)0x7ffff7eb7c30;
            if (mod4) {
                mod4 = mod4;
            }
            // print all expected defs not used
            if (len(a->defs_expect))
                pairs(a->defs_expect, i) {
                    bool   f = get(a->defs_used, i->key) != null;
                    verify(f, "expected def not provided: %o", i->key);
                }
            
            // print unused defs from an import
            if (len(a->defs) != len(a->defs_used)) {
                string unused = string(alloc, 64);
                pairs(a->defs, i) {
                    string k = (string)i->key;
                    if (!get(a->defs_used, (Au)k)) {
                        if (len(unused))
                            append(unused, ", ");
                        concat(unused, k);
                    }
                }
                fault("defs not found in %o: %o", a->name, unused);
            }

            build(a);

            exporter(a);

            if (a->run) {
                string arg_str = string(alloc, 32);
                int argc = len(a->run) + 2;
                char** argv = calloc(argc, sizeof(char*));
                argv[0] = a->product->chars;
                int i = 1;
                each(a->run, Au, arg) {
                    argv[i++] = cast(string, arg)->chars;
                }
                argv[i] = NULL; // Brannigans law
                execvp(argv[0], argv);
                //_exit(1);
            }
        }
        on_error() {
            mtime = current_time();
            retry = !is_once;
            a->error = true;
        }
        finally()
    } while (!a->is_external && retry); // externals do not watch (your watcher must invoke everything)
                                        // they handle their own exceptions

    unload_libs(a);
    module_erase(a->au, null);
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

// for statement will never call casts per iteration; we would be sure to do this manually
// implicit conversion is sometimes not known -- for fors, we do not want a performance penalty tripwire
// silver is about being extremely noticable, and easy on the brain.  
// operations do conversion, not baked in assignment stages.. it takes extra work to make it worse here

static void silver_module() {
    keywords = hold(array_of_cstr(
        "class",    "struct",   "scalar",   "expect",   "fault",    "abstract", "context",  "public",   "intern",
        "import",   "export",   "typeid",   
        "is",       "inherits", "ref",      "in",   "lambda",
        "const",    "no-op",    "<>",
        "return",   "->",       "::",       "...",  
        "asm",      "if",       "switch",   "any",
        "enum",     "ifdef",    "el",       "while",
        "cast",     "try",      "throw",    "catch",
        "finally",  "for",      "func",     "attrib",
        "operator", "construct", "alias",   "getter", "setter",
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
        "%",  string("mod"),
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
        //"??", string("value_default"), -- these two exist as branches off () parenthesis. as such their scope is clear
        //"?:", string("cond_value"),
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
        "<=>", string("compare"),
        "==", string("equal"), // placeholder impossible match, just so we have the enum ordering
        "!=", string("not_equal"),
        "is", string("is"),
        "inherits", string("inherits"),
        "...", string("range_exclusive"),
        "..<", string("range_inclusive"),
        null));

    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 3; j++) {
            OPType op = level->ops[j];
            if (!op) continue;
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
    if (!a->tokens || !len(a->tokens))
        return null;
    int r = a->cursor + rel;
    if (r < 0 || r > a->tokens->count - 1)
        return null;
    token t = (token)a->tokens->origin[r];
    return t;
}

bool silver_next_indent(silver a) {
    token p = a->statement_origin ? a->statement_origin : element(a, -1);
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

static bool next_neighbor(silver a) {
    token t0 = element(a, 0);
    token t1 = element(a, 1);
    if (t0 && t1 && t0->line == t1->line)
        return true;
    return false;
}

bool dbg_addr_to_line(void *addr,
        const char **file,
        int *line,
        const char **func);


token silver_next(silver a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    if (!a->clipping && res && res->annotation && strcmp(res->annotation->chars, "#break") == 0) {
        breakpoint(res, "breaking at %o", res);
    }
    a->cursor++;
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

// read compacted keyword tokens inside { ... } — returns tokens literal
static enode read_keywords(silver a) {
    if (!next_is(a, "{"))
        return null;
    consume(a); // consume {
    array toks = array(16);
    int depth = 1;
    for (;;) {
        token t = peek(a);
        if (!t) break;
        if (eq(t, "}")) { depth--; consume(a); if (depth == 0) break; }
        if (eq(t, "{")) { depth++; }
        // compact neighboring tokens on the same line (like commit-id parsing)
        string compacted = string(t->chars);
        consume(a);
        while (next_is_neighbor(a) && !next_is(a, "}")) {
            token nb = peek(a);
            concat(compacted, (string)nb);
            consume(a);
        }
        push(toks, (Au)compacted);
    }
    return e_create(a, etypeid(tokens), (Au)toks);
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
    token p = a->statement_origin ? a->statement_origin : element(a, -1);
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

  
static array read_body_br(silver a, int bracket_depth);

/*
static array read_body(silver a) { return read_body_br(a, 0); }

static array read_body_br(silver a, int bracket_depth) {
    a->clipping = true;
    array body = array(32);
    token n = element(a, 0);
    if (!n)
        return null;
    token p = a->statement_origin ? a->statement_origin : element(a, -1);
    bool mult = n->line > p->line;
    int tokens_depth = 0;
    while (1) {
        token k = peek(a);
        if (!k)
            break;
        if (eq(k, "{")) tokens_depth++;
        if (eq(k, "}")) tokens_depth--;
        if (tokens_depth <= 0) {
            if (eq(k, "[")) bracket_depth++;
            if (eq(k, "]")) {
                if (bracket_depth <= 0)
                    break; // hit ] without matching [ — belongs to outer scope
                bracket_depth--;
            }
        }
        if (bracket_depth <= 0 && tokens_depth <= 0) {
            if (!mult && k->line > n->line)
                break;
            if (mult && k->indent <= p->indent)
                break;
        }
        push(body, (Au)k);
        consume(a);
    }
    a->clipping = false;
    return body;
}
*/

static array peek_body(silver a) {
    push_current(a);
    array body = read_body(a);
    pop_tokens(a, false);
    return len(body) ? body : null;
}

evar read_evar(silver a) {
    string name = read_alpha(a);
    if   (!name) return null;
    etype  mem  = elookup(name->chars);
    Au_t info = isa(mem);
    evar   node = instanceof(mem, evar);
    return node;
}

/// check if a platform define is truthy (used by asm conditional and ifdef)
static bool eval_define(silver a, string name) {
    Au_t  mem  = lexical(a->lexical, cstring(name));
    enode node = mem ? (enode)get(a->registry, (Au)mem) : null;
    Au    val  = node ? node->literal : null;
    return val && cast(bool, val);
}

enode aether_e_memop(aether, enode, enode, enode, bool);

/// parse memcpy/memset — emit LLVM intrinsics directly, bypassing C macro resolution
static enode parse_memop(silver a) { sequencer
    bool is_memcpy = read_if(a, "memcpy") != null;
    if (!is_memcpy) verify(read_if(a, "memset"), "expected memcpy or memset");

    bool read_br = read_if(a, "[") != null;
    a->expr_level++;
    enode dst  = parse_expression(a, null, true, true);
    validate(read_if(a, ","), "expected , after dst");
    enode arg2 = parse_expression(a, null, true, true);
    validate(read_if(a, ","), "expected , after %s", is_memcpy ? "src" : "val");
    enode size = parse_expression(a, null, true, true);
    a->expr_level--;
    if (read_br) validate(read_if(a, "]"), "expected ] after %s", is_memcpy ? "memcpy" : "memset");

    return aether_e_memop((aether)a, dst, arg2, size, is_memcpy);
}

enode parse_asm(silver a, etype rtype) {
    //validate(read_if(a, "asm") != null, "expected asm");

    // conditional asm: asm <define>  — skip block if define is falsy
    // the define must be on the same line as 'asm'
    token asm_tok = element(a, -1);
    token pk = peek(a);
    if (pk && pk->line == asm_tok->line && isalpha(pk->chars[0]) && !next_is(a, "[")) {
        string cond_name = read_alpha(a);
        if (!eval_define(a, cond_name)) {
            read_body(a); // consume and discard the body
            return enode(mod, (aether)a, au, null);
        }
    }

    array input_nodes  = array(alloc, 8);
    array input_tokens = next_is(a, "[") ? read_within(a) : null;
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

    // auto-gather: if no [ inputs ] given, scan body for in-scope variables
    if (!input_tokens) {
        for (int i = 0; i < len(body); i++) {
            token t = (token)get(body, i);
            if (!t->chars[0] || !isalpha(t->chars[0]))
                continue;
            Au_t m = lexical(a->lexical, t->chars);
            if (!m) continue;
            evar node = instanceof(u(etype, m), evar);
            if (!node) continue;
            // check if already added
            bool found = false;
            for (int j = 0; j < len(input_nodes); j++)
                if (get(input_nodes, j) == (Au)node)
                    { found = true; break; }
            if (!found)
                push(input_nodes, (Au)node);
        }
    }

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

static array read_initializer(silver a) { sequencer
    array body = array(32);
    token n    = element(a,  0);
    if  (!n || eq(n, "sub") || eq(n, "asm")) return null;
    token prev = element(a, -1);
    token p    = a->statement_origin ? a->statement_origin : prev;

    // when [ follows a token on the same line, it's always a bracket expr
    // (use prev, not statement_origin, to detect inline [ after type/func names)
    if (eq(n, "[") && ((prev && n->line == prev->line) || n->line == p->line || (n->line > p->line && n->indent == p->indent))) {
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
        // count open brackets from the line preceding the continuation
        int pre_brackets = 0;
        token prev_tok = (a->cursor > 0) ? (token)a->tokens->origin[a->cursor - 1] : null;
        int prev_line = prev_tok ? prev_tok->line : -1;
        for (int i = a->cursor - 1; i >= 0; i--) {
            token t = (token)a->tokens->origin[i];
            if (t->line != prev_line) break;
            if (eq(t, "[")) pre_brackets++;
            if (eq(t, "]")) pre_brackets--;
        }
        // if there's an unclosed [ on the preceding line, this is expression
        // continuation, not a body block — don't wrap in brackets
        if (pre_brackets > 0)
            return null;
        array res = read_body(a);
        array r = array(alloc, res->count + 2);
        push(r, (Au)token(chars, "["));
        concat(r, res);
        push(r, (Au)token(chars, "]"));
        return r;
    }

    return n->line == p->line ? read_enode_tokens(a) : null;
}

array peek_initializer(silver a) {
    push_current(a);
    array result = read_initializer(a);
    pop_tokens(a, false);
    return result;
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

// returns null if not a numeric literal
// handles: 0xFF, 0b1010, 0o77, 123, -45, 3.14, 3.14f, 1.0e-7
static Au parse_numeric(string str, string* str_res, i64* index) {
    int ln  = len(str);
    int i   = *index;
    i32 chr = idx(str, i);
    int start = i;
    
    // optional leading minus
    if (chr == '-') {
        if (i + 1 >= ln || !isdigit(idx(str, i + 1)))
            return null;
        i++;
        chr = idx(str, i);
    }
    
    if (!isdigit(chr))
        return null;
    
    // hex: 0x[0-9a-fA-F]+
    // hex: 0x[0-9a-fA-F]+ or hex float:0x[0-9a-fA-F]*.[0-9a-fA-F]*p[+-]?[0-9]+
    if (chr == '0' && i + 1 < ln && idx(str, i + 1) == 'x') {
        i += 2;
        if (i >= ln || !isxdigit(idx(str, i)))
            return _i64(0);
        while (i < ln && isxdigit(idx(str, i)))
            i++;
        // hex float: 0x1.0p-24 style
        if (i < ln && idx(str, i) == '.') {
            i++;
            while (i < ln && isxdigit(idx(str, i)))
                i++;
            if (i < ln && (idx(str, i) == 'p' || idx(str, i) == 'P')) {
                i++;
                if (i < ln && (idx(str, i) == '+' || idx(str, i) == '-'))
                    i++;
                while (i < ln && isdigit(idx(str, i)))
                    i++;
            }
            string crop = mid(str, start, i - start);
            *str_res = crop;
            *index = i;
            return _f64(strtod(crop->chars, NULL));
        }
        string crop = mid(str, start, i - start);
        *str_res = crop;
        *index = i;
        return _i64((i64)strtoull(crop->chars, NULL, 16));
    }

    
    // binary: 0b[01]+
    if (chr == '0' && i + 1 < ln && idx(str, i + 1) == 'b') {
        i += 2;
        if (i >= ln || (idx(str, i) != '0' && idx(str, i) != '1'))
            return _i64(0);
        while (i < ln && (idx(str, i) == '0' || idx(str, i) == '1'))
            i++;
        string crop = mid(str, start, i - start);
        *str_res = crop;
        *index = i;
        return _i64(strtoll(crop->chars + 2, NULL, 2));
    }
    
    // octal: 0o[0-7]+
    if (chr == '0' && i + 1 < ln && idx(str, i + 1) == 'o') {
        i += 2;
        if (i >= ln || idx(str, i) < '0' || idx(str, i) > '7')
            return _i64(0);
        while (i < ln && idx(str, i) >= '0' && idx(str, i) <= '7')
            i++;
        string crop = mid(str, start, i - start);
        *str_res = crop;
        *index = i;
        return _i64(strtoll(crop->chars + 2, NULL, 8));
    }
    
    return null;
}

string trim_annotation(string input) {
    string annotation = trim(input);
    int i = index_of(annotation, " ");
    if (i >= 0)
        annotation = mid(annotation, 0, i);
    return annotation;
}

string unicode_char(i32);

static array parse_tokens(silver a, Au input, array output) { sequencer
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
    string special = string(".{}$,<>()![]/+*:=#~");
    i32 special_ln = len(special);
    for (int i = 0; i < special_ln; i++)
        push(symbols, (Au)unicode_char((i32)special->chars[i]));
    push(symbols, (Au)string(".."));
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

    num     line_num    = 1;
    num     length      = len(input_string);
    num     index       = 0;
    num     line_start  = 0;
    num     indent      = 0;
    bool    num_start   = 0;
    bool    cmode       = a->cmode || (len(a->tokens) && is_cmode(a));
    i32     chr0        = idx(input_string, index);
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

        if (seq == 1542 && index == 12020) {
            seq = seq;
        }

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
                // single-line / #annotations [ this is why hash-tag comments are brilliant; its not just for looks ]
                string annotation = null;
                int    start = index;
                while (index < length && idx(input_string, index) != '\n') {
                    index += 1;
                }
                annotation = trim_annotation(mid(input_string, start, index - start));
                token last = (token)last_element(tokens);
                if (last) {
                    if (!eq(annotation, "#break-last")) { // break will break on first-token, and break-last breaks on the last
                        int line_ref = last->line;
                        int offset = 1;
                        while (offset < len(tokens)) {
                            token t = (token)tokens->origin[len(tokens) - offset];
                            if (t->line != line_ref)
                                break;
                            last = t;
                            offset++;
                        }
                        annotation = trim_annotation(annotation);
                    } else {
                        annotation = annotation;
                    }
                    last->annotation = hold(new(string, chars, annotation->chars));
                }
            }
            continue;
        }

        if (strncmp(&input_string->chars[index], "99998.0", 7) == 0)
            input_string = input_string;

        string name = scan_map(mapping, input_string, index);
        if (name && len(name) == 1 && name->chars[0] == '-' && index + 1 < length && isdigit(idx(input_string, index + 1))) {
            token prev = (token)last_element(tokens);
            if (!prev || (prev->column + len(prev) < index - line_start) || 
                    eq(prev, "[") || eq(prev, "(") || eq(prev, ","))
                name = null; // gap before '-', treat as negative literal
        }
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

            // work on sub strings at depth

            int brace_depth = 0;
            while (index < length) {
                i32 c = idx(input_string, index);
                if (c == '\\' && index + 1 < length &&
                    idx(input_string, index + 1) == quote_char) {
                    index += 2;
                    continue;
                }
                if (c == '{') {
                    if (index + 1 < length && idx(input_string, index + 1) == '{') {
                        index += 2;
                        continue;
                    }
                    brace_depth++;
                    index += 1;
                    continue;
                }
                if (c == '}') {
                    if (index + 1 < length && idx(input_string, index + 1) == '}') {
                        index += 2;
                        continue;
                    }
                    if (brace_depth > 0)
                        brace_depth--;
                    index += 1;
                    continue;
                }
                if (brace_depth > 0 && (c == '"' || c == '\'')) {
                    i32 inner_quote = c;
                    index += 1;
                    while (index < length) {
                        i32 ic = idx(input_string, index);
                        if (ic == '\\' && index + 1 < length &&
                            idx(input_string, index + 1) == inner_quote) {
                            index += 2;
                            continue;
                        }
                        if (ic == inner_quote)
                            break;
                        index += 1;
                    }
                    index += 1;
                    continue;
                }
                if (c == quote_char && brace_depth == 0)
                    break;
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

            // combine literal strings in c
            token l = (token)last_element(tokens);
            if (cmode && l && chr == '\"' && isa(l->literal) == typeid(const_string)) {
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

        num     start           = index;
        bool    last_dash       = false;
        i32     st_char         = idx(input_string, start);
        bool    start_numeric   = st_char == '-' || (st_char >= '0' && st_char <= '9');
        string  shape_str       = null;
        bool    is_b16          = start_numeric && st_char == '0' && 
                    index + 1 < length && idx(input_string, index + 1) == 'x' &&
                    index + 2 < length && isxdigit(idx(input_string, index + 2));
        Au      literal         = (start_numeric && !is_b16) ? (Au)parse_shape(input_string, &shape_str, &index) : null;

        if (start_numeric && is_b16) {
            literal = parse_numeric(input_string, &shape_str, &index);
        }
        if (literal) {
            push(tokens, (Au)token(
                    chars,   shape_str->chars,
                    indent,  indent,
                    source,  src,
                    line,    line_num,
                    literal, literal,
                    column,  start - line_start));
            continue;
        }

        int seps = 0;
        while (index < length) {
            i32     v               = idx(input_string, index);
            bool    is_sep          = v == '.';  if (is_sep) seps++;
            bool    cont_numeric    = (is_sep && seps <= 1) || (v >= '0' && v <= '9');
            char    sval[2]         = {v, 0};
            bool    is_dash         = v == '-';
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
        // strip C integer suffixes (U, u, L, l, UL, LL, etc.) — not floats
        if (num_start && len(crop) > 1 && !strchr(crop->chars, '.')) {
            int end = len(crop);
            while (end > 1) {
                char c = crop->chars[end - 1];
                if (c == 'U' || c == 'u' || c == 'L' || c == 'l')
                    end--;
                else
                    break;
            }
            if (end < len(crop))
                crop = mid(crop, 0, end);
        }
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

string silver_peek_def(silver a) {
    token n = element(a, 0);
    Au_t top = top_scope(a);
    etype t = u(etype, top);
    etype rec = is_rec(t) ? t : null;
    if (!rec && next_is_class(a, false))
        return string(n->chars);

    
    if (n && is_keyword((Au)n))
        if (eq(n, "import") || eq(n, "export") || eq(n, "func") || eq(n, "cast") ||
            eq(n, "attrib") || eq(n, "class")  || eq(n, "enum") || eq(n, "struct") ||
            eq(n, "scalar") || eq(n, "alias"))
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

OPType silver_read_operator(silver a, ARef fname) {
    token n = element(a, 0);
    if (!n)
        return OPType__undefined;
    string found = (string)get(operators, (Au)n);
    if (found) {
        consume(a);
        char uname[64];
        snprintf(uname, sizeof(uname), "_%s", found->chars);
        *(string*)fname = string(uname);
        return evalue(typeid(OPType), uname);
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

bool in_context(Au_t au, Au_t ctx) {
    if (ctx->is_pointer)
        ctx = ctx->src;
    while (ctx && au) {
        if (au->context == ctx) return true;
        if (ctx == ctx->context) break;
        ctx = ctx->context;
    }
    return false;
}

string read_alpha_macrofilter(silver a, bool is_decl) {
    enode res = null;

    push_current(a);
    string n = read_alpha(a);
    if (!n) {
        pop_tokens(a, false);
        return null;
    }

    bool next_is_paren = next_is(a, "(");
    Au_t mem = null;
    bool use_name;
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t au = (Au_t)a->lexical->origin[i];
        while (au) {
            if (au->member_type == AU_MEMBER_TYPE || is_func(au))
                for (int ii = 0; ii < au->args.count; ii++) {
                    Au_t m = (Au_t)au->args.origin[ii];
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
    use_name = (is_decl || mem != null || next_is(a, ":"));
    pop_tokens(a, use_name);
    return use_name ? n : null;
}

enode enode_super(etype, enode);


etype etype_create(silver, Au_t);

enode silver_parse_member(silver a, ARef assign_type, Au_t in_decl, etype scope_mdl, bool in_ref) { static int seq = 0; seq++;
    OPType assign_enum = OPType__undefined;
    Au_t   top     = top_scope(a);
    etype  rec_top = context_record(a);
    silver module  =  !is_cmode(a) && (top->is_namespace) ? a : null;
    efunc  f       =  !is_cmode(a) ? context_func(a) : null;
    bool   in_rec  = rec_top && rec_top->au == top;
    token t1 = element(a, 1);
    bool new_bind = t1 && eq(t1, ":");
    
    if (seq == 593) {
        seq = seq;
    }

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
            if (mdl && (m->au->member_type != AU_MEMBER_VAR && (mdl->au->member_type == AU_MEMBER_TYPE || mdl->au->member_type == AU_MEMBER_MODULE)))
                skip_member_check = true;
                // might replace type[...] -> i32[array]
                // specifying a type gives us more types from an open concept
        }
    }

    bool is_super = false;
    string first_alpha = null;
    for (;!skip_member_check;) {
        bool first = !mem;

        token pkzip = peek(a);
        bool new_name = in_decl != null || in_rec;
        alpha = read_alpha_macrofilter(a, new_name);





        if (alpha && eq(alpha, "res_dealloc")) {
            mem = mem;
        }

        if (!alpha && first && next_is(a, "super"))
            alpha = read_alpha(a);
        if (!alpha && first && scope_mdl) {
            string bare = peek_alpha(a);
            if (bare && find_member(scope_mdl->au, bare->chars, 0, 0, true))
                alpha = read_alpha(a);
        }
        if (alpha && eq(alpha, "undefined")) {
            alpha = alpha;
        }
        if (!first_alpha) first_alpha = alpha;

        if (!(!first || alpha || new_name)) {
            //print_tokens(a, seq);
            first = first;
        }
        validate(!first || alpha || new_name,
            "[%i] expected member, found %o ", seq, peek(a) ? peek(a) : (token)string("[empty]"));

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
            if (seq == 438) {
                int test2 = 2;
                test2    += 2;
            }
            // we may only define our own members within our own space
            if (first) {

                if (eq(alpha, "super")) { // take care now
                    validate(rec_top, "super only valid in class context");
                    mem = enode_super(rec_top, f->target);
                    is_super = true;
                }
                else if (!in_rec) {
                    // try implicit 'this' access in instance methods
                    if (!mem && f && f->target) {
                        mem = (enode)rlookup((aether)a, alpha);
                        //mem = (enode)elookup(alpha->chars);

                        if (mem && !mem->au->is_static && mem->au->member_type != AU_MEMBER_TYPE) {
                            etype ftarg = etype_resolve((etype)f->target);
                            if (ftarg && in_context(mem->au, ftarg->au)) {
                                mem = access(f->target, alpha, true);
                            }
                        }

                    } else if (!f || !f->target) {
                        mem = (enode)rlookup((aether)a, alpha);
                    }
                } else if (in_rec && !in_decl) {
                    Au_t m = find_member(rec_top->au, alpha->chars, 0, 0, false);
                    if (m) mem = (enode)u(etype, m);
                    else   mem = (enode)rlookup((aether)a, alpha);
                }

                if (!mem && scope_mdl) {
                    Au_t sm = find_member(scope_mdl->au, alpha->chars, 0, 0, true);
                    if (sm)
                        mem = access((enode)scope_mdl, alpha, true);
                }

                // type name followed by : is a new declaration, not a type reference
                if (mem && mem->au->member_type == AU_MEMBER_TYPE && next_is(a, ":"))
                    mem = null;

                if (!mem) {
                    token tm1 = element(a, -2); // sorry for the mess (coin-flip)

                    validate(next_is(a, ":") || (tm1 && index_of(keywords, (Au)tm1) >= 0), "unknown identifier %o", alpha);
                    validate(!find_member(top, alpha->chars, 0, 0, false), "duplicate member: %o", alpha);
                    Au_t m = def_member(top, alpha->chars, null, AU_MEMBER_DECL, 0); // this is promoted to different sorts of members based on syntax
                    mem = (enode)edecl(mod, (aether)a, au, m);
                    break;
                }
                    
            } else if (instanceof(mem, enode) && !is_loaded((Au)mem)) {
                // Subsequent iterations - access from previous member
                verify(mem && mem->au, "cannot resolve from null member");
                
                // Load previous member to traverse into it
                enode prop = !is_struct(canonical(mem)) ? e_load(a, mem, null) : mem;
                mem = access(prop, alpha, true);
            } else {
                Au info = head(mem);
                mem = access(mem, alpha, true);
            }

            Au_t mem_type = isa(mem);
            bool b0, b1, b2, b3, b4;
            if (seq == 438) {
                seq = seq;
            }

            // setter intercept
            if (next_is(a, "[") && in_decl != typeid(efunc) && in_decl != typeid(macro)) {
                Au_t au_rec = is_rec((Au)mem);
                etype r = au_rec ? u(etype, au_rec) : null;
                Au_t setter = r ? find_member(r->au, "setter", AU_MEMBER_SETTER, 0, true) : null;
                if (setter) {
                    push_current(a);
                    array index_keys = read_within(a);
                    token k = element(a, 0);
                    num assign_index = k ? index_of(assign, (Au)k) : -1;
                    bool use_setter = assign_index >= 0;
                    pop_tokens(a, use_setter);
                    if (use_setter) {
                        a->setter_key_tokens = hold(index_keys);
                        a->setter_fn  = setter;
                        break;
                    }
                }
            }
            if (in_decl != typeid(efunc) &&
                in_decl != typeid(macro) &&
                (next_is(a, "[") || instanceof(mem, macro) || (b0=is_func((Au)mem)) || inherits(mem->au->src, typeid(lambda)))) {
                
                token p0 = peek(a);
                // pointers to functions require a ref for actual func's, where as func-ptr do not (thats a reference to the memory for it)
                if (a->expr_level > 0 && !next_is(a, "[") && ((in_ref && is_func((Au)mem)) || (!in_ref && is_func_ptr((Au)mem)))) {
                    // we are returning the function-pointer, the mem->value direct
                    mem = enode_value(mem, false);
                } else {
                    array prev = array(alloc, 32);
                    for (int i = 0; i < depth; i++) {
                        etype mm = u(etype, top_scope(a));
                        push(prev, (Au)mm);
                        pop_scope(a);
                    }
                    prev = reverse(prev);
                    mem = parse_member_expr(a, mem);
                    // inside this expression, we must have the previous scope; 
                    // we could save it at top and push the top again, but that isnt handled \properly
                    for (int i = 0; i < depth; i++) {
                        etype mm = (etype)get(prev, i);
                        push_scope(a, (Au)mm);
                    }
                }
            }
        }

        // check if there's more chaining
        bool br = read_if(a, ".") == null;
        if (br) {
            //if (in_ref)
                break;

            // final load if needed [ assign_type, when set, indicates a L-hand side parse ]
            if (instanceof(mem, enode) && !is_loaded((Au)mem) && !assign_type) {
                mem = enode_value(mem, false); // validate the LLVMValueRef we have for these; it should be the memory location (so in effect we have a struct* used as target)
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

enode block_builder(silver, array, Au);

enode parse_sub(silver a, etype rtype) {
    array body = read_body(a);
    validate(len(body), "expected body for sub-routine");

    // e_sub in aether creates merge block, catcher,
    // parses body via builder callback,
    // builds phi from all return points
    enode r_last = a->last_sub_return;
    enode b_last = a->last_break;
    subprocedure build_body = subproc(a, block_builder, null);
    enode res = e_subroutine(a, rtype, body, build_body);
    validate(r_last != a->last_sub_return || b_last != a->last_break,
        "all paths require return/break in sub-routine");

    // store body tokens on the result for callable re-invocation
    res->body = (tokens)hold(body);
    return res;
}

// re-invoke a callable sub — pushes stored body tokens back and re-evaluates
enode invoke_sub(silver a, enode sub) {
    verify(sub->body, "sub has no stored body for re-invocation");
    array body = (array)sub->body;
    etype rtype = canonical(sub);

    enode r_last = a->last_sub_return;
    enode b_last = a->last_break;
    subprocedure build_body = subproc(a, block_builder, null);
    enode res = e_subroutine(a, rtype, body, build_body);
    validate(r_last != a->last_sub_return || b_last != a->last_break,
        "all paths require return/break in sub-routine");
    return res;
}

etype shape_pointer(silver, Au, enode);

enode silver_read_enode(silver a, etype mdl_expect, bool from_ref, bool load) { sequencer
    bool      cmode     = is_cmode(a);
    array     expr      = null;
    token     peek      = peek(a);
    bool      is_expr0  = !cmode && a->expr_level == 0;
    bool      is_static = is_expr0 && read_if(a, "static") != null;
    string    kw        = is_expr0 ? peek_keyword(a) : null;
    etype     rec_ctx   = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    Au_t      top       = top_scope(a);
    etype     rec_top   = (!cmode && is_rec(top)) ? u(etype, top) : null;
    efunc     f         = !cmode ? context_func(a) : null;
    silver    module    = !cmode && (top->is_namespace) ? a : null;
    enode     mem       = null;

    if (!cmode && read_if(a, "[")) {
        // C fixed-size array: read N elements of the element type
        if (mdl_expect && mdl_expect->au->elements > 0 && mdl_expect->au->src) {
            etype elem_type = u(etype, mdl_expect->au->src);
            if (!elem_type) elem_type = (etype)etype_prep((silver)a, mdl_expect->au->src);
            array elems = array(alloc, mdl_expect->au->elements);
            while (!next_is(a, "]")) {
                enode elem = parse_expression(a, elem_type, true, true);
                push(elems, (Au)elem);
                if (!read_if(a, ",")) break;
            }
            validate(read_if(a, "]"), "expected ] after array elements");
            return e_create(a, mdl_expect, (Au)elems);
        }
        enode n = parse_expression(a, mdl_expect, false, true);
        validate(n, "could not read expression");
        validate(read_if(a, "]"),
            "expected ] after %o expression %i", u(etype, n->au->src), seq);
        return n;
    }

    // handle typed operations, converting to our expected model (if no difference, it passes through)
    if (a->expr_level > 0 && peek && is_alpha(peek)) {
        etype mdl_found = read_etype(a, null);
        if (mdl_found) {
            // here we must 'peek' at a body; which if not available we go default
            array b = peek_initializer(a);
            enode res0 = null;
            if (from_ref) mdl_found = pointer((aether)a, (Au)mdl_found);
            if (b) {
                array expr = read_initializer(a);
                if (!len(expr) && read_if(a, "sub")) {
                    res0 = e_create(a, mdl_expect, (Au)parse_sub(a, mdl_found)); 
                } else if (!len(expr) && read_if(a, "asm")) {
                    res0 = e_create(a, mdl_expect, (Au)parse_asm(a, mdl_found));
                } else if (expr) {
                    push_tokens(a, (tokens)expr, 0);
                    if (next_is(a, "[") && is_rec(mdl_found))
                        res0 = parse_object(a, mdl_found, false);
                    else
                        res0 = read_enode(a, mdl_found, false, load);

                    pop_tokens(a, false);
                } else {
                    res0 = null; // use default
                    if (!mdl_expect) {
                        mdl_expect = mdl_found; // not expecting anything, no conversion
                    } else if (mdl_expect != mdl_found) {
                        res0 = e_create(a, mdl_found, (Au)null); // required conversion
                    }
                }
            } else if (a->assign_type == OPType__bind &&
                    (from_ref || is_class(mdl_found) || is_ptr(mdl_found))) {
                res0 = e_null(a, mdl_found);
            } else {
                res0 = e_create(a, mdl_found, null);
            }
            enode conv = e_create(a, mdl_expect, (Au)res0);
            return conv;
        }
    }

    shape sh = (shape)read_literal(a, typeid(shape));
    if (sh && (sh->count == 1 || sh->explicit)) {
        enode op;
        if (sh->explicit || mdl_expect == etypeid(shape)) 
            op = e_create(a, etypeid(shape), (Au)sh);
        else
            op = e_operand(a, _i64(sh->data[0]), mdl_expect ? mdl_expect : etypeid(i64));
        
        return op; // otherwise interpreted as an i64
    }

    Au lit = read_literal(a, null);
    if (lit) {
        a->expr_level++;
        enode res = e_operand(a, lit, mdl_expect);
        a->expr_level--;

        // scalar suffix: 200px, 1.5em, 90deg — number immediately followed by type name
        if (!cmode && next_is_neighbor(a) && peek(a)) {
            string suffix = peek_alpha(a);
            if (suffix) {
                etype scalar_type = rlookup((aether)a, suffix);
                if (scalar_type && scalar_type->au->is_struct) {
                    consume(a); // consume the suffix token
                    return e_create(a, scalar_type, (Au)res);
                }
            }
        }

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

    // functional macros are only useful for these few built-in's outside of vtable stuff
    // theres no reason to implement actual macro keyword in silver until its simply a 'better solution in general'
    if (read_if(a, "min")) {
        verify(read_if(a, "["), "expected [ after min");
        enode val_a = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode val_b = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_min(a, val_a, val_b);
    }

    if (read_if(a, "max")) {
        verify(read_if(a, "["), "expected [ after max");
        enode val_a = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode val_b = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_max(a, val_a, val_b);
    }

    if (read_if(a, "clamp")) {
        verify(read_if(a, "["), "expected [ after clamp");
        enode val = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode lo  = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode hi  = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_clamp(a, val, lo, hi);
    }

    if (!cmode && read_if(a, "typeid")) {
        bool read_br = read_if(a, "[") != null;
        etype mdl = read_etype(a, null);
        
        if (read_br)
            verify(read_if(a, "]"), "expected closing-bracket after typeof");
        
        return e_create(a, (etype)mdl_expect, (Au)e_typeid(a, mdl));
    }    

    if (!cmode && read_if(a, "new")) {
        etype mdl = read_etype(a, null);
        enode esize = null;
        shape sh  = null;
        if (read_if(a, "[")) {
            esize = parse_expression(a, etypeid(shape), false, true);
            sh = instanceof(esize->literal, shape);
            validate(read_if(a, "]"), "expected closing-bracket after new Type [");
        }
        etype  ptr_type = (etype)shape_pointer(a, (Au)mdl->au, esize);
        enode  vec      = e_vector(a, mdl, esize);
 
        /// parse optional constant data: new i32[4x4] [ 1 2 3 4, 1 1 1 1, ... ]
        if (read_if(a, "[")) {
            int   top_stride = (sh && sh->count > 1) ? sh->data[sh->count - 1] : 0;
            int   num_index  = 0;
            array nodes      = array(64);
 
            while (peek(a) && !next_is(a, "]")) {
                enode e = read_enode(a, mdl, false, true);
                e = e_create(a, mdl, (Au)e);
                push(nodes, (Au)e);
                num_index++;
                if (top_stride && (num_index % top_stride == 0)) {
                    validate(read_if(a, ",") || next_is(a, "]"),
                        "expected ',' between rows (stride: %i)", top_stride);
                }
            }
            validate(read_if(a, "]"), "expected ] after constant data");
 
            /// copy constant data into allocated vector
            if (len(nodes) > 0)
                e_vector_init(a, mdl, vec, nodes);
        }
        return e_create(a, ptr_type, (Au)vec);
    }

    if (!cmode && read_if(a, "null"))
        return e_null(a, mdl_expect);

    if (cmode && read_if(a, "*")) {
        enode n = read_enode(a, null, false, true);
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
                    enode res = e_create(a, inner, (Au)parse_expression(a, inner, false, true));
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
        enode expr = parse_expression(a, null, false, true); // Parse the expression
        validate(read_if(a, ")"), "expected ) after expression, found %o", peek(a));
        a->parens_depth--;
        return e_create(a, mdl_expect, (Au)
            parse_ternary(a, (enode)expr, (etype)mdl_expect, load));
    }

    // { keyword tokens } — compacted token literals
    if (!cmode && next_is(a, "{") && mdl_expect && mdl_expect->au == typeid(tokens)->src) {
        return read_keywords(a);
    }

    if (!cmode && next_is(a, "[")) {
        validate(mdl_expect, "expected model name before [");
        array expr = read_within(a);
        // we need to get mdl from argument
        enode r = typed_expr(a, (enode)mdl_expect, (array)expr);
        return r;
    }

    // expect: inline verify with debug break on failure
    else if (!cmode && read_if(a, "expect")) {
        enode cond = read_enode(a, null, false, true);
        return e_expect(a, cond, null);
    }

    // fault: unconditional abort with message
    else if (!cmode && read_if(a, "fault")) {
        enode msg = read_enode(a, etypeid(string), false, true);
        return e_fault(a, msg);
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read_if(a, "!") || (!cmode && read_if(a, "not"))) {
        validate(!from_ref, "unexpected not after ref");
        token t = peek(a);
        enode expr = read_enode(a, null, false, true); // Parse the following expression
        return e_create(a,
            mdl_expect, (Au)e_not(a, expr));
    }

    // bitwise NOT operator
    else if (read_if(a, "~")) {
        validate(!from_ref, "unexpected ~ after ref");
        enode expr = read_enode(a, null, false, true);
        return e_create(a,
            mdl_expect, (Au)e_bitwise_not(a, expr));
    }

    // unary negation
    else if (read_if(a, "-")) {
        enode expr = read_enode(a, null, false, true);
        validate(canonical(expr)->au->is_integral || canonical(expr)->au->is_realistic,
            "negation requires numeric type");
        return e_create(a,
            mdl_expect, (Au)e_neg(a, expr));
    }

    // 'ref' operator (reference)
    // we only allow one reference depth, for multiple we would resort to using defined types
    // this is to make code cleaner, with more explicit definition
    // we must use in-ref state only at neighboring calls
    // silver doesnt really want to tell you how to code, 
    // it is reduced in nature, to define line by line rather than add horizontal inline features
    // its less surface area for complexity

    else if (!cmode && read_if(a, "ref")) {
        static int seq2;
        seq2++;
        if (seq2 == 25) {
            seq2 = seq2;
        }
        validate(!from_ref, "unexpected double-ref (use type definitions)");

        // peek ahead: if next tokens form a type, this is a cast (ref u8 vdata)
        // otherwise it's address-of (ref vdata)
        push_current(a);
        etype ref_cast_type = read_etype(a, null);
        pop_tokens(a, false);

        if (ref_cast_type && peek(a)) {
            // ref type expr — cast expr to ref type
            etype cast_type = read_etype(a, null);
            etype ref_type = pointer((aether)a, (Au)cast_type);
            enode expr = read_enode(a, null, false, true);
            return e_create(a, ref_type, (Au)expr);
        }

        // ref expr — take address of expr
        enode expr = read_enode(a, null, true, false);

        if (next_is(a, "[")) //  || next_neighbor(a) <- breaks with comma of course; we have to form syntax differently here and use Multiple Lines.
            expr = parse_member_expr(a, expr);

        etype ref_type = pointer((aether)a, (Au)expr->au);

        // when expr is unloaded, its value is already the address (GEP) —
        // return it directly as a loaded pointer rather than going through
        // e_create which would load and inttoptr the dereferenced value
        if (expr->loaded) {
            expr->loaded = expr->loaded;
        }
        validate(!expr->loaded, "cannot take ref of loaded value");
        enode ref_node = enode_ref((aether)a, expr, ref_type);
        return mdl_expect ? e_create(a, mdl_expect, (Au)ref_node) : ref_node;
    }

    // we may only support a limited set of C functionality for #define macros
    mem = parse_member(a, null, null, mdl_expect, from_ref); // we never parse assignment here

    if (!mem && cmode) return null;
    if (!mem) {
        etype unexpected_type = read_etype(a, null);
        validate(!unexpected_type, "unexpected type %o", unexpected_type);
    }

    validate(!instanceof(mem, edecl), "unexpected declaration of member %s", mem->au->ident);
    if (load && !is_loaded((Au)mem))
        mem = enode_value(mem, false);
    Au info = head(mem);
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
        a->no_build = true;
        enode n = parse_expression(a, null, false, true);
        a->no_build = false;
        if (n) t = (etype)n;
        pop_tokens(a, false);
    }
    return t;
}

static tokens map_initializer(silver a, string field, tokens def_tokens, interface access) {
    if (access == interface_intern || !a->defs || !contains(a->defs, (Au)field))
        return def_tokens;
    tokens value = (tokens)get(a->defs, (Au)field);
    if (value) {
        set(a->defs_used, (Au)field, _bool(true));
    }
    return value;
}

enode parse_statement(silver a)
{
    sequencer
#ifndef NDEBUG
    if (a->verbose) {
        token tk = peek(a);
        if (tk) {
            num line = tk->line;
            fprintf(stderr, "[%4lld] ", line);
            for (int s = 0; s < tk->indent; s++) fputc(' ', stderr);
            for (int i = a->cursor; i < len(a->tokens); i++) {
                token t = (token)a->tokens->origin[i];
                if (t->line != line) break;
                fprintf(stderr, "%s ", t->chars);
            }
            fprintf(stderr, "\n");
        }
    }
#endif
    efunc f = context_func(a);
    verify(!f || !f->au->is_mod_init, "unexpected init function");
    a->last_return      = null;
    a->expr_level       = 0;
    a->assign_type      = OPType__undefined;
    a->setter_key_tokens = null;
    a->setter_fn        = null;
    a->statement_origin = peek(a);
    if (f && a->statement_origin && !a->no_build)
        emit_debug_loc((aether)a, a->statement_origin->line, a->statement_origin->column);
    Au_t      top       = top_scope(a);
    silver    module    = is_module(top) ? a : null;
    etype     rec_top   = is_rec(top) ? u(etype, top) : null;

    // standard statements first, only in context of functions
    if (f) {
        if (read_if(a, "no-op"))  return e_noop(a, null);
        if (next_is(a, "return")) return parse_return (a);
        if (next_is(a, "break"))    return parse_break   (a);
        if (next_is(a, "continue")) return parse_continue(a);
        if (next_is(a, "expect")) {
            push_current(a);
            consume(a);
            string pk = peek_alpha(a);
            token pk2 = pk ? element(a, 1) : null;
            bool is_bind = pk && pk2 && index_of(assign, (Au)pk2) >= 0;
            pop_tokens(a, true);
            if (is_bind) {
                read_if(a, "expect");
                a->expect_state = true;
            } else {
                read_if(a, "expect");
                enode cond = parse_expression(a, null, false, true);
                enode msg  = read_if(a, ",") ? parse_expression(a, etypeid(string), false, true) : null;
                return e_expect(a, cond, msg);
            }
        }
        if (next_is(a, "for"))    return parse_for    (a);
        if (next_is(a, "if"))     return parse_if_else(a);
        if (next_is(a, "switch")) return parse_switch (a);
        if (read_if(a, "asm"))    return parse_asm    (a, null);
        if (next_is(a, "memcpy") || next_is(a, "memset")) return parse_memop(a);
    }
    
    if (next_is(a, "ifdef")) return parse_ifdef_else(a);

    verify(!next_is(a, "undefined"), "undefined is invalid access-level");

    

    u64 traits = 0;
    interface access = interface_undefined;

    if (rec_top && next_is(a, "flux")) { // adds more capacity to objects
        access = interface_public;
        traits = AU_TRAIT_IS_FLUX;
    } else if (rec_top && next_is(a, "context")) {
        access = interface_public;
        traits = AU_TRAIT_IS_CONTEXT;
    } else
        access = read_enum(a, interface_undefined, typeid(interface));

    //bool is_default = !access ? read_if(a, "default") != null : false;
    bool has_access = access != interface_undefined;

    //print_tokens(a, seq);

    if (module && !next_is(a, "func") && peek_def(a)) {
        verify(!has_access || (access == interface_public || access == interface_intern),
            "undefined is invalid access-level");

        return (enode)read_def(a, access);
    }

    if (access == interface_context)
        access = access;
    
    bool is_static = read_if(a, "static") != null;

    validate(!is_struct(top) || (!access || access == interface_public),
        "unexpected access level found in struct");

    // not yet sold on needing override; its less arguments but you can do that with func init, too
    // also override would need to require the cast and operator
    // no args would mean override.. thats not difficult to implement..

    //bool      is_override = !f ?
    //    read_if(a, "override")   != null : false;
    token entry = peek(a);
    bool      is_lambda = !f ?
        read_if(a, "lambda")     != null : false;
    bool      is_func   = !f && !is_lambda ?
        read_if(a, "func")       != null : false;
    bool      is_cast   = !f && !is_static && !(is_func|is_lambda) ?
        read_if(a, "cast")       != null : false;
    bool      is_oper   = !f && !is_static && !(is_func|is_lambda) && !is_cast ?
        read_if(a, "operator")   != null : false;
    bool      is_left   = is_oper ? read_if(a, "left") != null : false;
    bool      is_ctr    = !f && !is_static && !(is_func|is_lambda) && !is_cast && !is_oper ?
        read_if(a, "construct")  != null : false;
    bool      is_getter = !f && !is_static && !(is_func|is_lambda) && !is_cast && !is_oper && !is_ctr ?
        read_if(a, "getter")     != null : false;
    bool      is_setter = !f && !is_static && !(is_func|is_lambda) && !is_cast && !is_oper && !is_ctr && !is_getter ?
        read_if(a, "setter")     != null : false;

    if (seq == 609) {
        int test2 = 2;
        test2    += 2;
    }
    OPType assign_enum = OPType__undefined;
    enode mem = (!is_cast && !is_oper && !is_getter && !is_setter && !is_ctr) ?
        parse_member(a, (ARef)&assign_enum,
            is_func ? typeid(efunc) : ((access || f || (!!module)) ? typeid(evar) : null), null, false) : null;
    Au_t mem_info = isa(mem);

    if (mem && mem->au->ident && strcmp(mem->au->ident, "rng_state2") == 0) {
        seq = seq;
    }

    validate(!mem || (!is_func || !instanceof(mem, efunc) || mem->au->context != top),
        "redefinition of %o", mem);

    if (module && mem &&
            mem->au->member_type == AU_MEMBER_VAR &&
            mem->au->access_type == interface_expect) {
        set(a->defs_expect, string(mem->au->ident), true);
    }

    validate(!(is_lambda|is_func) || mem,
        "expected member identifier to follow function or lambda");

    validate(!is_static || mem,
        "expected member identifier to follow static");

    validate(mem || (!mem || access == interface_undefined),
        "expected member-name after access '%o'", estring(typeid(interface), access));

    if (access && mem && mem->au) {
        if (access == interface_abstract) {
            mem->au->is_abstract = true;
            mem->au->access_type = interface_public;
        } else {
            mem->au->access_type = (u8)access;
        }
        if (access == interface_expect || !!(traits & AU_TRAIT_IS_CONTEXT))
            mem->au->is_required = true;
        mem->au->is_context = access == interface_context;
    }

    if (mem)
        mem->au->traits |= traits;

    //if (is_default && mem && mem->au) {
    //    mem->au->is_default  = true;
    //    mem->au->is_required = true;
    //}

    // if no access then full access
    if (!access) access = interface_public;

    push_current(a);

    string op_name = null;
    OPType op_type = is_oper ? read_operator(a, (ARef)&op_name) : OPType__undefined;
    enode  e       = null;

    if (is_oper && is_left && op_type == OPType__mul) op_type = OPType__lmul;
    if (is_oper && is_left && op_type == OPType__div) op_type = OPType__ldiv;
    
    if (is_oper && is_left && op_type == OPType__left)  op_type = OPType__lleft;
    if (is_oper && is_left && op_type == OPType__right) op_type = OPType__lright;

    if (mem!=null || is_ctr || is_getter || is_setter || is_oper || is_lambda || is_func || is_cast)
    {
        validate(!is_oper || op_type != OPType__undefined, "operator required");
        
        // check if this is a nested, static member (we need to back off and read_enode can handle this)
        Au_t top_type = isa(u(etype, top));

        
        statements in_code    = context_code(a);
        bool       is_const   = mem && mem->au->is_const;

        if (is_func|is_lambda|is_ctr|is_getter|is_setter|is_cast|is_oper) {
            if (module || rec_top) {
                u64 traits = (is_static ? AU_TRAIT_STATIC :
                             (is_lambda ? 0 : (rec_top ? AU_TRAIT_IMETHOD : 0))) |
                             (is_lambda ? AU_TRAIT_LAMBDA : 0);
                Au_t au = mem ? mem->au : null;
                enum AU_MEMBER ftype = is_lambda ? AU_MEMBER_FUNC      :
                    is_ctr    ? AU_MEMBER_CONSTRUCT : is_cast ?
                                AU_MEMBER_CAST      : is_getter ?
                                AU_MEMBER_GETTER    : is_setter ?
                                AU_MEMBER_SETTER    : AU_MEMBER_FUNC;

                Au_t top = top_scope(a);
                aether top_user;

                if (!au)
                     au = def(top_scope(a), null, AU_MEMBER_DECL, 0);
                
                etype_register((aether)a, (Au)au, (Au)null, true);
                e = (enode)parse_func(a, au, // for cast, we read the rtype first; for others, its parsed after ->
                    ftype,
                    traits, op_type, op_name);
                ((efunc)e)->origin_token = entry;
                e->au->access_type = (u8)access;
                efunc fn = (efunc)get(a->registry, (Au)au);
                verify(fn && fn == e && fn->au == au, "unexpected registration state");
            }

        }
        else if (rec_top || module) {

            bool is_f = is_getter|is_ctr|is_lambda|is_func|is_cast;
            verify(assign_enum == OPType__bind || is_f, "invalid member syntax, expected member:type[ initializer ]");
            if (seq == 438)
                seq = seq;
            etype rtype = (mem && mem->au->member_type == AU_MEMBER_DECL) && !is_f ?
                            read_etype(a, null) : null;
            bool  is_const = false;
            a->expr_level++;
            array expr = rtype ? read_initializer(a) : read_expression(a, (etype*)&rtype, &is_const);
            a->expr_level--;

            validate(rtype, "could not infer type");

            mem->au->access_type = (u8)access;
            mem->au->member_type = AU_MEMBER_VAR;
            mem->au->src         = canonical(rtype)->au;
            mem->au->is_static   = is_static;
            if (rtype->meta_a) {
                mem->au->meta.a = (Au_t)rtype->meta_a;
            }
            if (rtype->meta_b) {
                mem->au->meta.b = instanceof(rtype->meta_b, Au_t) ?
                    (Au)rtype->meta_b : (Au)hold(rtype->meta_b);
            }

            Au_t au = mem->au;
            etype_register((aether)a, (Au)au, null, true);
            mem = (enode)evar(mod, (aether)a, au, au, loaded, false,
                meta_a, rtype->meta_a, meta_b, rtype->meta_b,
                initializer, (tokens)map_initializer(a, string(au->ident), (tokens)expr, au->access_type));
            
            e = (enode)mem;
            etype_register((aether)a, (Au)au, (Au)mem, true);

            efunc fn = (efunc)get(a->registry, (Au)e->au);
            verify(fn && fn == e && fn->au == mem->au, "unexpected registration state");

            //au_register(e->au, (etype)e);
            if (is_static || module) {
                if (is_static && rec_top) {
                    verify(rec_top, "invalid use of static (must be a class member, not a global item -- use intern for module-interns)");
                    mem->au->alt = (cstr)cstr_copy((cstr)((string)(f(string, "%o_%o", symbol_name((Au)rec_top), e))->chars));
                }
                etype_implement((etype)e, false);
            }

        } else if (a->setter_key_tokens && a->setter_fn) {
            validate(mem, "expected member for setter");
            if (seq == 544)
                seq = seq;
            a->expr_level++;
            a->assign_type = assign_enum;
            e = parse_assignment(a, (enode)mem, assign_enum, false);
            a->expr_level--;
            a->assign_type = OPType__undefined;

        } else if (assign_enum) {
            validate(mem, "expected member (%o)", peek(a));

            a->left_hand = false;
            a->expr_level++;
            a->assign_type = assign_enum;
            mem->au->is_const = module != null;
            validate (!is_func(mem->au->context), "function arguments are read-only");

            e = parse_assignment(a, (enode)mem, assign_enum, mem->au->is_const);
            a->expr_level--;
            a->assign_type = OPType__undefined;
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
        e = parse_expression(a, null, false, true); /// at module level, supports keywords
        a->left_hand = false;
    }
    if (a->expect_state && e) {
        a->expect_state = false;
        enode msg = read_if(a, ",") ? read_enode(a, etypeid(string), false, true) : null;
        e_expect((aether)a, e, msg);
    }
    return e;
}

void aether_emit_block_probe(silver, i32);

enode parse_statements(silver a) { sequencer
    statements st = new(statements, mod, (aether)a, au, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_scope(a, (Au)st);
    if (seq == 99)
        seq = seq;
    enode vr = null;
    while (peek(a)) {
        Au_t mod2 = (Au_t)0x7ffff7eb7c30;
        if (mod2) {
            mod2 = mod2;
        }
        vr = parse_statement(a);
    }
    pop_scope(a);
    return vr;
}

void silver_incremental_resolve(silver a) {
    // type implementation and function building are deferred to after parsing
}


ARef lltype(Au a);

Au_t alloc_arg(Au_t context, symbol ident, Au_t arg);

static none next_function_index_update(Au_t mdl, int* index) {
    if (!mdl) return;
    if (mdl->context != mdl || !mdl->context)
        next_function_index_update(mdl->context, index);

    for (int i = 0; i < mdl->members.count; i++) {
        Au_t au = (Au_t)mdl->members.origin[i];
        if (au->is_smethod || au->is_static || au->is_override) continue;
        if (au->member_type == AU_MEMBER_FUNC       || 
            au->member_type == AU_MEMBER_OPERATOR   ||
            au->member_type == AU_MEMBER_GETTER      ||
            au->member_type == AU_MEMBER_SETTER     ||
            au->member_type == AU_MEMBER_CAST       ||
            au->member_type == AU_MEMBER_CONSTRUCT) { 
            (*index)++;
        }
    }
}

static int next_function_index(Au_t mdl) {
    if (!mdl) return 0;
    int index = 0;
    next_function_index_update(mdl, &index);
    return index;
}


#undef find_member

efunc parse_func(silver a, Au_t mem, enum AU_MEMBER member_type, u64 traits, OPType op_type, string op_name) {
    sequencer
    if (mem->ident && strcmp(mem->ident, "init") == 0) {
        mem = mem;
    }
    if (member_type == AU_MEMBER_CONSTRUCT) {
        seq = seq;
    }
    etype  rtype   = null;
    string name    = string(mem->ident);
    bool   is_cast = member_type == AU_MEMBER_CAST;
    //etype rec_ctx = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    etype rec_ctx = context_class(a);
    if (!rec_ctx) rec_ctx = context_struct(a);

    validate(member_type == AU_MEMBER_CAST || read_if(a, "["), "expected function args [");
    Au_t au = mem; //def(top_scope(a), ident ? ident->chars : null, AU_MEMBER_FUNC, traits);
    verify(mem->member_type == AU_MEMBER_DECL, "already defined: %o", mem); // since we allow for prop-style invocation of functions, the design must be no clashing with var names
    
    au->member_type = member_type;
    au->operator_type = op_type;
    au->traits = traits;
    if (au->module && au->module != a->au) {
        fprintf(stderr, "MODULE OVERWRITE [silver_read_function]: %s (%p) module %p -> %p\n", au->ident, au, au->module, a->au);
        exit(1);
    }
    au->module = a->au;

    Au_t override = null;
    if (!rec_ctx)
        rec_ctx = context_struct(a);
    else {
        override = find_member(
            rec_ctx->au->context, name->chars,
            member_type, 0, true);

        au->is_override = override != null;

        if (strcmp(rec_ctx->au->ident, "conv") == 0 && strcmp(mem->ident, "init") == 0) {
            mem = mem;
        }

        if (au->is_override) {
            au->index = override->index;
        } else {
            au->index = next_function_index(rec_ctx->au);
        }
    }

    // fill out args in function model
    bool is_instance = (traits & AU_TRAIT_IMETHOD) != 0 ||
        (member_type == AU_MEMBER_CAST) ||
        (member_type == AU_MEMBER_GETTER) ||
        (member_type == AU_MEMBER_SETTER);
    if (is_instance) {
        Au_t top    = top_scope(a);
        Au_t rec    = is_rec(top);
        verify(rec, "cannot parse IMETHOD without record in scope");
        Au_t au_arg = alloc_arg(au, "a", rec);
        au_arg->is_target = true;
        micro_push(&au->args, (Au)au_arg);
    }

    bool is_lambda = (traits & AU_TRAIT_LAMBDA) != 0;
    bool in_context = false;

    // create model entries for the args (enodes created on func init)
    push_scope(a, (Au)mem);
    bool first = true;
    Au_t target = null;

    // parse args (move to generic)
    for (; member_type != AU_MEMBER_CAST ;) {
        if (read_if(a, "]"))
            break;
        
        bool skip = false;
        if (!first && is_lambda && read_if(a, "::")) {
            skip = true;
            in_context = true;
        }
        validate(skip || first || read_if(a, ","), "expected comma separator between arguments %i", seq);
        
        bool    is_inlay  = read_if(a, "inlay") != null;    push_current(a);
        etype   t = read_etype(a, null);            pop_tokens(a, t != null);
        string  n         = t ? null : read_alpha(a); // optional
        micro*  ar        = in_context ? (micro*)&au->members : (micro*)&au->args;

        if (!t)
            validate(read_if(a, ":"),
                "expected seperator between name and type %i", seq);
        else
            validate(!read_if(a, ":"),
                "unexpected : after type provided first: %o", t);

        // verify arg name does not shadow a class member
        if (n) {
            etype rec = context_record(a);
            if (rec) {
                Au_t found = find_member(rec->au, cstring(n), AU_MEMBER_VAR, 0, false);
                validate(!found, "argument '%o' shadows member of %s", n, rec->au->ident);
            }
        }
        
        bool is_ref = read_if(a, "ref") != null;
        if (!t) t = read_etype(a, null); // we need to avoid the literal check in here!
        if (is_ref) 
        verify(t, "expected alpha-numeric identity for type or name");
        Au_t arg = alloc_arg(au, n ? n->chars : null, t->au);
        arg->is_inlay = is_inlay;
        arg->is_explicit_ref = t->is_explicit_ref;

        if (member_type == AU_MEMBER_CONSTRUCT && !len(name))
            name = form(string, "with_%s%o", arg->is_explicit_ref ? "ref_" : "", t);
        else if (member_type == AU_MEMBER_CAST && !len(name))
            name = form(string, "cast_%o", t);
        
        if (is_inlay) {
            validate(is_struct(arg->src),
                "inlay applies only to struct members in arguments");
        }
        micro_push(ar, (Au)arg);
        if (first)
            first = false;
    }
    pop_scope(a);

    if (op_name)
        name = op_name;
    
    bool arrow = read_if(a, "->") != null;
    rtype = arrow ? read_etype(a, null) : null;
    array inline_expr = null;

    if (next_is(a, "[")) {
        inline_expr = read_body(a);
        inline_expr = inline_expr;
    }

    if (member_type == AU_MEMBER_CAST) {
        validate(rtype, "expected explicit type for cast");
        name = f(string, "cast_%o", rtype);
    } else if (member_type == AU_MEMBER_GETTER) {
        validate(rtype, "expected explicit type for index");
        name = f(string, "index_%o", rtype);
    } else if (member_type == AU_MEMBER_SETTER) {
        if (!rtype) rtype = etypeid(none);
        if (!name || !len(name))
            name = string("setter");
    } else if (!rtype)
        rtype = etypeid(none); // functional programmers would want to return target type

    validate(len(name), "could not bind name for function");

    string fname = rec_ctx ? f(string, "%o_%o", rec_ctx, name) : (string)name;
    au->alt     = rec_ctx ? cstr_copy(symbol_name((Au)fname)->chars) : null;
    au->ident   = cstr_copy(name->chars); // free other instance
    au->rtype   = rtype->au;

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

    array b = inline_expr ? inline_expr : (array)read_body(a);
    // all instances of func enode need special handling to bind the unique user space to it; or, we could make efunc

    efunc func = efunc(
        mod,    (aether)a,
        au,     au,
        body,   (tokens)b,
        inline_return, inline_expr,
        remote_code, !is_using && !len(b),
        has_code,    len(b) || is_init || is_dealloc || cgen,
        cgen,   cgen,
        used,   true,
        target, null);
    
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

etype read_etype(silver a, array* p_expr) { sequencer
    etype mdl   = null;
    array expr  = null;
    
    token f = peek(a);
    if (!f || f->literal) return null;

    push_current(a);
    bool is_ref = read_if(a, "ref") != null;
    bool  explicit_sign = !mdl && read_if(a, "signed") != null;
    bool  explicit_un   = !mdl && !explicit_sign && read_if(a, "unsigned") != null;
    etype prim_mdl      = null;

    if (!mdl && !explicit_un) {
        if      (read_if(a, "void"))  prim_mdl = etypeid(none);
        else if (read_if(a, "char"))  prim_mdl = etypeid(i8);
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

        if (mdl) {
            bool has_depth_meta = read_if(a, "<") != null;
            bool read_meta = mdl->au->meta.a && is_class(mdl) &&
                             (has_depth_meta || a->etype_level == 0);

            Au meta_a_val = null;
            Au meta_b_val = null;

            if (read_meta) {
                // meta_a: always a type
                a->etype_level++;
                etype t = read_etype(a, null);
                if (!t) t = etypeid(Au);
                meta_a_val = (Au)t->au;
                a->etype_level--;

                // meta_b: shape or bracketed type, optional
                if (mdl->au->meta.b) {
                    if (mdl->au->meta.b == typeid(shape)) {
                        shape s = read_shape(a);
                        if (s) meta_b_val = (Au)s;
                    } else if (next_is(a, "[")) {
                        read_if(a, "[");
                        a->etype_level++;
                        etype imdl = read_etype(a, null);
                        a->etype_level--;
                        validate(imdl, "expected type for meta_b, found %o", peek(a));
                        meta_b_val = (Au)imdl->au;
                        validate(read_if(a, "]"), "expected ] after meta_b type");
                    }
                }
            }

            validate(!has_depth_meta || read_if(a, ">"), "expected > to close <meta>");

            mdl = meta_a_val ? etype(mod, (aether)a, au, mdl->au,
                is_explicit_ref, is_ref, meta_a, meta_a_val, meta_b, meta_b_val) : mdl;
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

    bool is_any = false;
    if (!is_cmode(a) && mdl && is_class(mdl)) {
        is_any = read_if(a, "*") != null;
        if (is_any) {
            validate(mdl->au->access_type != interface_intern, "polymorphic types cannot be defined internal");
        }
    }

    etype t = (mdl && (is_any || is_ref)) ?
        etype(mod, (aether)a, au,
            is_ref ? pointer((aether)a, (Au)mdl->au)->au : mdl->au,
            is_explicit_ref, is_ref, is_any, is_any) : mdl;

    pop_tokens(a, mdl != null); // if we read a model, we transfer token state
    return t;
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, efunc f, array query) {
    fault("implement generate_fn");
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

array gemini_generate_fn(gemini google, Au_t f, array query) {
    silver a = (silver)u(efunc, f)->mod;
    error("implement gemini");
    return null;
}

array claude_generate_fn(claude jean, Au_t f, array query) {
    silver a = (silver)u(efunc, f)->mod;
    error("implement claude");
    return null;
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
                error("unknown type in dictation: %s", isa(info)->ident);
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
        if (token_line == -1 && !starts_with(t, "-l") && !starts_with(t, "-I")) {
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

string import_libs(array input, map output) {
    string libs = string(alloc, 128);
    each(input, string, t) {
        if (starts_with(t, "-l")) {
            string n = mid(t, 2, len(t) - 2);
            set(output, n, _bool(true));
        }
    }
    return libs;
}

void import_includes(silver a, array input, array output) {
    each(input, string, t) {
        if (starts_with(t, "-I")) {
            string expanded = interpolate(t, (Au)a);
            push(output, (Au)expanded);
        }
    }
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

string command_run(command cmd, bool verbose);

static none checkout(silver a, path uri, string commit, array prebuild, array postbuild, string conf, string env) {
    path    install     = a->install;
    string  s           = cast(string, uri);
    num     sl          = rindex_of(s, "/");
    validate(sl >= 0, "invalid uri");
    string  name        = mid(s, sl + 1, len(s) - sl - 1);
    path    project_f   = f(path, "%o/checkout/%o", a->root_path, name);
    bool    debug       = false;
    string  config      = interpolate(conf, (Au)a);
    string  docker      = a->docker ? f(string, "%o ", a->docker) : string("");

    validate(command_exists("git"), "git required for import feature");

    // checkout or symlink to src
    if (!dir_exists("%o", project_f)) {
        path src_path = f(path, "%o/%o", a->src_loc, name);
        if (dir_exists("%o", src_path)) {
            vexec(a->verbose, "symlink", "ln -s %o %o", src_path, project_f);
            project_f = src_path;
        } else {
            // we need to check if its full hash
            bool is_short = len(commit) == 7 && !is_branchy(commit);

            if (is_short) {
                vexec(a->verbose, "remote", "git clone %o %o", uri, project_f); // FULL CHECKOUTS with short
                vexec(a->verbose, "checkout", "git -C %o checkout %o", project_f, commit);
            } else {
                vexec(a->verbose, "init", "git init %o", project_f);
                vexec(a->verbose, "remote", "git -C %o remote add origin %o", project_f, uri);
                if (!commit) {
                    command c = f(command, "git remote show origin");
                    string res = run(a->verbose, c);
                    verify(starts_with(res, "HEAD branch: "), "unexpected result for git remote show origin");
                    commit = mid(res, 13, len(res) - 13);
                }
                vexec(a->verbose, "fetch", "git -C %o fetch origin %o", project_f, commit);
                vexec(a->verbose, "checkout", "git -C %o reset --hard FETCH_HEAD", project_f);
            }

            // apply module-path diff if one exists (e.g. foundry/vulkan/MoltenVK.diff)
            path diff_f = f(path, "%o/%o.diff", a->module_path, name);
            if (file_exists("%o", diff_f))
                vexec(a->verbose, "patch", "git -C %o apply %o", project_f, diff_f);
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
            command_exec((command)icmd, a->verbose);
        }
        cd(cw);
    }

    if (is_cmake) { // build for cmake
        cstr build = debug ? "Debug" : "Release";
        string opt = a->isysroot ? f(string, "-DCMAKE_OSX_SYSROOT=%o", a->isysroot) : string("");

        vexec(a->verbose, "configure",
              "%o%o cmake -B %o -S %o %o -DCMAKE_INSTALL_PREFIX=%o -DCMAKE_BUILD_TYPE=%s %o",
              docker, env, build_f, project_f, opt, install, build, config);

        vexec(a->verbose, "build", "%o%o cmake --build %o -j16", docker, env, build_f);
        vexec(a->verbose, "install", "%o%o cmake --install %o", docker, env, build_f);
    } else if (is_meson) { // build for meson
        cstr build = debug ? "debug" : "release";

        vexec(a->verbose, "setup",
              "%o%o meson setup %o --prefix=%o --buildtype=%s %o",
              docker, env, build_f, install, build, config);

        vexec(a->verbose, "compile", "%o%o meson compile -C %o", docker, env, build_f);
        vexec(a->verbose, "install", "%o%o meson install -C %o", docker, env, build_f);
    } else if (is_gn) {
        cstr is_debug = debug ? "true" : "false";
        vexec(a->verbose, "gen", "%ogn gen %o --args='is_debug=%s is_official_build=true %o'", docker, build_f, is_debug, config);
        vexec(a->verbose, "ninja", "%oninja -C %o -j8", docker, build_f);
    } else if (is_rust) { // todo: copy bin/lib after
        vexec(a->verbose, "rust", "%ocargo build --%s --manifest-path %o/Cargo.toml --target-dir %o",
              docker, debug ? "debug" : "release", project_f, build_f);
    } else if (is_silver) { // build for Au-type projects
        silver sf = silver(module, silver_f, breakpoint, a->breakpoint, debug, a->debug,
            verbose, a->verbose, is_external, a->is_external ? a->is_external : a);
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
                verify(exec(a->verbose, "%olibtoolize --install --copy --force", docker) == 0, "libtoolize");

            // common preference on these repos
            if (file_exists("%o/autogen.sh", project_f))
                verify(exec(a->verbose, "%o(cd %o && bash autogen.sh)", docker, project_f) == 0, "autogen");

            // generate configuration scripts if available
            else if (!file_exists("%o/configure", project_f) && file_exists("%o/configure.ac", project_f)) {
                verify(exec(a->verbose, "%oautoupdate --verbose --force --output=%o/configure.ac %o/configure.ac",
                            docker, project_f, project_f) == 0,
                       "autoupdate");
                verify(exec(a->verbose, "%oautoreconf -i %o",
                            docker, project_f) == 0,
                       "autoreconf");
            }

            // prefer pre/generated script configure, fallback to config
            path configure = file_exists("%o/configure", project_f) ? f(path, "./configure") : f(path, "./config");

            if (file_exists("%o/%o", project_f, configure)) {
                verify(exec(a->verbose, "%o%o (cd %o && %o%s --prefix=%o %o)",
                            docker, env,
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
            verify(exec(a->verbose, "%o%o (cd %o && make PREFIX=%o -f %o install)", docker, env, project_f, install, Makefile) == 0, "make");
    }

    if (postbuild && len(postbuild)) {
        path cw = path_cwd();
        cd(build_f);
        each(postbuild, string, cmd) {
            string icmd = interpolate(cmd, (Au)a);
            command_exec((command)icmd, a->verbose);
        }
        cd(cw);
    }

    save(token, (Au)config, null);
}

string compile_implements(silver a, array files, string cflags) {
    path   install = a->install;
    string objs    = string();
#ifdef __APPLE__
    cstr   sysroot_flag = "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
#else
    cstr   sysroot_flag = "";
#endif
    each(files, path, i) {
        string i_name   = f(string, "%o/%o.o", a->build_dir, filename(i));
        string ext      = ext(i);
        bool   is_cpp   = eq(ext, "cc") || eq(ext, "cpp");
        cstr   compiler = is_cpp ? "clang++" : "clang";
        cstr   std_flag = is_cpp ? "-std=c++17" : "-std=c11";
        string st       = stem(i);
        verify(exec(a->verbose, "%o/bin/%s %s %s %o %s -c %o -o %o -I%o/include/%o -I%o/include -I%o/include/Au",
            install, compiler, std_flag, sysroot_flag, cflags, a->debug ? "-g" : "", i, i_name, install, st, install, install) == 0,
            "failed to compile %o", i);
        if (len(objs)) append(objs, " ");
        concat(objs, i_name);
    }
    return objs;
}

// recursively deploy resource files from src into dst
// directories merge; duplicate files are an error
// only copies when filesize or mtime differs; preserves original timestamp
static void deploy_resources(path src, path dst) {
    DIR *dir = opendir(src->chars);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        path s = form(path, "%o/%s", src, entry->d_name);
        path d = form(path, "%o/%s", dst, entry->d_name);
        if (entry->d_type == DT_DIR) {
            make_dir(d);
            deploy_resources(s, d);
        } else {
            struct stat ss;
            verify(stat(s->chars, &ss) == 0, "cannot stat resource: %o", s);
            struct stat ds;
            if (stat(d->chars, &ds) == 0) {
                // dest exists: skip if same size and same mtime (already deployed)
                if (ss.st_size == ds.st_size && ss.st_mtime == ds.st_mtime)
                    continue;
                // different size with same mtime = collision from another module
                verify(ss.st_mtime != ds.st_mtime,
                    "resource file collision: %o", d);
            }
            cp(s, d, false, false);
            struct utimbuf ut;
            ut.actime  = ss.st_atime;
            ut.modtime = ss.st_mtime;
            utime(d->chars, &ut);
        }
    }
    closedir(dir);
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
        len(a->defs_hash) ? "-" : "",
        a->defs_hash,
        a->is_library ? lib_ext : "");
    
    if (a->product) drop(a->product);

    a->product = hold(product);

    // platform dispatch: compile and link inside docker for non-native targets
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
        string image = f(string, "silver-platform-%o", a->platform);
        path   plat  = f(path,   "%o/platform/%o", a->install, a->platform);
        path   df    = f(path,   "%o/Dockerfile", plat);

        // build docker image if it doesn't exist
        if (exec(false, "docker image inspect %o >/dev/null 2>&1", image) != 0) {
            verify(file_exists("%o", df), "no Dockerfile for platform: %o", a->platform);
            verify(exec(a->verbose,
                "docker build -t %o -f %o %o", image, df, a->install) == 0,
                "docker build failed for platform: %o", a->platform);
        }

        // run llc + compile_implements + link inside container
        // mount: build_dir (has .ll), install (has libs/headers), platform dir as native
        string docker_pre = f(string,
            "docker run --rm "
            "-v %o:%o "
            "-v %o:/silver/platform/native "
            "-v %o:%o ",
            a->build_dir, a->build_dir,
            plat,
            a->install, a->install);

        // llc: .ll -> .o
        verify(exec(a->verbose, "%o /silver/platform/native/bin/llc -filetype=obj %o/%o.ll -o %o/%o.o -relocation-model=pic %s",
                    docker_pre, a->build_dir, a->name, a->build_dir, a->name,
                    a->debug ? "-O0" : "") == 0,
               ".ll -> .o compilation failed (platform: %o)", a->platform);

        string cflags = a->asan ? string("-fsanitize=address") : string("");

        if (len(a->implements))
            write_header(a);

        // compile .c/.cc implementations inside docker
        string objs = string();
        each(a->implements, path, i) {
            string i_name   = f(string, "%o/%o.o", a->build_dir, filename(i));
            string ext      = ext(i);
            cstr   compiler = eq(ext, ".cc") ? "clang++" : "clang";
            string st       = stem(i);
            verify(exec(a->verbose, "%o /silver/platform/native/bin/%s %o %s -c %o -o %o -I%o/include/%o -I%o/include -I%o/include/Au",
                docker_pre, compiler, cflags, a->debug ? "-g" : "", i, i_name, install, st, install, install) == 0,
                "failed to compile %o (platform: %o)", i, a->platform);
            if (len(objs)) append(objs, " ");
            concat(objs, i_name);
        }

        // link inside docker; use clang++ when C++ objects are present
        bool has_cpp_d = false;
        each(a->implements, path, impl) {
            string ext_d = ext(impl);
            if (eq(ext_d, "cc") || eq(ext_d, "cpp")) { has_cpp_d = true; break; }
        }
        cstr linker_d  = has_cpp_d ? "clang++" : "clang";
        cstr cpp_libs_d = has_cpp_d ? "-stdlib=libc++" : "";
        string isysroot = a->isysroot ? f(string, "-isysroot %o ", a->isysroot) : string("");
        verify(exec(a->verbose, "%o /silver/platform/native/bin/%s %s %s %s %o %o/%o.o %o -o %o -L%o -L%o/lib -Wl,-rpath,%o -Wl,-rpath,%o/lib %o %o %s",
            docker_pre, linker_d,
            a->is_library ? shared : "", a->debug ? "-g" : "",
            a->is_library ? "-Wl,-Bsymbolic" : "",
            isysroot, a->build_dir, a->name, objs,
            a->product,
            a->build_dir,
            install, a->build_dir, install, libs, cflags, cpp_libs_d) == 0,
            "link failed (platform: %o)", a->platform);

        unlink(a->product_link->chars);
        verify(create_symlink(a->product, a->product_link),
            "could not create product symlink from %o -> %o", a->product_link, a->product);

    } else {

    verify(exec(a->verbose, "%o/bin/llc -filetype=obj %o/%o.ll -o %o/%o.o -relocation-model=pic %s",
                install, a->build_dir, a->name, a->build_dir, a->name,
                a->debug ? "-O0" : "") == 0,
           ".ll -> .o compilation failed");

    string cflags = a->asan ? string("-fsanitize=address") : string("");

    // build compile-only flags (includes + cflags)
    string ccflags = string(cflags->chars);
    if (a->include_paths) {
        each(a->include_paths, string, inc) {
            if (len(ccflags)) append(ccflags, " ");
            if (!starts_with(inc, "-I"))
                append(ccflags, "-I");
            concat(ccflags, inc);
        }
    }

    if (len(a->implements))
        write_header(a);

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
    string objs = compile_implements(a, a->implements, ccflags);

    // link - include the implementation objects; use clang++ when C++ objects are present
    bool has_cpp = false;
    each(a->implements, path, impl) {
        string ext = ext(impl);
        if (eq(ext, "cc") || eq(ext, "cpp")) { has_cpp = true; break; }
    }
    cstr linker   = has_cpp ? "clang++" : "clang";
#ifdef __APPLE__
    cstr cpp_pre   = has_cpp ? "-nostdlib++ -L/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/lib -lc++ -lc++abi" : "";
#else
    cstr cpp_pre   = "";
    cstr cpp_post  = has_cpp ? "-lstdc++" : "";
#endif
    string isysroot = a->isysroot ? f(string, "-isysroot %o ", a->isysroot) : string("");
    verify(exec(a->verbose, "%o/bin/%s %s %s %s %o %s %o/%o.o %o -o %o -L%o -L%o/lib -Wl,-rpath,%o -Wl,-rpath,%o/lib %o %o %s",
        install, linker, a->is_library ? shared : "", a->debug ? "-g" : "",

#ifdef __linux__
        a->is_library ? "-Wl,-Bsymbolic" : "",
#else
        "",
#endif
        isysroot, cpp_pre, a->build_dir, a->name, objs,
        a->product,
        a->build_dir,
        install, a->build_dir, install, libs, cflags,
#ifdef __APPLE__
        ""
#else
        cpp_post
#endif
        ) == 0,
        "link failed");
    
    unlink(a->product_link->chars);

    verify(create_symlink(a->product, a->product_link),
        "could not create product symlink from %o -> %o", a->product_link, a->product);

    }

    // deploy resource files into share/{app-name}/
    // directories merge across modules; file collisions are an error
    if (len(a->resources) && !a->is_library) {
        path share = f(path, "%o/share/%o", install, a->name);
        make_dir(share);
        each(a->resources, path, res) {
            deploy_resources(res, share);
        }
    }

    // write out each ark we find
    fdata ar = fdata(write, true, src, a->artifacts_path);
    each(a->artifacts, path, ark)
        file_write(ar, (Au)string(ark->chars));

    file_close(ar);
}

bool silver_next_is_neighbor(silver a) {
    token b = element(a, -1);
    token c = element(a, 0);
    return c && (b->column + b->count == c->column);
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

    // it could be a sub module
    path c = f(path, "%o/%o.c", a->module, stem(to_path));
    path sfc = absolute(c);
    if (file_exists("%o", sfc)) {
        *is_bin = false;
        return sfc;
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

enode silver_parse_ternary(silver a, enode expr, etype mdl_expect, bool load) {
    if (!read_if(a, "?")) {
        if (!read_if(a, "??"))
            return expr;
        enode expr_true = parse_expression(a, mdl_expect, false, load);
        return e_ternary(a, expr, expr_true, null);
    }
    enode expr_true = parse_expression(a, mdl_expect, false, load);
    verify(read_if(a, ":"), "expected : after expression");
    enode expr_false = parse_expression(a, mdl_expect, false, load);
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
    aether a = f->mod;
    bool is_lambda_call = inherits(f->au, typeid(lambda));

    if (is_lambda_call) {
        return user_arg_count(u(efunc, f->au->src));
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
    if (f->au->member_type == AU_MEMBER_GETTER) {
        return 1;
    }
    if (is_func_ptr((Au)f)) {
        Au_t fn = au_arg_type((Au)f->au);
        return fn->args.count;
    }
    return 0;
}


array read_arg(array tokens, int start, int *next_read);
array read_arg_br(array tokens, int start, int *next_read, cstr open, cstr close);

none copy_lambda_info(enode mem, enode lambda_fn);

enode eshape_from_indices(aether a, array indices);

enode enode_shape(enode);

static enode parse_create_lambda(silver a, enode mem) {
    validate(read_if(a, "["), "expected [ context ] after lambda");

    // mem is the lambda function definition
    enode   lambda_f = (enode)evar_type((evar)mem);
    micro*  ctx_mem  = (micro*)&lambda_f->au->members;  // context members after ::
    int     ctx_ln   = ctx_mem->count;
    array   ctx      = array(alloc, ctx_ln);
    
    Au_t lt = isa(lambda_f);

    // Parse context references - these become pointers in the context struct
    for (int i = 0; i < ctx_ln; i++) {
        Au_t  ctx_arg  = (Au_t)micro_get(ctx_mem, i);
        enode ctx_expr = parse_expression(a, u(etype, ctx_arg->src), false, true);
        
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

static enode parse_lambda_call(silver a, efunc mem) {
    // get arg info from underlying func via src
    efunc  src_fn = u(efunc, mem->au->src);
    int    n_args = user_arg_count(src_fn);

    // Parse the bracket if needed
    bool br = false;
    validate(n_args == 0 || (br = read_if(a, "[") != null) || a->expr_level == 0,
        "expected bracket for lambda call");

    a->expr_level++;

    // Build array of arg values: user args + context at end
    array call_values = array(alloc, 32);

    // Parse each user arg using src func's arg types
    int arg_offset = src_fn->au->is_imethod ? 1 : 0;
    for (int i = 0; i < n_args; i++) {
        Au_t  arg_decl = (Au_t)src_fn->au->args.origin[i + arg_offset];
        etype arg_type = u(etype, arg_decl->src);
        enode arg_expr = parse_expression(a, arg_type, false, true);
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

etype etype_ptr(aether a, Au_t au, enode eshape);

static enode parse_func_call(silver a, efunc f) { sequencer
    push_current(a);
    validate(is_func((Au)f) || is_func_ptr((Au)f), "expected function got %o", f);
    enode caller_target = f->target;
    bool read_br = false;
    bool cmode = false;

    token pk00 = peek(a);

    if (seq == 114) {
        seq = seq;
    }

    // lets see if the user insists on brackets as syntax only
    // if single arg, silver handles it in read_enode
    bool user_likes_brackets = false;
    if (a->expr_level == 0 && next_is(a, "[") && (f->au->is_vargs || user_arg_count(f) > 1)) {
        push_current(a);
        read_body(a);
        user_likes_brackets = !next_is(a, ",");
        pop_tokens(a, false);
    }

    int test2 = user_arg_count(f);

    Au_t    fmdl   = au_arg_type((Au)f);
    efunc   fn     = (efunc)(is_func_ptr(f) ? (enode)f : u(enode, fmdl));
    micro*  m      = (micro*)&fmdl->args;
    int     ln     = m->count, i = 0;

    token pk0 = peek(a);
    
    if (is_cmode(a)) {
        cmode = true;
        read_br = read_if(a, "(") != null;
        if (!read_br) {
            pop_tokens(a, true);
            return efunc_fptr(f);
        }
    }
    else validate((a->expr_level == 0 && !user_likes_brackets) || 
                  (user_arg_count(f) == 0 || (read_br = read_if(a, "[") != null)),
            "expected call-bracket [ at expression depth past statement level");
    
    a->expr_level++;

    array   values = array(alloc, 32, assorted, true);
    enode   target = null;
    i32     offset = 0;

    if (is_func((Au)f) && (caller_target || f->target)) {
        push(values, caller_target ? (Au)caller_target : (Au)f->target);
        offset = 1;
    }

    // track which args have been matched (for commaless type-matching)
    // commaless type-matching only applies inside brackets [ ]
    bool* matched = calloc(ln, sizeof(bool));
    bool  comma_mode = !read_br; // no brackets = positional (old behavior)

    while (i + offset < ln || fn->au->is_vargs) {
        Au_t   arg_decl = (Au_t)micro_get(m, i + offset);
        Au_t   src  = (Au_t)au_arg_type((Au)arg_decl);
        etype  typ  = (arg_decl && arg_decl->is_formatter) ? null : u(etype, src);

        enode  expr = parse_expression(a, comma_mode ? typ : null, true, true);
        verify(expr, "invalid expression");

        if (!comma_mode && !fn->au->is_vargs) {
            // commaless: match expr type to best-fit unmatched parameter
            Au_t expr_type = au_arg_type((Au)expr);
            int  best = -1;
            for (int j = 0; j < ln; j++) {
                if (matched[j]) continue;
                Au_t ptype = au_arg_type((Au)micro_get(m, j + offset));
                if (expr_type == ptype || inherits(expr_type, ptype) ||
                    (expr_type->is_integral && ptype->is_integral) ||
                    (expr_type->is_realistic && ptype->is_realistic)) {
                    best = j;
                    break;
                }
            }
            if (best >= 0) {
                // convert to expected type and place at matched position
                Au_t best_decl = (Au_t)micro_get(m, best + offset);
                etype best_type = u(etype, au_arg_type((Au)best_decl));
                expr = e_create(a, best_type, (Au)expr);
                // ensure values array is big enough and place at correct index
                while (len(values) <= best + offset)
                    push(values, (Au)null);
                values->origin[best + offset] = (Au)expr;
                matched[best] = true;
            } else {
                push(values, (Au)expr);
            }
        } else {
            push(values, (Au)expr);
        }
        i++;

        if (read_if(a, ",")) {
            comma_mode = true; // first comma switches to positional mode
            continue;
        }

        if (comma_mode) {
            verify(len(values) >= ln, "expected %i args for function %o (%i)", ln, f, seq);
        }
        break;
    }

    // fill any unmatched slots with null for commaless mode
    if (!comma_mode) {
        while (len(values) < ln)
            push(values, (Au)null);
    }
    free(matched);
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
        micro*  m      = (micro*)&f_decl->au->args;
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
            Au_t   arg  = (Au_t)micro_get(m, i + offset);
            etype  typ  = u(etype, arg);
            enode  expr = parse_expression(a, typ, true, true); // self contained for '{interp}' to cstr!
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
        shape sh            = instanceof(f->au->meta.b, shape);
        validate(sh, "expected shape on array");
        int   shape_len     = shape_total(sh);
        int   top_stride    = sh->count ? sh->data[sh->count - 1] : 0;
        validate((!top_stride && shape_len == 0) || (top_stride && shape_len),
            "unknown stride information");  
        int   num_index     = 0;

        while (peek(a)) {
            token n = peek(a);
            enode e = parse_expression(a, element_type, false, true);
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
    } else if (is_struct(f)) {
        // positional struct construction: Type [ val, val, val ]
        conv = false;
        r    = (enode)parse_object(a, (etype)f, true);
    } else {
        /// this is a conversion operation
        r = (enode)parse_expression(a, (etype)f, false, true);
        conv = canonical(r) != canonical(f);
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
        k = (Au)parse_expression(a, key_type, false, true);
        validate(read_if(a, "}"), "expected }");
    } else {
        string name = (string)read_alpha(a);
        validate(name, "expected member identifier (%o)", peek(a));
        k = (Au)const_string(chars, name->chars);
    }
    return k;
}

enode parse_export(silver a) {
    sequencer;
    validate(read_if(a, "export"), "expected export keyword");
    a->exported_version = read_compacted(a);
    verify(len(a->exported_version), "expected version");

    // register with the main silver instance (og) so tags are collected in one place
    silver og = a->is_external ? a->is_external : a;
    set(og->exports, (Au)string(a->name->chars), (Au)exports(
        module_path,    a->module_path,
        module_file,    a->module_file,
        project_path,   a->project_path,
        version,        a->exported_version));
        
    // a hash can be made of the entire module-dir, 
    // not so efficient to compute back from git data
    // encompassing all resources in folder is not what we want, though -- nor would we track artifacts

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

    if (bb && eq(bb, "Vulkan-Tools")) {
        bb =  bb;
    }
    array b = hold(read_body(a));
    if (len(b)) {
        array bt = compact_tokens(b);
        import_libs(bt, a->libs);
        if (!a->include_paths)
            a->include_paths = array(16);
        import_includes(a, bt, a->include_paths);
    }

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
    array all_config = (is_fields || (!b || !b->count)) ? array() : compact_tokens(b);

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
        if (mod && !module_source) {
            set(a->libs, string(mod->ident), (Au)_bool(true));

        } else if (!mod && !module_source && !lib_path) {
            prev(a);
            error("could not find module %o", mpath);
        }
        
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
        bool has_link = false;
        each(all_config, string, t)
            if (starts_with(t, "-l")) {
                set(a->libs, (Au)mid(t, 2, len(t) - 2), (Au)_bool(true));
                has_link = true;
            }
        // auto-link: if no -l specified, use the project name (only if lib exists)
        if (!has_link && project) {
            string lib_check = f(string, "%o/lib/%s%o%s", a->install, lib_pre, project, lib_ext);
            if (file_exists("%o", lib_check))
                set(a->libs, (Au)project, (Au)_bool(true));
        }
    } else if (module_source) {
        path module = parent_dir(module_source);
        bool ag = eq(ext(module_source), "ag");
        bool c  = eq(ext(module_source), "c");
        verify(!ag || compare(stem(module_source), stem(module)) == 0, "silver expects identical module stem");

        
        // we should turn on object tracking here, as to trace which objects are still in memory after we drop
        // the simple ternary statement allows us to give an og silver
        // og silver is keeper of artifacts
        if (c) {
            // handled after 'as' is read
        } else {
            silver og = a->is_external ? a->is_external : a;
            silver external = silver(module, module, breakpoint, a->breakpoint,
                verbose, a->verbose, is_external, og, debug, a->debug, defs, defs);

            // these should be the only two objects remaining.
            external_name    = hold(external->name);
            external_product = hold(external->product);

            if (external_product) {
                if (index_of(og->artifacts, (Au)external_product) < 0) {
                    push(og->artifacts, (Au)external_product);
                }
            }

            if (external->module_file) {
                if (index_of(og->artifacts, (Au)external->module_file) < 0) {
                    push(og->artifacts, (Au)external->module_file);
                }
            }
            validate (!external->error, "error importing silver module %o", external);

            drop(external);
            drop(external); // this is to compensate for the initial hold in silver_init [ quirk for build in init ]
            set(a->libs, (Au)string(external_product->chars), (Au)_bool(true));
        }

    }
    else if (is_codegen) {
        cg = (codegen)construct_with(is_codegen, (Au)props, null);
        cg->mod = (aether)a;
    }

    if (next_is(a, "as")) {
        consume(a);
        namespace = hold(read_alpha(a));
        validate(namespace, "expected alpha-numeric %s",
                 is_codegen ? "alias" : "namespace");
    } else if (is_codegen) {
        namespace = hold(string(is_codegen->ident));
    }

    // .c sub-module: compile, and include .h if present
    string ext = module_source ? ext(module_source) : null;
    if (module_source && (eq(ext, "c") || eq(ext, "cc") || eq(ext, "rs"))) {
        path   dir    = parent_dir(module_source);
        string name   = stem(module_source);
        path   header = f(path, "%o/%o.h", dir, name);
        if (file_exists("%o", header))
            push(includes, (Au)string(header->chars));
        if (!a->implements)
            a->implements = array(2);
        push(a->implements, (Au)module_source);
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

    mdl->au->alt = namespace ? cstr_copy(symbol_name((Au)namespace)->chars) : null;
    
    if (len(includes)) {
        push_scope(a, (Au)mdl);
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

void assign_if_cond(aether a, enode targ, enode cond, subprocedure expr);
enode is_set(enode n, evar prop);

enode assign_builder(silver a, enode targ, array post_const) { sequencer
    int level = a->expr_level;
    a->expr_level++;
    push_tokens(a, (tokens)post_const, 0);
    enode expr = parse_expression(a, targ ? (etype)evar_type((evar)targ) : null, false, true);
    pop_tokens(a, false);
    a->expr_level = level;
    return expr;
}

// works for class and module init
void silver_build_user_initializer(silver a, enode prop) {
    if (prop && prop->au && prop->au->member_type == AU_MEMBER_VAR && prop->initializer) {

        verify (!instanceof(prop->initializer, enode), "unexpected enode");
        array initializer = (array)prop->initializer;
        if (len(initializer))
            a->statement_origin = (token)initializer->origin[0];
        array post_const = parse_const(a, (array)prop->initializer);
        subprocedure set_if = subproc(a, assign_builder, post_const);
        efunc  ctx = context_func(a);

        if (is_class(prop->au->context)) {
            Au_t    f  = (Au_t)ctx->au->args.origin[0];
            evar instance = (evar)u(enode, (Au_t)f);
            enode L = access(instance, string(prop->au->ident), true);
            enode set = is_set((enode)instance, (evar)prop);
            assign_if_cond((aether)a, (enode)L, set, set_if);
            
        } else {

            bool   is_module_mem = prop->au->context == a->au;

            push_tokens(a, (tokens)initializer, 0);
            enode L = (enode)prop;

            etype mdl = canonical(prop);
            e_assign(a, L, (Au)parse_expression(a, mdl, false, true), OPType__assign);
            
            pop_tokens(a, false);
        }
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
    line(init_f, "#ifndef _%o_INIT_", NAME);
    line(init_f, "#define _%o_INIT_", NAME);

    // generate constructor macros for each class
    members(a->au, m) {
        if (is_class(m)) {
            string n = cname(string(m->ident));
            line(init_f, "#define TC_%o(MEMBER, VALUE) ({ AF_set((u64*)&instance->__f, FIELD_ID(%o, MEMBER)); VALUE; })", n, n);
            fprintf(init_f, "#define _ARG_COUNT_IMPL_%s(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N, ...) N\n", n->chars);
            fprintf(init_f, "#define _ARG_COUNT_I_%s(...) _ARG_COUNT_IMPL_%s(__VA_ARGS__, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)\n", n->chars, n->chars);
            fprintf(init_f, "#define _ARG_COUNT_%s(...)   _ARG_COUNT_I_%s(\"Au object model\", ## __VA_ARGS__)\n", n->chars, n->chars);
            fprintf(init_f, "#define _COMBINE_%s_(A, B)   A##B\n", n->chars);
            fprintf(init_f, "#define _COMBINE_%s(A, B)    _COMBINE_%s_(A, B)\n", n->chars, n->chars);
            fprintf(init_f, "#define _N_ARGS_%s_0( TYPE)\n", n->chars);
            fprintf(init_f, "#define _N_ARGS_%s_1( TYPE, a) _Generic((a), TYPE##_schema(TYPE, GENERICS, Au) Au_schema(TYPE, GENERICS, Au) const void *: (void)0)((TYPE)(instance), a)\n", n->chars);
            for (int i = 2; i <= 22; i += 2) {
                if (i == 2)
                    fprintf(init_f, "#define _N_ARGS_%s_2( TYPE, a,b) instance->a = TC_%s(a,b);\n", n->chars, n->chars);
                else {
                    string args = string();
                    string prev_args = string();
                    for (int j = 0; j < i; j += 2) {
                        char letter = 'a' + j;
                        char next   = 'a' + j + 1;
                        if (len(args)) append(args, ", ");
                        concat(args, f(string, "%c,%c", letter, next));
                        if (j < i - 2) {
                            if (len(prev_args)) append(prev_args, ", ");
                            concat(prev_args, f(string, "%c,%c", letter, next));
                        }
                    }
                    char new_var = 'a' + (i - 2);
                    char new_val = 'a' + (i - 1);
                    fprintf(init_f, "#define _N_ARGS_%s_%d( TYPE, %s) _N_ARGS_%s_%d(TYPE, %s) instance->%c = TC_%s(%c,%c);\n",
                        n->chars, i, args->chars, n->chars, i - 2, prev_args->chars, new_var, n->chars, new_var, new_val);
                }
            }
            fprintf(init_f, "#define _N_ARGS_HELPER2_%s(TYPE, N, ...)  _COMBINE_%s(_N_ARGS_%s_, N)(TYPE, ## __VA_ARGS__)\n", n->chars, n->chars, n->chars);
            fprintf(init_f, "#define _N_ARGS_%s(TYPE,...)    _N_ARGS_HELPER2_%s(TYPE, _ARG_COUNT_%s(__VA_ARGS__), ## __VA_ARGS__)\n", n->chars, n->chars, n->chars);
            line(init_f, "#define %o(...) ({ \\", n);
            line(init_f, "    %o instance = (%o)alloc_dbg(typeid(%o), 1, __FILE__, __LINE__, seq); \\", n, n, n);
            line(init_f, "    _N_ARGS_%o(%o, ## __VA_ARGS__); \\", n, n);
            line(init_f, "    Au_initialize((Au)instance); \\");
            line(init_f, "    instance; \\");
            line(init_f, "})");
        }
    }

    // generate struct constructors
    members(a->au, m) {
        if (m->is_struct && !m->is_system && !m->is_schema) {
            string n = cname(type_name((Au)m));
            line(init_f, "#define %o(...) structure_of(%o __VA_OPT__(,) __VA_ARGS__)", n, n);
        }
    }

    line(init_f, "#endif");
    line(init_f, "");
    line(init_f, "");


    // write module-name header
    line(module_f, "#ifndef _%o_",     NAME);
    line(module_f, "#define _%o_\n",   NAME);

    // write enum schemas
    members(a->au, m) {
        if (m->traits & AU_TRAIT_ENUM) {
            string n = cname(string(m->ident));
            write(module_f, "#define %o_schema(E,T,Y,...)", n);
            members(m, mi) {
                if (mi->member_type != AU_MEMBER_ENUMV) continue;
                line(module_f, "\\");
                i32 val = *(i32*)mi->value;
                write(module_f, "    enum_value(E,T,Y, %s, %i)", mi->ident, val);
            }
            line(module_f, "\n");
            line(module_f, "declare_enum(%o)\n", n);
        }
    }

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
                        etype aa = u(etype, arg);
                        if (len(args))
                            append(args, ",");
                        string ss = cast(string, aa);
                        concat(args, cname(ss));
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
                    bool   has_meta = mi->args.count > 0;
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
    // write struct schemas
    members(a->au, m) {
        if (m->is_struct && !m->is_system && !m->is_schema) {
            string n = cname(type_name((Au)m));
            string base = m->src ? cname(string(m->src->ident)) : null;

            // schema macro: #define Name_schema(O, Y, T, ...) ...
            if (base)
                write(module_f, "#define %o_schema(O, Y, T, ...)", n);
            else
                write(module_f, "#define %o_schema(O, Y, ...)", n);

            members(m, mi) {
                line(module_f, "\\");
                string mn = cname(string(mi->ident));

                if (mi->member_type == AU_MEMBER_CONSTRUCT) {
                    // only i_struct_ctr
                    Au_t arg = mi->args.count > 1 ? (Au_t)mi->args.origin[1] : null;
                    if (arg) {
                        string arg_type = cname(type_name((Au)arg));
                        write(module_f, "    i_struct_ctr(O, Y, %o)", arg_type);
                    }
                } else if (is_func((Au)mi)) {
                    enode f = u(enode, mi);
                    string args = string();
                    bool first = true;
                    int arg_count = 0;
                    arg_types(mi, arg) {
                        if (f->target && first) {
                            first = false;
                            continue;
                        }
                        first = false;
                        etype aa = u(etype, arg);
                        if (len(args))
                            append(args, ",");
                        string ss = cast(string, aa);
                        concat(args, cname(ss));
                        arg_count++;
                    }
                    string rtype = cname(cast(string, u(etype, f->au->rtype)));
                    // count consecutive leading struct args for suffix
                    int struct_count = 0;
                    bool first2 = true;
                    arg_types(mi, arg2) {
                        if (f->target && first2) { first2 = false; continue; }
                        first2 = false;
                        if (au_arg_type((Au)arg2)->is_struct)
                            struct_count++;
                        else
                            break;
                    }
                    string suffix = struct_count == 0 ? string("") :
                                    struct_count == 1 ? string("_1") :
                                    struct_count == 2 ? string("_2") :
                                                        string("_3");
                    bool show_comma = len(args) > 0;
                    if (mi->is_static) {
                        write(module_f, "    i_struct_static%o(O, Y, %o, %o%s%o)", suffix, rtype, mn, show_comma ? ", " : "", args);
                    } else {
                        write(module_f, "    i_struct_method%o(O, Y, %o, %o%s%o)", suffix, rtype, mn, show_comma ? ", " : "", args);
                    }
                } else {
                    // prop
                    if (base)
                        write(module_f, "    i_struct_prop(O, Y, T, %o)", mn);
                    else {
                        string prop_type = cname(type_name((Au)mi));
                        write(module_f, "    i_struct_prop(O, Y, %o, %o)", prop_type, mn);
                    }
                }
            }
            line(module_f, "\n");

            if (base)
                line(module_f, "declare_struct(%o, %o)\n", n, base);
            else
                line(module_f, "declare_struct(%o)\n", n);
        }
    }

    line(module_f, "#endif");

    // write methods (guarded for C++ — Au macros clash with stdlib)
    line(method_f, "#ifndef _%o_METHODS_", NAME);
    line(method_f, "#define _%o_METHODS_", NAME);
    line(method_f, "#ifndef __cplusplus");
    members(a->au, m) {
        if (is_class(m)) {
            members(m, mi) {
                efunc fn = u(efunc, mi);
                if   (fn)  line(method_f, "%o", method_def((enode)fn));
            }
        }
    }
    line(method_f, "#endif /* __cplusplus */");
    line(method_f, "#endif");
    fclose(method_f);


    // write import header
    line(import_f, "#ifndef _%o_IMPORT_",   NAME);
    line(import_f, "#define _%o_IMPORT_\n", NAME);
    line(import_f, "#ifdef __cplusplus");
    line(import_f, "extern \"C\" {");
    line(import_f, "#endif");

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
    //line(import_f, "#ifndef __cplusplus");
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
    //line(import_f, "#include <%o/init>", a->name); // disabled: clang 22 preprocessor bug with large macro parameter lists in included files
    //line(import_f, "#endif");
    line(import_f, "#ifdef __cplusplus");
    line(import_f, "#undef M");
    line(import_f, "#undef str");
    line(import_f, "#undef typeid");
    line(import_f, "#undef print");
    line(import_f, "#undef a");
    line(import_f, "#undef m");
    line(import_f, "}");
    line(import_f, "#endif");
    line(import_f, "#endif");

    fclose(import_f);
    fclose(module_f);
    fclose(intern_f);
    fclose(public_f);
}

i32 read_enum(silver a, i32 def, Au_t etype);

static enode typed_expr(silver a, enode src, array expr);

none push_lambda_members(aether a, efunc f);

void build_fn(silver a, efunc f, callback preamble, callback postamble) { sequencer
    if (f->user_built)
        return;

    bool user_has_code = len(f->body) || f->cgen;
    f->user_built = true;

    // if there is no code, then this is an external c function; implement must do this
    implement(f, false);

    if (f->has_code && (f->inline_return || f->body || preamble)) {
        if (f->target)
            push_scope(a, (Au)f->target);

        // reasonable convention for silver's debugging facility
        // if this is a standard for IDE, then we can rely on this to improve productivity
        //Au_t au_calls = def(f->au, "sequence", AU_MEMBER_VAR, AU_TRAIT_STATIC);
        //au_calls->src = etypeid(i64)->au;
        //evar e_calls = evar(mod, (aether)a,
        //    au, au_calls);
        
        a->last_return = null;
        if (len(f->body))
            a->statement_origin = (token)f->body->origin[0];
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
            push(call_args, (Au)f->target);
            e_fn_call(a, f->remote_func, call_args);
        } else if (f->cgen) {
            array gen = generate_fn(f->cgen, f, (array)f->body);
        } else if (!f->inline_return && f->body) {
            array source_tokens = parse_const(a, (array)f->body);
            if (strcmp(f->au->ident, "with_path") == 0) {
                f = f;
            }
            push_tokens(a, (tokens)source_tokens, 0);
            parse_statements(a);
            pop_tokens(a, false);
        }

        if (postamble)
            postamble((Au)f, null);

        if (f->inline_return) {
            if (seq == 10) {
                seq = seq;
            }
            push_tokens(a, (tokens)f->inline_return, 0);
            e_fn_return(a, len(f->inline_return) ? 
                (Au)parse_expression(a, u(etype, f->au->rtype), false, true) : null);
            pop_tokens(a, false);
        }
        
        validate(f->au->has_return || (!f->au->rtype || is_void(u(etype, f->au->rtype))),
            "expected return statement in %o", f);
        
        if (is_lambda((Au)f))
            pop_scope(a);

        if (!f->inline_return && !a->last_return)
            e_fn_return(a, null);

        // int len2 = len(a->lexical);
        
        pop_scope(a);
        if (f->target)
            pop_scope(a);
    }
}

/// phase 1: parse the record body so all members are registered
static void build_record_parse(silver a, etype mrec) {
    if (mrec->user_built) return;
    mrec->user_built = true;
    mrec->parsing = true;
    verify(mrec->au->is_class || mrec->au->is_struct, "not a record");
    array body = mrec->body ? (array)mrec->body : array();
    push_tokens(a, (tokens)body, 0);
    push_scope(a, (Au)mrec);

    while (peek(a)) {
        parse_statement(a);
    }
    pop_tokens(a, false);
    mrec->parsing = false;
    pop_scope(a);
}

/// phase 2: implement LLVM types for a record (all records already parsed)
static void build_record_implement(silver a, etype mrec) {
    if (mrec->is_elsewhere) return;
    mrec->is_elsewhere = true;

    push_scope(a, (Au)mrec);
    etype_implement(mrec, false);
    create_type_members(a, a->au);

    // if no init, create one (but don't build it yet)
    if (mrec->au->is_class) {
        Au_t m_init = find_member(mrec->au, "init", AU_MEMBER_FUNC, 0, false);
        if (!m_init) {
            efunc f = function(a, mrec,
                string("init"), etypeid(none), a(mrec), AU_MEMBER_FUNC,
                AU_TRAIT_IMETHOD | AU_TRAIT_OVERRIDE, 0);
            f->has_code = true;
            f->au->alt = cstr_copy(((string)f(string, "%o_init", symbol_name((Au)mrec)))->chars);
            etype_implement((etype)f, false);
        }
    }

    // implement all member function etypes (so they have LLVM types before phase 3)
    members(mrec->au, m) {
        efunc n = u(efunc, m);
        if (n) implement(n, false);
    }
    pop_scope(a);
}

/// phase 3: build init and member functions (all LLVM types now complete)
static void build_record_functions(silver a, etype mrec) {
    if (mrec->au->is_class || mrec->au->is_struct) {
        push_scope(a, (Au)mrec);
        Au_t m_init = find_member(mrec->au, "init", AU_MEMBER_FUNC, 0, false);
        if (m_init)
            build_fn(a, u(efunc, m_init), build_init_preamble, null);
        members(mrec->au, m) {
            efunc n = u(efunc, m);
            if (n) build_fn(a, n, null, null);
        }
        pop_scope(a);
    }
}

static void build_record(silver a, etype mrec) {
    build_record_parse(a, mrec);
    build_record_implement(a, mrec);
    build_record_functions(a, mrec);
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
    efunc ctx   = context_func(a);
    if (strcmp(ctx->au->ident, "get") == 0) {
        ctx = ctx;
    }
    consume(a);
    a->expr_level++;
    enode expr  = is_v ? null : parse_expression(a, rtype, false, true);
    a->expr_level--;

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
        a->last_sub_return = e_noop(a, (etype)expr);
        return a->last_sub_return;
    }
}

catcher context_catcher(silver);
catcher context_catcher_depth(silver, int);

enode parse_expect(silver a) {
    consume(a); // consume 'expect'
    a->expr_level++;
    enode cond = read_enode(a, null, false, true);
    a->expr_level--;
    return e_expect(a, cond, null);
}

enode parse_break(silver a) {
    consume(a);
    int depth = 0;
    array within = next_is(a, "[") ? read_within(a) : null;
    if (within) {
        push_tokens(a, (tokens)within, 0);
        Au extra = read_numeric(a);
        verify(isa(extra) == typeid(i64), "expected constant numeric argument");
        depth = *(int*)extra;
        pop_tokens(a, false);
    }
    catcher cat = context_catcher_depth(a, depth);
    verify(cat, "expected catcher at depth %i", depth);
    a->last_break = e_break(a, cat);
    return a->last_break;
}

enode parse_continue(silver a) {
    consume(a);
    int depth = 0;
    array within = next_is(a, "[") ? read_within(a) : null;
    if (within) {
        push_tokens(a, (tokens)within, 0);
        Au extra = read_numeric(a);
        depth = *(int*)extra;
        pop_tokens(a, false);
    }
    catcher cat = context_catcher_depth(a, depth);
    verify(cat, "continue: no loop at depth %i", depth);
    a->last_continue = e_continue(a, cat);
    return a->last_continue;
}

// read-expression does not pass in 'expected' models, because 100% of the time we run conversion when they differ
// the idea is to know what model is returning from deeper calls
static array read_expression(silver a, etype *mdl_res, bool *is_const) {
    array exprs = array(32);
    int start = a->cursor;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    bool use_hint = mdl_res && *mdl_res;
    enode n = parse_expression(a, use_hint ? *mdl_res : null, use_hint, true);
    if (mdl_res)
        *mdl_res = (etype)n;
    a->no_build = false;
    int e = a->cursor;
    for (int i = start; i < e; i++) {
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
    enode n = read_enode(a, null, false, true);
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
        if (m->member_type == AU_MEMBER_VAR && !m->is_static) {
            if (index == prop) {
                return u(etype, m->src);
            }
            prop++;
        }
    }
    return null;
}

enode constructable(etype fr, etype to);
enode castable(etype fr, etype to);

enode parse_object(silver a, etype mdl, bool within_expr) { sequencer
    validate(within_expr || read_if(a, "["), "expected [");
    if (seq == 14)
        seq = seq;
    //print("seq %i\n", seq);
    bool is_fields = peek_fields(a) || inherits(mdl->au, typeid(map));
    bool is_mdl_map = mdl->au == typeid(map);
    bool is_mdl_collective = inherits(mdl->au, typeid(collective));
    bool was_ptr = false;
    int  iter = 0;

    if (!is_fields && !is_mdl_map) {
        array args = array(alloc, 32);
        bool  first = true;
        do {
            if (first && ((!peek(a) && within_expr) || read_if(a, "]"))) {
                return e_create(a, mdl, (Au)null);
            }
            enode expr     = parse_expression(a, null, false, true);
            bool  has_more = read_if(a, ",") != null;

            etype t0 = canonical(expr);
            etype t1 = canonical(mdl);
            
            if (t0 == t1) {
                if (first && !has_more) return expr;
            }
            else
            // check if we can perform copies or referenced construction, or convert from/to cast/ctr 
            if (first && !has_more) {
                enode mcast    = castable(t0, t1);
                enode mctr     = constructable(t0, t1);
                if (mcast || mctr) {
                    validate(!within_expr || read_if(a, "]"), "expected ]");
                    return e_create(a, mdl, (Au)expr);
                }
            }
            push(args, (Au)expr);

            // trivially construct with fields
            if (!has_more) {
                validate(within_expr || read_if(a, "]"), "expected ]");
                return aether_e_create((aether)a, mdl, (Au)args);
            }
            first = false;
        } while (1);
    }

    validate(!is_mdl_map || is_fields, "expected fields for map");

    if (is_ptr(mdl) && is_struct(mdl->au->src)) {
        was_ptr = true;
        mdl = resolve(mdl);
    }

    etype key = is_mdl_map ? u(etype, mdl->meta_a) : null;
    etype val = is_mdl_map ? u(etype, mdl->meta_b) : null;

    if (!key) key = etypeid(string);
    if (!val) val = etypeid(Au);

    // Lazy-initialized containers
    map   imap   = null;
    array iarray = null;
    shape s      = is_mdl_collective ? instanceof(mdl->meta_b, shape) : null;
    int shape_stride = (s && s->count > 1) ? s->data[s->count - 1] : 0;
    
    while (peek(a)) {
        if (!peek(a) || next_is(a, "]"))
            break;

        Au    k = null;
        token t = peek(a);
        bool  is_literal = instanceof(t->literal, string) != null;
        bool  is_enode_key = false;

        a->statement_origin = peek(a);

        bool auto_bind = is_fields && read_if(a, ":");
        // -- KEY --
        if (is_fields && read_if(a, "{")) {
            k = (Au)parse_field(a, key);
            validate(read_if(a, "}"), "expected }");
            is_enode_key = true;
        } else if (!is_fields && is_mdl_collective) {
            // we are parsing individual scalar value f64 -> vec2f
            etype e = u(etype, mdl->meta_a);
            k = (Au)parse_expression(a, e, false, true);
        } else if (!is_fields && !is_mdl_map) {
            etype e = prop_value_at(mdl, iter);
            validate(e, "cannot find prop for %o at index %i", mdl, iter);
            k = (Au)parse_expression(a, e, false, true);
        } else if (!is_mdl_map) {
            //k = (Au)parse_field(a, key); 
            string name = (string)read_alpha(a);
            validate(name, "expected member identifier (%o)", peek(a));
            k = (Au)const_string(chars, name->chars);
        } else if (key && key != etypeid(string)) {
            k = (Au)parse_expression(a, key, false, true);
            is_enode_key = true;
        } else {
            token t = peek(a);
            string name = (string)read_literal(a, typeid(string));
            if (!name)
                name = name;
            validate(name, "expected literal string");
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
            static int seq2 = 0;
            seq2++;
            if (seq2 == 47) {
                seq2 = 47;
            }
            validate(auto_bind || read_if(a, ":"), "expected : after key %o", t);
            if (auto_bind) prev(a);
            // look up the member type for this key if we know it at design time
            etype mdl_field = null;
            if (!is_mdl_map && k) {
                cstr key_name = isa(k) == typeid(const_string) ? 
                    ((const_string)k)->chars : null;
                if (key_name) {
                    Au_t mem = find_member(mdl->au, key_name, AU_MEMBER_VAR, 0, false);
                    if (mem && mem->src) {
                        mdl_field = u(etype, mem->src);
                        if (!mdl_field)
                            mdl_field = (etype)etype_prep((silver)a, mem->src);
                    }
                }
            } else if (is_mdl_map) {
                mdl_field = val; // the map's value type from meta
            }
    
            a->statement_origin = peek(a);
            v = (Au)parse_expression(a, mdl_field, true, true);
        } else {
            a->statement_origin = peek(a);
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
    if (imap)
        return e_create(a, mdl, (Au)imap);

    // validation check
    if (iarray && mdl->au == typeid(array)) {
        shape dims = instanceof(mdl->meta_b, shape);
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

enode silver_parse_member_expr(silver a, enode mem) { sequencer
    push_current(a);

    if (seq == 37) {
        seq = seq;
    }
    macro is_macro = instanceof(mem, macro);
    bool is_lambda_call = inherits(mem->au, typeid(lambda));
    int indexable = !is_func((Au)mem) && !is_func_ptr((Au)mem) && !is_macro && !is_lambda_call;

    // callable sub: x[] or x[ field: value, ... ] re-invokes the sub body
    if (indexable && next_is(a, "[") && mem->body) {
        push_current(a);
        consume(a); // consume [
        if (read_if(a, "]")) {
            // x[] — invoke stored sub, no overrides
            pop_tokens(a, false);
            return invoke_sub(a, mem);
        }
        // check for field overrides: x[ name: value, ... ]
        // peek to see if this is field: value pattern
        string pk = peek_alpha(a);
        token  pk2 = pk ? element(a, 1) : null;
        if (pk && pk2 && eq(pk2, ":")) {
            pop_tokens(a, false);
            // find sub's position in statement scope
            Au_t scope = mem->au->context;
            int sub_pos = -1;
            for (int k = 0; k < scope->members.count; k++) {
                if ((Au_t)scope->members.origin[k] == mem->au) {
                    sub_pos = k;
                    break;
                }
            }
            // read and apply field overrides
            for (;;) {
                string name = read_alpha(a);
                validate(name, "expected field name in sub override");
                validate(read_if(a, ":"), "expected ':' after field name %o", name);
                // find the member in scope — must be before the sub
                Au_t field = find_member(scope, cstring(name), AU_MEMBER_VAR, 0, false);
                validate(field, "unknown variable '%o' in sub scope", name);
                if (sub_pos >= 0) {
                    int field_pos = -1;
                    for (int k = 0; k < scope->members.count; k++) {
                        if ((Au_t)scope->members.origin[k] == field) {
                            field_pos = k;
                            break;
                        }
                    }
                    validate(field_pos < sub_pos,
                        "cannot override '%o' — declared after sub", name);
                }
                // assign the value
                enode target = (enode)u(enode, field);
                enode value  = parse_expression(a, canonical(target), true, true);
                e_assign(a, target, (Au)value, OPType__assign);
                if (!read_if(a, ",")) break;
            }
            validate(read_if(a, "]"), "expected ']' after sub overrides");
            return invoke_sub(a, mem);
        }
        pop_tokens(a, true); // not field overrides, restore and fall through
    }

    /// handle compatible indexing methods / lambda / and general pointer dereference @ index
    if (indexable && next_is(a, "[")) {
        // C arrays with elements > 0 are indexable like pointers
        bool is_indexable_ptr = is_ptr((Au)mem) || mem->au->elements > 0;
        Au_t au_rec = is_rec((Au)mem);
        etype r = au_rec ? u(etype, au_rec) : null;

        validate(is_indexable_ptr || r, "no indexing available for model %s",
                 mem->au->ident);

        /// we must read the arguments given to the indexer
        consume(a);
        array args = array(16);
        if (r && mem->target)
            push(args, (Au)mem->target);
        enode first_index = null;
        while (!next_is(a, "]")) {
            // if 2 args, the 1 is an indicator of index type 
            // (map types; collective reserves first for value)
            etype meta_key_shape = u(etype, mem->meta_b);
            enode expr = parse_expression(a, meta_key_shape, false, true);
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
        Au_t idx      = null;
        Au_t fallback = null;
        if (r) {
            // select best indexer overload by matching argument type
            if (len(args) == 1) {
                enode inner      = (enode)args->origin[0];
                Au_t  inner_type = au_arg_type((Au)inner->au);
                Au_t  scan       = r->au;
                do {
                    for (int i = 0; i < scan->members.count; i++) {
                        Au_t m = (Au_t)scan->members.origin[i];
                        if (m->member_type != AU_MEMBER_GETTER || m->args.count < 2)
                            continue;
                        Au_t p = au_arg_type(m->args.origin[1]);
                        if (p == typeid(Au)) {
                            if (!fallback) fallback = m;
                        } else if (inner_type == p || inherits(inner_type, p) ||
                                   (inner_type->is_integral && p->is_integral)) {
                            idx = m;
                            break;
                        }
                    }
                    if (idx) break;
                    scan = scan->context;
                } while (scan && scan != scan->context);
                if (!idx) idx = fallback;
            }
            if (!idx) idx = find_member(r->au, null, AU_MEMBER_GETTER, 0, true);

            if (!idx && is_indexable_ptr && !inherits(au_rec, typeid(collective))) {
                r = null;
            } else {
                validate(idx, "index method not found on %o", mem);
            }
        }
        if (r) {
            validate(idx->args.count >= 2, "expected target and index args");
            etype idx_type = u(etype, au_arg_type(idx->args.origin[1]));
            if (idx_type == etypeid(shape)) {
                enode eshape = eshape_from_indices((aether)a, args);
                index_expr = e_fn_call(a, (efunc)u(efunc, idx), a(mem, eshape));
            } else {
                validate(len(args) == 1, "index operators are single instance methods, unless a shape type is used");
                enode inner = (enode)args->origin[0];
                index_expr  = e_fn_call(a, (efunc)u(efunc, idx), a(mem, inner));
                etype rtype = u(etype, idx->rtype);

                // propagate design-time meta type for member access
                // convert Au -> meta_a
                if (rtype && rtype == etypeid(Au) && mem->au->meta.a) {
                    index_expr->au = mem->au->meta.a;
                }
            }

        } else {
            if (len(args) > 1) {
                enode eref_shape = eshape_from_indices((aether)a, args);

                // data shape from the member's meta (this can be enode or literal)
                // we need to read this from the instance
                // enode edata_shape = (enode)u(etype, mem->au->src)->meta->origin[0];
                enode edata_shape = enode_shape(mem);

                // call runtime: shape_flat_index(data_shape, idx_shape) -> i64
                Au_t flat_fn = find_member(typeid(shape), "flat_index", AU_MEMBER_FUNC, 0, false);
                enode flat_idx = e_fn_call(a, (efunc)u(efunc, flat_fn), a(edata_shape, eref_shape));

                index_expr = e_offset(a, mem, (Au)flat_idx);
            } else
                index_expr = e_offset(a, mem, (Au)first_index);

            if (!mem->is_explicit_ref) {
                index_expr->au = index_expr->au->src;
                index_expr->loaded = false;
            }
        }
        pop_tokens(a, true);
        return index_expr;
    } else if (is_macro) {

        // function-like macro without ( — not an invocation, skip expansion (allowing namespace for other members)
        // shouldnt be possible with our read-ahead on this:
        //if (is_macro->params && !next_is(a, "(")) {
        //    pop_tokens(a, true);
        //    return mem;
        //}

        bool mac_cmode = is_cmode(a);
        cstr open_br  = mac_cmode ? "(" : "[";
        cstr close_br = mac_cmode ? ")" : "]";
        verify(!is_macro->params || read_if(a, open_br), "expected %s for macro call", open_br);
        array args = is_macro->params ? array(alloc, 32) : null;
        macro mac  = (macro)mem;

        // read arguments
        if (is_macro->params) {
            while (peek(a) && !next_is(a, close_br)) {
                int next;
                array arg = read_arg_br((array)a->tokens, a->cursor, &next, open_br, close_br);
                arg = arg;
                validate(arg, "macro expansion failed");
                if (arg)
                    push(args, (Au)arg);
                a->cursor = next;
                token pk = peek(a);
                if (!read_if(a, ","))
                    break;
            }
            validate(read_if(a, close_br), "expected %s to end macro call", close_br);
        }

        // expand macro
        token f = (token)first_element((array)mac->def);
        array exp = macro_expand(mac, args);
        push_tokens(a, (tokens)exp, 0);
        bool cmode = a->cmode;
        a->cmode = true;
        mem = parse_expression(a, null, false, true);
        a->cmode = cmode;
        pop_tokens(a, false);

    } else if (mem) {
        if (is_func((Au)mem) || is_func_ptr((Au)mem) || is_lambda_call) {

            if (!is_lambda_call && is_lambda((Au)mem))
                mem = parse_create_lambda(a, mem);
            else if (is_lambda_call)
                mem = parse_lambda_call( a, (efunc)mem);
            else {
                mem = parse_func_call(a, (efunc)mem);
            }

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


enode silver_parse_assignment(silver a, enode mem, OPType op_val, bool is_const) { sequencer

    if (strcmp(mem->au->ident, "data") == 0) {
        mem = mem;
    }
    
    // handle setter logic, state set by parse_member
    if (a->setter_key_tokens && a->setter_fn) {
        array  key_tokens    = a->setter_key_tokens;
        Au_t   setter        = a->setter_fn;
        a->setter_key_tokens = null;
        a->setter_fn         = null;

        push_tokens(a, (tokens)key_tokens, 0);
        enode key = parse_expression(a, null, false, true);
        pop_tokens(a, false);
        enode R = parse_expression(a, null, false, true); 
        efunc fn = (efunc)u(efunc, setter);
        validate(fn, "setter function not found in registry");
        return e_fn_call(a, fn, a(mem, key, R, _i32(op_val)));
    }

    validate(isa(mem) == typeid(enode) || !mem->au->is_const,
        "mem %s is a constant", mem->au->ident);
    if (seq == 11 || seq == 12) {
        mem = mem;
    }
    etype t = (etype)etype_of(mem);
    bool is_bind_ref = (op_val == OPType__bind) ? next_is(a, "ref") : false;

    etype t_expect = t;
    if (next_is(a, "[") && t && mem->au->meta.a) {
        t_expect = etype(mod, (aether)a, au, t->au,
            meta_a, (Au)mem->au->meta.a, meta_b, mem->au->meta.b);
    }



    bool is_explicit = is_explicit_ref(mem);
    enode R = parse_expression(a, is_explicit ? u(etype, t->au->src) :
        next_is(a, "[") ? t_expect : null, false, true);

    // Handle Promotion and Inference for AU_MEMBER_DECL
    if (mem->au->member_type == AU_MEMBER_DECL) {
        // verify name is not a type alias
        array name_tokens = a(token(mem->au->ident));
        push_tokens(a, (tokens)name_tokens, 0);
        etype name_type = read_etype(a, null);
        pop_tokens(a, false);
        validate(!name_type, "%s is a defined type", mem->au->ident);

        // Promote the member to a variable
        Au_t ctx = top_scope(a);
        if (strcmp(mem->au->ident, "data") == 0) {
            mem = mem;
        }
        mem->au->context = ctx;
        mem->au->member_type = AU_MEMBER_VAR;
        mem->au->src = au_arg_type((Au)R);
        mem->au->is_const = is_const;
        rm(a->registry, (Au)mem->au);

        mem = (enode)evar(mod, (aether)a, au, mem->au,
            loaded, false, meta_a, R->meta_a, meta_b, R->meta_b,
            is_explicit_ref, is_bind_ref);

        if (seq == 7) {
            seq = seq;
        }
        // Register and allocate the variable in the backend
        etype_implement((etype)mem, false);
    }

    enode result = e_assign(a, mem, (Au)R, op_val);
    mem->au->is_assigned = true;
    return mem;
}

enode expr_builder(silver a, array cond_tokens, etype mdl_scope) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    a->statement_origin = peek(a);
    bool use_scope = mdl_scope != null;
    enode cond_expr = parse_expression(a, mdl_scope, use_scope, true);
    validate(a->cursor == len(cond_tokens), "expected condition expression, found remaining: %o", peek(a));
    pop_tokens(a, false);
    a->expr_level--;
    return cond_expr;
}

enode cond_builder(silver a, array cond_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    a->statement_origin = peek(a);
    enode cond_expr = parse_expression(a, etypeid(bool), false, true);
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
    a->statement_origin = peek(a);
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
    a->statement_origin = peek(a);
    last = parse_statements(a);
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
    a->statement_origin = peek(a);
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
    a->expr_level++;
    enode last = null;
    push_tokens(a, (tokens)expr_tokens, 0);
    a->statement_origin = peek(a);
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

enode parse_ifdef_else(silver a) {
    bool one_truth = false;
    enode statements = null;

    verify(a->expr_level == 0, "unexpected expression level at ifdef");

    // first ifdef [cond]
    validate(read_if(a, "ifdef"), "expected ifdef");
    validate(read_if(a, "["), "expected [ after ifdef");
    string def_name = read_alpha(a);
    validate(def_name, "expected identifier after ifdef [");
    bool cond = eval_define(a, def_name);
    validate(read_if(a, "]"), "expected ] after ifdef condition");
    array block = read_body(a);
    if (cond) {
        push_tokens(a, (tokens)block, 0);
        statements = parse_statements(a);
        pop_tokens(a, false);
        one_truth = true;
    }

    // chain of el [cond] / el
    while (read_if(a, "el")) {
        bool has_cond = false;
        bool el_cond  = false;
        if (read_if(a, "[")) {
            string el_name = read_alpha(a);
            validate(el_name, "expected identifier after el [");
            el_cond  = eval_define(a, el_name);
            has_cond = true;
            validate(read_if(a, "]"), "expected ] after ifdef el condition");
        }
        array block = read_body(a);
        if (has_cond) {
            if (!one_truth && el_cond) {
                push_tokens(a, (tokens)block, 0);
                statements = parse_statements(a);
                pop_tokens(a, false);
                one_truth = true;
            }
        } else if (!one_truth) {
            push_tokens(a, (tokens)block, 0);
            statements = parse_statements(a);
            pop_tokens(a, false);
            break; // bare el is terminal
        }
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
    bool is_const = false;
    etype mdl_read = null;
    validate(next_is(a, "["), "expected [ after if");
    array cond  = read_within(a);
    verify(cond, "expected [condition] after if");
    array block = read_body(a);
    verify(block, "expected body");
    push(tokens_cond,  (Au)cond);
    push(tokens_block, (Au)block);

    // chain of el [cond] / el
    while (read_if(a, "el")) {
        bool is_const = false;
        etype mdl_read = null;
        array cond  = next_is(a, "[") ? read_within(a) : null; // null when no [...] → final else
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
    enode e_expr = parse_expression(a, null, false, true);
    map cases = map(hsize, 16);
    array expr_def = null;
    bool all_const = is_prim((Au)e_expr) || is_enum((Au)e_expr);
    etype hint_mdl = null;
    bool  first    = true;
    while (true) {

        if (read_if(a, "case")) {
            array body = null;
            array values = array(alloc, 4);
            
            // read comma-separated case values
            do {
                bool is_const = false;
                etype hint = canonical(e_expr);
                etype mdl_read = hint && hint->au->is_enum ? hint : null;
                if (first) {
                    hint_mdl = mdl_read;
                    first = false;
                } else if (hint_mdl && hint_mdl->au != mdl_read->au)
                    hint_mdl = null;
                
                array value = read_expression(a, &mdl_read, &is_const);
                all_const &= is_const;
                push(values, (Au)value);
            } while (read_if(a, ","));
            
            body = read_body(a);
            
            // point each value at the same body
            each(values, array, value)
                set(cases, (Au)value, (Au)body);
            continue;
        } else if (read_if(a, "default")) {
            expr_def = read_body(a);
            continue;
        } else
            break;
    }

    subprocedure build_expr = subproc(a, expr_builder, hint_mdl);
    subprocedure build_body = subproc(a, block_builder, null);
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
    push_tokens(a, (tokens)expr_tokens, 0);
    push_scope(a, (Au)s_members);
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

enode parse_for(silver a) { sequencer
    token for_token = read_if(a, "for");
    validate(for_token != null, "expected for");
    a->statement_origin = for_token;

    token after         = null;
    array all           = read_within(a); // null if no [...] after for
    enode in_expr       = read_if(a, "in") ? parse_expression(a, null, false, true) : null;
    array init_exprs    = array(alloc, 32);
    array cond_exprs    = array(alloc, 32);
    array step_exprs    = array(alloc, 32);
    evar  key_var       = null;
    evar  val_var       = null;
    bool  do_while      = false;

    statements st = new(statements, mod, (aether)a, au, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_scope(a, (Au)st);

    // if we use in_expr, then we do not split by :: in a traditional for,
    // we will read statements within all; each of which should be an enode binding from a : operation
    // we will use value first, then key.
    
    if (in_expr) {
        // parse bindings from all tokens: [v: Value, k: Key]
        // first binding is value, second (optional) is key
        push_tokens(a, (tokens)all, 0);
        
        // read first binding: v: Type
        string val_name = read_alpha(a);
        validate(val_name, "expected variable name in for-in");
        validate(read_if(a, ":"), "expected : after variable name");
        etype val_type = read_etype(a, null);
        validate(val_type, "expected type after :");
        
        // check for second binding (key)
        if (read_if(a, ",")) {
            string key_name = read_alpha(a);
            validate(key_name, "expected key variable name");
            validate(read_if(a, ":"), "expected : after key name");
            etype key_type = read_etype(a, null);
            validate(key_type, "expected type after :");
            
            // create key variable in current scope
            Au_t key_mem = def_member(top_scope(a), key_name->chars, key_type->au, AU_MEMBER_VAR, 0);
            key_var = evar(mod, (aether)a, au, key_mem);
            etype_implement((etype)key_var, false);
        }
        
        // create value variable in current scope
        Au_t val_mem = def_member(top_scope(a), val_name->chars, val_type->au, AU_MEMBER_VAR, 0);
        val_var = evar(mod, (aether)a, au, val_mem);
        val_var->is_any = val_type->is_any;
        etype_implement((etype)val_var, false);
        pop_tokens(a, false);
    }
    else if (all) {
        // split on commas at bracket depth 0 into segments
        array segments = array(alloc, 8);
        array cur_seg  = array(alloc, 32);
        int depth = 0;
        each(all, token, t) {
            if (eq(t, "["))      depth++;
            else if (eq(t, "]")) depth--;
            else if (eq(t, ",") && depth == 0) {
                push(segments, (Au)cur_seg);
                cur_seg = array(alloc, 32);
                continue;
            }
            push(cur_seg, (Au)t);
        }
        push(segments, (Au)cur_seg);
        int n = len(segments);

        // 1: cond
        // 2: init, cond
        // 3: init, cond, step
        validate(n <= 3, "for loop accepts at most 3 parts: init, condition, step");
        if (n == 1) {
            cond_exprs = (array)segments->origin[0];
        } else if (n == 2) {
            init_exprs = (array)segments->origin[0];
            cond_exprs = (array)segments->origin[1];
        } else {
            init_exprs = (array)segments->origin[0];
            cond_exprs = (array)segments->origin[1];
            step_exprs = (array)segments->origin[2];
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
    subprocedure build_body = subproc(a, block_builder, null);

    a->statement_origin = for_token;
    enode res = e_for(a,
        init_exprs, cond_exprs, body, step_exprs,
        build_init, build_cond, build_body, build_step,
        do_while, in_expr, val_var, key_var);
    pop_scope(a);

    if (!in_expr && len(init_exprs)) // only pop init scope when init vars were declared
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

Au_t next_is_keyword(silver a, Au_t *fn) {
    token t = peek(a);
    if (!t)
        return null;
    if (!isalpha(t->chars[0]))
        return null;
    Au_t f = find_type((cstr)t->chars, null);
    string kw = f(string, "parse_%o", t);
    if (f && inherits(f, typeid(etype)) && (*fn = find_member(f, kw->chars, AU_MEMBER_FUNC, 0, false)))
        return f;
    return null;
}

/// called after : or before, where the user has access
etype silver_read_def(silver a, interface access) {
    Au_t  parse_fn  = null;
    Au_t  is_type   = next_is_keyword(a, &parse_fn);
    etype is_class  = !is_type ? next_is_class(a, false) : null;
    bool  is_struct  = next_is(a, "struct");
    bool  is_scalar  = next_is(a, "scalar");
    bool  is_enum    = next_is(a, "enum");
    bool  is_export  = next_is(a, "export");
    bool  is_alias   = next_is(a, "alias");

    if (!is_type && !is_class && !is_struct && !is_scalar && !is_enum && !is_export && !is_alias)
        return null;

    if (is_type) {
        validate(!access, "unexpected access level");
        struct _etype* (*parser)(silver) = (void*)parse_fn->value;
        validate(parser, "expected parse fn on member");
        return parser(a);
    }

    if (is_export) {
        validate(!access, "unexpected access level");
        return (etype)parse_export(a);
    }

    if (is_alias) {
        consume(a);
        string  alias_name = read_alpha(a);
        validate(alias_name, "expected name after alias");
        validate(read_if(a, ":"), "expected ':' after alias name");

        bool    is_ref = read_if(a, "ref") != null;
        etype   target = read_etype(a, null);
        validate(target, "expected type after alias %o:", alias_name);
        
        Au_t    top = top_scope(a);
        Au_t    alias_au = def(top, alias_name->chars, AU_MEMBER_TYPE, AU_TRAIT_ALIAS);
        alias_au->is_pointer = is_ref;
        
        etype_register((aether)a, (Au)alias_au, (Au)hold(target), false);
        return target;
    }

    consume(a);
    string n = read_alpha(a);
    validate(n, "expected alpha-numeric identity, found %o", next(a));

    Au_t top = top_scope(a);
    etype mtop = u(etype, top);
    enode mem  = null; // = emember(mod, (aether)a, name, n, context, mtop);
    etype mdl  = null;
    array meta = null;
    if (is_class || is_struct) {
        validate(is_module(mtop),
            "expected record definition at module level");

        mdl = record(a, (etype)a, is_class, n,
            is_struct ? AU_TRAIT_STRUCT : AU_TRAIT_CLASS);
        mdl->au->access_type = access;

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
                etype_register((aether)a, (Au)m, (Au)hold(emeta(mod, (aether)a, au, m, meta_index, index)), false);
                micro_push(&mdl->au->args, (Au)m);
                first = false;
            }
        }

        mdl->body = (tokens)read_body(a);

    } else if (is_scalar) {
        // scalar px : f32  — struct with single 'value' member, no body
        validate(is_module(mtop), "expected scalar definition at module level");
        validate(read_if(a, ":"), "expected ':' after scalar name %o", n);
        etype value_type = read_etype(a, null);
        validate(value_type, "expected type after scalar %o:", n);

        Au_t top2 = top_scope(a);
        mdl = record(a, (etype)a, null, n, AU_TRAIT_STRUCT);
        if (!mdl) {
            fault("failed to create scalar type %o", n);
            return null;
        }
        mdl->au->access_type = access;
        mdl->au->src = value_type->au;
        mdl->body = null;
        mdl->user_built = true;

        // create the single 'value' member
        Au_t val_mem = def_member(mdl->au, "value", value_type->au, AU_MEMBER_VAR, 0);
        val_mem->access_type = interface_public;

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
        validate(len(enum_body), "expected body for enum %o", n);

        Au_t enum_au = def(top_scope(a),
            n->chars, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
        enum_au->access_type = access;
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
                enode n = parse_expression(a, store, false, true);
                Au lit = n ? literal_value(n,
                    isa(n->literal) == typeid(shape) ? typeid(i64) : isa(n->literal)) : null;
                verify(n && ((Au_t)isa(lit))->is_integral,
                    "expected integral literal");
                v = lit;
                u8 sp[64];
                memcpy(sp, lit, ((Au_t)isa(lit))->abi_size / 8);
                value = *(i64*)sp;
            } else
                v = primitive(store->au, &value);
            
            Au_t enum_v         = def_enum_value(enum_au, e->chars, v);
            enum_v->src         = enum_au;
            enum_v->value       = (object)hold(v);
            enum_v->member_type = AU_MEMBER_ENUMV;
            enum_v->is_const    = true;
            
            enode enum_node = enode(
                mod, (aether)a, au, enum_v, literal, v);
            etype_register((aether)a, (Au)enum_v, (Au)hold(enum_node), false);
            implement(mdl, false);
            value += 1;
        }
        pop_scope(a);
        pop_tokens(a, false);

        etype_implement(mdl, false);
        create_type_members(a, a->au);

    } else {
        error("unknown error");
    }

    validate(mdl && len(n),
             "name required for model: %s", isa(mdl)->ident);

    if (!get(a->registry, (Au)mdl->au))
    etype_register((aether)a, (Au)mdl->au, (Au)hold(mdl), false);
    return mdl;
}

define_class(chatgpt, codegen)
define_class(claude,  codegen)
define_class(gemini,  codegen)
define_class(silver, aether)
define_class(import, enamespace)
define_class(exports, Au)

initializer(silver_module)