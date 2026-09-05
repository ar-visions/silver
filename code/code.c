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
        kinds[i] = kind;
    }
    // DEFINITIONS. a function name with its body on the line (or the brace
    // opening the next), a #define, a struct/enum/union with a body, and
    // the name a typedef line gives; a prototype or a call is neither
    for (int i = 0; i < n; i++) {
        rec* r = &rs[i];
        if (r->klen != 14 || memcmp(r->kind, "raw_identifier", 14) != 0) continue;
        rec* prev = (i > 0) ? &rs[i - 1] : NULL;
        rec* next = (i + 1 < n) ? &rs[i + 1] : NULL;
        int  same_prev = prev && prev->line == r->line;
        #define KIND_IS(rr, k) ((rr)->klen == (int)strlen(k) && memcmp((rr)->kind, k, (rr)->klen) == 0)
        #define SP_IS(rr, k)   ((rr)->splen == (int)strlen(k) && memcmp((rr)->sp, k, (rr)->splen) == 0)
        if (kinds[i] == K_FUNCTION && same_prev &&
            (KIND_IS(prev, "raw_identifier") || KIND_IS(prev, "star") || KIND_IS(prev, "amp"))) {
            int body = 0, j = i + 1;
            for (; j < n && rs[j].line == r->line; j++) {
                if (KIND_IS(&rs[j], "l_brace")) { body = 1; break; }
                if (KIND_IS(&rs[j], "semi"))    { body = 0; j = n; break; }
            }
            if (!body && j < n && rs[j].line == r->line + 1 && KIND_IS(&rs[j], "l_brace")) body = 1;
            if (body) def_put(r->sp, r->splen, r->line);
        }
        else if (same_prev && KIND_IS(prev, "raw_identifier") && SP_IS(prev, "define") &&
                 i > 1 && KIND_IS(&rs[i - 2], "hash"))
            def_put(r->sp, r->splen, r->line);
        else if (same_prev && KIND_IS(prev, "raw_identifier") &&
                 (SP_IS(prev, "struct") || SP_IS(prev, "enum") || SP_IS(prev, "union")) &&
                 next && KIND_IS(next, "l_brace"))
            def_put(r->sp, r->splen, r->line);
        else if (next && KIND_IS(next, "semi") && (i + 2 >= n || rs[i + 2].line != r->line)) {
            // the name a typedef line ends with; a member inside its braces is not it
            int j = i;
            while (j > 0 && rs[j - 1].line == r->line) j--;
            if (KIND_IS(&rs[j], "raw_identifier") && SP_IS(&rs[j], "typedef"))
                def_put(r->sp, r->splen, r->line);
        }
        #undef KIND_IS
        #undef SP_IS
    }
    for (int i = 0; i < n; i++) {
        rec* r = &rs[i];
        int  k = kinds[i];
        int  d = (k == K_IDENT || k == K_FUNCTION || k == K_TYPE) ? def_line(r->sp, r->splen) : 0;
        if (d) put(r->line, r->col, r->splen, k, d);
        else   put_span(r->line, r->col, r->sp, r->splen, k);
    }
    free(kinds);
    free(rs);
    free(out);
}

// python's tokenize for the tokens, its ast for what defines a name: each
// module, function and class is a scope holding what it binds, and a name
// resolves from the innermost scope holding its line outward. printed as
// line col len kind decl
static const char* py_script =
"import tokenize,keyword,sys,ast\n"
"f=sys.argv[1]\n"
"src=open(f,encoding='utf-8',errors='replace').read()\n"
"lines=src.split('\\n')\n"
"toks=list(tokenize.tokenize(open(f,'rb').readline))\n"
"scopes=[]\n"
"def walk(node,si):\n"
"    b=scopes[si][2]\n"
"    if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef,ast.ClassDef)):\n"
"        b.setdefault(node.name,node.lineno)\n"
"        scopes.append((node.lineno,getattr(node,'end_lineno',node.lineno),{},si)); ci=len(scopes)-1\n"
"        if not isinstance(node,ast.ClassDef):\n"
"            a=node.args\n"
"            for x in a.posonlyargs+a.args+a.kwonlyargs+([a.vararg] if a.vararg else [])+([a.kwarg] if a.kwarg else []):\n"
"                scopes[ci][2].setdefault(x.arg,x.lineno)\n"
"        for c in ast.iter_child_nodes(node): walk(c,ci)\n"
"        return\n"
"    if isinstance(node,ast.Name) and isinstance(node.ctx,ast.Store): b.setdefault(node.id,node.lineno)\n"
"    elif isinstance(node,ast.ExceptHandler) and node.name: b.setdefault(node.name,node.lineno)\n"
"    elif isinstance(node,(ast.Import,ast.ImportFrom)):\n"
"        for al in node.names:\n"
"            nm=al.asname or al.name.split('.')[0]\n"
"            if nm!='*': b.setdefault(nm,node.lineno)\n"
"    for c in ast.iter_child_nodes(node): walk(c,si)\n"
"try:\n"
"    tree=ast.parse(src)\n"
"    scopes.append((1,len(lines)+1,{},-1))\n"
"    walk(tree,0)\n"
"except Exception:\n"
"    scopes=[]\n"
"def resolve(name,line):\n"
"    best=-1\n"
"    for i,(s,e,b,p) in enumerate(scopes):\n"
"        if s<=line<=e and (best<0 or (e-s)<(scopes[best][1]-scopes[best][0])): best=i\n"
"    while best>=0:\n"
"        s,e,b,p=scopes[best]\n"
"        if name in b: return b[name]\n"
"        best=p\n"
"    return 0\n"
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
"        d=resolve(s,t.start[0]) if t.type==tokenize.NAME else 0\n"
"        (l1,c1),(l2,c2)=t.start,t.end\n"
"        if l1==l2: print(l1,c1,c2-c1,k,d)\n"
"        else:\n"
"            print(l1,c1,len(lines[l1-1])-c1,k,0)\n"
"            for L in range(l1+1,l2): print(L,0,len(lines[L-1]),k,0)\n"
"            print(l2,0,c2,k,0)\n"
"    if t.type not in (tokenize.NL,tokenize.NEWLINE,tokenize.INDENT,tokenize.DEDENT,tokenize.COMMENT): prev=t\n";

static void parse_py(const char* path) {
    char cmd[8192];
    snprintf(cmd, sizeof cmd, "python3 -c \"%s\" '%s' 2>/dev/null", py_script, path);
    char* out = run(cmd);
    if (!out) return;
    for (const char* p = out; *p; ) {
        int line, col, len, kind, decl;
        if (sscanf(p, "%d %d %d %d %d", &line, &col, &len, &kind, &decl) == 5) put(line, col, len, kind, decl);
        const char* nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    free(out);
}

// any other text file: a plain lexer. words, numbers, quoted strings,
// #, // and /* */ comments, and the rest as punctuation -- enough to read
// by, with no claim about the language
static void parse_generic(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (8 << 20)) { fclose(f); return; }
    char* s = malloc(n + 1);
    if (fread(s, 1, n, f) != (size_t)n) { free(s); fclose(f); return; }
    fclose(f);
    s[n] = 0;
    if (memchr(s, 0, n < 4096 ? n : 4096)) { free(s); return; }     // binary: nothing to colour
    int line = 1, col = 0, in_block = 0;
    for (long i = 0; i < n; ) {
        char c = s[i];
        if (c == '\n') { line++; col = 0; i++; continue; }
        long j = i;
        int  kind = K_PUNCT;
        if (in_block) {
            while (j < n && s[j] != '\n' && !(s[j] == '*' && j + 1 < n && s[j + 1] == '/')) j++;
            if (j < n && s[j] == '*') { j += 2; in_block = 0; }
            kind = K_COMMENT;
        }
        else if (c == ' ' || c == '\t' || c == '\r') { i++; col++; continue; }
        else if (c == '#' || (c == '/' && i + 1 < n && s[i + 1] == '/')) {
            while (j < n && s[j] != '\n') j++;
            kind = K_COMMENT;
        }
        else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            in_block = 1;
            j = i + 2;
            while (j < n && s[j] != '\n' && !(s[j] == '*' && j + 1 < n && s[j + 1] == '/')) j++;
            if (j < n && s[j] == '*') { j += 2; in_block = 0; }
            kind = K_COMMENT;
        }
        else if (c == '"' || c == '\'') {
            j = i + 1;
            while (j < n && s[j] != c && s[j] != '\n') { if (s[j] == '\\' && j + 1 < n) j++; j++; }
            if (j < n && s[j] == c) j++;
            kind = K_STR;
        }
        else if (c >= '0' && c <= '9') {
            while (j < n && ((s[j] >= '0' && s[j] <= '9') || (s[j] >= 'a' && s[j] <= 'z') ||
                             (s[j] >= 'A' && s[j] <= 'Z') || s[j] == '.' || s[j] == '_')) j++;
            kind = K_NUMBER;
        }
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80) {
            while (j < n && ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') ||
                             (s[j] >= '0' && s[j] <= '9') || s[j] == '_' || (unsigned char)s[j] >= 0x80)) j++;
            kind = K_IDENT;
        }
        else { j = i + 1; kind = strchr("()[]{};,.", c) ? K_PUNCT : K_OP; }
        put(line, col, (int)(j - i), kind, 0);
        col += (int)(j - i);
        i = j;
    }
    free(s);
}

// tokens for path, by its language; the count, then fields by index
int code_parse(const char* path, const char* lang) {
    ntok = 0;
    if      (strcmp(lang, "py") == 0) parse_py(path);
    else if (strcmp(lang, "c")  == 0) parse_c(path);
    else                              parse_generic(path);
    return ntok;
}

int code_token(int i, int field) {
    if (i < 0 || i >= ntok) return 0;
    ctok* t = &toks[i];
    return field == 0 ? t->line : field == 1 ? t->col : field == 2 ? t->len : field == 3 ? t->kind : t->decl;
}
