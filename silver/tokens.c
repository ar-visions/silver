#include <tokens>

bool next_is(Tokens tokens, symbol cs) {
    return call(tokens, next_is, cs);
}

/// Loc
Token Loc_with_path(Loc a, path source, num line, num col) {
    a->source = A_hold(source);
    a->line   = line;
    a->column = col;
}

/// Token
Token Token_with_cstr(Token a, cstr chars, Loc loc) {
    sz length = strlen(chars);
    a->chars  = (cstr)calloc(length + 1, 1);
    a->len    = length;
    a->loc    = A_hold(loc);
    memcpy(a->chars, chars, length);
    return a;
}

Token Token_with_i32(Token a, i32 chr, Loc loc) {
    a->chars    = (cstr)calloc(2, 1);
    a->chars[0] = chr;
    a->len      = 1;
    a->loc    = A_hold(loc);
    return a;
}

/// Tokens
typedef struct tokens_data {
    array tokens;
    num   cursor;
} *tokens_data;

Tokens Tokens_with_path(Tokens a, path file) {
    a->tokens = parse_tokens(file);
    a->cursor = 0;
    return a;
}

Tokens Tokens_with_array(Tokens a, array tokens) {
    a->tokens = hold(tokens);
    a->cursor = 0;
    return a;
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

Token Tokens_peek(Tokens a) {
    return call(a, read, 0);
}

bool Tokens_next_is(Tokens a, symbol cs) {
    Token n = call(a, read, 0);
    return strcmp(n->chars, cs) == 0;
}

void Tokens_transfer(Tokens a, Tokens b) {
    assert(a->tokens == b->tokens);
    a->cursor = b->cursor;
}

void Tokens_push_state(Tokens a, array tokens, num cursor) {
    tokens_data state = A_struct(tokens_data);
    state->tokens = tokens;
    state->cursor = cursor;
    call(a->stack, push, state);
}

void Tokens_pop(Tokens a) {
    call(a->stack, pop);
}

void Tokens_push_current(Tokens a) {
    call(a, push_state, a->tokens, a->cursor);
}

bool Tokens_cast_bool(Tokens a) {
    return a->cursor < len(a->tokens) - 1;
}

/// parse_tokens
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
        assert(false);
    
    string  special_chars   = str(".$,<>()![]/+*:=#");
    array   tokens          = ctr(array, sz, 128);
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
            Loc lc = ctr(Loc, path, file, line_num, 0);
            if (chr == ':' && idx(input_string, index + 1) == ':') {
                call(tokens, push, ctr(Token, cstr, "::", lc));
                index += 2;
            } else if (chr == '=' && idx(input_string, index + 1) == '=') {
                call(tokens, push, ctr(Token, cstr, "==", lc));
                index += 2;
            } else {
                call(tokens, push, ctr(Token, i32, chr, lc));
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
            Loc    lc      = ctr(Loc, path, file, line_num, start);
            call(tokens, push, ctr(Token, cstr, crop->chars, lc));
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
        
        Loc    lc   = ctr(Loc, path, file, line_num, start);
        string crop = call(input_string, mid, start, index - start);
        call(tokens, push, ctr(Token, cstr, crop->chars, lc));
    }
    return tokens;
}

define_class(Token)
define_class(Loc)
define_class(Tokens)