#include <tokens>
#include <silver>

static array keywords;
static array consumables;
static array assign;
static array compare;

#define intern(I,M,...) Token_ ## M(I, ## __VA_ARGS__)

void init() {
    keywords = array_of_cstr(
        "class",  "proto",    "struct", "import", "typeof", "schema", "is", "inherits",
        "init",   "destruct", "ref",    "const",  "volatile",
        "return", "asm",      "if",     "switch",
        "while",  "for",      "do",     "signed", "unsigned", "cast", null);
    consumables = array_of_cstr(
        "ref", "schema", "enum", "class", "union", "proto", "struct",
        "const", "volatile", "signed", "unsigned", null);
    assign  = array_of_cstr(":", "=" , "+=",  "-=", "*=",  "/=", "|=",
                "&=", "^=", ">>=", "<<=", "%=", null);
    compare = array_of_cstr("==", "!=", null);
}

bool next_is(Tokens tokens, symbol cs) {
    return call(tokens, next_is, cs);
}

/// Token
void Token_init(Token a) {
    cstr prev = a->chars;
    sz length = prev ? strlen(prev) : 0;
    if (prev) {
        a->chars  = (cstr)calloc(length + 1, 1);
        a->len    = length;
        memcpy(a->chars, prev, length);
    } else if (a->chr) {
        a->chars = (cstr)calloc(2, 1);
        a->chars[0] = a->chr;
        a->len   = 1;
    } else {
        assert (false, "required: chars or chr");
    }
}

Token Token_with_i32(Token a, i32 chr, Loc loc) {
    a->chars    = (cstr)calloc(2, 1);
    a->chars[0] = chr;
    a->len      = 1;
    a->loc    = A_hold(loc);
    return a;
}

bool Token_eq(Token a, cstr cs) {
    return strcmp(a->chars, cs) == 0;
}

num Token_cmp(Token a, cstr cs) {
    return strcmp(a->chars, cs);
}

string Token_cast_string(Token a) {
    return new(string, chars, a->chars);
}

AType Token_is_bool(Token a) {
    string t = cast(a, string);
    return (call(t, cmp, "true") || call(t, cmp, "false")) ?
        (AType)typeid(ELiteralBool) : (AType)typeid(EUndefined);
}

A Token_is_numeric(Token a) {
    bool is_digit = a->chars[0] >= '0' && a->chars[0] <= '9';
    bool has_dot  = strstr(a->chars, ".") != 0;
    if (!is_digit && !has_dot)
        return null;
    char* e = null;
    if (!has_dot) {
        i64 v = strtoll(a->chars, &e, 10);
        return A_primitive(typeid(i64), &v);
    }
    f64 v = strtod(a->chars, &e);
    return A_primitive(typeid(f64), &v);
}

AType Token_is_string(Token a) {
    char t = a->chars[0];
    if (t == '"' || t == '\'') return typeid(ELiteralStr);
    return typeid(EUndefined);
}

string Token_convert_literal(Token a) {
    assert(call(a, is_string) == typeid(ELiteralStr), "not given a string literal");
    string entire = str(a->chars);
    string result = call(entire, mid, 1, a->len - 2);
    return result;
}

num Token_compare(Token a, Token b) {
    return strcmp(a->chars, b->chars);
}

bool Token_cast_bool(Token a) {
    return a->len > 0;
}

bool is_alpha(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string)) {
        s = any;
    } else if (type == typeid(Token)) {
        Token token = any;
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

/// Tokens
array parse_tokens(A input) {
    string input_string;
    AType  type = isa(input);
    path   file = null;
    if (type == typeid(path)) {
        file = input;
        input_string = call(file, read, typeid(string));
    } else if (type == typeid(string))
        input_string = input;
    else
        assert(false, "can only parse from path");
    
    string  special_chars   = str(".$,<>()![]/+*:=#");
    array   tokens          = new(array, alloc, 128);
    num     line_num        = 1;
    num     length          = len(input_string);
    num     index           = 0;

    while (index < length) {
        i32 chr = idx(input_string, index);
        
        if (isspace(chr)) {
            if (chr == '\n')
                line_num += 1;
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
        if (call(special_chars, index_of, sval) >= 0) {
            Loc lc = new(Loc, source, file, line, line_num, column, 0);
            if (chr == ':' && idx(input_string, index + 1) == ':') {
                call(tokens, push, new(Token, chars, "::", loc, lc));
                index += 2;
            } else if (chr == '=' && idx(input_string, index + 1) == '=') {
                call(tokens, push, new(Token, chars, "==", loc, lc));
                index += 2;
            } else {
                call(tokens, push, new(Token, chr, chr, loc, lc));
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
            string crop    = call(input_string, mid, start, index - start);
            Loc    lc      = new(Loc,
                source, file,
                line,   line_num,
                column, start);
            Token token    = new(Token, chars, crop->chars, loc, lc);
            call(tokens, push, token);
            continue;
        }

        num start = index;
        while (index < length) {
            i32 v = idx(input_string, index);
            char sval[2] = { v, 0 };
            if (isspace(v) || call(special_chars, index_of, sval) >= 0)
                break;
            index += 1;
        }
        
        Loc    lc   = new(Loc, source, file, line, line_num, column, start);
        string crop = call(input_string, mid, start, index - start);
        call(tokens, push, new(Token, chars, crop->chars, loc, lc));
    }
    return tokens;
}

none Tokens_init(Tokens a) {
    if (a->file)
        a->tokens = parse_tokens(a->file);
    else if (!a->tokens)
        assert (false, "file/tokens not set");
    a->stack = new(array, alloc, 4);
}

object Tokens_read_numeric(Tokens tokens) {
    Token first = call(tokens, peek);
    object    n = intern(first, is_numeric);
    if (n)
        call(tokens, consume);
    return n;
}

// does not read past the type into [ model-query ]
type Tokens_read_type(Tokens tokens, silver module) {
    bool is_ref = false;
    Token first = call(tokens, peek);
    if (call(first, eq, "ref")) {
        call(tokens, consume);
        is_ref = true;
        first  = call(tokens, peek);
    }
    string  key = cast(first, string);
    type    def = call(module->defs, get, key);
    assert(def || !is_ref, "type-identifier expected after ref keyword, found: %o", key);
    if (def)
        call(tokens, consume);
    return def;
}

Token Tokens_read(Tokens a, num rel) {
    return a->tokens->elements[a->cursor + rel];
}

Token Tokens_next(Tokens a) {
    if (a->cursor >= len(a->tokens))
        return null;
    Token res = call(a, read, 0);
    a->cursor++;
    return res;
}

Token Tokens_consume(Tokens a) {
    return Tokens_next(a);
}

Token Tokens_peek(Tokens a) {
    return call(a, read, 0);
}

bool Tokens_next_is(Tokens a, symbol cs) {
    Token n = call(a, read, 0);
    return strcmp(n->chars, cs) == 0;
}

bool Tokens_next_alpha(Tokens a) {
    Token n = call(a, read, 0);
    return is_alpha(n);
}

typedef struct tokens_data {
    array tokens;
    num   cursor;
} *tokens_data;

void Tokens_push_state(Tokens a, array tokens, num cursor) {
    tokens_data state = A_struct(tokens_data);
    state->tokens = tokens;
    state->cursor = cursor;
    call(a->stack, push, state);
}

void Tokens_pop(Tokens a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data state = (tokens_data)call(a->stack, last); // we should call this element or ele
    call(a->stack, pop);
    if(!transfer)
        a->cursor = state->cursor;
}

void Tokens_push_current(Tokens a) {
    call(a, push_state, a->tokens, a->cursor);
}

bool Tokens_cast_bool(Tokens a) {
    return a->cursor < len(a->tokens) - 1;
}

bool ENode_equals(ENode a, object b) {
    return (A)a == (A)b;
}

bool ENode_cast_bool(ENode a) {
    return a->type || cast(a->name, bool);
}

string ENode_emit(ENode a) {
    fault("emit not implemented for ENode %s", isa(a)->name);
    return null;
}

void ENode_init(ENode a) {
}

void Loc_init(Loc a) {
}

define_class(ENode)
define_class(EUndefined)

define_mod(ELiteralBool, ENode);
define_mod(ELiteralStr,  ENode);
define_mod(ELiteralInt,  ENode);
define_mod(ELiteralReal, ENode);

define_enum(Visibility)

define_class(Token)
define_class(Loc)
define_class(Tokens)

module_init(init)