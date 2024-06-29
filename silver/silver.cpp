#include <silver/silver.hpp>

/// use mx internally
#include <mx/mx.hpp>

namespace ion {

using string = str;

// i want this naming across, no more UpperCase stuff just call that upper_case_t
// the pointer has the _t; its easier to remember

/// does . this-ever-need-to-be-separate . or

struct Ident:A {
    string value;
    string fname;
    int line_num;

    Ident(null_t = null) : A(typeof(Ident)) { }
    Ident(object m, string file = "none", int line = 0) : Ident() {
        assert (m.type() == typeof(String));
        value = m.to_string();
        fname    = file;
        line_num = line;
    }
    String *to_string() override {
        return (String*)value->hold();
    }
    int compare(const object& m) override {
        bool same;
        if (m.type() == typeof(Ident)) {
            Ident  &b = m;
            same = value == b.value;
        } else {
            String& b = m;
            same = (String&)value == b;
        }
        return same ? 0 : -1;
    }
    u64 hash() override {
        u64 h = OFFSET_BASIS;
            h *= FNV_PRIME;
            h ^= value->hash();
        return h;
    }
    cstr cs() { return value->cs(); }
    
    Vector<string> *split(object obj) {
        return value->split(obj);
    }

    bool operator==(const string &s) const {
        return value == s;
    }
    char &operator[](int i) const { return value[i]; }
    operator bool() const { return bool(value); }
};

struct ident {
    char &operator[](int i) const { return (*a)[i]; }
    UA_decl(ident, Ident)
};
UA_impl(ident, Ident)

using tokens = vector<ident>;

struct silver_t:A {
    vector<ident> type_tokens;
    object def;
    
    silver_t(null_t = null) : A(typeof(silver_t)) { }
    silver_t(tokens type_tokens, object def) : 
        A(typeof(silver_t)), type_tokens(type_tokens), def(def) { }
    operator bool() {
        return type_tokens && def;
    }
};

struct silver {
    UA_decl(silver, silver_t)
};

UA_impl(silver, silver_t)

struct Parser;
void assertion(Parser& parser, bool cond, const string& m, const array& a = {});
void assertion(bool cond, const string& m, const array& a = {});

/// we want the generic, or template keyword to be handled by translation at run-time
/// we do not want the code instantiated a bunch of times; we can perform lookups at runtime for the T'emplate args
/// its probably quite important not to re-instantiate the code a bunch of times.  once only, even with templates
/// you simply host your own object-based types

/// why cant templated keywords simply be object, then?

static vector<string> keywords = { "class", "proto", "struct", "import", "return", "asm", "if", "switch", "while", "for", "do" };

struct EClass;
struct EProto;
struct EStruct;
struct silver;
struct silver_t;

/// twins; just data and user
struct var_bind;
struct Var_Bind:A {
    ident       name;
    silver_t*   utype; /// silver types are created here from our library of modules
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


struct emember;
struct ENode:A {
    enums(Type, Undefined,
        Undefined, 
        Statements, Assign, AssignAdd, AssignSub, AssignMul, AssignDiv, AssignOr, AssignAnd, AssignXor, AssignShiftR, AssignShiftL, AssignMod,
        If, For, While, DoWhile, Break,
        LiteralReal, LiteralInt, LiteralStr, LiteralStrInterp, Array, AlphaIdent, Var, Add, Sub, Mul, Div, Or, And, Xor, MethodCall, MethodReturn)
   
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

    /// @brief invocation of enode-based method
    /// @param method - the module member's eclass's emember
    /// @param args   - array of enodes for solving the arguments
    /// @return 
    static enode method_call(vector<string> method, vector<enode> args) {
        /// each in args is an enode
        /// perhaps a value given, or perhaps a method with operand args
        ENode* op = new ENode;
        op->etype = ENode::Type::MethodCall;
        op->value = method;
        /// method->translation is an enode to the method, 
        /// however we need context of method since that has args to validate against, and perform type conversions from
        /// the arg type are there for that reason
        for (ENode* arg_op: args)
            op->operands += arg_op; // make sure the ref count increases here
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

    static string string_interpolate(const object &m_input, const vector<map> &stack) {
        string input = string(m_input);
        string output = input->interpolate([&](const string &arg) -> string {
            ident f = arg;
            bool found = false;
            object m = lookup(stack, f, false, found);
            if (!found)
                return arg;
            return m.to_string();
        });
        return output;
    }
    
    /// map to 'var-name', value: instance [  ]
    static var exec(const enode &op, const vector<map> &stack) { /// rethink stack, should be emember based
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

using tokens = vector<ident>;
struct silver_t;

struct EMember:A {
    enums(Type, Undefined,
        Undefined, Variable, Lambda, Method, Constructor) // methods without values would be lambdas; thats the clearest standard
    bool            is_template;
    bool            intern; /// intern is not reflectable
    bool            is_static;
    bool            is_public;
    lambda<void()>  resolve;
    Type            member_type;
    type_t          runtime; /// some allocated runtime for method translation (proxy remote of sort); needs to be a generic-lambda type
    string          name;
    vector<emember> args;
    silver          type;
    string          base_class;
    vector<ident>   type_tokens;
    vector<ident>   group_tokens;
    tokens          value; /// code for methods go here; for lambda, this is the default lambda instance; lambdas of course can be undefined
    tokens          base_forward;
    bool            is_ctr;
    enode           translation;
    EMember(null_t = null) : A(typeof(EMember)) { }
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

    Parser(string input, string fname) {
        string        sp         = "$,<>()![]/+*:\"\'#"; /// needs string logic in here to make a token out of the entire "string inner part" without the quotes; those will be tokens neighboring
        char          until      = 0; /// either ) for $(script) ", ', f or i
        sz_t          len        = input->len();
        char*         origin     = input->cs();
        char*         start      = 0;
        char*         cur        = origin - 1;
        int           line_num   = 1;
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
                /// ws does not work with new lines
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
            }// else if (cur[0] == ':' && cur[1] == ':') { /// :: is a single token
            //    rel = ++cur;
            //}
            if (!until && !multi_comment) {
                int type = sp->index_of(*cur);
                new_line |= *cur == '\n';

                if (start && (is_ws || add_str || (token_type != (type >= 0) || token_type) || new_line)) {
                    tokens += parse_token(start, (sz_t)(rel - start), fname, line_num);
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
                        if (start[0] == ':' && start[1] == ':') /// double :: is a single token
                            cur++;
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
            tokens += parse_token(start, (sz_t)(cur - start), fname, line_num);
    }

    string line_info() {
        ident n = next();
        str templ = "{0}:{1}";
        return templ->format({ n->fname, n->line_num });
    }

    static void ws(char *&cur) {
        while (*cur == ' ' || *cur == '\t') {
            ++cur;
        }
    }

    static ident parse_token(char *start, sz_t len, string fname, int line_num) {
        while (start[len - 1] == '\t' || start[len - 1] == ' ')
            len--;
        string all { start, len };
        char t = all[0];
        bool is_number = (t == '-' || (t >= '0' && t <= '9'));
        return ident(all, fname, line_num); /// mr b: im a token!  line-num can be used for breakpoints (need the file path too)
    }

    vector<ident> parse_raw_block() {
        if (next() != "[")
            return vector<ident> { pop() };
        assertion(*this, next() == "[", "expected beginning of block [");
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

    emember parse_member(A* object, emember peer, bool template_mode);

    /// parse members from a block
    vector<emember> parse_args(A* object, bool template_mode) {
        vector<emember> result;
        assertion(*this, pop() == "[", "expected [ for arguments");
        /// parse all symbols at level 0 (levels increased by [, decreased by ]) until ,

        /// # [ int arg, int arg2[int, string] ]
        /// # args look like this, here we have a lambda as a 2nd arg
        /// 
        while (next() && next() != "]") {
            emember a = parse_member(object, null, template_mode); /// we do not allow type-context in args but it may be ok to try in v2
            ident n = next();
            assertion(*this, n == "]" || n == ",", ", or ] in arguments");
            if (n == "]")
                break;
            pop();
        }
        assertion(*this, pop() == "]", "expected end of args ]");
        return result;
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
        string t = token;
        return (t[0] >= '0' && t[0] <= '9') ? (strchr(t->cs(), '.') ? 
            ENode::Type::LiteralReal : ENode::Type::LiteralInt) : ENode::Type::Undefined;
    };

    ENode::Type is_string(ident& token) {
        char t = token[0];
        return t == '"' ? ENode::Type::LiteralStr : t == '\'' ? ENode::Type::LiteralStrInterp : ENode::Type::Undefined;
    };

    Index expect(ident& token, const vector<ident> &tokens) {
        Index i = tokens->index_of(token);
        return i;
    };

    ENode::Type is_alpha_ident(ident token) { /// type, var, or method (all the same); its a name that isnt a keyword
        if (!token)
            return ENode::Type(ENode::Type::Undefined);
        char t = token[0];
        if (isalpha(t) && keywords->index_of(token) == -1) {
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
            assertion(*this, n, "expected statement or expression");
            block += n;
            if (!multiple)
                break;
            else if (next() == "]")
                break;
        }
        bind_stack->pop();
        if (multiple) {
            assertion(*this, next() == "]", "expected end of block ']'");
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
                assertion(*this, pop() == "]", "expected ] after break[expression...");
            }
            return ENode::create_operation(ENode::Type::Break, { levels });
        } else if (t0 == "for") {
            consume();
            assertion(*this, next() == "[", "expected condition expression '['"); consume();
            enode statement = parse_statements();
            assertion(*this, next() == ";", "expected ;"); consume();
            bind_stack->push(statement->vars);
            enode condition = parse_expression();
            assertion(*this, next() == ";", "expected ;"); consume();
            enode post_iteration = parse_expression();
            assertion(*this, next() == "]", "expected ]"); consume();
            enode for_block = parse_statements();
            bind_stack->pop(); /// the vspace is manually pushed above, and thus remains for the parsing of these
            enode for_statement = ENode::create_operation(ENode::Type::For, vector<enode> { statement, condition, post_iteration, for_block });
            return for_statement;
        } else if (t0 == "while") {
            consume();
            assertion(*this, next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(*this, next() == "]", "expected condition expression ']'"); consume();
            enode statements = parse_statements();
            return ENode::create_operation(ENode::Type::While, { condition, statements });
        } else if (t0 == "if") {
            consume();
            assertion(*this, next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(*this, next() == "]", "expected condition expression ']'"); consume();
            enode statements = parse_statements();
            enode else_statements;
            bool else_if = false;
            if (next() == "else") { /// if there is no 'if' following this, then there may be no other else's following
                consume();
                else_if = next() == "if";
                else_statements = parse_statements();
                assertion(*this, !else_if && next() == "else", "else proceeding else");
            }
            return ENode::create_operation(ENode::Type::If, { condition, statements, else_statements });
        } else if (t0 == "do") {
            consume();
            enode statements = parse_statements();
            assertion(*this, next() == "while", "expected while");                consume();
            assertion(*this, next() == "[", "expected condition expression '['"); consume();
            enode condition  = parse_expression();
            assertion(*this, next() == "]", "expected condition expression '['");
            consume();
            return ENode::create_operation(ENode::Type::DoWhile, { condition, statements });
        } else {
            return parse_expression();
        }
    }

    i64 parse_numeric(ident &token) {
        char *e;
        i64   num = strtoll(token->cs(), &e, 10);
        return num;
    }

    ENode::Type is_var(ident &token) { /// type, var, or method (all the same); its a name that isnt a keyword
        char t = token[0];
        if (isalpha(t) && keywords->index_of(token) == -1) {
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

            cstr cs = f->cs();

            bool is_int = n == ENode::Type::LiteralInt;
            consume();
            return ENode::create_value(n, object::from_string(cs, is_int ? typeof(i64) : typeof(double)));
        }
        ENode::Type s = is_string(id);
        if (s) {
            ident f = next();
            cstr cs = f->cs();
            consume();
            struct id* t = typeof(string);
            auto v = object::from_string(cs, t);
            string str_literal = v;
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

        if (next() == "[") {
            // now we must resolve id to a member in class
            // the id can be complex;
            // it could be my-member.another.method-call-on-instance [ another-member.another-method[] ]
            //             id -------------------------------------- args ------------------------------
            //             
            // the stack can have an emember and a current value; its reference can be a place in an enode
            // we dont exactly return yet (it was implemented before, in another module)
            vector<string> emember_path = id->split(".");
            
            assert(i == ENode::Type::MethodCall);
            consume();
            vector<enode> enode_args;
            for (;;) {
                enode op = parse_expression(); // do not read the , [verify this]
                enode_args += op;
                if (next() == ",")
                    pop();
                else
                    break;
            }
            assertion(*this, next() == "]", "expected ] after method invocation");
            consume();

            enode method_call = ENode::method_call(emember_path, enode_args);
            return method_call;
        } else {
            assert(i != ENode::Type::MethodCall);
        }
        return {};
    }

    bool expect(const ident &token) {
        if (token != tokens[cur])
            console.fault("expected token: {0}", {token});
        return true;
    }

};

void assertion(Parser& parser, bool cond, const string& m, const array& a) {
    if (!cond) {
        console.log(parser.line_info());
        console.fault(m, a);
    }
}

void assertion(bool cond, const string& m, const array& a) {
    if (!cond) {
        console.fault(m, a);
    }
}


struct EProp:A {
    string          name;
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
    string          name;
    int             value;
    EnumSymbol() : A(typeof(EnumSymbol)) { }
    EnumSymbol(string name, int value) : A(typeof(EnumSymbol)), name(name), value(value) { }
    operator bool() { return bool(name); }
};

struct enum_symbol {
    UA_decl(enum_symbol, EnumSymbol)
};
UA_impl(enum_symbol, EnumSymbol)


struct EModuleMember:A {
    string      name;
    bool        intern;
    struct EModule* module;
    EModuleMember(type_t type) : A(type) { }
};

struct EnumDef:EModuleMember {
    string              name;
    bool                intern;
    vector<enum_symbol> symbols;

    EnumDef(bool intern = false) : EModuleMember(typeof(EnumDef)), intern(intern) { }

    EnumDef(Parser &parser, bool intern) : EnumDef(intern) {
        ident token_name = parser.pop();
        assertion(parser, parser.is_alpha_ident(token_name), "expected qualified name for enum, found {0}", { token_name });
        name = token_name;
        assertion(parser, parser.pop() == "[", "expected [ in enum statement");
        i64  prev_value = 0;
        for (;;) {
            ident symbol = parser.pop();
            ENode* exp = parser.parse_expression(); /// this will pop tokens until a valid expression is made
            if (symbol == "]")
                break;
            assertion(parser, parser.is_alpha_ident(symbol),
                "expected identifier in enum, found {0}", { symbol });
            ident peek = parser.next();
            if (peek == ":") {
                parser.pop();
                enode enum_expr = parser.parse_expression();
                object enum_value = ENode::exec(enum_expr, {});
                assertion(parser, enum_value.type()->traits & traits::integral,
                    "expected integer value for enum symbol {0}, found {1}", { symbol, enum_value });
                prev_value = i64(enum_value);
                assertion(parser, prev_value >= INT32_MIN && prev_value <= INT32_MAX,
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

struct emodule;
struct EModule;
struct EClass;

enums(EMembership, normal, normal, internal)

/// this is the basis for the modeling of data
/// mods use alloated reference by default, but can embed inside primitive data by specifying a primitive type
/// this lets us sub-class our basic mod types for numerics and boolean
enums(EModel, allocated,
    allocated,
    boolean_32,
    unsigned_8, unsigned_16, unsigned_32, unsigned_64,
    signed_8, signed_16, signed_32, signed_64,
    real_32, real_64, real_128)

struct eclass { UA_decl(eclass, EClass) };
struct EClass:EModuleMember {
    vector<emember> template_args; /// if nothing in here, no template args [ template ... ctr ] -- lets parse type variables
    EModel          model; /// model types are primitive types of bit length
    string          from; /// inherits from
    vector<emember> members;
    vector<ident>   friends; // these friends have to exist in the module
    bool            is_translated;
    
    EClass(bool intern = false) : EModuleMember(typeof(EClass)) { }
    EClass(Parser &parser, EMembership membership, vector<emember> &templ_args, string keyword = "class") : EClass(intern) { // replace intern with enum for normal, intern -- its a membership type
        /// parse class members
        assertion(parser, parser.pop() == ident(keyword), "expected {0}", { keyword });
        assertion(parser, parser.is_alpha_ident(parser.next()), "expected class identifier");
        name = parser.pop();
        template_args = templ_args;

        if (parser.next() == "::") {
            parser.consume();
            model = string(parser.pop());
            /// boolean-32 is a model-type, integer-i32, integer-u32, object is another (implicit); 
            /// one can inherit over a model-bound mod; this is essentially the allocation size for 
            /// its membership identity; for classes that is pointer-based, but with boolean, integers, etc we use the value alone
            /// mods have a 'model' for storage; the models change what forms its identity, by value or reference
            /// [u8 1] [u8 2]   would be values [ 1, 2 ] in a u8 array; these u8's have callable methods on them
            /// so we can objectify anything if we have facilities around how the object is referenced, inline or by allocation
            /// this means there is no reference counts on these value models
                
        } else if (parser.next() == ":") {
            parser.consume();
            from = parser.pop();
        }
        if (parser.next() == "[") {
            assertion(parser, parser.pop() == "[", "expected beginning of class");
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
                bool is_public = false;
                if (parser.next() == "public") {
                    parser.pop();
                    is_public = true;
                }
                bool is_static = false;
                if (parser.next() == "static") {
                    parser.pop();
                    is_static = true;
                }

                ident m0 = parser.next();
                assertion(parser, parser.is_alpha_ident(m0), "expected type identifier");
                bool is_construct = m0 == ident(name);
                bool is_cast = false;

                if (!is_construct) {
                    is_cast = m0 == "cast";
                    if (is_cast)
                        parser.pop();
                }

                auto set_attribs = [&](emember last) {
                    last->intern = intern;
                    last->is_public = is_public;
                    last->is_static = is_static;
                };
                members += parser.parse_member(this, null, false);
                emember mlast = members->last();
                set_attribs(mlast);

                //assert(mlast->type); there is no resolved type here, we are parsing class-defs still
                // we set these types on resolve

                for (;;) {
                    if (parser.next() != ",")
                        break;
                    parser.pop();
                    members += parser.parse_member(this, mlast, false);
                    /// it may still have a type first; for multiple returns this is required; one could certainly call a class method inside the definition of a class, as an initializer; we would want the members to support this
                    /// we also want members to be the same inside of methods; this is the 'stack' variable effectively
                    set_attribs(members->last());
                }

                /// the lambda type still has the [], so we have parsing issues for types that clash with method-ident[args]
                /// int [] name [int arg] [ ... ]
                /// int [] array
                /// int [int] map
            }
            ident n = parser.pop();
            assertion(parser, n == "]", "expected end of class");
        }
        /// classes and structs
    }
    operator bool() { return bool(name); }

    /// this performs the actual translation to enode (this happens when forging/resolving types)
    eclass translate(vector<silver> args);
};
UA_impl(eclass, EClass)

// we need a way to instance eclass types

/// the map must be populated with the initialized data
/// we can use our own map type
struct instance {
    silver type;
    /// allocation in here
    /// it could be map
    /// more and more the runtime points to an A-type
    map memory;
    /// well, we must use our types in C++
    /// its a good venture to reduce to C99; 
    /// the types are simply A-type, and we port that to C99;
    /// A-type can transition to a faster runtime,
    /// just not a shared compilation + script of the same object type.
    /// i think its quicker for the programmer to be
    /// script or compilation, but not both.  that is, we develop and test with
    /// scripting brush, and we compile to run-benchmarks with compilation brush
    /// i asked ChatGPT if they knew of anything that let one do this and there was
    /// nothing mentioned to me.. silver should be used in script or compilation, even for 1.0 thats a feature we must have.
    /// C++ is fine enough to designed runtime environment for 1.0
    /// can goto python
    /// if we are prototyping just to port we may as well start in script.  thats probably easier to port to C99,
    /// C99 can work now. i keep pushing myself to do this and i cannot design without it working in C99
    /// 
};

/// with C we have a type registry



struct eproto { UA_decl(eproto, EProto) };
struct EProto:EClass {
    EProto(bool intern = false) {
        type = typeof(EProto);
    }
    EProto(Parser &parser, EMembership membership, vector<emember> &templ_args) : EClass(parser, membership, templ_args, "proto") {
        type = typeof(EProto);
    }
};
UA_impl(eproto, EProto)

struct EStruct;

struct EStruct:EModuleMember {
    map         members;

    EStruct(bool intern = false) : EModuleMember(typeof(EStruct)) {
        EModuleMember::intern = intern;
    }
    EStruct(Parser &parser, bool intern) : EStruct(intern) {
        /// parse struct members, which includes eprops
        assertion(parser, parser.pop() == "struct", "expected struct");
        name = parser.pop();
    }
    operator bool() { return bool(name); }
};

emember Parser::parse_member(A* obj_type, emember peer, bool template_mode) {
    EStruct* st = null;
    EClass*  cl = null;
    string parent_name;

    if (obj_type) {
        if (obj_type->type == typeof(EStruct))
            parent_name = ((EStruct*)obj_type)->name;
        else if (obj_type->type == typeof(EClass))
            parent_name = ((EClass*)obj_type)->name;
    }
    emember result;
    bool is_ctr = false;

    ident ntop = next();
    if (!peer && ntop == ident(parent_name)) {
        assertion(*this, obj_type->type == typeof(EClass), "expected class when defining constructor");
        result->name = pop();
        is_ctr = true;
    } else {
        if (peer) {
            result->type_tokens = peer->type_tokens;
        } else {

            auto read_type_tokens = [&]() -> vector<ident> {
                vector<ident> res;
                if (next() == "ref")
                    res += pop();
                for (;;) {
                    assertion(*this, is_alpha_ident(next()), "expected type identifier");
                    res += pop();
                    if (token_at(0) == "::") {
                        res += pop();
                        continue;
                    }
                    break;
                }
                return res;
            };

            /// read type -- they only consist of-symbols::another::and::another
            /// i dont see the real point of embedding types
            result->type_tokens = read_type_tokens();
            if (result->type_tokens[0] == "ref") {
                int test = 0;
                test++;
            }
            /// this is an automatic instance type method; so replace type_tokens with its own type
            if (!template_mode && next() == "[") {
                assertion(*this, result->type_tokens->len() == 1, "name identifier expected for automatic return type method");
                result->name = result->type_tokens[0];
                result->type_tokens[0] = ident(parent_name);
            }

            if (template_mode) {
                /// templates do not always define a variable name (its used for replacement)
                if (next() == ":") { /// its a useful feature to not allow the :: for backward namespace; we dont need it because we reduced our ability to code to module plane
                    pop();
                    result->group_tokens = read_type_tokens();
                } else if (is_alpha_ident(next())) {
                    /// this name is a replacement variable; we wont use them until we have expression blocks (tapestry)
                    result->name = pop();
                    if (next() == ":") {
                        pop();
                        result->group_tokens = read_type_tokens();
                    }
                }
            }
        }

        if (!template_mode && !result->name) {
            assertion(*this, is_alpha_ident(next()),
                "{1}:{2}: expected identifier for member, found {0}", 
                { next(), next()->fname, next()->line_num });
            result->name = pop();
        }
    }
    ident n = next();
    assertion(*this, (n == "[" && is_ctr) || !is_ctr, "invalid syntax for constructor; expected [args]");
    if (n == "[") {
        // [args] this is a lambda or method
        result->args = parse_args(obj_type, false);
        //var_bind args_vars;
        //for (emember& member: result->args) {
        //    args_vars->vtypes += member->member_type;
        //    args_vars->vnames += member->name;
        //}
        int line_num_def = n->line_num;
        if (is_ctr) {
            if (next() == ":") {
                pop();
                ident class_name = pop();
                assertion(*this, class_name == ident(cl->from) || class_name == ident(parent_name), "invalid constructor base call");
                result->base_class = class_name; /// should be assertion checked above
                result->base_forward = parse_raw_block();
            }
            result->member_type = EMember::Type::Constructor;
            ident n = next();
            if (n == "[") {
                assertion(*this, n == "[", "expected [constructor code block], found {0}", { n });
                result->value = parse_raw_block();
            } else {
                result->value += ident("[");
                assert(obj_type);
                assert((obj_type->type == typeof(EClass)));
                EClass *cl = ((EClass*)obj_type);

                for (emember& arg: result->args) {
                    // member must exist
                    bool found = false;
                    for (emember& m: cl->members) {
                        if (m->name == arg->name) {
                            found = true;
                            break;
                        }
                    }
                    assertion(found, "arg cannot be found in membership");
                    result->value += ident(arg->name);
                    result->value += ident(":");
                    result->value += ident(arg->name);
                }
                result->value += ident("]");
            }
            /// the automatic constructor we'll for-each for the args
        } else {
            ident next_token = next();
            if (next_token != "return" && (next_token == ":" || next_token != "[")) {
                result->member_type = EMember::Type::Lambda;
                if (next_token == ":")
                    pop(); /// lambda is being assigned, we need to set a state var
            } else {
                result->member_type = EMember::Type::Method;
            }
            ident n = next();
            if (result->member_type == EMember::Type::Method || n == "[") {
                // needs to be able to handle trivial methods if we want this supported
                if (n == "return") {
                    assertion(*this, n->line_num == line_num_def, "single line return must be on the same line as the method definition");
                    for (;;) {
                        if (n->line_num == next()->line_num) {
                            result->value += pop();
                        } else 
                            break;
                    }
                } else {
                    assertion(*this, n == "[", "expected [method code block], found {0}", { n });
                    result->value = parse_raw_block();
                }
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
struct EVar:EModuleMember {
    EVar(bool intern = false) : EModuleMember(typeof(EVar)), intern(intern) { }
    EVar(Parser &parser, bool intern) : EVar(intern) {
        type  = parser.pop();
        assertion(parser, parser.is_alpha_ident(type), "expected type identifier, found {0}", { type });
        name = parser.pop();
        assertion(parser, parser.is_alpha_ident(name), "expected variable identifier, found {0}", { name });
        if (parser.next() == ":") {
            parser.consume();
            initializer = parser.parse_statements(); /// this needs to work on initializers too
            /// we need to think about the data conversion from type to type; in C++ there is operator and constructors battling
            /// i would like to not have that ambiguity, although thats a very easy ask and tall order
            /// 
        }
    }
    string      name;
    ident       type;
    enode       initializer;
    bool        intern;
    operator bool() { return bool(name); }
};
UA_impl(evar, EVar)

struct EImport;
struct eimport { UA_decl(eimport, EImport) };
struct EImport:EModuleMember {
    EImport() : EModuleMember(typeof(EImport)) { }
    EImport(Parser& parser) : EImport() {
        assertion(parser, parser.pop() == "import", "expected import");
        if (parser.next() == "[") {
            parser.pop();
            for (;;) {
                if (parser.next() == "]") {
                    parser.pop();
                    break;
                }
                ident arg_name = parser.pop();
                assertion(parser, parser.is_alpha_ident(arg_name), "expected identifier for import arg");
                assertion(parser, parser.pop() == ":", "expected : after import arg (argument assignment)");
                if (arg_name == "name") {
                    ident token_name = parser.pop();
                    assertion(parser, !parser.is_string(token_name), "expected token for import name");
                    name = string(token_name);
                } else if (arg_name == "links") {
                    assertion(parser, parser.pop() == "[", "expected array for library links");
                    for (;;) {
                        ident link = parser.pop();
                        if (link == "]") break;
                        assertion(parser, parser.is_string(link), "expected library link string");
                        links += string(link);
                        if (parser.next() == ",") {
                            parser.pop();
                            continue;
                        } else {
                            assertion(parser, parser.pop() == "]", "expected ] in includes");
                            break;
                        }
                    }
                } else if (arg_name == "includes") {
                    assertion(parser, parser.pop() == "[", "expected array for includes");
                    for (;;) {
                        ident include = parser.pop();
                        if (include == "]") break;
                        assertion(parser, parser.is_string(include), "expected include string");
                        includes += path(string(include));
                        if (parser.next() == ",") {
                            parser.pop();
                            continue;
                        } else {
                            assertion(parser, parser.pop() == "]", "expected ] in includes");
                            break;
                        }
                    }
                } else if (arg_name == "source") {
                    ident token_source = parser.pop();
                    assertion(parser, parser.is_string(token_source), "expected quoted url for import source");
                    source = string(token_source);
                } else if (arg_name == "shell") {
                    ident token_shell = parser.pop();
                    assertion(parser, parser.is_string(token_shell), "expected shell invocation for building");
                    shell = string(token_shell);
                } else if (arg_name == "defines") {
                    // none is a decent name for null.
                    assertion(parser, false, "not implemented");
                } else {
                    assertion(parser, false, "unknown arg: {0}", { arg_name });
                }

                if (parser.next() == ",")
                    parser.pop();
                else {
                    assertion(parser, parser.pop() == "]", "expected ] after end of args, with comma inbetween");
                    break;
                }
            }
            /// named arguments will be part of var data instantiation when we actually do that
            /// then, import can be defined as a class

        } else {
            ident module_name = parser.pop();
            ident as = parser.next();
            if (as == "as") {
                parser.consume();
                isolate_namespace = parser.pop(); /// inlay overrides this -- it would have to; modules decide if their types are that important
            }
            //assertion(parser.is_string(mod), "expected type identifier, found {0}", { type });
            assertion(parser, parser.is_alpha_ident(module_name), "expected variable identifier, found {0}", { module_name });
            name = module_name;
        }

    }

    string      name;
    string      source;
    string      shell;
    vector<string> links;
    vector<path>   includes;
    var            defines;
    string      isolate_namespace;
    path        module_path;

    operator bool() { return bool(name); }
};
UA_impl(eimport, EImport)

vector<ident> parse_tokens(string s) {
    Parser parser(s, null);
    return parser.tokens;
}

struct EModule;
struct emodule { UA_decl(emodule, EModule) };
struct EModule:A {
    EModule() : A(typeof(EModule)) { }
    string            module_name;
    vector<eimport>   imports;
    eclass            app;
    map               implementation;
    bool              translated = false;
    hashmap           cache; /// silver_t cache

    /// call this within the enode translation; all referenced types must be forged
    /// resolving type involves designing, so we may choose to forge a type that wont forge at all.  thats just a null
    static silver forge_type(EModule* module_instance, vector<ident> type_tokens) {
        vector<string> sp(type_tokens->len());
        if (type_tokens->len() == 1) {
            sp += string(type_tokens[0]); /// single class reference (no templating)
        } else {
            assert(type_tokens->len() & 1); /// cannot be an even number of tokens
            for (int i = 0; i < type_tokens->len() - 1; i += 2) {
                ident &i0 = type_tokens[i + 0];
                ident &i1 = type_tokens[i + 1];
                assert(isalpha(i0[0]));
                assert(i1 == "::");
                sp += i0;
            }
        }
        int  cursor = 0;
        int  remain = sp->len();
        auto pull_sp = [&]() mutable -> string {
            if (cursor >= sp->len()) return string();
            remain--;
            return sp[cursor++];
        };

        lambda<silver(string&)> resolve;
        resolve = [&](string& key_name) -> silver {
            /// pull the type requested, and the template args for it at depth
            string class_name = pull_sp(); /// resolve from template args, needs 
            eclass class_def  = module_instance->find_class(class_name);
            assertion(bool(class_def), "class not found: {0}", { class_name });
            int class_t_args = class_def->template_args->len();
            assertion(class_t_args <= remain, "template args mismatch");
            key_name += class_name;
            vector<silver> template_args;
            for (int i = 0; i < class_t_args; i++) {
                string k;
                silver type_from_arg = resolve(k);
                assert(type_from_arg); /// grabbing context would be nice if the security warranted it
                assert(k);
                template_args += type_from_arg;
                key_name += "::";
                key_name += k;
            }
            if (class_def->module) {
                if (class_def->module->cache->contains(key_name))
                    return class_def->module->cache[key_name];
            }
            /// translation means applying template, then creating enode operations for the methods and initializers
            silver res = silver(parse_tokens(key_name), class_def->translate(template_args));
            class_def->module->cache[key_name] = res;
            return res;
        };

        string key;
        silver res = resolve(key);
        assert(key);
        module_instance->cache[key] = res;
        assert(module_instance->cache->contains(key));
        assert(module_instance->cache[key] == res);
        return res;
    }

    emodule find_module(string name) {
        for (eimport &i: imports)
            if (i->name == name)
                return i->module;
        return { };
    }

    object find_implement(ident name) {
        EModule* m = this;
        vector<string> sp = name->split(".");
        int n_len = sp->len();
        if (n_len == 2) {
            string module_ref  = string(sp[0]);
            string content_ref = string(sp[1]); 
            for (eimport i: imports) {
                if (i->isolate_namespace == module_ref && i->module)
                    return i->module->implementation->contains(content_ref) ?
                        i->module->implementation->value(content_ref) : object();
            }
        } else {
            assert(n_len == 1);
            string content_ref = string(sp[0]); 
            for (eimport i: imports) {
                if (i->module)
                    return i->module->implementation->contains(content_ref) ?
                        i->module->implementation->value(content_ref) : object();
            }
            if (implementation->contains(content_ref)) {
                return implementation->value(content_ref);
            }
        }
        return object();
    }

    eclass find_class(ident name) {
        // for referencing classes and types:
        // its not possible to have anymore than module.class
        // thats by design and it reduces the amount that we acn be overwhelmed with
        // arbitrary trees are not good on the human brain
        object impl = find_implement(name);
        if (impl.type() == typeof(EClass))
            return eclass((const object &)impl);
        return eclass();
    }

    bool run() {
        /// app / run method must translate
        /// static object method(T& obj, const struct string &name, const vector<object> &args);
        /// parser must have ability to run any method we describe too.. here this is a user invocation
        if (app) {
            /// we want silver types to be the most basic
            /// any type_t will be a portability snag keeping us in C++
        } else
            printf("no app with run method defined\n");
        return true;
    }
    
    bool graph() {
        lambda<void(EModule*)> graph;
        graph = [&](EModule *module) {
            if (module->translated)
                return;
            module->translated = true;
            for (eimport& import: module->imports)
                if (import->module)
                    graph(import->module);

            for (Field& f: module->implementation) {
                if (f.value.type() == typeof(EClass)) {
                    eclass cl = f.value;
                    if (!cl->template_args)
                        cl->translate({});
                }
            }
            
            /// the ENode schema is bound to the classes stored in module
        };
        for (eimport& import: imports) {
            if (import->module)
                graph(import->module);
        }
        graph(this);
        return true;
    }

    static emodule parse(path module_path) {
        EModule *m = new EModule();
        m->module_name = string(module_path);
        string contents = module_path->read<string>();
        Parser parser = Parser(contents, module_path);
        
        int imports = 0;
        int includes = 0;
        bool inlay = false;
        vector<emember> templ_args;
        ///
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
            auto push_implementation = [&](ident keyword, ident name, object value) mutable {
                assertion(parser, !m->implementation->fetch(name),  "duplicate identifier for {0}: {1}", { keyword, name });
                m->implementation[name] = value;
                EModuleMember *mm = (EModuleMember*)value;
                mm->module = (EModule*)m->hold();
                if (name == "app")
                    m->app = (EClass*)value;
            };

            if (token != "class" && templ_args) {
                assertion(parser, false, "expected class after template definition");
            }

            if (token == "import") {
                assertion(parser, !intern, "intern keyword not applicable to import");
                eimport import(parser);
                imports++;
                string import_str = "import{0}";
                string import_id = import_str->format({ imports });
                push_implementation(token, import_id, import);
                m->imports += import;

                /// load silver module if name is specified without source
                if (import->name && !import->source) {
                    string  loc = "{1}{0}.si";
                    vector<string> attempt = {"", "spec/"};
                    bool exists = false;
                    for (string pre: attempt) {
                        path si_path = loc->format({ import->name, pre });
                        //console.log("si_path = {0}", { si_path });
                        if (!si_path->exists())
                            continue;
                        import->module_path = si_path;
                        console.log("module {0}", { si_path });
                        import->module = parse(si_path);
                        exists = true;
                        break;
                    }
                    assertion(parser, exists, "path does not exist for silver module: {0}", { import->name });
                }
            } else if (token == "enum") {
                EnumDef* edef = new EnumDef(parser, intern);
                push_implementation(token, edef->name, edef);
                edef->drop();
            } else if (token == "class") {
                EClass* cl = new EClass(parser, intern, templ_args);
                push_implementation(token, cl->name, cl);
                cl->drop();
            } else if (token == "proto") {
                EProto* cl = new EProto(parser, intern, templ_args);
                push_implementation(token, cl->name, cl);
                cl->drop();
            } else if (token == "struct") {
                EStruct* st = new EStruct(parser, intern);
                push_implementation(token, st->name, st);
                st->drop();
            } else if (token == "template") {
                parser.pop();
                /// state var we would expect to be null for any next token except for class
                templ_args = parser.parse_args(null, true); /// a null indicates a template; this would change if we allow for isolated methods in module (i dont quite want this; we would be adding more than 1 entry)
                
            } else {
                EVar* data = new EVar(parser, intern);
                push_implementation(token, data->name, data);
                data->drop();
            }

        }
        return m;
    }
};
UA_impl(emodule, EModule)

eclass EClass::translate(vector<silver> args) {
    if (is_translated)
        return this;
    assert(args->len() == template_args->len());
    int     tlen = args->len();
    eclass  res;

    if (tlen == 0) {
        res = *this;
    } else {
        res->name = name;
        res->intern = intern;
        res->model  = model;
    
        auto get_template_index = [&](ident& token) -> int {
            int index = 0;
            for (emember &m: template_args) {
                if (m->type_tokens && m->type_tokens[0] == token) {
                    assert(m->type_tokens->len() == 1);
                    return index;
                }
                index++;
            }
            return -1;
        };

        auto translate_tokens = [&](vector<ident>& tokens, ident& token) {
            int index_template_arg = get_template_index(token);
            if (index_template_arg >= 0) {
                /// replace all tokens; working at token level is not thee most optimum but its simpler
                silver type = args[index_template_arg];
                assert(type);
                for (ident& token: type->type_tokens)
                    tokens += token;
            } else {
                tokens += token;
            }
        };

        auto translate_member = [&](emember &m_trans, emember &m) {
            m_trans->member_type = m->member_type;
            m_trans->name        = m->name;
            m_trans->intern      = m->intern;
            m_trans->is_public   = m->is_public;
            m_trans->is_ctr      = m->is_ctr;

            for (ident& token: m->type_tokens)
                translate_tokens(m_trans->type_tokens, token);
            
            for (ident& token: m->value) 
                translate_tokens(m_trans->value, token);

            /// it needs the arguments as well
            m_trans->group_tokens = m->group_tokens;
            m_trans->base_forward = m->base_forward;
        };
        
        for (emember& m: members) {
            emember m_trans;

            translate_member(m_trans, m);
            if (m_trans->member_type == EMember::Type::Method) {
                for (emember &arg: m->args) {
                    emember arg_resolve;
                    translate_member(arg_resolve, arg);
                    // translate type_string

                    arg_resolve->type = EModule::forge_type(module, arg_resolve->type_tokens);
                    m_trans->args += arg_resolve;
                }
                if (!m_trans->value)
                    console.fault("method {0} has no value", { m_trans });
                
                vector<ident> code = m_trans->value;
                Parser parser(code);
                console.log("translating method {0}", { m_trans->name });
                //parser.bind_stack->push(member->arg_vars);
                m_trans->translation = parser.parse_statements(); /// the parser must know if the tokens are translatable
            }
            res->members += m_trans;
        }
    }

    res->is_translated = true;
    return res;
}



/*
eclass EClass::composed() {
    if (composed_ident && !composed_last) {
        composed_last = module->find_class(composed_ident);
        assert(composed_last);
    }
    return composed_last;
}*/
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