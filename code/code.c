// code — the language tools do the lexing: clang's raw token dump for c
// and c++, python's own tokenize for python. this side runs the tool and
// keeps what came back as flat tokens the ag side reads a field at a time
#define _GNU_SOURCE   // popen under -std=c99
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Syntax kinds, as the Au enum numbers them
enum { K_NONE, K_KEYWORD, K_TYPE, K_IDENT, K_FUNCTION, K_PARAM, K_PROPERTY, K_NUMBER,
       K_STR, K_CHARACTER, K_BOOLEAN, K_COMMENT, K_OP, K_PUNCT, K_NAMESPACE, K_CONSTANT,
       K_USERTOKEN, K_PARENT, K_CLASSNAME, K_META };

// decl: the line in this file where the token's name is defined, 0 if none
typedef struct { int line, col, len, kind, decl; } ctok;
static ctok* toks;
static int   ntok, tokcap;

static void put(int line, int col, int len, int kind, int decl) {
    if (len <= 0) return;
    if (ntok == tokcap) {
        tokcap = tokcap ? tokcap * 2 : 1024;
        toks   = realloc(toks, tokcap * sizeof(ctok));
    }
    toks[ntok++] = (ctok){ line, col, len, kind, decl };
}

// a spelling that spans lines lands as one piece per line
static void put_span(int line, int col, const char* sp, int n, int kind) {
    int start = 0;
    for (int i = 0; i <= n; i++)
        if (i == n || sp[i] == '\n') {
            put(line, col, i - start, kind, 0);
            line++; col = 0; start = i + 1;
        }
}

// the names this file defines, first definition wins: a later use of the
// name gets that line, which is what a cmd-click goes to
typedef struct { const char* s; int n, line; } cdef;
static cdef* defs;
static int   ndef, defcap;

static int def_line(const char* s, int n) {
    for (int i = 0; i < ndef; i++)
        if (defs[i].n == n && memcmp(defs[i].s, s, n) == 0) return defs[i].line;
    return 0;
}

static void def_put(const char* s, int n, int line) {
    if (n <= 0 || def_line(s, n)) return;
    if (ndef == defcap) {
        defcap = defcap ? defcap * 2 : 256;
        defs   = realloc(defs, defcap * sizeof(cdef));
    }
    defs[ndef++] = (cdef){ s, n, line };
}

static char* run(const char* cmd) {
    FILE* f = popen(cmd, "r");
    if (!f) return NULL;
	// this is a comment
    size_t cap = 1 << 16, n = 0;
    char*  buf = malloc(cap);
    for (;;) {
        if (n + 4096 > cap) { cap *= 2; buf = realloc(buf, cap); }
        size_t r = fread(buf + n, 1, cap - n - 1, f);
        if (!r) break;
        n += r;
    }
    pclose(f);
    buf[n] = 0;
    return buf;
}

static const char* c_keywords[] = {
    "auto","break","case","const","continue","default","do","else","enum","extern","for",
    "goto","if","inline","register","restrict","return","sizeof","static","struct","switch",
    "typedef","union","volatile","while","_Atomic","_Static_assert","_Alignas","_Alignof",
    "class","public","private","protected","virtual","override","final","template","typename",
    "namespace","using","new","delete","this","try","catch","throw","operator","constexpr",
    "consteval","constinit","explicit","friend","mutable","noexcept","static_assert",
    "static_cast","dynamic_cast","reinterpret_cast","const_cast","decltype","export","import",
    "module","co_await","co_return","co_yield","concept","requires","alignas","alignof",
    "typeid","and","or","not","xor","asm","__asm__","__attribute__","__typeof__","typeof", NULL };
static const char* c_types[] = {
    "void","char","short","int","long","float","double","signed","unsigned","bool","_Bool",
    "size_t","ssize_t","wchar_t","char8_t","char16_t","char32_t","int8_t","int16_t","int32_t",
    "int64_t","uint8_t","uint16_t","uint32_t","uint64_t","intptr_t","uintptr_t","ptrdiff_t",
    "FILE","va_list", NULL };
static const char* c_punct[] = {
    "l_paren","r_paren","l_brace","r_brace","l_square","r_square","semi","comma", NULL };

static int in_table(const char** t, const char* s, int n) {
    for (int i = 0; t[i]; i++)
        if ((int)strlen(t[i]) == n && memcmp(t[i], s, n) == 0) return 1;
    return 0;
}

// one raw token record: kind 'spelling'<tabs/flags>Loc=<file:line:col>\n
// the spelling can hold quotes, tabs and newlines, so the record is cut
// at its Loc and the spelling closed at the last quote before it
typedef struct { const char* kind; int klen; const char* sp; int splen; int line, col; } rec;

static const char* next_rec(const char* p, rec* r) {
    const char* loc = strstr(p, "\tLoc=<");
    if (!loc) return NULL;
    const char* end = strchr(loc, '\n');
    if (!end) end = loc + strlen(loc);
    r->kind = p;
    const char* sp = strchr(p, ' ');
    if (!sp || sp > loc) return NULL;
    r->klen = (int)(sp - p);
    // a diagnostic line (path:line:col: ...) is not a record: skip the line
    if (memchr(p, ':', r->klen)) {
        const char* nl = strchr(p, '\n');
        r->klen = 0;
        return nl ? nl + 1 : NULL;
    }
    sp += 2;                                   // past "space quote"
    const char* q = loc;
    while (q > sp && *q != '\'') q--;
    r->sp    = sp;
    r->splen = (int)(q - sp);
    // <file:line:col>: the numbers sit after the last two colons
    const char* gt = end;
    while (gt > loc && *gt != '>') gt--;
    const char* c2 = gt; while (c2 > loc && *c2 != ':') c2--;
    const char* c1 = c2 - 1; while (c1 > loc && *c1 != ':') c1--;
    r->line = atoi(c1 + 1);
    r->col  = atoi(c2 + 1) - 1;
    return (*end == '\n') ? end + 1 : end;
}

static void parse_c(const char* path) {
    char cmd[4096];
    // the dump goes to stderr, and diagnostics with it: both end up in the pipe,
    // but a diagnostic line has no Loc=< record shape and is passed over
    snprintf(cmd, sizeof cmd, "clang -fsyntax-only -Xclang -dump-raw-tokens '%s' 2>&1", path);
    char* out = run(cmd);
    if (!out) return;
    // every record first, so an identifier can look at its neighbours
    int  cap = 4096, n = 0;
    rec* rs  = malloc(cap * sizeof(rec));
    for (const char* p = out; p && *p; ) {
        rec r;
        const char* nx = next_rec(p, &r);
        if (!nx) break;
        p = nx;
        if (r.klen == 0) continue;                                          // not a record
        if (r.klen == 7 && memcmp(r.kind, "unknown", 7) == 0) continue;   // whitespace
        if (r.klen == 3 && memcmp(r.kind, "eof", 3) == 0) continue;
        if (n == cap) { cap *= 2; rs = realloc(rs, cap * sizeof(rec)); }
        rs[n++] = r;
    }
    int  directive_line = 0;                   // a #include's target reads as a string
    int  include_line   = 0;
    int* kinds = malloc((n ? n : 1) * sizeof(int));
    ndef = 0;
    for (int i = 0; i < n; i++) {
        rec* r = &rs[i];
        int kind = K_OP;
        #define IS(k) (r->klen == (int)strlen(k) && memcmp(r->kind, k, r->klen) == 0)
        if (IS("raw_identifier")) {
            rec* prev = (i > 0) ? &rs[i - 1] : NULL;
            rec* next = (i + 1 < n) ? &rs[i + 1] : NULL;
            if (prev && prev->line == r->line && prev->klen == 4 && memcmp(prev->kind, "hash", 4) == 0) {
                kind = K_META;
                directive_line = r->line;
                if (r->splen == 7 && memcmp(r->sp, "include", 7) == 0) include_line = r->line;
            }
            else if (include_line == r->line) kind = K_STR;
            else if (in_table(c_keywords, r->sp, r->splen)) kind = K_KEYWORD;
            else if (in_table(c_types, r->sp, r->splen))    kind = K_TYPE;
            else if ((r->splen == 4 && !memcmp(r->sp, "true", 4)) || (r->splen == 5 && !memcmp(r->sp, "false", 5))) kind = K_BOOLEAN;
            else if ((r->splen == 4 && !memcmp(r->sp, "NULL", 4)) || (r->splen == 7 && !memcmp(r->sp, "nullptr", 7))) kind = K_CONSTANT;
            else if (next && next->klen == 7 && memcmp(next->kind, "l_paren", 7) == 0) kind = K_FUNCTION;
            else if (prev && prev->klen == 14 && memcmp(prev->kind, "raw_identifier", 14) == 0 &&
                     (in_table((const char*[]){ "struct","class","enum","union","typename", NULL }, prev->sp, prev->splen)))
                kind = K_TYPE;
            else if (r->splen > 2 && memcmp(r->sp + r->splen - 2, "_t", 2) == 0) kind = K_TYPE;
            else kind = K_IDENT;
        }
        else if (IS("numeric_constant")) kind = K_NUMBER;
        else if (IS("comment"))          kind = K_COMMENT;
        else if (IS("hash"))             kind = K_META;
        else if (strstr(r->kind, "string_literal") && strstr(r->kind, "string_literal") < r->kind + r->klen) kind = K_STR;
        else if (strstr(r->kind, "char_constant")  && strstr(r->kind, "char_constant")  < r->kind + r->klen) kind = K_CHARACTER;
        else if (include_line == r->line) kind = K_STR;
        else {
            kind = K_OP;
            for (int k = 0; c_punct[k]; k++)
                if (IS(c_punct[k])) { kind = K_PUNCT; break; }
        }
        #undef IS
        put_span(r->line, r->col, r->sp, r->splen, kind);
    }
    free(rs);
    free(out);
}

// python's tokenize, classified in the script, printed as line col len kind
static const char* py_script =
"import tokenize,keyword,sys\n"
"f=sys.argv[1]\n"
"lines=open(f,encoding='utf-8',errors='replace').read().split('\\n')\n"
"toks=list(tokenize.tokenize(open(f,'rb').readline))\n"
"prev=None\n"
"for i,t in enumerate(toks):\n"
"    k=None; s=t.string\n"
"    if t.type==tokenize.NAME:\n"
"        if s in ('True','False'): k=10\n"
"        elif s=='None': k=15\n"
"        elif keyword.iskeyword(s): k=1\n"
"        elif prev and prev.string=='def': k=4\n"
"        elif prev and prev.string=='class': k=18\n"
"        elif prev and prev.string=='@': k=19\n"
"        elif s in ('int','str','float','bool','list','dict','set','tuple','bytes','object','type'): k=2\n"
"        elif i+1<len(toks) and toks[i+1].string=='(': k=4\n"
"        elif s=='self': k=5\n"
"        else: k=3\n"
"    elif t.type==tokenize.NUMBER: k=7\n"
"    elif t.type==tokenize.STRING: k=8\n"
"    elif t.type==tokenize.COMMENT: k=11\n"
"    elif t.type==tokenize.OP: k=13 if s in '()[]{},;:.' else 12\n"
"    if k is not None:\n"
"        (l1,c1),(l2,c2)=t.start,t.end\n"
"        if l1==l2: print(l1,c1,c2-c1,k)\n"
"        else:\n"
"            print(l1,c1,len(lines[l1-1])-c1,k)\n"
"            for L in range(l1+1,l2): print(L,0,len(lines[L-1]),k)\n"
"            print(l2,0,c2,k)\n"
"    if t.type not in (tokenize.NL,tokenize.NEWLINE,tokenize.INDENT,tokenize.DEDENT,tokenize.COMMENT): prev=t\n";

static void parse_py(const char* path) {
    char cmd[8192];
    snprintf(cmd, sizeof cmd, "python3 -c \"%s\" '%s' 2>/dev/null", py_script, path);
    char* out = run(cmd);
    if (!out) return;
    for (const char* p = out; *p; ) {
        int line, col, len, kind;
        if (sscanf(p, "%d %d %d %d", &line, &col, &len, &kind) == 4) put(line, col, len, kind);
        const char* nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    free(out);
}

// tokens for path, by its language; the count, then fields by index
int code_parse(const char* path, const char* lang) {
    ntok = 0;
    if (strcmp(lang, "py") == 0) parse_py(path);
    else                         parse_c(path);
    return ntok;
}

int code_token(int i, int field) {
    if (i < 0 || i >= ntok) return 0;
    ctok* t = &toks[i];
    return field == 0 ? t->line : field == 1 ? t->col : field == 2 ? t->len : t->kind;
}
