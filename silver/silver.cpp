#include <silver/silver.hpp>

/// use mx internally
#include <mx/mx.hpp>

namespace ion {

/// we will describe keywords in here first
/// a_for, a_while, a_if and we make them design-time operators

struct ident;
struct Ident:A {
    vector<str> strings;
    int line_num;

    Ident(null_t = null) : A(typeof(ident)) { }
    Ident(object m, int line = 0) : Ident() {
        if (m.type() == typeof(str))
            strings = vector<str> { m.to_string() };
        else if (m.type() == typeof(vector<str>))
            for (str &s: *(vector<str>*)m)
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
    int compare(const object& m) override {
        bool same;
        if (m.type() == typeof(Ident)) {
            Ident &b = m;
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

struct ident {
    /// making all A-based classed called M inside of the user type; we support modular internals this way still
    UA_decl(ident, Ident)
    str &operator[](int i) const;
};

UA_impl(ident, Ident)

str &ident::operator[](int i) const { return a->strings[i]; }

void assertion(bool cond, const str& m, const array& a = {}) {
    if (!cond)
        console.fault(m, a);
};

static vector<str> keywords = { "class", "struct", "import", "return", "asm", "if", "switch", "while", "for", "do" };

struct EClass;
struct EStruct;

struct unique_type {
    object type; // object can be anything! // it could be an array [ typeof(int) ] // lets make the simplest model even if it takes a bit more run-time
    // more here later
};

/// twins; just data and user
struct var_bind;
struct Var_Bind:A {
    ident       name;
    unique_type utype;
    bool        read_only;
    Var_Bind() : A(typeof(var_bind)) { }
};

struct var_bind {
    UA_decl(var_bind, Var_Bind);
};

UA_impl(var_bind, Var_Bind);

using var_binds = vector<var_bind>;

struct enode {
    UA_decl(enode, ENode)
};

struct ENode:A {
    enums(Type, Undefined,
        Undefined, 
        Statements, Assign, AssignAdd, AssignSub, AssignMul, AssignDiv, AssignOr, AssignAnd, AssignXor, AssignShiftR, AssignShiftL, AssignMod,
        If, For, While, DoWhile, Break,
        LiteralReal, LiteralInt, LiteralStr, LiteralStrInterp, Array, AlphaIdent, Var, Add, Sub, Mul, Div, Or, And, Xor, MethodDef, MethodCall, MethodReturn)
   
    Type    etype;
    object  value;
    vector<enode> operands;
    var_binds vars;

    static enode create_operation(Type etype, const vector<enode>& operands, var_binds vars = {}) {
        ENode* op = new ENode;
        op->etype    = etype;
        op->operands = operands;
        op->vars     = vars;
        return op;
    }

    static enode create_value(Type etype, const object& value) {
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

    static object lookup(const vector<map> &stack, ident id, bool top_only, bool &found) {
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

    static str string_interpolate(const object &m_input, const vector<map> &stack) {
        str input = str(m_input);
        str output = input->interpolate([&](const str &arg) -> str {
            ident f = arg;
            bool found = false;
            object m = lookup(stack, f, false, found);
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
                object m = lookup(stack, op->value, false, found);
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

UA_impl(enode, ENode)

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


struct EMember;
struct emember {
    UA_decl(emember, EMember)
};

struct EMember:A {
    enums(Type, Undefined,
        Undefined, Variable, Lambda, Method, Constructor)
    bool            intern; /// intern is not reflectable
    bool            is_static;
    Type            member_type; /// this could perhaps be inferred from the members alone but its good to have an enum
    str             name;
    unique_type     type;  /// for lambda, this is the return result
    str             base_class;
    vector<ident>   value; /// code for methods go here; for lambda, this is the default lambda instance; lambdas of course can be undefined
    vector<emember> args;  /// args for both methods and lambda; methods will be on the class model alone, not in the 'instance' memory of the class
    var_binds       arg_vars; /// these are pushed into the parser vspace stack when translating a method; lambdas will need another set of read vars
    vector<ident>   base_forward;
    bool            is_ctr;
    enode           translation;
    EMember() : A(typeof(EMember)) { }
    String* to_string() {
        return name; /// needs weak reference to class
    }
    operator bool() { return bool(name); }
};


UA_impl(emember, EMember)

struct Parser {
    static inline vector<ident> assign = {":", "+=", "-=", "*=", "/=", "|=", "&=", "^=", ">>=", "<<=", "%="};
    vector<ident> tokens;
    vector<var_binds> bind_stack;
    int cur = 0;

    Parser(vector<ident> tokens) : tokens(tokens) { }

    Parser(str input) {
        str           sp         = "$,<>()![]/+-*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
        char          until      = 0; /// either ) for $(script) ", ', f or i
        sz_t          len        = input->len();
        char*         origin     = input->cs();
        char*         start      = 0;
        char*         cur        = origin - 1;
        int           line_num   = 0;
        bool          new_line   = true;
        bool          token_type = false;
        bool          found_null = false;
        bool          multi_comment = false;
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
                if (cur[1] == '#')
                    multi_comment = !multi_comment;
                while (*cur && *cur != '\n')
                    cur++;
                found_null = !*cur;
                new_line = true;
                until = 0; // requires further processing if not 0
            }
            if (until) {
                if (*cur == until && *(cur - 1) != '/') {
                    add_str = true;
                    until = 0;
                    cur++;
                    rel = cur;
                }
            }
            if (!until && !multi_comment) {
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
        if (start && (cur - start))
            tokens += parse_token(start, (sz_t)(cur - start), line_num);
    }

    static void ws(char *&cur) {
        while (*cur == ' ' || *cur == '\r' || *cur == '\t') ++cur;
    }

    static ident parse_token(char *start, sz_t len, int line_num) {
        while (start[len - 1] == '\t' || start[len - 1] == ' ')
            len--;
        str all { start, len };
        char t = all[0];
        bool is_number = (t == '-' || (t >= '0' && t <= '9'));
        return is_number ?
            Ident(all, line_num) : 
            Ident(all->split(str(".")), line_num); /// mr b: im a token!
    }

    vector<ident> parse_raw_block() {
        if (next() != "[")
            return vector<ident> { pop() };
        assertion(next() == "[", "expected beginning of block [");
        vector<ident> res;
        res += pop();
        int level = 1;
        for (;;) {
            if (next() == "[") {
                level++;
            } else if (next() == "]") {
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
    vector<emember> parse_args(A* object) {
        vector<emember> result;
        assertion(pop() == "[", "expected [ for arguments");
        /// parse all symbols at level 0 (levels increased by [, decreased by ]) until ,

        /// # [ int arg, int arg2[int, string] ]
        /// # args look like this, here we have a lambda as a 2nd arg
        /// 
        while (next() && next() != "]") {
            emember a = parse_member(object);
            ident n = next();
            assertion(n == "]" || n == ",", ", or ] in arguments");
            if (n == "]")
                break;
            pop();
        }
        assertion(pop() == "]", "expected end of args ]");
        return result;
    }

    unique_type parse_type() {
        return unique_type(); // we need to start again, from the tokens alone; we 
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
        if (!token)
            return ENode::Type(ENode::Type::Undefined);
        char t = token[0][0];
        if (isalpha(t) && keywords->index_of(token[0]) == -1) {
            /// lookup against variable table; declare if in isolation
            return ENode::Type::AlphaIdent; /// will be Var, or Type after
        }
        return ENode::Type(ENode::Type::Undefined);
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
        if (multiple) {
            pop(); /// add space for variables (both cases, actually, since a singular expression can create temporaries)
        }
        var_binds vars {};
        bind_stack->push(vars); /// statements alone constitute new variable space
        while (next()) {
            enode n = parse_statement();
            assertion(n, "expected statement or expression");
            block += n;
            if (!multiple)
                break;
            else if (next() == "]")
                break;
        }
        bind_stack->pop();
        if (multiple) {
            assertion(next() == "]", "expected end of block ']'");
            consume();
        }
        return ENode::create_operation(ENode::Type::Statements, block, vars);
    }

    enode parse_expression() {
        return parse_add();
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
            } else if (t1 == "[") {
                /// method call / array lookup
                /// determine if its a method
                bool is_static;
                //type = lookup_type(t0, is_static);
                //vspaces
            }
            return parse_expression();
        } else if (t0 == "return") {
            consume();
            enode result = parse_expression();
            return ENode::create_operation(ENode::Type::MethodReturn, { result });
        } else if (t0 == "break") {
            consume();
            enode levels;
            if (next() == "[") {
                consume();
                levels = parse_expression();
                assertion(pop() == "]", "expected ] after break[expression...");
            }
            return ENode::create_operation(ENode::Type::Break, { levels });
        } else if (t0 == "for") {
            consume();
            assertion(next() == "[", "expected condition expression '['"); consume();
            enode statement = parse_statements();
            assertion(next() == ";", "expected ;"); consume();
            bind_stack->push(statement->vars);
            enode condition = parse_expression();
            assertion(next() == ";", "expected ;"); consume();
            enode post_iteration = parse_expression();
            assertion(next() == "]", "expected ]"); consume();
            enode for_block = parse_statements();
            bind_stack->pop(); /// the vspace is manually pushed above, and thus remains for the parsing of these
            enode for_statement = ENode::create_operation(ENode::Type::For, vector<enode> { statement, condition, post_iteration, for_block });
            return for_statement;
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
            return ENode::create_value(n, object::from_string(cs, is_int ? typeof(i64) : typeof(double)));
        }
        ENode::Type s = is_string(id);
        if (s) {
            ident f = next();
            assert(f->len() == 1);
            cstr cs = f[0]->cs();
            consume();
            struct id* t = typeof(str);
            auto v = object::from_string(cs, t);
            str str_literal = v;
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
    UA_decl(eprop, EProp)
};
UA_impl(eprop, EProp)

struct EnumSymbol:A {
    str             name;
    int             value;
    EnumSymbol() : A(typeof(EnumSymbol)) { }
    EnumSymbol(str name, int value) : A(typeof(EnumSymbol)), name(name), value(value) { }
    operator bool() { return bool(name); }
};

struct enum_symbol {
    UA_decl(enum_symbol, EnumSymbol)
};
UA_impl(enum_symbol, EnumSymbol)

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
                object enum_value = ENode::exec(enum_expr, {});
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
    UA_decl(enum_def, EnumDef)
};
UA_impl(enum_def, EnumDef)

struct EClass;
struct eclass { UA_decl(eclass, EClass) };
struct EClass:A {
    str             name;
    bool            intern;
    str             from; /// inherits from
    vector<emember> members;
    vector<ident>   friends; // these friends have to exist in the module

    EClass(bool intern = false)         : A(typeof(EClass)) { }
    EClass(Parser &parser, bool intern) : EClass(intern) {
        /// parse class members
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
            bool is_static = false;
            if (parser.next() == "static") {
                parser.pop();
                is_static = true;
            }

            ident m0 = parser.next();
            assertion(parser.is_alpha_ident(m0), "expected type identifier");
            bool is_construct = m0 == ident(name);
            unique_type member_type;
            bool is_cast = false;

            if (!is_construct) {
                is_cast = m0 == "cast";
                if (is_cast)
                    parser.pop();
            }

            members += parser.parse_member(this);
            emember last = members->last();
            last->intern = intern;
            last->is_static = is_static;

            /// int [] name [int arg] [ ... ]
            /// int [] array
            /// int [int] map
        }
        ident n = parser.pop();
        assertion(n == "]", "expected end of class");
        /// classes and structs
    }
    operator bool() { return bool(name); }
};
UA_impl(eclass, EClass)


struct EStruct;
struct estruct { UA_decl(estruct, EStruct) };
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
UA_impl(estruct, EStruct)

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

    if (next() == ident(parent_name)) {
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
        result->args = parse_args(obj_type);
        //var_bind args_vars;
        //for (emember& member: result->args) {
        //    args_vars->vtypes += member->member_type;
        //    args_vars->vnames += member->name;
        //}
        if (is_ctr) {
            if (next() == ":") {
                pop();
                ident class_name = pop();
                assertion(class_name == ident(cl->from) || class_name == ident(parent_name), "invalid constructor base call");
                result->base_class = class_name; /// should be assertion checked above
                result->base_forward = parse_raw_block();
            }
            ident n = next();
            assertion(n == "[", "expected [constructor code block], found {0}", { n });
            result->member_type = EMember::Type::Constructor;
            result->value = parse_raw_block();
        } else {
            if (next() == ":" || next() != "[") {
                result->member_type = EMember::Type::Lambda;
                if (next() == ":")
                    pop();
            } else {
                result->member_type = EMember::Type::Method;
            }
            ident n = next();
            if (result->member_type == EMember::Type::Method || n == "[") {
                assertion(n == "[", "expected [method code block], found {0}", { n });
                result->value = parse_raw_block();
            }
        }
    } else if (n == ":") {
        pop();
        result->value = parse_raw_block();
    } else {
        // not assigning variable
    }
    return result;
}

struct EVar;
struct evar { UA_decl(evar, EVar) };
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
UA_impl(evar, EVar)

struct EImport;
struct eimport { UA_decl(eimport, EImport) };
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
        str s_mod = mod;
        name = s_mod->mid(1, s_mod->length - 2);
        assertion(parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
    }
    str         name;
    str         isolate_namespace;
    path        module_path;
    struct EModule* module;
    operator bool() { return bool(name); }
};
UA_impl(eimport, EImport)

struct eincludes { UA_decl(eincludes, EIncludes) };
struct EIncludes:A {
    EIncludes() : A(typeof(EIncludes)) { }
    EIncludes(Parser& parser) : EIncludes() {
        assertion(parser.pop() == "includes", "expected includes");
        assertion(parser.pop() == "<", "expected < after includes");
        for (;;) {
            ident inc = parser.pop();
            assertion(parser.is_alpha_ident(inc), "expected include file, found {0}", { inc });
            path include = str(inc);
            includes += include;
            if (parser.next() == ",") {
                parser.pop();
                continue;
            } else {
                assertion(parser.pop() == ">", "expected > after includes");
                break;
            }
        }
        /// read optional fields for defines, libs
    }
    vector<str>  library; /// optional library identifiers (these will omit the lib prefix & ext, as defined regularly in builds)
    vector<path> includes;
    map          defines;
    operator bool() { return bool(includes); }
};
UA_impl(eincludes, EIncludes)

struct EModule;
struct emodule { UA_decl(emodule, EModule) };
struct EModule:A {
    EModule() : A(typeof(EModule)) { }
    vector<eimport>   imports;
    vector<eincludes> includes;
    map               implementation;
    //vector<eclass>    classes;
    //vector<estruct>   structs;
    //vector<enum_def>  enumerables;
    bool              translated = false;

    bool graph() {
        lambda<void(EModule*)> graph;
        graph = [&](EModule *module) {
            if (module->translated)
                return;
            for (eimport& import: module->imports)
                graph(import->module);

            for (Field& f: module->implementation) {
                if (f.value.type() == typeof(EClass)) {
                    eclass cl = f.value;
                    for (emember& member: cl->members) {
                        auto convert_enode = [&](emember& member) -> enode {
                            vector<ident> code = member->value;
                            Parser parser(code);
                            console.log("parsing {0}", { member->name });
                            parser.bind_stack->push(member->arg_vars);
                            enode n = parser.parse_statements();
                            return n;
                        };
                        if (member->member_type == EMember::Type::Method) {
                            assertion(member->value, "method {0} has no value", { member });
                            member->translation = convert_enode(member);
                        }
                    }
                }
            }
            module->translated = true;
            /// the ENode schema is bound to the classes stored in module
        };
        for (eimport& import: imports) {
            graph(import->module);
        }
        graph(this);
        return true;
    }

    static EModule *parse(path module_path) {
        EModule *m = new EModule();
        str contents = module_path->read<str>();
        Parser parser = Parser(contents);

        int imports = 0;
        int includes = 0;
        for (;;) {
            ident token = parser.next();
            if (!token)
                break;
            bool intern = false;
            if (token == "intern") {
                parser.consume();
                token  = parser.pop();
                intern = true;
            }
            auto push_implementation = [&](ident keyword, ident name, object value) {
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
                str  loc = "{0}.si";
                path si_path = loc->format({ import->name });
                assertion(si_path->exists(), "path does not exist for silver module: {0}", { si_path });
                import->module_path = si_path;
                import->module = parse(si_path); /// needs to be found relative to the location of this module
            } else if (token == "includes") {
                assertion(!intern, "intern keyword not applicable to includes");
                eincludes includes_obj(parser);
                includes++;
                str includes_str = "includes{0}";
                str includes_id  = includes_str->format({ includes });
                push_implementation(token, includes_id, includes_obj);
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
UA_impl(emodule, EModule)
}

using namespace ion;

int main(int argc, char **argv) {
    map  def     { field { "source",  path(".") } };
    map  args    { map::args(argc, argv, def, "source") };
    path source  { args["source"]  };

    emodule m = EModule::parse(source);
    m->graph();
    m->run();
    return 0;
}