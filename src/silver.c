#include <import>
#include <ports.h>
#include <limits.h>

// for Michael

#define emodel(MDL) ({ \
    emember  m = aether_lookup2(mod, string(MDL), null); \
    if (m) mark_used(m); \
    model mdl = (m && m->is_type) ? m->mdl : null; \
    mdl; \
})

#define validate(a, t, ...) ({ \
    if (!(a)) { \
        formatter((AType)null, stderr, (A)true,  (symbol)"\n%o:%i:%i " t, mod->source, \
            peek(mod)->line, peek(mod)->column, ## __VA_ARGS__); \
        if (level_err >= fault_level) { \
            raise(SIGTRAP); \
            exit(1); \
        } \
        false; \
    } else { \
        true; \
    } \
    true; \
})

// implement automatic .c bindings; for methods without a function, we may bind the method with .c source of the same name
// 

static map   operators;
static array keywords;
static array assign;
static array compare;

static bool  is_alpha(A any);
static enode parse_expression(silver mod, model expect_mdl);

void print_tokens(silver mod, symbol label) {
    print("[%s] tokens: %o %o %o %o %o %o...", label,
        element(mod, 0), element(mod, 1), element(mod, 2), element(mod, 3),
        element(mod, 4), element(mod, 5));
}

static void print_all(silver mod, symbol label, array list) {
    print("[%s] tokens", label);
    each(list, token, t)
        put("%o ", t);
    put("\n");
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

i32 read_enum(silver mod, i32 def, AType etype);

typedef struct {
    OPType ops   [2];
    string method[2];
    string token [2];
} precedence;

static precedence levels[] = {
    { { OPType__mul,       OPType__div        } },
    { { OPType__add,       OPType__sub        } },
    { { OPType__right,     OPType__left       } },
    { { OPType__greater,   OPType__less       } },
    { { OPType__greater_eq, OPType__less_eq   } },
    { { OPType__equal,     OPType__not_equal  } },
    { { OPType__is,        OPType__inherits   } },
    { { OPType__xor,       OPType__xor        } },
    { { OPType__and,       OPType__or         } },
    { { OPType__bitwise_and, OPType__bitwise_or } },
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
        "class",    "proto",    "struct", "public", "intern",
        "import",   "typeof",   "context",
        "is",       "inherits", "ref",
        "const",    "require",  "no-op",    "return",   "->",       "::",     "...",
        "asm",      "if",       "switch",   "any",      "enum",
        "e-if",    "else",     "while",
        "for",      "do",       "cast",     "fn",
        null);
    
    assign = array_of_cstr(
        "=",  ":",  "+=", "-=", "*=",  "/=", 
        "|=", "&=", "^=", "%=", ">>=", "<<=",
        null);
    
    compare = array_of_cstr(
        "==", "!=", "<=>", ">=", "<=", ">", "<",
        null);
    
    operators = map_of( /// aether quite needs some operator bindings, and resultantly ONE interface to use them
        "+",        string("add"),
        "-",        string("sub"),
        "*",        string("mul"),
        "/",        string("div"),
        "||",       string("or"),
        "&&",       string("and"),
        "|",        string("bitwise_or"),
        "&",        string("bitwise_and"),
        "^",        string("xor"),
        ">>",       string("right"),
        "<<",       string("left"),
        ">=",       string("greater_eq"),
        ">",        string("greater"),
        "<=",       string("less_eq"),
        "<",        string("less"),
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

enode parse_statements(silver mod, bool unique_members);
enode parse_statement(silver mod);

static enode typed_expr(silver mod, model src, array expr);

void build_fn(silver mod, fn f, callback preamble, callback postamble) {
    if (!f->body && !preamble && !postamble) return;
    if (f->instance)
        push(mod, f->instance);
    mod->last_return = null;
    push(mod, f);
    if (f->target) {
        register_member(mod, f->target, true); // lookup should look at args; its pretty stupid to register twice
    }
    if (preamble)
        preamble(f, null);
    array after_const = parse_const(mod, f->body);
    if (f->cgen) {
        // generate code with cgen delegate imported (todo)
        array gen = generate_fn(f->cgen, f, f->body);
    }
    else if (f->single_expr) {
        enode single = typed_expr(mod, f->rtype, after_const);
        e_fn_return(mod, single);
    } else {
        push_state(mod, after_const, 0);
        record cl = f ? f->instance : null;
        if (cl) pairs(cl->members, m) print("class emember: %o: %o", cl->name, m->key);
        parse_statements(mod, true);
        if (!mod->last_return) {
            enode r_auto = null;
            if (f->rtype == emodel("none"))
                r_auto = null;
            else if (f->instance)
                r_auto = lookup2(mod, string("a"), null);
            else if (!mod->cmode) {
                fault("return statement required for function: %o", f->name);
            }
            e_fn_return(mod, r_auto);
        }
        pop_state(mod, false);
    }
    if (postamble)
        postamble(f, null);
    pop(mod);
    if (f->instance) pop(mod);
    finalize(f);
}

A build_init_preamble(fn f, A arg) {
    silver mod = f->mod;
    Class  rec = f->instance;

    pairs(rec->members, i) {
        emember mem = i->value;
        mem->target_member = f->target;
        if (mem->initializer)
            build_initializer(mod, mem);
    }
    return null;
}

void create_schema(model mdl);

void build_record(silver mod, record rec) {
    AType t = isa(rec);
    rec->parsing = true;
    bool   is_class = instanceof(rec, typeid(Class)) != null;
    symbol sname    = is_class ? "class" : "struct";
    array  body     = rec->body ? rec->body : array();

    push_state(mod, body, 0);
    push      (mod, rec);
    while     (peek(mod)) {
        print_tokens(mod, "parse-statement");
        parse_statement(mod); // must not 'build' code here, and should not (just initializer type[expr] for members)
    }

    pop       (mod);
    pop_state (mod, false);
    rec->parsing = false;
    finalize(rec);
    create_schema(rec);

    if (is_class) {
        push(mod, rec);

        // if no init, create one
        emember m_init = member_lookup(rec, string("init"), typeid(fn));
        if (!m_init) {
            fn f_init = fn(mod, mod,
                name, string("init"),
                extern_name, f(string, "%s_init", rec->name),
                rtype, emodel("none"), instance, rec, args, eargs(mod, mod));
            m_init    = emember(mod, mod, name, f_init->name, mdl, f_init);
            register_member(mod, m_init, true);
        }
        pop(mod);

        // build with preamble
        build_fn(mod, m_init->mdl, build_init_preamble, null); // we may need to 
        
        // build remaining functions
        pairs(rec->members, ii) {
            emember m = ii->value;
            if (!m->mdl->finalized && instanceof(m->mdl, typeid(fn)))
                build_fn(mod, m->mdl, null, null);
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
    { "", "exe", "lib", "dll" }
#elif defined(__APPLE__)
    { "", "",    "a",   "dylib" }
#else
    { "", "",    "a",   "so" }
#endif
    ;
}


bool silver_next_indent(silver a) {
    token p = element(a, -1);
    token n = element(a,  0);
    return p && n->indent > p->indent;
}

bool silver_next_is_eq(silver a, symbol first, ...) {
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

#define next_is(a, ...) silver_next_is_eq(a, __VA_ARGS__, null)

array read_within(silver mod) {
    array body    = array(32);
    token n       = element(mod,  0);
    bool  bracket = n && eq(n, "[");
    if (!bracket) return null;

    consume(mod);
    int depth = 1; // inner expr signals depth 1, and a bracket does too.  we need both togaether sometimes, as in inner expression that has parens
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

array read_body(silver mod) {
    array body = array(32);
    token n    = element(mod,  0);
    if (!n) return null;
    token p    = element(mod, -1);
    bool  mult = n->line > p->line;

    while (1) {
        token k = peek(mod);
        if (!k) break;
        if (!mult && k->line    > n->line)   break;
        if ( mult && k->indent <= p->indent) break;
        push(body, k);
        consume(mod);
    }
    return body;
}

num silver_current_line(silver a) {
    token  t = element(a, 0);
    return t->line;
}

string silver_location(silver a) {
    token  t = element(a, 0);
    return t ? (string)location(t) : (string)form(string, "n/a");
}

num index_of_cstr(A a, cstr f) {
    AType t = isa(a);
    if (t == typeid(string)) return index_of((string)a, f);
    if (t == typeid(array))  return index_of((array)a, string(f));
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal)) {
        cstr v = strstr(a, f);
        return v ? (num)(v - f) : (num)-1;
    }
    fault("len not handled for type %s", t->name);
    return 0;
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

array parse_tokens(silver mod, A input) {
    string input_string;
    AType  type = isa(input);
    path    src = null;
    if (type == typeid(path)) {
        src = input;
        input_string = load(src, typeid(string), null); // this was read before, but we 'load' files; way less conflict wit posix
    } else if (type == typeid(string))
        input_string = input;
    else
        assert(false, "can only parse from path");

    mod->source_raw = hold(input_string);
    
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

    i32 chr0 = idx(input_string, index);
    validate(!isspace(chr0) || chr0 == '\n', "initial statement off indentation");

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

        if (!mod->cmode && chr == '#') {
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
            if (mod->cmode && len(name) == 1 && strncmp(&input_string->chars[index], "##", 2) == 0) {
                name = string("##");
            }
            token  t = token(
                mod,        mod,
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
                    mod,    mod,
                    chars,  ch,
                    indent, indent,
                    source, src,
                    line,   line_num,
                    column, start - line_start));
                crop = string(&crop->chars[1]);
                line_start++;
            }
            push(tokens, token(
                mod,    mod,
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
            mod,    mod,
            chars,  crop->chars,
            indent, indent,
            source, src,
            line,   line_num,
            column, start - line_start));
        if (is_dim) {
            push(tokens, token(
                mod,    mod,
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

bool silver_next_is_alpha(silver a) {
    if (peek_keyword(a)) return false;
    token n = element(a, 0);
    return is_alpha(n) ? true : false;
}


token silver_read_if(silver a, symbol cs) {
    token n = element(a, 0);
    if (n && strcmp(n->chars, cs) == 0) {
        a->cursor++;
        return n;
    }
    return null;
}

A silver_read_literal(silver a, AType of_type) {
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

A silver_read_numeric(silver a) {
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

string silver_read_alpha_any(silver a) {
    token n = element(a, 0);
    if (n && isalpha(n->chars[0])) {
        a->cursor ++;
        return string(n->chars);
    }
    return null;
}

string silver_read_alpha(silver a) {
    token n = element(a, 0);
    if (is_alpha(n)) {
        a->cursor ++;
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

fn    parse_fn  (silver mod, AFlag member_type, A ident, OPType assign_enum);
model read_model(silver mod, array* expr);

bool ref_level(model f);

enode silver_read_node(silver mod, AType constant_result, model mdl_expect) {
    bool cmode = mod->cmode;
    array expr = null;
    token peek = peek(mod);

    bool      is_expr0  = !mod->cmode && mod->expr_level == 0;
    interface access    = is_expr0 ? read_enum(mod, interface_undefined, typeid(interface)) : 0;
    bool      is_static = is_expr0 && read_if(mod, "static") != null;
    string    kw        = is_expr0 ? peek_keyword(mod) : null;
    bool      is_cast   = kw && eq(kw, "cast");
    bool      is_fn     = kw && eq(kw, "fn");
    fn        f         = !mod->cmode ? context_model(mod,   typeid(fn))     : null;
    fn        in_args   = !mod->cmode ? instanceof(mod->top, typeid(eargs))  : null;
    record    rec       = !mod->cmode ? context_model(mod,   typeid(Class))  : null;
    record    in_record = !mod->cmode ? instanceof(mod->top, typeid(record)) : null;
    silver    module    = (!mod->cmode && (mod->top == (model)mod || 
                           instanceof(mod->top, typeid(import))  || 
                          (f && f->is_module_init))) ? mod : null;
    emember   mem       = null;
    AFlag     mtype     = (is_fn && !is_static) ? A_FLAG_IMETHOD : (!is_static && is_cast) ? A_FLAG_CAST : A_FLAG_SMETHOD;

    if (is_expr0 && !mod->cmode && (module || rec) && peek_def(mod)) {
        print_tokens(mod, "peek def");
        emember mem = read_def(mod);
        if (!mem) {
            if (is_cast || is_fn) consume(mod);
            token alpha = !is_cast ? read_alpha(mod) : null;
            /// check if module or record constructor
            emember rmem = alpha ? lookup2(mod, alpha, null) : null;
            if (rmem) mark_used(rmem);
            if (module && rmem && rmem->mdl == (model)module) {
                validate(!is_cast && is_fn, "invalid constructor for module; use fn keyword");
                mtype = A_FLAG_CONSTRUCT;
            } else if (rec && rmem && rmem->mdl == (model)rec) {
                validate(!is_cast && !is_fn, "invalid constructor for class; use class-name[] [ no fn, not static ]");
                mtype = A_FLAG_CONSTRUCT;
            }

            bool is_fn_contents = is_cast || (next_is(mod, "[") || next_is(mod, "->"));
            validate(is_fn_contents, "expected fn contents for ");

            print_tokens(mod, "before read fn");

            // parse function contents
            mem = emember(
                mod,        mod,
                access,     access,
                name,       alpha,
                mdl,        parse_fn(mod, mtype, alpha, OPType__undefined),
                is_module,  module);
            if (is_cast) {
                fn f = (fn)mem->mdl;
                validate(len(f->rtype->name), "rtype cannot be anonymous for cast");
                mem->name = hold(f(string, "cast_%o", f->rtype->name));
            }
        }
        register_member(mod, mem, true); /// do not finalize in push member
        return mem;
    }

    if (peek && is_alpha(peek) && !lookup2(mod, peek, typeid(macro))) {
        model mdl_found = read_model(mod, &expr);
        if (mdl_found) {
            enode res = typed_expr(mod, mdl_found, expr);
            return e_create(mod, mdl_expect, res);
        }
    }

    A lit = read_literal(mod, null);
    if (lit) {
        enode res = e_operand(mod, lit, null);
        return e_create(mod, mdl_expect, res);
    }
    
    if (!cmode && next_is(mod, "$", "(")) {
        consume(mod);
        consume(mod);
        fault("shell syntax not implemented for 88");
    }

    if (!cmode && read_if(mod, "null"))
        return e_null(mod, mdl_expect);

    // parenthesized expressions
    if (next_is(mod, "(")) {
        consume(mod);
        // support C-style cast here, only in cmode (macro definitions)
        if (cmode) {
            push_current(mod);
            model inner = read_model(mod, null);
            if (inner) {
                if (next_is(mod, ")")) {
                    consume(mod);
                    pop_state(mod, true);
                    enode res = e_create(mod, inner, parse_expression(mod, null));
                    return e_create(mod, mdl_expect, res);
                } else {
                    pop_state(mod, false);
                    mod->expr_level = 0;
                    return null;
                }
            }
            pop_state(mod, false);
        }
        mod->parens_depth++;
        print_tokens(mod, "within ( ...");
        enode expr = parse_expression(mod, null); // Parse the expression
        print_tokens(mod, "expecting ) ...");
        token t = read_if(mod, ")");
        if (t) mod->parens_depth--;
        return e_create(mod, mdl_expect, parse_ternary(mod, expr));
    }

    if (!cmode && next_is(mod, "[")) {
        validate(mdl_expect, "expected model name before [");
        array expr = read_within(mod);
        // we need to get mdl from argument
        enode r = typed_expr(mod, mdl_expect, expr);
        return r;
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read_if(mod, "!") || (!cmode && read_if(mod, "not"))) {
        enode expr = parse_expression(mod, null); // Parse the following expression
        return e_create(mod, mdl_expect, e_not(mod, expr));
    }

    // bitwise NOT operator
    else if (read_if(mod, "~")) {
        enode expr = parse_expression(mod, null);
        return e_create(mod, mdl_expect, e_bitwise_not(mod, expr));
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    else if (!cmode && read_if(mod, "typeof")) {
        bool bracket = false;
        if (next_is(mod, "[")) {
            consume(mod); // Consume '['
            bracket = true;
        }
        enode expr = parse_expression(mod, null); // Parse the type expression
        if (bracket) {
            assert(next_is(mod, "]"), "Expected ']' after type expression");
            consume(mod); // Consume ']'
        }
        return e_create(mod, mdl_expect, expr); // Return the type reference
    }

    // 'ref' operator (reference)
    else if (!cmode && read_if(mod, "ref")) {
        mod->in_ref = true;
        enode expr = parse_expression(mod, null);
        mod->in_ref = false;
        return e_create(mod, mdl_expect, e_addr_of(mod, expr, null));
    }

    // we may only support a limited set of C functionality for #define macros
    if (cmode) return null;

    push_current(mod);
    
    string    alpha     = null;
    int       depth     = 0;
    
    bool skip_member_check = false;
    if (module) {
        string alpha = peek_alpha(mod);
        if (alpha) {
            emember m = lookup2(mod, alpha, null);
            if (m) mark_used(m);
            model mdl = m ? m->mdl : null;
            while (mdl && mdl->is_ref)
                mdl = mdl->src;
            if (mdl && isa(mdl) == typeid(Class))
                skip_member_check = true;
        }
    }

    for (;!skip_member_check;) {
        bool first = !mem;
        if (first && (is_fn || is_cast)) consume(mod);
        alpha = read_alpha(mod);
        if (!alpha) {
            validate(mem == null, "expected alpha ident after .");
            break;
        }

        /// only perform on first iteration
        bool ns_found = false;
        if (first) {
            /// if token is a module name, consume an expected . and emember-name
            each (mod->spaces, Namespace, im) {
                if (im->namespace_name && eq(alpha, im->namespace_name->chars)) {
                    string module_name = alpha;
                    validate(read_if(mod, "."), "expected . after module-name: %o", alpha);
                    alpha = read_alpha(mod);
                    validate(alpha, "expected alpha-ident after module-name: %o", module_name);
                    mem = lookup2(mod, alpha, typeid(import)); // if import has no namespace assignment, we should not push it to members
                    validate(mem, "%o not found in module-name: %o", alpha, module_name);
                    ns_found = true;
                }
            }
        }
        

        if (!ns_found) {
            print_tokens(mod, "before lookup2");
            mem = (is_fn || is_cast) ? null : lookup2(mod, alpha, null);
            if (mem) mark_used(mem);
        }
        
        record rec = instanceof(mod->top, typeid(record));
        if (mem && eq(mem->name, "printf")) {
            int test2 = 2;
            test2    += 2;
        }
        if (!mem) {
            validate(mod->expr_level == 0, "member not found: %o", alpha);
            /// parse new member, parsing any associated function
            mem = emember(
                mod,        mod,
                access,     access,
                name,       alpha,
                is_module,  module);
        } else if (!mem->is_type || instanceof(mem->mdl, typeid(macro)) || instanceof(mem->mdl, typeid(fn))) {
            bool is_macro = instanceof(mem->mdl, typeid(macro)) != null;
            // from record if no value; !in_record means its not a record definition 
            // but this is an expression within a function
            if (!in_record && !mem->is_func && !is_macro && !has_value(mem)) { // if mem from_record_in_context
                validate(f,         "expected function");
                validate(f->target, "no target found in context");
                mem = resolve(f->target, alpha);
                validate(mem, "failed to resolve emember in context: %o", f->name);
            }

            bool chain = next_is(mod, ".") || next_is(mod, "->");

            if (!chain) {
                emember m = instanceof(mem, typeid(emember));
                mem = parse_member_expr(mod, mem);
            } else {
                if (mem && !module && !mem->is_func && mem->mdl) {
                    push(mod, mem->mdl);
                    depth++;
                }
                if (read_if(mod, ".")) {
                    validate(mem->mdl, "cannot resolve from new emember %o", mem->name);
                    push(mod, hold(mem->mdl));
                    string alpha = read_alpha(mod);
                    validate(alpha, "expected alpha identifier");
                    mem = resolve(mem, alpha); /// needs an argument
                    mem = parse_member_expr(mod, mem);
                    pop(mod);
                } else {
                    /// implement a null guard for A -> emember syntax
                    /// make pointers safe again
                    if (first) {
                        /// if next is [, we are defining a fn
                        validate(false, "not implemented 1");
                    } else {
                        validate(false, "not implemented 2");
                    }
                }
            }
        }

        if (!read_if(mod, ".")) break;
        validate(!access, "unexpected . after access specification");
        validate(!mem->is_func, "cannot resolve into function");
    }

    /// restore namespace after resolving emember
    for (int i = 0; i < depth; i++)
        pop(mod);

    pop_state(mod, true);

    if (mod->expr_level == 0) {
        OPType assign_enum  = OPType__undefined;
        bool   assign_const = false;
        string assign_type  = read_assign  (mod, &assign_enum, &assign_const);
        if (in_args) {
            validate(mem, "expected emember");
            validate(assign_enum == OPType__assign, "expected : operator in eargs");
            validate(!mem->mdl, "duplicate member exists in eargs");
            mem->mdl = read_model(mod, &expr);
            validate(expr == null, "unexpected assignment in args");
            validate(mem->mdl, "cannot read model for arg: %o", mem->name);
        }
        else if ((in_record || module) && assign_type) {
            token t = peek(mod);

            // this is where member types are read
            mem->mdl = read_model(mod, &expr); // given VkInstance intern instance2 (should only read 1 token)
            validate(mem->mdl, "expected type after ':' in %o, found %o",
                (in_record ? (model)in_record : (model)module)->name, peek(mod));
            mem->initializer = hold(expr);

            // lets read meta types here, at the record member level
            /*
            if (next_is(mod, ",")) {
                mem->meta_args = array(alloc, 32, assorted, true);
                consume(mod);
                while (true) {
                    token t = peek(mod);
                    model f = emodel(t->chars);
                    if (!f) break;
                    push(mem->meta_args, f);
                    consume(mod);
                }
                validate(len(mem->meta_args), "expected one or more types after ','");
            }*/

        } else if (mem && assign_type) {
            mod->left_hand = false;
            mod->expr_level++;
            validate(mem, "member expected before assignment operator");
            expr = parse_assignment(mod, mem, assign_type);
            mod->expr_level--;
        } else if (mem && !assign_type && !mem->mdl) {
            validate (false, "expected assignment after '%o'", alpha);
        }
    }

    if (mem && isa(mem) == typeid(emember) && !mem->target_member && mem->mdl) {
        register_member(mod, mem, true); /// do not finalize in push member
    }

    return e_create(mod, mdl_expect, mem);
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

static model next_is_class(silver mod, bool read_token) {
    token t = peek(mod);
    if (!t) return null;
    if (eq(t, "class")) {
        if (read_token)
            consume(mod);
        return emodel("A");
    }

    model f = emodel(t->chars);
    if (f && isa(f) == typeid(Class)) {
        if (read_token)
            consume(mod);
        return f;
    }
    return null;
}

string silver_peek_def(silver a) {
    token n = element(a, 0);
    record rec = instanceof(a->top, typeid(record));
    if (!rec && next_is_class(a, false))
        return string(n->chars);

    if (n && is_keyword(n))
        if (eq(n, "import") || eq(n, "fn") || eq(n, "cast") || eq(n, "class") || eq(n, "enum") || eq(n, "struct"))
            return string(n->chars);
    return null;
}

A silver_read_bool(silver a) {
    token  n       = element(a, 0);
    if (!n) return null;
    bool   is_true = strcmp(n->chars, "true")  == 0;
    bool   is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool) a->cursor ++;
    return is_bool ? A_bool(is_true) : null;
}

array silver_parse_const(silver mod, array tokens) {
    array res = array(32);
    push_state(mod, tokens, 0);
    while (peek(mod)) {
        token t = next(mod);
        if (eq(t, "const")) {
            validate(false, "implement const");
        } else {
            push(res, t);
        }
    }
    pop_state(mod, false);
    return res;
}

void silver_build_initializer(silver mod, emember mem) {
    if (mem->initializer) {
        enode expr;
        if (instanceof(mem->initializer, typeid(enode)))
            expr = mem->initializer;
        else {
            array post_const = parse_const(mod, mem->initializer);
            push_state(mod, post_const, 0);
            print_token_array(mod, mod->tokens);
            int level = mod->expr_level;
            mod->expr_level++;
            expr = parse_expression(mod, null); // we have tokens for the name pushed to the stack
            mod->expr_level = level;
            pop_state(mod, false);
        }

        //enode L = e_load(mod, mem);
        emember target = mem->target_member; //lookup2(mod, string("a"), null); // unique to the function in class, not the class
        validate(target, "no target found in context");
        emember L = resolve(target, mem->name);
        
        e_assign(mod, L, expr, OPType__assign);
        
    } else {
        /// aether can memset to zero
    }
}

enode parse_return(silver mod) {
    model rtype = return_type(mod);
    bool  is_v  = is_void(rtype);
    model ctx = context_model(mod, typeid(fn));
    consume(mod);
    enode expr   = is_v ? null : parse_expression(mod, rtype);
    A_log("return-type", "%o", is_v ? (A)string("none") : 
                                    (A)expr->mdl);
    return e_fn_return(mod, expr);
}

enode parse_break(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

enode parse_for(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

enode parse_while(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

enode silver_parse_do_while(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

static enode reverse_descent(silver mod, model mdl_expect) {
    bool cmode = mod->cmode;
    print_tokens(mod, "reverse_descent");
    enode L = read_node(mod, null, mdl_expect); // build-arg
    validate(cmode || L, "unexpected '%o'", peek(mod));
    if (!L) {
        int statement_count = mod->statement_count;
        print_tokens(mod, "reverse_descent");
        return null;
    }
    mod->expr_level++;
    for (int i = 0; i < sizeof(levels) / sizeof(precedence); i++) {
        precedence *level = &levels[i];
        bool  m = true;
        while(m) {
            m = false;
            for (int j = 0; j < 2; j++) {
                string token  = level->token [j];
                if (!read_if(mod, cstring(token)))
                    continue;
                OPType op_type = level->ops   [j];
                string method  = level->method[j];
                enode R = read_node(mod, null, null);
                if (!mod->current_include) {
                    print_tokens(mod, "after read_node in reverse_descent");
                }
                     L = e_op(mod, op_type, method, L, R);
                if (!mod->current_include) {
                    print_tokens(mod, "after read_node in reverse_descent");
                }
                m      = true;
                break;
            }
        }
    }
    mod->expr_level--;
    return L;
}

static enode read_expression(silver mod, model mdl_expect) {
    mod->no_build = true;
    enode vr = reverse_descent(mod, mdl_expect);
    mod->no_build = false;
    return vr;
}

static enode parse_expression(silver mod, model mdl_expect) {
    enode vr = reverse_descent(mod, mdl_expect);
    return vr;
}

model read_named_model(silver mod) {
    model mdl = null;
    push_current(mod);
    if (instanceof(mod->top, typeid(import)) == 0) {
        bool any = read_if(mod, "any") != null;
        if (any) {
            emember mem = lookup2(mod, string("any"), null);
            model mdl_any = mem->mdl;
            if (mem) mark_used(mem);
            return mdl_any;
        }
    }
    string a = read_alpha(mod);
    if (a && !next_is(mod, ".")) {
        emember f = lookup2(mod, a, null);
        if (f) mark_used(f);
        if (f && f->is_type) mdl = f->mdl;
    }
    print("cursor 1: %i", mod->cursor);
    pop_state(mod, mdl != null); /// save if we are returning a model
    print("cursor 2: %i", mod->cursor);
    return mdl;
}

static model model_adj(silver mod, model mdl) {
    while (mod->cmode && read_if(mod, "*"))
        mdl = model(mod, mod, src, mdl, is_ref, true);
    return mdl;
}

model read_model(silver mod, array* expr) {
    model mdl       = null;
    bool  body_set  = false;
    bool  type_only = false;
    model type      = null;
    if (expr) *expr = null;

    push_current(mod);

    bool explicit_sign = read_if(mod, "signed") != null;

    model prim_mdl = null;
    if (read_if(mod, "char"))   prim_mdl = emodel("i8");
    else if (read_if(mod, "short"))  prim_mdl = emodel("i16");
    else if (read_if(mod, "int"))    prim_mdl = emodel("i32");
    else if (read_if(mod, "long"))   prim_mdl = read_if(mod, "long") ? emodel("i64") : emodel("i32");
    else if (explicit_sign)          prim_mdl = emodel("i32");
    if (prim_mdl)
        prim_mdl = model_adj(mod, prim_mdl);

    if (read_if(mod, "unsigned")) {
        if (read_if(mod, "char"))  prim_mdl = emodel("u8");
        if (read_if(mod, "short")) prim_mdl = emodel("u16");
        if (read_if(mod, "int"))   prim_mdl = emodel("u32");
        if (read_if(mod, "long"))  prim_mdl = read_if(mod, "long") ? emodel("u64") : emodel("u32");

        prim_mdl = model_adj(mod, prim_mdl ? prim_mdl : emodel("u32"));
    }

    if (!mod->cmode && read_if(mod, "array")) {
        validate(read_if(mod, "<"), "expected < after array");
        string ident = string(alloc, 32);

        // read manditory type
        type = read_model(mod, null);
        validate(type, "expected type after <");
        concat(ident, f(string, "array_%o", type->name));

        // read optional shape
        shape sh = shape();
        if (!read_if(mod, ">")) {
            append(ident, "_");
            A lit = read_literal(mod, null);
            validate(lit, "expected shape literal after <");
            do {
                validate(isa(lit) == typeid(i64), "expected numeric for array size");
                sh->data[sh->count] = *(i64*)lit;
                sh->count++;
                concat(ident, f(string, "%o", lit));
                if (!read_if(mod, "x")) // these must be compacted
                    break;
                append(ident, "x");
                lit = read_literal(mod, null);
                validate(lit, "expecting literal after x");
            } while (lit);
            validate(read_if(mod, ">"), "expected > after shape");
        }

        // we must register type information for these models for use in this module
        // further we must identity these by symbol:  array_i32_4x4 for example
        mdl = prim_mdl ? prim_mdl : emodel(ident->chars);
        if (!mdl) {
            array ameta = a(type);
            Class parent = emodel("array");
            push(mod, mod);
            mdl = Class(mod, mod, name, ident, src, type, parent, parent, shape, sh, meta, ameta, is_internal, true); // shape will be used when registering the AType
            register_model(mod, mdl, true);
            finalize(mdl);
            pop(mod);
            create_schema(mdl);
            mdl = pointer(mdl, null);
        }

        if (next_is(mod, "[")) {
            array e = read_within(mod);
            array with_type = array(32);
            push(with_type, token(chars, mdl->name->chars, mod, mod)); // we re-parse with the registered name
            push(with_type, token(chars, (cstr)"[", mod, mod));
            each (e, token, t)
                push(with_type, t);
            push(with_type, token(chars, (cstr)"]", mod, mod));
            validate(expr, "expected expression holder");
            *expr = with_type;
        }

    } else if (!mod->cmode && read_if(mod, "map")) {
        validate(false, "todo");
        //mdl = model(mod, mod, src, type, src, emodel("map"), shape, shape); // shape will be used when registering the AType

    } else {
        print_tokens(mod, "before read-named");
        mdl = prim_mdl ? prim_mdl : read_named_model(mod);
        if (!mod->cmode && expr && next_is(mod, "[")) {
            body_set = true;
            *expr = read_within(mod);
        }
    }

    pop_state(mod, mdl != null); // if we read a model, we transfer token state
    return mdl;
}

emember cast_method(silver mod, Class class_target, model cast) {
    record rec = class_target;
    validate(isa(rec) == typeid(Class), "cast target expected class");
    pairs(rec->members, i) {
        emember mem = i->value;
        model fmdl = mem->mdl;
        if (isa(fmdl) != typeid(fn)) continue;
        fn f = fmdl;
        if (f->is_cast && f->rtype == cast)
            return mem;
    }
    return null;
}

static enode parse_fn_call(silver, fn, enode);

map parse_map(silver mod, model mdl_schema) {
    map args  = map(hsize, 16, assorted, true);

    // we need e_create to handle this as well; since its given a map of fields and a is_ref struct it knows to make an alloc
    if (mdl_schema->is_ref && instanceof(mdl_schema->src, typeid(structure)))
        mdl_schema = mdl_schema->src;

    while (peek(mod)) {
        print_tokens(mod, "parse_map_read_name");
        string name  = read_alpha(mod);
        validate(read_if(mod, ":"), "expected : after arg %o", name);
        model   mdl_expect = null;
        if (mdl_schema && mdl_schema->members) {
            emember m = get(mdl_schema->members, name);
            validate(m, "member %o not found on model %o", name, mdl_schema->name);
            mdl_expect = m->mdl;
        }
        print_tokens(mod, "parse_map_value");
        enode   value = parse_expression(mod, mdl_expect);
        validate(!contains(args, name), "duplicate initialization of %o", name);
        set(args, name, value);
    }
    return args;
}

static bool class_inherits(model mdl, Class of_cl) {
    silver mod = mdl->mod;
    Class cl = instanceof(mdl, typeid(Class));
    Class aa = of_cl;
    while (cl && cl != aa) {
        if (!cl->parent) break;
        cl = cl->parent;
    }
    return cl && cl == aa;
}

static bool peek_fields(silver mod) {
    token t0 = element(mod, 0);
    token t1 = element(mod, 1);
    if (t0 && is_alpha(t0) && t1 && eq(t1, ":"))
        return true;
    return false;
}

static enode typed_expr(silver mod, model mdl, array expr) {
    
    if (expr)
        print_token_array(mod, expr);

    push_state(mod, expr ? expr : mod->tokens, expr ? 0 : mod->cursor);
    
    // function calls
    if (instanceof(mdl, typeid(fn))) {
        fn      f      = mdl;
        map     m      = f->args->members;
        int     ln     = len(m), i = 0;
        array   values = array(alloc, 32);
        enode   target = null;

        while (i < ln || f->va_args) {
            emember arg  = value_by_index(m, i);
            enode   expr = parse_expression(mod, arg->mdl);
            verify(expr, "invalid expression");
            model   arg_mdl  = arg ? arg->mdl : null;
            if (arg_mdl && expr->mdl != arg_mdl)
                expr = e_create(mod, arg_mdl, expr);
            
            if (!target && f->instance)
                target = expr;
            else
                push(values, expr);
            
            if (read_if(mod, ","))
                continue;
            
            validate(len(values) >= ln, "expected %i args for function %o", ln, f->name);
            break;
        }
        return e_fn_call(mod, f, target, values);
    }
    
    // this is only suitable if reading a literal constitutes the token stack
    // for example:  i32 100
    A  n = read_literal(mod, null);
    if (n && mod->cursor == len(mod->tokens)) {
        pop_state(mod, expr ? false : true);
        return e_operand(mod, n, mdl);
    } else if (n) {
        // reset if we read something
        pop_state(mod, expr ? false : true);
        push_state(mod, expr ? expr : mod->tokens, expr ? 0 : mod->cursor);
    }
    bool    has_content = !!expr && len(expr); //read_if(mod, "[") && !read(mod, "]");
    enode   r           = null;
    bool    conv        = false;
    print_tokens(mod, "before peek fields");
    bool    has_init    = peek_fields(mod);
    
    if (!has_content) {
        r = e_create(mod, mdl, null); // default
        conv = false;
    } else if (class_inherits(mdl, emodel("array"))) {
        array nodes = array(64);
        model element_type = mdl->src;
        shape sh = mdl->shape;
        validate(sh, "expected shape on array");
        int shape_len = shape_total(sh);
        int top_stride = sh->count ? sh->data[sh->count - 1] : 0;
        validate((!top_stride && shape_len == 0) || (top_stride && shape_len),
            "unknown stride information");  
        int num_index = 0; /// shape_len is 0 on [ int 2x2 : 1 0, 2 2 ]

        while (peek(mod)) {
            token n = peek(mod);
            enode  e = parse_expression(mod, element_type);
            e = e_create(mod, element_type, e);
            push(nodes, e);
            num_index++;
            if (top_stride && (num_index % top_stride == 0)) {
                validate(read_if(mod, ",") || !peek(mod),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    top_stride);
            }
        }
        r = e_create(mod, mdl, nodes);
    } else if (has_init || class_inherits(mdl, emodel("map"))) {
        validate(has_init, "invalid initialization of map model");
        conv = true;
        print_tokens(mod, "before parse_map");
        r    = parse_map(mod, mdl);
    } else {
        /// this is a conversion operation
        r = parse_expression(mod, mdl);
        conv = r->mdl != mdl;
        //validate(read_if(mod, "]"), "expected ] after mdl-expr %o", src->name);
    }
    if (conv)
        r = e_create(mod, mdl, r);
    pop_state(mod, expr ? false : true);
    return r;
}

array read_arg(aether e, array tokens, int start, int* next_read);

enode silver_parse_member_expr(silver mod, emember mem) {
    push_current(mod);
    int indexable = !mem->is_func;
    bool is_macro = mem && instanceof(mem->mdl, typeid(macro));

    /// handle compatible indexing methods and general pointer dereference @ index
    if (indexable && next_is(mod, "[")) {
        record r = instanceof(mem->mdl, typeid(record));
        /// must have an indexing method, or be a reference_pointer
        validate(mem->mdl->is_ref || r, "no indexing available for model %o",
            mem->mdl->name);
        
        /// we must read the arguments given to the indexer
        consume(mod);
        array args = array(16);
        while (!next_is(mod, "]")) {
            enode expr = parse_expression(mod, null);
            push(args, expr);
            validate(next_is(mod, "]") || next_is(mod, ","), "expected ] or , in index arguments");
            if (next_is(mod, ","))
                consume(mod);
        }
        consume(mod);
        enode index_expr = null;
        if (r) {
            emember indexer = compatible(mod, r, null, A_FLAG_INDEX, args); /// we need to update emember model to make all function members exist in an array
            validate(indexer, "%o: no suitable indexing method", r->name);
            index_expr = e_fn_call(mod, indexer->mdl, mem, args); // needs a target
        } else {
            index_expr = e_offset(mod, mem, args);
        }
        pop_state(mod, true);
        return index_expr;
    } else if (is_macro) {
        bool is_functional = next_is(mod, "(");
        array args = array(alloc, 32);
        if (is_functional) {
            consume(mod);
            while (!next_is(mod, ")")) {
                int next;
                array arg = read_arg(mod, mod->tokens, mod->cursor, &next);
                validate(arg, "macro expansion failed");
                if (arg)
                    push(args, arg);
                mod->cursor = next;
            }
            validate(read_if(mod, ")"), "expected parenthesis to end macro fn call");
        }
        token cur = peek(mod);

        // read arguments, and call macro
        macro m = mem->mdl;
        array tokens = macro_expand(m, is_functional ? args : null, null);
        push_state(mod, tokens, 0); // set these tokens as the parser state, run parse, then return the statement
        enode res = parse_expression(mod, null);
        pop_state(mod, false);
        print_tokens(mod, "after m");
        pop_state(mod, true);
        return res;

    } else if (mem) {
        if (mem->is_func || mem->is_type) {
            validate(next_is(mod, "["), "expected [ to initialize type");
            array expr = read_within(mod);
            mem = typed_expr(mod, mem->mdl, expr); // this, is the construct
        } else if (mod->in_ref || mem->literal) {
            mem = mem; // we will not load when ref is being requested on a emember
        } else {
            // member in isolation must load its value
            mem = e_load(mod, mem, null); // todo: perhaps wait to AddFunction until they are used; keep the emember around but do not add them until they are referenced (unless we are Exporting the import)
        }
    }
    pop_state(mod, mem != null);
    return mem;
}

/// parses emember args for a definition of a function
eargs parse_args(silver mod) {
    validate(read_if(mod, "["), "parse-args: expected [");
    eargs args = eargs(mod, mod);

    if (!next_is(mod, "]")) {
        push(mod, args); // if we push null, then it should not actually create debug info for the members since we dont 'know' what type it is... this wil let us delay setting it on function
        int statements = 0;
        for (;;) {
            /// we expect an arg-like statement when we are in this context (eargs)
            if (next_is(mod, "...")) {
                consume(mod);
                args->is_ext = true;
                continue;
            }
            parse_statement(mod);
            statements++;
            if (next_is(mod, "]"))
                break;
        }
        consume(mod);
        pop(mod);
        validate(statements == len(args->members), "argument parser mismatch");
    } else {
        consume(mod);
    }
    return args;
}

static enode auto_ref(enode expr, model expect) {
    silver mod = expr->mod;
    int rlevel_expect = ref_level(expect);
    int rlevel_mem    = ref_level(expr->mdl);
    if (rlevel_expect == (rlevel_mem + 1) && !mod->in_ref) {
        expr = e_addr_of(mod, expr, expr->mdl);
    }
    return expr;
}

enode silver_parse_ternary(silver mod, enode expr) {
    if (!read_if(mod, "?")) return expr;
    enode expr_true  = parse_expression(mod, null);
    enode expr_false = parse_expression(mod, null);
    return e_ternary(mod, expr, expr_true, expr_false);
}

// with constant literals, this should be able to merge the nodes into a single value
enode silver_parse_assignment(silver mod, emember mem, string oper) {
    validate(isa(mem) == typeid(enode) || !mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    mod->in_assign = mem;
    enode   L       = mem;
    enode   R       = parse_expression(mod, mem->mdl); /// getting class2 as a struct not a pointer as it should be. we cant lose that pointer info
    if (!mem->mdl) {
        mem->is_const = eq(oper, ":");
        set_model(mem, R->mdl); /// this is erroring because no 
        if (mem->literal)
            drop(mem->literal);
        mem->literal = hold(R->literal);
    } else {
        // we 'assign' to a member that has a reference+1 on it related to the schema
        //R = auto_ref(R, mem->mdl);
    }
    validate(contains(operators, oper), "%o not an assignment-operator");
    string op_name = get(operators, oper);
    string op_form = form(string, "_%o", op_name);
    OPType op_val  = e_val(OPType, cstring(op_form));
    enode  result  = e_op(mod, op_val, op_name, L, R);
    mod->in_assign = null;
    return result;
}

enode cond_builder_ternary(silver mod, array cond_tokens, A unused) {
    push_state(mod, cond_tokens, 0);
    enode cond_expr = parse_expression(mod, null);
    pop_state(mod, false);
    return cond_expr;
}

enode expr_builder_ternary(silver mod, array expr_tokens, A unused) {
    push_state(mod, expr_tokens, 0);
    enode exprs = parse_expression(mod, null);
    pop_state(mod, false);
    return exprs;
}

enode cond_builder(silver mod, array cond_tokens, A unused) {
    push_state(mod, cond_tokens, 0);
    enode cond_expr = parse_expression(mod, null);
    pop_state(mod, false);
    return cond_expr;
}

enode expr_builder(silver mod, array expr_tokens, A unused) {
    push_state(mod, expr_tokens, 0);
    enode exprs = parse_statements(mod, true);
    pop_state(mod, false);
    return exprs;
}

// we must have separate if/ifdef, since ifdef does not push the variable scope (it cannot by design)
// being adaptive to this based on constant status is too ambiguous

// this needs to perform const expressions (test this; also rename to something like if-const)
enode parse_ifdef_else(silver mod) {
    bool  require_if   = true;
    bool  one_truth    = false;
    bool  expect_last  = false;
    enode  statements   = null;

    while (true) {
        validate(!expect_last, "continuation after else");
        bool  is_if  = read_if(mod, "ifdef") != null;
        validate(is_if && require_if || !require_if, "expected if");
        enode n_cond = is_if ? parse_expression(mod, null) : null;
        array block  = read_body(mod);

        if (n_cond) {
            bool prev      = mod->in_const;
            mod->in_const  = true;
            //push_state(mod, cond, 0);
            //n_cond = parse_expression(mod, null);
            A const_v = n_cond->literal;
            //pop_state(mod, false);
            mod->in_const  = prev;
            if (!one_truth && const_v && cast(bool, const_v)) { // we need to make sure that we do not follow anymore in this block!
                push_state(mod, block, 0); // are we doing that?
                statements = parse_statements(mod, false);
                pop_state(mod, false);
                one_truth = true; /// we passed the condition, so we cannot enter in other blocks.
            }
        } else if (!one_truth) {
            validate(!is_if, "if statement incorrect");
            push_state(mod, block, 0);
            statements = parse_statements(mod, false); /// make sure parse-statements does not use its own members
            pop_state(mod, false);
            expect_last = true;
        }
        if (!is_if)
            break;
        bool next_else = read_if(mod, "else") != null;
        if (!next_else)
            break;
        require_if = false;
    }
    return statements;
}

/// parses entire chain of if, [else-if, ...] [else]
// if the cond is a constant evaluation then we do not build the condition in with LLVM build, but omit the blocks that are not used
// and only proceed
enode parse_if_else(silver mod) {
    bool  require_if   = true;
    array tokens_cond  = array(32);
    array tokens_block = array(32);
    while (true) {
        bool is_if  = read_if(mod, "if") != null;
        validate(is_if && require_if || !require_if, "expected if");
        array cond  = is_if ? read_within(mod) : array();
        array block = read_body(mod);
        verify(block, "expected body");
        push(tokens_cond,  cond);
        push(tokens_block, block);
        if (!is_if)
            break;
        bool next_else = read_if(mod, "else") != null;
        require_if = false;
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    return e_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}

enode parse_do_while(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

bool is_model(silver mod) {
    token  k = peek(mod);
    emember m = lookup2(mod, k, null);
    return m && m->is_type;
}

// these are for public, intern, etc; A-Type enums, not someting the user defines in silver context
i32 read_enum(silver mod, i32 def, AType etype) {
    for (int m = 1; m < etype->member_count; m++) {
        member enum_v = &etype->members[m];
        if (read_if(mod, enum_v->name))
            return *(i32*)enum_v->ptr;
    }
    return def;
}

path module_path(silver mod, string name) {
    cstr exts[] = { "sf", "sr" };
    path res    = null;
    path cw     = path_cwd();
    for (int  i = 0; i < 2; i++) {
        path  r = f(path, "%o/%o.%s", cw, name, exts[i]);
        if (file_exists("%o", res)) {
            res = r;
            break;
        }
    }
    return res;
}

#undef find_member

AType next_is_keyword(silver mod, member* fn) {
    token t = peek(mod);
    if  (!t)  return null;
    if  (!isalpha(t->chars[0])) return null;
    AType f = A_find_type((cstr)t->chars);
    if (f && inherits(f, typeid(model)) && (*fn = find_member(f, A_FLAG_SMETHOD, "parse", true)))
        return f;
    return null;
}

/// called after : or before, where the user has access
emember silver_read_def(silver mod) {
    bool   is_import = next_is(mod, "import");
    member parse_fn  = null;
    AType  is_type   = next_is_keyword(mod, &parse_fn);
    model  is_class  = !is_type ? next_is_class(mod, false) : null;
    bool   is_struct = next_is(mod, "struct");
    bool   is_enum   = next_is(mod, "enum");
    bool   is_alias  = next_is(mod, "alias");

    if (!is_type && !is_class && !is_struct && !is_enum && !is_alias)
        return null;

    if (is_type) {
        typedef enode n;
        n (*func)(silver) = parse_fn->ptr;
        validate(func, "expected parse fn on member");
        emember mem = func(mod);
        print_tokens(mod, "after-keyword");
        return mem;
    }

    consume(mod);
    string n = read_alpha(mod);
    validate(n, "expected alpha-numeric identity, found %o", next(mod));

    model mtop = instanceof(mod->top, typeid(import)) ? (model)mod : mod->top;
    emember mem = emember(mod, mod, name, n, context, mtop);

    if (is_class || is_struct) {
        validate(mtop == mod, "expected record definition at module level");
        array schema = array();
        /// read class schematics
        array meta = null;
        if (is_class && next_is(mod, "[")) {
            consume(mod);
            while (!next_is(mod, "]")) {
                model mdl = instanceof(read_model(mod, null), typeid(model));
                validate(mdl, "expected model name in meta for %o", is_class->name);
                push(meta, mdl);
                // these we now give to the type when we make it (they are runtime args
                // must also handle defining a type alias this way)
                // for those, i believe its a subclass now; having alias and subclass is very redundant
            }
            consume(mod);
        }
        array body   = read_body(mod);

        /// todo: call build_record right after this is done
        push(mod, mod); // make sure we are not in an import space
        if (is_class) {
            validate(!is_class || !is_class->is_ref, "unexpected pointer");
            mem->mdl = Class    (mod, mod, parent, is_class, name, n, body, body, meta, meta);
        } else
            mem->mdl = structure(mod, mod, name, n, body, body);
        pop(mod);

    } else if (is_enum) {
        model store = null, suffix = null;
        if (read_if(mod, ",")) {
            store  = instanceof(read_model(mod, null), typeid(model));
            suffix = instanceof(read_model(mod, null), typeid(model));
            validate(store, "invalid storage type");
        }
        array enum_body = read_body(mod);
        print_all(mod, "enum-tokens", enum_body);
        validate(len(enum_body), "expected body for enum %o", n);

        mem->mdl  = enumeration(
            mod, mod,  src, store,  name, n);
        
        push_state(mod, enum_body, 0);
        push(mod, mem->mdl);

        store = mem->mdl->src;

        /// verify model is a primitive type
        AType atype = isa(mem->mdl->src->src);
        validate(atype && (atype->traits & A_TRAIT_PRIMITIVE),
            "enumeration can only be based on primitive types (i32 default)");

        i64  i64_v = 0;
        f64  f64_v = 0;
        f32  f32_v = 0;
        i64* i64_value = store == emodel("i64") ? &i64_v : null;
        i64* i32_value = store == emodel("i32") ? &i64_v : null;
        f64* f64_value = store == emodel("f64") ? &f64_v : null;
        f32* f32_value = store == emodel("f32") ? &f32_v : null;
        A value = (A)(i64_value ? (A)i64_value : i32_value ? (A)i32_value :
                                f64_value ? (A)f64_value : (A)f32_value);
        
        while (true) {
            token e = next(mod);
            if  (!e) break;
            A v = null;
            
            if (read_if(mod, ":")) {
                v = read_node(mod, atype, null); // i want this to parse an entire literal, with operations
                //validate(v && isa(v) == atype, "expected numeric literal");
            } else
                v = primitive(atype, value); /// this creates i8 to i64 data, using &value i64 addr

            array aliases = null;
            while(read_if(mod, ",")) {
                if (!aliases) aliases = array(32);
                string a = read_alpha(mod);
                validate(a, "could not read identifier");
                push(aliases, a);
            }
            if (eq(e, "none")) {
                int test2 = 2;
                test2    += 2;
            }
            emember emem = emember(
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
        finalize(mem->mdl);
    } else {
        validate(is_alias, "unknown error");
        mem->mdl = model(mod, mod, src, mem->mdl, name, mem->name);
    }
    mem->is_type = true;
    validate(mem && (is_import || len(mem->name)),
        "name required for model: %s", isa(mem->mdl)->name);
    register_member(mod, mem, true);
    return mem; // for these keywords, we have one emember registered (not like our import)
}

enode parse_statement(silver mod) {
    token     t            = peek(mod);
    record    rec          = instanceof(mod->top, typeid(record));
    fn        f            = context_model(mod, typeid(fn));
    eargs     args         = instanceof(mod->top, typeid(eargs));
    silver    module       = (f && f->is_module_init) ? mod : null;
    bool      is_func_def  = false;
    string    assign_type  = null;
    OPType    assign_enum  = 0;
    bool      assign_const = false;

    mod->last_return = null;
    mod->expr_level = 0;

    if (!module) {
        if (next_is(mod, "no-op"))  return enode(mdl, emodel("none"));
        if (next_is(mod, "return")) {
            mod->expr_level++;
            mod->last_return = parse_return(mod);
            return mod->last_return;
        }
        if (next_is(mod, "break"))  return parse_break(mod);
        if (next_is(mod, "for"))    return parse_for(mod);
        if (next_is(mod, "while"))  return parse_while(mod);
        if (next_is(mod, "if"))     return parse_if_else(mod);
        if (next_is(mod, "ifdef"))
            return parse_ifdef_else(mod);
        if (next_is(mod, "do"))     return parse_do_while(mod);
    } else {
        if (next_is(mod, "ifdef"))
            return parse_ifdef_else(mod);
    }
    mod->left_hand = true;

    // lets read ahead to get the type here, so read-node has less scope to cover
    enode   e = read_node(mod, null, null); /// at module level, supports keywords
    return e;
}

enode parse_statements(silver mod, bool unique_members) {
    mod->statement_count++;

    if (unique_members)
        push(mod, new(statements, mod, mod));
    
    enode  vr = null;
    while(peek(mod)) {
        print_tokens(mod, "parse_statements");
        vr = parse_statement(mod);
    }
    if (unique_members)
        pop(mod);
    return vr;
}

fn parse_fn(silver mod, AFlag member_type, A ident, OPType assign_enum) {
    model      rtype = null;
    A     name  = instanceof(ident, typeid(token));
    array      body  = null;
    if (!name) name  = instanceof(ident, typeid(string));
    bool     is_cast = member_type == A_FLAG_CAST;

    if (is_cast) {
        print_tokens(mod, "parse_fn before read_model");
        rtype = read_model(mod, &body);
    } else if (!name) {
        name = read_alpha(mod);
        validate(name, "expected alpha-identifier");
    }

    record rec = context_model(mod, typeid(record));
    validate(rec, "expected rec"); // todo: revert to record here
    eargs args = eargs();
    validate((is_cast || next_is(mod, "[")) || next_is(mod, "->"), "expected function args [");
    bool r0 = read_if(mod, "->") != null;
    if (!r0 && next_is(mod, "[")) {
        args = parse_args(mod);
    }
    
    bool single_expr = false;
    if ( r0 || read_if(mod, "->")) {
        rtype = read_model(mod, &body); 
        if (body && len(body)) {
            // if body is set, then parse_fn will return the typed_expr call, with model of rtype, and expr of body
            single_expr = true;
        }
        validate(rtype, "type must proceed the -> keyword in fn");
    } else
        rtype = emodel("none");

    if (!rtype)
        rtype = emodel("none");
    
    // check if using ai model
    codegen cgen = null;
    if (read_if(mod, "using")) {
        verify(!body, "body already defined in type[expr]");
        token codegen_name = read_alpha(mod);
        verify(codegen_name, "expected codegen-identifier after 'using'");
        cgen = get(mod->codegens, codegen_name);
        verify(cgen, "codegen identifier not found: %o", codegen_name);
    }
    
    validate(rtype, "rtype not set, void is something we may lookup");
    if (!name) {
        validate((member_type & A_FLAG_CAST) != 0, "with no name, expected cast");
        name = form(string, "cast_%o", rtype->name);
    }
    
    bool is_static = (member_type & A_FLAG_SMETHOD) != 0;
    fn f = fn(
        mod,      mod,     name,   name, function_type, member_type, extern_name, f(string, "%o_%o", rec->name, name),
        instance, is_static ? null : rec,
        rtype,    rtype,   single_expr, single_expr,
        args,     args,    body,   (body && len(body)) ? body : read_body(mod),
        cgen,     cgen);
    
    return f;
}

void silver_incremental_resolve(silver mod) {
    bool in_top = mod->in_top;
    mod->in_top = false;
    
    /// finalize included structs
    pairs(mod->members, i) {
        emember mem = i->value;
        model base = mem->mdl->is_ref ? mem->mdl->src : mem->mdl;
        if (!mem->mdl->finalized && instanceof(base, typeid(record)) && base->imported_from)
            finalize(base);
    }

    /// finalize imported C functions, which use those structs perhaps literally in argument form
    pairs(mod->members, i) {
        emember mem = i->value;
        model  mdl = mem->mdl;
        if (!mdl->finalized && instanceof(mem->mdl, typeid(fn)) && mdl->imported_from) {
            finalize(mem->mdl);
        }
    }

    /// process/finalize all remaining member models 
    /// calls process sub-procedure and poly-based finalize
    pairs(mod->members, i) {
        emember mem = i->value;
        record rec = instanceof(mem->mdl, typeid(record));
        Class  cl  = instanceof(mem->mdl, typeid(Class));
        if ((!cl || !cl->is_abstract) && rec && !rec->parsing && !rec->finalized && !rec->imported_from) {
            build_record(mod, rec);
        }
    }

    // finally, process functions (last step in parsing)
    pairs(mod->members, i) {
        emember mem = i->value;
        if (mem != mod->mem_init && !mem->mdl->finalized && instanceof(mem->mdl, typeid(fn)) && !mem->mdl->imported_from) {
            build_fn(mod, mem->mdl, null, null);
        }
    }

    mod->in_top = in_top;
}

void silver_parse(silver mod) {
    /// im a module!
    emember mem_init = initializer(mod); // publish initializer
    mod->in_top = true;
    map members = mod->members;

    while (peek(mod)) {
        print("token origin %p", mod->tokens);
        print_tokens(mod, "pre-statement");
        enode res = parse_statement(mod);
        validate(res, "unexpected token found for statement: %o", peek(mod));
        print_tokens(mod, "post-statement");
        incremental_resolve(mod);
    }
    mod->in_top = false;

    finalize(mem_init->mdl);
}

string git_remote_info(path path, string *out_service, string *out_owner, string *out_project) {
    // run git command
    string cmd = f(string, "git -C %s remote get-url origin", path->chars);
    string remote = command_run(cmd);

    if (!remote || !remote->len)
        error("git_remote_info: failed to get remote url");

    cstr url = remote->chars;

    // strip trailing newline(s)
    for (int i = remote->len - 1; i >= 0 && (url[i] == '\n' || url[i] == '\r'); i--)
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
    if (!domain_end) error("git_remote_info: malformed URL");
    *domain_end = '\0';

    // next part: owner/repo
    cstr next = domain_end + 1;
    owner = next;

    cstr slash = strchr(owner, '/');
    if (!slash) error("git_remote_info: missing owner/repo");
    *slash = '\0';
    repo = slash + 1;

    // remove .git if present
    cstr dot = strrchr(repo, '.');
    if (dot && strcmp(dot, ".git") == 0)
        *dot = '\0';
    
    if (!*out_service) *out_service = string(domain);
    if (!*out_owner)   *out_owner   = string(owner);
    if (!*out_project) *out_project = string(repo);

    return remote; // optional if you want to keep full URL
}

silver silver_with_path(silver mod, path external) {
    mod->source = hold(external);
    return mod;
}

i64 path_wait_for_change(path, i64, i64);

// implement watcher now
void silver_init(silver mod) {
    mod->defs     = map(hsize, 8);
    mod->codegens = map(hsize, 8);
    mod->import_cache = map(hsize, 8);

    bool is_watch = mod->watch; // -w or --watch

    if (!mod->source) {
        fault("required argument: source-file");
    }
    if ( mod->source)  mod->source  = absolute(mod->source);
    if (!mod->install) {
        cstr import = getenv("IMPORT");
        if (import)
            mod->install = f(path, "%s", import);
        else {
            path   exe = path_self();
            path   bin = parent(exe);
            path   install = absolute(f(path, "%o/..", bin));
            mod->install = install;
        }
    }
    build_info(mod, mod->install);
    mod->project_path = parent(mod->source);

    verify(dir_exists ("%o", mod->install), "silver-import location not found");
    verify(len        (mod->source),       "no source given");
    verify(file_exists("%o", mod->source),  "source not found: %o", mod->source);

    print("source pointer = %p", &mod->source);
    print("source is %o", mod->source);
    verify(exists(mod->source), "source (%o) does not exist", mod->source);
    verify(mod->std == language_silver || eq(ext(mod->source), "sf"),
        "only .sf extension supported; specify --std silver to override");
    
    cstr _SRC    = getenv("SRC");
    cstr _DBG    = getenv("DBG");
    cstr _IMPORT = getenv("IMPORT");
    verify(_IMPORT, "silver requires IMPORT environment");

    mod->mod        = mod;
    mod->spaces     = array(32);
    mod->parse_f    = parse_tokens;
    mod->parse_expr = parse_expression;
    mod->read_model = read_model;
    mod->src_loc = absolute(path(_SRC ? _SRC : "."));
    verify(dir_exists("%o", mod->src_loc), "SRC path does not exist");

    path        af = path_cwd();
    path   install = path(_IMPORT);
    git_remote_info(af, &mod->git_service, &mod->git_owner, &mod->git_project);
    
    bool retry   = false;
    i64  mtime   = modified_time(mod->source);
    do {
        if (retry) {
            hold_members(mod);
            A_recycle();
            mtime = path_wait_for_change(mod->source, mtime, 0);
            print("rebuilding...");
            drop(mod->tokens);
            drop(mod->stack);
            reinit_startup(mod);
        }
        retry = false;
        mod->tokens  = parse_tokens(mod, mod->source);
        mod->stack   = array(4);

        attempt() {
            path c_file   = f(path, "%o/%o.c",  mod->project_path, stem(mod->source));
            path cc_file  = f(path, "%o/%o.cc", mod->project_path, stem(mod->source));
            path files[2] = { c_file, cc_file };
            for (int i = 0; i < 2; i++)
                if (exists(files[i])) {
                    if (!mod->implements) mod->implements = array(2);
                    push(mod->implements, files[i]);
                }
            parse(mod);
            build(mod);
        } on_error() {
            retry = is_watch;
        }
        finally()
    } while (retry);
}

silver silver_load_module(silver mod, path uri) {
    silver mod_load = silver(
        source,  mod->source,
        install, mod->install,
        name,    stem(uri));
    return mod_load;
}

static i64 ancestor_mod = 0;

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
static symbol lib_pre  = "lib"; static symbol lib_ext  = ".so";     static symbol app_ext  = "";        static symbol platform = "linux";
#elif defined(_WIN32)
static symbol lib_pre  = "";    static symbol lib_ext  = ".dll";    static symbol app_ext  = ".exe";    static symbol platform = "windows";
#elif defined(__APPLE__)
static symbol lib_pre  = "lib"; static symbol lib_ext  = ".dylib";  static symbol app_ext  = "";        static symbol platform = "darwin";
#endif

static bool is_dbg(import t, string query, cstr name, bool is_remote) {
    cstr  dbg = (cstr)query->chars; // getenv("DBG");
    char  dbg_str[PATH_MAX];
    char name_str[PATH_MAX];
    sprintf(dbg_str, ",%s,", dbg);
    sprintf(name_str, "%s,", name_str);
    int   name_len    = strlen(name);
    int   dbg_len     = strlen(dbg_str);
    int   has_astrick = 0;
    
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

none sync_tokens(import t, path build_path, string name) {
    path t0 = form(path, "%o/import-token", build_path);
    path t1 = form(path, "%o/tokens/%o", t->mod->install, name);
    struct stat build_token, installed_token;
    /// create token pair (build & install) to indicate no errors during config/build/install
    cstr both[2] = { cstring(t0), cstring(t1) };
    for (int i = 0; i < 2; i++) {
        FILE* ftoken = fopen(both[i], "wb");
        fwrite("im-a-token", 10, 1, ftoken);
        fclose(ftoken);
    }
    int istat_build   = stat(cstring(t0), &build_token);
    int istat_install = stat(cstring(t1), &installed_token);
    struct utimbuf times;
    times.actime  = build_token.st_atime;  // access time
    times.modtime = build_token.st_mtime;  // modification time
    utime(cstring(t1), &times);
}

string serialize_environment(map environment, bool b_export);

static array headers(path dir) {
    array all = ls(dir, null, false);
    array res = array();
    each (all, path, f) {
        string e = ext(f);
        if (len(e) == 0 || cmp(e, ".h") == 0)
            push(res, f);
    }
    drop(all);
    return res;
}

static int filename_index(array files, path f) {
    string fname = filename(f);
    int    index = 0;
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
    path par = parent(a);
    string st = stem(par);
    if (eq(st, "checkout")) {
        return true;
    }
    return false;
}

enode export_share(silver mod) {
    string dir         = read_alpha(mod);
    path   export_from = f(path, "%o/%o", mod->project_path, dir);

    validate(dir_exists("%o", export_from),
        "dir does not exist for export: %o", export_from);

    // if this is debug, we want to rsync everything
    array res_dir = ls(export_from, null, false); // no pattern or recursion
    
    // create share folder for dest
    path share_dir = f(path, "%o/share/%o",
        mod->install, mod->name);
    make_dir(share_dir);

    // iterate through directory given [ this should support individual files, and patterns ]
    each (res_dir, path, res) {
        string rtype  = filename(res);
        array  rfiles = is_dir(res) ? ls(res, null, false) : null;
        if (rfiles) {
            path rtype_dir  = f(path, "%o/%o",
                share_dir, rtype);
            make_dir(rtype_dir);
            each (rfiles, path, res) {
                // create symlink at dest:
                //      install/share/our-target-name/each-resource-dir/each-file ->
                //              im->import_path/share/each-resource-dir/each-resource-file
                string fn  = filename(res);
                path   src = f(path, "%o/%o/%o", export_from, rtype, fn);
                path   dst = f(path, "%o/%o/%o", share_dir,   rtype, fn);
                if (file_exists("%o", dst) && !is_symlink(dst))
                    continue; // being used by the user (needs an option for release/packaging mode here)
                bool needs_link = !eq(src, dst);
                if (needs_link) {
                    exec("rm -rf %o", dst);
                    verify(!file_exists("%o", dst), "cannot create symlink");
                    exec("ln -s %o %o", src, dst);
                }
            }
        } else {
            string fn  = filename(res);
            path   src = f(path, "%o/%o", export_from, fn);
            path   dst = f(path, "%o/%o", share_dir,   fn);
            if (!(file_exists("%o", dst) && !is_symlink(dst))) {
                exec("rm -rf %o", dst);
                verify(!file_exists("%o", dst), "cannot create symlink");
                exec("ln -s %o %o", src, dst);
            }
        }
    }
    return null;
}

enode export_parse(silver mod) {
    validate(read_if(mod, "export"), "expected export");

    if (read_if(mod, "share"))
        return export_share(mod);

    fault("export mode not supported: %o", next(mod));
    return null;
}

array compact_tokens(array tokens) {
    if (len(tokens) <= 1) return tokens;

    array  res     = array(32);
    int    ln      = len(tokens);
    token  prev    = null;
    token  current = null;

    for (int i = 1; i < ln; i++) {
        token t    = tokens->elements[i];
        token prev = tokens->elements[i - 1];
        if (!current) current = copy(prev);

        if (prev->line == t->line && (prev->column + prev->len) == t->column) {
            concat(current, t);
            current->column += t->len;
        } else {
            push(res, current);
            current = copy(t);
        }
    }
    if (current)
        push(res, current);
    return res;
}

string model_keyword() { return null; }

static array import_build_commands(array input, symbol sym) {
    array   res        = array(alloc, 32);
    int     token_line = -1;
    string  cmd        = null;

    each (input, token, t) {
        bool is_cmd = eq(t, sym);
        if (is_cmd || (t->line == token_line)) {
            if (!is_cmd) {
                if (!cmd) cmd = string(alloc, 32);
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

static string import_config(array input) {
    string config = string(alloc, 128);
    int token_line = -1;
    each (input, token, t) {
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

static string import_env(array input) {
    string env = string(alloc, 128);
    each (input, string, t) {
        if (isalpha(t->chars[0]) && index_of(t, "=") >= 0) {
            if (len(env))
                append(env, " ");
            concat(env, t);
        }
    }
    return env;
}

static string import_libs(array input) {
    string libs = string(alloc, 128);
    each (input, string, t) {
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
    silver mod       = im->mod;
    path   install   = mod->install;
    string s         = cast(string, uri);
    num    sl        = rindex_of(s, "/");   validate(sl >= 0, "invalid uri");
    string name      = mid(s, sl + 1, len(s) - sl - 1);
    path   project_f = f(path, "%o/checkout/%o", install, name);
    bool   debug     = false;
    string config    = interpolate(conf, mod);

    validate(command_exists("git"), "git required for import feature");

    // we need to check if its full hash
    validate(len(commit) == 40 || is_branchy(commit),
        "commit-id must be a full SHA-1 hash or a branch name (short-hand does not work for depth=1 checkouts)");

    // checkout or symlink to src
    if (!dir_exists("%o", project_f)) {
        path src_path = f(path, "%o/%o", mod->src_loc, name);
        if (dir_exists("%o", src_path)) {
            project_f = src_path;
            vexec("ln -s %o/%o %o", mod->src_loc, name, name);
        } else {
            vexec("init",     "git init %o",                        project_f);
            vexec("remote",   "git -C %o remote add origin %o",     project_f, uri);
            vexec("fetch",    "git -C %o fetch origin %o",          project_f, commit);
            vexec("checkout", "git -C %o reset --hard FETCH_HEAD",  project_f);
        }
    }

    // we build to another folder, not inside the source, or checkout
    path build_f   = f(path, "%o/%s/%o", install, debug ? "debug" : "build", name);
    path rust_f    = f(path, "%o/Cargo.toml",     project_f);
    path meson_f   = f(path, "%o/meson.build",    project_f);
    path cmake_f   = f(path, "%o/CMakeLists.txt", project_f);
    path silver_f  = f(path, "%o/build.sf",       project_f);
    path gn_f      = f(path, "%o/BUILD.gn",       project_f);
    bool is_rust   = file_exists("%o", rust_f);
    bool is_meson  = file_exists("%o", meson_f);
    bool is_cmake  = file_exists("%o", cmake_f);
    bool is_gn     = file_exists("%o", gn_f);
    bool is_silver = file_exists("%o", silver_f);
    path token     = f(path, "%o/silver-token", build_f);
    
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
            string icmd = interpolate(cmd, mod);
            command_exec((command)icmd);
        }
        cd(cw);
    }

    if (is_cmake) { // build for cmake
        cstr build = debug ? "Debug" : "Release";
        string opt = mod->isysroot ? f(string, "-DCMAKE_OSX_SYSROOT=%o", mod->isysroot) : string("");

        vexec("configure",
            "%o cmake -B %o -S %o %o -DCMAKE_INSTALL_PREFIX=%o -DCMAKE_BUILD_TYPE=%s %o",
                env, build_f, project_f, opt, install, build, config);

        vexec("build",   "%o cmake --build %o -j16", env, build_f);
        vexec("install", "%o cmake --install %o",    env, build_f);
    }
    else if (is_meson) { // build for meson
        cstr build = debug ? "debug" : "release";

        vexec("setup",
            "%o meson setup %o --prefix=%o --buildtype=%s %o",
                env, build_f, install, build, config);

        vexec("compile", "%o meson compile -C %o", env, build_f);
        vexec("install", "%o meson install -C %o", env, build_f);
    }
    else if (is_gn) {
        cstr is_debug = debug ? "true" : "false";
        vexec("gen", "gn gen %o --args='is_debug=%s is_official_build=true %o'", build_f, is_debug, config);
        vexec("ninja", "ninja -C %o -j8", build_f);
    }
    else if (is_rust) { // todo: copy bin/lib after
        vexec("rust", "cargo build --%s --manifest-path %o/Cargo.toml --target-dir %o",
            debug ? "debug" : "release", project_f, build_f);
    } else if (is_silver) { // build for A-type projects
        silver sf = silver(source, silver_f);
        validate(sf, "silver module compilation failed: %o", silver_f);
    } else {
        /// build for automake
        if (file_exists("%o/autogen.sh",   project_f) || 
            file_exists("%o/configure.ac", project_f) || 
            file_exists("%o/configure",    project_f) ||
            file_exists("%o/config",       project_f)) {
            
            // fix common race condition with autotools
            if (!file_exists("%o/ltmain.sh", project_f))
                verify(exec("libtoolize --install --copy --force") == 0, "libtoolize");
            
            // common preference on these repos
            if (file_exists("%o/autogen.sh", project_f))
                verify(exec("(cd %o && bash autogen.sh)", project_f) == 0, "autogen");
             
            // generate configuration scripts if available
            else if (!file_exists("%o/configure", project_f) && file_exists("%o/configure.ac", project_f)) {
                verify(exec("autoupdate --verbose --force --output=%o/configure.ac %o/configure.ac",
                    project_f, project_f) == 0, "autoupdate");
                verify(exec("autoreconf -i %o",
                    project_f) == 0, "autoreconf");
            }

            // prefer pre/generated script configure, fallback to config
            path configure = file_exists("%o/configure", project_f) ?
                f(path, "./configure") : f(path, "./config");
            
            if (file_exists("%o/%o", project_f, configure)) {
                verify(exec("%o (cd %o && %o%s --prefix=%o %o)",
                    env,
                    project_f,
                    configure,
                    debug ? " --enable-debug" : "",
                    install,
                    config) == 0, "config script %o", configure);
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
            string icmd = interpolate(cmd, mod);
            command_exec((command)icmd);
        }
        cd(cw);
    }

    save(token, config, null);
}

// build with optional bc path; if no bc path we use the project file system
i32 silver_build(silver a) {
    path ll = null, bc = null;
    emit(a, &ll, &bc);
    verify(bc != null, "compilation failed");

    int  error_code = 0;
    path install = a->install;

    // simplified process for .bc case
    string name = stem(bc);
    verify(exec("%o/bin/llc -filetype=obj %o.ll -o %o.o -relocation-model=pic",
        install, name, name) == 0,
            ".ll -> .o compilation failed");
    string libs, cflags;

    // create libs, and describe in reverse order from import
    libs = string("");
    array rlibs = reverse(a->shared_libs);
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
    return 0;
}

bool silver_next_is_neighbor(silver mod) {
    token b = element(mod, -1);
    token c = element(mod,  0);
    return b->column + b->len == c->column;
}

string expect_alpha(silver mod) {
    token t = next(mod);
    verify (t && isalpha(*t->chars), "expected alpha identifier");
    return string(t->chars);
}

path is_module_dir(silver mod, string ident) {
    path dir = f(path, "%o/%o", mod->project_path, ident);
    if (dir_exists("%o", dir))
        return dir;
    return null;
}

// when we load silver files, we should look for and bind corresponding .c files that have implementation
// this is useful for implementing in C or other languages

path module_exists(silver mod, array idents) {
    string to_path = join(idents, "/");
    path sf = f(path, "%o/%o.sf", mod->project_path, to_path);
    return file_exists("%o", sf) ? sf : null;
}


enode import_parse(silver mod) {
    print_tokens(mod, "import parse");
    validate(next_is(mod, "import"), "expected import keyword");
    consume(mod);

    model current_top = mod->top;

    int    from          = mod->cursor;
    codegen cg           = null;
    string namespace     = null;
    array  includes      = array(32);
    array  module_paths  = array(32);
    path   local_mod     = null;

    // what differentiates a codegen from others, just class name?
    token t = peek(mod);
    AType is_codegen = null;
    token commit = null;
    string uri    = null;

    if (t && isalpha(t->chars[0])) {
        bool cont = false;
        string service    = mod->git_service;
        string user       = mod->git_owner;
        string project    = null;
        string aa         = expect_alpha(mod); // value of t
        string bb         = read_if(mod, ":") ? expect_alpha(mod) : null;
        string cc         = bb && read_if(mod, ":") ? expect_alpha(mod) : null;
        array  mpath      = null;
        AType  f          = A_find_type((cstr)aa->chars);

        if (f && inherits(f, typeid(codegen)))
            is_codegen = f;
        else if (next_is(mod, ".")) {
            while (read_if(mod, ".")) {
                if (!mpath) {
                    mpath = array(alloc, 32);
                    push(mpath, cc ? cc : bb ? bb : aa ? aa : (string)null);
                }
                string ident = read_alpha(mod);
                push(mpath, ident);
            }
        } else {
            mpath = array(alloc, 32);
            string f = cc ? cc : bb ? bb : aa ? aa : (string)null;
            if (index_of(f, ".") >= 0) {
                array sp = split(f, ".");
                array sh = shift(sp);
                push(mpath, sh);
            }
        }
        
        if (!is_codegen) {
            // read commit if given
            if (read_if(mod, "/")) commit = next(mod);
            
            if (aa && !bb && !commit) {
                local_mod = module_exists(mod, mpath);
                if (!local_mod) {
                    // push entire directory
                    string j = join(mpath, "/");
                    verify(dir_exists("%o", j), "module/directory not found: %o", j);
                    path f = f(path, "%o", j);
                    array dir = ls(f, string("*.sf"), false);
                    verify(len(dir), "no modules in directory %o", f);
                    each(dir, path, m)
                        push(module_paths, m);
                } else
                    push(module_paths, local_mod);
            } else if (aa && !bb) {
                verify(!local_mod, "unexpected import chain containing different methodologies");

                // contains commit, so logically cannot be a singular module
                // for commit # it must be a project for now, this is not a 
                // constraint that we directly need to mitigate, but it may 
                // be a version difference
                project     = aa;
                verify(!mpath || len(mpath) == 0, "unexpected path to module (expected 1st arg as project)");
            } else if (aa && !cc) {
                verify(!local_mod, "unexpected import chain containing different methodologies");
                user        = aa;
                project     = bb;
                //verify(!mpath || len(mpath) == 0, "unexpected path to module (expected 2nd arg as project)");
            } else {
                verify(!local_mod, "unexpected import chain containing different methodologies");
                user        = aa;
                project     = bb;
            }

            string path_str = string();
            if (len(mpath)) {
                string str_mpath = join(mpath, "/") ? cc       : string("");
                path_str  = len(str_mpath) ?
                    f(string, "blob/%o/%o", commit, str_mpath) : string("");
            }
            
            verify(project || local_mod, "could not decipher module references from import statement");

            if (local_mod) {
                uri = null;
                cont = read_if(mod, ",") != null;
                verify (!cont, "comma not yet supported in import (fn needs restructuring to create multiple imoprts in enode)");
            } else
                uri = f(string, "https://%o/%o/%o%s%o", service, user, project,
                    cast(bool, path_str) ? "/" : "", path_str);
        }
    }
    
    // includes for this import
    if (read_if(mod, "<")) {
        for (;;) {
            string f = read_alpha_any(mod);
            validate(f, "expected include");

            // we may read: something/is-a.cool\file.hh.h
            while (next_is_neighbor(mod) && (!next_is(mod, ",") && !next_is(mod, ">")))
                concat(f, next(mod));
            
            push(includes, f);

            if (!read_if(mod, ",")) {
                token n = read_if(mod, ">");
                validate(n, "expected '>' after include, list, of, headers");
                break;
            }
        }
    }

    array  c          = read_body(mod);
    array  all_config = compact_tokens(c);
    map    props      = map();

    print_tokens(mod, "before [");

    // this invokes import by git; a local repo may be possible but not very usable
    // arguments / config not stored / used after this
    if (next_is(mod, "[") || next_indent(mod)) {
        array b = read_body(mod);
        int index = 0;
        while (index < len(b)) {
            verify(index - len(b) >= 3, "expected prop: value for codegen object");
            token prop_name  = b->elements[index++];
            token col        = b->elements[index++];
            token prop_value = b->elements[index++];
            // this will not work for reading {fields}
            // trouble is the merging of software with build config, and props we set in module.
            
            verify(eq(col, ":"), "expected prop: value for codegen object");
            set(props, string(prop_name->chars), string(prop_value->chars));
        }
    }
    
    silver external = null;

    if (uri) {
        checkout(mod, uri, commit,
            import_build_commands(all_config, ">"),
            import_build_commands(all_config, ">>"),
            import_config(all_config),
            import_env(all_config));
        each(all_config, string, t)
            if (starts_with(t, "-l"))
                push(mod->shared_libs, mid(t, 2, len(t) - 2));
    } else if (local_mod)
        external = silver(local_mod);
    else if (is_codegen) {
        cg = construct_with(is_codegen, props, null);
    }
    
    if (next_is(mod, "as")) {
        consume(mod);
        namespace = hold(read_alpha(mod));
        validate(namespace, "expected alpha-numeric %s",
            is_codegen ? "alias" : "namespace");
    } else if (is_codegen) {
        namespace = hold(string(is_codegen->name));
    }

    // hash. for cache.  keep cache warm
    int to = mod->cursor;
    array tokens = array(alloc, to - from + 1);
    for (int i = from; i < to; i++) {
        token t = mod->tokens->elements[i];
        push(tokens, t);
    }

    import mdl = get(mod->import_cache, tokens);
    bool has_cache = mdl != null;

    if (!has_cache) {
        mdl = import(
            mod,        mod,
            is_user,    true,
            codegen,    cg,
            external,   external,
            tokens,     tokens);

        set(mod->import_cache, tokens, mdl);
    }
    
    emember mem = emember(
        mod,    mod,
        name,   namespace,
        mdl,    mdl);
    set_model  (mem, mdl);
    register_member(mod, mem, true);
    
    if (namespace)
        mdl->name = namespace;
    
    if (!has_cache && !is_codegen) {
        push(mod, mdl);
    
        // include each, collecting the clang instance for which we will invoke macros through
        each (includes, string, inc) {
            clang_cc instance;
            include(mod, inc, &instance);
            set(mod->instances, inc, instance);
        }

        each(module_paths, path, m) {
            A_import(mod, m);
        }
        
        // should only do this if its a global import and has no namespace
        if (!namespace)
            pop(mod);
    }

    if (is_codegen) {
        string name = mdl->name ? (string)mdl->name : string(is_codegen->name);
        set(mod->codegens, name, mdl->codegen);
    }
    
    return mem;
}


/*
A request2(uri url, map args) {
    map     st_headers   = new(map);
    A       null_content = null;
    map     headers      = contains(args, "headers") ? (map)get (args, "headers") : st_headers;
    A       content      = contains(args, "content") ? get (args, "content") : null_content;
    web     type         = contains(args, "method")  ? e_val(web, get(args, "method")) : web_Get;
    uri     query        = url;

    query->mtype = type;
    verify(query->mtype != web_undefined, "undefined web method type");

    sock client = sock(query);
    print("(net) request: %o", url);
    if (!connect_to(client))
        return null;

    // Send request line
    string method = e_str(web, query->mtype);
    send_object(client, f(string, "%o %o HTTP/1.1\r\n", method, query->query));

    // Default headers
    if (!contains(headers, "User-Agent"))      set(headers, "User-Agent", "silver");
    if (!contains(headers, "Accept"))          set(headers, "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*;q=0.8");
    if (!contains(headers, "Accept-Language")) set(headers, "Accept-Language", "en-US,en;q=0.9");
    if (!contains(headers, "Accept-Encoding")) set(headers, "Accept-Encoding", "gzip, deflate, br");
    if (!contains(headers, "Host"))            set(headers, "Host", query->host);

    message request = message(content, content, headers, headers, query, query);
    write(request, client);

    message response = message(client);
    close(client);

    return response;
}*/


// todo: integrate the ssl into chatgpt type
// implement 'using' keyword to facilitate this

int main(int argc, cstrs argv) {
    A_engage(argv);
    silver mod = silver(argv);
    return 0;
}

define_enum  (build_state)
define_enum  (language)



// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, fn f, array query) {
    fault("must subclass codegen for usable code generation");
    return null;
}

// design-time for dictation
array read_dictation(silver mod, array input) {
    // we want to read through [ 'tokens', image[ 'file.png' ] ]
    // also 'token here' 'and here' as two messages
    array result = array();

    push_state(mod, input, 0);
    while (read_if(mod, "[")) {
        array content = array();
        while (peek(mod) && !next_is(mod, "]")) {
            if (read_if(mod, "file")) {
                verify (read_if(mod, "["), "expected [ after file");
                string file = read_literal(mod, typeid(string));
                verify (file, "expected 'path' of file in resources");
                path share = path_share_path();
                path fpath = f(path, "%o/%o", share, file);
                verify(exists(fpath), "path does not exist: %o", fpath);
                verify (read_if(mod, "]"), "expected ] after file [ literal string path... ] ");
                push(content, fpath); // we need to bring in the image/media api
            } else {
                string msg = read_literal(mod, typeid(string));
                verify (msg, "expected 'text' message");
                push(content, msg);
            }
            read_if(mod, ","); // optional for arrays of 1 dimension
        }
        verify(len(content), "expected more than one message entry");
        verify(read_if(mod, "]"), "expected ] after message");

        push(result, content);
    }
    verify(len(result), "expected dictation message");
    pop_state(mod, false);
    return result;
}

array chatgpt_generate_fn(chatgpt a, fn f, array query) {
    silver mod = f->mod;
    array  res = array(alloc, 32);
    // we need to construct the query for chatgpt from our query tokens
    // as well as the preamble system context 
    // we have simple strings

    string key = f(string, "%s", getenv("CHATGPT"));
    verify(len(key),
        "chatgpt requires an api key stored in environment variable CHATGPT");
    
    map headers = m(
        "Authorization", f(string, "Bearer %o", key));
    
    uri  addr      = uri("POST https://api.openai.com/v1/chat/completions");
    sock chatgpt   = sock(addr);
    bool connected = connect_to(chatgpt);
    verify(connected, "failed to connect to chatgpt (online access required for remote codegen)");
    string str_args = string();
    pairs(f->args->members, i) {
        string name = i->key;
        emember mem  = i->value;
        if (len(str_args))
            append(str_args, ",");
        concat(str_args, f(string, "%o: %o", name, mem->mdl->name));
    }
    string signature = f(string, "fn %o[%o] -> %o", f->name, str_args, f->rtype->name);
    
    // main system message
    map sys_intro = m(
        "role",    string("system"),
        "content", f(string, 
            "this is silver compiler, and your job is to write the code for inside of method: %o, "
            "no [ braces ] containing it, just the inner method code; next we will provide entire module "
            "source, so you know context and other components available", signature));
    
    // include our module source code
    map sys_module = m(
        "role",     string("system"),
        "content",  mod->source_raw );

    // now we need a silver document with reasonable how-to
    // this can be fetched from resource, as its meant for both human and AI learning
    path   docs         = path_share_path();
    path   test_sf      = f(path, "%o/docs/test.sf", docs);
    string test_content = load(test_sf, typeid(string), null);
    map    sys_howto    = m(
        "role",     string("system"),
        "content",  test_content);
    
    array  messages     = a(sys_intro, sys_module, sys_howto);
    
    // now we have 1 line of dictation: ['this is text describing an image', image[ 'file.png' ] ]
    // for each dictation message, there is a response from the server which we also include as assistant
    // it must error if there are missing responses from the servea                                                                                                                                                                                                                                                                                                                                                                                      
    array dictation = read_dictation(mod, f->body);

    each (dictation, array, msg) {
        array content = array();
        each (msg, A, info) {
            map item;
            if (instanceof(typeid(path), info)) {
                string mime_type = mime((path)info);
                string b64 = base64((path)info);
                map m_url = m("url", f(string, "data:%o;base64,%o", mime_type, b64)); // data:image/png;base64,
                item = m("type", "image_url", "image_url", m_url);
            } else if (instanceof(typeid(string), info)) {
                item = m("type", "text", "text", info);
            } else {
                fault("unknown type in dictation: %s", isa(info)->name);
            }
            push(content, item);
        }
        map user_dictation = m(
            "role",     string("user"),
            "content",  content);

        push(messages, user_dictation);

        path test_sf = f(path, "%o/docs/test.sf", docs);

        // if there is a response on file, append that
        // if not, we should error if the next message contains another dictation
    }

    map user = m(
        "role",     string("user"),
        "content",  string("write a function that adds the args a and b"));
    
    
    hold(messages);
    map body = m("model", string("gpt-5"), "messages", messages);
    
    return res;
}


define_class (chatgpt, codegen)

define_class (silver, aether)
define_class (export, model)
define_class (import, model) // we should put these in ext/*.c to exemplify add-ons

module_init  (initialize)