#include <silver/silver.hpp>

/// use mx internally
#include <mx/mx.hpp>

namespace ion {

struct Ident;
struct ident {
    A_decl(ident, Ident)
    str &operator[](int i) const;
};

struct Ident:A {
    vector<str> strings;
    int line_num;

    Ident(null_t = null) : A(typeof(Ident)) { }
    Ident(const M &m, int line = 0) : Ident() {
        if (m.type() == typeof(String))
            strings = strings = vector<str> { str(m) };
        else if (m.type() == typeof(Ident))
            strings = ((Ident*)m.a)->strings;
        else if (m.type() == typeof(Vector<M>))
            for (String &s: *(Vector<M>*)m.a)
                strings += s;
        else if (m.type() == typeof(Vector<str>))
            for (String &s: *(Vector<str>*)m.a)
                strings += s;
        line_num = line;
    }
    String *to_string() override {
        str res(1024);
        for (String &s: strings) {
            if (res)
                res += ".";
            res += s;
        }
        return res;
    }
    int compare(const M &m) const override {
        bool same;
        if (m.type() == typeof(Ident)) {
            Ident &b = (Ident&)m;
            if (b.strings->len() == strings->len()) {
                int i = 0;
                same = true;
                for (str &s: strings) {
                    if (s != b.strings[i++]) {
                        same = false;
                        break;
                    }
                }
            } else
                same = false;
        } else {
            str b = m;
            if (strings->len() == 1)
                same = strings[0] == b;
            else
                same = false;
        }
        return same ? 0 : -1;
    }
    u64 hash() override {
        u64 h = OFFSET_BASIS;
        for (str &fs: strings) {
            h *= FNV_PRIME;
            h ^= fs->hash();
        }
        return h;
    }
    bool operator==(const str &s) const {
        if (len() == 1 && strings[0] == s)
            return true;
        return false;
    }
    str &operator[](int i) const { return strings[i]; }
    int len()              const { return strings->len(); }
    operator bool()        const { return strings->len() > 0; }
};

A_impl(ident, Ident)

str &ident::operator[](int i) const { return (*data)[i]; }

void assertion(bool cond, const str& m, const array& a = {}) {
    if (!cond)
        console.fault(m, a);
};

static vector<str> keywords = { "class", "struct", "import", "return", "asm", "if", "switch", "while", "for", "do" };

struct enode {
    A_decl(enode, ENode)
};

struct ENode:A {
    enums(Type, Undefined,
        Undefined, 
        Statements, Assign, AssignAdd, AssignSub, AssignMul, AssignDiv, AssignOr, AssignAnd, AssignXor, AssignShiftR, AssignShiftL, AssignMod,
        If, For, While, DoWhile,
        LiteralReal, LiteralInt, LiteralStr, LiteralStrInterp, Array, AlphaIdent, Var, Add, Sub, Mul, Div, Or, And, Xor, MethodDef, MethodCall, MethodReturn)

    Type    etype;
    M       value;
    vector<enode> operands;

    static enode create_operation(Type etype, const vector<enode>& operands) {
        ENode* op = new ENode;
        op->etype    = etype;
        op->operands = operands;
        return op;
    }

    static enode create_value(Type etype, const M& value) {
        ENode* op = new ENode;
        op->etype    = etype;
        op->value    = value;
        return op;
    }

    ENode() : A(typeof(ENode)) { }

    ENode(ident& id_var) : ENode() {
        etype = Type::Var;
        value = id_var;
    }

    static M lookup(const vector<map> &stack, ident id, bool top_only, bool &found) {
        for (int i = stack->len() - 1; i >= 0; i--) {
            map &m = stack[i];
            Field *f = m->fetch(id);
            if (f) {
                found = true;
                return f->value;
            }
            if (top_only)
                break;
        }
        found = false;
        return null;
    }

    static str string_interpolate(const M &m_input, const vector<map> &stack) {
        str input = str(m_input);
        str output = input->interpolate([&](const str &arg) -> str {
            ident f = arg;
            bool found = false;
            M m = lookup(stack, f, false, found);
            if (!found)
                return arg;
            return str(m.to_string());
        });
        return output;
    }
    
    static var exec(const enode &op, const vector<map> &stack) {
        switch (op->etype) {
            // we need 
            case Type::LiteralInt:
            case Type::LiteralReal:
            case Type::LiteralStr:
                return var(op->value);
            case Type::LiteralStrInterp:
                return var(string_interpolate(op->value, stack));
            case Type::Array: {
                array res(op->operands->len());
                for (enode& operand: op->operands) {
                    var v = exec(operand, stack);
                    res->push(v);
                }
                return var(res);
            }
            case Type::Var: {
                assert(op->value.type() == typeof(Ident));
                bool found = false;
                M m = lookup(stack, op->value, false, found);
                if (found)
                    return var(m); /// in key results, we return its field
                console.fault("variable not declared: {0}", { op->value });
                throw Type(Type::Var);
                break;
            }
            case Type::Add: return exec(op->operands[0], stack) +  exec(op->operands[1], stack);
            case Type::Sub: return exec(op->operands[0], stack) -  exec(op->operands[1], stack);
            case Type::Mul: return exec(op->operands[0], stack) *  exec(op->operands[1], stack);
            case Type::Div: return exec(op->operands[0], stack) /  exec(op->operands[1], stack);
            case Type::And: return exec(op->operands[0], stack) && exec(op->operands[1], stack);
            case Type::Or:  return exec(op->operands[0], stack) || exec(op->operands[1], stack);
            case Type::Xor: return exec(op->operands[0], stack) ^  exec(op->operands[1], stack);
            default:
                break;
        }
        return var(null);
    }

    operator bool() { return etype != null; }
};

enums(Access, Public,
    Public, Intern)

A_impl(enode, ENode)

struct Index {
    int i;
    Index(int i) : i(i) { }
    operator bool() {
        return i >= 0;
    }
    operator int() {
        return i;
    }
};


struct TypeIdent;
struct type_ident {
    A_decl(type_ident, TypeIdent)
};
struct TypeIdent:A {
    ident base_type;
    TypeIdent* map;
    int   array;
    vector<TypeIdent*> dims; /// array, map of int, array, map of string  <- would be from  type[][int][][type[string]]
    TypeIdent() : A(typeof(TypeIdent)) {
        map = null;
        array = 0;
    }
};

A_impl(type_ident, TypeIdent)

struct EMember;
struct emember {
    A_decl(emember, EMember)
};

struct EMember:A {
    enums(Type, Undefined,
        Undefined, Variable, Lambda, Method)
    bool            intern; /// intern is not reflectable
    Type            member_type; /// this could perhaps be inferred from the members alone but its good to have an enum
    str             name;
    type_ident      type;  /// for lambda, this is the return result
    str             base_class;
    vector<ident>   value; /// code for methods go here; for lambda, this is the default lambda instance; lambdas of course can be undefined
    vector<emember> args;  /// args for both methods and lambda; methods will be on the class model alone, not in the 'instance' memory of the class
    vector<ident>   base_forward;
    bool            is_ctr;
    EMember() : A(typeof(EMember)) { }
    operator bool() { return bool(name); }
};


A_impl(emember, EMember)

struct Parser {
    static inline vector<ident> assign = {":", "+=", "-=", "*=", "/=", "|=", "&=", "^=", ">>=", "<<=", "%="};
    vector<ident> tokens;
    int cur = 0;

    Parser(str input) {
        str           sp         = "$()![]/+-*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
        char          until      = 0; /// either ) for $(script) ", ', f or i
        sz_t          len        = input->len();
        char*         origin     = input->cs();
        char*         start      = 0;
        char*         cur        = origin - 1;
        int           line_num   = 0;
        bool          new_line   = true;
        bool          token_type = false;
        bool          found_null = false;
        tokens = vector<ident>(256 + input->len() / 8);
        ///
        while (*(++cur)) {
            bool is_ws = false;
            if (!until) {
                if (new_line)
                    new_line = false;
                if (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r') {
                    is_ws = true;
                    ws(cur);
                }
            }
            if (!*cur) break;
            bool add_str = false;
            char *rel = cur;
            if (*cur == '#') { // comment
                while (*cur && *cur != '\n')
                    cur++;
                found_null = !*cur;
                new_line = true;
            }
            if (until) {
                if (*cur == until && *(cur - 1) != '/') {
                    add_str = true;
                    until = 0;
                    cur++;
                    rel = cur;
                }
            }
            if (!until) {
                int type = sp->index_of(*cur);
                new_line |= *cur == '\n';

                if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                    tokens += parse_token(start, (sz_t)(rel - start), line_num);
                    if (!add_str) {
                        if (*cur == '$' && *(cur + 1) == '(') // shell
                            until = ')';
                        else if (*cur == '"') // double-quote
                            until = '"';
                        else if (*cur == '\'') // single-quote
                            until = '\'';
                    }
                    if (new_line) {
                        start = null;
                    } else {
                        ws(cur);
                        start = cur;
                        token_type = (type >= 0);
                    }
                }
                else if (!start && !new_line) {
                    start = cur;
                    token_type = (type >= 0);
                }
            }
            if (new_line) {
                until = 0;
                line_num++;
                if (found_null)
                    break;
            }
        }
    }

    static void ws(char *&cur) {
        while (*cur == ' ' || *cur == '\r' || *cur == '\t') ++cur;
    }

    static ident parse_token(char *start, sz_t len, int line_num) {
        while (start[len - 1] == '\t' || start[len - 1] == ' ')
            len--;
        str all { start, len };
        char t = all[0];
        return (t == '-' || (t >= '0' && t <= '9')) ? Ident(all, line_num) : Ident(all->split("."), line_num); /// mr b: im a token!
    }

    vector<ident> parse_raw_block() {
        assertion(next() == "[", "expected beginning of block [");
        pop();
        vector<ident> res;
        res += pop();
        int level = 1;
        for (;;) {
            if (next() == "[") {
                level++;
            } else of (next() == "]") {
                level--;
            }
            res += pop();
            if (level == 0)
                break;
        }
        return res;
    }

    emember parse_member(A* object);

    /// parse members from a block
    vector<emember> parse_args() {
        vector<emember> result;
        assertion(pop() == "[", "expected [ for arguments");
        /// parse all symbols at level 0 (levels increased by [, decreased by ]) until ,

        /// # [ int arg, int arg2[int, string] ]
        /// # args look like this, here we have a lambda as a 2nd arg
        /// 
        while (next() && next() != "]") {
            emember a = parse_member();
            a->type = parse_type();
            a->name = pop();
            assertion(next() == "]" || next() == ",", ", or ] in arguments");
        }
        assertion(pop() == "]", "expected end of args ]");
        return result;
    }

    type_ident parse_type() {
        lambda<void(TypeIdent*)> parse_next;
        parse_next = [&](TypeIdent *type_ident) {
            if (is_alpha_ident(next()))
                type_ident->base_type = pop();
            if (next() != "[") return;
            int level = 1;
            while (next()) {
                if (next() == "]") {
                    assertion(level > 0, "unexpected ] when parsing type");
                    if (!type_ident->map)
                        type_ident->array = 1;
                    level--;
                    if (next() != "[" && level == 0)
                        break;
                } else if (next() == ",") {
                    int dims = 1;
                    while (next() == ",") {
                        dims++;
                        pop();
                    }
                    type_ident->array = dims;
                    assertion(next() == "]", "expected array ] declaration");
                } else {
                    if (level == 0)
                        break; /// wandering off
                    type_ident->map = new TypeIdent();
                    parse_next(type_ident->map);
                }
            }
            assertion(level == 0, "expected valid type identifier");
        };

        assertion(is_alpha_ident(next()), "expected identifier for type");
        type_ident type;
        parse_next(type);
        return type;
    }

    ident token_at(int rel) {
        if ((cur + rel) >= tokens->len())
            return {};
        return tokens[cur + rel];
    }

    ident next() {
        return token_at(0);
    }

    ident pop() {
        if (cur >= tokens->len())
            return {};
        return tokens[cur++];
    }

    void consume() { cur++; }

    ENode::Type is_numeric(ident& token) {
        if (token->len() > 1)
            return ENode::Type::Undefined;
        str t = token[0];
        return (t[0] >= '0' && t[0] <= '9') ? (strchr(t->cs(), '.') ? 
            ENode::Type::LiteralReal : ENode::Type::LiteralInt) : ENode::Type::Undefined;
    };

    ENode::Type is_string(ident& token) {
        if (token->len() > 1)
            return ENode::Type::Undefined;
        char t = token[0][0];
        return t == '"' ? ENode::Type::LiteralStr : t == '\'' ? ENode::Type::LiteralStrInterp : ENode::Type::Undefined;
    };

    Index expect(ident& token, const vector<ident> &tokens) {
        Index i = tokens->index_of(token);
        return i;
    };

    ENode::Type is_alpha_ident(ident token) { /// type, var, or method (all the same); its a name that isnt a keyword
        char t = token[0][0];
        if (isalpha(t) && keywords->index_of(token[0]) == -1) {
            /// lookup against variable table; declare if in isolation
            return ENode::Type::AlphaIdent; /// will be Var, or Type after
        }
        return ENode::Type::Undefined;
    };

    ident &next(int rel = 0) const {
        return tokens[cur + rel];
    }

    ENode::Type is_assign(const ident& token) {
        int id = assign->index_of(token);
        if (id >= 0)
            return ENode::Type((int)ENode::Type::Assign + id);
        return ENode::Type::Undefined;
    }

    enode parse_statements() {
        vector<enode> block;
        bool multiple = next() == "[";
        while (next()) {
            enode n = parse_statement();
            assertion(n, "expected statement or expression");
            block += n;
            if (!multiple)
                break;
            else if (next() == "]")
                break;
        }
        if (multiple) {
            assertion(next() == "]", "expected end of block ']'");
            consume();
        }
        return ENode::create_operation(ENode::Type::Statements, block);
    }

    enode parse_statement() {
        ident t0 = next(0);
        if (is_alpha_ident(t0)) {
            ident t1 = next(1);
            ENode::Type assign = is_assign(t1);
            if (assign) {
                consume();
                consume();
                return ENode::create_operation(assign, { t0, parse_expression() });
            } else {
                if (t0 == "for") {
                    consume();
                    assertion(next() == "[", "expected condition expression '['"); consume();
                    enode statement = parse_statement();
                    assertion(next() == ";", "expected ;"); consume();
                    enode condition = parse_expression();
                    assertion(next() == ";", "expected ;"); consume();
                    enode post_iteration = parse_expression();
                    assertion(next() == "]", "expected ]"); consume();
                    return ENode::create_operation(ENode::Type::For, vector<enode> { statement, condition, post_iteration });
                } else if (t0 == "while") {
                    consume();
                    assertion(next() == "[", "expected condition expression '['"); consume();
                    enode condition  = parse_expression();
                    assertion(next() == "]", "expected condition expression ']'"); consume();
                    enode statements = parse_statements();
                    return ENode::create_operation(ENode::Type::While, { condition, statements });
                } else if (t0 == "if") {
                    consume();
                    assertion(next() == "[", "expected condition expression '['"); consume();
                    enode condition  = parse_expression();
                    assertion(next() == "]", "expected condition expression ']'"); consume();
                    enode statements = parse_statements();
                    enode else_statements;
                    bool else_if = false;
                    if (next() == "else") { /// if there is no 'if' following this, then there may be no other else's following
                        consume();
                        else_if = next() == "if";
                        else_statements = parse_statements();
                        assertion(!else_if && next() == "else", "else proceeding else");
                    }
                    return ENode::create_operation(ENode::Type::If, { condition, statements, else_statements });
                } else if (t0 == "do") {
                    consume();
                    enode statements = parse_statements();
                    assertion(next() == "while", "expected while");                consume();
                    assertion(next() == "[", "expected condition expression '['"); consume();
                    enode condition  = parse_expression();
                    assertion(next() == "]", "expected condition expression '['");
                    consume();
                    return ENode::create_operation(ENode::Type::DoWhile, { condition, statements });
                } else {
                    return parse_expression();
                }
            }
        } else {
            return parse_expression();
        }
    }


    i64 parse_numeric(ident &token) {
        char *e;
        i64   num = strtoll(token[0]->cs(), &e, 10);
        return num;
    }

    ENode::Type is_var(ident &token) { /// type, var, or method (all the same); its a name that isnt a keyword
        char t = token[0][0];
        if (isalpha(t) && keywords->index_of(token[0]) == -1) {
            /// lookup against variable table; declare if in isolation
            return ENode::Type::Var;
        }
        return ENode::Type::Undefined;
    }

    enode parse_add() {
        enode left = parse_mult();
        while (next() == "+" || next() == "/") {
            ENode::Type etype = next() == "+" ? ENode::Type::Add : ENode::Type::Sub;
            consume();
            enode right = parse_mult();
            left = ENode::create_operation(etype, { left, right });
        }
        return left;
    }

    enode parse_mult() {
        enode left = parse_primary();
        while (next() == "*" || next() == "/") {
            ENode::Type etype = next() == "*" ? ENode::Type::Mul : ENode::Type::Div;
            consume();
            enode right = parse_primary();
            left = ENode::create_operation(etype, { left, right });
        }
        return left;
    }

    enode parse_primary() {
        ident id = next();
        console.log("parse_primary: {0}", { id });
        ENode::Type n = is_numeric(id);
        if (n) {
            ident f = next();
            assert(f->len() == 1);
            cstr cs = f[0]->cs();
            bool is_int = n == ENode::Type::LiteralInt;
            consume();
            return ENode::create_value(n, M::from_string(cs, is_int ? typeof(i64) : typeof(double)));
        }
        ENode::Type s = is_string(id);
        if (s) {
            ident f = next();
            assert(f->len() == 1);
            cstr cs = f[0]->cs();
            consume();
            str str_literal = M::from_string(cs, typeof(String));
            assert(str_literal->len() >= 2);
            str_literal = str_literal->mid(1, str_literal->len() - 2);
            return ENode::create_value(s, str_literal);
        }
        ENode::Type i = is_var(id); /// its a variable or method (same thing; methods consume optional args)
        if (i) {
            ident f = next();
            consume();
            return ENode::create_value(i, f); /// the entire Ident is value
        }

        if (next() == "(") {
            consume();
            enode op = parse_expression();
            expect(str(")"));
            consume();
            return op;
        }
        return {};
    }

    enode parse_expression() {
        return parse_add();
    }

    bool expect(const ident &token) {
        if (token != tokens[cur])
            console.fault("expected token: {0}", {token});
        return true;
    }

};

struct EProp:A {
    str             name;
    Access          access;
    vector<ident>   type;
    vector<ident>   value;

    EProp() : A(typeof(EProp)) { }
    operator bool() { return bool(name); }
};

struct eprop {
    A_decl(eprop, EProp)
};
A_impl(eprop, EProp)

struct EnumSymbol:A {
    str             name;
    int             value;
    EnumSymbol() : A(typeof(EnumSymbol)) { }
    EnumSymbol(str name, int value) : A(typeof(EnumSymbol)), name(name), value(value) { }
    operator bool() { return bool(name); }
};

struct enum_symbol {
    A_decl(enum_symbol, EnumSymbol)
};
A_impl(enum_symbol, EnumSymbol)

struct EnumDef:A {
    str                 name;
    bool                intern;
    vector<enum_symbol> symbols;

    EnumDef(bool intern = false) : A(typeof(EnumDef)), intern(intern) { }

    EnumDef(Parser &parser, bool intern) : EnumDef(intern) {
        ident token_name = parser.pop();
        assertion(parser.is_alpha_ident(token_name), "expected qualified name for enum, found {0}", { token_name });
        name = token_name;
        assertion(parser.pop() == "[", "expected [ in enum statement");
        i64  prev_value = 0;
        for (;;) {
            ident symbol = parser.pop();
            ENode* exp = parser.parse_expression(); /// this will pop tokens until a valid expression is made
            if (symbol == "]")
                break;
            assertion(parser.is_alpha_ident(symbol),
                "expected identifier in enum, found {0}", { symbol });
            ident peek = parser.next();
            if (peek == ":") {
                parser.pop();
                enode enum_expr = parser.parse_expression();
                M enum_value = ENode::exec(enum_expr, {});
                assertion(enum_value.type()->traits & traits::integral,
                    "expected integer value for enum symbol {0}, found {1}", { symbol, enum_value });
                prev_value = i64(enum_value);
                assertion(prev_value >= INT32_MIN && prev_value <= INT32_MAX,
                    "integer out of range in enum {0} for symbol {1} ({2})", { name, symbol, prev_value });
            } else {
                prev_value += 1;
            }
            symbols += enum_symbol(symbol, prev_value);

        }
    }
    operator bool() { return bool(name); }
};

struct enum_def {
    A_decl(enum_def, EnumDef)
};
A_impl(enum_def, EnumDef)

struct EConstruct:A {
    Access          access;
    vector<emember> args;
    vector<vector<ident>> initializers;
    vector<ident>   body; /// we create an enode out of this once we parse all EConstructs

    EConstruct() : A(typeof(EConstruct)) { }
    operator bool() { return bool(body); }
};

struct econstruct {
    A_decl(econstruct, EConstruct)
};
A_impl(econstruct, EConstruct)


struct EMethod:A {
    str             name;
    Access          access;
    vector<emember> args;
    vector<ident>   body;

    EMethod() : A(typeof(EMethod)) { }
    operator bool() { return bool(name); }
};

struct emethod {
    A_decl(emethod, EMethod)
};
A_impl(emethod, EMethod)

struct EClass;
struct eclass { A_decl(eclass, EClass) };
struct EClass:A {
    str         name;
    bool        intern;
    str         from; /// inherits from
    map         econstructs;
    map         eprops;
    map         emethods;
    map         friends; // these friends have to exist in the module

    EClass(bool intern = false)         : A(typeof(EClass)) { }
    EClass(Parser &parser, bool intern) : EClass(intern) {
        /// parse class members, which includes econstructs, eprops and emethods
        assertion(parser.pop() == "class", "expected class");
        if (parser.next() == ":") {
            parser.consume();
            from = parser.pop();
        }
        assertion(parser.is_alpha_ident(parser.next()), "expected class identifier");
        name = parser.pop();
        assertion(parser.pop() == "[", "expected beginning of class");
        for (;;) {
            ident t = parser.next();
            if (!t || t == "]")
                break;
            /// expect intern, or type-name token
            bool intern = false;
            if (parser.next() == "intern") {
                parser.pop();
                intern = true;
            }

            ident m0 = parser.next();
            assertion(parser.is_alpha_ident(m0), "expected type identifier");
            bool is_construct = m0 == ident(name);
            type_ident member_type;

            if (!is_construct) {
                bool is_cast = m0 == "cast";
                if (is_cast)
                    parser.pop();

                members += parser.parse_member(this);
            }

            /// int [] name [int arg] [ ... ]
            /// int [] array
            /// int [int] map
        }
        assertion(parser.next() == "]", "expected end of class");
        /// classes and structs
    }
    operator bool() { return bool(name); }
};
A_impl(eclass, EClass)


struct EStruct;
struct estruct { A_decl(estruct, EStruct) };
struct EStruct:A {
    str         name;
    bool        intern;
    map         eprops;

    EStruct(bool intern = false)         : A(typeof(EStruct)) { }
    EStruct(Parser &parser, bool intern) : EStruct(intern) {
        /// parse struct members, which includes eprops
        assertion(parser.pop() == "struct", "expected struct");
    }
    operator bool() { return bool(name); }
};
A_impl(estruct, EStruct)


emember Parser::parse_member(A* obj_type) {
    EStruct* st = null;
    EClass*  cl = null;
    str parent_name;
    if (obj_type->type == typeof(EStruct))
        parent_name = ((EStruct*)obj_type)->name;
    else if (obj_type->type == typeof(EClass))
        parent_name = ((EClass*)obj_type)->name;
    emember result;
    bool is_ctr = false;

    if (next() == parent_name) {
        assertion(obj_type->type == typeof(EClass), "expected class when defining constructor");
        result->name = pop();
        is_ctr = true;
    } else {
        result->type = parse_type(); /// lets test this first.
        assertion(is_alpha_ident(next()), "expected identifier for member, found {0}", { next() });
        result->name = pop();
    }
    ident n = next();
    assertion((n == "[" && is_ctr) || !is_ctr, "invalid syntax for constructor; expected [args]");
    if (n == "[") {
        // [args] this is a lambda or method
        result->args = parse_args();
        if (is_ctr) {
            if (next() == ":") {
                pop();
                ident class_name = pop();
                assertion(class_name == cl->from || class_name == parent_name, "invalid constructor base call");
                result->base_class = class_name;
                result->base_forward = parse_raw_block();
                result->value = parse_raw_block();
            }
        } else {
            if (next() == ":" || next() != "[") {
                result->member_type = EMember::Type::Lambda;
                if (next() == ":") {
                    pop();
                    assertion(next() == "[", "given assignment, expected [lambda code block]");
                    /// we need to parse the tokens alone here, since this is the first pass
                    result->value = parse_raw_block();
                }
            } else {
                assertion(next() == "[", "expected [method code block]");
                result->member_type = EMember::Type::Method;
            }
        }
    } else if (n == ":") {
    } else {
        // not assigning variable
    }
    return result;
}

struct EVar;
struct evar { A_decl(evar, EVar) };
struct EVar:A {
    EVar(bool intern = false) : A(typeof(EVar)), intern(intern) { }
    EVar(Parser &parser, bool intern) : EVar(intern) {
        type  = parser.pop();
        assertion(parser.is_alpha_ident(type), "expected type identifier, found {0}", { type });
        name = parser.pop();
        assertion(parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
        if (parser.next() == ":") {
            parser.consume();
            initializer = parser.parse_statements(); /// this needs to work on initializers too
            /// we need to think about the data conversion from type to type; in C++ there is operator and constructors battling
            /// i would like to not have that ambiguity, although thats a very easy ask and tall order
            /// 
        }
    }
    str         name;
    ident       type;
    enode       initializer;
    bool        intern;
    operator bool() { return bool(name); }
};
A_impl(evar, EVar)

struct EImport;
struct eimport { A_decl(eimport, EImport) };
struct EImport:A {
    EImport() : A(typeof(EImport)) { }
    EImport(Parser& parser) : EImport() {
        assertion(parser.pop() == "import", "expected import");
        ident mod = parser.pop();
        ident as = parser.next();
        if (as == "as") {
            parser.consume();
            isolate_namespace = parser.pop();
        }
        assertion(parser.is_string(mod), "expected type identifier, found {0}", { type });
        name = parser.pop();
        assertion(parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
    }
    str         name;
    str         isolate_namespace;
    path        module_path;
    operator bool() { return bool(name); }
};
A_impl(eimport, EImport)

struct einclude { A_decl(einclude, EInclude) };
struct EInclude:A {
    EInclude() : A(typeof(EInclude)) { }
    vector<str> library; /// optional libraries
    path        include_file;
    map         defines;
    operator bool() { return bool(include_file); }
};
A_impl(einclude, EInclude)

struct EModule;
struct emodule { A_decl(emodule, EModule) };
struct EModule:A {
    EModule() : A(typeof(EModule)) { }
    vector<eimport>  imports;
    vector<einclude> includes;
    map              implementation;

    /// i think its a better idea to not have embedded classes and structs, which may be hilarious coming from me (see: mx model)
    /// its more than just being unable to really parse it effectively on first pass.  with modules, you simply dont need this
    static EModule *parse(path module_path) {
        EModule *m = new EModule();
        str contents = module_path->read<str>();
        Parser parser = Parser(contents);
        int imports = 0;
        for (;;) {
            ident token = parser.next();
            bool intern = false;
            if (token == "intern") {
                parser.consume();
                token  = parser.pop();
                intern = true;
            }
            auto push_implementation = [&](ident keyword, ident name, M value) {
                assertion(!m->implementation->fetch(name),  "duplicate identifier for {0}: {1}", { keyword, name });
                m->implementation[name] = value;
            };

            if (token == "import") {
                assertion(!intern, "intern keyword not applicable to import");
                eimport import(parser);
                imports++;
                str import_str = "import{0}";
                str import_id = import_str->format({ imports });
                push_implementation(token, import_id, import);
            } else if (token == "enum") {
                enum_def edef(parser, intern);
                push_implementation(token, edef->name, edef);
            } else if (token == "class") {
                eclass cl(parser, intern);
                push_implementation(token, cl->name, cl);
            } else if (token == "struct") {
                estruct st(parser, intern);
                push_implementation(token, st->name, st);
            } else {
                evar data(parser, intern);
                push_implementation(token, data->name, data);
            }
        }
        return m;
    }
};
A_impl(emodule, EModule)

}

using namespace ion;

int main(int argc, char **argv) {
    map  def     { field { "source",  path(".") } };
    map  args    { map::args(argc, argv, def, "source") };
    path source  { args["source"]  };

    /// the script will have access to our types, and their methods
    /// we should be fine with having just 1 software source given, no 'project' json or anything of the sort.  why would that be needed?
    /// similarly to python, we only need a root module given and a whole program can be built and run

    emodule m = EModule::parse(source);
    return 0;
}