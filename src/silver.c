#include <import>
#include <limits.h>
#include <posix.h>      // on windows this is the whole posix surface
#ifndef _WIN32
#include <execinfo.h>
#include <sys/file.h>   // flock — serialize external-checkout builds across processes
#include <sys/ioctl.h>
#include <sys/wait.h>   // export funcs run forked; the build waits on them
#include <fcntl.h>
#include <dlfcn.h>      // coverage libraries run in-process after build
#include <ctype.h>      // package names are lower-case on every distro
#include <time.h>       // rpm build time
#endif

#ifdef BUILD_LIBRARY

static void silver_module();
Au_t lexical_traits(array lex, symbol f, u64 traits, int member_type);
bool au_is_expanding(Au_t m);
none au_expanding_push(Au_t m);
none au_expanding_pop();
enode e_convert_or_cast(aether a, etype output, enode input);

etype etype_prep(aether, Au_t);
enode aether_e_frameaddress(aether, enode);
enode aether_e_try(aether, array, array, array, evar,
    subprocedure, subprocedure, subprocedure);
enode parse_statements(silver a);
enode parse_statement(silver a);
enode parse_try(silver a);
void build_fn(silver a, efunc fmem, callback preamble, callback postamble);
bool is_explicit_ref(enode);
enode enode_ref(aether, enode, etype);
void aether_ensure_terminator(aether, enode);
void aether_init_listen(aether);
void aether_emit_listen_entry(aether, const char*);
void aether_emit_listen_line(aether, const char*);
bool aether_has_listen(aether);
void aether_clear_listen(aether);
void aether_listen_gate_create(aether);
void aether_listen_gate_store(aether, enode);
etype evar_type(evar a);
enode parse_import(silver a);
static void uninstall_products(silver a);
static symbol platform_triple(silver a);
static bool   platform_is_windows(silver a);
static bool   target_is_android(silver a);
static bool   target_is_mobile(silver a);
static string target_dir(silver a);
static string silver_release_version(silver a);
static symbol platform_abi_clang(silver a);
static symbol platform_abi_cxx(silver a);
static symbol platform_abi_link(silver a);
static string device_cmake_toolchain(silver a);
static string device_meson_cross(silver a);
path module_exists(silver a, array idents, bool binary_finary, bool* is_bin);
string symbol_name(Au obj);

// per-session module products: name -> path, published only once the link has
// written the file. in-progress builds live in building_list with the owning
// thread, so a concurrent instance waits instead of racing into the same
// output or handing out a path that is mid-relink
static map             silver_compiled = null;
static pthread_mutex_t compiled_lock   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  compiled_cond   = PTHREAD_COND_INITIALIZER;

typedef struct building {
    char             name[256];
    pthread_t        owner;
    struct building* next;
} building;

static building* building_list = null;

static building* building_find(cstr name) {
    for (building* b = building_list; b; b = b->next)
        if (strcmp(b->name, name) == 0) return b;
    return null;
}

static void building_add(cstr name) {
    building* b = (building*)calloc(1, sizeof(building));
    snprintf(b->name, sizeof(b->name), "%s", name);
    b->owner = pthread_self();
    b->next  = building_list;
    building_list = b;
}

static void building_remove(cstr name) {
    building** p = &building_list;
    while (*p) {
        if (strcmp((*p)->name, name) == 0) {
            building* dead = *p;
            *p = dead->next;
            free(dead);
            return;
        }
        p = &(*p)->next;
    }
}

static void symlink_resources(path src, path dst);

static bool is_silver_repo(silver a) {
    return a->git_owner && a->git_project &&
        cmp(a->git_owner, "ar-visions") == 0 &&
        cmp(a->git_project, "silver") == 0;
}

static string silver_install_name(silver a) {
    if (!a->git_owner) return a->name;
    string prefix = is_silver_repo(a)
        ? string("silver") : a->git_owner;
    return f(string, "%o-%o", prefix, a->name);
}

static string silver_symbol_prefix(silver a) {
    bool system = !a->autype || a->is_Au_import ||
        a->autype->is_system || a->autype->is_au_native;
    string identity = !system && a->share_name && len(a->share_name)
        ? a->share_name : a->name;
    return symbol_name((Au)identity);
}

static void collect_resource_dirs(silver a, path source) {
    DIR *dir = opendir(source->chars);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_DIR) continue;
        path res = form(path, "%o/%s", source, entry->d_name);
        if (index_of(a->resources, (Au)res) < 0)
            push(a->resources, (Au)hold(res));
    }
    closedir(dir);
}

// share/<name>/<dir> symlinks for every resource folder. must run on the
// CACHED path too: a module last built as someone else's dependency deployed
// its dirs under THAT root's share, so its own bundle can be missing while
// its product is current — the app then starts with no fonts/models at all
// a resource deleted or renamed in the module leaves its link behind in
// the share: drop every link whose source is gone. links only: the share
// also holds real files the exports bake, and those stay
static void prune_dangling(path dir) {
    DIR *d = opendir(dir->chars);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        path p = form(path, "%o/%s", dir, e->d_name);
        struct stat ls, ts;
        if (lstat(p->chars, &ls) != 0) continue;
        if (S_ISLNK(ls.st_mode)) {
            if (stat(p->chars, &ts) != 0) unlink(p->chars);
        } else if (S_ISDIR(ls.st_mode))
            prune_dangling(p);
    }
    closedir(d);
}

static void deploy_module_resources(silver a) {
    path share = f(path, "%o/share/%o", a->install,
        silver_install_name(a));
    make_dir(share);
    if (!len(a->resources)) return;
    each(a->resources, path, res) {
        path dst = f(path, "%o/%o", share, stem(res));
        // a stale directory symlink must go before the real dir is made
        struct stat dsts;
        if (lstat(dst->chars, &dsts) == 0 && S_ISLNK(dsts.st_mode))
            unlink(dst->chars);
        make_dir(dst);
        symlink_resources(res, dst);
    }
    prune_dangling(share);
}

// a module has one product per platform: the host copy a cross build
// loads and the device copy it links are both cached, apart
static string compiled_key(silver a) {
    return f(string, "%o@%o", a->name,
        a->platform && len(a->platform) ? a->platform : string("native"));
}

// the product exists on disk at this point; release anyone waiting on it
static void publish_product(silver a) {
    pthread_mutex_lock(&compiled_lock);
    if (a->product && silver_compiled)
        set(silver_compiled, (Au)compiled_key(a), (Au)a->product);
    building_remove(compiled_key(a)->chars);
    pthread_cond_broadcast(&compiled_cond);
    pthread_mutex_unlock(&compiled_lock);
}
// on a clean build, a source dated in the future (a wrong system clock stamped
// it) stays newer than every product forever -> endless rebuild. correct it to
// now, once, on clean only. a normal build reads it untouched.
static u64 source_mtime(silver a, path p) {
    u64 m = modified_time(p);
    if (a->clean && m > (u64)current_time()) {
        utime(p->chars, NULL);        // NULL sets mtime to now
        m = modified_time(p);
    }
    return m;
}

static void bg_build_start(silver a, silver og, path module, map defs);
static enode parse_inline_lambda(silver a);
enode parse_export(silver a);
enode parse_log(silver a);
enode parse_return(silver a);
enode parse_break(silver a);
enode parse_continue(silver a);
enode parse_expect(silver a);
enode parse_for(silver a);
enode parse_loop_while(silver a);
enode parse_if_else(silver a);
enode parse_ifdef_else(silver a, bool negate);
static enode typed_expr(silver mod, enode n, array expr);
i32 read_enum(silver a, i32 def, Au_t etype);
efunc parse_func(silver, Au_t, enum AU_MEMBER, u64, OPType, string);
etype etype_resolve(etype t);
enode enode_value(enode mem, bool force);

aether au_active(aether);
AU_EXPORT aether au_codegen_active_get(void);
AU_EXPORT void   au_codegen_active_set(aether);
// transient hand-off for `import M with ext…`: parse_import sets this to the ext path
// list right before constructing the external silver(), and silver_init reads it at the
// top into a->extensions (the prop-pair ctor is already at its 22-arg max).
static array g_import_with = null;
// thread-local: imports build in parallel, and the child is constructed on
// the thread that sets this — a process-global would cross between them
static _Thread_local bool g_host_build = false;
// --listen propagates to every imported module's compile
static string g_listen = null;
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
    struct _token* prev = (struct _token*)silver_element(a, -1); \
    if (prev && pk && prev->line != pk->line) pk = prev; \
    /* replayed/synthesized tokens carry line 0 — fall back to the last \
       consumed token so the module location is never dropped */ \
    if (pk && pk->line == 0 && prev && prev->line) pk = prev; \
    string s = (string)formatter( \
        (Au_t)null, false, stderr, (Au) true, seq, \
        (symbol) "\n%o:%i:%i (%s:%i%o)\n" t, \
        (pk && pk->source ? (Au)pk->source : (Au)a->module_file), \
        pk ? pk->line : 0, pk ? pk->column : 0, __FILE__, __LINE__, \
        seq ? f(string, "@%i", seq) : string("") __VA_OPT__(,) __VA_ARGS__); \
    if (level_err >= fault_level) { \
        halt(s, aether_peek_safe(a)); \
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
enode ternary_expr_builder(silver a, array expr_tokens, Au unused);

token silver_element(silver, num);

// where a type or member was declared. the token already knows its own
// file and line, so a definition never has to be told
static void stamp_source(Au_t m, token t) {
    if (!m || !t || !t->source) return;
    // FIRST writer wins: the declaration is parsed once, and whatever
    // sees the record later is a use, not an origin
    if (m->source) return;
    m->source   = cstr_copy((cstr)t->source->chars);
    m->src_line = (i32)t->line;
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
static symbol lib_static = ".a";
static symbol app_ext = "";
static symbol platform = "linux";
static symbol shared   = "-shared";
#elif defined(_WIN32)
// mingw installs an import library beside the dll; that is what a link finds
static symbol lib_pre = "lib";
static symbol lib_ext = ".dll.a";
static symbol lib_static = ".a";
static symbol app_ext = ".exe";
static symbol platform = "windows";
static symbol shared   = "-shared";
#elif defined(__APPLE__)
static symbol lib_pre = "lib";
static symbol lib_ext = ".dylib";
static symbol lib_static = ".a";
static symbol app_ext = "";
static symbol platform = "darwin";
static symbol shared   = "-dynamiclib";
#endif

#define next_is(a, ...) silver_next_is_eq(a, __VA_ARGS__, null)

static bool target_is_apple(silver a);
static bool is_cpp_source_ext(silver a, string ext);
static bool is_native_source_ext(silver a, string ext);
static cstr source_lang_flag(silver a, string ext);
static string framework_name(string fw);
static string framework_import_name(array mpath, string single);

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
static Au_t  vec_elem_of(etype t);
static enode vector_literal(silver a, Au_t elem);
AU_EXPORT bool au_is_vector(Au_t t);

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
        next(a, Syntax__none);
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

// windows sdk libs sit in the linker's own search path, never in our install
// tree, so every file-existence check for them fails and drops the flag
static bool is_sdk_lib(cstr nm) {
#ifdef _WIN32
    static const char* sdk[] = {"ole32","oleaut32","uuid","user32","kernel32",
        "gdi32","shell32","advapi32","winmm","ws2_32","avrt","dbghelp",
        "shlwapi","comdlg32","version","setupapi","imm32",0};
    for (int i = 0; sdk[i]; i++)
        if (strcmp(nm, sdk[i]) == 0) return true;
#else
    (void)nm;
#endif
    return false;
}

// unix resolves -lfoo through the libfoo.so symlink; windows bakes the version
// into the filename (opencv_core4140.lib, OpenEXR-3_4.lib), so find that name
static string resolve_versioned_lib(silver a, string nm) {
#ifdef _WIN32
    if (is_sdk_lib((cstr)nm->chars)) return nm;
    // the crt covers these, or posix.cc does; there is no such .lib here
    static symbol none[] = {"m","c","dl","pthread","util","ncurses","tinfo",
                            "stdc++","c++","c++abi","objc","System",0};
    for (int i = 0; none[i]; i++)
        if (cmp(nm, none[i]) == 0) return null;
    // spelled differently here; png keeps its version, so let the scan below
    // finish that one (libpng -> libpng16)
    if (cmp(nm, "z")   == 0) nm = string("zlib");
    if (cmp(nm, "png") == 0) nm = string("libpng");
    // mbedtls 4 folded the crypto library into tf-psa-crypto
    if (cmp(nm, "mbedcrypto") == 0) nm = string("tfpsacrypto");
    // -l:libfoo.so.N names an exact file for gnu ld; nothing here answers to it
    if (len(nm) && nm->chars[0] == ':') return null;

    if (file_exists("%o/lib/%s%o%s", a->install, lib_pre, nm, lib_ext) ||
        file_exists("%o/lib/%s%o%s", a->install, lib_pre, nm, lib_static))
        return nm;
    array  all  = ls(f(path, "%o/lib", a->install), null, false);
    string pick = null;
    int    nlen = (int)len(nm);
    each (all, path, p) {
        string base = stem(p);
        string e    = ext(p);
        // ext() reports the extension without its dot
        if (!base || !e || cmp(e, &lib_static[1]) != 0)     continue;
        if (strncmp((cstr)base->chars, (cstr)nm->chars, nlen) != 0) continue;
        cstr rest = &((cstr)base->chars)[nlen];
        if (!*rest)                                         continue;
        if (!isdigit((unsigned char)*rest) && *rest != '-') continue;
        if (!pick || len(base) < len(pick)) pick = base;
    }
    if (pick) return pick;
#endif
    return nm;
}

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
    return eq(stem(parent_dir(par)), "checkout");
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
    // a [ v1, v2 ] literal against a vec-typed slot seeds a vector
    Au_t vex = vec_elem_of(expect);
    if (vex && next_is(a, "[")) {
        consume(a, Syntax__none);
        return vector_literal(a, vex);
    }
    if (is_rec(expect) && next_is(a, "[")) {
        // collections and structs go straight to parse_object
        if (inherits(expect->autype, typeid(collective)) || expect->autype->is_struct) {
            enode res = parse_object(a, expect, false);
            return expr_load(res, load);
        }

        push_current(a);

        consume(a, Syntax__none);
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
    return expr_load(e_create(a, expect, (Au)unbias, false), load); // parse assignment needs to expect a deref'd type, or, we call it loaded:false,
}

enode e_short_circuit_pair(silver a, OPType combine, enode L, enode R);

static bool is_multi_expression(silver a, OPType match_op) {
    if (match_op != OPType__equal && match_op != OPType__not_equal)
        return false;
    if (!next_is(a, "("))
        return false;
    
    push_current(a);
    consume(a, Syntax__none);
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
    

    //print_tokens(a, seq);
    token pk2 = peek(a);
    enode L = read_enode(a, expect, false, true);
    token t = peek(a);
    if (!cmode && !L) {
        etype l = t ? elookup(t->chars) : null;
        error("unexpected %s'%o'", l->autype->member_type == AU_MEMBER_TYPE ? "type " : "", t);
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
            return e_create(a, expect, (Au)L, false);
            //return L;
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
                enode ret_node = fallback ? e_create(a, rtype, (Au)fallback, false) : null;
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
            enode type_R;
            if (type) {
                type_R = e_typeid(a, (etype)type);
            } else if (next_is(a, "typeid")) {
                type_R = read_enode(a, etypeid(Au_t), false, true);
                verify(type_R, "expected type expression after typeid");
            } else {
                verify(false, "expected type after %s",
                    match_op == OPType__is ? "is" : "inherits");
                type_R = null;
            }
            if (match_op == OPType__inherits) {
                Au_t f_instanceof = find_member(typeid(Au), "instance_of",
                                              AU_MEMBER_FUNC, 0, false);
                enode instanceof_result = e_fn_call(a, u(efunc, f_instanceof), a(L, type_R), false, false);
                L = type ? e_direct_cast(a, instanceof_result, type) : instanceof_result;
            } else {
                enode type_L = e_typeid(a, (etype)L);
                enode cond   = e_cmp_op(a, OPType__equal, type_L, type_R);
                L = type ? e_ternary(a, cond, L, e_null(a, type)) : cond;
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
            consume(a, Syntax__none);
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
etype etype_ptr(aether a, Au_t au, enode eshape);

Au   build_init_preamble(enode f, Au arg);
void aether_emit_recover();
aether aether_clone(aether, int);
void build_fn(silver a, efunc fmem, callback preamble, callback postamble);

typedef struct {
    silver a;
    silver root;
    array  work, wrec;
    map    inits;
    int    first, step;
    int*   done;
    string err;
} fn_worker_t;

static void* build_fn_worker(void* arg) {
    fn_worker_t* w = (fn_worker_t*)arg;
    silver a = w->a;
    au_codegen_active_set((aether)a);
    attempt() {
        int nwork = len(w->work);
        for (int i = w->first; i < nwork; i += w->step) {
            if (w->root->error) break; // first error stops all workers
            efunc    f2  = (efunc)w->work->origin[i];
            etype    rec = (etype)w->wrec->origin[i];
            callback pre = get(w->inits, (Au)f2) ? build_init_preamble : null;
            if (rec) push_scope(a, (Au)rec, 27);
            build_fn(a, f2, pre, null);
            if (rec) pop_scope(a);
            if (w->done) __atomic_add_fetch(w->done, 1, __ATOMIC_RELAXED);
        }
    }
    on_error() {
        aether_emit_recover();
        w->err   = _frame.message ? (string)hold((Au)_frame.message) : string("worker error");
        a->error = true;
        w->root->error = true; // wake the monitor loop on the root
    }
    finally()
    au_codegen_active_set(null);
    // no pool drain: registry/type objects live past the worker
    return null;
}


Au build_init_preamble(enode f, Au arg) {
    silver a = (silver)au_active(f->mod);
    etype  rec = f->target ? resolve((etype)f->target) : (etype)a;

    // emit default + override initializers as part of this class's own init[].
    // baking overrides into the class's init means every construction site —
    // regardless of which module is constructing — gets the override store
    // via the normal init call, without cross-module evar lookup at each site.
    members(rec->autype, mem) {
        if (mem->is_static) continue; // statics emit from module init, not per-instance
        enode n = u(enode, mem);
        if (!n || !n->initializer) continue;
        if (mem->is_override) {
            // override targets the inherited slot via e_inherited_access
            enode self = f->target ? (enode)f->target : null;
            if (self) emit_override_init(a, self, mem);
        } else {
            build_user_initializer(a, (etype)n);
        }
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
int  command_exec_hook(command, bool, bool,
                       bool (*)(void*, cstr, ssize_t), void*);

void finalize_coverage(silver);

// redraw one progress line on stderr
static bool progress_active = false;
static char progress_top[640];
static char progress_sub[640];
static bool progress_sub_active = false;
static int  progress_lines = 0;

static void progress_bar(char* bar, int width, double frac) {
    static const char* parts[] =
        { "", "▏", "▎", "▍", "▌", "▋", "▊", "▉" };
    double cells = frac * width;
    int full = (int)cells;
    int part = (int)((cells - full) * 8);
    int o = 0;
    for (int i = 0; i < full; i++) o += sprintf(bar + o, "█");
    if (full < width && part)
        o += sprintf(bar + o, "%s", parts[part]);
    int used = full + (part ? 1 : 0);
    for (int i = used; i < width; i++) bar[o++] = ' ';
    bar[o] = 0;
}

static void progress_render() {
    if (!isatty(2) || !progress_top[0]) return;
    int width = 0;
#ifndef _WIN32
    struct winsize ws;
    if (ioctl(2, TIOCGWINSZ, &ws) == 0)
        width = ws.ws_col;
#endif
    if (progress_lines == 2)
        fprintf(stderr, "\r\x1b[K\x1b[1A\r\x1b[K");
    else if (progress_lines == 1)
        fprintf(stderr, "\r\x1b[K");

    char top[640];
    snprintf(top, sizeof(top), "%s", progress_top);
    if (width > 0 && width < (int)sizeof(top) &&
        (int)strlen(top) > width) {
        if (width > 3) {
            top[width - 3] = '.';
            top[width - 2] = '.';
            top[width - 1] = '.';
        }
        top[width] = 0;
    }
    fprintf(stderr, "%s", top);
    progress_lines = 1;
    if (progress_sub_active && progress_sub[0]) {
        char sub[640];
        snprintf(sub, sizeof(sub), "%s", progress_sub);
        if (width > 0 && width < (int)sizeof(sub) &&
            (int)strlen(sub) > width) {
            if (width > 3) {
                sub[width - 3] = '.';
                sub[width - 2] = '.';
                sub[width - 1] = '.';
            }
            sub[width] = 0;
        }
        fprintf(stderr, "\n\r\x1b[K%s", sub);
        progress_lines = 2;
    }
    fflush(stderr);
    progress_active = true;
}

static void progress_clear_line() {
    if (!progress_active || !isatty(2)) return;
    if (progress_lines == 2)
        fprintf(stderr, "\r\x1b[K\x1b[1A\r\x1b[K");
    else
        fprintf(stderr, "\r\x1b[K");
    fflush(stderr);
    progress_active = false;
    progress_lines = 0;
}

static void progress_draw(silver a, double frac) {
    if (a->verbose || !isatty(2)) return;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    static silver last_mod   = null;
    static int    last_mille = -1;
    int mille = (int)(frac * 1000.0);
    if (a == last_mod && mille == last_mille) return;
    last_mod   = a;
    last_mille = mille;
    const int width = 14;
    char bar[512];
    progress_bar(bar, width, frac);
    snprintf(progress_top, sizeof(progress_top), "%s %3d%% %s",
        bar, (int)(frac * 100.0), a->name ? a->name->chars : "");
    progress_render();
}

static void progress_command(symbol module, symbol phase, int percent,
                             symbol detail, bool done) {
    if (!isatty(2)) return;
    if (done) {
        progress_sub_active = false;
        progress_sub[0] = 0;
        progress_render();
        return;
    }
    if (!module || !phase) return;
    const int width = 14;
    char bar[128];
    if (percent >= 0) {
        progress_bar(bar, width, (double)percent / 100.0);
        snprintf(progress_sub, sizeof(progress_sub),
            "%s %3d%% %s %s", bar, percent, module, phase);
    } else {
        progress_bar(bar, width, 0.0);
        snprintf(progress_sub, sizeof(progress_sub),
            "%s      %s %s", bar, module, phase);
    }
    progress_sub_active = true;
    progress_render();
}

static void progress_done(silver a) {
    if (!progress_active) return;
    progress_clear_line();
    progress_top[0] = 0;
    progress_sub[0] = 0;
    progress_sub_active = false;
}

void silver_parse(silver a) {
    // aim codegen at the device BEFORE any IR exists: PE import/export
    // storage classes are decided as each global is created, not at emit
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0)
        set_target((aether)a, platform_triple(a));

    efunc init = module_initializer(a);

    // what the compiler itself has registered: an import resolves against
    // this list first, so a missing name here is why a module isn't found
    if (a->verbose) {
        i64   n_mods = 0;
        Au_t* mods   = (Au_t*)module_list(&n_mods);
        print("registered modules (%i):", (int)n_mods);
        for (int i = 0; i < n_mods; i++)
            if (mods[i] && mods[i]->ident)
                print("  %s (%i members)%s", mods[i]->ident,
                    (int)mods[i]->members.count, mods[i]->is_hidden ? " hidden" : "");
    }

    // determine target arch/os — use --platform if set, otherwise host
    symbol target_arch = arch;
    bool   target_mac  = SILVER_IS_MAC;
    bool   target_lin  = SILVER_IS_LINUX;
    bool   target_win  = SILVER_IS_WINDOWS;

    if (a->platform && len(a->platform)) {
        string p = a->platform;
        // derive OS from platform name
        target_mac = strstr(p->chars, "apple")   != NULL || strstr(p->chars, "ios")     != NULL;
        target_lin = strstr(p->chars, "linux")   != NULL || strstr(p->chars, "jetson")  != NULL;
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
#ifndef NDEBUG
    Au_t m_debug = def_member(a->autype, "debug",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
#endif
    Au_t m_mac   = def_member(a->autype, "apple",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_lin   = def_member(a->autype, "linux",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_win   = def_member(a->autype, "windows", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_x86   = def_member(a->autype, "x86_64",  typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_arm64 = def_member(a->autype, "arm64",   typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_ios   = def_member(a->autype, "ios",     typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    Au_t m_and   = def_member(a->autype, "android", typeid(bool), AU_MEMBER_VAR, AU_TRAIT_CONST);
    bool target_ios = a->platform && len(a->platform) && strstr(a->platform->chars, "ios") != NULL;
    bool target_and = target_is_android(a);

    etype_register((aether)a, (Au)m_debug, (Au)hold(e_operand(a, _bool(!a->release), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_mac,   (Au)hold(e_operand(a, _bool(target_mac), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_lin,   (Au)hold(e_operand(a, _bool(target_lin), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_win,   (Au)hold(e_operand(a, _bool(target_win), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_x86,   (Au)hold(e_operand(a, _bool(strcmp(target_arch, "x86_64") == 0), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_arm64, (Au)hold(e_operand(a, _bool(strcmp(target_arch, "arm64")  == 0), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_ios,   (Au)hold(e_operand(a, _bool(target_ios), etypeid(bool))), false);
    etype_register((aether)a, (Au)m_and,   (Au)hold(e_operand(a, _bool(target_and), etypeid(bool))), false);

    // AU_MEMBER_* constants — available as const i32 in all .ag code
    struct { const char* name; int value; } au_consts[] = {
        {"AU_MEMBER_NONE",      AU_MEMBER_NONE},
        {"AU_MEMBER_MODULE",    AU_MEMBER_MODULE},
        {"AU_MEMBER_TYPE",      AU_MEMBER_TYPE},
        {"AU_MEMBER_CONSTRUCT", AU_MEMBER_CONSTRUCT},
        {"AU_MEMBER_VAR",       AU_MEMBER_VAR},
        {"AU_MEMBER_FUNC",      AU_MEMBER_FUNC},
        {"AU_MEMBER_OPERATOR",  AU_MEMBER_OPERATOR},
        {"AU_MEMBER_CAST",      AU_MEMBER_CAST},
        {"AU_MEMBER_GETTER",    AU_MEMBER_GETTER},
        {"AU_MEMBER_SETTER",    AU_MEMBER_SETTER},
        {"AU_MEMBER_ENUMV",     AU_MEMBER_ENUMV},
        {"AU_MEMBER_OVERRIDE",  AU_MEMBER_OVERRIDE},
        {"AU_MEMBER_NAMESPACE", AU_MEMBER_NAMESPACE},
        {"AU_MEMBER_DECL",      AU_MEMBER_DECL},
        {"AU_MEMBER_MACRO",     AU_MEMBER_MACRO},
        {NULL, 0}
    };
    for (int i = 0; au_consts[i].name; i++) {
        Au_t m = def_member(a->autype, au_consts[i].name, typeid(i32), AU_MEMBER_VAR, AU_TRAIT_CONST);
        etype_register((aether)a, (Au)m, (Au)hold(e_operand(a, _i32(au_consts[i].value), etypeid(i32))), false);
    }

    // module-stem rust companion: <module>.rs binds automatically —
    // cbindgen emits its extern "C" header, imported like a C header
    string rs_stem = stem(a->module);
    path rs_file = f(path, "%o/%o.rs", a->module_path, rs_stem);
    if (exists(rs_file)) {
        path gen = f(path, "%o/%o_rs.h", a->build_dir, rs_stem);
        string cbg = f(string, "%o/bin/cbindgen", a->base_install ? a->base_install : a->install);
        if (!file_exists("%o", cbg)) cbg = string("cbindgen");
        validate(exec(a->verbose, "%o --lang c -o %o %o", cbg, gen, rs_file) == 0,
            "cbindgen failed for %o", rs_file);
        import rs_mdl = import(
            mod,           (aether)a,
            external_name, f(string, "%o_rs", rs_stem),
            module_source, rs_file,
            is_au_rt,      false);
        rs_mdl->include_paths = hold(a(gen));
        push(a->imports, (Au)rs_mdl);
    }

    // suppress body parsing during statement loop — register names only
    void* saved_prepare = a->prepare_record;
    a->prepare_record = null;

    // the leading imports are known before we parse them: start the silver
    // module builds now so they overlap the imports that follow. only the
    // plain `import <name>` form — qualified and `with` forms stay serial.
    push_current(a);
    while (peek(a) && next_is(a, "import")) {
        token it   = peek(a);
        i64   line = it->line;
        consume(a, Syntax__keyword);
        token nm    = peek(a);
        token after = element(a, 1);
        bool  plain = nm && nm->chars && isalpha(nm->chars[0]) &&
                      !(after && after->line == line);
        if (plain) {
            bool  is_bin = false;
            path  m = module_exists(a, a(string(nm->chars)), true, &is_bin);
            if (m && !is_bin && eq(ext(m), "ag") &&
                compare(parent_dir(m), absolute(a->module)) != 0) {
                silver og = a; while (og->is_external) og = og->is_external;
                bg_build_start(a, og, m, a->defs);
            }
        }
        while (peek(a) && peek(a)->line == line) consume(a, Syntax__none);
    }
    pop_tokens(a, false);

    i64 ntok = len(a->tokens);
    while (peek(a)) {
        progress_draw(a, 0.5 * (double)a->cursor / (double)(ntok ? ntok : 1));
        validate(parse_statement(a), "unexpected token found for statement: %o", peek(a));
        incremental_resolve(a);
    }

    // --test on a module that declares no tests proves nothing; say so
    // rather than exiting 0 and reading as a pass
    if (a->test && !a->is_external) {
        bool any = false;
        members(a->autype, mem)
            if (mem->member_type == AU_MEMBER_FUNC &&
                mem->access_type == interface_expect &&
                mem->rtype == typeid(bool)) { any = true; break; }
        validate(any, "--test: module %o declares no expect tests", a->name);
    }


    /// resolve deferred aliases (types that weren't available during initial parse)
    /// multi-pass: aliases may depend on each other
    if (a->pending_aliases && len(a->pending_aliases)) {
        int count = len(a->pending_aliases) / 2;
        int resolved = 1;
        while (resolved) {
            resolved = 0;
            for (int i = 0; i < count; i++) {
                Au_t  alias_au     = (Au_t)a->pending_aliases->origin[i * 2];
                if (alias_au->src) continue;
                array alias_tokens = (array)a->pending_aliases->origin[i * 2 + 1];
                push_tokens(a, (tokens)alias_tokens, 0);
                etype target = read_etype(a, null);
                pop_tokens(a, false);
                if (target) {
                    alias_au->src = target->autype;
                    alias_au->meta.a = (Au_t)target->meta_a;
                    alias_au->meta.b = target->meta_b;
                    etype ealias = etype(mod, (aether)a, autype, alias_au,
                        meta_a, (Au)alias_au->meta.a, meta_b, alias_au->meta.b);
                    etype_register((aether)a, (Au)alias_au, (Au)hold(ealias), true);
                    e_typeid((aether)a, u(etype, alias_au));
                    resolved++;
                }
            }
        }
        for (int i = 0; i < count; i++) {
            Au_t alias_au = (Au_t)a->pending_aliases->origin[i * 2];
            validate(alias_au->src, "could not resolve deferred alias '%s'",
                alias_au->ident ? alias_au->ident : "?");
        }
    }

    // restore body parsing now that all type names are registered
    a->prepare_record = saved_prepare;

    /// phase 1: parse all record bodies so every class/struct name and member is registered
    members(a->autype, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && !rec->parsing && !rec->user_built)
            build_record_parse(a, rec);
    }

    /// phase 2: implement all LLVM types (records and free functions)
    members(a->autype, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (rec && !mem->is_system && !mem->is_schema && rec->user_built)
            build_record_implement(a, rec);
    }
    members(a->autype, mem) {
        etype e = u(etype, mem);
        if (is_func((Au)mem) && !mem->is_system && e && e != a->fn_init && !e->user_built)
            implement(e, false);
    }

    /// phase 3: build all functions (all LLVM types now complete)
    /// Flatten every function to a work-list, then build each body. The work-list
    /// is the unit of distribution for the per-context (lltype[core]) threaded
    /// codegen that builds on this; for now it runs serially in this instance.
    array work  = array(256);  // efuncs to implement, in deterministic order
    array wrec  = array(256);  // owning record per work item (etype, null for free fns)
    map   inits = map(hsize, 32);  // efunc -> needs build_init_preamble

    members(a->autype, mem) {
        etype rec = (mem->is_class || mem->is_struct) ? u(etype, mem) : null;
        if (!rec || mem->is_system || mem->is_schema || !rec->user_built) continue;
        if (!(rec->autype->is_class || rec->autype->is_struct)) continue;

        Au_t m_init = find_member(rec->autype, "init", AU_MEMBER_FUNC, 0, false);
        efunc init  = m_init ? u(efunc, m_init) : null;
        if (init) {
            push(work, (Au)init); push(wrec, (Au)rec);
            set(inits, (Au)init, (Au)_bool(true));
        }
        members(rec->autype, m) {
            efunc n = u(efunc, m);
            if (n && n != init) { push(work, (Au)n); push(wrec, (Au)rec); }
        }
    }
    members(a->autype, mem) {
        etype e = u(etype, mem);
        // strip_expect: expect tests are not emitted at all
        if (a->strip_expect && is_func((Au)mem) && mem->access_type == interface_expect)
            continue;
        if (is_func((Au)mem) && !mem->is_system && e && e != a->fn_init && !e->user_built) {
            push(work, (Au)e); push(wrec, null);
        }
    }

    int nwork    = len(work);
    silver aroot = a->is_external ? a->is_external : a;
    int    njobs = a->jobs > 0 ? a->jobs : aroot->jobs;
    int nthreads = njobs > 0 ? njobs : (int)sysconf(_SC_NPROCESSORS_ONLN) - 2;
    if (nthreads > nwork)     nthreads = nwork;
    if (nthreads > AU_CORES)  nthreads = AU_CORES; // one context per worker

    if (nthreads <= 1) {
        for (int i = 0; i < nwork; i++) {
            progress_draw(a, 0.5 + 0.5 * (double)i / (double)nwork);
            efunc    f2  = (efunc)work->origin[i];
            etype    rec = (etype)wrec->origin[i];
            callback pre = get(inits, (Au)f2) ? build_init_preamble : null;
            if (rec) push_scope(a, (Au)rec, 27);
            build_fn(a, f2, pre, null);
            if (rec) pop_scope(a);
        }
    } else {
        fn_worker_t* ws = (fn_worker_t*)calloc(nthreads, sizeof(fn_worker_t));
        pthread_t*   ts = (pthread_t*)  calloc(nthreads, sizeof(pthread_t));
        int done_ct = 0;
        for (int t = 0; t < nthreads; t++) {
            ws[t].a     = (silver)aether_clone((aether)a, t);
            ws[t].root  = a;
            ws[t].work  = work;
            ws[t].wrec  = wrec;
            ws[t].inits = inits;
            ws[t].first = t;
            ws[t].step  = nthreads;
            ws[t].done  = &done_ct;
            pthread_create(&ts[t], null, build_fn_worker, &ws[t]);
        }
        // the bar tracks completed functions; joins happen once all land
        for (;;) {
            int d = __atomic_load_n(&done_ct, __ATOMIC_RELAXED);
            progress_draw(a, 0.5 + 0.5 * (double)d / (double)(nwork ? nwork : 1));
            if (d >= nwork || a->error) break;
            usleep(20000);
        }
        string werr = null;
        for (int t = 0; t < nthreads; t++) {
            pthread_join(ts[t], null);
            if (ws[t].err && !werr) werr = ws[t].err;
            if (ws[t].a->error) a->error = true;
        }
        free(ts);
        // worker clones stay alive: dealloc would dispose shared LLVM state
        free(ws);
        if (werr) halt(werr, null);
    }


    members(a->autype, mem) {
        etype e = u(etype, mem);
        if (e && e->autype->is_alias)
            e_typeid(a, (etype)e);
    }


    // when done parsing, we are able to create a module schema (type_id definition) and the evar instance for the type_id (module_m with info/type)
    implement_type_id((etype)a);

    // explicit call to finalize the coverage globals
    // we need to not emit during init
    finalize_coverage(a);

    a->building_initializer = true;
    build_fn(a, init, build_init_preamble, null);
    a->building_initializer = false;
    progress_draw(a, 1.0);
    progress_done(a);
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
    while (dir && len(dir) > 1 && !dir_exists("%o/.git", dir))
        dir = parent_dir(dir);

    return (dir && len(dir) > 1) ? dir : null;
}

static void exporter(silver a) {
    if (a->is_external || !len(a->exports))
        return;

    // Write registries before optional Git tags.
    pairs(a->exports, i) {
        exports exp = (exports)i->value;
        if (!exp->version && !exp->areas)
            continue;
        path edir = f(path, "%o/export", a->install);
        make_dir(edir);
        string install_name = exp->install_name
            ? exp->install_name : (string)i->key;
        path efile = f(path, "%o/%o.agi", edir, install_name);
        if (compare(install_name, (string)i->key) != 0) {
            path legacy = f(path, "%o/%o.agi", edir, i->key);
            unlink(legacy->chars);
        }
        string body  = string(alloc, 256);
        if (exp->version)
            concat(body, f(string, "version: %o\n", exp->version));
        if (exp->areas)
            pairs(exp->areas, j) {
                array vals = (array)j->value;
                char line[1024];
                int  off = snprintf(line, sizeof(line), "%s: [", ((string)j->key)->chars);
                bool first = true;
                each(vals, string, v) {
                    char* col = strchr(v->chars, ':');
                    if (col)
                        off += snprintf(line + off, sizeof(line) - off, "%s'%.*s': %s",
                                        first ? "" : ", ", (int)(col - v->chars), v->chars, col + 1);
                    else
                        off += snprintf(line + off, sizeof(line) - off, "%s'%s'",
                                        first ? "" : ", ", v->chars);
                    first = false;
                }
                snprintf(line + off, sizeof(line) - off, "]\n");
                append(body, line);
            }
        save(efile, (Au)body, null);
    }

    // Tag exports after registries are installed.
    pairs(a->exports, i) {
        exports exp         = (exports)i->value;

        if (!exp->module_file || !exp->project_path || !exp->version)
            continue;
        string mod_file = cast(string, exp->module_file);
        string rel_mod = mid(mod_file, exp->project_path->count + 1, len(exp->project_path) - exp->project_path->count);
        string  tag         = f(string, "%o-%o", i->key, exp->version);
        string  cmd         = f(string, "git rev-parse %o:%o 2>/dev/null", tag, rel_mod);
        string  rev_parse   = command_run((command)cmd, false);
        string  hash_cmd    = f(string, "git hash-object %o", exp->module_file);
        string  hash        = command_run((command)hash_cmd, false);

        if (compare(hash, rev_parse) != 0)
            vexec(false, "git-tag", "git -C %o tag -f %o", a->project_path, tag);
    }
}


void llvm_reinit(silver);
aether aether_clone(aether, int);
void aether_reinit_startup(aether);
void emit_debug_loc(aether, cstr, u32, u32);
void update_current_file(aether, path);

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


// a dependency builds with ITS own build system, so the device's toolchain
// has to be handed over in each one's dialect. written once per device,
// beside its sysroot; returns the flag that points the tool at it
static string device_cmake_toolchain(silver a) {
    if (!a->platform || !len(a->platform) || cmp(a->platform, "native") == 0) return string("");
    if (!a->sysroot) return string("");
    symbol triple = platform_triple(a);
    bool   win    = platform_is_windows(a);
    cstr   pl     = a->platform->chars;
    bool   ios    = strstr(pl, "ios") != NULL;
    // android is Linux to cmake here: its own Android mode wants an ndk
    // toolchain, and the flags below already say everything it would
    symbol system = win ? "Windows" : ios ? "iOS" : strstr(pl, "macos") ? "Darwin" : "Linux";
    symbol proc   = strstr(triple, "arm64")   ? "arm64"   :
                    strstr(triple, "aarch64") ? "aarch64" :
                    strstr(triple, "riscv")   ? "riscv64" :
                    strstr(triple, "mips")    ? "mips64"  :
                    strstr(triple, "arm")     ? "arm"     :
                    strstr(triple, "i686")    ? "i686"    : "x86_64";
    path   tools  = f(path, "%s/platform/native/bin", SILVER);
    path   tfile  = f(path, "%s/platform/%o/target.cmake", SILVER, target_dir(a));

    // windows is a mingw sysroot, so one recipe serves every target. pic is
    // meaningless on PE, and a dependency's warnings are not the user's
    symbol pic = win ? "" : "-fPIC ";
    // built in pieces: the formatter writes into a fixed buffer, and a
    // whole toolchain file overruns it
    string content = string(alloc, 2048);
    concat(content, f(string,
        "# generated by silver for device platform %o\n"
        "set(CMAKE_SYSTEM_NAME %s)\n"
        "set(CMAKE_SYSTEM_PROCESSOR %s)\n", a->platform, system, proc));
    concat(content, f(string,
        "set(CMAKE_C_COMPILER   \"%o/clang\"   CACHE STRING \"\")\n"
        "set(CMAKE_CXX_COMPILER \"%o/clang++\" CACHE STRING \"\")\n"
        "set(CMAKE_SYSROOT      \"%o\" CACHE STRING \"\")\n",
        tools, tools, a->sysroot));
    concat(content, f(string,
        "set(CMAKE_C_FLAGS   \"--target=%s %s%s-w\" CACHE STRING \"\")\n",
        triple, platform_abi_clang(a), pic));
    // the device sdk's libc++ headers must match the libc++ it ships; a
    // bare sysroot does not put the ndk's on the default search path
    string cxx_sdk = (ios || target_is_android(a)) ?
        f(string, "-nostdinc++ -isystem %o/usr/include/c++/v1 ", a->sysroot) : string("");
    concat(content, f(string,
        "set(CMAKE_CXX_FLAGS \"--target=%s %s%s%s%o-w\" CACHE STRING \"\")\n",
        triple, platform_abi_clang(a), platform_abi_cxx(a), pic, cxx_sdk));
    // objective-c++ (.mm) takes its own flag set
    if (ios)
        concat(content, f(string,
            "set(CMAKE_OBJC_FLAGS   \"--target=%s %s-w\" CACHE STRING \"\")\n"
            "set(CMAKE_OBJCXX_FLAGS \"--target=%s %s%o-w\" CACHE STRING \"\")\n",
            triple, pic, triple, pic, cxx_sdk));
    // a MODULE is its own flag set: miss it and cmake falls back to the
    // host's ld, which knows neither this target nor its libraries
    concat(content, f(string,
        "set(CMAKE_EXE_LINKER_FLAGS    \"-fuse-ld=lld %s\" CACHE STRING \"\")\n"
        "set(CMAKE_SHARED_LINKER_FLAGS \"-fuse-ld=lld %s\" CACHE STRING \"\")\n",
        platform_abi_link(a), platform_abi_link(a)));
    concat(content, f(string,
        "set(CMAKE_MODULE_LINKER_FLAGS \"-fuse-ld=lld %s\" CACHE STRING \"\")\n",
        platform_abi_link(a)));
    // bionic keeps libm apart, in the api-level dir cmake never searches:
    // name it on every link, since find_library comes back empty. our clang
    // is not the ndk's, so it does not add the shared libc++ itself either —
    // name it, and the dir it lives in, on every c++ link
    if (target_is_android(a)) {
        cstr abi = strstr(triple, "x86_64") ? "x86_64-linux-android" : "aarch64-linux-android";
        concat(content, f(string,
            "set(CMAKE_C_STANDARD_LIBRARIES   \"-lm\" CACHE STRING \"\")\n"
            "set(CMAKE_CXX_STANDARD_LIBRARIES \"-nostdlib++ -L%o/usr/lib/%s -lc++_shared -lm\" CACHE STRING \"\")\n",
            a->sysroot, abi));
    }
    // an apple target names its sdk and floor through cmake's own knobs
    // cmake wants the sdk's own name (iPhoneOS*.sdk), not our link to it
    if (ios)
        concat(content, f(string,
            "set(CMAKE_OSX_SYSROOT \"%o\" CACHE STRING \"\")\n"
            "set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING \"\")\n"
            "set(CMAKE_OSX_DEPLOYMENT_TARGET 16.0 CACHE STRING \"\")\n",
            absolute(a->sysroot)));
    // .rc is windows-only. windres, not llvm-rc: llvm-rc emits an msvc .res,
    // and a mingw link takes objects. it also names no target of its own,
    // and mingw's headers refuse to preprocess without one
    if (win)
        concat(content, f(string,
            "set(CMAKE_RC_COMPILER \"%o/llvm-windres\" CACHE STRING \"\")\n"
            "set(CMAKE_RC_FLAGS \"-D_WIN32%s -DRC_INVOKED -I%o/include\""
            " CACHE STRING \"\")\n",
            tools, strstr(triple, "i686") ? "" : " -D_WIN64", a->sysroot));
    // the target's OWN prefix is a root too: dependencies install there, not
    // into the sysroot, and a package search that misses it finds the host's
    // build of the same library instead
    concat(content, f(string,
        "set(CMAKE_FIND_ROOT_PATH \"${CMAKE_SYSROOT};%s/platform/%o\")\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n"
        "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n", SILVER, target_dir(a)));
    // raw text: file_write serializes with a length prefix
    path_save(tfile, (Au)content, null);
    return f(string, "-DCMAKE_TOOLCHAIN_FILE=%o ", tfile);
}

// meson says the same thing in its own file
static string device_meson_cross(silver a) {
    if (!a->platform || !len(a->platform) || cmp(a->platform, "native") == 0) return string("");
    if (!a->sysroot) return string("");
    symbol triple = platform_triple(a);
    bool   win    = platform_is_windows(a);
    cstr   pl     = a->platform->chars;
    symbol system = win ? "windows" : strstr(pl, "macos") || strstr(pl, "ios") ? "darwin" :
                    strstr(pl, "android") ? "android" : "linux";
    symbol cpu_f  = strstr(triple, "arm64")   ? "aarch64" :
                    strstr(triple, "aarch64") ? "aarch64" :
                    strstr(triple, "riscv")   ? "riscv64" :
                    strstr(triple, "mips")    ? "mips64"  :
                    strstr(triple, "arm")     ? "arm"     :
                    strstr(triple, "i686")    ? "x86" : "x86_64";
    path   tools  = f(path, "%s/platform/native/bin", SILVER);
    path   xfile  = f(path, "%s/platform/%o/meson-cross.ini", SILVER, target_dir(a));
    string content = f(string,
        "# generated by silver for device platform %o\n"
        "[binaries]\n"
        "c = ['%o/clang', '--target=%s']\n"
        "cpp = ['%o/clang++', '--target=%s']\n"
        "ar = '%o/llvm-ar'\n"
        "strip = '%o/llvm-strip'\n"
        "[host_machine]\n"
        "system = '%s'\n"
        "cpu_family = '%s'\n"
        "cpu = '%s'\n"
        "endian = 'little'\n",
        a->platform, tools, triple, tools, triple, tools, tools,
        system, cpu_f, cpu_f);
    path_save(xfile, (Au)content, null);
    return f(string, "--cross-file %o ", xfile);
}

static void prepare_record_cb(Au a_au, Au t_au) {
    silver a = (silver)a_au;
    etype  t = (etype)t_au;
    build_record_parse(a, t);
}

// --format output: a binary token-syntax map written to a caller-provided FILE (a->format)
// as a BYPRODUCT of the normal build — not a separate parse. a standalone --format pass
// would compete with the watcher's build and throw the build work away; instead the same
// parse that feeds codegen also stamps tokens (lexer + parser), and we dump that map to the
// file. the watcher re-runs the build on change, so the file stays current; the caller
// (orbiter) reads it and recolors. logging is irrelevant — the map is in the file, not
// stdout. the same build's stderr carries compilation diagnostics (file:line) for free.
//
// file layout (little-endian — root truncates + writes header, every module in the graph
// appends its sections during the build, root appends the end marker; magic words let the
// reader frame + resync, and detect a truncated/incomplete write):
//   header : u32 0x53464D54 ('SFMT')   u32 version(=2)
//   section: u32 0xC0DEFACE             u32 path_len, path_len bytes (canonical abs path),
//            i64 mtime (source modified time, ms — for incremental: skip re-tokenizing a
//            file whose section mtime matches the file on disk),
//            u32 token_count, token_count * { u32 line, u32 col, u32 len, u32 syntax }
//   end    : u32 0x00000000             (zero — can't collide with the section magic)
// the section path is the symlink-resolved absolute identity of the source file, so the
// caller matches a buffer regardless of how it opened it — we don't mess with identities.
// reader: read a u32 — C0DEFACE = section follows, 0 = clean end, EOF-before-either = truncated.
// SILVER_FMT_* live in the Au header: the reader is in Au.c and a version
// that moved on only one side is exactly the failure this prevents

static void fmt_u32(FILE* f, u32 v) { fwrite(&v, 4, 1, f); }

typedef struct fmt_sec { char* path; unsigned char* buf; unsigned len; i64 mtime; } fmt_sec;
static fmt_sec* fmt_secs = null;
static int      fmt_nsec = 0;
static int      fmt_cap  = 0;

static void fmt_put(symbol path, unsigned char* buf, unsigned len, i64 mtime) {
    for (int i = 0; i < fmt_nsec; i++)
        if (strcmp(fmt_secs[i].path, path) == 0) {
            free(fmt_secs[i].buf);
            fmt_secs[i].buf = buf;
            fmt_secs[i].len = len;
            fmt_secs[i].mtime = mtime;
            return;
        }
    if (fmt_nsec == fmt_cap) {
        fmt_cap  = fmt_cap ? fmt_cap * 2 : 32;
        fmt_secs = realloc(fmt_secs, fmt_cap * sizeof(fmt_sec));
    }
    fmt_secs[fmt_nsec].path  = strdup(path);
    fmt_secs[fmt_nsec].buf   = buf;
    fmt_secs[fmt_nsec].len   = len;
    fmt_secs[fmt_nsec].mtime = mtime;
    fmt_nsec++;
}

// true if the format map already holds this file's section AND its stored mtime equals
// the file on disk — i.e. nothing changed, so re-tokenizing it would be a no-op.
static bool fmt_current(symbol path, i64 mtime) {
    if (mtime == 0) return false;
    for (int i = 0; i < fmt_nsec; i++)
        if (strcmp(fmt_secs[i].path, path) == 0)
            return fmt_secs[i].mtime == mtime;
    return false;
}

// serialize one parsed fmt_file (from Au's shared read_format) back into the exact .f
// section bytes, so silver_fmt_end can re-emit an unchanged file's section verbatim.
static unsigned char* fmt_serialize(fmt_file ff, u32* out_total) {
    cstr cp = (ff->source && len(ff->source)) ? ff->source->chars : "";
    u32  cl = (u32)strlen(cp);
    // lines is a vector of vector fmt_token
    vector  lv     = (vector)ff->lines;
    u32     nlines = lv ? (u32)lv->count : 0;
    vector* lns    = lv ? (vector*)lv->origin : null;
    u32  n = 0;
    for (u32 L = 0; L < nlines; L++) n += (u32)lns[L]->count;
    cstr* dp = calloc(n ? n : 1, sizeof(cstr));
    u32   np = 0;
    for (u32 L = 0; L < nlines; L++) {
        u32        nt  = (u32)lns[L]->count;
        fmt_token* tks = (fmt_token*)lns[L]->origin;
        for (u32 t = 0; t < nt; t++) {
            fmt_token tk = tks[t];
            if (!tk->decl_source) continue;
            bool have = false;
            for (u32 i = 0; i < np; i++) if (strcmp(dp[i], tk->decl_source->chars) == 0) { have = true; break; }
            if (!have) dp[np++] = tk->decl_source->chars;
        }
    }
    u32 ptotal = 4;
    for (u32 i = 0; i < np; i++) ptotal += 4 + (u32)strlen(dp[i]);

    u32  total = 4 + 4 + cl + 8 + ptotal + 4 + nlines * 4 + n * 20;
    unsigned char* b = malloc(total);
    u32 o = 0;
    #define SP(v) do { u32 _v = (u32)(v); memcpy(b + o, &_v, 4); o += 4; } while (0)
    SP(SILVER_FMT_SECTION);
    SP(cl);
    memcpy(b + o, cp, cl); o += cl;
    i64 mt = ff->mtime; memcpy(b + o, &mt, 8); o += 8;
    SP(np);
    for (u32 i = 0; i < np; i++) {
        u32 pl = (u32)strlen(dp[i]);
        SP(pl);
        memcpy(b + o, dp[i], pl); o += pl;
    }
    SP(nlines);
    for (u32 L = 0; L < nlines; L++) {
        u32        nt  = (u32)lns[L]->count;
        fmt_token* tks = (fmt_token*)lns[L]->origin;
        SP(nt);
        for (u32 t = 0; t < nt; t++) {
            fmt_token tk = tks[t];
            i32 px = 0;
            if (tk->decl_source)
                for (u32 i = 0; i < np; i++)
                    if (strcmp(dp[i], tk->decl_source->chars) == 0) { px = (i32)i + 1; break; }
            SP(tk->column); SP(tk->length); SP(tk->syntax);
            SP(px); SP(tk->decl_line);
        }
    }
    #undef SP
    free(dp);
    *out_total = total;
    return b;
}

// load the caller-provided map into memory (via Au's single shared read_format) so an
// unchanged module's section is reused and re-emitted as-is instead of rebuilt.
static void fmt_load(silver a) {
    for (int i = 0; i < fmt_nsec; i++) { free(fmt_secs[i].path); free(fmt_secs[i].buf); }
    fmt_nsec = 0;
    if (!a->format || !len(a->format)) return;
    vector files  = (vector)read_format(a->format);
    u32    nf     = files ? (u32)files->count : 0;
    Au*    fitems = files ? (Au*)files->origin : null;
    for (u32 fx = 0; fx < nf; fx++) {
        fmt_file ff = (fmt_file)fitems[fx];
        if (!ff->source || !len(ff->source)) continue;
        // a section for a moved/deleted file never re-tokenizes: drop it
        struct stat st;
        if (stat(ff->source->chars, &st) != 0) continue;
        u32 total = 0;
        unsigned char* b = fmt_serialize(ff, &total);
        fmt_put(ff->source->chars, b, total, ff->mtime);
    }
}

void silver_fmt_header(silver a) { fmt_load(a); }

void silver_fmt_end(silver a) {
    if (!a->format || !len(a->format)) return;
    // write to a sibling temp file, then atomically rename it into place. a reader
    // (orbiter's fmt_poll) therefore only ever opens a COMPLETE map — never a half-written
    // one mid-truncate, which showed up as random / missing syntax highlighting.
    path tmp = form(path, "%o.tmp", a->format);
    FILE* f = fopen(tmp->chars, "wb");
    if (!f) return;
    fmt_u32(f, SILVER_FMT_MAGIC);
    fmt_u32(f, SILVER_FMT_VERSION);
    for (int i = 0; i < fmt_nsec; i++)
        fwrite(fmt_secs[i].buf, 1, fmt_secs[i].len, f);
    fmt_u32(f, SILVER_FMT_END);
    fclose(f);
    rename(tmp->chars, a->format->chars);
}

void silver_write_fmt(silver a, array toks) {
    if (!a->format || !a->format->count || !toks || !toks->count) return;
    path src = null;
    each(toks, token, t) if (t->source) { src = t->source; break; }
    if (!src) src = a->module_file;
    if (!src) return;
    path canon = absolute(src);                 // realpath: absolute + symlinks resolved
    cstr cp = (canon && len(canon)) ? canon->chars : src->chars;
    u32  cl = (u32)strlen(cp);
    i64  mt = modified_time((canon && len(canon)) ? canon : src);  // source mtime (ms)
    // unchanged file: the cached section from fmt_load is current — keep it, don't
    // overwrite with the (possibly under-classified) tokens of a skipped-body parse.
    if (fmt_current(cp, mt)) return;
    u32  n  = (u32)len(toks);

    // EXTERNALS. a token whose identifier resolves to a declaration in
    // another file: record which token, where it lives, and its kind.
    // never the Au_t itself -- the whole point is that jumping needs a
    // file and a line, and serializing type records is what costs
    // a POTENTIAL declaration per token, resolved by name: a type, or a
    // member of this module (which covers free functions). the map only
    // ever offers a candidate -- the editor is free to find nothing
    // declaring files interned once; each token names one by index. the
    // Au_t was stamped on the token as it parsed -- nothing resolves here
    cstr* dp = calloc(n ? n : 1, sizeof(cstr));
    u32   np = 0;
    each(toks, token, t) {
        Au_t d = t->decl;
        if (!d || !d->source || d->src_line <= 0) continue;
        bool have = false;
        for (u32 i = 0; i < np; i++) if (strcmp(dp[i], d->source) == 0) { have = true; break; }
        if (!have) dp[np++] = d->source;
    }
    u32 ptotal = 4;
    for (u32 i = 0; i < np; i++) ptotal += 4 + (u32)strlen(dp[i]);

    // line-major: bucket the tokens by their 1-based source line
    u32 nlines = 0;
    each(toks, token, t) if (t->line >= 1 && (u32)t->line > nlines) nlines = (u32)t->line;
    u32* lcount = calloc(nlines ? nlines : 1, 4);
    u32  nv = 0;
    each(toks, token, t) if (t->line >= 1) { lcount[t->line - 1]++; nv++; }

    u32  total = 4 + 4 + cl + 8 + ptotal + 4 + nlines * 4 + nv * 20;
    unsigned char* b = malloc(total);
    u32 o = 0;
    #define FMT_PUT(v) do { u32 _v = (u32)(v); memcpy(b + o, &_v, 4); o += 4; } while (0)
    FMT_PUT(SILVER_FMT_SECTION);
    FMT_PUT(cl);
    memcpy(b + o, cp, cl); o += cl;
    memcpy(b + o, &mt, 8); o += 8;              // i64 source mtime
    FMT_PUT(np);
    for (u32 i = 0; i < np; i++) {
        u32 pl = (u32)strlen(dp[i]);
        FMT_PUT(pl);
        memcpy(b + o, dp[i], pl); o += pl;
    }
    FMT_PUT(nlines);
    // records are variable-length: place each line's count now, then
    // drop tokens into their line's slots in one pass over toks
    u32* tpos = malloc((nlines ? nlines : 1) * sizeof(u32));
    for (u32 L = 0; L < nlines; L++) {
        memcpy(b + o, &lcount[L], 4);
        tpos[L] = o + 4;
        o += 4 + lcount[L] * 20;
    }
    each(toks, token, t) {
        if (t->line < 1) continue;
        Au_t d = t->decl;
        u32  px = 0, dln = 0;                   // 1-based; 0 = resolves nowhere
        if (d && d->source && d->src_line > 0) {
            for (u32 i = 0; i < np; i++)
                if (strcmp(dp[i], d->source) == 0) { px = i + 1; break; }
            dln = (u32)d->src_line;
        }
        u32 rec[5] = { (u32)t->column, (u32)len(t), (u32)t->syntax, px, dln };
        memcpy(b + tpos[t->line - 1], rec, 20);
        tpos[t->line - 1] += 20;
    }
    #undef FMT_PUT
    free(tpos);
    free(lcount);
    free(dp);
    fmt_put(cp, b, total, mt);
}

// run a live app by default (build+run when invoked directly). recovers is_live for
// compile silver-host.c into build_dir/name (the live-app launcher binary) and
// return that path. never cached — recompiled every build so a host source edit
// (silver-host.c) always takes. callers own their own guard + symlink/live_binary.
static path build_silver_host(silver a) {
    path host_src = f(path, "%s/src/silver-host.c", SILVER);
    string share_name = silver_install_name(a);
    // app_ext is "" on unix and ".exe" here; exec cannot find it without one
    path host_dst = f(path, "%o/%o%s", a->build_dir, a->name, app_ext);
    verify(file_exists("%o", host_src), "silver-host.c not found at %o", host_src);
#ifdef __APPLE__
    cstr host_libs = "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -lAu -lglfw3 -lm -framework Cocoa -framework IOKit -framework CoreFoundation -framework CoreGraphics -framework QuartzCore";
#elif defined(_WIN32)
    // glfw's win32 backend; no dl/X11/m here
    // -lAu: the posix shims (dlopen, backtrace, basename, ...) live in posix.obj
    cstr host_libs = "-lAu -lglfw3 -lgdi32 -lopengl32 -luser32 -lkernel32 -lshell32";
#else
    cstr host_libs = "-lAu -ldl -lglfw3 -lX11 -lm";
#endif
#ifdef _WIN32
    // the define cannot use shell quoting: nothing expands it before
    // CreateProcess, so the quotes are escaped for the command line instead.
    // the vk target enters at WinMain, so it links /SUBSYSTEM:WINDOWS and no
    // console opens beside the app; the console target keeps the default
    cstr subsystem = ((aether)a)->is_live ? "-Wl,/SUBSYSTEM:WINDOWS" : "";
    // link to a temp, then replace: a running app or a mid-scan file locks
    // the exe against in-place relink (LNK1168). the temp always writes.
    path host_out = f(path, "%o.new%i", host_dst, (i32)getpid());
    vexec(a->verbose, "silver-host", "%s/install/bin/clang %s %s -o %o %o %s %s -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -I%s/install/include -L%s/install/lib -DSILVER_ROOT=\\\"%s\\\" -DSILVER_SHARE_NAME=\\\"%o\\\"",
        SILVER, a->debug ? "-O0 -g" : "-O2", a->asan ? "-fsanitize=address -shared-libasan" : "", host_out, host_src, host_libs, subsystem, SILVER, SILVER, SILVER, SILVER, share_name);
    verify(au_replace_file(host_out->chars, host_dst->chars) == 0,
        "could not replace %o: locked by another process", host_dst);
#else
    // libAu resolves by soname: the tree's lib/ here, /usr/lib/<app>/ packaged
    vexec(a->verbose, "silver-host", "%s/install/bin/clang %s %s -o %o %o %s -I%s/install/include -L%s/install/lib -Wl,-rpath,%s/install/lib -Wl,-rpath,'$ORIGIN/../lib/%o' -DSILVER_ROOT='\"%s\"' -DSILVER_SHARE_NAME='\"%o\"'",
        SILVER, a->debug ? "-O0 -g" : "-O2", a->asan ? "-fsanitize=address -shared-libasan" : "", host_dst, host_src, host_libs, SILVER, SILVER, SILVER, a->name, SILVER, share_name);
#endif
    return host_dst;
}

// CACHED builds — the host binary (build_dir/name) persists from a prior build, so a
// fully-cached `silver <app>` still runs instead of silently building and exiting.
// execvp replaces this process; returns only when there's nothing to run (library /
// external sub-module / no host binary). windows cannot replace a process, so there
// it runs the app as a child and exits with its code — and the child is held in a
// kill-on-close job, so quitting silver quits the app with it.
// build-session lock (install/build/.silver.lock); held from init to run
static int build_lock_fd = -1;

#ifdef _WIN32
// the app cannot print to a console: it is linked /SUBSYSTEM:WINDOWS and its
// descriptors come back -2 (a live fd with nothing behind it), so its writes
// fail EBADF wherever they are made. it logs to install/tmp/<app>.log, and
// THIS process owns the console -- so tail that file while the app runs. this
// is what puts the app's output on screen the way an exec'd app does on unix
static void* live_log_tail(void* arg) {
    const char* path = (const char*)arg;
    long        pos  = 0;
    char        buf[4096];
    for (;;) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long end = ftell(f);
            if (end < pos) pos = 0;          // truncated: the app restarted it
            if (end > pos) {
                fseek(f, pos, SEEK_SET);
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
                    fwrite(buf, 1, n, stdout);
                fflush(stdout);
                pos = end;
            }
            fclose(f);
        }
        usleep(50000);
    }
    return null;
}
#endif


// the dylib closure of an ios binary into Frameworks/: every dependency
// from the device tree or the build dir rewritten to @rpath/<leaf>
static void ios_bundle_dylibs(silver a, path bin, path fw, array done) {
    path   root  = f(path, "%s/platform/%o", SILVER, target_dir(a));
    string out   = command_run((command)f(string, "otool -L %o", bin), false);
    array  lines = split(out, "\n");
    for (int i = 1; i < len(lines); i++) {
        string ln = trim((string)lines->origin[i]);
        int sp = index_of(ln, " (");
        if (sp <= 0) continue;
        string dep  = mid(ln, 0, sp);
        path   src  = null;
        string leaf = null;
        if (starts_with(dep, "@rpath/")) {
            leaf = mid(dep, 7, len(dep) - 7);
            path c1 = f(path, "%o/%o", a->build_dir, leaf);
            path c2 = f(path, "%o/lib/%o", root, leaf);
            src = file_exists("%o", c1) ? c1 : file_exists("%o", c2) ? c2 : null;
        } else if (dep->chars[0] == '/' && starts_with(dep, SILVER)) {
            src  = path(dep->chars);
            leaf = f(string, "%o.%o", stem(src), ext(src));
            exec(false, "install_name_tool -change %o @rpath/%o %o", dep, leaf, bin);
        }
        if (!src || !leaf) continue;
        if (index_of(done, (Au)leaf) >= 0) continue;
        push(done, (Au)leaf);
        path dst = f(path, "%o/%o", fw, leaf);
        exec(false, "cp -L %o %o && chmod u+w %o", src, dst, dst);
        exec(false, "install_name_tool -id @rpath/%o %o", leaf, dst);
        ios_bundle_dylibs(a, dst, fw, done);
    }
}

// the profile that names this phone: xcode 16 keeps them under UserData,
// older ones under MobileDevice. its entitlements sign the app
static path ios_profile(silver a, string udid, string* team) {
    cstr dirs[] = {
        "Library/Developer/Xcode/UserData/Provisioning Profiles",
        "Library/MobileDevice/Provisioning Profiles", null };
    for (int d = 0; dirs[d]; d++) {
        string found = trim(command_run((command)f(string,
            "for p in \"$HOME/%s\"/*.mobileprovision; do "
            "security cms -D -i \"$p\" 2>/dev/null | grep -q %o && echo \"$p\"; done 2>/dev/null | "
            "xargs -I{} stat -f '%%m {}' {} 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-",
            dirs[d], udid), false));
        if (!found || !len(found)) continue;
        *team = trim(command_run((command)f(string,
            "security cms -D -i \"%o\" | plutil -extract TeamIdentifier.0 raw -o - -", found), false));
        return path(found->chars);
    }
    return null;
}

// <build>/<Name>.app: the ios host, Frameworks/ with the product and its
// closure, share/<name>, the profile, then one signature per binary
static void silver_ios_bundle(silver a) {
    Device dev   = a->target;
    string name  = a->name;
    string share = silver_install_name(a);
    path   root  = f(path, "%s/platform/%o", SILVER, target_dir(a));
    path   tools = f(path, "%s/platform/native/bin", SILVER);
    path   app   = f(path, "%o/%o.app", a->build_dir, name);
    path   fw    = f(path, "%o/Frameworks", app);
    symbol triple = platform_triple(a);
    bool   sim   = strstr(a->platform->chars, "simulator") != NULL;
    string ver   = silver_release_version(a);
    if (!ver) ver = string("1.0");
    exec(false, "rm -rf %o", app);
    make_dir(fw);
    print("[%o] ios: staging %o", name, app);

    // the host: uikit's loop ticks the product's frame
    path   exe    = f(path, "%o/%o", app, name);
    string leaf   = f(string, "%o.%o", stem(a->product), ext(a->product));
    path   devlib = f(path, "%o/libsilver-devices.dylib", a->build_dir);
    verify(file_exists("%o", devlib), "ios: devices not built for %o (%o)", a->platform, devlib);
    verify(exec(a->verbose, "%o/clang -target %s -isysroot %o -fuse-ld=lld -B%o %s "
        "-I%s/devices -DSILVER_PRODUCT='\"%o\"' -DSILVER_SHARE_NAME='\"%o\"' "
        "%s/src/silver-host-ios.c %o -L%o/lib -lAu -framework UIKit -framework Foundation "
        "-Wl,-rpath,@executable_path/Frameworks -o %o",
        tools, triple, a->sysroot, tools, a->debug ? "-g" : "-O2",
        SILVER, leaf, share, SILVER, devlib, root, exe) == 0,
        "ios: host link failed");

    array done = array(alloc, 64);
    exec(false, "cp -L %o %o/%o && chmod u+w %o/%o", a->product, fw, leaf, fw, leaf);
    exec(false, "install_name_tool -id @rpath/%o %o/%o", leaf, fw, leaf);
    push(done, (Au)leaf);
    ios_bundle_dylibs(a, f(path, "%o/%o", fw, leaf), fw, done);
    ios_bundle_dylibs(a, exe, fw, done);

    path share_src = f(path, "%o/share/%o", a->install, share);
    if (dir_exists("%o", share_src)) {
        make_dir(f(path, "%o/share", app));
        exec(false, "cp -RL %o %o/share/%o", share_src, app, share);
    }

    string plist = f(string,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n"
        "  <key>CFBundleName</key><string>%o</string>\n"
        "  <key>CFBundleDisplayName</key><string>%o</string>\n"
        "  <key>CFBundleIdentifier</key><string>com.silver.%o</string>\n"
        "  <key>CFBundleExecutable</key><string>%o</string>\n"
        "  <key>CFBundlePackageType</key><string>APPL</string>\n"
        "  <key>CFBundleVersion</key><string>%o</string>\n"
        "  <key>CFBundleShortVersionString</key><string>%o</string>\n"
        "  <key>CFBundleDevelopmentRegion</key><string>en</string>\n"
        "  <key>CFBundleSupportedPlatforms</key><array><string>%s</string></array>\n"
        "  <key>DTPlatformName</key><string>%s</string>\n"
        "  <key>LSRequiresIPhoneOS</key><true/>\n"
        "  <key>MinimumOSVersion</key><string>16.0</string>\n"
        "  <key>UIDeviceFamily</key><array><integer>1</integer><integer>2</integer></array>\n"
        "  <key>UIRequiresFullScreen</key><true/>\n"
        "  <key>UILaunchScreen</key><dict/>\n"
        "  <key>UISupportedInterfaceOrientations</key><array>\n"
        "    <string>UIInterfaceOrientationPortrait</string>\n"
        "    <string>UIInterfaceOrientationLandscapeLeft</string>\n"
        "    <string>UIInterfaceOrientationLandscapeRight</string>\n"
        "  </array>\n"
        "</dict></plist>\n",
        name, name, name, name, ver, ver,
        sim ? "iPhoneSimulator" : "iPhoneOS", sim ? "iphonesimulator" : "iphoneos");
    path_save(f(path, "%o/Info.plist", app), (Au)plist, null);

    // the simulator takes an ad-hoc signature and no profile
    if (sim) {
        each(done, string, l)
            exec(false, "codesign --force --sign - %o/%o 2>/dev/null", fw, l);
        exec(false, "codesign --force --sign - %o 2>/dev/null", app);
        a->live_binary = hold(app);
        return;
    }

    // sign with the profile that lists this phone; without one the bundle
    // still stages, it just cannot install
    string udid = dev && dev->host ? dev->host : null;
    string team = null;
    path   prof = udid ? ios_profile(a, udid, &team) : null;
    if (!prof) {
        print("[%o] ios: no provisioning profile lists device %o — bundle unsigned", name, udid);
        return;
    }
    exec(false, "cp \"%o\" %o/embedded.mobileprovision", prof, app);
    path ents = f(path, "%o/%o.entitlements", a->build_dir, name);
    exec(false, "security cms -D -i \"%o\" | plutil -extract Entitlements xml1 -o %o -", prof, ents);
    string ident = a->sign && len(a->sign) ? a->sign : trim(command_run((command)f(string,
        "security find-identity -v -p codesigning 2>/dev/null | grep 'Apple Development' | "
        "grep '%o' | head -1 | sed 's/.*\"\\(.*\\)\"/\\1/'", team ? team : string("")), false));
    if (!ident || !len(ident))
        ident = trim(command_run((command)string(
            "security find-identity -v -p codesigning 2>/dev/null | grep 'Apple Development' | "
            "head -1 | sed 's/.*\"\\(.*\\)\"/\\1/'"), false));
    verify(ident && len(ident), "ios: no 'Apple Development' identity in the keychain");
    print("[%o] ios: signing as %o (team %o)", name, ident, team);
    cstr quiet = a->verbose ? "" : " 2>/dev/null";
    each(done, string, l)
        verify(exec(a->verbose, "codesign --force --sign \"%o\" %o/%o%s", ident, fw, l, quiet) == 0,
            "ios: codesign failed for %o", l);
    verify(exec(a->verbose, "codesign --force --sign \"%o\" --entitlements %o %o%s",
        ident, ents, app, quiet) == 0, "ios: codesign failed for %o", app);
    a->live_binary = hold(app);
}

// ---- android: an apk is a zip with a binary manifest and a v2 signature
// block. both are written here; openssl does the rsa part, as it does for
// the keys, and nothing of the android sdk is needed beyond the ndk

typedef struct { u8* p; size_t n, cap; } bytes;

static void bput(bytes* b, const void* d, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 1024; b->p = realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void b16(bytes* b, u32 v) { u8 d[2] = { (u8)v, (u8)(v >> 8) }; bput(b, d, 2); }
static void b32(bytes* b, u32 v) { u8 d[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) }; bput(b, d, 4); }
static void b64(bytes* b, u64 v) { b32(b, (u32)v); b32(b, (u32)(v >> 32)); }
static void bfix32(bytes* b, size_t at, u32 v) { u8* d = b->p + at; d[0] = v; d[1] = v >> 8; d[2] = v >> 16; d[3] = v >> 24; }
static void bpad(bytes* b, size_t al) { u8 z = 0; while (b->n % al) bput(b, &z, 1); }
static bool bload(bytes* b, path p) {
    FILE* f = fopen(p->chars, "rb");
    if (!f) return false;
    u8 buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) bput(b, buf, n);
    fclose(f);
    return true;
}

static u32 crc32_of(const u8* d, size_t n) {
    static u32 t[256];
    if (!t[1]) for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
    }
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) c = t[(c ^ d[i]) & 0xff] ^ (c >> 8);
    return ~c;
}

static void sha256(const u8* d, size_t n, u8 out[32]) {
    static const u32 K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    u32 h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    // the padded message: length, 0x80, zeros, the bit length big-endian
    size_t total = ((n + 9 + 63) / 64) * 64;
    u8*    m     = calloc(1, total);
    memcpy(m, d, n);
    m[n] = 0x80;
    u64 bits = (u64)n * 8;
    for (int i = 0; i < 8; i++) m[total - 1 - i] = (u8)(bits >> (8 * i));
    #define R(x, k) (((x) >> (k)) | ((x) << (32 - (k))))
    for (size_t off = 0; off < total; off += 64) {
        u32 w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (u32)m[off + 4*i] << 24 | (u32)m[off + 4*i + 1] << 16 | (u32)m[off + 4*i + 2] << 8 | m[off + 4*i + 3];
        for (int i = 16; i < 64; i++) {
            u32 s0 = R(w[i-15], 7) ^ R(w[i-15], 18) ^ (w[i-15] >> 3);
            u32 s1 = R(w[i-2], 17) ^ R(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        u32 a = h[0], b = h[1], c = h[2], dd = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            u32 t1 = hh + (R(e, 6) ^ R(e, 11) ^ R(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
            u32 t2 = (R(a, 2) ^ R(a, 13) ^ R(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = dd + t1; dd = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += dd; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    #undef R
    free(m);
    for (int i = 0; i < 8; i++) { out[4*i] = h[i] >> 24; out[4*i+1] = h[i] >> 16; out[4*i+2] = h[i] >> 8; out[4*i+3] = h[i]; }
}

// the binary xml android reads: a string pool (attribute names carrying a
// resource id first, in id order), the id map, then the element chunks
typedef struct { cstr s; u32 id; } axml_str;
typedef struct { axml_str strs[64]; int n, nids; bytes body; } axml;
typedef struct { cstr name; int type; cstr sval; u32 ival; } axml_attr;

#define AXML_STRING 3
#define AXML_INT    0x10
#define AXML_BOOL   0x12

static int axml_index(axml* x, cstr s) {
    for (int i = 0; i < x->n; i++) if (strcmp(x->strs[i].s, s) == 0) return i;
    verify(x->n < 64, "axml: string pool exceeded");
    x->strs[x->n].s = s;
    return x->n++;
}

static void axml_elem(axml* x, cstr name, axml_attr* at, int n) {
    bytes* b = &x->body;
    b32(b, 0x00100102);
    b32(b, 36 + 20 * n);
    b32(b, 0); b32(b, 0xffffffff);
    b32(b, 0xffffffff);
    b32(b, axml_index(x, name));
    b16(b, 20); b16(b, 20); b16(b, n);
    b16(b, 0); b16(b, 0); b16(b, 0);
    for (int i = 0; i < n; i++) {
        int ni = axml_index(x, at[i].name);
        b32(b, ni < x->nids ? axml_index(x, "http://schemas.android.com/apk/res/android") : 0xffffffff);
        b32(b, ni);
        int raw = at[i].type == AXML_STRING ? axml_index(x, at[i].sval) : -1;
        b32(b, raw);
        b16(b, 8); bput(b, "\0", 1); bput(b, &(u8){ at[i].type }, 1);
        b32(b, at[i].type == AXML_STRING ? (u32)raw : at[i].type == AXML_BOOL ? (at[i].ival ? 0xffffffff : 0) : at[i].ival);
    }
}

static void axml_end(axml* x, cstr name) {
    bytes* b = &x->body;
    b32(b, 0x00100103); b32(b, 24); b32(b, 0); b32(b, 0xffffffff);
    b32(b, 0xffffffff); b32(b, axml_index(x, name));
}

static void axml_write(axml* x, bytes* out) {
    int   prefix = axml_index(x, "android");
    int   uri    = axml_index(x, "http://schemas.android.com/apk/res/android");
    bytes pool   = {0};
    for (int i = 0; i < x->n; i++) b32(&pool, 0);
    // offsets first, then utf-16 strings, each length-prefixed and 0-ended
    for (int i = 0; i < x->n; i++) {
        bfix32(&pool, 4 * i, (u32)(pool.n - 4 * x->n));
        cstr s = x->strs[i].s;
        b16(&pool, (u32)strlen(s));
        for (cstr c = s; *c; c++) b16(&pool, (u8)*c);
        b16(&pool, 0);
    }
    bpad(&pool, 4);
    bytes ns = {0};
    b32(&ns, 0x00100100); b32(&ns, 24); b32(&ns, 0); b32(&ns, 0xffffffff);
    b32(&ns, prefix);
    b32(&ns, uri);
    b32(out, 0x00080003);
    b32(out, 0);
    b16(out, 1); b16(out, 28); b32(out, 28 + pool.n);
    b32(out, x->n); b32(out, 0); b32(out, 0); b32(out, 28 + 4 * x->n); b32(out, 0);
    bput(out, pool.p, pool.n);
    b16(out, 0x180); b16(out, 8); b32(out, 8 + 4 * x->nids);
    for (int i = 0; i < x->nids; i++) b32(out, x->strs[i].id);
    bput(out, ns.p, ns.n);
    bput(out, x->body.p, x->body.n);
    ns.p[0] = 0x01; bput(out, ns.p, ns.n);
    bfix32(out, 4, out->n);
    free(pool.p); free(ns.p);
}

static void android_manifest(silver a, bytes* out, string ver) {
    axml x = {0};
    // every android: attribute used below, in resource id order
    axml_str ids[] = {
        { "label",             0x01010001 }, { "name",           0x01010003 },
        { "hasCode",           0x0101000c }, { "debuggable",     0x0101000f },
        { "exported",          0x01010010 }, { "launchMode",     0x0101001d },
        { "configChanges",     0x0101001f }, { "value",          0x01010024 },
        { "minSdkVersion",     0x0101020c }, { "targetSdkVersion", 0x01010270 },
        { "versionCode",       0x0101021b }, { "versionName",    0x0101021c },
        { "extractNativeLibs", 0x010104ea } };
    for (int i = 0; i < 13; i++) x.strs[x.n++] = ids[i];
    x.nids = x.n;
    string pkg  = f(string, "com.silver.%o", a->name);
    string host = f(string, "%o-host", a->name);
    axml_elem(&x, "manifest", (axml_attr[]) {
        { "versionCode", AXML_INT, null, 1 }, { "versionName", AXML_STRING, ver->chars },
        { "package", AXML_STRING, pkg->chars } }, 3);
    axml_elem(&x, "uses-sdk", (axml_attr[]) {
        { "minSdkVersion", AXML_INT, null, 33 }, { "targetSdkVersion", AXML_INT, null, 34 } }, 2);
    axml_end(&x, "uses-sdk");
    axml_elem(&x, "uses-permission", (axml_attr[]) {
        { "name", AXML_STRING, "android.permission.INTERNET" } }, 1);
    axml_end(&x, "uses-permission");
    axml_elem(&x, "application", (axml_attr[]) {
        { "label", AXML_STRING, a->name->chars }, { "hasCode", AXML_BOOL, null, 0 },
        { "debuggable", AXML_BOOL, null, 1 }, { "extractNativeLibs", AXML_BOOL, null, 1 } }, 4);
    // the activity is android's own NativeActivity; lib_name is the host
    axml_elem(&x, "activity", (axml_attr[]) {
        { "label", AXML_STRING, a->name->chars }, { "name", AXML_STRING, "android.app.NativeActivity" },
        { "exported", AXML_BOOL, null, 1 }, { "launchMode", AXML_INT, null, 2 },
        { "configChanges", AXML_INT, null, 0x17a0 } }, 5);
    axml_elem(&x, "meta-data", (axml_attr[]) {
        { "name", AXML_STRING, "android.app.lib_name" }, { "value", AXML_STRING, host->chars } }, 2);
    axml_end(&x, "meta-data");
    axml_elem(&x, "intent-filter", null, 0);
    axml_elem(&x, "action", (axml_attr[]) { { "name", AXML_STRING, "android.intent.action.MAIN" } }, 1);
    axml_end(&x, "action");
    axml_elem(&x, "category", (axml_attr[]) { { "name", AXML_STRING, "android.intent.category.LAUNCHER" } }, 1);
    axml_end(&x, "category");
    axml_end(&x, "intent-filter");
    axml_end(&x, "activity");
    axml_end(&x, "application");
    axml_end(&x, "manifest");
    axml_write(&x, out);
    free(x.body.p);
}

// one stored entry; its data aligned so .so files map straight from the zip
static void apk_add(bytes* zip, bytes* cd, cstr name, const u8* d, size_t n, size_t align) {
    u32    crc  = crc32_of(d, n);
    size_t at   = zip->n;
    size_t nlen = strlen(name);
    size_t pad  = (align - (at + 30 + nlen + 6) % align) % align;
    b32(zip, 0x04034b50); b16(zip, 10); b16(zip, 0); b16(zip, 0); b16(zip, 0); b16(zip, 0x21);
    b32(zip, crc); b32(zip, n); b32(zip, n); b16(zip, nlen); b16(zip, 6 + pad);
    bput(zip, name, nlen);
    b16(zip, 0xd935); b16(zip, 2 + pad); b16(zip, align);
    for (size_t i = 0; i < pad; i++) bput(zip, "\0", 1);
    bput(zip, d, n);
    b32(cd, 0x02014b50); b16(cd, 20); b16(cd, 10); b16(cd, 0); b16(cd, 0); b16(cd, 0); b16(cd, 0x21);
    b32(cd, crc); b32(cd, n); b32(cd, n); b16(cd, nlen); b16(cd, 0); b16(cd, 0);
    b16(cd, 0); b16(cd, 0); b32(cd, 0); b32(cd, at);
    bput(cd, name, nlen);
}

static void apk_eocd(bytes* b, int entries, size_t cd_size, size_t cd_off) {
    b32(b, 0x06054b50); b16(b, 0); b16(b, 0); b16(b, entries); b16(b, entries);
    b32(b, cd_size); b32(b, cd_off); b16(b, 0);
}

static void apk_chunks(bytes* digests, const u8* d, size_t n, int* count) {
    for (size_t off = 0; off < n; off += 1048576) {
        size_t len = n - off < 1048576 ? n - off : 1048576;
        bytes  c   = {0};
        bput(&c, "\xa5", 1); b32(&c, len); bput(&c, d + off, len);
        u8 h[32];
        sha256(c.p, c.n, h);
        bput(digests, h, 32);
        free(c.p);
        (*count)++;
    }
}

// the v2 block over the three zip sections. a key is made once per device
// dir with openssl; the block carries the signature and the certificate
static bool apk_sign(silver a, path root, bytes* zip, bytes* cd, bytes* out, int entries) {
    path key = f(path, "%o/sign.key", root);
    path crt = f(path, "%o/sign.crt", root);
    path tmp = f(path, "%o/build", root);
    make_dir(tmp);
    if (!file_exists("%o", key) &&
        exec(a->verbose, "openssl req -x509 -newkey rsa:2048 -nodes -days 10000 -subj /CN=silver "
             "-keyout %o -out %o 2>/dev/null", key, crt) != 0) return false;
    bytes der = {0}, pub = {0};
    if (exec(false, "openssl x509 -in %o -outform DER -out %o/sign.der", crt, tmp) != 0 ||
        exec(false, "openssl x509 -in %o -pubkey -noout | openssl pkey -pubin -outform DER -out %o/sign.pub", crt, tmp) != 0 ||
        !bload(&der, f(path, "%o/sign.der", tmp)) || !bload(&pub, f(path, "%o/sign.pub", tmp))) return false;

    // digest: 1M chunks of entries, central directory, then the eocd as it
    // would read with the directory offset pointing at this block
    bytes eocd = {0};
    apk_eocd(&eocd, entries, cd->n, zip->n);
    bytes chunks = {0};
    int   count  = 0;
    apk_chunks(&chunks, zip->p, zip->n, &count);
    apk_chunks(&chunks, cd->p, cd->n, &count);
    apk_chunks(&chunks, eocd.p, eocd.n, &count);
    bytes top = {0};
    bput(&top, "\x5a", 1); b32(&top, count); bput(&top, chunks.p, chunks.n);
    u8 digest[32];
    sha256(top.p, top.n, digest);

    bytes sd = {0};
    b32(&sd, 4 + 4 + 4 + 32); b32(&sd, 4 + 4 + 32); b32(&sd, 0x0103); b32(&sd, 32); bput(&sd, digest, 32);
    b32(&sd, 4 + der.n); b32(&sd, der.n); bput(&sd, der.p, der.n);
    b32(&sd, 0);
    path sd_file = f(path, "%o/sign.sd", tmp);
    FILE* sf = fopen(sd_file->chars, "wb");
    if (!sf) return false;
    fwrite(sd.p, 1, sd.n, sf);
    fclose(sf);
    bytes sig = {0};
    if (exec(false, "openssl dgst -sha256 -sign %o -out %o/sign.sig %o", key, tmp, sd_file) != 0 ||
        !bload(&sig, f(path, "%o/sign.sig", tmp))) return false;

    bytes signer = {0};
    b32(&signer, sd.n); bput(&signer, sd.p, sd.n);
    b32(&signer, 4 + 4 + 4 + sig.n); b32(&signer, 4 + 4 + sig.n); b32(&signer, 0x0103); b32(&signer, sig.n); bput(&signer, sig.p, sig.n);
    b32(&signer, pub.n); bput(&signer, pub.p, pub.n);
    bytes v2 = {0};
    b32(&v2, 4 + signer.n); b32(&v2, signer.n); bput(&v2, signer.p, signer.n);

    // the block: (u64 len, u32 id, value) pairs, padded to a page, its size
    // at both ends and the magic last
    size_t body = 8 + 4 + v2.n;
    size_t pad  = (4096 - (8 + body + 8 + 16) % 4096) % 4096;
    if (pad && pad < 12) pad += 4096;
    size_t size = body + (pad ? pad : 0) + 8 + 16;
    b64(out, size);
    b64(out, 4 + v2.n); b32(out, 0x7109871a); bput(out, v2.p, v2.n);
    if (pad) { b64(out, pad - 8); b32(out, 0x42726577); for (size_t i = 0; i < pad - 12; i++) bput(out, "\0", 1); }
    b64(out, size);
    bput(out, "APK Sig Block 42", 16);
    free(der.p); free(pub.p); free(eocd.p); free(chunks.p); free(top.p); free(sd.p); free(sig.p); free(signer.p); free(v2.p);
    return true;
}

// the package's lib dir is named for the abi
static cstr android_abi(silver a) {
    return strstr(platform_triple(a), "x86_64") ? "x86_64" : "arm64-v8a";
}

// the shared-object closure of a binary: every DT_NEEDED found in the
// build dir, the device's lib dir or the ndk's shared libc++, copied into
// the package. anything the api-level dir carries is the system's
static void android_bundle_libs(silver a, path bin, bytes* zip, bytes* cd, array done) {
    path   root   = f(path, "%s/platform/%o", SILVER, target_dir(a));
    symbol triple = platform_triple(a);
    char   base[64];
    snprintf(base, sizeof(base), "%s", triple);
    for (int n = strlen(base); n > 0 && isdigit(base[n - 1]); n--) base[n - 1] = 0;
    string out   = command_run((command)f(string, "%s/platform/native/bin/llvm-readelf --needed-libs %o", SILVER, bin), false);
    array  lines = split(out, "\n");
    each(lines, string, ln0) {
        string ln = trim(ln0);
        if (!ends_with(ln, ".so")) continue;
        if (index_of(done, (Au)ln) >= 0) continue;
        if (file_exists("%o/usr/lib/%s/33/%o", a->sysroot, base, ln)) continue;
        path c1 = f(path, "%o/%o", a->build_dir, ln);
        path c2 = f(path, "%o/lib/%o", root, ln);
        path c3 = f(path, "%o/usr/lib/%s/%o", a->sysroot, base, ln);
        path src = file_exists("%o", c1) ? c1 : file_exists("%o", c2) ? c2 : file_exists("%o", c3) ? c3 : null;
        if (!src) { print("[%o] android: %o not found for the package", a->name, ln); continue; }
        push(done, (Au)ln);
        bytes d = {0};
        bload(&d, src);
        apk_add(zip, cd, ((string)f(string, "lib/%s/%o", android_abi(a), ln))->chars, d.p, d.n, 16384);
        free(d.p);
        android_bundle_libs(a, src, zip, cd, done);
    }
}

// <build>/<name>.apk: the host, the product and its closure under lib/,
// share/<name> under assets/ with a list the host extracts by, the manifest
static void silver_android_bundle(silver a) {
    string name  = a->name;
    string share = silver_install_name(a);
    path   root  = f(path, "%s/platform/%o", SILVER, target_dir(a));
    path   tools = f(path, "%s/platform/native/bin", SILVER);
    path   apk   = f(path, "%o/%o.apk", a->build_dir, name);
    symbol triple = platform_triple(a);
    string ver   = silver_release_version(a);
    if (!ver) ver = string("1.0");
    print("[%o] android: staging %o", name, apk);

    // the host: NativeActivity loads it by name and it ticks the product
    string leaf   = f(string, "%o.%o", stem(a->product), ext(a->product));
    path   host   = f(path, "%o/lib%o-host.so", a->build_dir, name);
    path   devlib = f(path, "%o/libsilver-devices.so", a->build_dir);
    verify(file_exists("%o", devlib), "android: devices not built for %o (%o)", a->platform, devlib);
    cstr habi = strstr(triple, "x86_64") ? "x86_64-linux-android" : "aarch64-linux-android";
    verify(exec(a->verbose, "%o/clang -target %s --sysroot=%o -fuse-ld=lld -B%o %s %s -shared -fPIC "
        "-ftls-model=global-dynamic -Wl,-soname,lib%o-host.so -I%s/devices -DSILVER_PRODUCT='\"%o\"' -DSILVER_SHARE_NAME='\"%o\"' "
        "%s/src/silver-host-android.c %o -L%o/lib -L%o/usr/lib/%s/33 -L%o/usr/lib/%s -lAu -landroid -llog -o %o",
        tools, triple, a->sysroot, tools, a->debug ? "-g" : "-O2", platform_abi_link(a),
        name, SILVER, leaf, share, SILVER, devlib, root, a->sysroot, habi, a->sysroot, habi, host) == 0,
        "android: host link failed");

    bytes zip = {0}, cd = {0};
    int   entries = 0;
    bytes man = {0};
    android_manifest(a, &man, ver);
    apk_add(&zip, &cd, "AndroidManifest.xml", man.p, man.n, 4); entries++;
    free(man.p);

    array done = array(alloc, 64);
    bytes d = {0};
    bload(&d, host);
    apk_add(&zip, &cd, ((string)f(string, "lib/%s/lib%o-host.so", android_abi(a), name))->chars, d.p, d.n, 16384); entries++;
    free(d.p);
    push(done, (Au)f(string, "lib%o-host.so", name));
    d = (bytes){0};
    bload(&d, a->product);
    apk_add(&zip, &cd, ((string)f(string, "lib/%s/%o", android_abi(a), leaf))->chars, d.p, d.n, 16384); entries++;
    free(d.p);
    push(done, (Au)leaf);
    int before = len(done);
    android_bundle_libs(a, a->product, &zip, &cd, done);
    android_bundle_libs(a, host, &zip, &cd, done);
    entries += len(done) - before;

    // share/<name>: the asset dir lists no subdirectories, so a list goes
    // with it, and a stamp tells the host when to extract again
    path share_src = f(path, "%o/share/%o", a->install, share);
    if (dir_exists("%o", share_src)) {
        string list  = command_run((command)f(string, "cd %o && find . -type f | sort | sed 's|^\\./||'", share_src), false);
        array  files = split(list, "\n");
        bytes  ls    = {0};
        each(files, string, rel) {
            if (!len(rel)) continue;
            bytes fd = {0};
            bload(&fd, f(path, "%o/%o", share_src, rel));
            apk_add(&zip, &cd, ((string)f(string, "assets/share/%o/%o", share, rel))->chars, fd.p, fd.n, 4); entries++;
            free(fd.p);
            bput(&ls, rel->chars, len(rel)); bput(&ls, "\n", 1);
        }
        apk_add(&zip, &cd, "assets/share.list", ls.p, ls.n, 4); entries++;
        free(ls.p);
    }
    string stamp = f(string, "%o-%i", ver, (i32)time(null));
    apk_add(&zip, &cd, "assets/share.stamp", (u8*)stamp->chars, len(stamp), 4); entries++;

    bytes block = {0};
    verify(apk_sign(a, root, &zip, &cd, &block, entries), "android: signing failed — is openssl installed?");
    FILE* f = fopen(apk->chars, "wb");
    verify(f, "android: cannot write %o", apk);
    fwrite(zip.p, 1, zip.n, f);
    fwrite(block.p, 1, block.n, f);
    fwrite(cd.p, 1, cd.n, f);
    bytes eocd = {0};
    apk_eocd(&eocd, entries, cd.n, zip.n + block.n);
    fwrite(eocd.p, 1, eocd.n, f);
    fclose(f);
    free(zip.p); free(cd.p); free(block.p); free(eocd.p);
    a->live_binary = hold(apk);
}

static void silver_mobile_bundle(silver a) {
    if (target_is_android(a)) silver_android_bundle(a); else silver_ios_bundle(a);
}

// push to the device and start it there. ssh owns the credentials — the
// device names a host ALIAS (~/.ssh/config), never a user or a password.
// a device with no host is a build target only
static void device_run(silver a) {
    Device dev = a->target;
    if (!dev) return;
    // adb finds the phone itself; host is a serial only when several are on
    if (target_is_android(a)) {
        if (build_lock_fd >= 0) { flock(build_lock_fd, LOCK_UN); close(build_lock_fd); build_lock_fd = -1; }
        path   apk = f(path, "%o/%o.apk", a->build_dir, a->name);
        path   sdk = f(path, "%s/platform/%o/sdk", SILVER, target_dir(a));
        string adb = f(string, "%o/platform-tools/adb%s%o", sdk,
            dev->host && len(dev->host) ? " -s " : "", dev->host && len(dev->host) ? dev->host : string(""));
        if (!file_exists("%o", apk)) { print("[%o] android: no package at %o", a->name, apk); a->error = true; return; }
        // the emulator: its avd is written here the first time, then it is
        // started when none is running, and waited for until android is up
        if (strstr(a->platform->chars, "sim")) {
            path avd = f(path, "%s/platform/%o/avd/silver.avd", SILVER, target_dir(a));
            if (!dir_exists("%o", avd)) {
                make_dir(avd);
                cstr abi = android_abi(a);
                path_save(f(path, "%o/../silver.ini", avd), (Au)f(string,
                    "avd.ini.encoding=UTF-8\npath=%o\npath.rel=avd/silver.avd\ntarget=android-34\n", avd), null);
                path_save(f(path, "%o/config.ini", avd), (Au)f(string,
                    "AvdId=silver\navd.ini.displayname=silver\navd.ini.encoding=UTF-8\n"
                    "abi.type=%s\nhw.cpu.arch=%s\nhw.cpu.ncore=4\nhw.ramSize=2048\n"
                    "image.sysdir.1=system-images/android-34/google_apis/%s/\n"
                    "tag.id=google_apis\ntag.display=Google APIs\nPlayStore.enabled=no\n"
                    "hw.lcd.width=1080\nhw.lcd.height=2400\nhw.lcd.density=420\n"
                    "hw.gpu.enabled=yes\nhw.gpu.mode=host\nhw.keyboard=yes\nhw.sdCard=no\n"
                    "hw.audioInput=no\ndisk.dataPartition.size=4G\nfastboot.forceColdBoot=no\n",
                    abi, strcmp(abi, "x86_64") == 0 ? "x86_64" : "arm64", abi), null);
            }
            if (exec(false, "%o devices 2>/dev/null | grep -q '^emulator-'", adb) != 0) {
                print("[%o] starting the emulator", a->name);
                // swiftshader renders in software, so this boots the same with
                // or without a gpu and a display; a dev still gets a window
                exec(false, "ANDROID_SDK_ROOT=%o ANDROID_AVD_HOME=%o/.. %o/emulator/emulator -avd silver "
                     "-gpu swiftshader_indirect -no-boot-anim -no-snapshot-save "
                     "> %o/../emulator.log 2>&1 &", sdk, avd, sdk, avd);
            }
            if (exec(false, "%o wait-for-device shell 'while [ \"$(getprop sys.boot_completed 2>/dev/null | tr -d \"\\r\")\" != \"1\" ]; "
                     "do sleep 1; done'", adb) != 0) {
                print("[%o] android: the emulator did not come up — see platform/%o/emulator.log", a->name, target_dir(a));
                a->error = true;
                return;
            }
        }
        print("[%o] installing on %s", a->name, strstr(a->platform->chars, "sim") ? "the emulator" : "the phone");
        if (exec(a->verbose, "%o install -r %o", adb, apk) != 0) {
            print("[%o] android: install failed — is a phone plugged in with usb debugging on?", a->name);
            a->error = true;
            return;
        }
        print("[%o] starting", a->name);
        exec(a->verbose, "%o shell am start -n com.silver.%o/android.app.NativeActivity", adb, a->name);
        // its log, from the moment it has a pid
        exec(false, "p=''; for i in 1 2 3 4 5 6 7 8 9 10; do p=$(%o shell pidof -s com.silver.%o 2>/dev/null | tr -d '\\r'); "
             "[ -n \"$p\" ] && break; sleep 1; done; [ -n \"$p\" ] && %o logcat --pid=$p", adb, a->name, adb);
        return;
    }
    if (!dev->host || !len(dev->host)) return;
    // the app runs for as long as it likes: the build lock goes first
    if (build_lock_fd >= 0) { flock(build_lock_fd, LOCK_UN); close(build_lock_fd); build_lock_fd = -1; }
    // an iphone installs and launches through devicectl; host is its udid
    if (a->platform && strstr(a->platform->chars, "ios")) {
        path app = f(path, "%o/%o.app", a->build_dir, a->name);
        if (!dir_exists("%o", app)) { print("[%o] ios: no bundle at %o", a->name, app); a->error = true; return; }
        // the simulator: host names a device or 'booted'
        if (strstr(a->platform->chars, "simulator")) {
            if (cmp(dev->host, "booted") != 0) exec(false, "xcrun simctl boot %o 2>/dev/null", dev->host);
            exec(false, "open -a Simulator");
            print("[%o] installing on simulator %o", a->name, dev->host);
            if (exec(a->verbose, "xcrun simctl install %o %o", dev->host, app) != 0) {
                print("[%o] ios: simulator install failed — is one booted?", a->name);
                a->error = true;
                return;
            }
            print("[%o] starting on simulator %o", a->name, dev->host);
            exec(a->verbose, "xcrun simctl launch --console %o com.silver.%o", dev->host, a->name);
            return;
        }
        print("[%o] installing on %o", a->name, dev->host);
        if (exec(a->verbose, "xcrun devicectl device install app --device %o %o", dev->host, app) != 0) {
            print("[%o] ios: install failed — is the phone unlocked and trusted?", a->name);
            a->error = true;
            return;
        }
        print("[%o] starting on %o", a->name, dev->host);
        exec(a->verbose, "xcrun devicectl device process launch --console --device %o com.silver.%o",
            dev->host, a->name);
        return;
    }
    path root = dev->root ? dev->root : (path)f(path, "~/silver");
    print("[%o] sending to %o", a->name, dev->host);
    if (exec(a->verbose, "ssh %o 'mkdir -p %o'", dev->host, root) != 0 ||
        exec(a->verbose, "rsync -az %o %o:%o/", a->product, dev->host, root) != 0) {
        print("[%o] cannot reach device '%o' over ssh (%o)", a->name, a->device, dev->host);
        a->error = true;
        return;
    }
    // whatever runs there now is the previous build of this app
    string stopc = (dev->stop && len(dev->stop)) ? hold(dev->stop) :
        f(string, "pkill -x %o 2>/dev/null; true", a->name);
    exec(a->verbose, "ssh %o '%o'", dev->host, stopc);
    string runc = (dev->run && len(dev->run)) ? hold(dev->run) :
        f(string, "cd %o && ./%o", root, filename(a->product));
    print("[%o] starting on %o", a->name, dev->host);
    exec(a->verbose, "ssh %o '%o'", dev->host, runc);
}

// --lldb: silver hands you the debugger directly. locally that is lldb on
// the product; with a device it is lldb HERE driving lldb-server THERE, so
// one command gets you a session on the board. the sysroot we already
// pulled is what resolves the device's own libraries
static void device_debug(silver a) {
    Device dev  = a->target;
    path   tool = f(path, "%s/platform/native/bin/lldb", SILVER);
    if (!file_exists("%o", tool)) {
        print("[%o] no lldb at %o", a->name, tool);
        a->error = true;
        return;
    }

    symbol port = "1234";
    if (dev && dev->host && len(dev->host)) {
        path   root = dev->root ? dev->root : (path)f(path, "~/silver");
        if (exec(a->verbose, "ssh %o 'mkdir -p %o'", dev->host, root) != 0 ||
            exec(a->verbose, "rsync -az %o %o:%o/", a->product, dev->host, root) != 0) {
            print("[%o] cannot reach device '%o' over ssh (%o)", a->name, a->device, dev->host);
            a->error = true;
            return;
        }
        // the debug wire rides ssh like everything else: the server binds
        // LOOPBACK on the device and we forward the port. an open *:port is
        // an unauthenticated debug server on the network
        string srv = (dev->debugger && len(dev->debugger)) ? hold(dev->debugger) :
            f(string, "lldb-server platform --listen 127.0.0.1:%s --server", port);
        exec(false, "ssh %o 'pkill -f lldb-server 2>/dev/null; true'", dev->host);
        if (exec(a->verbose, "ssh -f %o '%o >/dev/null 2>&1 &'", dev->host, srv) != 0) {
            print("[%o] no debug server on %o — install lldb-server there, or name "
                  "one in the device's debugger: line", a->name, dev->host);
            a->error = true;
            return;
        }
        // one forwarded port; -N carries no command, -f backgrounds it
        exec(false, "pkill -f 'ssh -f -N -L %s:127.0.0.1:%s' 2>/dev/null; true", port, port);
        if (exec(a->verbose, "ssh -f -N -L %s:127.0.0.1:%s %o", port, port, dev->host) != 0) {
            print("[%o] could not forward the debug port from %o", a->name, dev->host);
            a->error = true;
            return;
        }
        // an lldb script, so the session opens already connected
        path cmds = f(path, "%o/%o.lldb", a->build_dir, a->name);
        string body = f(string,
            "platform select remote-linux\n"
            "platform connect connect://127.0.0.1:%s\n"
            "settings set target.sysroot %o\n"
            "target create %o\n",
            port, a->sysroot, a->product);
        path_save(cmds, (Au)body, null);
        print("[%o] debugging on %o", a->name, dev->host);
        char* argv[] = { tool->chars, "-s", cmds->chars, NULL };
        execvp(argv[0], argv);
        fprintf(stderr, "could not start %s\n", tool->chars);
        _exit(1);
    }
    print("[%o] debugging here", a->name);
    char* argv[] = { tool->chars, a->product->chars, NULL };
    execvp(argv[0], argv);
    fprintf(stderr, "could not start %s\n", tool->chars);
    _exit(1);
}

// a module that exports `dependencies [ 'app' ]` plugs into that app at
// runtime (the export registry names it, nothing imports it), so the app's
// build carries it: built here as an external of this instance, the way an
// import is, and silver's cache returns at once when it is current
static void build_dependents(silver a) {
    if (a->is_external || a->target) return;
    path edir = f(path, "%o/export", a->install);
    DIR* d = opendir(edir->chars);
    if (!d) return;
    string tag = f(string, "'%o'", a->name);
    struct dirent* e;
    while ((e = readdir(d))) {
        char* ext = strrchr(e->d_name, '.');
        if (!ext || strcmp(ext, ".agi") != 0) continue;
        path   ef  = f(path, "%o/%s", edir, e->d_name);
        string txt = (string)load(ef, typeid(string), null);
        if (!txt) continue;
        char* ln = strstr(txt->chars, "dependencies:");
        if (!ln || (ln != txt->chars && ln[-1] != '\n')) continue;
        char* eol = strchr(ln, '\n');
        if (eol) *eol = 0;
        if (!strstr(ln, tag->chars)) continue;
        // registry stem is owner-qualified (silver-scenes); the module dir is the tail
        char stem[256];
        snprintf(stem, sizeof(stem), "%.*s", (int)(ext - e->d_name), e->d_name);
        char* dash = strchr(stem, '-');
        cstr nm = dash ? dash + 1 : stem;
        path mdir = f(path, "%o/%s", a->src_loc, nm);
        if (!dir_exists("%o", mdir)) continue;
        silver dep = silver(module, mdir, breakpoint, a->breakpoint,
            verbose, a->verbose, is_external, a, is_child, a, release, a->release,
            clean, a->clean, format, a->format,
            debug_type, a->debug_type, debugmember, a->debugmember);
        if (dep && dep->error) a->error = true;
    }
    closedir(d);
}

// --uninstall: everything the build wrote for this module goes — products,
// registry, share, syntax map. the imports were walked first, each dep
// removing itself by its manifest, so this is the last step before exit
static void uninstall_products(silver a) {
    string install_name = silver_install_name(a);
    exec(a->verbose,
        "rm -rf %o/lib%o.so %o/%o.artifacts %o/%o.product %o/%o.source "
        "%o/%o.o %o/%o.o.core*.o %o/%o.bc %o/%o.ll %o/%o "
        "%o/export/%o.agi %o/share/%o %o/share/%o %o/syntax/%o.f",
        a->build_dir, install_name, a->build_dir, install_name,
        a->build_dir, install_name, a->build_dir, install_name,
        a->build_dir, a->name, a->build_dir, a->name,
        a->build_dir, a->name, a->build_dir, a->name, a->build_dir, a->name,
        a->install, install_name, a->install, install_name,
        a->install, a->name, a->install, a->name);
    print("[%o] uninstalled", a->name);
    exit(0);
}

// a cached build never parsed the app element: the host binary beside the
// product says it is live, and that host is what tests, ships and runs
static void silver_recover_live(silver a) {
    if (((aether)a)->is_live || a->is_external) return;
    // the same app_ext the host was written with, or a cached run finds
    // nothing and silver exits 0 having neither built nor launched
    path host = f(path, "%o/%o%s", a->build_dir, a->name, app_ext);
    if (file_exists("%o", host)) {
        ((aether)a)->is_live = true;
        if (!a->live_binary) a->live_binary = hold(host);
    }
}

// the version a release ships: parsed export, else the registry, else the
// module source itself (a cached run parsed nothing and may have no registry)
static string silver_release_version(silver a) {
    string share = silver_install_name(a);
    if (a->exported_version) return string(a->exported_version->chars);
    path reg = f(path, "%o/export/%o.agi", a->install, share);
    if (file_exists("%o", reg)) {
        string v = trim(command_run((command)f(string,
            "sed -n 's/^version: *//p' %o", reg), false));
        if (len(v)) return v;
    }
    if (a->module_file && file_exists("%o", a->module_file)) {
        string v = trim(command_run((command)f(string,
            "sed -n 's/^export  *\\([0-9][0-9.]*\\).*/\\1/p' %o", a->module_file), false));
        if (len(v)) return v;
    }
    return null;
}

static void silver_live_run(silver a) {
    // a failed build must NOT launch (or relaunch) the app. execvp'ing the host on an
    // error makes the host re-trigger `silver <app>`, which fails and re-execs — the
    // infinite rebuild loop. on the first build (app never ran) we simply quit; main
    // returns non-zero so the live-host's rebuild sees the failure and aborts.
    if (a->error) {
        // silent before: a build that printed nothing still declined to launch,
        // which reads as "silver did nothing" -- say so instead
        if (!a->is_external && !a->build)
            print("[%o] not launching: the build reported an error", a->name);
        return;
    }
    build_dependents(a);
    // --lldb goes straight into a session, here or on the device. it execs
    // into lldb, so returning at all means it could not start one
    if (a->lldb && !a->is_external && !a->build) {
        device_debug(a);
        return;
    }
    // a device build runs THERE, not here — unless --build says stop at build
    if (a->target && !a->is_external) {
        if (!a->build) device_run(a);
        return;
    }
    // coverage library built directly: load it and run its exported tests
    if (((aether)a)->has_coverage && !a->is_external && !a->build) {
        void* h = dlopen(a->product->chars, RTLD_NOW);
        verify(h, "coverage: cannot load %o: %s", a->product, dlerror());
        int (*cov)(void) = (int(*)(void))dlsym(h, "silver_coverage_run");
        verify(cov, "coverage: silver_coverage_run not found in %o", a->product);
        exit(cov());
    }
    silver_recover_live(a);
    // --build compiles only: skip launching the app (a bare `silver <app>` runs it).
    if (!a->build && (a->run || (((aether)a)->is_live && !a->is_external))) {
        int n = a->run ? len(a->run) : 0;
        char** argv = calloc(n + 4, sizeof(char*));
        path run_binary = a->live_binary ? a->live_binary : a->product;
        argv[0] = run_binary->chars;
        int i = 1;
        char leaks_n[16];
        if (au_leaks()) {
            snprintf(leaks_n, sizeof(leaks_n), "%d", au_leaks());
            argv[i++] = "--leaks";
            argv[i++] = leaks_n;
        }
        if (a->run) each(a->run, Au, arg) {
            argv[i++] = cast(string, arg)->chars;
        }
        argv[i] = NULL;
        verify(run_binary && file_exists("%o", run_binary),
            "cannot launch %o: no binary at %o", a->name, run_binary);
#ifdef _WIN32
        {   // put the app's output on this console while it runs
            static char lp[512];
            snprintf(lp, sizeof(lp), "%s/%s.log", temp_dir(), a->name->chars);
            // clear it FIRST: the tail starts at offset 0, so whatever the
            // last run left behind would be replayed to the console as if the
            // app had just printed it
            FILE* clr = fopen(lp, "wb");
            if (clr) fclose(clr);
            pthread_t lt;
            if (pthread_create(&lt, 0, live_log_tail, lp) == 0) {
                pthread_detach(lt);
                // WE are the console writer now. an app that also wrote to its
                // own stdout would print every line twice -- which it can do
                // whenever it happens to inherit a usable one (a pipe, say)
                setenv("SILVER_LOG_TAIL", "1", 1);
            }
        }
#endif
        // --test: the app runs its expect tests, reports, and exits
        if (a->test) setenv("SILVER_EXPECT", "1", 1);
        // release the build lock: the app must not hold it while running
        if (build_lock_fd >= 0) {
            flock(build_lock_fd, LOCK_UN);
            (close)(build_lock_fd);
            build_lock_fd = -1;
        }
        execvp(argv[0], argv);
        fprintf(stderr, "execvp failed for %s: %s\n", argv[0], strerror(errno));
        _exit(1);
    }
    // declining to run a directly-invoked app is never the intent: name the
    // flags that stopped it rather than exiting 0 with nothing said. NOT gated
    // on is_library -- a live app sets that true, which is exactly this case
    if (!a->is_external && !a->build)
        print("[%o] not launching: is_live=%s is_library=%s run=%i live_binary=%o product=%o",
            a->name, ((aether)a)->is_live ? "true" : "false",
            a->is_library ? "true" : "false",
            a->run ? (int)len(a->run) : 0, a->live_binary, a->product);
}

// run the product with an env flag set (SILVER_EXPORT / SILVER_EXPECT):
// module init does the work and exits 0. a library has no exe to spawn,
// so it is dlopen'd and its global ctor runs the init. returns wait status
static int silver_spawn_product(silver a, path bin, bool lib, path cwd,
                                cstr env, cstr env_force) {
#ifdef _WIN32
    // no fork here, so the setup a child would do runs in THIS process and the
    // app is spawned outright. a library's module init is loaded in-process,
    // which is the one thing isolation cannot buy us
    setenv(env, "1", 1);
    if (env_force) setenv(env_force, "1", 1);
    int st = 0;
    if (lib) {
        path back = path_cwd();
        chdir(cwd->chars);
        void* h = dlopen(bin->chars, RTLD_NOW);
        chdir(back->chars);
        st = h ? 0 : (1 << 8);
        // release it: a pinned dll cannot be relinked later this run
        if (h) dlclose(h);
    } else {
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addchdir_np(&fa, cwd->chars);
        char* argv[2] = { bin->chars, NULL };
        pid_t pid = 0;
        int   sp  = posix_spawn(&pid, bin->chars, &fa, NULL, argv, environ);
        posix_spawn_file_actions_destroy(&fa);
        verify(sp == 0, "%s: cannot spawn %o: %s", env, bin, strerror(sp));
        waitpid(pid, &st, 0);
    }
    unsetenv(env);
    if (env_force) unsetenv(env_force);
#else
    pid_t pid = fork();
    if (pid == 0) {
        setenv(env, "1", 1);
        if (env_force) setenv(env_force, "1", 1);
        // as the host does: tests and exports resolve assets against the
        // launch dir through path_startup, not the share dir
        if (!getenv("SILVER_STARTUP")) setenv("SILVER_STARTUP", path_startup()->chars, 1);
        if (a->verbose) fprintf(stderr, "%s: startup %s\n", env, getenv("SILVER_STARTUP"));
        if (lib) {
            // a library has no main: module init does the work, exits 0.
            // an exe is the host: it records its launch cwd as the startup
            // and cd's to the share itself, so only a library is cd'd here
            chdir(cwd->chars);
            // an import the compiler already mapped comes back as the same
            // handle and its init never runs again: load a fresh copy
            path load = bin;
            void* pre = dlopen(bin->chars, RTLD_NOW | RTLD_NOLOAD);
            if (pre) {
                load = f(path, "%o/tmp/%s-%i.so", a->install, env, (i32)getpid());
                make_dir(f(path, "%o/tmp", a->install));
                exec(false, "cp %o %o", bin, load);
            }
            void* h = dlopen(load->chars, RTLD_NOW);
            if (!h) fprintf(stderr, "%s: dlopen %s: %s\n", env, load->chars, dlerror());
            if (pre) unlink(load->chars);
            _exit(h ? 0 : 1);
        }
        char* argv[2] = { bin->chars, NULL };
        execvp(argv[0], argv);
        _exit(1);
    }
    int st = 0;
    waitpid(pid, &st, 0);
#endif
    return st;
}


#ifdef __APPLE__
// copy the dylib closure of a Mach-O into lib/, every dependency under
// our install or build tree rewritten to @rpath/<leaf>. system libs stay
static void bundle_dylibs(silver a, path bin, path lib_dir, array done) {
    string out = command_run((command)f(string, "otool -L %o", bin), false);
    array  lines = split(out, "\n");
    for (int i = 1; i < len(lines); i++) {
        string ln = trim((string)lines->origin[i]);
        int sp = index_of(ln, " (");
        if (sp <= 0) continue;
        string dep = mid(ln, 0, sp);
        path   src = null;
        string leaf = null;
        if (starts_with(dep, "@rpath/")) {
            leaf = mid(dep, 7, len(dep) - 7);
            path c1 = f(path, "%o/lib/%o", a->install, leaf);
            path c2 = f(path, "%o/%o", a->build_dir, leaf);
            src = file_exists("%o", c1) ? c1 : file_exists("%o", c2) ? c2 : null;
        } else if (dep->chars[0] == '/' &&
                   (starts_with(dep, a->install->chars) ||
                    starts_with(dep, a->build_dir->chars))) {
            src  = path(dep->chars);
            leaf = stem(src);
            leaf = f(string, "%o.%o", leaf, ext(src));
            vexec(a->verbose, "package", "install_name_tool -change %o @rpath/%o %o",
                dep, leaf, bin);
        }
        if (!src || !leaf) continue;
        if (index_of(done, (Au)leaf) >= 0) continue;
        push(done, (Au)leaf);
        path dst = f(path, "%o/%o", lib_dir, leaf);
        // -L: the install tree links versioned names; ship real files
        vexec(a->verbose, "package", "cp -L %o %o", src, dst);
        vexec(a->verbose, "package", "chmod u+w %o", dst);
        vexec(a->verbose, "package", "install_name_tool -id @rpath/%o %o", leaf, dst);
        bundle_dylibs(a, dst, lib_dir, done);
    }
}

// --release on an app: stage <Name>.app (MacOS/, lib/, share/<name>/ —
// the same shape path_share_path and the @executable_path/../lib rpath
// already resolve), sign it, and wrap it in a .dmg under install/packages
static void silver_package(silver a) {
    if (a->error || a->is_external || !a->release || a->test) return;
    if (!a->product || !file_exists("%o", a->product)) return;
    string name  = a->name;
    string share = silver_install_name(a);
    string ver = silver_release_version(a);
    verify(ver, "--release: %o exports no version (export 1.0.0)", name);
#if defined(__aarch64__)
    cstr arch = "arm64";
#else
    cstr arch = "x86_64";
#endif
    // packages outlive the install tree: they land in the module's own
    // repository, every platform's side by side
    path pkgs  = f(path, "%o/packages", a->project_path);
    path stage = f(path, "%o/tmp/%o-stage", a->install, name);
    path app   = f(path, "%o/%o.app", stage, name);
    path cts   = f(path, "%o/Contents", app);
    path macos = f(path, "%o/MacOS", cts);
    path lib   = f(path, "%o/lib", cts);
    path res   = f(path, "%o/Resources", cts);
    path shd   = f(path, "%o/share", cts);
    make_dir(pkgs);
    vexec(a->verbose, "package", "rm -rf %o", stage);
    make_dir(macos); make_dir(lib); make_dir(res); make_dir(shd);
    print("[%o] package: staging %o", name, app);

    path exe = f(path, "%o/%o", macos, name);
    vexec(a->verbose, "package", "cp %o %o", a->product, exe);
    vexec(a->verbose, "package", "chmod u+w %o", exe);
    array done = array(alloc, 64);
    bundle_dylibs(a, exe, lib, done);
    // only the bundle-relative rpath survives: the build tree's absolute
    // ones would resolve first on this machine and hide a packaging gap
    string rp = command_run((command)f(string,
        "otool -l %o | grep -A2 LC_RPATH | grep path | awk '{print $2}'", exe), false);
    array rps = split(rp, "\n");
    each(rps, string, r) {
        string t = trim(r);
        if (len(t) && t->chars[0] == '/')
            exec(false, "install_name_tool -delete_rpath %o %o", t, exe);
    }
    exec(false, "install_name_tool -add_rpath @executable_path/../lib %o 2>/dev/null", exe);

    path share_src = f(path, "%o/share/%o", a->install, share);
    if (dir_exists("%o", share_src))
        vexec(a->verbose, "package", "cp -RL %o %o/%o", share_src, shd, share);

    // icon: <module>/images/icon.png or <module>/icon.png; a release needs one
    path icon = f(path, "%o/images/icon.png", a->module_path);
    if (!file_exists("%o", icon)) icon = f(path, "%o/icon.png", a->module_path);
    verify(file_exists("%o", icon), "--release: %o has no icon (images/icon.png)", name);
    bool has_icon = true;
    {
        path iset = f(path, "%o/icon.iconset", stage);
        make_dir(iset);
        // icon.png is the master; a hand-drawn images/icon-<16..256>.png
        // replaces the scaled copy at that size
        int sizes[] = { 16, 32, 64, 128, 256, 512, 1024 };
        path by_size[7] = { 0 };
        for (int i = 0; i < 5; i++) {
            path alt = f(path, "%o/images/icon-%i.png", a->module_path, sizes[i]);
            by_size[i] = file_exists("%o", alt) ? alt : null;
        }
        for (int i = 0; i < 6; i++) {
            path s1 = by_size[i]     ? by_size[i]     : icon;
            path s2 = by_size[i + 1] ? by_size[i + 1] : icon;
            exec(false, "sips -z %i %i %o --out %o/icon_%ix%i.png >/dev/null",
                sizes[i], sizes[i], s1, iset, sizes[i], sizes[i]);
            exec(false, "sips -z %i %i %o --out %o/icon_%ix%i@2x.png >/dev/null",
                sizes[i] * 2, sizes[i] * 2, s2, iset, sizes[i], sizes[i]);
        }
        has_icon = exec(a->verbose, "iconutil -c icns %o -o %o/%o.icns", iset, res, name) == 0;
    }

    string icon_line = has_icon ?
        f(string, "  <key>CFBundleIconFile</key><string>%o.icns</string>\n", name) : string("");
    string plist = f(string,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n"
        "  <key>CFBundleName</key><string>%o</string>\n"
        "  <key>CFBundleDisplayName</key><string>%o</string>\n"
        "  <key>CFBundleIdentifier</key><string>com.silver.%o</string>\n"
        "  <key>CFBundleExecutable</key><string>%o</string>\n"
        "  <key>CFBundlePackageType</key><string>APPL</string>\n"
        "  <key>CFBundleVersion</key><string>%o</string>\n"
        "  <key>CFBundleShortVersionString</key><string>%o</string>\n"
        "%s"
        "  <key>LSMinimumSystemVersion</key><string>12.0</string>\n"
        "  <key>NSHighResolutionCapable</key><true/>\n"
        "</dict></plist>\n",
        name, name, name, name, ver, ver, icon_line->chars);
    path_save(f(path, "%o/Info.plist", cts), (Au)plist, null);

    // ad-hoc signing runs locally; a Developer ID (--sign) ships. the
    // hardened runtime is what notarization checks for, and its library
    // validation rejects ad-hoc dylibs, so it comes only with an identity
    // a Developer ID in the keychain signs by itself; --sign overrides
    string ident = a->sign && len(a->sign) ? a->sign : trim(command_run((command)string(
        "security find-identity -v -p codesigning 2>/dev/null | "
        "grep 'Developer ID Application' | head -1 | sed 's/.*\"\\(.*\\)\"/\\1/'"), false));
    bool signed_ = ident && len(ident);
    verify(signed_, "--release: no Developer ID Application certificate in the keychain "
        "(developer.apple.com > Certificates)");
    cstr id      = signed_ ? ident->chars : "-";
    if (signed_) print("[%o] package: signing as %s", name, id);
    // --deep skips Contents/lib (not a standard nested-code dir), and the
    // hardened runtime's library validation rejects an unsigned dylib
    // one codesign process for all of it: the keychain asks once per process
    cstr   opts  = signed_ ? "--options runtime --timestamp" : "";
    string paths = string(alloc, 1024);
    each(done, string, leaf)
        concat(paths, f(string, "%o/%o ", lib, leaf));
    cstr quiet = a->verbose ? "" : " 2>/dev/null";
    verify(exec(a->verbose, "codesign --force %s --sign \"%s\" %o%o%s",
        opts, id, paths, app, quiet) == 0, "package: codesign failed for %o", app);

    path dmgdir = f(path, "%o/dmg", stage);
    make_dir(dmgdir);
    vexec(a->verbose, "package", "cp -R %o %o/", app, dmgdir);
    vexec(a->verbose, "package", "ln -s /Applications %o/Applications", dmgdir);
    path dmg = f(path, "%o/%o-%o-macos-%s.dmg", pkgs, name, ver, arch);
    verify(exec(a->verbose, "hdiutil create -quiet -volname %o -srcfolder %o -ov -format UDZO %o",
        name, dmgdir, dmg) == 0, "package: hdiutil failed for %o", dmg);
    if (signed_)
        verify(exec(a->verbose, "codesign --force --timestamp --sign \"%s\" %o%s", id, dmg, quiet) == 0,
            "package: codesign failed for %o", dmg);
    // notarytool store-credentials leaves a keychain item whose account is
    // the profile name: a stored profile notarizes by itself; --notarize picks one
    string profile = a->notarize && len(a->notarize) ? a->notarize : !signed_ ? null :
        trim(command_run((command)string(
            "security find-generic-password -s com.apple.gke.notary.tool 2>/dev/null | "
            "sed -n 's/.*\"acct\"<blob>=\"\\(.*\\)\"/\\1/p' | head -1"), false));
    // a Developer ID release ships notarized; --skip_notary is the opt-out
    if (signed_ && !a->skip_notary && !(profile && len(profile))) {
        // the team id is the parenthesized tail of the identity name
        cstr lp = strrchr(id, '('), rp = strrchr(id, ')');
        string team = lp && rp && rp > lp ? string(chars, lp + 1, ref_length, (int)(rp - lp - 1)) : string("<team-id>");
        fault("--release: notarization needs your Apple ID stored once (asks for an "
              "app-specific password from appleid.apple.com):\n"
              "  xcrun notarytool store-credentials silver --apple-id <apple-id> --team-id %o\n"
              "or pass --skip_notary to ship unnotarized", team);
    }
    if (profile && len(profile) && !a->skip_notary) {
        print("[%o] package: notarizing with profile %o", name, profile);
        verify(exec(a->verbose, "xcrun notarytool submit %o --keychain-profile \"%o\" --wait",
            dmg, profile) == 0, "package: notarization failed for %o", dmg);
        verify(exec(a->verbose, "xcrun stapler staple %o", dmg) == 0,
            "package: staple failed for %o", dmg);
    }
    print("[%o] package: %o", name, dmg);
    vexec(a->verbose, "package", "rm -rf %o", stage);
    a->build = true; // a packaged release does not launch
}
#elif defined(__linux__)
// the ELF closure: every dependency ldd resolves under our install or build
// tree ships in lib/ (real files, not the versioned links); system libs stay
static void bundle_sos(silver a, path bin, path lib_dir, array done) {
    string inst  = trim(command_run((command)f(string, "readlink -f %o", a->install), false));
    string bld   = trim(command_run((command)f(string, "readlink -f %o", a->build_dir), false));
    string out   = command_run((command)f(string, "ldd %o", bin), false);
    array  lines = split(out, "\n");
    each(lines, string, ln0) {
        string ln  = trim(ln0);
        int    ar  = index_of(ln, "=> ");
        string dep = ar >= 0 ? mid(ln, ar + 3, len(ln) - ar - 3) : ln;
        int    sp  = index_of(dep, " (");
        if (sp > 0) dep = mid(dep, 0, sp);
        dep = trim(dep);
        if (!len(dep) || dep->chars[0] != '/') continue;
        string real = trim(command_run((command)f(string, "readlink -f %o", dep), false));
        if (!(starts_with(real, inst->chars) || starts_with(real, bld->chars))) continue;
        cstr   slash = strrchr(dep->chars, '/');
        string leaf  = string(slash + 1);
        if (index_of(done, (Au)leaf) >= 0) continue;
        push(done, (Au)leaf);
        vexec(a->verbose, "package", "cp -L %o %o/%o", dep, lib_dir, leaf);
        vexec(a->verbose, "package", "chmod u+w %o/%o", lib_dir, leaf);
    }
}

// ---- package writers: no distro tooling. tar, gzip and the coreutils sums
// are on every linux; the formats themselves are written here

static string sum_of(cstr tool, path p) {
    return trim(command_run((command)f(string, "%s %o | cut -d' ' -f1", tool, p), false));
}

static long size_of(path p) {
    struct stat st;
    return stat(p->chars, &st) == 0 ? (long)st.st_size : 0;
}

static void copy_into(FILE* out, path src) {
    FILE* in = fopen(src->chars, "rb");
    if (!in) return;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
}

// .deb = an ar archive of debian-binary, control.tar.gz, data.tar.gz, in
// that order; the 60-byte ar header is the whole format
static void ar_member(FILE* out, cstr name, path src) {
    long sz = size_of(src);
    fprintf(out, "%-16s%-12ld%-6d%-6d%-8s%-10ld`\n", name, 0L, 0, 0, "100644", sz);
    copy_into(out, src);
    if (sz & 1) fputc('\n', out);
}

static void write_deb(silver a, path stage, path root, string control, path deb) {
    path w = f(path, "%o/deb", stage);
    path c = f(path, "%o/control", w);
    make_dir(c);
    path_save(f(path, "%o/debian-binary", w), (Au)string("2.0\n"), null);
    path_save(f(path, "%o/control", c), (Au)control, null);
    vexec(a->verbose, "package", "tar --owner=0 --group=0 -C %o -czf %o/control.tar.gz ./control", c, w);
    vexec(a->verbose, "package", "tar --owner=0 --group=0 -C %o -czf %o/data.tar.gz ./usr", root, w);
    FILE* out = fopen(deb->chars, "wb");
    verify(out, "package: cannot write %o", deb);
    fputs("!<arch>\n", out);
    ar_member(out, "debian-binary",  f(path, "%o/debian-binary",  w));
    ar_member(out, "control.tar.gz", f(path, "%o/control.tar.gz", w));
    ar_member(out, "data.tar.gz",    f(path, "%o/data.tar.gz",    w));
    fclose(out);
}

// rpm: a lead, a signature header, the main header and a gzip'd newc cpio.
// a header is an index of 16-byte entries over a data store, all big-endian;
// entries ascend by tag and an immutable region entry at index 0 points at
// a trailer that closes the region (what the header digests cover)
typedef struct { i32 tag, type, offset, count; } rpm_ent;
typedef struct { rpm_ent ents[80]; int n; u8* data; int len, cap; } rpm_hdr;

enum { RT_CHAR = 1, RT_INT8, RT_INT16, RT_INT32, RT_INT64, RT_STRING, RT_BIN, RT_STRINGS, RT_I18N };

static void be32(u8* p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void be16(u8* p, u16 v) { p[0] = v >> 8; p[1] = v; }

static void rh_put(rpm_hdr* h, const void* p, int n) {
    if (h->len + n > h->cap) {
        h->cap = (h->len + n) * 2 + 256;
        h->data = realloc(h->data, h->cap);
    }
    memcpy(h->data + h->len, p, n);
    h->len += n;
}

static void rh_align(rpm_hdr* h, int al) {
    u8 z = 0;
    while (h->len % al) rh_put(h, &z, 1);
}

static rpm_ent* rh_ent(rpm_hdr* h, int tag, int type, int count) {
    verify(h->n < 80, "rpm: header entries exceeded");
    rpm_ent* e = &h->ents[h->n++];
    e->tag = tag; e->type = type; e->offset = h->len; e->count = count;
    return e;
}

static void rh_str(rpm_hdr* h, int tag, int type, cstr s) {
    rh_ent(h, tag, type, 1);
    rh_put(h, s, (int)strlen(s) + 1);
}

static void rh_strs(rpm_hdr* h, int tag, cstr* v, int n) {
    rh_ent(h, tag, RT_STRINGS, n);
    for (int i = 0; i < n; i++) rh_put(h, v[i], (int)strlen(v[i]) + 1);
}

static void rh_i32s(rpm_hdr* h, int tag, i32* v, int n) {
    rh_align(h, 4);
    rh_ent(h, tag, RT_INT32, n);
    for (int i = 0; i < n; i++) { u8 b[4]; be32(b, (u32)v[i]); rh_put(h, b, 4); }
}

static void rh_i16s(rpm_hdr* h, int tag, i32* v, int n) {
    rh_align(h, 2);
    rh_ent(h, tag, RT_INT16, n);
    for (int i = 0; i < n; i++) { u8 b[2]; be16(b, (u16)v[i]); rh_put(h, b, 2); }
}

static void rh_bin(rpm_hdr* h, int tag, u8* v, int n) {
    rh_ent(h, tag, RT_BIN, n);
    rh_put(h, v, n);
}

// index 0 is reserved for the region; close it with the trailer and write
static void rh_write(rpm_hdr* h, int region_tag, FILE* out) {
    u8 tr[16];
    be32(tr, region_tag); be32(tr + 4, RT_BIN); be32(tr + 8, (u32)(-(h->n * 16))); be32(tr + 12, 16);
    h->ents[0].tag = region_tag; h->ents[0].type = RT_BIN; h->ents[0].offset = h->len; h->ents[0].count = 16;
    rh_put(h, tr, 16);
    u8 magic[8] = { 0x8e, 0xad, 0xe8, 0x01, 0, 0, 0, 0 }, b[16];
    fwrite(magic, 1, 8, out);
    be32(b, h->n); be32(b + 4, h->len); fwrite(b, 1, 8, out);
    for (int i = 0; i < h->n; i++) {
        be32(b, h->ents[i].tag); be32(b + 4, h->ents[i].type);
        be32(b + 8, h->ents[i].offset); be32(b + 12, h->ents[i].count);
        fwrite(b, 1, 16, out);
    }
    fwrite(h->data, 1, h->len, out);
}

// the staged tree as rpm sees it: every file and link, and the dirs the
// package owns (its own under lib/ and share/; the system's are not ours)
typedef struct { char rel[1024]; struct stat st; char link[1024]; } rpm_file;
typedef struct { rpm_file* v; int n, cap; } rpm_files;

static int cstr_compare(const void* a, const void* b) {
    return strcmp(*(char* const*)a, *(char* const*)b);
}

static void rpm_walk(rpm_files* fs, path root, cstr rel, cstr own_lib, cstr own_share) {
    path dir = strlen(rel) ? f(path, "%o/%s", root, rel) : root;
    DIR* d = opendir(dir->chars);
    if (!d) return;
    // sorted, so the header and the payload agree with rpm's own order
    struct dirent* e;
    char** names = null;
    int    nn = 0, ncap = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (nn == ncap) { ncap = ncap * 2 + 32; names = realloc(names, ncap * sizeof(char*)); }
        names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof(char*), cstr_compare);
    for (int ni = 0; ni < nn; ni++) {
        char sub[1024];
        snprintf(sub, sizeof(sub), "%s%s%s", rel, strlen(rel) ? "/" : "", names[ni]);
        path p = f(path, "%o/%s", root, sub);
        struct stat st;
        if (lstat(p->chars, &st) != 0) continue;
        bool is_dir = S_ISDIR(st.st_mode);
        bool owned  = strncmp(sub, own_lib, strlen(own_lib)) == 0 ||
                      strncmp(sub, own_share, strlen(own_share)) == 0;
        if (!is_dir || owned) {
            if (fs->n == fs->cap) { fs->cap = fs->cap * 2 + 64; fs->v = realloc(fs->v, fs->cap * sizeof(rpm_file)); }
            rpm_file* rf = &fs->v[fs->n++];
            snprintf(rf->rel, sizeof(rf->rel), "%s", sub);
            rf->st = st;
            rf->link[0] = 0;
            if (S_ISLNK(st.st_mode)) {
                ssize_t ln = readlink(p->chars, rf->link, sizeof(rf->link) - 1);
                rf->link[ln > 0 ? ln : 0] = 0;
            }
        }
        if (is_dir) rpm_walk(fs, root, sub, own_lib, own_share);
    }
    for (int ni = 0; ni < nn; ni++) free(names[ni]);
    free(names);
}

static void cpio_entry(FILE* out, cstr name, u32 ino, u32 mode, u32 mtime, u32 nlink, u32 fsize) {
    u32 nsz = (u32)strlen(name) + 1;
    fprintf(out, "070701%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
        ino, mode, 0u, 0u, nlink, mtime, fsize, 0u, 0u, 0u, 0u, nsz, 0u);
    fwrite(name, 1, nsz, out);
    for (u32 pad = (110 + nsz) % 4; pad && pad < 4; pad++) fputc(0, out);
}

static void cpio_pad(FILE* out, u32 n) {
    for (u32 pad = n % 4; pad && pad < 4; pad++) fputc(0, out);
}

static void write_rpm(silver a, path stage, path root, cstr lower, string ver, cstr arch,
                      string name, string who, path rpm) {
    path w = f(path, "%o/rpm", stage);
    make_dir(w);
    char own_lib[256], own_share[256];
    snprintf(own_lib,   sizeof(own_lib),   "usr/lib/%s",   name->chars);
    snprintf(own_share, sizeof(own_share), "usr/share/%s", name->chars);
    rpm_files fs = { 0 };
    rpm_walk(&fs, root, "", own_lib, own_share);
    int n = fs.n;
    verify(n > 0, "package: nothing staged for %o", name);

    // the payload: newc cpio, names ./-prefixed, then gzip
    path cpio = f(path, "%o/payload.cpio", w);
    FILE* pf = fopen(cpio->chars, "wb");
    verify(pf, "package: cannot write %o", cpio);
    long total = 0;
    for (int i = 0; i < n; i++) {
        rpm_file* rf = &fs.v[i];
        char nm[1100];
        snprintf(nm, sizeof(nm), "./%s", rf->rel);
        bool lnk = S_ISLNK(rf->st.st_mode), dir = S_ISDIR(rf->st.st_mode);
        u32 fsz = lnk ? (u32)strlen(rf->link) : dir ? 0 : (u32)rf->st.st_size;
        cpio_entry(pf, nm, (u32)(i + 1), (u32)rf->st.st_mode, (u32)rf->st.st_mtime, dir ? 2 : 1, fsz);
        if (lnk)      fwrite(rf->link, 1, fsz, pf);
        else if (!dir) copy_into(pf, f(path, "%o/%s", root, rf->rel));
        cpio_pad(pf, fsz);
        total += fsz;
    }
    cpio_entry(pf, "TRAILER!!!", 0, 0, 0, 1, 0);
    fclose(pf);
    long payload_size = size_of(cpio);
    vexec(a->verbose, "package", "gzip -9 -n -f %o", cpio);
    path gz = f(path, "%o/payload.cpio.gz", w);

    // the main header
    rpm_hdr h = { 0 };
    rh_ent(&h, 0, 0, 0); // region, filled on write
    cstr  i18n[1] = { "C" };
    rh_strs(&h, 100, i18n, 1);
    string nvr = f(string, "%s-%o-1", lower, ver);
    rh_str(&h, 1000, RT_STRING, lower);
    rh_str(&h, 1001, RT_STRING, ver->chars);
    rh_str(&h, 1002, RT_STRING, "1");
    rh_str(&h, 1004, RT_I18N, name->chars);
    string desc = f(string, "%o %o", name, ver);
    rh_str(&h, 1005, RT_I18N, desc->chars);
    i32 now = (i32)time(NULL);
    rh_i32s(&h, 1006, &now, 1);
    i32 tsz = (i32)total;
    rh_i32s(&h, 1009, &tsz, 1);
    rh_str(&h, 1014, RT_STRING, "Proprietary");
    rh_str(&h, 1016, RT_I18N, "Applications");
    rh_str(&h, 1021, RT_STRING, "linux");
    rh_str(&h, 1022, RT_STRING, arch);
    i32*  sizes  = calloc(n, sizeof(i32)); i32* modes = calloc(n, sizeof(i32));
    i32*  zeros  = calloc(n, sizeof(i32)); i32* mtimes = calloc(n, sizeof(i32));
    i32*  vflags = calloc(n, sizeof(i32)); i32* ones  = calloc(n, sizeof(i32));
    i32*  inodes = calloc(n, sizeof(i32)); i32* diridx = calloc(n, sizeof(i32));
    cstr* digests = calloc(n, sizeof(cstr)); cstr* links = calloc(n, sizeof(cstr));
    cstr* users   = calloc(n, sizeof(cstr)); cstr* langs = calloc(n, sizeof(cstr));
    cstr* bases   = calloc(n, sizeof(cstr)); cstr* dirs  = calloc(n, sizeof(cstr));
    int   ndirs   = 0;
    for (int i = 0; i < n; i++) {
        rpm_file* rf = &fs.v[i];
        bool lnk = S_ISLNK(rf->st.st_mode), dir = S_ISDIR(rf->st.st_mode);
        sizes[i]  = lnk ? (i32)strlen(rf->link) : dir ? 0 : (i32)rf->st.st_size;
        modes[i]  = (i32)rf->st.st_mode;
        mtimes[i] = (i32)rf->st.st_mtime;
        vflags[i] = -1;
        ones[i]   = 1;
        inodes[i] = i + 1;
        digests[i] = (lnk || dir) ? "" : strdup(sum_of("sha256sum", f(path, "%o/%s", root, rf->rel))->chars);
        links[i]  = lnk ? rf->link : "";
        users[i]  = "root";
        langs[i]  = "";
        // /dir/ + base
        char full[1100];
        snprintf(full, sizeof(full), "/%s", rf->rel);
        cstr slash = strrchr(full, '/');
        bases[i] = strdup(slash + 1);
        char dn[1100];
        snprintf(dn, sizeof(dn), "%.*s/", (int)(slash - full), full);
        int di = -1;
        for (int k = 0; k < ndirs; k++) if (strcmp(dirs[k], dn) == 0) { di = k; break; }
        if (di < 0) { di = ndirs; dirs[ndirs++] = strdup(dn); }
        diridx[i] = di;
    }
    rh_i32s(&h, 1028, sizes, n);
    rh_i16s(&h, 1030, modes, n);
    rh_i16s(&h, 1033, zeros, n);
    rh_i32s(&h, 1034, mtimes, n);
    rh_strs(&h, 1035, digests, n);
    rh_strs(&h, 1036, links, n);
    rh_i32s(&h, 1037, zeros, n);
    rh_strs(&h, 1039, users, n);
    rh_strs(&h, 1040, users, n);
    string srpm = f(string, "%o.src.rpm", nvr);
    rh_str (&h, 1044, RT_STRING, srpm->chars);
    rh_i32s(&h, 1045, vflags, n);
    cstr prov[1] = { lower };
    rh_strs(&h, 1047, prov, 1);
    // what the payload relies on: ./-prefixed names, compressed file names,
    // sha256 file digests. rpm itself provides these
    i32  rflags[3] = { 0x100000A, 0x100000A, 0x100000A };
    cstr rnames[3] = { "rpmlib(CompressedFileNames)", "rpmlib(FileDigests)", "rpmlib(PayloadFilesHavePrefix)" };
    cstr rvers[3]  = { "3.0.4-1", "4.6.0-1", "4.0-1" };
    rh_i32s(&h, 1048, rflags, 3);
    rh_strs(&h, 1049, rnames, 3);
    rh_strs(&h, 1050, rvers, 3);
    rh_i32s(&h, 1095, ones, n);
    rh_i32s(&h, 1096, inodes, n);
    rh_strs(&h, 1097, langs, n);
    i32 pflags[1] = { 8 };
    rh_i32s(&h, 1112, pflags, 1);
    string pv = f(string, "%o-1", ver);
    cstr pver[1] = { pv->chars };
    rh_strs(&h, 1113, pver, 1);
    rh_i32s(&h, 1116, diridx, n);
    rh_strs(&h, 1117, bases, n);
    rh_strs(&h, 1118, dirs, ndirs);
    rh_str (&h, 1124, RT_STRING, "cpio");
    rh_str (&h, 1125, RT_STRING, "gzip");
    rh_str (&h, 1126, RT_STRING, "9");
    i32 algo[1] = { 8 };
    rh_i32s(&h, 5011, algo, 1);
    path hdr = f(path, "%o/header.bin", w);
    FILE* hf = fopen(hdr->chars, "wb");
    verify(hf, "package: cannot write %o", hdr);
    rh_write(&h, 63, hf);
    fclose(hf);

    // the signature header: digests of the main header, and of header+payload
    path both = f(path, "%o/header+payload.bin", w);
    vexec(a->verbose, "package", "cat %o %o > %o", hdr, gz, both);
    string sha1   = sum_of("sha1sum",   hdr);
    string sha256 = sum_of("sha256sum", hdr);
    string md5hex = sum_of("md5sum",    both);
    u8 md5[16];
    for (int i = 0; i < 16; i++) {
        unsigned v = 0;
        sscanf(md5hex->chars + i * 2, "%2x", &v);
        md5[i] = (u8)v;
    }
    rpm_hdr s = { 0 };
    rh_ent(&s, 0, 0, 0);
    rh_str(&s, 269, RT_STRING, sha1->chars);
    rh_str(&s, 273, RT_STRING, sha256->chars);
    i32 ssz = (i32)size_of(both);
    rh_i32s(&s, 1000, &ssz, 1);
    rh_bin(&s, 1004, md5, 16);
    i32 psz = (i32)payload_size;
    rh_i32s(&s, 1007, &psz, 1);
    path sig = f(path, "%o/sig.bin", w);
    FILE* sf = fopen(sig->chars, "wb");
    verify(sf, "package: cannot write %o", sig);
    rh_write(&s, 62, sf);
    // the main header starts 8-aligned
    for (long at = ftell(sf); at % 8; at++) fputc(0, sf);
    fclose(sf);

    // the lead, then the parts
    FILE* out = fopen(rpm->chars, "wb");
    verify(out, "package: cannot write %o", rpm);
    u8 lead[96] = { 0xed, 0xab, 0xee, 0xdb, 3, 0 };
    be16(lead + 6, 0);
    be16(lead + 8, (u16)(strcmp(arch, "aarch64") == 0 ? 19 : 1));
    snprintf((char*)lead + 10, 66, "%s", nvr->chars);
    be16(lead + 76, 1);
    be16(lead + 78, 5);
    fwrite(lead, 1, 96, out);
    copy_into(out, sig);
    copy_into(out, hdr);
    copy_into(out, gz);
    fclose(out);
    free(fs.v); free(h.data); free(s.data);
    free(sizes); free(modes); free(zeros); free(mtimes); free(vflags); free(ones);
    free(inodes); free(diridx); free(digests); free(links); free(users); free(langs); free(bases); free(dirs);
}

// --release on an app: stage the distros' own layout (/usr/bin/<name>,
// /usr/lib/<name>/, /usr/share/<name>/), a .desktop and the hicolor icon set
// (img's icons export scales it), then write .deb (debian/ubuntu), .rpm
// (fedora/suse) and .pkg.tar.zst (arch) into the module's packages/
static void silver_package(silver a) {
    if (a->error || a->is_external || !a->release || a->test) return;
    if (!a->product || !file_exists("%o", a->product)) return;
    string name  = a->name;
    string share = silver_install_name(a);
    string ver = silver_release_version(a);
    verify(ver, "--release: %o exports no version (export 1.0.0)", name);
#if defined(__aarch64__)
    cstr arch = "aarch64", deb_arch = "arm64";
#else
    cstr arch = "x86_64",  deb_arch = "amd64";
#endif
    // package names are lower-case on every distro
    char lower[128];
    int  li = 0;
    for (cstr c = name->chars; *c && li < 127; c++) lower[li++] = (char)tolower((unsigned char)*c);
    lower[li] = 0;
    cstr quiet = a->verbose ? "" : " >/dev/null 2>&1";

    // the distros' own layout: /usr/bin/<app>, its private closure in
    // /usr/lib/<app>/ (never bare /usr/lib), data in /usr/share/<share>/
    // packages outlive the install tree: they land in the module's own
    // repository, every platform's side by side
    path pkgs  = f(path, "%o/packages", a->project_path);
    path stage = f(path, "%o/tmp/%o-stage", a->install, name);
    path root  = f(path, "%o/root", stage);
    path bin   = f(path, "%o/usr/bin", root);
    path lib   = f(path, "%o/usr/lib/%o", root, name);
    path shd   = f(path, "%o/usr/share", root);
    path apps  = f(path, "%o/usr/share/applications", root);
    path icons = f(path, "%o/usr/share/icons/hicolor", root);
    make_dir(pkgs);
    vexec(a->verbose, "package", "rm -rf %o", stage);
    make_dir(bin); make_dir(lib); make_dir(shd); make_dir(apps); make_dir(icons);
    print("[%o] package: staging %o", name, root);

    // a live app: the host is the exe, the module .so rides in lib/<app>/
    // with <share>.product pointing the host at it (it readlinks that); an
    // empty <share>.source says no source is newer, so it never rebuilds
    bool live    = ((aether)a)->is_live && a->live_binary;
    path src_exe = live ? a->live_binary : a->product;
    path exe = f(path, "%o/%o", bin, name);
    vexec(a->verbose, "package", "cp -L %o %o", src_exe, exe);
    vexec(a->verbose, "package", "chmod u+w %o", exe);
    array done = array(alloc, 64);
    bundle_sos(a, exe, lib, done);
    if (live) {
        string leaf = path_filename(a->product);
        vexec(a->verbose, "package", "cp -L %o %o/%o", a->product, lib, leaf);
        vexec(a->verbose, "package", "chmod u+w %o/%o", lib, leaf);
        push(done, (Au)leaf);
        bundle_sos(a, f(path, "%o/%o", lib, leaf), lib, done);
        vexec(a->verbose, "package", "ln -sfn %o %o/%o.product", leaf, lib, share);
        path_save(f(path, "%o/%o.source", lib, share), (Au)string(""), null);
    }

    // the share is named after the app, as the bin is: no owner prefix
    path share_src = f(path, "%o/share/%o", a->install, share);
    if (dir_exists("%o", share_src))
        vexec(a->verbose, "package", "cp -RL %o %o/%o", share_src, shd, name);

    // icon: <module>/images/icon.png, icons/icon.png or icon.png; a release needs one
    path icon = f(path, "%o/images/icon.png", a->module_path);
    if (!file_exists("%o", icon)) icon = f(path, "%o/icons/icon.png", a->module_path);
    if (!file_exists("%o", icon)) icon = f(path, "%o/icon.png", a->module_path);
    verify(file_exists("%o", icon), "--release: %o has no icon (images/icon.png)", name);
    // img's icons export writes the hicolor set; SILVER_ICONS tells it what
    path img_so = f(path, "%o/libsilver-img.so", a->build_dir);
    verify(file_exists("%o", img_so), "--release: icons need img built (silver --build img)");
    path img_share = f(path, "%o/share/silver-img", a->install);
    make_dir(img_share);
    string icon_spec = f(string, "%o;%o;%o", icon, icons, name);
    setenv("SILVER_ICONS", icon_spec->chars, 1);
    int ist = silver_spawn_product(a, img_so, true, img_share, "SILVER_EXPORT", "SILVER_EXPORT_FORCE");
    unsetenv("SILVER_ICONS");
    verify(WIFEXITED(ist) && WEXITSTATUS(ist) == 0, "package: icons failed for %o", name);

    string desktop = f(string,
        "[Desktop Entry]\nType=Application\nName=%o\nExec=/usr/bin/%o\n"
        "Icon=%o\nCategories=Utility;\nTerminal=false\n", name, name, name);
    path_save(f(path, "%o/%o.desktop", apps, name), (Au)desktop, null);

    string who_n = trim(command_run((command)string("git config user.name"),  false));
    string who_e = trim(command_run((command)string("git config user.email"), false));
    string who   = f(string, "%o <%o>", len(who_n) ? who_n : string("silver"),
                                        len(who_e) ? who_e : string("silver@localhost"));

    // debian / ubuntu
    string control = f(string,
        "Package: %s\nVersion: %o\nSection: misc\nPriority: optional\nArchitecture: %s\n"
        "Maintainer: %o\nDescription: %o %o\n", lower, ver, deb_arch, who, name, ver);
    path deb = f(path, "%o/%s_%o_%s.deb", pkgs, lower, ver, deb_arch);
    write_deb(a, stage, root, control, deb);
    print("[%o] package: %o", name, deb);

    // fedora / suse: one rpm serves both
    path rpm = f(path, "%o/%s-%o-1.%s.rpm", pkgs, lower, ver, arch);
    write_rpm(a, stage, root, lower, ver, arch, name, who, rpm);
    print("[%o] package: %o", name, rpm);

    // arch: pacman reads .PKGINFO first in a zstd tar; no tool needed
    string size  = trim(command_run((command)f(string, "du -sb %o | cut -f1", root), false));
    string bdate = trim(command_run((command)string("date +%s"), false));
    string pkginfo = f(string,
        "pkgname = %s\npkgbase = %s\npkgver = %o-1\npkgdesc = %o %o\nurl = \n"
        "builddate = %o\npackager = %o\nsize = %o\narch = %s\n",
        lower, lower, ver, name, ver, bdate, who, size, arch);
    path_save(f(path, "%o/.PKGINFO", root), (Au)pkginfo, null);
    path arc = f(path, "%o/%s-%o-1-%s.pkg.tar.zst", pkgs, lower, ver, arch);
    verify(exec(a->verbose, "tar --zstd --owner=0 --group=0 -C %o -cf %o .PKGINFO usr", root, arc) == 0,
        "package: tar failed for %o", arc);
    print("[%o] package: %o", name, arc);
    vexec(a->verbose, "package", "rm -rf %o", stage);
    a->build = true; // a packaged release does not launch
}
#else
static void silver_package(silver a) { }
#endif

// export funcs are installation: run the product under SILVER_EXPORT
// (module init runs them, then exits 0) and wait — the build is not
// complete until the module's assets are baked. same launch as --test.
static void silver_run_exports(silver a) {
    if (a->error || a->is_external) return;
    bool any = false;
    members(a->autype, mem)
        if (mem->member_type == AU_MEMBER_FUNC &&
            mem->access_type == interface_export) any = true;
    // a cached build has no parsed members; --export launches regardless
    // (every product exits 0 under SILVER_EXPORT after module init)
    if (!any && !a->export) return;
    path bin = a->live_binary ? a->live_binary : a->product;
    if (!bin || !file_exists("%o", bin)) return;
    // a cached build never learns is_library: the product's ext says it
    string ex  = ext(bin);
    bool   lib = eq(ex, "dylib") || eq(ex, "so") || eq(ex, "dll");
    // exports write into the module's own share bundle
    path share = f(path, "%o/share/%o", a->install,
        silver_install_name(a));
    make_dir(share);
    print("[%o] export: running %o", a->name, bin);
    int st = silver_spawn_product(a, bin, lib, share, "SILVER_EXPORT",
        a->export ? "SILVER_EXPORT_FORCE" : null);
    verify(WIFEXITED(st) && WEXITSTATUS(st) == 0,
        "export funcs failed for %o", a->name);
}

// --release gate: every expect test in the product and its silver imports
// must pass before it ships. imports run theirs from their global ctors,
// so one launch under SILVER_EXPECT covers the whole tree
static void silver_run_tests(silver a) {
    if (a->error || a->is_external || !a->release || a->test) return;
    path bin = a->live_binary ? a->live_binary : a->product;
    if (!bin || !file_exists("%o", bin)) return;
    // a cached build never learns is_library: the product's ext says it
    string ex  = ext(bin);
    bool   lib = eq(ex, "dylib") || eq(ex, "so") || eq(ex, "dll");
    path share = f(path, "%o/share/%o", a->install,
        silver_install_name(a));
    make_dir(share);
    print("[%o] release: running expect tests in %o", a->name, bin);
    int st = silver_spawn_product(a, bin, lib, share, "SILVER_EXPECT", null);
    verify(WIFEXITED(st) && WEXITSTATUS(st) == 0,
        "release: expect tests failed for %o", a->name);
}

AU_EXPORT void silver_init(silver a) {
    hold(a);

    // one build at a time: the root instance holds a lock for the session
    if (!a->is_external) {
        path lk = f(path, "%s/install/build/.silver.lock", SILVER);
        build_lock_fd = open(cstring(lk), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
        if (build_lock_fd >= 0) {
            if (flock(build_lock_fd, LOCK_EX | LOCK_NB) != 0) {
                printf("silver: currently building in separate process, waiting for finish...\n");
                fflush(stdout);
                flock(build_lock_fd, LOCK_EX);
            }
        }
    }

    // silver [flags] module [app-args…] — the module name is the separator;
    // Au stopped parsing there and the rest rides to the launched app as-is
    if (!a->is_external && !a->run && au_argv() && au_argv_stop() > 0) {
        cstrs av = au_argv();
        array r  = array(8);
        for (int i = au_argv_stop(); av[i]; i++)
            push(r, (Au)string(av[i]));
        if (len(r))
            a->run = hold(r);
    }

    // claimed before any device is inherited: the importer stashed this right
    // before constructing us, and the ctor is already at its prop-pair max
    if (g_host_build) {
        a->host_build = true;
        g_host_build  = false;
    }

    // an imported module builds for the SAME device as the module importing
    // it — the importer is linked here, so take it from there. a host_build
    // is the exception: the compiler runs HERE, so it has to load a library
    // this machine can open, and that copy is never linked into the product
    if ((!a->device || !len(a->device)) && a->is_child && !a->host_build) {
        silver up = (silver)a->is_child;
        if (up->device && len(up->device)) a->device = hold(up->device);
    }
    // a timing run instruments every module in the tree, not just the root
    if (a->is_child && ((aether)(silver)a->is_child)->timing)
        ((aether)a)->timing = true;

    // --rsync pulls a device's sysroot again; with no device there is
    // nothing to pull, and silently doing nothing reads as success
    verify(!a->rsync || (a->device && len(a->device)),
        "--rsync names no device: pass -d <alias> to pull its sysroot again");

    // --device <name>: the device names its own platform, so no triple is typed.
    // a device serves its own sysroot — that is why no emulation is involved
    if (a->device && len(a->device)) {
        // the device list belongs to the USER, not to silver: look where the
        // command was typed first, then fall back to the silver root
        path here  = path_startup();
        path dpath = here ? f(path, "%o/devices.agi", here) : null;
        if (!dpath || !file_exists("%o", dpath))
            dpath = f(path, "%s/devices.agi", SILVER);
        verify(file_exists("%o", dpath),
            "no devices.agi in %o or %s — list your devices there "
            "(alias, platform, and either host: or image:)",
            here ? (Au)here : (Au)string("."), SILVER);
        string dtext = (string)load(dpath, typeid(string), null);
        verify(dtext && len(dtext), "could not read %o", dpath);
        // the file IS the map: alias -> Device
        map    dmap = (map)parse_agi(typeid(map), dtext->chars, null);
        Au     dent = dmap ? get(dmap, (Au)a->device) : null;
        verify(dent, "device '%o' not found in %o", a->device, dpath);
        Device dev  = (Device)instanceof(dent, Device);
        if (!dev) {
            // untyped block (no 'alias: Device' on the line): take the fields
            map dinfo = (map)instanceof(dent, map);
            verify(dinfo, "device '%o': expected a Device block", a->device);
            string droot = (string)instanceof(get(dinfo, (Au)string("root")), string);
            dev = new0(Device);
            dev->platform = hold((string)instanceof(get(dinfo, (Au)string("platform")), string));
            dev->host     = hold((string)instanceof(get(dinfo, (Au)string("host")),     string));
            dev->run      = hold((string)instanceof(get(dinfo, (Au)string("run")),      string));
            if (droot) dev->root = hold(path(droot->chars));
        }
        verify(dev->platform && len(dev->platform),
            "device '%o' has no platform", a->device);
        a->target = hold(dev);
        // the device names the platform; a child's default "native" yields too
        if (!a->platform || !len(a->platform) || cmp(a->platform, "native") == 0) {
            drop(a->platform);
            a->platform = hold(dev->platform);
        }

        // the device IS the sysroot: the exact libc, headers and library
        // versions the binary will run against. pulled once, then cached —
        // no image, no emulation. --rsync refreshes it
        path sysroot = f(path, "%s/platform/%o/sysroot", SILVER, target_dir(a));
        a->sysroot   = hold(sysroot);
        // posix sysroots carry usr/, a windows SDK carries crt/ — either
        // one standing means the sysroot is already laid out
        path marker  = f(path, "%o/usr", sysroot);
        path marker2 = f(path, "%o/include", sysroot);
        if (a->rsync || (!is_dir(marker) && !is_dir(marker2))) {
            make_dir(sysroot);
            // the DEVICE says which paths are its sysroot and what to skip —
            // only it knows. these are its .devices.agi directives
            verify((dev->sysroot && len(dev->sysroot)) || (dev->fetch && len(dev->fetch)),
                "device '%o': add a 'sysroot:' line naming the paths to pull "
                "(e.g. /usr/include /usr/lib/*-linux-*), or a 'fetch:' command "
                "that lays one out", a->device);

            // some sysroots are assembled by a tool rather than copied: the
            // device names those commands; {sysroot} and every other property
            // of this build interpolate into them
            if (dev->fetch && len(dev->fetch)) {
                // a tarball transfers in full either way, and untarring over
                // the old tree can only add — so a rsync starts clean here
                if (a->rsync) {
                    exec(false, "rm -rf %o", sysroot);
                    make_dir(sysroot);
                }
                print("[%o] fetching sysroot", a->device);
                // the fetch block is a list of commands, one per line; each
                // runs in turn and the first failure stops the pull
                array lines = split(dev->fetch, "\n");
                each (lines, string, ln) {
                    if (!len(ln)) continue;
                    string cmd = interpolate(ln, (Au)a);
                    if (exec(a->verbose, "%o", cmd) != 0) {
                        print("[%o] sysroot fetch failed: %o", a->device, cmd);
                        a->error = true;
                        return;
                    }
                }
            } else {
                // no host means this machine — a device you can test against
                string src = (dev->host && len(dev->host)) ?
                    f(string, "%o:'%o'", dev->host, dev->sysroot) : hold(dev->sysroot);
                string skip = string(alloc, 128);
                if (dev->exclude && len(dev->exclude)) {
                    array pats = split(dev->exclude, " ");
                    each (pats, string, p)
                        if (len(p)) concat(skip, f(string, "--exclude='%o' ", p));
                }
                print("[%o] pulling sysroot from %s", a->device,
                    (dev->host && len(dev->host)) ? dev->host->chars : "this machine");
                // a glob that matches nothing is not an error: a device need
                // not carry every path a sibling device does
                // --delete prunes what the device dropped; never clear first,
                // since transferring only the differences is the whole point
                verify(exec(a->verbose,
                    "rsync -a --relative --copy-unsafe-links --ignore-missing-args "
                    "--delete %o%o %o/", skip, src, sysroot) == 0,
                    "sysroot pull failed for device '%o' — is rsync present on both ends?",
                    a->device);
            }
            verify(is_dir(marker) || is_dir(marker2),
                "device '%o': nothing landed in %o — check its sysroot: paths",
                a->device, sysroot);
            // merged-usr devices have /lib -> usr/lib, and libc.so's linker
            // script names the /lib form; mirror it so lld resolves inside
            if (!is_dir(f(path, "%o/lib", sysroot)))
                exec(false, "ln -sfn usr/lib %o/lib", sysroot);
            if (!is_dir(f(path, "%o/lib64", sysroot)) &&
                 is_dir(f(path, "%o/usr/lib64", sysroot)))
                exec(false, "ln -sfn usr/lib64 %o/lib64", sysroot);
        }
        // the header parse must see the DEVICE's headers: posix.h hides its
        // whole surface behind _WIN32, which a host parse never defines
        ((aether)a)->target_sysroot = hold(sysroot);
        // write the device's toolchain files now, so every build system has
        // them and you can point other tools at them too
        device_cmake_toolchain(a);
        device_meson_cross(a);
    }

    // codegen targets the device from here on: llvm emits every core's
    // object for that machine, so nothing external lowers the IR
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
        set_target((aether)a, platform_triple(a));
    }

    // expects live in every build, release included: --release runs them
    // as its gate (silver_run_tests) and they stay in the shipped product
    a->strip_expect = false;

    // `import M with ext…`: the importer stashed the ext path list here right before
    // constructing this instance — claim it (and clear) before the build runs below.
    if (g_import_with) {
        a->extensions = hold(g_import_with);
        g_import_with = null;
    }

    // top-level --listen flows down; imports inherit it here
    if (a->listen && !g_listen)
        g_listen = (string)hold((Au)a->listen);
    else if (!a->listen && g_listen)
        a->listen = (string)hold((Au)g_listen);

    if (!keywords) silver_module();

    bool is_once = !a->watch || a->is_external;

    if (a->version) {
        printf("silver 0.88\n");
        printf("Copyright (C) 2017 Kalen Novis White\n");
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

#ifndef NDEBUG
    //a->asan         = true;
#endif
    a->exports      = map(hsize, 16);
    // a cross build keeps its own build dir: the toolchain stays at install
    // (native), but mips objects must never land beside the native ones
    a->build_dir    = (a->platform && len(a->platform) && cmp(a->platform, "native") != 0)
        ? f(path, "%s/platform/%o/build", SILVER, target_dir(a))
        : f(path, "%o/build", a->install);
    make_dir(a->build_dir);

    // silver User:Project[:Module]/Commit — run that import alone, then
    // quit (colon at index 1 is a windows drive, not a spec)
    if (!a->is_external && a->module) {
        cstr ms  = ((path)a->module)->chars;
        cstr col = strchr(ms, ':');
        cstr sl  = strchr(ms, '/');
        if (col && col - ms > 1 && (!sl || col < sl)) {
            cstr pe = sl ? sl : ms + strlen(ms);
            cstr nm = pe;
            while (nm > ms && nm[-1] != ':') nm--;
            drop(a->name);
            a->name = hold(string(chars, nm, ref_length, (int)(pe - nm)));
            a->imports = array(32);
            a->artifacts = array(32);
            a->resources = array(32);
            a->parse_f = parse_tokens;
            if (!a->git_service)
                a->git_service = hold(string("github.com"));
            cstr col2 = memchr(col + 1, ':', (size_t)(pe - col - 1));
            drop(a->git_owner);
            a->git_owner = hold(string(chars, ms, ref_length,
                (int)(col - ms)));
            drop(a->git_project);
            a->git_project = hold(string(chars, col + 1, ref_length,
                (int)((col2 ? col2 : pe) - col - 1)));
            // llvm_reinit is gated on module_file; give it one
            a->module_file = hold(f(path, "%o/%o.ag", a->build_dir, a->name));
            aether_reinit_startup((aether)a);
            string raw = string(ms);
            string import_text = f(string, "import %o", raw);
            if (!memchr(col + 1, ':', (size_t)(pe - col - 1))) {
                string head = mid(raw, 0, (int)(pe - ms));
                string selector = mid(raw, (int)(col - ms + 1),
                    (int)(pe - col - 1));
                string tail = sl ? mid(raw, (int)(sl - ms),
                    len(raw) - (int)(sl - ms)) : string("");
                import_text = f(string, "import %o:%o%o",
                    head, selector, tail);
            }
            a->tokens = hold(tokens(target, (Au)a, parser, parse_tokens,
                input, (Au)import_text));
            parse_import(a);
            deploy_module_resources(a);
            if (!a->url_product) {
                // local-namespace and cached runs land here
                path p1 = f(path, "%o/build/%o", a->install, a->name);
                if (file_exists("%o", p1)) a->url_product = hold(p1);
            }
            if (a->url_product && file_exists("%o", a->url_product)) {
                print("[silver] running %o", a->url_product);
                fflush(stdout);
                char* argv2[2] = { (cstr)a->url_product->chars, NULL };
                execvp(argv2[0], argv2);
            }
            return;
        }
    }

    // a module builds in the config THIS compiler was built in; there is one
    // silver on disk, so it is by definition the one last built. --release
    // is the one override: it tests, then ships, whatever silver is
    if (!a->is_external && !a->release) {
#ifdef CONFIG_RELEASE
        a->release = true;
#else
        a->release = false;
#endif
        a->debug   = !a->release;
    }
    a->defs_expect  = map(hsize, 4);
    a->defs_used    = map(hsize, 4);
    a->defs_hash    = defs_hash;
    //a->import_cache = map();
    a->artifacts    = array(32);
    a->resources    = array(32);

    // each module owns its OWN tree node (this map): it holds this module's source files
    // and, as children, the tree map of every module it imports — so the nesting forms a
    // real dependency tree, with the root instance (from main) as the tree's root node.
    a->tree = hold(map(hsize, 16));

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

    // search fallback paths if module file not found
    if (!module_file_m) {
        path silver_root = absolute(path(SILVER));
        path search_paths[] = {
            silver_root,
            NULL
        };
        string m_stem = stem(a->module_file);
        for (int i = 0; search_paths[i]; i++) {
            path try = f(path, "%o/%o/%o.ag", search_paths[i], m_stem, m_stem);
            u64  try_m = modified_time(try);
            if (try_m) {
                a->module_file = hold(try);
                a->module      = parent_dir(a->module_file);
                drop(a->module_path);
                a->module_path = hold(a->module);
                module_file_m  = try_m;
                break;
            }
        }
    }

    a->project_path = is_git_project(a);
    path af = a->module ? directory(a->module) : path_cwd();
    git_remote_info(af, &a->git_service, &a->git_owner,
        &a->git_project);
    string install_name = silver_install_name(a);
    drop(((aether)a)->share_name);
    ((aether)a)->share_name = hold(install_name);
    a->product_link = f(path, "%o/%o.product", a->build_dir,
        install_name);
    a->artifacts_path = f(path, "%o/%o.artifacts", a->build_dir,
        install_name);
    a->source_path = f(path, "%o/%o.source", a->build_dir,
        install_name);
    if (!a->format || !len(a->format))
        a->format = f(path, "%o/syntax/%o.f", a->install, a->name);

    aether_reinit_startup((aether)a);
    
    // discover resource folders within module directory and register on the
    // nearest install owner: an overlay build keeps its own share bundle
    {
        silver og = a;
        while (og->is_external &&
               compare(((silver)og->is_external)->install, og->install) == 0)
            og = (silver)og->is_external;
        DIR *dir = opendir(a->module_path->chars);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                if (entry->d_type != DT_DIR) continue;
                path res = form(path, "%o/%s", a->module_path,
                    entry->d_name);
                if (index_of(og->resources, (Au)res) < 0)
                    push(og->resources, (Au)hold(res));
            }
            closedir(dir);
        }
        // bundle exists before imports parse: their > commands write into it
        deploy_module_resources(og);
    }

    // check extension modules (.ag files in same dir) — if any are newer, bust the cache
    {
        DIR *dir = opendir(a->module_path->chars);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                cstr n = entry->d_name;
                int  nl = strlen(n);
                if (nl <= 3 || strcmp(n + nl - 3, ".ag") != 0) continue;
                path ag = form(path, "%o/%s", a->module_path, n);
                u64  m  = source_mtime(a, ag);
                if (m > module_file_m) module_file_m = m;
            }
            closedir(dir);
        }
    }

    // check all sources from previous build (stored in .source) — if any import's .ag is newer, bust cache
    if (file_exists("%o", a->source_path)) {
        FILE *sf = fopen(a->source_path->chars, "r");
        if (sf) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), sf)) {
                buf[strcspn(buf, "\n")] = '\0';
                if (!*buf) continue;
                path src = path(buf);
                u64  m   = source_mtime(a, src);
                if (m > module_file_m) module_file_m = m;
                // also check sibling .ag files in the same dir (extension modules)
                path src_dir = parent_dir(src);
                if (src_dir) {
                    DIR *dir = opendir(src_dir->chars);
                    if (dir) {
                        struct dirent *entry;
                        while ((entry = readdir(dir)) != NULL) {
                            cstr n = entry->d_name;
                            int  nl = strlen(n);
                            if (nl <= 3 || strcmp(n + nl - 3, ".ag") != 0) continue;
                            path ag = form(path, "%o/%s", src_dir, n);
                            u64  am = source_mtime(a, ag);
                            if (am > module_file_m) module_file_m = am;
                        }
                        closedir(dir);
                    }
                }
            }
            fclose(sf);
        }
    }

    // 1ms resolution time comparison (it could be nano-second based)
    bool update_product = true; //!a->is_external;

    verify(module_file_m, "module file not found: %o", a->module_file);
    push(a->include_paths, (Au)a->module); // add include folder just for our module (this was in aether's init, interfering with our filter logic)

    bool product_exists = file_exists("%o", a->product_link);
    u64  product_m      = product_exists ? modified_time(a->product_link) : 0;

    // epoch: a product older than the compiler that built it is stale —
    // this catches a config switch and any other compiler rebuild
    path compiler   = f(path, "%o/build/silver", a->install);
    u64  compiler_m = file_exists("%o", compiler) ? modified_time(compiler) : 0;

    if (product_exists && product_m > module_file_m && product_m > compiler_m) {

        // .source, not .artifacts: the writer sends .ag/.c/.cc to .source and
        // everything else to .artifacts, so .artifacts holds ONLY .so/.a — which
        // the loop below skips. reading it made newest always 0, so a changed
        // dependency source never invalidated this product
        if (file_exists("%o", a->source_path) && modified_time(a->source_path) > 0) {
            FILE *f = fopen(a->source_path->chars, "r");
            u64 newest = 0;
            if (f) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), f)) {
                    buf[strcspn(buf, "\n")] = '\0';
                    if (!*buf) continue;
                    // a submodule's .so/.a is a LINK input, not a source: it
                    // being newer means the dep recompiled, which never
                    // invalidates OUR object code. only real sources (.source)
                    // decide staleness.
                    int bl = (int)strlen(buf);
                    if ((bl > 3 && strcmp(buf + bl - 3, ".so") == 0) ||
                        (bl > 2 && strcmp(buf + bl - 2, ".a")  == 0)) continue;
                    path artifact = path(buf);
                    u64 m = modified_time(artifact);
                    if (m > newest) newest = m;
                }
                fclose(f);
            }
            // a dep .so is "stale relative to us" only if it's newer than OUR product —
            // deps are always rebuilt after our sources, so comparing to module_file_m
            // marked us dirty on every build. if no artifact post-dates the product, the
            // product already incorporates them → cached.
            if (!newest || newest < product_m)
                update_product = false;
        } else
            update_product = false;
    }

    // an uninstall walks the imports for their ledgers: that is a parse
    if (a->clean || a->uninstall) update_product = true;
    // instrumented code is not the cached product: a timing run rebuilds
    if (((aether)a)->timing) update_product = true;
    // the syntax map is a product too: cached runs must not leave a map
    // older than the source (the editor withholds stale coloring)
    if (!a->is_external && a->format && len(a->format)) {
        u64 fmt_m = file_exists("%o", a->format) ? modified_time(a->format) : 0;
        if (fmt_m < module_file_m) update_product = true;
    }
    // --release --test builds are measurement artifacts: always rebuilt,
    // never cached — the shipping release product must stay expect-free
    if (a->release && a->test) update_product = true;
    // (removed) root no longer force-rebuilt on --run: the dependency tree's transitive
    // .source now busts the root's cache precisely when any inner source changed, so the
    // blanket force is redundant — a no-change --run relinks/relaunches from cache.
    // --format is just the language service: the syntax map is written as a normal byproduct
    // of the build, with no special cache logic. a changed source rebuilds (and rewrites its
    // section); --clean forces a full rebuild (full map); an unchanged module is a cache hit
    // and keeps its existing section. nothing special here.

    a->mod = (aether)a;
    // one build per module per session, and concurrent instances WAIT for it.
    // the link unlinks its .so before writing it (silver_build_product), so a
    // second instance that proceeds on the same module either races into the
    // same output file or hands out a path whose file is mid-relink — which
    // reached dlopen as ENOENT ("cannot open shared object file")
    if (!silver_compiled) silver_compiled = hold(hold(map(hsize, 16)));
    pthread_mutex_lock(&compiled_lock);
    for (;;) {
        Au done = get(silver_compiled, (Au)compiled_key(a));
        if (done && instanceof(done, path)) {
            a->product = hold((path)done);
            pthread_mutex_unlock(&compiled_lock);
            return;
        }
        building* owner = building_find(compiled_key(a)->chars);
        if (!owner) { building_add(compiled_key(a)->chars); break; }   // we own this build
        // same thread re-entering (recursive import) must not wait on itself
        if (pthread_equal(owner->owner, pthread_self())) break;
        pthread_cond_wait(&compiled_cond, &compiled_lock);
    }
    pthread_mutex_unlock(&compiled_lock);

    if (!update_product) {
        a->product = hold(absolute(a->product_link));
        deploy_module_resources(a);
        publish_product(a);
        module_erase(a->autype, null);
        // the bundle is re-staged and re-signed even when the product is
        // current: profiles and devices change without a source edit
        if (target_is_mobile(a) && !a->is_external && ((aether)a)->is_live)
            silver_mobile_bundle(a);
        // silver-host.c is the LIVE-app launcher ONLY — a plain app has its own
        // main, so never build the host for it (it would overwrite the real exe).
        // recompiled (never cached) for live apps, matching the gated build below.
        if (((aether)a)->is_live) {
            // with app_ext missing this never found the host, so a cached build
            // silently SKIPPED recompiling it -- an edit to silver-host.c only
            // took effect on a full build, and only there did its errors appear
            path host_dst = f(path, "%o/%o%s", a->build_dir, a->name, app_ext);
            if (file_exists("%o", host_dst) && !target_is_mobile(a))
                build_silver_host(a);
        }
        // the host is what tests and ships: know it before either
        silver_recover_live(a);
        // --export: run export funcs even on a cached build, no launch
        if (a->export) { silver_run_exports(a); return; }
        silver_run_tests(a);
        silver_package(a);
        // cached build still runs by default (recovers is_live from the host binary)
        silver_live_run(a);
        return;
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
    a->imports      = array(32);
    a->parse_f        = parse_tokens;
    a->parse_expr     = parse_expression;
    //a->parse_enode  = silver_read_enode;
    //a->reverse_descent = reverse_descent;
    a->read_etype     = read_etype;
    a->prepare_record  = (callback)prepare_record_cb;
    void silver_emit_overrides_cb(silver, enode);
    a->emit_overrides  = (callback_extra)silver_emit_overrides_cb;
    a->src_loc      = absolute(path(_SRC ? _SRC : "."));
    verify(dir_exists("%o", a->src_loc), "SRC path does not exist");

    path install    = (a->platform && len(a->platform) && cmp(a->platform, "native") != 0)
                    ? f(path, "%s/platform/%o", _SILVER, target_dir(a))
                    : f(path, "%s/install",     _SILVER);

    bool retry = false;
    i64 mtime = current_time();// modified_time(a->module);
    hold_members(a);
    
    if (update_product)
    do {
        if (retry) {
            print("awaiting iteration: %o", a->module);
            auto_free(false);
            if (silver_compiled) {
                silver_compiled->unmanaged = true;
                clear(silver_compiled);
                silver_compiled->unmanaged = false;
            }
            mtime = silver_watch(a, a->module, mtime, 0); // it was easiest to fork path's implementation and add arks
            print("rebuilding...");
            drop(a->tokens);
            a->tokens = null;
            drop(a->stack);
            a->stack = null;
            // clear(a->artifacts); [ its best to accumulate with dupe-checking -- otherwise design stage fail can result in less hooks on your workflow]
            reinit_startup(a);
            a->processed_imports = false;
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
            bool target_apple = target_is_apple(a);
            path c_file = f(path, "%o/%o.c", a->module_path, m);
            path cc_file = f(path, "%o/%o.cc", a->module_path, m);
            path rs_file = f(path, "%o/%o.rs", a->module_path, m);
            path mm_file = f(path, "%o/%o.mm", a->module_path, m);
            path files[4] = {c_file, cc_file, rs_file, mm_file};
            int file_count = target_apple ? 4 : 3;
            for (int i = 0; i < file_count; i++)
                if (exists(files[i])) {
                    if (!a->implements)
                        a->implements = hold(array(2));
                    push(a->implements, (Au)files[i]);
                }
            
            // --format root: truncate + header the map file BEFORE parse, so the deps
            // built during parse append their sections into it.
            if (a->format && len(a->format) && !a->is_external)
                silver_fmt_header(a);

            parse(a);

            // --format: tokens are now classified — append this module's own file section
            // (deps + extend files appended their own during parse). this is additive; the
            // normal build below still runs (same pipeline, map is just an extra output).
            if (a->format && len(a->format)) {
                silver_write_fmt(a, (array)a->tokens);
                // extension files (Editor.ag, …) were parsed earlier but their bodies only
                // got keyword/type-stamped during build_record_parse above — write them now.
                if (a->fmt_ext)
                    each(a->fmt_ext, array, et)
                        silver_write_fmt(a, et);
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

            // a real build, always — products + types + the syntax-map byproduct. no --format
            // special-casing: an unchanged module already cache-hit above (no relink), so the
            // running app isn't needlessly relinked and the watcher isn't disturbed; only an
            // actually-changed module rebuilds (which the host would rebuild anyway).
            build_product(a);

            silver_run_exports(a);

            silver_run_tests(a);

            silver_package(a);

            exporter(a);

        }
        on_error() {
            aether_emit_recover();
            mtime = current_time();
            a->error = true;
            // an import builds on its own thread; unwinding past the locks
            // it held leaves the importer waiting on them forever. a failed
            // one-shot build ends the process instead of hanging it
            if (a->is_external && is_once) {
                progress_clear_line();
                fprintf(stderr, "[%s] build failed\n",
                    a->name ? a->name->chars : "?");
                fflush(stderr); fflush(stdout);
                _exit(1);
            }
        }
        finally()
        // --format: cap THIS build's map (header→sections→end) so the reader can frame each
        // emit and detect a partial write. written per iteration — in --watch it runs after
        // every rebuild, giving a fresh complete map each time the watched source changes.
        if (a->format && len(a->format) && !a->is_external)
            silver_fmt_end(a);
        // --watch: loop after EVERY build (success or error), blocking in silver_watch at the
        // top until the next change. one-shot (no --watch) leaves retry false and exits.
        retry = !is_once;
    } while (!a->is_external && retry); // externals do not watch (your watcher must invoke everything)
                                        // they handle their own exceptions

    unload_libs(a);
    module_erase(a->autype, null);
    au_space_end((void*)a);

    // a live app build+runs by default when invoked directly; --run passes extra
    // args to it. libraries and imported sub-modules never auto-run.
    // --export is a task: bake and stop, never launch
    if (!a->export) silver_live_run(a);
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
        "class",    "struct",   "scalar",   "expect",   "fault",    "abstract", "public",   "intern",
        "import",   "export",   "typeid",   
        "is",       "inherits", "ref",      "@",    "in",   "lambda",
        "const",    "no-op",    "<>",
        "return",   "->",       "::",       "...",  
        "asm",      "if",       "switch",   "any",
        "enum",     "ifdef",    "ifndef",   "el",       "while",
        "cast",     "try",      "throw",    "catch",
        "finally",  "for",      "func", 
        "operator", "construct", "alias",   "getter", "setter",
        "vec",      "new",      "local",    "elaborate",
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
        "??", string("value_default"),
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


// `syn` flags the syntax role of the token being consumed (Syntax__none = leave the
// lexer's lexical classification intact). callers pass the role they're parsing —
// function / member(property) / type / keyword / constant — which is the meaning the
// lexer can't infer. critical distinction: a name consumed as a call target is function,
// as a field access is property, as a declared/used type is type.
token silver_next(silver a, Syntax syn) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    if (!a->clipping && res && res->annotation && strcmp(res->annotation->chars, "#break") == 0) {
        breakpoint(res, "breaking at %o", res);
    }
    if (syn != Syntax__none && res)
        res->syntax = syn;
    a->cursor++;
    return res;
}

token silver_consume(silver a, Syntax syn) {
    return next(a, syn);
}

static array read_within(silver a) {
    //return read_expression(a, null, null);

    array body = array(32);
    token n = element(a, 0);
    bool proceed = a->expr_level == 0 ? true : eq(n, "[");
    if (!proceed)
        return null;

    bool bracket = eq(n, "[");
    consume(a, Syntax__none);
    int depth = bracket == true; // inner expr signals depth 1, and a bracket does too.  we need both togaether sometimes, as in inner expression that has parens
    for (;;) {
        token inner = next(a, Syntax__none);
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

// read compacted keyword tokens inside { ... } — returns tokens literal.
// an expected non-tokens type with a string ctr receives the literal as a
// space-joined string through that ctr (area: Region { l0 t0 r0 b0 })
static enode read_keywords(silver a, etype mdl_expect) {
    if (!next_is(a, "{"))
        return null;
    consume(a, Syntax__none); // consume {
    token open = element(a, -1);
    array toks   = array(16);
    array chunks = array(16); // first token of each chunk (line/indent)
    int depth = 1;
    for (;;) {
        token t = peek(a);
        if (!t) break;
        if (eq(t, "}")) {
            depth--;
            if (depth == 0) { consume(a, Syntax__none); break; }
        }
        if (eq(t, "{")) { depth++; }
        // compact neighboring tokens on the same line (like commit-id parsing). these are
        // the user/meta tokens inside the { } block (GLSL symbol/token descriptors) →
        // tag them `usertoken`. the { } brackets themselves stay punctuation (none above).
        string compacted = string(t->chars);
        consume(a, Syntax__usertoken);
        while (next_is_neighbor(a) && !next_is(a, "}")) {
            token nb = peek(a);
            concat(compacted, (string)nb);
            consume(a, Syntax__usertoken);
        }
        push(toks,   (Au)compacted);
        push(chunks, (Au)t);
    }
    Au_t target = (mdl_expect && mdl_expect->autype) ?
        au_arg_type((Au)mdl_expect->autype) : null;
    if (target && target != typeid(tokens)) {
        if (constructs_with(target, typeid(string)) ||
            constructs_with(target, typeid(cstr))   ||
            constructs_with(target, typeid(symbol))) {
            // rebuild line/indent structure (css/glsl bodies are line-based)
            string joined = string(alloc, 64);
            num prev_line = open ? open->line : 0;
            for (int i = 0; i < len(toks); i++) {
                token ft = (token)chunks->origin[i];
                if (ft->line != prev_line) {
                    push(joined, '\n');
                    for (num k = 0; k < ft->indent; k++)
                        push(joined, ' ');
                } else if (i)
                    push(joined, ' ');
                prev_line = ft->line;
                concat(joined, (string)toks->origin[i]);
            }
            return (enode)e_create((aether)a, (etype)mdl_expect,
                (Au)e_operand((aether)a, (Au)joined, etypeid(string)), false);
        }
    }
    // build tokens object at runtime: alloc + push each string
    efunc f_alloc = (efunc)u(efunc, find_member(etypeid(Au)->autype, "alloc_new", AU_MEMBER_FUNC, 0, false));
    // bind array's push (tokens extends array) — collective's is the fault stub
    efunc f_push  = (efunc)u(efunc, find_member(etypeid(array)->autype, "push", AU_MEMBER_FUNC, 0, true));
    enode n_src; Au n_line, n_seq;
    alloc_origin_args((aether)a, &n_src, &n_line, &n_seq);
    enode res = e_fn_call(a, f_alloc, a(
        e_typeid(a, etypeid(tokens)), _i32(0), e_null(a, etypeid(shape)),
        e_null(a, etypeid(Au_t)), e_null(a, etypeid(Au)),
        (Au)n_src, n_line, n_seq), false, false);
    res->autype = etypeid(tokens)->autype;
    // tokens is meta'd to token elements: construct a real token per compacted
    // string via runtime __convert (e_create shortcuts subclass conversions)
    efunc f_convert = (efunc)u(efunc, find_member(etypeid(Au)->autype, "__convert", AU_MEMBER_FUNC, 0, false));
    for (int i = 0; i < len(toks); i++) {
        string s = (string)toks->origin[i];
        enode str_const = e_operand(a, (Au)s, etypeid(string));
        enode type_node = e_typeid(a, etypeid(token));
        enode tok_const = e_fn_call(a, f_convert, a(type_node, str_const), false, false);
        e_fn_call(a, f_push, a(res, tok_const), false, false);
    }
    return res;
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
        if (mult && k->indent == 0 && k->line > p->line)
            break;
        push(body, (Au)k);
        consume(a, Syntax__none);
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
        consume(a, Syntax__none);
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
    // module defines first: class members must not shadow them
    Au_t  mem  = find_member(a->autype, cstring(name), AU_MEMBER_VAR, 0, false);
    if (!mem) mem = lexical(a->lexical, cstring(name));
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
            return enode(mod, (aether)a, autype, null);
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
    if  (!n || eq(n, "asm")) return null;
    token prev = element(a, -1);
    token p    = a->statement_origin ? a->statement_origin : prev;
    bool member_meta = false;
    if (eq(n, "[") && n->line > p->line &&
        n->indent == p->indent && is_rec(top_scope(a))) {
        int depth = 0;
        for (int i = 0; ; i++) {
            token t = element(a, i);
            if (!t) break;
            if (eq(t, "[")) depth++;
            if (eq(t, "]") && --depth == 0) {
                token tail = element(a, i + 1);
                member_meta = tail && tail->line == n->line;
                break;
            }
        }
    }

    // when [ follows a token on the same line, it's always a bracket expr
    // (use prev, not statement_origin, to detect inline [ after type/func names)
    if (!member_meta && eq(n, "[") && ((prev && n->line == prev->line) || n->line == p->line || (n->line > p->line && n->indent == p->indent))) {
        consume(a, Syntax__none);
        int depth = 1; // inner expr signals depth 1, and a bracket does too.  we need both together sometimes, as in inner expression that has parens
        push(body, (Au)token("["));
        for (;;) {
            token inner = next(a, Syntax__none);
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
        next(a, Syntax__none);
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
    // 0x 0b 0o prefixes are numeric literals, never a shape
    int h = *index + (idx(str, *index) == '-' ? 1 : 0);
    if (idx(str, h) == '0' && h + 2 < ln) {
        i32 p = idx(str, h + 1), d = idx(str, h + 2);
        if ((p == 'x' && isxdigit(d)) || (p == 'b' && (d == '0' || d == '1')) ||
            (p == 'o' && d >= '0' && d <= '7'))
            return null;
    }
    for (int i = *index; i < ln; i++) {
        i32 chr = idx(str, i);
        bool start = (i == *index);
        if ((chr == '-' && start) || ((chr == 'x' && !start) || (chr >= '0' && chr <= '9'))) {
            single |= (chr >= '0' && chr <= '9');
            explicit |= chr == 'x';
            index_stop = i;
            continue;
        }
        if (chr == '.' || chr == 'e' || chr == 'E')
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
    
    // decimal integer or float with optional scientific notation: 123, 3.14, 1e20, 1.5e-7
    while (i < ln && isdigit(idx(str, i)))
        i++;
    bool is_float = false;
    if (i < ln && idx(str, i) == '.') {
        is_float = true;
        i++;
        while (i < ln && isdigit(idx(str, i)))
            i++;
    }
    if (i < ln && (idx(str, i) == 'e' || idx(str, i) == 'E')) {
        is_float = true;
        i++;
        if (i < ln && (idx(str, i) == '+' || idx(str, i) == '-'))
            i++;
        while (i < ln && isdigit(idx(str, i)))
            i++;
    }
    // strip C suffix — but only when the literal ENDS there. otherwise
    // a scalar unit starting with one of these letters loses it, and
    // 2.0ft reads as 2.0f followed by a stray t
    int sfx = i;
    while (sfx < ln && (idx(str, sfx) == 'f' || idx(str, sfx) == 'F' ||
                        idx(str, sfx) == 'l' || idx(str, sfx) == 'L' ||
                        idx(str, sfx) == 'u' || idx(str, sfx) == 'U'))
        sfx++;
    if (sfx > i && (sfx >= ln || !isalnum(idx(str, sfx))))
        i = sfx;
    if (i > start) {
        string crop = mid(str, start, i - start);
        *str_res = crop;
        *index = i;
        return is_float ? _f64(strtod(crop->chars, NULL)) : _i64(strtoll(crop->chars, NULL, 10));
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

static bool is_char_uni(string crop, i64* out) {
    if (crop->chars[0] != '\'') return false;
    // 'a' — single character
    if (crop->count == 3) {
        *out = (i64)(u8)crop->chars[1];
        return true;
    }
    // '\n' '\t' '\r' '\0' '\\' '\''
    if (crop->count == 4 && crop->chars[1] == '\\') {
        switch (crop->chars[2]) {
            case 'n':  *out = '\n'; return true;
            case 'r':  *out = '\r'; return true;
            case 't':  *out = '\t'; return true;
            case '0':  *out = '\0'; return true;
            case '\\': *out = '\\'; return true;
            case '\'': *out = '\''; return true;
            default:   *out = (i64)(u8)crop->chars[2]; return true;
        }
    }
    // '\xN' through '\xFFFFFFFF'
    if (crop->count >= 5 && crop->count <= 12 &&
        crop->chars[1] == '\\' && crop->chars[2] == 'x') {
        int hlen = crop->count - 4;
        char hex[9] = {0};
        memcpy(hex, &crop->chars[3], hlen);
        *out = strtol(hex, NULL, 16);
        return true;
    }
    return false;
}

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

    string special = string(".{}$,<>()![]/+*:=#~@|&^?`");

    // the symbol table is the same for every parse; build it once. modules
    // parse on their own threads, so the map must be fully built before the
    // pointer that reveals it becomes visible
    static map             _mapping;
    static pthread_mutex_t mapping_lock = PTHREAD_MUTEX_INITIALIZER;
    map mapping = __atomic_load_n(&_mapping, __ATOMIC_ACQUIRE);
    if (!mapping) {
        pthread_mutex_lock(&mapping_lock);
        mapping = __atomic_load_n(&_mapping, __ATOMIC_ACQUIRE);
        if (!mapping) {
            list symbols = list();
            i32 special_ln = len(special);
            for (int i = 0; i < special_ln; i++)
                push(symbols, (Au)unicode_char((i32)special->chars[i]));
            push(symbols, (Au)string(".."));
            each(keywords, string, kw) push(symbols, (Au)kw);
            each(assign, string, a) push(symbols, (Au)a);
            each(compare, string, a) push(symbols, (Au)a);
            pairs(operators, i) push(symbols, i->key);
            sort(symbols, (ARef)sfn);
            map m = map(hsize, 32);
            for (item i = symbols->first; i; i = i->next) {
                string sym = (string)i->value;
                set(m, (Au)sym, (Au)sym);
            }
            mapping = (map)hold(m);
            __atomic_store_n(&_mapping, mapping, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&mapping_lock);
    }

    array tokens = output;
    verify(tokens, "no output set");

    num     line_num    = 1;
    num     length      = len(input_string);
    num     index       = 0;
    num     line_start  = 0;
    num     indent      = 0;
    num     curly_depth = 0;
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


        // comments; inside { } bodies '#' is data (css colors)
        if (!a->cmode && chr == '#' && curly_depth == 0) {
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
                    }
                    last->annotation = hold(new(string, chars, annotation->chars));
                }
            }
            continue;
        }

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
            if (len(name) == 1) {
                if      (name->chars[0] == '{')                curly_depth++;
                else if (name->chars[0] == '}' && curly_depth) curly_depth--;
            }
            // lexical syntax: brackets/delimiters are punctuation, the rest are operators.
            i32    nc0 = name->chars[0];
            Syntax nsk = (nc0=='['||nc0==']'||nc0=='('||nc0==')'||nc0=='{'||nc0=='}'||
                          nc0==','||nc0==':'||nc0==';'||nc0=='.') ? Syntax__punctuation : Syntax__op;
            token t = token(
                chars, (cstr)name->chars,
                indent, indent,
                source, src,
                line, line_num,
                syntax, nsk,
                neighbor, index > 0 && !isspace(idx(input_string, index - 1)),
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
                if (c == '\\' && index + 1 < length) {
                    index += 2; // skip any escape sequence: \\, \', \", \n, etc.
                    continue;
                }
                if (c == '{') {
                    if (index + 1 < length && idx(input_string, index + 1) == '{') {
                        index += 2; // {{ escape -> literal brace
                        continue;
                    }
                    // a '{' sitting right before the closing quote (e.g. the char
                    // literal '{') can't be an interpolation — leave it literal so
                    // single-char brace literals still tokenize.
                    if (index + 1 < length && idx(input_string, index + 1) == quote_char) {
                        index += 1;
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
            // single character in quotes → uchar (unicode codepoint) literal
            i64 char_val;
            if (is_char_uni(crop, &char_val)) {
                unichar uc = (unichar)char_val;
                push(tokens, (Au)token(
                    chars, crop->chars,
                    indent, indent,
                    source, src,
                    line, line_num,
                    literal, primitive(typeid(unichar), &uc),
                    syntax, Syntax__character,
                    neighbor, start > 0 && !isspace(idx(input_string, start - 1)),
                    column, start - line_start));
                continue;
            }
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
                s2 = unescape(s2);
                concat(s, s2);
                drop(l->literal);
                l->literal = hold((Au)s);
            } else {
                string content = mid(crop, 1, len(crop) - 2);
                content = unescape(content);
                Au     lit = (Au)hold((quote_char == '\'') ? (Au)content : (Au)const_string(chars, cstring(content)));
                push(tokens, (Au)token(
                                chars, crop->chars,
                                indent, indent,
                                source, src,
                                line, line_num,
                                literal, lit,
                                syntax, Syntax__str,
                                neighbor, start > 0 && !isspace(idx(input_string, start - 1)),
                                column, start - line_start));
            }
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

        if (start_numeric && !literal) {
            literal = parse_numeric(input_string, &shape_str, &index);
        }
        if (literal) {
            push(tokens, (Au)token(
                    chars,   shape_str->chars,
                    indent,  indent,
                    source,  src,
                    line,    line_num,
                    literal, literal,
                    syntax,  Syntax__number,
                    neighbor, start > 0 && !isspace(idx(input_string, start - 1)),
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
                         syntax, Syntax__ident,
                         neighbor, start > 0 && !isspace(idx(input_string, start - 1)),
                         column, start - line_start));
    }
    return tokens;
}

token silver_read_if(silver a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        // infer syntax from the matched literal: read_if matching a word is matching a
        // keyword (func/class/if/return/…); a symbol stays as the lexer tagged it
        // (op/punctuation). this classifies all 271 read_if sites with no call-site churn.
        if (isalpha(cs[0]) || cs[0] == '_')
            n->syntax = Syntax__keyword;
        next(a, Syntax__none);
        return n;
    }
    return null;
}

Au silver_read_literal(silver a, Au_t of_type) {
    token n = element(a, 0);
    if (!n) return null;
    Au res = get_literal(n, of_type);
    if (res) {
        next(a, Syntax__none);
        return res;
    }
    return null;
}

string silver_read_string(silver a) {
    token n = element(a, 0);
    if (n && instanceof(n->literal, string)) {
        string token_s = string(n->chars);
        string result = mid(token_s, 1, token_s->count - 2);
        next(a, Syntax__none);
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
        next(a, Syntax__none);
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
            consume(a, Syntax__none);
        return etypeid(Au);
    }

    // a keyword is never a base-class name (lambda name [] is a definition)
    if (is_keyword((Au)t))
        return null;

    etype f = elookup(t->chars);
    if (is_class(f)) {
        if (read_token)
            consume(a, Syntax__none);
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
        next(a, Syntax__none);
    return is_bool ? _bool(is_true) : null;
}

OPType silver_read_operator(silver a, ARef fname) {
    token n = element(a, 0);
    if (!n)
        return OPType__undefined;
    string found = (string)get(operators, (Au)n);
    if (found) {
        consume(a, Syntax__none);
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
        next(a, Syntax__none);
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
    push_current(a);
    string n = read_alpha(a);
    if (!n) {
        pop_tokens(a, false);
        return null;
    }

    Au_t mem = lexical(a->lexical, cstring(n));
    if (mem && mem->member_type == AU_MEMBER_MACRO) {
        macro mac = u(macro, mem);
        verify(mac, "unresolved macro: %o", mem);
        cstr open = is_cmode(a) ? "(" : "[";
        if (mac->params && !next_is(a, open))
            mem = null;
    }

    bool use_name = is_decl || mem || next_is(a, ":");
    pop_tokens(a, use_name);
    return use_name ? n : null;
}

enode enode_super(etype, enode);


etype etype_create(silver, Au_t);

// single shared context: nodes carry their value directly
static enode worker_view(silver a, enode mem) {
    return mem;
}

Au_t alloc_arg(Au_t context, symbol ident, Au_t arg);

// inline-lambda gather: first use of an enclosing local becomes a
// context member, captured by value at the instance site
static none gather_capture(silver a, string alpha, enode mem) { static int seq = 0; seq++;
    Au_t m = mem->autype;
    if (!m || m->member_type != AU_MEMBER_VAR) return;
    if (m->is_static || m->is_const) return;
    // internal to the lambda: an arg, a body local, or a prior capture
    for (int i = len(a->lexical) - 1; i >= a->gather_base; i--) {
        Au_t s = (Au_t)a->lexical->origin[i];
        for (int j = 0; j < s->args.count; j++)
            if ((Au_t)s->args.origin[j] == m) return;
        for (int j = 0; j < s->members.count; j++)
            if ((Au_t)s->members.origin[j] == m) return;
    }
    if (find_member(a->autype, cstring(alpha), AU_MEMBER_VAR, 0, false) == m)
        return; // module globals stay direct
    validate(!mem->target,
        "cannot capture member '%o' — copy it to a local first", alpha);
    if (find_member(a->gather_fn, cstring(alpha), AU_MEMBER_VAR, 0, false))
        return;
    Au_t cap = alloc_arg(a->gather_fn, cstring(alpha), (Au_t)au_arg((Au)m));
    cap->meta = m->meta;
    micro_push((micro_*)&a->gather_fn->members, (Au)cap);
}

// the element type of a vector-typed slot (vec T), or null
static Au_t vec_elem_of(etype t) {
    if (!t) return null;
    Au_t au = t->autype;
    while (au && au->member_type == AU_MEMBER_VAR) au = au->src;
    while (au && au->is_alias && au->src && !au_is_vector(au)) au = au->src;
    if (!au || !au_is_vector(au)) return null;
    Au_t m = t->meta_a ? (Au_t)t->meta_a : t->autype->meta.a;
    if (!m && t->autype->member_type == AU_MEMBER_VAR && t->autype->src)
        m = t->autype->src->meta.a;
    return m ? m : typeid(Au);
}

// the vector type carrying T as its meta
static etype vector_etype(silver a, Au_t elem) {
    etype vt = etype_prep((aether)a, typeid(vector));
    return etype(mod, (aether)a, autype, vt->autype, meta_a, (Au)elem);
}

// [ v1, v2, ... ] against a vector-typed slot: construct, then push
// each element through the vector's own += (the cursor sits after [)
static enode vector_literal(silver a, Au_t elem) {
    etype vm   = vector_etype(a, elem);
    enode vecn = e_create((aether)a, vm, null, false);
    vecn->meta_a = (Au)elem;
    etype el = u(etype, elem);
    if (!el) el = etype_prep((aether)a, elem);
    while (peek(a) && !next_is(a, "]")) {
        enode e = parse_expression(a, el, false, true);
        e_assign((aether)a, vecn, (Au)e, OPType__assign_add);
        read_if(a, ",");
    }
    validate(read_if(a, "]"), "expected ] after vec literal");
    return vecn;
}

enode silver_parse_member(silver a, ARef assign_type, Au_t in_decl, etype scope_mdl, bool in_ref) { static int seq = 0; seq++;
    token pk1 = peek(a);
    OPType assign_enum = OPType__undefined;
    Au_t   top     = top_scope(a);
    etype  rec_top = context_record(a);
    silver module  =  !is_cmode(a) && (top->is_namespace) ? a : null;
    efunc  f       =  !is_cmode(a) ? context_func(a) : null;
    bool   in_rec  = rec_top && rec_top->autype == top;
    token t1 = element(a, 1);
    bool new_bind = t1 && eq(t1, ":");

    if (assign_type) *(OPType*)assign_type = OPType__undefined;
    push_current(a);

    enode  mem                = null;
    string alpha              = null;
    int    depth              = 0;
    bool   skip_member_check  = false;

    if (module) {
        string alpha = peek_alpha(a);
        if (alpha) {
            enode m = (enode)elookup(alpha->chars);
            etype mdl = m ? resolve(m) : null;
            if (mdl && (m->autype->member_type != AU_MEMBER_VAR && (mdl->autype->member_type == AU_MEMBER_TYPE || mdl->autype->member_type == AU_MEMBER_MODULE)))
                skip_member_check = true;
        }
    }

    bool is_super = false;
    bool qualified = false;
    bool null_guard = false;
    string first_alpha = null;
    for (;!skip_member_check;) {
        bool first = !mem;

        token pkzip = peek(a);
        bool new_name = in_decl != null || in_rec;
        alpha = read_alpha_macrofilter(a, new_name);
        if (!alpha && mem)
            alpha = read_alpha_any(a);

        if (!alpha && first && next_is(a, "super"))
            alpha = read_alpha(a);
        if (!alpha && first && scope_mdl) {
            string bare = peek_alpha(a);
            if (bare && find_member(scope_mdl->autype, bare->chars, 0, 0, true))
                alpha = read_alpha(a);
        }

        if (!first_alpha) first_alpha = alpha;

        // a::b<T> — sampled C++ registers flat (std::clamp<i32>): join the
        // path and args into that key and resolve it as one name
        if (first && alpha && next_is(a, "::")) {
            push_current(a);
            string key = f(string, "%o", alpha);
            bool   ok  = true;
            while (read_if(a, "::")) {
                string seg = read_alpha(a);
                if (!seg) { ok = false; break; }
                concat(key, string("::"));
                concat(key, seg);
            }
            if (ok && read_if(a, "<")) {
                concat(key, string("<"));
                for (;;) {
                    string targ = read_alpha(a);
                    if (targ) {
                        Au_t targ_au = lexical(a->lexical, cstring(targ));
                        if (!targ_au || targ_au->member_type != AU_MEMBER_TYPE) { ok = false; break; }
                        concat(key, string(targ_au->ident));
                    } else {
                        i64* n = (i64*)read_literal(a, typeid(i64));
                        if (!n) { ok = false; break; }
                        concat(key, f(string, "%i", (i32)*n));
                    }
                    if (!read_if(a, ",")) break;
                    concat(key, string(","));
                }
                if (ok) ok = read_if(a, ">") != null;
                if (ok) concat(key, string(">"));
            }
            Au_t fa = ok ? lexical(a->lexical, cstring(key)) : null;
            if (fa && is_func((Au)fa)) {
                pop_tokens(a, true);
                mem       = (enode)u(etype, fa);
                alpha     = key;
                qualified = true;
            } else
                pop_tokens(a, false);
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
            members (a->autype, im) {
                if (!im->is_namespace || im->is_nameless) continue;
                if (im->ident && eq(alpha, im->ident)) {
                    // a nearer lexical member (local, arg, field) overrides the
                    // import alias, and `name :` declares one — the namespace
                    // path applies only when the alias is the nearest resolution
                    Au_t nearest = lexical(a->lexical, cstring(alpha));
                    if ((nearest && nearest != im) ||
                        (next_is(a, ":") && a->expr_level == 0))
                        break;
                    string module_name = alpha;
                    validate(read_if(a, "."), "expected . after module-name: %o", alpha);
                    alpha = read_alpha(a);
                    validate(alpha, "expected alpha-ident after module-name: %o", module_name);
                    mem = (enode)elookup(alpha->chars);
                    if (mem && mem->autype->is_namespace) {
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
            if (first && !qualified) {

                if (eq(alpha, "super")) { // take care now
                    validate(rec_top, "super only valid in class context");
                    mem = enode_super(rec_top, f->target);
                    is_super = true;
                }
                else if (eq(alpha, "sequence")) {
                    // debug: read the per-statement-block __seq local (coverage_seq_local)
                    extern enode aether_sequence_enode(aether a);
                    mem = aether_sequence_enode((aether)a);
                    break;
                }
                else if (!in_rec) {
                    // try implicit 'this' access in instance methods
                    if (!mem && f && f->target) {
                        mem = (enode)rlookup((aether)a, alpha);
                        //mem = (enode)elookup(alpha->chars);
                        // stamp at the RESOLUTION: this branch is where a
                        // free/C function lands, and it never reaches the
                        // classification below
                        if (mem && mem->autype) {
                            token rt9 = element(a, -1);
                            if (rt9 && rt9->chars && strcmp(rt9->chars, alpha->chars) == 0)
                                rt9->decl = mem->autype;
                        }

                        if (mem && !mem->autype->is_static && mem->autype->member_type != AU_MEMBER_TYPE) {
                            etype ftarg = etype_resolve((etype)f->target);
                            if (ftarg && in_context(mem->autype, ftarg->autype)) {
                                mem = access(f->target, alpha);
                            }
                        }

                    } else if (!f || !f->target) {
                        mem = (enode)rlookup((aether)a, alpha);
                    }
                } else if (in_rec && !in_decl) {
                    Au_t m = find_member(rec_top->autype, alpha->chars, 0, 0, false);
                    if (m) mem = (enode)u(etype, m);
                    else   mem = (enode)rlookup((aether)a, alpha);
                }

                if (!mem && scope_mdl) {
                    Au_t sm = find_member(scope_mdl->autype, alpha->chars, 0, 0, true);
                    if (sm)
                        mem = access((enode)scope_mdl, alpha);
                }
                mem = worker_view(a, mem);

                if (a->gather_fn && mem && instanceof(mem, enode))
                    gather_capture(a, alpha, (enode)mem);

                // first token + : means new declaration — but only at expression level 0
                if (first && mem && next_is(a, ":") && a->expr_level == 0) {
                    // disallow declaring a local that shadows a member of
                    // the enclosing class. silently shadowing a field is
                    // a footgun: subsequent `name = ...` writes go to the
                    // local, not the field, and the field stays at its
                    // default forever (e.g. `proj : mat4f.perspective[...]`
                    // inside Earth.init while Earth has `mutable proj : mat4f`).
                    // force the user to either rename the local or use
                    // `=` to assign the field.
                    if (f && rec_top && mem->autype &&
                        mem->autype->member_type == AU_MEMBER_VAR &&
                        (mem->autype->context == rec_top->autype ||
                         inherits(rec_top->autype, mem->autype->context))) {
                        validate(false,
                            "local '%o' shadows class member '%s.%o': "
                            "use '=' to assign the field, or rename the local",
                            alpha, mem->autype->context->ident, alpha);
                    }
                    mem = null;
                }

                if (!mem) {
                    token tm1 = element(a, -2); // sorry for the mess (coin-flip)

                    // compound assignment on unknown identifier = clear "undefined variable" error
                    token pk_next = peek(a);
                    if (pk_next && index_of(assign, (Au)pk_next) > 0) // >0 skips plain "="
                        validate(false, "undefined variable '%o' (used with %o)", alpha, pk_next);

                    validate(next_is(a, ":") || (tm1 && index_of(keywords, (Au)tm1) >= 0), "unknown identifier %o", alpha);
                    {   Au_t existing = find_member(top, alpha->chars, 0, 0, false);
                        validate(!existing || existing->member_type == AU_MEMBER_DECL, "duplicate member: %o", alpha);
                    }
                    Au_t m = def_member(top, alpha->chars, null, AU_MEMBER_DECL, 0); // this is promoted to different sorts of members based on syntax
                    // alpha is the NAME, not the token. only stamp when the
                    // cursor's last token really is that identifier
                    token a_tok = element(a, -1);
                    if (a_tok && a_tok->chars && strcmp(a_tok->chars, alpha->chars) == 0)
                        stamp_source(m, a_tok);
                    mem = (enode)edecl(mod, (aether)a, autype, m);
                    break;
                }
                    
            } else if (instanceof(mem, enode) && !is_loaded((Au)mem)) {
                // Subsequent iterations - access from previous member
                verify(mem && mem->autype, "cannot resolve from null member");

                // Load previous member to traverse into it
                enode prop = !is_struct(canonical(mem)) ? e_load(a, mem, null) : mem;
                if (null_guard && is_ptr(prop)) {
                    // emit null check: if null, short-circuit to default —
                    // primitives get their zero value; objects are NULL,
                    // never a constructed instance
                    enode cond = e_not(a, prop);
                    mem = access(prop, alpha);
                    etype ct  = canonical(mem);
                    enode def = is_prim((Au)ct) ? e_default_value(a, ct)
                                                : e_null(a, ct);
                    mem = e_ternary(a, cond, def, mem);
                } else {
                    mem = access(prop, alpha);
                }
            } else {
                Au info = head(mem);
                if (null_guard && is_ptr(mem)) {
                    enode cond = e_not(a, mem);
                    enode accessed = access(mem, alpha);
                    etype ct  = canonical(accessed);
                    enode def = is_prim((Au)ct) ? e_default_value(a, ct)
                                                : e_null(a, ct);
                    mem = e_ternary(a, cond, def, accessed);
                } else if (!qualified) {
                    mem = access(mem, alpha);
                }
            }

            Au_t mem_type = isa(mem);
            bool b0, b1, b2, b3, b4;

            // syntax: classify this segment's name token from the member it resolved to,
            // BEFORE any call-expr is applied (so is_func still detects a method/function).
            //   function  — a method or free function (call target)
            //   type      — a type name used here
            //   property  — a field accessed off a parent (a.field); the FIRST segment that
            //               is a plain var stays `ident` (it's a variable reference).
            if (pkzip && mem && mem->autype) {
                Au_t   mt = mem->autype;
                Syntax sk = Syntax__none;
                if      (is_func((Au)mem) || instanceof(mem, macro))   sk = Syntax__function;
                else if (mt->member_type == AU_MEMBER_TYPE)            sk = Syntax__type;
                else if (!first && mt->member_type == AU_MEMBER_VAR)   sk = Syntax__property;
                // the identifier's OWN token, not the one peeked before it
                // was consumed -- pkzip trails the cursor on a chain
                token idt = element(a, -1);
                if (!idt || !idt->chars || !alpha ||
                    strcmp(idt->chars, alpha->chars) != 0) idt = pkzip;
                if (sk != Syntax__none) { idt->syntax = sk; idt->decl = mt; }
            }

            // setter intercept
            if (next_is(a, "[") && in_decl != typeid(efunc) && in_decl != typeid(macro)) {
                Au_t au_rec = is_rec((Au)mem);
                etype r = au_rec ? u(etype, au_rec) : null;
                Au_t setter = r ? find_member(r->autype, "setter", AU_MEMBER_SETTER, 0, true) : null;
                // a vector of packed elements writes its origin in place;
                // class elements are held refs and go through the setter
                Au_t vel9 = (au_rec && au_is_vector(au_rec)) ? vec_elem_of((etype)mem) : null;
                if (setter && !(vel9 && !vel9->is_class)) {
                    push_current(a);
                    array index_keys = read_within(a);
                    token k = element(a, 0);
                    num assign_index = k ? index_of(assign, (Au)k) : -1;
                    bool use_setter = assign_index > 0; // skip ':' (bind) — could be ternary
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
                (next_is(a, "[") || instanceof(mem, macro) || (b0=is_func((Au)mem)) || inherits(mem->autype->src, typeid(lambda)))) {
                
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
                    mem = parse_member_expr(a, mem, in_ref);

                    for (int i = 0; i < depth; i++) {
                        etype mm = (etype)get(prev, i);
                        push_scope(a, (Au)mm, 19);
                    }
                }
            }
        }

        // check if there's more chaining (. or -> for null-guard)
        null_guard = read_if(a, "->") != null;
        bool br = !null_guard && read_if(a, ".") == null;
        if (br) {
            //if (in_ref)
                break;

            // final load if needed [ assign_type, when set, indicates a L-hand side parse ]
            if (instanceof(mem, enode) && !is_loaded((Au)mem) && !assign_type) {
                mem = enode_value(mem, false); // validate the LLVMValueRef we have for these; it should be the memory location (so in effect we have a struct* used as target)
            }
            break;
        }

        // C-style format postfix: x.4f, n.08x, s.20s — the value formats to
        // a string enode and the chain goes on from that string. the lexer
        // may hand the digits and the conversion letter as two tokens
        {
            token spec = peek(a);
            token spec2 = element(a, 1);
            cstr  sc   = spec ? spec->chars : null;
            size_t sl  = sc ? strlen(sc) : 0;
            char  fmt[64] = { 0 };
            int   take = 0;
            if (spec && spec->neighbor && !null_guard && sl && isdigit(sc[0]) &&
                    instanceof(mem, enode)) {
                bool digits = true;
                for (size_t k = 0; k < sl; k++) if (!isdigit(sc[k])) digits = false;
                if (!digits && strchr("dixXofeEgGsc", sc[sl - 1])) {
                    snprintf(fmt, sizeof(fmt), "%s", sc);
                    take = 1;
                } else if (digits && spec2 && spec2->neighbor && spec2->chars &&
                           strchr("dixXofeEgGsc", spec2->chars[strlen(spec2->chars) - 1])) {
                    snprintf(fmt, sizeof(fmt), "%s%s", sc, spec2->chars);
                    take = 2;
                }
            }
            if (take) {
                for (int k = 0; k < take; k++) consume(a, Syntax__number);
                enode v = (enode)mem;
                if (!is_loaded((Au)v)) v = enode_value(v, false);
                etype ct = canonical(v);
                // a scalar formats its value and keeps its unit: 4.9213ft
                Au_t scalar = (ct && ct->autype->is_scalar) ? ct->autype : null;
                if (scalar) {
                    ct = u(etype, scalar->src);
                    v  = e_create(a, ct, (Au)v, false);
                }
                cstr helper = is_realistic((Au)ct) ? "format_f64" :
                              is_integral ((Au)ct) ? "format_i64" : "format_cstr";
                Au_t fm = find_member(typeid(Au), helper, AU_MEMBER_FUNC, 0, false);
                validate(fm, "format helper %s not found", helper);
                mem = e_fn_call(a, u(efunc, fm),
                    a(v, const_string(chars, fmt)), false, false);
                if (scalar)
                    mem = e_add(a, (Au)mem, (Au)e_create(a, etypeid(string),
                        (Au)const_string(chars, scalar->ident), false));
                null_guard = read_if(a, "->") != null;
                if (!null_guard && !read_if(a, ".")) break;
            }
        }

        // More chaining - push context for next iteration
        validate(!is_func((Au)mem), "cannot resolve into function");
        if (mem->autype && !module) {
            push_scope(a, (Au)mem, 20);
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
            next(a, Syntax__none);
            *(OPType*)assign_type = eq(k, ":") ? OPType__bind : (OPType__bind + assign_index);
        }
    }
    return mem;
}

etype pointer(aether, Au);

enode block_builder(silver, array, Au);
enode catch_block_builder(silver, array, Au);

etype shape_pointer(silver, Au, enode);

// scalar suffix: 200px, 1.5em, 90deg — number immediately followed by type name
static enode scalar_suffix(silver a, enode res) {
    if (is_cmode(a) || !next_is_neighbor(a) || !peek(a)) return null;
    string suffix = peek_alpha(a);
    if (!suffix) return null;
    etype scalar_type = rlookup((aether)a, suffix);
    if (!scalar_type || !scalar_type->autype->is_struct || !scalar_type->autype->src)
        return null;
    Au_t storage = scalar_type->autype->src;
    bool lit_is_float = res->literal && isa(res->literal)->is_realistic;
    validate(!lit_is_float || storage->is_realistic,
        "scalar %o requires %s value, got float literal",
        scalar_type, storage->ident);
    consume(a, Syntax__none); // consume the suffix token
    return e_create(a, scalar_type, (Au)res, false);
}
// a builtin is only its call form: a bare name is an identifier (a member
// or local may be named abs, min, max, clamp)
static bool read_builtin(silver a, cstr name) {
    token nm = element(a, 0);
    token br = element(a, 1);
    if (!nm || !br || strcmp(nm->chars, name) != 0 || strcmp(br->chars, "[") != 0)
        return false;
    read_if(a, name);
    return true;
}


enode silver_read_enode(silver a, etype mdl_expect, bool from_ref, bool load) { sequencer

    // consume type_given immediately — acts like a parameter scoped to this depth only
    bool      type_given = a->type_given;
    a->type_given = false;

    // this is more useful than anyone realizes, being in the center of the stack here in read_enode
    // likely needs implementation in read_etype since thats a very common place
    if (!a->no_build && read_if(a, "`"))
        raise(SIGTRAP);

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
        if (mdl_expect && mdl_expect->autype->elements > 0 && mdl_expect->autype->src) {
            etype elem_type = u(etype, mdl_expect->autype->src);
            if (!elem_type) elem_type = (etype)etype_prep((aether)a, mdl_expect->autype->src);
            array elems = array(alloc, mdl_expect->autype->elements);
            while (!next_is(a, "]")) {
                enode elem = parse_expression(a, elem_type, true, true);
                push(elems, (Au)elem);
                if (!read_if(a, ",")) break;
            }
            validate(read_if(a, "]"), "expected ] after array elements");
            return e_create(a, mdl_expect, (Au)elems, false);
        }
        // shorthand struct init: [ field: val, ... ] when type is known
        if (mdl_expect && is_rec(mdl_expect) && peek_fields(a)) {
            enode res = parse_object(a, mdl_expect, true);
            validate(read_if(a, "]"), "expected ] after struct init");
            return res;
        }
        // [ v1, v2 ] literal against a vec-typed slot (alias) seeds
        Au_t vex9 = vec_elem_of(mdl_expect);
        if (vex9)
            return vector_literal(a, vex9);
        enode n = parse_expression(a, mdl_expect, false, true);
        validate(n, "could not read expression");
        validate(read_if(a, "]"),
            "expected ] after %o expression %i", u(etype, n->autype->src), seq);
        // [expr].member — continue member chain on expression result
        while (next_is(a, ".") || next_is(a, "->")) {
            bool null_guard = read_if(a, "->") != null;
            if (!null_guard) read_if(a, ".");
            string field = read_alpha(a);
            validate(field, "expected member name after .");
            etype t = canonical(n);
            enode accessed = (enode)access(n, field);
            validate(accessed, "failed to find member %o on %o", field, t);
            n = accessed;
            if (next_is(a, "[") || instanceof(n, macro) || is_func((Au)n) ||
                inherits(n->autype->src, typeid(lambda)))
                n = parse_member_expr(a, n, from_ref);
        }
        if (load && !is_loaded((Au)n) && (!is_struct(n) || is_ptr(n)))
            n = enode_value(n, false);
        return n;
    }

    // construct [ arg ] — constructor chaining, must be inside a constructor
    if (!cmode && next_is(a, "construct")) {
        consume(a, Syntax__none);
        efunc fn = context_func(a);
        validate(fn && fn->autype->member_type == AU_MEMBER_CONSTRUCT,
            "construct can only be called inside a constructor");
        etype rec = context_record(a);
        validate(rec, "construct requires class context");
        validate(read_if(a, "["), "expected [ after construct");
        enode arg = parse_expression(a, null, false, true);
        validate(arg, "expected argument for construct");
        validate(read_if(a, "]"), "expected ] after construct argument");
        // find matching constructor by arg type
        Au_t arg_type = au_arg_type((Au)arg);
        Au_t ctr = null;
        members(rec->autype, m) {
            if (m->member_type == AU_MEMBER_CONSTRUCT && m->args.count >= 2) {
                Au_t first_arg = au_arg_type(m->args.origin[1]);
                if (first_arg == arg_type || inherits(arg_type, first_arg)) {
                    ctr = m;
                    break;
                }
            }
        }
        validate(ctr, "no matching constructor for type %s", arg_type->ident);
        efunc ctr_fn = (efunc)u(efunc, ctr);
        validate(ctr_fn, "constructor not registered");
        // call on self — no allocation
        enode self_node = fn->target ? (enode)fn->target : null;
        validate(self_node, "no self in constructor context");
        return e_fn_call(a, ctr_fn, a(self_node, arg), false, false);
    }

    // cast To [ from ] — explicit downcast expression
    if (!cmode && read_if(a, "cast")) {
        etype target = read_etype(a, null);
        validate(target, "expected type after cast");
        validate(read_if(a, "["), "expected [ after cast type");
        enode expr = parse_expression(a, null, false, true);
        validate(expr, "expected expression in cast [ ]");
        validate(read_if(a, "]"), "expected ] after cast expression");
        return e_direct_cast(a, expr, target);
    }

    // handle typed operations, converting to our expected model (if no difference, it passes through)
    if (a->expr_level > 0 && peek && (is_alpha(peek) || eq(peek, "struct"))) {
        etype mdl_found = read_etype(a, null);
        // we need to address definition of ordered members within collections of all sort
        validate (!type_given || !mdl_found || (mdl_found != mdl_expect),
            "redundant type expression");
        
        if (mdl_found) {
            // here we must 'peek' at a body; which if not available we go default
            array b = peek_initializer(a);
            enode res0 = null;
            if (from_ref) mdl_found = pointer((aether)a, (Au)mdl_found);
            // T asm [ inputs ]: the asm body is the value
            if (read_if(a, "asm")) {
                res0 = e_create(a, mdl_expect, (Au)parse_asm(a, mdl_found), false);
            } else if (b) {
                array expr = read_initializer(a);
                if (expr) {
                    push_tokens(a, (tokens)expr, 0);
                    if (next_is(a, "[") && is_rec(mdl_found))
                        res0 = parse_object(a, mdl_found, false);
                    else {
                        res0 = read_enode(a, mdl_found, false, load);
                    }
                    pop_tokens(a, false);
                } else {
                    res0 = null; // use default
                    if (!mdl_expect) {
                        mdl_expect = mdl_found; // not expecting anything, no conversion
                    } else if (mdl_expect != mdl_found) {
                        res0 = e_create(a, mdl_found, (Au)null, false); // required conversion
                    }
                }
                    
            } else if (a->assign_type == OPType__bind &&
                    (from_ref || is_class(mdl_found) || is_ptr(mdl_found))) {
                res0 = e_null(a, mdl_found);
            } else {
                res0 = e_create(a, mdl_found, null, false);
            }
            enode conv = e_create(a, mdl_expect, (Au)res0, false);
            return conv;
        }
    }

    shape sh = (shape)read_literal(a, typeid(shape));
    if (sh && (sh->count == 1 || sh->explicit || mdl_expect == etypeid(shape))) {
        enode op;
        if (sh->explicit || mdl_expect == etypeid(shape))
            op = e_create(a, etypeid(shape), (Au)sh, false);
        else {
            op = e_operand(a, _i64(sh->data[0]), mdl_expect ? mdl_expect : etypeid(i64));
            enode sc = scalar_suffix(a, op);
            if (sc) return sc;
        }

        return op; // otherwise interpreted as an i64
    }

    Au lit = read_literal(a, null);
    if (lit) {
        Au info = header(lit);
        if (isa(lit) == typeid(i64) && mdl_expect &&
           (mdl_expect->autype == typeid(string) || mdl_expect->autype == typeid(symbol) || mdl_expect->autype == typeid(const_string)))
            lit = (Au)unicode_char(*(i64*)lit);
        else if (isa(lit) == typeid(unichar) && mdl_expect &&
           (mdl_expect->autype == typeid(string) || mdl_expect->autype == typeid(symbol) ||
            mdl_expect->autype == typeid(const_string) || mdl_expect->autype == typeid(cstr) ||
            (mdl_expect->autype->is_pointer && mdl_expect->autype->src == typeid(i8))))
            lit = (Au)unicode_char(*(unichar*)lit);
        a->expr_level++;
        enode res = e_operand(a, lit, mdl_expect);
        a->expr_level--;

        enode sc = scalar_suffix(a, res);
        if (sc) return sc;

        return e_create(a, mdl_expect, (Au)res, false);
    }
    
    // shell script at design and runtime, for a silver 1.0
    if (!cmode && next_is(a, "$", "(")) {
        consume(a, Syntax__none);
        consume(a, Syntax__none);
        fault("shell syntax not implemented for 88");
    }

    if (read_if(a, "sizeof")) {
        bool read_br = (cmode && read_if(a, "(")) || (!cmode && read_if(a, "["));
        push_current(a);
        etype mdl = read_etype(a, null);
        if (!mdl) {
            pop_tokens(a, false);
            enode expr = parse_expression(a, null, false, true);
            verify(expr, "expected type or expression for sizeof");
            mdl = canonical(expr);
        } else {
            pop_tokens(a, true);
        }
        if (read_br)
            verify((cmode && read_if(a, ")")) || (!cmode && read_if(a, "]")), "expected closing-bracket");

        return e_operand(a, _i64(mdl->autype->typesize), mdl_expect);
    }

    // functional macros are only useful for these few built-in's outside of vtable stuff
    // theres no reason to implement actual macro keyword in silver until its simply a 'better solution in general'
    if (read_builtin(a, "min")) {
        verify(read_if(a, "["), "expected [ after min");
        enode val_a = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode val_b = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_min(a, val_a, val_b);
    }

    if (read_builtin(a, "max")) {
        verify(read_if(a, "["), "expected [ after max");
        enode val_a = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode val_b = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_max(a, val_a, val_b);
    }

    // math builtins: sqrt, sin, cos, tan, asin, acos, atan, exp, log, floor, ceil, round
    {
        static struct { cstr name; int op; } math_ops[] = {
            {"sqrt", 0}, {"sin", 1}, {"cos", 2}, {"tan", 3},
            {"asin", 4}, {"acos", 5}, {"atan", 6}, {"exp", 7},
            {"log", 8}, {"floor", 9}, {"ceil", 10}, {"round", 11}, {"atan2", 12}, {"pow", 13},
            {null, 0}
        };
        for (int i = 0; math_ops[i].name; i++) {
            token nm = element(a, 0);
            if (!nm || !nm->chars || strcmp(nm->chars, math_ops[i].name) != 0)
                continue;
            // only a call (name followed by '[') is the math builtin; a bare name is a
            // plain identifier (e.g. a member named `log`), so leave it for lookup.
            token after = element(a, 1);
            if (!after || !after->chars || strcmp(after->chars, "[") != 0)
                continue;
            {
                read_if(a, math_ops[i].name);
                verify(read_if(a, "["), "expected [ after %s", math_ops[i].name);
                enode val = parse_expression(a, null, false, false);
                if (math_ops[i].op == 12 || math_ops[i].op == 13) { // atan2, pow are two-arg
                    read_if(a, ",");
                    enode val2 = parse_expression(a, null, false, false);
                    verify(read_if(a, "]"), "expected ] after %s", math_ops[i].name);
                    return e_math2(a, math_ops[i].op, val, val2);
                }
                verify(read_if(a, "]"), "expected ] after %s", math_ops[i].name);
                return e_math(a, math_ops[i].op, val);
            }
        }
    }

    // mingw's setjmp macro expands to this; C spells the call with parens
    if (read_if(a, "__builtin_frame_address")) {
        bool br = read_if(a, "[") != null;
        if (!br) verify(read_if(a, "("), "expected ( after __builtin_frame_address");
        enode lvl = parse_expression(a, null, false, false);
        verify(read_if(a, br ? "]" : ")"), "expected close of __builtin_frame_address");
        return aether_e_frameaddress((aether)a, lvl);
    }

    if (read_builtin(a, "abs")) {
        verify(read_if(a, "["), "expected [ after abs");
        enode val = parse_expression(a, null, false, true);
        verify(read_if(a, "]"), "expected ] after abs");
        enode neg = e_op(a, OPType__sub, string("-"), (Au)e_operand(a, _i64(0), canonical(val)), (Au)val);
        enode cmp = e_op(a, OPType__less, string("<"), (Au)val, (Au)e_operand(a, _i64(0), canonical(val)));
        return e_ternary(a, cmp, neg, val);
    }

    if (read_builtin(a, "clamp")) {
        verify(read_if(a, "["), "expected [ after clamp");
        enode val = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode lo  = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode hi  = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ]");
        return e_clamp(a, val, lo, hi);
    }

    if (read_if(a, "mix")) {
        verify(read_if(a, "["), "expected [ after mix");
        enode from = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode to   = parse_expression(a, null, false, false);
        read_if(a, ",");
        enode t    = parse_expression(a, null, false, false);
        verify(read_if(a, "]"), "expected ] after mix");
        // for typed operands, inline: from * (1 - t) + to * t.
        // only requires * (scalar) and + on the target type; subtraction
        // would force every interpolable type (rgba, vec4f, ...) to also
        // define operator-, which is uncommon on color/vector aggregates.
        if (is_rec(from) || is_prim(from)) {
            enode one      = e_operand(a, (Au)_f64(1.0), etypeid(f64));
            enode one_mt   = e_op(a, OPType__sub, string("-"), (Au)one, (Au)t);
            enode la       = e_op(a, OPType__mul, string("*"), (Au)from, (Au)one_mt);
            enode lb       = e_op(a, OPType__mul, string("*"), (Au)to,   (Au)t);
            return e_op(a, OPType__add, string("+"), (Au)la, (Au)lb);
        }
        // for generic Au, dispatch through Au.mix (polymorphic runtime)
        Au_t f_mix = find_member(typeid(Au), "mix", AU_MEMBER_FUNC, 0, false);
        verify(f_mix, "Au.mix not found");
        return e_fn_call(a, u(efunc, f_mix), a(from, to, t), false, true);
    }

    // moduleid[ name ] — the module's runtime Au_t. a bare known module
    // name resolves at design time; a string expression looks up at runtime
    if (!cmode && read_if(a, "moduleid")) {
        validate(read_if(a, "["), "expected [ after moduleid");
        Au_t f_mod = find_member(typeid(Au), "module_lookup", AU_MEMBER_FUNC, 0, false);
        verify(f_mod, "Au.module_lookup not found");
        enode marg = null;
        string mname = peek_alpha(a);
        token  mt1   = mname ? element(a, 1) : null;
        bool   known = false;
        if (mname && mt1 && eq(mt1, "]")) {
            if (a->name && eq(mname, a->name->chars))
                known = true;
            else
                members (a->autype, im) {
                    if (im->is_namespace && im->ident && eq(mname, im->ident)) {
                        known = true;
                        break;
                    }
                }
        }
        if (known) {
            consume(a, Syntax__none);
            marg = e_operand(a, (Au)mname, etypeid(symbol));
        } else
            marg = parse_expression(a, null, false, true);
        validate(read_if(a, "]"), "expected ] after moduleid");
        return e_fn_call(a, (efunc)u(efunc, f_mod), a(marg), false, false);
    }

    if (!cmode && read_if(a, "typeid")) {
        bool read_br = read_if(a, "[") != null;
        push_current(a);
        etype mdl = read_etype(a, null);
        if (mdl) {
            pop_tokens(a, true);
            if (read_br)
                verify(read_if(a, "]"), "expected closing-bracket after typeid");
            return e_create(a, (etype)mdl_expect, (Au)e_typeid(a, mdl), false);
        }
        pop_tokens(a, false);
        enode expr = parse_expression(a, null, false, true);
        if (read_br)
            verify(read_if(a, "]"), "expected closing-bracket after typeid");
        Au_t f_typeid = find_member(typeid(Au), "__typeid", AU_MEMBER_FUNC, 0, false);
        return e_fn_call(a, u(efunc, f_typeid), a(expr), false, false);
    }

    // `vec` is the keyword; `new` parses as an alias until the sweep ends
    if (!cmode && (read_if(a, "vec") || read_if(a, "new"))) {
        etype mdl = read_etype(a, null);
        validate(mdl, "expected element type after %o", element(a, -1));
        etype mdl_c = canonical(mdl);
        enode esize = null;
        shape sh  = null;
        array seeds = null;
        // vec T = null slot; vec T [] = made growable; a shape allocates fixed
        bool  made   = false;
        bool  shaped = false;
        if (read_if(a, "[")) {
            made = read_if(a, "]") != null;
            if (!made) {
                esize = parse_expression(a, null, false, true);
                sh = instanceof(esize->literal, shape);
                validate(read_if(a, "]"), "expected closing-bracket after vec Type [");
                shaped = true;
            }
            // vec T [] [ v1, v2 ] — seed values into the made growable
            if (made && read_if(a, "[")) {
                seeds = array(32);
                while (peek(a) && !next_is(a, "]")) {
                    push(seeds, (Au)parse_expression(a, null, false, true));
                    read_if(a, ",");
                }
                validate(read_if(a, "]"), "expected ] after vec seed data");
            }
        }
        if (!shaped) {
            Au_t  elem = (mdl_c ? mdl_c : mdl)->autype;
            etype pt   = vector_etype(a, elem);
            if (!made) {
                // vec T <expr> on the same line: the expr is the value
                token  tt = element(a, -1);
                token  nx = peek(a);
                if (nx && tt && nx->line == tt->line && !eq(nx, "]") &&
                        !eq(nx, ",") && !eq(nx, ")")) {
                    enode r = parse_expression(a, pt, false, load);
                    r = e_create(a, pt, (Au)r, false);
                    r->meta_a = (Au)elem;
                    return r;
                }
                enode nul = e_null((aether)a, pt);
                nul->meta_a = (Au)elem;
                return nul;
            }
            enode vec = e_create(a, pt, null, false);
            vec->meta_a = (Au)elem;
            if (seeds)
                each(seeds, enode, s)
                    e_assign((aether)a, vec, (Au)s, OPType__assign_add);
            return vec;
        }
        // a shaped vec is the vector class sized to the shape: count is
        // its element count and origin is the raw memory C code takes
        Au_t  selem = (mdl_c ? mdl_c : mdl)->autype;
        etype spt   = vector_etype(a, selem);
        enode total;
        if (sh)
            total = e_operand(a, _i64(shape_total(sh)), etypeid(i64));
        else if (canonical(esize) && canonical(esize)->autype == typeid(shape)) {
            efunc f_tot = (efunc)u(efunc, find_member(typeid(shape), "total",
                AU_MEMBER_FUNC, 0, false));
            total = e_fn_call(a, f_tot, a(esize), false, false);
        } else
            total = e_create(a, etypeid(i64), (Au)esize, false);
        // the count rides into alloc_new: elements sit inline, one allocation
        if (!a->no_build) ((aether)a)->alloc_count = total;
        enode vec   = e_create(a, spt, null, false);
        vec->meta_a = (Au)selem;
        efunc f_rs  = (efunc)u(efunc, find_member(typeid(vector), "resize",
            AU_MEMBER_FUNC, 0, true));
        e_fn_call(a, f_rs, a(vec, total), false, false);

        /// parse optional constant data: vec i32[4x4] [ 1 2 3 4, 1 1 1 1, ... ]
        if (read_if(a, "[")) {
            int   top_stride = (sh && sh->count > 1) ? sh->data[sh->count - 1] : 0;
            int   num_index  = 0;
            array nodes      = array(64);

            while (peek(a) && !next_is(a, "]")) {
                enode e = read_enode(a, mdl, false, true);
                e = e_create(a, mdl, (Au)e, false);
                push(nodes, (Au)e);
                num_index++;
                if (top_stride && (num_index % top_stride == 0)) {
                    validate(read_if(a, ",") || next_is(a, "]"),
                        "expected ',' between rows (stride: %i)", top_stride);
                }
                if (!top_stride)
                    read_if(a, ",");
            }
            validate(read_if(a, "]"), "expected ] after constant data");

            /// copy constant data into the vector's origin
            if (len(nodes) > 0) {
                enode origin9 = e_convert_or_cast(
                    (aether)a, pointer((aether)a, (Au)selem), vec);
                e_vector_init(a, mdl, origin9, nodes);
            }
        }
        return vec;
    }

    if (!cmode && read_if(a, "local")) {
        etype mdl = read_etype(a, null);
        validate(read_if(a, "["), "expected [ after local Type");
        shape sh = (shape)read_literal(a, typeid(shape));
        i64   count = 0;
        enode esize = null;
        if (sh) {
            count = shape_total(sh);
        } else {
            esize = parse_expression(a, etypeid(i64), true, true);
            if (esize->literal)
                count = *(i64*)esize->literal;
        }
        validate(read_if(a, "]"), "expected ] after local Type [");
        enode arr = (count > 0) ?
            e_stack_array(a, mdl, count) :
            e_stack_array_dynamic(a, mdl, esize);
        if (read_if(a, "[")) {
            array nodes = array(64);
            while (peek(a) && !next_is(a, "]")) {
                a->type_given = true;
                enode e = read_enode(a, mdl, false, true);
                e = e_create(a, mdl, (Au)e, false);
                push(nodes, (Au)e);
                read_if(a, ",");
            }
            validate(read_if(a, "]"), "expected ] after local initializer");
            if (len(nodes) > 0)
                e_vector_init(a, mdl, arr, nodes);
        }
        return arr;
    }

    // cast syntax: (expr) to Type — handled in parse_ternary after (expr)

    if (!cmode && read_if(a, "null"))
        return e_null(a, mdl_expect);

    if (cmode && read_if(a, "*")) {
        enode n = read_enode(a, null, false, true);
        return deref(n);
    }

    // parenthesized expressions
    if (next_is(a, "(")) {
        consume(a, Syntax__none);
        // support C-style cast here, only in cmode (macro definitions)
        if (cmode) {
            push_current(a);
            array meta = null;
            etype inner = read_etype(a, null);
            if (inner) {
                if (next_is(a, ")")) {
                    consume(a, Syntax__none);
                    pop_tokens(a, true);
                    enode res = e_create(a, inner, (Au)parse_expression(a, inner, false, true), false);
                    return e_create(a, mdl_expect, (Au)res, false);
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
        /*
        // (expr to Type) — inline cast within parens
        if (read_if(a, "to")) {
            etype target = read_etype(a, null);
            if (target) {
                validate(read_if(a, ")"), "expected ) after (expr to Type)");
                a->parens_depth--;
                return e_convert_or_cast((aether)a, canonical(target), expr);
            }
            // runtime to — fall back to parse_expression for typeid[...]
            enode target_type = parse_expression(a, etypeid(Au_t), false, true);
            verify(target_type, "expected type or type expression after 'to'");
            Au_t f_convert = find_member(typeid(Au), "__convert", AU_MEMBER_FUNC, 0, false);
            verify(f_convert, "Au.__convert not found for runtime 'to' conversion");
            validate(read_if(a, ")"), "expected ) after (expr to typeid[...])");
            a->parens_depth--;
            return e_fn_call(a, u(efunc, f_convert), a(target_type, expr), false, false);
        } */
        validate(read_if(a, ")"), "expected ) after expression, found %o", peek(a));
        a->parens_depth--;
        enode n = (enode)e_create(a, mdl_expect, (Au)
            parse_ternary(a, (enode)expr, (etype)mdl_expect, load), false);
        // (expr).member — continue member chain on parenthesized result
        while (next_is(a, ".") || next_is(a, "->")) {
            bool null_guard = read_if(a, "->") != null;
            if (!null_guard) read_if(a, ".");
            string field = read_alpha(a);
            validate(field, "expected member name after .");
            enode accessed = (enode)access(n, field);
            validate(accessed, "failed to find member %o on %o", field, n);
            n = accessed;
            if (next_is(a, "[") || instanceof(n, macro) || is_func((Au)n) ||
                inherits(n->autype->src, typeid(lambda)))
                n = parse_member_expr(a, n, from_ref);
        }
        if (load && !is_loaded((Au)n) && (!is_struct(n) || is_ptr(n)))
            n = enode_value(n, false);
        return n;
    }

    // { keyword tokens } — compacted token literals
    if (!cmode && next_is(a, "{")) {
        return read_keywords(a, (etype)mdl_expect);
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
        enode cond = read_enode(a, etypeid(bool), false, true);
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
            mdl_expect, (Au)e_not(a, expr), false);
    }

    // bitwise NOT operator
    else if (read_if(a, "~")) {
        validate(!from_ref, "unexpected ~ after ref");
        enode expr = read_enode(a, null, false, true);
        return e_create(a,
            mdl_expect, (Au)e_bitwise_not(a, expr), false);
    }

    // unary negation
    else if (read_if(a, "-")) {
        enode expr = read_enode(a, null, false, true);
        validate(a->no_build || canonical(expr)->autype->is_integral || canonical(expr)->autype->is_realistic,
            "negation requires numeric type");
        return e_create(a,
            mdl_expect, (Au)e_neg(a, expr), false);
    }

    // 'ref' operator (reference)
    // we only allow one reference depth, for multiple we would resort to using defined types
    // this is to make code cleaner, with more explicit definition
    // we must use in-ref state only at neighboring calls
    // silver doesnt really want to tell you how to code, 
    // it is reduced in nature, to define line by line rather than add horizontal inline features
    // its less surface area for complexity

    else if (!cmode && (read_if(a, "ref") || read_if(a, "@"))) {
        static int seq2;
        seq2++;
        validate(!from_ref, "unexpected double-ref (use type definitions)");

        // peek ahead: if next tokens form a type, this is a cast (ref u8 vdata)
        // otherwise it's address-of (ref vdata)
        push_current(a);
        etype ref_cast_type = read_etype(a, null);
        token ref_last = element(a, -1);
        token ref_next = element(a,  0);
        bool  ref_has_expr  = ref_cast_type && ref_next &&
                              (ref_last->line == ref_next->line || next_is(a, "["));
        pop_tokens(a, false);

        if (ref_has_expr) {
            // ref type expr — cast expr to ref type
            etype cast_type = read_etype(a, null);
            etype ref_type = pointer((aether)a, (Au)cast_type);
            enode expr = read_enode(a, null, false, true);
            return e_create(a, ref_type, (Au)expr, false);
        }

        if (ref_cast_type) {
            // ref Type with no expr — null pointer
            etype cast_type = read_etype(a, null);
            return e_null(a, pointer((aether)a, (Au)cast_type));
        }

        // ref expr — take address of expr
        enode expr = read_enode(a, null, true, false);

        if (next_is(a, "[")) //  || next_neighbor(a) <- breaks with comma of course; we have to form syntax differently here and use Multiple Lines.
            expr = parse_member_expr(a, expr, from_ref);

        etype ref_type = pointer((aether)a, (Au)expr->autype);

        // when expr is unloaded, its value is already the address (GEP) —
        // return it directly as a loaded pointer rather than going through
        // e_create which would load and inttoptr the dereferenced value
        validate(!expr->loaded || is_func((Au)expr->autype), "cannot take ref of loaded value");
        enode ref_node = enode_ref((aether)a, expr, ref_type);
        return mdl_expect ? e_create(a, mdl_expect, (Au)ref_node, false) : ref_node;
    }

    //printf("seq = %i\n", seq);

    if (!cmode && (next_is(a, "__FILE__") || next_is(a, "__LINE__") || next_is(a, "__SEQUENCE__"))) {
        token tk = consume(a, Syntax__none);
        enode n_src; Au n_line, n_seq;
        alloc_origin_args((aether)a, &n_src, &n_line, &n_seq);
        if (eq(tk, "__FILE__"))     return n_src;
        if (eq(tk, "__LINE__"))     return e_operand(a, n_line, etypeid(i32));
        if (instanceof(n_seq, enode)) return (enode)n_seq;
        return e_operand(a, n_seq, etypeid(i64));
    }

    // lambda fname [ ctx… ] — the function's address, bound to context values
    if (!cmode && next_is(a, "lambda")) {
        push_current(a);
        consume(a, Syntax__none);
        // lambda [ args ] body — inline function; copies in what it uses
        if (next_is(a, "[")) {
            pop_tokens(a, true);
            return parse_inline_lambda(a);
        }
        string fname = peek_alpha(a);
        Au_t   fau   = fname ? lexical(a->lexical, cstring(fname)) : null;
        // a func, or the head of a member chain (obj.method) — not a type name
        if (fau && fau->member_type != AU_MEMBER_TYPE)
            pop_tokens(a, true); // keep `lambda` consumed; member follows
        else
            pop_tokens(a, false);
    }

    mem = parse_member(a, null, null, mdl_expect, from_ref);

    verify(!next_is(a, "?") && !next_is(a, "??"),
        "ternaries require parenthesis: ( condition ) %o", peek(a));

    if (!mem && cmode) return null;
    if (!mem) {
        etype unexpected_type = read_etype(a, null);
        validate(!unexpected_type, "unexpected type %o", unexpected_type);
    }

    validate(!instanceof(mem, edecl), "'%s' parsed as declaration but expected expression (name conflicts with type?)", mem->autype->ident);
    validate(mem, "unexpected token '%o'", peek(a));
    if (load && !is_loaded((Au)mem) && (!is_struct(mem) || is_ptr(mem)))
        mem = enode_value(mem, false);
    Au info = head(mem);

    if (f && mdl_expect && !(mdl_expect->autype == typeid(bool) && !is_bool(mem)))
        return e_create(a, mdl_expect, (Au)mem, false);

    return (enode)mem;
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

// parse an extension .ag file inline into THIS module: track its source (host watch),
// strip an optional `extend <module>` header (validating it names this module), then
// replay its statements so its classes/methods register against this module. used both
// for sibling extensions (import within the module dir) and for `import M with ext`,
// where ext.ag is compiled into M's own build.
static void parse_extension(silver a, path module_source, bool reset_imports) {
    if (index_of(a->artifacts, (Au)module_source) < 0)
        push(a->artifacts, (Au)module_source);
    array ext_toks = array(alloc, 64);
    parse_tokens(a, (Au)module_source, ext_toks);
    int   start = 0;
    token first = len(ext_toks) ? (token)ext_toks->origin[0] : null;
    if (first && !strcmp(first->chars, "extend")) {
        token name_tok = len(ext_toks) > 1 ? (token)ext_toks->origin[1] : null;
        validate(name_tok && !strcmp(name_tok->chars, a->name->chars),
            "extend declares '%s' but compiled into module '%o'",
            name_tok ? name_tok->chars : "?", a->name);
        first->syntax = Syntax__keyword;
        if (name_tok) name_tok->syntax = Syntax__type;
        start = 2;
    }
    // siblings reset so each one's own includes re-run the import finalization; the
    // `with` drain does NOT (it keeps processed_imports true so it can't re-drain).
    if (reset_imports) a->processed_imports = false;
    path saved_module_file = hold(a->module_file);
    a->module_file = hold(module_source);
    push_tokens(a, (tokens)ext_toks, start);
    while (a->cursor < len(a->tokens))
        parse_statement(a);
    // defer the format write: bodies are stashed and parsed later in build_record_parse
    if (a->format && len(a->format)) {
        if (!a->fmt_ext) a->fmt_ext = hold(array(alloc, 8));
        push(a->fmt_ext, (Au)hold(ext_toks));
    }
    pop_tokens(a, false);
    drop(a->module_file);
    a->module_file = saved_module_file;
}

// --listen 'Class.method [ expr ]' — re-evaluate the query each statement,
// storing into the gate; listen prints emit only while the gate holds true
static void silver_listen_requery(silver a) {
    aether ae = (aether)a;
    if (!a->listen_query || a->no_build) return;
    array q = a->listen_query;
    for (int i = 0; i < len(q); i++) {
        token t = (token)q->origin[i];
        char  c = t->chars[0];
        if (!(isalpha(c) || c == '_')) continue;
        if (i > 0 && !strcmp(((token)q->origin[i - 1])->chars, ".")) continue;
        if (!strcmp(t->chars, "true") || !strcmp(t->chars, "false") ||
            !strcmp(t->chars, "null")) continue;
        if (!lexical(a->lexical, t->chars)) return; // out of scope: keep gate
    }
    aether_listen_gate_create(ae);
    ae->listen_emitting = true;
    push_tokens(a, (tokens)q, 0);
    enode cond = parse_expression(a, etypeid(bool), false, true);
    pop_tokens(a, false);
    ae->listen_emitting = false;
    aether_listen_gate_store(ae, cond);
}

static void validate_try_block(silver a, array body) {
    for (int i = 0; body && i < len(body); i++) {
        token t = (token)body->origin[i];
        if (eq(t, "return") || eq(t, "break") ||
                eq(t, "continue"))
            validate(false,
                "%s is not supported inside try/catch/finally yet",
                t->chars);
    }
}

enode parse_try(silver a) { sequencer
    validate(read_if(a, "try"), "expected try");
    array try_tokens = read_body(a);
    validate(try_tokens && len(try_tokens), "expected try body");
    validate_try_block(a, try_tokens);

    array catch_tokens = null;
    array finally_tokens = null;
    string catch_name = null;
    if (read_if(a, "catch")) {
        if (next_is(a, "[")) {
            array spec = read_within(a);
            push_tokens(a, (tokens)spec, 0);
            catch_name = read_alpha(a);
            validate(catch_name, "expected catch binding name");
            validate(read_if(a, ":"),
                "expected : after catch binding");
            etype catch_type = read_etype(a, null);
            validate(catch_type &&
                canonical(catch_type)->autype == typeid(string),
                "catch binding must be string");
            validate(!peek(a), "unexpected catch binding token: %o",
                peek(a));
            pop_tokens(a, false);
        }
        catch_tokens = read_body(a);
        validate(catch_tokens && len(catch_tokens),
            "expected catch body");
        validate_try_block(a, catch_tokens);
    }
    if (read_if(a, "finally")) {
        finally_tokens = read_body(a);
        validate(finally_tokens && len(finally_tokens),
            "expected finally body");
        validate_try_block(a, finally_tokens);
    }
    validate(catch_tokens || finally_tokens,
        "try requires catch or finally");

    evar catch_var = null;
    statements catch_scope = null;
    if (catch_name) {
        catch_scope = new(statements, mod, (aether)a,
            autype, def(top_scope(a), null,
                AU_MEMBER_NAMESPACE, 0));
        Au_t member = def_member(catch_scope->autype,
            cstring(catch_name), typeid(string), AU_MEMBER_VAR, 0);
        catch_var = evar(mod, (aether)a, autype, member,
            loaded, false, is_local, true);
        etype_register((aether)a, (Au)member,
            (Au)catch_var, true);
        etype_implement((etype)catch_var, false);
    }

    array catch_context = null;
    if (catch_scope) {
        catch_context = array(1);
        push(catch_context, (Au)catch_scope);
    }
    subprocedure build_try = subproc(a, block_builder, null);
    subprocedure build_catch = catch_scope ?
        subproc(a, catch_block_builder, catch_context) :
        subproc(a, block_builder, null);
    subprocedure build_finally = subproc(a, block_builder, null);
    return aether_e_try((aether)a, try_tokens, catch_tokens,
        finally_tokens, catch_var, build_try, build_catch,
        build_finally);
}

// C++ specializations exist only if the sampled TU instantiates them. scan
// the raw token stream for Name<args> requests before include finalization
// so the import TU can carry explicit instantiations for them — aclang
// filters: the name must be a class template in that unit, and the
// specialization must not already exist
static void scan_template_requests(silver a) {
    // the module's own stream: inside an extension or import parse the
    // live tokens are the pushed file, the root sits at the stack bottom
    aether e  = (aether)a;
    array  ts = (e->stack && e->stack->count)
              ? (array)((tokens_data)e->stack->origin[0])->tokens_list
              : (array)a->tokens;
    for (int i = 1; ts && i + 2 < ts->count; i++) {
        token id = (token)ts->origin[i];
        token lt = (token)ts->origin[i + 1];
        if (!isalpha(id->chars[0]) || !eq(lt, "<") || !lt->neighbor ||
            id->literal || is_keyword((Au)id))
            continue;
        // qualified base: A::B<...> — fold in neighboring :: segments
        string base = string(id->chars);
        for (int b = i; b >= 2; b -= 2) {
            token qs = (token)ts->origin[b - 1];
            token qn = (token)ts->origin[b - 2];
            if (!eq(qs, "::") || !qs->neighbor || !((token)ts->origin[b])->neighbor ||
                !isalpha(qn->chars[0]))
                break;
            base = f(string, "%s::%o", qn->chars, base);
        }
        // args: idents or integer literals, comma separated — the key
        // format matches spec_name (Name<a,b>) so registration lines up
        string key = f(string, "%o<", base);
        bool   ok  = false;
        bool   arg = true;
        for (int j = i + 2; j < ts->count; j++) {
            token t = (token)ts->origin[j];
            if (arg) {
                if (isdigit(t->chars[0]))
                    // decimal-normalize so the key matches spec_name output
                    concat(key, f(string, "%i", (i32)strtoll(t->chars, null, 0)));
                else if (isalpha(t->chars[0]))
                    concat(key, (string)t);
                else break;
                arg = false;
            } else if (eq(t, ",")) {
                concat(key, string(","));
                arg = true;
            } else {
                ok = eq(t, ">");
                break;
            }
        }
        if (!ok) continue;
        concat(key, string(">"));
        if (!e->template_requests)
            e->template_requests = (array)hold((Au)array(alloc, 8));
        bool dup = false;
        each (e->template_requests, string, s)
            if (eq(s, key->chars)) { dup = true; break; }
        if (!dup) push(e->template_requests, (Au)key);
    }
}

enode parse_statement(silver a)
{
    sequencer
#ifndef NDEBUG
    if (a->verbose) {
        token tk = peek(a);
        if (tk) {
            num line = tk->line;
            path src = tk->source;
            string f = filename(src);
            fprintf(stderr, "[%s:%-4lld] ", f->chars, line);
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
    //catcher cat = context_catcher(a);
    //if (cat) cat->last_return = false;
    verify(!f || !f->autype->is_mod_init, "unexpected init function");
    a->last_return      = null;
    a->expr_level       = 0;
    a->assign_type      = OPType__undefined;
    a->setter_key_tokens = null;
    a->setter_fn        = null;
    a->statement_origin = hold(peek(a));
    if (f && a->statement_origin && !a->no_build)
        emit_debug_loc((aether)a,
            a->statement_origin->source->chars,
            a->statement_origin->line, a->statement_origin->column);
#ifndef NDEBUG
    if (f && aether_has_listen((aether)a) && a->statement_origin && !a->no_build) {
        silver_listen_requery(a);
        token tk = a->statement_origin;
        num   ln = tk->line;
        char  trace_msg[512];
        int   pos = 0;
        // build "[file:line] tokens..." prefix
        string fn = filename(tk->source);
        pos += snprintf(trace_msg, sizeof(trace_msg), "[%s:%-4lld] ", fn->chars, ln);
        for (int j = 0; j < tk->indent && pos < (int)sizeof(trace_msg) - 2; j++)
            trace_msg[pos++] = ' ';
        for (int i = a->cursor; i < len(a->tokens) && pos < (int)sizeof(trace_msg) - 2; i++) {
            token t = (token)a->tokens->origin[i];
            if (t->line != ln) break;
            int r = snprintf(trace_msg + pos, sizeof(trace_msg) - pos, "%s ", t->chars);
            if (r > 0) pos += r;
        }
        aether_emit_listen_line((aether)a, trace_msg);
    }
#endif
    Au_t      top       = top_scope(a);
    silver    module    = is_module(top) ? a : null;
    etype     rec_top   = is_rec(top) ? u(etype, top) : null;

    if (!a->processed_imports && !next_is(a, "export") && !next_is(a, "import") && !next_is(a, "extend") && !next_is(a, "ifdef") && !next_is(a, "ifndef")) {
        a->processed_imports = true;
        if (a->uninstall && !a->is_external) uninstall_products(a);
        // `import M with ext…`: the ext files were handed to THIS module's build via
        // the `extensions` list. now that our own imports are resolved, fold each one in
        // (its own imports + `extend M` + members) BEFORE include finalization so their
        // headers are covered too. parse_extension keeps processed_imports true here, so
        // the recursive parse can't re-enter this block — processed_imports IS the guard
        // (no separate drained flag), and the list survives for the product id below.
        if (a->extensions && len(a->extensions)) {
            each (a->extensions, path, ep)
                parse_extension(a, ep, false);
        }
        scan_template_requests(a);
        aether_import_includes((aether)a);
        // NOTE: no format-cache body-skip here. sub-modules / sibling extension
        // files are parsed inline as part of THIS compilation (not dlopen'd), so
        // skipping any body un-registers its classes (e.g. Navigate) and breaks
        // the build. always parse fully; the .f map is still written as a byproduct.
    }

    // standard statements first, only in context of functions
    if (f) {
        if (read_if(a, "no-op"))  return e_noop(a, null);
        if (next_is(a, "try"))   return parse_try(a);
        if (read_if(a, "throw")) {
            enode msg = parse_expression(a, etypeid(string),
                false, true);
            return e_fault(a, msg);
        }
        if (next_is(a, "return")) return parse_return (a);
        if (next_is(a, "break"))    return parse_break   (a);
        if (next_is(a, "continue")) return parse_continue(a);
        if (next_is(a, "expect")) {
            push_current(a);
            consume(a, Syntax__none);
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
                if (read_if(a, ",")) {
                    // message emits INSIDE the fail branch — a passing
                    // expect never builds its string
                    enode ctx = e_expect_begin(a, cond);
                    enode msg = parse_expression(a, etypeid(string), false, true);
                    return e_expect_end(a, ctx, msg);
                }
                return e_expect(a, cond, null);
            }
        }
        if (next_is(a, "for") || next_is(a, "while"))
                                  return parse_for    (a);
        if (next_is(a, "if"))     return parse_if_else(a);
        if (next_is(a, "switch")) return parse_switch (a);
        if (read_if(a, "asm"))    return parse_asm    (a, null);
        if (next_is(a, "memcpy") || next_is(a, "memset")) return parse_memop(a);
        if (next_is(a, "log")) {
            // log[ x ] stays the math builtin; log <expr> is the logger
            token t1 = element(a, 1);
            bool ident_use = t1 && (eq(t1, "[") || eq(t1, ".") || eq(t1, ":") ||
                index_of(assign, (Au)t1) >= 0);
            if (!ident_use) return parse_log(a);
        }
    }
    
    if (next_is(a, "ifdef"))  return parse_ifdef_else(a, false);
    if (next_is(a, "ifndef")) return parse_ifdef_else(a, true);

    if (next_is(a, "extend")) {
        consume(a, Syntax__keyword);
        string ext_module = read_alpha(a);
        token ext_tok = element(a, -1);
        if (ext_tok) ext_tok->syntax = Syntax__type;
        validate(ext_module && !strcmp(ext_module->chars, a->name->chars),
            "extend declares '%o' but current module is '%o'", ext_module, a->name);
        return e_noop(a, null);
    }

    //verify(!next_is(a, "undefined"), "undefined is invalid access-level");



    u64 traits = 0;
    interface access = interface_undefined;

    // export func: tagged to run under SILVER_EXPORT after module
    // init; every other export form parses in silver_read_def
    if (!rec_top && next_is(a, "export")) {
        token nx = element(a, 1);
        if (nx && nx->chars && strcmp(nx->chars, "func") == 0) {
            consume(a, Syntax__keyword);
            access = interface_export;
        }
    }
    if (access == interface_export) {
        // tagged above; skip the access chain
    } else if (rec_top && read_if(a, "mutable")) {
        access = interface_mutable;
        traits = AU_TRAIT_IS_FLUX;
    } else if (rec_top && read_if(a, "manual")) {
        access = interface_public;
        traits = AU_TRAIT_UNMANAGED;
    } else if (rec_top && read_if(a, "attrib")) {
        access = interface_public;
        traits = AU_TRAIT_STATIC | AU_TRAIT_IS_ATTRIB;
    } else if (rec_top && read_if(a, "context")) {
        access = interface_context;
        traits = AU_TRAIT_IS_CONTEXT;
    } else if (rec_top && read_if(a, "default")) {
        /// a public that also carries the default-argument bit: a bare argv
        /// value converts into it (Au_args -> find_member AU_TRAIT_IS_DEFAULT),
        /// and it is the separator — everything after belongs to the program.
        access = interface_public;
        traits = AU_TRAIT_IS_DEFAULT;
    } else if (!next_is(a, "export"))
        // export is an interface value only via the export-func
        // branch above; manifest exports must not read as access
        access = read_enum(a, interface_undefined, typeid(interface));
    bool has_access = access != interface_undefined;
    if (access == interface_intern && next_is(a, "")) {

    }
    validate(!(has_access && rec_top && is_struct(rec_top)),
        "access levels are not applicable to struct members");

    etype member_meta = null;
    if (rec_top && read_if(a, "[")) {
        member_meta = read_etype(a, null);
        validate(member_meta && is_class(member_meta),
            "expected class type for member meta A");
        validate(read_if(a, "]"),
            "expected ] after member meta A");
    }

    //print_tokens(a, seq);

    if (module && !next_is(a, "func") && peek_def(a)) {
        verify(!has_access || (access == interface_public || access == interface_intern || access == interface_abstract),
            "undefined is invalid access-level");
        {
            string def_name = peek_def(a);
            bool is_enum  = def_name && eq(def_name, "enum");
            bool is_type  = def_name && (eq(def_name, "class") || eq(def_name, "struct") ||
                eq(def_name, "alias") || next_is_class(a, false));
            if (is_type)
                a->has_module_type = true;
            if (a->strict && a->has_module_func && (is_type || is_enum))
                validate(false, "type definitions must appear before functions at module level");
            if (a->strict && a->has_module_type && is_enum)
                validate(false, "enum definitions must appear before class/struct/alias at module level");
        }

        return (enode)read_def(a, access);
    }


    bool is_static = read_if(a, "static") != null;
    if (read_if(a, "unmanaged"))
        traits |= AU_TRAIT_UNMANAGED;

    validate(!is_struct(top) || (!access || access == interface_public),
        "unexpected access level found in struct");

    // elaborate keyword: narrow an inherited member's type without adding new storage
    /*
    
    redundant from property overrides
        its best not to add new keyword when the activity is merely specifying a member without a value
    
    if (rec_top && read_if(a, "elaborate")) {
        string name = read_alpha(a);
        validate(name, "expected member name after elaborate");
        validate(read_if(a, ":"), "expected ':' after elaborate %o", name);
        etype  rtype = read_etype(a, null);
        validate(rtype, "expected type after elaborate %o:", name);
        Au_t   base_m = find_member(rec_top->autype->context, cstring(name), AU_MEMBER_VAR, 0, true);
        validate(base_m, "elaborate: '%o' not found in base class", name);
        Au_t   m = def_member(top, cstring(name), canonical(rtype)->autype, AU_MEMBER_VAR, 0);
        m->is_elaborate = true;
        m->access_type  = base_m->access_type;
        etype_register((aether)a, (Au)m, null, true);
        enode e = (enode)evar(mod, (aether)a, autype, m, loaded, false);
        etype_register((aether)a, (Au)m, (Au)e, true);
        return e;
    }
    */

    // not yet sold on needing override; its less arguments but you can do that with func init, too
    // also override would need to require the cast and operator
    // no args would mean override.. thats not difficult to implement..

    //bool      is_override = !f ?
    //    read_if(a, "override")   != null : false;
    token entry = peek(a);
    bool      is_lambda = !f ?
        read_if(a, "lambda")     != null : false;
    bool      def_func  = !f && !is_lambda ?
        read_if(a, "func")       != null : false;
    if (def_func && module)
        a->has_module_func = true;
    bool      is_cast   = !f && !is_static && !(def_func|is_lambda) ?
        read_if(a, "cast")       != null : false;
    bool      is_oper   = !f && !is_static && !(def_func|is_lambda) && !is_cast ?
        read_if(a, "operator")   != null : false;
    bool      is_left   = is_oper ? read_if(a, "left") != null : false;
    bool      is_post_ctr = !f && !is_static && !(def_func|is_lambda) && !is_cast && !is_oper ?
        (read_if(a, "post") && read_if(a, "construct")) : false;
    bool      is_ctr    = !f && !is_static && !(def_func|is_lambda) && !is_cast && !is_oper && !is_post_ctr ?
        read_if(a, "construct")  != null : false;
    if (is_post_ctr) is_ctr = true; // post construct parses like construct, flagged differently
    bool      is_getter = !f && !is_static && !(def_func|is_lambda) && !is_cast && !is_oper && !is_ctr ?
        read_if(a, "getter")     != null : false;
    bool      is_setter = !f && !is_static && !(def_func|is_lambda) && !is_cast && !is_oper && !is_ctr && !is_getter ?
        read_if(a, "setter")     != null : false;

    OPType assign_enum = OPType__undefined;
    enode mem = (!is_cast && !is_oper && !is_getter && !is_setter && !is_ctr) ?
        parse_member(a, (ARef)&assign_enum,
            (def_func || is_lambda) ? typeid(efunc) : ((access || f || (!!module)) ? typeid(evar) : null), null, false) : null;
    Au_t mem_info = isa(mem);


    validate(!mem || (!def_func || !instanceof(mem, efunc) || mem->autype->context != top),
        "redefinition of %o", mem);

    if (module && mem &&
            mem->autype->member_type == AU_MEMBER_VAR &&
            mem->autype->access_type == interface_expect) {
        set(a->defs_expect, string(mem->autype->ident), true);
    }

    validate(!(is_lambda|def_func) || mem,
        "expected member identifier to follow function or lambda");

    // entry is the first token of this definition -- funcs, constructs,
    // casts, operators and accessors all come through here.
    // context == top means it is DECLARED here; a mere reference to an
    // imported type must never claim to be its origin
    if (mem && mem->autype && mem->autype->context == top)
        stamp_source(mem->autype, entry);

    validate(!is_static || mem,
        "expected member identifier to follow static");

    validate(mem || (!mem || access == interface_undefined),
        "expected member-name after access '%o'", estring(typeid(interface), access));

    if (access && mem && mem->autype) {
        if (access == interface_abstract) {
            mem->autype->is_abstract = true;
            mem->autype->access_type = interface_public;
        } else {
            mem->autype->access_type = (u8)access;
        }
        if (access == interface_expect || access == interface_export ||
            !!(traits & AU_TRAIT_IS_CONTEXT))
            mem->autype->is_required = true;
        mem->autype->is_context = access == interface_context;
    }

    if (mem)
        mem->autype->traits |= traits;

    //if (is_default && mem && mem->autype) {
    //    mem->autype->is_default  = true;
    //    mem->autype->is_required = true;
    //}

    // if no access then full access
    if (!access) access = interface_public;

    push_current(a);

    string op_name = null;
    OPType op_type = is_oper ? read_operator(a, (ARef)&op_name) : OPType__undefined;
    enode  e       = null;

    // reverse slots need their own symbol, or they collide with the forward one
    if (is_oper && is_left && op_type == OPType__mul)
        { op_type = OPType__lmul;   op_name = string("_lmul");   }
    if (is_oper && is_left && op_type == OPType__div)
        { op_type = OPType__ldiv;   op_name = string("_ldiv");   }
    if (is_oper && is_left && op_type == OPType__left)
        { op_type = OPType__lleft;  op_name = string("_lleft");  }
    if (is_oper && is_left && op_type == OPType__right)
        { op_type = OPType__lright; op_name = string("_lright"); }

    if (mem!=null || is_ctr || is_getter || is_setter || is_oper || is_lambda || def_func || is_cast)
    {
        validate(!is_oper || op_type != OPType__undefined, "operator required");
        
        // check if this is a nested, static member (we need to back off and read_enode can handle this)
        Au_t top_type = isa(u(etype, top));

        
        statements in_code    = context_code(a);
        bool       is_const   = mem && mem->autype->is_const;

        if (def_func|is_lambda|is_ctr|is_getter|is_setter|is_cast|is_oper) {
            if (module || rec_top) {
                u64 traits = (is_static ? AU_TRAIT_STATIC :
                             (is_lambda ? 0 : (rec_top ? AU_TRAIT_IMETHOD : 0))) |
                             (is_lambda ? AU_TRAIT_LAMBDA : 0);
                Au_t au = mem ? mem->autype : null;
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
                if (entry && entry->source)
                    ((efunc)e)->source_file = hold((path)entry->source);
                if (is_post_ctr) e->autype->is_default = true; // flag post construct (reuse is_default)
                e->autype->access_type = (u8)access;
                if (access != interface_intern) {
                    arg_list(e->autype, arg) {
                        Au_t src = arg->src;
                        if (src && src->is_c && !src->is_primitive && !src->is_struct && !src->is_enum && !src->typesize)
                            validate(false,
                                "public func '%s' exposes C type '%s' in args — use intern func",
                                e->autype->ident, src->ident ? src->ident : "?");
                    }
                    Au_t rtype = e->autype->rtype;
                    if (rtype && rtype->is_c && !rtype->is_primitive && !rtype->is_struct && !rtype->is_enum && !rtype->typesize)
                        validate(false,
                            "public func '%s' returns C type '%s' — use intern func",
                            e->autype->ident, rtype->ident ? rtype->ident : "?");
                }
                efunc fn = (efunc)get(a->registry, (Au)au);
                verify(fn && fn == e && fn->autype == au, "unexpected registration state");
            }

        }
        else if (rec_top || module) {

            bool is_f = is_getter|is_ctr|is_lambda|def_func|is_cast;
            verify(assign_enum == OPType__bind || is_f, "invalid member syntax, expected member:type[ initializer ]");
            etype rtype = (mem && mem->autype->member_type == AU_MEMBER_DECL) && !is_f ?
                            read_etype(a, null) : null;
            bool  is_const = false;
            // `member : vec T [...]` — a managed buffer slot: assignment
            // auto-drops the old value and holds the new one (e_assign)
            bool  decl_new = !rtype && (next_is(a, "vec") || next_is(a, "new"));
            a->expr_level++;
            array expr = rtype ? read_initializer(a) : read_expression(a, (etype*)&rtype, &is_const);
            a->expr_level--;

            validate(rtype, "could not infer type");

            if (a->debugmember && mem->autype->ident && eq(a->debugmember, mem->autype->ident)) {
                print("breaking for member registration: %o", a->debugmember);
                //raise(SIGTRAP);
            }

            mem->autype->access_type = (u8)access;
            mem->autype->member_type = AU_MEMBER_VAR;
            mem->autype->src         = canonical(rtype)->autype;
            mem->autype->is_static   = is_static;

            // inline fixed-size array member (e.g. `local i16 [4]`): canonical()
            // resolves the shaped stack-array type down to its element type, so
            // carry the element count onto the member itself. aether's struct
            // layout (etype_implement) then emits [N x T] inline rather than a
            // dangling pointer, and the member becomes indexable.
            if (rtype->autype->elements > 0 && !mem->autype->elements)
                mem->autype->elements = rtype->autype->elements;

            // a vector member is a class slot; only a raw buffer is shaped
            if (decl_new && !rtype->autype->is_class)
                mem->autype->is_shaped = true;

            if (mem->autype->src->is_pointer && rtype->is_explicit_ref) { // this is easier to register and maintain (membership will dictate the start of this ref model)
                mem->autype->is_explicit_ref = true;
                mem->autype->src = mem->autype->src->src;
                mem->is_explicit_ref = true;
            }

            // override: redeclaring an inherited member to change its default
            // initializer. the type must match the inherited slot; no new
            // storage is added — just a different initializer is emitted at
            // construction time for this subclass.
            // members from Au itself are not user-defined overrides — skip.
            if (rec_top && rec_top->autype->context) {
                Au_t inherited = find_member(rec_top->autype->context,
                    (cstr)mem->autype->ident, AU_MEMBER_VAR, 0, true);
                if (inherited && inherited->context != typeid(Au)) {
                    // default-override only when the type is an exact match.
                    // refinement (type differs) falls through as a shadow for
                    // now; proper refinement requires rebinding `a` to alloc
                    // during initializer parse.
                    bool exact_type = mem->autype->src == inherited->src &&
                        mem->autype->meta.a == inherited->meta.a;
                    if (exact_type) {
                        // overriding an intern is invalid: interns have no af-bit
                        // slot, so the override's default can't be marked to suppress
                        // the base default. error rather than silently shadow.
                        validate(inherited->access_type != interface_intern,
                            "cannot override intern member '%s' of '%s' — interns have no af-bit slot; make it public or rename",
                            (cstr)mem->autype->ident,
                            inherited->context && inherited->context->ident ? inherited->context->ident : "?");
                        mem->autype->is_override = true;
                    }
                }
            }
            if (access == interface_public && rec_top) {
                Au_t src = mem->autype->src;
                if (src && src->is_c && !src->is_primitive && !src->is_struct && !src->is_enum && !src->typesize)
                    validate(false,
                        "public member '%s' exposes C type '%s' — use intern",
                        mem->autype->ident, src->ident ? src->ident : "?");
            }
            if (rtype->meta_a) {
                mem->autype->meta.a = (Au_t)rtype->meta_a;
            }
            if (rtype->meta_b) {
                mem->autype->meta.b = instanceof(rtype->meta_b, Au_t) ?
                    (Au)rtype->meta_b : (Au)hold(rtype->meta_b);
            }

            Au_t au = mem->autype;
            etype_register((aether)a, (Au)au, null, true);
            mem = (enode)evar(mod, (aether)a, autype, au, loaded, false,
                meta_a, rtype->meta_a, meta_b, rtype->meta_b,
                initializer, (tokens)map_initializer(a, string(au->ident), (tokens)expr, au->access_type));
            
            e = (enode)mem;
            etype_register((aether)a, (Au)au, (Au)mem, true);

            efunc fn = (efunc)get(a->registry, (Au)e->autype);
            verify(fn && fn == e && fn->autype == mem->autype, "unexpected registration state");

            //au_register(e->autype, (etype)e);
            if (is_static || mem->autype->is_static || module) {
                if (is_static && rec_top) {
                    verify(rec_top, "invalid use of static (must be a class member, not a global item -- use intern for module-interns)");
                    mem->autype->alt = (cstr)cstr_copy((cstr)((string)(f(string, "%o_%o", symbol_name((Au)rec_top), e))->chars));
                }
                etype_implement((etype)e, false);
            }

        } else if (a->setter_key_tokens && a->setter_fn) {
            validate(mem, "expected member for setter");
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
            mem->autype->is_const = module != null;
            validate (!is_func(mem->autype->context) || mem->autype->is_explicit_ref || is_ptr(mem),
                "function arguments are read-only");

            e = parse_assignment(a, (enode)mem, assign_enum, mem->autype->is_const);
            a->expr_level--;
            a->assign_type = OPType__undefined;
            a->left_hand = true;

        } else {
            // default
            validate(!assign_enum, "unexpected assignment");
            //validate((!mem || mem->autype->member_type != AU_MEMBER_VAR) || !f,
            //    "orphaned member expression in function");
        }
        
    } else {
        validate(!is_cast, "expected type after cast keyword");
    }

    if (member_meta) {
        validate(mem && (def_func ||
            mem->autype->member_type == AU_MEMBER_VAR),
            "member meta A applies only to fields and functions");
        mem->autype->meta.m = member_meta->autype;
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
    statements st = new(statements, mod, (aether)a, autype, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_scope(a, (Au)st, 21);
    enode vr = null;
    while (peek(a)) {
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
    etype  rtype   = null;
    string name    = string(mem->ident);
    bool   is_cast = member_type == AU_MEMBER_CAST;
    //etype rec_ctx = context_class(a); if (!rec_ctx) rec_ctx = context_struct(a);
    etype rec_ctx = context_class(a);
    if (!rec_ctx) rec_ctx = context_struct(a);

    validate(member_type == AU_MEMBER_CAST || read_if(a, "["), "expected function args [");
    Au_t au = mem; //def(top_scope(a), ident ? ident->chars : null, AU_MEMBER_FUNC, traits);
    verify(mem->member_type == AU_MEMBER_DECL, "already defined: %o", mem); // since we allow for prop-style invocation of functions, the design must be no clashing with var names
    
    // `::` in the arg list makes this a lambda before any arg is read:
    // a lambda takes no implicit target — its context struct holds the object
    if (member_type == AU_MEMBER_FUNC) {
        int depth = 0;
        for (int i = 0; ; i++) {
            token t = element(a, i);
            if (!t) break;
            if (eq(t, "[")) depth++;
            else if (eq(t, "]")) { if (!depth) break; depth--; }
            else if (!depth && eq(t, "::")) {
                traits |=  AU_TRAIT_LAMBDA;
                traits &= ~AU_TRAIT_IMETHOD;
                break;
            }
        }
    }

    au->member_type = member_type;
    au->operator_type = op_type;
    au->traits = traits;
    if (au->module && au->module != a->autype) {
        fprintf(stderr, "MODULE OVERWRITE [silver_read_function]: %s (%p) module %p -> %p\n", au->ident, au, au->module, a->autype);
        exit(1);
    }
    au->module = a->autype;

    
    Au_t override = null;
    if (!rec_ctx)
        rec_ctx = context_struct(a);
    else {
        override = find_member(
            rec_ctx->autype->context, name->chars,
            member_type, 0, true);

        au->is_override = override != null;

        if (au->is_override) {
            au->member_index = override->member_index;
        } else {
            au->member_index = next_function_index(rec_ctx->autype);
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
    push_scope(a, (Au)mem, 22);
    bool first = true;
    Au_t target = null;

    // parse args (move to generic)
    for (; member_type != AU_MEMBER_CAST ;) {
        if (read_if(a, "]"))
            break;
        
        // `::` opens contextual args on ANY function — that makes it a lambda
        bool skip = false;
        if (read_if(a, "::")) {
            skip = true;
            in_context = true;
        }
        validate(skip || first || read_if(a, ","), "expected comma separator between arguments %i", seq);
        
        bool    is_inlay  = read_if(a, "inlay") != null;    push_current(a);
        etype   t         = read_etype(a, null);            pop_tokens(a, t != null);
        string  n         = t ? null : read_alpha(a); // optional
        micro*  ar        = in_context ? (micro*)&au->members : (micro*)&au->args;

        if (!t) {
            if (!read_if(a, ":")) {
                // no colon: treat name as a type (C-style declaration)
                t = elookup(n->chars);
                validate(t, "unknown type or expected ':' after name '%o'", n);
                n = null;
            }
        } else
            validate(!read_if(a, ":"),
                "unexpected : after type provided first: %o", t);

        // verify arg name does not shadow a class member
        if (n) {
            etype rec = context_record(a);
            if (rec) {
                Au_t found = find_member(rec->autype, cstring(n), AU_MEMBER_VAR, 0, false);
                validate(!found, "argument '%o' shadows member of %s", n, rec->autype->ident);
            }
        }
        
        bool is_ref = read_if(a, "ref") != null || read_if(a, "@") != null;
        if (!t) t = read_etype(a, null); // we need to avoid the literal check in here!
        validate(t, "expected alpha-numeric identity for type or name, found %o", peek(a));
        Au_t arg = alloc_arg(au, n ? n->chars : null, t->autype);
        // propagate meta (e.g. PathPt for `array PathPt`) onto the arg Au_t so
        // the parameter retains its element type for indexing/copying
        if (t->meta_a) arg->meta.a = (Au_t)t->meta_a;
        if (t->meta_b) arg->meta.b = t->meta_b;
        arg->is_inlay = is_inlay;
        arg->is_explicit_ref = is_ref;

        if (member_type == AU_MEMBER_CONSTRUCT && !len(name))
            name = form(string, "with_%s%o", arg->is_explicit_ref ? "ref_" : "", t);
        else if (member_type == AU_MEMBER_CAST && !len(name))
            name = form(string, "cast_%o", t);
        
        if (is_inlay) {
            validate(is_struct(arg->src),
                "inlay applies only to struct members in arguments");
        }
        micro_push((micro_*)ar, (Au)arg);
        if (first)
            first = false;
    }
    pop_scope(a);

    if (op_name)
        name = op_name;
    
    bool arrow = read_if(a, "->") != null;
    rtype = arrow ? read_etype(a, null) : null;
    validate(!arrow || rtype, "unknown return type after -> (found %o)", peek(a));
    array inline_expr = null;
    array const_tokens = null;

    if (next_is(a, "[")) {
        inline_expr = read_body(a);
    } else if (next_is(a, "{")) {
        // capture { ... } as raw tokens, excluding outer { }
        array raw = array(32);
        consume(a, Syntax__none); // skip opening {
        int depth = 1;
        while (depth > 0 && peek(a)) {
            token t = peek(a);
            if (eq(t, "{")) depth++;
            if (eq(t, "}")) depth--;
            if (depth > 0) // skip closing }
                push(raw, (Au)t);
            consume(a, Syntax__none);
        }
        const_tokens = raw;
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
    au->rtype   = rtype->autype;
    if (rtype->meta_a) au->meta.a = (Au_t)rtype->meta_a;
    if (rtype->meta_b) au->meta.b = rtype->meta_b;

    // cast/getter/setter get their canonical name (cast_bool, index_*, setter)
    // only here, after the return type is read — so the override probe above ran
    // against an empty ident and missed it. re-probe with the real name so an
    // override of e.g. Au.cast_bool collapses onto the inherited vtable slot
    // instead of emitting a duplicate method.
    if (rec_ctx && !override &&
        (member_type == AU_MEMBER_CAST || member_type == AU_MEMBER_GETTER ||
         member_type == AU_MEMBER_SETTER)) {
        override = find_member(rec_ctx->autype->context, name->chars, member_type, 0, true);
        if (override) {
            au->is_override  = true;
            au->member_index = override->member_index;
        }
    }

    // validate override return type matches base (resolve aliases on both sides)
    if (override && override->rtype && rtype->autype != override->rtype) {
        Au_t r_resolved = rtype->autype;
        while (r_resolved && r_resolved->is_alias && r_resolved->src)
            r_resolved = r_resolved->src;
        Au_t o_resolved = override->rtype;
        while (o_resolved && o_resolved->is_alias && o_resolved->src)
            o_resolved = o_resolved->src;
        validate(r_resolved == o_resolved || inherits(r_resolved, o_resolved),
            "override '%s' return type '%s' does not match base return type '%s'",
            au->ident, rtype->autype->ident, override->rtype->ident);
    }

    // validate override arg count and types match base
    if (override) {
        validate(au->args.count == override->args.count,
            "override '%s' has %i args, base has %i",
            au->ident, au->args.count, override->args.count);
        for (int oi = 0; oi < au->args.count && oi < override->args.count; oi++) {
            Au_t arg_au   = (Au_t)au->args.origin[oi];
            Au_t arg_base = (Au_t)override->args.origin[oi];
            Au_t a_src = arg_au->src;
            Au_t b_src = arg_base->src;
            while (a_src && a_src->is_alias && a_src->src) a_src = a_src->src;
            while (b_src && b_src->is_alias && b_src->src) b_src = b_src->src;
            validate(a_src == b_src || inherits(a_src, b_src),
                "override '%s' arg %i type '%s' does not match base type '%s'",
                au->ident, oi, arg_au->src->ident, arg_base->src->ident);
        }
    }

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
        autype, au,
        body,   (tokens)b,
        const_tokens, const_tokens,
        inline_return, inline_expr,
        remote_code, !is_using && !len(b),
        has_code,    len(b) || is_init || is_dealloc || cgen || const_tokens,
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

    token  name_tok = element(a, 0);   // the candidate type-name token
    string alpha = read_alpha(a);
    if (alpha && !next_is(a, ".")) {
        Au_t found_au = lexical(a->lexical, cstring(alpha));
        if (found_au && found_au->member_type == AU_MEMBER_VAR) {
            pop_tokens(a, false);
            return null;
        }
        bool q_next = next_is(a, "::");
        Au_t plain_au = found_au; // `::` may be a lambda separator, not a scope
        if ((!found_au && next_is(a, "<")) || q_next) {
            // sampled C++ types register flat: geo::box, minmax<i32>
            if (q_next) found_au = null;
            push_current(a);
            string key = f(string, "%o", alpha);
            bool ok = true;
            while (read_if(a, "::")) {
                string seg = read_alpha(a);
                if (!seg) {
                    ok = false;
                    break;
                }
                concat(key, string("::"));
                concat(key, seg);
            }
            if (ok && read_if(a, "<")) {
                concat(key, string("<"));
                for (;;) {
                    string targ  = read_alpha(a);
                    if (targ) {
                        Au_t targ_au = lexical(a->lexical, cstring(targ));
                        if (!targ_au || targ_au->member_type != AU_MEMBER_TYPE) {
                            ok = false;
                            break;
                        }
                        concat(key, string(targ_au->ident));
                    } else {
                        i64* n = (i64*)read_literal(a, typeid(i64));
                        if (!n) {
                            ok = false;
                            break;
                        }
                        concat(key, f(string, "%i", (i32)*n));
                    }
                    if (!read_if(a, ","))
                        break;
                    concat(key, string(","));
                }
                if (ok) ok = read_if(a, ">") != null;
                if (ok) concat(key, string(">"));
            }
            if (ok) found_au = lexical(a->lexical, cstring(key));
            if (ok && !found_au) {
                // using-directive: Imf::X publishes Imf_3_4::X via src
                Au_t ns = lexical(a->lexical, cstring(alpha));
                num  al = (num)strlen(alpha->chars);
                while (!found_au && ns && ns->src && ns->src->ident) {
                    string key2 = f(string, "%s%o", ns->src->ident,
                        mid(key, al, len(key) - al));
                    found_au = lexical(a->lexical, cstring(key2));
                    ns = ns->src;
                }
            }
            pop_tokens(a, found_au != null);
            if (!found_au) found_au = plain_au;
        }
        mdl = found_au ? etype_prep((aether)a, found_au) : null;
        if (instanceof(mdl, evar)) {
            pop_tokens(a, false);
            return null;
        }
    }
    // it resolved to a model (not a var) → this name is a TYPE; color it as such. when read
    // at a nested etype_level (a meta/generic argument, e.g. the `string` in map element[string]),
    // give it the unique _meta kind instead of a plain type ref.
    if (mdl && name_tok) {
        name_tok->syntax = a->etype_level > 0 ? Syntax__meta : Syntax__type;
        name_tok->decl   = mdl->autype;
    }
    pop_tokens(a, mdl != null); /// save if we are returning a model
    return mdl;
}

static shape read_shape(silver a) {
    shape s = (shape)read_literal(a, typeid(shape));
    if (s) {
        // a dimensional shape (32x32x1) is a special type, not a plain number — re-stamp
        // the literal the lexer tagged `number`.
        token st = element(a, -1);
        if (st) st->syntax = Syntax__type;
    }
    if (!s) {
        i64* i = (i64*)read_literal(a, typeid(i64));
        if (i) {
            i64* cp = (i64*)calloc(1, sizeof(i64));
            *cp = *i;
            s = shape_from(1, cp);
        }
    }
    return s;
}

etype read_etype(silver a, array* p_expr) { sequencer
    etype mdl   = null;
    array expr  = null;

    token f = peek(a);
    if (!f || f->literal) return null;

    push_current(a);
    bool is_ref    = read_if(a, "ref") != null || read_if(a, "@") != null;
    bool is_struct = read_if(a, "struct") != null;
    // type position: `vec T` is Au's vector class with T as its meta.
    // a bracket after T is the VALUE form (vec T [] / vec T [ n ]) — bail
    // so the expression parser owns the whole thing
    if (!is_ref && !is_struct && read_if(a, "vec")) {
        etype elem = read_etype(a, null);
        if (!elem || next_is(a, "[")) {
            pop_tokens(a, false);
            return null;
        }
        etype ec = canonical(elem);
        pop_tokens(a, true);
        etype vt = etype_prep((aether)a, typeid(vector));
        return etype(mod, (aether)a, autype, vt->autype,
            meta_a, (Au)(ec ? ec : elem)->autype);
    }
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
        
        if (prim_mdl) {
            // read_if stamped the primitive name as `keyword`; it's actually a TYPE
            // (int/char/float/void/…). re-stamp the just-consumed token.
            token pt = element(a, -1);
            if (pt) pt->syntax = Syntax__type;
            prim_mdl = model_adj(a, prim_mdl);
        }
        if (is_struct) {
            string alpha = read_alpha(a);
            if (alpha) {
                Au_t au = lexical_traits(a->lexical, cstring(alpha), AU_TRAIT_STRUCT, 0);
                if (au)
                    mdl = (etype)etype_prep((aether)a, au);
            }
        } else if (read_if(a, "lambda")) {
            // lambda [ name: type ] body — an inline literal, not a type
            if (next_is(a, "[")) {
                token t1 = element(a, 1);
                token t2 = element(a, 2);
                if ((t1 && eq(t1, "]")) ||
                    (t1 && isalpha(t1->chars[0]) && t2 && eq(t2, ":"))) {
                    pop_tokens(a, false);
                    return null;
                }
            }
            // `lambda fname [ ctx ]` instances one from a func — that is
            // an expression, not a type; leave it for read_enode
            string l_pk  = peek_alpha(a);
            Au_t   l_pau = l_pk ? lexical(a->lexical, cstring(l_pk)) : null;
            if (l_pau && l_pau->member_type != AU_MEMBER_TYPE) {
                pop_tokens(a, false);
                return null;
            }
            // lambda ReturnType [ ArgTypes ]
            etype rtype = read_etype(a, null);
            if (!rtype) rtype = etypeid(none);
            Au_t lambda_au = def(top_scope(a), null, AU_MEMBER_TYPE, AU_TRAIT_LAMBDA);
            lambda_au->src     = typeid(lambda);
            lambda_au->rtype   = rtype->autype;
            lambda_au->context = typeid(lambda);
            lambda_au->is_funcptr = true;
            // read arg types [ Type, Type, ... ]
            if (read_if(a, "[")) {
                if (!read_if(a, "]")) {
                    do {
                        etype arg_type = read_etype(a, null);
                        validate(arg_type, "expected type in lambda args");
                        def_arg(lambda_au, null, arg_type->autype, 0);
                    } while (read_if(a, ","));
                    validate(read_if(a, "]"), "expected ] after lambda arg types");
                }
            }
            lambda_au->is_pointer = true;
            mdl = etype(mod, (aether)a, autype, lambda_au);
        } else
            mdl = prim_mdl ? prim_mdl : read_named_model(a);

        if (mdl && mdl->autype->is_meta)
            mdl = pointer((aether)a, (Au)mdl->autype->src);
        
        if (mdl && mdl->autype->member_type != AU_MEMBER_TYPE && !mdl->autype->is_meta) {
            pop_tokens(a, false);
            validate(!is_ref, "expected valid type after ref");
            return null;
        }

        if (mdl) {
            bool has_depth_meta = read_if(a, "<") != null;
            bool already_has_meta = mdl->meta_a != null;
            bool read_meta = !already_has_meta && mdl->autype->meta.a && is_class(mdl) &&
                             (has_depth_meta || a->etype_level == 0);

            Au meta_a_val = null;
            Au meta_b_val = null;

            if (!is_cmode(a) && read_meta) {
                // meta_a: always a type
                a->etype_level++;
                etype t = read_etype(a, null);
                if (!t && !(mdl->autype->context && mdl->autype->context->meta.a) && mdl->autype != typeid(shape)) {
                    a->deferred_hit = true;
                }
                if (!t) t = etypeid(Au);
                meta_a_val = (Au)t->autype;
                a->etype_level--;

                // meta_b: shape or bracketed type, optional
                if (mdl->autype->meta.b) {
                    if (mdl->autype->meta.b == typeid(shape)) {
                        shape s = read_shape(a);
                        if (s) meta_b_val = (Au)s;
                    } else if (next_is(a, "[")) {
                        read_if(a, "[");
                        a->etype_level++;
                        etype imdl = read_etype(a, null);
                        a->etype_level--;
                        validate(imdl, "expected type for meta_b, found %o", peek(a));
                        meta_b_val = (Au)imdl->autype;
                        validate(read_if(a, "]"), "expected ] after meta_b type");
                    }
                }
            }

            validate(!has_depth_meta || read_if(a, ">"), "expected > to close <meta>");

            mdl = meta_a_val ? etype(mod, (aether)a, autype, mdl->autype,
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

    if (mdl && mdl->autype->member_type != AU_MEMBER_TYPE && !mdl->autype->is_meta)
        mdl = null;

    bool is_any = false;
    if (!is_cmode(a) && mdl && is_class(mdl)) {
        is_any = read_if(a, "*") != null;
        if (is_any) {
            validate(mdl->autype->access_type != interface_intern, "polymorphic types cannot be defined internal");
        }
    }

    etype t = (mdl && (is_any || is_ref)) ?
        etype(mod, (aether)a, autype,
            is_ref ? pointer((aether)a, (Au)mdl->autype)->autype : mdl->autype,
            is_explicit_ref, is_ref, is_any, is_any) : mdl;

    pop_tokens(a, mdl != null); // if we read a model, we transfer token state
    return t;
}

// codegen_generate_fn is aether's: it owns the codegen class and the
// base implementation. a second definition here is a duplicate symbol.

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

    // remote transport moved to the silver tls module;
    // wire Http there when this feature lands
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
            } else {
                // each > begins its OWN command: without this flush,
                // consecutive > lines ran glued into one shell line
                if (cmd) {
                    push(res, (Au)cmd);
                    cmd = null;
                }
                token_line = t->line;
            }
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
    for (int i = 0; i < len(input); i++) {
        token t = (token)input->origin[i];
        if (starts_with(t, ">")) {
            token_line = t->line;
        } else if (token_line >= 0 && t->line != token_line) {
            token_line = -1;
        }
        if (starts_with(t, "+"))
            continue;
        if (starts_with(t, "-framework")) {
            i++; // skip the framework name that follows
            continue;
        }
        // bare -DNAME (no =) is a consumer define, never build config
        if (starts_with(t, "-D") && !strchr(t->chars, '='))
            continue;
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

string import_libs(silver a, array input, map output, map fw_output) {
    string libs = string(alloc, 128);
    for (int i = 0; i < len(input); i++) {
        string t = (string)input->origin[i];
        if (starts_with(t, "-l")) {
            string n = mid(t, 2, len(t) - 2);
            if (strchr(n->chars, '{'))
                n = interpolate(n, (Au)a);
            set(output, n, _bool(true));
        } else if (starts_with(t, "-framework") && i + 1 < len(input)) {
            string fw = framework_name((string)input->origin[++i]);
            set(fw_output, fw, _bool(true));
        }
    }
    return libs;
}

void import_include_paths(silver a, array input, array output) {
    each(input, string, t) {
        if (starts_with(t, "-I")) {
            string expanded = interpolate(t, (Au)a);
            push(output, (Au)f(path, "%s", expanded->chars + 2));
        }
    }
}

// { (define) ?? tokens… } keeps the tokens when define is true;
// a single 'string' value token is unquoted to its literal
static array import_conditionals(silver a, array b) {
    int   ln  = len(b);
    array res = array(alloc, ln ? ln : 1);
    for (int i = 0; i < ln; i++) {
        token t = (token)b->origin[i];
        token o = (token)(i + 1 < ln ? b->origin[i + 1] : null);
        if (!eq(t, "{") || !o || !eq(o, "(")) {
            push(res, (Au)t);
            continue;
        }
        token name = (token)(i + 2 < ln ? b->origin[i + 2] : null);
        token cl   = (token)(i + 3 < ln ? b->origin[i + 3] : null);
        token op   = (token)(i + 4 < ln ? b->origin[i + 4] : null);
        verify(name && cl && eq(cl, ")") && op && eq(op, "??"),
            "line %i: expected { (define) ?? value } in import config",
            t ? t->line : 0);
        int depth = 1, end = -1;
        for (int j = i + 5; j < ln; j++) {
            token n = (token)b->origin[j];
            if (eq(n, "{")) depth++;
            else if (eq(n, "}") && --depth == 0) { end = j; break; }
        }
        verify(end > 0, "line %i: unterminated { (define) ?? value }",
            t->line);
        if (eval_define(a, string(name->chars)) && end > i + 5) {
            token val = (token)b->origin[i + 5];
            if (end == i + 6 && instanceof(val->literal, string)) {
                string v = (string)val->literal;
                push(res, (Au)token(chars, v->chars, source, val->source,
                    line, val->line, column, val->column));
            } else
                for (int k = i + 5; k < end; k++)
                    push(res, (Au)b->origin[k]);
        }
        i = end;
    }
    return res;
}

void import_defines(silver a, array input, map output) {
    for (int i = 0; i < len(input); i++) {
        token t = (token)input->origin[i];
        // marker + must start the flag; name must glue to it
        // (rules out the + tokens inside -lstdc++)
        if (eq(t, "+") && !t->neighbor && i + 1 < len(input) &&
            ((token)input->origin[i + 1])->neighbor) {
            string def = (string)input->origin[++i];
            // check for = in next token
            if (i + 1 < len(input) && eq((string)input->origin[i + 1], "=") && i + 2 < len(input)) {
                i += 2; // skip = and value
                set(output, (Au)def, (Au)input->origin[i]);
            } else {
                set(output, (Au)def, (Au)_bool(true));
            }
        }
    }
}

static bool command_exists(cstr cmd) {
    char buf[256];
#ifdef _WIN32
    // system() is cmd.exe here: no `command -v`, and /dev/null is not a path
    snprintf(buf, sizeof(buf), "where %s >nul 2>nul", cmd);
#else
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
#endif
    return system(buf) == 0;
}

static bool target_is_apple(silver a) {
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
        string p = a->platform;
        return strstr(p->chars, "apple")  != NULL ||
               strstr(p->chars, "ios")    != NULL ||
               strstr(p->chars, "darwin") != NULL ||
               strstr(p->chars, "macos")  != NULL ||
               strstr(p->chars, "osx")    != NULL;
    }
    return SILVER_IS_MAC;
}

static bool is_cpp_source_ext(silver a, string ext) {
    return ext && (
        eq(ext, "cc") || eq(ext, "cpp") || eq(ext, "cxx") ||
        (target_is_apple(a) && eq(ext, "mm"))
    );
}

static bool is_native_source_ext(silver a, string ext) {
    return ext && (eq(ext, "c") || eq(ext, "rs") || is_cpp_source_ext(a, ext));
}

static cstr source_lang_flag(silver a, string ext) {
    return ext && target_is_apple(a) && eq(ext, "mm") ? "-x objective-c++" : "";
}

static void collect_mm_frameworks(silver a, array sources) {
    if (!target_is_apple(a) || !sources)
        return;
    if (!a->frameworks)
        a->frameworks = hold(map(16));

    each(sources, path, src) {
        string sx = ext(src);
        if (!sx || !eq(sx, "mm"))
            continue;

        string content = (string)load(src, typeid(string), null);
        if (!content || !content->chars)
            continue;

        cstr line = content->chars;
        while (line && *line) {
            cstr line_end = strchr(line, '\n');
            if (!line_end)
                line_end = line + strlen(line);

            cstr scan = line;
            while (scan < line_end && isspace((unsigned char)*scan))
                scan++;

            if (!strncmp(scan, "#import <", 9) || !strncmp(scan, "#include <", 10)) {
                cstr open  = strchr(scan, '<');
                cstr slash = open ? strchr(open + 1, '/') : null;
                if (open && slash && slash < line_end && isupper((unsigned char)open[1])) {
                    string fw = string(chars, open + 1, ref_length, (sz)(slash - open - 1));
                    if (len(fw))
                        set(a->frameworks, fw, _bool(true));
                }
            }

            line = *line_end ? line_end + 1 : line_end;
        }
    }
}

static string framework_name(string fw) {
    if (fw && ends_with(fw, ".framework"))
        return mid(fw, 0, len(fw) - 10);
    return fw;
}

static string framework_import_name(array mpath, string single) {
    if (single && ends_with(single, ".framework"))
        return framework_name(single);
    if (!mpath || len(mpath) < 2)
        return null;
    string tail = (string)mpath->origin[len(mpath) - 1];
    if (!tail || strcmp(tail->chars, "framework") != 0)
        return null;
    string fw = string(alloc, 32);
    for (int i = 0; i < len(mpath) - 1; i++) {
        if (len(fw))
            append(fw, ".");
        concat(fw, (string)mpath->origin[i]);
    }
    return len(fw) ? fw : null;
}

static bool is_commit_hash(string n) {
    if (!n) return false;
    i32 ln = len(n);
    if (ln != 7 && ln != 40) return false;
    for (int i = 0; i < ln; i++) {
        char l = tolower(n->chars[i]);
        if ((l >= 'a' && l <= 'f') || (l >= '0' && l <= '9'))
            continue;
        return false;
    }
    return true;
}

string command_run(command cmd, bool verbose);

typedef struct checkout_progress_t {
    string label;
    symbol phase;
    string output;
    bool   verbose;
    char   line[4096];
    int    len;
    int    percent;
} checkout_progress_t;

static int checkout_line_percent(cstr s) {
    cstr pct = strrchr(s, '%');
    if (!pct) return -1;
    cstr p = pct;
    while (p > s && isspace((unsigned char)p[-1])) p--;
    cstr end = p;
    while (p > s && isdigit((unsigned char)p[-1])) p--;
    if (p == end) return -1;
    int res = atoi(p);
    if (res < 0) res = 0;
    if (res > 100) res = 100;
    return res;
}

static void checkout_progress_line(checkout_progress_t* p) {
    if (!p->len) return;
    p->line[p->len] = 0;
    int percent = checkout_line_percent(p->line);
    if (percent >= 0 && percent != p->percent) {
        p->percent = percent;
        progress_command((symbol)p->label->chars, p->phase,
            percent, null, false);
    }
}

static bool checkout_output(void* ctx, cstr buf, ssize_t bytes) {
    checkout_progress_t* p = (checkout_progress_t*)ctx;
    if (p->verbose) {
        fwrite(buf, 1, (size_t)bytes, stdout);
        fflush(stdout);
        return true;
    }
    append_count(p->output, buf, (int)bytes);
    for (ssize_t i = 0; i < bytes; i++) {
        char c = buf[i];
        if (c == '\r' || c == '\n') {
            checkout_progress_line(p);
            p->len = 0;
        } else if (p->len < (int)sizeof(p->line) - 1) {
            p->line[p->len++] = c;
        }
    }
    return true;
}

static int checkout_exec(silver a, string label,
                         symbol phase, command cmd) {
    checkout_progress_t p = {
        .label = label,
        .phase = phase,
        .output = string(alloc, 4096),
        .verbose = a->verbose,
        .len = 0,
        .percent = -1
    };
    if (!a->verbose)
        progress_command((symbol)label->chars, phase, -1, null, false);
    int rc = command_exec_hook(cmd, a->verbose, true, checkout_output, &p);
    checkout_progress_line(&p);
    if (!a->verbose)
        progress_command((symbol)label->chars, phase, 0, null, true);
    if (rc) {
        progress_clear_line();
        fputc('\n', stderr);
    }
    if (rc && !a->verbose && len(p.output)) {
        fwrite(p.output->chars, 1, (size_t)len(p.output), stderr);
        if (p.output->chars[len(p.output) - 1] != '\n') fputc('\n', stderr);
        fflush(stderr);
    }
    drop(p.output);
    return rc;
}

static none checkout_verify(silver a, string label,
                            symbol phase, symbol name, command cmd) {
    int rc = checkout_exec(a, label, phase, cmd);
    verify(rc == 0, "shell command failed: %s", name);
}

// >> commands patch the INSTALLED tree, so they belong to every path that
// leaves the import satisfied — including the cached one, which returns
// before the build ever runs
static none run_import_commands(silver a, string label,
                                array cmds, path in_dir) {
    if (!cmds || !len(cmds)) return;
    path cw = path_cwd();
    cd(in_dir);
    each(cmds, string, cmd) {
        string icmd = interpolate(cmd, (Au)a);
        checkout_verify(a, label, "command", "command",
            (command)icmd);
    }
    cd(cw);
}

static none checkout(silver a, path uri, string commit, array prebuild, array postbuild, string conf, string env, string mod_sel, string import_name) {
    // a dependency built for a device belongs to THAT platform: sharing the
    // native prefix would install a windows glfw over the linux one. the
    // SOURCE checkout stays shared — only build and install are per-platform
    path    install     = (a->platform && len(a->platform) && cmp(a->platform, "native") != 0)
                        ? f(path, "%s/platform/%o", SILVER, target_dir(a)) : a->install;
    string  s           = cast(string, uri);
    num     sl          = rindex_of(s, "/");
    validate(sl >= 0, "invalid uri");
    string  name        = mid(s, sl + 1, len(s) - sl - 1);
    // the owner IS the namespace: two owners may name a project the same,
    // so a checkout lives under its owner's directory
    string  head9       = mid(s, 0, sl);
    num     sl2         = rindex_of(head9, "/");
    string  owner       = sl2 >= 0 ? mid(head9, sl2 + 1, len(head9) - sl2 - 1) : null;
    string  label       = import_name ? import_name : name;
    num     version_at  = index_of(label, "/");
    if (version_at >= 0)
        label = mid(label, 0, version_at);
    path    project_f   = (owner && len(owner)) ?
          f(path, "%o/checkout/%o/%o", a->root_path, owner, name)
        : f(path, "%o/checkout/%o",    a->root_path, name);
    make_dir(parent_dir(project_f));
    bool    debug       = false;
    string  config      = interpolate(conf, (Au)a);
    // a dependency may invoke windres or clang itself, with flags of its own
    // that our toolchain file never reaches. CPATH is how the target headers
    // still arrive, since clang reads it for every compile it runs
    string  cenv        = (a->sysroot && platform_is_windows(a))
                        ? f(string, "CPATH=%o/include %o ", a->sysroot, env)
                        : env;

    validate(command_exists("git"), "git required for import feature");

    // serialize this dependency across concurrent silver builds. two processes both
    // entering checkout() for the same dep race on remove_dir(build_f)+cmake — one
    // wipes the build folder the other is mid-build in (missing .o.d, the spurious
    // "vulkan rebuild" failures) — and an interrupted build never writes silver-token,
    // so the next run rebuilds. an exclusive flock over the whole clone→build→install→
    // token means only one process touches a given checkout at a time; the rest block,
    // then read the fresh token and return cached. lock lives beside (not inside) the
    // build dir so remove_dir(build_f) can't delete it.
    make_dir(install);
    path lock_path = (owner && len(owner)) ?
          f(path, "%o/%o-%o.checkout-lock", install, owner, name)
        : f(path, "%o/%o.checkout-lock",    install, name);
    int  lock_fd   = open(lock_path->chars, O_CREAT | O_RDWR, 0644);
    if (lock_fd >= 0) flock(lock_fd, LOCK_EX);

    // checkout or symlink to src
    if (!dir_exists("%o", project_f)) {
        path src_path = f(path, "%o/%o", a->src_loc, name);
        if (dir_exists("%o", src_path)) {
            checkout_verify(a, label, "symlink", "symlink",
                f(command, "ln -s %o %o", src_path, project_f));
            project_f = src_path;
        } else {
            if (!commit) {
                checkout_verify(a, label, "clone", "clone",
                    f(command, "git clone --progress %o %o",
                        uri, project_f));
            } else if (is_commit_hash(commit)) {
                checkout_verify(a, label, "clone", "clone",
                    f(command, "git clone --progress %o %o",
                        uri, project_f));
                checkout_verify(a, label, "checkout", "checkout",
                    f(command, "git -C %o checkout %o", project_f, commit));
            } else {
                checkout_verify(a, label, "clone", "clone",
                    f(command,
                        "git clone --progress --branch %o --single-branch %o %o",
                        commit, uri, project_f));
            }

            // apply module-path diff if one exists (e.g. vulkan/MoltenVK.diff)
            path diff_f = f(path, "%o/%o.diff", a->module_path, name);
            if (file_exists("%o", diff_f))
                checkout_verify(a, label, "patch", "patch",
                    f(command, "git -C %o apply %o", project_f, diff_f));
        }
    }

    // we build to another folder, not inside the source, or checkout.
    // imports build ONCE to install/build — never per-config, never debug
    path build_f    = (owner && len(owner))
                    ? f(path, "%o/build/%o/%o", install, owner, name)
                    : f(path, "%o/build/%o", install, name);
    path rust_f     = f(path, "%o/Cargo.toml", project_f);
    path meson_f    = f(path, "%o/meson.build", project_f);
    // --uninstall: cmake's install_manifest.txt is the ledger of what this
    // dep put into install; walk it, then drop the build and the checkout
    if (a->uninstall) {
        path manifest = f(path, "%o/install_manifest.txt", build_f);
        int  removed  = 0;
        if (file_exists("%o", manifest)) {
            string txt = (string)load(manifest, typeid(string), null);
            char* ln = txt ? txt->chars : null;
            while (ln && *ln) {
                char* eol = strchr(ln, '\n');
                if (eol) *eol = 0;
                if (*ln && unlink(ln) == 0) removed++;
                ln = eol ? eol + 1 : null;
            }
        }
        struct stat lst;
        bool linked = lstat(project_f->chars, &lst) == 0 && S_ISLNK(lst.st_mode);
        exec(a->verbose, "rm -rf %o; rm -f %o", build_f, lock_path);
        if (linked) unlink(project_f->chars);
        else        exec(a->verbose, "rm -rf %o", project_f);
        print("[%o] uninstalled %o (%i installed files)", a->name, label, removed);
        if (lock_fd >= 0) close(lock_fd);
        return;
    }
    // an import may name its own source dir with -S: a project whose cmake
    // port is not at the root (mpg123 keeps one in ports/cmake) says so in
    // its config rather than silver guessing at layouts
    path cmake_src  = project_f;
    cstr s_flag     = strstr(config->chars, "-S ");
    if (s_flag && (s_flag == config->chars || s_flag[-1] == ' ')) {
        cstr v = s_flag + 3;
        while (*v == ' ') v++;
        cstr e = v;
        while (*e && *e != ' ') e++;
        if (e > v && (e - v) < 1024) {
            char buf[1024];
            memcpy(buf, v, (size_t)(e - v));
            buf[e - v] = 0;
            cmake_src = f(path, "%s", buf);
        }
    }
    path cmake_f    = f(path, "%o/CMakeLists.txt", cmake_src);
    string msel     = mod_sel ? mod_sel : name;
    path silver_f   = f(path, "%o/%o/%o.ag", project_f, msel, msel);
    path gn_f       = f(path, "%o/BUILD.gn", project_f);
    bool is_rust    = file_exists("%o", rust_f);
    bool is_meson   = file_exists("%o", meson_f);
    bool is_cmake   = file_exists("%o", cmake_f);
    bool is_gn      = file_exists("%o", gn_f);
    bool is_silver  = file_exists("%o", silver_f);
    validate(!mod_sel || is_silver,
        "module selector %o: no silver module at %o", mod_sel, silver_f);
    // selector is part of the cache identity, not the checkout's
    if (mod_sel)
        config = f(string, "%o mod:%o", config, mod_sel);
    path token = is_silver
        ? f(path, "%o/build/.%o-%o.silver-token", install,
            owner ? owner : string("git"), name)
        : f(path, "%o/silver-token", build_f);
    string product_key = is_silver && mod_sel
        ? ((owner && eq(owner, "ar-visions") && eq(name, "silver"))
            ? f(string, "silver-%o", mod_sel)
            : f(string, "%o-%o", owner, mod_sel))
        : null;
    path product_token = product_key
        ? f(path, "%o/build/%o.product", install, product_key)
        : null;
    path compiler_token = f(path, "%o/build/silver", install);
    bool product_current = !product_token ||
        (file_exists("%o", product_token) &&
         modified_time(product_token) > modified_time(compiler_token));

    if (file_exists("%o", token) &&
        product_current) {
        string s = (string)load(token, typeid(string), null);
        if (s && eq(s, config->chars)) {
            run_import_commands(a, label, postbuild,
                is_silver ? install : build_f);
            if (is_silver && mod_sel) {
                silver root = a;
                while (root->is_external)
                    root = (silver)root->is_external;
                path host = f(path, "%o/build/%o", install, mod_sel);
                if (file_exists("%o", host)) {
                    drop(root->url_product);
                    root->url_product = hold(host);
                }
            }
            if (lock_fd >= 0) { flock(lock_fd, LOCK_UN); (close)(lock_fd); }
            return; // cached / built / error, etc
        }
    }

    // the only reliable way of rebuilding on reconfig is to have a new
    // build-folder; a silver child manages its own build cache
    if (!is_silver) {
        remove_dir(build_f);
        make_dir(build_f);
    }

    // a clone stops at the top repo, so a dep that builds from its submodules
    // (mbedtls -> framework) configures against empty dirs. done here, not at
    // clone, so a checkout left incomplete by an earlier run repairs itself
    if (dir_exists("%o/.git", project_f) || file_exists("%o/.git", project_f))
        checkout_verify(a, label, "submodule", "submodule",
            f(command,
              "git -C %o submodule update --init --recursive --progress",
              project_f));

    // this is the only place we 'cd' anywhere, where there are serial shell commands
    // however we go right back to where we were after
    if (prebuild && len(prebuild)) {
        path cw = path_cwd();
        cd(project_f);
        each(prebuild, string, cmd) {
            string icmd = interpolate(cmd, (Au)a);
            checkout_verify(a, label, "prebuild", "prebuild",
                (command)icmd);
        }
        cd(cw);
    }

    bool child_ok = true;
    if (is_cmake) { // build for cmake
        // externals always build Release — debug is for OUR code, not deps
        cstr build = "Release";
        // a device build names its own sdk; the host's would override the toolchain file
        bool   x_apple = a->sysroot && target_is_apple(a) && a->platform && cmp(a->platform, "native") != 0;
        string opt = x_apple ? f(string, "-DCMAKE_OSX_SYSROOT=%o", absolute(a->sysroot)) :
                     a->isysroot ? f(string, "-DCMAKE_OSX_SYSROOT=%o", a->isysroot) : string("");

        // --config below is what picks Release under a multi-config generator
        // a device build hands cmake the target's toolchain; native is ""
        string x_cmake = device_cmake_toolchain(a);
        checkout_verify(a, label, "configure", "configure",
            f(command,
              "%o cmake -B %o -S %o %o%o -DCMAKE_INSTALL_PREFIX=%o -DCMAKE_BUILD_TYPE=%s %o",
              cenv, build_f, cmake_src, x_cmake, opt, install, build, config));

        checkout_verify(a, label, "build", "build",
            f(command, "%o cmake --build %o --config %s -j16",
              cenv, build_f, build));
        checkout_verify(a, label, "install", "install",
            f(command, "%o cmake --install %o --config %s",
              cenv, build_f, build));
    } else if (is_meson) { // build for meson
        // externals always build release — debug is for OUR code
        cstr build = "release";

        string x_meson = device_meson_cross(a);
        checkout_verify(a, label, "setup", "setup",
            f(command, "%o meson setup %o --prefix=%o --buildtype=%s %o%o",
              cenv, build_f, install, build, x_meson, config));

        checkout_verify(a, label, "build", "compile",
            f(command, "%o meson compile -C %o", cenv, build_f));
        checkout_verify(a, label, "install", "install",
            f(command, "%o meson install -C %o", cenv, build_f));
    } else if (is_gn) {
        cstr is_debug = "false";
        checkout_verify(a, label, "configure", "gen",
            f(command, "gn gen %o --args='is_debug=%s is_official_build=true %o'",
              build_f, is_debug, config));
        checkout_verify(a, label, "build", "ninja",
            f(command, "ninja -C %o -j8", build_f));
    } else if (is_rust) {
        string x_rust = (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) ?
            f(string, "--target %s ", platform_triple(a)) : string("");
        checkout_verify(a, label, "build", "rust",
            f(command,
              "cargo build --release %o--manifest-path %o/Cargo.toml --target-dir %o",
              x_rust, project_f, build_f));
        // cargo has no install step — stage artifacts into the prefix
        checkout_exec(a, label, "install",
            f(command, "cp -f %o/release/*.so %o/lib/ 2>/dev/null || true",
              build_f, install));
        checkout_exec(a, label, "install",
            f(command, "cp -f %o/release/*.a %o/lib/ 2>/dev/null || true",
              build_f, install));
        // emit the C header for the crate's extern "C" surface
        string cbg = f(string, "%o/bin/cbindgen", install);
        if (!file_exists("%o", cbg))
            cbg = command_exists("cbindgen") ? string("cbindgen") : null;
        if (cbg) {
            if (checkout_exec(a, label, "header",
                    f(command, "%o %o --output %o/include/%o.h",
                      cbg, project_f, install, name)) != 0)
                print("cbindgen failed for %o — rust import has no header", name);
        } else
            print("cbindgen not found — rust import %o builds without a header", name);
    } else if (is_silver) { // build for Au-type projects
        silver sf = silver(debug_type, a->debug_type, debugmember, a->debugmember, module, silver_f, breakpoint, a->breakpoint, release, a->release,
            clean, a->clean, verbose, a->verbose, format, a->format, jobs, a->jobs, is_external, a->is_external ? a->is_external : a, is_child, a);
        validate(sf, "silver module compilation failed: %o", silver_f);
        // nest the imported module's node under THIS (its direct parent), keyed by the
        // child's source-file path → its node map; a->tree holds the reference so the
        // subtree survives the child's drop and the tree stays intact.
        if (sf->module_file && sf->tree) set(a->tree, (Au)sf->module_file, (Au)sf->tree);
        child_ok = !sf->error;
        // a CLI url run launches the selected module once the build lands
        if (mod_sel && child_ok) {
            silver root = a;
            while (root->is_external) root = (silver)root->is_external;
            drop(root->url_product);
            root->url_product = hold(f(path, "%o/build/%o", install, mod_sel));
        }
        drop(sf);
    } else {
        /// build for automake
        if (file_exists("%o/autogen.sh", project_f) ||
            file_exists("%o/configure.ac", project_f) ||
            file_exists("%o/configure", project_f) ||
            file_exists("%o/config", project_f)) {

            // fix common race condition with autotools
            if (!file_exists("%o/ltmain.sh", project_f))
                checkout_verify(a, label, "configure", "libtoolize",
                    f(command, "libtoolize --install --copy --force"));

            // common preference on these repos
            if (file_exists("%o/autogen.sh", project_f))
                checkout_verify(a, label, "configure", "autogen",
                    f(command, "(cd %o && bash autogen.sh)", project_f));

            // generate configuration scripts if available
            else if (!file_exists("%o/configure", project_f) && file_exists("%o/configure.ac", project_f)) {
                checkout_verify(a, label, "configure", "autoupdate",
                    f(command,
                      "autoupdate --verbose --force --output=%o/configure.ac %o/configure.ac",
                      project_f, project_f));
                checkout_verify(a, label, "configure", "autoreconf",
                    f(command, "autoreconf -i %o", project_f));
            }

            // prefer pre/generated script configure, fallback to config
            path configure = file_exists("%o/configure", project_f) ? f(path, "./configure") : f(path, "./config");

            // autotools names the target with --host, and takes the
            // compiler out of the environment
            string x_host = string("");
            if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
                symbol tri = platform_triple(a);
                path   tb  = f(path, "%s/platform/native/bin", SILVER);
                x_host = f(string, "--host=%s CC='%o/clang --target=%s' "
                                   "CXX='%o/clang++ --target=%s' AR=%o/llvm-ar "
                                   "RANLIB=%o/llvm-ranlib ", tri, tb, tri, tb, tri, tb, tb);
            }
            if (file_exists("%o/%o", project_f, configure)) {
                checkout_verify(a, label, "configure", "configure",
                    f(command, "%o (cd %o && %o%s --prefix=%o %o%o)",
                      cenv, project_f, configure,
                      debug ? " --enable-debug" : "",
                      install, x_host, config));
            }
        }

        path Makefile = f(path, "%o/Makefile", project_f);
        if (file_exists("%o", Makefile))
            checkout_verify(a, label, "build", "make",
                f(command, "%o (cd %o && make PREFIX=%o -f %o install)",
                  cenv, project_f, install, Makefile));
    }

    run_import_commands(a, label, postbuild,
        is_silver ? install : build_f);

    // a failed silver child must not cache as built
    if (!is_silver || child_ok)
        save(token, (Au)config, null);
    if (lock_fd >= 0) { flock(lock_fd, LOCK_UN); (close)(lock_fd); }
}

static bool is_core_module(symbol name) {
    return file_exists("%s/src/%s.c", SILVER, name);
}

// the project's own suppressions, the same set the native compile line uses
static symbol core_warn =
    "-Wno-write-strings -Wno-incompatible-function-pointer-types "
    "-Wno-compare-distinct-pointer-types -Wno-deprecated-declarations "
    "-Wno-shift-op-parentheses -Wno-covered-switch-default "
    "-Wno-nullability-completeness -Wno-expansion-to-defined";

// one Au submodule, built for the device as its own dll. AU_LINK_<mod> is
// dllimport for every consumer — the owner flips it to export, or its own
// type-info globals never leave the dll. SILVER names the install root ON
// THE DEVICE, not here
static bool build_core_module(silver a, symbol mod, path lib_dir, path objs,
                              string tgt, string ldld, string tools, string deps) {
    path implib = f(path, "%o/lib%s.dll.a", lib_dir, mod);
    if (file_exists("%o", implib)) return true;
    print("[%s] building for the device", mod);
    path   obj = f(path, "%o/%s.obj", objs, mod);
    string inc = f(string,
        "-I %s/install/build/src/Au -I %s/src -I %s/install/build/src -I %s/install/include",
        SILVER, SILVER, SILVER, SILVER);
    string defs = f(string, "-DMODULE='\"%s\"' -DSILVER='\"C:/silver\"' "
                            "-DAU_LINK_%s=__attribute__\\(\\(dllexport\\)\\)", mod, mod);
    if (exec(a->verbose, "%o/clang %o %s -c %s/src/%s.c -o %o %o %o",
             tools, tgt, core_warn, SILVER, mod, obj, inc, defs) != 0) return false;
    if (exec(a->verbose, "%o/clang++ %o %s %o -shared %o %o -o %o/%s.dll "
             "-Wl,--out-implib,%o",
             tools, tgt, platform_abi_cxx(a), ldld, obj, deps, lib_dir, mod, implib) != 0)
        return false;
    return file_exists("%o", implib);
}

// an apple device has no silver on it: Au, its ffi and every core module
// the app reached for are cross-built once into platform/<device>/lib.
// core_lib receives the -L that puts that dir ahead of the native one
static bool ensure_apple_runtime(silver a, symbol triple, string tgt,
                                 string tools, string core_lib) {
    path root    = f(path, "%s/platform/%o", SILVER, target_dir(a));
    path lib_dir = f(path, "%o/lib",   root);
    path objs    = f(path, "%o/build", root);
    make_dir(lib_dir);
    make_dir(objs);
    // the device libs, then the sdk's own, ahead of the host's install/lib
    concat(core_lib, f(string, "-L%o -L%o/usr/lib ", lib_dir, a->sysroot));

    // libffi is autotools: an out-of-tree configure against our clang
    if (!file_exists("%o/libffi.a", lib_dir)) {
        print("[ffi] building for %s", triple);
        path ffi_b = f(path, "%o/libffi", objs);
        make_dir(ffi_b);
        if (exec(a->verbose, "cd %o && %s/checkout/libffi/configure --host=aarch64-apple-darwin "
                 "--prefix=%o --disable-shared --disable-docs --disable-multi-os-directory "
                 "CC='%o/clang %o' CXX='%o/clang++ %o' && make -j8 install",
                 ffi_b, SILVER, root, tools, tgt, tools, tgt) != 0) return false;
    }

    string inc = f(string,
        "-I %s/install/build/src/silver -I %s/src -I %s/install/build/src "
        "-I %s/platform/native/include -I %o/include",
        SILVER, SILVER, SILVER, SILVER, root);
    string base = f(string, "%s -Wno-nullability-completeness -Wno-expansion-to-defined "
        "-fPIC -fvisibility=default -DSILVER='\"%s\"' %s", core_warn, SILVER, a->debug ? "-g" : "-O2");

    if (!file_exists("%o/libAu.dylib", lib_dir)) {
        print("[Au] building the runtime for %s", triple);
        path au_o = f(path, "%o/Au.o",    objs);
        path po_o = f(path, "%o/posix.o", objs);
        if (exec(a->verbose, "%o/clang %o %o -DMODULE='\"Au\"' -I %s/install/build/src/Au %o -c %s/src/Au.c -o %o",
                 tools, tgt, base, SILVER, inc, SILVER, au_o) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o %o -std=c++17 -stdlib=libc++ -DMODULE='\"posix\"' -I %s/install/build/src/posix %o -c %s/src/posix.cc -o %o",
                 tools, tgt, base, SILVER, inc, SILVER, po_o) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o -fuse-ld=lld -B%o -dynamiclib %o %o -o %o/libAu.dylib "
                 "-L%o -lffi -lc++ -install_name @rpath/libAu.dylib",
                 tools, tgt, tools, au_o, po_o, lib_dir, lib_dir) != 0) return false;
    }

    pairs(a->libs, li) {
        string nm = (string)instanceof(li->key, string);
        if (!nm || cmp(nm, "Au") == 0)  continue;
        if (!is_core_module(nm->chars)) continue;
        if (file_exists("%o/lib%o.dylib", lib_dir, nm)) continue;
        print("[%o] building for the device", nm);
        path obj = f(path, "%o/%o.o", objs, nm);
        if (exec(a->verbose, "%o/clang %o %o -DMODULE='\"%o\"' -I %s/install/build/src/%o %o -c %s/src/%o.c -o %o",
                 tools, tgt, base, nm, SILVER, nm, inc, SILVER, nm, obj) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o -fuse-ld=lld -B%o -dynamiclib %o -o %o/lib%o.dylib "
                 "-L%o -lAu -lc++ -install_name @rpath/lib%o.dylib",
                 tools, tgt, tools, obj, lib_dir, nm, lib_dir, nm) != 0) return false;
    }
    return true;
}

// a phone has no silver on it either: the same set, as bionic .so files.
// libc++ is the ndk's shared one, which the package carries beside them
static bool ensure_android_runtime(silver a, symbol triple, string tgt, string ldld,
                                   string tools, string core_lib) {
    path root    = f(path, "%s/platform/%o", SILVER, target_dir(a));
    path lib_dir = f(path, "%o/lib",   root);
    path objs    = f(path, "%o/build", root);
    make_dir(lib_dir);
    make_dir(objs);
    // bionic keeps the shared libc/libm and crt under usr/lib/<triple>/<api>
    // and libc++_shared under usr/lib/<triple>; the api dir must come FIRST,
    // or -lc resolves to the static libc.a beside libc++_shared, whose
    // internal hidden symbols do not link standalone
    cstr abi = strstr(triple, "x86_64") ? "x86_64-linux-android" : "aarch64-linux-android";
    concat(core_lib, f(string, "-L%o -L%o/usr/lib/%s/33 -L%o/usr/lib/%s ",
        lib_dir, a->sysroot, abi, a->sysroot, abi));
    // the clang driver puts the <triple> dir (static libc.a) ahead of the
    // <triple>/<api> dir (shared libc.so); naming the api dir first here
    // pulls the shared libc, so the runtime .so does not statically absorb
    // bionic's malloc/gwp_asan (whose IE-model TLS a dlopen then rejects)
    string sys_l = f(string, "-L%o/usr/lib/%s/33 -L%o/usr/lib/%s ",
        a->sysroot, abi, a->sysroot, abi);

    if (!file_exists("%o/libffi.a", lib_dir)) {
        print("[ffi] building for %s", triple);
        path ffi_b = f(path, "%o/libffi", objs);
        make_dir(ffi_b);
        if (exec(a->verbose, "cd %o && %s/checkout/libffi/configure --host=%s-linux-android "
                 "--prefix=%o --disable-shared --disable-docs --disable-multi-os-directory "
                 "CC='%o/clang %o -fPIC' CXX='%o/clang++ %o -fPIC' LD='%o/ld.lld' AR='%o/llvm-ar' "
                 "RANLIB='%o/llvm-ranlib' && make -j8 install",
                 ffi_b, SILVER, strstr(triple, "x86_64") ? "x86_64" : "aarch64",
                 root, tools, tgt, tools, tgt, tools, tools, tools) != 0) return false;
    }

    string inc = f(string,
        "-I %s/install/build/src/silver -I %s/src -I %s/install/build/src "
        "-I %s/platform/native/include -I %o/include",
        SILVER, SILVER, SILVER, SILVER, root);
    string base = f(string, "%s -Wno-nullability-completeness -Wno-expansion-to-defined "
        "-fPIC -fvisibility=default -DSILVER='\"%s\"' %s", core_warn, SILVER, a->debug ? "-g" : "-O2");

    if (!file_exists("%o/libAu.so", lib_dir)) {
        print("[Au] building the runtime for %s", triple);
        path au_o = f(path, "%o/Au.o",    objs);
        path po_o = f(path, "%o/posix.o", objs);
        if (exec(a->verbose, "%o/clang %o %o -DMODULE='\"Au\"' -I %s/install/build/src/Au %o -c %s/src/Au.c -o %o",
                 tools, tgt, base, SILVER, inc, SILVER, au_o) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o %o -std=c++17 -DMODULE='\"posix\"' -I %s/install/build/src/posix %o -c %s/src/posix.cc -o %o",
                 tools, tgt, base, SILVER, inc, SILVER, po_o) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o %o -shared -Wl,-soname,libAu.so %o %o -o %o/libAu.so "
                 "-L%o %o -lffi -llog",
                 tools, tgt, ldld, au_o, po_o, lib_dir, lib_dir, sys_l) != 0) return false;
    }

    pairs(a->libs, li) {
        string nm = (string)instanceof(li->key, string);
        if (!nm || cmp(nm, "Au") == 0)  continue;
        if (!is_core_module(nm->chars)) continue;
        if (file_exists("%o/lib%o.so", lib_dir, nm)) continue;
        print("[%o] building for the device", nm);
        path obj = f(path, "%o/%o.o", objs, nm);
        if (exec(a->verbose, "%o/clang %o %o -DMODULE='\"%o\"' -I %s/install/build/src/%o %o -c %s/src/%o.c -o %o",
                 tools, tgt, base, nm, SILVER, nm, inc, SILVER, nm, obj) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o %o -shared -Wl,-soname,lib%o.so %o -o %o/lib%o.so -L%o %o -lAu",
                 tools, tgt, ldld, nm, obj, lib_dir, nm, lib_dir, sys_l) != 0) return false;
    }
    return true;
}

// a windows DLL must resolve every symbol at link time, so every Au submodule
// the app imported needs its own build for the device. built once, beside its
// sysroot; returns the import libs to hand the module link
static bool ensure_core_runtime(silver a, path install, symbol triple,
                                string tgt, string ldld, string tools, string core_lib) {
    path lib_dir = f(path, "%s/platform/%o/lib", SILVER, target_dir(a));
    path objs    = f(path, "%s/platform/%o/build", SILVER, target_dir(a));
    make_dir(lib_dir);
    make_dir(objs);

    // Au is the base every other submodule links against, and the only one
    // carrying the posix layer and the generic atomics
    path au_lib = f(path, "%o/libAu.dll.a", lib_dir);
    if (!file_exists("%o", au_lib)) {
        print("[Au] building the runtime for %s", triple);
        path au_o = f(path, "%o/Au.obj",     objs);
        path po_o = f(path, "%o/posix.obj",  objs);
        path at_o = f(path, "%o/atomic.obj", objs);
        string inc = f(string,
            "-I %s/install/build/src/Au -I %s/src -I %s/install/build/src -I %s/install/include",
            SILVER, SILVER, SILVER, SILVER);
        string defs = f(string, "-DMODULE='\"Au\"' -DSILVER='\"C:/silver\"' "
                                "-DAU_LINK_Au=__attribute__\\(\\(dllexport\\)\\)");
        if (exec(a->verbose, "%o/clang %o %s -c %s/src/Au.c -o %o %o %o",
                 tools, tgt, core_warn, SILVER, au_o, inc, defs) != 0) return false;
        if (exec(a->verbose, "%o/clang++ %o %s %s -c %s/src/posix.cc -o %o %o %o",
                 tools, tgt, platform_abi_cxx(a), core_warn, SILVER, po_o, inc, defs) != 0) return false;
        // generic atomics: linux has libatomic, windows has nothing — compiler-rt
        // carries the implementation and we already vendor its source
        if (exec(a->verbose, "%o/clang %o -c %s/checkout/LLVM/llvm-project/compiler-rt/lib/builtins/atomic.c "
                 "-o %o -I %s/checkout/LLVM/llvm-project/compiler-rt/lib/builtins",
                 tools, tgt, SILVER, at_o, SILVER) != 0) return false;
        // psapi carries EnumProcessModules for dlsym; winpthread the posix clock
        if (exec(a->verbose, "%o/clang++ %o %s %o -shared %o %o %o -o %o/Au.dll "
                 "-ldbghelp -lpsapi -lwinpthread -Wl,--out-implib,%o",
                 tools, tgt, platform_abi_cxx(a), ldld, au_o, po_o, at_o, lib_dir, au_lib) != 0)
            return false;
    }
    concat(core_lib, f(string, "%o ", au_lib));

    // then every other submodule this app reached for, each against Au
    pairs(a->libs, li) {
        string nm = (string)instanceof(li->key, string);
        if (!nm || cmp(nm, "Au") == 0)     continue;
        if (!is_core_module(nm->chars))    continue;
        if (!build_core_module(a, nm->chars, lib_dir, objs, tgt, ldld, tools,
                               f(string, "%o", au_lib))) return false;
        concat(core_lib, f(string, "%o/lib%o.dll.a ", lib_dir, nm));
    }
    return true;
}

// the target triple for a platform name. with the device's own sysroot in
// hand, llvm cross-compiles here — nothing is emulated and no image is built
// a sysroot and everything linked against it belong to the DEVICE, not to
// its architecture: two machines can share a triple and share nothing else.
// platform names only the triple, so it is the fallback when none is given
static string target_dir(silver a) {
    return (a->device && len(a->device)) ? a->device : a->platform;
}

// windows is a mingw sysroot, so it is shaped like every other target
static bool platform_is_windows(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    return strstr(p, "windows") != NULL;
}

// bionic, not glibc: neither a linux nor an apple target, though it is ELF
static bool target_is_android(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    return strstr(p, "android") != NULL;
}

// a phone ships the app inside a signed package with its own host
static bool target_is_mobile(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    return strstr(p, "ios") != NULL || strstr(p, "android") != NULL;
}

static symbol platform_triple(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    if (strstr(p, "ios"))                             return strstr(p, "simulator") ?
                                                             "arm64-apple-ios16.0-simulator" : "arm64-apple-ios16.0";
    // the api level is part of the triple: it selects the sysroot's lib dir.
    // the emulator runs this machine's own architecture
    if (strstr(p, "android"))                         return strstr(p, "x86_64") ||
                                                             (strstr(p, "sim") && strcmp(arch, "x86_64") == 0) ?
                                                             "x86_64-linux-android33" : "aarch64-linux-android33";
    if (strstr(p, "windows")) {
        if (strstr(p, "arm64"))  return "aarch64-w64-windows-gnu";
        if (strstr(p, "x86_64")) return "x86_64-w64-windows-gnu";
        return "i686-w64-windows-gnu";
    }
    if (strstr(p, "mips"))                            return "mips64el-linux-gnuabi64";
    if (strstr(p, "arm64") || strstr(p, "aarch64") ||
        strstr(p, "jetson"))                          return "aarch64-linux-gnu";
    if (strstr(p, "arm32") || strstr(p, "armv7"))     return "arm-linux-gnueabihf";
    if (strstr(p, "riscv"))                           return "riscv64-linux-gnu";
    if (strstr(p, "x86_64"))                          return "x86_64-linux-gnu";
    if (strstr(p, "x86"))                             return "i686-linux-gnu";
    return "x86_64-linux-gnu";
}

// some targets must have their ABI named: debian riscv64 is rv64gc/lp64d,
// and an object built for the default soft-float abi will not link at all
static symbol platform_abi_clang(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    if (strstr(p, "riscv")) return "-march=rv64gc -mabi=lp64d ";
    // the ndk keys its arch headers by the triple without its api level,
    // a dir this clang does not add on its own. every silver library on a
    // phone is dlopen'd, and android's loader rejects initial-exec TLS in a
    // dlopened object — so all its thread-locals must use the dynamic model
    if (strstr(p, "android")) {
        static char inc[1024];
        snprintf(inc, sizeof(inc), "-isystem %s/usr/include/%s-linux-android -ftls-model=global-dynamic ",
                 a->sysroot->chars, strstr(platform_triple(a), "x86_64") ? "x86_64" : "aarch64");
        return inc;
    }
    return "";
}

// mingw carries libc++; a posix sysroot brings its own libstdc++
static symbol platform_abi_cxx(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    if (strstr(p, "windows")) return "-stdlib=libc++ ";
    return "";
}

// debian mips64el ships crt*.o with no GNU-stack note, so lld refuses the
// link unless an executable stack is explicitly permitted
static symbol platform_abi_link(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    if (strstr(p, "mips")) return "-Wl,-z,execstack ";
    // mingw's clang driver still reaches for gcc's runtime by default
    if (strstr(p, "windows")) return "-rtlib=compiler-rt -unwindlib=libunwind ";
    // android 15 loads 16k-page devices: every segment aligns to that
    if (strstr(p, "android")) return "-Wl,-z,max-page-size=16384 ";
    return "";
}

static symbol platform_abi_llc(silver a) {
    cstr p = a->platform ? a->platform->chars : "";
    if (strstr(p, "riscv")) return "-mattr=+m,+a,+f,+d,+c --target-abi=lp64d ";
    return "";
}

string compile_implements(silver a, array files, string cflags) {
    path   install = a->install;
    string objs    = string();
#ifdef __APPLE__
    // _DARWIN_C_SOURCE: an import's _XOPEN_SOURCE would hide u_char
    cstr   sysroot_flag = "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -D_DARWIN_C_SOURCE";
#else
    cstr   sysroot_flag = "";
#endif
    each(files, path, i) {
        string ext      = ext(i);
        if (eq(ext, "rs")) {
            // self-contained staticlib: no rust std linkage on our side
            string a_name = f(string, "%o/lib%o_rs.a", a->build_dir, stem(i));
            verify(exec(a->verbose, "rustc --crate-type=staticlib %s -O %o -o %o",
                a->debug ? "-g" : "", i, a_name) == 0,
                "failed to compile %o (rustc required)", i);
            if (len(objs)) append(objs, " ");
            concat(objs, a_name);
            continue;
        }
        string i_name   = f(string, "%o/%o.o", a->build_dir, filename(i));
        bool   is_cpp   = is_cpp_source_ext(a, ext);
        cstr   compiler = is_cpp ? "clang++" : "clang";
        cstr   std_flag = is_cpp ? "-std=c++17" : "-std=c11";
        cstr   lang_flag = source_lang_flag(a, ext);
        string st       = stem(i);
        // a module's own implementation source OWNS its symbols. without this
        // the header's default makes them dllimport and the module imports
        // what it defines itself -- lld warns LNK4217 on every one
#ifdef _WIN32
        string own_link = f(string, "-DAU_LINK_%o=__attribute__((dllexport))", st);
#else
        string own_link = string("");
#endif
        au_exec_prefix(cstring(a->name)); // this compile belongs to US
        path tool_root = a->base_install ? a->base_install : install;
        string base_inc = a->base_install ?
            f(string, " -I%o/include -I%o/include/Au", a->base_install, a->base_install) : string("");
        string cmd = f(string, "%o/bin/%s %s %s %s %o %o %s -w -c %o -o %o -I%o/include/%o -I%o/include -I%o/include/Au%o",
            tool_root, compiler, std_flag, lang_flag, sysroot_flag, own_link, cflags, a->debug ? "-g" : "", i, i_name, install, st, install, install, base_inc);
        if (a->verbose) print("[%o] %o", a->name, cmd);
        verify(exec(a->verbose, "%o", cmd) == 0, "failed to compile %o", i);
        au_exec_prefix(null);
        if (len(objs)) append(objs, " ");
        concat(objs, i_name);
    }
    return objs;
}

static void deploy_resources(path src, path dst);

static void symlink_resources(path src, path dst) {
    DIR *dir = opendir(src->chars);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        path s = form(path, "%o/%s", src, entry->d_name);
        path d = form(path, "%o/%s", dst, entry->d_name);
        path abs_s = absolute(s);
        if (entry->d_type == DT_DIR) {
            make_dir(d);
            symlink_resources(s, d);
        } else {
            struct stat st;
            if (lstat(d->chars, &st) == 0)
                unlink(d->chars);
            symlink(abs_s->chars, d->chars);
        }
    }
    closedir(dir);
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
// walk a module's dependency tree (key = source path, value = that source's node map)
// and collect every transitive source path into `out`, de-duped via `seen`. the tree is
// the authoritative structure; flattening it here yields a COMPLETE source list (unlike
// the old flat per-module shortcut, which missed transitive deps).
static void silver_collect_tree(map tree, array out, map seen) {
    if (!tree || !len(tree)) return;
    pairs(tree, i) {
        path p = (path)i->key;
        if (p && !get(seen, (Au)p)) {
            set(seen, (Au)p, (Au)_bool(true));
            push(out, (Au)p);
        }
        if (i->value && instance_of((Au)i->value, typeid(map)))
            silver_collect_tree((map)i->value, out, seen);
    }
}

none silver_build_product(silver a) {
    path ll = null, bc = null;
    bool emit_ok = emit(a, (ARef)&ll, (ARef)&bc);
    verify(emit_ok, "compilation failed");
    verify(bc != null, "compilation failed");

    //bool   is_debug   = a->debug;
    int    error_code = 0;
    path   install    = a->install;
    //string name       = stem(bc);
    path   cwd        = path_cwd();
    string libs       = string("");
    array  lib_paths  = array();

    // `import M with ext…` folds the extension names into M's product id so an
    // extended build is both cache-distinct and legible (libtrinity-ext1.ext2-<hash>).
    string ext_tag = string("");
    if (a->extensions && len(a->extensions)) {
        each (a->extensions, path, ep) {
            if (len(ext_tag)) append(ext_tag, ".");
            concat(ext_tag, stem(ep));
        }
    }
    // a product is named for the machine it will RUN on, not for this one
    symbol t_pre = lib_pre, t_lib = lib_ext, t_app = app_ext;
    if (a->platform && len(a->platform)) {
        cstr pl = a->platform->chars;
        if (strstr(pl, "windows"))    { t_pre = "";    t_lib = ".dll";   t_app = ".exe"; }
        else if (strstr(pl, "macos") || strstr(pl, "ios"))
                                      { t_pre = "lib"; t_lib = ".dylib"; t_app = ""; }
        else if (cmp(a->platform, "native") != 0)
                                      { t_pre = "lib"; t_lib = ".so";    t_app = ""; }
    }
    string product_name = a->is_library
        ? silver_install_name(a) : a->name;
    path product    = f(path, "%o/%s%o%s%o%s%o%s",
        a->build_dir, a->is_library ? t_pre : "", product_name,
        len(ext_tag) ? "-" : "", ext_tag,
        len(a->defs_hash) ? "-" : "",
        a->defs_hash,
        a->is_library ? t_lib : t_app);
    
    if (a->product) drop(a->product);

    a->product = hold(product);
    // a live app is a library with a host: it ships
    verify(!(a->release && a->is_library && !((aether)a)->is_live && !a->is_external),
        "--release is for apps: %o is a library", a->name);
    collect_mm_frameworks(a, a->implements);

    string cflags  = a->asan ? string("-fsanitize=address -shared-libasan") : string("");
    string ccflags = string(cflags->chars);
    if (a->include_paths) {
        each(a->include_paths, string, inc) {
            if (len(ccflags)) append(ccflags, " ");
            if (!starts_with(inc, "-I"))
                append(ccflags, "-I");
            concat(ccflags, inc);
        }
    }
    // consumer defines apply to native compiles, same as header parse
    each(a->imports, import, im) {
        for (item it = im->define_map ? im->define_map->first : null; it; it = it->next) {
            if (len(ccflags)) append(ccflags, " ");
            if (isa(it->value) == typeid(bool))
                concat(ccflags, f(string, "-D%o", it->key));
            else
                concat(ccflags, f(string, "-D%o=%o", it->key, it->value));
        }
    }

    // add imported Silver module source directories for C header resolution
    each(a->imports, import, im) {
        if (im->is_au_rt && im->module_source) {
            path dir = parent_dir(im->module_source);
            if (dir) {
                if (len(ccflags)) append(ccflags, " ");
                append(ccflags, "-I");
                string sdir = cast(string, dir);
                concat(ccflags, sdir);
            }
        }
    }

    if (len(a->implements))
        write_header(a);

    // compile implementation in c/cc, and select for linking
    // create libs, and describe in reverse order from import
    pairs(a->libs, i) {
        string name = (string)i->key;
        push(lib_paths, (Au)name);
    }

    array rlibs = reverse(lib_paths);
    each(rlibs, string, lib_name) {
        if (file_exists("%o", lib_name)) {
            string use = lib_name;
#ifdef _WIN32
            // a .product points at the module's dll, and a dll cannot be
            // linked against directly here -- its import lib is what we need
            path   lp   = path(lib_name->chars);
            string pext = ext(lp);
            if (pext && (cmp(pext, "product") == 0 || cmp(pext, "dll") == 0)) {
                path il = f(path, "%o/%o%s", parent_dir(lp), stem(lp), lib_static);
                verify(file_exists("%o", il),
                    "no import library for %o (expected %o)", lib_name, il);
                use = cast(string, il);
            }
#endif
            if (len(libs)) append(libs, " ");
            concat(libs, use);
            continue;
        }
        string rl = resolve_versioned_lib(a, lib_name);
        if (!rl) continue;              // no counterpart on this platform
        if (len(libs)) append(libs, " ");
        concat(libs, f(string, "-l%o", rl));
    }
    // the link is far below; nothing in between may free this
    hold(libs);

    // platform dispatch: a device serves its own sysroot, and llvm is already
    // a cross compiler, so the whole lowering runs HERE
    if (a->platform && len(a->platform) && cmp(a->platform, "native") != 0) {
        verify(a->sysroot && is_dir(a->sysroot),
            "platform '%o' has no sysroot at %o — name a device with -d, or give "
            "it a fetch: line", a->platform, a->sysroot);
        symbol triple = platform_triple(a);
        string tools = f(string, "%s/platform/native/bin", SILVER);
        // -target/--sysroot for clang, -mtriple for llc
        // debian keeps arch headers at /usr/include/<triple>; clang does not
        // add that itself for a cross sysroot, and <linux/types.h> needs it.
        // mingw carries one flat include tree, so it names no such path
        bool   win      = platform_is_windows(a);
        bool   apple    = target_is_apple(a);
        bool   android  = target_is_android(a);
        string arch_inc = win || apple || android ? string("") :
            f(string, "-isystem %o/usr/include/%s ", a->sysroot, triple);
        // an apple sysroot is an sdk, which clang takes as -isysroot
        string tgt   =
            f(string, "-target %s %s%o %o%s",
              triple, apple ? "-isysroot " : "--sysroot=", a->sysroot, arch_inc, platform_abi_clang(a));
        string mtri  = f(string, "-mtriple=%s %s", triple, platform_abi_llc(a));
        // our lld cross-links; the system ld only knows the host arch
        string ldld  =
            f(string, "-fuse-ld=lld -B%o %s", tools, platform_abi_link(a));
        print("[%o] cross-compiling %s against %o", a->name, triple, a->sysroot);

        // llc: .ll -> .o
        // aim codegen at the device HERE: this is the instance that emits,
        // and its modules and machines outlive whatever init did
        set_target((aether)a, triple);
        // every core emits its own object; an external llc sees only core 0
        path   x_obj = f(path, "%o/%o.o", a->build_dir, a->name);
        verify(emit_object(a, x_obj), ".o emission failed (platform: %o)", a->platform);
        string x_core = core_objects(a, x_obj);


        if (len(a->implements))
            write_header(a);

        // compile the module's own .c/.cc/.mm implementations
        string objs = string();
        each(a->implements, path, i) {
            string ext      = ext(i);
            if (eq(ext, "rs")) {
                string a_name = f(string, "%o/lib%o_rs.a", a->build_dir, stem(i));
                verify(exec(a->verbose, "rustc --crate-type=staticlib %s -O %o -o %o",
                    a->debug ? "-g" : "", i, a_name) == 0,
                    "failed to compile %o (rustc required; platform: %o)", i, a->platform);
                if (len(objs)) append(objs, " ");
                concat(objs, a_name);
                continue;
            }
            string i_name   = f(string, "%o/%o.o", a->build_dir, filename(i));
            bool   is_cpp   = is_cpp_source_ext(a, ext);
            cstr   compiler = is_cpp ? "clang++" : "clang";
            cstr   std_flag = is_cpp ? "-std=c++17" : "-std=c11";
            // the device sdk's libc++ headers match the libc++ it ships;
            // clang's newer ones name symbols the device lacks
            string cxx_abi  = !is_cpp ? string("") : (apple || android) ?
                f(string, "-nostdinc++ -isystem %o/usr/include/c++/v1 ", a->sysroot) :
                string(platform_abi_cxx(a));
            cstr   lang_flag = source_lang_flag(a, ext);
            string st       = stem(i);
            verify(exec(a->verbose, "%o/%s %o%s %o%s %o %s -c %o -o %o -I%o/include/%o -I%o/include -I%o/include/Au -I%s/platform/%o/include",
                tools, compiler, tgt, std_flag, cxx_abi, lang_flag, ccflags, a->debug ? "-g" : "", i, i_name, install, st, install, install, SILVER, target_dir(a)) == 0,
                "failed to compile %o (platform: %o)", i, a->platform);
            if (len(objs)) append(objs, " ");
            concat(objs, i_name);
        }

        // windows resolves every symbol at link time, so the runtime must
        // exist for this device before the module can link against it
        // import libraries are named outright; no -L search has to find them
        string core_lib = string(alloc, 512);
        if (win && !ensure_core_runtime(a, install, triple, tgt, ldld, tools, core_lib)) {
            print("[%o] could not build the Au runtime for %s", a->name, triple);
            a->error = true;
            return;
        }
        // no device carries silver's runtime: build it beside the sysroot
        if (apple && !ensure_apple_runtime(a, triple, tgt, tools, core_lib)) {
            print("[%o] could not build the Au runtime for %s", a->name, triple);
            a->error = true;
            return;
        }
        if (android && !ensure_android_runtime(a, triple, tgt, ldld, tools, core_lib)) {
            print("[%o] could not build the Au runtime for %s", a->name, triple);
            a->error = true;
            return;
        }

        // use clang++ when C++ objects are present
        bool has_cpp_d = false;
        each(a->implements, path, impl) {
            string ext_d = ext(impl);
            if (is_cpp_source_ext(a, ext_d)) { has_cpp_d = true; break; }
        }
        cstr linker_d  = has_cpp_d ? "clang++" : "clang";
        string cpp_libs_d = string(has_cpp_d ? "-stdlib=libc++" : "");
        // the device sdk is already named in tgt; the host's must not follow it
        string isysroot = (a->isysroot && !apple) ? f(string, "-isysroot %o ", a->isysroot) : string("");
        string fw_flags_d = string("");
        if (target_is_apple(a) && a->frameworks) {
            pairs(a->frameworks, fw_i) {
                if (len(fw_flags_d)) append(fw_flags_d, " ");
                concat(fw_flags_d, f(string, "-framework %o", (string)fw_i->key));
            }
        }
        // both are ELF-only: a PE resolves dlls by exe dir and PATH, and it
        // binds its own symbols first without being told to
        // $ORIGIN/../lib: a packaged /opt/<app>/bin finds its bundled lib/
        // android resolves sonames from the package's own lib dir: no rpath
        string rpaths = win || android ? string("") : apple ?
            f(string, "-Wl,-rpath,@loader_path -Wl,-rpath,@executable_path/Frameworks -Wl,-rpath,%s/platform/%o/lib ", SILVER, target_dir(a)) :
            f(string, "-Wl,-rpath,%o -Wl,-rpath,%o/lib -Wl,-rpath,'$ORIGIN' -Wl,-rpath,'$ORIGIN/../lib' ", a->build_dir, install);
        // a library carries its leaf as soname: consumers record that, not
        // the absolute build path, and resolve by rpath
        string shared_d = a->is_library ? (target_is_apple(a) || win ? string(shared) :
            f(string, "%s -Wl,-soname,%o", shared, path_filename(a->product))) : string("");
        if (!win && !android && a->base_install)
            concat(rpaths, f(string, "-L%o/lib -Wl,-rpath,%o/lib ",
                a->base_install, a->base_install));
        cstr   bsym   = (a->is_library && !win && !apple) ? "-Wl,-Bsymbolic" : "";
        // the host's install/lib is the wrong platform for a phone
        path   link_inst = apple || android ? (path)f(path, "%o/usr", a->sysroot) : install;
        string x_link = f(string, "%o/%s %o%o%s %s %s %o %o/%o.o%o %o %o-o %o -L%o -L%o/lib %o%o %o %o %o",
            tools, linker_d, tgt, ldld,
            shared_d->chars, a->debug ? "-g" : "",
            bsym,
            isysroot, a->build_dir, a->name, x_core, objs, core_lib,
            a->product,
            a->build_dir,
            link_inst, rpaths, libs, cflags, fw_flags_d, cpp_libs_d);
        if (a->verbose) print("[%o] %o", a->name, x_link);
        verify(exec(a->verbose, "%o", x_link) == 0,
            "link failed (platform: %o)", a->platform);

        // --release --test artifacts are never recorded as the product:
        // the next plain --release must not inherit a test-carrying binary
        if (!(a->release && a->test)) {
            unlink(a->product_link->chars);
            verify(create_symlink(a->product, a->product_link),
                "could not create product symlink from %o -> %o", a->product_link, a->product);
        }
        // the root phone app ships as a bundle; libraries stay bare
        if (target_is_mobile(a) && !a->is_external && ((aether)a)->is_live)
            silver_mobile_bundle(a);

    } else {

    // worker cores emit their own objects alongside core 0's
    path   obj_path  = f(path, "%o/%o.o", a->build_dir, a->name);
    verify(emit_object(a, obj_path), ".o emission failed");
    string core_objs = core_objects(a, obj_path);


    // build compile-only flags
    string objs = compile_implements(a, a->implements, ccflags);

    // link - include the implementation objects; use clang++ when C++ objects are present
    bool has_cpp = false;
    each(a->implements, path, impl) {
        string ext = ext(impl);
        if (is_cpp_source_ext(a, ext)) { has_cpp = true; break; }
    }
    if (a->import_objects && len(a->import_objects)) {
        has_cpp = true;
        each(a->import_objects, path, io) {
            if (len(objs)) append(objs, " ");
            concat(objs, f(string, "%o", io));
        }
    }
    cstr linker   = has_cpp ? "clang++" : "clang";
#ifdef __APPLE__
    cstr cpp_pre   = has_cpp ? "-nostdlib++ -L/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/lib -lc++ -lc++abi" : "";
#else
    cstr cpp_pre   = "";
#ifdef _WIN32
    // mingw carries libc++, and clang will not reach for it unnamed
    cstr cpp_post  = has_cpp ? "-stdlib=libc++" : "";
#else
    cstr cpp_post  = has_cpp ? "-lstdc++" : "";
#endif
#endif
    string isysroot = a->isysroot ? f(string, "-isysroot %o ", a->isysroot) : string("");
    string fw_flags = string("");
    if (target_is_apple(a) && a->frameworks) {
        pairs(a->frameworks, fw_i) {
            if (len(fw_flags)) append(fw_flags, " ");
            concat(fw_flags, f(string, "-framework %o", (string)fw_i->key));
        }
    }
    if (a->is_library) unlink(a->product->chars);

    // windows: no rpath — a dll resolves by exe dir + PATH. link.rsp is the
    // system-lib set bootstrap wrote; mingw needs no crt named by hand
#ifdef _WIN32
    string plat_link = f(string,
        "@%o/link.rsp -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind",
        a->build_dir);
#else
    string plat_link = f(string, "-Wl,-rpath,%o -Wl,-rpath,%o/lib -Wl,-rpath,'$ORIGIN' -Wl,-rpath,'$ORIGIN/../lib'", a->build_dir, install);
    if (a->base_install)
        plat_link = f(string, "%o -L%o/lib -Wl,-rpath,%o/lib",
            plat_link, a->base_install, a->base_install);
#endif

    // windows: link to a fresh temp, then atomically replace the product.
    // the linker deletes its output before writing it, and a scanner mid-scan
    // on the live dll makes that delete fail ("unable to remove file"). a temp
    // has nothing to remove, and au_replace_file rides out the scan window.
#ifdef _WIN32
    path link_out = f(path, "%o.new%i", a->product, (i32)getpid());
#else
    path link_out = a->product;
#endif
    // install/lib BEFORE build_dir: linking an exe leaves an import library
    // beside it named for the exe, and silver.exe's shadows the silver
    // MODULE's -lsilver. module libs of our own are passed by full path, so
    // only the core modules resolve through -l, and those live in install/lib
    // a library carries its leaf as soname: consumers record that, not the
    // absolute build path, and resolve by rpath
#ifdef __linux__
    string shared_n = a->is_library ?
        f(string, "%s -Wl,-soname,%o", shared, path_filename(a->product)) : string("");
#else
    string shared_n = string(a->is_library ? shared : "");
#endif
    verify(exec(a->verbose, "%o/bin/%s %s %s %s %o %s %o/%o.o%o %o -o %o -L%o/lib -L%o %o %o %o %o %s",
        a->base_install ? a->base_install : install, linker, shared_n->chars, a->debug ? "-g" : "",

#ifdef __linux__
        a->is_library ? "-Wl,-Bsymbolic" : "",
#else
        "",
#endif
        isysroot, cpp_pre, a->build_dir, a->name, core_objs, objs,
        link_out,
        install,
        a->build_dir, plat_link, libs, cflags, fw_flags,
#ifdef __APPLE__
        ""
#else
        cpp_post
#endif
        ) == 0,
        "link failed");
#ifdef _WIN32
    verify(au_replace_file(link_out->chars, a->product->chars) == 0,
        "could not replace %o: locked by another process", a->product);
#endif
    
    unlink(a->product_link->chars);

    verify(create_symlink(a->product, a->product_link),
        "could not create product symlink from %o -> %o", a->product_link, a->product);

    }

    // the file is on disk now — waiters may take it
    publish_product(a);

    // for live_app modules: compile the host launcher as the app binary (never cached)
    // a phone app got its host inside its bundle
    if (((aether)a)->is_live && !target_is_mobile(a)) {
        path host_dst = build_silver_host(a);
        a->live_binary = hold(host_dst);
        // symlink install/bin/<name> -> the built host binary, so the app runs
        // by name and dbg/lldb find it (the binary itself stays in build/)
        path bin_link = f(path, "%o/bin/%o", a->install, a->name);
        unlink(bin_link->chars);
        create_symlink(host_dst, bin_link);
    }

    // --link: symlink this app's binary into the first writable PATH dir, so it
    // runs by name with no env-var/profile edits (the binary self-locates its
    // libs/tree from its own path).
    if (a->link) {
        path bin = a->live_binary ? a->live_binary : a->product;
        cstr pathenv = bin ? getenv("PATH") : null;
        char dir[1024]; dir[0] = 0;
        if (pathenv) {
            char buf[8192];
            strncpy(buf, pathenv, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
            for (char* d = strtok(buf, ":"); d; d = strtok(null, ":"))
                if (d[0] && (access)(d, W_OK) == 0) { strncpy(dir, d, sizeof(dir) - 1); break; }
        }
        if (dir[0]) {
            path lnk = f(path, "%s/%o", dir, a->name);
            unlink(lnk->chars);
            if (symlink(bin->chars, lnk->chars) == 0)
                print("linked %o -> %o", lnk, bin);
            else
                print("--link: could not symlink into %s", dir);
        } else {
            print("--link: no writable directory on PATH");
        }
    }

    // deploy resource files into share/{app-name}/ — ALWAYS symlink, both
    // configs. copying froze a snapshot that could desync from source (a
    // .gltf and its .bin drifting apart); one source of truth removes that
    // libraries deploy too: exported scenes/plugins carry their own assets
    deploy_module_resources(a);

    // record this module's OWN source files into its tree node (key = full source path,
    // value = an empty node map; imports already nested their own subtrees during parse).
    // .so/.a artifacts are NOT sources — they go to the flat .artifacts cache list.
    FILE *ar = fopen(a->artifacts_path->chars, "w");
    if (a->tree && a->module_file && !get(a->tree, (Au)a->module_file))
        set(a->tree, (Au)a->module_file, (Au)map(hsize, 8));
    each(a->artifacts, path, ark) {
        const char *dot = strrchr(ark->chars, '.');
        if (dot && (strcmp(dot, ".ag") == 0 || strcmp(dot, ".c") == 0 || strcmp(dot, ".cc") == 0)) {
            if (a->tree && !get(a->tree, (Au)ark)) set(a->tree, (Au)ark, (Au)map(hsize, 8));
        } else {
            if (ar) fprintf(ar, "%s\n", ark->chars);
        }
    }
    if (ar) fclose(ar);

    // .source = the FULL transitive source set, gathered by flattening the dependency
    // tree. the next build's cache check reads this: if ANY source in the tree is newer
    // than the product, the module re-parses (which re-processes its imports, letting
    // each changed inner module rebuild) — so an inner change no longer needs --clean.
    FILE *sr = fopen(a->source_path->chars, "w");
    if (sr) {
        array all  = array(64);
        map   seen = map(hsize, 64);
        silver_collect_tree(a->tree, all, seen);
        each(all, path, p) fprintf(sr, "%s\n", p->chars);
        // the module's own .c/.cc/.mm are sources too: an edit there rebuilds
        each(a->implements, path, i) fprintf(sr, "%s\n", i->chars);
        fclose(sr);
    }
}

bool silver_next_is_neighbor(silver a) {
    token b = element(a, -1);
    token c = element(a, 0);
    return c && (b->column + b->count == c->column);
}

string expect_alpha(silver a) {
    token t = next(a, Syntax__none);
    verify(t && isalpha(*t->chars), "expected alpha identifier");
    return string(t->chars);
}

// when we load silver files, we should look for and bind corresponding .c files that have implementation
// this is useful for implementing in C or other languages
path module_exists(silver a, array idents, bool binary_finary, bool* is_bin) {
    verify(len(idents), "invalid module 'path");

    path to_path = cast(path, join(idents, "/"));

    // part file: same module directory, e.g. orbiter/console.ag with 'part orbiter' at top
    path pf = absolute(f(path, "%o/%o.ag", a->module, stem(to_path)));
    if (file_exists("%o", pf)) {
        *is_bin = false;
        return pf;
    }

    path sf = absolute(f(path, "%o/../%o/%o.ag", a->module, stem(to_path), stem(to_path)));
    if (file_exists("%o", sf)) {
        *is_bin = false;
        return sf;
    }

    // it could be a sub module
    path c  = f(path, "%o/%o.c",  a->module, stem(to_path));
    path cc = f(path, "%o/%o.cc", a->module, stem(to_path));
    path mm = f(path, "%o/%o.mm", a->module, stem(to_path));
    path files[3] = {c, cc, mm};
    int file_count = target_is_apple(a) ? 3 : 2;
    for (int i = 0; i < file_count; i++) {
        path sfc = absolute(files[i]);
        if (file_exists("%o", sfc)) {
            *is_bin = false;
            return sfc;
        }
    }

    if (binary_finary && len(idents) == 1) {
        path installs[2] = {a->install, a->base_install};
        string local_prefix = is_silver_repo(a)
            ? string("silver") : a->git_owner;
        string names[3] = {
            local_prefix ? f(string, "%o-%o", local_prefix,
                idents->origin[0]) : null,
            f(string, "silver-%o", idents->origin[0]),
            (string)idents->origin[0]
        };
        for (int i = 0; i < 2 && installs[i]; i++) {
            for (int n = 0; n < 3; n++) {
                if (!names[n]) continue;
                path lib = f(path, "%o/lib/%s%o%s", installs[i],
                    lib_pre, names[n], lib_ext);
                path build = f(path, "%o/build/%s%o%s", installs[i],
                    lib_pre, names[n], lib_ext);
                if (file_exists("%o", lib)) {
                    *is_bin = true;
                    return lib;
                }
                if (file_exists("%o", build)) {
                    *is_bin = true;
                    return build;
                }
            }
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
    // ternary condition MUST be parenthesized: `(cond) ? a : b`. silver
    // does not parse a bare-condition `cond ? a : b` correctly — what looks
    // like a ternary without parens is something else syntactically (the
    // `?` ends up bound to a constructor-style cast or similar), and silently
    // produces null/garbage at runtime. Catch it here at parse time.
    {
        token prev = (token)silver_element(a, -2);
        bool  paren = prev && prev->chars && prev->chars[0] == ')' && prev->chars[1] == 0;
        validate(paren,
            "ternary condition must be parenthesized: write `(cond) ? a : b`, not `cond ? a : b`");
    }
    bool is_const = false;
    etype mdl_true = mdl_expect;
    array true_tokens = read_expression(a, &mdl_true, &is_const);
    verify(read_if(a, ":"), "expected : after expression");
    etype mdl_false = mdl_expect;
    array false_tokens = read_expression(a, &mdl_false, &is_const);
    subprocedure build_expr = subproc(a, ternary_expr_builder, null);
    return e_ternary_deferred(a, expr, true_tokens, false_tokens, build_expr);
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
    aether a = au_active(f->mod);
    bool is_lambda_call = inherits(f->autype, typeid(lambda));

    if (is_lambda_call) {
        return user_arg_count(u(efunc, f->autype->src));
    }

    if (f->autype->member_type == AU_MEMBER_FUNC) {
        if (f->autype->is_imethod || f->autype->is_smethod) return f->autype->args.count - 1;
        return f->autype->args.count;
    }
    if (f->autype->member_type == AU_MEMBER_CAST) {
        return 0;
    }
    if (f->autype->member_type == AU_MEMBER_OPERATOR) {
        return f->autype->args.count - 1;
    }
    if (f->autype->member_type == AU_MEMBER_GETTER) {
        return 1;
    }
    if (is_func_ptr((Au)f)) {
        Au_t fn = au_arg_type((Au)f->autype);
        return fn->args.count;
    }
    return 0;
}


array read_arg(array tokens, int start, int *next_read);
array read_arg_br(array tokens, int start, int *next_read, cstr open, cstr close);

none copy_lambda_info(enode mem, enode lambda_fn);

enode eshape_from_indices(aether a, array indices);

enode enode_shape(enode);

// instance an inline lambda: context values re-resolve by capture name
static enode inline_lambda_instance(silver a, efunc fmem) { static int seq = 0; seq++;
    array captures = array(alloc, 8);
    members(fmem->autype, m) {
        if (m->member_type != AU_MEMBER_VAR) continue;
        enode oe = (enode)rlookup((aether)a, string(m->ident));
        validate(oe, "lambda capture '%s' not in scope", m->ident);
        push(captures, (Au)oe);
    }
    return e_create((aether)a, (etype)fmem, (Au)captures, false);
}

// lambda [ args ] body — the body parses once (no_build) to gather its
// captures at resolve time; the fn is memoized by its bracket token so
// type-inference re-parses reuse it instead of defining twice
static enode parse_inline_lambda(silver a) { static int seq = 0; seq++;
    token key = peek(a);
    if (a->inline_lambdas) {
        efunc prior = (efunc)get(a->inline_lambdas, (Au)_i64((i64)(size_t)key));
        if (prior) {
            read_within(a);
            if (read_if(a, "->")) read_etype(a, null);
            read_body(a);
            return inline_lambda_instance(a, prior);
        }
    }
    efunc encl = context_func(a);
    validate(encl, "inline lambda requires an enclosing function");
    validate(!a->gather_fn, "inline lambda inside an inline lambda is not supported");

    a->lambda_ordinal++;
    string lname = f(string, "%s_lam%i", encl->autype->ident, a->lambda_ordinal);

    // the gather runs on a scratch fn: anything resolution registers
    // against it stays orphaned, so the real fn builds clean
    Au_t scratch = def(null, cstring(f(string, "%o_gather", lname)),
        AU_MEMBER_FUNC, AU_TRAIT_LAMBDA);
    scratch->context = encl->autype;
    scratch->module  = a->autype;

    validate(read_if(a, "["), "expected [ args ] after lambda");
    array arg_names = array(alloc, 8);
    array arg_types = array(alloc, 8);
    bool first_arg = true;
    while (!read_if(a, "]")) {
        validate(first_arg || read_if(a, ","), "expected , between lambda args");
        string n = read_alpha(a);
        validate(n, "expected arg name in lambda args");
        validate(read_if(a, ":"), "expected : after lambda arg %o", n);
        etype t = read_etype(a, null);
        validate(t, "expected type for lambda arg %o", n);
        def_arg(scratch, cstring(n), t->autype, 0);
        push(arg_names, (Au)n);
        push(arg_types, (Au)t);
        first_arg = false;
    }
    if (read_if(a, "->")) {
        etype rt = read_etype(a, null);
        validate(rt, "expected return type after ->");
        scratch->rtype = rt->autype;
    }
    array body = (array)hold(read_body(a));
    validate(body && len(body), "expected lambda body");

    push_scope(a, (Au)scratch, 40);
    // def-target buffer: keeps parse_statements' namespace out of the
    // fn members, which are exactly the context member list
    Au_t ns = def(null, null, AU_MEMBER_NAMESPACE, 0);
    ns->context = encl->autype;
    statements sblock = statements(mod, (aether)a, autype, ns);
    push_scope(a, (Au)sblock, 41);

    bool  nb  = a->no_build;         a->no_build        = true;
    i32   el  = a->expr_level;       a->expr_level      = 0;
    enode lr  = a->last_return;      a->last_return     = null;
    token so  = a->statement_origin;
    bool  ehr = encl->autype->has_return;
    a->gather_fn   = scratch;
    a->gather_base = len(a->lexical) - 2;
    push_tokens(a, (tokens)body, 0);
    parse_statements(a);
    pop_tokens(a, false);
    a->gather_fn        = null;
    a->no_build         = nb;
    a->expr_level       = el;
    a->last_return      = lr;
    a->statement_origin = so;
    encl->autype->has_return = ehr;
    pop_scope(a); // sblock
    pop_scope(a); // scratch

    // the real fn: fresh aus so no gather-time registration shadows it
    Au_t fn_au = def(null, cstring(lname), AU_MEMBER_FUNC, AU_TRAIT_LAMBDA);
    fn_au->context = encl->autype;  // names the context struct; no membership
    fn_au->module  = a->autype;
    fn_au->rtype   = scratch->rtype ? scratch->rtype : etypeid(none)->autype;
    for (int i = 0; i < len(arg_names); i++)
        def_arg(fn_au, cstring((string)get(arg_names, i)),
            ((etype)get(arg_types, i))->autype, 0);
    for (int i = 0; i < scratch->members.count; i++) {
        Au_t sm  = (Au_t)scratch->members.origin[i];
        Au_t cap = alloc_arg(fn_au, sm->ident, sm->src);
        cap->meta = sm->meta;
        micro_push((micro_*)&fn_au->members, (Au)cap);
    }

    efunc fmem = efunc(
        mod,    (aether)a,
        autype, fn_au,
        body,   (tokens)body,
        remote_code, false,
        has_code,    true,
        used,   true,
        target, null);
    implement(fmem, false);

    if (!a->inline_lambdas)  a->inline_lambdas  = (map)hold((Au)map(hsize, 16));
    if (!a->pending_lambdas) a->pending_lambdas = (array)hold((Au)array(alloc, 8));
    set(a->inline_lambdas, (Au)_i64((i64)(size_t)key), (Au)fmem);
    push(a->pending_lambdas, (Au)fmem);
    return inline_lambda_instance(a, fmem);
}

static enode parse_create_lambda(silver a, enode mem) {
    validate(read_if(a, "["), "expected [ context ] after lambda");

    // mem is the lambda function definition
    enode   lambda_f = (enode)evar_type((evar)mem);
    micro*  ctx_mem  = (micro*)&lambda_f->autype->members;  // context members after ::
    int     ctx_ln   = ctx_mem->count;
    array   ctx      = array(alloc, ctx_ln);
    
    Au_t lt = isa(lambda_f);

    // Parse context references - these become pointers in the context struct
    for (int i = 0; i < ctx_ln; i++) {
        Au_t  ctx_arg  = (Au_t)micro_get((micro_*)ctx_mem, i);
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

    return e_create(a, (etype)mem, (Au)ctx, false);
}

static enode parse_lambda_call(silver a, efunc mem) {
    // get arg info from underlying func via src
    efunc  src_fn = u(efunc, mem->autype->src);
    // an inferred var holds the lambda class: the func rides in meta_b
    if (!src_fn && mem->meta_b)
        src_fn = u(efunc, (Au_t)mem->meta_b);
    // no bracket: the lambda is handed over, not called
    if (!next_is(a, "["))
        return (enode)mem;
    if (!src_fn) {
        // declared `lambda ReturnType [ Args ]` — a member or argument
        // keeps that signature on its TYPE, not on the slot itself
        Au_t lt = au_arg_type((Au)mem->autype);
        if (!lt || !lt->args.count) lt = mem->autype;
        int n_args = lt->args.count;
        // a zero-arg call still spells its [ ] — consume it
        bool br = read_if(a, "[") != null;
        validate(n_args == 0 || br || a->expr_level == 0,
            "expected bracket for lambda call");
        a->expr_level++;
        array call_values = array(alloc, 32);
        for (int i = 0; i < n_args; i++) {
            Au_t arg = (Au_t)lt->args.origin[i];
            etype arg_type = u(etype, arg);
            enode arg_expr = parse_expression(a, arg_type, false, true);
            verify(arg_expr, "invalid lambda argument");
            push(call_values, (Au)arg_expr);
            if (i < n_args - 1) read_if(a, ",");
        }
        if (br) validate(read_if(a, "]"), "expected ] after lambda args");
        a->expr_level--;
        return lambda_fcall(a, mem, call_values);
    }
    int    n_args = user_arg_count(src_fn);

    // Parse the bracket if needed; a zero-arg call still spells [ ]
    bool br = read_if(a, "[") != null;
    validate(n_args == 0 || br || a->expr_level == 0,
        "expected bracket for lambda call");

    a->expr_level++;

    // Build array of arg values: user args + context at end
    array call_values = array(alloc, 32);

    // Parse each user arg using src func's arg types
    int arg_offset = (src_fn->autype->is_imethod || src_fn->autype->is_smethod) ? 1 : 0;
    for (int i = 0; i < n_args; i++) {
        Au_t  arg_decl = (Au_t)src_fn->autype->args.origin[i + arg_offset];
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

AU_EXPORT enode convertible(etype fr, etype to);

etype etype_ptr(aether a, Au_t au, enode eshape);

static enode parse_func_call(silver a, efunc f, bool poly) { sequencer
    push_current(a);
    validate(is_func((Au)f) || is_func_ptr((Au)f), "expected function got %o", f);
    bool read_br = false;
    bool cmode = false;
    bool user_explicit_target = false;
    bool empty_brackets = false;

    Au_t    fmdl   = au_arg_type((Au)f);
    efunc   fn     = (efunc)(is_func_ptr(f) ? (enode)f : u(enode, fmdl));
    micro*  m      = (micro*)&fmdl->args;
    int     ln     = m->count, i = 0;

    // C++ overload set: all same-ident is_c funcs on the record
    Au_t cands[32];
    int  n_cands = 0;
    if (fmdl->is_c && fmdl->is_imethod && fmdl->context && fmdl->ident) {
        Au_t rec = fmdl->context;
        for (int ci = 0; ci < rec->members.count && n_cands < 32; ci++) {
            Au_t mm = (Au_t)rec->members.origin[ci];
            if (mm->member_type == AU_MEMBER_FUNC && mm->ident &&
                strcmp(mm->ident, fmdl->ident) == 0)
                cands[n_cands++] = mm;
        }
        if (n_cands < 2) n_cands = 0;
    }

    if (is_cmode(a)) {
        cmode = true;
        read_br = read_if(a, "(") != null;
        if (!read_br) {
            pop_tokens(a, true);
            return efunc_fptr(f);
        }
    }
    else if (n_cands) {
        read_br = read_if(a, "[") != null;
        if (read_br && next_is(a, "]")) {
            read_if(a, "]");
            read_br = false;
            empty_brackets = true;
        }
    }
    else {
        if (user_arg_count(f) == 0 && next_is(a, "[")) {
            push_current(a);
            read_if(a, "[");
            if (next_is(a, "]")) {
                read_if(a, "]");
                pop_tokens(a, true);
                empty_brackets = true;
            } else {
                pop_tokens(a, false);
                read_br = read_if(a, "[") != null;
                user_explicit_target = true;
            }
        } else {
            read_br = read_if(a, "[") != null;
            validate(read_br || a->expr_level == 0,
                "expected [ for function call %o", f);
            if (read_br && next_is(a, "]")) {
                read_if(a, "]");
                read_br = false;
                empty_brackets = true;
            }
        }
    }
    
    a->expr_level++;

    array   values = array(alloc, 32, assorted, true);
    i32     offset = 0;

    enode push_target = f->target;
    if (!user_explicit_target && is_func((Au)f) && push_target &&
            (fmdl->is_imethod || (fmdl->is_smethod && push_target->loaded && empty_brackets && ln > 0))) {
        push(values, (Au)push_target);
        offset = 1;
    }

    bool* matched = calloc(ln, sizeof(bool));
    if (offset > 0) matched[0] = true;
    bool  comma_mode = (ln == 1 + (fmdl->is_imethod || fmdl->is_smethod)) || !read_br;

    // a top-level comma means positional syntax — switch out of commaless mode
    // before reading any args (avoids phantom arg count from type-match misplacement)
    if (read_br && !comma_mode) {
        int depth = 0;
        for (int k = a->cursor; k < len(a->tokens); k++) {
            token t = (token)a->tokens->origin[k];
            if (eq(t, "[") || eq(t, "(") || eq(t, "{")) depth++;
            else if (eq(t, "]") || eq(t, ")") || eq(t, "}")) {
                if (depth == 0) break;
                depth--;
            } else if (depth == 0 && eq(t, ",")) {
                comma_mode = true;
                break;
            }
        }
    }
    if (n_cands && read_br) {
        // overloads parse positional, hint-free; selection follows
        comma_mode = true;
        while (!next_is(a, "]")) {
            enode expr = parse_expression(a, null, true, true);
            verify(expr, "invalid expression");
            if (!is_loaded((Au)expr) && is_ptr(expr))
                expr = enode_value(expr, true);
            push(values, (Au)expr);
            if (!read_if(a, ","))
                break;
        }
    }
    else
    while (!empty_brackets && (i + offset < ln || fn->autype->is_vargs)) {
        Au_t   arg_decl = (Au_t)micro_get((micro_*)m, i + offset);
        Au_t   src  = (Au_t)au_arg_type((Au)arg_decl);
        etype  typ  = (arg_decl && arg_decl->is_formatter) ? null : u(etype, src);

        bool   ref_arg = arg_decl && arg_decl->is_explicit_ref;
        etype  expr_typ = ref_arg ? null : (comma_mode ? typ : null);
        enode  expr = parse_expression(a, expr_typ, !ref_arg, !ref_arg);
        verify(expr, "invalid expression");

        if (!comma_mode && !fn->autype->is_vargs) {
            // commaless: match expr type to best-fit unmatched parameter
            Au_t expr_type = au_arg_type((Au)expr);
            int  best = -1;
            for (int j = 0; j < ln; j++) {
                if (matched[j]) continue;
                Au_t ptype = au_arg_type((Au)micro_get((micro_*)m, j));
                if (expr_type == ptype || inherits(expr_type, ptype) ||
                    (expr_type->is_integral && ptype->is_integral) ||
                    (expr_type->is_realistic && ptype->is_realistic)) {
                    best = j;
                    break;
                }
            }
            if (best >= 0) {
                // convert to expected type and place at matched position
                Au_t best_decl = (Au_t)micro_get((micro_*)m, best);
                etype best_type = u(etype, au_arg_type((Au)best_decl));
                expr = e_create(a, best_type, (Au)expr, false);
                // ensure values array is big enough and place at correct index
                while (len(values) <= best)
                    push(values, (Au)null);
                values->origin[best] = (Au)expr;
                matched[best] = true;
            } else {
                push(values, (Au)expr);
            }
        } else {
            // load unloaded pointer values (e.g. opaque handle from new array offset)
            if (!ref_arg && !is_loaded((Au)expr) && is_ptr(expr))
                expr = enode_value(expr, true);
            // varargs: also load unloaded *primitive* indexer results so
            // printf %f / %i don't end up reading slot GEP pointer bytes
            // instead of the actual value at that slot. this is the
            // companion to the e_convert_or_cast bool-coercion fix.
            if (fn->autype->is_vargs && !ref_arg && !is_loaded((Au)expr))
                expr = enode_value(expr, true);
            push(values, (Au)expr);
        }
        i++;

        if (read_if(a, ",")) {
            comma_mode = true;
            continue;
        }

        if (comma_mode) {
            verify(len(values) >= ln, "expected %i args for function %o (%i)", ln, f, seq);
            break;
        }
        // commaless: nothing separates the args, so read to the bracket
        if (read_br && !next_is(a, "]")) continue;
        break;
    }

    if (n_cands) {
        // exact type 3 > numeric family 2 > convertible 1; unique max wins
        int  user_n = len(values) - offset;
        Au_t best = null;
        int  best_score = -1, ties = 0;
        for (int ci = 0; ci < n_cands; ci++) {
            Au_t cand = cands[ci];
            if ((int)cand->args.count - 1 != user_n) continue;
            int  score = 0;
            bool fit   = true;
            for (int k = 0; k < user_n; k++) {
                enode ex = (enode)values->origin[offset + k];
                Au_t  et = au_arg_type((Au)ex);
                Au_t  pt = au_arg_type((Au)micro_get((micro_*)&cand->args, k + 1));
                if (et == pt) score += 3;
                else if ((et->is_integral  && pt->is_integral) ||
                         (et->is_realistic && pt->is_realistic)) score += 2;
                else if (et->is_pointer && pt->is_pointer && ({
                    Au_t ea = et; while (ea->src && !ea->is_enum) ea = ea->src;
                    Au_t pa = pt; while (pa->src && !pa->is_enum) pa = pa->src;
                    ea == pa; })) score += 2;
                else {
                    etype ee = etype_prep((aether)a, et);
                    etype pp = etype_prep((aether)a, pt);
                    if (ee && pp && convertible(ee, pp)) score += 1;
                    else { fit = false; break; }
                }
            }
            if (!fit) continue;
            if (score > best_score) { best_score = score; best = cand; ties = 1; }
            else if (score == best_score) ties++;
        }
        validate(best, "no matching overload of %s for %i args", fmdl->ident, user_n);
        validate(ties == 1, "ambiguous call: %i overloads of %s match equally", ties, fmdl->ident);
        if (best != fmdl) {
            etype bt = etype_prep((aether)a, best);
            etype_implement(bt, false);
            fn   = efunc(mod, (aether)a, autype, best, loaded, true,
                is_super, ((enode)f)->is_super, target, f->target);
            fmdl = best;
        }
    }
    else if (!comma_mode) {
        while (len(values) < ln)
            push(values, (Au)null);
    }
    free(matched);
    a->expr_level--;
    validate(!read_br || read_if(a, cmode ? ")" : "]"),
        "%o: expected %i args, got %i", fn, ln, len(values));
    pop_tokens(a, true);
    bool saved_direct = a->direct;
    if (poly) a->direct = false;
    enode result = e_fn_call(a, fn, values, f->is_super, poly);
    a->direct = saved_direct;

    return result;
}

static enode typed_expr(silver a, enode f, array expr) {
    push_tokens(a, expr ? (tokens)expr : a->tokens, expr ? 0 : a->cursor);

    // EnumType['string'] — runtime enum lookup by name
    if (is_enum(f) && expr) {
        enode str_arg = parse_expression(a, etypeid(string), false, true);
        pop_tokens(a, expr ? false : true);
        if (str_arg) {
            Au_t fn_evalue = find_member(typeid(Au), "evalue", AU_MEMBER_FUNC, 0, false);
            verify(fn_evalue, "evalue function not found");
            efunc f_evalue = (efunc)u(efunc, fn_evalue);
            enode type_id  = e_typeid(a, (etype)f);
            return e_fn_call(a, f_evalue, a(type_id, str_arg), false, false);
        }
    }

    // function calls
    efunc f_decl = u(efunc, f->autype);
    if (f_decl) {
        micro*  m      = (micro*)&f_decl->autype->args;
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

        while (i + offset < ln || f_decl->autype->is_vargs) {
            Au_t   arg  = (Au_t)micro_get((micro_*)m, i + offset);
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
        return e_fn_call(a, f_decl, values, false, false);
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
    Au_t vau9 = vec_elem_of((etype)f);
    if (!has_content) {
        if (vau9) {
            // empty [ ] initializer on a vec slot: an empty vector
            r = e_create(a, vector_etype(a, vau9), null, false);
            r->meta_a = (Au)vau9;
        } else
            r = e_create(a, (etype)f, null, false); // default
        conv = false;
    } else if (vau9) {
        // [ v1, v2, ... ] literal seeds a vector
        etype element_type = u(etype, vau9);
        if (!element_type) element_type = etype_prep((aether)a, vau9);
        enode vecn = e_create(a, vector_etype(a, vau9), null, false);
        vecn->meta_a = (Au)vau9;
        while (peek(a)) {
            enode e = parse_expression(a, element_type, false, true);
            e_assign((aether)a, vecn, (Au)e, OPType__assign_add);
            read_if(a, ",");
        }
        r = vecn;
        conv = false;
    } else if (class_inherits((etype)f, etypeid(array))) {
        array nodes         = array(64);
        etype element_type  = u(etype, f->autype->src);
        shape sh            = instanceof(f->autype->meta.b, shape);
        validate(sh, "expected shape on array");
        int   shape_len     = shape_total(sh);
        int   top_stride    = sh->count ? sh->data[sh->count - 1] : 0;
        validate((!top_stride && shape_len == 0) || (top_stride && shape_len),
            "unknown stride information");  
        int   num_index     = 0;

        while (peek(a)) {
            token n = peek(a);
            enode e = parse_expression(a, element_type, false, true);
            e = e_create(a, element_type, (Au)e, false);
            push(nodes, (Au)e);
            num_index++;
            if (top_stride && (num_index % top_stride == 0)) {
                validate(read_if(a, ",") || !peek(a),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    top_stride);
            }
        }
        r = e_create(a, (etype)f, (Au)nodes, false);
    } else if (peek_fields(a) || class_inherits((etype)f, etypeid(map))) {
        conv = false; // parse map will attempt to go direct
        r    = (enode)parse_object(a, (etype)evar_type((evar)f), true);
    } else if (canonical((etype)f)->autype->is_scalar) {
        etype scalar_type = canonical((etype)f);
        etype value_type = u(etype, scalar_type->autype->src);
        r = parse_expression(a, value_type, false, true);
        r = e_create(a, scalar_type, (Au)r, false);
        conv = false;
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
        r = e_create(a, (etype)f, (Au)r, false);
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
    a->module = hold(eq(e, "ag") ? parent_dir(module_path) : module_path);
    return a;
}

token read_compacted(silver a) {
    token  f = next(a, Syntax__none);
    if (!f) return null;
    string r = string(f->chars);
    int len = r->count;
    int start_col = f->column;

    for (;;) {
        token n = peek(a);
        if (!n || n->column != (start_col + len) || n->line != f->line)
            break;
        concat(r, (string)n);
        consume(a, Syntax__none);
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

    // export KEY: 'value' — environment variable export
    token t = peek(a);
    if (t && t->chars[0] >= 'A' && t->chars[0] <= 'Z') {
        string key = read_alpha(a);
        validate(key, "expected environment variable name");
        validate(read_if(a, ":"), "expected : after export key");
        string val = read_string(a);
        validate(val, "expected string value for export");
        val = interpolate(val, (Au)a);
        silver og = a->is_external ? a->is_external : a;
        exports exp = (exports)get(og->exports, (Au)string(a->name->chars));
        if (!exp) {
            exp = exports(module_path, a->module_path, module_file, a->module_file,
                          project_path, a->project_path,
                          install_name, silver_install_name(a));
            set(og->exports, (Au)string(a->name->chars), (Au)exp);
        }
        if (!exp->env_vars)
            exp->env_vars = map(hsize, 8);
        set(exp->env_vars, (Au)key, (Au)val);
        return e_noop(a, null);
    }

    // export area ['value', 'value'] — registry entry (export extensions ['.n64'])
    if (t && t->chars[0] >= 'a' && t->chars[0] <= 'z') {
        string area = read_alpha(a);
        validate(area, "expected export area name");
        validate(read_if(a, "["), "expected [ after export %o", area);
        silver og = a->is_external ? a->is_external : a;
        exports exp = (exports)get(og->exports, (Au)string(a->name->chars));
        if (!exp) {
            exp = exports(module_path, a->module_path, module_file, a->module_file,
                          project_path, a->project_path,
                          install_name, silver_install_name(a));
            set(og->exports, (Au)string(a->name->chars), (Au)exp);
        }
        if (!exp->areas)
            exp->areas = map(hsize, 8);
        array vals = (array)get(exp->areas, (Au)area);
        if (!vals) {
            vals = array(8);
            set(exp->areas, (Au)area, (Au)vals);
        }
        for (;;) {
            string val = read_string(a);
            validate(val, "expected string value for export %o", area);
            string entry = interpolate(val, (Au)a);
            // optional applicability tag: 'name': FileType.member
            if (read_if(a, ":")) {
                string ety = read_alpha(a);
                validate(ety && strcmp(ety->chars, "FileType") == 0,
                    "expected FileType enum for export %o", area);
                validate(read_if(a, "."), "expected . after FileType");
                string mem = read_alpha(a);
                validate(mem, "expected FileType member");
                Au_t ev = find_member(typeid(FileType), cstring(mem),
                    AU_MEMBER_ENUMV, 0, false);
                validate(ev, "unknown FileType member %o", mem);
                entry = f(string, "%o:%o", entry, mem);
            }
            push(vals, (Au)entry);
            if (!read_if(a, ","))
                break;
        }
        validate(read_if(a, "]"), "expected ] after export %o values", area);
        return e_noop(a, null);
    }

    a->exported_version = hold(read_compacted(a));
    verify(len(a->exported_version), "expected version");

    // register with the main silver instance (og) so tags are collected in one place
    // find-or-create: an earlier area export must not be clobbered here
    silver og = a->is_external ? a->is_external : a;
    exports exp = (exports)get(og->exports, (Au)string(a->name->chars));
    if (!exp) {
        exp = exports(module_path, a->module_path, module_file, a->module_file,
                      project_path, a->project_path,
                      install_name, silver_install_name(a));
        set(og->exports, (Au)string(a->name->chars), (Au)exp);
    }
    exp->version = hold(a->exported_version);

    // a hash can be made of the entire module-dir,
    // not so efficient to compute back from git data
    // encompassing all resources in folder is not what we want, though -- nor would we track artifacts

    verify(a->project_path, "expected silver invocation into main project module");
    return e_noop(a, null);
}

// ── background module builds ────────────────────────────────────────────────
// the leading run of `import <name>` statements is known before parsing; the
// silver modules among them build on their own threads so the work overlaps
// the imports that follow. parse_import is unchanged in shape — it collects a
// finished module here instead of constructing one itself.
typedef struct bg_build {
    path      module;      // the module's .ag
    silver    a, og;       // importing instance and artifact keeper
    map       defs;
    silver    result;
    pthread_t th;
    bool      launched;
    struct bg_build* next;
} bg_build;

static bg_build*       bg_list = null;
static pthread_mutex_t bg_lock = PTHREAD_MUTEX_INITIALIZER;

extern void aether_overlay(path install, path base);

static void* bg_build_run(void* arg) {
    bg_build* b = (bg_build*)arg;
    // an overlay build's children stay inside the overlay
    if (b->a->base_install)
        aether_overlay(b->a->install, b->a->base_install);
    b->result = silver(module, b->module, breakpoint, b->a->breakpoint,
        verbose, b->a->verbose, is_external, b->og, is_child, b->a,
        release, b->a->release, clean, b->a->clean, format, b->og->format,
        defs, b->defs, debug_type, b->a->debug_type, debugmember, b->a->debugmember);
    return null;
}

static void bg_build_start(silver a, silver og, path module, map defs) {
    bg_build* b = (bg_build*)calloc(1, sizeof(bg_build));
    b->module = (path)hold(module);
    b->a = a; b->og = og; b->defs = defs;
    pthread_mutex_lock(&bg_lock);
    b->next = bg_list; bg_list = b;
    pthread_mutex_unlock(&bg_lock);
    b->launched = pthread_create(&b->th, null, bg_build_run, b) == 0;
    if (!b->launched) bg_build_run(b);
}

// collect the module built for this path, waiting if it is still going
static silver bg_build_take(path module) {
    pthread_mutex_lock(&bg_lock);
    bg_build* b = bg_list;
    // match canonically: the prescan and the import can spell the same module
    // differently, and a miss here silently builds a second instance that
    // short-circuits on silver_compiled and yields an empty product
    path want = absolute(module);
    while (b && (!b->module || compare(absolute(b->module), want) != 0)) b = b->next;
    pthread_mutex_unlock(&bg_lock);
    if (!b) return null;
    if (b->launched) { pthread_join(b->th, null); b->launched = false; }
    silver r = b->result;
    b->result = null;
    drop(b->module);
    b->module = null;
    return r;
}

enode parse_import(silver a) {
    sequencer;

    validate(next_is(a, "import"), "expected import keyword");
    consume(a, Syntax__keyword);

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
    string  name         = null;
    array   with_exts    = null;   // `import M with ext…` — ext.ag paths in THIS dir

    // a url import is a FILE, never a repo: nothing is cloned, configured
    // or built. it lands in install/imports as <hash>-<stem>, so two
    // images.zip from different urls cannot collide. the file existing IS
    // the cache. the path is published as {import_file} for the > lines.
    // the lexer splits on ':' so the peeked token is only "https" -- glue
    // the run back, then require the scheme SEPARATOR, since a bare
    // "http" prefix would also match an identifier like httpMaster
    token utok = null;
    bool  url_fetched = false;
    if (t && (eq(t, "https") || eq(t, "http"))) {
        push_current(a);
        utok = read_compacted(a);
        bool ok = utok && (strncmp(utok->chars, "https://", 8) == 0 ||
                           strncmp(utok->chars, "http://",  7) == 0);
        pop_tokens(a, ok);   // keep the read only if it really is a url
        if (!ok) utok = null;
    }
    if (utok) {
        string url  = string(utok->chars);
        cstr   sl   = strrchr(url->chars, '/');
        string stem = string(sl ? sl + 1 : url->chars);
        path   idir = f(path, "%o/imports", a->install);
        make_dir(idir);
        path   dest = f(path, "%o/%08llx-%o", idir,
                        (u64)(Au_hash((Au)url) & 0xffffffff), stem);
        if (!file_exists("%o", dest)) {
            // fetch beside it and move on success. curl writing the final
            // name directly leaves a partial file when it fails, and
            // 'file exists' is the cache signal -- it would never retry
            path part = f(path, "%o.part", dest);
            au_exec_prefix(cstring(stem));
            vexec(a->verbose, "import-url", "curl -fL -o %o %o", part, url);
            verify(file_exists("%o", part), "import: fetch failed %o", url);
            vexec(a->verbose, "import-url", "mv %o %o", part, dest);
            au_exec_prefix(null);
            url_fetched = true;
        }
        a->import_file = hold(dest);
    }

    if (!utok && t && isalpha(t->chars[0])) {
        bool   cont     = false;
        service  = a->git_service;
        user     = a->git_owner;
        project  = null;
        aa       = expect_alpha(a); // value of t
        // the module name is a NAMESPACE, not a plain identifier; the
        // editor keys navigation off this kind
        token aa_tok = element(a, -1);
        if (aa_tok) aa_tok->syntax = Syntax__namespace;
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

    // `import <silver-module> with ext1 ext2 …` — each `ext` is an ext.ag file in
    // THIS module's dir, headed `extend <module>`, compiled into the imported
    // module's build. validated below as silver-module-only (errors for C headers,
    // git deps, codegen, and sub-module/extension imports).
    if (read_if(a, "with")) {
        token wt = element(a, -1);
        if (wt) wt->syntax = Syntax__keyword;
        path mdir = a->module_file ? parent_dir(a->module_file) : a->module;
        for (;;) {
            string en = read_alpha(a);
            validate(en, "import with: expected extension name");
            token et = element(a, -1);
            if (et) et->syntax = Syntax__type;
            path ep = f(path, "%o/%o.ag", mdir, en);
            validate(file_exists("%o", ep),
                "import with: extension '%o.ag' not found in %o", en, mdir);
            if (!with_exts) with_exts = array(8);
            push(with_exts, (Au)hold(ep));
            if (!read_if(a, ",")) break;
        }
    }

    array includes = array(32);
    string first_include = null;

    // determine includes, uri, and config
    // includes for this import
    token lt = is_codegen ? null : read_if(a, "<");
    if (lt) {
        // the angle brackets are punctuation (same kind as '['); the header path inside is
        // colored like a string so `import <sys/wait.h>` reads as keyword + punct + path.
        lt->syntax = Syntax__punctuation;
        for (;;) {
            string f = read_alpha_any(a);
            validate(f, "expected include");
            token ft = element(a, -1);
            if (ft) ft->syntax = Syntax__str;

            if (!first_include)
                first_include = f;

            // we may read: something/is-a.cool\file.hh.h
            while (next_is_neighbor(a) && (!next_is(a, ",") && !next_is(a, ">")))
                concat(f, (string)next(a, Syntax__str));

            push(includes, (Au)f);

            if (!read_if(a, ",")) {
                token n = read_if(a, ">");
                validate(n, "expected '>' after include, list, of, headers");
                n->syntax = Syntax__punctuation;
                break;
            }
        }
    }

    map define_map = null;
    bool import_cpp = false;
    array b = hold(import_conditionals(a, read_body(a)));
    if (len(b)) {
        array bt = compact_tokens(b);
        each(bt, string, tcx)
            if (starts_with(tcx, "-std=c++"))
                import_cpp = true;
        if (!a->frameworks)
            a->frameworks = hold(map(16));
        import_libs(a, bt, a->libs, a->frameworks);
        if (!a->include_paths)
            a->include_paths = hold(array(16));
        if (!define_map)
            define_map = map(hsize, 16);
        import_include_paths(a, bt, a->include_paths);
        import_defines(a, b, define_map);
        // bare -DNAME (no =) is a consumer define, not build config
        each(bt, string, t)
            if (starts_with(t, "-D") && !strchr(t->chars, '='))
                set(define_map, (Au)mid(t, 2, len(t) - 2), (Au)_bool(true));
    }

    map defs = len(b) ? map() : null;
    bool is_fields = false;
    if (defs) {
        push_tokens(a, (tokens)b, 0);
        // every `name: value` line is a config input; reading only the
        // first dropped the rest silently
        while (peek_fields(a)) {
            is_fields = true;
            Au k = parse_field(a, null);
            verify(read_if(a, ":"), "expected : after field");
            bool is_const = false;
            array tokens = read_expression(a, null, &is_const);
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
    string import_address = null;
    path   external_product = null;
    // a cross build's loadable copy: built for THIS machine, never linked
    path   host_product = null;
    string framework = framework_import_name(mpath, single);
    bool   is_framework_import = !!framework;

    if (is_framework_import) {
        if (!a->frameworks)
            a->frameworks = hold(map(16));
        set(a->frameworks, framework_name(framework), _bool(true));
        if (!len(includes))
            push(includes, (Au)f(path, "%o/%o.h", framework, framework));
    }

    if (read_if(a, "from")) {
        uri = hold(read_alpha(a)); // todo: compact neighboring tokens with https:// and git://
        validate(uri, "expected uri");
    }

    if (is_framework_import) {
    } else if (!is_codegen && aa && !bb && !commit) {
        path m = module_exists(a, mpath, true, &is_binary); // useful to resolve in either case

        if (!is_binary && m) {
            module_source = hold(m);
        } else if (is_binary && m) {
            lib_path = hold(m);
            string built_name = stem(m);
            if (strlen(lib_pre) && starts_with(built_name, lib_pre))
                built_name = mid(built_name, strlen(lib_pre),
                    len(built_name) - strlen(lib_pre));
            string local_prefix = is_silver_repo(a)
                ? string("silver") : a->git_owner;
            string names[3] = {
                local_prefix ? f(string, "%o-%o", local_prefix, aa) : null,
                f(string, "silver-%o", aa), aa
            };
            string share_name = null;
            for (int n = 0; n < 3 && !share_name; n++) {
                if (!names[n]) continue;
                int nl = len(names[n]);
                if (starts_with(built_name, cstring(names[n])) &&
                    (len(built_name) == nl || built_name->chars[nl] == '-'))
                    share_name = names[n];
            }
            if (share_name) {
                silver og = a;
                while (og->is_external) og = (silver)og->is_external;
                path module_install = parent_dir(parent_dir(m));
                collect_resource_dirs(og,
                    f(path, "%o/share/%o", module_install, share_name));
                external_name = hold(share_name);
            }
        }
        
        // if the module is built into our run-time already, we support this
        if (mod && !module_source) {
            set(a->libs, string(mod->ident), (Au)_bool(true));
            external_name = hold(string(mod->ident));
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

    // your own namespace IS this repository: owner:project naming the repo
    // we stand in resolves locally — no clone, no overlay, no extra syntax
    if (user && project && cc && !module_source && !lib_path &&
        a->git_owner && a->git_project &&
        cmp(user, a->git_owner->chars) == 0 &&
        cmp(project, a->git_project->chars) == 0) {
        path lm = f(path, "%s/%o/%o.ag", SILVER, cc, cc);
        if (file_exists("%o", lm)) {
            print("[silver] %o:%o is this repository — using local %o", user, project, cc);
            module_source = hold(lm);
        }
    }

    if (project && !lib_path && !module_source) {
        if (aa && bb && cc)
            import_address = commit
                ? f(string, "%o:%o:%o/%s", aa, bb, cc, commit->chars)
                : f(string, "%o:%o:%o", aa, bb, cc);
        else if (aa && bb)
            import_address = commit
                ? f(string, "%o:%o/%s", aa, bb, commit->chars)
                : f(string, "%o:%o", aa, bb);
        else if (aa)
            import_address = commit
                ? f(string, "%o/%s", aa, commit->chars)
                : aa;

        string path_str = string();
        // cc is the module selector, never a blob path
        if (len(mpath) && !cc) {
            string str_mpath = join(mpath, "/") ? cc : string("");
            path_str = len(str_mpath) ? f(string, "blob/%o/%o", commit, str_mpath) : string("");
        }
        uri = f(string, "https://%o/%o/%o%s%o", service, user, project,
                cast(bool, path_str) ? "/" : "", path_str);
    }

    validate(!with_exts || (module_source && !is_codegen),
        "import with: only valid for a silver module (not a C header, git dep, or codegen)");

    // a url import has no checkout: run only its `>` commands, read from
    // all_config exactly as a repo import reads them. a failing command
    // is an import error -- no hand-woven && chains in the .ag
    if (utok) {
        // the cache proves the fetch, not its artifacts: a --clean or a
        // wiped share bundle drops them, so the > lines always run
        {
            array cmds = import_build_commands(all_config, ">");
            each(cmds, string, cmd) {
                string icmd = interpolate(cmd, (Au)a);
                int    rc   = command_exec((command)icmd, a->verbose);
                verify(rc == 0, "import: command failed (%i): %o", rc, icmd);
            }
        }
        // null reads as parse failure in the statement loop
        return e_noop(a, null);
    }

    validate(!cc || uri,
        "module selector %o: only valid on a git import", cc);
    if (uri) {
        checkout(a, path(uri->chars), (string)commit,
                 import_build_commands(all_config, ">"),
                 import_build_commands(all_config, ">>"),
                 import_config(all_config),
                 import_env(all_config),
                 cc,
                 import_address ? import_address : external_name);
        // an uninstall walks the imports for their ledgers only
        if (a->uninstall) return e_noop(a, null);
        bool has_link = false;
        if (!a->frameworks)
            a->frameworks = hold(map(16));
        for (int fi = 0; fi < len(all_config); fi++) {
            string t = (string)all_config->origin[fi];
            if (starts_with(t, "-l")) {
                set(a->libs, (Au)mid(t, 2, len(t) - 2), (Au)_bool(true));
                has_link = true;
            } else if (starts_with(t, "-framework") && fi + 1 < len(all_config)) {
                string fw = framework_name((string)all_config->origin[++fi]);
                set(a->frameworks, fw, _bool(true));
            }
        }
        // auto-link: if no -l specified, use the project name (only if lib exists)
        if (!has_link && project) {
            string lib_check = f(string, "%o/lib/%s%o%s", a->install, lib_pre, project, lib_ext);
            if (file_exists("%o", lib_check))
                set(a->libs, (Au)project, (Au)_bool(true));
        }
        // rust checkout: cbindgen wrote include/<project>.h — import it
        // automatically when the user listed no headers
        if (project && !len(includes)) {
            path rust_checkout = user
                ? f(path, "%o/checkout/%o/%o", a->root_path, user, project)
                : f(path, "%o/checkout/%o", a->root_path, project);
            if (file_exists("%o/Cargo.toml", rust_checkout)) {
                path rust_h = f(path, "%o/include/%o.h", a->install, project);
                if (file_exists("%o", rust_h))
                    push(includes, (Au)rust_h);
            }
        }
    } else if (module_source) {
        path module = parent_dir(module_source);

        bool ag = eq(ext(module_source), "ag");
        bool c  = is_native_source_ext(a, ext(module_source));
        bool is_extension = ag && compare(parent_dir(module_source), absolute(a->module)) == 0;
        verify(!ag || is_extension || compare(stem(module_source), stem(module)) == 0, "silver expects identical module stem");
        validate(!with_exts || (ag && !c && !is_extension),
            "import with: only valid for a silver module, not a native source or sub-module");

        
        // we should turn on object tracking here, as to trace which objects are still in memory after we drop
        // the simple ternary statement allows us to give an og silver
        // og silver is keeper of artifacts
        if (c) {
            // handled after 'as' is read
        } else {
            if (is_extension) {
                // sibling extension (Editor.ag etc.) imported within the module dir —
                // parse it inline via the shared helper (tracks source, strips the
                // `extend <name>` header, replays its statements into this module).
                parse_extension(a, module_source, true);
            } else {
                // artifacts/resources scope to the nearest install owner,
                // so an overlay build stays inside its own install
                silver og = a;
                while (og->is_external &&
                       compare(((silver)og->is_external)->install, og->install) == 0)
                    og = (silver)og->is_external;

            {
                Au_t f = find_module("vulkan2");
                // hand the `with` extensions to the about-to-be-built external via a
                // transient static (silver_init reads it at the top) — the prop-pair
                // ctor is already at its 22-arg max, and the external builds in init.
                // a cross build still has to LOAD this import: the compiler
                // runs on THIS machine, so build a host copy first and keep
                // its product for the loader. the device copy links
                if (a->device && len(a->device)) {
                    g_host_build = true;
                    if (a->base_install) aether_overlay(a->install, a->base_install);
                    silver host  = silver(module, module, breakpoint, a->breakpoint,
                        verbose, a->verbose, is_external, og, is_child, a, release, a->release, clean, a->clean,
                        format, og->format,
                        defs, defs, debug_type, a->debug_type, debugmember, a->debugmember);
                    g_host_build = false;
                    if (host && host->product) host_product = hold(host->product);
                }
                g_import_with = with_exts;
                // a prescanned import is already building: collect it
                // a silver module import is shared: an uninstall leaves it be
                if (a->uninstall) return e_noop(a, null);
                silver external = with_exts ? null : bg_build_take(module);
                if (!external) {
                    if (a->base_install) aether_overlay(a->install, a->base_install);
                    external = silver(module, module, breakpoint, a->breakpoint,
                        verbose, a->verbose, is_external, og, is_child, a, release, a->release, clean, a->clean,
                        format, og->format,
                        defs, defs, debug_type, a->debug_type, debugmember, a->debugmember);
                }
                g_import_with = null;
                Au_t f2 = find_module("vulkan2");
                // these should be the only two objects remaining.
                external_name    = silver_install_name(external);
                external_product = hold(external->product);

                if (external_product) {
                    if (index_of(og->artifacts, (Au)external_product) < 0) {
                        push(og->artifacts, (Au)external_product);
                    }
                    // credit the importing module too — a module built as an
                    // external otherwise writes an EMPTY .artifacts and never
                    // notices its deps rebuilding (stale .so vs new trinity)
                    if (a != og && index_of(a->artifacts, (Au)external_product) < 0) {
                        push(a->artifacts, (Au)external_product);
                    }
                }

                if (external->module_file) {
                    if (index_of(og->artifacts, (Au)external->module_file) < 0) {
                        push(og->artifacts, (Au)external->module_file);
                    }
                }
                // propagate the external module's source files (its own extends,
                // e.g. trinity's Canvas.ag / element.ag) into the app's
                // artifacts so the host watch list (.source) covers them too.
                if (external->artifacts) {
                    each (external->artifacts, path, ext_src) {
                        const char *dot = strrchr(ext_src->chars, '.');
                        bool is_src = dot && (strcmp(dot, ".ag") == 0 ||
                                              strcmp(dot, ".c")  == 0 ||
                                              strcmp(dot, ".cc") == 0);
                        if (is_src && index_of(og->artifacts, (Au)ext_src) < 0)
                            push(og->artifacts, (Au)ext_src);
                    }
                }
                validate (!external->error, "error importing silver module %o", external);

                // propagate the external module's own external lib deps (e.g.
                // img -> -lpng -lz) into our link. its objects are merged into
                // this app and reference those symbols, which resolve only at
                // the final app link — copy before `external` is dropped.
                if (external->libs) {
                    pairs(external->libs, li) {
                        set(a->libs, (Au)hold(li->key), (Au)_bool(true));
                    }
                }

                drop(external);
                //drop(external); // this is to compensate for the initial hold in silver_init [ quirk for build in init ]
            }
                // a --format build skips build_product (no codegen/link), so an external
                // has no product → external_product is null. only link it when it exists;
                // --format doesn't link anyway, so skipping is correct (not a workaround).
                if (external_product)
                    set(a->libs, (Au)string(external_product->chars), (Au)_bool(true));
            } // end else (not frag)
        }

    }
    else if (is_codegen) {
        cg = (codegen)construct_with(is_codegen, (Au)props, null);
        cg->mod = (aether)a;
    }

    if (next_is(a, "as")) {
        consume(a, Syntax__none);
        namespace = hold(read_alpha(a));
        validate(namespace, "expected alpha-numeric %s",
                 is_codegen ? "alias" : "namespace");
    } else if (is_codegen) {
        namespace = hold(string(is_codegen->ident));
    }

    // native sub-module: compile, and include .h if present
    string ext = module_source ? ext(module_source) : null;
    if (module_source && is_native_source_ext(a, ext)) {
        path   dir    = parent_dir(module_source);
        string name   = stem(module_source);
        path   header = f(path, "%o/%o.h", dir, name);
        if (file_exists("%o", header))
            push(includes, (Au)string(header->chars));
        else if (eq(ext, "rs")) {
            // no hand-written header: cbindgen emits the extern "C" surface
            path gen = f(path, "%o/%o.h", a->build_dir, name);
            string cbg = f(string, "%o/bin/cbindgen", a->base_install ? a->base_install : a->install);
            if (!file_exists("%o", cbg)) cbg = string("cbindgen");
            validate(exec(a->verbose, "%o --lang c -o %o %o", cbg, gen, module_source) == 0,
                "cbindgen failed for %o", module_source);
            push(includes, (Au)gen);
        }
        if (!a->implements)
            a->implements = hold(array(2));
        push(a->implements, (Au)module_source);
    }

    // hash. for cache.  keep cache warm
    int to = a->cursor;
    array tokens = array(alloc, to - from + 1);
    for (int i = from; i < to; i++) {
        token t = (token)a->tokens->origin[i];
        push(tokens, (Au)t);
    }

    bool import_Au = !!external_name;
    if (!external_name && !is_framework_import) {
        if (project)
            external_name = project;
        else if (aa)
            external_name = aa;
        else if (first_include)
            external_name = first_include;
        else {
            fault("no identity found for import");
        }
    }

    bool is_au_rt = !module_source || eq(ext(module_source), "ag");
    import mdl = import(
        mod,                (aether)a,
        codegen,            cg,
        external_name,      external_name,
        external_product,   external_product,
        host_product,       host_product,
        tokens,             tokens,
        define_map,         define_map,
        module_source,      module_source,
        is_au_rt,           is_au_rt);

    push(a->imports, (Au)mdl);
    mdl->is_cpp = import_cpp;

    mdl->autype->alt = namespace ? cstr_copy(symbol_name((Au)namespace)->chars) : null;
    
    if (len(includes)) {
        //push_scope(a, (Au)mdl);
        mdl->include_paths = hold(array());

        // include each, collecting the clang instance for which we will invoke macros through
        each(includes, string, inc) {
            path ipath = (Au_t)isa(inc) == typeid(string) ?
                aether_lookup_include((aether)a, (string)inc) : (path)inc;

            // null = header not found in tracked paths (e.g. platform-guarded
            // system include); skip — the C compiler resolves it itself.
            if (ipath)
                push(mdl->include_paths, (Au)ipath);
        }
    }


    // loads the actual library here -- DO NOT integrate external->autype module; we load it direct with our own
    // and let the runtime register itself
    // this is so we may be Au-centric, and language agnostic
    if (!is_codegen && (import_Au || mod || lib_path)) {
        import_Au(a,
            mdl->external_name,
            // the loader runs HERE: prefer the host copy on a cross build
            mdl->host_product     ? (Au)mdl->host_product :
            mdl->external_product ? (Au)mdl->external_product : mod ? (Au)mod : (Au)lib_path);
        mdl->autype->is_closed = true;
        // the loader took the host copy; the link must take the device's
        if (mdl->host_product && mdl->external_product &&
            compare(mdl->host_product, mdl->external_product) != 0) {
            map_rm(a->libs, (Au)string(mdl->host_product->chars));
            set(a->libs, (Au)string(mdl->external_product->chars), (Au)_bool(true));
        }
    }

    //mdl->autype->is_closed = true;
    mdl->lib_path = hold(lib_path);
    mdl->module_source = hold(module_source);

    if (is_codegen) {
        string name = namespace ? (string)namespace : string(is_codegen->ident);
        set(a->codegens, (Au)name, (Au)mdl->codegen);
    }

    return (enode)mdl;
}

void assign_if_cond(aether a, enode targ, enode cond, subprocedure expr);
enode is_set(enode n, evar prop);

enode assign_builder(silver a, enode targ, array post_const) { sequencer
    int level = a->expr_level;
    a->expr_level++;
    push_tokens(a, (tokens)post_const, 0);
    // { tokens } defaults parse biased by the member's own type
    bool kw = next_is(a, "{");
    enode expr = parse_expression(a, targ ? (etype)evar_type((evar)targ) : null, kw, true);
    pop_tokens(a, false);
    a->expr_level = level;
    return expr;
}

// works for class and module init
void silver_build_user_initializer(silver a, enode prop) {
    if (prop && prop->autype && prop->autype->member_type == AU_MEMBER_VAR && prop->initializer) {
        // skip if this member was already set by constructor props
        if (a->init_props && prop->autype->ident) {
            bool in_props = false;
            pairs(a->init_props, p) {
                string k = (string)instanceof(p->key, string);
                if (k && strcmp(k->chars, prop->autype->ident) == 0) {
                    in_props = true; break;
                }
            }
            if (in_props) return;
        }

        verify (!instanceof(prop->initializer, enode), "unexpected enode");
        array initializer = (array)prop->initializer;
        array post_const = parse_const(a, (array)prop->initializer);
        subprocedure set_if = subproc(a, assign_builder, post_const);
        efunc  ctx = context_func(a);

        // establish statement_origin so deep aether allocations in this
        // init path can stamp source/line on every Au they create.
        token saved_origin = a->statement_origin;
        if (len(initializer))
            a->statement_origin = hold((token)initializer->origin[0]);
        else if (ctx && ctx->origin_token)
            a->statement_origin = hold(ctx->origin_token);

        if (is_class(prop->autype->context) && !prop->autype->is_static) {
            Au_t    f  = (Au_t)ctx->autype->args.origin[0];
            evar instance = (evar)u(enode, (Au_t)f);
            enode L = access(instance, string(prop->autype->ident));
            enode set = is_set((enode)instance, (evar)prop);
            // binding stamp: member defaults log as 'Class:member'
            a->bind_name   = prop->autype->ident;
            a->bind_holder = prop->autype->context;
            a->bind_au     = prop->autype->src;
            assign_if_cond((aether)a, (enode)L, set, set_if);
            a->bind_name   = null;
            a->bind_holder = null;
            a->bind_au     = null;

        } else {

            bool   is_module_mem = prop->autype->context == a->autype;

            push_tokens(a, (tokens)initializer, 0);
            enode L = (enode)prop;

            etype mdl = canonical(prop);
            e_assign(a, L, (Au)parse_expression(a, mdl, false, true), OPType__assign);

            pop_tokens(a, false);
        }
        a->statement_origin = saved_origin;
    }
}

// emit an override-member's initializer at construction time. `alloc` is the
// instance being constructed; `override_member` is the derived-class member
// with is_override=true. the assignment targets the INHERITED slot so base
// class init code reads the overridden value.
void silver_emit_override_init(silver a, enode instance, Au_t override_member) {
    if (!override_member || !override_member->is_override) return;
    enode prop = u(enode, override_member);
    if (!prop || !prop->initializer) return;
    array initializer = (array)prop->initializer;
    if (!len(initializer)) return;
    a->statement_origin = hold((token)initializer->origin[0]);
    array post_const = parse_const(a, initializer);
    subprocedure set_if = subproc(a, assign_builder, post_const);
    enode L = e_inherited_access((aether)a, instance, override_member);
    // a prop passed at the call site outranks the override's default
    enode set = is_set(instance, (evar)prop);
    assign_if_cond((aether)a, L, set, set_if);
}

// emit all override initializers for alloc before the parent init chain runs.
// called from aether_apply_overrides via the emit_overrides callback.
void silver_emit_overrides_cb(silver a, enode alloc) {
    Au_t alloc_type = alloc->autype;
    if (!alloc_type) return;
    Au_t cur = alloc_type;
    while (cur && cur != typeid(Au)) {
        if (cur->context && cur->context != typeid(Au)) {
            for (int i = 0; i < cur->members.count; i++) {
                Au_t mb = (Au_t)cur->members.origin[i];
                if (mb->member_type == AU_MEMBER_VAR && mb->is_override)
                    silver_emit_override_init(a, alloc, mb);
            }
        }
        if (cur->context == cur) break;
        cur = cur->context;
    }
}

// build attrib initializer: parse tokens and return the constructed object
enode silver_build_attrib_value(silver a, evar var) {
    if (!var || !var->initializer) return null;
    array init_tokens = (array)var->initializer;
    if (!len(init_tokens)) return null;
    push_tokens((aether)a, (tokens)init_tokens, 0);
    etype attrib_type = etype_prep((aether)a, var->autype->src);
    enode constructed = parse_expression(a, attrib_type, false, true);
    pop_tokens((aether)a, false);
    return constructed;
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

// a silver identifier that is a C/C++/ObjC++ reserved word would be illegal as a
// C struct field or function name (e.g. `volatile : bool` -> `unsigned long
// volatile:1;`). emitted code is offset/index based at the aether level, so the
// C name is free to be mangled — append '_' to dodge the collision.
static bool is_c_reserved(symbol s) {
    // only keywords that never appear as a silver *type* reference — i.e.
    // declaration specifiers, statements and operators. primitive type names
    // (bool, int, char, float, void, ...) and ObjC type words (id) are left
    // alone because the schema emits them as real types.
    static symbol kw[] = {
        "auto","break","case","const","continue","default","do","else","extern",
        "for","goto","if","inline","register","restrict","return","sizeof",
        "static","switch","typedef","volatile","while","enum","struct","union",
        "class","new","delete","this","operator","namespace","template","typename",
        "virtual","friend","using","try","catch","throw","explicit","mutable",0 };
    for (int i = 0; kw[i]; i++)
        if (strcmp(s, kw[i]) == 0) return true;
    return false;
}

static string cname(string s) {
    string u = string(chars, s->chars);
    do {
        int i = index_of(u, "-");
        if (i == -1)
            break;
        ((cstr)u->chars)[i] = '_';
    } while (1);
    if (is_c_reserved(u->chars))
        u = f(string, "%o_", u);
    return u;
}

static string method_def(enode emem) {
    string name = cname(string(emem->autype->ident));
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

// anonymous pointer types (vec T, opaque handles) have no C typedef
// name; they are single pointer slots in a C signature: emit ARef
static string carg_name(etype t) {
    Au_t au = t ? au_arg_type((Au)t->autype) : null;
    if (au && au->is_pointer && !au->ident) return string("ARef");
    return cname(cast(string, t));
}

// whether an import contributes its own generated header to our import header.
// binary/C-wrapped modules (!is_au_rt) always do. silver .ag modules only do
// when they are standalone (their own header exists) — modules that `extend`
// us are inlined into our own header and built-ins like `vec` live in Au.
static bool import_emits_header(silver a, import im) {
    if (!im->external_name) return false;
    if (!im->is_au_rt)      return true;
    return file_exists("%o/include/%o/%o", a->install, im->external_name, im->external_name);
}

// emit `#define Name_schema(...)` + `declare_struct(Name)` for every struct.
// called before the class schemas so a class can use a struct by value.
static void write_struct_schemas(silver a, FILE* module_f) {
    #undef write
    #undef line
    #define write(f,s,...) fputs(fmt(s     __VA_OPT__(,) __VA_ARGS__)->chars, f)
    #define line(f,s,...)  fputs(fmt(s"\n" __VA_OPT__(,) __VA_ARGS__)->chars, f)
    members(a->autype, m) {
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
                        concat(args, carg_name(aa));
                        arg_count++;
                    }
                    string rtype = carg_name(u(etype, f->autype->rtype));
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
                        // opaque/anonymous pointer handles lose their typedef
                        // name (type_name empty); they are single pointer slots,
                        // so emit the generic pointer ARef to match layout.
                        if (!prop_type || !prop_type->count)
                            prop_type = string("ARef");
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
    #undef write
    #undef line
}

void silver_write_header(silver a) {
    string m           = string(a->autype->ident);
    string module_sym  = silver_symbol_prefix(a);
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
    #undef write
    #undef line
    #define write(f,s,...) fputs(fmt(s     __VA_OPT__(,) __VA_ARGS__)->chars, f)
    #define line(f,s,...)  fputs(fmt(s"\n" __VA_OPT__(,) __VA_ARGS__)->chars, f)
    

    // write intern header
    line(intern_f, "#ifndef _%o_INTERN_", NAME);
    line(intern_f, "#define _%o_INTERN_", NAME);
    members(a->autype, m) {
        if (is_class(m)) {
            string n = cname(string(m->ident));
            line(intern_f,
                "#undef %o_intern", n);
            line(intern_f,
                "#define %o_intern(A,B,...) A##_schema(A,B, __VA_ARGS__)", n);
            // Native companions use this module's type globals.
            fprintf(intern_f, "#define %s_module_ %s\n",
                n->chars, module_sym->chars);
        }
    }
    line(intern_f, "#include <%o/%o>", m, m);
    line(intern_f, "#endif");

    // write public header
    line(public_f, "#ifndef _%o_PUBLIC_", NAME);
    line(public_f, "#define _%o_PUBLIC_", NAME);
    members(a->autype, m) {
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
    members(a->autype, m) {
        if (is_class(m)) {
            string n = cname(string(m->ident));
            line(init_f, "#define TC_%o(MEMBER, VALUE) ({ AF_set((u64*)&instance->af_bits, FIELD_ID(%o, MEMBER)); VALUE; })", n, n);
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
    members(a->autype, m) {
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

    // importers read this header, not intern: they need the per-type module
    // tag too, and a default linkage for the types this module owns
    fprintf(module_f, "#ifndef AU_LINK_%s\n", module_sym->chars);
    fprintf(module_f, "#ifdef _WIN32\n#define AU_LINK_%s __attribute__((dllimport))\n",
        module_sym->chars);
    fprintf(module_f, "#else\n#define AU_LINK_%s\n#endif\n#endif\n\n",
        module_sym->chars);
    members(a->autype, mt) {
        if (!(mt->traits & AU_TRAIT_ENUM) && !is_rec((Au)mt)) continue;
        string tn = cname(string(mt->ident));
        fprintf(module_f, "#ifndef %s_module_\n#define %s_module_ %s\n#endif\n",
            tn->chars, tn->chars, module_sym->chars);
    }
    fprintf(module_f, "\n");

    // write enum schemas
    members(a->autype, m) {
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

    // forward declare all classes. aliases-to-class (e.g. `alias Elements : map`)
    // still need the forward typedef so members typed by the alias name resolve
    // to an (incomplete) pointer; only their declare_class body is skipped below.
    members(a->autype, m) {
        if (is_class(m))
            line(module_f, "forward(%o)", cname(type_name((Au)m)));
    }

    // forward declare all structs (value types), then emit their full
    // definitions BEFORE the class schemas below. a class may use a struct by
    // value — as a method parameter or a field — and source order does not
    // guarantee the struct was defined first (e.g. SVGPathBuilder.cmd_quad
    // takes `affine`, but `struct affine` appears later in the same file).
    // by-value fields need the complete struct, so structs must be fully
    // declared ahead of classes.
    members(a->autype, m) {
        if (m->is_struct && !m->is_system && !m->is_schema) {
            string sn = cname(type_name((Au)m));
            line(module_f, "typedef struct _%o %o;", sn, sn);
        }
    }
    write_struct_schemas(a, module_f);

    // write class schemas
    members(a->autype, m) {
        if (is_class(m) && !m->is_alias) {
            string n   = cname(type_name((Au)m));
            array  acl = etype_class_list(u(etype, m));

            // silver user classes carry an `fbits` prefix at struct offset 0
            // (a {i64, i64} bitset tracking which props were set during
            // construction). It is added by aether.c as the leading struct
            // member when emitting the LLVM type, but ONLY for the first
            // silver class in the inheritance chain — subclasses inherit
            // the parent's fbits via the parent's section. The C struct
            // needs the same 16 bytes of space at the same place, otherwise
            // C-side `a->field` reads land at the wrong offset.
            bool is_silver_class = m->is_class && !m->is_system && !m->is_c &&
                m->module != typeid(Au)->module;
            bool parent_is_silver_class = m->context && m->context->is_class &&
                !m->context->is_system && !m->context->is_c &&
                m->context->module != typeid(Au)->module &&
                m->context != typeid(Au);
            write(module_f, "#define %o_schema(A,B,...)", n);
            members(m, mi) {
                // a member that re-declares one from a FLATTENED user-class base
                // (e.g. `ModelViewProject Basic` restating proj/model/view) reuses
                // the inherited slot in aether's layout, so emitting it again is a
                // duplicate C field. only user-class bases are flattened — Au's own
                // members live behind the `au` pointer and never collide, so the
                // walk stops before Au (otherwise Image.source would wrongly match
                // Au's `source`).
                bool inherited = false;
                for (Au_t b = m->context; b && b != m && b != typeid(Au) && is_class(b); b = b->context)
                    if (find_member(b, mi->ident, 0, 0, false)) { inherited = true; break; }
                if (inherited)
                    continue;
                line(module_f, "\\", n);
                string mn = cname(string(mi->ident));
                u8 header_access = mi->access_type ? mi->access_type : interface_public;
                // these access modifiers carry no i_prop_<access>_* macro family;
                // their extra semantics (required/read-only) are tracked on the
                // Au_t (is_required/is_context), so the C struct treats them as
                // plain public fields.
                if (header_access == interface_mutable || header_access == interface_manual ||
                    header_access == interface_context || header_access == interface_expect)
                    header_access = interface_public;
                string access_type = estring(typeid(interface), header_access);

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
                        concat(args, carg_name(aa));
                    }

                    string rtype = carg_name(u(etype, f->autype->rtype));
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
                    // members typed as an opaque C handle (e.g. VkBuffer ->
                    // VkBuffer_T*, or `new VkRenderPass[2]`) carry an anonymous
                    // pointer type whose typedef name was elided, so type_name
                    // returns empty. they are single `ptr` slots in the LLVM
                    // struct, so emit the generic pointer ARef to keep the C
                    // struct layout in sync (8 bytes). these are always intern
                    // and never referenced by C name from hand-written glue.
                    if (!prop_type || !prop_type->count)
                        prop_type = string("ARef");
                    // is_explicit_ref members ARE pointers (8 bytes). i_ref_*
                    // expands `R* N;` so declare_class_N sees a pointer slot
                    // matching LLVM's ptr emit. plain prop would emit `R N;`
                    // and inline the struct — layout drift.
                    symbol kind = mi->is_explicit_ref ? "ref" : "prop";
                    write(module_f, "M(A,B, %o,%s,%o,%o,%o%s%o)",
                        i, kind, access_type, prop_type, mn, has_meta ? "," : "", meta);
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

    // write methods (guarded for C++ — Au macros clash with stdlib)
    line(method_f, "#ifndef _%o_METHODS_", NAME);
    line(method_f, "#define _%o_METHODS_", NAME);
    line(method_f, "#ifndef __cplusplus");
    members(a->autype, m) {
        if (is_class(m) && !m->is_alias) {
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
    // C++ imports carry their own linkage; they cannot sit in extern "C"
    each(a->imports, import, im) {
        if (!im->is_cpp) continue;
        line(import_f, "#ifdef __cplusplus");
        each(im->include_paths, path, i)
            line(import_f, "#include <%o>", i);
        line(import_f, "#endif");
    }
    line(import_f, "#ifdef __cplusplus");
    line(import_f, "extern \"C\" {");
    line(import_f, "#endif");

    each(a->imports, import, im) {
        if (im->is_cpp) continue;
        each(im->include_paths, path, i)
            line(import_f, "#include <%o>", i);
    }

    line(import_f, "#include <Au/public>");
    each(a->imports, import, im) {
        if (import_emits_header(a, im))
            line(import_f, "#include <%o/public>", im->external_name);
    }
    line(import_f, "#include <Au/Au>");
    each(a->imports, import, im) {
        if (import_emits_header(a, im))
            line(import_f, "#include <%o/%o>", im->external_name, im->external_name);
    }
    line(import_f, "#include <%o/intern>",  a->name);
    line(import_f, "#include <%o/%o>",      a->name, a->name);
    line(import_f, "#include <%o/methods>", a->name);
    line(import_f, "#include <undefcpp.h>");
    //line(import_f, "#ifndef __cplusplus");
    line(import_f, "#include <Au/init>");
    each(a->imports, import, im) {
        if (import_emits_header(a, im))
            line(import_f, "#include <%o/init>", im->external_name);
    }
    line(import_f, "#include <Au/methods>");
    each(a->imports, import, im) {
        if (import_emits_header(a, im))
            line(import_f, "#include <%o/methods>", im->external_name);
    }
    // a module's own /init (the per-type X(...) prop-pair constructor macros) is
    // NOT emitted into its import: C++ TUs construct via new0(T, ...) (generic
    // _N_ARGS in macros.h), and C modules that want the X(...) form include
    // <module/init> directly. emitting it here triggers a preprocessor
    // "unterminated conditional" cascade (and the clang-22 large-macro bug).
    //line(import_f, "#include <%o/init>", a->name);
    line(import_f, "#ifdef __cplusplus");
    line(import_f, "#include <undefcpp.h>");
    // short names that only collide in a module mixing heavy c++ libraries
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

    // a remote wrapper has no body of its own — it is the call to the user impl
    if (f->has_code && (f->const_tokens || f->inline_return || f->body || f->remote_func || preamble)) {
        // each body build starts at statement level; drift never carries over
        a->expr_level = 0;
        update_current_file((aether)a, f->source_file);

        if (f->target)
            push_scope(a, (Au)f->target, 23);

        // reasonable convention for silver's debugging facility
        // if this is a standard for IDE, then we can rely on this to improve productivity
        //Au_t au_calls = def(f->autype, "sequence", AU_MEMBER_VAR, AU_TRAIT_STATIC);
        //au_calls->src = etypeid(i64)->autype;
        //evar e_calls = evar(mod, (aether)a,
        //    au, au_calls);
        
        a->last_return = null;
        if (len(f->body))
            a->statement_origin = hold((token)f->body->origin[0]);
        push_scope(a, (Au)f, 24);

#ifndef NDEBUG
        {
            etype rec = context_record(a);
            const char* cname = (rec && rec->autype && rec->autype->ident) ? rec->autype->ident : null;
            const char* fname = f->autype->ident ? f->autype->ident : "?";
            char listen_key[256];
            if (cname)
                snprintf(listen_key, sizeof(listen_key), "%s.%s", cname, fname);
            else
                snprintf(listen_key, sizeof(listen_key), "%s", fname);
            aether_clear_listen((aether)a);
            // split '--listen "Class.method [ expr ]"' into key + query tokens, once
            if (a->listen && !a->listen_key) {
                cstr br = strchr(a->listen->chars, '[');
                if (br) {
                    cstr end = strrchr(a->listen->chars, ']');
                    string key = trim(mid(a->listen, 0, (num)(br - a->listen->chars)));
                    num    qat = (num)(br + 1 - a->listen->chars);
                    num    qln = (end && end > br) ? (num)(end - br - 1)
                                                   : (num)len(a->listen) - qat;
                    string qtx = trim(mid(a->listen, qat, qln));
                    a->listen_key = (string)hold(key);
                    if (len(qtx)) {
                        array q = array(alloc, 32);
                        parse_tokens(a, (Au)qtx, q);
                        a->listen_query = (array)hold(q);
                    }
                } else
                    a->listen_key = (string)hold(a->listen);
                // 'Class.name' also arms the member-write watch (aether checks
                // VAR idents only, so function listens are unaffected)
                cstr dot = strchr(a->listen_key->chars, '.');
                if (dot) {
                    aether ae2 = (aether)a;
                    ae2->watch_class  = (string)hold(mid(a->listen_key, 0, (num)(dot - a->listen_key->chars)));
                    ae2->watch_member = (string)hold(mid(a->listen_key, (num)(dot - a->listen_key->chars) + 1,
                        (num)len(a->listen_key) - (num)(dot - a->listen_key->chars) - 1));
                }
            }
            if (a->listen_key && (strcmp(a->listen_key->chars, "*") == 0 || strcmp(a->listen_key->chars, listen_key) == 0)) {
                ((aether)a)->listen_active = true;
                ((aether)a)->listen_values = true; // values everywhere, '*' included (heavy: full value trace)
            }
        }
#endif

        if (is_lambda((Au)f))
            push_lambda_members((aether)a, f);

        // we need to initialize the schemas first, then we can actually perform user-based inits
        if (f->autype->is_mod_init) {
            // exported env vars FIRST: the expect tests the initializer
            // tail runs (and any init code) must see them set
            silver og = a->is_external ? a->is_external : a;
            exports exp = (exports)get(og->exports, (Au)string(a->name->chars));
            if (exp && exp->env_vars) {
                pairs(exp->env_vars, ev) {
                    string key = (string)ev->key;
                    string val = (string)ev->value;
                    e_setenv((aether)a, key->chars, val->chars);
                }
            }
            build_module_initializer(a, (enode)f);
        }

        // before the preamble we handle guard
        if (preamble)
            preamble((Au)f, null);

        if (f->const_tokens) {
            // join all tokens into one string, preserving line breaks and indentation
            string joined = string();
            int last_line = -1;
            for (int i = 0; i < len(f->const_tokens); i++) {
                token t = (token)f->const_tokens->origin[i];
                if (last_line < 0 || t->line != last_line) {
                    if (last_line >= 0) append(joined, "\n");
                    for (int j = 0; j < t->indent; j++)
                        append(joined, " ");
                } else if (!t->neighbor) {
                    append(joined, " ");
                }
                concat(joined, string(t->chars));
                last_line = t->line;
            }
            // allocate GLSL and construct with symbol
            etype rtype = u(etype, f->autype->rtype);
            Au_t  rtype_au = rtype->autype;
            enode meta_a_node = f->autype->meta.a ?
                e_typeid(a, u(etype, f->autype->meta.a)) : e_null(a, etypeid(Au_t));
            efunc f_alloc = (efunc)u(efunc, find_member(etypeid(Au)->autype, "alloc_new", AU_MEMBER_FUNC, 0, false));
            enode gn_src; Au gn_line, gn_seq;
            alloc_origin_args((aether)a, &gn_src, &gn_line, &gn_seq);
            enode glsl = e_fn_call(a, f_alloc, a(
                e_typeid(a, rtype), _i32(1), e_null(a, etypeid(shape)),
                meta_a_node, e_null(a, etypeid(Au)),
                (Au)gn_src, gn_line, gn_seq), false, false);
            glsl->autype = rtype_au;
            // find symbol constructor and call via e_init
            Au_t ctr_au = find_member(rtype_au, "with_symbol", AU_MEMBER_CONSTRUCT, 0, true);
            efunc ctr = ctr_au ? u(efunc, ctr_au) : null;
            const_string jstr = const_string(chars, joined->chars);
            enode sym_node = e_operand(a, (Au)jstr, etypeid(symbol));
            glsl = e_init(a, glsl, null, ctr, sym_node);
            e_fn_return(a, (Au)glsl);
        } else if (f->remote_func) {
            // the user may implement their own init/dealloc inbetween pre-amble
            // we init our own too but its name is changed on init to facilitate
            array call_args = array(alloc, 32);
            push(call_args, (Au)f->target);
            e_fn_call(a, f->remote_func, call_args, false, false);
        } else if (f->cgen) {
            array gen = generate_fn(f->cgen, f, (array)f->body);
        } else if (!f->inline_return && f->body) {
            array source_tokens = parse_const(a, (array)f->body);
            push_tokens(a, (tokens)source_tokens, 0);
            parse_statements(a);
            pop_tokens(a, false);
        }

        if (postamble)
            postamble((Au)f, null);

        if (f->inline_return) {
            if (f->inline_return)
                a->statement_origin = hold((token)f->inline_return->origin[0]);
            push_tokens(a, (tokens)f->inline_return, 0);
            etype inline_type = u(etype, f->autype->rtype);
            etype target_type = f->target ?
                canonical((etype)f->target) : null;
            bool scalar_ctr = f->autype->member_type ==
                AU_MEMBER_CONSTRUCT && target_type &&
                target_type->autype->is_scalar;
            if (scalar_ctr) {
                etype value_type = u(etype,
                    target_type->autype->src);
                enode value = parse_expression(a, value_type,
                    false, true);
                enode wrapped = e_create(a, target_type,
                    (Au)value, false);
                e_assign(a, (enode)f->target, (Au)wrapped,
                    OPType__assign);
                e_fn_return(a, null);
            } else {
                etype inline_parse_type = inline_type &&
                    canonical(inline_type)->autype->is_scalar ?
                    u(etype, canonical(inline_type)->autype->src) :
                    inline_type;
                e_fn_return(a, len(f->inline_return) ?
                    (Au)parse_expression(a, inline_parse_type,
                        false, true) : null);
            }
            pop_tokens(a, false);
        }
        
        validate(f->autype->has_return || (!f->autype->rtype || is_void(u(etype, f->autype->rtype))),
            "expected return statement in %o", f);
        
        if (is_lambda((Au)f))
            pop_scope(a);

        if (!f->inline_return && !a->last_return) {
            // tag the implicit fall-through return with the function's LAST body token,
            // not the stale statement_origin (which can still point at the first body
            // statement after a nested block like a for-loop restores it). otherwise
            // the synthetic ret carries the top-of-function line and the debugger cursor
            // jumps to the top every time a void function falls off its end.
            if (len(f->body))
                a->statement_origin = hold((token)f->body->origin[len(f->body) - 1]);
            e_fn_return(a, null);
        }

        // int len2 = len(a->lexical);
        
        pop_scope(a);
        if (f->target)
            pop_scope(a);
        aether_clear_listen((aether)a);
    }

    // safety: ensure every function with code has a terminator on its last block
    if (f->has_code)
        aether_ensure_terminator((aether)a, (enode)f);

    // inline lambdas found in this body build once it completes
    while (a->pending_lambdas && len(a->pending_lambdas)) {
        efunc lf = (efunc)hold(get(a->pending_lambdas, 0));
        remove(a->pending_lambdas, 0);
        build_fn(a, lf, null, null);
        drop((Au)lf);
    }
}

/// phase 1: parse the record body so all members are registered
static void build_record_parse(silver a, etype mrec) {
    if (mrec->user_built) return;
    mrec->user_built = true;
    mrec->parsing = true;
    verify(mrec->autype->is_class || mrec->autype->is_struct, "not a record");
    array body = mrec->body ? (array)mrec->body : array();
    push_tokens(a, (tokens)body, 0);
    push_scope(a, (Au)mrec, 25);

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

    push_scope(a, (Au)mrec, 26);
    etype_implement(mrec, false);
    if (!mrec->type_id && mrec->autype->is_class)
        implement_type_id(mrec);
    create_type_members(a, a->autype);

    // if no init, create one (but don't build it yet)
    if (mrec->autype->is_class) {
        Au_t m_init = find_member(mrec->autype, "init", AU_MEMBER_FUNC, 0, false);
        if (!m_init) {
            efunc f = function(a, mrec,
                string("init"), etypeid(none), a(mrec), AU_MEMBER_FUNC,
                AU_TRAIT_IMETHOD | AU_TRAIT_OVERRIDE, 0);
            f->has_code = true;
            f->autype->alt = cstr_copy(((string)f(string, "%o_init", symbol_name((Au)mrec)))->chars);
            etype_implement((etype)f, false);
        }
    }

    // implement all member function etypes (so they have LLVM types before phase 3)
    members(mrec->autype, m) {
        efunc n = u(efunc, m);
        if (n) implement(n, false);
    }
    pop_scope(a);
}


/// phase 3: build init and member functions (all LLVM types now complete)
static void build_record_functions(silver a, etype mrec) {
    if (mrec->autype->is_class || mrec->autype->is_struct) {
        push_scope(a, (Au)mrec, 27);
        Au_t m_init = find_member(mrec->autype, "init", AU_MEMBER_FUNC, 0, false);
        if (m_init)
            build_fn(a, u(efunc, m_init), build_init_preamble, null);
        members(mrec->autype, m) {
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
        token t = next(a, Syntax__none);
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

// log <expr>: prints under the target object's binding stamp
enode parse_log(silver a) {
    consume(a, Syntax__keyword);
    a->expr_level++;
    enode msg = parse_expression(a, etypeid(string), false, true);
    a->expr_level--;
    efunc f    = context_func(a);
    enode self = (f && f->target) ? (enode)f->target : null;
    return e_log((aether)a, self, msg);
}

enode parse_return(silver a) {
    // inline-lambda gather: rtype comes from (or is set by) this return
    etype rtype = a->gather_fn ?
        (a->gather_fn->rtype ? u(etype, a->gather_fn->rtype) : null) :
        return_type(a);
    bool  is_v  = rtype ? is_void(rtype) : !a->gather_fn;
    etype parse_type = rtype && canonical(rtype)->autype->is_scalar ?
        u(etype, canonical(rtype)->autype->src) : rtype;
    efunc ctx   = context_func(a);
    consume(a, Syntax__keyword);
    a->expr_level++;
    enode expr  = is_v ? null : parse_expression(a,
        parse_type, false, true);
    a->expr_level--;
    if (a->gather_fn && !a->gather_fn->rtype && expr)
        a->gather_fn->rtype = canonical(expr)->autype;

    Au_t au_top = top_scope(a);
    //catcher cat = u(catcher, au_top);
    verify (!au_top->has_return, "return already built at statement level");

    e_fn_return(a, (Au)expr);
    au_top->has_return = true;
    if (a->gather_fn) a->gather_fn->has_return = true;
    else if (ctx) ctx->autype->has_return = true;
    a->last_return = hold(e_noop(a, (etype)expr));
    return a->last_return;
}

catcher context_catcher(silver);
catcher context_catcher_depth(silver, int);

enode parse_expect(silver a) {
    consume(a, Syntax__none); // consume 'expect'
    a->expr_level++;
    enode cond = read_enode(a, null, false, true);
    a->expr_level--;
    return e_expect(a, cond, null);
}

enode parse_break(silver a) {
    consume(a, Syntax__none);
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
    a->last_break = hold(e_break(a, cat));
    return a->last_break;
}

enode parse_continue(silver a) {
    consume(a, Syntax__none);
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
    a->last_continue = hold(e_continue(a, cat));
    return a->last_continue;
}

// read-expression does not pass in 'expected' models, because 100% of the time we run conversion when they differ
// the idea is to know what model is returning from deeper calls
static array read_expression(silver a, etype *mdl_res, bool *is_const) {
    array exprs = array(32);
    int start = a->cursor;
    bool prev_no_build = a->no_build;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    bool use_hint = mdl_res && *mdl_res;
    enode n = parse_expression(a, use_hint ? *mdl_res : null, use_hint, true);
    if (mdl_res)
        *mdl_res = (etype)n;
    a->no_build = prev_no_build;
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
    bool prev_no_build = a->no_build;
    a->no_build = true;
    a->is_const_op = true; // set this, and it can only &= to true with const ops; any build op sets to false
    enode n = read_enode(a, null, false, true);
    a->no_build = prev_no_build;
    int e = a->cursor;
    for (int i = s; i < e; i++) {
        push(exprs, (Au)a->tokens->origin[i]);
    }
    return exprs;
}

static enode parse_func_call(silver, efunc, bool);

// this will have to adapt to parsing into a map, or parsing into a real type
// for real types, we cannot use the string as its redundant and can be reduced by the user
//

bool is_map(etype);

etype prop_value_at(etype aa, i64 index) {
    aether a = au_active(aa->mod);
    i64 prop = 0;
    members(aa->autype, m) {
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
    token pk4 = peek(a);
    validate(within_expr || read_if(a, "["), "expected [");
    token pk5 = peek(a);
    Au_t  mdl_au   = au_arg_type((Au)mdl->autype);
    bool is_fields = peek_fields(a) || inherits(mdl_au, typeid(map));
    token pk6 = peek(a);
    bool is_mdl_map = inherits(mdl_au, typeid(map));
    bool is_mdl_collective = inherits(mdl_au, typeid(collective));
    bool was_ptr = false;
    int  iter = 0;

    
    if (!is_fields && !is_mdl_map) {
        array args = array(alloc, 32);
        bool  first = true;
        do {
            if (first && ((!peek(a) && within_expr) || read_if(a, "]"))) {
                return e_create(a, mdl, (Au)null, false);
            }
            token pk3 = peek(a);
            enode expr     = parse_expression(a, null, false, true);
            bool  has_more = read_if(a, ",") != null;

            etype t0 = canonical(expr);
            etype t1 = canonical(mdl);

            if (t0 == t1) {
                if (first && !has_more) {
                    bool has_meta = mdl->autype->meta.a != null;
                    validate(!expr->from_call || has_meta, "type '%o' is redundant; expression already returns this type", mdl);
                    return expr;
                }
            }
            else
            // check if we can perform copies or referenced construction, or convert from/to cast/ctr 
            if (first && !has_more) {
                enode mcast    = castable(t0, t1);
                enode mctr     = constructable(t0, t1);
                if (mcast || mctr) {
                    validate(!within_expr || read_if(a, "]"), "expected ]");
                    return e_create(a, mdl, (Au)expr, false);
                }
            }

            push(args, (Au)expr);

            if (mdl->autype->src == typeid(collective) && !has_more) {
                verify(within_expr || read_if(a, "]"), "expected ] after collection listing");
                return e_create(a, mdl, (Au)args, false);
            }

            // trivially construct with fields
            if (!has_more) {
                validate(within_expr || read_if(a, "]"), "expected ]");
                
                // for structs, assign positional args to fields
                if (is_struct(mdl) && !inherits(mdl->autype, typeid(collective))) {
                    map props = map(assorted, true);
                    int idx = 0;
                    Au_t scan = mdl->autype;
                    while (scan && idx < len(args)) {
                        for (int mi = 0; mi < scan->members.count && idx < len(args); mi++) {
                            Au_t m = (Au_t)scan->members.origin[mi];
                            if (m->member_type == AU_MEMBER_VAR && !m->is_static) {
                                set(props, (Au)const_string(chars, m->ident), args->origin[idx]);
                                idx++;
                            }
                        }
                        if (scan->context == scan) break;
                        scan = scan->context;
                    }
                    return e_create(a, mdl, (Au)props, false);
                }
                return aether_e_create((aether)a, mdl, (Au)args, false);
            }
            first = false;
        } while (1);
    }

    validate(!is_mdl_map || is_fields, "expected fields for map");

    if (is_ptr(mdl) && is_struct(mdl->autype->src)) {
        was_ptr = true;
        mdl = resolve(mdl);
    }

    etype key = is_mdl_map ? u(etype, mdl->meta_b) : null;
    etype val = is_mdl_map ? u(etype, mdl->meta_a) : null;

    if (!key) key = etypeid(string);
    if (!val) val = etypeid(Au);

    // Lazy-initialized containers
    map   imap   = null;
    map   inames = null;
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

        a->statement_origin = hold(peek(a));

        bool auto_bind = is_fields && read_if(a, ":");
        string name = null;
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
            name = (string)read_alpha(a);
            validate(name, "expected member identifier (%o)", peek(a));
            k = (Au)const_string(chars, name->chars);
        } else if (key && key != etypeid(string)) {
            k = (Au)parse_expression(a, key, false, true);
            is_enode_key = true;
        } else if (is_mdl_map) {
            // map with string keys: read literal and convert to runtime string
            token t = peek(a);
            name = (string)read_literal(a, typeid(string));
            if (!name) {
                // a one-character literal tokenizes as unichar; a key spelled
                // 'a' is still a string key
                unichar* uc = (unichar*)read_literal(a, typeid(unichar));
                if (uc) name = unicode_char(*uc);
            }
            validate(name, "expected literal string key");
            k = (Au)e_create(a, key ? key : etypeid(string), (Au)name, false);
            is_enode_key = true;
        } else {
            token t = peek(a);
            name = (string)read_literal(a, typeid(string));
            validate(name, "expected literal string");
            k = (Au)const_string(chars, name->chars);
        }
        

        // -- Handle literal short case --
        if (iter == 0 && next_is(a, "]")) {
            // single element, return e_create(k, false)
            return e_create(a, mdl, k, false);
        }

        // -- VALUE --
        Au v = null;
        if (is_fields) {
            static int seq2 = 0;
            seq2++;
            validate(auto_bind || read_if(a, ":"), "expected : after key %o", t);
            if (auto_bind) prev(a);
            // look up the member type for this key if we know it at design time
            etype mdl_field = null;
            if (!is_mdl_map && k) {
                cstr key_name = isa(k) == typeid(const_string) ? 
                    ((const_string)k)->chars : null;
                if (key_name) {
                    Au_t mem = find_member(mdl->autype, key_name, AU_MEMBER_VAR, 0, true);
                    if (mem && mem->src) {
                        mdl_field = u(etype, mem->src);
                        if (!mdl_field)
                            mdl_field = (etype)etype_prep((aether)a, mem->src);
                        // a @T member holds a pointer: that is the target type
                        if (mem->is_explicit_ref && mdl_field && !mem->src->is_class)
                            mdl_field = (etype)pointer((aether)a, (Au)mem->src);
                        // propagate the member's meta so parse_expression sees
                        // the right element type (e.g. models: array Model → meta_a=Model).
                        // check both mem->meta_a (direct) and mem->autype->meta.a.
                        Au_t member_meta_a = mdl->meta_a ? (Au_t)mdl->meta_a : mem->meta.a;
                        Au   member_meta_b = mdl->meta_b ? (Au)  mdl->meta_b : mem->meta.b;
                        if (mdl_field && member_meta_a && !mdl_field->meta_a) {
                            mdl_field = etype(mod, (aether)a, autype, mdl_field->autype,
                                meta_a, (Au)member_meta_a, meta_b, member_meta_b);
                        }
                    }
                }
            } else if (is_mdl_map) {
                mdl_field = val; // the map's value type from meta
            }
            a->statement_origin = hold(peek(a));
            // a construction given for a prop pair stamps as 'Class:field'
            cstr sv_name   = a->bind_name;
            Au_t sv_holder = a->bind_holder;
            Au_t sv_au     = a->bind_au;
            if (!is_mdl_map && k && mdl_field && isa(k) == typeid(const_string)) {
                a->bind_name   = ((const_string)k)->chars;
                a->bind_holder = mdl->autype;
                a->bind_au     = mdl_field->autype;
            }
            v = (Au)parse_expression(a, mdl_field, true, true);
            a->bind_name   = sv_name;
            a->bind_holder = sv_holder;
            a->bind_au     = sv_au;
            // load unloaded pointer values (e.g. opaque handle from new array offset)
            if (v && instanceof(v, enode) && !is_loaded(v) && is_ptr(v))
                v = (Au)enode_value((enode)v, true);
        } else {
            a->statement_origin = hold(peek(a));
        }

        // -- Lazy allocate --
        if (!imap && is_fields)
            imap   = map(assorted, true);
        else if (!iarray && !is_fields)
            iarray = array(alloc, 32, assorted, true);
        
        // -- Insert --
        if (is_fields) {
            validate(v, "expected value after key %o", k);
            // enode keys hash alike; literal keys dedupe by text
            if (is_enode_key && name) {
                if (!inames) inames = map(assorted, true);
                validate(!get(inames, (Au)name), "duplicate key %o", name);
                set(inames, (Au)name, (Au)name);
            } else
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
    if (imap) {
        validate(within_expr || read_if(a, "]"), "expected ] after fields");
        return e_create(a, mdl, (Au)imap, false);
    }

    // validation check
    if (iarray && mdl->autype == typeid(array)) {
        shape dims = instanceof(mdl->meta_b, shape);
        int max_items = dims ? shape_total(dims) : -1;
        verify(max_items == -1 || len(iarray) <= max_items,
            "too many elements (total array size: %i, user provides %i)", max_items, len(iarray));
    }

    validate(within_expr || read_if(a, "]"), "expected ]");

    // a default is made if we give a []; if iarray is provided, e_create will iterate through members
    return e_create(a, mdl, (Au)iarray, false);
}


static bool class_inherits(etype cl, etype of_cl) {
    silver a = (silver)au_active(cl->mod);
    etype aa = canonical(of_cl);
    while (cl && cl != aa) {
        if (!cl->autype->context || cl->autype->context == cl->autype)
            break;
        cl = u(etype, cl->autype->context);
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

enode silver_parse_member_expr(silver a, enode mem, bool in_ref) { sequencer
    push_current(a);

    macro is_macro = instanceof(mem, macro);

    // an object-like macro whose body is one identifier is an alias, not an
    // invocation; windows spells setjmp that way (#define setjmp _setjmp).
    // resolving it here leaves any following [ ] to the normal call path
    if (is_macro && !is_macro->params && is_macro->def &&
        len((array)is_macro->def) == 1) {
        token id = (token)first_element((array)is_macro->def);
        Au_t  al = id ? lexical(a->lexical, id->chars) : null;
        if (al && is_func((Au)al)) {
            // the aliased function may have no etype yet; prep builds one
            etype alias = u(etype, al);
            if (!alias) alias = etype_prep((aether)a, al);
            if (alias) {
                mem      = (enode)alias;
                is_macro = null;
            }
        }
    }

    bool is_lambda_call = inherits(mem->autype, typeid(lambda));
    int indexable = !is_func((Au)mem) && !is_func_ptr((Au)mem) && !is_macro && !is_lambda_call;

    /// handle compatible indexing methods / lambda / and general pointer dereference @ index
    if (indexable && next_is(a, "[")) {
        // C arrays with elements > 0 are indexable like pointers
        bool is_indexable_ptr = is_ptr((Au)mem) || mem->autype->elements > 0 || mem->autype->is_explicit_ref;
        // a borrow of a ref member (`dst: obj.floats`) chains VAR->VAR; resolve deep
        if (!is_indexable_ptr) {
            Au_t deep = mem->autype;
            while (deep && (deep->member_type == AU_MEMBER_VAR || deep->is_alias))
                deep = deep->src;
            if (deep && (deep->is_pointer || deep->is_explicit_ref || deep->elements > 0))
                is_indexable_ptr = true;
        }
        Au_t au_rec = is_rec((Au)mem);
        etype r = au_rec ? u(etype, au_rec) : null;

        validate(is_indexable_ptr || r, "no indexing available for model %s",
                 mem->autype->ident);

        /// we must read the arguments given to the indexer
        consume(a, Syntax__none);
        array args = array(16);
        if (r && mem->target)
            push(args, (Au)mem->target);
        enode first_index = null;
        while (!next_is(a, "]")) {
            // if 2 args, the 1 is an indicator of index type 
            // (map types; collective reserves first for value)
            etype meta_key_shape = u(etype, mem->meta_b);
            enode expr = parse_expression(a, meta_key_shape, false, true);
            // coerce to string when map key type is string (e_create is identity if already string)
            if (expr && meta_key_shape == etypeid(string))
                expr = e_create(a, meta_key_shape, (Au)expr, false);
            if (!first_index && expr)
                first_index = expr;
            push(args, (Au)expr);
            validate(next_is(a, "]") || next_is(a, ","), "expected ] or , in index arguments");
            if (next_is(a, ","))
                consume(a, Syntax__none);
        }
        validate(next_is(a, "]"), "expected ] after index expression");
        consume(a, Syntax__none);

        enode index_expr = null;
        Au_t idx      = null;
        Au_t fallback = null;
        // Shaped allocations (`local T[N]`, `new T[N]`) are flagged with
        // AU_TRAIT_SHAPED on their au — those NEVER call the element type's
        // getter, even if T inherits one (e.g. `path` inherits `string`'s
        // `i32 getter(i32)`). Shaped operands always use raw pointer
        // arithmetic via the e_offset path.
        //
        // For non-shaped class instances (e.g. a `string` variable), the
        // class IS heap-allocated so is_indexable_ptr is true, but if the
        // class declares an indexer getter we want THAT — `text[i]` on a
        // string should produce a byte via the `i32 getter(i32)`, not a
        // ptr-sized GEP into text-as-array-of-pointers.
        // walk through to the underlying type — for a referenced variable,
        // mem->autype is the var (member_type=AU_MEMBER_VAR), not the type, so
        // the shape trait lives on mem->autype->src (or further down via
        // au_arg_type for aliases).
        Au_t mem_type  = au_arg_type((Au)mem);
        bool is_shaped = (mem_type && mem_type->is_shaped) || mem->autype->is_shaped;
        Au_t has_getter = (!is_shaped && au_rec) ?
            find_member(au_rec, null, AU_MEMBER_GETTER, 0, true) : null;
        // a vector of packed elements (non-class) indexes its origin
        // inline; class elements go through the getter/setter (held refs)
        Au_t velem = (au_rec && au_is_vector(au_rec)) ? vec_elem_of((etype)mem) : null;
        bool vec_packed = velem && !velem->is_class;
        if (vec_packed || (is_indexable_ptr && !inherits(au_rec, typeid(collective)) && !has_getter)) {
            r = null;
        } else if (r) {
            // select best indexer overload by matching argument type
            if (len(args) == 1) {
                enode inner      = (enode)args->origin[0];
                Au_t  inner_type = au_arg_type((Au)inner->autype);
                Au_t  scan       = r->autype;
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
            if (!idx) idx = find_member(r->autype, null, AU_MEMBER_GETTER, 0, true);

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
                index_expr = e_fn_call(a, (efunc)u(efunc, idx), a(mem, eshape), false, true);
            } else {
                validate(len(args) == 1, "index operators are single instance methods, unless a shape type is used");
                enode inner = (enode)args->origin[0];
                index_expr  = e_fn_call(a, (efunc)u(efunc, idx), a(mem, inner), false, true);
                etype rtype = u(etype, idx->rtype);

                Au info = header((Au)mem);
                // propagate design-time meta type for member access
                // convert Au -> meta_a (for collective types: array, map, etc.)
                Au_t meta_a = mem->autype->meta.a ? mem->autype->meta.a :
                    (mem->meta_a ? ((Au_t)mem->meta_a) :
                    (mem_type ? mem_type->meta.a : null));
                // a vector getter is already element-typed by e_fn_call
                if (rtype && rtype == etypeid(Au) && meta_a && index_expr->autype != meta_a) {
                    index_expr->autype = meta_a;
                    // the element's own meta (a vec T element keeps T)
                    index_expr->meta_a = (Au)meta_a->meta.a;
                    index_expr->meta_b = meta_a->meta.b;
                    // getter returns ptr-to-element for primitive types; load through
                    // the returned data pointer immediately so downstream sees a value.
                    // e_fn_call sets loaded=true for all non-struct returns, but the
                    // indexer result is inherently a pointer to the boxed value — treat
                    // it as unloaded so e_load emits the dereference.
                    if (meta_a->is_primitive && !meta_a->is_pointer) {
                        index_expr->loaded = false;
                        index_expr = e_load((aether)a, index_expr, null);
                    }
                }
            }

        } else {
            if (len(args) > 1) {
                enode eref_shape = eshape_from_indices((aether)a, args);

                // data shape from the member's meta (this can be enode or literal)
                // we need to read this from the instance
                // enode edata_shape = (enode)u(etype, mem->autype->src)->meta->origin[0];
                enode edata_shape = enode_shape(mem);

                // call runtime: shape_flat_index(data_shape, idx_shape) -> i64
                Au_t flat_fn = find_member(typeid(shape), "flat_index", AU_MEMBER_FUNC, 0, false);
                enode flat_idx = e_fn_call(a, (efunc)u(efunc, flat_fn), a(edata_shape, eref_shape), false, false);

                index_expr = e_offset(a, mem, (Au)flat_idx, in_ref);
            } else
                index_expr = e_offset(a, mem, (Au)first_index, in_ref);

            if (in_ref) {
                //if (index_expr->autype->src)
                //    index_expr->autype = index_expr->autype->src;
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

        // expand macro (guard against recursion, per parse thread)
        if (au_is_expanding(mac->autype)) {
            pop_tokens(a, true);
            return mem;
        }
        token f = (token)first_element((array)mac->def);
        array exp = macro_expand(mac, args);
        au_expanding_push(mac->autype);
        push_tokens(a, (tokens)exp, 0);
        bool cmode = a->cmode;
        a->cmode = true;
        mem = parse_expression(a, null, false, true);
        a->cmode = cmode;
        pop_tokens(a, false);
        au_expanding_pop();

    } else if (mem) {
        if (is_func((Au)mem) || is_func_ptr((Au)mem) || is_lambda_call) {
            bool is_poly = !is_cmode(a) && read_if(a, "*") != null;
            if (is_poly)
                validate(is_func((Au)mem) && !is_func_ptr((Au)mem) && !is_lambda_call,
                    "poly dispatch (*) only applies to method calls");

            if (!is_lambda_call && is_lambda((Au)mem))
                mem = parse_create_lambda(a, mem);
            else if (is_lambda_call)
                mem = parse_lambda_call( a, (efunc)mem);
            else {
                mem = parse_func_call(a, (efunc)mem, is_poly);
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
    aether a = au_active(mem->mod);
    return (mem->autype->member_type == AU_MEMBER_VAR) ?
                (etype)u(etype, mem->autype->src) :
           (mem->autype->member_type == AU_MEMBER_DECL) ?
                (etype)null : (etype)mem;
}


enode silver_parse_assignment(silver a, enode mem, OPType op_val, bool is_const) { sequencer

    
    // handle setter logic, state set by parse_member
    if (a->setter_key_tokens && a->setter_fn) {
        array  key_tokens    = a->setter_key_tokens;
        Au_t   setter        = a->setter_fn;
        a->setter_key_tokens = null;
        a->setter_fn         = null;

        push_tokens(a, (tokens)key_tokens, 0);
        // coerce to the key type, as the read path does — else they hash apart.
        // an alias (`byname : map i64 [string]`) carries meta on its type
        Au_t key_au = mem->meta_b ? (Au_t)mem->meta_b : (Au_t)mem->autype->meta.b;
        if (!key_au) {
            Au_t mt = au_arg_type((Au)mem->autype);
            if (mt) key_au = (Au_t)mt->meta.b;
        }
        enode key = parse_expression(a, u(etype, key_au), false, true);
        pop_tokens(a, false);
        enode R = parse_expression(a, null, false, true); 
        efunc fn = (efunc)u(efunc, setter);
        validate(fn, "setter function not found in registry");
        return e_fn_call(a, fn, a(mem, key, R, _i32(op_val)), false, true);
    }

    validate(isa(mem) == typeid(enode) || !mem->autype->is_const,
        "mem %s is a constant", mem->autype->ident);
    // public members are read-only outside init/construct (use mutable for writable)
    // structs are value types — always writable
    if (mem->autype->access_type == interface_public && mem->autype->context && !mem->autype->context->is_struct &&
        !(mem->autype->context->is_system || (mem->autype->context->module && mem->autype->context->module == typeid(Au)->module))) {
        efunc fn   = context_func(a);
        bool in_lifecycle = fn && (fn->autype->member_type == AU_MEMBER_CONSTRUCT ||
            (fn->autype->ident && (strcmp(fn->autype->ident, "init") == 0 ||
                               strcmp(fn->autype->ident, "dealloc") == 0 ||
                               strcmp(fn->autype->ident, "copy") == 0)));
        if (!in_lifecycle) {
            // inside any class method we allow writing to public members on
            // other objects. `mutable` is for outsiders that aren't inside a
            // method context (e.g. module-level code). `intern` remains strict.
            Au_t owner = mem->autype->context;
            bool in_method = fn != null;
            bool is_mutable = in_method || owner->is_au_native;
            validate(is_mutable,
                "cannot assign to public member '%s' outside of %s (use mutable)",
                mem->autype->ident, owner->ident);
        }
    }
    // context members were previously locked to init/construct, but bootstrapping
    // isn't always shaped that neatly — call sites need to wire owner pointers
    // back through context slots after the fact. The lifecycle gate in
    // e_assign already keeps these slots weak (no auto hold/drop), so the
    // restriction here was protecting the wrong thing.
    etype t = (etype)etype_of(mem);
    bool is_bind_ref = (op_val == OPType__bind) ? (next_is(a, "ref") || next_is(a, "@")) : false;

    // for : bind on DECL, peek at explicit type for meta propagation (don't consume)
    etype bind_type = null;
    if (op_val == OPType__bind && mem->autype->member_type == AU_MEMBER_DECL) {
        push_current(a);
        bind_type = read_etype(a, null);
        pop_tokens(a, false); // always revert — don't consume type tokens
        // an alias naming a pointer type binds as a ref, like a literal @T
        if (bind_type && (bind_type->is_explicit_ref ||
                (bind_type->autype && bind_type->autype->is_pointer)))
            is_bind_ref = true;
    }

    etype t_expect = t;
    if (next_is(a, "[") && t && mem->autype->meta.a) {
        t_expect = etype(mod, (aether)a, autype, t->autype,
            meta_a, (Au)mem->autype->meta.a, meta_b, mem->autype->meta.b);
    }



    bool is_explicit = is_explicit_ref(mem);
    bool is_compound = op_val > OPType__assign && op_val <= OPType__assign_left;

    // binding stamp: a class construction here logs as 'Holder:ident'
    bool stamp = false;
    if (op_val == OPType__bind && mem->autype->ident) {
        etype bt = bind_type ? bind_type : t;
        if (bt && is_class(bt)) {
            etype rec9     = context_record(a);
            a->bind_name   = mem->autype->ident;
            a->bind_holder = rec9 ? rec9->autype : a->autype;
            a->bind_au     = bt->autype;
            stamp = true;
        }
    }
    enode R = parse_expression(a, is_compound ? null :
        (is_explicit ? t : next_is(a, "[") ? t_expect : null),
        false, true);
    if (stamp) {
        a->bind_name   = null;
        a->bind_holder = null;
        a->bind_au     = null;
    }

    // Handle Promotion and Inference for AU_MEMBER_DECL
    if (mem->autype->member_type == AU_MEMBER_DECL) {
        // verify name is not a type alias
        array name_tokens = a(token(mem->autype->ident));
        push_tokens(a, (tokens)name_tokens, 0);
        etype name_type = read_etype(a, null);
        pop_tokens(a, false);
        validate(!name_type, "%s is a defined type", mem->autype->ident);

        // Promote the member to a variable
        Au_t ctx = top_scope(a);
        mem->autype->context = ctx;
        mem->autype->member_type = AU_MEMBER_VAR;
        Au_t rhs_type = au_arg_type((Au)R);
        // a borrow of a ref member (`dst: t.floats` where floats: ref f32) must
        // stay a ref — au_arg_type resolves to the bare element type and drops
        // the member's is_explicit_ref
        // loaded gate: an unloaded GEP (`grad: d_out_f[i]`) binds the ELEMENT,
        // not a ref — only a loaded ref value keeps ref-ness. an explicit bind
        // type (`sc : cstr p_sc` from a ref param) states a deref, not a ref
        bool rhs_ref = !bind_type &&
            ((R->is_explicit_ref && R->loaded) ||
             (R->autype && R->autype->member_type == AU_MEMBER_VAR && R->autype->is_explicit_ref));
        // decay fixed-size char arrays (char[N]) to cstr for variable inference
        // — only when the RHS is a string literal. An explicit `local u8[N]`
        // stack-array allocation should keep its array type.
        if (R->literal && rhs_type && rhs_type->elements > 0 && rhs_type->src &&
            (rhs_type->src == typeid(i8) || rhs_type->src == typeid(u8)))
            rhs_type = typeid(cstr);
        mem->autype->src = (is_bind_ref && bind_type) ? bind_type->autype :
            (bind_type && bind_type->autype->is_class && rhs_type && !rhs_type->is_class) ? bind_type->autype :
            (bind_type && rhs_type && rhs_type->elements > 0 && !bind_type->autype->elements) ? bind_type->autype :
            rhs_type;
        mem->autype->is_const = is_const;
        Au meta_a_src = bind_type && bind_type->meta_a ? bind_type->meta_a :
                        (t && t->meta_a ? t->meta_a : R->meta_a);
        Au meta_b_src = bind_type && bind_type->meta_b ? bind_type->meta_b :
                        (t && t->meta_b ? t->meta_b : R->meta_b);
        if (meta_a_src) mem->autype->meta.a = (Au_t)meta_a_src;
        if (meta_b_src) mem->autype->meta.b = meta_b_src;
        if (rhs_ref) mem->autype->is_explicit_ref = true;
        rm(a->registry, (Au)mem->autype);

        mem = (enode)evar(mod, (aether)a, autype, mem->autype,
            loaded, false, meta_a, R->meta_a, meta_b, R->meta_b,
            is_explicit_ref, is_bind_ref || rhs_ref);

        ((evar)mem)->is_local = context_func((aether)a) != null;

        // Register and allocate the variable in the backend
        etype_implement((etype)mem, false);
    }

    // callable sub: keep the stored body on the member for x[]
    if (R->body && !mem->body)
        mem->body = (tokens)hold((Au)R->body);

    enode result = e_assign(a, mem, (Au)R, op_val);
    mem->autype->is_assigned = true;
    return mem;
}

enode expr_builder(silver a, array cond_tokens, etype mdl_scope) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    a->statement_origin = hold(peek(a));
    bool use_scope = mdl_scope != null;
    enode cond_expr = parse_expression(a, mdl_scope, use_scope, true);
    validate(a->cursor == len(cond_tokens), "expected condition expression, found remaining: %o", peek(a));
    pop_tokens(a, false);
    a->expr_level--;
    return cond_expr;
}

enode ternary_expr_builder(silver a, array expr_tokens, Au unused) {
    a->expr_level++;
    push_tokens(a, (tokens)expr_tokens, 0);
    a->statement_origin = hold(peek(a));
    enode expr = parse_expression(a, null, false, true);
    validate(a->cursor == len(expr_tokens), "expected ternary expression, found remaining: %o", peek(a));
    pop_tokens(a, false);
    a->expr_level--;
    return expr;
}

enode cond_builder(silver a, array cond_tokens, Au unused) {
    a->expr_level++; // make sure we are not at 0
    push_tokens(a, (tokens)cond_tokens, 0);
    a->statement_origin = hold(peek(a));
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
    a->statement_origin = hold(peek(a));
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
    a->statement_origin = hold(peek(a));
    last = parse_statements(a);
    pop_tokens(a, false);
    a->expr_level = level;
    return last;
}

enode catch_block_builder(silver a, array block_tokens, Au context) {
    array scopes = (array)context;
    statements scope = scopes && len(scopes) ?
        (statements)scopes->origin[0] : null;
    if (scope) push_scope(a, (Au)scope, 42);
    enode last = block_builder(a, block_tokens, null);
    if (scope) pop_scope(a);
    return last;
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_builder(silver a, array expr_tokens, Au unused) {
    int level = a->expr_level;
    a->expr_level = 0;
    enode last = null;
    push_tokens(a, (tokens)expr_tokens, 0);
    a->statement_origin = hold(peek(a));
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
    a->statement_origin = hold(peek(a));
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

// negate is what separates ifndef from ifdef; the el chain is identical
enode parse_ifdef_else(silver a, bool negate) {
    bool one_truth = false;
    enode statements = null;

    verify(a->expr_level == 0, "unexpected expression level at ifdef");

    // first ifdef [cond] / ifndef [cond]
    validate(read_if(a, negate ? "ifndef" : "ifdef"), "expected ifdef or ifndef");
    validate(read_if(a, "["), "expected [ after ifdef");
    string def_name = read_alpha(a);
    validate(def_name, "expected identifier after ifdef [");
    bool cond = eval_define(a, def_name) != negate;
    validate(read_if(a, "]"), "expected ] after ifdef condition");
    array block = read_body(a);
    if (cond) {
        push_tokens(a, (tokens)block, 0);
        while (peek(a))
            statements = parse_statement(a);
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
                while (peek(a))
                    statements = parse_statement(a);
                pop_tokens(a, false);
                one_truth = true;
            }
        } else if (!one_truth) {
            push_tokens(a, (tokens)block, 0);
            while (peek(a))
                statements = parse_statement(a);
            pop_tokens(a, false);
            break; // bare el is terminal
        }
    }

    verify(a->expr_level == 0, "unexpected expression level after ifdef");
    return statements ? statements : enode(mod, (aether)a, autype, null);
}

/// parses entire chain of if, [else-if, ...] [else]
// if the cond is a constant evaluation then we do not build the condition in with LLVM build, but omit the blocks that are not used
// and only proceed
enode parse_if_else(silver a) {
    validate(read_if(a, "if") != null, "expected if");
    // anchor read_body's inline/multi-line decision to the `if` keyword's line, so a
    // same-line body (if [cond] stmt) is read as inline rather than mistaken for a block.
    a->statement_origin = hold(element(a, -1));

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
        // re-anchor to the `el` keyword's line — without this, an inline el body
        // (el stmt on the el's own line) is read against the `if` line above, wrongly
        // treated as a block, and clipped to empty by the indent check.
        a->statement_origin = hold(element(a, -1));
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
            a->statement_origin = hold(element(a, -1));
            array body = null;
            array values = array(alloc, 4);

            // read comma-separated case values
            do {
                bool is_const = false;
                etype hint = canonical(e_expr);
                etype mdl_read = hint && hint->autype->is_enum ? hint : null;
                if (first) {
                    hint_mdl = mdl_read;
                    first = false;
                } else if (hint_mdl && hint_mdl->autype != mdl_read->autype)
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
            a->statement_origin = hold(element(a, -1));
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
    statements s_members = statements(mod, (aether)a, autype, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_tokens(a, (tokens)expr_tokens, 0);
    push_scope(a, (Au)s_members, 28);
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
    if (!for_token) for_token = read_if(a, "while");
    validate(for_token != null, "expected for or while");
    a->statement_origin = hold(for_token);

    token after         = null;
    array all           = next_is(a, "[") ? read_within(a) : null; // no [...] = do-while
    bool  reverse       = read_if(a, "reverse") != null;
    enode in_expr       = read_if(a, "in") ? parse_expression(a, null, false, true) : null;
    validate(!reverse || in_expr, "reverse requires 'in' collection");
    array init_exprs    = array(alloc, 32);
    array cond_exprs    = array(alloc, 32);
    array step_exprs    = array(alloc, 32);
    evar  key_var       = null;
    evar  val_var       = null;
    bool  do_while      = false;

    statements st = new(statements, mod, (aether)a, autype, def(top_scope(a), null, AU_MEMBER_NAMESPACE, 0));
    push_scope(a, (Au)st, 29);

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
            Au_t key_mem = def_member(top_scope(a), key_name->chars, key_type->autype, AU_MEMBER_VAR, 0);
            key_var = evar(mod, (aether)a, autype, key_mem);
            etype_implement((etype)key_var, false);
        }
        
        // create value variable in current scope
        Au_t val_mem = def_member(top_scope(a), val_name->chars, val_type->autype, AU_MEMBER_VAR, 0);
        val_var = evar(mod, (aether)a, autype, val_mem);
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
        bool is_while = eq(for_token, "while");
        if (is_while)
            validate(n == 1, "while accepts only a condition");
        else
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

    a->statement_origin = hold(for_token);
    enode res = e_for(a,
        init_exprs, cond_exprs, body, step_exprs,
        build_init, build_cond, build_body, build_step,
        do_while, in_expr, val_var, key_var, reverse);
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
    Au_t  kwd_type  = next_is_keyword(a, &parse_fn);
    etype is_class  = !kwd_type ? next_is_class(a, false) : null;
    bool  is_struct  = next_is(a, "struct");
    bool  is_scalar  = next_is(a, "scalar");
    bool  is_enum    = next_is(a, "enum");
    bool  is_export  = next_is(a, "export");
    bool  is_import  = next_is(a, "import");
    bool  is_alias   = next_is(a, "alias");

    if (!kwd_type && !is_import && !is_class && !is_struct && !is_scalar && !is_enum && !is_export && !is_alias)
        return null;

    if (kwd_type) {
        validate(!access, "unexpected access level");
        struct _etype* (*parser)(silver) = (void*)parse_fn->value;
        validate(parser, "expected parse fn on member");
        return parser(a);
    }

    if (is_export) {
        validate(!access, "unexpected access level");
        return (etype)parse_export(a);
    }

    if (is_import) {
        validate(!access, "unexpected access level");
        return (etype)parse_import(a);
    }

    if (is_alias) {
        consume(a, Syntax__keyword);
        string  alias_name = read_alpha(a);
        validate(alias_name, "expected name after alias");
        // hold the name token: the cursor is well past it by the time
        // the alias type is made
        token   alias_tok = element(a, -1);
        validate(read_if(a, ":"), "expected ':' after alias name");

        bool    is_ref = read_if(a, "ref") != null || read_if(a, "@") != null;

        Au_t    top = top_scope(a);
        Au_t    alias_au = def(top, alias_name->chars, AU_MEMBER_TYPE, AU_TRAIT_ALIAS);
        stamp_source(alias_au, alias_tok);
        alias_au->is_pointer = is_ref;

        // try immediate resolution — check all type tokens are consumed
        // and no unresolved alias dependencies were hit
        token   type_start = peek(a);
        a->deferred_hit = false;
        push_current(a);
        etype   target = read_etype(a, null);
        token   after  = peek(a);
        bool    fully_parsed = target && !a->deferred_hit &&
                               (!after || !type_start || after->line != type_start->line);
        a->deferred_hit = false;
        if (fully_parsed) {
            pop_tokens(a, true);
            // alias @T: the alias IS the pointer type, not bare T
            if (alias_au->is_pointer)
                target = pointer((aether)a, (Au)target);
            alias_au->src = target->autype;
            etype_register((aether)a, (Au)alias_au, (Au)hold(target), false);
            return target;
        }

        // deferred: capture remaining type tokens for later resolution
        pop_tokens(a, false);
        array deferred_tokens = array(alloc, 16);
        token line_start = peek(a);
        while (peek(a)) {
            token t = peek(a);
            if (t->line != line_start->line) break;
            push(deferred_tokens, (Au)consume(a, Syntax__none));
        }
        if (!a->pending_aliases)
            a->pending_aliases = hold(array(alloc, 16));
        push(a->pending_aliases, (Au)alias_au);
        push(a->pending_aliases, (Au)deferred_tokens);
        return (etype)e_noop(a, null);
    }

    // `Parent Child` record form: the leading token is the parent class (color it _parent,
    // unique). the bare `class`/`struct` keyword form has no parent → leave it as keyword.
    token def_tok = peek(a);
    bool  parent_named = is_class && def_tok && !eq(def_tok, "class");
    consume(a, parent_named ? Syntax__parent : Syntax__keyword);
    string n = read_alpha(a);
    validate(n, "expected alpha-numeric identity, found %o", next(a, Syntax__none));
    // the name token is where this definition LIVES; hold it, the cursor
    // moves on long before the type is made
    token def_name_tok = element(a, -1);
    // the record's own name gets its own unique kind (_classname), distinct from a type ref.
    if (is_class || is_struct) {
        token name_tok = def_name_tok;
        if (name_tok) name_tok->syntax = Syntax__classname;
    }

    Au_t top = top_scope(a);
    etype mtop = u(etype, top);
    enode mem  = null; // = emember(mod, (aether)a, name, n, context, mtop);
    etype mdl  = null;
    array meta = null;
    if (is_class || is_struct) {
        validate(is_module(mtop),
            "expected record definition at module level");
        etype existing = elookup(n->chars);
        validate(!existing || !is_type((Au)existing),
            "type '%o' already defined%s%s", n,
            existing && existing->autype->module ? " (from " : "",
            existing && existing->autype->module ? existing->autype->module->ident : "");

        mdl = record(a, (etype)a, is_class, n,
            is_struct ? AU_TRAIT_STRUCT : AU_TRAIT_CLASS);
        if (mdl) stamp_source(mdl->autype, def_name_tok);
        if (access == interface_abstract) {
            mdl->autype->is_abstract = true;
            mdl->autype->access_type = interface_public;
        } else {
            mdl->autype->access_type = access;
        }

        // meta via < > or :
        if (read_if(a, "<") || read_if(a, ":")) {
            etype meta_a = read_etype(a, null);
            validate(meta_a, "expected meta type");
            mdl->autype->meta.a = meta_a->autype;
            if (read_if(a, "[")) {
                etype meta_b = read_etype(a, null);
                validate(meta_b, "expected meta_b type");
                mdl->autype->meta.b = (Au)meta_b->autype;
                validate(read_if(a, "]"), "expected ] after meta_b type");
            }
            read_if(a, ">"); // consume > if present (from < > syntax)
        }

        if (inherits(mdl->autype, typeid(ielement)))
            mdl->autype->is_user_init = true;

        if (mdl->autype->meta.a && inherits(mdl->autype->meta.a, typeid(live_app)))
            mdl->autype->is_app = true;

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
        mdl->autype->is_scalar    = true;
        mdl->autype->access_type  = access;
        mdl->autype->src = value_type->autype;

        // read body (cast, funcs, etc.)
        mdl->body = (tokens)read_body(a);

        // every scalar prints as its value with the unit as suffix (4.9ft)
        // unless it defines its own string cast
        bool own_string = false;
        if (mdl->body) {
            array bt = (array)mdl->body;
            for (int i = 0; i + 2 < bt->count; i++)
                if (eq((token)bt->origin[i], "cast") && eq((token)bt->origin[i + 1], "->") &&
                    eq((token)bt->origin[i + 2], "string")) { own_string = true; break; }
        }
        if (!own_string) {
            string src = f(string,
                "\n    cast -> string\n        v : %s [ a ]\n        return '{v}%o'\n",
                value_type->autype->ident, n);
            string keep = a->source_raw;
            array  gen  = array(alloc, 32);
            parse_tokens(a, (Au)src, gen);
            a->source_raw = keep;
            if (!mdl->body) mdl->body = (tokens)array(alloc, 32);
            // generated tokens report at the scalar's own line
            token ref = element(a, -1);
            each(gen, token, t) {
                if (ref) {
                    t->source = (string)hold(ref->source);
                    t->line  += ref->line;   // keep the block's own line breaks
                }
                push((array)mdl->body, (Au)t);
            }
        }

    } else if (is_enum) {
        etype store = null;

        // storage type after :
        if (read_if(a, ":")) {
            store = instanceof(read_etype(a, null), etype);
            validate(store, "invalid storage type after :");
        } else
            store = etypeid(i32);

        // meta types in [ ]
        Au meta_a = null, meta_b = null;
        if (read_if(a, "[")) {
            a->etype_level++;
            etype mt = read_etype(a, null);
            if (mt) meta_a = (Au)mt->autype;
            if (read_if(a, ",")) {
                etype mt2 = read_etype(a, null);
                if (mt2) meta_b = (Au)mt2->autype;
            }
            a->etype_level--;
            validate(read_if(a, "]"), "expected ] after enum meta types");
        }

        array enum_body = read_body(a);
        validate(len(enum_body), "expected body for enum %o", n);

        Au_t enum_au = def(top_scope(a),
            n->chars, AU_MEMBER_TYPE, AU_TRAIT_ENUM);
        stamp_source(enum_au, def_name_tok);
        enum_au->access_type = access;
        enum_au->src = store->autype;
        if (meta_a) enum_au->meta.a = (Au_t)meta_a;
        if (meta_b) enum_au->meta.b = meta_b;
        mdl = etype(mod, (aether)a, autype, enum_au);

        push_tokens(a, (tokens)enum_body, 0);
        push_scope(a, (Au)mdl, 30);
        bool is_float_enum = enum_au->src->traits & AU_TRAIT_REALISTIC;
        validate(enum_au->src->is_integral || is_float_enum,
                 "enumeration must be based on integral or float types (i32 default)");
        i64 value = 0;
        f64 fvalue = 0.0;
        while (true) {
            token e = next(a, Syntax__none);
            if  (!e) break;
            Au    v = null;

            bool is_explicit = read_if(a, ":") != null;

            if (is_explicit) {
                enode n = parse_expression(a, store, false, true);
                Au lit = n ? literal_value(n,
                    isa(n->literal) == typeid(shape) ? typeid(i64) : isa(n->literal)) : null;
                if (is_float_enum) {
                    verify(n && lit, "expected float literal");
                    v = lit;
                    Au_t lt = isa(lit);
                    if (lt == typeid(f64))       fvalue = *(f64*)lit;
                    else if (lt == typeid(f32))  fvalue = *(f32*)lit;
                    else if (lt->is_integral) {
                        u8 sp[64];
                        memcpy(sp, lit, lt->abi_size / 8);
                        fvalue = (f64)*(i64*)sp;
                    }
                    if (enum_au->src == typeid(f32)) {
                        f32 fv = (f32)fvalue;
                        v = primitive(typeid(f32), &fv);
                    } else
                        v = primitive(typeid(f64), &fvalue);
                } else {
                    verify(n && ((Au_t)isa(lit))->is_integral,
                        "expected integral literal");
                    v = lit;
                    u8 sp[64];
                    memcpy(sp, lit, ((Au_t)isa(lit))->abi_size / 8);
                    value = *(i64*)sp;
                }
            } else {
                if (is_float_enum) {
                    if (enum_au->src == typeid(f32)) {
                        f32 fv = (f32)fvalue;
                        v = primitive(typeid(f32), &fv);
                    } else
                        v = primitive(typeid(f64), &fvalue);
                } else
                    v = primitive(store->autype, &value);
            }

            Au_t enum_v         = def_enum_value(enum_au, e->chars, v);
            enum_v->src         = enum_au;
            enum_v->value       = (object)hold(v);
            enum_v->member_type = AU_MEMBER_ENUMV;
            enum_v->is_const    = true;

            enode enum_node = enode(
                mod, (aether)a, autype, enum_v, literal, v);
            etype_register((aether)a, (Au)enum_v, (Au)hold(enum_node), false);
            implement(mdl, false);
            if (is_float_enum)
                fvalue += 1.0;
            else
                value += 1;
        }
        pop_scope(a);
        pop_tokens(a, false);

        etype_implement(mdl, false);
        create_type_members(a, a->autype);

    } else {
        error("unknown error");
    }

    validate(mdl && len(n),
             "name required for model: %s", isa(mdl)->ident);

    if (!get(a->registry, (Au)mdl->autype))
    etype_register((aether)a, (Au)mdl->autype, (Au)hold(mdl), false);
    return mdl;
}

#else

// orbiter could build silver in this way from .c
// importing 
int main(int argc, cstrs argv) {
    setvbuf(stdout, NULL, _IONBF, 0);   // TEMP
#ifdef _WIN32
    // there is no rpath here: a module we dlopen finds its own dependencies
    // (opencv, OpenEXR, ...) through PATH, so put our directories first
    {
        cstr prev = getenv("PATH");
        char buf[8192];
        snprintf(buf, sizeof(buf), "%s/install/bin;%s/install/build;%s",
            SILVER, SILVER, prev ? prev : "");
        setenv("PATH", buf, 1);
    }
#endif
    engage(argv);
    silver a = silver(argv);
    // a successful live build execvp's the host and never returns here; if we DID
    // return, a->error tells us the build failed → exit non-zero so a caller (the
    // live-host's rebuild_blocking, or a shell) sees the failure instead of looping.
    return (a && a->error) ? 1 : 0;
}

#endif

#ifdef BUILD_LIBRARY
define_class(chatgpt, codegen)
define_class(claude,  codegen)
define_class(gemini,  codegen)
define_class(silver, aether)
define_class(exports, Au)
define_class(Device,  Au)

initializer(silver_module)
#endif


AU_EXPORT void silver_module_anchor(void) { }
