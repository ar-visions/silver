#include <import>
#include <ports.h>
#include <limits.h>

// for Michael

#define emodel(MDL) ({ \
    emember  m = aether_lookup2(mod, string(MDL), null); \
    model mdl = (m && m->is_type) ? m->mdl : null; \
    mdl; \
})

#define validate(a, t, ...) ({ \
    if (!(a)) { \
        formatter((Au_t)null, stderr, (Au)true,  (symbol)"\n%o:%i:%i " t, mod->source, \
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

static bool  is_alpha(Au any);
static enode parse_expression(silver mod, model expect_mdl, array meta);

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

i32 read_enum(silver mod, i32 def, Au_t etype);

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
        "class",    "struct", "public", "intern",
        "import",   "typeid",   "context",
        "is",       "inherits", "ref",
        "const",    "require",  "no-op",    "return",   "->",       "::",     "...",
        "asm",      "if",       "switch",   "any",      "enum",
        "e-if",     "else",     "while",
        "for",      "loop",     "cast",     "fn", "operator", "index", "ctr",
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

static enode typed_expr(silver mod, model src, array meta, array expr);

void build_fn(silver mod, emember fmem, callback preamble, callback postamble) {
    fn f = fmem->mdl;
    if (f->user_built || (!f->body && !preamble && !postamble)) return;

    // finalize first: this prepares args and gives us a value for our function
    // otherwise we could not call recursively
    finalize(fmem);
    f->user_built = true;

    if (f->instance)
        push(mod, f->instance);
    mod->last_return = null;
    push(mod, f);
    if (f->target) {
        register_member(mod, f->target, true); // lookup should look at args; its pretty stupid to register twice
    }
    push(mod, f->args);
    // before the preamble we handle guard
    if (f->is_guarded) {
        // we have a target
    }
    if (preamble)
        preamble(f, null);
    array after_const = parse_const(mod, f->body);
    if (f->cgen) {
        // generate code with cgen delegate imported (todo)
        array gen = generate_fn(f->cgen, f, f->body);
    }
    else if (f->single_expr) {
        mod->left_hand = false;
        enode single = typed_expr(mod, f->rtype, f->rmeta, after_const);
        e_fn_return(mod, single);
    } else {
        push_state(mod, after_const, 0);
        record cl = f ? f->instance : null;
        if (cl) pairs(cl->members, m) print("class emember: %o: %o", cl, m->key);
        print_tokens(mod, "build-fn-statements");
        parse_statements(mod, true);
        if (!mod->last_return) {
            enode r_auto = null;
            if (!f->rtype || f->rtype == emodel("none"))
                r_auto = null;
            else if (f->instance)
                r_auto = lookup2(mod, string("a"), null);
            else if (!mod->cmode) {
                fault("return statement required for function: %o", f);
            }
            e_fn_return(mod, r_auto);
        }
        pop_state(mod, false);
    }
    if (postamble)
        postamble(f, null);
    pop(mod);
    pop(mod);
    if (f->instance) pop(mod);

}

Au build_init_preamble(fn f, Au arg) {
    silver mod = f->mod;
    model  rec = f->instance ? f->instance : mod->userspace;

    pairs(rec->members, i) {
        emember mem = i->value;
        if (mem->initializer)
            build_initializer(mod, mem);
    }
    return null;
}

void create_schema(model mdl, string name);

void build_record(silver mod, model mrec) {
    Au_t t = isa(mrec);
    record rec = t == typeid(model) ? mrec->src : mrec;
    rec->parsing = true;
    Class is_class = mrec->src ? instanceof(mrec->src, typeid(Class)) : null;
    verify(is_class, "not a class");
    print_token_array(mod, rec->body);
    array  body     = rec->body ? rec->body : array();
    push_state(mod, body, 0);
    push      (mod, mrec);
    while     (peek(mod)) {
        print_tokens(mod, "parse-statement");
        parse_statement(mod); // must not 'build' code here, and should not (just initializer type[expr] for members)
    }

    pop       (mod);
    pop_state (mod, false);
    rec->parsing = false;
    
    // register the init method (without defining; not required for finalize!)
    emember m_init = null;
    if (is_class) {
        push(mod, mrec);

        // if no init, create one
        m_init = find_member(rec, string("init"), typeid(fn));
        if (!m_init) {
            fn f_init = fn(mod, mod,
                extern_name, f(string, "%s_init", rec),
                rtype, emodel("none"), instance, pointer(rec), args, eargs(mod, mod));
            m_init    = emember(mod, mod, name, string("init"), mdl, f_init);
            register_member(mod, m_init, true);
        }
        pop(mod);
    }

    finalize(rec->mem);
    // schema must be there before we build functions
    create_schema(emodel(rec->mem->name->chars), rec->mem->name);

    if (is_class) {
        // build init with preamble
        build_fn(mod, m_init, build_init_preamble, null); // we may need to 
        
        // build remaining functions
        pairs(rec->members, ii) {
            emember m = ii->value;
            if (!m->finalized && instanceof(m->mdl, typeid(fn)))
                build_fn(mod, m, null, null);
        }
    }
    rec->user_built = true;
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
    bool  proceed = mod->expr_level == 0 ? true : eq(n, "[");
    if  (!proceed) return null;

    bool bracket = eq(n, "[");
    consume(mod);
    int depth = bracket == true; // inner expr signals depth 1, and a bracket does too.  we need both togaether sometimes, as in inner expression that has parens
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

num index_of_cstr(Au a, cstr f) {
    Au_t t = isa(a);
    if (t == typeid(string)) return index_of((string)a, f);
    if (t == typeid(array))  return index_of((array)a, string(f));
    if (t == typeid(cstr) || t == typeid(symbol) || t == typeid(cereal)) {
        cstr v = strstr(a, f);
        return v ? (num)(v - f) : (num)-1;
    }
    fault("len not handled for type %s", t->name);
    return 0;
}

bool is_keyword(Au any) {
    Au_t  type = isa(any);
    string s;
    if (type == typeid(string))
        s = any;
    else if (type == typeid(token))
        s = string(((token)any)->chars);
    
    return index_of_cstr(keywords, cstring(s)) >= 0;
}

bool is_alpha(Au any) {
    if (!any)
        return false;
    Au_t  type = isa(any);
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

array parse_tokens(silver mod, Au input) {
    string input_string;
    Au_t  type = isa(input);
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
        bool last_dash = false;
        while (index < length) {
            i32 v = idx(input_string, index);
            char sval[2] = { v, 0 };
            bool is_dash = v == '-';
            is_dim = (num_start && v == 'x');
            if (isspace(v) || index_of(special, sval) >= 0 || is_dim) {
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

Au silver_read_literal(silver a, Au_t of_type) {
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

Au silver_read_numeric(silver a) {
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

    if (is_const)    *(bool*)  is_const     = false;
    if (assign_type) *(OPType*)assign_type  = OPType__undefined;

    if (found) {
        if (eq(k, ":")) {
            if (is_const)    *(bool*)is_const      = true;
            if (assign_type) *(OPType*)assign_type = OPType__assign;
        } else {
            if (is_const)    *(bool*)is_const      = false;
            if (assign_type) *(OPType*)assign_type = OPType__assign + assign_index;
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

fn    parse_fn  (silver mod, AFlag member_type, Au ident, OPType assign_enum);
static model read_model(silver mod, array* expr, array* meta);

int ref_level(model f);

enode silver_read_node(silver mod, Au_t constant_result, model mdl_expect, array meta_expect) {

    print_tokens(mod, "read-node");
    bool      cmode     = mod->cmode;
    array     expr      = null;
    token     peek      = peek(mod);
    bool      is_expr0  = !mod->cmode && mod->expr_level == 0;
    interface access    = is_expr0 ? read_enum(mod, interface_undefined, typeid(interface)) : 0;
    bool      is_static = is_expr0 && read_if(mod, "static") != null;
    string    kw        = is_expr0 ? peek_keyword(mod) : null;
    bool      is_oper   = kw && eq(kw, "operator");
    bool      is_index  = kw && eq(kw, "index");
    bool      is_cast   = kw && eq(kw, "cast");
    bool      is_fn     = kw && eq(kw, "fn");
    record    rec_ctx   = null;
    model     top       = top(mod);
    record    rec_top   = !mod->cmode ? is_record(top) : null;
    fn        f         = null;
    eargs     args      = !mod->cmode ? instanceof(top, typeid(eargs)) : null;
    silver    module    = !mod->cmode && (top->is_global) ? mod : null;
    emember   mem       = null;
    AFlag     mtype     = (is_fn && !is_static) ? AU_FLAG_IMETHOD : (!is_static && is_cast) ? AU_FLAG_CAST : AU_FLAG_SMETHOD;

    if (!mod->cmode) {
        context(mod, &rec_ctx, &f, typeid(fn));
    }

    if (is_expr0 && !mod->cmode && (module || rec_ctx) && peek_def(mod)) {
        emember mem = read_def(mod);
        if (!mem) {
            if (is_cast || is_fn) consume(mod);
            token alpha = !is_cast ? read_alpha(mod) : null;
            // check if module or record constructor
            emember rmem = alpha ? lookup2(mod, alpha, null) : null;
            if (module && rmem && rmem->mdl == (model)module) {
                validate(!is_cast && is_fn, "invalid constructor for module; use fn keyword");
                mtype = AU_FLAG_CONSTRUCT;
            } else if (rec_ctx && rmem && rmem->mdl == (model)rec_ctx) {
                validate(!is_cast && !is_fn, "invalid constructor for class; use class-name[] [ no fn, not static ]");
                mtype = AU_FLAG_CONSTRUCT;
            }

            bool has_args = is_cast || next_is(mod, "[");
            validate(has_args, "expected fn args for %o", alpha);

            // parse function contents
            if (eq(alpha, "member")) {
                int test2 = 2;
                test2    += 2;
            }
            mem = emember(
                mod,        mod,
                access,     access,
                name,       alpha,
                mdl,        parse_fn(mod, mtype, alpha, OPType__undefined),
                is_module,  module);
            mem->mdl->mem = mem;

            if (is_cast) {
                fn f = (fn)mem->mdl;
                validate(len(f->rtype->mem->name), "rtype cannot be anonymous for cast");
                mem->name = hold(f(string, "cast_%o", f->rtype));
            }
        }
        register_member(mod, mem, false); /// do not finalize in push member
        return mem;
    }

    if (peek && is_alpha(peek) && !lookup2(mod, peek, typeid(macro))) {
        array meta = null;
        model mdl_found = read_model(mod, &expr, &meta);
        if (mdl_found) {
            enode res = typed_expr(mod, mdl_found, meta, expr);
            return e_create(mod, mdl_expect, meta_expect, res); // we need a 'meta_expect' here
        }
    }

    Au lit = read_literal(mod, null);
    if (lit) {
        enode res = e_operand(mod, lit, mdl_expect, meta_expect);
        return e_create(mod, mdl_expect, meta_expect, res);
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
            array meta = null;
            model inner = read_model(mod, null, null);
            if (inner) {
                if (next_is(mod, ")")) {
                    consume(mod);
                    pop_state(mod, true);
                    enode res = e_create(mod, inner, null, parse_expression(mod, inner, null));
                    return e_create(mod, mdl_expect, meta_expect, res);
                } else {
                    pop_state(mod, false);
                    mod->expr_level = 0;
                    return null;
                }
            }
            pop_state(mod, false);
        }
        mod->parens_depth++;
        enode expr = parse_expression(mod, null, null); // Parse the expression
        token t = read_if(mod, ")");
        if (t) mod->parens_depth--;
        return e_create(mod, mdl_expect, meta_expect,
            parse_ternary(mod, expr, mdl_expect, meta_expect));
    }

    if (!cmode && next_is(mod, "[")) {
        validate(mdl_expect, "expected model name before [");
        array expr = read_within(mod);
        // we need to get mdl from argument
        enode r = typed_expr(mod, mdl_expect, meta_expect, expr);
        return r;
    }

    // handle the logical NOT operator (e.g., '!')
    else if (read_if(mod, "!") || (!cmode && read_if(mod, "not"))) {
        enode expr = parse_expression(mod, null, null); // Parse the following expression
        return e_create(mod,
            mdl_expect, meta_expect, e_not(mod, expr));
    }

    // bitwise NOT operator
    else if (read_if(mod, "~")) {
        enode expr = parse_expression(mod, null, null);
        return e_create(mod,
            mdl_expect, meta_expect, e_bitwise_not(mod, expr));
    }

    // 'typeof' operator
    // should work on instances as well as direct types
    else if (!cmode && read_if(mod, "typeid")) {
        bool bracket = false;
        if (next_is(mod, "[")) {
            consume(mod); // Consume '['
            bracket = true;
        }
        enode expr = parse_expression(mod, null, null); // Parse the type expression
        if (bracket) {
            assert(next_is(mod, "]"), "Expected ']' after type expression");
            consume(mod); // Consume ']'
        }
        return e_create(mod,
            mdl_expect, meta_expect, expr); // Return the type reference
    }

    // 'ref' operator (reference)
    else if (!cmode && read_if(mod, "ref")) {
        mod->in_ref = true;
        enode expr = parse_expression(mod, null, null);
        mod->in_ref = false;
        return e_create(mod,
            mdl_expect, meta_expect, e_addr_of(mod, expr, null));
    }

    // we may only support a limited set of C functionality for #define macros
    if (cmode) return null;

    push_current(mod);
    
    string    alpha     = null;
    int       depth     = 0;
    
    bool skip_member_check = false;
    bool avoid_model_lookup = mod->read_model_abort;
    if (!skip_member_check && module) {
        string alpha = peek_alpha(mod);
        if (alpha && !mod->read_model_abort) {
            emember m = lookup2(mod, alpha, null); // silly read as string here in int [ silly.len ]
            model mdl = m ? m->mdl : null;
            while (mdl && mdl->is_ref)
                mdl = mdl->src;
            if (m && m->is_type && mdl && isa(mdl) == typeid(Class))
                skip_member_check = true;
        }
        mod->read_model_abort = false; // state var used for this purpose with the singular call to read-model above
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
        if (first && !avoid_model_lookup) {
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
        
        if (!ns_found)
            mem = (is_fn || is_cast || avoid_model_lookup) ? null : lookup2(mod, alpha, null);
        avoid_model_lookup = false;

        if (!mem) {
            validate(mod->expr_level == 0, "member not found: %o", alpha);
            // parse new member, parsing any associated function
            mem = emember(
                mod,        mod,
                access,     access,
                name,       alpha,
                is_module,  module);
        } else if (!mem->is_type || instanceof(mem->mdl, typeid(macro)) || instanceof(mem->mdl, typeid(fn))) {
            bool is_macro = instanceof(mem->mdl, typeid(macro)) != null;

            // from record if no value; !in_record means its not a record definition 
            // but this is an expression within a function
            if (!rec_top && !mem->is_func && !is_macro && !has_value(mem)) { // if mem from_record_in_context
                validate(f, "expected function");
                if (f->target) {
                    validate(f->target, "no target found in context");
                    emember mem2 = lookup2(mod, alpha, null);
                    validate(mem, "failed to resolve emember in context: %o", f);

                    mem = access(f->target, alpha);
                    validate(mem, "failed to lookup emember in context: %o", f->target);
                }
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
                    string alpha = read_alpha(mod);
                    validate(alpha, "expected alpha identifier");
                    model mdl = mem->mdl;
                    mem = access(mem, alpha);
                    mem = e_load(mod, mem, null);
                    mem = parse_member_expr(mod, mem);
                }/* else {
                    // make pointers safe again
                    consume(mod);
                    string alpha = read_alpha(mod);
                    validate(alpha, "expected member name after ->");
                    mem = access_guarded(mem, alpha);
                }*/
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
        if (args) {
            validate(mem, "expected emember");
            validate(assign_enum == OPType__assign, "expected : operator in eargs");
            validate(!mem->mdl, "duplicate member exists in eargs");
            mem->mdl = read_model(mod, &expr, &mem->meta);
            validate(expr == null, "unexpected assignment in args");
            validate(mem->mdl, "cannot read model for arg: %o", mem->name);
        }
        else if ((rec_top || module) && assign_type) {
            token t = peek(mod);

            // this is where member types are read
            mem->mdl = read_model(mod, &expr, &mem->meta); // given VkInstance intern instance2 (should only read 1 token)
            validate(mem->mdl, "expected type after ':' in %o, found %o",
                (rec_top ? (model)rec_top : (model)module)->mem->name, peek(mod));
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

    if (mem && isa(mem) == typeid(emember) && !mem->membership && mem->mdl && mem->name && len(mem->name)) {
        //push(mod, mod->userspace);
        register_member(mod, mem, false); // was true
        //pop(mod);
    }

    // this only happens when in a function
    return f ? e_create(mod, mdl_expect, meta_expect, mem) : null;
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
        return emodel("Au");
    }

    model f = emodel(t->chars);
    if (f && isa(f->src) == typeid(Class)) {
        if (read_token)
            consume(mod);
        return f;
    }
    return null;
}

string silver_peek_def(silver a) {
    token n = element(a, 0);
    record rec = instanceof(top(a), typeid(record));
    if (!rec && next_is_class(a, false))
        return string(n->chars);

    if (n && is_keyword(n))
        if (eq(n, "import") || eq(n, "fn") || eq(n, "cast") || eq(n, "class") || eq(n, "enum") || eq(n, "struct"))
            return string(n->chars);
    return null;
}

Au silver_read_bool(silver a) {
    token  n       = element(a, 0);
    if (!n) return null;
    bool   is_true = strcmp(n->chars, "true")  == 0;
    bool   is_bool = strcmp(n->chars, "false") == 0 || is_true;
    if (is_bool) a->cursor ++;
    return is_bool ? _bool(is_true) : null;
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
            //push_state(mod, post_const, 0);
            int level = mod->expr_level;
            mod->expr_level++;
            expr = typed_expr(mod, mem->mdl, mem->meta, post_const); // we have tokens for the name pushed to the stack
            mod->expr_level = level;
            //pop_state(mod, false);
        }

        fn ctx = context_model(mod, typeid(fn));
        enode L = (!mem->is_module && ctx) ? 
            access(ctx->target, mem->name) : (enode)mem;
        
        e_assign(mod, L, expr, OPType__assign);
        
    } else {
        /// aether can memset to zero
    }
}

enode parse_return(silver mod) {
    array rmeta;
    model rtype = return_type(mod, &rmeta);
    bool  is_v  = is_void(rtype);
    model ctx = context_model(mod, typeid(fn));
    consume(mod);
    enode expr   = is_v ? null : parse_expression(mod, rtype, rmeta);
    Au_log("return-type", "%o", is_v ? (Au)string("none") : 
                                    (Au)expr->mdl);
    return e_fn_return(mod, expr);
}

enode parse_break(silver mod) {
    consume(mod);
    catcher cat = context_model(mod, typeid(catcher));
    verify(cat, "expected cats");
    return e_break(mod, cat);
}

enode silver_parse_do_while(silver mod) {
    consume(mod);
    enode vr = null;
    return null;
}

static enode reverse_descent(silver mod, model mdl_expect, array meta_expect) {
    bool cmode = mod->cmode;
    enode L = read_node(mod, null, mdl_expect, meta_expect); // build-arg
    token t = peek(mod);
    validate(cmode || L, "unexpected '%o'", t);
    if (!L)
        return null;
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
                enode R = read_node(mod, null, null, null);
                L = e_op(mod, op_type, method, L, R);
                m      = true;
                break;
            }
        }
    }
    mod->expr_level--;
    return L;
}

// read-expression does not pass in 'expected' models, because 100% of the time we run conversion when they differ
// the idea is to know what model is returning from deeper calls
static array read_expression(silver mod, model* mdl_res, array* meta_res, bool* is_const) {
    array exprs = array(32);
    int s = mod->cursor;
    mod->no_build = true;
    mod->is_const = true; // set this, and it can only &= to true with const ops; any build op sets to false
    enode   n = reverse_descent(mod, null, null);
    if (mdl_res)  *mdl_res  = n->mdl;
    if (meta_res) *meta_res = n->meta;
    mod->no_build = false;
    int e = mod->cursor;
    for (int i = s; i < e; i++) {
        push(exprs, mod->tokens->elements[i]);
    }
    *is_const = mod->is_const;
    return exprs;
}

static enode parse_expression(silver mod, model mdl_expect, array meta_expect) {
    print_tokens(mod, "parse-expr");
    enode vr = reverse_descent(mod, mdl_expect, meta_expect);
    return vr;
}

model read_named_model(silver mod) {
    model mdl = null;
    push_current(mod);

    bool any = read_if(mod, "any") != null;
    if (any) {
        emember mem = lookup2(mod, string("any"), null);
        model mdl_any = mem->mdl;
        return mdl_any;
    }
    string a = read_alpha(mod);
    if (a && !next_is(mod, ".")) {
        emember f = lookup2(mod, a, null);
        if (f && f->is_type) mdl = f->mdl;
    }
    pop_state(mod, mdl != null); /// save if we are returning a model
    return mdl;
}

static model model_adj(silver mod, model mdl) {
    while (mod->cmode && read_if(mod, "*"))
        mdl = model(mod, mod, src, mdl, is_ref, true);
    return mdl;
}

array read_meta(silver mod) {
    array res = null;
    if (read_if(mod, "<")) {
        bool first = true;
        while (!next_is(mod, ">")) {
            if (!res) res = array(alloc, 32, assorted, true);
            if (!first)
                verify(read_if(mod, ","), "expected ',' seperator between models");
            Au n = read_numeric(mod);
            bool f = true;
            shape s = null;
            if (n) {
                s = shape();
                while (n) {
                    verify (isa(n) == typeid(i64),
                        "expected numeric type i64, found %s", isa(n)->name);
                    i64 value = *(i64*)n;
                    shape_push(s, value);
                    if (!read_if(mod, "x")) 
                        break;
                    n = read_numeric(mod);
                }
                push(res, s);
            } else {
                model mdl = read_named_model(mod);
                verify(mdl, "expected model name, found %o", peek(mod));
                push(res, mdl);
            }
            first = false;
        }
        consume(mod);
    }
    return res;
}

static model read_model(silver mod, array* p_expr, array* p_meta) {
    model mdl       = null;
    bool  body_set  = false;
    bool  type_only = false;
    model type      = null;
    array expr      = null;
    array meta      = null;

    mod->read_model_abort = false;

    push_current(mod);

    if (!mod->cmode) {
        Au lit = read_literal(mod, null);
        if (lit) {
            mod->cursor--;
            // we support var:1  or var:2.2 or var:'this is a test with {var2}'
            if (isa(lit) == typeid(string)) {
                expr = a(token("string"), token("["), element(mod, 0), token("]"));
                mdl = emodel("string");
            } else if (isa(lit) == typeid(i64)) {
                expr = a(token("i64"), token("["), element(mod, 0), token("]"));
                mdl = emodel("i64");
            } else if (isa(lit) == typeid(u64)) {
                expr = a(token("u64"), token("["), element(mod, 0), token("]"));
                mdl = emodel("u64");
            } else if (isa(lit) == typeid(i32)) {
                expr = a(token("i32"), token("["), element(mod, 0), token("]"));
                mdl = emodel("i32");
            } else if (isa(lit) == typeid(u32)) {
                expr = a(token("u32"), token("["), element(mod, 0), token("]"));
                mdl = emodel("u32");
            } else if (isa(lit) == typeid(f32)) {
                expr = a(token("f32"), token("["), element(mod, 0), token("]"));
                mdl = emodel("f32");
            } else if (isa(lit) == typeid(f64)) {
                expr = a(token("f64"), token("["), element(mod, 0), token("]"));
                mdl = emodel("f64");
            } else {
                verify(false, "implement literal %s in read_model", isa(lit)->name);
            }
            mod->cursor++;
            return mdl;
        }
    }

    bool explicit_sign = read_if(mod, "signed") != null;
    bool explicit_un   = !explicit_sign && read_if(mod, "unsigned") != null;

    model prim_mdl = null;
    if (!explicit_un) {
        if (read_if(mod, "char"))   prim_mdl = emodel("i8");
        else if (read_if(mod, "short"))  prim_mdl = emodel("i16");
        else if (read_if(mod, "int"))    prim_mdl = emodel("i32");
        else if (read_if(mod, "long"))   prim_mdl = read_if(mod, "long") ? emodel("i64") : emodel("i32");
        else if (explicit_sign)          prim_mdl = emodel("i32");
        if (prim_mdl)
            prim_mdl = model_adj(mod, prim_mdl);

        mdl = prim_mdl ? prim_mdl : read_named_model(mod);

        if (!mod->cmode && meta && next_is(mod, "<")) {
            meta = read_meta(mod);
        }

        if (!mod->cmode && next_is(mod, "[")) {
            body_set = true;
            expr = read_within(mod);
        }

    } else if (explicit_un) {
        if (read_if(mod, "char"))  prim_mdl = emodel("u8");
        if (read_if(mod, "short")) prim_mdl = emodel("u16");
        if (read_if(mod, "int"))   prim_mdl = emodel("u32");
        if (read_if(mod, "long"))  prim_mdl = read_if(mod, "long") ? emodel("u64") : emodel("u32");

        prim_mdl = model_adj(mod, prim_mdl ? prim_mdl : emodel("u32"));
    }

    // abort if there is assignment following isolated type-like token
    if (!expr && mod->expr_level == 0 && read_assign(mod, null, null)) {
        mod->read_model_abort = true;
        mdl = null;
    }

    if (p_expr) *p_expr = expr;
    if (p_meta) *p_meta = meta;
    
    pop_state(mod, mdl != null); // if we read a model, we transfer token state
    return mdl;
}

static enode parse_fn_call(silver, fn, enode);

map parse_map(silver mod, model mdl_schema) {
    map args  = map(hsize, 16, assorted, true);

    // we need e_create to handle this as well; since its given a map of fields and a is_ref struct it knows to make an alloc
    if (mdl_schema->is_ref && instanceof(mdl_schema->src, typeid(structure)))
        mdl_schema = mdl_schema->src;

    while (peek(mod)) {
        string name  = read_alpha(mod);
        validate(read_if(mod, ":"), "expected : after arg %o", name);
        model   mdl_expect = null;
        array   meta_expect = null;
        if (mdl_schema && mdl_schema->members) {
            emember m = get(mdl_schema->members, name);
            validate(m, "member %o not found on model %o", name, mdl_schema->mem->name);
            mdl_expect = m->mdl;
            meta_expect = m->meta;
        }
        enode   value = parse_expression(mod, mdl_expect, meta_expect);
        validate(!contains(args, name), "duplicate initialization of %o", name);
        set(args, name, value);
    }
    return args;
}

static bool class_inherits(model cl, model of_cl) {
    silver mod = cl->mod;
    model  aa = of_cl;
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

static enode typed_expr(silver mod, model mdl, array meta, array expr) {
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
            enode   expr = parse_expression(mod, arg->mdl, arg->meta); // self contained for '{interp}' to cstr!
            verify(expr, "invalid expression");
            model   arg_mdl  = arg ? arg->mdl  : null;
            array   arg_meta = arg ? arg->meta : null;
            if (arg_mdl && expr->mdl != arg_mdl)
                expr = e_create(mod, arg_mdl, arg_meta, expr);
            
            if (!target && f->instance)
                target = expr;
            else
                push(values, expr);
            
            if (read_if(mod, ","))
                continue;
            
            validate(len(values) >= ln, "expected %i args for function %o", ln, f);
            break;
        }
        return e_fn_call(mod, f, target, values);
    }
    
    // this is only suitable if reading a literal constitutes the token stack
    // for example:  i32 100
    Au  n = read_literal(mod, null);
    if (n && mod->cursor == len(mod->tokens)) {
        pop_state(mod, expr ? false : true);
        return e_operand(mod, n, mdl, meta);
    } else if (n) {
        // reset if we read something
        pop_state(mod, expr ? false : true);
        push_state(mod, expr ? expr : mod->tokens, expr ? 0 : mod->cursor);
    }
    bool    has_content = !!expr && len(expr); //read_if(mod, "[") && !read(mod, "]");
    enode   r           = null;
    bool    conv        = false;
    bool    has_init    = peek_fields(mod);

    mod->expr_level++;
    if (!has_content) {
        r = e_create(mod, mdl, meta, null); // default
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
            enode  e = parse_expression(mod, element_type, null);
            e = e_create(mod, element_type, null, e);
            push(nodes, e);
            num_index++;
            if (top_stride && (num_index % top_stride == 0)) {
                validate(read_if(mod, ",") || !peek(mod),
                    "expected ',' when striding between dimensions (stride size: %o)",
                    top_stride);
            }
        }
        r = e_create(mod, mdl, meta, nodes);
    } else if (has_init || class_inherits(mdl, emodel("map"))) {
        validate(has_init, "invalid initialization of map model");
        conv = true;
        r    = parse_map(mod, mdl);
    } else {
        /// this is a conversion operation
        r = parse_expression(mod, mdl, meta);
        conv = r->mdl != mdl;
        //validate(read_if(mod, "]"), "expected ] after mdl-expr %o", src->name);
    }
    mod->expr_level--;
    if (conv)
        r = e_create(mod, mdl, meta, r);
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
            mem->name);
        
        /// we must read the arguments given to the indexer
        consume(mod);
        array args = array(16);
        while (!next_is(mod, "]")) {
            enode expr = parse_expression(mod, null, null);
            push(args, expr);
            validate(next_is(mod, "]") || next_is(mod, ","), "expected ] or , in index arguments");
            if (next_is(mod, ","))
                consume(mod);
        }
        consume(mod);
        enode index_expr = null;
        if (r) {
            emember indexer = compatible(mod, r, null, AU_FLAG_INDEX, args); /// we need to update emember model to make all function members exist in an array
            validate(indexer, "%o: no suitable indexing method", r);
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
        enode res = parse_expression(mod, null, null);
        pop_state(mod, false);
        pop_state(mod, true);
        return res;

    } else if (mem) {
        if (mem->is_func || mem->is_type) {
            //validate(next_is(mod, "["), "expected [ to initialize type");
            array expr = read_within(mod);
            mem = typed_expr(mod, mem->mdl, mem->meta, expr); // this, is the construct
        }
    }
    pop_state(mod, mem != null);
    return mem;
}

/// parses emember args for a definition of a function
eargs parse_args(silver mod) {
    validate(read_if(mod, "["), "parse-args: expected [");
    eargs args = eargs(mod, mod, open, true);

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
            int count0 = args->members->count;
            print_tokens(mod, "parse-args");
            parse_statement(mod);
            int count1 = args->members->count;
            verify(count0 == count1 - 1, "arg could not parse");
            
            //emember arg = value_by_index(args->members, count0);

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
    args->open = false;
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

enode silver_parse_ternary(silver mod, enode expr, model mdl_expect, array meta_expect) {
    if (!read_if(mod, "?")) return expr;
    enode expr_true  = parse_expression(mod, mdl_expect, meta_expect);
    enode expr_false = parse_expression(mod, mdl_expect, meta_expect);
    return e_ternary(mod, expr, expr_true, expr_false);
}

// with constant literals, this should be able to merge the nodes into a single value
enode silver_parse_assignment(silver mod, emember mem, string oper) {
    validate(isa(mem) == typeid(enode) || !mem->is_assigned || !mem->is_const, "mem %o is a constant", mem->name);
    mod->in_assign = mem;
    enode   L       = mem;
    enode   R       = parse_expression(mod, mem->mdl, mem->meta); /// getting class2 as a struct not a pointer as it should be. we cant lose that pointer info
    if (!mem->mdl) {
        mem->is_const = eq(oper, ":");
        mem->mdl = hold(R->mdl);
        if (mem->literal)
            drop(mem->literal);
        mem->literal = hold(R->literal);
        finalize(mem); // todo: i would make sure we are never performing any action here if its inside of a function
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

enode cond_builder(silver mod, array cond_tokens, Au unused) {
    mod->expr_level++; // make sure we are not at 0
    push_state(mod, cond_tokens, 0);
    enode cond_expr = parse_expression(mod, emodel("bool"), null);
    pop_state(mod, false);
    mod->expr_level--;
    return cond_expr;
}

// singular statement (not used)
enode statement_builder(silver mod, array expr_tokens, Au unused) {
    int level = mod->expr_level;
    mod->expr_level = 0;
    push_state(mod, expr_tokens, 0);
    enode expr = parse_statement(mod);
    pop_state(mod, false);
    mod->expr_level = level;
    return expr;
}

enode block_builder(silver mod, array block_tokens, Au unused) {
    int level = mod->expr_level;
    mod->expr_level = 0;
    enode last = null;
    push_state(mod, block_tokens, 0);
    last = parse_statements(mod, false);
    pop_state(mod, false);
    mod->expr_level = level;
    return last;
}

// we separate this, that:1, other:2 -- thats not an actual statements protocol generally, just used in for
enode statements_builder(silver mod, array expr_groups, Au unused) {
    int level = mod->expr_level;
    mod->expr_level = 0;
    enode last = null;
    each (expr_groups, array, expr_tokens) {
        push_state(mod, expr_tokens, 0);
        last = parse_statement(mod);
        pop_state(mod, false);
    }
    mod->expr_level = level;
    return last;
}

enode exprs_builder(silver mod, array expr_groups, Au unused) {
    mod->expr_level++; // make sure we are not at 0
    enode last = null;
    each (expr_groups, array, expr_tokens) {
        push_state(mod, expr_tokens, 0);
        last = parse_expression(mod, null, null);
        pop_state(mod, false);
    }
    mod->expr_level--;
    return last;
}

enode expr_builder(silver mod, array expr_tokens, Au unused) {
    mod->expr_level++; // make sure we are not at 0
    push_state(mod, expr_tokens, 0);
    enode exprs = parse_expression(mod, null, null);
    pop_state(mod, false);
    mod->expr_level--;
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

    verify(mod->expr_level == 0, "unexpected expression level at ifdef");
    
    
    while (true) {
        validate(!expect_last, "continuation after else");
        bool  is_if  = read_if(mod, "ifdef") != null;
        validate(is_if && require_if || !require_if, "expected if");
        mod->expr_level++;
        enode n_cond = is_if ? parse_expression(mod, null, null) : null;
        mod->expr_level--;
        array block  = read_body(mod);
        if (n_cond) {
            Au const_v = n_cond->literal;
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

    verify(mod->expr_level == 0, "unexpected expression level after ifdef");
    return statements ? statements : enode(mdl, null, meta, null);
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
    subprocedure build_expr = subproc(mod, block_builder, null);
    return e_if_else(mod, tokens_cond, tokens_block, build_cond, build_expr);
}

bool is_primitive(model f);

enode parse_switch(silver mod) {
    
    validate(read_if(mod, "switch") != null, "expected switch");
    enode e_expr = parse_expression(mod, null, null);
    map   cases  = map(hsize, 16);
    array expr_def = null;
    bool  all_const = is_primitive(e_expr->mdl);

    while (true) {
        if (read_if(mod, "case")) {
            bool  is_const  = false;
            model mdl_read  = null;
            array meta_read = null;
            array value = read_expression(mod,
                &mdl_read, &meta_read, &is_const);
            all_const &= is_const && (mdl_read == e_expr->mdl) && (meta_read == e_expr->meta);
            array body  = read_body(mod);
            //value->deep_compare = true; // tell array when its given a compare to not just do element equation but effectively deep compare (i will do this)
            set(cases, value, body);
            continue;
        } else if (read_if(mod, "default")) {
            expr_def = read_body(mod);
            continue;
        } else
            break;
    }

    subprocedure build_expr = subproc(mod, expr_builder, null);
    subprocedure build_body = subproc(mod, statements_builder, null);
    if (all_const)
        return e_native_switch(mod, e_expr, cases, expr_def, build_expr, build_body);
    else
        return e_switch(mod, e_expr, cases, expr_def, build_expr, build_body);
}

// improve this to use read_expression (todo: read_expression needs to be able to keep the stack read)
array read_expression_groups(silver mod) {
    array result  = array(8);
    array current = array(32);
    int   level   = 0;

    token f = peek(mod);
    if (!eq(f, "[")) return null;

    consume(mod);
    
    while (true) {
        token t = consume(mod);
        if (!t)
            break;

        // adjust nesting
        if (eq(t, "[")) 
            level++;
        else if (eq(t, "]")) {
            level--;
            if (level < 0)
                break;
        }

        // comma at base-level → end group
        if (eq(t, ",") && level == 0) {
            push(result, current);
            current = array(32);
            continue;
        }

        // normal token inside current expression
        push(current, t);
    }

    // Push final group if not empty
    if (len(current) > 0)
        push(result, current);

    return result;
}

enode parse_for(silver mod) {
    validate(read_if(mod, "for") != null, "expected for");

    array groups = read_expression_groups(mod);
    validate(groups != null, "expected [ init , cond , step [ , step-b , step-c , ...] ]");
    validate(len(groups) == 3, "for expects exactly 3 expressions");

    array init_exprs = groups->elements[0];
    array cond_expr  = groups->elements[1];
    array step_exprs = groups->elements[2];

    verify(isa(init_exprs) == typeid(array), "expected array for init exprs");
    verify(isa(cond_expr)  == typeid(array), "expected group of cond expr");
    cond_expr = cond_expr->len ? cond_expr->elements[0] : null;
    verify(isa(cond_expr)  == typeid(array), "expected array for inner cond expr");

    array body = read_body(mod);
    verify(body, "expected for-body");

    subprocedure build_init = subproc(mod, statements_builder, null);
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_body = subproc(mod, expr_builder, null);
    subprocedure build_step = subproc(mod, exprs_builder, null);

    return e_for(
        mod,
        init_exprs,
        cond_expr,
        step_exprs,
        body,
        build_init,
        build_cond,
        build_body,
        build_step
    );
}


enode parse_loop_while(silver mod) {
    bool is_loop = read_if(mod, "loop") != null;
    validate(is_loop, "expected loop");
    array cond  = read_within(mod);
    array block = read_body(mod);
    verify(block, "expected body");
    bool is_loop_while = read_if(mod, "while") != null;
    if (is_loop_while) {
        verify(!cond, "condition given above conflicts with while below");
        cond = read_within(mod);
    }
    subprocedure build_cond = subproc(mod, cond_builder, null);
    subprocedure build_expr = subproc(mod, expr_builder, null);
    return e_loop(mod, cond, block, build_cond, build_expr, is_loop_while);
}


bool is_model(silver mod) {
    token  k = peek(mod);
    emember m = lookup2(mod, k, null);
    return m && m->is_type;
}

// these are for public, intern, etc; Au-Type enums, not someting the user defines in silver context
i32 read_enum(silver mod, i32 def, Au_t etype) {
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

Au_t next_is_keyword(silver mod, member* fn) {
    token t = peek(mod);
    if  (!t)  return null;
    if  (!isalpha(t->chars[0])) return null;
    Au_t f = Au_find_type((cstr)t->chars);
    if (f && inherits(f, typeid(model)) && (*fn = find_member(f, AU_FLAG_SMETHOD, "parse", true)))
        return f;
    return null;
}

/// called after : or before, where the user has access
emember silver_read_def(silver mod) {
    member parse_fn  = null;
    Au_t   is_type   = next_is_keyword(mod, &parse_fn);
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
        return mem;
    }

    consume(mod);
    string n = read_alpha(mod);
    validate(n, "expected alpha-numeric identity, found %o", next(mod));

    model   mtop = top(mod);
    emember mem  = null; // = emember(mod, mod, name, n, context, mtop);
    model   mdl  = null;
    array   meta = null;

    if (is_class || is_struct) {
        validate(mtop->is_global, "expected record definition at module level");
        Au_t tt = isa(mtop);
        array schema = array();
              meta   = (is_class && next_is(mod, "<")) ? read_meta(mod) : null;
        array body   = read_body(mod);

        if (is_class)
            mdl = Class(mod, mod, ident, n, parent, is_class, body, body, members, map(hsize, 16), meta, meta);
        else
            mdl = structure(mod, mod, ident, n, body, body, members, map(hsize, 16));
        
    } else if (is_enum) {
        model store = null, suffix = null;
        if (read_if(mod, ",")) {
            store  = instanceof(read_model(mod, null, null), typeid(model));
            suffix = instanceof(read_model(mod, null, null), typeid(model));
            validate(store, "invalid storage type");
        }
        array enum_body = read_body(mod);
        print_all(mod, "enum-tokens", enum_body);
        validate(len(enum_body), "expected body for enum %o", n);

        mdl = enumeration(
            mod, mod, src, store);
        
        push_state(mod, enum_body, 0);
        push(mod, mdl);
        store = mdl->src;

        // verify model is a primitive type
        Au_t atype = isa(mdl->src->src);
        validate(atype && (atype->traits & AU_TRAIT_PRIMITIVE),
            "enumeration can only be based on primitive types (i32 default)");

        i64  i64_v = 0;
        f64  f64_v = 0;
        f32  f32_v = 0;
        i64* i64_value = store == emodel("i64") ? &i64_v : null;
        i64* i32_value = store == emodel("i32") ? &i64_v : null;
        f64* f64_value = store == emodel("f64") ? &f64_v : null;
        f32* f32_value = store == emodel("f32") ? &f32_v : null;
        Au value = (Au)(i64_value ? (Au)i64_value : i32_value ? (Au)i32_value :
                                f64_value ? (Au)f64_value : (Au)f32_value);
        
        while (true) {
            token e = next(mod);
            if  (!e) break;
            Au v = null;
            
            if (read_if(mod, ":")) {
                v = read_node(mod, atype, null, null); // i want this to parse an entire literal, with operations
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
            emember emem = emember(
                mod, mod, name, e, mdl, mdl,
                is_const, true, is_decl, true, literal, v, aliases, aliases);
            
            set_value(emem, v); // redundant with the first literal field; we aren't to simply store the token[0] or tokens, either
            
            string estr = cast(string, e);
            set(mdl->members, estr, emem);
            if (aliases)
                each(aliases, string, a)
                    set(mdl->members, a, emem);
            
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
    } else {
        validate(is_alias, "unknown error");
        mdl  = read_named_model(mod);
        validate(is_alias, "expected model for alias, found %o", peek(mod));
        mdl  = model(mod, mod, src, mdl); // todo: support meta too
        //meta = read_meta(mod);
    }

    validate(mdl && len(n),
        "name required for model: %s", isa(mdl)->name);
    
    return register_model(mod, mdl, n, false);
}

enode parse_statement(silver mod) { 
    fn f = context_model(mod, typeid(fn));
    verify(!f || !f->is_module_init, "unexpected init function");

    print_tokens(mod, "parse-statement");
    mod->last_return = null;
    mod->expr_level  = 0;

    if (f) {
        if (next_is(mod, "no-op"))  return e_noop(mod, null);
        if (next_is(mod, "return")) {
            mod->expr_level++;
            mod->last_return = parse_return(mod);
            return mod->last_return;
        }
        if (next_is(mod, "break"))  return parse_break(mod);
        if (next_is(mod, "for"))    return parse_for(mod);
        if (next_is(mod, "loop"))   return parse_loop_while(mod);
        if (next_is(mod, "if"))     return parse_if_else(mod);
        if (next_is(mod, "ifdef"))
            return parse_ifdef_else(mod);
        if (next_is(mod, "loop"))   return parse_loop_while(mod);
    } else {
        if (next_is(mod, "ifdef"))
            return parse_ifdef_else(mod);
    }
    mod->left_hand = true;

    // lets read ahead to get the type here, so read-node has less scope to cover
    enode   e = read_node(mod, null, null, null); /// at module level, supports keywords
    mod->left_hand = false;
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

fn parse_fn(silver mod, AFlag member_type, Au ident, OPType assign_enum) {
    model      rtype   = null;
    Au         name    = instanceof(ident, typeid(token));
    array      body    = null;
    array      meta    = null;
    if (!name) name    = instanceof(ident, typeid(string));
    bool       is_cast = member_type == AU_FLAG_CAST;
    record     rec_ctx = null;
    model      mdl_ctx = null;    
    context(mod, &rec_ctx, &mdl_ctx, null);

    if (is_cast) {
        rtype = read_model(mod, &body, &meta);
    } else if (!name) {
        name = read_alpha(mod);
        validate(name, "expected alpha-identifier");
    }

    eargs args = eargs(mod, mod, open, true); // default is no args
    validate((is_cast || next_is(mod, "[")) || next_is(mod, "->"), "expected function args [");

    if (next_is(mod, "[")) args = parse_args(mod);
    args->open = false;

    bool guarded = read_if(mod, "->") != null;
    verify(rec_ctx || !guarded, "guard ( -> ) applies only to class fn");

    rtype = read_model(mod, &body, &meta);
    if (!rtype) rtype = emodel("none");
    bool single_expr = body && len(body);
    
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
        validate((member_type & AU_FLAG_CAST) != 0, "with no name, expected cast");
        name = form(string, "cast_%o", rtype);
    }
    
    bool is_static = (member_type & AU_FLAG_SMETHOD) != 0;
    fn f = fn(
        mod,            mod,
        function_type,  member_type,
        is_guarded,     guarded,
        extern_name,    rec_ctx ? f(string, "%o_%o", rec_ctx, name) : (string)name,
        instance,       is_static ? null : rec_ctx,
        rtype,          rtype,
        rmeta,          meta,
        single_expr,    single_expr,
        args,           args,
        body,           (body && len(body)) ? body : read_body(mod),
        cgen,           cgen);
    
    return f;
}

void silver_incremental_resolve(silver mod) {

    pairs(mod->userspace->members, i) {
        emember mem = i->value;
        if (mem != mod->mem_init && instanceof(mem->mdl, typeid(fn)) &&
                !mem->mdl->user_built) {
            build_fn(mod, mem, null, null);
        }
    }

    pairs(mod->userspace->members, i) {
        emember mem = i->value;
        Au_t    t   = isa(mem->mdl);
        record  rec = instanceof(mem->mdl, typeid(record));
        if (mem->mdl->is_system) continue;
        Class   cl  = mem->mdl->src ? instanceof(mem->mdl->src, typeid(Class)) : null;
        if (rec || cl) {
            if (!rec) rec = (record)cl;
            if ((!cl || !cl->is_abstract) && rec && !rec->parsing && !rec->user_built) {
                build_record(mod, cl ? pointer(cl) : (model)rec);
            }
        }
    }
}

void silver_parse(silver mod) {
    /// im a module!
    emember mem_init = initializer(mod); // publish initializer
    map members = mod->members;

    while (peek(mod)) {
        enode res = parse_statement(mod);
        validate(res, "unexpected token found for statement: %o", peek(mod));
        incremental_resolve(mod);
    }

    build_fn(mod, mem_init, build_init_preamble, null);
    finalize(mem_init);
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

string func_ptr(emember emem) {
    fn func = emem->mdl;
    string args = string();
    pairs (func->args->members, arg) {
        emember a = arg->value;
        if (len(args)) append(args, ", ");
        concat(args, f(string, "%o %o", a->mdl, a->name));
    }
    return f(string, "%o (*%o)(%o);", func->rtype, func, args);
}

string method_def(emember emem) {
    return f(string,
        "#define %o(I,...) ({{ __typeof__(I) _i_ = I; ftableI(_i_)->%o(_i_, ## __VA_ARGS__); }})",
            emem->name);
}

static void process_forward(map forwards, model mdl, FILE* f) {
    aether e = mdl->mod;
    Class is_cl = mdl->src ?
        instanceof(mdl->src, typeid(Class)) : null;
    string n_forward = cast(string, mdl);
    if (!get(forwards, n_forward)) {
        fputs(fmt("typedef struct _%o%s %o;\n",
            mdl, is_cl ? "*" : "", mdl)->chars, f);
        set(forwards, n_forward, _bool(true));
    }
}

static string ptr_string(model mdl) {
    return mdl->is_ref ? string("*") : string();
}

static string to_cmodel(model mdl) {
    if (!mdl->atype_src) return null;
    
    Au_t t = mdl->atype_src;
    if (t == typeid(i8))  return string("char");
    if (t == typeid(u8))  return string("unsigned char");
    if (t == typeid(i16)) return string("short");
    if (t == typeid(u16)) return string("unsigned short");
    if (t == typeid(i32)) return string("int");
    if (t == typeid(u32)) return string("unsigned int");
    if (t == typeid(i64)) return string("long long");
    if (t == typeid(u64)) return string("unsigned long long");
    if (t == typeid(fp16)) return string("_fp16");
    if (t == typeid(bf16)) return string("_bf16");
    if (t == typeid(f32)) return string("float");
    if (t == typeid(f64)) return string("double");
    if (t == typeid(f80)) return string("long double");
    if (t == typeid(handle)) return string("void*");
    if (t == typeid(cstr)) return string("char*");
    if (t == typeid(symbol)) return string("const char*");
    if (t == typeid(numeric)) return string("void*");
    if (t == typeid(raw)) return string("void*");
    if (t == typeid(floats)) return string("float*");
    if (t == typeid(cstrs)) return string("char**");
    if (t == typeid(cstrs)) return string("char**");
    if (t == typeid(bool)) return string("unsigned char");
    if (t == typeid(num)) return string("long long");
    if (t == typeid(sz))  return string("long long");
    if (t == typeid(none)) return string("void");
    
    // must define primitive enums
    //if (t == typeid(AFlag))  return string("unsigned long long");
    
    if (mdl->src) {
        string b = to_cmodel(mdl->src);
        return f(string, "%o*", b);
    }
    fault("handle primitive: %s", t->name);
    return string("unhandled");
}

// rather than include Au header, its best to reproduce its exact structure without macros
static void write_header(silver mod) {
    return;
    
    string m   = stem(mod->source);
    path i_gen = f(path, "%o/%o.i",  mod->project_path, m);
    // lets generate a header from our models
    // we are making ONE file here, just import!

    map forwards = map(hsize, 16);
    FILE* f = fopen(cstring(i_gen), "wb");

    #define props(MDL, P_ITER) \
        pairs (MDL->members, P_ITER) \
            if (!instanceof(((emember)P_ITER)->mdl, typeid(fn)))

    #define funcs(MDL, P_ITER) \
        pairs (MDL->members, P_ITER) \
            if (instanceof(((emember)P_ITER)->mdl, typeid(fn)))

    model Au_mdl   = emodel("Au");
    model Au_type  = emodel("Au_t")->src;

    // forward declare all classes used in type/instance containers
    props (Au_mdl, i) {
        emember emem = i->value;
        if (emem->mdl->src && instanceof(emem->mdl->src, typeid(Class)))
            process_forward(forwards, emem->mdl, f);
    }
    props (Au_type, i) {
        emember emem = i->value;
        if (emem->mdl->src && instanceof(emem->mdl->src, typeid(Class)))
            process_forward(forwards, emem->mdl, f);
    }

    // emit all includes first
    each (mod->lex, model, mdl) {
        Au_t t = isa(mdl);
        import im = instanceof(mdl, typeid(import));
        if (im && im->include_paths)
            each(im->include_paths, path, inc) {
                // verify this is the full path to include that we resolve ourselves
                fputs(fmt("#include <%o>\n", inc)->chars, f);
            }
    }

    // emit everything we cannot enumerate here (emit the structs after primitives and aliases)
    //fputs(fmt("typedef struct cereal { char* value; } cereal;\n")->chars, f);
    
    fputs("_Pragma(\"pack(push, 1)\")\n", f);
    each (mod->lex, model, mdl) {
        Au_t t = isa(mdl);
        import im = instanceof(mdl, typeid(import));

        if (im && !im->external) continue;

        // declarations

        // all primitives first
        pairs (mdl->members, i) {
            emember emem = i->value;
            if (emem->is_codegen || !emem->name || !len(emem->name)) continue;
            Au_t type = emem->mdl->atype_src;
            if (!type) continue;
            bool is_primitive = (type->traits & AU_TRAIT_PRIMITIVE) != 0;
            if (!is_primitive) continue;
            model primitive = emem->mdl;
            fputs(fmt("typedef %o %o;\n",
                to_cmodel(emem->mdl), emem->name)->chars, f);
        }

        // emit typedefs used in classes and structs
        pairs (mdl->members, i) {
            emember emem = i->value;
            if (emem->is_codegen) continue;
            if (!emem->name || !len(emem->name)) continue;
            fn    func     = instanceof(emem->mdl, typeid(fn));
            Class cl       = emem->mdl->src ? instanceof(emem->mdl->src, typeid(Class)) : null;
            structure st   = instanceof(emem->mdl, typeid(structure));
            enumeration en = instanceof(emem->mdl, typeid(enumeration));
            model alias    = (!func && !cl && !en && emem->mdl->src) ? emem->mdl : null;
            if (alias) {
                if (isa(alias->src) == typeid(fn)) {
                    fn f = alias->src;
                    pairs(f->args->members, i) {
                        fputs(fmt("%o;\n", func_ptr(i->value))->chars, f);
                    }
                } else {
                    if (emem->mdl->use_count || emem->mdl->count > 0)
                        fputs(fmt("typedef %o%o[%i] %o;\n",
                            emem->mdl->src, ptr_string(emem->mdl),
                            emem->mdl->count, emem->name)->chars, f);
                    else
                        fputs(fmt("typedef %o%o %o;\n",
                            emem->mdl->src, ptr_string(emem->mdl), emem->name)->chars, f);
                }
            } else if (en && !en->atype) {
                fputs(fmt("enum %o {\n", en)->chars, f);
                bool first = true;
                pairs(en->members, i) {
                    if (!first) fputs(fmt(",\n")->chars, f);
                    emember emem = i->value;
                    verify(emem->literal && isa(emem->literal) == typeid(i32), "expected i32 enum (%o)", emem->name);
                    fputs(fmt("\t%o = %o", emem->name, emem->literal)->chars, f);
                    first = false;
                }
                fputs(fmt("\n};\n")->chars, f);
            }
        }

        // emit basic structs (cereal, etc)
        if (!im)
        pairs (mdl->members, i) {
            emember emem = i->value;
            if (emem->is_codegen) continue;
            if (!emem->name || !len(emem->name)) continue;
            structure st = instanceof(emem->mdl, typeid(structure));
            if (emem->mdl->is_system) continue;
            if (st) {
                fputs(fmt("typedef struct _%o {\n", emem->mdl)->chars, f);
                props (st, i) {
                    emember emem = i->value;
                    Au_t t = isa(emem->mdl);
                    Au info = header(emem);
                    if (emem->mdl->count || emem->mdl->use_count)
                        fputs(fmt("\t%o %o[%i];\n", emem->mdl, emem->name, emem->mdl->count)->chars, f);
                    else
                        fputs(fmt("\t%o %o;\n", emem->mdl, emem->name)->chars, f);
                }
                fputs(fmt("} %o;\n", emem->mdl)->chars, f);
            }
        }

        if (!im || im->external) // if external is set, this is a silver compatible module to be used by this .c
        pairs (mdl->members, i) {
            emember emem = i->value;
            if (emem->is_codegen) continue;
            if (!emem->name || !len(emem->name)) continue;
            Au_t  mem_type = isa(emem->mdl);
            fn    func     = instanceof(emem->mdl, typeid(fn));
            Class cl       = emem->mdl->src ? instanceof(emem->mdl->src, typeid(Class)) : null;
            structure st   = instanceof(emem->mdl, typeid(structure));
            enumeration en = instanceof(emem->mdl, typeid(enumeration));
            model alias    = (!func && !cl && emem->mdl->src) ? emem->mdl : null;
            array cls      = (st || !cl) ? 
                a(Au_mdl, st) : class_list(emem->mdl, null, true);
            
            each (cls, model, cl_mdl)
                process_forward(forwards, cl_mdl, f);
            
            if (func) {
                string args = string();
                pairs (func->args->members, arg) {
                    emember a = arg->value;
                    if (len(args)) append(args, ", ");
                    concat(args, fmt("%o %o", a->mdl, a->name));
                }
                fputs(fmt("%o %o(%o);\n", func->rtype, func, args)->chars, f);
            }

            // class
            //      members and the function table
            if (cl || st || (en && en->atype)) {
                record r = emem->mdl;
                if (cl || st) {
                    // emit field members (this is a design-time index into prop bit flags, so we indicate that hte user set a required, even w null value)
                    fputs(fmt("typedef struct %o_fields {\n", r)->chars, f);
                    each (cls, model, cl_mdl)
                        if (isa(cl_mdl) != Au_mdl)
                            props (cl_mdl, i) {
                                emember emem = i->value;
                                process_forward(forwards, emem->mdl, f);
                                fputs(fmt("\t%o %o;\n", emem->mdl, emem->name)->chars, f);
                            }
                    fputs(fmt("};\n")->chars, f);
                }

                // order is preserved from our import, we store in fifo map (the only way to roll)
                fputs(fmt("typedef struct _%o {\n", r)->chars, f);
                each (cls, model, cl_mdl)
                    if (isa(cl_mdl) != Au_mdl)
                        props (cl_mdl, i) {
                            emember emem = i->value;
                            if (emem->mdl->count || emem->mdl->use_count)
                                fputs(fmt("\t%o %o[%i];\n", emem->mdl, emem->name, emem->mdl->count)->chars, f);
                            else
                                fputs(fmt("\t%o %o;\n", emem->mdl, emem->name)->chars, f);
                        }
                if (cl) {
                    fputs(fmt("\tstruct _%o_f *f, *f2;\n", emem->name)->chars, f);
                    fputs(fmt("} *%o;\n", emem->name)->chars, f);
                } else {
                    fputs(fmt("} %o;\n", emem->name)->chars, f);
                }
            } else if (alias) {
                continue;
                
                if (emem->mdl->use_count || emem->mdl->count > 0)
                    fputs(fmt("typedef %o[%i] %o;\n",
                        emem->mdl->src, emem->mdl->count, emem->name)->chars, f);
                else
                    fputs(fmt("typedef %o %o;\n", emem->mdl->src, emem->name)->chars, f);
            }
            // write f table for all classes (including Au; we only hide their field members)
            fputs(fmt("typedef struct _%o_f {\n", emem->mdl)->chars, f);
            
            pairs (Au_type->members, i) {
                emember emem = i->value;
                if (isa(emem->mdl) != typeid(fn))
                    fputs(fmt("\t%o %o;\n", emem->mdl->mem->name, emem->name)->chars, f);
            }
            each (cls, model, cl_mdl)
                funcs (cl_mdl, i) {
                    emember fmem = i->value;
                    if (fmem->access == interface_public)
                        fputs(fmt("\t%o\n", func_ptr(fmem)), f);
                }
            fputs(fmt("} %o_f, *%o_ft;\n", emem->name, emem->name)->chars, f);

            each (cls, model, cl_mdl)
                funcs (cl_mdl, i) {
                    emember fmem = i->value;
                    if (fmem->access == interface_public) {
                        fputs(fmt("\t%o\n", func_ptr(fmem)), f);
                    }
                }

            // generate methods, lets just #undef and then #define each
            each (cls, model, cl_mdl)
                funcs (cl_mdl, i) {
                    emember fmem = i->value;
                    if (fmem->access == interface_public) {
                        fputs(fmt("#undef %o\n", fmem->name), f);
                        fputs(fmt("%o\n", method_def(fmem)), f);
                    }
                }
        }
    }
    fputs("_Pragma(\"pack(pop)\")\n", f);
    fclose(f);
}

// implement watcher now
void silver_init(silver mod) {
    mod->defs     = map(hsize, 8);
    mod->codegens = map(hsize, 8);
    mod->import_cache = map(hsize, 8);

    bool is_once = mod->single; // -s or --single [ single build ]

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
            print("awaiting iteration: %o", mod->source);
            hold_members(mod);
            Au_recycle();
            mtime = path_wait_for_change(mod->source, mtime, 0);
            print("rebuilding...");
            drop(mod->tokens);
            drop(mod->stack);
            reinit_startup(mod);
        }
        retry = false;
        mod->tokens  = parse_tokens(mod, mod->source);
        mod->stack   = array(4);
        mod->implements = array();
        
        // our verify infrastructure is now production useful
        attempt() {
            string m = stem(mod->source);
            path i_gen    = f(path, "%o/%o.i",  mod->project_path, m);
            path c_file   = f(path, "%o/%o.c",  mod->project_path, m);
            path cc_file  = f(path, "%o/%o.cc", mod->project_path, m);
            path files[2] = { c_file, cc_file };
            for (int i = 0; i < 2; i++)
                if (exists(files[i])) {
                    if (!mod->implements) mod->implements = array(2);
                    push(mod->implements, files[i]);
                }
            parse(mod);
            build(mod);
            
            if (len(mod->implements))
                write_header(mod);
        } on_error() {
            retry = !is_once;
        }
        finally()
    } while (retry);
}

silver silver_load_module(silver mod, path uri) {
    emember mem = emember(mod, mod, name, stem(uri));
    // uses cache control from import
    silver mod_load = silver(
        source,  mod->source,
        install, mod->install,
        mem,     mem);
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
        mod->install, mod);
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
            if (!commit) {
                command c = f(command, "git remote show origin");
                string res = run(c);
                verify(starts_with(res, "HEAD branch: "), "unexpected result for git remote show origin");
                commit = mid(res, 13, len(res) - 13);
            }
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
    } else if (is_silver) { // build for Au-type projects
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
    path cwd = path_cwd();
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
    Au_t is_codegen = null;
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
        Au_t  f          = Au_find_type((cstr)aa->chars);

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
            codegen,    cg,
            is_global,  cg == null && !namespace,
            external,   external,
            tokens,     tokens);

        set(mod->import_cache, tokens, mdl);
    }

    emember mem = emember(
        mod,    mod,
        name,   namespace,
        mdl,    mdl);
    register_member(mod, mem, true);
    if (!has_cache && !is_codegen) {
        mdl->open = true;
        push(mod, mdl);
        mdl->include_paths = array();

        // include each, collecting the clang instance for which we will invoke macros through
        each (includes, string, inc) {
            clang_cc instance;
            path i = include(mod, inc, &instance);
            push(mdl->include_paths, i);
            set(mod->instances, i, instance);
        }

        each(module_paths, path, m) {
            Au_import(mod, m);
        }

        // should only do this if its a global import and has no namespace
        if (namespace)
            pop(mod);
        mdl->open = false; // registrations do not flow into these global imports
    }

    mdl->module_paths  = hold(module_paths);

    if (is_codegen) {
        mem->is_codegen = true;
        string name = namespace ? (string)namespace : string(is_codegen->name);
        set(mod->codegens, name, mdl->codegen);
    }
    
    return mem;
}


/*
Au request2(uri url, map args) {
    map     st_headers   = new(map);
    Au       null_content = null;
    map     headers      = contains(args, "headers") ? (map)get (args, "headers") : st_headers;
    Au       content      = contains(args, "content") ? get (args, "content") : null_content;
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
    Au_engage(argv);
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
        concat(str_args, f(string, "%o: %o", name, mem->name));
    }
    string signature = f(string, "fn %o[%o] -> %o", f, str_args, f->rtype);
    
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
        each (msg, Au, info) {
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