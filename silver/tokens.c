#include <tokens>
#include <silver>

static array keywords;
static array consumables;
static map   assign;
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
    //assign  = array_of_cstr(":", "=" , "+=",  "-=", "*=",  "/=", "|=",
    //            "&=", "^=", ">>=", "<<=", "%=", null);
    assign = map_of(
        ":",   allocate(string, chars, "assign"),
        "=",   allocate(string, chars, "assign_const"),
        "+=",  allocate(string, chars, "assign_add"),
        "-=",  allocate(string, chars, "assign_sub"),
        "*=",  allocate(string, chars, "assign_mul"),
        "/=",  allocate(string, chars, "assign_div"),
        "|=",  allocate(string, chars, "assign_or"),
        "&=",  allocate(string, chars, "assign_and"),
        "^=",  allocate(string, chars, "assign_xor"),
        ">>=", allocate(string, chars, "assign_right"),
        "<<=", allocate(string, chars, "assign_left"),
        "%=",  allocate(string, chars, "assign_mod"),
    null);
    compare = array_of_cstr("==", "!=", null);
}

bool next_is(Tokens tokens, symbol cs) {
    return call(tokens, next_is, cs);
}

string Token_op_name(string op) {
    return get(assign, op);
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
    return (cmp(t, "true") || cmp(t, "false")) ?
        (AType)typeid(bool) : null;
}

num Tokens_line(Tokens a) {
    Token  t = idx(a->tokens, 0);
    return t->loc ? t->loc->line : 0;
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

string Token_convert_literal(Token a) {
    assert(call(a, get_type) == typeid(string), "not given a string literal");
    string entire = str(a->chars);
    string result = mid(entire, 1, a->len - 2);
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
        input_string = read(file, typeid(string));
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
        if (call(special_chars, index_of, sval) >= 0) {
            Loc lc = new(Loc, source, file, line, line_num, column, 0);
            if (chr == ':' && idx(input_string, index + 1) == ':') {
                push(tokens, new(Token, chars, "::", loc, lc));
                index += 2;
            } else if (chr == '=' && idx(input_string, index + 1) == '=') {
                push(tokens, new(Token, chars, "==", loc, lc));
                index += 2;
            } else {
                push(tokens, new(Token, chr, chr, loc, lc));
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
            Loc    lc      = new(Loc,
                source, file,
                line,   line_num,
                column, start - line_start);
            Token token    = new(Token, chars, crop->chars, loc, lc);
            push(tokens, token);
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
        
        Loc    lc   = new(Loc, source, file, line, line_num, column, start - line_start);
        string crop = mid(input_string, start, index - start);
        push(tokens, new(Token, chars, crop->chars, loc, lc));
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
    Token first = peek(tokens);
    object    n = intern(first, is_numeric);
    if (n)
        call(tokens, consume);
    return n;
}

// does not read past the type into [ model-query ]
type Tokens_read_type(Tokens tokens, silver module) {
    bool is_ref = false;
    Token first = peek(tokens);
    if (eq(first, "ref")) {
        call(tokens, consume);
        is_ref = true;
        first  = peek(tokens);
    }
    string  key = cast(first, string);
    type    def = call(module, get_type, key);
    assert(def || !is_ref, "type-identifier expected after ref keyword, found: %o", key);
    if (def)
        call(tokens, consume);
    return def;
}

Token Tokens_read(Tokens a, num rel) {
    return a->tokens->elements[clamp(a->cursor + rel, 0, a->tokens->len)];
}

Token Tokens_prev(Tokens a) {
    if (a->cursor <= 0)
        return null;
    a->cursor--;
    Token res = read(a, 0);
    return res;
}

Token Tokens_next(Tokens a) {
    if (a->cursor >= len(a->tokens))
        return null;
    Token res = read(a, 0);
    a->cursor++;
    return res;
}

Token Tokens_consume(Tokens a) {
    return Tokens_next(a);
}

Token Tokens_peek(Tokens a) {
    return read(a, 0);
}

bool Tokens_next_is(Tokens a, symbol cs) {
    Token n = read(a, 0);
    return n && strcmp(n->chars, cs) == 0;
}

AType Token_get_type(Token a) {
    char t = a->chars[0];
    if (t == '"' || t == '\'') return typeid(string);
    return null;
}

string Tokens_next_string(Tokens a) {
    Token  n = read(a, 0);
    AType  t = Token_get_type(n);
    if (t == typeid(string)) {
        string token_s = str(n->chars);
        string result  = mid(token_s, 1, token_s->len - 2);
        a->cursor ++;
        return result;
    }
    return null;
}

object Tokens_next_numeric(Tokens a) {
    object num = Tokens_read_numeric(a);
    return num;
}

string Tokens_next_assign(Tokens a) {
    Token  n = read(a, 0);
    string k = str(n->chars);
    string m = get(assign, k);
    if (m) a->cursor ++;
    return k;
}

string Tokens_next_alpha(Tokens a) {
    Token n = read(a, 0);
    if (is_alpha(n)) {
        a->cursor ++;
        return str(n->chars);
    }
    return null;
}

object Tokens_next_bool(Tokens a) {
    Token  n       = read(a, 0);
    bool   is_true = strcmp(n->chars, "true")  == 0;
    bool   is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool) a->cursor ++;
    return is_bool ? A_bool(is_true) : null;
}

object Tokens_next_literal(Tokens a) {
    object res;
    res = Tokens_next_bool   (a); if (res) return res;
    res = Tokens_next_numeric(a); if (res) return res;
    res = Tokens_next_string (a);
    return res;
}

typedef struct tokens_data {
    array tokens;
    num   cursor;
} *tokens_data;

void Tokens_push_state(Tokens a, array tokens, num cursor) {
    tokens_data state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}

void Tokens_pop_state(Tokens a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data state = (tokens_data)call(a->stack, last); // we should call this element or ele
    pop(a->stack);
    if(!transfer)
        a->cursor = state->cursor;
}

void Tokens_push_current(Tokens a) {
    call(a, push_state, a->tokens, a->cursor);
}

bool Tokens_cast_bool(Tokens a) {
    return a->cursor < len(a->tokens) - 1;
}

Loc Token_location(Token a) {
    return new(Loc,
        source, a->loc->source,
        line,   a->loc->line,
        column, a->loc->column);
}

Loc Tokens_location(Token a) {
    return call(Tokens_peek(a), location);
}

void Loc_init(Loc a) {
}

string Loc_cast_string(Loc a) {
    return format("%o:%i:%i", a->source, a->line, a->column);
}

define_class(Token)
define_class(Loc)
define_class(Tokens)

module_init(init)