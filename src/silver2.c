// silver2: a small .ag compiler — tokens, syntax tree, LLVM emission
// through a module, clang links
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

typedef struct Token {
    char*   text;
    int     line, col, indent;
    char    kind;
    bool    no_space;
    int64_t int_value;
    double  float_value;
    bool    is_float;
    long    dims[8];
    int     dim_count;
    bool    xshape;
} Token;
typedef struct {
    char* data;
    int   count, capacity;
} Buf;
typedef struct {
    void** data;
    int    count, capacity;
} List;

static char* format(const char* spec, ...) {
    char    text[65536];
    va_list args;
    va_start(args, spec);
    vsnprintf(text, sizeof text, spec, args);
    va_end(args);
    return strdup(text);
}
static void append(Buf* buf, const char* spec, ...) {
    char    tmp[65536];
    va_list args;
    va_start(args, spec);
    int length = vsnprintf(tmp, sizeof tmp, spec, args);
    va_end(args);
    if (buf->count + length + 1 > buf->capacity)
        buf->data =
            realloc(buf->data, buf->capacity =
                                   (buf->count + length + 1) * 2 + 256);
    memcpy(buf->data + buf->count, tmp, length + 1);
    buf->count += length;
}
static void list_push(List* list, void* item) {
    if (list->count == list->capacity)
        list->data = realloc(
            list->data,
            sizeof(void*) * (list->capacity = list->capacity * 2 + 8));
    list->data[list->count++] = item;
}
static bool same(const char* left, const char* right) {
    return left && right && !strcmp(left, right);
}
static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char* text = malloc(size + 1);
    size       = fread(text, 1, size, file);
    text[size] = 0;
    fclose(file);
    return text;
}

// ----------------------------------------------------------------
// tokens
static const char* syms[] = {
    "...", "..<", "<=>", ">>=", "<<=", "->", "::", "<>", "..",
    "==",  "!=",  ">=",  "<=",  "+=",  "-=", "*=", "/=", "|=",
    "&=",  "^=",  "%=",  ">>",  "<<",  "||", "&&", "??", ".",
    "{",   "}",   "$",   ",",   "<",   ">",  "(",  ")",  "!",
    "[",   "]",   "/",   "+",   "*",   ":",  "=",  "~",  "@",
    "|",   "&",   "^",   "?",   "`",   "-",  "%",  0};
static const char* special = ".{}$,<>()![]/+*:=#~@|&^?`";

static bool parse_shape(const char* source, long length, long* cursor,
                        Token* token) {
    long head = *cursor + (source[*cursor] == '-'), stop = *cursor;
    bool has_digit = false, has_x = false;
    if (source[head] == '0' && head + 2 < length &&
        ((source[head + 1] == 'x' && isxdigit(source[head + 2])) ||
         (source[head + 1] == 'b' &&
          (source[head + 2] == '0' || source[head + 2] == '1')) ||
         (source[head + 1] == 'o' && source[head + 2] >= '0' &&
          source[head + 2] <= '7')))
        return false;
    for (long k = *cursor; k < length; k++) {
        char ch    = source[k];
        bool start = k == *cursor;
        if ((ch == '-' && start) || (ch == 'x' && !start) ||
            isdigit(ch)) {
            has_digit |= isdigit(ch) > 0;
            has_x |= ch == 'x';
            stop = k;
            continue;
        }
        if (ch == '.' || ch == 'e' || ch == 'E') return false;
        break;
    }
    if (!has_digit) return false;
    long span        = stop - *cursor + 1;
    token->text      = strndup(source + *cursor, span);
    token->kind      = 'n';
    token->xshape    = has_x;
    token->dim_count = 0;
    for (char* digits = token->text; *digits;) {
        token->dims[token->dim_count++] = strtol(digits, &digits, 10);
        if (*digits == 'x') digits++;
    }
    token->int_value = token->dims[0];
    *cursor += span;
    return true;
}
static bool parse_numeric(const char* source, long length, long* cursor,
                          Token* token) {
    long at = *cursor, start = at;
    bool is_float = false;
    if (source[at] == '-') {
        if (at + 1 >= length || !isdigit(source[at + 1])) return false;
        at++;
    }
    if (!isdigit(source[at])) return false;
    if (source[at] == '0' && at + 1 < length &&
        (source[at + 1] == 'x' || source[at + 1] == 'b' ||
         source[at + 1] == 'o')) {
        char base = source[at + 1];
        at += 2;
        while (at < length &&
               (base == 'x' ? isxdigit(source[at])
                : base == 'b'
                    ? (source[at] == '0' || source[at] == '1')
                    : (source[at] >= '0' && source[at] <= '7')))
            at++;
        if (base == 'x' && at < length && source[at] == '.') {
            at++;
            while (at < length && isxdigit(source[at])) at++;
            if (at < length &&
                (source[at] == 'p' || source[at] == 'P')) {
                at++;
                if (source[at] == '+' || source[at] == '-') at++;
                while (at < length && isdigit(source[at])) at++;
            }
            is_float = true;
        }
        token->text     = strndup(source + start, at - start);
        token->kind     = 'n';
        token->is_float = is_float;
        if (is_float) token->float_value = strtod(token->text, 0);
        else
            token->int_value =
                strtoll(
                    token->text + (base == 'x' ? 0 : 2) +
                        (start < at && source[start] == '-' ? 1 : 0),
                    0,
                    base == 'x'   ? 16
                    : base == 'b' ? 2
                                  : 8) *
                (source[start] == '-' ? -1 : 1);
        *cursor = at;
        return true;
    }
    while (at < length && isdigit(source[at])) at++;
    if (at < length && source[at] == '.' && at + 1 < length &&
        isdigit(source[at + 1])) {
        is_float = true;
        at++;
        while (at < length && isdigit(source[at])) at++;
    }
    if (at < length && (source[at] == 'e' || source[at] == 'E')) {
        is_float = true;
        at++;
        if (source[at] == '+' || source[at] == '-') at++;
        while (at < length && isdigit(source[at])) at++;
    }
    long sfx = at;
    while (sfx < length && strchr("fFlLuU", source[sfx])) sfx++;
    if (sfx > at && (sfx >= length || !isalnum(source[sfx]))) at = sfx;
    token->text     = strndup(source + start, at - start);
    token->kind     = 'n';
    token->is_float = is_float;
    if (is_float) token->float_value = strtod(token->text, 0);
    else token->int_value = strtoll(token->text, 0, 10);
    *cursor = at;
    return true;
}
static bool char_lit(const char* chars, long length, int64_t* out) {
    if (length == 3) {
        *out = (uint8_t)chars[1];
        return true;
    }
    if (length == 4 && chars[1] == '\\') {
        const char* escapes = "n\nr\rt\t0\0\\\\''";
        for (int k = 0; k < 12; k += 2)
            if (escapes[k] == chars[2]) {
                *out = escapes[k + 1];
                return true;
            }
        *out = (uint8_t)chars[2];
        return true;
    }
    if (length >= 5 && length <= 12 && chars[1] == '\\' &&
        chars[2] == 'x') {
        *out = strtol(strndup(chars + 3, length - 4), 0, 16);
        return true;
    }
    return false;
}
static Token* tokenize(const char* source, int* count) {
    List list   = {0};
    long length = strlen(source), at = 0, line_start = 0;
    int  line = 1, indent = 0, brace_depth = 0;
    while (at < length) {
        char ch = source[at];
        if (isspace(ch)) {
            if (ch == '\n') {
                line++;
                line_start = at + 1;
                indent     = 0;
                at++;
                while (at < length &&
                       (source[at] == ' ' || source[at] == '\t')) {
                    indent += source[at] == '\t' ? 4 : 1;
                    at++;
                }
            } else at++;
            continue;
        }
        if (ch == '#' && !brace_depth) {
            if (source[at + 1] == '#') {
                at += 2;
                while (at < length &&
                       !(source[at] == '#' && source[at + 1] == '#')) {
                    if (source[at] == '\n') line++;
                    at++;
                }
                at += 2;
            } else
                while (at < length && source[at] != '\n') at++;
            continue;
        }
        Token* token    = calloc(1, sizeof(Token));
        token->line     = line;
        token->indent   = indent;
        token->col      = at - line_start;
        token->no_space = at > 0 && !isspace(source[at - 1]);
        Token* prev_tok = list.count ? list.data[list.count - 1] : 0;
        const char* sym = 0;
        int         best_len = 0;
        for (int k = 0; syms[k]; k++) {
            int sym_len = strlen(syms[k]);
            if (sym_len > best_len &&
                !strncmp(source + at, syms[k], sym_len)) {
                sym      = syms[k];
                best_len = sym_len;
            }
        }
        if (sym && best_len == 1 && *sym == '-' &&
            isdigit(source[at + 1]) &&
            (!prev_tok ||
             prev_tok->col + (int)strlen(prev_tok->text) < token->col ||
             same(prev_tok->text, "[") || same(prev_tok->text, "(") ||
             same(prev_tok->text, ",")))
            sym = 0;
        if (sym) {
            if (*sym == '{') brace_depth++;
            else if (*sym == '}' && brace_depth) brace_depth--;
            token->text = strdup(sym);
            token->kind =
                strchr("[](){},:;.", *sym) && best_len == 1 ? 'p' : 'o';
            list_push(&list, token);
            at += best_len;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            long start = at++;
            int  depth = 0;
            while (at < length) {
                char inner = source[at];
                if (inner == '\\') {
                    at += 2;
                    continue;
                }
                if (inner == '{') {
                    if (source[at + 1] == '{') {
                        at += 2;
                        continue;
                    }
                    if (source[at + 1] == ch) {
                        at++;
                        continue;
                    }
                    depth++;
                    at++;
                    continue;
                }
                if (inner == '}') {
                    if (source[at + 1] == '}') {
                        at += 2;
                        continue;
                    }
                    if (depth) depth--;
                    at++;
                    continue;
                }
                if (depth && (inner == '"' || inner == '\'')) {
                    char quote = inner;
                    at++;
                    while (at < length && source[at] != quote)
                        at += source[at] == '\\' ? 2 : 1;
                    at++;
                    continue;
                }
                if (inner == ch && !depth) break;
                at++;
            }
            at++;
            int64_t char_value;
            if (ch == '\'' &&
                char_lit(source + start, at - start, &char_value)) {
                token->kind      = 'u';
                token->int_value = char_value;
                token->text      = strndup(source + start, at - start);
            } else {
                token->kind = ch == '\'' ? 's' : 'c';
                token->text =
                    strndup(source + start + 1, at - start - 2);
            }
            list_push(&list, token);
            continue;
        }
        long start   = at;
        bool numeric = ch == '-' || isdigit(ch);
        if (numeric && (parse_shape(source, length, &at, token) ||
                        parse_numeric(source, length, &at, token))) {
            list_push(&list, token);
            continue;
        }
        int  dot_count = 0;
        bool last_dash = false;
        while (at < length) {
            char letter = source[at];
            bool sep    = letter == '.';
            if (sep) dot_count++;
            bool continues = (sep && dot_count <= 1) || isdigit(letter);
            if ((!numeric || !continues) &&
                (isspace(letter) || strchr(special, letter))) {
                if (last_dash && at - start > 1 &&
                    source[at - 2] != '-')
                    at--;
                break;
            }
            at++;
            last_dash = letter == '-';
        }
        token->text = strndup(source + start, at - start);
        token->kind = 'a';
        list_push(&list, token);
    }
    *count = list.count;
    return (Token*)({
        Token* copy = calloc(list.count + 1, sizeof(Token));
        for (int k = 0; k < list.count; k++)
            copy[k] = *(Token*)list.data[k];
        copy;
    });
}

// ----------------------------------------------------------------
// syntax tree
enum {
    N_NUM,
    N_FLT,
    N_STR,
    N_CSTR,
    N_CHAR,
    N_IDENT,
    N_BIN,
    N_UN,
    N_CALL,
    N_MEMBER,
    N_ARROW,
    N_TERN,
    N_TUPLE,
    N_RANGE,
    N_TYPE,
    N_CONSTRUCT,
    N_CONV,
    N_CAST,
    N_TYPEID,
    N_SIZEOF,
    N_LAMBDA,
    N_BIND,
    N_ASMX,
    N_RAW,
    N_EXPECTX,
    N_LIST,
    N_PROP,
    N_SHORTPROP,
    N_ARGS,
    N_TYPEMEMBER,
    N_FMT,
    N_ORRET,
    N_MODULEID,
    N_SCALARLIT,
    N_SUPER,
    N_TYPEREF,
    S_DECL,
    S_ASSIGN,
    S_EXPR,
    S_RETURN,
    S_BREAK,
    S_CONTINUE,
    S_IF,
    S_IFDEF,
    S_WHILE,
    S_FOR,
    S_DOWHILE,
    S_FORIN,
    S_SWITCH,
    S_CASE,
    S_TRY,
    S_THROW,
    S_EXPECT,
    S_FAULT,
    S_ASM,
    S_NOOP,
    S_BLOCK,
    S_PUTS,
    S_LOG,
    S_CONSTRUCT,
    S_CHECK,
    D_ENUM,
    D_SCALAR,
    D_ALIAS,
    D_STRUCT,
    D_CLASS,
    D_FUNC,
    D_VAR,
    D_IMPORT,
    D_APP,
    D_MEMBER,
    D_PARAM,
    D_ENUMV,
    D_CASTFN,
    D_OPFN,
    D_CTOR,
    D_GETTER,
    D_SETTER,
    D_BROKEN
};
typedef struct Type Type;
typedef struct Node {
    int           kind;
    char*         text;
    struct Node** kids;
    int           count;
    int           line;
    Type*         type;
    int           flag;
    Token*        token;
    char*         raw;
    char*         error;
} Node;

#include <dlfcn.h>
#include <stddef.h>
#include <llvm-c/Core.h>
#define LLVM_VERSION_MAJOR 22
typedef int                i32;
typedef unsigned           u32;
typedef long long          i64;
typedef unsigned long long u64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef char*              cstr;
typedef void               none;
typedef struct _Au**       ARef;
typedef struct _Au*        Au;
#include "object.h"
#define HDR                                                            \
    ((long)offsetof(struct _object, f)) // the mock carries one trailing
                                        // field the live header lacks
#include <llvm-c/Core.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/DebugInfo.h>
typedef LLVMValueRef      Value;
typedef LLVMTypeRef       LType;
typedef LLVMBasicBlockRef Block;
typedef struct {
    Value value;
    Type* type;
} Val;
typedef struct Ir {
    LLVMContextRef context;
    LLVMModuleRef  module;
    LLVMBuilderRef build, init_build;
    LType          i1, i8, i32, i64, f32, f64, bf16, ptr, void_type;
    Value          func;
} Ir;
typedef struct CT {
    char        kind;
    int         bits;
    bool        is_signed, is_ref, is_const;
    struct CT*  elem;
    struct CD*  record;
    const char* spell;
} CT;
typedef struct CD {
    int          kind;
    const char*  name;
    const char*  qualified;
    const char*  symbol;
    CT*          result;
    CT**         params;
    int          param_count;
    struct CD*   owner;
    long         size, offset;
    int          vtable_index;
    bool         is_static, is_virtual, is_variadic, is_inline;
    long         int_value;
    double       float_value;
    struct CD**  members;
    int          member_count;
    const char*  body;
    const char** param_names;
    int          param_name_count;
} CD;
enum {
    CD_FUNC = 1,
    CD_RECORD,
    CD_FIELD,
    CD_METHOD,
    CD_CTOR,
    CD_CONV,
    CD_MACRO,
    CD_TEMPLATE,
    CD_NAMESPACE,
    CD_ENUMCONST,
    CD_FTEMPLATE,
    CD_TYPEDEF
};
CD** c_import(const char* tu, const char** args, int nargs, int* count,
              const char* driver);
typedef struct Var {
    char*       name;
    Type*       type;
    Value       address;
    struct Var* next;
} Var;
typedef struct Scope {
    Var*          vars;
    struct Scope* parent;
} Scope;
typedef struct Loop {
    Block break_block, continue_block;
} Loop;
typedef struct Fn {
    Type* class_type;
    Type* result;
    bool  infer_result;
    Loop  loops[64];
    int   loop_count;
    Value self;
    Value result_ptr;
    Value timing_start; // --timing: the entry timestamp, settled at
                        // every return
    int             timing_id;
    LLVMMetadataRef di_scope; // the DWARF subprogram: statements take
                              // their lines here
} Fn;

// the command line as the compiler reads it: the same struct the silver
// object fills (see src/silver2)
#ifndef _SILVER2_OPTIONS_
#define _SILVER2_OPTIONS_
struct DeviceInfo {
    const char *alias, *host, *platform, *root, *run, *stop, *sysroot,
        *exclude, *fetch, *debugger;
};
struct SilverOptions {
    const char*        module;
    const char*        platform;
    struct DeviceInfo* device;
    bool test, lib, coverage, timing, release, rsync, lldb, build;
    const char** extend_paths;
    int          extend_count;
    bool         clean;
};
typedef struct DeviceInfo    DeviceInfo;
typedef struct SilverOptions SilverOptions;
#endif
typedef struct SilverState { // everything the compiler holds while it
                             // runs; the silver object owns one
    Token*      tokens;
    int         pos, token_count;
    jmp_buf     parse_jump;
    char*       parse_message;
    const char* cur_file;
    char*       cur_src;
    List        type_names, local_names;
    int         local_mark;
    List        prescanned;
    List        types;
    List        share_dirs; // name, dir pairs: ours and every import's;
                            // each folder joins the root share
    Buf import_ledger; // the imports this build made, for a cached run
    Type*       basic_types[32];  // the primitive types, one each
    List        au_seen, cd_seen; // records already turned into types
    Node*       MOD;
    const char* modname;
    jmp_buf     emit_jump;
    char*       emit_message;
    bool        emit_guarded;
    void*       au_lib;
    List        imported, imported_names;
    CD**        cdecls;
    int         ncdecls;
    Ir          ir;
    LLVMTargetDataRef target_data;
    Fn*               cur_fn;
    List              pending, ft_slots;
    int               lam_counter, seq_counter;
    Value             modrec;
    Buf         c_includes, c_insts, c_flags, extra_srcs, link_flags;
    List        c_args, c_probed, au_mods;
    const char* main_dir;
    const char* self_exe;
    bool        lib_mode;
    List        extends;
    List        test_names, test_skips;
    List        struct_rets;
    bool        coverage, timing;
    bool        clean; // the output tree goes first: every product and
                // checkout is made again   // the two report flags; the
                // Au runtime prints the report at exit and on ctrl-c
    bool release; // -O2, the expect tests must pass, then the distro
                  // packages
    bool        build, rsync, lldb, cross;
    const char* platform; // the target's name; "native" or empty means
                          // this machine
    const char* target_dir; // platform/<device alias or platform>: its
                            // sysroot, runtime and build
    const char* sysroot;
    const char* triple;
    const char* tools; // silver's own clang, lld, llvm tools
    const char* silver_root;
    const char*
          out_dir; // .silver2, or .silver2/<target> for a device build
    char* tgt;     // -target/--sysroot for clang
    char* ldld;    // -fuse-ld=lld and the target's link quirks
    struct {
        const char *alias, *host, *platform, *root, *run, *stop,
            *sysroot, *exclude, *fetch, *debugger;
    } device;
    LLVMDIBuilderRef di;
    LLVMMetadataRef  di_unit, di_file;
    List             di_types,
        di_records; // DWARF: silver types, and each class's own Au_t
                    // record with its function table
    LLVMMetadataRef di_au_t, di_object;
    int             cur_line;
    Value           cov_probes, cov_seq, timings, func_names;
    int             probe_count, func_count;
    List            func_name_list;
} SilverState;
static SilverState* S;
static const char*  cxx_abi(
     void); // the target's C++ header flags, defined with the devices
static bool  target_is_mobile(void);
static Node* new_node(int kind, const char* text, int line) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->text = text ? strdup(text) : 0;
    node->line = line;
    return node;
}
static Node* add_kid(Node* parent, Node* child) {
    parent->kids =
        realloc(parent->kids, sizeof(Node*) * (parent->count + 1));
    parent->kids[parent->count++] = child;
    return parent;
}
static Node* new_node2(int kind, const char* text, int line, Node* left,
                       Node* right) {
    Node* node = new_node(kind, text, line);
    add_kid(node, left);
    add_kid(node, right);
    return node;
}
#define kid(nd_, i) ((nd_)->count > (i) ? (nd_)->kids[i] : 0)

static char* line_text(int line) {
    const char* cursor = S->cur_src;
    for (int at_line = 1; at_line < line && cursor; at_line++) {
        cursor = strchr(cursor, '\n');
        if (cursor) cursor++;
    }
    if (!cursor) return strdup("");
    const char* end = strchr(cursor, '\n');
    char* text = strndup(cursor, end ? end - cursor : strlen(cursor));
    char* hash = strchr(text, '#');
    if (hash) *hash = 0;
    while (*text == ' ') text++;
    return text;
}
static const char* prim_names[] = {
    "i8",     "i16",    "i32",    "i64",      "u8",     "u16",
    "u32",    "u64",    "f32",    "f64",      "bool",   "none",
    "void",   "cstr",   "string", "path",     "object", "any",
    "Au",     "Au_t",   "handle", "unichar",  "half",   "shape",
    "tokens", "async",  "vector", "sz",       "num",    "real",
    "symbol", "hook",   "signed", "unsigned", "short",  "int",
    "long",   "float",  "double", "char",     "vec",    "map",
    "@",      "lambda", "local",  "struct",   "inlay",  "new",
    "ARef",   0};
static void parse_fail(const char* spec, ...) {
    char    text[1024];
    va_list args;
    va_start(args, spec);
    vsnprintf(text, sizeof text, spec, args);
    va_end(args);
    S->parse_message = format(
        "%s:%d: %s (at '%s')", S->cur_file,
        S->pos < S->token_count ? S->tokens[S->pos].line : 0, text,
        S->pos < S->token_count ? S->tokens[S->pos].text : "eof");
    longjmp(S->parse_jump, 1);
}
static Token* cur_tok(void) {
    return S->pos < S->token_count ? &S->tokens[S->pos] : 0;
}
static bool tok_is(const char* text) {
    return S->pos < S->token_count &&
           same(S->tokens[S->pos].text, text);
}
static bool tok_at_is(int ahead, const char* text) {
    return S->pos + ahead < S->token_count &&
           same(S->tokens[S->pos + ahead].text, text);
}
static bool same_line(void) {
    return S->pos < S->token_count && S->pos > 0 &&
           S->tokens[S->pos].line == S->tokens[S->pos - 1].line;
}
static bool new_line(void) {
    return S->pos >= S->token_count || S->pos == 0 ||
           S->tokens[S->pos].line != S->tokens[S->pos - 1].line;
}
static Token* next_tok(void) {
    if (S->pos >= S->token_count) parse_fail("unexpected end");
    return &S->tokens[S->pos++];
}
static Token* expect_tok(const char* text) {
    if (!tok_is(text)) parse_fail("expected '%s'", text);
    return next_tok();
}
static bool accept(const char* text) {
    if (tok_is(text)) {
        S->pos++;
        return true;
    }
    return false;
}
static bool c_type_name(const char* name);
static bool imported_type_name(const char* name);
static bool is_type_name(const char* name) {
    for (int i = 0; i < S->local_names.count; i++)
        if (same(S->local_names.data[i], name)) return false;
    if (c_type_name(name) || imported_type_name(name)) return true;
    for (int i = 0; prim_names[i]; i++)
        if (same(prim_names[i], name)) return true;
    for (int i = 0; i < S->type_names.count; i++)
        if (same(S->type_names.data[i], name)) return true;
    return false;
}
static bool starts_type(void) {
    Token* token = cur_tok();
    if (!token) return false;
    if (same(token->text, "@") || same(token->text, "lambda"))
        return S->pos + 1 < S->token_count &&
               S->tokens[S->pos + 1].kind == 'a' &&
               is_type_name(S->tokens[S->pos + 1].text);
    return token->kind == 'a' && is_type_name(token->text);
}
static bool starts_primary(void) {
    Token* token = cur_tok();
    if (!token) return false;
    if (token->kind == 'a' || token->kind == 'n' ||
        token->kind == 's' || token->kind == 'c' || token->kind == 'u')
        return true;
    return same(token->text, "@") || same(token->text, "(") ||
           same(token->text, "!") || same(token->text, "~");
}

static Node* parse_expr(void);
static Node* parse_postfix(void);
static Node* parse_type(void);
static Node* parse_block(int indent);
static Node* parse_stmt(void);

static Node*
parse_args(void) { // after '[' ; entries: expr | name: expr | :name
    Node* args = new_node(N_ARGS, 0, cur_tok() ? cur_tok()->line : 0);
    while (!tok_is("]")) {
        if (tok_is(":") && S->pos + 1 < S->token_count &&
            S->tokens[S->pos + 1].kind == 'a') {
            S->pos++;
            add_kid(args, new_node(N_SHORTPROP, next_tok()->text,
                                   S->tokens[S->pos - 1].line));
        } else {
            Node* expr = parse_expr();
            if (accept(":")) {
                Node* value = parse_expr();
                add_kid(args, new_node2(N_PROP,
                                        expr->kind == N_IDENT ||
                                                expr->kind == N_TYPEREF
                                            ? expr->text
                                            : 0,
                                        expr->line, expr, value));
            } else add_kid(args, expr);
        }
        accept(",");
    }
    expect_tok("]");
    return args;
}
static Node* parse_params(void) { // after '['
    Node* params = new_node(N_ARGS, 0, cur_tok()->line);
    while (!tok_is("]")) {
        if (accept("::")) {
            params->flag = params->count;
            continue;
        } // flag: index where contextual args begin
        accept("inlay");
        Node* param = new_node(D_PARAM, 0, cur_tok()->line);
        if (cur_tok()->kind == 'a' && tok_at_is(1, ":")) {
            param->text = next_tok()->text;
            S->pos++;
        }
        add_kid(param, parse_type());
        add_kid(params, param);
        accept(",");
    }
    expect_tok("]");
    params->flag = params->flag ? params->flag : params->count;
    return params;
}
static Node* parse_type(void) {
    Token* token     = next_tok();
    Node*  type_node = new_node(N_TYPE, token->text, token->line);
    if (same(token->text, "struct") || same(token->text, "inlay")) {
        free(type_node->text);
        type_node->text = strdup(next_tok()->text);
    } else if (same(token->text, "vec") || same(token->text, "local") ||
               same(token->text, "new")) {
        if (same(token->text, "new")) {
            free(type_node->text);
            type_node->text = strdup("vec");
        }
        add_kid(type_node, parse_type());
        if (tok_is("[") && same_line()) {
            S->pos++;
            add_kid(type_node, parse_args());
            type_node->flag = 1;
        }
    } else if (same(token->text, "map")) {
        add_kid(type_node, parse_type());
        expect_tok("[");
        add_kid(type_node, parse_type());
        expect_tok("]");
    } else if (same(token->text,
                    "@")) { // @T: a reference; Au has no ref type
        free(type_node->text);
        type_node->text = strdup("@");
        add_kid(type_node, parse_type());
    } else if (same(token->text, "lambda")) {
        add_kid(type_node, parse_type());
        expect_tok("[");
        add_kid(type_node, parse_params());
    } else if (same(token->text, "signed") ||
               same(token->text, "unsigned")) {
        bool        is_unsigned = token->text[0] == 'u';
        const char* width       = "32";
        if (tok_is("char")) {
            S->pos++;
            width = "8";
        } else if (tok_is("short")) {
            S->pos++;
            width = "16";
        } else if (tok_is("int")) S->pos++;
        else if (tok_is("long")) {
            S->pos++;
            accept("long");
            width = "64";
        }
        free(type_node->text);
        type_node->text =
            format("%c%s", is_unsigned ? 'u' : 'i', width);
    } else if (same(token->text, "long")) {
        accept("long");
        free(type_node->text);
        type_node->text = strdup("i64");
    } else if (same(token->text, "int"))
        type_node->text = strdup("i32");
    else if (same(token->text, "short"))
        type_node->text = strdup("i16");
    else if (same(token->text, "char")) type_node->text = strdup("i8");
    else if (same(token->text, "float"))
        type_node->text = strdup("f32");
    else if (same(token->text, "double"))
        type_node->text = strdup("f64");
    else if (token->kind != 'a') parse_fail("type expected");
    while (tok_is("::") && cur_tok()->no_space) {
        S->pos++;
        type_node->text =
            format("%s::%s", type_node->text, next_tok()->text);
    } // C++ namespaces and template arguments ride in the type name
    if (tok_is("<") && cur_tok()->no_space) {
        Buf buf = {0};
        S->pos++;
        int depth = 1;
        append(&buf, "<");
        while (depth) {
            Token* inner = next_tok();
            if (same(inner->text, "<")) depth++;
            if (same(inner->text, ">") && !--depth) break;
            append(&buf, "%s", inner->text);
        }
        append(&buf, ">");
        type_node->text = format("%s%s", type_node->text, buf.data);
    }
    if (tok_is("*") && cur_tok()->no_space &&
        !(S->pos + 1 < S->token_count &&
          S->tokens[S->pos + 1].line == cur_tok()->line &&
          (S->tokens[S->pos + 1].kind == 'a' ||
           S->tokens[S->pos + 1].kind == 'n' ||
           same(S->tokens[S->pos + 1].text, "(")))) {
        S->pos++;
        type_node->flag = 2;
    }
    return type_node;
}
static Node*
parse_body(int indent) { // [ expr ] | { raw } | indented block
    if (tok_is("[") && same_line()) {
        S->pos++;
        Node* args  = parse_args();
        Node* block = new_node(S_BLOCK, 0, args->line);
        Node* ret   = new_node(S_RETURN, 0, args->line);
        add_kid(ret, args);
        add_kid(block, ret);
        return block;
    }
    if (tok_is("{") && same_line()) {
        Buf buf = {0};
        S->pos++;
        while (!tok_is("}")) {
            append(&buf, "%s%s", buf.count ? " " : "",
                   next_tok()->text);
        }
        S->pos++;
        Node* raw   = new_node(N_RAW, buf.data ? buf.data : "",
                               S->tokens[S->pos - 1].line);
        Node* block = new_node(S_BLOCK, 0, raw->line);
        Node* ret   = new_node(S_RETURN, 0, raw->line);
        add_kid(ret, raw);
        add_kid(block, ret);
        return block;
    }
    return parse_block(indent);
}
static Node* parse_lambda_inline(int indent) {
    Node* lambda = new_node(N_LAMBDA, 0, S->tokens[S->pos - 1].line);
    expect_tok("[");
    add_kid(lambda, parse_params());
    add_kid(lambda, accept("->") ? parse_type() : 0);
    add_kid(lambda, parse_body(indent));
    return lambda;
}
static Node* parse_asm_lines(int indent, Node* into) {
    while (S->pos < S->token_count && cur_tok()->indent > indent &&
           new_line()) {
        int   line      = cur_tok()->line;
        Buf   buf       = {0};
        Node* line_node = new_node(N_RAW, 0, line);
        while (S->pos < S->token_count && cur_tok()->line == line) {
            Token* token = next_tok();
            append(&buf, "%s%s",
                   buf.count && !(token->no_space) ? " " : "",
                   token->text);
            if (token->kind == 'a')
                add_kid(line_node,
                        new_node(N_IDENT, token->text, line));
        }
        line_node->text = buf.data;
        add_kid(into, line_node);
    }
    return into;
}
static Node* parse_primary(void) {
    Token* token = cur_tok();
    if (!token) parse_fail("expression expected");
    int line = token->line;
    if (token->kind == 'n') {
        S->pos++;
        Node* node  = token->is_float
                          ? new_node(N_FLT, token->text, line)
                          : new_node(N_NUM, token->text, line);
        node->token = token;
        if (S->pos < S->token_count && cur_tok()->kind == 'a' &&
            cur_tok()->no_space && is_type_name(cur_tok()->text)) {
            Node* scalar_node =
                new_node(N_SCALARLIT, next_tok()->text, line);
            add_kid(scalar_node, node);
            return scalar_node;
        }
        return node;
    }
    if (token->kind == 's') {
        S->pos++;
        return new_node(N_STR, token->text, line);
    }
    if (token->kind == 'c') {
        S->pos++;
        return new_node(N_CSTR, token->text, line);
    }
    if (token->kind == 'u') {
        S->pos++;
        Node* node  = new_node(N_CHAR, token->text, line);
        node->token = token;
        return node;
    }
    if (accept("(")) {
        Node* expr = parse_expr();
        if (tok_is(",")) {
            Node* tuple = new_node(N_TUPLE, 0, line);
            add_kid(tuple, expr);
            while (accept(",")) add_kid(tuple, parse_expr());
            expect_tok(")");
            return tuple;
        }
        if (tok_is("...") || tok_is("..<")) {
            Node* range = new_node(N_RANGE, next_tok()->text, line);
            add_kid(range, expr);
            add_kid(range, parse_expr());
            expect_tok(")");
            return range;
        }
        expect_tok(")");
        Node* group = new_node(N_LIST, 0, line);
        group->flag = 1;
        add_kid(group, expr);
        return group;
    }
    if (accept("[")) {
        Node* list = parse_args();
        list->kind = N_LIST;
        return list;
    }
    if (accept("!") || accept("not")) {
        Node* node = new_node(N_UN, "!", line);
        add_kid(node, parse_postfix());
        return node;
    }
    if (tok_is("-") && !starts_type()) {
        S->pos++;
        Node* node = new_node(N_UN, "-", line);
        add_kid(node, parse_postfix());
        return node;
    }
    if (accept("~")) {
        Node* node = new_node(N_UN, "~", line);
        add_kid(node, parse_postfix());
        return node;
    }
    if (accept("*")) {
        Node* node = new_node(N_UN, "*", line);
        add_kid(node, parse_postfix());
        return node;
    } /* C-style dereference, from expanded macros */
    if (tok_is("@") && !(S->pos + 1 < S->token_count &&
                         S->tokens[S->pos + 1].kind == 'a' &&
                         is_type_name(S->tokens[S->pos + 1].text))) {
        S->pos++;
        Node* node = new_node(N_UN, "@", line);
        add_kid(node, parse_postfix());
        return node;
    }
    if (accept("cast")) {
        Node* node = new_node(N_CAST, 0, line);
        add_kid(node, parse_type());
        expect_tok("[");
        add_kid(node, parse_args());
        return node;
    }
    if (accept("typeid")) {
        expect_tok("[");
        Node* node = new_node(N_TYPEID, 0, line);
        add_kid(node, starts_type() ? parse_type() : parse_expr());
        expect_tok("]");
        return node;
    }
    if (accept("sizeof")) {
        Node* node = new_node(N_SIZEOF, 0, line);
        if (accept("[")) {
            add_kid(node, starts_type() ? parse_type() : parse_expr());
            expect_tok("]");
        } else add_kid(node, parse_type());
        return node;
    } // sizeof [ x ] or sizeof T
    if (accept("moduleid")) {
        expect_tok("[");
        Node* node = new_node(N_MODULEID, 0, line);
        add_kid(node, parse_expr());
        expect_tok("]");
        return node;
    }
    if (accept("expect")) {
        Node* node = new_node(N_EXPECTX, 0, line);
        add_kid(node, parse_expr());
        return node;
    }
    if (accept("super")) return new_node(N_SUPER, 0, line);
    if (tok_is("lambda") && tok_at_is(1, "[")) {
        S->pos++;
        return parse_lambda_inline(token->indent);
    }
    if (tok_is("lambda") && !is_type_name(S->tokens[S->pos + 1].text)) {
        S->pos++;
        Node* bind = new_node(N_BIND, 0, line);
        add_kid(bind, parse_postfix());
        return bind;
    }
    if (token->kind == 'a' && token->text[0] == '-' && token->text[1] &&
        !isdigit(token->text[1])) {
        S->pos++;
        Node* node = new_node(N_UN, "-", line);
        add_kid(node, new_node(N_IDENT, token->text + 1, line));
        return node;
    }
    if (starts_type()) {
        int   saved_pos = S->pos;
        Node* type_node = parse_type();
        if (tok_is("[") && same_line()) {
            S->pos++;
            Node* construct = new_node(N_CONSTRUCT, 0, line);
            add_kid(construct, type_node);
            add_kid(construct, parse_args());
            return construct;
        }
        if (tok_is(".") && cur_tok()->no_space && !kid(type_node, 0)) {
            S->pos = saved_pos;
            S->pos++;
            S->pos++;
            Node* member =
                new_node(N_TYPEMEMBER, next_tok()->text, line);
            add_kid(member, type_node);
            return member;
        }
        if (tok_is("{") && same_line()) {
            Buf buf = {0};
            S->pos++;
            while (!tok_is("}"))
                append(&buf, "%s%s", buf.count ? " " : "",
                       next_tok()->text);
            S->pos++;
            Node* raw = new_node(N_RAW, buf.data ? buf.data : "", line);
            add_kid(raw, type_node);
            return raw;
        }
        if (tok_is("asm") && same_line()) {
            S->pos++;
            Node* asm_node = new_node(N_ASMX, 0, line);
            add_kid(asm_node, type_node);
            expect_tok("[");
            add_kid(asm_node, parse_args());
            parse_asm_lines(token->indent, asm_node);
            return asm_node;
        }
        if (same_line() && starts_primary() && !tok_is("in") &&
            !tok_is("reverse")) {
            Node* conv = new_node(N_CONV, 0, line);
            add_kid(conv, type_node);
            add_kid(conv, parse_expr());
            return conv;
        }
        Node* ref = new_node(N_TYPEREF, type_node->text, line);
        add_kid(ref, type_node);
        return ref;
    }
    if (token->kind == 'a') {
        S->pos++;
        return new_node(N_IDENT, token->text, line);
    }
    parse_fail("unexpected token");
    return 0;
}
static Node* parse_postfix(void) {
    Node* expr = parse_primary();
    for (;;) {
        if (!same_line()) return expr;
        if (tok_is(".") && cur_tok()->no_space) {
            S->pos++;
            Token* token = next_tok();
            if (token->kind == 'n') {
                char* fmt_text = token->text;
                if (S->pos < S->token_count && cur_tok()->kind == 'a' &&
                    cur_tok()->no_space &&
                    strlen(cur_tok()->text) == 1 &&
                    strchr("fdxs", cur_tok()->text[0]))
                    fmt_text =
                        format("%s%s", fmt_text, next_tok()->text);
                Node* member = new_node(N_FMT, fmt_text, token->line);
                add_kid(member, expr);
                expr = member;
                continue;
            }
            Node* member = new_node(N_MEMBER, token->text, token->line);
            add_kid(member, expr);
            expr = member;
            if (tok_is("*") && cur_tok()->no_space &&
                tok_at_is(1, "[")) {
                S->pos++;
                member->flag = 1;
            }
            continue;
        }
        if (tok_is("->") && cur_tok()->no_space) {
            S->pos++;
            Node* member =
                new_node(N_ARROW, next_tok()->text, expr->line);
            add_kid(member, expr);
            expr = member;
            continue;
        }
        if (tok_is("[")) {
            S->pos++;
            Node* call = new_node(N_CALL, 0, expr->line);
            add_kid(call, expr);
            add_kid(call, parse_args());
            expr = call;
            continue;
        }
        return expr;
    }
}
typedef struct {
    const char* op;
    int         precedence;
} Prec;
static Prec precs[] = {
    {"??", 1},  {"||", 2}, {"&&", 3}, {"|", 4},  {"^", 5},
    {"&", 6},   {"==", 7}, {"!=", 7}, {"is", 7}, {"inherits", 7},
    {"<=>", 7}, {"<", 8},  {">", 8},  {"<=", 8}, {">=", 8},
    {"<<", 9},  {">>", 9}, {"+", 10}, {"-", 10}, {"*", 11},
    {"/", 11},  {"%", 11}, {0, 0}};
static int prec_of(void) {
    if (!same_line() || S->pos >= S->token_count) return 0;
    for (int i = 0; precs[i].op; i++)
        if (same(S->tokens[S->pos].text, precs[i].op))
            return precs[i].precedence;
    return 0;
}
static Node* parse_bin(int min_prec) {
    Node* left = parse_postfix();
    for (int prec; (prec = prec_of()) >= min_prec && prec > 0;) {
        Token* op = next_tok();
        if (same(op->text, "||") && tok_is("return")) {
            S->pos++;
            Node* right = new_node(N_ORRET, 0, op->line);
            add_kid(right, left);
            add_kid(right, new_line() ? 0 : parse_expr());
            return right;
        }
        Node* right = parse_bin(prec + 1);
        left        = new_node2(N_BIN, op->text, op->line, left, right);
    }
    return left;
}
static Node* parse_expr(void) {
    Node* cond = parse_bin(1);
    if (same_line() && accept("?")) {
        Node* yes = parse_expr();
        expect_tok(":");
        Node* no   = parse_expr();
        Node* tern = new_node(N_TERN, 0, cond->line);
        add_kid(tern, cond);
        add_kid(tern, yes);
        add_kid(tern, no);
        return tern;
    }
    return cond;
}
static bool is_assign_op(const char* op_text) {
    static const char* ops[] = {"=",  "+=", "-=", "*=",  "/=",  "%=",
                                "|=", "&=", "^=", "<<=", ">>=", 0};
    for (int i = 0; ops[i]; i++)
        if (same(op_text, ops[i])) return true;
    return false;
}
static Node* parse_decl_rhs(Node* decl, int indent) { // after 'name :'
    if (starts_type() && !(tok_is("lambda") && tok_at_is(1, "["))) {
        int   saved_pos = S->pos;
        Node* type_node = parse_type();
        if (tok_is(".") && cur_tok()->no_space) {
            S->pos = saved_pos;
            add_kid(decl, 0);
            add_kid(decl, parse_expr());
            return decl;
        }
        add_kid(decl, type_node);
        if (tok_is("[") && same_line()) {
            S->pos++;
            add_kid(decl, parse_args());
            decl->flag = 1;
            return decl;
        }
        if (tok_is("{") && same_line()) {
            S->pos                      = saved_pos;
            decl->kids[decl->count - 1] = 0;
            add_kid(decl, parse_expr());
            return decl;
        }
        if (tok_is("asm") && same_line()) {
            S->pos                      = saved_pos;
            decl->kids[decl->count - 1] = 0;
            add_kid(decl, parse_expr());
            return decl;
        }
        if (same_line() && starts_primary()) {
            add_kid(decl, parse_expr());
            return decl;
        }
        if (S->pos < S->token_count && new_line() &&
            cur_tok()->indent > indent &&
            ((cur_tok()->kind == 'a' && tok_at_is(1, ":")) ||
             tok_is(":"))) { // multi-line prop block
            Node* props        = new_node(N_ARGS, 0, cur_tok()->line);
            int   block_indent = cur_tok()->indent;
            while (S->pos < S->token_count &&
                   cur_tok()->indent == block_indent) {
                if (accept(":"))
                    add_kid(props,
                            new_node(N_SHORTPROP, next_tok()->text,
                                     S->tokens[S->pos - 1].line));
                else {
                    Node* key = new_node(N_IDENT, next_tok()->text,
                                         S->tokens[S->pos - 1].line);
                    expect_tok(":");
                    add_kid(props,
                            new_node2(N_PROP, key->text, key->line, key,
                                      parse_expr()));
                }
                accept(",");
            }
            add_kid(decl, props);
            decl->flag = 1;
        }
        return decl;
    }
    add_kid(decl, 0);
    add_kid(decl, parse_expr());
    return decl;
}
static Node* parse_if_chain(const char* keyword, int indent, int line) {
    Node* node =
        new_node(same(keyword, "if") ? S_IF : S_IFDEF, keyword, line);
    expect_tok("[");
    add_kid(node, parse_expr());
    expect_tok("]");
    add_kid(node, parse_body(indent));
    if (S->pos < S->token_count && tok_is("el") &&
        cur_tok()->indent == indent && new_line()) {
        S->pos++;
        if (tok_is("[") && same_line()) {
            Node* block = new_node(S_BLOCK, 0, cur_tok()->line);
            add_kid(block,
                    parse_if_chain(keyword, indent, cur_tok()->line));
            add_kid(node, block);
        } else add_kid(node, parse_body(indent));
    }
    return node;
}
static Node* parse_stmt(void) {
    Token* token = cur_tok();
    int    line = token->line, indent = token->indent;
    Node*  node;
#define BODY parse_body(indent)
    if (accept("return")) {
        node = new_node(S_RETURN, 0, line);
        if (same_line()) add_kid(node, parse_expr());
        return node;
    }
    if (accept("no-op")) return new_node(S_NOOP, 0, line);
    if (tok_is("break") || tok_is("continue")) {
        node = new_node(same(next_tok()->text, "break") ? S_BREAK
                                                        : S_CONTINUE,
                        0, line);
        if (accept("[")) {
            node->flag = next_tok()->int_value;
            expect_tok("]");
        }
        return node;
    }
    if (tok_is("if") || tok_is("ifdef") || tok_is("ifndef"))
        return parse_if_chain(next_tok()->text, indent, line);
    if (accept("while")) {
        node = new_node(S_WHILE, 0, line);
        expect_tok("[");
        add_kid(node, parse_expr());
        expect_tok("]");
        add_kid(node, BODY);
        return node;
    }
    if (accept("for")) {
        if (!(tok_is("[") && same_line())) {
            node = new_node(S_DOWHILE, 0, line);
            add_kid(node, BODY);
            expect_tok("while");
            expect_tok("[");
            add_kid(node, parse_expr());
            expect_tok("]");
            return node;
        }
        S->pos++;
        Node* parts = new_node(N_ARGS, 0, line);
        while (!tok_is("]")) {
            Node* expr = parse_expr();
            if (accept(":")) {
                Node* decl = new_node(S_DECL, expr->text, expr->line);
                parse_decl_rhs(decl, indent);
                add_kid(parts, decl);
            } else if (S->pos < S->token_count &&
                       is_assign_op(cur_tok()->text)) {
                Node* assign_node =
                    new_node2(S_ASSIGN, next_tok()->text, expr->line,
                              expr, parse_expr());
                add_kid(parts, assign_node);
            } else
                add_kid(parts,
                        new_node2(S_EXPR, 0, expr->line, expr, 0));
            accept(",");
        }
        S->pos++;
        if (tok_is("in") || tok_is("reverse")) {
            node       = new_node(S_FORIN, 0, line);
            node->flag = accept("reverse");
            expect_tok("in");
            add_kid(node, parts);
            add_kid(node, parse_expr());
            add_kid(node, BODY);
            return node;
        }
        node = new_node(S_FOR, 0, line);
        add_kid(node, parts);
        add_kid(node, BODY);
        return node;
    }
    if (accept("switch")) {
        node = new_node(S_SWITCH, 0, line);
        expect_tok("[");
        add_kid(node, parse_expr());
        expect_tok("]");
        while (S->pos < S->token_count && cur_tok()->indent > indent &&
               (tok_is("case") || tok_is("default"))) {
            Node* case_node    = new_node(S_CASE, 0, cur_tok()->line);
            int   child_indent = cur_tok()->indent;
            if (same(next_tok()->text, "case")) {
                Node* values = new_node(N_ARGS, 0, case_node->line);
                do add_kid(values, parse_expr());
                while (accept(","));
                add_kid(case_node, values);
            } else add_kid(case_node, 0);
            add_kid(case_node, parse_block(child_indent));
            add_kid(node, case_node);
        }
        return node;
    }
    if (accept("try")) {
        node = new_node(S_TRY, 0, line);
        add_kid(node, BODY);
        add_kid(node, 0);
        add_kid(node, 0);
        add_kid(node, 0);
        if (S->pos < S->token_count && tok_is("catch") &&
            cur_tok()->indent == indent) {
            S->pos++;
            if (accept("[")) {
                Node* params  = parse_params();
                node->kids[1] = params;
            } else node->kids[1] = new_node(N_ARGS, 0, line);
            node->kids[2] = BODY;
        }
        if (S->pos < S->token_count && tok_is("finally") &&
            cur_tok()->indent == indent) {
            S->pos++;
            node->kids[3] = BODY;
        }
        return node;
    }
    if (accept("throw")) {
        node = new_node(S_THROW, 0, line);
        add_kid(node, parse_expr());
        return node;
    }
    if (accept("fault")) {
        node = new_node(S_FAULT, 0, line);
        add_kid(node, parse_expr());
        return node;
    }
    if ((tok_is("puts") || tok_is("log") || tok_is("print")) &&
        !tok_at_is(1, ":")) {
        node = new_node(tok_is("log") ? S_LOG : S_PUTS,
                        next_tok()->text, line);
        if (accept("[")) {
            Node* args = parse_args();
            add_kid(node, kid(args, 0));
        } else add_kid(node, parse_expr());
        return node;
    }
    if (accept("expect")) {
        node = new_node(S_EXPECT, 0, line);
        if (cur_tok()->kind == 'a' && tok_at_is(1, ":")) {
            Node* decl = new_node(S_DECL, next_tok()->text, line);
            S->pos++;
            parse_decl_rhs(decl, indent);
            add_kid(node, decl);
        } else add_kid(node, parse_expr());
        add_kid(node, accept(",") ? parse_expr() : 0);
        return node;
    }
    if (accept("asm")) {
        node = new_node(S_ASM, next_tok()->text, line);
        parse_asm_lines(indent, node);
        return node;
    }
    if (accept("construct")) {
        node = new_node(S_CONSTRUCT, 0, line);
        expect_tok("[");
        add_kid(node, parse_args());
        return node;
    }
    if (accept("check")) {
        node = new_node(S_CHECK, 0, line);
        add_kid(node, parse_expr());
        expect_tok(",");
        add_kid(node, parse_expr());
        return node;
    } /* check cond, message: raises when false */
    if (accept("el")) parse_fail("el without if");
    Node* expr = parse_expr();
    if (accept(":")) {
        if (expr->kind != N_IDENT && expr->kind != N_TYPEREF)
            parse_fail("declaration name expected");
        node = new_node(S_DECL, expr->text, line);
        list_push(&S->local_names, expr->text);
        return parse_decl_rhs(node, indent);
    }
    if (S->pos < S->token_count && same_line() &&
        is_assign_op(cur_tok()->text)) {
        char* op_text = next_tok()->text;
        node = new_node2(S_ASSIGN, op_text, line, expr, parse_expr());
        return node;
    }
    if (S->pos < S->token_count &&
        ((same_line() && (starts_primary() || tok_is(","))) ||
         (new_line() && cur_tok()->indent > indent &&
          (expr->kind == N_IDENT || expr->kind == N_MEMBER)))) {
        Node* call = new_node(N_CALL, 0, line);
        add_kid(call, expr);
        Node* args = new_node(N_ARGS, 0, line);
        do add_kid(args, parse_expr());
        while (accept(","));
        add_kid(call, args);
        call->flag = 1;
        expr       = call;
    }
    node = new_node(S_EXPR, 0, line);
    add_kid(node, expr);
    return node;
}
static Node* parse_block(int indent) {
    Node* block = new_node(S_BLOCK, 0, cur_tok() ? cur_tok()->line : 0);
    if (S->pos < S->token_count && same_line()) {
        add_kid(block, parse_stmt());
        return block;
    }
    int block_indent = S->pos < S->token_count ? cur_tok()->indent : 0;
    if (block_indent <= indent) return block;
    while (S->pos < S->token_count &&
           cur_tok()->indent == block_indent && new_line()) {
        add_kid(block, parse_stmt());
        if (S->pos < S->token_count && same_line())
            parse_fail("unexpected token after statement");
    }
    return block;
}

// declarations: members, functions, records
static Node* parse_func(int indent, int kind, char* name) {
    Node* func = new_node(kind, name, cur_tok()->line);
    if (kind == D_FUNC || kind == D_GETTER || kind == D_SETTER ||
        kind == D_CTOR) {
        expect_tok("[");
        add_kid(func, parse_params());
    } else add_kid(func, new_node(N_ARGS, 0, func->line));
    if (kind == D_CASTFN) {
        expect_tok("->");
        add_kid(func, parse_type());
    } else if (kind == D_OPFN) {
        expect_tok("[");
        func->kids[0] = parse_params();
        add_kid(func, accept("->") ? parse_type() : 0);
    } else add_kid(func, accept("->") ? parse_type() : 0);
    if (S->pos < S->token_count &&
        (same_line() || cur_tok()->indent > indent))
        add_kid(func, parse_body(indent));
    else add_kid(func, 0);
    return func;
}
static const char* opname(const char* op) {
    static const char* names[] = {
        "+",   "add",         "-",   "sub",          "*",  "mul",
        "/",   "div",         "%",   "mod",          "|",  "bitwise_or",
        "&",   "bitwise_and", "^",   "xor",          "<<", "left",
        ">>",  "right",       "+=",  "assign_add",   "-=", "assign_sub",
        "*=",  "assign_mul",  "/=",  "assign_div",   "%=", "assign_mod",
        "|=",  "assign_or",   "&=",  "assign_and",   "^=", "assign_xor",
        "<<=", "assign_left", ">>=", "assign_right", "==", "equal",
        "!=",  "not_equal",   "<",   "less",         ">",  "greater",
        "<=",  "less_eq",     ">=",  "greater_eq",   0}; // silver's
                                                         // OPType names
    for (int i = 0; names[i]; i += 2)
        if (same(names[i], op)) return names[i + 1];
    return "op";
}
static Node* parse_record_body(Node* record, int indent) {
    while (S->pos < S->token_count && cur_tok()->indent > indent &&
           new_line()) {
        int    member_indent = cur_tok()->indent;
        Token* token         = cur_tok();
        Node*  member        = 0;
        int    mods          = 0;
        char*  access        = 0;
        while (
            tok_is("public") || tok_is("intern") || tok_is("mutable") ||
            tok_is("static") || tok_is("expect") || tok_is("context") ||
            tok_is("manual") || tok_is("unmanaged") ||
            tok_is("default") || tok_is("attrib") || tok_is("post")) {
            Token* word = next_tok();
            if (same(word->text, "static")) mods |= 1;
            if (same(word->text, "context")) mods |= 2;
            if (same(word->text, "manual") ||
                same(word->text, "unmanaged"))
                mods |= 4;
            if (same(word->text, "default")) mods |= 8;
            if (same(word->text, "attrib")) mods |= 16;
            if (same(word->text, "post")) mods |= 32;
            if (same(word->text, "intern")) mods |= 64;
        }
        char* meta = 0;
        if (accept("[")) {
            meta = next_tok()->text;
            expect_tok("]");
        } // [ Cls ] annotates the member with a meta type
        if (accept("func")) {
            member =
                parse_func(member_indent, D_FUNC, next_tok()->text);
        } else if (accept("construct"))
            member = parse_func(member_indent, D_CTOR, "construct");
        else if (accept("cast"))
            member = parse_func(member_indent, D_CASTFN, "cast");
        else if (accept("operator")) {
            bool is_left = accept("left");
            member       = parse_func(member_indent, D_OPFN,
                                      opname(next_tok()->text));
            member->flag = is_left;
        } else if (accept("getter"))
            member = parse_func(member_indent, D_GETTER, "getter");
        else if (accept("setter"))
            member = parse_func(member_indent, D_SETTER, "setter");
        else {
            member = new_node(D_MEMBER, next_tok()->text, token->line);
            expect_tok(":");
            add_kid(member, parse_type());
            if (tok_is("[") && same_line()) {
                S->pos++;
                add_kid(member, parse_args());
            } else if (same_line() && starts_primary()) {
                Node* args = new_node(N_ARGS, 0, token->line);
                add_kid(args, parse_expr());
                add_kid(member, args);
            } else add_kid(member, 0);
        }
        member->flag |= mods << 4;
        if (meta) member->raw = meta;
        add_kid(record, member);
    }
    return record;
}
static Node* parse_toplevel(void) {
    Token* token = cur_tok();
    int    line  = token->line;
    Node*  decl;
    if (accept("import")) { // body lines: `> command` or `name: value`
                            // config
        decl = new_node(D_IMPORT, 0, line);
        while (S->pos < S->token_count && cur_tok()->line == line)
            add_kid(decl, new_node(N_IDENT, next_tok()->text, line));
        while (S->pos < S->token_count && cur_tok()->indent > 0) {
            int body_line = cur_tok()->line;
            if (accept(">")) {
                Node* raw = new_node(N_RAW, line_text(body_line) + 1,
                                     body_line);
                while (S->pos < S->token_count &&
                       cur_tok()->line == body_line)
                    S->pos++;
                add_kid(decl, raw);
            } else if (cur_tok()->kind == 'a' && tok_at_is(1, ":")) {
                Node* key =
                    new_node(N_IDENT, next_tok()->text, body_line);
                expect_tok(":");
                add_kid(decl, new_node2(N_PROP, key->text, body_line,
                                        key, parse_expr()));
            } else {
                Node* raw =
                    new_node(N_RAW, line_text(body_line), body_line);
                raw->flag = 1;
                while (S->pos < S->token_count &&
                       cur_tok()->line == body_line)
                    S->pos++;
                add_kid(decl, raw);
            }
        } // a build flag for a git import
        return decl;
    }
    if (accept("export")) {
        if (accept("func")) {
            decl       = parse_func(0, D_FUNC, next_tok()->text);
            decl->flag = 4;
            return decl;
        }
        decl = new_node(N_RAW, line_text(line), line);
        while (S->pos < S->token_count && cur_tok()->line == line)
            S->pos++;
        while (S->pos < S->token_count && cur_tok()->indent > 0)
            S->pos++;
        return decl;
    }
    if (accept("public")) {
        if (accept("func"))
            return parse_func(0, D_FUNC, next_tok()->text);
        decl = new_node(D_VAR, next_tok()->text, line);
        expect_tok(":");
        add_kid(decl, parse_type());
        if (accept("[")) add_kid(decl, parse_args());
        else if (same_line() && starts_primary()) {
            Node* args = new_node(N_ARGS, 0, line);
            add_kid(args, parse_expr());
            add_kid(decl, args);
        }
        return decl;
    }
    if (accept("extend")) {
        S->pos++;
        return new_node(S_NOOP, 0, line);
    }
    if (tok_is("ifdef") || tok_is("ifndef")) {
        decl = new_node(S_IFDEF, next_tok()->text, line);
        expect_tok("[");
        add_kid(decl, parse_expr());
        expect_tok("]");
        Node* block = new_node(S_BLOCK, 0, line);
        while (S->pos < S->token_count && cur_tok()->indent > 0)
            add_kid(block, parse_toplevel());
        add_kid(decl, block);
        return decl;
    }
    if (accept("enum")) {
        decl = new_node(D_ENUM, next_tok()->text, line);
        add_kid(decl, accept(":") ? parse_type() : 0);
        double value = 0;
        while (S->pos < S->token_count && cur_tok()->indent > 0) {
            Node* entry =
                new_node(D_ENUMV, next_tok()->text, cur_tok()->line);
            if (accept(":")) {
                Node* expr = parse_expr();
                value = expr->kind == N_FLT ? expr->token->float_value
                                            : expr->token->int_value;
            }
            entry->raw = format("%.17g", value);
            value += 1;
            add_kid(decl, entry);
        }
        return decl;
    }
    if (accept("scalar")) {
        decl = new_node(D_SCALAR, next_tok()->text, line);
        expect_tok(":");
        add_kid(decl, parse_type());
        parse_record_body(decl, 0);
        return decl;
    }
    if (accept("alias")) {
        decl = new_node(D_ALIAS, next_tok()->text, line);
        expect_tok(":");
        add_kid(decl, parse_type());
        return decl;
    }
    if (accept("struct")) {
        decl = new_node(D_STRUCT, next_tok()->text, line);
        add_kid(decl, 0);
        parse_record_body(decl, 0);
        return decl;
    }
    bool is_intern = accept("intern"), is_abstract = accept("abstract");
    token = cur_tok();
    if (accept("class")) {
        decl = new_node(D_CLASS, next_tok()->text, line);
        add_kid(decl, 0);
        parse_record_body(decl, 0);
        decl->flag = is_intern;
        return decl;
    }
    if (accept("expect")) {
        expect_tok("func");
        decl       = parse_func(0, D_FUNC, next_tok()->text);
        decl->flag = 1;
        return decl;
    }
    if (accept("func")) return parse_func(0, D_FUNC, next_tok()->text);
    if (accept("app")) {
        decl = new_node(D_APP, next_tok()->text, line);
        add_kid(decl, 0);
        parse_record_body(decl, 0);
        return decl;
    }
    if (token->kind == 'a' && tok_at_is(1, ":")) {
        decl = new_node(D_VAR, next_tok()->text, line);
        S->pos++;
        add_kid(decl, parse_type());
        if (accept("[")) add_kid(decl, parse_args());
        return decl;
    }
    if (token->kind == 'a' && S->pos + 1 < S->token_count &&
        S->tokens[S->pos + 1].kind == 'a' &&
        S->tokens[S->pos + 1].line == line) {
        char* base = next_tok()->text;
        decl       = new_node(D_CLASS, next_tok()->text, line);
        add_kid(decl, new_node(N_TYPE, base, line));
        parse_record_body(decl, 0);
        decl->flag = is_intern | (is_abstract << 1);
        return decl;
    }
    parse_fail("declaration expected");
    return 0;
}
static void prescan_types(void);
static void
prescan_file(const char* path) { // an imported module's type names must
                                 // be known before parsing
    for (int i = 0; i < S->prescanned.count; i++)
        if (same(S->prescanned.data[i], path)) return;
    char* source = read_file(path);
    if (!source) return;
    list_push(&S->prescanned, (void*)path);
    Token* saved_tokens = S->tokens;
    int    saved_count = S->token_count, saved_pos = S->pos;
    S->tokens = tokenize(source, &S->token_count);
    prescan_types();
    S->tokens      = saved_tokens;
    S->token_count = saved_count;
    S->pos         = saved_pos;
}
static void prescan_types(void) {
    for (int i = 0; i < S->token_count; i++) {
        if (S->tokens[i].col || S->tokens[i].kind != 'a') continue;
        const char* word = S->tokens[i].text;
        int         j    = i + 1;
        if (same(word, "import") && j < S->token_count &&
            S->tokens[j].kind == 'a' && S->cur_file) {
            char* dir   = strdup(S->cur_file);
            char* slash = strrchr(dir, '/');
            if (slash) *slash = 0;
            else strcpy(dir, ".");
            char* sibling = format("%s/%s.ag", dir, S->tokens[j].text);
            char* root    = format("%s/../%s/%s.ag", dir,
                                   S->tokens[j].text, S->tokens[j].text);
            prescan_file(!access(sibling, R_OK) ? sibling : root);
            if (j + 2 < S->token_count &&
                same(S->tokens[j + 1].text, "with") &&
                S->tokens[j + 2].kind == 'a')
                prescan_file(format(
                    "%s/%s.ag", dir,
                    S->tokens[j + 2].text)); /* an extension's types are
                                                the module's too */
            continue;
        }
        if (same(word, "intern") || same(word, "abstract")) {
            word = S->tokens[j].text;
            j++;
        }
        if (same(word, "class") || same(word, "struct") ||
            same(word, "enum") || same(word, "scalar") ||
            same(word, "alias"))
            list_push(&S->type_names, S->tokens[j].text);
        else if (same(word, "abstract"))
            list_push(&S->type_names, S->tokens[j + 1].text);
        else if (!same(word, "import") && !same(word, "export") &&
                 !same(word, "app") && !same(word, "extend") &&
                 !same(word, "func") && !same(word, "expect") &&
                 !same(word, "ifdef") && !same(word, "ifndef") &&
                 j < S->token_count && S->tokens[j].kind == 'a' &&
                 S->tokens[j].line == S->tokens[i].line &&
                 (j + 1 >= S->token_count ||
                  S->tokens[j + 1].line != S->tokens[i].line))
            list_push(&S->type_names, S->tokens[j].text);
    }
}
static Node* parse_module(Token* token_list, int count,
                          Node* module_node) {
    S->tokens      = token_list;
    S->token_count = count;
    S->pos         = 0;
    prescan_types();
    while (S->pos < S->token_count) {
        Node* decl;
        int   start          = S->pos;
        S->local_names.count = 0;
        if (cur_tok()->indent > 0) {
            S->pos++;
            continue;
        }
        if (setjmp(S->parse_jump) == 0) decl = parse_toplevel();
        else {
            S->pos      = start;
            decl        = new_node(D_BROKEN, S->tokens[start].text,
                                   S->tokens[start].line);
            decl->error = S->parse_message;
            if (tok_is("expect") && tok_at_is(1, "func")) {
                decl->text = strdup(S->tokens[S->pos + 2].text);
                decl->flag = 1;
            }
            S->pos = start + 1;
            while (S->pos < S->token_count &&
                   (cur_tok()->indent > 0 ||
                    cur_tok()->line == S->tokens[start].line))
                S->pos++;
        }
        add_kid(module_node, decl);
    }
    return module_node;
}

// ----------------------------------------------------------------
// types
enum {
    T_UNK,
    T_NONE,
    T_INT,
    T_FLOAT,
    T_BOOL,
    T_CSTR,
    T_UNICHAR,
    T_STRING,
    T_PATH,
    T_OBJECT,
    T_TYPE,
    T_HANDLE,
    T_VEC,
    T_MAP,
    T_REF,
    T_LAMBDA,
    T_CLASS,
    T_STRUCT,
    T_ENUM,
    T_SCALAR,
    T_LOCAL,
    T_SHAPE,
    T_TOKENS,
    T_ASYNC,
    T_HDR
};
struct Type {
    int    kind;
    char*  name;
    int    bits;
    bool   is_signed;
    Type*  elem;
    Type*  key;
    Type*  result;
    Type** args;
    int    arg_count;
    Node*  decl;
    Node*  dims;
    bool   growable;
    Type*  base;
    void*  llvm;
    List   field_names;
    List   field_types;
    void*  record;
    void*  c_decl;
};
static Type* c_type(const char* name);
static Type* new_type(int kind, const char* name) {
    Type* type = calloc(1, sizeof(Type));
    type->kind = kind;
    type->name = strdup(name);
    return type;
}
static void emit_fail(const char* spec, ...) {
    char    text[1024];
    va_list args;
    va_start(args, spec);
    vsnprintf(text, sizeof text, spec, args);
    va_end(args);
    S->emit_message = strdup(text);
    if (!S->emit_guarded) {
        fprintf(stderr, "%s: %s\n", S->cur_file, text);
        exit(1);
    }
    longjmp(S->emit_jump, 1);
}
static Type* type_of(Node* tn);
static Type* imported_type(const char* name);
static Type* type_named(const char* name) {
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (same(type->name, name)) {
            if (type->kind == T_UNK && type->decl) {
                Type* resolved = type_of(type->decl);
                return resolved;
            }
            return type;
        }
    }
    static const struct {
        const char* name;
        int         kind;
        int         bits;
        bool        is_signed;
    } prims[] = {{"i8", T_INT, 8, 1},
                 {"i16", T_INT, 16, 1},
                 {"i32", T_INT, 32, 1},
                 {"i64", T_INT, 64, 1},
                 {"u8", T_INT, 8, 0},
                 {"u16", T_INT, 16, 0},
                 {"u32", T_INT, 32, 0},
                 {"u64", T_INT, 64, 0},
                 {"f32", T_FLOAT, 32, 1},
                 {"f64", T_FLOAT, 64, 1},
                 {"half", T_FLOAT, 16, 1},
                 {"bool", T_BOOL, 8, 0},
                 {"none", T_NONE},
                 {"void", T_NONE},
                 {"cstr", T_CSTR},
                 {"string", T_STRING},
                 {"path", T_PATH},
                 {"object", T_OBJECT},
                 {"any", T_OBJECT},
                 {"Au", T_OBJECT},
                 {"Au_t", T_TYPE},
                 {"handle", T_HANDLE},
                 {"ARef", T_HANDLE},
                 {"hook", T_HANDLE},
                 {"symbol", T_CSTR},
                 {"sz", T_INT, 64, 0},
                 {"num", T_INT, 64, 1},
                 {"real", T_FLOAT, 64, 1},
                 {"unichar", T_UNICHAR, 32, 0},
                 {"shape", T_SHAPE},
                 {"tokens", T_TOKENS},
                 {"async", T_ASYNC},
                 {"vector", T_VEC},
                 {0}};
    for (int i = 0; prims[i].name; i++)
        if (same(prims[i].name, name)) {
            Type* type      = new_type(prims[i].kind, name);
            type->bits      = prims[i].bits;
            type->is_signed = prims[i].is_signed;
            list_push(&S->types, type);
            return type;
        }
    Type* imported_t = imported_type(name);
    return imported_t ? imported_t : c_type(name);
}
static Type* type_of(Node* type_node) {
    if (!type_node) return new_type(T_NONE, "none");
    Type* type;
    if (same(type_node->text, "vec") ||
        same(type_node->text, "local")) {
        type = new_type(same(type_node->text, "vec") ? T_VEC : T_LOCAL,
                        type_node->text);
        type->elem     = type_of(kid(type_node, 0));
        type->dims     = kid(type_node, 1);
        type->growable = type_node->flag == 1;
        return type;
    }
    if (same(type_node->text, "map")) {
        type       = new_type(T_MAP, "map");
        type->elem = type_of(kid(type_node, 0));
        type->key  = type_of(kid(type_node, 1));
        return type;
    }
    if (same(type_node->text, "@")) {
        Type* elem = type_of(kid(type_node, 0));
        if (elem->kind == T_CLASS && elem->c_decl) return elem;
        type       = new_type(T_REF, "@");
        type->elem = elem;
        return type;
    }
    if (same(type_node->text, "lambda")) {
        type         = new_type(T_LAMBDA, "lambda");
        type->result = type_of(kid(type_node, 0));
        Node* params = kid(type_node, 1);
        type->args   = calloc(params->count + 1, sizeof(Type*));
        for (int i = 0; i < params->count; i++)
            type->args[type->arg_count++] =
                type_of(params->kids[i]->kids[0]);
        return type;
    }
    type = type_named(type_node->text);
    if (!type)
        emit_fail("unknown type %s (line %d)", type_node->text,
                  type_node->line);
    return type;
}
static bool is_num(Type* type) {
    return type->kind == T_INT || type->kind == T_FLOAT ||
           type->kind == T_BOOL || type->kind == T_UNICHAR ||
           type->kind == T_ENUM || type->kind == T_SCALAR;
}
static bool is_obj(Type* type) {
    return type->kind == T_STRING || type->kind == T_PATH ||
           type->kind == T_OBJECT || type->kind == T_VEC ||
           type->kind == T_MAP || type->kind == T_LAMBDA ||
           type->kind == T_CLASS || type->kind == T_ASYNC ||
           type->kind == T_TOKENS;
}
static bool is_str(Type* type) {
    return type->kind == T_STRING || type->kind == T_PATH;
}
static Type* backing(Type* type) {
    return (type->kind == T_ENUM || type->kind == T_SCALAR) &&
                   type->base
               ? type->base
               : type;
}
static bool same_type(Type* left, Type* right) {
    if (left->kind != right->kind) return false;
    if (left->kind == T_INT || left->kind == T_FLOAT)
        return left->bits == right->bits || !left->bits || !right->bits;
    return same(left->name, right->name) ||
           (left->kind != T_CLASS && left->kind != T_STRUCT &&
            left->kind != T_ENUM && left->kind != T_SCALAR);
}
static bool class_is(Type* type, Type* base) {
    for (Type* cur = type; cur; cur = cur->base)
        if (cur == base || (base && same(cur->name, base->name)))
            return true;
    return false;
}
static Type* num_result(Type* left, Type* right) {
    if (left->kind == T_SCALAR) return left;
    if (right->kind == T_SCALAR) return right;
    Type* left_base  = backing(left);
    Type* right_base = backing(right);
    if (left_base->kind == T_FLOAT && right_base->kind == T_FLOAT)
        return left_base->bits >= right_base->bits ? left_base
                                                   : right_base;
    if (left_base->kind == T_FLOAT) return left_base;
    if (right_base->kind == T_FLOAT) return right_base;
    if (left_base->kind == T_INT && !left_base->bits)
        return right_base->kind == T_INT ? right_base : left_base;
    if (right_base->kind == T_INT && !right_base->bits)
        return left_base;
    return left_base->bits >= right_base->bits ? left_base : right_base;
}
static Type* basic(int kind) {
    Type** cache = S->basic_types;
    if (!cache[kind]) {
        cache[kind] = new_type(kind, kind == T_INT       ? "i64"
                                     : kind == T_FLOAT   ? "f64"
                                     : kind == T_BOOL    ? "bool"
                                     : kind == T_STRING  ? "string"
                                     : kind == T_OBJECT  ? "object"
                                     : kind == T_CSTR    ? "cstr"
                                     : kind == T_UNICHAR ? "unichar"
                                     : kind == T_NONE    ? "none"
                                     : kind == T_TYPE    ? "Au_t"
                                     : kind == T_HDR     ? "hdr"
                                     : kind == T_SHAPE   ? "shape"
                                     : kind == T_PATH    ? "path"
                                                         : "?");
        if (kind == T_INT) {
            cache[kind]->is_signed = 1;
            cache[kind]->bits      = 64;
        }
        if (kind == T_FLOAT) cache[kind]->bits = 64;
        if (kind == T_UNICHAR) cache[kind]->bits = 32;
    }
    return cache[kind];
}
static Type* lit_int(void) {
    Type* type      = new_type(T_INT, "i64");
    type->is_signed = 1;
    return type;
}
static Type* lit_flt(void) { return new_type(T_FLOAT, "f64"); }
static Type* ref_to(Type* elem) {
    Type* type = new_type(T_REF, "@");
    type->elem = elem;
    return type;
}
static Type* vec_of(Type* elem) {
    Type* type = new_type(T_VEC, "vec");
    type->elem = elem;
    return type;
}

// records: members and methods
static Node* member_named(Type* class_type, const char* name,
                          Type** owner) {
    for (Type* cur = class_type; cur; cur = cur->base) {
        Node* decl = cur->decl;
        if (!decl) break;
        for (int i = 1; i < decl->count; i++)
            if (decl->kids[i]->kind == D_MEMBER &&
                same(decl->kids[i]->text, name)) {
                if (owner) *owner = cur;
                return decl->kids[i];
            }
    }
    return 0;
}
static int         member_mods(Node* member);
static const char* member_at(
    Type* class_type,
    int   index) { // the idx-th non-static member, base classes first
    Type* chain[32];
    int   depth = 0;
    for (Type* cur = class_type; cur && cur->decl; cur = cur->base)
        chain[depth++] = cur;
    for (int k = depth - 1; k >= 0; k--) {
        Node* decl = chain[k]->decl;
        for (int i = 1; i < decl->count; i++)
            if (decl->kids[i]->kind == D_MEMBER &&
                !(member_mods(decl->kids[i]) & 1) && index-- == 0)
                return decl->kids[i]->text;
    }
    return 0;
}
static Node* find_method(Type* class_type, const char* name, int kind,
                         Type** owner) {
    for (Type* cur = class_type; cur; cur = cur->base) {
        Node* decl = cur->decl;
        if (!decl) break;
        for (int i = 1; i < decl->count; i++)
            if (decl->kids[i]->kind == kind &&
                same(decl->kids[i]->text, name)) {
                if (owner) *owner = cur;
                return decl->kids[i];
            }
    }
    return 0;
}
static Node* find_func(const char* name) {
    for (int i = 0; i < S->MOD->count; i++)
        if (S->MOD->kids[i]->kind == D_FUNC &&
            same(S->MOD->kids[i]->text, name))
            return S->MOD->kids[i];
    return 0;
}
static Node* find_global(const char* name) {
    for (int i = 0; i < S->MOD->count; i++)
        if (S->MOD->kids[i]->kind == D_VAR &&
            same(S->MOD->kids[i]->text, name))
            return S->MOD->kids[i];
    return 0;
}
static int  member_mods(Node* member) { return member->flag >> 4; }
static bool member_held(Node* member) {
    Type* type = type_of(member->kids[0]);
    int   mods = member_mods(member);
    return is_obj(type) && !(mods & (2 | 4));
}
static Type* fn_ret(Node* func) {
    return func->kind == D_CASTFN ? type_of(func->kids[1])
           : func->kind == D_CTOR || func->kind == D_SETTER
               ? basic(T_NONE)
               : type_of(kid(func, 1));
}
static Type* param_type(Node* param) { return type_of(param->kids[0]); }
static char*
member_name(Node* func) { // silver's member spellings: _add, _lmul,
                          // with_i64, index_i64, cast_string, setter
    if (func->kind == D_CASTFN)
        return format("cast_%s", func->kids[1]->text);
    if (func->kind == D_OPFN)
        return format("_%s%s", func->flag & 1 ? "l" : "", func->text);
    if (func->kind == D_GETTER) return strdup("index_i64");
    if (func->kind == D_CTOR)
        return format("with_%s", func->kids[0]->kids[0]->kids[0]->text);
    return strdup(func->text);
}
static char* method_cname(Type* owner, Node* func) {
    return format("%s_%s", owner->name, member_name(func));
}
static char* module_macro(const char* ident) {
    char* text = format("%s", ident);
    for (char* cursor = text; *cursor; cursor++)
        if (*cursor == '-') *cursor = '_';
    return text;
} // silver-features -> silver_features
static char* record_global(const char* module_ident, const char* type) {
    return format("%s_%s_i", module_macro(module_ident), type);
} // Au's Type_i(): <module>_<type>_i
static Node* find_cast(Type* type, const char* to, Type** owner) {
    for (Type* cur = type; cur; cur = cur->base) {
        Node* decl = cur->decl;
        if (!decl) break;
        for (int i = 1; i < decl->count; i++)
            if (decl->kids[i]->kind == D_CASTFN &&
                same(decl->kids[i]->kids[1]->text, to)) {
                if (owner) *owner = cur;
                return decl->kids[i];
            }
    }
    return 0;
}
static Node* find_ctor(Type* type, Type* arg, bool* is_post) {
    Node* decl = type->decl;
    for (int i = 1; i < decl->count; i++)
        if (decl->kids[i]->kind == D_CTOR &&
            decl->kids[i]->kids[0]->count == 1 &&
            same_type(param_type(decl->kids[i]->kids[0]->kids[0]),
                      arg)) {
            if (is_post)
                *is_post = (member_mods(decl->kids[i]) & 32) != 0;
            return decl->kids[i];
        }
    return 0;
}
static Node* find_op(Type* type, const char* op, bool is_left) {
    for (Type* cur = type; cur; cur = cur->base) {
        Node* decl = cur->decl;
        if (!decl) break;
        for (int i = 1; i < decl->count; i++)
            if (decl->kids[i]->kind == D_OPFN &&
                same(decl->kids[i]->text, opname(op)) &&
                ((decl->kids[i]->flag & 1) != 0) == is_left)
                return decl->kids[i];
    }
    return 0;
}

// ---------------------------------------------------------------- Au:
// the object model, read from libAu's own type tables
static Au_t au_type(const char* name) {
    if (!S->au_lib) {
        S->au_lib = dlopen("libAu.so", RTLD_NOW | RTLD_GLOBAL);
        if (!S->au_lib) {
            fprintf(stderr, "libAu.so: %s\n", dlerror());
            exit(1);
        }
    }
    void* ptr = dlsym(S->au_lib, format("Au_%s_i", name));
    return ptr ? (Au_t)((char*)ptr + HDR) : 0;
}
static Au_t au_member(Au_t type_rec, const char* name, int kind) {
    for (Au_t cur = type_rec; cur; cur = cur->context)
        for (int i = 0; i < cur->members.count; i++) {
            Au_t member = (Au_t)cur->members.origin[i];
            if (member->ident && same(member->ident, name) &&
                (!kind || member->member_type == kind))
                return member;
        }
    return 0;
}
static Type* from_au(Au_t rec) { // an Au type record as a silver type
    List* seen = &S->au_seen;
    for (int i = 0; i + 1 < seen->count; i += 2)
        if (seen->data[i] == rec) return seen->data[i + 1];
    const char* name = rec->ident ? rec->ident : "?";
    Type*       type = 0;
    static const struct {
        const char* name;
        int         kind;
        int         bits;
        bool        is_signed;
    } prims[] = {{"i8", T_INT, 8, 1},
                 {"i16", T_INT, 16, 1},
                 {"i32", T_INT, 32, 1},
                 {"i64", T_INT, 64, 1},
                 {"num", T_INT, 64, 1},
                 {"sz", T_INT, 64, 1},
                 {"u8", T_INT, 8, 0},
                 {"u16", T_INT, 16, 0},
                 {"u32", T_INT, 32, 0},
                 {"u64", T_INT, 64, 0},
                 {"f32", T_FLOAT, 32, 1},
                 {"f64", T_FLOAT, 64, 1},
                 {"real", T_FLOAT, 64, 1},
                 {"bool", T_BOOL, 8, 0},
                 {"none", T_NONE},
                 {"cstr", T_CSTR},
                 {"symbol", T_CSTR},
                 {"sz", T_INT, 64, 0},
                 {"num", T_INT, 64, 1},
                 {"real", T_FLOAT, 64, 1},
                 {"unichar", T_UNICHAR, 32, 0},
                 {"Au_t", T_TYPE},
                 {"Au", T_OBJECT},
                 {"object", T_OBJECT},
                 {"any", T_OBJECT},
                 {"string", T_STRING},
                 {"path", T_PATH},
                 {"vector", T_VEC},
                 {"map", T_MAP},
                 {"lambda", T_LAMBDA},
                 {"async", T_ASYNC},
                 {"shape", T_SHAPE},
                 {"tokens", T_TOKENS},
                 {0}};
    for (int i = 0; prims[i].name; i++)
        if (same(prims[i].name, name)) {
            type            = new_type(prims[i].kind, name);
            type->bits      = prims[i].bits;
            type->is_signed = prims[i].is_signed;
        }
    if (!type)
        type = new_type(rec->is_class    ? T_CLASS
                        : rec->is_enum   ? T_INT
                        : rec->is_struct ? T_STRUCT
                                         : T_HANDLE,
                        name);
    if (rec->is_enum) type->bits = 32, type->is_signed = 1;
    if (type->kind == T_VEC) type->elem = basic(T_OBJECT);
    if (type->kind == T_MAP) {
        type->elem = basic(T_OBJECT);
        type->key  = basic(T_STRING);
    }
    if (type->kind == T_STRUCT && !rec->is_class) type->llvm = 0;
    type->record = rec;
    list_push(seen, rec);
    list_push(seen, type);
    return type;
}
static Au_t imported_member(const char* name, int kind) {
    for (int i = 0; i < S->imported.count; i++) {
        Au_t member = au_member(S->imported.data[i], name, kind);
        if (member) return member;
    }
    return 0;
}
static bool imported_type_name(const char* name) {
    return imported_member(name, AU_MEMBER_TYPE) != 0;
}
static Type* imported_type(const char* name) {
    Au_t member = imported_member(name, AU_MEMBER_TYPE);
    return member && (member->is_class || member->is_struct ||
                      member->is_enum || member->is_scalar)
               ? from_au(member)
               : 0;
}

// ---------------------------------------------------------------- C
// and C++: declarations clang read from the imported headers
// (silver2_clang.cc)
static char* c_spell(
    const char* text) { // a silver type spelling as C: pairT<i32> ->
                        // pairT<int>; spaces dropped for comparison
    static const char* names[] = {
        "i8",   "signed char",   "i16",  "short",
        "i32",  "int",           "i64",  "long long",
        "u8",   "unsigned char", "u16",  "unsigned short",
        "u32",  "unsigned int",  "u64",  "unsigned long long",
        "f32",  "float",         "f64",  "double",
        "bool", "bool",          "cstr", "const char*",
        0};
    Buf buf = {0};
    for (const char* cursor = text; *cursor;) {
        if (isalnum(*cursor) || *cursor == '_') {
            const char* end = cursor;
            while (isalnum(*end) || *end == '_') end++;
            char* word = strndup(cursor, end - cursor);
            for (int i = 0; names[i]; i += 2)
                if (same(names[i], word)) word = (char*)names[i + 1];
            append(&buf, "%s", word);
            cursor = end;
        } else {
            if (*cursor != ' ') append(&buf, "%c", *cursor);
            cursor++;
        }
    }
    return buf.data ? buf.data : strdup("");
}
static bool c_same(const char* left, const char* right) {
    Buf left_buf = {0}, right_buf = {0};
    for (; *left; left++)
        if (*left != ' ') append(&left_buf, "%c", *left);
    for (; *right; right++)
        if (*right != ' ') append(&right_buf, "%c", *right);
    return same(left_buf.data ? left_buf.data : "",
                right_buf.data ? right_buf.data : "");
}
static CD* c_find(const char* qualified_name, int kind) {
    char* want = c_spell(qualified_name);
    for (int i = 0; i < S->ncdecls; i++)
        if (S->cdecls[i]->kind == kind &&
            (c_same(S->cdecls[i]->qualified ? S->cdecls[i]->qualified
                                            : S->cdecls[i]->name,
                    want) ||
             (kind != CD_RECORD &&
              same(S->cdecls[i]->name, qualified_name))))
            return S->cdecls[i];
    return 0;
}
static CD* c_func(const char* name,
                  int         nargs) { // a free function by (possibly
                               // templated) name and argument count
    char* base_name = strdup(name);
    char* angle     = strchr(base_name, '<');
    if (angle) *angle = 0;
    for (int i = 0; i < S->ncdecls; i++) {
        CD* decl = S->cdecls[i];
        if (decl->kind == CD_FUNC &&
            (same(decl->qualified, base_name) ||
             same(decl->name, base_name)) &&
            (decl->param_count == nargs ||
             (decl->is_variadic && nargs >= decl->param_count)))
            return decl;
    }
    return 0;
}
static Au_t
au_rec_of(Type* type) { // the libAu record a silver type maps onto; a
                        // module's own types have none at compile time
    if (type->record) return type->record;
    if (type->decl || type->c_decl) return 0;
    const char* name =
        type->kind == T_INT
            ? (type->bits ? format("%c%d", type->is_signed ? 'i' : 'u',
                                   type->bits)
                          : "i64")
        : type->kind == T_FLOAT   ? (type->bits == 32   ? "f32"
                                     : type->bits == 16 ? "bf16"
                                                        : "f64")
        : type->kind == T_BOOL    ? "bool"
        : type->kind == T_CSTR    ? "cstr"
        : type->kind == T_UNICHAR ? "unichar"
        : type->kind == T_STRING  ? "string"
        : type->kind == T_PATH    ? "path"
        : type->kind == T_VEC     ? "vector"
        : type->kind == T_TOKENS  ? "tokens"
        : type->kind == T_MAP     ? "map"
        : type->kind == T_LAMBDA  ? "lambda"
        : type->kind == T_ASYNC   ? "async"
        : type->kind == T_SHAPE   ? "shape"
        : type->kind == T_TYPE    ? "Au_t"
        : type->kind == T_NONE    ? "none"
        : type->kind == T_REF || type->kind == T_HANDLE ? "ARef"
                                                        : "Au";
    return type->record = au_type(name);
}

// ----------------------------------------------------------------
// module: every LLVM call lives here
static LType llvm_of(Type* type);
static void  ensure_layout(Type* type);
static Value const_int(LType type, long long value) {
    return LLVMConstInt(type, value, 1);
}
static Value const_i64(long long value) {
    return const_int(S->ir.i64, value);
}
static Value const_i32(long long value) {
    return const_int(S->ir.i32, value);
}
static Value const_str(const char* text) {
    return LLVMBuildGlobalString(S->ir.build, text, "");
}
static Value load(LType type, Value ptr) {
    return LLVMBuildLoad2(S->ir.build, type, ptr, "");
}
static Value field_ptr(LType struct_type, Value ptr, int index) {
    return LLVMBuildStructGEP2(S->ir.build, struct_type, ptr, index,
                               "");
}
static Value index_ptr(LType elem_type, Value ptr, Value index) {
    return LLVMBuildGEP2(S->ir.build, elem_type, ptr, &index, 1, "");
}
static Value byte_ptr(Value ptr, long offset) {
    return index_ptr(S->ir.i8, ptr, const_i64(offset));
}
static Block new_block(const char* name) {
    return LLVMAppendBasicBlockInContext(S->ir.context, S->ir.func,
                                         name);
}
static Block cur_block(void) { return LLVMGetInsertBlock(S->ir.build); }
static void  build_at(Block block) {
    LLVMPositionBuilderAtEnd(S->ir.build, block);
}
static bool block_ended(void) {
    return LLVMGetBasicBlockTerminator(cur_block()) != 0;
}
static void branch_to(Block right) {
    if (!block_ended()) LLVMBuildBr(S->ir.build, right);
}
static Value phi_of(LType type, Value left, Block left_block,
                    Value right, Block right_block) {
    Value phi   = LLVMBuildPhi(S->ir.build, type, "");
    Value vs[2] = {left, right};
    Block bs[2] = {left_block, right_block};
    LLVMAddIncoming(phi, vs, bs, 2);
    return phi;
}
static Value stack_slot(LType type) {
    Block saved_block = cur_block();
    Block entry       = LLVMGetEntryBasicBlock(S->ir.func);
    Value first       = LLVMGetFirstInstruction(entry);
    if (first) LLVMPositionBuilderBefore(S->ir.build, first);
    else build_at(entry);
    Value slot = LLVMBuildAlloca(S->ir.build, type, "");
    build_at(saved_block);
    return slot;
}
static Value addr_of(Value value) {
    Value slot = stack_slot(LLVMTypeOf(value));
    LLVMBuildStore(S->ir.build, value, slot);
    return slot;
}
static LType sig_type(char letter) {
    switch (letter) {
    case 'p': return S->ir.ptr;
    case 'i': return S->ir.i32;
    case 'l': return S->ir.i64;
    case 'd': return S->ir.f64;
    case 'f': return S->ir.f32;
    case 'b': return S->ir.i8;
    case 'v': return S->ir.void_type;
    }
    return S->ir.i64;
}
static Value
fit(Value value,
    LType want) { // fit a value to a call parameter; no parameter type
                  // means C varargs promotion
    LType type = LLVMTypeOf(value);
    if (!want)
        want = type == S->ir.f32 || type == S->ir.bf16 ? S->ir.f64
               : type == S->ir.i1 || type == S->ir.i8  ? S->ir.i32
                                                       : type;
    if (type == want) return value;
    LLVMTypeKind kind      = LLVMGetTypeKind(type),
                 want_kind = LLVMGetTypeKind(want);
    if (kind == LLVMIntegerTypeKind && want_kind == LLVMIntegerTypeKind)
        return LLVMGetIntTypeWidth(type) < LLVMGetIntTypeWidth(want)
                   ? (type == S->ir.i1
                          ? LLVMBuildZExt(S->ir.build, value, want, "")
                          : LLVMBuildSExt(S->ir.build, value, want, ""))
                   : LLVMBuildTrunc(S->ir.build, value, want, "");
    if (kind == LLVMIntegerTypeKind && want_kind == LLVMPointerTypeKind)
        return LLVMBuildIntToPtr(S->ir.build, value, want, "");
    if (kind == LLVMPointerTypeKind && want_kind == LLVMIntegerTypeKind)
        return LLVMBuildPtrToInt(S->ir.build, value, want, "");
    if (want_kind == LLVMDoubleTypeKind && kind != LLVMDoubleTypeKind)
        return kind == LLVMIntegerTypeKind
                   ? LLVMBuildSIToFP(S->ir.build, value, want, "")
                   : LLVMBuildFPExt(S->ir.build, value, want, "");
    if (want_kind == LLVMFloatTypeKind && kind != LLVMFloatTypeKind)
        return kind == LLVMIntegerTypeKind
                   ? LLVMBuildSIToFP(S->ir.build, value, want, "")
                   : LLVMBuildFPTrunc(S->ir.build, value, want, "");
    return value;
}
static Value fn_named(const char* name, LType func_type) {
    Value func = LLVMGetNamedFunction(S->ir.module, name);
    return func ? func : LLVMAddFunction(S->ir.module, name, func_type);
}
static Value call_fn(Value func, int count, Value* args) {
    LType func_type = LLVMGlobalGetValueType(func);
    LType param_types[34];
    int   param_count = LLVMCountParamTypes(func_type);
    LLVMGetParamTypes(func_type, param_types);
    Type* struct_type = 0;
    for (int i = 0; i + 1 < S->struct_rets.count; i += 2)
        if (S->struct_rets.data[i] == func)
            struct_type = S->struct_rets.data[i + 1];
    int hidden =
        struct_type && LLVMGetReturnType(func_type) == S->ir.void_type;
    Value slot = struct_type ? stack_slot(struct_type->llvm) : 0;
    Value call_args[34];
    if (hidden) call_args[0] = slot;
    for (int i = 0; i < count; i++)
        call_args[i + hidden] =
            fit(args[i],
                i + hidden < param_count ? param_types[i + hidden] : 0);
    Value result = LLVMBuildCall2(S->ir.build, func_type, func,
                                  call_args, count + hidden, "");
    if (!struct_type) return result;
    if (!hidden) LLVMBuildStore(S->ir.build, result, slot);
    return load(struct_type->llvm, slot);
}
static Value call_au(const char* name, const char* sig, int count,
                     ...) { // call a libAu or C function by signature:
                            // ret then params, '.' = varargs
    LType param_types[16];
    int   param_count = 0;
    bool  is_variadic = false;
    for (int i = 1; sig[i]; i++) {
        if (sig[i] == '.') is_variadic = true;
        else param_types[param_count++] = sig_type(sig[i]);
    }
    Value func =
        fn_named(name, LLVMFunctionType(sig_type(sig[0]), param_types,
                                        param_count, is_variadic));
    Value   call_args[16];
    va_list arg_list;
    va_start(arg_list, count);
    for (int i = 0; i < count; i++)
        call_args[i] = va_arg(arg_list, Value);
    va_end(arg_list);
    Value result = call_fn(func, count, call_args);
    return sig[0] == 'b'
               ? LLVMBuildTrunc(S->ir.build, result, S->ir.i1, "")
               : result;
}
static Value env_is_set(const char* key) { // getenv(key) != null
    Value env = call_au("getenv", "pp", 1, const_str(key));
    return LLVMBuildICmp(S->ir.build, LLVMIntNE, env,
                         LLVMConstNull(S->ir.ptr), "");
}
static Value global_var(const char* name, LType type, bool external) {
    Value global = LLVMGetNamedGlobal(S->ir.module, name);
    if (global) return global;
    global = LLVMAddGlobal(S->ir.module, type, name);
    if (!external) LLVMSetInitializer(global, LLVMConstNull(type));
    return global;
}
static void abi_flatten(LType type, long base, LType* type_list,
                        long* offsets, int* count) {
    int kind = LLVMGetTypeKind(type);
    if (kind == LLVMStructTypeKind) {
        for (unsigned i = 0; i < LLVMCountStructElementTypes(type); i++)
            abi_flatten(
                LLVMStructGetTypeAtIndex(type, i),
                base + LLVMOffsetOfElement(S->target_data, type, i),
                type_list, offsets, count);
    } else if (kind == LLVMArrayTypeKind) {
        LType elem      = LLVMGetElementType(type);
        long  elem_size = LLVMABISizeOfType(S->target_data, elem);
        for (unsigned i = 0; i < LLVMGetArrayLength2(type); i++)
            abi_flatten(elem, base + i * elem_size, type_list, offsets,
                        count);
    } else {
        type_list[*count] = type;
        offsets[*count]   = base;
        (*count)++;
    }
}
static LType
abi_ret(Type* type) { // how a value returns in the C ABI: structs over
                      // 16 bytes through a hidden pointer (0), smaller
                      // ones packed per eightbyte
    if (type->kind != T_STRUCT) return llvm_of(type);
    LType struct_type = llvm_of(type);
    long  size        = LLVMABISizeOfType(S->target_data, struct_type);
    if (size > 16) return 0;
    LType type_list[64];
    long  offsets[64];
    int   count = 0;
    abi_flatten(struct_type, 0, type_list, offsets, &count);
    LType parts[2];
    int   part_count = 0;
    for (long low = 0; low < size; low += 8) {
        int   field_count = 0, float_count = 0;
        LType only = 0;
        for (int i = 0; i < count; i++)
            if (offsets[i] >= low && offsets[i] < low + 8) {
                field_count++;
                only          = type_list[i];
                int type_kind = LLVMGetTypeKind(type_list[i]);
                if (type_kind == LLVMFloatTypeKind ||
                    type_kind == LLVMDoubleTypeKind ||
                    type_kind == LLVMHalfTypeKind)
                    float_count++;
            }
        long bytes = size - low < 8 ? size - low : 8;
        parts[part_count++] =
            field_count && field_count == float_count
                ? (field_count == 1 ? only
                                    : LLVMVectorType(S->ir.f32, 2))
                : LLVMIntTypeInContext(S->ir.context, bytes * 8);
    }
    return part_count == 1
               ? parts[0]
               : LLVMStructTypeInContext(S->ir.context, parts, 2, 0);
}
static Value
declare_abi(const char* name, LType* param_types, int param_count,
            Type* result_type) { // declare a function whose return
                                 // follows abi_ret
    Value func = LLVMGetNamedFunction(S->ir.module, name);
    if (func) return func;
    LType result_llvm = abi_ret(result_type);
    if (!result_llvm) {
        for (int i = param_count; i > 0; i--)
            param_types[i] = param_types[i - 1];
        param_types[0] = S->ir.ptr;
        param_count++;
    }
    func =
        fn_named(name, LLVMFunctionType(result_llvm ? result_llvm
                                                    : S->ir.void_type,
                                        param_types, param_count, 0));
    if (result_type->kind == T_STRUCT) {
        if (!result_llvm)
            LLVMAddAttributeAtIndex(
                func, 1,
                LLVMCreateTypeAttribute(
                    S->ir.context,
                    LLVMGetEnumAttributeKindForName("sret", 4),
                    llvm_of(result_type)));
        list_push(&S->struct_rets, func);
        list_push(&S->struct_rets, result_type);
    }
    return func;
}
static long offset_of(LType struct_type, int index) {
    return (long)LLVMOffsetOfElement(S->target_data, struct_type,
                                     index);
}
static LType llvm_of(Type* type) {
    switch (type->kind) {
    case T_INT:
        return LLVMIntTypeInContext(S->ir.context,
                                    type->bits ? type->bits : 64);
    case T_FLOAT:
        return type->bits == 16   ? S->ir.bf16
               : type->bits == 32 ? S->ir.f32
                                  : S->ir.f64;
    case T_BOOL: return S->ir.i1;
    case T_UNICHAR: return S->ir.i32;
    case T_NONE: return S->ir.void_type;
    case T_STRUCT:
        ensure_layout(type);
        return type->llvm
                   ? type->llvm
                   : LLVMArrayType2(S->ir.i8,
                                    type->record
                                        ? ((Au_t)type->record)->typesize
                                        : 1);
    case T_ENUM:
    case T_SCALAR: return llvm_of(backing(type));
    case T_LOCAL:
        return LLVMArrayType2(
            llvm_of(type->elem),
            type->dims && type->dims->count &&
                    type->dims->kids[0]->kind == N_NUM
                ? type->dims->kids[0]->token->int_value
                : 1);
    case T_UNK: return S->ir.i64;
    default: return S->ir.ptr;
    }
}
static Value type_record(
    Type* type) { // the runtime type record of a type: a constant
                  // address, named as Au's Type_i() names it
    Value header_offset = const_i64(HDR);
    if ((type->decl &&
         (type->kind == T_CLASS || type->kind == T_STRUCT ||
          type->kind == T_ENUM || type->kind == T_SCALAR)) ||
        (!type->record &&
         (type->c_decl || !strncmp(type->name, "lamctx", 6))))
        return LLVMConstGEP2(
            S->ir.i8,
            LLVMGetNamedGlobal(
                S->ir.module,
                record_global(format("silver-%s", S->modname),
                              type->name)),
            &header_offset, 1);
    Au_t rec = au_rec_of(type);
    if (!rec) emit_fail("no Au record for %s", type->name);
    return LLVMConstGEP2(
        S->ir.i8,
        global_var(record_global(rec->module && rec->module->ident
                                     ? rec->module->ident
                                     : "Au",
                                 rec->ident),
                   S->ir.i8, true),
        &header_offset, 1);
}
static void ensure_layout(
    Type* type) { // an imported struct: fields from its Au record
    if (type->kind != T_STRUCT || type->llvm || !type->record) return;
    Au_t  rec = type->record;
    LType elem_types[64];
    int   count = 0;
    for (int i = 0; i < rec->members.count; i++) {
        Au_t member = (Au_t)rec->members.origin[i];
        if (member->member_type != AU_MEMBER_VAR) continue;
        Type* member_type = from_au(member->type);
        list_push(&type->field_names, (void*)member->ident);
        list_push(&type->field_types, member_type);
        elem_types[count++] = llvm_of(member_type);
    }
    type->llvm =
        LLVMStructTypeInContext(S->ir.context, elem_types, count, 0);
}
static int field_index(Type* record, const char* name) {
    ensure_layout(record);
    for (int i = 0; i < record->field_names.count; i++)
        if (same(record->field_names.data[i], name))
            return i + (record->kind == T_CLASS ? 2 : 0);
    emit_fail("no field %s on %s", name, record->name);
    return 0;
}
static Type* field_type(Type* record, const char* name) {
    ensure_layout(record);
    for (int i = 0; i < record->field_names.count; i++)
        if (same(record->field_names.data[i], name))
            return record->field_types.data[i];
    return 0;
}
static Value header_of(Value obj) { return byte_ptr(obj, -HDR); }
static Value au_field(Value obj, Au_t member, LType llvm_type) {
    return load(llvm_type, byte_ptr(obj, member->offset));
} // an Au member by its registered offset
static LLVMMetadataRef di_basic(const char* name, int bits,
                                int encoding) {
    return LLVMDIBuilderCreateBasicType(S->di, name, strlen(name), bits,
                                        encoding, LLVMDIFlagZero);
}
static LLVMMetadataRef di_pointer(LLVMMetadataRef to,
                                  const char*     name) {
    return LLVMDIBuilderCreatePointerType(S->di, to, 64, 0, 0, name,
                                          strlen(name));
}
static LLVMMetadataRef di_member(const char* name, long offset,
                                 int bits, LLVMMetadataRef type) {
    return LLVMDIBuilderCreateMemberType(
        S->di, S->di_unit, name, strlen(name), S->di_file, 0, bits, 0,
        offset * 8, LLVMDIFlagZero, type);
}
static LLVMMetadataRef di_struct(const char* name, long size,
                                 LLVMMetadataRef* members, int count) {
    return LLVMDIBuilderCreateStructType(
        S->di, S->di_unit, name, strlen(name), S->di_file, 0, size * 8,
        0, LLVMDIFlagZero, 0, members, count, 0, 0, "", 0);
}
static LLVMMetadataRef di_type(Type* type);
static LLVMMetadataRef di_type_record(Type* type);
static LLVMMetadataRef
di_au_t(void) { // the generic Au_t record: what a type record looks
                // like before a class refines it
    if (S->di_au_t) return S->di_au_t;
    LLVMMetadataRef fwd = LLVMDIBuilderCreateReplaceableCompositeType(
        S->di, 0x13, "Au_t_", 5, S->di_unit, S->di_file, 0, 0, 0, 0,
        LLVMDIFlagZero, "", 0);
    S->di_au_t =
        di_pointer(fwd, "Au_t"); // members refer back through it
    LLVMMetadataRef au_t = S->di_au_t,
                    chars =
                        di_pointer(di_basic("char", 8, 0x06), "cstr"),
                    ptr  = di_pointer(di_basic("u8", 8, 0x08), "ptr");
    LLVMMetadataRef i32t = di_basic("i32", 32, 0x05),
                    i64t = di_basic("i64", 64, 0x05),
                    u8t  = di_basic("u8", 8, 0x08),
                    u64t = di_basic("u64", 64, 0x07);
    LLVMMetadataRef micro_members[3] = {
        di_member("origin", 0, 64, ptr),
        di_member("count", 8, 32, i32t),
        di_member("alloc", 12, 32, i32t)};
    LLVMMetadataRef micro =
        di_struct("micro_", sizeof(micro_), micro_members, 3);
    LLVMMetadataRef meta_members[3] = {di_member("a", 0, 64, au_t),
                                       di_member("b", 8, 64, ptr),
                                       di_member("m", 16, 64, au_t)};
    LLVMMetadataRef meta =
        di_struct("meta_t_", sizeof(meta_t_), meta_members,
                  3); // the meta triple stays generic
#define AU_T_MEMBERS(SELF, PARENT, FT)                                 \
    di_member("context", offsetof(struct _Au_t, context), 64, PARENT), \
        di_member("type", offsetof(struct _Au_t, type), 64, au_t),     \
        di_member("schema", offsetof(struct _Au_t, schema), 64, au_t), \
        di_member("module", offsetof(struct _Au_t, module), 64, au_t), \
        di_member("ident", offsetof(struct _Au_t, ident), 64, chars),  \
        di_member("alt", offsetof(struct _Au_t, alt), 64, chars),      \
        di_member("source", offsetof(struct _Au_t, source), 64,        \
                  chars),                                              \
        di_member("src_line", offsetof(struct _Au_t, src_line), 32,    \
                  i32t),                                               \
        di_member("table_size", offsetof(struct _Au_t, table_size),    \
                  32, i32t),                                           \
        di_member("member_index",                                      \
                  offsetof(struct _Au_t, member_index), 64, i64t),     \
        di_member("af_index", offsetof(struct _Au_t, af_index), 64,    \
                  i64t),                                               \
        di_member("value", offsetof(struct _Au_t, value), 64, ptr),    \
        di_member("member_type", offsetof(struct _Au_t, member_type),  \
                  8, u8t),                                             \
        di_member("operator_type",                                     \
                  offsetof(struct _Au_t, operator_type), 8, u8t),      \
        di_member("access_type", offsetof(struct _Au_t, access_type),  \
                  8, u8t),                                             \
        di_member("traits", offsetof(struct _Au_t, traits), 64, u64t), \
        di_member("offset", offsetof(struct _Au_t, offset), 32, i32t), \
        di_member("typesize", offsetof(struct _Au_t, typesize), 32,    \
                  i32t),                                               \
        di_member("members", offsetof(struct _Au_t, members),          \
                  sizeof(micro_) * 8, micro),                          \
        di_member("args", offsetof(struct _Au_t, args),                \
                  sizeof(micro_) * 8, micro),                          \
        di_member("meta", offsetof(struct _Au_t, meta),                \
                  sizeof(meta_t_) * 8, meta),                          \
        FT
    LLVMMetadataRef members[24] = {AU_T_MEMBERS(
        au_t, au_t,
        di_member("ft", offsetof(struct _Au_t, ft), 64, ptr))};
    LLVMMetadataRef record =
        di_struct("Au_t_", sizeof(struct _Au_t), members, 22);
    LLVMMetadataReplaceAllUsesWith(fwd, record);
    return S->di_au_t;
}
static LLVMMetadataRef
di_object(void) { // the Au header that sits before every object
    if (S->di_object) return S->di_object;
    LLVMMetadataRef au_t = di_au_t(),
                    ptr  = di_pointer(di_basic("u8", 8, 0x08), "ptr"),
                    chars =
                        di_pointer(di_basic("char", 8, 0x06), "cstr");
    LLVMMetadataRef i32t        = di_basic("i32", 32, 0x05),
                    i64t        = di_basic("i64", 64, 0x05),
                    u64t        = di_basic("u64", 64, 0x07);
    LLVMMetadataRef members[15] = {
        di_member("type", offsetof(struct _object, type), 64, au_t),
        di_member("shape", offsetof(struct _object, shape), 64, ptr),
        di_member("scalar", offsetof(struct _object, scalar), 64, au_t),
        di_member("refs", offsetof(struct _object, refs), 32, i32t),
        di_member("managed", offsetof(struct _object, managed), 32,
                  i32t),
        di_member("data", offsetof(struct _object, data), 64, ptr),
        di_member("source", offsetof(struct _object, source), 64,
                  chars),
        di_member("line", offsetof(struct _object, line), 32, i32t),
        di_member("sequence", offsetof(struct _object, sequence), 32,
                  i32t),
        di_member("alloc", offsetof(struct _object, alloc), 64, i64t),
        di_member("count", offsetof(struct _object, count), 64, i64t),
        di_member("iflags", offsetof(struct _object, iflags), 64, u64t),
        di_member("meta_a", offsetof(struct _object, meta_a), 64, au_t),
        di_member("bind", offsetof(struct _object, bind), 64, chars),
        di_member("holder", offsetof(struct _object, holder), 64,
                  au_t)};
    return S->di_object = di_struct("Au_object", HDR, members, 15);
}
static LLVMMetadataRef di_signature(
    Type* self, Au_t member, Node* method,
    Type* owner) { // a function table slot: (X, args) -> result
    LLVMMetadataRef types[34];
    int             count = 0;
    if (member) {
        types[count++] = di_type(from_au(member->type));
        for (int i = 0; i < member->args.count; i++)
            types[count++] =
                i == 0 ? di_type(self)
                       : di_type(from_au(
                             ((Au_t)member->args.origin[i])->src));
    } else {
        Type* result   = fn_ret(method);
        types[count++] = result->kind == T_NONE ? 0 : di_type(result);
        types[count++] = di_type(self);
        Node* params   = method->kids[0];
        for (int i = 0; i < params->count && count < 34; i++)
            types[count++] = di_type(param_type(params->kids[i]));
    }
    return di_pointer(
        LLVMDIBuilderCreateSubroutineType(S->di, S->di_file, types,
                                          count, LLVMDIFlagZero),
        "");
}
static LLVMMetadataRef di_type_record(
    Type* type) { // the class's unique Au_t: its parent's record as
                  // context, its function table by slot
    for (int i = 0; i + 1 < S->di_records.count; i += 2)
        if (S->di_records.data[i] == type)
            return S->di_records.data[i + 1];
    LLVMMetadataRef fwd = LLVMDIBuilderCreateReplaceableCompositeType(
        S->di, 0x13, type->name, strlen(type->name), S->di_unit,
        S->di_file, 0, 0, 0, 0, LLVMDIFlagZero, "", 0);
    LLVMMetadataRef self_ptr =
        di_pointer(fwd, format("%s_f", type->name));
    list_push(&S->di_records, type);
    list_push(&S->di_records, self_ptr);
    LLVMMetadataRef au_t = di_au_t(),
                    chars =
                        di_pointer(di_basic("char", 8, 0x06), "cstr"),
                    ptr  = di_pointer(di_basic("u8", 8, 0x08), "ptr");
    LLVMMetadataRef i32t = di_basic("i32", 32, 0x05),
                    i64t = di_basic("i64", 64, 0x05),
                    u8t  = di_basic("u8", 8, 0x08),
                    u64t = di_basic("u64", 64, 0x07);
    LLVMMetadataRef micro_members[3] = {
        di_member("origin", 0, 64, ptr),
        di_member("count", 8, 32, i32t),
        di_member("alloc", 12, 32, i32t)};
    LLVMMetadataRef micro =
        di_struct("micro_", sizeof(micro_), micro_members, 3);
    LLVMMetadataRef meta_members[3] = {di_member("a", 0, 64, au_t),
                                       di_member("b", 8, 64, ptr),
                                       di_member("m", 16, 64, au_t)};
    LLVMMetadataRef meta =
        di_struct("meta_t_", sizeof(meta_t_), meta_members, 3);
    LLVMMetadataRef parent     = type->base && type->base->decl
                                     ? di_type_record(type->base)
                                     : au_t;
    LLVMMetadataRef slots[128] = {0};
    int             slot_count = 0;
    Au_t            root       = au_type("Au");
    for (int i = 0; i < root->members.count; i++) {
        Au_t member =
            (Au_t)root->members
                .origin[i]; // Au's own methods first, at their slots
        if (member->member_type == AU_MEMBER_FUNC &&
            member->member_index > 0 && member->member_index < 128) {
            slots[member->member_index] =
                di_member(member->ident, member->member_index * 8, 64,
                          di_signature(type, member, 0, 0));
            if (member->member_index >= slot_count)
                slot_count = member->member_index + 1;
        }
    }
    Type* chain[32];
    int   depth = 0;
    for (Type* cur = type; cur && cur->decl; cur = cur->base)
        chain[depth++] = cur;
    for (int k = depth - 1; k >= 0; k--) {
        Node* decl =
            chain[k]->decl; // then the class chain, base first:
                            // overrides land on the same slot
        for (int i = 1; i < decl->count; i++) {
            Node* method = decl->kids[i];
            if (method->kind == D_MEMBER) continue;
            const char* cname = method_cname(chain[k], method);
            int         slot  = -1;
            for (int q = 0; q + 1 < S->ft_slots.count; q += 2)
                if (same(S->ft_slots.data[q], cname))
                    slot = (int)(long)S->ft_slots.data[q + 1];
            if (slot < 0 || slot >= 128) continue;
            slots[slot] =
                di_member(member_name(method), slot * 8, 64,
                          di_signature(type, 0, method, chain[k]));
            if (slot >= slot_count) slot_count = slot + 1;
        }
    }
    LLVMMetadataRef entries[128];
    int             entry_count = 0;
    for (int i = 0; i < slot_count; i++)
        if (slots[i]) entries[entry_count++] = slots[i];
    LLVMMetadataRef table =
        di_struct(format("%s_ft", type->name), slot_count * 8, entries,
                  entry_count);
    LLVMMetadataRef members[24] = {
        AU_T_MEMBERS(self_ptr, parent,
                     di_member("ft", offsetof(struct _Au_t, ft),
                               slot_count * 64, table))};
    LLVMMetadataRef record = di_struct(
        format("%s_f", type->name),
        offsetof(struct _Au_t, ft) + slot_count * 8, members, 22);
    LLVMMetadataReplaceAllUsesWith(fwd, record);
    return self_ptr;
}
static LLVMMetadataRef
di_type(Type* type) { // a silver type as DWARF: numbers as they are,
                      // records with their fields, the rest a pointer
    for (int i = 0; i + 1 < S->di_types.count; i += 2)
        if (S->di_types.data[i] == type) return S->di_types.data[i + 1];
    LLVMMetadataRef result;
    Type*           base = backing(type);
    if (base->kind == T_INT)
        result = di_basic(type->name, base->bits ? base->bits : 64,
                          base->is_signed ? 0x05 : 0x07);
    else if (base->kind == T_FLOAT)
        result =
            di_basic(type->name, base->bits ? base->bits : 64, 0x04);
    else if (base->kind == T_BOOL) result = di_basic("bool", 8, 0x02);
    else if (base->kind == T_UNICHAR)
        result = di_basic("unichar", 32, 0x08);
    else if (type->kind == T_NONE) return 0;
    else if ((type->kind == T_CLASS || type->kind == T_STRUCT) &&
             type->decl && type->llvm) {
        LLVMMetadataRef opaque =
            di_pointer(di_basic("u8", 8, 0x08), type->name);
        list_push(&S->di_types, type);
        list_push(&S->di_types,
                  opaque); // a record inside itself stays a pointer
        LLVMMetadataRef members[132];
        int             count = 0;
        int             first = type->kind == T_CLASS ? 2 : 0;
        if (type->kind ==
            T_CLASS) { // the instance: its own type record, the af
                       // bits, then the fields
            members[count++] =
                di_member("au", 0, 64, di_type_record(type));
            LLVMMetadataRef sub[1] = {
                LLVMDIBuilderGetOrCreateSubrange(S->di, 0, 4)};
            members[count++] = di_member(
                "af_bits", 8, 256,
                LLVMDIBuilderCreateArrayType(
                    S->di, 256, 0, di_basic("u64", 64, 0x07), sub, 1));
        }
        for (int i = 0; i < type->field_names.count && count < 132;
             i++) {
            Type*           field_type = type->field_types.data[i];
            LLVMMetadataRef field_di   = di_type(field_type);
            if (!field_di) continue;
            const char* name = type->field_names.data[i];
            members[count++] = LLVMDIBuilderCreateMemberType(
                S->di, S->di_unit, name, strlen(name), S->di_file,
                type->decl->line,
                LLVMABISizeOfType(S->target_data, llvm_of(field_type)) *
                    8,
                0,
                LLVMOffsetOfElement(S->target_data, type->llvm,
                                    i + first) *
                    8,
                LLVMDIFlagZero, field_di);
        }
        LLVMMetadataRef record = LLVMDIBuilderCreateStructType(
            S->di, S->di_unit, type->name, strlen(type->name),
            S->di_file, type->decl->line,
            LLVMABISizeOfType(S->target_data, type->llvm) * 8, 0,
            LLVMDIFlagZero, 0, members, count, 0, 0, "", 0);
        result = type->kind == T_CLASS ? di_pointer(record, type->name)
                                       : record;
        for (int i = 0; i + 1 < S->di_types.count; i += 2)
            if (S->di_types.data[i] == type)
                S->di_types.data[i + 1] = result;
        return result;
    } else result = di_pointer(di_basic("u8", 8, 0x08), type->name);
    list_push(&S->di_types, type);
    list_push(&S->di_types, result);
    return result;
}
static void set_line(int line) { // the statement's line rides on every
                                 // instruction from here
    S->cur_line = line;
    if (S->cur_fn && S->cur_fn->di_scope)
        LLVMSetCurrentDebugLocation2(
            S->ir.build,
            LLVMDIBuilderCreateDebugLocation(S->ir.context, line, 0,
                                             S->cur_fn->di_scope, 0));
}
static Value
clock_ns(void) { // clock_gettime(CLOCK_MONOTONIC) as nanoseconds
    LType stamp = LLVMStructTypeInContext(
        S->ir.context, (LType[]){S->ir.i64, S->ir.i64}, 2, 0);
    Value slot    = stack_slot(stamp);
    Value args[2] = {const_i32(1), slot};
    call_fn(fn_named("clock_gettime",
                     LLVMFunctionType(S->ir.i32,
                                      (LType[]){S->ir.i32, S->ir.ptr},
                                      2, 0)),
            2, args);
    Value sec  = load(S->ir.i64, field_ptr(stamp, slot, 0));
    Value nsec = load(S->ir.i64, field_ptr(stamp, slot, 1));
    return LLVMBuildAdd(
        S->ir.build,
        LLVMBuildMul(S->ir.build, sec, const_i64(1000000000LL), ""),
        nsec, "");
}
static void timing_end(
    void) { // --timing: add this call's time to the function's total
    if (!S->cur_fn || !S->cur_fn->timing_start) return;
    Value elapsed = LLVMBuildSub(S->ir.build, clock_ns(),
                                 S->cur_fn->timing_start, "");
    Value slot    = index_ptr(S->ir.i64, S->timings,
                              const_i64(S->cur_fn->timing_id));
    LLVMBuildStore(
        S->ir.build,
        LLVMBuildAdd(S->ir.build, load(S->ir.i64, slot), elapsed, ""),
        slot);
}

// ----------------------------------------------------------------
// emission
static Var* find_var(Scope* scope, const char* name) {
    for (; scope; scope = scope->parent)
        for (Var* var = scope->vars; var; var = var->next)
            if (same(var->name, name)) return var;
    return 0;
}
static Var* declare_var(Scope* scope, const char* name, Type* type,
                        Value address) {
    Var* var     = calloc(1, sizeof(Var));
    var->name    = strdup(name);
    var->type    = type;
    var->address = address;
    var->next    = scope->vars;
    scope->vars  = var;
    if (S->cur_fn && S->cur_fn->di_scope && address &&
        LLVMIsAAllocaInst(address) &&
        name[0] != '$') { // lldb sees it by name
        LLVMMetadataRef info = LLVMDIBuilderCreateAutoVariable(
            S->di, S->cur_fn->di_scope, name, strlen(name), S->di_file,
            S->cur_line, di_type(type), true, LLVMDIFlagZero, 0);
        LLVMDIBuilderInsertDeclareRecordAtEnd(
            S->di, address, info,
            LLVMDIBuilderCreateExpression(S->di, 0, 0),
            LLVMGetCurrentDebugLocation2(S->ir.build), cur_block());
        if (type->kind == T_CLASS &&
            type->decl) { // the Au header before the object:
                          // <name>_header, at the object minus HDR
            char*           header_name = format("%s_header", name);
            LLVMMetadataRef header_info =
                LLVMDIBuilderCreateAutoVariable(
                    S->di, S->cur_fn->di_scope, header_name,
                    strlen(header_name), S->di_file, S->cur_line,
                    di_object(), true, LLVMDIFlagZero, 0);
            uint64_t ops[4] = {
                0x06, 0x10, HDR,
                0x1c}; // DW_OP_deref, DW_OP_constu HDR, DW_OP_minus
            LLVMDIBuilderInsertDeclareRecordAtEnd(
                S->di, address, header_info,
                LLVMDIBuilderCreateExpression(S->di, ops, 4),
                LLVMGetCurrentDebugLocation2(S->ir.build), cur_block());
        }
    }
    return var;
}
static Val  eval(Node* node, Scope* scope, Type* want);
static Val  lvalue_of(Node* node, Scope* scope);
static void emit_stmt(Node* node, Scope* scope);
static void emit_block(Node* block, Scope* scope);
static Val  make_val(Value value, Type* type) {
    Val result = {value, type};
    return result;
}
static Value c_call(CD* func, Value self, Node* args, Scope* scope,
                    Type** out);
static Value au_alloc(Type* type, int line) {
    return call_au("alloc", "pplppppii", 8, type_record(type),
                   const_i64(1), LLVMConstNull(S->ir.ptr),
                   LLVMConstNull(S->ir.ptr), LLVMConstNull(S->ir.ptr),
                   const_str(S->cur_file), const_i32(line),
                   const_i32(0));
}
static Value au_init(Value obj) {
    return call_au("Au_initialize", "pp", 1, obj);
}
static Value new_string(const char* ctor, const char* sig, Value arg) {
    Value str          = au_alloc(basic(T_STRING), 0);
    Value call_args[2] = {str, arg};
    Value func         = fn_named(
        ctor,
        LLVMFunctionType(S->ir.ptr,
                                 (LType[]){S->ir.ptr, sig_type(sig[0])}, 2, 0));
    call_fn(func, 2, call_args);
    return au_init(str);
}
static Val lit_str(const char* str) {
    return make_val(new_string("string_with_cstr", "p", const_str(str)),
                    basic(T_STRING));
}
static Value chars_of(Value str, Type* type) {
    return au_field(
        str,
        au_member(
            au_type(type && type->kind == T_PATH ? "path" : "string"),
            "chars", 0),
        S->ir.ptr);
} // path has its own layout
static Value to_bool(Val val) {
    Type* type = val.type;
    if (type->kind == T_BOOL) return val.value;
    if (type->kind == T_CLASS && type->decl) {
        Type* owner;
        Node* cast = find_cast(type, "bool", &owner);
        if (cast) {
            Value func = LLVMGetNamedFunction(
                S->ir.module, method_cname(owner, cast));
            return call_fn(func, 1, &val.value);
        }
    }
    LType        llvm_type = LLVMTypeOf(val.value);
    LLVMTypeKind kind      = LLVMGetTypeKind(llvm_type);
    if (kind == LLVMPointerTypeKind || kind == LLVMIntegerTypeKind)
        return LLVMBuildICmp(S->ir.build, LLVMIntNE, val.value,
                             LLVMConstNull(llvm_type), "");
    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind ||
        kind == LLVMBFloatTypeKind)
        return LLVMBuildFCmp(S->ir.build, LLVMRealONE, val.value,
                             LLVMConstNull(llvm_type), "");
    emit_fail("no truth value for %s", type->name);
    return 0;
}
static Value fmt_str(const char* spec, Value value, LType llvm_type) {
    Value buf          = stack_slot(LLVMArrayType2(S->ir.i8, 128));
    Value call_args[4] = {buf, const_i64(128), const_str(spec),
                          fit(value, llvm_type)};
    call_fn(
        fn_named("snprintf",
                 LLVMFunctionType(
                     S->ir.i32,
                     (LType[]){S->ir.ptr, S->ir.i64, S->ir.ptr}, 3, 1)),
        4, call_args);
    return new_string("string_with_cstr", "p", buf);
}
static Value au_cast_string(Val value);
static Value au_call_member(Au_t member, Value self, int count,
                            Value* args, Type** out);
static Value to_string(Val val) {
    Type* type = val.type;
    switch (type->kind) {
    case T_STRING:
    case T_PATH: return val.value;
    case T_CSTR: return new_string("string_with_cstr", "p", val.value);
    case T_INT:
        return type->is_signed || !type->bits
                   ? new_string("string_with_i64", "l",
                                fit(val.value, S->ir.i64))
                   : new_string("string_with_u64", "l",
                                LLVMBuildZExt(S->ir.build, val.value,
                                              S->ir.i64, ""));
    case T_FLOAT:
        return new_string("string_with_f64", "d",
                          fit(val.value, S->ir.f64));
    case T_BOOL:
        return new_string("string_with_cstr", "p",
                          LLVMBuildSelect(S->ir.build, val.value,
                                          const_str("true"),
                                          const_str("false"), ""));
    case T_UNICHAR:
        return new_string("string_with_unichar", "i", val.value);
    case T_SCALAR: {
        Value str = to_string(make_val(val.value, type->base));
        call_au("string_append", "vpp", 2, str, const_str(type->name));
        return str;
    }
    case T_ENUM:
        return new_string(
            "string_with_cstr", "p",
            chars_of(call_au("estring", "ppi", 2, type_record(type),
                             fit(val.value, S->ir.i32)),
                     0));
    case T_TYPE:
        return new_string(
            "string_with_cstr", "p",
            load(S->ir.ptr,
                 byte_ptr(val.value, offsetof(struct _Au_t, ident))));
    case T_STRUCT: {
        Type* owner;
        Node* cast = type->decl ? find_cast(type, "string", &owner) : 0;
        if (cast) {
            Value ptr = addr_of(val.value);
            return call_fn(LLVMGetNamedFunction(
                               S->ir.module, method_cname(owner, cast)),
                           1, &ptr);
        }
        return lit_str(type->name).value;
    }
    case T_REF:
    case T_HANDLE: return fmt_str("%p", val.value, S->ir.ptr);
    default: return au_cast_string(val);
    }
}
static Value au_cast_string(
    Val val) { // an object to string: its cast, else its type name
    if (val.type->decl) {
        Type* owner;
        Node* cast = find_cast(val.type, "string", &owner);
        if (cast) {
            Value ptr = val.type->kind == T_STRUCT ? addr_of(val.value)
                                                   : val.value;
            return call_fn(LLVMGetNamedFunction(
                               S->ir.module, method_cname(owner, cast)),
                           1, &ptr);
        }
        return lit_str(val.type->name).value;
    }
    Au_t rec = au_rec_of(val.type);
    Au_t member =
        rec ? au_member(rec, "cast_string", AU_MEMBER_CAST) : 0;
    if (member) {
        Type* owner;
        return au_call_member(member, val.value, 0, 0, &owner);
    }
    return new_string(
        "string_with_cstr", "p",
        load(S->ir.ptr, byte_ptr(load(S->ir.ptr, header_of(val.value)),
                                 offsetof(struct _Au_t, ident))));
}
static Value number_cast(Value value, Type* from,
                         Type* to) { // between numeric representations
    LType from_llvm = LLVMTypeOf(value), to_llvm = llvm_of(to);
    if (from_llvm == to_llvm) return value;
    Type* from_base = backing(from);
    Type* to_base   = backing(to);
    bool  from_float =
        LLVMGetTypeKind(from_llvm) == LLVMFloatTypeKind ||
        LLVMGetTypeKind(from_llvm) == LLVMDoubleTypeKind ||
        LLVMGetTypeKind(from_llvm) == LLVMBFloatTypeKind;
    bool to_float = to_base->kind == T_FLOAT;
    if (LLVMGetTypeKind(from_llvm) == LLVMPointerTypeKind)
        value = LLVMBuildPtrToInt(S->ir.build, value, S->ir.i64, ""),
        from_llvm = S->ir.i64, from_float = false;
    if (from_float && to_float)
        return (to_llvm == S->ir.f64 ? 64
                : to_llvm == S->ir.f32
                    ? 32
                    : 16) > (from_llvm == S->ir.f64   ? 64
                             : from_llvm == S->ir.f32 ? 32
                                                      : 16)
                   ? LLVMBuildFPExt(S->ir.build, value, to_llvm, "")
                   : LLVMBuildFPTrunc(S->ir.build, value, to_llvm, "");
    if (from_float)
        return to_base->kind == T_BOOL
                   ? LLVMBuildFCmp(S->ir.build, LLVMRealONE, value,
                                   LLVMConstNull(from_llvm), "")
                   : LLVMBuildFPToSI(S->ir.build, value, to_llvm, "");
    if (to_float)
        return from_base->is_signed || from_base->kind == T_BOOL
                   ? LLVMBuildSIToFP(S->ir.build, value, to_llvm, "")
                   : LLVMBuildUIToFP(S->ir.build, value, to_llvm, "");
    if (to_base->kind == T_BOOL)
        return LLVMBuildICmp(S->ir.build, LLVMIntNE, value,
                             LLVMConstNull(from_llvm), "");
    unsigned from_width = LLVMGetIntTypeWidth(from_llvm),
             to_width   = LLVMGetIntTypeWidth(to_llvm);
    return from_width < to_width
               ? (from_base->kind == T_BOOL || !from_base->is_signed
                      ? LLVMBuildZExt(S->ir.build, value, to_llvm, "")
                      : LLVMBuildSExt(S->ir.build, value, to_llvm, ""))
               : LLVMBuildTrunc(S->ir.build, value, to_llvm, "");
}
static Value box_value(Val val) { // a primitive as an Au object
    Type* type = backing(val.type);
    if (!is_num(type) && type->kind != T_CSTR) return val.value;
    LType llvm_type = llvm_of(type);
    Value value =
        type->kind == T_BOOL
            ? LLVMBuildZExt(S->ir.build, val.value, S->ir.i8, "")
            : val.value;
    return call_au("primitive", "ppp", 2, type_record(type),
                   addr_of(value));
}
static Value cast_value(Val val, Type* to) {
    Type* from  = val.type;
    Value value = val.value;
    if (!to || to->kind == T_UNK || from->kind == T_NONE) return value;
    if (from->kind == T_UNK) return fit(value, llvm_of(to));
    switch (to->kind) {
    case T_STRING:
        if (is_str(from)) return value;
        if (from->kind == T_OBJECT || from->kind == T_HANDLE)
            return value;
        if (from->kind == T_REF)
            return new_string("string_with_cstr", "p", value);
        if (from->kind == T_CLASS) return au_cast_string(val);
        return to_string(val);
    case T_PATH:
        if (from->kind == T_STRING) {
            Value ptr = au_alloc(to, 0);
            call_au("path_with_string", "ppp", 2, ptr, value);
            return au_init(ptr);
        }
        return value;
    case T_CSTR:
        if (is_str(from)) return chars_of(value, from);
        if (from->kind == T_UNICHAR) return chars_of(to_string(val), 0);
        return fit(value, S->ir.ptr);
    case T_BOOL: return to_bool(val);
    case T_INT:
    case T_FLOAT:
    case T_UNICHAR:
        if (from->kind == T_OBJECT) return load(llvm_of(to), value);
        if (from->c_decl) {
            CD* rec = from->c_decl;
            for (int i = 0; i < rec->member_count; i++)
                if (rec->members[i]->kind == CD_CONV &&
                    (rec->members[i]->result->kind == 'f') ==
                        (to->kind == T_FLOAT)) {
                    Type* owner;
                    Node* no_args = new_node(N_ARGS, 0, 0);
                    return number_cast(c_call(rec->members[i], value,
                                              no_args, 0, &owner),
                                       owner, to);
                }
        }
        return number_cast(value, from, to);
    case T_ENUM:
        if (is_str(from))
            return number_cast(call_au("evalue", "ipp", 2,
                                       type_record(to),
                                       chars_of(value, 0)),
                               type_named("i32"), to);
        return number_cast(value, from, to);
    case T_SCALAR:
        if (from->kind == T_SCALAR && !same(from->name, to->name)) {
            Node* decl = to->decl;
            for (int i = 1; i < decl->count; i++)
                if (decl->kids[i]->kind == D_CTOR &&
                    same(decl->kids[i]->kids[0]->kids[0]->kids[0]->text,
                         from->name))
                    return call_fn(
                        LLVMGetNamedFunction(
                            S->ir.module,
                            format("%s_with_%s", to->name, from->name)),
                        1, &value);
            if (find_cast(from, to->name, 0))
                return call_fn(
                    LLVMGetNamedFunction(
                        S->ir.module,
                        format("%s_cast_%s", from->name, to->name)),
                    1, &value);
        }
        return number_cast(value, from, to);
    case T_OBJECT: return box_value(val);
    case T_REF:
        if (from->kind == T_VEC)
            return au_field(value,
                            au_member(au_type("vector"), "origin", 0),
                            S->ir.ptr);
        return fit(value, S->ir.ptr);
    case T_HANDLE:
        if (from->kind == T_VEC)
            return au_field(value,
                            au_member(au_type("vector"), "origin", 0),
                            S->ir.ptr);
        return fit(value, S->ir.ptr);
    case T_STRUCT:
        if (from->kind == T_STRUCT && !same(from->name, to->name))
            return load(to->llvm, addr_of(value));
        return value;
    default: return fit(value, S->ir.ptr);
    }
}
static Value eval_as(Node* node, Scope* scope, Type* to) {
    return cast_value(eval(node, scope, to), to);
}
static Value hold_value(Value value) {
    return call_au("hold", "pp", 1, value);
}
static void emit_raise(Value msg) {
    call_au("au_error_raise", "vpp", 2, msg, LLVMConstNull(S->ir.ptr));
}
static void set_slot(Value owner, Value slot, Value value) {
    Value prev = load(S->ir.ptr, slot);
    LLVMBuildStore(S->ir.build, value, slot);
    call_au("Au_slot_replace", "vppp", 3, owner, value, prev);
}
static Value interpolate(const char* text, Scope* scope) {
    Value result  = new_string("string_with_cstr", "p", const_str(""));
    Buf   literal = {0};
#define FLUSH                                                          \
    if (literal.count) {                                               \
        call_au("string_append", "vpp", 2, result,                     \
                const_str(literal.data));                              \
        literal = (Buf){0};                                            \
    }
    for (const char* cursor = text; *cursor;) {
        if ((*cursor == '{' || *cursor == '}') &&
            cursor[1] == *cursor) {
            append(&literal, "%c", *cursor);
            cursor += 2;
            continue;
        }
        if (*cursor == '{') {
            FLUSH const char* end   = cursor + 1;
            int               depth = 1;
            while (*end && depth) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                if (depth) end++;
            }
            char*  inner        = strndup(cursor + 1, end - cursor - 1);
            Token* saved_tokens = S->tokens;
            int saved_pos = S->pos, saved_count = S->token_count, count;
            Token* inner_tokens = tokenize(inner, &count);
            S->tokens           = inner_tokens;
            S->pos              = 0;
            S->token_count      = count;
            Node* expr          = parse_expr();
            S->tokens           = saved_tokens;
            S->pos              = saved_pos;
            S->token_count      = saved_count;
            call_au("string_append", "vpp", 2, result,
                    chars_of(to_string(eval(expr, scope, 0)), 0));
            cursor = end + 1;
            continue;
        }
        if (*cursor == '\\' && cursor[1]) {
            const char* escapes = "n\nt\tr\r0\0\\\\''\"\"";
            char        ch      = cursor[1];
            for (int k = 0; k < 14; k += 2)
                if (escapes[k] == ch) ch = escapes[k + 1];
            append(&literal, "%c", ch);
            cursor += 2;
            continue;
        }
        append(&literal, "%c", *cursor);
        cursor++;
    }
    FLUSH return result;
#undef FLUSH
}
static Value
args_call(Node* args, Scope* scope, Node* params, Value self,
          Value* call_args, int* out_count,
          Type* owner) { // positional args matched to params; commaless
                         // args match by type; struct methods take
                         // struct arguments by address
    int count = 0;
    if (self) call_args[count++] = self;
    bool by_addr     = owner && owner->kind == T_STRUCT;
    int  param_count = params ? params->flag : args->count;
    bool used[64]    = {0};
    for (int i = 0; i < param_count; i++) {
        Type* want = params ? param_type(params->kids[i]) : 0;
        Node* arg  = 0;
        if (!params || args->count == param_count)
            arg = i < args->count ? args->kids[i] : 0;
        else
            for (int j = 0; j < args->count && !arg; j++)
                if (!used[j]) {
                    Type* arg_type = 0;
                    Node* cand     = args->kids[j];
                    if (cand->kind == N_SCALARLIT)
                        arg_type = type_named(cand->text);
                    else if (cand->kind == N_NUM) arg_type = lit_int();
                    else if (cand->kind == N_IDENT) {
                        Var* var = find_var(scope, cand->text);
                        arg_type = var ? var->type : 0;
                    }
                    if (arg_type &&
                        (same_type(arg_type, want) ||
                         (is_num(arg_type) && is_num(want) &&
                          arg_type->kind != T_SCALAR &&
                          want->kind != T_SCALAR))) {
                        arg     = cand;
                        used[j] = true;
                    }
                }
        if (!arg) emit_fail("missing argument %d", i);
        Val val =
            eval(arg->kind == N_PROP ? arg->kids[1] : arg, scope, want);
        call_args[count++] =
            want               ? (by_addr && want->kind == T_STRUCT
                                      ? addr_of(cast_value(val, want))
                                      : cast_value(val, want))
            : is_str(val.type) ? chars_of(val.value, val.type)
            : val.type->kind == T_VEC ? cast_value(val, basic(T_HANDLE))
                                      : val.value;
    }
    *out_count = count;
    return 0;
}
static LType lambda_fty(Type* lambda_t) {
    LType param_types[17];
    param_types[0] = S->ir.ptr;
    for (int i = 0; i < lambda_t->arg_count; i++)
        param_types[i + 1] = llvm_of(lambda_t->args[i]);
    return LLVMFunctionType(llvm_of(lambda_t->result), param_types,
                            lambda_t->arg_count + 1, 0);
}
static Val call_lambda(Value func, Type* lambda_t, Node* args,
                       Scope* scope) {
    Au_t  lambda_rec = au_type("lambda");
    Value call_args[17];
    call_args[0] =
        au_field(func, au_member(lambda_rec, "context", 0), S->ir.ptr);
    for (int i = 0; i < args->count; i++)
        call_args[i + 1] =
            i < lambda_t->arg_count
                ? eval_as(args->kids[i], scope, lambda_t->args[i])
                : eval(args->kids[i], scope, 0).value;
    return make_val(
        LLVMBuildCall2(
            S->ir.build, lambda_fty(lambda_t),
            au_field(func, au_member(lambda_rec, "vfn", 0), S->ir.ptr),
            call_args, args->count + 1, ""),
        lambda_t->result);
}
static Type* lambda_type(Type* result_type, Node* params, int first,
                         int last) {
    Type* type   = new_type(T_LAMBDA, "lambda");
    type->result = result_type;
    type->args   = calloc(last - first + 1, sizeof(Type*));
    for (int i = first; i < last; i++)
        type->args[type->arg_count++] = param_type(params->kids[i]);
    return type;
}
static void collect_idents(Node* node, List* out) {
    if (!node) return;
    if (node->kind == N_IDENT) list_push(out, node->text);
    for (int i = 0; i < node->count; i++)
        collect_idents(node->kids[i], out);
}
static Value emit_function(Node* func_node, Type* class_type,
                           Value func, Type* context_type,
                           List* captures, Type** result_out);
static Value declare_function(
    Node* func, Type* class_type, const char* name, Type* context_type,
    Type* result_type) { // struct methods take self and struct
                         // arguments by pointer, as silver's do
    LType param_types[64];
    int   param_count = 0;
    if (context_type) param_types[param_count++] = S->ir.ptr;
    else if (class_type)
        param_types[param_count++] = class_type->kind == T_STRUCT
                                         ? S->ir.ptr
                                         : llvm_of(class_type);
    Node* params = func->kids[0];
    for (int i = 0; i < params->count; i++) {
        Type* want = param_type(params->kids[i]);
        param_types[param_count++] =
            class_type && class_type->kind == T_STRUCT &&
                    want->kind == T_STRUCT
                ? S->ir.ptr
                : llvm_of(want);
    }
    return declare_abi(name, param_types, param_count,
                       result_type ? result_type : fn_ret(func));
}
static Value type_blob(const char* name, int slots) {
    Value global =
        global_var(record_global(format("silver-%s", S->modname), name),
                   LLVMArrayType2(S->ir.i8, HDR + sizeof(struct _Au_t) +
                                                8 * slots),
                   false);
    return global;
}
static Value
def_var(Value record_val, const char* name, Type* type, long offset,
        u64 traits, Value value,
        Node* meta) { // register a member variable the way silver does:
                      // def_prop with offset, access, meta
    int access = traits & AU_TRAIT_IS_HIDDEN ? 1 : 2;
    return call_au(
        "def_prop", "ppppliipppiipip", 14, record_val, const_str(name),
        type_record(type),
        const_i64((traits & ~AU_TRAIT_IS_HIDDEN) | AU_TRAIT_IPROP),
        const_i32(offset), const_i32(0),
        value ? value : LLVMConstNull(S->ir.ptr),
        LLVMConstNull(S->ir.ptr), LLVMConstNull(S->ir.ptr),
        const_i32(0), const_i32(access), const_str(S->cur_file),
        const_i32(0),
        meta ? type_record(type_of(meta)) : LLVMConstNull(S->ir.ptr));
}
static Value def_fn(Value record_val, const char* name,
                    Type* result_type, int kind, u64 traits,
                    Value func_val, const char* alt_name, int slot,
                    int arg_count, Type** args, const char** arg_names,
                    int   optype,
                    Node* meta) { // register a function member with
                                  // named, typed arguments
    Value member = call_au(
        "def_func", "ppppiiilppipppip", 15, record_val, const_str(name),
        type_record(result_type), const_i64(kind), const_i64(2),
        const_i64(optype), const_i64(traits), func_val,
        alt_name ? const_str(alt_name) : LLVMConstNull(S->ir.ptr),
        const_i64(slot), LLVMConstNull(S->ir.ptr),
        LLVMConstNull(S->ir.ptr), const_str(S->cur_file), const_i64(0),
        meta ? type_record(type_of(meta)) : LLVMConstNull(S->ir.ptr));
    for (int i = 0; i < arg_count; i++)
        call_au("def_arg", "ppppl", 4, member,
                const_str(arg_names && arg_names[i] ? arg_names[i]
                          : i == 0 && !(traits & AU_TRAIT_SMETHOD)
                              ? "a"
                              : format("arg%d", i)),
                type_record(args[i]),
                const_i64(i == 0 && !(traits & AU_TRAIT_SMETHOD)
                              ? AU_TRAIT_IS_TARGET
                              : 0));
    if (kind == AU_MEMBER_CONSTRUCT && arg_count > 1)
        LLVMBuildStore(S->ir.build, type_record(args[1]),
                       byte_ptr(member, offsetof(struct _Au_t, meta)));
    return member;
}
static Type* from_ct(CT* ct);
static Type*
from_cd(CD* decl) { // a C or C++ record: flat memory Au allocates and
                    // frees, fields at clang's offsets
    List* seen = &S->cd_seen;
    for (int i = 0; i + 1 < seen->count; i += 2)
        if (seen->data[i] == decl) return seen->data[i + 1];
    Type* type   = new_type(T_CLASS, decl->qualified);
    type->c_decl = decl;
    list_push(seen, decl);
    list_push(seen, type);
    LLVMBuilderRef saved_build = S->ir.build;
    S->ir.build                = S->ir.init_build;
    Value blob                 = type_blob(type->name, 0);
    Value header_offset        = const_i64(HDR);
    Value record_val = LLVMConstGEP2(S->ir.i8, blob, &header_offset, 1);
    call_au("emplace_type", "ppppppilllipi", 12, record_val,
            LLVMConstNull(S->ir.ptr), LLVMConstNull(S->ir.ptr),
            S->modrec, const_str(type->name), const_i32(AU_MEMBER_TYPE),
            const_i64(AU_TRAIT_STRUCT | AU_TRAIT_IS_C),
            const_i64(decl->size), const_i64(0), const_i32(0),
            const_str(S->cur_file), const_i32(0));
    for (int k = 0; k < decl->member_count; k++)
        if (decl->members[k]->kind == CD_FIELD &&
            decl->members[k]->result->kind != 'r')
            def_var(record_val, decl->members[k]->name,
                    from_ct(decl->members[k]->result),
                    decl->members[k]->offset, 0, 0, 0);
    call_au("push_type", "vpp", 2, record_val, S->modrec);
    S->ir.build = saved_build;
    return type;
}
static Type* from_ct(CT* ct) {
    switch (ct->kind) {
    case 'v': return basic(T_NONE);
    case 'f': {
        Type* type = new_type(T_FLOAT, ct->bits == 32 ? "f32" : "f64");
        type->bits = ct->bits;
        type->is_signed = 1;
        return type;
    }
    case 'i':
        if (ct->bits == 1) return basic(T_BOOL);
        {
            Type* type = new_type(
                T_INT,
                format("%c%d", ct->is_signed ? 'i' : 'u', ct->bits));
            type->bits      = ct->bits;
            type->is_signed = ct->is_signed;
            return type;
        }
    case 'p':
        if (ct->bits && ct->elem) {
            Type* array_ref = new_type(T_REF, "carray");
            array_ref->elem = from_ct(ct->elem);
            return array_ref;
        } /* a C array: a reference that decays */
        if (ct->elem && ct->elem->kind == 'r')
            return ct->elem->record
                       ? from_cd(ct->elem->record)
                       : new_type(
                             T_HANDLE,
                             format(
                                 "%s*",
                                 ct->elem->spell)); /* an opaque record
                                                       pointer keeps its
                                                       C spelling */
        if (ct->elem && ct->elem->kind == 'i' && ct->elem->bits == 8)
            return basic(T_CSTR);
        if (ct->elem && ct->elem->kind == 'v')
            return new_type(T_HANDLE, "handle");
        return ct->elem ? ref_to(from_ct(ct->elem))
                        : new_type(T_HANDLE, "handle");
    case 'r':
        if (ct->record) return from_cd(ct->record);
    default: return new_type(T_UNK, "unknown");
    }
}
static bool c_type_name(const char* name) {
    CD* decl = c_find(name, CD_RECORD);
    if (!decl) decl = c_find(name, CD_TYPEDEF);
    return decl && decl->qualified && same(decl->qualified, name);
} // a global C type name reads as a type
static Type* c_type(const char* name) {
    CD* record_decl = c_find(name, CD_RECORD);
    if (record_decl) return from_cd(record_decl);
    CD* typedef_decl = c_find(name, CD_TYPEDEF);
    return typedef_decl ? from_ct(typedef_decl->result) : 0;
}
static Value c_call(
    CD* func, Value self, Node* args, Scope* scope,
    Type** out) { // a C or C++ function: clang's parameter types decide
                  // every conversion; records by value ride as integers
    LType param_types[34];
    Value call_args[34];
    int   count       = 0;
    Type* result_type = from_ct(func->result);
    bool  hidden =
        func->result->kind == 'r' && func->result->record->size > 16;
    Value result_slot = 0;
    if (hidden) {
        result_slot        = au_alloc(result_type, args->line);
        param_types[count] = S->ir.ptr;
        call_args[count++] = result_slot;
    }
    if (self) {
        param_types[count] = S->ir.ptr;
        call_args[count++] = self;
    }
    for (int i = 0; i < args->count; i++) {
        CT*   param_ct = i < func->param_count ? func->params[i] : 0;
        Type* want     = param_ct ? from_ct(param_ct) : 0;
        Val   val      = eval(args->kids[i], scope, want);
        if (param_ct && param_ct->kind == 'r') {
            long  size = param_ct->record->size;
            LType int_type =
                size <= 8 ? S->ir.i64 : LLVMArrayType2(S->ir.i64, 2);
            param_types[count] = int_type;
            call_args[count++] = load(int_type, val.value);
            continue;
        } // by value
        if (param_ct && param_ct->is_ref && param_ct->kind != 'r' &&
            param_ct->kind != 'p') {
            param_types[count] = S->ir.ptr;
            call_args[count++] = addr_of(cast_value(val, want));
            continue;
        } // T& to a scalar: by address
        Value value        = want ? cast_value(val, want)
                             : is_str(val.type) ? chars_of(val.value, val.type)
                             : val.type->kind == T_VEC
                                 ? cast_value(val, basic(T_HANDLE))
                                 : val.value;
        param_types[count] = param_ct ? LLVMTypeOf(value) : 0;
        if (!param_ct) value = fit(value, 0);
        call_args[count++] = value;
    }
    int fixed_count =
        (hidden ? 1 : 0) + (self ? 1 : 0) + func->param_count;
    for (int i = 0; i < fixed_count && i < count; i++)
        if (!param_types[i]) param_types[i] = LLVMTypeOf(call_args[i]);
    bool returns_ref =
        func->result->is_ref && func->result->kind != 'r';
    LType result_llvm = hidden ? S->ir.void_type
                        : func->result->kind == 'r'
                            ? (func->result->record->size <= 8
                                   ? S->ir.i64
                                   : LLVMArrayType2(S->ir.i64, 2))
                        : returns_ref ? S->ir.ptr
                                      : llvm_of(result_type);
    LType func_type   = LLVMFunctionType(result_llvm, param_types,
                                         fixed_count, func->is_variadic);
    Value callee;
    if (func->is_virtual && self) {
        Value vtable = load(S->ir.ptr, self);
        callee =
            load(S->ir.ptr, index_ptr(S->ir.ptr, vtable,
                                      const_i64(func->vtable_index)));
    } else callee = fn_named(func->symbol, func_type);
    Value result = LLVMBuildCall2(S->ir.build, func_type, callee,
                                  call_args, count, "");
    *out         = result_type;
    if (returns_ref) result = load(llvm_of(result_type), result);
    if (hidden) return result_slot;
    if (func->result->kind == 'r') {
        Value slot = au_alloc(result_type, args->line);
        LLVMBuildStore(S->ir.build, result, slot);
        return slot;
    }
    if (result_type->kind == T_BOOL)
        return LLVMBuildTrunc(S->ir.build, result, S->ir.i1, "");
    return result;
}
static CD*
c_method(CD* record_decl, const char* name, Node* args,
         Scope* scope) { // overloads: by argument count, then a float
                         // argument picks a float parameter
    bool want_float = args->count && args->kids[0]->kind == N_FLT;
    CD*  best       = 0;
    for (int i = 0; i < record_decl->member_count; i++) {
        CD* member = record_decl->members[i];
        if (member->kind == CD_FIELD || !same(member->name, name) ||
            member->param_count != args->count)
            continue;
        if (!best) best = member;
        if (args->count &&
            (member->params[0]->kind == 'f') == want_float)
            return member;
    }
    return best;
}
static Type*
ctx_record(int id, List* names,
           List* type_list) { // a lambda context: a struct type Au can
                              // drop the captures of
    Type* type = new_type(T_CLASS, format("lamctx%d", id));
    LType elem_types[65];
    for (int i = 0; i < names->count; i++) {
        elem_types[i] = llvm_of(type_list->data[i]);
        list_push(&type->field_names, names->data[i]);
        list_push(&type->field_types, type_list->data[i]);
    }
    type->llvm = LLVMStructCreateNamed(S->ir.context, type->name);
    LLVMStructSetBody(type->llvm, elem_types, names->count, 0);
    type->kind                 = T_STRUCT;
    LLVMBuilderRef saved_build = S->ir.build;
    S->ir.build                = S->ir.init_build;
    Value blob                 = type_blob(type->name, 0);
    Value header_offset        = const_i64(HDR);
    Value record_val = LLVMConstGEP2(S->ir.i8, blob, &header_offset, 1);
    call_au("emplace_type", "ppppppilllipi", 12, record_val,
            LLVMConstNull(S->ir.ptr), LLVMConstNull(S->ir.ptr),
            S->modrec, const_str(type->name), const_i32(AU_MEMBER_TYPE),
            const_i64(AU_TRAIT_STRUCT), LLVMSizeOf(type->llvm),
            const_i64(0), const_i32(0), const_str(S->cur_file),
            const_i32(0));
    for (int i = 0; i < names->count; i++)
        def_var(record_val, names->data[i], type_list->data[i],
                (offset_of(type->llvm, i)), 0, 0, 0);
    S->ir.build = saved_build;
    return type;
}
static Value ctx_alloc(Type* ctx) { return au_alloc(ctx, 0); }
static Val
make_lambda(Node*  node,
            Scope* scope) { // inline lambda: hoisted function + context
                            // of the captured locals
    int   id         = S->lam_counter++;
    Node* params     = node->kids[0];
    List  ident_list = {0};
    collect_idents(node->kids[2], &ident_list);
    List names = {0}, type_list = {0}, vars = {0};
    if (S->cur_fn && S->cur_fn->class_type) {
        Var* var = find_var(scope, "self");
        if (var) {
            list_push(&names, "self");
            list_push(&type_list, var->type);
            list_push(&vars, var);
        }
    }
    for (int i = 0; i < ident_list.count; i++) {
        const char* name   = ident_list.data[i];
        bool        is_dup = same(name, "self");
        for (int j = 0; j < names.count; j++)
            if (same(names.data[j], name)) is_dup = true;
        for (int j = 0; j < params->count; j++)
            if (same(params->kids[j]->text, name)) is_dup = true;
        Var* var = is_dup ? 0 : find_var(scope, name);
        if (!var || var->type->kind == T_LOCAL) continue;
        if (S->cur_fn && S->cur_fn->class_type &&
            member_named(S->cur_fn->class_type, name, 0) &&
            !(member_mods(
                  member_named(S->cur_fn->class_type, name, 0)) &
              1))
            continue; // reached through self
        list_push(&names, name);
        list_push(&type_list, var->type);
        list_push(&vars, var);
    }
    Type* context_type = ctx_record(id, &names, &type_list);
    Node* func_node = new_node(D_FUNC, format("lam%d", id), node->line);
    add_kid(func_node, params);
    add_kid(func_node, kid(node, 1));
    add_kid(func_node, node->kids[2]);
    Type* result_type = kid(node, 1) ? type_of(node->kids[1]) : 0;
    if (!result_type) {
        Value probe =
            declare_function(func_node, 0, format("lamprobe%d", id),
                             context_type, basic(T_NONE));
        Type* probe_result = 0;
        emit_function(func_node, 0, probe, context_type, &vars,
                      &probe_result);
        LLVMDeleteFunction(probe);
        result_type = probe_result ? probe_result : basic(T_NONE);
    }
    Value func_val = declare_function(func_node, 0, func_node->text,
                                      context_type, result_type);
    list_push(&S->pending, func_val);
    emit_function(func_node, 0, func_val, context_type, &vars,
                  &result_type);
    Value context_val = ctx_alloc(context_type);
    for (int i = 0; i < names.count; i++) {
        Var*  var = vars.data[i];
        Value val = load(llvm_of(var->type), var->address);
        LLVMBuildStore(S->ir.build, val,
                       field_ptr(context_type->llvm, context_val, i));
        if (is_obj(var->type)) hold_value(val);
    }
    return make_val(call_au("lambda_instance", "ppppp", 4,
                            LLVMConstNull(S->ir.ptr), func_val,
                            LLVMConstNull(S->ir.ptr), context_val),
                    lambda_type(result_type, params, 0, params->count));
}
static Val bind_lambda(Node*  node,
                       Scope* scope) { // lambda obj.method[ ctx args ]
                                       // / lambda func[ ctx args ]
    Node* call = node->kids[0];
    if (call->kind != N_CALL)
        emit_fail("lambda binding expects a call");
    Node* callee    = call->kids[0];
    Node* args      = call->kids[1];
    Node* func_node = 0;
    Type* owner     = 0;
    Val   obj       = {0};
    if (callee->kind == N_MEMBER) {
        obj       = eval(callee->kids[0], scope, 0);
        func_node = find_method(obj.type, callee->text, D_FUNC, &owner);
    } else func_node = find_func(callee->text);
    if (!func_node) emit_fail("no function to bind: %s", callee->text);
    int   id            = S->lam_counter++;
    Node* params        = func_node->kids[0];
    int   regular_count = params->flag;
    List  names = {0}, type_list = {0};
    if (obj.value) {
        list_push(&names, "self");
        list_push(&type_list, obj.type);
    }
    for (int i = regular_count; i < params->count; i++) {
        list_push(&names, params->kids[i]->text);
        list_push(&type_list, param_type(params->kids[i]));
    }
    Type* context_type = ctx_record(id, &names, &type_list);
    Type* result_type  = fn_ret(func_node);
    Value context_val  = ctx_alloc(context_type);
    int   field_i      = 0;
    if (obj.value) {
        LLVMBuildStore(
            S->ir.build, obj.value,
            field_ptr(context_type->llvm, context_val, field_i++));
        hold_value(obj.value);
    }
    for (int i = regular_count; i < params->count; i++) {
        Type* want = param_type(params->kids[i]);
        Value value =
            eval_as(args->kids[i - regular_count], scope, want);
        LLVMBuildStore(
            S->ir.build, value,
            field_ptr(context_type->llvm, context_val, field_i++));
        if (is_obj(want)) hold_value(value);
    }
    Node* wrap_node = new_node(D_FUNC, format("lam%d", id), node->line);
    Node* wrap_params = new_node(N_ARGS, 0, node->line);
    for (int i = 0; i < regular_count; i++)
        add_kid(wrap_params, params->kids[i]);
    wrap_params->flag = regular_count;
    add_kid(wrap_node, wrap_params);
    add_kid(wrap_node, kid(func_node, 1));
    add_kid(wrap_node, 0);
    Value wrapper = declare_function(wrap_node, 0, wrap_node->text,
                                     context_type, result_type);
    list_push(&S->pending, wrapper);
    Block saved_block = cur_block();
    Value saved_func  = S->ir.func;
    S->ir.func        = wrapper;
    build_at(new_block("entry"));
    Value context_arg = LLVMGetParam(wrapper, 0);
    Value call_args[32];
    int   arg_count = 0;
    field_i         = 0;
    if (obj.value)
        call_args[arg_count++] =
            load(S->ir.ptr,
                 field_ptr(context_type->llvm, context_arg, field_i++));
    for (int i = 0; i < regular_count; i++)
        call_args[arg_count++] = LLVMGetParam(wrapper, i + 1);
    for (int i = regular_count; i < params->count; i++)
        call_args[arg_count++] =
            load(llvm_of(param_type(params->kids[i])),
                 field_ptr(context_type->llvm, context_arg, field_i++));
    Value result = call_fn(
        LLVMGetNamedFunction(S->ir.module,
                             obj.value ? method_cname(owner, func_node)
                                       : func_node->text),
        arg_count, call_args);
    if (result_type->kind == T_NONE) LLVMBuildRetVoid(S->ir.build);
    else LLVMBuildRet(S->ir.build, result);
    S->ir.func = saved_func;
    build_at(saved_block);
    return make_val(call_au("lambda_instance", "ppppp", 4,
                            LLVMConstNull(S->ir.ptr), wrapper,
                            LLVMConstNull(S->ir.ptr), context_val),
                    lambda_type(result_type, params, 0, regular_count));
}
static Val   construct(Type* type, Node* args, Scope* scope);
static Value shape_of(int count, Value* dims) {
    Value array = stack_slot(LLVMArrayType2(S->ir.i64, 8));
    for (int i = 0; i < count; i++) {
        Value ix[2] = {const_i64(0), const_i64(i)};
        LLVMBuildStore(S->ir.build, dims[i],
                       LLVMBuildGEP2(S->ir.build,
                                     LLVMArrayType2(S->ir.i64, 8),
                                     array, ix, 2, ""));
    }
    return call_au("shape_from", "plp", 2, const_i64(count), array);
}
static Value vec_origin(Value vec) {
    return au_field(vec, au_member(au_type("vector"), "origin", 0),
                    S->ir.ptr);
}
static Value vec_count(Value vec) {
    return LLVMBuildSExt(
        S->ir.build,
        au_field(vec, au_member(au_type("vector"), "count", 0),
                 S->ir.i32),
        S->ir.i64, "");
}
static Value vec_elem_ptr(Type* elem, Value vec, Value index) {
    return index_ptr(llvm_of(elem), vec_origin(vec), index);
}
static Value vec_new(Type* type, Node* seed,
                     Scope* scope) { // vec T [dims] [seed]
    Type* elem      = type->elem;
    LType elem_llvm = llvm_of(elem);
    Value count     = 0;
    Value shape     = 0;
    if (type->dims && type->dims->count) {
        Node* dim_node = type->dims->kids[0];
        if (dim_node->kind == N_NUM && dim_node->token &&
            dim_node->token->xshape) {
            Value dims[8];
            long  total = 1;
            for (int i = 0; i < dim_node->token->dim_count; i++) {
                dims[i] = const_i64(dim_node->token->dims[i]);
                total *= dim_node->token->dims[i];
            }
            shape = shape_of(dim_node->token->dim_count, dims);
            count = const_i64(total);
        } else count = eval_as(dim_node, scope, basic(T_INT));
    }
    Value vec = call_au("vector_of", "pp", 1, type_record(elem));
    if (shape) {
        Value shape_slot =
            byte_ptr(header_of(vec), offsetof(struct _object, shape));
        LLVMBuildStore(S->ir.build, hold_value(shape), shape_slot);
    }
    if (count && !(seed && seed->count))
        call_au("vector_resize", "ppl", 2, vec, count);
    if (seed && seed->count)
        for (int i = 0; i < seed->count; i++) {
            Value item = eval_as(seed->kids[i], scope, elem);
            call_au("vector_push", "ppp", 2, vec,
                    is_obj(elem) ? item : addr_of(item));
        }
    return vec;
}
static Value au_call_member(Au_t member, Value self, int count,
                            Value* args, Type** out);
static Val
au_member_val(Val         obj,
              const char* name) { // obj.name on an Au object, from its
                                  // registered members
    Au_t rec    = au_rec_of(obj.type);
    Au_t member = rec ? au_member(rec, name, AU_MEMBER_VAR) : 0;
    if (!member) {
        Au_t func_member =
            rec ? au_member(rec, name, AU_MEMBER_FUNC) : 0;
        if (!func_member)
            emit_fail("no member %s on %s", name, obj.type->name);
        Type* out_type;
        Value value =
            au_call_member(func_member, obj.value, 0, 0, &out_type);
        return make_val(value, out_type);
    } // a bare obj.method calls it
    Type* member_type = from_au(member->type);
    Value value = au_field(obj.value, member, llvm_of(member_type));
    return make_val(value, member_type);
}
static Val member_val(Val         obj,
                      const char* name) { // obj.name as a value
    Type* type = obj.type;
    if (type->kind == T_HDR) {
        Value header = header_of(obj.value);
        return same(name, "au")
                   ? make_val(load(S->ir.ptr, header), basic(T_TYPE))
                   : make_val(
                         load(
                             S->ir.i32,
                             byte_ptr(
                                 header,
                                 same(name, "managed")
                                     ? offsetof(struct _object, managed)
                                     : offsetof(struct _object, refs))),
                         type_named("i32"));
    }
    if (type->c_decl) {
        CD* c_rec = type->c_decl;
        for (int i = 0; i < c_rec->member_count; i++)
            if (c_rec->members[i]->kind == CD_FIELD &&
                same(c_rec->members[i]->name, name)) {
                Type* field_ty = from_ct(c_rec->members[i]->result);
                return make_val(
                    load(
                        llvm_of(field_ty),
                        byte_ptr(obj.value, c_rec->members[i]->offset)),
                    field_ty);
            }
        emit_fail("no field %s on %s", name, type->name);
    }
    if (type->kind == T_TYPE) { // an Au_t record's own fields
        if (same(name, "traits"))
            return make_val(
                load(S->ir.i64,
                     byte_ptr(obj.value,
                              offsetof(struct _Au_t, traits))),
                type_named("u64"));
        if (same(name, "value"))
            return make_val(
                load(
                    S->ir.ptr,
                    byte_ptr(obj.value, offsetof(struct _Au_t, value))),
                basic(T_OBJECT));
        if (same(name, "meta"))
            return make_val(
                byte_ptr(obj.value, offsetof(struct _Au_t, meta)),
                new_type(T_UNK, "meta"));
        if (same(name, "members"))
            return make_val(
                byte_ptr(obj.value, offsetof(struct _Au_t, members)),
                new_type(T_UNK, "micro"));
        if (same(name, "ident"))
            return make_val(
                load(
                    S->ir.ptr,
                    byte_ptr(obj.value, offsetof(struct _Au_t, ident))),
                basic(T_CSTR));
        emit_fail("no field %s on Au_t", name);
    }
    if (type->kind == T_UNK && same(type->name, "meta"))
        return make_val(
            load(S->ir.ptr,
                 byte_ptr(obj.value, same(name, "a")   ? 0
                                     : same(name, "b") ? 8
                                                       : 16)),
            basic(T_TYPE));
    if (same(name, "count") &&
        (is_str(type) || type->kind == T_VEC || type->kind == T_MAP ||
         type->kind == T_TOKENS)) { // null-safe
        Block before      = cur_block();
        Block count_block = new_block("count"),
              end_block   = new_block("end");
        LLVMBuildCondBr(S->ir.build, to_bool(obj), count_block,
                        end_block);
        build_at(count_block);
        Val   count_val   = au_member_val(obj, "count");
        Value count_i64   = fit(count_val.value, S->ir.i64);
        Block after_block = cur_block();
        branch_to(end_block);
        build_at(end_block);
        return make_val(phi_of(S->ir.i64, const_i64(0), before,
                               count_i64, after_block),
                        basic(T_INT));
    }
    if (type->kind == T_STRUCT ||
        (type->kind == T_REF &&
         type->elem->kind == T_STRUCT)) { // own or imported: fields by
                                          // layout, statics by global
        Type* record = type->kind == T_REF ? type->elem : type;
        Type* owner;
        Node* member =
            record->decl ? member_named(record, name, &owner) : 0;
        if (member && (member_mods(member) & 1)) {
            Type* member_type = type_of(member->kids[0]);
            return make_val(
                load(llvm_of(member_type),
                     global_var(format("%s_%s", owner->name, name),
                                llvm_of(member_type), false)),
                member_type);
        }
        Type* member_type = field_type(record, name);
        if (!member_type)
            emit_fail("no member %s on %s", name, record->name);
        if (type->kind == T_STRUCT)
            return member_type->kind == T_LOCAL
                       ? make_val(field_ptr(record->llvm,
                                            addr_of(obj.value),
                                            field_index(record, name)),
                                  member_type)
                       : make_val(LLVMBuildExtractValue(
                                      S->ir.build, obj.value,
                                      field_index(record, name), ""),
                                  member_type);
        Value ptr = field_ptr(record->llvm, obj.value,
                              field_index(record, name));
        return make_val(member_type->kind == T_LOCAL
                            ? ptr
                            : load(llvm_of(member_type), ptr),
                        member_type);
    }
    if (type->kind == T_CLASS && type->decl) {
        Type* record = type->kind == T_REF ? type->elem : type;
        Type* owner;
        Node* member = member_named(record, name, &owner);
        if (!member) {
            Node* method = find_method(record, name, D_FUNC, &owner);
            if (!method)
                emit_fail("no member %s on %s", name, record->name);
            Value before[1] = {obj.value};
            return make_val(
                call_fn(LLVMGetNamedFunction(
                            S->ir.module, method_cname(owner, method)),
                        1, before),
                fn_ret(method));
        } // a bare obj.method calls it
        Type* member_type = type_of(member->kids[0]);
        if (member_mods(member) & 1)
            return make_val(
                load(llvm_of(member_type),
                     global_var(format("%s_%s", owner->name, name),
                                llvm_of(member_type), false)),
                member_type);
        if (type->kind == T_STRUCT)
            return member_type->kind == T_LOCAL
                       ? make_val(field_ptr(record->llvm,
                                            addr_of(obj.value),
                                            field_index(record, name)),
                                  member_type)
                       : make_val(LLVMBuildExtractValue(
                                      S->ir.build, obj.value,
                                      field_index(record, name), ""),
                                  member_type);
        Value ptr = field_ptr(record->llvm, obj.value,
                              field_index(record, name));
        return make_val(member_type->kind == T_LOCAL
                            ? ptr
                            : load(llvm_of(member_type), ptr),
                        member_type);
    }
    if (is_obj(type) || type->kind == T_SHAPE || type->kind == T_CLASS)
        return au_member_val(obj, name);
    emit_fail("no member %s on %s", name, type->name);
    return obj;
}
static Val index_addr(Val container, Node* args, Scope* scope,
                      bool write) { // address of container[ args ]
    Type* type = container.type;
    if (type->kind == T_VEC) {
        Value index = eval_as(args->kids[0], scope, basic(T_INT));
        return make_val(
            vec_elem_ptr(type->elem, container.value, index),
            type->elem);
    }
    if (type->kind == T_REF)
        return make_val(
            index_ptr(llvm_of(type->elem), container.value,
                      eval_as(args->kids[0], scope, basic(T_INT))),
            type->elem);
    if (type->kind == T_LOCAL) {
        Value indices[2] = {
            const_i64(0), eval_as(args->kids[0], scope, basic(T_INT))};
        return make_val(LLVMBuildGEP2(S->ir.build, llvm_of(type),
                                      container.value, indices, 2, ""),
                        type->elem);
    }
    return make_val(0, type);
}
static Val
au_index(Val container, Node* args,
         Scope* scope) { // the registered getter of an Au class
    Au_t rec    = au_rec_of(container.type);
    Au_t getter = 0;
    for (Au_t cur = rec; cur && !getter; cur = cur->context)
        for (int i = 0; i < cur->members.count; i++) {
            Au_t member = (Au_t)cur->members.origin[i];
            if (member->member_type == AU_MEMBER_GETTER) {
                getter = member;
                break;
            }
        }
    if (!getter) emit_fail("cannot index %s", container.type->name);
    Type* out_type;
    Value call_args[8];
    int   arg_count = 0;
    for (int i = 0; i < args->count; i++) {
        Type* arg_type =
            from_au(((Au_t)getter->args.origin[i + 1])->src);
        call_args[arg_count++] =
            eval_as(args->kids[i], scope, arg_type);
    }
    return make_val(au_call_member(getter, container.value, arg_count,
                                   call_args, &out_type),
                    out_type);
}
static Val index_val(Val container, Node* args, Scope* scope) {
    Type* type = container.type;
    if (type->kind == T_HANDLE && same(type->name, "hook")) {
        Value call_args[1] = {
            args->count ? eval_as(args->kids[0], scope, basic(T_OBJECT))
                        : LLVMConstNull(S->ir.ptr)};
        return make_val(
            LLVMBuildCall2(
                S->ir.build,
                LLVMFunctionType(S->ir.ptr, &S->ir.ptr, 1, 0),
                container.value, call_args, 1, ""),
            basic(T_OBJECT));
    } // a hook is Au (*)(Au)
    if (type->kind == T_MAP) {
        Value key = eval_as(args->kids[0], scope, type->key);
        Value found =
            call_au("map_get", "ppp", 2, container.value, key);
        return make_val(is_obj(type->elem)
                            ? found
                            : load(llvm_of(type->elem), found),
                        type->elem);
    }
    if (type->kind == T_CLASS && type->decl) {
        Type* owner;
        Node* getter = find_method(type, "getter", D_GETTER, &owner);
        if (!getter) emit_fail("no getter on %s", type->name);
        Value func         = LLVMGetNamedFunction(S->ir.module,
                                                  method_cname(owner, getter));
        Value call_args[2] = {container.value, 0};
        if (param_type(getter->kids[0]->kids[0])->kind == T_SHAPE) {
            Value dims[8];
            for (int i = 0; i < args->count; i++)
                dims[i] = eval_as(args->kids[i], scope, basic(T_INT));
            call_args[1] = shape_of(args->count, dims);
        } else
            call_args[1] =
                eval_as(args->kids[0], scope,
                        param_type(getter->kids[0]->kids[0]));
        return make_val(call_fn(func, 2, call_args), fn_ret(getter));
    }
    if (is_str(type) || type->kind == T_SHAPE ||
        type->kind == T_TOKENS ||
        (type->kind == T_CLASS && !type->decl))
        return au_index(container, args, scope);
    Val addr = index_addr(container, args, scope, false);
    if (!addr.value) emit_fail("cannot index %s", type->name);
    return make_val(addr.type->kind == T_LOCAL
                        ? addr.value
                        : load(llvm_of(addr.type), addr.value),
                    addr.type);
}
static Value
vec_map(Val left, Val right, Type* elem,
        Value (*op)(Value, Value, void*),
        void* context) { // element-wise over one or two vectors, or a
                         // vector and a scalar
    LType elem_llvm = llvm_of(elem);
    Value count     = vec_count(left.value);
    Value result    = call_au("vector_of", "pp", 1, type_record(elem));
    call_au("vector_resize", "ppl", 2, result, count);
    Value index_slot = stack_slot(S->ir.i64);
    LLVMBuildStore(S->ir.build, const_i64(0), index_slot);
    Block cond = new_block("vcond"), body = new_block("vbody"),
          end = new_block("vend");
    branch_to(cond);
    build_at(cond);
    Value index = load(S->ir.i64, index_slot);
    LLVMBuildCondBr(
        S->ir.build,
        LLVMBuildICmp(S->ir.build, LLVMIntSLT, index, count, ""), body,
        end);
    build_at(body);
    Value left_val =
        load(elem_llvm, vec_elem_ptr(elem, left.value, index));
    Value right_val =
        right.value
            ? (right.type->kind == T_VEC
                   ? load(elem_llvm,
                          vec_elem_ptr(elem, right.value, index))
                   : cast_value(right, elem))
            : 0;
    LLVMBuildStore(S->ir.build, op(left_val, right_val, context),
                   vec_elem_ptr(elem, result, index));
    LLVMBuildStore(S->ir.build,
                   LLVMBuildAdd(S->ir.build, index, const_i64(1), ""),
                   index_slot);
    branch_to(cond);
    build_at(end);
    return result;
}
static Value arith_op(const char* op, Value left, Value right,
                      Type* type) {
    bool is_float = backing(type)->kind == T_FLOAT,
         is_unsigned =
             !backing(type)->is_signed && backing(type)->kind == T_INT;
    if (same(op, "+"))
        return is_float ? LLVMBuildFAdd(S->ir.build, left, right, "")
                        : LLVMBuildAdd(S->ir.build, left, right, "");
    if (same(op, "-"))
        return is_float ? LLVMBuildFSub(S->ir.build, left, right, "")
                        : LLVMBuildSub(S->ir.build, left, right, "");
    if (same(op, "*"))
        return is_float ? LLVMBuildFMul(S->ir.build, left, right, "")
                        : LLVMBuildMul(S->ir.build, left, right, "");
    if (same(op, "/"))
        return is_float ? LLVMBuildFDiv(S->ir.build, left, right, "")
               : is_unsigned
                   ? LLVMBuildUDiv(S->ir.build, left, right, "")
                   : LLVMBuildSDiv(S->ir.build, left, right, "");
    if (same(op, "%"))
        return is_float ? LLVMBuildFRem(S->ir.build, left, right, "")
               : is_unsigned
                   ? LLVMBuildURem(S->ir.build, left, right, "")
                   : LLVMBuildSRem(S->ir.build, left, right, "");
    if (same(op, "|")) return LLVMBuildOr(S->ir.build, left, right, "");
    if (same(op, "&"))
        return LLVMBuildAnd(S->ir.build, left, right, "");
    if (same(op, "^"))
        return LLVMBuildXor(S->ir.build, left, right, "");
    if (same(op, "<<"))
        return LLVMBuildShl(S->ir.build, left, right, "");
    if (same(op, ">>"))
        return is_unsigned
                   ? LLVMBuildLShr(S->ir.build, left, right, "")
                   : LLVMBuildAShr(S->ir.build, left, right, "");
    emit_fail("operator %s", op);
    return 0;
}
static Value compare(const char* op, Value left, Value right,
                     Type* type) {
    bool is_float = backing(type)->kind == T_FLOAT,
         is_unsigned =
             !backing(type)->is_signed && backing(type)->kind == T_INT;
    if (LLVMGetTypeKind(LLVMTypeOf(left)) == LLVMPointerTypeKind) {
        if (LLVMGetTypeKind(LLVMTypeOf(right)) != LLVMPointerTypeKind)
            right =
                LLVMBuildIntToPtr(S->ir.build, right, S->ir.ptr, "");
        is_float    = false;
        is_unsigned = true;
    }
    if (is_float)
        return LLVMBuildFCmp(S->ir.build,
                             same(op, "==")   ? LLVMRealOEQ
                             : same(op, "!=") ? LLVMRealUNE
                             : same(op, "<")  ? LLVMRealOLT
                             : same(op, ">")  ? LLVMRealOGT
                             : same(op, "<=") ? LLVMRealOLE
                                              : LLVMRealOGE,
                             left, right, "");
    return LLVMBuildICmp(
        S->ir.build,
        same(op, "==")   ? LLVMIntEQ
        : same(op, "!=") ? LLVMIntNE
        : same(op, "<")  ? (is_unsigned ? LLVMIntULT : LLVMIntSLT)
        : same(op, ">")  ? (is_unsigned ? LLVMIntUGT : LLVMIntSGT)
        : same(op, "<=") ? (is_unsigned ? LLVMIntULE : LLVMIntSLE)
                         : (is_unsigned ? LLVMIntUGE : LLVMIntSGE),
        left, right, "");
}
typedef struct {
    const char* op;
    Type*       type;
    const char* name;
    Value       lo, hi, tt;
} OpCtx;
static Value op_arith(Value left, Value right, void* context) {
    OpCtx* op_ctx = context;
    return arith_op(op_ctx->op, left, right, op_ctx->type);
}
static Value math_call1(const char* name, Value value, Type* type) {
    bool  is_float  = backing(type)->kind == T_FLOAT;
    LType llvm_type = llvm_of(type);
    if (same(name, "abs"))
        return is_float
                   ? call_fn(fn_named(llvm_type == S->ir.f32
                                          ? "llvm.fabs.f32"
                                          : "llvm.fabs.f64",
                                      LLVMFunctionType(
                                          llvm_type, &llvm_type, 1, 0)),
                             1, &value)
                   : LLVMBuildSelect(
                         S->ir.build,
                         LLVMBuildICmp(S->ir.build, LLVMIntSLT, value,
                                       LLVMConstNull(llvm_type), ""),
                         LLVMBuildNeg(S->ir.build, value, ""), value,
                         "");
    static const char* intrinsics[] = {"sqrt",  "floor", "ceil",
                                       "round", "exp",   "log",
                                       "sin",   "cos",   0};
    bool               is_intrinsic = false;
    for (int i = 0; intrinsics[i]; i++)
        if (same(intrinsics[i], name)) is_intrinsic = true;
    LType float_type  = llvm_type == S->ir.f32 ? S->ir.f32 : S->ir.f64;
    Value float_value = fit(value, float_type);
    char* func_name =
        is_intrinsic
            ? format("llvm.%s.%s", name,
                     float_type == S->ir.f32 ? "f32" : "f64")
            : format("%s%s", name, float_type == S->ir.f32 ? "f" : "");
    return call_fn(
        fn_named(func_name,
                 LLVMFunctionType(float_type, &float_type, 1, 0)),
        1, &float_value);
}
static Value op_math1(Value left, Value right, void* context) {
    OpCtx* op_ctx = context;
    return math_call1(op_ctx->name, left, op_ctx->type);
}
static Value math_call2(const char* name, Value left, Value right,
                        Type* type) {
    bool  is_float  = backing(type)->kind == T_FLOAT;
    LType llvm_type = llvm_of(type);
    if (same(name, "min") || same(name, "max")) {
        Value cond = is_float
                         ? LLVMBuildFCmp(S->ir.build,
                                         name[1] == 'i' ? LLVMRealOLT
                                                        : LLVMRealOGT,
                                         left, right, "")
                         : LLVMBuildICmp(S->ir.build,
                                         name[1] == 'i' ? LLVMIntSLT
                                                        : LLVMIntSGT,
                                         left, right, "");
        return LLVMBuildSelect(S->ir.build, cond, left, right, "");
    }
    LType float_type   = llvm_type == S->ir.f32 ? S->ir.f32 : S->ir.f64;
    Value call_args[2] = {fit(left, float_type),
                          fit(right, float_type)};
    LType param_types[2] = {float_type, float_type};
    return call_fn(
        fn_named(same(name, "pow")
                     ? (float_type == S->ir.f32 ? "llvm.pow.f32"
                                                : "llvm.pow.f64")
                     : format("%s%s", name,
                              float_type == S->ir.f32 ? "f" : ""),
                 LLVMFunctionType(float_type, param_types, 2, 0)),
        2, call_args);
}
static Value op_math2(Value left, Value right, void* context) {
    OpCtx* op_ctx = context;
    return math_call2(op_ctx->name, left, right, op_ctx->type);
}
static Value op_clamp(Value left, Value right, void* context) {
    OpCtx* op_ctx = context;
    return math_call2("min",
                      math_call2("max", left, op_ctx->lo, op_ctx->type),
                      op_ctx->hi, op_ctx->type);
}
static Value op_mix(Value left, Value right, void* context) {
    OpCtx* op_ctx = context;
    Value  one    = backing(op_ctx->type)->kind == T_FLOAT
                        ? LLVMConstReal(llvm_of(op_ctx->type), 1)
                        : const_int(llvm_of(op_ctx->type), 1);
    return arith_op(
        "+",
        arith_op("*", left,
                 arith_op("-", one, op_ctx->tt, op_ctx->type),
                 op_ctx->type),
        arith_op("*", right, op_ctx->tt, op_ctx->type), op_ctx->type);
}
static Val math_builtin(const char* name, Node* args, Scope* scope) {
    static const char* one_arg[] = {
        "sqrt", "abs", "floor", "ceil", "round", "exp",  "log",
        "sin",  "cos", "tan",   "asin", "acos",  "atan", 0};
    static const char* two_arg[] = {"min", "max", "pow", "atan2", 0};
    bool               takes_one = false, takes_two = false;
    for (int i = 0; one_arg[i]; i++)
        if (same(one_arg[i], name)) takes_one = true;
    for (int i = 0; two_arg[i]; i++)
        if (same(two_arg[i], name)) takes_two = true;
    if (!takes_one && !takes_two && !same(name, "clamp") &&
        !same(name, "mix"))
        return make_val(0, 0);
    Val   first = eval(args->kids[0], scope, 0);
    Type* base =
        first.type->kind == T_VEC ? first.type->elem : first.type;
    if (base->kind == T_STRUCT && same(name, "mix")) {
        Val   second = eval(args->kids[1], scope, base);
        Value blend  = eval_as(args->kids[2], scope, basic(T_FLOAT));
        Value result = LLVMGetUndef(base->llvm);
        for (int i = 0; i < base->field_names.count; i++) {
            Type* field_ty = base->field_types.data[i];
            OpCtx op_ctx   = {
                0, field_ty,
                0, 0,
                0, number_cast(blend, basic(T_FLOAT), field_ty)};
            result = LLVMBuildInsertValue(
                S->ir.build, result,
                op_mix(LLVMBuildExtractValue(S->ir.build, first.value,
                                             i, ""),
                       LLVMBuildExtractValue(S->ir.build, second.value,
                                             i, ""),
                       &op_ctx),
                i, "");
        }
        return make_val(result, base);
    }
    Type* result_type = takes_one && !same(name, "abs") &&
                                backing(base)->kind != T_FLOAT
                            ? basic(T_FLOAT)
                            : base;
    OpCtx op_ctx      = {0, result_type, name, 0, 0, 0};
    if (first.type->kind == T_VEC) {
        if (takes_one)
            return make_val(
                vec_map(first, make_val(0, 0), base, op_math1, &op_ctx),
                first.type);
        if (same(name, "clamp")) {
            op_ctx.lo = eval_as(args->kids[1], scope, base);
            op_ctx.hi = eval_as(args->kids[2], scope, base);
            return make_val(
                vec_map(first, make_val(0, 0), base, op_clamp, &op_ctx),
                first.type);
        }
        if (same(name, "mix")) {
            op_ctx.tt = eval_as(args->kids[2], scope, base);
            return make_val(
                vec_map(first, eval(args->kids[1], scope, first.type),
                        base, op_mix, &op_ctx),
                first.type);
        }
        return make_val(vec_map(first, eval(args->kids[1], scope, base),
                                base, op_math2, &op_ctx),
                        first.type);
    }
    Value value = cast_value(first, result_type);
    if (takes_one)
        return make_val(math_call1(name, value, result_type),
                        result_type);
    if (same(name, "clamp")) {
        op_ctx.lo = eval_as(args->kids[1], scope, result_type);
        op_ctx.hi = eval_as(args->kids[2], scope, result_type);
        return make_val(op_clamp(value, 0, &op_ctx), result_type);
    }
    if (same(name, "mix")) {
        op_ctx.tt = eval_as(args->kids[2], scope, result_type);
        return make_val(
            op_mix(value, eval_as(args->kids[1], scope, result_type),
                   &op_ctx),
            result_type);
    }
    return make_val(
        math_call2(name, value,
                   eval_as(args->kids[1], scope, result_type),
                   result_type),
        result_type);
}
static Value
au_call_member(Au_t member, Value self, int count, Value* args,
               Type** out) { // call a registered Au method by its
                             // symbol and argument types
    const char* symbol    = member->alt ? member->alt : member->ident;
    bool        is_static = member->is_smethod;
    LType       param_types[32];
    int         param_count = 0;
    Value       call_args[32];
    int         arg_count = 0;
    if (!is_static) {
        param_types[param_count++] = S->ir.ptr;
        call_args[arg_count++]     = self;
    }
    for (int i = is_static ? 0 : 1; i < member->args.count; i++) {
        Au_t  arg_rec  = (Au_t)member->args.origin[i];
        Type* arg_type = from_au(arg_rec->src);
        int   index    = arg_count - (is_static ? 0 : 1);
        Value value    = index < count ? args[index]
                                       : LLVMConstNull(llvm_of(arg_type));
        if (arg_type->kind == T_STRUCT) {
            param_types[param_count++] = S->ir.ptr;
            call_args[arg_count++]     = addr_of(value);
        } else {
            param_types[param_count++] = llvm_of(arg_type);
            call_args[arg_count++]     = value;
        }
    }
    Type* result_type = from_au(member->type);
    *out              = result_type;
    return call_fn(
        declare_abi(symbol, param_types, param_count, result_type),
        arg_count, call_args);
}
static Val au_root_call(Au_t member, Value self, Node* args,
                        Scope* scope) { // a method registered on Au
                                        // itself, called on any object
    Value call_args[32];
    int   arg_count = 0;
    for (int i = self ? 1 : 0;
         i < member->args.count && arg_count < args->count; i++) {
        call_args[arg_count] =
            eval_as(args->kids[arg_count], scope,
                    from_au(((Au_t)member->args.origin[i])->src));
        arg_count++;
    }
    Type* out_type;
    Value value =
        au_call_member(member, self, arg_count, call_args, &out_type);
    return make_val(value, out_type);
}
static Val au_method(Val obj, const char* name, Node* args,
                     Scope* scope) { // obj.name[ args ] resolved
                                     // through the Au type tables
    Au_t rec    = au_rec_of(obj.type);
    Au_t member = rec ? au_member(rec, name, 0) : 0;
    if (!member || (member->member_type != AU_MEMBER_FUNC &&
                    member->member_type != AU_MEMBER_CAST))
        emit_fail("no method %s on %s", name, obj.type->name);
    bool  is_vec = obj.type->kind == T_VEC;
    Type* elem   = is_vec ? obj.type->elem : 0;
    Value call_args[32];
    int   arg_count = 0;
    for (int i = member->is_smethod ? 0 : 1;
         i < member->args.count && arg_count < args->count; i++) {
        Type* arg_type = from_au(((Au_t)member->args.origin[i])->src);
        Val   val      = eval(args->kids[arg_count], scope,
                       arg_type->kind == T_OBJECT
                                  ? (is_vec ? elem
                                     : obj.type->kind == T_MAP ? obj.type->key
                                                               : arg_type)
                                  : arg_type);
        call_args[arg_count++] =
            is_vec && arg_type->kind == T_OBJECT && !is_obj(elem)
                ? addr_of(cast_value(val, elem))
                : cast_value(val, arg_type);
    } // a vector takes primitive elements by address
    Type* result_type;
    Value value = au_call_member(
        member,
        obj.type->kind == T_STRUCT ? addr_of(obj.value) : obj.value,
        arg_count, call_args, &result_type);
    if (is_vec && result_type->kind == T_OBJECT)
        return is_obj(elem)
                   ? make_val(value, elem)
                   : make_val(load(llvm_of(elem), value), elem);
    if (result_type->kind == T_VEC && is_vec) result_type = obj.type;
    return make_val(value, result_type);
}
static Val call_method(Node* callee, Node* args,
                       Scope* scope) { // obj.name[ args ]
    Node*       obj_node = callee->kids[0];
    const char* name     = callee->text;
    if (obj_node->kind == N_SUPER) {
        Type* owner;
        Node* method = find_method(S->cur_fn->class_type->base, name,
                                   D_FUNC, &owner);
        if (!method) {
            Au_t root_member =
                au_member(au_type("Au"), name, AU_MEMBER_FUNC);
            if (!root_member) emit_fail("no super method %s", name);
            return au_root_call(root_member, S->cur_fn->self, args,
                                scope);
        }
        Value call_args[32];
        int   arg_count;
        args_call(args, scope, method->kids[0], S->cur_fn->self,
                  call_args, &arg_count, owner);
        return make_val(
            call_fn(LLVMGetNamedFunction(S->ir.module,
                                         method_cname(owner, method)),
                    arg_count, call_args),
            fn_ret(method));
    }
    Val   obj      = eval(obj_node, scope, 0);
    Type* obj_type = obj.type;
    if (obj_type->c_decl) {
        CD* method = c_method(obj_type->c_decl, name, args, scope);
        if (!method)
            emit_fail("no method %s on %s", name, obj_type->name);
        Type* result_type;
        Value value = c_call(method, method->is_static ? 0 : obj.value,
                             args, scope, &result_type);
        return make_val(value, result_type);
    }
    if (same(name, "hold"))
        return make_val(hold_value(obj.value), obj_type);
    if (same(name, "drop")) {
        call_au("drop", "vp", 1, obj.value);
        return make_val(0, basic(T_NONE));
    }
    if (same(name, "set_context_from") && obj_type->kind == T_CLASS) {
        call_au("Au_set_context_from", "vpp", 2, obj.value,
                eval(args->kids[0], scope, 0).value);
        return make_val(0, basic(T_NONE));
    }
    if (obj_type->kind == T_FLOAT || obj_type->kind == T_SCALAR) {
        if (same(name, "round")) {
            Value power = math_call2(
                "pow", LLVMConstReal(S->ir.f64, 10),
                eval_as(args->kids[0], scope, basic(T_FLOAT)),
                basic(T_FLOAT));
            Value float_val = fit(obj.value, S->ir.f64);
            return make_val(
                number_cast(
                    LLVMBuildFDiv(
                        S->ir.build,
                        math_call1("round",
                                   LLVMBuildFMul(S->ir.build, float_val,
                                                 power, ""),
                                   basic(T_FLOAT)),
                        power, ""),
                    basic(T_FLOAT), obj_type),
                obj_type);
        }
        if (same(name, "is_nan"))
            return make_val(LLVMBuildFCmp(S->ir.build, LLVMRealUNO,
                                          obj.value, obj.value, ""),
                            basic(T_BOOL));
        if (same(name, "is_finite"))
            return make_val(
                LLVMBuildFCmp(S->ir.build, LLVMRealOEQ,
                              LLVMBuildFSub(S->ir.build, obj.value,
                                            obj.value, ""),
                              LLVMConstNull(LLVMTypeOf(obj.value)), ""),
                basic(T_BOOL));
    }
    if ((obj_type->kind == T_CLASS && obj_type->decl) ||
        (obj_type->kind == T_STRUCT && obj_type->decl)) {
        Type* owner;
        Node* method = find_method(obj_type, name, D_FUNC, &owner);
        if (!method && obj_type->kind == T_CLASS) {
            Au_t root_member =
                au_member(au_type("Au"), name, AU_MEMBER_FUNC);
            if (root_member)
                return au_root_call(root_member, obj.value, args,
                                    scope);
        }
        if (method) {
            Value call_args[32];
            int   arg_count;
            Value self_val = obj.value;
            if (obj_type->kind == T_STRUCT) {
                Val addr = lvalue_of(obj_node, scope);
                self_val = addr.value ? addr.value : addr_of(obj.value);
            }
            args_call(args, scope, method->kids[0], self_val, call_args,
                      &arg_count, owner);
            if (callee->flag & 1) {
                LType func_type =
                    LLVMGlobalGetValueType(LLVMGetNamedFunction(
                        S->ir.module, method_cname(owner, method)));
                Value found =
                    call_au("find_member", "pppilb", 5,
                            load(S->ir.ptr, header_of(obj.value)),
                            const_str(name), const_i32(AU_MEMBER_FUNC),
                            const_i64(0), const_i32(1));
                return make_val(
                    LLVMBuildCall2(
                        S->ir.build, func_type,
                        load(S->ir.ptr,
                             byte_ptr(found,
                                      offsetof(struct _Au_t, value))),
                        call_args, arg_count, ""),
                    fn_ret(method));
            }
            return make_val(
                call_fn(LLVMGetNamedFunction(
                            S->ir.module, method_cname(owner, method)),
                        arg_count, call_args),
                fn_ret(method));
        }
        if (member_named(obj_type, name, 0)) {
            Val field = member_val(obj, name);
            if (field.type->kind == T_LAMBDA)
                return call_lambda(field.value, field.type, args,
                                   scope);
            return index_val(field, args, scope);
        }
        emit_fail("no method %s on %s", name, obj_type->name);
    }
    if (obj_type->kind == T_VEC && same(name, "equals")) {
        Type* bool_type;
        Value arg = eval(args->kids[0], scope, obj_type).value;
        return make_val(
            au_call_member(au_member(au_type("vector"), "equals", 0),
                           obj.value, 1, &arg, &bool_type),
            basic(T_BOOL));
    }
    return au_method(obj, name, args, scope);
}
static Val type_static_call(Node* callee, Node* args,
                            Scope* scope) { // Type.name[ args ]
    Type*       type = type_of(callee->kids[0]);
    const char* name = callee->text;
    if (type->kind == T_OBJECT) {
        if (same(name, "header"))
            return make_val(eval(args->kids[0], scope, 0).value,
                            basic(T_HDR));
        if (same(name, "auto_free")) {
            call_au("auto_free", "vb", 1,
                    eval_as(args->kids[0], scope, basic(T_BOOL)));
            return make_val(0, basic(T_NONE));
        }
        if (same(name, "string_agi"))
            return make_val(
                call_au("string_agi", "pp", 1,
                        eval(args->kids[0], scope, 0).value),
                basic(T_STRING));
        if (same(name, "parse_agi"))
            return make_val(
                call_au("parse_agi", "pppp", 3,
                        eval(args->kids[0], scope, 0).value,
                        eval_as(args->kids[1], scope, basic(T_CSTR)),
                        eval_as(args->kids[2], scope, basic(T_OBJECT))),
                basic(T_OBJECT));
        if (same(name, "find_member"))
            return make_val(
                call_au(
                    "find_member", "pppilb", 5,
                    eval(args->kids[0], scope, 0).value,
                    eval_as(args->kids[1], scope, basic(T_CSTR)),
                    eval_as(args->kids[2], scope, type_named("i32")),
                    eval_as(args->kids[3], scope, basic(T_INT)),
                    eval_as(args->kids[4], scope, basic(T_BOOL))),
                basic(T_TYPE));
        emit_fail("unsupported Au.%s", name);
    }
    Au_t rec    = au_rec_of(type);
    Au_t member = rec ? au_member(rec, name, AU_MEMBER_FUNC) : 0;
    if (member && member->is_smethod) {
        Value call_args[32];
        int   arg_count = 0;
        for (int i = 0;
             i < member->args.count && arg_count < args->count; i++)
            call_args[arg_count++] =
                eval_as(args->kids[arg_count], scope,
                        from_au(((Au_t)member->args.origin[i])->src));
        Type* out_type;
        Value value =
            au_call_member(member, 0, arg_count, call_args, &out_type);
        return make_val(value, out_type);
    }
    if (type->kind == T_CLASS && type->decl) {
        Type* owner;
        Node* func_node = find_method(type, name, D_FUNC, &owner);
        if (func_node && (member_mods(func_node) & 1)) {
            Value call_args[32];
            int   arg_count;
            args_call(args, scope, func_node->kids[0], 0, call_args,
                      &arg_count, 0);
            return make_val(call_fn(LLVMGetNamedFunction(
                                        S->ir.module,
                                        method_cname(owner, func_node)),
                                    arg_count, call_args),
                            fn_ret(func_node));
        }
    }
    emit_fail("unsupported static call %s.%s", type->name, name);
    return make_val(0, 0);
}
static Val call_expr(Node* node, Scope* scope) {
    Node* callee = node->kids[0];
    Node* args   = node->kids[1];
    if (callee->kind == N_IDENT) {
        const char* name = callee->text;
        Var*        var  = find_var(scope, name);
        if (var) {
            Val callee_val =
                make_val(var->type->kind == T_LOCAL
                             ? var->address
                             : load(llvm_of(var->type), var->address),
                         var->type);
            if (var->type->kind == T_LAMBDA)
                return call_lambda(callee_val.value, var->type, args,
                                   scope);
            return index_val(callee_val, args, scope);
        }
        Node* global_node = find_global(name);
        if (global_node) {
            Type* global_type = type_of(global_node->kids[0]);
            Value global_val =
                global_var(name, llvm_of(global_type), false);
            return index_val(
                make_val(global_type->kind == T_LOCAL
                             ? global_val
                             : load(llvm_of(global_type), global_val),
                         global_type),
                args, scope);
        }
        Node* func_node = find_func(name);
        if (func_node) {
            Value call_args[32];
            int   arg_count;
            args_call(args, scope, func_node->kids[0], 0, call_args,
                      &arg_count, 0);
            return make_val(
                call_fn(LLVMGetNamedFunction(S->ir.module, name),
                        arg_count, call_args),
                fn_ret(func_node));
        }
        if (S->cur_fn && S->cur_fn->class_type) {
            Type* owner;
            Node* method = find_method(S->cur_fn->class_type, name,
                                       D_FUNC, &owner);
            if (method) {
                Value call_args[32];
                int   arg_count;
                args_call(args, scope, method->kids[0],
                          (member_mods(method) & 1) ? 0
                                                    : S->cur_fn->self,
                          call_args, &arg_count,
                          owner); /* a static method takes no self */
                return make_val(
                    call_fn(
                        LLVMGetNamedFunction(
                            S->ir.module, method_cname(owner, method)),
                        arg_count, call_args),
                    fn_ret(method));
            }
        }
        if (same(name, "header") && args->count == 1)
            return make_val(
                eval(args->kids[0], scope, 0).value,
                basic(T_HDR)); /* the Au header behind an object */
        {
            Au_t root_member =
                au_member(au_type("Au"), name, AU_MEMBER_FUNC);
            if (root_member && (root_member->is_smethod ||
                                (S->cur_fn && S->cur_fn->self)))
                return au_root_call(
                    root_member,
                    root_member->is_smethod ? 0 : S->cur_fn->self, args,
                    scope);
        } /* Au's own methods (vdata, copy, check) reach every object */
        Val math_val = math_builtin(name, args, scope);
        if (math_val.type) return math_val;
        Au_t imported_func = imported_member(name, AU_MEMBER_FUNC);
        if (imported_func) {
            Value call_args[32];
            int   arg_count = 0;
            for (int i = 0; i < imported_func->args.count &&
                            arg_count < args->count;
                 i++)
                call_args[arg_count++] = eval_as(
                    args->kids[arg_count], scope,
                    from_au(
                        ((Au_t)imported_func->args.origin[i])->src));
            Type* out_type;
            Value var = au_call_member(imported_func, 0, arg_count,
                                       call_args, &out_type);
            return make_val(var, out_type);
        }
        if (same(name, "__builtin_frame_address")) {
            Value zero = const_i32(0);
            return make_val(
                call_fn(fn_named("llvm.frameaddress.p0",
                                 LLVMFunctionType(S->ir.ptr, &S->ir.i32,
                                                  1, 0)),
                        1, &zero),
                ref_to(basic(T_NONE)));
        }
        CD* c_decl_func = c_func(name, args->count);
        if (c_decl_func) {
            Type* out_type;
            Value var = c_call(c_decl_func, 0, args, scope, &out_type);
            return make_val(var, out_type);
        }
        emit_fail("unknown function %s", name);
    }
    if (callee->kind == N_MEMBER)
        return call_method(callee, args, scope);
    if (callee->kind == N_TYPEMEMBER)
        return type_static_call(callee, args, scope);
    Val callee_val = eval(callee, scope, 0);
    if (callee_val.type->kind == T_LAMBDA)
        return call_lambda(callee_val.value, callee_val.type, args,
                           scope);
    return index_val(callee_val, args, scope);
}
static Val binop(Node* node, Scope* scope, Type* want) {
    const char* op        = node->text;
    Val         left      = eval(node->kids[0], scope, 0);
    Type*       left_type = left.type;
    if (same(op, "is") || same(op, "inherits")) {
        Value type_val =
            node->kids[1]->kind == N_TYPEREF
                ? type_record(type_of(node->kids[1]->kids[0]))
                : eval(node->kids[1], scope, 0).value;
        Value isa = load(S->ir.ptr, header_of(left.value));
        if (same(op, "is"))
            return make_val(
                LLVMBuildSelect(
                    S->ir.build,
                    LLVMBuildAnd(S->ir.build, to_bool(left),
                                 LLVMBuildICmp(S->ir.build, LLVMIntEQ,
                                               isa, type_val, ""),
                                 ""),
                    left.value, LLVMConstNull(S->ir.ptr), ""),
                left_type->kind == T_CLASS || is_str(left_type)
                    ? left_type
                    : basic(T_OBJECT));
        return make_val(call_au("inherits", "bpp", 2, isa, type_val),
                        basic(T_BOOL));
    }
    if (node->kids[1]->kind == N_TUPLE ||
        node->kids[1]->kind == N_RANGE) {
        Node* set_node = node->kids[1];
        Value is_in;
        if (set_node->kind == N_RANGE) {
            Value low  = eval_as(set_node->kids[0], scope, left_type),
                  high = eval_as(set_node->kids[1], scope, left_type);
            is_in      = LLVMBuildAnd(
                S->ir.build, compare(">=", left.value, low, left_type),
                compare(same(set_node->text, "...") ? "<=" : "<",
                        left.value, high, left_type),
                "");
        } else {
            is_in = const_int(S->ir.i1, 0);
            for (int i = 0; i < set_node->count; i++)
                is_in = LLVMBuildOr(S->ir.build, is_in,
                                    compare("==", left.value,
                                            eval_as(set_node->kids[i],
                                                    scope, left_type),
                                            left_type),
                                    "");
        }
        return make_val(same(op, "!=")
                            ? LLVMBuildNot(S->ir.build, is_in, "")
                            : is_in,
                        basic(T_BOOL));
    }
    if (same(op, "||") || same(op, "&&") ||
        same(op,
             "??")) { // short-circuit; objects keep the chosen operand
        bool objs = is_obj(left_type) || left_type->kind == T_REF ||
                    left_type->kind == T_CSTR;
        Block before      = cur_block();
        Block right_block = new_block("rhs"),
              end_block   = new_block("end");
        Value left_bool   = to_bool(left);
        if (same(op, "&&"))
            LLVMBuildCondBr(S->ir.build, left_bool, right_block,
                            end_block);
        else
            LLVMBuildCondBr(S->ir.build, left_bool, end_block,
                            right_block);
        build_at(right_block);
        Val  right = eval(node->kids[1], scope, objs ? left_type : 0);
        bool both =
            objs && (is_obj(right.type) || right.type->kind == T_REF ||
                     right.type->kind == T_CSTR);
        Value right_val =
            both ? cast_value(right, left_type) : to_bool(right);
        Block right_end = cur_block();
        branch_to(end_block);
        build_at(end_block);
        if (both)
            return make_val(phi_of(S->ir.ptr,
                                   same(op, "&&")
                                       ? LLVMConstNull(S->ir.ptr)
                                       : left.value,
                                   before, right_val, right_end),
                            left_type);
        return make_val(
            phi_of(S->ir.i1,
                   const_int(S->ir.i1, same(op, "&&") ? 0 : 1), before,
                   right_val, right_end),
            basic(T_BOOL));
    }
    Val   right      = eval(node->kids[1], scope,
                     left_type->kind == T_ENUM || is_str(left_type) ||
                             left_type->kind == T_UNICHAR ||
                             left_type->kind == T_SCALAR
                                ? left_type
                                : 0);
    Type* right_type = right.type;
    bool  is_compare = same(op, "==") || same(op, "!=") ||
                      same(op, "<") || same(op, ">") ||
                      same(op, "<=") || same(op, ">=");
    if (same(op, "<=>")) {
        Type* type      = num_result(left_type, right_type);
        Value left_num  = cast_value(left, type),
              right_num = cast_value(right, type);
        return make_val(
            LLVMBuildSub(
                S->ir.build,
                LLVMBuildZExt(S->ir.build,
                              compare(">", left_num, right_num, type),
                              S->ir.i32, ""),
                LLVMBuildZExt(S->ir.build,
                              compare("<", left_num, right_num, type),
                              S->ir.i32, ""),
                ""),
            type_named("i32"));
    }
    if ((is_str(left_type) || left_type->kind == T_UNICHAR ||
         left_type->kind == T_CSTR) &&
        (is_str(right_type) || right_type->kind == T_UNICHAR ||
         right_type->kind == T_OBJECT || right_type->kind == T_CSTR) &&
        (is_str(left_type) || is_str(right_type)) &&
        !LLVMIsNull(right.value) &&
        !LLVMIsNull(
            left.value)) { /* a C literal compares by content too */
        Value left_str  = cast_value(left, basic(T_STRING)),
              right_str = cast_value(right, basic(T_STRING));
        if (same(op, "=="))
            return make_val(call_au("string_eq", "bpp", 2, left_str,
                                    chars_of(right_str, 0)),
                            basic(T_BOOL));
        if (same(op, "!="))
            return make_val(
                LLVMBuildNot(S->ir.build,
                             call_au("string_eq", "bpp", 2, left_str,
                                     chars_of(right_str, 0)),
                             ""),
                basic(T_BOOL));
        if (is_compare)
            return make_val(
                compare(op,
                        call_au("string_cmp", "lpp", 2, left_str,
                                chars_of(right_str, 0)),
                        const_i64(0), basic(T_INT)),
                basic(T_BOOL));
        if (same(op, "+"))
            return make_val(call_au("string_operator__add", "ppp", 2,
                                    left_str, right_str),
                            basic(T_STRING));
    }
    if (left_type->kind == T_STRUCT && right_type->kind == T_STRUCT &&
        (same(op, "==") || same(op, "!="))) {
        Value cmp_val =
            call_au("memcmp", "ippl", 3, addr_of(left.value),
                    addr_of(right.value), LLVMSizeOf(left_type->llvm));
        return make_val(
            LLVMBuildICmp(S->ir.build,
                          same(op, "==") ? LLVMIntEQ : LLVMIntNE,
                          cmp_val, const_i32(0), ""),
            basic(T_BOOL));
    }
    if (left_type->c_decl) {
        Node* arg_list = new_node(N_ARGS, 0, node->line);
        add_kid(arg_list, node->kids[1]);
        CD* method =
            c_method(left_type->c_decl, format("operator%s", op),
                     arg_list, scope);
        if (!method)
            emit_fail("no operator%s on %s", op, left_type->name);
        Type* out_type;
        Value value =
            c_call(method, left.value, arg_list, scope, &out_type);
        return make_val(value, out_type);
    }
    if ((left_type->kind == T_CLASS || left_type->kind == T_STRUCT) &&
        !left_type->decl && au_rec_of(left_type)) {
        Au_t op_member =
            au_member(au_rec_of(left_type), format("_%s", opname(op)),
                      AU_MEMBER_OPERATOR);
        if (!op_member)
            op_member = au_member(
                au_rec_of(left_type), opname(op),
                AU_MEMBER_FUNC); // an imported type's operator, or its
                                 // method of that name
        if (op_member) {
            Type* out_type;
            Value right_val = cast_value(
                right, from_au(((Au_t)op_member->args.origin[1])->src));
            Value value = au_call_member(op_member,
                                         left_type->kind == T_STRUCT
                                             ? addr_of(left.value)
                                             : left.value,
                                         1, &right_val, &out_type);
            return make_val(value, out_type);
        }
    }
    if ((right_type->kind == T_CLASS || right_type->kind == T_STRUCT) &&
        !right_type->decl && au_rec_of(right_type)) {
        Au_t op_member =
            au_member(au_rec_of(right_type), format("_l%s", opname(op)),
                      AU_MEMBER_OPERATOR);
        if (op_member) {
            Type* out_type;
            Value left_val = cast_value(
                left, from_au(((Au_t)op_member->args.origin[1])->src));
            Value value = au_call_member(op_member,
                                         right_type->kind == T_STRUCT
                                             ? addr_of(right.value)
                                             : right.value,
                                         1, &left_val, &out_type);
            return make_val(value, out_type);
        }
    }
    if ((left_type->kind == T_CLASS || left_type->kind == T_STRUCT) &&
        left_type->decl) {
        Node* op_node = find_op(left_type, op, false);
        if (op_node) {
            Type* owner;
            find_method(left_type, op_node->text, D_OPFN, &owner);
            Type* want         = param_type(op_node->kids[0]->kids[0]);
            bool  is_struct    = left_type->kind == T_STRUCT;
            Value call_args[2] = {is_struct ? addr_of(left.value)
                                            : left.value,
                                  is_struct && want->kind == T_STRUCT
                                      ? addr_of(cast_value(right, want))
                                      : cast_value(right, want)};
            return make_val(
                call_fn(LLVMGetNamedFunction(
                            S->ir.module,
                            method_cname(owner ? owner : left_type,
                                         op_node)),
                        2, call_args),
                fn_ret(op_node));
        }
    }
    if ((right_type->kind == T_CLASS || right_type->kind == T_STRUCT) &&
        right_type->decl) {
        Node* op_node = find_op(right_type, op, true);
        if (op_node) {
            Type* owner;
            find_method(right_type, op_node->text, D_OPFN, &owner);
            Type* want         = param_type(op_node->kids[0]->kids[0]);
            bool  is_struct    = right_type->kind == T_STRUCT;
            Value call_args[2] = {is_struct ? addr_of(right.value)
                                            : right.value,
                                  is_struct && want->kind == T_STRUCT
                                      ? addr_of(cast_value(left, want))
                                      : cast_value(left, want)};
            return make_val(
                call_fn(LLVMGetNamedFunction(
                            S->ir.module,
                            method_cname(owner ? owner : right_type,
                                         op_node)),
                        2, call_args),
                fn_ret(op_node));
        }
    }
    if (left_type->kind == T_VEC) {
        OpCtx op_ctx = {op, left_type->elem};
        return make_val(
            vec_map(left, right, left_type->elem, op_arith, &op_ctx),
            left_type);
    }
    if (left_type->kind == T_REF && right_type->kind == T_REF &&
        same(op, "-"))
        return make_val(
            LLVMBuildSub(S->ir.build,
                         LLVMBuildPtrToInt(S->ir.build, left.value,
                                           S->ir.i64, ""),
                         LLVMBuildPtrToInt(S->ir.build, right.value,
                                           S->ir.i64, ""),
                         ""),
            basic(T_INT));
    if (left_type->kind == T_REF && (same(op, "+") || same(op, "-"))) {
        Value delta = cast_value(right, basic(T_INT));
        return make_val(
            index_ptr(S->ir.i8, left.value,
                      same(op, "-")
                          ? LLVMBuildNeg(S->ir.build, delta, "")
                          : delta),
            left_type);
    }
    if (is_compare) {
        if (is_num(left_type) && is_num(right_type)) {
            Type* type = num_result(left_type, right_type);
            return make_val(compare(op, cast_value(left, type),
                                    cast_value(right, type), type),
                            basic(T_BOOL));
        }
        return make_val(compare(op, fit(left.value, S->ir.ptr),
                                fit(right.value, S->ir.ptr), left_type),
                        basic(T_BOOL));
    }
    Type* type = is_num(left_type) && is_num(right_type)
                     ? num_result(left_type, right_type)
                 : left_type->kind == T_UNK ? right_type
                                            : left_type;
    if (left_type->kind == T_SCALAR && right_type->kind == T_SCALAR &&
        !same(left_type->name, right_type->name))
        type = backing(left_type);
    return make_val(arith_op(op, cast_value(left, type),
                             cast_value(right, type), type),
                    type);
}
static Value stamped_alloc(Type* type, int line, const char* bind,
                           Value holder) {
    return call_au("alloc_object", "pplppppiipp", 10, type_record(type),
                   const_i64(1), LLVMConstNull(S->ir.ptr),
                   LLVMConstNull(S->ir.ptr), LLVMConstNull(S->ir.ptr),
                   const_str(S->cur_file), const_i32(line),
                   const_i32(0), const_str(bind), holder);
}
static Val construct(Type* type, Node* args, Scope* scope) {
    int pos_count = 0, prop_count = 0;
    for (int i = 0; i < args->count; i++)
        (args->kids[i]->kind == N_PROP ||
         args->kids[i]->kind == N_SHORTPROP)
            ? prop_count++
            : pos_count++;
    Val first = {0};
    if (pos_count == 1 && !prop_count)
        first = eval(args->kids[0], scope,
                     type->kind == T_VEC ? type->elem
                     : type->kind == T_ENUM || type->kind == T_SCALAR ||
                             type->kind == T_CLASS
                         ? 0
                         : type);
    switch (type->kind) {
    case T_CLASS: {
        if (type->c_decl) {
            CD* c_rec = type->c_decl;
            if (first.value) return make_val(first.value, type);
            Value obj =
                au_alloc(type,
                         args->line); // a C++ record: flat memory,
                                      // fields by clang's offsets
            for (int i = 0; i < args->count; i++) {
                Node* prop  = args->kids[i];
                CD*   field = 0;
                for (int key = 0; key < c_rec->member_count; key++)
                    if (c_rec->members[key]->kind == CD_FIELD &&
                        same(c_rec->members[key]->name, prop->text))
                        field = c_rec->members[key];
                if (!field)
                    emit_fail("no field %s on %s", prop->text,
                              type->name);
                Type* field_ty = from_ct(field->result);
                LLVMBuildStore(S->ir.build,
                               eval_as(prop->kids[1], scope, field_ty),
                               byte_ptr(obj, field->offset));
            }
            return make_val(obj, type);
        }
        if (!type->decl) {
            Au_t rec = au_rec_of(
                type); // an imported class: props and constructors by
                       // its registered members
            if (first.value && is_num(first.type) ||
                first.type && is_str(first.type)) {
                const char* names[4] = {first.type->name, "i64", "f64",
                                        "string"};
                Au_t        ctor     = 0;
                for (int i = 0; i < 4 && !ctor; i++)
                    ctor = au_member(rec, format("with_%s", names[i]),
                                     AU_MEMBER_CONSTRUCT);
                if (ctor) {
                    Value obj = au_alloc(type, args->line);
                    Type* out_type;
                    Value call_args = cast_value(
                        first,
                        from_au(((Au_t)ctor->args.origin[1])->src));
                    au_call_member(ctor, obj, 1, &call_args, &out_type);
                    return make_val(au_init(obj), type);
                }
            }
            if (first.value) return make_val(first.value, type);
            Value obj = au_alloc(type, args->line);
            for (int i = 0; i < args->count; i++) {
                Node* prop   = args->kids[i];
                Au_t  member = au_member(au_rec_of(type), prop->text,
                                         AU_MEMBER_VAR);
                if (!member)
                    emit_fail("no member %s on %s", prop->text,
                              type->name);
                Type* member_type = from_au(member->type);
                LLVMBuildStore(
                    S->ir.build,
                    eval_as(prop->kids[1], scope, member_type),
                    byte_ptr(obj, member->offset));
            }
            return make_val(au_init(obj), type);
        }
        if (first.value) {
            bool  is_post = false;
            Node* ctor    = find_ctor(type, first.type, &is_post);
            if (!ctor) {
                if (is_obj(first.type) || first.type->kind == T_UNK)
                    return make_val(first.value, type);
                emit_fail("no construct on %s from %s", type->name,
                          first.type->name);
            }
            Value obj = au_alloc(type, args->line);
            call_au(format("%s_defaults", type->name), "vp", 1, obj);
            Value call_args[2] = {
                obj,
                cast_value(first, param_type(ctor->kids[0]->kids[0]))};
            if (is_post) {
                au_init(obj);
                call_fn(LLVMGetNamedFunction(S->ir.module,
                                             method_cname(type, ctor)),
                        2, call_args);
            } else {
                call_fn(LLVMGetNamedFunction(S->ir.module,
                                             method_cname(type, ctor)),
                        2, call_args);
                au_init(obj);
            }
            return make_val(obj, type);
        }
        Value obj = au_alloc(type, args->line);
        call_au(format("%s_defaults", type->name), "vp", 1, obj);
        for (int i = 0; i < args->count; i++) {
            Node* prop = args->kids[i];
            bool  is_positional =
                prop->kind != N_PROP && prop->kind != N_SHORTPROP;
            const char* name = is_positional ? member_at(type, i)
                               : prop->kind == N_SHORTPROP ? prop->text
                               : prop->text
                                   ? prop->text
                                   : (prop->kids[0]->kind == N_STR
                                          ? prop->kids[0]->text
                                          : 0);
            if (!name) emit_fail("prop name expected");
            Type* owner;
            Node* member = member_named(type, name, &owner);
            if (!member)
                emit_fail("no member %s on %s", name, type->name);
            Type* member_type = type_of(member->kids[0]);
            Val   val         = prop->kind == N_SHORTPROP
                                    ? ({
                                Var* var = find_var(scope, name);
                                if (!var)
                                    emit_fail("no local %s", name);
                                make_val(load(llvm_of(var->type),
                                                        var->address),
                                                   var->type);
                            })
                                    : eval(is_positional ? prop : prop->kids[1],
                                 scope, member_type);
            Value slot =
                field_ptr(type->llvm, obj, field_index(type, name));
            Value value = cast_value(val, member_type);
            LLVMBuildStore(S->ir.build, value, slot);
            if (member_type->kind == T_CLASS && prop->kind == N_PROP &&
                prop->kids[1]->kind == N_CONSTRUCT) {
                Value header = header_of(value);
                LLVMBuildStore(
                    S->ir.build, const_str(name),
                    byte_ptr(header, offsetof(struct _object, bind)));
                LLVMBuildStore(
                    S->ir.build, type_record(type),
                    byte_ptr(header, offsetof(struct _object, holder)));
            }
        }
        au_init(obj);
        return make_val(obj, type);
    }
    case T_STRUCT: {
        ensure_layout(type);
        if (first.value && first.type->kind == T_STRUCT)
            return make_val(cast_value(first, type), type);
        Value result = LLVMConstNull(type->llvm);
        for (int i = 0; i < args->count; i++) {
            Node*       prop = args->kids[i];
            const char* name =
                prop_count ? prop->text : type->field_names.data[i];
            Type* member_type = field_type(type, name);
            if (!member_type) emit_fail("no field %s", name);
            Val val = prop->kind == N_SHORTPROP
                          ? ({
                                Var* var = find_var(scope, name);
                                make_val(load(llvm_of(var->type),
                                              var->address),
                                         var->type);
                            })
                          : eval(prop_count ? prop->kids[1] : prop,
                                 scope, member_type);
            result  = LLVMBuildInsertValue(S->ir.build, result,
                                           cast_value(val, member_type),
                                           field_index(type, name), "");
        }
        return make_val(result, type);
    }
    case T_VEC: return make_val(vec_new(type, args, scope), type);
    case T_MAP: {
        Value map_obj = au_alloc(type, args->line);
        LLVMBuildStore(
            S->ir.build, const_i32(16),
            byte_ptr(map_obj,
                     au_member(au_type("map"), "hsize", 0)->offset));
        au_init(map_obj);
        for (int i = 0; i < args->count; i++) {
            Node* prop = args->kids[i];
            Value key  = eval_as(prop->kids[0], scope, type->key);
            Value var =
                cast_value(eval(prop->kids[1], scope, type->elem),
                           basic(T_OBJECT));
            call_au("map_set", "vppp", 3, map_obj, key, var);
        }
        return make_val(map_obj, type);
    }
    case T_ASYNC: {
        Value obj       = au_alloc(type, args->line);
        Au_t  async_rec = au_type("async");
        for (int i = 0; i < args->count; i++) {
            Value value = eval(args->kids[i]->kids[1], scope, 0).value;
            LLVMBuildStore(
                S->ir.build, value,
                byte_ptr(obj,
                         au_member(async_rec, args->kids[i]->text, 0)
                             ->offset));
        }
        return make_val(au_init(obj), type);
    }
    case T_STRING:
        if (!first.value) return lit_str("");
        return make_val(cast_value(first, type), type);
    case T_TOKENS: {
        Value obj =
            au_init(au_alloc(from_au(au_type("array")), args->line));
        char* words =
            strdup(args->kids[0]
                       ->text); /* bare words as an array of strings */
        for (char* word = strtok(words, " "); word;
             word       = strtok(0, " "))
            call_au("array_push", "ppp", 2, obj, lit_str(word).value);
        return make_val(obj, type);
    }
    default:
        if (!first.value)
            return make_val(LLVMConstNull(llvm_of(type)), type);
        return make_val(cast_value(first, type), type);
    }
}
static Value asm_block(Node* line_nodes, int first, Scope* scope,
                       const char* out_reg, Type* out_type) {
    Buf   template = {0}, constraints = {0};
    List  seen = {0};
    Value inputs[32];
    LType input_types[32];
    int   input_count = 0;
    Buf   clobbers    = {0};
    if (out_reg) append(&constraints, "=r");
    for (int i = first; i < line_nodes->count; i++) {
        Node* line_node = line_nodes->kids[i];
        if (line_node->count &&
            same(line_node->kids[0]->text, "return"))
            continue;
        for (const char* cursor = line_node->text; *cursor;) {
            if (isalpha(*cursor) || *cursor == '_') {
                const char* end = cursor;
                while (isalnum(*end) || *end == '_') end++;
                char* word = strndup(cursor, end - cursor);
                Var*  var  = find_var(scope, word);
                if (var) {
                    int index = -1;
                    for (int k = 0; k < seen.count; k++)
                        if (same(seen.data[k], word)) index = k;
                    if (index < 0) {
                        index = seen.count;
                        list_push(&seen, word);
                        inputs[input_count] =
                            load(llvm_of(var->type), var->address);
                        input_types[input_count++] = llvm_of(var->type);
                        append(&constraints, "%sr",
                               constraints.count ? "," : "");
                    }
                    append(&template, "$%d", index + (out_reg ? 1 : 0));
                } else {
                    append(&template, "%s", word);
                    if ((word[0] == 'r' || word[0] == 'x') &&
                        strlen(word) <= 3 && isalnum(word[1]) &&
                        !same(word, "ptr") &&
                        !(out_reg && same(word, out_reg)) &&
                        !strstr(clobbers.data ? clobbers.data : "",
                                word))
                        append(&clobbers, ",~{%s}", word);
                }
                cursor = end;
                continue;
            }
            append(&template, "%c", *cursor);
            cursor++;
        }
        append(&template, "\n\t");
    }
    if (out_reg) append(&template, "mov $0, %s", out_reg);
    append(&constraints, "%s,~{memory}",
           clobbers.data ? clobbers.data : "");
    LType func_type =
        LLVMFunctionType(out_reg ? llvm_of(out_type) : S->ir.void_type,
                         input_types, input_count, 0);
    Value inline_asm = LLVMGetInlineAsm(
        func_type, template.data, template.count, constraints.data,
        constraints.count, 1, 0, LLVMInlineAsmDialectIntel, 0);
    return LLVMBuildCall2(S->ir.build, func_type, inline_asm, inputs,
                          input_count, "");
}
static bool host_arch(const char* name) { return same(name, "x86_64"); }
static bool platform_value(Node* expr) {
    if (expr->kind == N_IDENT)
        return same(expr->text, "linux") || same(expr->text, "x86_64");
    if (expr->kind == N_UN && same(expr->text, "!"))
        return !platform_value(expr->kids[0]);
    if (expr->kind == N_BIN && same(expr->text, "||"))
        return platform_value(expr->kids[0]) ||
               platform_value(expr->kids[1]);
    if (expr->kind == N_BIN && same(expr->text, "&&"))
        return platform_value(expr->kids[0]) &&
               platform_value(expr->kids[1]);
    return false;
}
static Val lvalue_of(
    Node*  node,
    Scope* scope) { // address of an assignable expression, or v = 0
    switch (node->kind) {
    case N_IDENT: {
        Var* var = find_var(scope, node->text);
        if (var) return make_val(var->address, var->type);
        Node* global_node = find_global(node->text);
        if (global_node) {
            Type* type = type_of(global_node->kids[0]);
            return make_val(
                global_var(node->text, llvm_of(type), false), type);
        }
        return make_val(0, 0);
    }
    case N_MEMBER: {
        Val   obj  = eval(node->kids[0], scope, 0);
        Type* type = obj.type;
        if ((type->kind == T_CLASS && type->decl) ||
            (type->kind == T_REF && type->elem->kind == T_STRUCT)) {
            Type* record = type->kind == T_REF ? type->elem : type;
            Type* owner;
            Node* member = member_named(record, node->text, &owner);
            if (!member) return make_val(0, 0);
            Type* member_type = type_of(member->kids[0]);
            if (member_mods(member) & 1)
                return make_val(
                    global_var(format("%s_%s", owner->name, node->text),
                               llvm_of(member_type), false),
                    member_type);
            return make_val(field_ptr(record->llvm, obj.value,
                                      field_index(record, node->text)),
                            member_type);
        }
        if (type->kind == T_STRUCT && type->decl) {
            Val base = lvalue_of(node->kids[0], scope);
            if (!base.value) return make_val(0, 0);
            Type* member_type = field_type(type, node->text);
            return make_val(field_ptr(type->llvm, base.value,
                                      field_index(type, node->text)),
                            member_type);
        }
        if (is_obj(type)) {
            Au_t rec = au_rec_of(type);
            Au_t member =
                rec ? au_member(rec, node->text, AU_MEMBER_VAR) : 0;
            if (member)
                return make_val(byte_ptr(obj.value, member->offset),
                                from_au(member->type));
        }
        return make_val(0, 0);
    }
    case N_CALL: {
        Val callee_val = eval(node->kids[0], scope, 0);
        if (callee_val.type->kind == T_CLASS ||
            is_str(callee_val.type) ||
            callee_val.type->kind == T_LAMBDA ||
            callee_val.type->kind == T_MAP)
            return make_val(0, 0);
        return index_addr(callee_val, node->kids[1], scope, true);
    }
    case N_LIST:
    case N_ARGS:
        return node->count == 1 ? lvalue_of(node->kids[0], scope)
                                : make_val(0, 0);
    default: return make_val(0, 0);
    }
}
static Val eval(Node* node, Scope* scope, Type* want) {
    switch (node->kind) {
    case N_NUM:
        if (node->token && node->token->xshape) {
            Value dims[8];
            for (int i = 0; i < node->token->dim_count; i++)
                dims[i] = const_i64(node->token->dims[i]);
            return make_val(shape_of(node->token->dim_count, dims),
                            basic(T_SHAPE));
        }
        {
            Type* type = want && (want->kind == T_ENUM ||
                                  want->kind == T_SCALAR ||
                                  want->kind == T_FLOAT)
                             ? want
                             : lit_int();
            return make_val(
                backing(type)->kind == T_FLOAT
                    ? LLVMConstReal(llvm_of(type),
                                    (double)node->token->int_value)
                    : const_int(llvm_of(type), node->token->int_value),
                type);
        }
    case N_FLT: {
        Type* type = want && want->kind == T_SCALAR ? want : lit_flt();
        return make_val(
            LLVMConstReal(llvm_of(type), node->token->float_value),
            type);
    }
    case N_STR:
        return make_val(interpolate(node->text, scope),
                        basic(T_STRING));
    case N_CSTR: return make_val(const_str(node->text), basic(T_CSTR));
    case N_CHAR:
        if (want && is_str(want))
            return make_val(
                new_string("string_with_unichar", "i",
                           const_i32(node->token->int_value)),
                basic(T_STRING));
        return make_val(const_i32(node->token->int_value),
                        basic(T_UNICHAR));
    case N_SCALARLIT: {
        Type* type = type_named(node->text);
        return make_val(
            backing(type)->kind == T_FLOAT
                ? LLVMConstReal(
                      llvm_of(type),
                      node->kids[0]->kind == N_FLT
                          ? node->kids[0]->token->float_value
                          : (double)node->kids[0]->token->int_value)
                : const_int(llvm_of(type),
                            node->kids[0]->token->int_value),
            type);
    }
    case N_IDENT: {
        const char* name = node->text;
        Var*        var  = find_var(scope, name);
        if (var)
            return make_val(
                var->type->kind == T_LOCAL
                    ? var->address
                    : load(llvm_of(var->type), var->address),
                var->type);
        if (same(name, "null")) {
            Type* type = want ? want : basic(T_OBJECT);
            return make_val(LLVMConstNull(llvm_of(type)), type);
        }
        if (same(name, "true") || same(name, "false"))
            return make_val(const_int(S->ir.i1, name[0] == 't'),
                            basic(T_BOOL));
        if (want && want->kind == T_ENUM)
            for (int i = 1; i < want->decl->count; i++)
                if (same(want->decl->kids[i]->text, name)) {
                    double enum_value = atof(want->decl->kids[i]->raw);
                    return make_val(
                        backing(want)->kind == T_FLOAT
                            ? LLVMConstReal(llvm_of(want), enum_value)
                            : const_int(llvm_of(want),
                                        (long long)enum_value),
                        want);
                }
        Node* global_node = find_global(name);
        if (global_node) {
            Type* type       = type_of(global_node->kids[0]);
            Value global_val = global_var(name, llvm_of(type), false);
            return make_val(type->kind == T_LOCAL
                                ? global_val
                                : load(llvm_of(type), global_val),
                            type);
        }
        if (same(name, "linux") || same(name, "x86_64") ||
            same(name, "apple") || same(name, "windows") ||
            same(name, "arm64"))
            return make_val(const_int(S->ir.i1, platform_value(node)),
                            basic(T_BOOL));
        if (same(name, "__LINE__"))
            return make_val(const_i32(node->line), type_named("i32"));
        if (same(name, "__FILE__"))
            return make_val(const_str(S->cur_file), basic(T_CSTR));
        if (same(name, "__SEQUENCE__"))
            return make_val(const_i64(S->seq_counter++), basic(T_INT));
        static const char* consts[] = {
            "AU_MEMBER_NONE",      "AU_MEMBER_MODULE",
            "AU_MEMBER_TYPE",      "AU_MEMBER_CONSTRUCT",
            "AU_MEMBER_VAR",       "AU_MEMBER_FUNC",
            "AU_MEMBER_OPERATOR",  "AU_MEMBER_CAST",
            "AU_MEMBER_GETTER",    "AU_MEMBER_SETTER",
            "AU_MEMBER_ENUMV",     "AU_MEMBER_OVERRIDE",
            "AU_MEMBER_NAMESPACE", "AU_MEMBER_DECL",
            "AU_MEMBER_MACRO",     0};
        for (int i = 0; consts[i]; i++)
            if (same(consts[i], name))
                return make_val(const_i64(i), basic(T_INT));
        Node* func_node = find_func(name);
        if (func_node)
            return make_val(LLVMGetNamedFunction(S->ir.module, name),
                            ref_to(basic(T_NONE)));
        for (int i = 0; i < S->ncdecls; i++)
            if (S->cdecls[i]->kind == CD_ENUMCONST &&
                same(S->cdecls[i]->name, name))
                return make_val(const_i32(S->cdecls[i]->int_value),
                                type_named("i32"));
        for (int i = 0; i < S->ncdecls; i++)
            if (S->cdecls[i]->kind == CD_FUNC &&
                same(S->cdecls[i]->name, name)) {
                CD*   c_func_decl = S->cdecls[i];
                LType param_types[34];
                for (int k = 0; k < c_func_decl->param_count; k++)
                    param_types[k] =
                        llvm_of(from_ct(c_func_decl->params[k]));
                return make_val(
                    fn_named(c_func_decl->symbol,
                             LLVMFunctionType(
                                 llvm_of(from_ct(c_func_decl->result)),
                                 param_types, c_func_decl->param_count,
                                 c_func_decl->is_variadic)),
                    basic(T_HANDLE));
            } /* a C function as a value */
        emit_fail("unknown identifier %s", name);
    }
    case N_UN: {
        if (same(node->text, "@")) {
            Node* operand = node->kids[0];
            if (operand->kind == N_IDENT &&
                !find_var(scope, operand->text) &&
                find_func(operand->text))
                return make_val(
                    LLVMGetNamedFunction(S->ir.module, operand->text),
                    ref_to(basic(T_NONE)));
            Val addr = lvalue_of(operand, scope);
            if (!addr.value) {
                Val val = eval(operand, scope, 0);
                return make_val(addr_of(val.value), ref_to(val.type));
            }
            return make_val(addr.value,
                            ref_to(addr.type->kind == T_LOCAL
                                       ? addr.type->elem
                                       : addr.type));
        }
        Val val = eval(node->kids[0], scope, want);
        if (same(node->text, "*")) {
            if (val.type->kind == T_REF &&
                val.type->elem->kind == T_REF &&
                same(val.type->elem->name, "carray"))
                return make_val(val.value, val.type->elem);
            if (val.type->kind == T_REF &&
                val.type->elem->kind != T_LOCAL)
                return make_val(
                    load(llvm_of(val.type->elem), val.value),
                    val.type->elem);
            return val;
        } /* records and arrays are already their pointer */
        if (same(node->text, "!"))
            return make_val(LLVMBuildNot(S->ir.build, to_bool(val), ""),
                            basic(T_BOOL));
        if (same(node->text, "~"))
            return make_val(LLVMBuildNot(S->ir.build, val.value, ""),
                            val.type);
        return make_val(backing(val.type)->kind == T_FLOAT
                            ? LLVMBuildFNeg(S->ir.build, val.value, "")
                            : LLVMBuildNeg(S->ir.build, val.value, ""),
                        val.type);
    }
    case N_BIN: return binop(node, scope, want);
    case N_TERN: {
        Value cond       = to_bool(eval(node->kids[0], scope, 0));
        Block then_block = new_block("then"),
              else_block = new_block("else"),
              end_block  = new_block("end");
        LLVMBuildCondBr(S->ir.build, cond, then_block, else_block);
        build_at(then_block);
        Val   then_val = eval(node->kids[1], scope, want);
        Block then_end = cur_block();
        branch_to(end_block);
        build_at(else_block);
        Val   else_val = eval(node->kids[2], scope, then_val.type);
        Type* type     = then_val.type->kind == T_OBJECT ? else_val.type
                                                         : then_val.type;
        Value else_conv = cast_value(else_val, type);
        Block else_end  = cur_block();
        branch_to(end_block);
        build_at(end_block);
        return make_val(phi_of(llvm_of(type),
                               cast_value(then_val, type), then_end,
                               else_conv, else_end),
                        type);
    }
    case N_LIST:
        if (node->flag ||
            (node->count == 1 && !(want && want->kind == T_VEC)))
            return eval(node->kids[0], scope, want);
        {
            Type* vec_type = want && want->kind == T_VEC ? want : 0;
            if (!vec_type) {
                if (!node->count) emit_fail("untyped empty list");
                vec_type = vec_of(eval(node->kids[0], scope, 0).type);
            }
            Type* list_type = vec_of(vec_type->elem);
            return make_val(vec_new(list_type, node, scope), list_type);
        }
    case N_TYPEREF: {
        Type* type = type_of(node->kids[0]);
        if (type->kind == T_VEC && type->growable)
            return make_val(vec_new(type, 0, scope), type);
        if (type->kind == T_MAP && node->kids[0]->flag)
            return construct(type, new_node(N_ARGS, 0, node->line),
                             scope);
        return make_val(LLVMConstNull(llvm_of(type)), type);
    }
    case N_CONSTRUCT: {
        CD* c_func_decl =
            type_named(node->kids[0]->text)
                ? 0
                : c_func(node->kids[0]->text, node->kids[1]->count);
        if (c_func_decl) {
            Type* out_type;
            Value value =
                c_call(c_func_decl, 0, node->kids[1], scope, &out_type);
            return make_val(value, out_type);
        }
        return construct(type_of(node->kids[0]), node->kids[1], scope);
    }
    case N_CONV: {
        Type* type = type_of(node->kids[0]);
        return make_val(eval_as(node->kids[1], scope, type), type);
    }
    case N_CAST: {
        Type* type = type_of(node->kids[0]);
        Val   val  = eval(node->kids[1]->kids[0], scope, 0);
        if (val.type->kind == T_SCALAR && type->kind == T_SCALAR &&
            find_cast(val.type, type->name, 0))
            return make_val(
                call_fn(LLVMGetNamedFunction(S->ir.module,
                                             format("%s_cast_%s",
                                                    val.type->name,
                                                    type->name)),
                        1, &val.value),
                type);
        return make_val(cast_value(val, type), type);
    }
    case N_TYPEID:
        if (node->kids[0]->kind == N_TYPE)
            return make_val(type_record(type_of(node->kids[0])),
                            basic(T_TYPE));
        if (node->kids[0]->kind == N_TYPEREF)
            return make_val(
                type_record(type_of(node->kids[0]->kids[0])),
                basic(T_TYPE));
        return make_val(
            load(S->ir.ptr,
                 header_of(eval(node->kids[0], scope, 0).value)),
            basic(T_TYPE));
    case N_SIZEOF: {
        if (node->kids[0]->kind == N_TYPE ||
            node->kids[0]->kind == N_IDENT) {
            CD* decl = c_find(node->kids[0]->text, CD_TYPEDEF);
            if (!decl) decl = c_find(node->kids[0]->text, CD_RECORD);
            if (decl && decl->size)
                return make_val(const_i64(decl->size), basic(T_INT));
        }
        LType llvm_type =
            node->kids[0]->kind == N_TYPE
                ? llvm_of(type_of(node->kids[0]))
            : node->kids[0]->kind == N_TYPEREF
                ? llvm_of(type_of(node->kids[0]->kids[0]))
                : LLVMTypeOf(eval(node->kids[0], scope, 0).value);
        return make_val(LLVMSizeOf(llvm_type), basic(T_INT));
    }
    case N_MODULEID: {
        if (node->kids[0]->kind == N_IDENT &&
            same(node->kids[0]->text, S->modname))
            return make_val(
                load(S->ir.ptr,
                     global_var("s2_module", S->ir.ptr, false)),
                basic(T_TYPE));
        Value str =
            new_string("string_with_cstr", "p", const_str("silver-"));
        call_au("string_append", "vpp", 2, str,
                chars_of(eval_as(node->kids[0], scope, basic(T_STRING)),
                         0));
        return make_val(
            call_au("module_lookup", "pp", 1, chars_of(str, 0)),
            basic(T_TYPE));
    }
    case N_EXPECTX: {
        Value ok         = to_bool(eval(node->kids[0], scope, 0));
        Block fail_block = new_block("expect_fail"),
              ok_block   = new_block("expect_ok");
        LLVMBuildCondBr(S->ir.build, ok, ok_block, fail_block);
        build_at(fail_block);
        call_au("fault_expect", "vpi", 2, const_str("expect"),
                const_i32(node->line));
        LLVMBuildUnreachable(S->ir.build);
        build_at(ok_block);
        return make_val(ok, basic(T_BOOL));
    }
    case N_RAW: {
        Type* type = node->count && node->kids[0]->kind == N_TYPE
                         ? type_of(node->kids[0])
                     : want ? want
                            : basic(T_STRING);
        if (type->kind == T_TOKENS) {
            Node* args = new_node(N_ARGS, 0, node->line);
            add_kid(args, new_node(N_CSTR, node->text, node->line));
            return construct(type, args, scope);
        }
        return lit_str(node->text);
    }
    case N_LAMBDA: return make_lambda(node, scope);
    case N_BIND: return bind_lambda(node, scope);
    case N_CALL: return call_expr(node, scope);
    case N_TYPEMEMBER: {
        Type* type = type_of(node->kids[0]);
        if (type->kind == T_ENUM)
            return eval(new_node(N_IDENT, node->text, node->line),
                        scope, type);
        if (type->kind == T_CLASS && type->decl) {
            Type* owner;
            Node* member = member_named(type, node->text, &owner);
            if (member && (member_mods(member) & 1)) {
                Type* member_type = type_of(member->kids[0]);
                return make_val(
                    load(llvm_of(member_type),
                         global_var(
                             format("%s_%s", owner->name, node->text),
                             llvm_of(member_type), false)),
                    member_type);
            }
        }
        emit_fail("unsupported %s.%s", type->name, node->text);
    }
    case N_MEMBER: {
        Val obj = eval(node->kids[0], scope, 0);
        if ((obj.type->kind == T_CLASS || obj.type->kind == T_STRUCT) &&
            obj.type->decl &&
            find_method(obj.type, node->text, D_FUNC, 0)) {
            Node* call = new_node(N_CALL, 0, node->line);
            add_kid(call, node);
            add_kid(call, new_node(N_ARGS, 0, node->line));
            return call_expr(call, scope);
        }
        if (obj.type->kind == T_STRUCT && obj.type->decl &&
            (node->kids[0]->kind == N_IDENT ||
             node->kids[0]->kind == N_MEMBER)) {
            Val addr = lvalue_of(node, scope);
            if (addr.value)
                return make_val(
                    addr.type->kind == T_LOCAL
                        ? addr.value
                        : load(llvm_of(addr.type), addr.value),
                    addr.type);
        }
        return member_val(obj, node->text);
    }
    case N_ARROW: {
        Val   obj         = eval(node->kids[0], scope, 0);
        Block before      = cur_block();
        Block arrow_block = new_block("arrow"),
              end_block   = new_block("end");
        LLVMBuildCondBr(S->ir.build, to_bool(obj), arrow_block,
                        end_block);
        build_at(arrow_block);
        Val   member    = member_val(obj, node->text);
        Block arrow_end = cur_block();
        branch_to(end_block);
        build_at(end_block);
        return make_val(phi_of(llvm_of(member.type),
                               LLVMConstNull(llvm_of(member.type)),
                               before, member.value, arrow_end),
                        member.type);
    }
    case N_FMT: {
        Val         val    = eval(node->kids[0], scope, 0);
        const char* spec   = node->text;
        char        letter = spec[strlen(spec) - 1];
        char*       digits = strndup(spec, strlen(spec) - 1);
        Value       result;
        if (letter == 'f')
            result =
                fmt_str(format("%%.%sf", digits), val.value, S->ir.f64);
        else if (letter == 's')
            result = fmt_str(format("%%.%ss", digits),
                             chars_of(val.value, val.type), S->ir.ptr);
        else
            result = fmt_str(format("%%0%sll%c", digits, letter),
                             val.value, S->ir.i64);
        if (val.type->kind == T_SCALAR)
            call_au("string_append", "vpp", 2, result,
                    const_str(val.type->name));
        return make_val(result, basic(T_STRING));
    }
    case N_ASMX: {
        Type*       type    = type_of(node->kids[0]);
        const char* out_reg = 0;
        for (int i = 2; i < node->count; i++)
            if (node->kids[i]->count &&
                same(node->kids[i]->kids[0]->text, "return"))
                out_reg = node->kids[i]->kids[1]->text;
        if (!host_arch("x86_64"))
            return make_val(LLVMConstNull(llvm_of(type)), type);
        return make_val(asm_block(node, 2, scope, out_reg, type), type);
    }
    case N_ARGS:
        if (node->count == 1 && node->kids[0]->kind != N_PROP)
            return eval(node->kids[0], scope, want);
        if (want) return construct(want, node, scope);
        emit_fail("untyped args");
    case N_SUPER: emit_fail("super outside a call");
    case N_ORRET: emit_fail("|| return outside a declaration");
    default: emit_fail("unsupported expression kind %d", node->kind);
    }
    return make_val(0, 0);
}
// statements
static void emit_return(
    Val val) { // return, then keep building in an unreachable block
    timing_end();
    if (S->cur_fn->infer_result) {
        S->cur_fn->result =
            val.type && val.type->kind == T_INT && !val.type->bits
                ? basic(T_INT)
                : val.type;
        S->cur_fn->infer_result = false;
        LLVMBuildRetVoid(S->ir.build);
    } else if (S->cur_fn->result->kind == T_NONE)
        LLVMBuildRetVoid(S->ir.build);
    else if (S->cur_fn->result->kind == T_STRUCT) {
        Value value = cast_value(val, S->cur_fn->result);
        if (S->cur_fn->result_ptr) {
            LLVMBuildStore(S->ir.build, value, S->cur_fn->result_ptr);
            LLVMBuildRetVoid(S->ir.build);
        } else {
            Value slot = stack_slot(S->cur_fn->result->llvm);
            LLVMBuildStore(S->ir.build, value, slot);
            LLVMBuildRet(S->ir.build,
                         load(abi_ret(S->cur_fn->result), slot));
        }
    } else
        LLVMBuildRet(
            S->ir.build,
            S->cur_fn->class_type &&
                    S->cur_fn->class_type->kind == T_SCALAR
                ? number_cast(val.value, val.type, S->cur_fn->result)
                : cast_value(val, S->cur_fn->result));
    build_at(new_block("dead"));
}
static void stamp(Value obj, const char* name) {
    Value header = header_of(obj);
    LLVMBuildStore(S->ir.build, const_str(name),
                   byte_ptr(header, offsetof(struct _object, bind)));
    LLVMBuildStore(
        S->ir.build,
        load(S->ir.ptr, global_var("s2_module", S->ir.ptr, false)),
        byte_ptr(header, offsetof(struct _object, holder)));
}
static void decl_stmt(Node* node, Scope* scope, Type** declared) {
    const char* name      = node->text;
    Node*       type_node = kid(node, 0);
    Node*       init      = kid(node, 1);
    Type*       type      = type_node ? type_of(type_node) : 0;
    Value       value     = 0, address;
    if (type && type->kind == T_LOCAL) {
        LType elem_llvm = llvm_of(type->elem);
        if (type->dims && type->dims->count &&
            type->dims->kids[0]->kind != N_NUM) {
            address = addr_of(LLVMBuildArrayAlloca(
                S->ir.build, elem_llvm,
                eval_as(type->dims->kids[0], scope, basic(T_INT)),
                name));
            type    = ref_to(type->elem);
        } else {
            long count = type->dims && type->dims->count
                             ? type->dims->kids[0]->token->int_value
                         : init ? init->count
                                : 1;
            if (!type->dims || !type->dims->count) {
                Node* dim_node  = new_node(N_NUM, 0, node->line);
                dim_node->token = calloc(1, sizeof(Token));
                dim_node->token->int_value = count;
                type->dims = new_node(N_ARGS, 0, node->line);
                add_kid(type->dims, dim_node);
            }
            address = stack_slot(llvm_of(type));
            LLVMBuildStore(S->ir.build, LLVMConstNull(llvm_of(type)),
                           address);
            if (node->flag && init)
                for (int i = 0; i < init->count; i++) {
                    Value indices[2] = {const_i64(0), const_i64(i)};
                    LLVMBuildStore(
                        S->ir.build,
                        eval_as(init->kids[i], scope, type->elem),
                        LLVMBuildGEP2(S->ir.build, llvm_of(type),
                                      address, indices, 2, ""));
                }
        }
        declare_var(scope, name, type, address);
        if (declared) *declared = type;
        return;
    }
    if (init && init->kind == N_ORRET) {
        Val   left         = eval(init->kids[0], scope, type);
        Type* value_type   = type ? type : left.type;
        Value or_value     = cast_value(left, value_type);
        Block ok_block     = new_block("ok"),
              return_block = new_block("orret");
        LLVMBuildCondBr(S->ir.build,
                        to_bool(make_val(or_value, value_type)),
                        ok_block, return_block);
        build_at(return_block);
        emit_return(init->kids[1]
                        ? eval(init->kids[1], scope, S->cur_fn->result)
                        : make_val(0, basic(T_NONE)));
        build_at(ok_block);
        address = stack_slot(llvm_of(value_type));
        LLVMBuildStore(S->ir.build, or_value, address);
        declare_var(scope, name, value_type, address);
        if (declared) *declared = value_type;
        return;
    }
    bool is_stamped = false;
    if (type && node->flag) {
        Val built = construct(
            type, init ? init : new_node(N_ARGS, 0, node->line), scope);
        value      = built.value;
        is_stamped = type->kind == T_CLASS;
    } else if (type && init) {
        value      = eval_as(init, scope, type);
        is_stamped = type->kind == T_CLASS && init->kind == N_CONSTRUCT;
    } else if (type)
        value = type->kind == T_VEC && type->growable
                    ? vec_new(type, 0, scope)
                : type->kind == T_MAP && type_node->flag
                    ? construct(type, new_node(N_ARGS, 0, node->line),
                                scope)
                          .value
                    : LLVMConstNull(llvm_of(type));
    else {
        Val expr_val = eval(init, scope, 0);
        type         = expr_val.type;
        if (type->kind == T_INT && !type->bits) type = basic(T_INT);
        if (type->kind == T_FLOAT && !type->bits) type = basic(T_FLOAT);
        value      = expr_val.value;
        is_stamped = type->kind == T_CLASS && init->kind == N_CONSTRUCT;
    }
    if (is_stamped && !type->c_decl)
        stamp(value, name); // C records carry no Au header
    if (type->kind == T_LOCAL) type = ref_to(type->elem);
    address = stack_slot(llvm_of(type));
    LLVMBuildStore(S->ir.build, value, address);
    declare_var(scope, name, type, address);
    if (declared) *declared = type;
}
static void assign_stmt(Node* node, Scope* scope) {
    const char* op  = node->text;
    Node*       lhs = node->kids[0];
    Node*       rhs = node->kids[1];
    if (lhs->kind == N_CALL) { // index target
        Val   container      = eval(lhs->kids[0], scope, 0);
        Node* args           = lhs->kids[1];
        Type* container_type = container.type;
        if (container_type->kind == T_CLASS && container_type->decl) {
            Type* owner;
            Node* setter =
                find_method(container_type, "setter", D_SETTER, &owner);
            if (!setter) emit_fail("no setter");
            Type* value_param = param_type(setter->kids[0]->kids[1]);
            static const char* ops[] = {
                "=",  "+=", "-=", "*=",  "/=",  "|=",
                "&=", "^=", "%=", ">>=", "<<=", 0};
            int code = 31;
            for (int key = 0; ops[key]; key++)
                if (same(ops[key], op)) code = 31 + key;
            Value call_args[4] = {
                container.value,
                eval_as(args->kids[0], scope,
                        param_type(setter->kids[0]->kids[0])),
                eval_as(rhs, scope, value_param), const_i32(code)};
            call_fn(LLVMGetNamedFunction(S->ir.module,
                                         method_cname(owner, setter)),
                    4, call_args);
            return;
        }
        if (container_type->kind == T_MAP && same(op, "=")) {
            Value key =
                eval_as(args->kids[0], scope, container_type->key);
            call_au("map_set", "vppp", 3, container.value, key,
                    cast_value(eval(rhs, scope, container_type->elem),
                               basic(T_OBJECT)));
            return;
        }
        if (container_type->kind == T_VEC && same(op, "=") &&
            is_obj(container_type->elem)) {
            Val   slot_addr = index_addr(container, args, scope, true);
            Value new_val   = eval_as(rhs, scope, container_type->elem);
            Value prev_val  = load(S->ir.ptr, slot_addr.value);
            hold_value(new_val);
            LLVMBuildStore(S->ir.build, new_val, slot_addr.value);
            call_au("drop", "vp", 1, prev_val);
            return;
        }
    }
    Val target = lvalue_of(lhs, scope);
    if (!target.value) emit_fail("cannot assign to this expression");
    Type* target_type = target.type;
    if (same(op, "+=") && target_type->kind == T_VEC) {
        Value item = eval_as(rhs, scope, target_type->elem);
        call_au("vector_push", "ppp", 2, load(S->ir.ptr, target.value),
                is_obj(target_type->elem) ? item : addr_of(item));
        return;
    }
    if (!same(op, "=") &&
        (target_type->kind == T_CLASS ||
         target_type->kind == T_STRUCT) &&
        target_type->decl) {
        Node* op_node = find_op(target_type, op, false);
        if (op_node) {
            Type* owner;
            find_method(target_type, op_node->text, D_OPFN, &owner);
            Type* want         = param_type(op_node->kids[0]->kids[0]);
            Value call_args[2] = {
                target_type->kind == T_STRUCT
                    ? target.value
                    : load(llvm_of(target_type), target.value),
                target_type->kind == T_STRUCT && want->kind == T_STRUCT
                    ? addr_of(eval_as(rhs, scope, want))
                    : eval_as(rhs, scope, want)};
            call_fn(LLVMGetNamedFunction(S->ir.module,
                                         method_cname(owner, op_node)),
                    2, call_args);
            return;
        }
    }
    Value value = eval_as(rhs, scope, target_type);
    if (!same(op, "=")) {
        char* base_op = strndup(op, strlen(op) - 1);
        value =
            arith_op(base_op, load(llvm_of(target_type), target.value),
                     value, target_type);
    }
    Value owner = 0;
    if (same(op, "=") && lhs->kind == N_MEMBER) {
        Val obj = eval(lhs->kids[0], scope, 0);
        if (obj.type->kind == T_CLASS && obj.type->decl) {
            Node* member = member_named(obj.type, lhs->text, 0);
            if (member && member_held(member)) owner = obj.value;
        }
    }
    if (same(op, "=") && lhs->kind == N_IDENT && S->cur_fn &&
        S->cur_fn->class_type &&
        S->cur_fn->class_type->kind == T_CLASS) {
        Node* member =
            member_named(S->cur_fn->class_type, lhs->text, 0);
        Var* var = find_var(scope, lhs->text);
        if (member && var && !(member_mods(member) & 1) &&
            member_held(member) &&
            LLVMIsAGetElementPtrInst(var->address))
            owner = S->cur_fn->self;
    }
    if (owner) set_slot(owner, target.value, value);
    else LLVMBuildStore(S->ir.build, value, target.value);
}
static void emit_stmt(Node* node, Scope* scope) {
    set_line(node->line);
    switch (node->kind) {
    case S_DECL: decl_stmt(node, scope, 0); break;
    case S_ASSIGN: assign_stmt(node, scope); break;
    case S_EXPR: {
        Node* expr = node->kids[0];
        if (expr->kind == N_MEMBER) {
            Type* obj_type = eval(expr->kids[0], scope, 0).type;
            if ((obj_type->kind == T_CLASS ||
                 obj_type->kind == T_STRUCT) &&
                obj_type->decl &&
                find_method(obj_type, expr->text, D_FUNC, 0)) {
                Node* call = new_node(N_CALL, 0, node->line);
                add_kid(call, expr);
                add_kid(call, new_node(N_ARGS, 0, node->line));
                expr = call;
            }
        }
        eval(expr, scope, 0);
        break;
    }
    case S_RETURN:
        if (!node->count || !node->kids[0])
            emit_return(make_val(0, basic(T_NONE)));
        else
            emit_return(
                eval(node->kids[0], scope,
                     S->cur_fn->infer_result ? 0 : S->cur_fn->result));
        break;
    case S_BREAK:
    case S_CONTINUE: {
        if (!S->cur_fn->loop_count) emit_fail("break outside a loop");
        Loop* loop =
            &S->cur_fn->loops[S->cur_fn->loop_count - 1 -
                              (node->flag < S->cur_fn->loop_count
                                   ? node->flag
                                   : S->cur_fn->loop_count - 1)];
        LLVMBuildBr(S->ir.build, node->kind == S_BREAK
                                     ? loop->break_block
                                     : loop->continue_block);
        build_at(new_block("dead"));
        break;
    }
    case S_IF: {
        Value cond       = to_bool(eval(node->kids[0], scope, 0));
        Block then_block = new_block("then"),
              else_block = new_block("else"),
              end_block  = new_block("endif");
        LLVMBuildCondBr(S->ir.build, cond, then_block, else_block);
        build_at(then_block);
        emit_block(node->kids[1], scope);
        branch_to(end_block);
        build_at(else_block);
        if (kid(node, 2)) emit_block(node->kids[2], scope);
        branch_to(end_block);
        build_at(end_block);
        break;
    }
    case S_IFDEF: {
        bool is_on = platform_value(node->kids[0]);
        if (same(node->text, "ifndef")) is_on = !is_on;
        if (is_on) emit_block(node->kids[1], scope);
        else if (kid(node, 2)) emit_block(node->kids[2], scope);
        break;
    }
    case S_WHILE:
    case S_FOR:
    case S_DOWHILE:
    case S_FORIN: {
        Scope inner_scope = {0, scope};
        Block cond = new_block("cond"), body = new_block("body"),
              continue_block = new_block("cont"),
              end            = new_block("end");
        S->cur_fn->loops[S->cur_fn->loop_count++] =
            (Loop){end, continue_block};
        if (node->kind == S_WHILE) {
            branch_to(cond);
            build_at(cond);
            LLVMBuildCondBr(S->ir.build,
                            to_bool(eval(node->kids[0], scope, 0)),
                            body, end);
            build_at(body);
            emit_block(node->kids[1], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            LLVMBuildBr(S->ir.build, cond);
        } else if (node->kind == S_DOWHILE) {
            branch_to(body);
            build_at(body);
            emit_block(node->kids[0], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            LLVMBuildCondBr(S->ir.build,
                            to_bool(eval(node->kids[1], scope, 0)),
                            body, end);
        } else if (node->kind == S_FOR) {
            Node* parts = node->kids[0];
            if (parts->count >= 2)
                emit_stmt(parts->kids[0], &inner_scope);
            branch_to(cond);
            build_at(cond);
            Node* cond_node = parts->count >= 2 ? parts->kids[1]
                              : parts->count    ? parts->kids[0]
                                                : 0;
            if (cond_node)
                LLVMBuildCondBr(
                    S->ir.build,
                    to_bool(eval(cond_node->kids[0], &inner_scope, 0)),
                    body, end);
            else LLVMBuildBr(S->ir.build, body);
            build_at(body);
            emit_block(node->kids[1], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            if (parts->count >= 3)
                emit_stmt(parts->kids[2], &inner_scope);
            LLVMBuildBr(S->ir.build, cond);
        } else if (node->kids[1]->kind == N_MEMBER &&
                   same(node->kids[1]->text,
                        "members")) { // an Au_t's member list
            Node* vars        = node->kids[0];
            Val   members_val = eval(node->kids[1], scope, 0);
            Value origin      = load(S->ir.ptr, members_val.value);
            Value count       = LLVMBuildSExt(
                S->ir.build,
                load(S->ir.i32, byte_ptr(members_val.value, 8)),
                S->ir.i64, "");
            Value index_slot = stack_slot(S->ir.i64);
            LLVMBuildStore(S->ir.build, const_i64(0), index_slot);
            branch_to(cond);
            build_at(cond);
            Value index = load(S->ir.i64, index_slot);
            LLVMBuildCondBr(S->ir.build,
                            LLVMBuildICmp(S->ir.build, LLVMIntSLT,
                                          index, count, ""),
                            body, end);
            build_at(body);
            Value elem_slot = stack_slot(S->ir.ptr);
            LLVMBuildStore(
                S->ir.build,
                load(S->ir.ptr, index_ptr(S->ir.ptr, origin, index)),
                elem_slot);
            declare_var(&inner_scope, vars->kids[0]->text,
                        basic(T_TYPE), elem_slot);
            emit_block(node->kids[2], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            LLVMBuildStore(
                S->ir.build,
                LLVMBuildAdd(S->ir.build, index, const_i64(1), ""),
                index_slot);
            LLVMBuildBr(S->ir.build, cond);
        } else if (eval(node->kids[1], scope, 0).type->kind ==
                   T_MAP) { // items, first to last or back
            Node* vars      = node->kids[0];
            Val   container = eval(node->kids[1], scope, 0);
            Au_t  map_rec = au_type("map"), item_rec = au_type("item");
            Value cursor = stack_slot(S->ir.ptr);
            LLVMBuildStore(
                S->ir.build,
                au_field(container.value,
                         au_member(map_rec,
                                   node->flag ? "last" : "first", 0),
                         S->ir.ptr),
                cursor);
            branch_to(cond);
            build_at(cond);
            Value item = load(S->ir.ptr, cursor);
            LLVMBuildCondBr(S->ir.build,
                            LLVMBuildICmp(S->ir.build, LLVMIntNE, item,
                                          LLVMConstNull(S->ir.ptr), ""),
                            body, end);
            build_at(body);
            Type* elem     = container.type->elem;
            Value item_val = au_field(
                item, au_member(item_rec, "value", 0), S->ir.ptr);
            Value elem_slot = stack_slot(llvm_of(elem));
            LLVMBuildStore(S->ir.build,
                           is_obj(elem) ? item_val
                                        : load(llvm_of(elem), item_val),
                           elem_slot);
            declare_var(&inner_scope, vars->kids[0]->text, elem,
                        elem_slot);
            if (vars->count > 1) {
                Value key_slot = stack_slot(S->ir.ptr);
                LLVMBuildStore(S->ir.build,
                               au_field(item,
                                        au_member(item_rec, "key", 0),
                                        S->ir.ptr),
                               key_slot);
                declare_var(&inner_scope, vars->kids[1]->text,
                            basic(T_STRING), key_slot);
            }
            emit_block(node->kids[2], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            LLVMBuildStore(
                S->ir.build,
                au_field(load(S->ir.ptr, cursor),
                         au_member(item_rec,
                                   node->flag ? "prev" : "next", 0),
                         S->ir.ptr),
                cursor);
            LLVMBuildBr(S->ir.build, cond);
        } else { // a vector, null iterates zero times
            Node* vars      = node->kids[0];
            Val   container = eval(node->kids[1], scope, 0);
            if (container.type->kind != T_VEC)
                emit_fail("cannot iterate %s", container.type->name);
            Value vec_slot    = addr_of(container.value);
            Value index_slot  = stack_slot(S->ir.i64);
            Block before      = cur_block();
            Block count_block = new_block("cnt");
            Block count_end   = new_block("cnt_end");
            LLVMBuildCondBr(S->ir.build, to_bool(container),
                            count_block, count_end);
            build_at(count_block);
            Value vec_len     = vec_count(container.value);
            Block count_after = cur_block();
            branch_to(count_end);
            build_at(count_end);
            Value count = phi_of(S->ir.i64, const_i64(0), before,
                                 vec_len, count_after);
            LLVMBuildStore(S->ir.build,
                           node->flag ? LLVMBuildSub(S->ir.build, count,
                                                     const_i64(1), "")
                                      : const_i64(0),
                           index_slot);
            branch_to(cond);
            build_at(cond);
            Value index = load(S->ir.i64, index_slot);
            LLVMBuildCondBr(S->ir.build,
                            node->flag
                                ? LLVMBuildICmp(S->ir.build, LLVMIntSGE,
                                                index, const_i64(0), "")
                                : LLVMBuildICmp(S->ir.build, LLVMIntSLT,
                                                index, count, ""),
                            body, end);
            build_at(body);
            Type* elem      = container.type->elem;
            Value elem_slot = stack_slot(llvm_of(elem));
            LLVMBuildStore(
                S->ir.build,
                load(llvm_of(elem),
                     vec_elem_ptr(elem, load(S->ir.ptr, vec_slot),
                                  index)),
                elem_slot);
            declare_var(&inner_scope, vars->kids[0]->text, elem,
                        elem_slot);
            emit_block(node->kids[2], &inner_scope);
            branch_to(continue_block);
            build_at(continue_block);
            LLVMBuildStore(
                S->ir.build,
                node->flag ? LLVMBuildSub(S->ir.build,
                                          load(S->ir.i64, index_slot),
                                          const_i64(1), "")
                           : LLVMBuildAdd(S->ir.build,
                                          load(S->ir.i64, index_slot),
                                          const_i64(1), ""),
                index_slot);
            LLVMBuildBr(S->ir.build, cond);
        }
        build_at(end);
        S->cur_fn->loop_count--;
        break;
    }
    case S_SWITCH: {
        Val   subject       = eval(node->kids[0], scope, 0);
        Block end           = new_block("swend");
        Block default_block = end;
        int   case_count    = 0;
        for (int i = 1; i < node->count; i++)
            if (!node->kids[i]->kids[0])
                default_block = new_block("default");
            else case_count += node->kids[i]->kids[0]->count;
        if (is_num(subject.type)) {
            Value switch_inst = LLVMBuildSwitch(
                S->ir.build, subject.value, default_block, case_count);
            for (int i = 1; i < node->count; i++) {
                Node* case_node  = node->kids[i];
                Block case_block = case_node->kids[0]
                                       ? new_block("case")
                                       : default_block;
                if (case_node->kids[0])
                    for (int k = 0; k < case_node->kids[0]->count; k++)
                        LLVMAddCase(
                            switch_inst,
                            const_int(
                                LLVMTypeOf(subject.value),
                                LLVMConstIntGetSExtValue(
                                    eval(case_node->kids[0]->kids[k],
                                         scope, subject.type)
                                        .value)),
                            case_block);
                build_at(case_block);
                emit_block(case_node->kids[1], scope);
                branch_to(end);
            }
        } else {
            Scope inner_scope = {0, scope};
            declare_var(&inner_scope, "$switch", subject.type,
                        addr_of(subject.value));
            Node* subject_node = new_node(
                N_IDENT, "$switch",
                node->line); // strings and objects compare in order
            for (int i = 1; i < node->count; i++) {
                Node* case_node = node->kids[i];
                if (!case_node->kids[0]) continue;
                Block case_block = new_block("case"),
                      next_block = new_block("next");
                Value cond       = LLVMConstInt(S->ir.i1, 0, 0);
                for (int k = 0; k < case_node->kids[0]->count; k++) {
                    Node* eq_node = new_node(N_BIN, "==", node->line);
                    add_kid(eq_node, subject_node);
                    add_kid(eq_node, case_node->kids[0]->kids[k]);
                    cond = LLVMBuildOr(
                        S->ir.build, cond,
                        to_bool(eval(eq_node, &inner_scope, 0)), "");
                }
                LLVMBuildCondBr(S->ir.build, cond, case_block,
                                next_block);
                build_at(case_block);
                emit_block(case_node->kids[1], scope);
                branch_to(end);
                build_at(next_block);
            }
            branch_to(default_block);
            if (default_block != end) {
                build_at(default_block);
                for (int i = 1; i < node->count; i++)
                    if (!node->kids[i]->kids[0])
                        emit_block(node->kids[i]->kids[1], scope);
                branch_to(end);
            }
        }
        build_at(end);
        break;
    }
    case S_TRY: { // Au error frames: setjmp on the frame's env, toss
                  // raises, the message rides the frame
        for (int i = 0; i < 2;
             i++) { // a function that jumps back keeps its locals
                    // honest: no optimization in it
            const char* keep = i ? "noinline" : "optnone";
            LLVMAddAttributeAtIndex(
                S->ir.func, LLVMAttributeFunctionIndex,
                LLVMCreateEnumAttribute(
                    S->ir.context,
                    LLVMGetEnumAttributeKindForName(keep, strlen(keep)),
                    0));
        }
        Value setjmp_fn = fn_named(
            "setjmp", LLVMFunctionType(S->ir.i32, &S->ir.ptr, 1, 0));
        LLVMAddAttributeAtIndex(
            setjmp_fn, LLVMAttributeFunctionIndex,
            LLVMCreateEnumAttribute(
                S->ir.context,
                LLVMGetEnumAttributeKindForName("returns_twice", 13),
                0));
        Value message_slot = stack_slot(S->ir.ptr);
        Value frame        = call_au("au_error_frame_new", "p", 0);
        call_au("au_error_frame_push", "vp", 1, frame);
        Value env       = call_au("au_error_frame_env", "pp", 1, frame);
        Value status    = call_fn(setjmp_fn, 1, &env);
        Block try_block = new_block("try"), after = new_block("after");
        LLVMBuildCondBr(S->ir.build,
                        LLVMBuildICmp(S->ir.build, LLVMIntEQ, status,
                                      const_i32(0), ""),
                        try_block, after);
        build_at(try_block);
        emit_block(node->kids[0], scope);
        branch_to(after);
        build_at(after);
        call_au("au_error_frame_pop", "vp", 1, frame);
        LLVMBuildStore(
            S->ir.build,
            call_au("au_error_frame_message", "pp", 1, frame),
            message_slot);
        if (node->kids[2]) {
            Scope inner_scope   = {0, scope};
            Block catch_block   = new_block("catch"),
                  finally_block = new_block("finally");
            LLVMBuildCondBr(S->ir.build,
                            LLVMBuildICmp(S->ir.build, LLVMIntNE,
                                          status, const_i32(0), ""),
                            catch_block, finally_block);
            build_at(catch_block);
            Value catch_frame = call_au("au_error_frame_new", "p", 0);
            call_au("au_error_frame_push", "vp", 1, catch_frame);
            Value catch_env =
                call_au("au_error_frame_env", "pp", 1, catch_frame);
            Value catch_status = call_fn(setjmp_fn, 1, &catch_env);
            Block catch_body   = new_block("cbody"),
                  catch_fail   = new_block("cfail"),
                  catch_pop    = new_block("cpop");
            LLVMBuildCondBr(S->ir.build,
                            LLVMBuildICmp(S->ir.build, LLVMIntEQ,
                                          catch_status, const_i32(0),
                                          ""),
                            catch_body, catch_fail);
            build_at(catch_body);
            if (node->kids[1]->count) {
                Value message_var = stack_slot(S->ir.ptr);
                LLVMBuildStore(S->ir.build,
                               load(S->ir.ptr, message_slot),
                               message_var);
                declare_var(&inner_scope, node->kids[1]->kids[0]->text,
                            basic(T_STRING), message_var);
            }
            LLVMBuildStore(S->ir.build, LLVMConstNull(S->ir.ptr),
                           message_slot);
            emit_block(node->kids[2], &inner_scope);
            branch_to(catch_pop);
            build_at(catch_fail);
            LLVMBuildStore(
                S->ir.build,
                call_au("au_error_frame_message", "pp", 1, catch_frame),
                message_slot);
            branch_to(catch_pop);
            build_at(catch_pop);
            call_au("au_error_frame_pop", "vp", 1, catch_frame);
            branch_to(finally_block);
            build_at(finally_block);
        }
        if (node->kids[3]) emit_block(node->kids[3], scope);
        Value message       = load(S->ir.ptr, message_slot);
        Block rethrow_block = new_block("rethrow"),
              done          = new_block("done");
        LLVMBuildCondBr(S->ir.build,
                        LLVMBuildICmp(S->ir.build, LLVMIntNE, message,
                                      LLVMConstNull(S->ir.ptr), ""),
                        rethrow_block, done);
        build_at(rethrow_block);
        emit_raise(message);
        LLVMBuildUnreachable(S->ir.build);
        build_at(done);
        break;
    }
    case S_THROW:
        emit_raise(eval_as(node->kids[0], scope, basic(T_STRING)));
        LLVMBuildUnreachable(S->ir.build);
        build_at(new_block("dead"));
        break;
    case S_CHECK: {
        Value cond     = to_bool(eval(node->kids[0], scope, 0));
        Block ok_block = new_block("ok"), bad_block = new_block("bad");
        LLVMBuildCondBr(S->ir.build, cond, ok_block, bad_block);
        build_at(bad_block);
        emit_raise(eval_as(node->kids[1], scope, basic(T_STRING)));
        LLVMBuildUnreachable(S->ir.build);
        build_at(ok_block);
        break;
    }
    case S_EXPECT: {
        Value msg = kid(node, 1)
                        ? eval_as(node->kids[1], scope, basic(T_STRING))
                        : LLVMConstNull(S->ir.ptr);
        Value ok;
        if (node->kids[0]->kind == S_DECL) {
            Type* decl_type;
            decl_stmt(node->kids[0], scope, &decl_type);
            Var* var = find_var(scope, node->kids[0]->text);
            ok       = to_bool(make_val(
                load(llvm_of(decl_type), var->address), decl_type));
        } else ok = to_bool(eval(node->kids[0], scope, 0));
        Block fail_block = new_block("expect_fail"),
              ok_block   = new_block("expect_ok");
        LLVMBuildCondBr(S->ir.build, ok, ok_block, fail_block);
        build_at(fail_block);
        call_au(
            "fault_expect", "vpi", 2,
            LLVMBuildSelect(S->ir.build,
                            LLVMBuildICmp(S->ir.build, LLVMIntNE, msg,
                                          LLVMConstNull(S->ir.ptr), ""),
                            chars_of(msg, 0), const_str("expect"), ""),
            const_i32(node->line));
        LLVMBuildUnreachable(S->ir.build);
        build_at(ok_block);
        break;
    }
    case S_FAULT:
        call_au(
            "fault_expect", "vpi", 2,
            chars_of(eval_as(node->kids[0], scope, basic(T_STRING)), 0),
            const_i32(node->line));
        LLVMBuildUnreachable(S->ir.build);
        build_at(new_block("dead"));
        break;
    case S_PUTS:
        call_au("puts", "ip", 1,
                chars_of(to_string(eval(node->kids[0], scope, 0)), 0));
        break;
    case S_LOG:
        call_au("Au_log", "vpp", 2,
                S->cur_fn->class_type &&
                        S->cur_fn->class_type->kind == T_CLASS
                    ? S->cur_fn->self
                    : LLVMConstNull(S->ir.ptr),
                chars_of(to_string(eval(node->kids[0], scope, 0)), 0));
        break;
    case S_ASM:
        if (host_arch(node->text)) asm_block(node, 0, scope, 0, 0);
        break;
    case S_NOOP: break;
    case S_CONSTRUCT: {
        Val   arg = eval(node->kids[0]->kids[0], scope, 0);
        bool  is_post;
        Node* ctor =
            find_ctor(S->cur_fn->class_type, arg.type, &is_post);
        if (!ctor) emit_fail("no matching construct");
        Value call_args[2] = {
            S->cur_fn->self,
            cast_value(arg, param_type(ctor->kids[0]->kids[0]))};
        call_fn(LLVMGetNamedFunction(
                    S->ir.module,
                    method_cname(S->cur_fn->class_type, ctor)),
                2, call_args);
        break;
    }
    case S_BLOCK: emit_block(node, scope); break;
    default: emit_fail("unsupported statement %d", node->kind);
    }
}
static void emit_block(Node* block, Scope* scope) {
    Scope inner_scope = {0, scope};
    if (S->coverage && S->cur_fn && S->cur_fn->di_scope &&
        S->probe_count <
            4096) { // --coverage: count the block's entries
        Value slot = index_ptr(S->ir.i64, S->cov_probes,
                               const_i64(S->probe_count++));
        LLVMBuildStore(S->ir.build,
                       LLVMBuildAdd(S->ir.build, load(S->ir.i64, slot),
                                    const_i64(1), ""),
                       slot);
        LLVMBuildStore(S->ir.build,
                       LLVMBuildAdd(S->ir.build,
                                    load(S->ir.i64, S->cov_seq),
                                    const_i64(1), ""),
                       S->cov_seq);
    }
    for (int i = 0; i < block->count; i++)
        emit_stmt(block->kids[i], &inner_scope);
}
static Value emit_function(Node* func_node, Type* class_type,
                           Value func, Type* context_type,
                           List* captures, Type** result_out) {
    Fn frame         = {0};
    frame.class_type = class_type;
    frame.result =
        result_out && *result_out ? *result_out : fn_ret(func_node);
    frame.infer_result =
        result_out && !*result_out && !kid(func_node, 1);
    if (frame.infer_result) frame.result = basic(T_NONE);
    Fn* saved_fn      = S->cur_fn;
    S->cur_fn         = &frame;
    Block saved_block = cur_block();
    Value saved_func  = S->ir.func;
    S->ir.func        = func;
    int saved_line    = S->cur_line;
    build_at(new_block("entry"));
    const char* func_name = LLVMGetValueName(func);
    if (strncmp(func_name, "lamprobe",
                8)) { // the DWARF subprogram; a return-type probe is
                      // deleted again and gets none
        LLVMMetadataRef sub_type = LLVMDIBuilderCreateSubroutineType(
            S->di, S->di_file, 0, 0, LLVMDIFlagZero);
        frame.di_scope = LLVMDIBuilderCreateFunction(
            S->di, S->di_unit, func_name, strlen(func_name), func_name,
            strlen(func_name), S->di_file, func_node->line, sub_type,
            true, true, func_node->line, LLVMDIFlagZero, false);
        LLVMSetSubprogram(func, frame.di_scope);
        set_line(func_node->line);
        if (S->timing && S->func_count < 512) {
            frame.timing_id = S->func_count++;
            list_push(&S->func_name_list, strdup(func_name));
            frame.timing_start = clock_ns();
        }
    }
    Scope scope   = {0, 0};
    int   param_i = 0;
    if (frame.result->kind == T_STRUCT && !abi_ret(frame.result))
        frame.result_ptr = LLVMGetParam(func, param_i++);
    bool is_static =
        class_type && func_node->kind == D_FUNC &&
        (member_mods(func_node) &
         1); // a static method: no self, only the static members
    if (class_type && class_type->kind != T_SCALAR) {
        Value self = is_static ? 0 : LLVMGetParam(func, param_i++);
        frame.self = self;
        if (self) {
            Value self_addr =
                class_type->kind == T_STRUCT ? self : addr_of(self);
            declare_var(&scope, "self", class_type, self_addr);
            declare_var(&scope, "a", class_type, self_addr);
        } // a struct self arrives by pointer; `a` names self too
        for (Type* cur = class_type; cur; cur = cur->base)
            if (cur->decl)
                for (int i = 1; i < cur->decl->count; i++) {
                    Node* member = cur->decl->kids[i];
                    if (member->kind != D_MEMBER ||
                        find_var(&scope, member->text) ||
                        (is_static && !(member_mods(member) & 1)))
                        continue;
                    Type* member_type = type_of(member->kids[0]);
                    declare_var(
                        &scope, member->text, member_type,
                        (member_mods(member) & 1)
                            ? global_var(format("%s_%s", cur->name,
                                                member->text),
                                         llvm_of(member_type), false)
                            : field_ptr(class_type->llvm, self,
                                        field_index(class_type,
                                                    member->text)));
                }
    }
    if (context_type) {
        Value context_arg = LLVMGetParam(func, param_i++);
        for (int i = 0; i < captures->count; i++) {
            Var*  var  = captures->data[i];
            Value slot = field_ptr(context_type->llvm, context_arg, i);
            if (same(var->name, "self")) {
                Value self       = load(S->ir.ptr, slot);
                frame.self       = self;
                frame.class_type = var->type;
                declare_var(&scope, "self", var->type, slot);
                for (Type* cur = var->type; cur; cur = cur->base)
                    if (cur->decl)
                        for (int k = 1; k < cur->decl->count; k++) {
                            Node* member = cur->decl->kids[k];
                            if (member->kind != D_MEMBER ||
                                find_var(&scope, member->text))
                                continue;
                            Type* member_type =
                                type_of(member->kids[0]);
                            declare_var(
                                &scope, member->text, member_type,
                                (member_mods(member) & 1)
                                    ? global_var(
                                          format("%s_%s", cur->name,
                                                 member->text),
                                          llvm_of(member_type), false)
                                    : field_ptr(
                                          var->type->llvm, self,
                                          field_index(var->type,
                                                      member->text)));
                        }
            } else declare_var(&scope, var->name, var->type, slot);
        }
    }
    Node* params = func_node->kids[0];
    for (int i = 0; i < params->count; i++) {
        Value param = LLVMGetParam(func, param_i++);
        Type* want  = param_type(params->kids[i]);
        if (params->kids[i]->text)
            declare_var(&scope, params->kids[i]->text, want,
                        class_type && class_type->kind == T_STRUCT &&
                                want->kind == T_STRUCT
                            ? param
                            : addr_of(param));
    }
    if (class_type && class_type->kind == T_SCALAR) {
        declare_var(&scope, "a", class_type,
                    addr_of(LLVMGetParam(func, 0)));
        if (func_node->kind == D_CTOR)
            declare_var(&scope, params->kids[0]->text,
                        param_type(params->kids[0]),
                        addr_of(LLVMGetParam(func, 0)));
    }
    Node* body = kid(func_node, 2);
    if (body) emit_block(body, &scope);
    LType result_llvm = LLVMGetReturnType(LLVMGlobalGetValueType(
        func)); // every open block ends the function; dead blocks after
                // returns included
    for (Block block = LLVMGetFirstBasicBlock(func); block;
         block       = LLVMGetNextBasicBlock(block))
        if (!LLVMGetBasicBlockTerminator(block)) {
            build_at(block);
            timing_end();
            if (result_llvm == S->ir.void_type)
                LLVMBuildRetVoid(S->ir.build);
            else LLVMBuildRet(S->ir.build, LLVMConstNull(result_llvm));
        }
    if (result_out) *result_out = frame.result;
    S->cur_fn  = saved_fn;
    S->ir.func = saved_func;
    if (saved_block) build_at(saved_block);
    set_line(saved_line);
    return func;
}
static void stub_body(
    Value       func,
    const char* message) { // replace a function's body with a fault
    for (Block block = LLVMGetFirstBasicBlock(func); block;) {
        Block next_block = LLVMGetNextBasicBlock(block);
        LLVMDeleteBasicBlock(block);
        block = next_block;
    }
    Value saved_func = S->ir.func;
    S->ir.func       = func;
    build_at(new_block("entry"));
    if (LLVMGetSubprogram(func))
        LLVMSetCurrentDebugLocation2(
            S->ir.build,
            LLVMDIBuilderCreateDebugLocation(
                S->ir.context, 1, 0, LLVMGetSubprogram(func), 0));
    call_au("fault_expect", "vpi", 2, const_str(message), const_i32(0));
    LLVMBuildUnreachable(S->ir.build);
    S->ir.func = saved_func;
}
static bool guarded(
    Node* func_node, Type* class_type, Value func,
    const char* what) { // emit a body; on failure stub it and report
    S->pending.count = 0;
    S->emit_guarded  = true;
    if (setjmp(S->emit_jump) == 0) {
        emit_function(func_node, class_type, func, 0, 0, 0);
        S->emit_guarded = false;
        return true;
    }
    S->emit_guarded = false;
    fprintf(stderr, "%s: %s\n", what, S->emit_message);
    stub_body(func, S->emit_message);
    for (int i = 0; i < S->pending.count; i++)
        stub_body(S->pending.data[i], S->emit_message);
    return false;
}
static int slots_of(Type* type) {
    int count = type->base ? slots_of(type->base)
                           : ((Au_t)au_type("Au"))->table_size / 8;
    if (type->decl)
        for (int i = 1; i < type->decl->count; i++)
            if (type->decl->kids[i]->kind != D_MEMBER) count++;
    return count;
}
static void declare_record(
    Type* type) { // struct/class layouts and every function they own
    Node* decl     = type->decl;
    bool  is_class = type->kind == T_CLASS;
    LType elem_types[128];
    int   elem_count = 0;
    if (is_class) {
        elem_types[elem_count++] = S->ir.ptr;
        elem_types[elem_count++] = LLVMArrayType2(S->ir.i64, 4);
    }
    List chain = {0};
    for (Type* cur = type; cur; cur = cur->base) list_push(&chain, cur);
    for (int k = chain.count - 1; k >= 0; k--)
        for (int pass = 0; pass < 2; pass++) {
            Type* cur = chain.data[k];
            if (!cur->decl)
                continue; // silver's layout: each level's public
                          // members, then its interns
            for (int i = 1; i < cur->decl->count; i++) {
                Node* member = cur->decl->kids[i];
                if (member->kind != D_MEMBER ||
                    (member_mods(member) & 1) ||
                    ((member_mods(member) & 64) != 0) != (pass == 1))
                    continue;
                bool is_dup = false;
                for (int j = 0; j < type->field_names.count; j++)
                    if (same(type->field_names.data[j], member->text))
                        is_dup = true;
                if (is_dup) continue;
                Type* member_type = type_of(member->kids[0]);
                list_push(&type->field_names, member->text);
                list_push(&type->field_types, member_type);
                elem_types[elem_count++] = llvm_of(member_type);
            }
        }
    LLVMStructSetBody(type->llvm, elem_types, elem_count, 0);
    type_blob(type->name, slots_of(type) + 4);
    for (int i = 1; i < decl->count; i++) {
        Node* member = decl->kids[i];
        if (member->kind == D_MEMBER) {
            if (member_mods(member) & 1)
                global_var(format("%s_%s", type->name, member->text),
                           llvm_of(type_of(member->kids[0])), false);
            continue;
        }
        declare_function(
            member,
            (member->kind == D_FUNC && (member_mods(member) & 1))
                ? 0
                : type,
            method_cname(type, member), 0, 0);
    }
    if (is_class)
        fn_named(format("%s_defaults", type->name),
                 LLVMFunctionType(S->ir.void_type, &S->ir.ptr, 1, 0));
}
static void register_record(
    Type* type) { // the Au type record: emplace, function table slots,
                  // members with offsets, methods with argument types
    Node*          decl        = type->decl;
    bool           is_class    = type->kind == T_CLASS;
    LLVMBuilderRef saved_build = S->ir.build;
    S->ir.build                = S->ir.init_build;
    Value record_val           = type_record(type);
    u64   traits               = is_class ? AU_TRAIT_CLASS
                                 : type->kind == T_STRUCT ? AU_TRAIT_STRUCT
                                 : type->kind == T_ENUM
                                     ? AU_TRAIT_ENUM
                                     : AU_TRAIT_SCALAR | AU_TRAIT_PRIMITIVE;
    if (type->decl->flag & 2) traits |= AU_TRAIT_ABSTRACT;
    Value base_rec   = is_class
                           ? (type->base ? type_record(type->base)
                                         : type_record(basic(T_OBJECT)))
                           : LLVMConstNull(S->ir.ptr);
    Value source_rec = type->kind == T_ENUM || type->kind == T_SCALAR
                           ? type_record(type->base)
                           : LLVMConstNull(S->ir.ptr);
    call_au("emplace_type", "ppppppilllipi", 12, record_val, base_rec,
            source_rec, S->modrec, const_str(type->name),
            const_i32(AU_MEMBER_TYPE), const_i64(traits),
            is_class || type->kind == T_STRUCT
                ? LLVMSizeOf(type->llvm)
                : LLVMSizeOf(llvm_of(type)),
            const_i64(0), const_i32(0), const_str(S->cur_file),
            const_i32(decl->line));
    int   next_slot = type->base ? slots_of(type->base)
                                 : ((Au_t)au_type("Au"))->table_size / 8;
    Value size_slot =
        byte_ptr(record_val, offsetof(struct _Au_t, table_size));
    for (int i = 1; i < decl->count; i++) {
        Node* member = decl->kids[i];
        int   mods   = member_mods(member);
        if (member->kind == D_MEMBER) {
            if (mods & 1) continue;
            Type* member_type = type_of(member->kids[0]);
            if (type->kind == T_SCALAR) continue;
            u64 member_traits = (mods & 2 ? AU_TRAIT_IS_CONTEXT : 0) |
                                (mods & 4 ? AU_TRAIT_UNMANAGED : 0) |
                                (mods & 8 ? AU_TRAIT_IS_DEFAULT : 0) |
                                (mods & 16 ? AU_TRAIT_IS_ATTRIB : 0) |
                                (mods & 64 ? AU_TRAIT_IS_HIDDEN : 0);
            Value value = 0;
            if (mods & 16 && kid(member, 1)) {
                Scope scope  = {0};
                Fn    frame  = {0};
                frame.result = basic(T_NONE);
                S->cur_fn    = &frame;
                S->ir.func   = LLVMGetBasicBlockParent(
                    LLVMGetInsertBlock(S->ir.init_build));
                value = hold_value(
                    construct(member_type, member->kids[1], &scope)
                        .value);
                S->cur_fn = 0;
            }
            def_var(record_val, member->text, member_type,
                    (offset_of(type->llvm,
                               field_index(type, member->text))),
                    member_traits, value,
                    member->raw
                        ? new_node(N_TYPE, member->raw, member->line)
                        : 0);
            continue;
        }
        if (member->kind == D_ENUMV) {
            Value global =
                global_var(format("%s_%s", type->name, member->text),
                           llvm_of(type), false);
            LLVMSetInitializer(
                global,
                backing(type)->kind == T_FLOAT
                    ? LLVMConstReal(llvm_of(type), atof(member->raw))
                    : const_int(llvm_of(type), atoll(member->raw)));
            Value em = call_au("def_enum_value", "pppp", 3, record_val,
                               const_str(member->text), global);
            LLVMBuildStore(S->ir.build, type_record(type->base),
                           byte_ptr(em, offsetof(struct _Au_t, type)));
            continue;
        }
        if (type->kind == T_SCALAR) continue;
        Value func_val = LLVMGetNamedFunction(
            S->ir.module, method_cname(type, member));
        Type* result_type = fn_ret(member);
        Node* params      = member->kids[0];
        Type* arg_types[34];
        int   arg_count = 0;
        bool  is_static = member->kind == D_FUNC && (mods & 1);
        if (!is_static) arg_types[arg_count++] = type;
        for (int k = 0; k < params->count; k++)
            arg_types[arg_count++] = param_type(params->kids[k]);
        int member_kind  = member->kind == D_CTOR ? AU_MEMBER_CONSTRUCT
                           : member->kind == D_CASTFN ? AU_MEMBER_CAST
                           : member->kind == D_OPFN ? AU_MEMBER_OPERATOR
                           : member->kind == D_GETTER ? AU_MEMBER_GETTER
                           : member->kind == D_SETTER ? AU_MEMBER_SETTER
                                                      : AU_MEMBER_FUNC;
        const char* name = member_name(member);
        const char* arg_names[34];
        int         name_count = 0;
        if (!is_static) arg_names[name_count++] = "a";
        for (int k = 0; k < params->count; k++)
            arg_names[name_count++] = params->kids[k]->text;
        static const char* op_names[] = {"undefined",
                                         "lmul",
                                         "ldiv",
                                         "lright",
                                         "lleft",
                                         "add",
                                         "sub",
                                         "mul",
                                         "div",
                                         "or",
                                         "and",
                                         "bitwise_or",
                                         "bitwise_and",
                                         "xor",
                                         "mod",
                                         "right",
                                         "left",
                                         "compare",
                                         "equal",
                                         "not_equal",
                                         "greater",
                                         "less",
                                         "greater_eq",
                                         "less_eq",
                                         "is",
                                         "inherits",
                                         "range_exclusive",
                                         "range_inclusive",
                                         "value_default",
                                         "cond_value",
                                         "bind",
                                         "assign",
                                         "assign_add",
                                         "assign_sub",
                                         "assign_mul",
                                         "assign_div",
                                         "assign_or",
                                         "assign_and",
                                         "assign_xor",
                                         "assign_mod",
                                         "assign_right",
                                         "assign_left",
                                         0};
        int                op_type    = 0;
        if (member->kind == D_OPFN)
            for (int k = 0; op_names[k]; k++)
                if (same(op_names[k], name + 1)) op_type = k;
        int  slot_at       = -1;
        Au_t au_member_rec = au_member(au_type("Au"), name, 0);
        if (au_member_rec && au_member_rec->member_index)
            slot_at =
                au_member_rec
                    ->member_index; // silver's table: overrides reuse
                                    // the slot, statics take none, the
                                    // rest append in schema order
        for (Type* base = type->base; base && slot_at < 0;
             base       = base->base)
            for (int q = 0; q < S->ft_slots.count; q += 2)
                if (same(S->ft_slots.data[q],
                               format("%s_%s", base->name, name)))
                    slot_at = (int)(long)S->ft_slots.data[q + 1];
        if (slot_at < 0 && !is_static) {
            slot_at = next_slot++;
            LLVMBuildStore(S->ir.build, const_i32(8 * next_slot),
                           size_slot);
        }
        if (slot_at >= 0) {
            list_push(&S->ft_slots, method_cname(type, member));
            list_push(&S->ft_slots, (void*)(long)slot_at);
            LLVMBuildStore(
                S->ir.build, func_val,
                byte_ptr(record_val,
                         offsetof(struct _Au_t, ft) + 8 * slot_at));
        }
        def_fn(record_val, name,
               member->kind == D_CTOR ? type : result_type, member_kind,
               is_static ? AU_TRAIT_SMETHOD : AU_TRAIT_IMETHOD,
               func_val, method_cname(type, member),
               slot_at < 0 ? 0 : slot_at, arg_count, arg_types,
               arg_names, op_type,
               member->raw ? new_node(N_TYPE, member->raw, member->line)
                           : 0);
    }
    call_au("push_type", "vpp", 2, record_val, S->modrec);
    S->ir.build = saved_build;
}
static void emit_record(Type* type) {
    Node* decl     = type->decl;
    bool  is_class = type->kind == T_CLASS;
    if (is_class) { // defaults: member initialisers, run right after
                    // alloc and before Au_initialize
        Value defaults_fn = LLVMGetNamedFunction(
            S->ir.module, format("%s_defaults", type->name));
        Fn frame         = {0};
        frame.class_type = type;
        frame.result     = basic(T_NONE);
        Fn* saved_fn     = S->cur_fn;
        S->cur_fn        = &frame;
        S->ir.func       = defaults_fn;
        build_at(new_block("entry"));
        Value self  = LLVMGetParam(defaults_fn, 0);
        frame.self  = self;
        Scope scope = {0};
        declare_var(&scope, "self", type, addr_of(self));
        if (type->base)
            call_au(format("%s_defaults", type->base->name), "vp", 1,
                    self);
        for (int i = 1; i < decl->count; i++) {
            Node* member = decl->kids[i];
            if (member->kind != D_MEMBER || (member_mods(member) & 1) ||
                (member_mods(member) & 16))
                continue;
            Type* member_type = type_of(member->kids[0]);
            Value slot        = field_ptr(type->llvm, self,
                                          field_index(type, member->text));
            if (kid(member, 1)) {
                Value value =
                    construct(member_type, member->kids[1], &scope)
                        .value;
                LLVMBuildStore(S->ir.build, value, slot);
                if (member_type->kind == T_CLASS) {
                    Value header = header_of(value);
                    LLVMBuildStore(
                        S->ir.build, const_str(member->text),
                        byte_ptr(header,
                                 offsetof(struct _object, bind)));
                    LLVMBuildStore(
                        S->ir.build, type_record(type),
                        byte_ptr(header,
                                 offsetof(struct _object, holder)));
                }
            } else if (member_type->kind == T_VEC && member_type->dims)
                LLVMBuildStore(S->ir.build,
                               vec_new(member_type, 0, &scope), slot);
        }
        LLVMBuildRetVoid(S->ir.build);
        S->cur_fn = saved_fn;
    }
    for (int i = 1; i < decl->count; i++) {
        Node* member = decl->kids[i];
        if (member->kind == D_MEMBER || !kid(member, 2)) continue;
        guarded(member, type,
                LLVMGetNamedFunction(S->ir.module,
                                     method_cname(type, member)),
                format("%s.%s", type->name, member->text));
    }
    register_record(type);
}
static void emit_scalar(Type* type) {
    Node* decl = type->decl;
    for (int i = 1; i < decl->count; i++) {
        Node* member = decl->kids[i];
        if (member->kind != D_CASTFN && member->kind != D_CTOR)
            continue;
        Type* other = member->kind == D_CASTFN
                          ? type_of(member->kids[1])
                          : param_type(member->kids[0]->kids[0]);
        LType param_llvm =
            llvm_of(member->kind == D_CASTFN ? type : other);
        Value func = fn_named(
            member->kind == D_CASTFN
                ? format("%s_cast_%s", type->name, other->name)
                : format("%s_with_%s", type->name, other->name),
            LLVMFunctionType(
                llvm_of(member->kind == D_CASTFN ? other : type),
                &param_llvm, 1, 0));
        Type* result_type = member->kind == D_CASTFN ? other : type;
        Node* func_node = new_node(D_FUNC, member->text, member->line);
        add_kid(func_node, member->kind == D_CASTFN
                               ? new_node(N_ARGS, 0, member->line)
                               : member->kids[0]);
        add_kid(func_node, 0);
        add_kid(func_node, member->kids[2]);
        S->pending.count = 0;
        S->emit_guarded  = true;
        if (setjmp(S->emit_jump) == 0) {
            Type* result_ref = result_type;
            emit_function(func_node, type, func, 0, 0, &result_ref);
            S->emit_guarded = false;
        } else {
            S->emit_guarded = false;
            fprintf(stderr, "%s: %s\n", type->name, S->emit_message);
            stub_body(func, S->emit_message);
        }
    }
    register_record(type);
}
static void register_types(Node* module_node) {
    for (int i = 0; i < module_node->count; i++) {
        Node* decl = module_node->kids[i];
        Type* type = 0;
        if (decl->kind == D_CLASS) type = new_type(T_CLASS, decl->text);
        else if (decl->kind == D_STRUCT)
            type = new_type(T_STRUCT, decl->text);
        else if (decl->kind == D_ENUM)
            type = new_type(T_ENUM, decl->text);
        else if (decl->kind == D_SCALAR)
            type = new_type(T_SCALAR, decl->text);
        else if (decl->kind == D_ALIAS) {
            type       = new_type(T_UNK, decl->text);
            type->decl = decl->kids[0];
            list_push(&S->types, type);
            continue;
        }
        if (!type) continue;
        type->decl = decl;
        list_push(&S->types, type);
        if (type->kind == T_CLASS || type->kind == T_STRUCT)
            type->llvm =
                LLVMStructCreateNamed(S->ir.context, type->name);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind == T_CLASS && type->decl->kids[0])
            type->base = type_named(type->decl->kids[0]->text);
        if ((type->kind == T_ENUM || type->kind == T_SCALAR))
            type->base = type->decl->kids[0]
                             ? type_of(type->decl->kids[0])
                             : type_named("i32");
        if (type->kind == T_ENUM || type->kind == T_SCALAR)
            type_blob(type->name, 0);
    }
}
static void module_init(void) {
    S->ir.context = LLVMContextCreate();
    S->ir.module =
        LLVMModuleCreateWithNameInContext(S->modname, S->ir.context);
    S->ir.build      = LLVMCreateBuilderInContext(S->ir.context);
    S->ir.init_build = LLVMCreateBuilderInContext(S->ir.context);
    S->ir.i1         = LLVMInt1TypeInContext(S->ir.context);
    S->ir.i8         = LLVMInt8TypeInContext(S->ir.context);
    S->ir.i32        = LLVMInt32TypeInContext(S->ir.context);
    S->ir.i64        = LLVMInt64TypeInContext(S->ir.context);
    S->ir.f32        = LLVMFloatTypeInContext(S->ir.context);
    S->ir.f64        = LLVMDoubleTypeInContext(S->ir.context);
    S->ir.bf16       = LLVMBFloatTypeInContext(S->ir.context);
    S->ir.ptr        = LLVMPointerTypeInContext(S->ir.context, 0);
    S->ir.void_type  = LLVMVoidTypeInContext(S->ir.context);
    S->target_data   = LLVMCreateTargetData(
        "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:"
          "128-n8:16:32:64-S128");
    LLVMSetModuleDataLayout(S->ir.module, S->target_data);
    S->di = LLVMCreateDIBuilder(
        S->ir.module); // DWARF: lldb steps the .ag source
    char*       dir   = strdup(S->cur_file);
    char*       slash = strrchr(dir, '/');
    const char* file  = slash ? slash + 1 : dir;
    if (slash) *slash = 0;
    else dir = ".";
    S->di_file = LLVMDIBuilderCreateFile(S->di, file, strlen(file),
                                         realpath(dir, 0),
                                         strlen(realpath(dir, 0)));
    S->di_unit = LLVMDIBuilderCreateCompileUnit(
        S->di, LLVMDWARFSourceLanguageC11, S->di_file, "silver2", 7,
        false, "", 0, 0, "", 0, LLVMDWARFEmissionFull, 0, false, false,
        "", 0, "", 0);
    LLVMAddModuleFlag(S->ir.module, LLVMModuleFlagBehaviorWarning,
                      "Debug Info Version", 18,
                      LLVMValueAsMetadata(const_int(S->ir.i32, 3)));
    LLVMAddModuleFlag(S->ir.module, LLVMModuleFlagBehaviorWarning,
                      "Dwarf Version", 13,
                      LLVMValueAsMetadata(const_int(S->ir.i32, 5)));
    if (S->coverage) { // --coverage: one counter per block, plus a
                       // sequence lldb can watch
        LType probes = LLVMArrayType2(S->ir.i64, 4096);
        S->cov_probes =
            LLVMAddGlobal(S->ir.module, probes,
                          format("__cov_probes_%s", S->modname));
        LLVMSetInitializer(S->cov_probes, LLVMConstNull(probes));
        LLVMSetLinkage(S->cov_probes, LLVMInternalLinkage);
        S->cov_seq =
            LLVMAddGlobal(S->ir.module, S->ir.i64, "__cov_seq");
        LLVMSetInitializer(S->cov_seq, const_i64(0));
    }
    if (S->timing) { // --timing: nanoseconds per function, named at the
                     // end
        LType totals = LLVMArrayType2(S->ir.i64, 512),
              names  = LLVMArrayType2(S->ir.ptr, 512);
        S->timings =
            LLVMAddGlobal(S->ir.module, totals,
                          format("__func_timings_%s", S->modname));
        LLVMSetInitializer(S->timings, LLVMConstNull(totals));
        S->func_names = LLVMAddGlobal(
            S->ir.module, names, format("__func_names_%s", S->modname));
        LLVMSetInitializer(S->func_names, LLVMConstNull(names));
        LLVMSetLinkage(S->func_names, LLVMInternalLinkage);
    }
}
static void emit_module(Node* module_node, const char* source_dir) {
    S->MOD = module_node;
    module_init();
    register_types(module_node);
    Value init_fn =
        fn_named(format("silver_%s_initializer", S->modname),
                 LLVMFunctionType(S->ir.void_type, 0, 0, 0));
    LLVMPositionBuilderAtEnd(
        S->ir.init_build,
        LLVMAppendBasicBlockInContext(
            S->ir.context, init_fn,
            "entry")); // silver calls this by name; importers call it
                       // from their own initializer
    {
        Value once_flag =
            LLVMAddGlobal(S->ir.module, S->ir.i1, "s2_init_once");
        LLVMSetInitializer(once_flag, LLVMConstInt(S->ir.i1, 0, 0));
        LLVMSetLinkage(once_flag, LLVMInternalLinkage);
        Block go_block   = LLVMAppendBasicBlockInContext(S->ir.context,
                                                         init_fn, "go"),
              done_block = LLVMAppendBasicBlockInContext(
                  S->ir.context, init_fn, "done");
        LLVMBuildCondBr(
            S->ir.init_build,
            LLVMBuildLoad2(S->ir.init_build, S->ir.i1, once_flag, ""),
            done_block, go_block);
        LLVMPositionBuilderAtEnd(S->ir.init_build, done_block);
        LLVMBuildRetVoid(S->ir.init_build);
        LLVMPositionBuilderAtEnd(S->ir.init_build, go_block);
        LLVMBuildStore(S->ir.init_build, LLVMConstInt(S->ir.i1, 1, 0),
                       once_flag);
        for (int i = 0; i < S->imported_names.count; i++)
            LLVMBuildCall2(
                S->ir.init_build,
                LLVMFunctionType(S->ir.void_type, 0, 0, 0),
                fn_named(format("silver_%s_initializer",
                                S->imported_names.data[i]),
                         LLVMFunctionType(S->ir.void_type, 0, 0, 0)),
                0, 0, "");
    }
    {
        LLVMBuilderRef saved_build = S->ir.build;
        S->ir.build                = S->ir.init_build;
        S->ir.func                 = init_fn;
        S->modrec                  = call_au("module_lookup", "pp", 1,
                                             const_str(format("silver-%s", S->modname)));
        LLVMBuildStore(S->ir.build, S->modrec,
                       global_var("s2_module", S->ir.ptr, false));
        S->ir.build = saved_build;
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind == T_STRUCT || type->kind == T_CLASS)
            declare_record(type);
    }
    for (int i = 0; i < module_node->count; i++) {
        Node* decl = module_node->kids[i];
        if (decl->kind == D_FUNC)
            declare_function(decl, 0, decl->text, 0, 0);
        if (decl->kind == D_VAR)
            global_var(decl->text, llvm_of(type_of(decl->kids[0])),
                       false);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind == T_ENUM) register_record(type);
        if (type->kind == T_SCALAR) emit_scalar(type);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind == T_STRUCT) emit_record(type);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind == T_CLASS) emit_record(type);
    }
    {
        LLVMBuilderRef saved_build = S->ir.build;
        S->ir.build                = S->ir.init_build;
        S->ir.func                 = init_fn;
        Scope global_scope         = {0};
        Fn    global_frame         = {0};
        global_frame.result        = basic(T_NONE);
        S->cur_fn =
            &global_frame; // globals and config imports initialise here
        for (int i = 0; i < module_node->count; i++) {
            Node* decl = module_node->kids[i];
            if (decl->kind == D_VAR && kid(decl, 1)) {
                Type* type = type_of(decl->kids[0]);
                Value value =
                    construct(type, decl->kids[1], &global_scope).value;
                if (is_obj(type)) hold_value(value);
                LLVMBuildStore(
                    S->ir.build, value,
                    global_var(decl->text, llvm_of(type), false));
            }
        }
        for (int i = 0; i < module_node->count; i++) {
            Node* decl = module_node->kids[i];
            if (decl->kind == D_IMPORT)
                for (int k = 0; k < decl->count; k++)
                    if (decl->kids[k]->kind ==
                        N_PROP) { // config: an imported module's own
                                  // members take the importer's values
                        Au_t imported_var = imported_member(
                            decl->kids[k]->text, AU_MEMBER_VAR);
                        if (!imported_var) continue;
                        Type* global_type = from_au(imported_var->type);
                        Value value =
                            eval_as(decl->kids[k]->kids[1],
                                    &global_scope, global_type);
                        if (is_obj(global_type)) hold_value(value);
                        LLVMBuildStore(S->ir.build, value,
                                       global_var(decl->kids[k]->text,
                                                  llvm_of(global_type),
                                                  true));
                    }
        }
        for (int i = 0; i < module_node->count; i++) {
            Node* decl =
                module_node
                    ->kids[i]; // the module record lists its functions
                               // and variables for importers
            if (decl->kind == D_FUNC && kid(decl, 2)) {
                Node*       params = decl->kids[0];
                Type*       arg_types[34];
                const char* arg_names[34];
                int         arg_count = 0;
                for (int k = 0; k < params->count; k++) {
                    arg_names[arg_count] = params->kids[k]->text;
                    arg_types[arg_count++] =
                        param_type(params->kids[k]);
                }
                def_fn(S->modrec, decl->text, fn_ret(decl),
                       AU_MEMBER_FUNC, AU_TRAIT_SMETHOD,
                       LLVMGetNamedFunction(S->ir.module, decl->text),
                       decl->text, 0, arg_count, arg_types, arg_names,
                       0, 0);
            }
            if (decl->kind == D_VAR)
                def_var(S->modrec, decl->text, type_of(decl->kids[0]),
                        0, 0, 0, 0);
        }
        S->cur_fn   = 0;
        S->ir.build = saved_build;
    }
    for (int i = 0; i < module_node->count; i++) {
        Node* decl = module_node->kids[i];
        if (decl->kind == D_BROKEN && decl->flag) {
            list_push(&S->test_names, decl->text);
            list_push(&S->test_skips, format("parse: %s", decl->error));
            continue;
        }
        if (decl->kind != D_FUNC || !kid(decl, 2)) continue;
        bool emitted = guarded(
            decl, 0, LLVMGetNamedFunction(S->ir.module, decl->text),
            decl->text);
        if (decl->flag & 1) {
            list_push(&S->test_names, decl->text);
            list_push(&S->test_skips,
                      emitted ? 0
                              : format("emit: %s", S->emit_message));
        }
    }
    if (S->coverage || S->timing) {
        LLVMBuilderRef saved_build = S->ir.build;
        S->ir.build                = S->ir.init_build;
        if (S->timing) {
            Value names[512];
            for (int i = 0; i < 512; i++)
                names[i] = i < S->func_name_list.count
                               ? const_str(S->func_name_list.data[i])
                               : LLVMConstNull(S->ir.ptr);
            LLVMSetInitializer(S->func_names,
                               LLVMConstArray2(S->ir.ptr, names, 512));
        }
        call_au(
            "__coverage_register", "vpipip", 5,
            S->cov_probes ? S->cov_probes : LLVMConstNull(S->ir.ptr),
            const_i32(S->probe_count),
            S->timings ? S->timings : LLVMConstNull(S->ir.ptr),
            const_i32(S->timing ? 512 : 0),
            S->func_names ? S->func_names : LLVMConstNull(S->ir.ptr));
        S->ir.build = saved_build;
    }
    LLVMBuildRetVoid(S->ir.init_build);
    if (S->lib_mode) {
        LLVMDIBuilderFinalize(S->di);
        char* verify_error = 0;
        LLVMPrintModuleToFile(
            S->ir.module,
            format("%s/.silver2/%s.ll", source_dir, S->modname), 0);
        if (LLVMVerifyModule(S->ir.module, LLVMPrintMessageAction,
                             &verify_error)) {
            fprintf(stderr, "module verification failed\n");
            exit(1);
        }
        return;
    }
    Value exports_fn = fn_named(
        "s2_exports", LLVMFunctionType(S->ir.void_type, 0, 0, 0));
    LLVMSetLinkage(exports_fn, LLVMInternalLinkage);
    S->ir.func = exports_fn;
    build_at(new_block("entry"));
    for (int i = 0; i < module_node->count; i++)
        if (module_node->kids[i]->kind == D_FUNC &&
            (module_node->kids[i]->flag & 4))
            call_au(module_node->kids[i]->text, "v", 0);
    LLVMBuildRetVoid(S->ir.build);
    Value app_fn = 0;
    for (int i = 0; i < module_node->count; i++)
        if (module_node->kids[i]->kind == D_APP)
            for (int k = 1; k < module_node->kids[i]->count; k++)
                if (module_node->kids[i]->kids[k]->kind == D_FUNC &&
                    same(module_node->kids[i]->kids[k]->text, "init")) {
                    app_fn = fn_named(
                        "s2_app_init",
                        LLVMFunctionType(S->ir.void_type, 0, 0, 0));
                    LLVMSetLinkage(app_fn, LLVMInternalLinkage);
                    guarded(module_node->kids[i]->kids[k], 0, app_fn,
                            "app.init");
                }
    LType fault_params[2] = {S->ir.ptr, S->ir.i32};
    Value fault =
        fn_named( // a failed expect prints and halts the product
            "fault_expect",
            LLVMFunctionType(S->ir.void_type, fault_params, 2, 0));
    LLVMSetLinkage(fault, LLVMInternalLinkage);
    S->ir.func = fault;
    build_at(new_block("entry"));
    call_au("puts", "ip", 1, LLVMGetParam(fault, 0));
    call_au("fflush", "ip", 1, LLVMConstNull(S->ir.ptr)); // _exit
    call_au("_exit", "vi", 1, const_i32(1));              // skips it
    LLVMBuildUnreachable(S->ir.build);
    if (S->cross && target_is_mobile()) { // a phone's host loads the
                                          // product and drives it: init
                                          // once, then frames until 0
        Value live_init =
            fn_named("silver_live_init",
                     LLVMFunctionType(S->ir.void_type, 0, 0, 0));
        S->ir.func = live_init;
        build_at(new_block("entry"));
        LLVMBuildCall2(S->ir.build,
                       LLVMFunctionType(S->ir.void_type, 0, 0, 0),
                       init_fn, 0, 0, "");
        if (app_fn)
            LLVMBuildCall2(S->ir.build,
                           LLVMFunctionType(S->ir.void_type, 0, 0, 0),
                           app_fn, 0, 0, "");
        LLVMBuildRetVoid(S->ir.build);
        Value live_frame = fn_named(
            "silver_live_frame", LLVMFunctionType(S->ir.i32, 0, 0, 0));
        S->ir.func = live_frame;
        build_at(new_block("entry"));
        LLVMBuildRet(S->ir.build,
                     const_i32(0)); // no frame loop yet: the app's init
                                    // is the whole program
        LLVMDIBuilderFinalize(S->di);
        char* verify_error = 0;
        LLVMPrintModuleToFile(
            S->ir.module,
            format("%s/.silver2/%s.ll", source_dir, S->modname), 0);
        if (LLVMVerifyModule(S->ir.module, LLVMPrintMessageAction,
                             &verify_error)) {
            fprintf(stderr, "module verification failed\n");
            exit(1);
        }
        return; // no main: the host is the program
    }
    LType main_params[2] = {S->ir.i32, S->ir.ptr};
    Value main_fn =
        LLVMAddFunction(S->ir.module, "main",
                        LLVMFunctionType(S->ir.i32, main_params, 2, 0));
    S->ir.func = main_fn;
    build_at(new_block("entry"));
    LLVMBuildCall2(S->ir.build,
                   LLVMFunctionType(S->ir.void_type, 0, 0, 0), init_fn,
                   0, 0, "");
    Block expect_run = new_block("expect.run");
    Block expect_go  = new_block("expect.go");
    LLVMBuildCondBr(S->ir.build, env_is_set("SILVER_EXPECT"),
                    expect_run, expect_go);
    build_at(expect_run); // every test returns true or the fault halts
    int ran = 0, skipped = 0;
    for (int i = 0; i < S->test_names.count; i++) {
        const char* name = S->test_names.data[i];
        if (S->test_skips.data[i]) {
            call_au("puts", "ip", 1,
                    const_str(format("[%s] expect: %s skipped (%s)",
                                     S->modname, name,
                                     S->test_skips.data[i])));
            skipped++;
            continue;
        }
        Value test = LLVMGetNamedFunction(S->ir.module, name);
        Value ok   = LLVMBuildCall2(
            S->ir.build, LLVMGlobalGetValueType(test), test, 0, 0, "");
        if (LLVMGetIntTypeWidth(LLVMTypeOf(ok)) != 1)
            ok = LLVMBuildICmp(S->ir.build, LLVMIntNE, ok,
                               LLVMConstNull(LLVMTypeOf(ok)), "");
        Block failed = new_block("expect.fail");
        Block next   = new_block("expect.next");
        LLVMBuildCondBr(S->ir.build, ok, next, failed);
        build_at(failed);
        call_au("fault_expect", "vpi", 2,
                const_str(
                    format("[%s] expect: %s failed", S->modname, name)),
                const_i32(0));
        LLVMBuildUnreachable(S->ir.build);
        build_at(next);
        ran++;
    }
    call_au(
        "puts", "ip", 1,
        const_str(skipped
                      ? format("[%s] expect: %d/%d passed, %d skipped",
                               S->modname, ran, ran, skipped)
                      : format("[%s] expect: %d/%d passed (100%%)",
                               S->modname, ran, ran)));
    call_au("exit", "vi", 1, const_i32(0));
    LLVMBuildUnreachable(S->ir.build);
    build_at(expect_go);
    Block export_run = new_block("export.run");
    Block export_go  = new_block("export.go");
    LLVMBuildCondBr(S->ir.build, env_is_set("SILVER_EXPORT"),
                    export_run, export_go);
    build_at(export_run);
    LLVMBuildCall2(S->ir.build,
                   LLVMFunctionType(S->ir.void_type, 0, 0, 0),
                   exports_fn, 0, 0, "");
    call_au("puts", "ip", 1,
            const_str(format("[%s] export: complete", S->modname)));
    call_au("exit", "vi", 1, const_i32(0));
    LLVMBuildUnreachable(S->ir.build);
    build_at(export_go);
    call_au("engage", "vp", 1, LLVMGetParam(main_fn, 1));
    if (app_fn)
        LLVMBuildCall2(S->ir.build,
                       LLVMFunctionType(S->ir.void_type, 0, 0, 0),
                       app_fn, 0, 0, "");
    LLVMBuildRet(S->ir.build, const_i32(0));
    LLVMDIBuilderFinalize(S->di);
    char* verify_error = 0;
    LLVMPrintModuleToFile(
        S->ir.module,
        format("%s/.silver2/%s.ll", source_dir, S->modname), 0);
    if (LLVMVerifyModule(S->ir.module, LLVMPrintMessageAction,
                         &verify_error)) {
        fprintf(stderr, "module verification failed\n");
        exit(1);
    }
}

// ----------------------------------------------------------------
// driver
static int run_shell(const char* command) {
    fprintf(stderr, "> %s\n", command);
    return system(command);
}
static long mtime_of(const char* file) {
    struct stat st;
    return stat(file, &st) ? 0 : st.st_mtime;
}
static bool is_source_name(const char* name) { // a module's own files
    static const char* exts[] = {".ag", ".c", ".cc",  ".m", ".mm",
                                 ".rs", ".h", ".hpp", 0};
    const char*        dot    = strrchr(name, '.');
    for (int i = 0; dot && exts[i]; i++)
        if (same(dot, exts[i])) return true;
    return false;
}
static long newest_source(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    long           newest = 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' || !is_source_name(e->d_name)) continue;
        long m = mtime_of(format("%s/%s", dir, e->d_name));
        if (m > newest) newest = m;
    }
    closedir(d);
    return newest;
}
static void link_tree(const char* src,
                      const char* dst) { // dirs are made, files are
                                         // linked by absolute path
    DIR* d = opendir(src);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char*       from = format("%s/%s", src, e->d_name);
        char*       to   = format("%s/%s", dst, e->d_name);
        struct stat st;
        if (stat(from, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            mkdir(to, 0755);
            link_tree(from, to);
        } else {
            unlink(to);
            symlink(realpath(from, 0), to);
        }
    }
    closedir(d);
}
static void prune_links(const char* dir) { // a link whose file is gone
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char*       at = format("%s/%s", dir, e->d_name);
        struct stat ls, ts;
        if (lstat(at, &ls)) continue;
        if (S_ISLNK(ls.st_mode)) {
            if (stat(at, &ts)) unlink(at);
        } else if (S_ISDIR(ls.st_mode)) prune_links(at);
    }
    closedir(d);
}
static void
deploy_share(const char* name, const char* module_dir,
             const char* install_dir) { // every folder of the module
                                        // becomes share/<name>/<folder>
    char* share = format("%s/share/%s", install_dir, name);
    mkdir(format("%s/share", install_dir), 0755);
    mkdir(share, 0755);
    DIR* d = opendir(module_dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char*       from = format("%s/%s", module_dir, e->d_name);
        char*       dst  = format("%s/%s", share, e->d_name);
        struct stat st, ls;
        if (stat(from, &st) || !S_ISDIR(st.st_mode)) continue;
        if (!lstat(dst, &ls) && S_ISLNK(ls.st_mode))
            unlink(dst); // a stale directory link goes before the
                         // real dir is made
        mkdir(dst, 0755);
        link_tree(from, dst);
    }
    closedir(d);
    prune_links(share);
}
static bool platform_name(const char* name) {
    return same(name, "linux") || same(name, "x86_64");
}
static void c_include(const char* header) {
    bool is_cxx = strstr(header, ".hpp") != 0;
    append(&S->c_includes,
           is_cxx ? "#include <%s>\n"
                  : "extern \"C\" {\n#include <%s>\n}\n",
           header);
}
static bool au_module_import(
    const char* name) { // an Au-internal module (vec): its library
                        // registers the record when it loads
    for (int i = 0; i < S->au_mods.count; i++)
        if (same(S->au_mods.data[i], name)) return true;
    char* exe_dir          = realpath("/proc/self/exe", 0);
    *strrchr(exe_dir, '/') = 0;
    char* lib_path = format("%s/../lib/lib%s.so", exe_dir, name);
    if (access(lib_path, R_OK)) return false;
    if (!dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "import %s: %s\n", name, dlerror());
        exit(1);
    }
    Au_t (*lookup_module)(const char*) =
        dlsym(S->au_lib, "module_lookup");
    Au_t rec = lookup_module(name);
    if (!rec) {
        fprintf(stderr, "import %s: no module record\n", name);
        exit(1);
    }
    list_push(&S->imported, rec);
    list_push(&S->au_mods, (void*)name);
    append(&S->link_flags, " -l%s", name);
    return true;
}
static void
git_flags(const char* install_dir,
          const char* repo) { /* the installed .pc file says where its
                                 headers and libraries are */
    FILE* pipe =
        popen(format("PKG_CONFIG_PATH=%s/lib/pkgconfig pkg-config "
                     "--cflags --libs %s 2>/dev/null",
                     install_dir, repo),
              "r");
    char line[4096] = {0};
    if (pipe) {
        if (!fgets(line, sizeof line, pipe)) line[0] = 0;
        pclose(pipe);
    }
    for (char* flag = strtok(line, " \n"); flag;
         flag       = strtok(0, " \n")) {
        if (!strncmp(flag, "-I", 2)) {
            list_push(&S->c_args, strdup(flag));
            append(&S->c_flags, " %s", flag);
        } else if (!strncmp(flag, "-l", 2) || !strncmp(flag, "-L", 2))
            append(&S->link_flags, " %s", flag);
    }
}
static void
git_import(const char* owner, const char* repo, const char* hash,
           Token* token_list, int count,
           int at) { // owner:repo/hash: shared checkout, built with
                     // cmake into the module's install
    char* root     = realpath(S->main_dir, 0);
    char* checkout = format("%s/../checkout/%s/%s", root, owner, repo);
    char* install_dir = format("%s/install", S->out_dir);
    run_shell(
        format("mkdir -p %s/lib %s/include", install_dir, install_dir));
    Buf build_defs = {0};
    int line = token_list[at].line; /* absolute: configure insists */
    while (at < count && token_list[at].line == line) at++;
    for (; at < count && token_list[at].indent > 0;) {
        int   body_line = token_list[at].line;
        char* text      = line_text(body_line);
        while (at < count && token_list[at].line == body_line) at++;
        if (*text == '{') {
            char* open_paren = strchr(text, '(');
            char* close_paren =
                open_paren ? strchr(open_paren, ')') : 0;
            char* value_part =
                close_paren ? strstr(close_paren, "??") : 0;
            if (!value_part ||
                !platform_name(strndup(open_paren + 1,
                                       close_paren - open_paren - 1)))
                continue;
            text = value_part + 2;
            while (*text == ' ') text++;
            char* end = strrchr(text, '}');
            if (end) *end = 0;
        } // { (platform) ?? flag }
        Buf expanded = {0};
        for (char* cursor = text; *cursor;) {
            if (!strncmp(cursor, "{install}", 9)) {
                append(&expanded, "%s", install_dir);
                cursor += 9;
            } else if (!strncmp(cursor, "{root_path}", 11)) {
                append(&expanded, "%s/..", S->main_dir);
                cursor += 11;
            } else append(&expanded, "%c", *cursor++);
        }
        text = expanded.data ? expanded.data : text;
        if (!strncmp(text, "-l", 2) || !strncmp(text, "-L", 2))
            append(&S->link_flags, " %s", text);
        else if (!strncmp(text, "-D", 2) || !strncmp(text, "--", 2))
            append(&build_defs, " %s", text);
        else if (*text == '+') append(&build_defs, " -D%s", text + 1);
    }
    append(&S->link_flags, " -L%s/lib -Wl,-rpath,%s/lib", install_dir,
           install_dir);
    char* token_file =
        format("%s/%s-%s.built", install_dir, repo, hash);
    if (!access(token_file, R_OK)) {
        git_flags(install_dir, repo);
        return;
    }
    if (access(checkout, R_OK) &&
        run_shell(format("git clone -q https://github.com/%s/%s %s",
                         owner, repo, checkout)))
        exit(1);
    if (run_shell(format("git -C %s checkout -q %s", checkout, hash)))
        exit(1);
    char* build_dir = format("%s/build/%s", S->out_dir, repo);
    if (S->cross) { // the target's toolchain files say the compiler,
                    // sysroot and quirks for every build system
        append(&build_defs,
               " -DCMAKE_TOOLCHAIN_FILE=%s/platform/%s/target.cmake",
               S->silver_root, S->target_dir);
    }
    char* def_text = build_defs.data ? build_defs.data : "";
    char* meson_cross =
        S->cross
            ? format(" --cross-file %s/platform/%s/meson-cross.ini",
                     S->silver_root, S->target_dir)
            : "";
    char* auto_host =
        S->cross ? format(" --host=%s CC='%s/clang %s' CXX='%s/clang++ "
                          "%s' LD='%s/ld.lld' AR='%s/llvm-ar' "
                          "RANLIB='%s/llvm-ranlib'",
                          S->triple, S->tools, S->tgt, S->tools, S->tgt,
                          S->tools, S->tools, S->tools)
                 : "";
    char* command; /* the checkout's own build system decides */
    if (!access(format("%s/CMakeLists.txt", checkout), R_OK))
        command = format(
            "cmake -S %s -B %s -DCMAKE_INSTALL_PREFIX=%s "
            "-DCMAKE_PREFIX_PATH=%s -DCMAKE_BUILD_TYPE=Release%s "
            "> /dev/null && cmake --build %s -j8 > /dev/null && cmake "
            "--install %s > /dev/null",
            checkout, build_dir, install_dir, install_dir, def_text,
            build_dir, build_dir);
    else if (!access(format("%s/meson.build", checkout), R_OK))
        command =
            format("meson setup %s %s --prefix=%s --libdir=lib "
                   "--buildtype=release "
                   "--pkg-config-path=%s/lib/pkgconfig%s%s > /dev/null "
                   "&& meson install -C %s > /dev/null",
                   build_dir, checkout, install_dir, install_dir,
                   meson_cross, S->cross ? "" : def_text, build_dir);
    else if (!access(format("%s/Cargo.toml", checkout), R_OK))
        command = format(
            "cargo build --release --manifest-path %s/Cargo.toml "
            "--target-dir %s%s > /dev/null 2>&1 && mkdir "
            "-p %s/lib %s/include && cp %s/release/*.so %s/release/*.a "
            "%s/lib/ 2>/dev/null; cbindgen --lang c "
            "-o %s/include/%s.h %s 2>/dev/null; true",
            checkout, build_dir, def_text, install_dir, install_dir,
            build_dir, build_dir, install_dir, install_dir, repo,
            checkout);
    else
        command =
            format("cd %s && ([ -f configure ] || autoreconf -fi > "
                   "/dev/null 2>&1) && ./configure --prefix=%s%s%s > "
                   "/dev/null && make -j8 install > /dev/null",
                   checkout, install_dir, S->cross ? "" : def_text,
                   auto_host); /* autotools builds in the tree */
    if (run_shell(format("%s && touch %s", command, token_file))) {
        fprintf(stderr, "import %s:%s: build failed\n", owner, repo);
        exit(1);
    }
    git_flags(install_dir, repo);
}
static CD* macro_named(const char* name) {
    for (int i = 0; i < S->ncdecls; i++)
        if (S->cdecls[i]->kind == CD_MACRO &&
            same(S->cdecls[i]->name, name))
            return S->cdecls[i];
    return 0;
}
static void token_push(Token** out, int* count, int* capacity,
                       Token token) {
    if (*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 64;
        *out      = realloc(*out, *capacity * sizeof(Token));
    }
    (*out)[(*count)++] = token;
}
static bool token_is_value(Token* token) {
    return token && (token->kind == 'a' || token->kind == 'n' ||
                     token->kind == 's' || token->kind == 'c' ||
                     token->kind == 'u' || same(token->text, ")") ||
                     same(token->text, "]"));
}
static const char* c_words[] = {"unsigned", "signed", "short", "long",
                                "int",      "char",   "float", "double",
                                "const",    0};
static bool        is_c_type_word(const char* word) {
    for (int i = 0; c_words[i]; i++)
        if (same(c_words[i], word)) return true;
    return false;
}
static Token make_token(Token like, const char* text, char kind) {
    Token token    = like;
    token.text     = (char*)text;
    token.kind     = kind;
    token.no_space = true;
    return token;
}
static Token*
translate(Token* in_tokens, int* in_count,
          Token like) { // C spelling to silver: calls take [ ], -> is
                        // ., & is @, NULL is null, C type words become
                        // silver names, a trailing * makes a reference
    Token* out       = 0;
    int    out_count = 0, capacity = 0;
    int    stack[64], depth        = 0;
    for (int i = 0; i < *in_count; i++) {
        Token       token    = in_tokens[i];
        Token*      prev_tok = i ? &in_tokens[i - 1] : 0;
        const char* text     = token.text;
        if (same(text, "(")) {
            bool is_call =
                prev_tok && ((prev_tok->kind == 'a' &&
                              !is_c_type_word(prev_tok->text)) ||
                             same(prev_tok->text, ")") ||
                             same(prev_tok->text, "]"));
            stack[depth++] = is_call;
            token_push(
                &out, &out_count, &capacity,
                make_token(like, is_call ? "[" : "(", token.kind));
            continue;
        }
        if (same(text, ")")) {
            bool is_call = depth ? stack[--depth] : false;
            token_push(
                &out, &out_count, &capacity,
                make_token(like, is_call ? "]" : ")", token.kind));
            continue;
        }
        if (same(text, "->")) {
            token_push(&out, &out_count, &capacity,
                       make_token(like, ".", token.kind));
            continue;
        }
        if (same(text, "&") && !token_is_value(prev_tok)) {
            token_push(&out, &out_count, &capacity,
                       make_token(like, "@", token.kind));
            continue;
        }
        if (same(text, "NULL") || same(text, "nullptr")) {
            token_push(&out, &out_count, &capacity,
                       make_token(like, "null", 'a'));
            continue;
        }
        if (token.kind == 'a' && prev_tok && prev_tok->kind == 'n' &&
            strlen(text) <= 3 && strspn(text, "uUlLfF") == strlen(text))
            continue; /* an integer suffix */
        if (token.kind == 'a' && is_c_type_word(text)) {
            bool is_unsigned = false, is_short = false, is_long = false,
                 is_char = false, is_float = false, is_double = false;
            while (i < *in_count && in_tokens[i].kind == 'a' &&
                   is_c_type_word(in_tokens[i].text)) {
                const char* word = in_tokens[i].text;
                is_unsigned |= same(word, "unsigned");
                is_short |= same(word, "short");
                is_long |= same(word, "long");
                is_char |= same(word, "char");
                is_float |= same(word, "float");
                is_double |= same(word, "double");
                i++;
            }
            i--;
            token_push(
                &out, &out_count, &capacity,
                make_token(like,
                           is_float      ? "f32"
                           : is_double   ? "f64"
                           : is_char     ? (is_unsigned ? "u8" : "i8")
                           : is_short    ? (is_unsigned ? "u16" : "i16")
                           : is_long     ? (is_unsigned ? "u64" : "i64")
                           : is_unsigned ? "u32"
                                         : "i32",
                           'a'));
            continue;
        }
        token_push(&out, &out_count, &capacity, token);
    }
    int stars = 0;
    while (out_count - 1 - stars > 0 &&
           same(out[out_count - 1 - stars].text, "*"))
        stars++; /* a type macro: T * becomes @ T */
    if (stars && out_count - stars == 1 && out[0].kind == 'a') {
        Token* wrapped       = 0;
        int    wrapped_count = 0, wrapped_cap = 0;
        for (int k = 0; k < stars; k++)
            token_push(&wrapped, &wrapped_count, &wrapped_cap,
                       make_token(like, "@", out[out_count - 1].kind));
        token_push(&wrapped, &wrapped_count, &wrapped_cap, out[0]);
        free(out);
        out       = wrapped;
        out_count = wrapped_count;
    }
    *in_count = out_count;
    return out;
}
static void expand_into(Token** out, int* out_count, int* capacity,
                        Token* token_list, int count, const char* skip,
                        int depth);
static void expand_body(Token** out, int* out_count, int* capacity,
                        CD* macro, Token** args, int* arg_lens,
                        int arg_count, Token like, int depth) {
    if (strstr(macro->body, "#")) {
        token_push(out, out_count, capacity, like);
        return;
    } /* # and ## have no silver form: leave the name */
    int    body_count;
    Token* body_tokens  = tokenize(macro->body, &body_count);
    Token* result       = 0;
    int    result_count = 0, result_cap = 0;
    for (int i = 0; i < body_count; i++) {
        Token token    = body_tokens[i];
        token.line     = like.line;
        token.col      = like.col;
        token.indent   = like.indent;
        token.no_space = true;
        int param_i    = -1;
        if (token.kind == 'a')
            for (int k = 0; k < macro->param_name_count; k++)
                if (same(macro->param_names[k], token.text))
                    param_i = k;
        if (param_i >= 0 && param_i < arg_count) {
            for (int k = 0; k < arg_lens[param_i]; k++) {
                Token arg_tok    = args[param_i][k];
                arg_tok.line     = like.line;
                arg_tok.col      = like.col;
                arg_tok.indent   = like.indent;
                arg_tok.no_space = true;
                token_push(&result, &result_count, &result_cap,
                           arg_tok);
            }
        } else token_push(&result, &result_count, &result_cap, token);
    }
    Token* translated = translate(result, &result_count, like);
    expand_into(out, out_count, capacity, translated, result_count,
                macro->name, depth + 1);
}
static void
expand_into(Token** out, int* out_count, int* capacity,
            Token* token_list, int count, const char* skip,
            int depth) { // C macros the imported headers define,
                         // replaced in the token stream
    for (int i = 0; i < count; i++) {
        Token token = token_list[i];
        CD*   macro =
            token.kind == 'a' && depth < 16 && !same(token.text, skip)
                  ? macro_named(token.text)
                  : 0;
        if (macro && !macro->is_static) {
            expand_body(out, out_count, capacity, macro, 0, 0, 0, token,
                        depth);
            continue;
        }
        if (macro && i + 1 < count &&
            same(token_list[i + 1].text, "[") &&
            token_list[i + 1].line == token.line) {
            Token* args[32];
            int    arg_lens[32];
            int arg_count = 0, nesting = 0, end = i + 1, start = i + 2;
            for (; end < count; end++) {
                if (same(token_list[end].text, "[") ||
                    same(token_list[end].text, "("))
                    nesting++;
                if (same(token_list[end].text, "]") ||
                    same(token_list[end].text, ")")) {
                    nesting--;
                    if (!nesting) break;
                }
                if (nesting == 1 && same(token_list[end].text, ",") &&
                    arg_count < 31) {
                    args[arg_count]       = token_list + start;
                    arg_lens[arg_count++] = end - start;
                    start                 = end + 1;
                }
            }
            if (end > start || arg_count) {
                args[arg_count]       = token_list + start;
                arg_lens[arg_count++] = end - start;
            }
            expand_body(out, out_count, capacity, macro, args, arg_lens,
                        arg_count, token, depth);
            i = end;
            continue;
        }
        token_push(out, out_count, capacity, token);
    }
}
static Token* expand_macros(Token* token_list, int* count) {
    Token* out       = 0;
    int    out_count = 0, capacity = 0;
    expand_into(&out, &out_count, &capacity, token_list, *count, "", 0);
    *count = out_count;
    return out ? out : token_list;
}
static void
c_phase(Token* token_list, int count,
        const char* dir) { // C and C++ import for one file: headers,
                           // template instantiations and macro probes
                           // it mentions, through clang
    bool is_on = true, has_imports = false;
    for (int i = 0; i < count; i++) {
        Token* token = &token_list[i];
        if (token->indent == 0 &&
            (i == 0 || token->line != token_list[i - 1].line))
            is_on = true;
        if ((same(token->text, "ifdef") ||
             same(token->text, "ifndef")) &&
            token->indent == 0 && i + 2 < count)
            is_on = platform_name(token_list[i + 2].text) !=
                    same(token->text, "ifndef");
        if (same(token->text, "import") && i + 1 < count &&
            token_list[i + 1].kind == 'a' && is_on) {
            const char* module_name = token_list[i + 1].text;
            if (access(format("%s/%s.ag", dir, module_name), R_OK) &&
                access(format("%s/../%s/%s.ag", dir, module_name,
                              module_name),
                       R_OK) &&
                access(format("%s/%s.c", dir, module_name), R_OK))
                au_module_import(module_name);
        }
        int after = i + 1;
        if (same(token->text, "import") && i + 5 < count &&
            same(token_list[i + 2].text, ":") &&
            token_list[i + 3].kind == 'a' &&
            same(token_list[i + 4].text, "/") && is_on) {
            Buf hash = {0};
            for (after = i + 5; after < count &&
                                token_list[after].line == token->line &&
                                !same(token_list[after].text, "<");
                 after++)
                append(&hash, "%s", token_list[after].text);
            git_import(token_list[i + 1].text, token_list[i + 3].text,
                       hash.data, token_list, count, i);
        } // the hash may tokenize as several pieces
        if (same(token->text, "import") && after < count &&
            same(token_list[after].text, "<") && is_on) {
            Buf header = {0};
            for (int k = after + 1;
                 k < count && !same(token_list[k].text, ">"); k++) {
                if (same(token_list[k].text, ",")) {
                    c_include(header.data);
                    header = (Buf){0};
                } else append(&header, "%s", token_list[k].text);
            }
            if (header.count) c_include(header.data);
            has_imports = true;
        }
    }
    char* out_dir   = (char*)S->out_dir;
    char* rust_file = format("%s/%s.rs", dir, S->modname);
    if (!access(rust_file, R_OK) &&
        !strstr(S->c_includes.data ? S->c_includes.data : "",
                "_rs.h")) {
        if (!run_shell(format("cbindgen --lang c -o %s/%s_rs.h %s",
                              out_dir, S->modname, rust_file)))
            append(&S->c_includes,
                   "extern \"C\" {\n#include \"%s/%s_rs.h\"\n}\n",
                   realpath(out_dir, 0), S->modname);
        has_imports = true;
    } // the rust companion's C surface
    if (!has_imports && !S->c_includes.count) return;
    char*       unit_file      = format("%s/cimport.cpp", out_dir);
    const char* clang_args[64] = {
        format("-I%s", dir), format("-I%s", S->main_dir),
        format("-I%s/install/include", S->out_dir)};
    int clang_arg_count = 3;
    if (S->cross) // clang's own front end takes cc1 spellings: -triple
                  // and -isysroot, not the driver's -target and
                  // --sysroot
        for (char* flag = strtok(strdup(S->tgt), " ");
             flag && clang_arg_count < 60; flag = strtok(0, " ")) {
            if (same(flag, "-target")) flag = "-triple";
            else if (!strncmp(flag, "--sysroot=", 10)) {
                clang_args[clang_arg_count++] = "-isysroot";
                flag += 10;
            } else if (same(flag, "-isysroot")) {
            } else if (flag[0] == '-' && !same(flag, "-isystem") &&
                       strncmp(flag, "-f", 2) && strncmp(flag, "-m", 2))
                continue;
            clang_args[clang_arg_count++] = flag;
        }
    for (int k = 0; k < S->c_args.count && clang_arg_count < 63; k++)
        clang_args[clang_arg_count++] = S->c_args.data[k];
    for (int pass = 0; pass < 2; pass++) {
        FILE* file = fopen(unit_file, "w");
        fprintf(file, "%s%s", S->c_includes.data,
                pass ? (S->c_insts.data ? S->c_insts.data : "") : "");
        fclose(file);
        S->cdecls = c_import(
            unit_file, clang_args, clang_arg_count, &S->ncdecls,
            S->cross ? format("%s/clang++ %s", S->tools, S->tgt)
                     : "clang++");
        if (!S->cdecls) {
            fprintf(stderr, "C import failed\n");
            exit(1);
        }
        if (pass) break;
        for (int i = 0; i < S->ncdecls; i++) {
            CD* decl = S->cdecls[i];
            if (decl->kind != CD_RECORD && decl->kind != CD_FUNC)
                continue; // reference every header-defined function so
                          // the import unit emits it
            for (int k = -1;
                 k < (decl->kind == CD_RECORD ? decl->member_count : 0);
                 k++) {
                CD* member = k < 0 ? decl : decl->members[k];
                if (member->kind == CD_FIELD ||
                    member->kind == CD_RECORD || !member->is_inline ||
                    (member->owner && member->owner != decl))
                    continue; // inherited members belong to their own
                              // record
                Buf arg_list = {0};
                for (int q = 0; q < member->param_count; q++)
                    append(
                        &arg_list, "%s*(%s*)0", q ? ", " : "",
                        member->params[q]->is_ref
                            ? format(
                                  "%s",
                                  member->params[q]->elem
                                      ? member->params[q]->elem->spell
                                      : member->params[q]->spell)
                            : member->params[q]->spell);
                static int use_count;
                if (member->kind == CD_CTOR)
                    append(&S->c_insts,
                           "__attribute__((used)) static void "
                           "s2_ref%d() { (void)%s(%s); }\n",
                           use_count++, decl->qualified,
                           arg_list.data ? arg_list.data : "");
                else if (member->kind == CD_FUNC)
                    append(&S->c_insts,
                           "__attribute__((used)) static void "
                           "s2_ref%d() { (void)%s(%s); }\n",
                           use_count++, member->qualified,
                           arg_list.data ? arg_list.data : "");
                else
                    append(&S->c_insts,
                           "__attribute__((used)) static void "
                           "s2_ref%d() { (void)((%s*)0)->%s(%s); }\n",
                           use_count++, decl->qualified,
                           member->kind == CD_CONV
                               ? format("operator %s",
                                        member->result->spell)
                               : member->name,
                           arg_list.data ? arg_list.data : "");
            }
        }
        for (int i = 0; i < S->ncdecls; i++) {
            CD* decl = S->cdecls[i];
            if (decl->kind == CD_RECORD || decl->kind == CD_TEMPLATE ||
                decl->kind == CD_NAMESPACE) {
                list_push(&S->type_names, (void*)decl->name);
                if (decl->qualified && strstr(decl->qualified, "::"))
                    list_push(&S->type_names,
                              strndup(decl->qualified,
                                      strstr(decl->qualified, "::") -
                                          decl->qualified));
            }
        }
        for (int i = 0; i < count; i++) {
            Token* token = &token_list[i];
            if (token->kind != 'a')
                continue; // mentions: T<args> instantiates, MACRO
                          // probes
            CD* template = 0;
            for (int k = 0; k < S->ncdecls; k++)
                if ((S->cdecls[k]->kind == CD_TEMPLATE ||
                     S->cdecls[k]->kind == CD_FTEMPLATE) &&
                    same(S->cdecls[k]->name, token->text))
                    template = S->cdecls[k];
            if (template && i + 1 < count &&
                same(token_list[i + 1].text, "<") &&
                token_list[i + 1].no_space) {
                Buf arg_list = {0};
                int depth = 1, k = i + 2;
                for (; k < count && depth; k++) {
                    if (same(token_list[k].text, "<")) depth++;
                    if (same(token_list[k].text, ">") && !--depth)
                        break;
                    append(&arg_list, "%s", token_list[k].text);
                }
                Buf prefix = {0};
                for (int q = i - 2;
                     q >= 0 && same(token_list[q + 1].text, "::");
                     q -= 2)
                    append(&prefix, "%s::", token_list[q].text);
                char* spelling = c_spell(format(
                    "%s%s<%s>", prefix.data ? prefix.data : "",
                    token->text, arg_list.data ? arg_list.data : ""));
                char* first_arg =
                    strdup(arg_list.data ? arg_list.data : "");
                char* comma = strchr(first_arg, ',');
                if (comma) *comma = 0;
                if (c_find(spelling, CD_RECORD)) continue;
                if (strchr(S->c_insts.data ? S->c_insts.data : "", 0) &&
                    strstr(S->c_insts.data ? S->c_insts.data : "",
                           spelling))
                    continue;
                int use_arg_count = 0;
                if (k + 1 < count &&
                    same(token_list[k + 1].text, "[")) {
                    int nesting = 1;
                    for (int q = k + 2; q < count && nesting; q++) {
                        if (same(token_list[q].text, "[")) nesting++;
                        if (same(token_list[q].text, "]")) nesting--;
                        if (nesting == 1 &&
                            same(token_list[q].text, ","))
                            use_arg_count++;
                        if (nesting == 1 && use_arg_count == 0 &&
                            !same(token_list[q].text, "]") &&
                            token_list[q].kind != 'p')
                            use_arg_count = 1;
                        if (!nesting) break;
                    }
                    if (use_arg_count == 1) {
                        int nesting2 = 1;
                        for (int q = k + 2; q < count && nesting2;
                             q++) {
                            if (same(token_list[q].text, "["))
                                nesting2++;
                            if (same(token_list[q].text, "]"))
                                nesting2--;
                            if (nesting2 == 1 &&
                                same(token_list[q].text, ","))
                                use_arg_count++;
                        }
                    }
                }
                bool is_func_template = template->kind == CD_FTEMPLATE;
                if (is_func_template) {
                    static int use_count;
                    Buf        use_args = {0};
                    for (int q = 0; q < use_arg_count; q++)
                        append(&use_args, "%s%s()", q ? ", " : "",
                               c_spell(first_arg));
                    append(&S->c_insts,
                           "__attribute__((used)) static void "
                           "s2_use_%d() { (void)%s(%s); }\n",
                           use_count++, spelling,
                           use_args.data ? use_args.data : "");
                } else
                    append(&S->c_insts, "template struct %s;\n",
                           spelling);
            }
        }
    }
    if (run_shell(format(
            "%s %s -w -c -fPIC -std=c++17 %s -I%s -I%s "
            "-I%s/install/include%s -o %s/cimport.o %s",
            S->cross ? format("%s/clang++", S->tools) : "clang++",
            S->cross ? S->tgt : "", S->cross ? cxx_abi() : "", dir,
            S->main_dir, S->out_dir,
            S->c_flags.data ? S->c_flags.data : "", out_dir,
            unit_file)))
        exit(1);
    if (!strstr(S->extra_srcs.data ? S->extra_srcs.data : "",
                "cimport.o"))
        append(&S->extra_srcs, " %s/cimport.o -lstdc++", out_dir);
}
static int build_import(const char* module_path,
                        const char* extend_path, int for_target);
static void
load_module(const char* path, const char* dir,
            Node* module_node) { // parse one .ag and resolve its
                                 // imports; decls land in mod
    char* source = read_file(path);
    if (!source) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    const char* saved_file   = S->cur_file;
    char*       saved_source = S->cur_src;
    S->cur_file              = path;
    S->cur_src               = source;
    int    count;
    Token* token_list = tokenize(source, &count);
    c_phase(token_list, count, dir);
    token_list = expand_macros(token_list, &count);
    Node* sub_module =
        parse_module(token_list, count, new_node(S_BLOCK, "module", 0));
    for (int i = 0; i < sub_module->count; i++) {
        Node* decl = sub_module->kids[i];
        if (decl->kind != D_IMPORT || !decl->count ||
            same(decl->kids[0]->text, "<") ||
            (decl->count > 2 && same(decl->kids[1]->text, ":") &&
             !same(decl->kids[2]->text, "/")))
            continue; /* owner:repo is a git import, done in the C phase
                       */
        const char* name = decl->kids[0]->text;
        if (decl->count > 3 && same(decl->kids[1]->text, ":") &&
            same(decl->kids[2]->text,
                 "/")) { // a single file by url, fetched once; > lines
                         // run after
            Buf url = {0};
            for (int k = 0;
                 k < decl->count && decl->kids[k]->kind == N_IDENT; k++)
                append(&url, "%s", decl->kids[k]->text);
            char* file = format("%s/.silver2/imports/%s", S->main_dir,
                                strrchr(url.data, '/') + 1);
            if (access(file, R_OK))
                run_shell(format("curl -sL -o %s %s", file, url.data));
            for (int k = 0; k < decl->count; k++)
                if (decl->kids[k]->kind == N_RAW) {
                    Buf command = {0};
                    for (const char* cursor = decl->kids[k]->text;
                         *cursor;) {
                        if (!strncmp(cursor, "{import_file}", 13)) {
                            append(&command, "%s", file);
                            cursor += 13;
                        } else if (!strncmp(cursor,
                                            "{install}/share/silver-",
                                            23)) {
                            append(&command, "%s/install/share/",
                                   S->out_dir); // the share holds
                                                // real files too
                            cursor += 23;
                        } else {
                            append(&command, "%c", *cursor);
                            cursor++;
                        }
                    }
                    run_shell(command.data);
                }
            continue;
        }
        char* sibling   = format("%s/%s.ag", dir, name);
        char* root_file = format("%s/../%s/%s.ag", dir, name, name);
        if (!access(sibling, R_OK))
            load_module(sibling, dir, module_node);
        else if (!access(root_file,
                         R_OK)) { // another silver module: build it as
                                  // its own library, then read its Au
                                  // module record
            char* root_dir = realpath(format("%s/../%s", dir, name), 0);
            char* import_path = format("%s/%s.ag", root_dir, name);
            char* extend_path =
                decl->count > 2 && same(decl->kids[1]->text, "with")
                    ? format("%s/%s.ag", dir, decl->kids[2]->text)
                    : 0;
            if (S->cross) { // the target's own build of the import,
                            // beside the host one whose record this
                            // build reads
                if (!build_import(import_path, extend_path, true)) {
                    fprintf(stderr, "import %s: build failed for %s\n",
                            name, S->platform);
                    exit(1);
                }
                append(&S->link_flags,
                       " -L%s/.silver2/%s/install/lib -lsilver-%s",
                       root_dir, S->target_dir, name);
            }
            if (!build_import(import_path, extend_path, false)) {
                fprintf(stderr, "import %s: build failed\n", name);
                exit(1);
            }
            append(&S->import_ledger, "%s\t%s\n", import_path,
                   extend_path ? extend_path : "");
            char* lib_dir = format("%s/.silver2/install/lib", root_dir);
            void* lib_handle =
                dlopen(format("%s/libsilver-%s.so", lib_dir, name),
                       RTLD_NOW | RTLD_GLOBAL);
            if (!lib_handle) {
                fprintf(stderr, "import %s: %s\n", name, dlerror());
                exit(1);
            }
            void (*init_fn)(void) = dlsym(
                lib_handle, format("silver_%s_initializer", name));
            init_fn();
            Au_t (*lookup_module)(const char*) =
                dlsym(S->au_lib, "module_lookup");
            list_push(&S->imported,
                      lookup_module(format("silver-%s", name)));
            list_push(&S->imported_names, name);
            if (!S->cross)
                append(&S->link_flags,
                       " -L%s -lsilver-%s -Wl,-rpath,%s", lib_dir, name,
                       lib_dir);
        } else if (!access(format("%s/%s.c", dir, name), R_OK))
            append(&S->extra_srcs, " %s/%s.c", dir,
                   name); // a C companion: its functions come through
                          // `intern func` declarations
        else if (!au_module_import(name)) {
            fprintf(stderr, "import %s: no module\n", name);
            exit(1);
        }
    }
    for (int i = 0; i < sub_module->count; i++)
        add_kid(module_node, sub_module->kids[i]);
    S->cur_file = saved_file;
    S->cur_src  = saved_source;
}
static const char*
htype(Node* type_node) { // a type as silver spells it in headers
    if (!type_node) return "none";
    const char* text = type_node->text;
    if (same(text, "local")) return htype(kid(type_node, 0));
    if (same(text, "vec")) return "vector";
    if (same(text, "lambda")) return "ARef";
    if (same(text, "@")) return htype(kid(type_node, 0));
    if (same(text, "object") || same(text, "any")) return "Au";
    return text;
}
static void init_block(
    Buf*        out,
    const char* name) { // the constructor macro silver's init header
                        // gives every class: X(prop, value, ...)
    append(
        out,
        "#define TC_%s(MEMBER, VALUE) ({ "
        "AF_set((u64*)&instance->af_bits, FIELD_ID(%s, MEMBER)); "
        "VALUE; })\n#define "
        "_ARG_COUNT_IMPL_%s(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, "
        "_10, _11, _12, _13, _14, _15, _16, _17, _18, "
        "_19, _20, _21, _22, N, ...) N\n#define _ARG_COUNT_I_%s(...) "
        "_ARG_COUNT_IMPL_%s(__VA_ARGS__, 22, 21, 20, "
        "19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, "
        "2, 1, 0)\n#define _ARG_COUNT_%s(...)   "
        "_ARG_COUNT_I_%s(\"Au object model\", ## __VA_ARGS__)\n#define "
        "_COMBINE_%s_(A, B)   A##B\n#define "
        "_COMBINE_%s(A, B)    _COMBINE_%s_(A, B)\n#define "
        "_N_ARGS_%s_0( TYPE)\n#define _N_ARGS_%s_1( TYPE, a) "
        "_Generic((a), TYPE##_schema(TYPE, GENERICS, Au) "
        "Au_schema(TYPE, GENERICS, Au) const void *: "
        "(void)0)((TYPE)(instance), a)\n",
        name, name, name, name, name, name, name, name, name, name,
        name, name);
    const char* letters = "abcdefghijklmnopqrstuv";
    for (int k = 2; k <= 22; k += 2) {
        append(out, "#define _N_ARGS_%s_%d( TYPE", name, k);
        for (int q = 0; q < k; q += 2)
            append(out, ", %c,%c", letters[q], letters[q + 1]);
        append(out, ") ");
        if (k > 2) {
            append(out, "_N_ARGS_%s_%d(TYPE", name, k - 2);
            for (int q = 0; q < k - 2; q += 2)
                append(out, ", %c,%c", letters[q], letters[q + 1]);
            append(out, ") ");
        }
        append(out, "instance->%c = TC_%s(%c,%c);\n", letters[k - 2],
               name, letters[k - 2], letters[k - 1]);
    }
    append(out,
           "#define _N_ARGS_HELPER2_%s(TYPE, N, ...)  "
           "_COMBINE_%s(_N_ARGS_%s_, N)(TYPE, ## __VA_ARGS__)\n#define "
           "_N_ARGS_%s(TYPE,...)    _N_ARGS_HELPER2_%s(TYPE, "
           "_ARG_COUNT_%s(__VA_ARGS__), ## __VA_ARGS__)\n#define "
           "%s(...) ({ \\\n    %s instance = (%s)alloc_dbg(typeid(%s), "
           "1, __FILE__, __LINE__, seq); \\\n    "
           "_N_ARGS_%s(%s, ## __VA_ARGS__); \\\n    "
           "Au_initialize((Au)instance); \\\n    instance; \\\n})\n",
           name, name, name, name, name, name, name, name, name, name,
           name, name);
}
static void
write_headers(Node*       module_node,
              const char* out_dir) { // the six headers silver installs
                                     // per module: <mod>, intern,
                                     // public, methods, init, import
    char* module_ident = module_macro(format("silver-%s", S->modname));
    char* upper_name   = strdup(S->modname);
    for (char* cursor = upper_name; *cursor; cursor++)
        *cursor = toupper(*cursor);
    char* include_dir =
        format("%s/install/include/%s", out_dir, S->modname);
    run_shell(format("mkdir -p %s", include_dir));
    Buf main_header = {0}, intern_header = {0}, public_header = {0},
        methods_header = {0}, init_header = {0}, import_header = {0};
    List method_names = {0};
    append(&main_header,
           "#ifndef _%s_\n#define _%s_\n\n#ifndef AU_LINK_%s\n#ifdef "
           "_WIN32\n#define AU_LINK_%s "
           "__attribute__((dllimport))\n#else\n#define "
           "AU_LINK_%s\n#endif\n#endif\n\n",
           upper_name, upper_name, module_ident, module_ident,
           module_ident);
    append(&intern_header, "#ifndef _%s_INTERN_\n#define _%s_INTERN_\n",
           upper_name, upper_name);
    append(&public_header, "#ifndef _%s_PUBLIC_\n#define _%s_PUBLIC_\n",
           upper_name, upper_name);
    append(&methods_header,
           "#ifndef _%s_METHODS_\n#define _%s_METHODS_\n#ifndef "
           "__cplusplus\n",
           upper_name, upper_name);
    append(&init_header, "#ifndef _%s_INIT_\n#define _%s_INIT_\n",
           upper_name, upper_name);
#define ALIAS_KIND(type)                                               \
    ((type)->kind == T_UNK && (type)->decl                             \
         ? (same((type)->decl->text, "vec") ||                         \
                    same((type)->decl->text, "map")                    \
                ? 2                                                    \
            : same((type)->decl->text, "@") ? 0                        \
                                            : 1)                       \
         : 3) // 2 alias to a class, 1 alias to a value, 0 skipped
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (!type->decl || type->record || !ALIAS_KIND(type)) continue;
        append(&main_header,
               "#ifndef %s_module_\n#define %s_module_ %s\n#endif\n",
               type->name, type->name, module_ident);
        if (type->kind == T_CLASS || ALIAS_KIND(type) == 2) {
            append(
                &intern_header,
                "#undef %s_intern\n#define %s_intern(A,B,...) "
                "A##_schema(A,B, __VA_ARGS__)\n#define %s_module_ %s\n",
                type->name, type->name, type->name, module_ident);
            append(&public_header,
                   "#ifndef %s_intern\n#define %s_intern(A,B,...) "
                   "A##_schema(A,B##_EXTERN, __VA_ARGS__)\n#endif\n",
                   type->name, type->name);
        }
    }
    append(&main_header,
           "#ifndef %s_module_\n#define %s_module_ %s\n#endif\n\n",
           S->modname, S->modname, module_ident);
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->kind != T_ENUM || !type->decl) continue;
        append(&main_header, "#define %s_schema(E,T,Y,...)\\\n",
               type->name);
        for (int k = 1; k < type->decl->count; k++) {
            Node*     enum_node = type->decl->kids[k];
            long long int_bits =
                backing(type)->kind == T_FLOAT ? ({
                    float   float_bits = atof(enum_node->raw);
                    int32_t bits_int;
                    memcpy(&bits_int, &float_bits, 4);
                    (long long)bits_int;
                })
                                               : atoll(enum_node->raw);
            append(&main_header, "    enum_value(E,T,Y, %s, %lld)%s\n",
                   enum_node->text, int_bits,
                   k + 1 < type->decl->count ? "\\" : "");
        }
        append(&main_header, "\ndeclare_enum(%s)\n\n", type->name);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->decl && !type->record &&
            (type->kind == T_CLASS || ALIAS_KIND(type) == 2))
            append(&main_header, "forward(%s)\n", type->name);
    }
    append(&main_header, "forward(%s)\n", S->modname);
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->decl &&
            (type->kind == T_SCALAR || type->kind == T_STRUCT))
            append(&main_header, "typedef struct _%s %s;\n", type->name,
                   type->name);
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (!type->decl ||
            (type->kind != T_SCALAR && type->kind != T_STRUCT))
            continue;
        Node* decl   = type->decl;
        Buf   schema = {0};
        for (int k = 1; k < decl->count; k++) {
            Node* member = decl->kids[k];
            if (member->kind == D_MEMBER)
                append(&schema, "    i_struct_prop(O, Y, %s, %s)\\\n",
                       htype(member->kids[0]), member->text);
            else if (member->kind == D_CTOR)
                append(&schema, "    i_struct_ctr(O, Y, %s)\\\n",
                       param_type(member->kids[0]->kids[0])->name);
            else if (member->kind == D_CASTFN)
                append(&schema,
                       "    i_struct_method(O, Y, %s, cast_%s)\\\n",
                       member->kids[1]->text, member->kids[1]->text);
            else {
                Node* params = member->kids[0];
                bool  struct_first =
                    params->count &&
                    param_type(params->kids[0])->kind == T_STRUCT;
                append(&schema, "    i_struct_method%s(O, Y, %s, %s",
                       struct_first ? "_1" : "",
                       fn_ret(member)->kind == T_NONE
                           ? "none"
                           : fn_ret(member)->name,
                       member_name(member));
                for (int q = 0; q < params->count; q++)
                    append(&schema, ", %s",
                           param_type(params->kids[q])->name);
                append(&schema, ")\\\n");
            }
        }
        if (type->kind == T_SCALAR && !find_cast(type, "string", 0))
            append(
                &schema,
                "    i_struct_method(O, Y, string, cast_string)\\\n");
        if (schema.count >= 2) {
            schema.count -= 2;
            schema.data[schema.count] = 0;
        }
        append(&main_header,
               "#define %s_schema(O, Y%s, "
               "...)\\\n%s\n\ndeclare_struct(%s%s)\n\n",
               type->name, type->kind == T_SCALAR ? ", T" : "",
               schema.data ? schema.data : "", type->name,
               type->kind == T_SCALAR ? format(", %s", type->base->name)
                                      : "");
    }
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (ALIAS_KIND(type) == 2 && type->decl) {
            init_block(&init_header, type->name);
            continue;
        }
        if (!type->decl || type->kind != T_CLASS || type->record)
            continue;
        Node* decl     = type->decl;
        Buf   schema   = {0};
        bool  has_init = false;
        for (int k = 1; k < decl->count; k++) {
            Node* member = decl->kids[k];
            int   mods   = member_mods(
                member); // a subclass schema lists only what it adds
            if (type->base &&
                (member->kind == D_MEMBER
                     ? member_named(type->base, member->text, 0) != 0
                     : find_method(type->base, member->text,
                                   member->kind, 0) != 0)) {
                if (member->kind != D_MEMBER)
                    list_push(&method_names,
                              (void*)member_name(member));
                continue;
            } // overrides still get method macros
            if (member->kind == D_MEMBER) {
                Type* member_type = type_of(member->kids[0]);
                append(&schema, "M(A,B, %s,%s,%s,%s,%s)\\\n",
                       mods & 1 ? "s" : "i",
                       member_type->kind == T_REF ? "ref" : "prop",
                       mods & 64 ? "intern" : "public",
                       member_type->kind == T_REF
                           ? member_type->elem->name
                           : htype(member->kids[0]),
                       member->text);
                continue;
            }
            const char* header_name = member_name(member);
            list_push(&method_names, (void*)header_name);
            if (same(header_name, "init")) {
                has_init = true;
                if (type->base) continue;
            }
            if (member->kind == D_CASTFN &&
                (same(member->kids[1]->text, "string") ||
                 same(member->kids[1]->text, "bool"))) {
                append(&schema, "M(A,B, i,override,method,%s)\\\n",
                       header_name);
                continue;
            }
            if (same(header_name, "init") ||
                same(header_name, "dealloc") ||
                (member->kind == D_FUNC &&
                 au_member(au_type("Au"), header_name,
                           AU_MEMBER_FUNC))) {
                append(&schema, "M(A,B, i,override,method,%s)\\\n",
                       header_name);
                continue;
            } /* Au's own methods are overrides */
            Node* params = member->kids[0];
            Type* result_type =
                member->kind == D_CTOR ? basic(T_NONE) : fn_ret(member);
            append(&schema, "M(A,B, %s,method,%s,%s,%s",
                   (mods & 1) || params->flag < params->count ? "s"
                                                              : "i",
                   mods & 64 ? "intern" : "public",
                   result_type->kind == T_NONE ? "none"
                                               : result_type->name,
                   header_name);
            for (int q = 0; q < params->flag; q++)
                append(&schema, ",%s", htype(params->kids[q]->kids[0]));
            append(&schema, ")\\\n");
        }
        if (!has_init) {
            list_push(&method_names, "init");
            if (!type->base)
                append(&schema, "M(A,B, i,override,method,init)\\\n");
        }
        if (schema.count >= 2) {
            schema.count -= 2;
            schema.data[schema.count] = 0;
        }
        Buf base_chain = {0};
        int base_count = 0;
        for (Type* cur = type->base; cur; cur = cur->base) {
            append(&base_chain, ",%s", cur->name);
            base_count++;
        }
        append(&main_header,
               "#define "
               "%s_schema(A,B,...)%s%s\n\ndeclare_class%s(%s%s)\n\n",
               type->name, schema.count ? "\\\n" : "",
               schema.count ? schema.data : "",
               base_count ? format("_%d", base_count + 1) : "",
               type->name, base_chain.data ? base_chain.data : "");
        init_block(&init_header, type->name);
    }
    append(&main_header,
           "#define %s_schema(A,B,...)\\\nM(A,B, "
           "i,override,method,init)\n\ndeclare_class_2(%s,app)\n\n#"
           "endif\n",
           S->modname, S->modname);
    append(
        &intern_header,
        "#undef %s_intern\n#define %s_intern(A,B,...) A##_schema(A,B, "
        "__VA_ARGS__)\n#define %s_module_ %s\n#include "
        "<%s/%s>\n#endif\n",
        S->modname, S->modname, S->modname, module_ident, S->modname,
        S->modname);
    init_block(&init_header, S->modname);
    append(&public_header,
           "#ifndef %s_intern\n#define %s_intern(A,B,...) "
           "A##_schema(A,B##_EXTERN, __VA_ARGS__)\n#endif\n#endif\n",
           S->modname, S->modname);
    for (int i = 0; i < method_names.count; i++)
        append(&methods_header,
               "#ifndef %s\n\t#define %s(I,...) ({{ __typeof__(I) _i_ "
               "= I; ftableI(_i_)->ft.%s(_i_, ## __VA_ARGS__); "
               "}})\n#endif\n\n",
               (char*)method_names.data[i], (char*)method_names.data[i],
               (char*)method_names.data[i]);
    for (int i = 0; i < S->types.count; i++) {
        Type* type = S->types.data[i];
        if (type->decl &&
            (type->kind == T_SCALAR || type->kind == T_STRUCT))
            append(&init_header,
                   "#define %s(...) structure_of(%s __VA_OPT__(,) "
                   "__VA_ARGS__)\n",
                   type->name, type->name);
    }
    append(&methods_header, "#endif /* __cplusplus */\n#endif\n");
    append(&init_header, "#endif\n");
    Buf   includes = {0}, cxx_includes = {0};
    char* rust_header = format("%s/%s_rs.h", out_dir, S->modname);
    if (!access(rust_header, R_OK))
        append(&includes, "#include <%s>\n",
               realpath(rust_header,
                        0)); // C headers as silver lists them: the
                             // module's own by absolute path
    for (int i = 0; i < module_node->count; i++) {
        Node* include_dir = module_node->kids[i];
        if (include_dir->kind == S_IFDEF &&
            platform_value(include_dir->kids[0]) !=
                same(include_dir->text, "ifndef"))
            for (int k = 0; k < include_dir->kids[1]->count; k++)
                add_kid(module_node, include_dir->kids[1]->kids[k]);
    } // platform-gated imports count for this host
    for (int i = 0; i < module_node->count; i++) {
        Node* include_dir = module_node->kids[i];
        if (include_dir->kind != D_IMPORT || !include_dir->count)
            continue;
        int angle_at = -1;
        for (int k = 0; k < include_dir->count; k++)
            if (same(include_dir->kids[k]->text, "<")) {
                angle_at = k;
                break;
            }
        if (angle_at < 0) continue;
        Buf header_name = {0};
        for (int k = angle_at + 1;
             k < include_dir->count &&
             !same(include_dir->kids[k]->text, ">");
             k++) { /* <headers> may follow a git spec */
            if (same(include_dir->kids[k]->text, ",")) {
                append(&includes, "#include <%s>\n", header_name.data);
                header_name = (Buf){0};
            } else
                append(&header_name, "%s", include_dir->kids[k]->text);
        }
        if (header_name.count) {
            char* local_path =
                format("%s/%s", S->main_dir, header_name.data);
            bool is_cxx = strstr(header_name.data, ".hpp") != 0;
            append(is_cxx ? &cxx_includes : &includes,
                   "#include <%s>\n",
                   access(local_path, R_OK) ? header_name.data
                                            : realpath(local_path, 0));
        }
    } /* C++ headers go before the extern "C" block */
    append(
        &import_header,
        "#ifndef _%s_IMPORT_\n#define _%s_IMPORT_\n\n%s#ifdef "
        "__cplusplus\nextern \"C\" {\n#endif\n%s#include "
        "<Au/public>\n#include <Au/Au>\n#include <%s/intern>\n#include "
        "<%s/%s>\n#include <%s/methods>\n#include "
        "<undefcpp.h>\n#include <Au/init>\n#include "
        "<Au/methods>\n#ifdef __cplusplus\n#include "
        "<undefcpp.h>\n#undef "
        "M\n#undef str\n#undef typeid\n#undef print\n#undef a\n#undef "
        "m\n}\n#endif\n#endif\n",
        upper_name, upper_name,
        cxx_includes.data ? cxx_includes.data : "",
        includes.data ? includes.data : "", S->modname, S->modname,
        S->modname, S->modname);
    const char* file_names[6] = {S->modname, "intern", "public",
                                 "methods",  "init",   "import"};
    Buf* buffers[6] = {&main_header,    &intern_header, &public_header,
                       &methods_header, &init_header,   &import_header};
    for (int i = 0; i < 6; i++) {
        FILE* file =
            fopen(format("%s/%s", include_dir, file_names[i]), "w");
        fputs(buffers[i]->data, file);
        fclose(file);
    }
}

// ----------------------------------------------------------------
// release: the distros' own layout and packages, written here without
// their tooling
#include <dirent.h>
#include <time.h>
static long file_size(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : 0;
}
static void copy_into(FILE* out, const char* source) {
    FILE* in = fopen(source, "rb");
    if (!in) return;
    char   chunk[65536];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, in)) > 0)
        fwrite(chunk, 1, got, out);
    fclose(in);
}
static char* shell_line(
    const char* command) { // the first line a command prints, trimmed
    FILE* pipe       = popen(command, "r");
    char  line[4096] = {0};
    if (pipe) {
        if (!fgets(line, sizeof line, pipe)) line[0] = 0;
        pclose(pipe);
    }
    char* end = line + strlen(line);
    while (end > line && (end[-1] == '\n' || end[-1] == ' '))
        *--end = 0;
    return strdup(line);
}
static char* sum_of(const char* tool, const char* path) {
    return shell_line(format("%s %s | cut -d' ' -f1", tool, path));
}
static void save_text(const char* path, const char* text) {
    FILE* file = fopen(path, "w");
    if (file) {
        fputs(text, file);
        fclose(file);
    }
}
static void
bundle_sos(const char* bin, const char* lib_dir,
           List* done) { // every shared library of ours the binary
                         // needs rides in /usr/lib/<name>
    char* repo = realpath(format("%s/..", S->main_dir), 0);
    char* own  = realpath(format("%s/../lib", ({
                                    char* d = strdup(S->self_exe);
                                    *strrchr(d, '/') = 0;
                                    d;
                                })),
                          0);
    FILE* pipe = popen(format("ldd %s", bin), "r");
    char  line[4096];
    while (pipe && fgets(line, sizeof line, pipe)) {
        char* dep = strstr(line, "=> ");
        dep       = dep ? dep + 3 : line;
        while (*dep == ' ' || *dep == '\t') dep++;
        char* paren = strstr(dep, " (");
        if (paren) *paren = 0;
        char* end = dep + strlen(dep);
        while (end > dep && (end[-1] == '\n' || end[-1] == ' '))
            *--end = 0;
        if (dep[0] != '/') continue;
        char* real = realpath(dep, 0);
        if (!real || !((repo && !strncmp(real, repo, strlen(repo))) ||
                       (own && !strncmp(real, own, strlen(own)))))
            continue;
        const char* leaf = strrchr(dep, '/') + 1;
        bool        seen = false;
        for (int i = 0; i < done->count; i++)
            if (same(done->data[i], leaf)) seen = true;
        if (seen) continue;
        list_push(done, strdup(leaf));
        run_shell(format("cp -L %s %s/%s && chmod u+w %s/%s", dep,
                         lib_dir, leaf, lib_dir, leaf));
    }
    if (pipe) pclose(pipe);
}
static void
ar_member(FILE* out, const char* name,
          const char* source) { // .deb: an ar archive of three members,
                                // the 60-byte header is the format
    long size = file_size(source);
    fprintf(out, "%-16s%-12ld%-6d%-6d%-8s%-10ld`\n", name, 0L, 0, 0,
            "100644", size);
    copy_into(out, source);
    if (size & 1) fputc('\n', out);
}
static void write_deb(const char* stage, const char* root,
                      const char* control, const char* deb) {
    char* work = format("%s/deb", stage);
    run_shell(format("mkdir -p %s/control", work));
    save_text(format("%s/debian-binary", work), "2.0\n");
    save_text(format("%s/control/control", work), control);
    run_shell(format("tar --owner=0 --group=0 -C %s/control -czf "
                     "%s/control.tar.gz ./control",
                     work, work));
    run_shell(format(
        "tar --owner=0 --group=0 -C %s -czf %s/data.tar.gz ./usr", root,
        work));
    FILE* out = fopen(deb, "wb");
    if (!out) {
        fprintf(stderr, "package: cannot write %s\n", deb);
        exit(1);
    }
    fputs("!<arch>\n", out);
    ar_member(out, "debian-binary", format("%s/debian-binary", work));
    ar_member(out, "control.tar.gz", format("%s/control.tar.gz", work));
    ar_member(out, "data.tar.gz", format("%s/data.tar.gz", work));
    fclose(out);
}
// rpm: a lead, a signature header, the main header and a gzip'd newc
// cpio. a header is an index of 16-byte entries over a data store,
// big-endian
typedef struct {
    i32 tag, type, offset, count;
} RpmEntry;
typedef struct {
    RpmEntry entries[80];
    int      count;
    u8*      data;
    int      length, capacity;
} RpmHeader;
enum {
    RT_CHAR = 1,
    RT_INT8,
    RT_INT16,
    RT_INT32,
    RT_INT64,
    RT_STRING,
    RT_BIN,
    RT_STRINGS,
    RT_I18N
};
static void be32(u8* at, u32 value) {
    at[0] = value >> 24;
    at[1] = value >> 16;
    at[2] = value >> 8;
    at[3] = value;
}
static void be16(u8* at, u16 value) {
    at[0] = value >> 8;
    at[1] = value;
}
static void rh_put(RpmHeader* header, const void* bytes, int size) {
    if (header->length + size > header->capacity) {
        header->capacity = (header->length + size) * 2 + 256;
        header->data     = realloc(header->data, header->capacity);
    }
    memcpy(header->data + header->length, bytes, size);
    header->length += size;
}
static void rh_align(RpmHeader* header, int to) {
    u8 zero = 0;
    while (header->length % to) rh_put(header, &zero, 1);
}
static RpmEntry* rh_entry(RpmHeader* header, int tag, int type,
                          int count) {
    RpmEntry* entry = &header->entries[header->count++];
    entry->tag      = tag;
    entry->type     = type;
    entry->offset   = header->length;
    entry->count    = count;
    return entry;
}
static void rh_str(RpmHeader* header, int tag, int type,
                   const char* text) {
    rh_entry(header, tag, type, 1);
    rh_put(header, text, (int)strlen(text) + 1);
}
static void rh_strs(RpmHeader* header, int tag, const char** texts,
                    int count) {
    rh_entry(header, tag, RT_STRINGS, count);
    for (int i = 0; i < count; i++)
        rh_put(header, texts[i], (int)strlen(texts[i]) + 1);
}
static void rh_i32s(RpmHeader* header, int tag, i32* values,
                    int count) {
    rh_align(header, 4);
    rh_entry(header, tag, RT_INT32, count);
    for (int i = 0; i < count; i++) {
        u8 bytes[4];
        be32(bytes, (u32)values[i]);
        rh_put(header, bytes, 4);
    }
}
static void rh_i16s(RpmHeader* header, int tag, i32* values,
                    int count) {
    rh_align(header, 2);
    rh_entry(header, tag, RT_INT16, count);
    for (int i = 0; i < count; i++) {
        u8 bytes[2];
        be16(bytes, (u16)values[i]);
        rh_put(header, bytes, 2);
    }
}
static void rh_bin(RpmHeader* header, int tag, u8* bytes, int count) {
    rh_entry(header, tag, RT_BIN, count);
    rh_put(header, bytes, count);
}
static void rh_write(RpmHeader* header, int region_tag,
                     FILE* out) { // index 0 is the region; its trailer
                                  // closes what the digests cover
    u8 trailer[16];
    be32(trailer, region_tag);
    be32(trailer + 4, RT_BIN);
    be32(trailer + 8, (u32)(-(header->count * 16)));
    be32(trailer + 12, 16);
    header->entries[0].tag    = region_tag;
    header->entries[0].type   = RT_BIN;
    header->entries[0].offset = header->length;
    header->entries[0].count  = 16;
    rh_put(header, trailer, 16);
    u8 magic[8] = {0x8e, 0xad, 0xe8, 0x01, 0, 0, 0, 0}, bytes[16];
    fwrite(magic, 1, 8, out);
    be32(bytes, header->count);
    be32(bytes + 4, header->length);
    fwrite(bytes, 1, 8, out);
    for (int i = 0; i < header->count; i++) {
        be32(bytes, header->entries[i].tag);
        be32(bytes + 4, header->entries[i].type);
        be32(bytes + 8, header->entries[i].offset);
        be32(bytes + 12, header->entries[i].count);
        fwrite(bytes, 1, 16, out);
    }
    fwrite(header->data, 1, header->length, out);
}
typedef struct {
    char        rel[1024];
    struct stat st;
    char        link[1024];
} RpmFile;
typedef struct {
    RpmFile* items;
    int      count, capacity;
} RpmFiles;
static int name_compare(const void* left, const void* right) {
    return strcmp(*(char* const*)left, *(char* const*)right);
}
static void rpm_walk(
    RpmFiles* files, const char* root, const char* rel,
    const char* own_lib,
    const char* own_share) { // files and links, plus the dirs we own
    DIR* dir = opendir(strlen(rel) ? format("%s/%s", root, rel) : root);
    if (!dir) return;
    struct dirent* entry;
    char**         names      = 0;
    int            name_count = 0, name_cap = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        if (name_count == name_cap) {
            name_cap = name_cap * 2 + 32;
            names    = realloc(names, name_cap * sizeof(char*));
        }
        names[name_count++] = strdup(entry->d_name);
    }
    closedir(dir);
    qsort(names, name_count, sizeof(char*),
          name_compare); // sorted: header and payload agree with rpm's
                         // own order
    for (int i = 0; i < name_count; i++) {
        char sub[1024];
        snprintf(sub, sizeof sub, "%s%s%s", rel, strlen(rel) ? "/" : "",
                 names[i]);
        char*       path = format("%s/%s", root, sub);
        struct stat st;
        if (lstat(path, &st) != 0) continue;
        bool is_dir = S_ISDIR(st.st_mode),
             owned  = !strncmp(sub, own_lib, strlen(own_lib)) ||
                     !strncmp(sub, own_share, strlen(own_share));
        if (!is_dir || owned) {
            if (files->count == files->capacity) {
                files->capacity = files->capacity * 2 + 64;
                files->items    = realloc(
                    files->items, files->capacity * sizeof(RpmFile));
            }
            RpmFile* file = &files->items[files->count++];
            snprintf(file->rel, sizeof file->rel, "%s", sub);
            file->st      = st;
            file->link[0] = 0;
            if (S_ISLNK(st.st_mode)) {
                ssize_t got =
                    readlink(path, file->link, sizeof file->link - 1);
                file->link[got > 0 ? got : 0] = 0;
            }
        }
        if (is_dir) rpm_walk(files, root, sub, own_lib, own_share);
    }
}
static void cpio_entry(FILE* out, const char* name, u32 inode, u32 mode,
                       u32 mtime, u32 links, u32 size) {
    u32 name_size = (u32)strlen(name) + 1;
    fprintf(
        out,
        "070701%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
        inode, mode, 0u, 0u, links, mtime, size, 0u, 0u, 0u, 0u,
        name_size, 0u);
    fwrite(name, 1, name_size, out);
    for (u32 pad = (110 + name_size) % 4; pad && pad < 4; pad++)
        fputc(0, out);
}
static void cpio_pad(FILE* out, u32 size) {
    for (u32 pad = size % 4; pad && pad < 4; pad++) fputc(0, out);
}
static void write_rpm(const char* stage, const char* root,
                      const char* lower, const char* version,
                      const char* arch, const char* name,
                      const char* who, const char* rpm) {
    char* work = format("%s/rpm", stage);
    run_shell(format("mkdir -p %s", work));
    RpmFiles files = {0};
    rpm_walk(&files, root, "", format("usr/lib/%s", name),
             format("usr/share/%s", name));
    int count = files.count;
    if (!count) {
        fprintf(stderr, "package: nothing staged for %s\n", name);
        exit(1);
    }
    char* cpio = format(
        "%s/payload.cpio",
        work); // the payload: newc cpio, ./-prefixed names, then gzip
    FILE* payload = fopen(cpio, "wb");
    if (!payload) {
        fprintf(stderr, "package: cannot write %s\n", cpio);
        exit(1);
    }
    long total = 0;
    for (int i = 0; i < count; i++) {
        RpmFile* file    = &files.items[i];
        bool     is_link = S_ISLNK(file->st.st_mode),
             is_dir      = S_ISDIR(file->st.st_mode);
        u32 size         = is_link  ? (u32)strlen(file->link)
                           : is_dir ? 0
                                    : (u32)file->st.st_size;
        cpio_entry(payload, format("./%s", file->rel), (u32)(i + 1),
                   (u32)file->st.st_mode, (u32)file->st.st_mtime,
                   is_dir ? 2 : 1, size);
        if (is_link) fwrite(file->link, 1, size, payload);
        else if (!is_dir)
            copy_into(payload, format("%s/%s", root, file->rel));
        cpio_pad(payload, size);
        total += size;
    }
    cpio_entry(payload, "TRAILER!!!", 0, 0, 0, 1, 0);
    fclose(payload);
    long payload_size = file_size(cpio);
    run_shell(format("gzip -9 -n -f %s", cpio));
    char*     gz     = format("%s/payload.cpio.gz", work);
    RpmHeader header = {0};
    rh_entry(&header, 0, 0, 0);
    const char* i18n[1] = {"C"};
    rh_strs(&header, 100, i18n, 1);
    char* nvr = format("%s-%s-1", lower, version);
    rh_str(&header, 1000, RT_STRING, lower);
    rh_str(&header, 1001, RT_STRING, version);
    rh_str(&header, 1002, RT_STRING, "1");
    rh_str(&header, 1004, RT_I18N, name);
    rh_str(&header, 1005, RT_I18N, format("%s %s", name, version));
    i32 now = (i32)time(0);
    rh_i32s(&header, 1006, &now, 1);
    i32 total_size = (i32)total;
    rh_i32s(&header, 1009, &total_size, 1);
    rh_str(&header, 1014, RT_STRING, "Proprietary");
    rh_str(&header, 1016, RT_I18N, "Applications");
    rh_str(&header, 1021, RT_STRING, "linux");
    rh_str(&header, 1022, RT_STRING, arch);
    i32*         sizes     = calloc(count, 4);
    i32*         modes     = calloc(count, 4);
    i32*         zeros     = calloc(count, 4);
    i32*         mtimes    = calloc(count, 4);
    i32*         vflags    = calloc(count, 4);
    i32*         ones      = calloc(count, 4);
    i32*         inodes    = calloc(count, 4);
    i32*         dir_index = calloc(count, 4);
    const char** digests   = calloc(count, sizeof(char*));
    const char** links     = calloc(count, sizeof(char*));
    const char** users     = calloc(count, sizeof(char*));
    const char** langs     = calloc(count, sizeof(char*));
    const char** bases     = calloc(count, sizeof(char*));
    const char** dirs      = calloc(count, sizeof(char*));
    int          dir_count = 0;
    for (int i = 0; i < count; i++) {
        RpmFile* file    = &files.items[i];
        bool     is_link = S_ISLNK(file->st.st_mode),
             is_dir      = S_ISDIR(file->st.st_mode);
        sizes[i]         = is_link  ? (i32)strlen(file->link)
                           : is_dir ? 0
                                    : (i32)file->st.st_size;
        modes[i]         = (i32)file->st.st_mode;
        mtimes[i]        = (i32)file->st.st_mtime;
        vflags[i]        = -1;
        ones[i]          = 1;
        inodes[i]        = i + 1;
        digests[i] =
            (is_link || is_dir)
                ? ""
                : sum_of("sha256sum", format("%s/%s", root, file->rel));
        links[i] = is_link ? file->link : "";
        users[i] = "root";
        langs[i] = "";
        char full[1100];
        snprintf(full, sizeof full, "/%s", file->rel);
        char* slash    = strrchr(full, '/');
        bases[i]       = strdup(slash + 1);
        char* dir_name = format("%.*s/", (int)(slash - full), full);
        int   at       = -1;
        for (int k = 0; k < dir_count; k++)
            if (same(dirs[k], dir_name)) at = k;
        if (at < 0) {
            at                = dir_count;
            dirs[dir_count++] = dir_name;
        }
        dir_index[i] = at;
    }
    rh_i32s(&header, 1028, sizes, count);
    rh_i16s(&header, 1030, modes, count);
    rh_i16s(&header, 1033, zeros, count);
    rh_i32s(&header, 1034, mtimes, count);
    rh_strs(&header, 1035, digests, count);
    rh_strs(&header, 1036, links, count);
    rh_i32s(&header, 1037, zeros, count);
    rh_strs(&header, 1039, users, count);
    rh_strs(&header, 1040, users, count);
    rh_str(&header, 1044, RT_STRING, format("%s.src.rpm", nvr));
    rh_i32s(&header, 1045, vflags, count);
    const char* provides[1] = {lower};
    rh_strs(&header, 1047, provides, 1);
    i32 require_flags[3] = {
        0x100000A, 0x100000A,
        0x100000A}; // ./-prefixed names, compressed file names, sha256
                    // digests: rpm itself provides these
    const char* require_names[3]    = {"rpmlib(CompressedFileNames)",
                                       "rpmlib(FileDigests)",
                                       "rpmlib(PayloadFilesHavePrefix)"};
    const char* require_versions[3] = {"3.0.4-1", "4.6.0-1", "4.0-1"};
    rh_i32s(&header, 1048, require_flags, 3);
    rh_strs(&header, 1049, require_names, 3);
    rh_strs(&header, 1050, require_versions, 3);
    rh_i32s(&header, 1095, ones, count);
    rh_i32s(&header, 1096, inodes, count);
    rh_strs(&header, 1097, langs, count);
    i32 platform_flags[1] = {8};
    rh_i32s(&header, 1112, platform_flags, 1);
    const char* platform_version[1] = {format("%s-1", version)};
    rh_strs(&header, 1113, platform_version, 1);
    rh_i32s(&header, 1116, dir_index, count);
    rh_strs(&header, 1117, bases, count);
    rh_strs(&header, 1118, dirs, dir_count);
    rh_str(&header, 1124, RT_STRING, "cpio");
    rh_str(&header, 1125, RT_STRING, "gzip");
    rh_str(&header, 1126, RT_STRING, "9");
    i32 algo[1] = {8};
    rh_i32s(&header, 5011, algo, 1);
    char* header_file = format("%s/header.bin", work);
    FILE* header_out  = fopen(header_file, "wb");
    if (!header_out) {
        fprintf(stderr, "package: cannot write %s\n", header_file);
        exit(1);
    }
    rh_write(&header, 63, header_out);
    fclose(header_out);
    char* both = format("%s/header+payload.bin",
                        work); // the signature header: digests of the
                               // main header, and of header+payload
    run_shell(format("cat %s %s > %s", header_file, gz, both));
    char* md5_hex = sum_of("md5sum", both);
    u8    md5[16];
    for (int i = 0; i < 16; i++) {
        unsigned value = 0;
        sscanf(md5_hex + i * 2, "%2x", &value);
        md5[i] = (u8)value;
    }
    RpmHeader signature = {0};
    rh_entry(&signature, 0, 0, 0);
    rh_str(&signature, 269, RT_STRING, sum_of("sha1sum", header_file));
    rh_str(&signature, 273, RT_STRING,
           sum_of("sha256sum", header_file));
    i32 both_size = (i32)file_size(both);
    rh_i32s(&signature, 1000, &both_size, 1);
    rh_bin(&signature, 1004, md5, 16);
    i32 payload_bytes = (i32)payload_size;
    rh_i32s(&signature, 1007, &payload_bytes, 1);
    char* sig_file = format("%s/sig.bin", work);
    FILE* sig_out  = fopen(sig_file, "wb");
    if (!sig_out) {
        fprintf(stderr, "package: cannot write %s\n", sig_file);
        exit(1);
    }
    rh_write(&signature, 62, sig_out);
    for (long at = ftell(sig_out); at % 8; at++)
        fputc(0, sig_out); // the main header starts 8-aligned
    fclose(sig_out);
    FILE* out = fopen(rpm, "wb");
    if (!out) {
        fprintf(stderr, "package: cannot write %s\n", rpm);
        exit(1);
    }
    u8 lead[96] = {0xed, 0xab, 0xee, 0xdb, 3, 0};
    be16(lead + 6, 0);
    be16(lead + 8, (u16)(same(arch, "aarch64") ? 19 : 1));
    snprintf((char*)lead + 10, 66, "%s", nvr);
    be16(lead + 76, 1);
    be16(lead + 78, 5);
    fwrite(lead, 1, 96, out);
    copy_into(out, sig_file);
    copy_into(out, header_file);
    copy_into(out, gz);
    fclose(out);
}
static char* release_version(
    const char* module_dir) { // `export 1.0.0` in the module
    char* version = shell_line(format(
        "sed -n 's/^export  *\\([0-9][0-9.]*\\).*/\\1/p' %s/%s.ag",
        module_dir, S->modname));
    if (!*version) {
        fprintf(stderr,
                "--release: %s exports no version (export 1.0.0)\n",
                S->modname);
        exit(1);
    }
    return version;
}
static void release_package(
    const char* exe_path, const char* install_dir,
    const char* module_dir) { // /usr/bin/<name>, /usr/lib/<name>/,
                              // /usr/share/<name>/, a .desktop, the
                              // icons; then .deb, .rpm, .pkg.tar.zst
    const char* name    = S->modname;
    char*       version = release_version(module_dir);
#if defined(__aarch64__)
    const char* arch     = "aarch64";
    const char* deb_arch = "arm64";
#else
    const char* arch     = "x86_64";
    const char* deb_arch = "amd64";
#endif
    char lower[128];
    int  at = 0;
    for (const char* c = name; *c && at < 127; c++)
        lower[at++] = (char)tolower((unsigned char)*c);
    lower[at]      = 0;
    char* packages = format("%s/packages", module_dir);
    char* stage    = format("%s/tmp/%s-stage", install_dir, name);
    char* root     = format("%s/root", stage);
    char* bin      = format("%s/usr/bin", root);
    char* lib      = format("%s/usr/lib/%s", root, name);
    char* share    = format("%s/usr/share", root);
    char* apps     = format("%s/usr/share/applications", root);
    char* icons    = format("%s/usr/share/icons/hicolor", root);
    run_shell(format("rm -rf %s && mkdir -p %s %s %s %s %s %s", stage,
                     packages, bin, lib, share, apps, icons));
    fprintf(stderr, "[%s] package: staging %s\n", name, root);
    char* exe = format("%s/%s", bin, name);
    run_shell(
        format("cp -L %s %s && chmod u+w %s", exe_path, exe, exe));
    List done = {0};
    bundle_sos(exe, lib, &done);
    for (int i = 0; i < done.count; i++)
        bundle_sos(format("%s/%s", lib, (char*)done.data[i]), lib,
                   &done); // the libraries' own needs too
    char* share_src = format("%s/share/%s", install_dir, name);
    if (!access(share_src, R_OK))
        run_shell(format("cp -RL %s %s/%s", share_src, share, name));
    char* icon =
        format("%s/images/icon.png",
               module_dir); // a release needs an icon: images/icon.png,
                            // icons/icon.png or icon.png
    if (access(icon, R_OK))
        icon = format("%s/icons/icon.png", module_dir);
    if (access(icon, R_OK)) icon = format("%s/icon.png", module_dir);
    if (access(icon, R_OK)) {
        fprintf(stderr, "--release: %s has no icon (images/icon.png)\n",
                name);
        exit(1);
    }
    char* img =
        format("%s/../img/.silver2/install/bin/img",
               module_dir); // img's icons export writes the hicolor
                            // set; SILVER_ICONS tells it what
    if (access(img, X_OK) && run_shell(format("%s %s/../img/img.ag",
                                              S->self_exe, module_dir)))
        exit(1);
    if (run_shell(format("SILVER_ICONS='%s;%s;%s' SILVER_EXPORT=1 %s",
                         icon, icons, name, img))) {
        fprintf(stderr, "package: icons failed for %s\n", name);
        exit(1);
    }
    save_text(
        format("%s/%s.desktop", apps, name),
        format("[Desktop "
               "Entry]\nType=Application\nName=%s\nExec=/usr/bin/"
               "%s\nIcon=%s\nCategories=Utility;\nTerminal=false\n",
               name, name, name));
    char* who_name = shell_line("git config user.name");
    char* who_mail = shell_line("git config user.email");
    char* who      = format("%s <%s>", *who_name ? who_name : "silver",
                       *who_mail ? who_mail : "silver@localhost");
    char* deb      = format("%s/%s_%s_%s.deb", packages, lower, version,
                            deb_arch); // debian / ubuntu
    write_deb(stage, root,
              format("Package: %s\nVersion: %s\nSection: "
                     "misc\nPriority: optional\nArchitecture: "
                     "%s\nMaintainer: %s\nDescription: %s %s\n",
                     lower, version, deb_arch, who, name, version),
              deb);
    fprintf(stderr, "[%s] package: %s\n", name, deb);
    char* rpm = format("%s/%s-%s-1.%s.rpm", packages, lower, version,
                       arch); // fedora / suse
    write_rpm(stage, root, lower, version, arch, name, who, rpm);
    fprintf(stderr, "[%s] package: %s\n", name, rpm);
    char* size = shell_line(format(
        "du -sb %s | cut -f1",
        root)); // arch: pacman reads .PKGINFO first in a zstd tar
    save_text(format("%s/.PKGINFO", root),
              format("pkgname = %s\npkgbase = %s\npkgver = "
                     "%s-1\npkgdesc = %s %s\nurl = \nbuilddate = "
                     "%s\npackager = %s\nsize = %s\narch = %s\n",
                     lower, lower, version, name, version,
                     shell_line("date +%s"), who, size, arch));
    char* arc = format("%s/%s-%s-1-%s.pkg.tar.zst", packages, lower,
                       version, arch);
    if (run_shell(format(
            "tar --zstd --owner=0 --group=0 -C %s -cf %s .PKGINFO usr",
            root, arc))) {
        fprintf(stderr, "package: tar failed for %s\n", arc);
        exit(1);
    }
    fprintf(stderr, "[%s] package: %s\n", name, arc);
    run_shell(format("rm -rf %s", stage));
}

// ----------------------------------------------------------------
// devices: the device IS the sysroot; llvm cross-compiles here, nothing
// is emulated
static bool target_is_android(void) {
    return S->platform && strstr(S->platform, "android");
}
static bool target_is_apple(void) {
    return S->platform &&
           (strstr(S->platform, "ios") || strstr(S->platform, "macos"));
}
static bool target_is_windows(void) {
    return S->platform && strstr(S->platform, "windows");
}
static bool target_is_mobile(void) {
    return target_is_android() ||
           (S->platform && strstr(S->platform, "ios"));
}
static const char* host_arch_name(void) {
#if defined(__aarch64__)
    return "aarch64";
#else
    return "x86_64";
#endif
}
static const char* platform_triple(
    void) { // the platform names the triple, so no triple is ever typed
    const char* p = S->platform ? S->platform : "";
    if (strstr(p, "ios"))
        return strstr(p, "simulator") ? "arm64-apple-ios16.0-simulator"
                                      : "arm64-apple-ios16.0";
    if (strstr(
            p,
            "android")) // the api level is part of the triple; the
                        // emulator runs this machine's own architecture
        return strstr(p, "x86_64") || (strstr(p, "sim") &&
                                       same(host_arch_name(), "x86_64"))
                   ? "x86_64-linux-android33"
                   : "aarch64-linux-android33";
    if (strstr(p, "windows"))
        return strstr(p, "arm64")    ? "aarch64-w64-windows-gnu"
               : strstr(p, "x86_64") ? "x86_64-w64-windows-gnu"
                                     : "i686-w64-windows-gnu";
    if (strstr(p, "mips")) return "mips64el-linux-gnuabi64";
    if (strstr(p, "arm64") || strstr(p, "aarch64") ||
        strstr(p, "jetson"))
        return "aarch64-linux-gnu";
    if (strstr(p, "arm32") || strstr(p, "armv7"))
        return "arm-linux-gnueabihf";
    if (strstr(p, "riscv")) return "riscv64-linux-gnu";
    if (strstr(p, "x86_64")) return "x86_64-linux-gnu";
    if (strstr(p, "x86")) return "i686-linux-gnu";
    return "x86_64-linux-gnu";
}
static const char* android_abi_dir(void) {
    return strstr(S->triple, "x86_64") ? "x86_64-linux-android"
                                       : "aarch64-linux-android";
}
static const char* android_abi(void) {
    return strstr(S->triple, "x86_64") ? "x86_64" : "arm64-v8a";
}
static const char*
abi_clang(void) { // some targets must have their ABI named; the ndk
                  // keys its arch headers by the bare triple
    const char* p = S->platform;
    if (strstr(p, "riscv")) return "-march=rv64gc -mabi=lp64d ";
    if (strstr(p, "android"))
        return format(
            "-isystem %s/usr/include/%s -ftls-model=global-dynamic ",
            S->sysroot, android_abi_dir());
    return "";
}
static const char*
abi_link(void) { // mips crt carries no GNU-stack note; android aligns
                 // to 16k pages and must take the shared libc first
    const char* p = S->platform;
    if (strstr(p, "mips")) return "-Wl,-z,execstack ";
    if (strstr(p, "windows"))
        return "-rtlib=compiler-rt -unwindlib=libunwind ";
    if (strstr(p, "android"))
        return format("-Wl,-z,max-page-size=16384 -L%s/usr/lib/%s/33 "
                      "-L%s/usr/lib/%s ",
                      S->sysroot, android_abi_dir(), S->sysroot,
                      android_abi_dir());
    return "";
}
static const char* cxx_abi(
    void) { // the device sdk's libc++ headers match the libc++ it ships
    if (target_is_apple() || target_is_android())
        return format("-nostdinc++ -isystem %s/usr/include/c++/v1 ",
                      S->sysroot);
    if (target_is_windows()) return "-stdlib=libc++ ";
    return "";
}
static void
write_toolchain_files(void) { // cmake and meson say the target in their
                              // own files; git imports build with them
    bool win           = target_is_windows(),
         ios           = strstr(S->platform, "ios") != 0,
         android       = target_is_android();
    const char* system = win                            ? "Windows"
                         : ios                          ? "iOS"
                         : strstr(S->platform, "macos") ? "Darwin"
                                                        : "Linux";
    const char* proc   = strstr(S->triple, "arm64")     ? "arm64"
                         : strstr(S->triple, "aarch64") ? "aarch64"
                         : strstr(S->triple, "riscv")   ? "riscv64"
                         : strstr(S->triple, "mips")    ? "mips64"
                         : strstr(S->triple, "arm")     ? "arm"
                         : strstr(S->triple, "i686")    ? "i686"
                                                        : "x86_64";
    const char* pic    = win ? "" : "-fPIC ";
    Buf         text   = {0};
    append(&text,
           "# generated by silver2 for device platform "
           "%s\nset(CMAKE_SYSTEM_NAME %s)\nset(CMAKE_SYSTEM_PROCESSOR "
           "%s)\n",
           S->platform, system, proc);
    append(&text,
           "set(CMAKE_C_COMPILER   \"%s/clang\"   CACHE STRING "
           "\"\")\nset(CMAKE_CXX_COMPILER \"%s/clang++\" CACHE STRING "
           "\"\")\nset(CMAKE_SYSROOT      \"%s\" CACHE STRING \"\")\n",
           S->tools, S->tools, S->sysroot);
    append(&text,
           "set(CMAKE_C_FLAGS   \"--target=%s %s%s-w\" CACHE STRING "
           "\"\")\n",
           S->triple, abi_clang(), pic);
    append(&text,
           "set(CMAKE_CXX_FLAGS \"--target=%s %s%s%s-w\" CACHE STRING "
           "\"\")\n",
           S->triple, abi_clang(), pic,
           ios || android        ? cxx_abi()
           : target_is_windows() ? cxx_abi()
                                 : "");
    if (ios)
        append(&text,
               "set(CMAKE_OBJC_FLAGS   \"--target=%s %s-w\" CACHE "
               "STRING \"\")\nset(CMAKE_OBJCXX_FLAGS \"--target=%s "
               "%s%s-w\" CACHE STRING \"\")\n",
               S->triple, pic, S->triple, pic, cxx_abi());
    append(&text,
           "set(CMAKE_EXE_LINKER_FLAGS    \"-fuse-ld=lld %s\" CACHE "
           "STRING \"\")\nset(CMAKE_SHARED_LINKER_FLAGS \"-fuse-ld=lld "
           "%s\" CACHE STRING \"\")\nset(CMAKE_MODULE_LINKER_FLAGS "
           "\"-fuse-ld=lld %s\" CACHE STRING \"\")\n",
           abi_link(), abi_link(), abi_link());
    if (android)
        append(
            &text,
            "set(CMAKE_C_STANDARD_LIBRARIES   \"-lm\" CACHE STRING "
            "\"\")\nset(CMAKE_CXX_STANDARD_LIBRARIES \"-nostdlib++ "
            "-L%s/usr/lib/%s -lc++_shared -lm\" CACHE STRING \"\")\n",
            S->sysroot, android_abi_dir());
    if (ios)
        append(&text,
               "set(CMAKE_OSX_SYSROOT \"%s\" CACHE STRING "
               "\"\")\nset(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "
               "\"\")\nset(CMAKE_OSX_DEPLOYMENT_TARGET 16.0 CACHE "
               "STRING \"\")\n",
               realpath(S->sysroot, 0));
    if (win)
        append(&text,
               "set(CMAKE_RC_COMPILER \"%s/llvm-windres\" CACHE STRING "
               "\"\")\nset(CMAKE_RC_FLAGS \"-D_WIN32%s -DRC_INVOKED "
               "-I%s/include\" CACHE STRING \"\")\n",
               S->tools, strstr(S->triple, "i686") ? "" : " -D_WIN64",
               S->sysroot);
    append(&text,
           "set(CMAKE_FIND_ROOT_PATH "
           "\"${CMAKE_SYSROOT};%s/platform/"
           "%s\")\nset(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM "
           "NEVER)\nset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "
           "ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE "
           "ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n",
           S->silver_root, S->target_dir);
    save_text(format("%s/platform/%s/target.cmake", S->silver_root,
                     S->target_dir),
              text.data);
    const char* msystem = win                 ? "windows"
                          : target_is_apple() ? "darwin"
                          : android           ? "android"
                                              : "linux";
    const char* cpu =
        strstr(S->triple, "arm64") || strstr(S->triple, "aarch64")
            ? "aarch64"
        : strstr(S->triple, "riscv") ? "riscv64"
        : strstr(S->triple, "mips")  ? "mips64"
        : strstr(S->triple, "arm")   ? "arm"
        : strstr(S->triple, "i686")  ? "x86"
                                     : "x86_64";
    save_text(
        format("%s/platform/%s/meson-cross.ini", S->silver_root,
               S->target_dir),
        format(
            "# generated by silver2 for device platform "
            "%s\n[binaries]\nc = ['%s/clang', '--target=%s']\ncpp = "
            "['%s/clang++', '--target=%s']\nar = '%s/llvm-ar'\nstrip = "
            "'%s/llvm-strip'\n[host_machine]\nsystem = "
            "'%s'\ncpu_family = '%s'\ncpu = '%s'\nendian = 'little'\n",
            S->platform, S->tools, S->triple, S->tools, S->triple,
            S->tools, S->tools, msystem, cpu, cpu));
}
static bool ensure_runtime_lib(
    const char* name) { // Au, or one of its C modules, built once for
                        // the target beside its sysroot
    char* root =
        format("%s/platform/%s", S->silver_root, S->target_dir);
    char *lib_dir = format("%s/lib", root),
         *objs    = format("%s/build", root);
    bool apple = target_is_apple(), android = target_is_android();
    const char* ext = apple ? "dylib" : "so";
    if (!access(format("%s/lib%s.%s", lib_dir, name, ext), R_OK))
        return true;
    run_shell(format("mkdir -p %s %s", lib_dir, objs));
    const char* warn =
        "-Wno-write-strings -Wno-incompatible-function-pointer-types "
        "-Wno-compare-distinct-pointer-types "
        "-Wno-deprecated-declarations "
        "-Wno-shift-op-parentheses -Wno-covered-switch-default "
        "-Wno-nullability-completeness -Wno-expansion-to-defined";
    char* inc = format(
        "-I %s/install/include/Au -I %s/src -I %s/install/include -I "
        "%s/platform/native/include -I %s/include",
        S->silver_root, S->silver_root, S->silver_root, S->silver_root,
        root);
    char* base =
        format("%s -fPIC -fvisibility=default -DSILVER='\"%s\"' %s",
               warn, S->silver_root, S->release ? "-O2" : "-g");
    if (same(name, "Au") && (apple || android) &&
        access(format("%s/libffi.a", lib_dir),
               R_OK)) { // libffi is autotools: an out-of-tree configure
                        // against our clang
        fprintf(stderr, "[ffi] building for %s\n", S->triple);
        const char* host =
            apple ? "aarch64-apple-darwin"
                  : format("%s-linux-android",
                           strstr(S->triple, "x86_64") ? "x86_64"
                                                       : "aarch64");
        if (run_shell(
                format("mkdir -p %s/libffi && cd %s/libffi && "
                       "%s/checkout/libffi/configure --host=%s "
                       "--prefix=%s --disable-shared --disable-docs "
                       "--disable-multi-os-directory "
                       "CC='%s/clang %s -fPIC' CXX='%s/clang++ %s "
                       "-fPIC' LD='%s/ld.lld' AR='%s/llvm-ar' "
                       "RANLIB='%s/llvm-ranlib' > /dev/null && make "
                       "-j8 install > /dev/null",
                       objs, objs, S->silver_root, host, root, S->tools,
                       S->tgt, S->tools, S->tgt, S->tools, S->tools,
                       S->tools)))
            return false;
    }
    fprintf(stderr, "[%s] building the runtime for %s\n", name,
            S->triple);
    char* obj = format("%s/%s.o", objs, name);
    if (run_shell(format(
            "%s/clang %s %s -DMODULE='\"%s\"' %s -c %s/src/%s.c -o %s",
            S->tools, S->tgt, base, name, inc, S->silver_root, name,
            obj)))
        return false;
    Buf objects = {0};
    append(&objects, "%s", obj);
    if (same(name, "Au")) { // Au carries the posix layer
        char* posix = format("%s/posix.o", objs);
        if (run_shell(format(
                "%s/clang++ %s %s -std=c++17 %s -DMODULE='\"posix\"' "
                "%s -c %s/src/posix.cc -o %s",
                S->tools, S->tgt, base, cxx_abi(), inc, S->silver_root,
                posix)))
            return false;
        append(&objects, " %s", posix);
    }
    const char* deps = same(name, "Au")
                           ? (android ? "-lffi -llog"
                              : apple ? "-lffi -lc++"
                                      : "-lffi -ldl -lpthread -lm")
                           : "-lAu";
    if (apple)
        return !run_shell(
            format("%s/clang++ %s -fuse-ld=lld -B%s -dynamiclib %s -o "
                   "%s/lib%s.dylib -L%s -L%s/usr/lib %s -install_name "
                   "@rpath/lib%s.dylib",
                   S->tools, S->tgt, S->tools, objects.data, lib_dir,
                   name, lib_dir, S->sysroot, deps, name));
    return !run_shell(
        format("%s/clang++ %s %s -shared -Wl,-soname,lib%s.so %s -o "
               "%s/lib%s.so -L%s %s",
               S->tools, S->tgt, S->ldld, name, objects.data, lib_dir,
               name, lib_dir, deps));
}
static char*
interpolate_sysroot(const char* line) { // {sysroot} in a fetch: line
    Buf out = {0};
    for (const char* at = line; *at;) {
        if (!strncmp(at, "{sysroot}", 9)) {
            append(&out, "%s", S->sysroot);
            at += 9;
        } else append(&out, "%c", *at++);
    }
    return out.data ? out.data : strdup("");
}
static char* silver_root_of(
    void) { // the tree above the executable that holds src/Au.c:
            // install may be a symlink into platform/native
    char* root = strdup(S->self_exe);
    for (int i = 0; i < 6; i++) {
        *strrchr(root, '/') = 0;
        if (!access(format("%s/src/Au.c", root), R_OK)) return root;
    }
    return root;
}
static bool
device_prepare(void) { // the target's name, its sysroot (pulled once),
                       // the flags every clang call takes, its runtime
    S->target_dir  = S->device.alias ? S->device.alias : S->platform;
    char* root     = silver_root_of();
    S->silver_root = root;
    S->tools       = format("%s/platform/native/bin", root);
    S->sysroot = format("%s/platform/%s/sysroot", root, S->target_dir);
    S->triple  = platform_triple();
    bool laid_out = !access(format("%s/usr", S->sysroot), R_OK) ||
                    !access(format("%s/include", S->sysroot), R_OK);
    if (S->rsync || !laid_out) {
        run_shell(format("mkdir -p %s", S->sysroot));
        if (!S->device.sysroot && !S->device.fetch) {
            fprintf(stderr,
                    "device '%s': add a sysroot: line naming the paths "
                    "to pull, or a fetch: command\n",
                    S->target_dir);
            return false;
        }
        if (S->device.fetch) { // a tarball transfers in full either
                               // way; a refresh starts clean
            if (S->rsync)
                run_shell(format("rm -rf %s && mkdir -p %s", S->sysroot,
                                 S->sysroot));
            fprintf(stderr, "[%s] fetching sysroot\n", S->target_dir);
            char* lines = strdup(S->device.fetch);
            for (char* line = strtok(lines, "\n"); line;
                 line       = strtok(0, "\n")) {
                if (!*line) continue;
                if (run_shell(interpolate_sysroot(line))) {
                    fprintf(stderr, "[%s] sysroot fetch failed: %s\n",
                            S->target_dir, line);
                    return false;
                }
            }
        } else { // no host means this machine; a glob that matches
                 // nothing is not an error
            Buf skip = {0};
            if (S->device.exclude) {
                char* pats = strdup(S->device.exclude);
                for (char* pat = strtok(pats, " "); pat;
                     pat       = strtok(0, " "))
                    append(&skip, "--exclude='%s' ", pat);
            }
            fprintf(stderr, "[%s] pulling sysroot from %s\n",
                    S->target_dir,
                    S->device.host ? S->device.host : "this machine");
            char* source = S->device.host
                               ? format("%s:'%s'", S->device.host,
                                        S->device.sysroot)
                               : strdup(S->device.sysroot);
            if (run_shell(format(
                    "rsync -a --relative --copy-unsafe-links "
                    "--ignore-missing-args --delete %s%s %s/",
                    skip.data ? skip.data : "", source, S->sysroot))) {
                fprintf(stderr,
                        "sysroot pull failed for device '%s' — is "
                        "rsync present on both ends?\n",
                        S->target_dir);
                return false;
            }
        }
        if (access(format("%s/usr", S->sysroot), R_OK) &&
            access(format("%s/include", S->sysroot), R_OK)) {
            fprintf(stderr, "device '%s': nothing landed in %s\n",
                    S->target_dir, S->sysroot);
            return false;
        }
        if (access(format("%s/lib", S->sysroot), R_OK))
            run_shell(format(
                "ln -sfn usr/lib %s/lib",
                S->sysroot)); // merged-usr: libc.so's script names /lib
        if (access(format("%s/lib64", S->sysroot), R_OK) &&
            !access(format("%s/usr/lib64", S->sysroot), R_OK))
            run_shell(format("ln -sfn usr/lib64 %s/lib64", S->sysroot));
    }
    bool  apple = target_is_apple();
    char* arch_inc =
        target_is_windows() || apple || target_is_android()
            ? strdup("")
            : format("-isystem %s/usr/include/%s ", S->sysroot,
                     S->triple); // debian keeps arch headers at
                                 // usr/include/<triple>
    S->tgt  = format("-target %s %s%s %s%s", S->triple,
                    apple ? "-isysroot " : "--sysroot=", S->sysroot,
                     arch_inc, abi_clang());
    S->ldld = format("-fuse-ld=lld -B%s %s", S->tools, abi_link());
    fprintf(stderr, "[%s] cross-compiling %s against %s\n",
            S->target_dir, S->triple, S->sysroot);
    write_toolchain_files();
    return ensure_runtime_lib("Au");
}
static const char* bundle_version(const char* module_dir) {
    char* v = shell_line(format(
        "sed -n 's/^export  *\\([0-9][0-9.]*\\).*/\\1/p' %s/%s.ag",
        module_dir, S->modname));
    return *v ? v : "1.0";
}
static const char*
devices_lib(const char* ext) { // the devices module built for the
                               // target: the host's window and loop
    char* lib = format(
        "%s/devices/.silver2/%s/install/lib/libsilver-devices.%s",
        S->silver_root, S->target_dir, ext);
    if (access(lib, R_OK) &&
        !build_import(format("%s/devices/devices.ag", S->silver_root),
                      0, true))
        exit(1);
    return lib;
}
#include "silver2_android.c"
#include "silver2_ios.c"
static char* mobile_bundle(const char* product, const char* install_dir,
                           const char* module_dir) {
    return target_is_android()
               ? android_bundle(product, install_dir, module_dir)
               : ios_bundle(product, install_dir, module_dir);
}
static int device_run(
    const char* product) { // push to the device and start it there; a
                           // device with no host is a build target only
    const char* name = S->modname;
    const char* host = S->device.host;
    if (target_is_android()) return android_run();
    if (!host) return 0;
    if (strstr(S->platform, "ios")) return ios_run();
    const char* root = S->device.root ? S->device.root : "~/silver";
    fprintf(stderr, "[%s] sending to %s\n", name, host);
    if (run_shell(format("ssh %s 'mkdir -p %s'", host, root)) ||
        run_shell(format("rsync -az %s %s:%s/", product, host, root))) {
        fprintf(stderr, "[%s] cannot reach device '%s' over ssh (%s)\n",
                name, S->target_dir, host);
        return 1;
    }
    run_shell(format("ssh %s '%s'", host,
                     S->device.stop
                         ? S->device.stop
                         : format("pkill -x %s 2>/dev/null; true",
                                  name))); // whatever runs there now is
                                           // the previous build
    fprintf(stderr, "[%s] starting on %s\n", name, host);
    return run_shell(format("ssh %s '%s'", host,
                            S->device.run
                                ? S->device.run
                                : format("cd %s && ./%s", root,
                                         strrchr(product, '/') + 1)));
}
static int device_debug(
    const char* product) { // --lldb: lldb here on the product, or lldb
                           // here driving lldb-server there over ssh
    char* tool =
        format("%s/platform/native/bin/lldb",
               S->silver_root ? S->silver_root : silver_root_of());
    if (access(tool, X_OK)) {
        fprintf(stderr, "[%s] no lldb at %s\n", S->modname, tool);
        return 1;
    }
    if (S->cross && S->device.host) {
        const char *host = S->device.host,
                   *root = S->device.root ? S->device.root : "~/silver",
                   *port = "1234";
        if (run_shell(format("ssh %s 'mkdir -p %s'", host, root)) ||
            run_shell(
                format("rsync -az %s %s:%s/", product, host, root))) {
            fprintf(stderr, "[%s] cannot reach device '%s' over ssh\n",
                    S->modname, S->target_dir);
            return 1;
        }
        const char* server =
            S->device.debugger
                ? S->device.debugger
                : format("lldb-server platform --listen 127.0.0.1:%s "
                         "--server",
                         port); // the debug wire rides ssh: loopback
                                // there, one forwarded port here
        run_shell(format(
            "ssh %s 'pkill -f lldb-server 2>/dev/null; true'", host));
        if (run_shell(format("ssh -f %s '%s >/dev/null 2>&1 &'", host,
                             server))) {
            fprintf(
                stderr,
                "[%s] no debug server on %s — install lldb-server "
                "there, or name one in the device's debugger: line\n",
                S->modname, host);
            return 1;
        }
        run_shell(format(
            "pkill -f 'ssh -f -N -L %s:127.0.0.1:%s' 2>/dev/null; true",
            port, port));
        if (run_shell(format("ssh -f -N -L %s:127.0.0.1:%s %s", port,
                             port, host))) {
            fprintf(stderr,
                    "[%s] could not forward the debug port from %s\n",
                    S->modname, host);
            return 1;
        }
        char* script = format("%s/%s.lldb", S->out_dir, S->modname);
        save_text(script,
                  format("platform select remote-linux\nplatform "
                         "connect connect://127.0.0.1:%s\nsettings set "
                         "target.sysroot %s\ntarget create %s\n",
                         port, S->sysroot, product));
        fprintf(stderr, "[%s] debugging on %s\n", S->modname, host);
        execlp(tool, tool, "-s", script, (char*)0);
        return 1;
    }
    fprintf(stderr, "[%s] debugging here\n", S->modname);
    execlp(tool, tool, product, (char*)0);
    return 1;
}
SilverState* silver2_state(void) {
    return calloc(1, sizeof(SilverState));
}
int silver2_compile(
    SilverState*   state,
    SilverOptions* options) { // the silver object's run: one module,
                              // its imports, tests
    S                       = state;
    const char* module_path = options->module;
    bool        run_tests   = options->test;
    S->lib_mode             = options->lib;
    S->release              = options->release;
    S->coverage             = options->coverage;
    S->timing               = options->timing;
    S->build                = options->build;
    S->clean                = options->clean;
    S->rsync                = options->rsync;
    S->lldb                 = options->lldb;
    S->self_exe             = realpath("/proc/self/exe", 0);
    for (int i = 0; i < options->extend_count; i++)
        list_push(&S->extends, (void*)options->extend_paths[i]);
    S->cur_file = module_path;
    if (options->device)
        memcpy(&S->device, options->device, sizeof S->device);
    S->platform =
        options->device ? options->device->platform : options->platform;
    if (S->platform && (!*S->platform || same(S->platform, "native")))
        S->platform = 0;
    S->cross = S->platform != 0;
    if (S->cross && !device_prepare())
        return 1; // the sysroot, the toolchain files, the runtime for
                  // the target
    char* module_dir = strdup(module_path);
    char* slash      = strrchr(module_dir, '/');
    if (slash) *slash = 0;
    else module_dir = strdup(".");
    char* base_name = strdup(slash ? slash + 1 : module_path);
    char* dot       = strrchr(base_name, '.');
    if (dot) *dot = 0;
    S->modname = base_name;
    char* out_dir =
        S->cross ? format("%s/.silver2/%s", module_dir, S->target_dir)
                 : format("%s/.silver2",
                          module_dir); // a target keeps its own tree
    S->out_dir = out_dir;
    if (S->clean) run_shell(format("rm -rf %s", out_dir));
    char* bin_dir  = format("%s/install/bin", out_dir);
    char* exe_path = format("%s/%s", bin_dir, S->modname);
    run_shell(format("mkdir -p %s/imports %s/install/export "
                     "%s/install/bin %s/install/lib %s/install/share",
                     out_dir, out_dir, out_dir, out_dir, out_dir));
    char* install_dir = format("%s/install", out_dir);
    list_push(&S->share_dirs, S->modname);
    list_push(&S->share_dirs, module_dir);
    bool mobile =
        S->cross && target_is_mobile(); // a phone runs the product as a
                                        // library under its own host
    bool  as_lib  = S->lib_mode || mobile;
    char* product = as_lib ? format("%s/install/lib/libsilver-%s.so",
                                    out_dir, S->modname)
                           : exe_path;
    long  newest  = newest_source(module_dir);
    for (int i = 0; i < S->extends.count; i++) {
        long m = mtime_of(S->extends.data[i]);
        if (m > newest) newest = m;
    }
    long  product_m = mtime_of(product);
    char* ledger    = format("%s/%s.imports", out_dir, S->modname);
    if (!S->clean && !S->coverage && !S->timing && product_m &&
        product_m > newest && product_m > mtime_of(S->self_exe) &&
        !access(ledger, R_OK)) { // the product is newer than every
                                 // source here: the imports still
                                 // build (each caches itself)
        fprintf(stderr, "[%s] cached\n", S->modname);
        char* text = read_file(ledger);
        for (char* line = strtok(text, "\n"); line;
             line       = strtok(0, "\n")) {
            char* tab = strchr(line, '\t');
            if (tab) *tab = 0;
            char* extend_path = tab && tab[1] ? tab + 1 : 0;
            if (S->cross && !build_import(line, extend_path, true))
                return 1;
            if (!build_import(line, extend_path, false)) return 1;
        }
        goto deploy;
    }
    au_type("Au");
    Node* module_node = new_node(S_BLOCK, "module", 0);
    S->main_dir       = module_dir;
    load_module(module_path, module_dir, module_node);
    for (int i = 0; i < S->extends.count; i++) {
        char* extend_path  = S->extends.data[i];
        char* extend_dir   = strdup(extend_path);
        char* extend_slash = strrchr(extend_dir, '/');
        if (extend_slash) *extend_slash = 0;
        else extend_dir = strdup(".");
        load_module(extend_path, extend_dir, module_node);
    }
    for (int i = 0; i < module_node->count; i++)
        if (module_node->kids[i]->kind == D_BROKEN)
            fprintf(stderr, "%s\n", module_node->kids[i]->error);
    FILE* ledger_file = fopen(ledger, "w");
    if (ledger_file) {
        fputs(S->import_ledger.data ? S->import_ledger.data : "",
              ledger_file);
        fclose(ledger_file);
    }
    Buf registry = {0};
    for (int i = 0; i < module_node->count; i++)
        if (module_node->kids[i]->kind == N_RAW &&
            strstr(module_node->kids[i]->text, "export extensions"))
            append(&registry, "%s\n", module_node->kids[i]->text + 7);
    if (registry.count) {
        FILE* file = fopen(
            format("%s/export/silver-%s.agi", install_dir, S->modname),
            "w");
        fputs(registry.data, file);
        fclose(file);
    }
    emit_module(module_node, module_dir);
    char* bitcode = format("%s/%s.bc", out_dir, S->modname);
    LLVMWriteBitcodeToFile(S->ir.module, bitcode);
    write_headers(module_node, out_dir);
    char* self_path  = strdup(S->self_exe);
    char* self_dir   = self_path ? strdup(self_path) : strdup(".");
    char* self_slash = strrchr(self_dir, '/');
    if (self_slash) *self_slash = 0;
    Buf         command = {0};
    const char* level =
        S->release ? "-O2 -g"
                   : "-O0 -g"; // debug is the default; a release
                               // optimizes and keeps the line tables
    const char* tgt  = S->cross ? S->tgt : "";
    const char* ldld = S->cross ? S->ldld : "";
    const char* cc = S->cross ? format("%s/clang", S->tools) : "clang";
    const char* cxx =
        S->cross ? format("%s/clang++", S->tools) : "clang++";
    if (as_lib)
        append(&command,
               "%s %s -w %s %s -shared -fPIC "
               "-Wl,-soname,libsilver-%s.so -o %s %s",
               cc, tgt, level, ldld, S->modname, product, bitcode);
    else
        append(&command,
               "%s %s -w %s %s -o %s %s "
               "-Wl,-rpath,'$ORIGIN/../lib/%s'",
               cc, tgt, level, ldld, exe_path, bitcode,
               S->modname); // a packaged exe finds its closure in
                            // /usr/lib/<name>
    if (S->cross)
        append(&command,
               " -L%s/platform/%s/lib -lAu -Wl,-rpath,'$ORIGIN' "
               "-Wl,-rpath,'$ORIGIN/../lib'%s",
               S->silver_root, S->target_dir,
               S->link_flags.data ? S->link_flags.data : "");
    else
        append(&command, " -L%s/../lib -lAu -Wl,-rpath,%s/../lib%s",
               self_dir, self_dir,
               S->link_flags.data ? S->link_flags.data : "");
    char* c_file = format("%s/%s.c", module_dir, S->modname);
    if (!access(c_file, R_OK)) append(&command, " %s", c_file);
    append(&command, "%s",
           S->extra_srcs.data ? S->extra_srcs.data : "");
    char* cxx_file = format("%s/%s.cc", module_dir, S->modname);
    if (!access(cxx_file, R_OK)) {
        if (run_shell(
                format("%s %s -w -c -fPIC -I%s/install/include/%s "
                       "-I%s/install/include -I%s/../include "
                       "-I%s/../include/Au%s -o %s/cc.o %s",
                       cxx, tgt, out_dir, S->modname, out_dir, self_dir,
                       self_dir, S->c_flags.data ? S->c_flags.data : "",
                       out_dir, cxx_file)))
            return 1;
        append(&command, " %s/cc.o -lstdc++", out_dir);
    }
    char* rust_file = format("%s/%s.rs", module_dir, S->modname);
    if (!access(rust_file, R_OK) &&
        run_shell(format("rustc --crate-type staticlib -O -o "
                         "%s/librs.a %s 2>/dev/null",
                         out_dir, rust_file)) == 0)
        append(&command, " %s/librs.a", out_dir);
    append(&command, " -lm %s -Wl,--unresolved-symbols=ignore-all",
           S->cross && target_is_android()
               ? ""
               : "-lpthread -fno-plt -Wl,-z,now");
    if (run_shell(command.data)) return 1;
deploy:
    for (int i = 0; i + 1 < S->share_dirs.count; i += 2)
        deploy_share(S->share_dirs.data[i], S->share_dirs.data[i + 1],
                     install_dir);
    if (S->lib_mode) return 0;
    if (S->cross) { // nothing of the target runs here: bundle for a
                    // phone, send to a board, or stop at the build
        if (mobile) mobile_bundle(product, install_dir, module_dir);
        if (S->build) return 0;
        if (S->lldb) return device_debug(mobile ? product : exe_path);
        return device_run(mobile ? product : exe_path);
    }
    run_shell(
        format("SILVER_EXPORT=1 %s/%s", bin_dir,
               S->modname)); /* launched from the caller's directory:
                                that is the product's startup path */
    if (S->lldb) return device_debug(exe_path);
    if (S->release) { // the gate, then the packages; a packaged release
                      // does not launch
        if (run_shell(
                format("SILVER_EXPECT=1 %s/%s", bin_dir, S->modname))) {
            fprintf(stderr, "release: expect tests failed for %s\n",
                    S->modname);
            return 1;
        }
        release_package(exe_path, install_dir, module_dir);
        return 0;
    }
    if (run_tests)
        return run_shell(
            format("SILVER_EXPECT=1 %s/%s", bin_dir, S->modname));
    return 0;
}

// ----------------------------------------------------------------
// the silver object: Au reads the command line into its publics, then
// init compiles. The generated headers come in here, below the compiler
#include <import>

define_class(Device, Au) define_class(silver, Au)

    static const char* text_of(Au value) {
    string s = (string)instanceof(value, string);
    return s ? s->chars : 0;
}

// devices.agi is the user's: looked up where the command was typed,
// then at the silver root
static bool read_device(const char* alias, DeviceInfo* info,
                        const char* silver_root) {
    path file = f(path, "%o/devices.agi", path_startup());
    if (!file_exists("%o", file))
        file = f(path, "%s/devices.agi", silver_root);
    if (!file_exists("%o", file)) {
        fprintf(stderr, "no devices.agi here or in %s\n", silver_root);
        return false;
    }
    string text  = (string)load(file, typeid(string), null);
    map    all   = (map)parse_agi(typeid(map), text->chars, null);
    Au     entry = all ? get(all, (Au)string(alias)) : null;
    if (!entry) {
        fprintf(stderr, "device '%s' not found in %o\n", alias, file);
        return false;
    }
    info->alias = alias;
    Device dev  = (Device)instanceof(entry, Device);
    if (dev) {
        info->host     = dev->host ? dev->host->chars : 0;
        info->platform = dev->platform ? dev->platform->chars : 0;
        info->root     = dev->root ? dev->root->chars : 0;
        info->run      = dev->run ? dev->run->chars : 0;
        info->stop     = dev->stop ? dev->stop->chars : 0;
        info->sysroot  = dev->sysroot ? dev->sysroot->chars : 0;
        info->exclude  = dev->exclude ? dev->exclude->chars : 0;
        info->fetch    = dev->fetch ? dev->fetch->chars : 0;
        info->debugger = dev->debugger ? dev->debugger->chars : 0;
    } else { // an untyped block: take the fields
        map fields = (map)instanceof(entry, map);
        if (!fields) {
            fprintf(stderr, "device '%s': expected a Device block\n",
                    alias);
            return false;
        }
        info->host     = text_of(get(fields, (Au)string("host")));
        info->platform = text_of(get(fields, (Au)string("platform")));
        info->root     = text_of(get(fields, (Au)string("root")));
        info->run      = text_of(get(fields, (Au)string("run")));
        info->stop     = text_of(get(fields, (Au)string("stop")));
        info->sysroot  = text_of(get(fields, (Au)string("sysroot")));
        info->exclude  = text_of(get(fields, (Au)string("exclude")));
        info->fetch    = text_of(get(fields, (Au)string("fetch")));
        info->debugger = text_of(get(fields, (Au)string("debugger")));
    }
    if (!info->platform) {
        fprintf(stderr, "device '%s' has no platform\n", alias);
        return false;
    }
    return true;
}

none silver_init(silver a) { // Au has read the command line
    a->state              = (ARef)silver2_state();
    SilverOptions options = {0};
    DeviceInfo    device  = {0};
    const char*   extends[1];
    options.module   = a->module->chars;
    options.platform = a->platform ? a->platform->chars : 0;
    options.test     = a->test;
    options.lib      = a->lib;
    options.build    = a->build;
    options.clean    = a->clean;
    options.coverage = a->coverage;
    options.timing   = a->timing;
    options.release  = a->release && !a->debug;
    options.rsync    = a->rsync;
    options.lldb     = a->lldb;
    if (a->extend) {
        extends[0]           = a->extend->chars;
        options.extend_paths = extends;
        options.extend_count = 1;
    }
    if (a->device) {
        char* root = realpath(
            "/proc/self/exe",
            0); // the tree above the executable that holds src/Au.c
        for (int i = 0; i < 6; i++) {
            *strrchr(root, '/') = 0;
            if (!access(((path)f(path, "%s/src/Au.c", root))->chars,
                        R_OK))
                break;
        }
        if (!read_device(a->device->chars, &device, root)) {
            a->error = true;
            return;
        }
        options.device = &device;
    }
    a->error = silver2_compile((SilverState*)a->state, &options) != 0;
}

static int build_import(const char* module_path,
                        const char* extend_path,
                        int for_target) { // an import is another
                                          // silver object, run
                                          // in this process
    SilverState* up = S;
    silver       dep =
        silver(module, path(module_path), lib, true, clean, up->clean,
               coverage, up->coverage, timing, up->timing, release,
               up->release, extend,
               extend_path ? path(extend_path) : (path)null, device,
               for_target && up->device.alias ? string(up->device.alias)
                                              : (string)null,
               platform,
               for_target && !up->device.alias && up->platform
                   ? string(up->platform)
                   : (string)null);
    S                  = up;
    SilverState* child = (SilverState*)dep->state; // its folders and
                                                   // its imports' join
                                                   // our share
    for (int i = 0; i + 1 < child->share_dirs.count; i += 2) {
        bool have = false;
        for (int k = 0; k + 1 < up->share_dirs.count; k += 2)
            if (!strcmp(up->share_dirs.data[k + 1],
                        child->share_dirs.data[i + 1]))
                have = true;
        if (have) continue;
        list_push(&up->share_dirs, child->share_dirs.data[i]);
        list_push(&up->share_dirs, child->share_dirs.data[i + 1]);
    }
    return !dep->error;
}

int main(int argc, cstrs argv) {
    engage(argv);
    silver a = silver(argv);
    return (a && a->error) ? 1 : 0;
}
