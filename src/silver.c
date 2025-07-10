#include <import>
#include <dlfcn.h>

/// silver 88
/// the goal of 88 is to support class, enum, struct with operator overloads
/// import keyword to import other silver modules as well as any other source project, including
/// git repos that build rust, C or C++ [ all must export C functions ]

#define   emodel(MDL)    ({ \
    member  m = ether_lookup(mod, string(MDL), null); \
    model mdl = (m && m->is_type) ? m->mdl : null; \
    mdl; \
})

static map   operators;
static array keywords;
static array assign;
static array compare;
static bool is_alpha(A any);
static node parse_expression(silver mod);

static void print_tokens(silver mod, symbol label) {
    print("[%s] tokens: %o %o %o...", label, element(mod, 0), element(mod, 1), element(mod, 2));
}

static void print_all(silver mod, symbol label, array list) {
    print("[%s] tokens", label);
    each(list, token, t)
        put("%o ", t);
    put("\n");
}

i32 read_enum(silver mod, i32 def, AType etype);

typedef struct {
    OPType ops   [2];
    string method[2];
    string token [2];
} precedence;

static precedence levels[] = {
    { { OPType__mul,       OPType__div        } },
    { { OPType__add,       OPType__sub        } },
    { { OPType__and,       OPType__or         } },
    { { OPType__xor,       OPType__xor        } },
    { { OPType__right,     OPType__left       } },
    { { OPType__is,        OPType__inherits   } },
    { { OPType__compare,   OPType__equal      } },
    { { OPType__value_default, OPType__cond_value } } // i find cond-value to be odd, but ?? (value_default) should work for most
};


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
        "class",    "proto",    "struct",
        "import",   "typeof",   "schema",
        "is",       "inherits", "ref",
        "const",    "inlaid",   "require",
        "no-op",    "return",   "->",       "::",     "...",
        "asm",      "if",       "switch",   "any",      "enum",
        "ifdef",    "else",     "while",
        "for",      "do",       "cast",     "fn",
        null);
    assign = array_of_cstr(
        "=",  ":",  "+=", "-=", "*=",  "/=", 
        "|=", "&=", "^=", "%=", ">>=", "<<=",
        null);
    compare = array_of_cstr(
        "==", "!=", "<=>", ">=", "<=", ">", "<",
        null);
    operators = map_of( /// ether quite needs some operator bindings, and resultantly ONE interface to use them
        "+",        string("add"),
        "-",        string("sub"),
        "*",        string("mul"),
        "/",        string("div"),
        "||",       string("or"),
        "&&",       string("and"),
        "^",        string("xor"),
        ">>",       string("right"),
        "<<",       string("left"),
        ">=",       string("greater_than_equal"),
        ">",        string("greater_than"),
        "<=",       string("less_than_equal"),
        "<",        string("less_than"),
        /// greater-than, less-than  gte, lte
        "??",       string("value_default"),
        "?:",       string("cond_value"),
        "=",        string("assign"),
        ":",        string("assign"),     /// dynamic behavior on this, turns into "equal" outside of parse-assignment
        "%=",       string("assign_mod"),
        "+=",       string("assign_add"),
        "-=",       string("assign_sub"),
        "*=",       string("assign_mul"),
        "/=",       string("assign_div"),
        "|=",       string("assign_or"),
        "&=",       string("assign_and"),
        "^=",       string("assign_xor"),
        ">>=",      string("assign_right"),
        "<<=",      string("assign_left"),
        "->",       string("resolve_member"),
        "==",       string("compare"),
        "~ ",       string("equal"), /// placeholder impossible match, just so we have the enum ordering
        "!=",       string("not_equal"),
        "is",       string("is"),
        "inherits", string("inherits"),
        null);
    
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        for (int j = 0; j < 2; j++) {
            OPType op        = level->ops[j];
            string e_name    = e_str(OPType, op);
            string op_name   = mid(e_name, 1, len(e_name) - 1);
            string op_token  = op_lang_token(op_name);
            level->method[j] = op_name; /// replace the placeholder; assignment is outside of precedence; the camel has spoken
            level->token [j] = eq(op_name, "equal") ? string("==") : op_token;
        }
    }
}

node parse_statements(silver mod, bool unique_members);
node parse_statement(silver mod);

static node parse_create(silver mod, model src, array expr);

void build_function(silver mod, function fn) {
    if (!fn->body) return;
    if (fn->record)
        push(mod, fn->record);
    mod->last_return = null;
    push(mod, fn);
    if (fn->single_expr) {
        node single = parse_create(mod, fn->rtype, fn->body);
        fn_return(mod, single);
    } else {
        push_state(mod, fn->body, 0);
        record cl = fn ? fn->record : null;
        if (cl) pairs(cl->members, m) print("class member: %o: %o", cl->name, m->key);
        parse_statements(mod, true);
        if (!mod->last_return) {
            node r_auto = null;
            if (fn->record && fn->target)
                r_auto = lookup(mod, string("this"), null);
            else if (fn->rtype == emodel("void"))
                r_auto = node(mdl, emodel("void"));
            else {
                fault("return statement required for function: %o", fn->name);
            }
            fn_return(mod, r_auto);
        }
        pop_state(mod, false);
    }

    pop(mod);
    if (fn->record) pop(mod);
}

void build_record(silver mod, record rec) {
    if (!rec->body) return;
    bool   is_class = instanceof(rec, class) != null;
    symbol sname    = is_class ? "class" : "struct";
    array  body     = rec->body;

    A_log       ("build_record", "%s %o", sname, rec->name);
    print_tokens(mod, "before-build-record");
    push_state(mod, body, 0); /// measure here
    print_tokens(mod, "during-build-record");
    push      (mod, rec);
    while     (peek(mod)) parse_statement(mod);
    pop       (mod);
    pop_state (mod, false); /// should be the same here after
    print_tokens(mod, "after-build-record");
}

path create_folder(silver mod, cstr name, cstr sub) {
    string dir = form(string,
        "%o/%s%s%s", mod->install, name,
            (sub && *sub) ? "/" : "",
            (sub && *sub) ? sub : "");
    path   res = cast(path, dir);
    make_dir(res);
    return res;
}


typedef struct {
    symbol lib_prefix;
    symbol exe_ext, static_ext, shared_ext; 
} exts;

exts get_exts() {
    return (exts)
#ifdef _WIN32
    { "", "exe", "lib", "dll" }
#elif defined(__APPLE__)
    { "", "",    "a",   "dylib" }
#else
    { "", "",    "a",   "so" }
#endif
    ;
}


array import_list(silver mod, bool use_text) {
    array list = new(array);
    if (use_text || next_is(mod, "[")) {
        if (!use_text)
            consume(mod);
        string word = new(string, alloc, 64);
        int neighbor_x = -1;
        int neighbor_y = -1;
        while (true) {
            token arg = next(mod);
            if (eq(arg, "]")) break;
            if (!use_text && get_type(arg) == typeid(string)) {
                push(list, arg->literal);
            } else {
                int x = arg->column;
                int y = arg->line;
                if (neighbor_x == x && neighbor_y == y) {
                    append(word, arg->chars);
                } else {
                    if (word->len)
                        push(list, word);
                    if (isa(arg->literal) == typeid(string))
                        word = new(string, chars, ((string)arg->literal)->chars);
                    else
                        word = new(string, chars, arg->chars);
                }
                neighbor_x = arg->column + strlen(arg->chars); 
                neighbor_y = arg->line;
            }
            
            verify (!next_is(mod, ","), "unexpected ,");
            if (next_is(mod, "]"))
                break;
            
            continue;
        }
        if (word->len)
            push(list, word);
        drop(word);
        assert (next_is(mod, "]"), "expected ] after build flags");
        consume(mod);
    } else {
        string next = read_string(mod);
        push(list, next);
    }
    return list;
}

array read_body(silver mod, bool inner_expr);


string configure_debug(bool debug) {
    return debug ? string("--with-debug") : string("");
}


string cmake_debug(bool debug) {
    return form(string, "-DCMAKE_BUILD_TYPE=%s", debug ? "Debug" : "Release");
}


string make_debug(bool debug) {
    return debug ? string("-g") : string("");
}

/// used for records and functions
array read_body(silver mod, bool inner_expr) {
    array body    = array(32);
    token n       = element(mod,  0);
    bool  bracket = n && eq(n, "[");
    if (!n) return null;
    if (bracket || inner_expr) {
        if (bracket) consume(mod);
        int depth = !!inner_expr + !!bracket; /// inner expr signals depth 1, and a bracket does too.  we need both together sometimes, as in inner expression that has parens
        for (;;) {
            token inner = next(mod);
            if (!inner) break;
            if (eq(inner, "]")) depth--;
            if (eq(inner, "[")) depth++;
            if (depth > 0) {
                push(body, inner);
                continue;
            }
            break;
        }
        return body;
    }
    token p    = element(mod, -1);
    bool  multiple  = n->line > p->line;

    while (1) {
        token k = peek(mod);
        if (!k) break;
        if (!multiple && k->line   > n->line)   break;
        if ( multiple && k->indent < n->indent) break;
        push(body, k);
        consume(mod);
    }
    return body;
}

//AType tokens_isa(tokens a) {
//    token  t = idx(a->tokens, 0);
//    return isa(t->literal);
//}

num silver_current_line(silver a) {
    token  t = element(a, 0);
    return t->line;
}


string silver_location(silver a) {
    token  t = element(a, 0);
    return t ? (string)location(t) : (string)form(string, "n/a");
}


bool is_keyword(A any) {
    AType  type = isa(any);
    string s;
    if (type == typeid(string))
        s = any;
    else if (type == typeid(token))
        s = string(((token)any)->chars);
    
    return index_of_cstr(keywords, cstring(s)) >= 0;
}


bool is_alpha(A any) {
    if (!any)
        return false;
    AType  type = isa(any);
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


int sfn(string a, string b) {
    int diff = len(a) - len(b);
    return diff;
}

string scan_map(map m, string source, int index) {
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

array parse_tokens(A input) {
    string input_string;
    AType  type = isa(input);
    path    src = null;
    if (type == typeid(path)) {
        src = input;
        input_string = read(src, typeid(string), null);
    } else if (type == typeid(string))
        input_string = input;
    else
        assert(false, "can only parse from path");
    
    list   symbols    = list();
    string special    = string(".$,<>()![]/+*:=#");
    i32    special_ln = len(special);
    for (int i = 0; i < special_ln; i++)
        push(symbols, string((i32)special->chars[i]));
    each (keywords, string, kw) push(symbols, kw);
    each (assign,   string, a)  push(symbols, a);
    each (compare,  string, a)  push(symbols, a);
    pairs(operators, i)         push(symbols, i->key);
    sort(symbols, sfn);
    map mapping = map(hsize, 32);
    for (item i = symbols->first; i; i = i->next) {
        string sym = i->value;
        set(mapping, sym, sym);
    }

    array   tokens          = array(128);
    num     line_num        = 1;
    num     length          = len(input_string);
    num     index           = 0;
    num     line_start      = 0;
    num     indent          = 0;
    bool    num_start       = 0;
    /// initial indent verify
    i32 chr0 = idx(input_string, index);
    verify(!isspace(chr0) || chr0 == '\n', "initial statement off indentation");

    while (index < length) {
        i32 chr = idx(input_string, index);

        if (isspace(chr)) {
            if (chr == '\n') {
                line_num  += 1;
                line_start = index + 1;
                indent     = 0;
                label: /// so we may use break and continue
                chr = idx(input_string, ++index);
                if       (!isspace(chr))  continue;
                else if  (chr ==  '\n')   continue;
                else if  (chr ==   ' ')   indent += 1;
                else if  (chr ==  '\t')   indent += 4;
                else                      continue;
                goto label;
            } else {
                index += 1;
                continue;
            }
        }

        num_start = isdigit(chr) > 0;

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
        
        string name = scan_map(mapping, input_string, index);
        if (name) {
            //cstr src = (cstr)&input_string->chars[index];
            token  t = token(
                chars,      (cstr)name->chars, 
                indent,     indent,
                source,     src,
                line,       line_num,
                column,     index - line_start);
            push(tokens, t);
            index += len(name);
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
            if (crop->chars[0] == '-') {
                char ch[2] = { crop->chars[0], 0 };
                push(tokens, token(
                    chars,  ch,
                    indent, indent,
                    source, src,
                    line,   line_num,
                    column, start - line_start));
                crop = string(&crop->chars[1]);
                line_start++;
            }
            push(tokens, token(
                chars,  crop->chars,
                indent, indent,
                source, src,
                line,   line_num,
                column, start - line_start));
            continue;
        }

        num start = index;
        bool is_dim = false;
        while (index < length) {
            i32 v = idx(input_string, index);
            char sval[2] = { v, 0 };
            is_dim = (num_start && v == 'x');
            if (isspace(v) || index_of(special, sval) >= 0 || is_dim)
                break;
            index += 1;
        }
        
        string crop = mid(input_string, start, index - start);
        push(tokens, token(
            chars,  crop->chars,
            indent, indent,
            source, src,
            line,   line_num,
            column, start - line_start));
        if (is_dim) {
            push(tokens, token(
                chars,  "x",
                indent, indent,
                source, src,
                line,   line_num,
                column, index - line_start));
            index++;
        }
    }
    return tokens;
}

token silver_element(silver a, num rel) {
    int r = a->cursor + rel;
    if (r < 0 || r > a->tokens->len - 1) return null;
    token t = a->tokens->elements[r];
    return t;
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


token silver_next(silver a) {
    if (a->cursor >= len(a->tokens))
        return null;
    token res = element(a, 0);
    a->cursor++;
    return res;
}


token silver_consume(silver a) {
    return silver_next(a);
}


token silver_peek(silver a) {
    if (a->cursor == len(a->tokens))
        return null;
    return element(a, 0);
}


bool silver_next_is(silver a, symbol cs) {
    token n = element(a, 0);
    return n && strcmp(n->chars, cs) == 0;
}

bool silver_next_is_alpha(silver a) {
    if (peek_keyword(a)) return false;
    token n = element(a, 0);
    return is_alpha(n) ? true : false;
}

bool silver_next_is_keyword(silver a) {
    if (peek_keyword(a))
        return true;
    return false;
}


token silver_read(silver a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        a->cursor++;
        return n;
    }
    return null;
}


object silver_read_literal(silver a, AType of_type) {
    token  n = element(a, 0);
    if (n && n->literal) {
        a->cursor++;
        return n->literal;
    }
    return null;
}


string silver_read_string(silver a) {
    token  n = element(a, 0);
    if (n && isa(n->literal) == typeid(string)) {
        string token_s = string(n->chars);
        string result  = mid(token_s, 1, token_s->len - 2);
        a->cursor ++;
        return result;
    }
    return null;
}


object silver_read_numeric(silver a) {
    token n = element(a, 0);
    if (n && (isa(n->literal) == typeid(f64) || isa(n->literal) == typeid(i64))) {
        a->cursor++;
        return n->literal;
    }
    return null;
}

// this merely reads the various assignment operators if set, and gives a constant bool
string silver_read_assign(silver a, ARef assign_type, ARef is_const) {
    token  n = element(a, 0);
    if (!n) return null;
    string    k            = string(n->chars);
    num       assign_index = index_of(assign, k);
    bool      found        = assign_index >= 0;
    *(bool*)  is_const     = false;
    *(OPType*)assign_type  = OPType__undefined;
    if (found) {
        if (eq(k, ":")) {
            *(bool*)is_const      = true;
            *(OPType*)assign_type = OPType__assign;
        } else {
            *(bool*)is_const      = false;
            *(OPType*)assign_type = OPType__assign + assign_index;
        }
        a->cursor ++;
    }
    return found ? k : null;
}


string silver_read_alpha(silver a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        a->cursor ++;
        return string(n->chars);
    }
    return null;
}

array silver_namespace_push(silver mod) {
    /// count the :: tokens in too many lines
    int back = 0;
    token n = null;
    array levels = array(32);
    while (1) {
        n = element(mod, 0);
        if (!n)
            break;
        if (eq(n, "::")) {
            mod->cursor ++;
            back++;
            continue;
        }
        if (!is_alpha(n)) {
            verify(back == 0, "expected alpha-numeric after :: in multi-ident context");
            return levels;
        }
        break;
    }
    /// push the stack with these many unique member states
    map   mlast  = mod->top->members;
    int   i      = 0;
    while (back > len(levels)) {
        int index = mod->lex->len - 1 - 1 - i++;
        verify(index >= 0, "invalid namespace");
        model mdl = mod->lex->elements[index];
        if (mlast != mdl->members) {
            mlast  = mdl->members;
            push(levels, hold(mdl));
        }
    }
    return levels;
}

none silver_namespace_pop(silver mod, array levels) {
    for (int i = 0; i < len(levels); i++)
        push(mod, levels->elements[i]);
}

function parse_fn        (silver mod, AMember  member_type, object ident, OPType assign_enum);
model    read_model      (silver mod, array* expr);

/// should not need to back out ever, since members are always being operated
/// unless its a type, in which case we have a membership for them.. so everything is a member
node silver_read_node(silver mod, AType constant_result) {
    print_tokens(mod, "read-node");

    /// only use :: keyword to navigate for code in functions
    object lit = read_literal(mod, null);
    if (lit)
        return operand(mod, lit, null);
    
    // parenthesized expressions, with a model check (make sure its not a type, which indicates array/map typed expression)
    if (next_is(mod, "[")) {
        push_current(mod);
        bool  in_inlay = mod->in_inlay;
        print_tokens(mod, "read-node2");
        model inner    = read_model(mod, null); /// if the model comes first, this is an array or map
        print_tokens(mod, "read-node3");
        if (!inner) {
            pop_state(mod, true);
            consume(mod);
            node expr = parse_expression(mod); // Parse the expression
            verify(read(mod, "]"), "Expected closing parenthesis");
            return parse_ternary(mod, expr);
        }
        mod->in_inlay = in_inlay;
        pop_state(mod, false); /// we may just avoid reading it twice, but i want to test the stack a bit more and make the code less prone to state issue
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read(mod, "!") || read(mod, "not")) {
        node expr = parse_expression(mod); // Parse the following expression
        return not(mod, expr);
    }

    // bitwise NOT operator
    else if (read(mod, "~")) {
        node expr = parse_expression(mod);
        return bitwise_not(mod, expr);
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    else if (read(mod, "typeof")) {
        bool bracket = false;
        if (next_is(mod, "[")) {
            assert(next_is(mod, "["), "Expected '[' after 'typeof'");
            consume(mod); // Consume '['
            bracket = true;
        }
        node expr = parse_expression(mod); // Parse the type expression
        if (bracket) {
            assert(next_is(mod, "]"), "Expected ']' after type expression");
            consume(mod); // Consume ']'
        }
        return expr; // Return the type reference
    }

    // 'ref' operator (reference)
    else if (read(mod, "ref")) {
        mod->in_ref = true;
        node expr = parse_expression(mod);
        mod->in_ref = false;
        return addr_of(mod, expr, null);
    }

    /// 'inlay' keyword forces a value type, where as pointer-type is used without
    /// we're doing this without a depth of models to avoid complexity
    /// 'ref' type on the model is merely about its end storage, 
    /// not anything to do with stride size
    /// hopefully this can be implemented simply
    else if (read(mod, "inlay")) {
        mod->in_inlay = true;
        node expr = parse_expression(mod);
        mod->in_inlay = false;
        return expr;
    }
    push_current(mod);
    
    string   kw       = peek_keyword(mod);
    function fn       = instanceof(mod->top, function);
    function in_args  = instanceof(mod->top, arguments);
    record   rec      = context_model(mod, typeid(class));
    silver   module   = (mod->top == (model)mod || (fn && fn->imdl == (model)mod)) ? mod : null;
    bool     is_cast  = kw && eq(kw, "cast");
    bool     is_fn    = kw && eq(kw, "fn");
    AMember  mtype    = is_fn ? A_MEMBER_SMETHOD : is_cast ? A_MEMBER_CAST : A_MEMBER_IMETHOD;
    function in_init  = (fn && fn->imdl) ? fn : null; /// no-op on levels if we are in membership only, not in a function
    array    levels   = (in_args || in_init || is_fn || is_cast) ? array() : namespace_push(mod);
    string   alpha    = null;
    member   mem      = null;
    int      back     = 0;
    int      depth    = 0;

    array expr = null;
    print_tokens(mod, "pre-read_model");
    model mdl = read_model(mod, &expr);
    print_tokens(mod, "post-read_model");
    if (mdl) {
        pop_state(mod, true);
        print_tokens(mod, "pre-parse_create");
        node cr = parse_create(mod, mdl, expr);
        print_tokens(mod, "post-parse_create");
        return cr;
    }

    // L hand modal switching might be needed for load()
    //mod->left_hand = true;
    record is_record = instanceof(mod->top, record);

    for (;;) {
        bool first = !mem;
        if (first && (is_fn || is_cast)) consume(mod);
        alpha = read_alpha(mod);
        if (!alpha) {
            verify(mem == null, "expected alpha ident after .");
            break;
        }

        /// only perform on first iteration
        bool ns_found = false;
        if (first) {
            /// if token is a module name, consume an expected . and member-name
            each (mod->imports, Import, im) {
                if (im->namespace && eq(alpha, im->namespace->chars)) {
                    string module_name = alpha;
                    verify(read(mod, "."), "expected . after module-name: %o", alpha);
                    alpha = read_alpha(mod);
                    verify(alpha, "expected alpha-ident after module-name: %o", module_name);
                    mem = lookup(mod, alpha, typeid(Import)); // if import has no namespace assignment, we should not push it to members
                    verify(mem, "%o not found in module-name: %o", alpha, module_name);
                    ns_found = true;
                }
            }
        }

        if (!ns_found)
            mem = (is_fn || is_cast) ? null : lookup(mod, alpha, null); 
        if (mem && !mem->is_func && mem->mdl) {
            push(mod, mem->mdl);
            depth++;
        }

        /// other things such as member,  member[expr],  member.something[expr].another
        model     ctx    = mod->top;
        record    rec    = instanceof(mod->top, record);
        function  fn     = instanceof(mod->top, function);
        bool parse_index = false;


        // if new type; verify we are allowed to create a function here
        // silver wont let us create context functions above level
        // its trivial to define the args we give in context, and 
        // it keeps the code more flat. 
        if (!mem) {
            verify(mod->expr_level == 0, "member not found: %o", alpha);
            /// check if module or record constructor
            member rmem = lookup(mod, alpha, null);
            if (module && rmem && rmem->mdl == (model)module) {
                verify(!is_cast && is_fn, "invalid constructor for module; use fn keyword");
                mtype = A_MEMBER_CONSTRUCT;
            } else if (rec && rmem && rmem->mdl == (model)rec) {
                verify(!is_cast && !is_fn, "invalid constructor for class; use class-name[] [ no fn, not static ]");
                mtype = A_MEMBER_CONSTRUCT;
            }

            token t = peek(mod);
            /// parse new member, parsing any associated function
            mem = member(
                mod,        mod,
                name,       alpha,
                mdl,        (next_is(mod, "[") || next_is(mod, "->")) ?
                    parse_fn(mod, mtype, alpha, OPType__undefined) : null,
                is_module,  module);
            
        } else if (!mem->is_type) {

            /// from record if no value; !is_record means its not a record definition but this is an expression within a function
            if (!is_record && !mem->is_func && !has_value(mem)) { // if mem from_record_in_context
                AType ctx_type = isa(ctx);
                member target = lookup(mod, string("this"), null); // unique to the function in class, not the class
                verify(target, "no target found in context");
                mem = resolve(target, alpha);
                verify(mem, "failed to resolve member in context: %o", mod->top->name);
            }

            bool chain = next_is(mod, ".") || next_is(mod, "->");
            if (!chain) {
                member m = instanceof(mem, member);
                print_tokens(mod, "before-parse_member_expr");
                mem = parse_member_expr(mod, mem);
                /// we need only the 'node' value of member; we are sometimes returning node here
                /// todo: use node value for this mem state
            } else
            if (read(mod, ".")) {
                verify(mem->mdl, "cannot resolve from new member %o", mem->name);
                push(mod, hold(mem->mdl));
                string alpha = read_alpha(mod);
                verify(alpha, "expected alpha identifier");
                mem = resolve(mem, alpha); /// needs an argument
                mem = parse_member_expr(mod, mem);
                pop(mod);
            } else {
                /// implement a null guard for object -> member syntax
                /// make pointers safe again
                if (first) {
                    /// if next is [, we are defining a fn
                    verify(false, "not implemented 1");
                } else {
                    verify(false, "not implemented 2");
                }
            }
        }

        if (!read(mod, ".")) break;
        verify(!mem->is_func, "cannot resolve into function");
    }

    /// restore namespace after resolving member
    for (int i = 0; i < depth; i++)
        pop(mod);

    /// then our relative :: backward depth
    namespace_pop(mod, levels);
    pop_state(mod, true);
    mod->left_hand = false;

    /// note:
    /// must complete class 'struct' type-ref (with SetBody) before we finish functions
    /// as is, we are finishing function when they are read in.  the class is incomplete at that point, although we have a type-ref for it

    if (mod->expr_level == 0) {
        OPType assign_enum  = OPType__undefined;
        bool   assign_const = false;
        string assign_type  = read_assign  (mod, &assign_enum, &assign_const);
        if (in_args) {
            verify(mem, "expected member");
            verify(assign_enum == OPType__assign, "expected : operator in arguments");
            verify(!mem->mdl, "duplicate member exists in arguments");
            mem->mdl = read_model(mod, &expr);
            verify(expr == null, "unexpected assignment in args");
            verify(mem->mdl, "cannot read model for arg: %o", mem->name);
        }
        else if (is_record && assign_type) {
            mem->mdl = read_model(mod, &expr);
            mem->initializer = hold(expr);
        }
        else if ((module || rec) && peek_def(mod)) {
            verify(!mem, "unexpected member state when next token is def");
            mem = read_def(mod);
        } else if (mem && assign_type) {
            mod->expr_level++;
            verify(mem, "member expected before assignment operator");
            expr = parse_assignment(mod, mem, assign_type);
            print_tokens(mod, "after-parse-assignment");
            mod->expr_level--;
        }
    }

    if (mem && mem->mdl) {
        AType model_type = mem->mdl ? isa(mem->mdl) : null;
        push_member(mod, mem); /// do not finalize in push member
    }

    return mem;
}


string silver_read_keyword(silver a) {
    token n = element(a, 0);
    if (n && is_keyword(n)) {
        a->cursor ++;
        return string(n->chars);
    }
    return null;
}

string silver_peek_keyword(silver a) {
    token   n = element(a, 0);
    return (n && is_keyword(n)) ? string(n->chars) : null;
}

string silver_peek_def(silver a) {
    token n = element(a, 0);
    if (n && is_keyword(n))
        if (eq(n, "import") || eq(n, "fn") || eq(n, "class") || eq(n, "enum") || eq(n, "struct"))
            return string(n->chars);
    return null;
}


object silver_read_bool(silver a) {
    token  n       = element(a, 0);
    if (!n) return null;
    bool   is_true = strcmp(n->chars, "true")  == 0;
    bool   is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool) a->cursor ++;
    return is_bool ? A_bool(is_true) : null;
}


typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;


void silver_push_state(silver a, array tokens, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    print("push cursor: %i", state->cursor);
    push(a->stack, state);
    tokens_data* state_saved = (tokens_data*)last(a->stack);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}


void silver_pop_state(silver a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data* state = (tokens_data*)last(a->stack); // we should call this element or ele
    
    if(!transfer)
        a->cursor = state->cursor;
    else if (transfer && state->tokens != a->tokens) {
        /// transfer implies we up as many tokens
        /// as we traveled in what must be a subset
        a->cursor += state->cursor;
    }

    if (state->tokens != a->tokens) {
        drop(a->tokens);
        a->tokens = state->tokens;
    }
    pop(a->stack);
}

void silver_build_initializer(silver mod, member mem) {
    if (mem->initializer) {
        node expr;
        /// need to decide if we parse expressions direct in read_node 
        /// for class members, because we do not yet have the member 
        /// value-ref pointers at that point; we ARE in the class-init, though
        if (instanceof(mem->initializer, node))
            expr = mem->initializer;
        else {
            push_state(mod, mem->initializer, 0);
            expr = parse_expression(mod);
            pop_state(mod, false);
        }
        member target = lookup(mod, string("this"), null); // unique to the function in class, not the class
        verify(target, "no target found in context");
        member rmem = resolve(target, mem->name);
        assign(mod, rmem, expr, OPType__assign);
        
    } else {
        /// ether can memset to zero
    }
}

void silver_push_current(silver a) {
    push_state(a, a->tokens, a->cursor);
}


node parse_return(silver mod) {
    model rtype = return_type(mod);
    bool  is_v  = is_void(rtype);
    model ctx = context_model(mod, typeid(function));
    consume(mod);
    node expr   = is_v ? null : parse_expression(mod);
    A_log("return-type", "%o", is_v ? (object)string("void") : 
                                    (object)expr->mdl);
    return fn_return(mod, expr);
}


node parse_break(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node parse_for(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


node parse_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}



node silver_parse_do_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}


static node reverse_descent(silver mod) {
    node L = read_node(mod, null); // build-arg
    verify(L, "failed to read L-value in reverse-descent");
    mod->expr_level++;
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        bool  m = true;
        while(m) {
            m = false;
            for (int j = 0; j < 2; j++) {
                string token  = level->token [j];
                if (!read(mod, cstring(token)))
                    continue;
                OPType op_type = level->ops   [j];
                string method  = level->method[j];
                node R = read_node(mod, null);
                     L = op(mod, op_type, method, L, R);
                m      = true;
                break;
            }
        }
    }
    mod->expr_level--;
    return L;
}

static node read_expression(silver mod) {
    mod->no_build = true;
    node vr = reverse_descent(mod);
    mod->no_build = false;
    return vr;
}


static node parse_expression(silver mod) {
    //mod->expr_level++;
    node vr = reverse_descent(mod);
    //mod->expr_level--;
    return vr;
}

model read_named_model(silver mod) {
    model mdl = null;
    push_current(mod);
    bool any = read(mod, "any") != null;
    if (any) {
        model mdl_any = lookup(mod, string("any"), null)->mdl;
        return mdl_any;
    }
    string a = read_alpha(mod);
    if (a && !next_is(mod, ".")) {
        member f = lookup(mod, a, null);
        if (f && f->is_type) mdl = f->mdl;
    }
    print("cursor 1: %i", mod->cursor);
    pop_state(mod, mdl != null); /// save if we are returning a model
    print("cursor 2: %i", mod->cursor);
    return mdl;
}

void print_token_array(silver mod, array tokens) {
    string res = string();
    for (int i = 0, ln = len(tokens); i < ln; i++) {
        token t = tokens->elements[i];
        append(res, t->chars);
        append(res, " ");
    }
    print("tokens = %o", res);
}

model read_model(silver mod, array* expr) {
    model mdl       = null;
    bool  body_set  = false;
    bool  type_only = false;
    model type      = null;
    if (expr) *expr = null;

    push_current(mod);
    
    if (read(mod, "[")) {
        type = read_model(mod, null);
        if (!type) {
            pop_state(mod, false);
            return null;
        }

        array shape    = null;
        bool  is_auto  = read(mod, "]") != null;
        bool  is_map   = false;
        bool  is_array = is_auto || next_is(mod, ","); /// it may also be looking at literal x literal
        bool  is_row   = false;
        bool  is_col   = false;
        verify(!next_is(mod, "]"), "unexpected ']' use [any] for any object");

        shape = array(8);

        /// determine if map or array if we have not already determined [is_auto]
        if (!is_auto) {
            object lit = read_literal(mod, null);

            /// this literal may be a value if is_array is set already
            if (lit) { 
                if (!is_array) { /// it would have seen ] or , ... both of which are after size
                    is_array = true;
                    do {
                        verify(isa(lit) == typeid(i64), "expected numeric for array size");
                        push(shape, lit);
                        if (!read(mod, "x"))
                            break;
                        lit = read_literal(mod, null);
                        verify(lit, "expecting literal after x");
                    } while (lit);
                    if (read(mod, "row")) {
                        is_row = true;
                    } else if (read(mod, "col"))
                        is_col = true;
                    verify(next_is(mod, ":") || next_is(mod, "]"), "expected : or ]");
                }
                /// if we specified a ] or : already we are in 'automatic' size
            } else if (read(mod, ":")) {
                is_map = true;
                model  value_type = read_model(mod, null);
                verify(value_type, "expected value type for map");
                print_tokens(mod, "map");
                drop(shape);
                shape = type;
                type  = value_type;
                print_tokens(mod, "read-model 2");
            }
        }

        type_only = is_auto || !read(mod, ","); // todo: attempting to read this twice, nope!
        
        token t = peek(mod);
        print("first token 1 = %o", t);

        verify(is_map || is_array, "failed to read model");
        /// we use simple alias for both array and map cases
        /// map likely needs a special model
        mdl = alias(type, null, mod->in_inlay ?
            reference_value : reference_pointer, shape);
        mod->in_inlay = false;
    } else {
        /// read-body calle twice because this is inside the token state
        mdl = read_named_model(mod);
        if (expr && next_is(mod, "[")) {
            body_set = true;
            print_tokens(mod, "pre-read-body");
            *expr = read_body(mod, false); /// todo: read body can adapt to seeing a [, and look for another ] at balance
            print_tokens(mod, "post-read-body");
        } else if (!mdl) {

            type_only = true;
            /// 
        } else {

        }
    }

    bool has_read = mdl != null;
    pop_state(mod, has_read); /// save if we are returning a model
    if (has_read && expr && !body_set && !type_only) {
        token t = peek(mod);
        print("first token = %o", t);
        *expr = read_body(mod, true);
    }

    if (expr && *expr)
        print_token_array(mod, *expr);

    verify(!expr || (*expr || type_only), "expression not set to array");
    return mdl;
}



member cast_method(silver mod, class class_target, model cast) {
    record rec = class_target;
    verify(isa(rec) == typeid(class), "cast target expected class");
    pairs(rec->members, i) {
        member mem = i->value;
        model fmdl = mem->mdl;
        if (isa(fmdl) != typeid(function)) continue;
        function fn = fmdl;
        if (fn->is_cast && fn->rtype == cast)
            return mem;
    }
    return null;
}

static node parse_function_call(silver mod, member fmem);

/// parse construct from and cast to, and named args
/// expr comes from read-model;
/// we need to do that to parse single-expr 
/// to gather model info, and its expression body prior
/// to builder being active on the function
/// meant to be read from a tokens state  key:value  key2:value, no [ ] brackets surrounding
map parse_map(silver mod) {
    map args  = map(hsize, 16);
    while (peek(mod)) {
        string name  = read_alpha(mod);
        verify(read(mod, ":"), "expected : after arg %o", name);
        node   value = parse_expression(mod);
        verify(!contains(args, name), "duplicate initialization of %o", name);
        set(args, name, value);
    }
    return args;
}

node parse_create(silver mod, model src, array expr) {
    if (expr)
        print_token_array(mod, expr);
    push_state(mod, expr ? expr : mod->tokens, expr ? 0 : mod->cursor);
    object  n = read_literal(mod, null);
    if (n) {
        pop_state(mod, false); /// issue here is we lost [ array ] context, expr has no brackets and it needs them
        return operand(mod, n, src);
    }
    bool    has_content = !!expr && len(expr); //read(mod, "[") && !read(mod, "]");
    node    r           = null;
    bool    conv        = false;
    token   k           = peek(mod);
    bool    has_init    = is_alpha(k) && eq(element(mod, 1), ":");
    
    if (!has_content) { /// same for all objects if no content
        r = create(mod, src, null); // default
        conv = false;
    } else if (src->is_array) {
        array nodes = array(64); /// initial alloc size
        model element_type = src->src;
        AType shape_type = isa(src->shape);
        int shape_len = len((array)src->shape);
        verify((!src->top_stride && shape_len == 0) || (src->top_stride && shape_len),
            "unknown stride information");  
        int num_index = 0; /// shape_len is 0 on [ int 2x2 : 1 0, 2 2 ]

        while (peek(mod)) {
            token n = peek(mod);
            node  e = parse_expression(mod);
            e = convert(mod, e, element_type);
            push(nodes, e);
            num_index++;
            if (src->top_stride && num_index == src->top_stride) {
                verify(read(mod, ",") || !peek(mod),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    src->top_stride);
            }
        }
        int array_size = 0;
        /// support both stack and heap here, depending on presence of inlay 
        r = operand(mod, nodes, src);
    } else if (src->is_map) {
        verify(has_init, "invalid initialization of map model");
        conv = true;
        r    = parse_map(mod);
    } else {
        if (has_init) {
            /// init with members [create performs required check]
            /// this converts operand to a map
            conv = true;
            r    = parse_map(mod);
        } else {
            /// this is a conversion operation
            r = parse_expression(mod);
            conv = r->mdl != src;
        }
        //verify(read(mod, "]"), "expected ] after mdl-expr %o", src->name);
    }
    if (conv)
        r = create(mod, src, r);
    pop_state(mod, false);
    return r;
}

static node parse_construct(silver mod, member mem) {
    verify(mem && mem->is_type, "expected member type");
    verify(read(mod, "["), "expected [ after type name for construction");
    node res = null;
    //print_tokens("parse-construct", mod);
    /// it may be a dedicated constructor (for primitives or our own), or named args (not for primitives)
    if (instanceof(mem->mdl->src, record)) {
        AType atype = isa(mem->mdl->src);
        map args = map(hsize, 16);
        int count = 0;
        while (!next_is(mod, "]")) {
            string name  = read_alpha(mod);
            verify(read(mod, ":"), "expected : after arg %o", name);
            node   value = parse_expression(mod);
            set(args, name, value);
            count++;
        }
        verify(len(args) == count, "arg count mismatch");
        verify(read(mod, "]"), "expected ] after construction");

        /// now we need to parse the named arguments, and perform assignment on each
        /// then we separately parse 'default values' (not now, but thats in member as a body attribute)
        res = create(mod, mem->mdl, args);
    } else {
        /// enum [ value ] <- same as below:
        /// primitive [ value ] <- not sure if we want this, as we have cast
    }
    return res;
}

node silver_parse_member_expr(silver mod, member mem) {
    print_tokens(mod, "parse_member_expr");
    push_current(mod);
    int parse_index = !mem->is_func;

    /// handle compatible indexing methods and general pointer dereference @ index
    if (parse_index && next_is(mod, "[")) {
        record r = instanceof(mem->mdl, record);
        /// must have an indexing method, or be a reference_pointer
        verify(mem->mdl->ref == reference_pointer || r, "no indexing available for model %o/%o",
            mem->mdl->name, e_str(reference, mem->mdl->ref));
        
        /// we must read the arguments given to the indexer
        consume(mod);
        array args = array(16);
        while (!next_is(mod, "]")) {
            node expr = parse_expression(mod);
            push(args, expr);
            verify(next_is(mod, "]") || next_is(mod, ","), "expected ] or , in index arguments");
            if (next_is(mod, ","))
                consume(mod);
        }
        consume(mod);
        node index_expr = null;
        if (r) {
            /// this returns a 'node' on this code path
            member indexer = compatible(mod, r, null, A_MEMBER_INDEX, args); /// we need to update member model to make all function members exist in an array
            /// todo: serialize arg type names too
            verify(indexer, "%o: no suitable indexing method", r->name);
            function fn = instanceof(indexer->mdl, function);
            indexer->target_member = mem; /// todo: may not be so hot to change the schema
            index_expr = fn_call(mod, indexer, args); // needs a target
        } else {
            index_expr = offset(mod, mem, args);
        }
        pop_state(mod, true);
        return index_expr;
    } else if (mem) {
        if (mem->is_func) {
            if (!mod->in_ref)
                mem = parse_function_call(mod, mem);
        } else if (mem->is_type && next_is(mod, "[")) {
            mem = parse_construct(mod, mem); /// this, is the construct
        } else if (mod->in_ref || mem->literal) {
            mem = mem; /// we will not load when ref is being requested on a member
        } else {
            mem = load(mod, mem); // todo: perhaps wait to AddFunction until they are used; keep the member around but do not add them until they are referenced (unless we are Exporting the import)
        }
    }
    pop_state(mod, mem != null);
    return mem;
}

/// parses member args for a definition of a function
arguments parse_args(silver mod) {
    verify(read(mod, "["), "parse-args: expected [");
    array       args = new(array,     alloc, 32);
    arguments   res  = new(arguments, mod, mod, args, args);

    //print_tokens("parse-args", mod);
    if (!next_is(mod, "]")) {
        push(mod, res); // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        int statements = 0;
        for (;;) {
            /// we expect an arg-like statement when we are in this context (arguments)
            if (next_is(mod, "...")) {
                consume(mod);
                res->is_ext = true;
                continue;
            }
            parse_statement(mod);
            statements++;
            if (next_is(mod, "]"))
                break;
        }
        consume(mod);
        pop(mod);
        verify(statements == len(res->members), "argument parser mismatch");
        pairs(res->members, i) {
            member arg = i->value;
            push(args, arg); // todo: make sure there is no value set on these, because we have no default at the moment (we could, of course with this)
        }
    } else {
        consume(mod);
    }
    return res;
}

static node parse_function_call(silver mod, member fmem) {
    bool     expect_br = false;
    function fn        = fmem->mdl;
    bool     has_expr  = len(fn->args) > 0;
    verify(isa(fn) == typeid(function), "expected function type");
    int  model_arg_count = len(fn->args);
    
    if (has_expr && next_is(mod, "[")) {
        consume(mod);
        expect_br = true;
    } else
        verify(!has_expr, "expected [ for function calls requiring arguments");

    int    arg_index = 0;
    array  values    = new(array, alloc, 32);
    member last_arg  = null;
    while(arg_index < model_arg_count || fn->va_args) {
        member arg      = arg_index < len(fn->args) ? get(fn->args, arg_index) : null;
        node   expr     = parse_expression(mod);
        model  arg_mdl  = arg ? arg->mdl : null;
        if (arg_mdl && expr->mdl != arg_mdl)
            expr = convert(mod, expr, arg_mdl);
        
        push(values, expr);
        arg_index++;
        verify(!next_is(mod, ","), "unexpected comma. for arguments (and silver) the expressions separate themselves");

        if (next_is(mod, "]")) {
            verify (arg_index >= model_arg_count, "expected %i args", model_arg_count);
            break;
        }
    }
    if (expect_br) {
        verify(next_is(mod, "]"), "expected ] end of function call");
        consume(mod);
    }
    return fn_call(mod, fmem, values);
}


node silver_parse_ternary(silver mod, node expr) {
    if (!read(mod, "?")) return expr;
    node expr_true  = parse_expression(mod);
    node expr_false = parse_expression(mod);
    return ether_ternary(mod, expr, expr_true, expr_false);
}

// with constant literals, this should be able to merge the nodes into a single value
// would be called 
node silver_parse_assignment(silver mod, member mem, string oper) {
    verify(!mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    mod->in_assign = mem;
    node   L       = mem;
    print_tokens(mod, "parse-assignment");
    node   R       = parse_expression(mod); /// getting class2 as a struct not a pointer as it should be. we cant lose that pointer info
    print_tokens(mod, "parse-assignment-after");
    if (!mem->mdl) {
        mem->is_const = eq(oper, ":");
        set_model(mem, R->mdl); /// this is erroring because no 
        if (mem->literal)
            drop(mem->literal);
        mem->literal = hold(R->literal);
    }
    verify(contains(operators, oper), "%o not an assignment-operator");
    string op_name = get(operators, oper);
    string op_form = form(string, "_%o", op_name);
    OPType op_val  = e_val(OPType, cstring(op_form));
    node   result  = op(mod, op_val, op_name, L, R);
    mod->in_assign = null;
    return result;
}


node cond_builder_ternary(silver mod, array cond_tokens, object unused) {
    push_state(mod, cond_tokens, 0);
    node cond_expr = parse_expression(mod);
    pop_state(mod, false);
    return cond_expr;
}


node expr_builder_ternary(silver mod, array expr_tokens, object unused) {
    push_state(mod, expr_tokens, 0);
    node exprs = parse_expression(mod);
    pop_state(mod, false);
    return exprs;
}


node cond_builder(silver mod, array cond_tokens, object unused) {
    push_state(mod, cond_tokens, 0);
    node cond_expr = parse_expression(mod);
    pop_state(mod, false);
    return cond_expr;
}


node expr_builder(silver mod, array expr_tokens, object unused) {
    push_state(mod, expr_tokens, 0);
    node exprs = parse_statements(mod, true);
    pop_state(mod, false);
    return exprs;
}


/// parses entire chain of if, [else-if, ...] [else]
/// lets do this a bit simpler
node parse_ifdef_else(silver mod) {
    bool  require_if   = true;
    bool  one_truth    = false;
    bool  expect_last  = false;
    node  statements   = null;

    while (true) {
        verify(!expect_last, "continuation after else");
        bool  is_if  = read(mod, "ifdef") != null;
        verify(is_if && require_if || !require_if, "expected if");
        array cond   = is_if ? read_body(mod, false) : null;
        array block  = read_body(mod, false);
        node  n_cond = null;

        if (cond) {
            bool prev      = mod->in_const;
            mod->in_const  = true;
            push_state(mod, cond, 0);
            n_cond = parse_expression(mod);
            object const_v = n_cond->literal;
            pop_state(mod, false);
            mod->in_const  = prev;
            one_truth      = true;
            if (const_v && cast(bool, const_v)) {
                push_state(mod, block, 0);
                statements = parse_statements(mod, false);
                pop_state(mod, false);
            }
        } else if (!one_truth) {
            verify(!is_if, "if statement incorrect");
            push_state(mod, block, 0);
            statements = parse_statements(mod, false); /// make sure parse-statements does not use its own members
            pop_state(mod, false);
            expect_last = true;
        }
        if (!is_if)
            break;
        bool next_else = read(mod, "else") != null;
        if (!next_else)
            break;
        require_if = false;
    }
    return statements;
}

/// parses entire chain of if, [else-if, ...] [else]
node parse_if_else(silver mod) {
    bool  require_if   = true;
    array tokens_cond  = array(32);
    array tokens_block = array(32);
    while (true) {
        bool is_if  = read(mod, "if") != null;
        verify(is_if && require_if || !require_if, "expected if");
        array cond  = is_if ? read_body(mod, false) : null;
        array block = read_body(mod, false);
        push(tokens_cond,  cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = read(mod, "else") != null;
        require_if = false;
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    return ether_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}


node parse_do_while(silver mod) {
    consume(mod);
    node vr = null;
    return null;
}

bool is_access(silver mod) {
    interface access = interface_public;
    token k = peek(mod);
    for (int m = 1; m < interface_type.member_count; m++) {
        type_member_t* enum_v = &interface_type.members[m];
        if (eq(k, enum_v->name)) {
            return true;
        }
    }
    return false;
}

bool is_model(silver mod) {
    token  k = peek(mod);
    member m = lookup(mod, k, null);
    return m && m->is_type;
}

// these are for public, intern, etc; A-Type enums, not someting the user defines in silver in context
i32 read_enum(silver mod, i32 def, AType etype) {
    for (int m = 1; m < etype->member_count; m++) {
        type_member_t* enum_v = &etype->members[m];
        if (read(mod, enum_v->name))
            return *(i32*)enum_v->ptr;
    }
    return def;
}

/// todo: these must be called manually by silver; otherwise its not as clear
/// do this in incremental-resolve [ now far more silver oriented ]



map ether_top_member_map(ether e);

path module_path(silver mod, string name) {
    cstr exts[] = { "sf", "sr" };
    path res    = null;
    path cw     = path_cwd(0);
    for (int  i = 0; i < 2; i++) {
        path  r = f(path, "%o/%o.%s", cw, name, exts[i]);
        if (file_exists("%o", res)) {
            res = r;
            break;
        }
    }
    return res;
}

member register_import(
            silver mod, string name, array _modules, array _includes)
{
    string fallback = len(_modules) ? first(_modules) : len(_includes) ? first(_includes) : null;
    array includes = array(alloc, 32);
    array modules  = array(alloc, 32);
    each(_modules, object, m) {
        verify(isa(m) == typeid(string),
            "expected string identifier for module");
        push(modules, module_path(mod, (string)m));
    }
    each(_includes, object, inc) {
        verify(isa(inc) == typeid(string),
            "expected string identifier for include");
        push(includes, include_path(mod, (string)inc));
    }
    Import im = Import(
        mod,        mod,
        name,       name ? name : (fallback && isalpha(first(fallback)) ? fallback : null),
        modules,    modules,
        includes,   includes,
        namespace,  name);
    
    member mem = member(mod, mod, name, im->name, context, mod->top);
    push(mod->imports, im); // push to imports array; which is a bit redundant since the model is registered in the context as a member

    set_model(mem, im);
    push_member(mod, mem);
    return mem;
}

/// called after : or before, where the user has access
member silver_read_def(silver mod) {
    bool   is_import = next_is(mod, "import");
    bool   is_class  = next_is(mod, "class");
    bool   is_struct = next_is(mod, "struct");
    bool   is_enum   = next_is(mod, "enum");
    bool   is_alias  = next_is(mod, "alias");

    if (!is_import && !is_class && !is_struct && !is_enum && !is_alias)
        return null;

    consume(mod);

    /// implement the import keyword; its a bit tricky around <stdio as std, another> ... its certainly possible to group both too with:
    if (is_import) { // import <stdio, another> as stdan  <- we group all of them when specifying it outside
        token inc       = read(mod, "<"); // ... silver-mode is without < >'s
        array includes  = null; // there you may also combine:  import silio another, as silan
        array modules   = null; // question is do we also allow this:
        path  inc_path  = null; // anyway we can also do:  import silver as si, <another>
        token alias     = null;
        token t         = null;

        verify(next_is_alpha(mod), "expected alpha-numeric after import");

        while ((t = next(mod))) {
            string n = cast(string, t);
            if (inc) {
                if (!includes) includes = array(alloc, 32);
                push(includes, n);
            } else {
                if (!modules) modules = array(alloc, 32);
                push(modules, n);
            }

            /// we may encouner a > to end our includes 
            bool end_of_inc   = inc && read(mod, ">");
            bool is_as        = (end_of_inc || !inc) && (read(mod, "as") != null);
            bool is_comma     = read(mod, ",") != null;

            if (end_of_inc || is_as || is_comma) {
                token name   = is_as ? next(mod) : null;
                verify(len(modules) || len(includes), "no modules or includes to import");
                register_import(mod, name, modules, includes);
                modules      = null;
                includes     = null;
                end_of_inc   = false;
                inc          = null;
                if (!is_comma)
                    break;
                
            } else if (next_is_keyword(mod) || !next_is_alpha(mod))
                break;

            verify(next_is_alpha(mod), "expected alpha-numeric continuation");
        }

        if (modules || includes) {
            register_import(mod, null, modules, includes);
        }

        return null;
    } else {
        string n   = read_alpha(mod);
        verify(n, "expected alpha-numeric identity, found %o", next(mod));

        member mem = member(mod, mod, name, n, context, mod->top);

        if (is_class || is_struct) {
            array schema = array();
            /// read class schematics
            model parent = null;
            if (is_class && next_is(mod, "[")) {
                consume(mod);
                parent = instanceof(read_model(mod, null), model);
                consume(mod);
            }
            array body   = read_body(mod, false);
            /// todo: call build_record right after this is done
            if (is_class)
                mem->mdl = class    (mod, mod, name, n, body, body, parent, parent);
            else
                mem->mdl = structure(mod, mod, name, n, body, body);
        
        } else if (is_enum) {
            model store = null, suffix = null;
            if (read(mod, ",")) {
                store  = instanceof(read_model(mod, null), model);
                suffix = instanceof(read_model(mod, null), model);
                verify(store, "invalid storage type");
            }
            array enum_body = read_body(mod, false);
            print_all(mod, "enum-tokens", enum_body);
            verify(len(enum_body), "expected body for enum %o", n);

            mem->mdl  = enumeration(
                mod, mod,  src, store,  name, n);
            
            push_state(mod, enum_body, 0);
            push(mod, mem->mdl);

            store = mem->mdl->src;

            /// verify model is a primitive type
            AType atype = isa(mem->mdl->src->src);
            verify(atype && (atype->traits & A_TRAIT_PRIMITIVE),
                "enumeration can only be based on primitive types (i32 default)");

            i64  i64_v = 0;
            f64  f64_v = 0;
            f32  f32_v = 0;
            i64* i64_value = store == emodel("i64") ? &i64_v : null;
            i64* i32_value = store == emodel("i32") ? &i64_v : null;
            f64* f64_value = store == emodel("f64") ? &f64_v : null;
            f32* f32_value = store == emodel("f32") ? &f32_v : null;
            object value = (object)(i64_value ? (A)i64_value : i32_value ? (A)i32_value :
                                    f64_value ? (A)f64_value : (A)f32_value);
            
            while (true) {
                token e = next(mod);
                if  (!e) break;
                object v = null;
                
                if (read(mod, ":")) {
                    v = read_node(mod, atype); // i want this to parse an entire literal, with operations
                    //verify(v && isa(v) == atype, "expected numeric literal");
                } else
                    v = A_primitive(atype, value); /// this creates i8 to i64 data, using &value i64 addr


                // lets resolve operations in here
                if (instanceof(v, node)) {
                    // v = resolve_constant(v);
                    int test2 = 2;
                    test2 += 2;
                } else if (instanceof(v, member)) {
                    int test2 = 2;
                    test2 += 2;
                } else {
                    AType res = isa(v);
                    int test2 = 2;
                    test2 += 2;
                }
                array aliases = null;
                while(read(mod, ",")) {
                    if (!aliases) aliases = array(alloc, 32);
                    string a = read_alpha(mod);
                    push(aliases, a);
                }
                member emem = member(
                    mod, mod, name, e, mdl, mem->mdl,
                    is_const, true, is_decl, true, literal, v, aliases, aliases);
                
                set_value(emem, v); // redundant with the first literal field; we aren't to simply store the token[0] or tokens, either
                
                string estr = cast(string, e);
                set(mem->mdl->members, estr, emem);
                if (aliases)
                    each(aliases, string, a)
                        set(mem->mdl->members, a, emem);
                
                if (i64_value) {
                    *i64_value += 1;
                } else if (i32_value) {
                    *i32_value += 1;
                } else if (f64_value) {
                    *f64_value += 1.0;
                } else if (f32_value) {
                    *f32_value += 1.0f;
                }
            }
            pop(mod);
            pop_state(mod, false);

            // is below required or did i comment this out as test?
            finalize(mem->mdl, mem);
        } else {
            verify(is_alias, "unknown error");
            mem->mdl = alias(mem->mdl, mem->name, reference_pointer, null);
        }
        mem->is_type = true;
        verify(mem && (is_import || len(mem->name)),
            "name required for model: %s", isa(mem->mdl)->name);
        push_member(mod, mem);
        return mem; // for these keywords, we have one member registered (not like our import)
    }
}


/// such a big change to this one, tectonic drift'ing ...
node parse_statement(silver mod) {
    print_tokens(mod, "parse-statement");

    token     t            = peek(mod);
    record    rec          = instanceof(mod->top, record);
    function  fn           = context_model(mod, typeid(function));
    arguments args         = instanceof(mod->top, arguments);
    silver    module       = (mod->prebuild || (fn && fn->imdl == (model)mod)) ? mod : null;
    bool      is_func_def  = false;
    string    assign_type  = null;
    OPType    assign_enum  = 0;
    bool      assign_const = false;

    //print_tokens("left-hand", mod);
    /// handle root statement expressions first
    /// yes a 'module' could be its own main with args
    /// however import would need succinct arg parsing
    /// having a singular class or struct you give to main is intuitive
    /// its an argument because functions take args; modules really dont
    mod->last_return = null;
    if (!module) {
        if (next_is(mod, "no-op"))  return node(mdl, emodel("void"));
        if (next_is(mod, "return")) {
            mod->last_return = parse_return(mod);
            return mod->last_return;
        }
        if (next_is(mod, "break"))  return parse_break(mod);
        if (next_is(mod, "for"))    return parse_for(mod);
        if (next_is(mod, "while"))  return parse_while(mod);
        if (next_is(mod, "if"))     return parse_if_else(mod);
        if (next_is(mod, "do"))     return parse_do_while(mod);
    } else {
        if (next_is(mod, "ifdef"))
            return parse_ifdef_else(mod);
    }
    node   e = read_node(mod, null); /// at module level, supports keywords
    return e;
}

node parse_statements(silver mod, bool unique_members) {
    if (unique_members)
        push(mod, new(statements, mod, mod));
    
    //print_tokens("parse_statements", mod);
    node  vr        = null;
    ///
    while(peek(mod)) {
        vr = parse_statement(mod);
    }
    if (unique_members)
        pop(mod);
    return vr;
}


function parse_fn(silver mod, AMember member_type, object ident, OPType assign_enum) {
    model      rtype = null;
    object     name  = instanceof(ident, token);
    array      body  = null;
    if (!name) name  = instanceof(ident, string);
    if (!name) {
        if (member_type != A_MEMBER_CAST) {
            name = read_alpha(mod);
            verify(name, "expected alpha-identifier");
        }
    } else
        verify(member_type != A_MEMBER_CAST, "unexpected name for cast");

    record rec = context_model(mod, typeid(class));
    arguments args = arguments();
    verify (next_is(mod, "[") || next_is(mod, "->"), "expected function args [");
    bool r0 = read(mod, "->") != null;
    if (!r0 && next_is(mod, "[")) {
        args = parse_args(mod);
    }
    
    bool single_expr = false;
    if ( r0 || read(mod, "->")) {
        //print_tokens("read_model-pre", mod);
        rtype = read_model(mod, &body); 
        //print_tokens("read_model-post", mod);
        if (body && len(body)) {
            // if body is set, then parse_fn will return the parse_create call, with model of rtype, and expr of body
            single_expr = true;
        }
        verify(rtype, "type must proceed the -> keyword in fn");
    } else if (member_type == A_MEMBER_IMETHOD)
        rtype = rec ? (model)pointer(rec) : emodel("generic");
    else
        rtype = emodel("void");
    
    verify(rtype, "rtype not set, void is something we may lookup");
    if (!name) {
        verify(member_type == A_MEMBER_CAST, "with no name, expected cast");
        name = form(token, "cast_%o", rtype->name);
    }
    
    function     fn      = function(
        mod,    mod,     name,   name, function_type, member_type,
        record, rec,     rtype,  rtype, single_expr, single_expr,
        args,   args,    body,   (body && len(body)) ? body : read_body(mod, false));
    
    return fn;
}

void silver_incremental_resolve(silver mod) {
    bool in_top = mod->in_top;
    mod->in_top = false;
    
    /// finalize included structs
    pairs(mod->members, i) {
        member mem = i->value;
        model base = mem->mdl->ref ? mem->mdl->src : mem->mdl;
        if (!mem->mdl->finalized && instanceof(base, record) && base->from_include)
            finalize(base, mem);
    }

    /// finalize imported C functions, which use those structs perhaps literally in argument form
    pairs(mod->members, i) {
        member mem = i->value;
        model  mdl = mem->mdl;
        if (!mdl->finalized && instanceof(mem->mdl, function) && mdl->from_include) {
            finalize(mem->mdl, mem);
        }
    }

    /// process/finalize all remaining member models 
    /// calls process sub-procedure and poly-based finalize
    pairs(mod->members, i) {
        member mem = i->value;
        model base = mem->mdl->ref ? mem->mdl->src : mem->mdl;
        if (!base->finalized && instanceof(base, record) && !base->from_include) {
            build_record(mod, mem->mdl);
            finalize(base, mem);
            pairs(mem->mdl->members, ii) {
                member rec_mem = ii->value;
                if (instanceof(rec_mem->mdl, function)) {
                    build_function(mod, rec_mem->mdl);
                    finalize(mod, rec_mem->mdl);
                }
            }
        }
    }

    /// finally, process functions (last step in parsing)
    pairs(mod->members, i) {
        member mem = i->value;
        if (!mem->mdl->finalized && instanceof(mem->mdl, function) && !mem->mdl->from_include) {
            build_function(mod, mem->mdl);
            finalize(mem->mdl, mem);
        }
    }

    mod->in_top = in_top;
}


void silver_parse(silver mod) {
    /// im a module!
    function module_init = initializer(mod);
    push(mod, mod->fn_init);
    mod->in_top = true;
    map members = mod->members;

    while (peek(mod)) {
        parse_statement(mod);
        incremental_resolve(mod);
        print_tokens(mod, "parse-after-resolve");
    }
    mod->in_top = false;

    member mem_init = push_model(mod, module_init); // publish initializer
    pop(mod);
    finalize(module_init, mem_init);
}

void Import_process(Import im) {
    /// this might be returning members from field meta data to something we may understand in ether models
}

// silver uses import for software we import by symbol map
// what this looks like -- as much like import as possible, to reveal silver
void silver_init(silver mod) {
    // always give it a file, or is code ok?  ... compilers like files!
    verify(exists(mod->source), "source (%o) does not exist", mod->source);
    verify(mod->std == language_silver || eq(ext(mod->source), "sf"),
        "only .sf extension supported; specify --std silver to override");
    
    mod->mod = mod;
    mod->products_used = array();
    mod->imports = array(32);
    mod->tokens  = parse_tokens(mod->source);
    mod->stack   = array(4);

    cstr      _DBG = getenv("DBG");
    cstr _IMPORT = getenv("IMPORT");
    verify(_IMPORT, "silver requires IMPORT environment");

    path        af = path_cwd(0);
    path   install = path(_IMPORT);
    map         m  = m(
        "path",    af,
        "dbg",     _DBG ? string(_DBG) : string(""),
        "install", install,
        "src",     parent(install));
    mod->import = import(m); // import could certainly build the .ll assemblies for us -- we have library access

    /// build a proper stack of tokens we may parse through normal means
    /// lets just set a state variable prebuild so that import->skip
    
    
    
    push_state(mod, a(
        token(chars, "import"),
        token(chars, "<"),
        token(chars, "A"),
        token(chars, ">")), 0);
    mod->prebuild = true;
    parse_statement(mod);
    mod->prebuild = false;
    pop_state  (mod, false);
 
    model mdl = emodel("A");
    verify(mdl, "A type not importing where it should be");



    AType mdl_type = isa(mdl); /// A should be a member for a alias ref of struct _A, its a typedef

    Import itop = mod->top;
    /// alias A -> any
    member mA      = lookup(mod, string("A"), null);
    AType  mA_type = isa(mA->mdl);
    string n       = string("any");
    model  mdl_A   = alias(mA->mdl, n, 0, null);
    member any     = member(mod, mod, name, n, mdl, mdl_A, is_type, true);
    member mem_mod = member(mod, mod, name, mod->name, mdl, mod, is_module, mod);
    push_member(mod, mem_mod);
    push_member(mod, any);


    member any_obj = lookup(mod, string("any"), null);

    /// we do not want to push this function to context unless it can be gracefully
    parse(mod);

    /// enumerating objects used, libs used, etc
    /// may be called earlier when the statement of import is finished
    each (mod->imports, Import, im)
        process(im);

    path ll = null, bc = null;
    write(mod, &ll, &bc);
    verify(bc != null, "compilation failed");
    
    build(mod->import, bc);
}

silver silver_load_module(silver mod, path uri) {
    silver mod_load = silver(
        source,  mod->source,
        install, mod->install,
        name,    stem(uri));
    // init performs the rebuild within; so this is going to perform lots of building, including import external
    // if you need to, anyway...

    return mod_load;
}

void silver_import_types(silver mod) {
    // import all types; even silver why not.
    i64 types_len = 0;
    A_f** types = A_types(&types_len);

    /// iterate through types
    for (num i = 0; i < types_len; i++) {
        A_f* type = types[i];
        for (num m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            if (mem->member_type & A_MEMBER_IMETHOD) {
                
            }
        }
    }
}

static none resolve_type(silver mod, model import_into, AType type) {

    /// import type into model/class/structure
    bool is_struct    = (type->traits & A_TRAIT_STRUCT    ) != 0;
    bool is_class     = (type->traits & A_TRAIT_CLASS     ) != 0;
    bool is_primitive = (type->traits & A_TRAIT_PRIMITIVE ) != 0;
    bool is_abstract  = (type->traits & A_TRAIT_ABSTRACT  ) != 0;
    bool is_enum      = (type->traits & A_TRAIT_ENUM      ) != 0;

    if (is_struct || is_class) {
        /// we use ->src to A-type heavily in ether, so we use a sort of direct translation
        /// doesnt work with 'A' -- perhaps A-alloc needs to give double for A.. (i think it does, though)
        record rec = is_struct ? 
            (record)structure(mod, mod, name, token(chars, type->name), body, null, src, A_alloc(type, 1)) :
            (record)class    (mod, mod, name, token(chars, type->name), body, null, src, A_alloc(type, 1));
        
        type->user = rec;
        //verify(mod->top == import_into, "somethings off");
        
        member rtype_member = member(
            mod, mod, name, string(type->name), context, import_into, is_type, true);
        set_model(rtype_member, rec);
        push_member(mod, rtype_member);

        push(mod, rec);
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];

            // model should be associaed to the user on the type
            model type_res = mem->type->user; // emodel(mem->type->name);
            if (!type_res) {
                print("lazy resolving type: %s", mem->type->name);
                resolve_type(mod, import_into, mem->type);
                type_res = mem->type->user;
                verify(type_res, "resolution fail for type: %s", type->name);
            }

            member rec_member = member(
                mod, mod, name, string(mem->name), context, rec);
            set_model(rec_member, type->user);
            push_member(mod, rec_member);
            // set top to this record

            set_model(rec_member, type_res); /// this is erroring because no 
        }
        /// return to our 'Import' space
        pop(mod);
    } else if (is_primitive) {
        model mdl  = model(mod, mod, name, token(chars, type->name), src, A_alloc(type, 1));
        type->user = mdl;
    }
}

none Import_init(Import a) {
    silver mod = a->mod;
    if (a->namespace)
        push(mod, a);

    model itop = mod->top;

    /// for A-type includes, we want to merely query the dependency with import
    /// this would mean live loading the A-type libraries because we use them ourselves
    /// we input the schemas, including properties + extra space for intern (polymorphic)

    each(a->includes, path, inc) {
        string ext = ext(inc);
        if(len(ext) == 0) {
            cstr preloaded[] = {"A", "silver", "ether"};
            string project_name = stem(inc);
            bool is_loaded = false;
            for (int i = 0; i < sizeof(preloaded) / sizeof(cstr); i++) {
                if (eq(project_name, preloaded[i])) {
                    is_loaded = true;
                    break;
                }
            }

            /// load library (if its not already compiled into silver, or we did this already in another silver instance)
            if (!is_loaded) {
                path lib = f(path, "%o/lib/lib%o.so", mod->import->install, project_name);
                verify(file_exists("%o", lib), "A-type library not found");

                /// needs a static map here
                static map instances;
                if (!instances) instances = map(hsize, 16, unmanaged, true);
                handle f = get(instances, project_name);
                
                /// load library and perform an invocation to A_shift() ... A-type sister function to start.
                if (!f) {
                    handle f = dlopen(project_name->chars, RTLD_NOW | RTLD_GLOBAL);
                    verify(f, "dlopen could not load: %o", project_name);
                    set(instances, project_name, f);
                }
            }


            num type_count = 0;
            AType* types = A_types(&type_count);

            for (int i = 0; i < type_count; i++) {
                AType type = types[i];
                if (type->user) continue; // already imported
                if (!eq(project_name, type->module))
                    continue;

                resolve_type(mod, itop, type);
            }

        } else {
            include(mod, inc);
        }
    }

    each(a->modules, path, m) {
        silver sv = load_module(mod, m);
    }
    if (a->namespace)
        pop(mod);
}

define_class (silver, ether)
define_enum  (import_t)
define_enum  (build_state)
define_enum  (language)
define_class (Import, model)
module_init  (initialize)