#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

using namespace std;

// Forward declarations
class ENode;
class EIdent;
class EMember;
class EModule;
class EClass;
class EMethod;

// Enums
enum class BuildState {
    None,
    Built
};

// Structs
struct Token {
    string value;
    int line_num;

    Token(string v, int ln = 0) : value(move(v)), line_num(ln) {}

    bool operator==(const Token& other) const {
        return value == other.value;
    }

    bool operator==(const string& str) const {
        return value == str;
    }
};

// Helper functions
bool is_alpha(const string& s) {
    return !s.empty() && (isalpha(s[0]) || s[0] == '_');
}

bool is_numeric(const string& s) {
    return !s.empty() && (isdigit(s[0]) || s[0] == '-');
}

int index_of(const vector<string>& a, const string& e) {
    auto it = find(a.begin(), a.end(), e);
    return it != a.end() ? distance(a.begin(), it) : -1;
}

string rem_spaces(const string& text) {
    string result = text;
    result.erase(remove(result.begin(), result.end(), ' '), result.end());
    return result;
}

// Base class for all nodes
class ENode {
protected:
    static int _next_id;

public:
    int id;

    ENode() : id(_next_id++) {}
    virtual ~ENode() = default;

    virtual string to_string() const {
        return "ENode(" + to_string(id) + ")";
    }

    bool operator==(const ENode& other) const {
        return id == other.id;
    }
};

int ENode::_next_id = 0;

// EIdent class
class EIdent : public ENode {
public:
    unordered_map<string, bool> decorators;
    vector<Token> list;
    string ident;
    string initial;
    EModule* module;
    bool is_fp;
    void* kind;
    EMember* base;
    unordered_map<string, EIdent*> meta_types;
    unordered_map<string, EMember*> args;
    vector<EMember*> members;
    bool ref_keyword;
    EMember* conforms;
    EMember* meta_member;

    EIdent(EModule* mod) : module(mod), is_fp(false), kind(nullptr), base(nullptr),
                           ref_keyword(false), conforms(nullptr), meta_member(nullptr) {}

    // Other methods will be implemented later
};

// EMember class
class EMember : public ENode {
public:
    string name;
    EIdent* type;
    EModule* module;
    ENode* value;
    EClass* parent;
    string access;
    bool imported;
    bool emitted;
    unordered_map<string, EMember*> members;
    unordered_map<string, EMember*> args;
    unordered_map<string, EIdent*> meta_types;
    unordered_map<string, EClass*> meta_model;
    bool static_member;
    string visibility;

    EMember(string n, EIdent* t, EModule* mod) : name(move(n)), type(t), module(mod), value(nullptr),
        parent(nullptr), imported(false), emitted(false), static_member(false) {}

    virtual string emit(const class EContext& ctx) const {
        // Implementation will be added later
        return "";
    }
};

class EClass : public EMember {
public:
    void* model;
    EClass* inherits;
    std::vector<Token> block_tokens;

    EClass(EModule* module, const std::string& name, const std::string& visibility, 
           const std::unordered_map<std::string, std::string>& meta_model)
        : EMember(name, nullptr, module), inherits(nullptr) {
        this->visibility = visibility;
        this->meta_model = meta_model;
    }

    std::string to_string() const override {
        return "EClass(" + name + ")";
    }

    void print() const {
        for (const auto& [name, members] : this->members) {
            for (const auto& member : members) {
                if (auto method = dynamic_cast<EMethod*>(member)) {
                    std::cout << "method: " << name << std::endl;
                    print_enodes(method->code, 0);
                }
            }
        }
    }

    void emit_header(std::ofstream& f) {
        if (model->name != "allocated") return;
        
        f << "\n/* class-declaration: " << name << " */\n";
        f << "#define " << name << "_meta(X,Y,Z) \\\n";
        
        for (const auto& [member_name, members] : this->members) {
            for (const auto& member : members) {
                if (auto prop = dynamic_cast<EProp*>(member)) {
                    auto base_type = prop->type->get_base();
                    f << "\ti_" << prop->visibility << "(X,Y,Z, " << base_type->name << ", " << prop->name << ")\\\n";
                }
            }
        }

        for (const auto& [name, members] : this->members) {
            for (const auto& member : members) {
                if (member->visibility != "intern" && dynamic_cast<EMethod*>(member)) {
                    auto method = static_cast<EMethod*>(member);
                    char m = method->static_member ? 's' : 'i';
                    std::string arg_types;
                    std::string first_type;
                    auto mdef = method->type->get_base();
                    
                    for (const auto& [_, arg] : method->args) {
                        std::string arg_type = arg->type->get_name();
                        if (first_type.empty()) first_type = arg_type;
                        arg_types += ", " + arg_type;
                    }

                    if (name == "cast")
                        f << "\ti_cast(     X,Y,Z, " << mdef->name << ")\\\n";
                    else if (name == "ctr")
                        f << "\ti_construct(X,Y,Z" << arg_types << ")\\\n";
                    else if (name == "index")
                        f << "\ti_index(    X,Y,Z, " << mdef->name << arg_types << ")\\\n";
                    else
                        f << "\t" << m << "_method(   X,Y,Z, " << mdef->name << ", " << method->name << arg_types << ")\\\n";
                }
            }
        }
        f << "\n";

        if (inherits)
            f << "declare_mod(" << name << ", " << inherits->name << ")\n";
        else
            f << "declare_class(" << name << ")\n";
    }

    void emit_source(std::ofstream& file) {
        if (model->name != "allocated") return;

        auto output_methods = [&](const std::string& visibility, bool with_body) {
            for (const auto& [member_name, members] : this->members) {
                for (const auto& member : members) {
                    if (auto method = dynamic_cast<EMethod*>(member)) {
                        if (!visibility.empty() && method->visibility != visibility) continue;

                        std::string args = method->static_member || method->args.empty() ? "" : ", ";
                        for (const auto& [arg_name, arg] : method->args) {
                            if (!args.empty() && args != ", ") args += ", ";
                            args += arg->type->get_def()->name + std::string(arg->type->ref(), '*') + " " + arg_name;
                        }

                        file << method->type->get_def()->name << " " << name << "_" << method->name
                             << "(" << (method->static_member ? "" : name + " self") << args << ")"
                             << (with_body ? " {" : ";") << "\n";

                        if (with_body) {
                            EContext ctx(this, method);
                            file << method->code->emit(ctx);
                            file << "}\n";
                        }
                    }
                }
            }
            file << "\n";
        };

        output_methods("intern", false);  // Forward declaration for intern methods
        output_methods("", true);         // Output all methods with body

        if (inherits)
            file << "define_mod(" << name << ", " << inherits->name << ")\n\n";
        else
            file << "define_class(" << name << ")\n\n";
    }
};

class EMethod : public EMember {
public:
    std::string method_type;
    bool type_expressed;
    std::vector<Token> body;
    ENode* code;

    EMethod(const std::string& name, EIdent* type, const std::string& method_type, 
            ENode* value, const std::string& access, EClass* parent, bool type_expressed,
            bool static_member, std::unordered_map<std::string, EMember*> args, 
            const std::vector<Token>& body, const std::string& visibility)
        : EMember(name, type, parent->module, value, parent, access),
          method_type(method_type), type_expressed(type_expressed), body(body), code(nullptr) {
        this->static_member = static_member;
        this->args = args;
        this->visibility = visibility;
    }
};

class EProp : public EMember {
public:
    bool is_prop;

    EProp(EIdent* type, const std::string& name, const std::string& access,
          ENode* value, EClass* parent, const std::string& visibility, 
          bool is_prop = true, std::unordered_map<std::string, EMember*> args = {})
        : EMember(name, type, parent->module, value, parent, access), is_prop(is_prop) {
        this->visibility = visibility;
        this->args = args;
    }
};

class EStatements : public ENode {
public:
    EIdent* type;  // last statement type
    std::vector<std::unique_ptr<ENode>> value;

    EStatements(EIdent* type, std::vector<std::unique_ptr<ENode>>&& value)
        : type(type), value(std::move(value)) {}

    std::string emit(const EContext& ctx) const override {
        std::string res;
        for (const auto& enode : value) {
            res += ctx.indent();
            std::string code = enode->emit(ctx);
            res += code;
            if (code.substr(code.length() - 2) != "}\n") {
                res += ";\n";
            }
        }
        return res;
    }
};

class EIf : public ENode {
public:
    EIdent* type;
    std::unique_ptr<ENode> condition;
    std::unique_ptr<EStatements> body;
    std::unique_ptr<ENode> else_body;

    EIf(EIdent* type, std::unique_ptr<ENode> condition, 
        std::unique_ptr<EStatements> body, std::unique_ptr<ENode> else_body)
        : type(type), condition(std::move(condition)), 
          body(std::move(body)), else_body(std::move(else_body)) {}

    std::string emit(const EContext& ctx) const override {
        std::string e = "if (" + condition->emit(ctx) + ") {\n";
        e += body->emit(ctx);
        e += ctx.indent() + "}";
        if (else_body) {
            e += " else {\n";
            e += else_body->emit(ctx);
            e += ctx.indent() + "}\n";
        }
        e += "\n";
        return e;
    }
};

class EWhile : public ENode {
public:
    EIdent* type;
    std::unique_ptr<ENode> condition;
    std::unique_ptr<ENode> body;

    EWhile(EIdent* type, std::unique_ptr<ENode> condition, std::unique_ptr<ENode> body)
        : type(type), condition(std::move(condition)), body(std::move(body)) {}

    std::string emit(const EContext& ctx) const override {
        std::string e = "while (" + condition->emit(ctx) + ") {\n";
        e += body->emit(ctx);
        e += ctx.indent() + "}";
        e += "\n";
        return e;
    }
};

class EOperator : public ENode {
public:
    EIdent* type;
    std::unique_ptr<ENode> left;
    std::unique_ptr<ENode> right;
    std::string op;

    EOperator(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : type(type), left(std::move(left)), right(std::move(right)) {}

    virtual ~EOperator() = default;

    std::string emit(const EContext& ctx) const override {
        return left->emit(ctx) + " " + op + " " + right->emit(ctx);
    }

protected:
    EOperator(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right, const std::string& op)
        : type(type), left(std::move(left)), right(std::move(right)), op(op) {}
};

class EAdd : public EOperator {
public:
    EAdd(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "+") {}
};

class ESub : public EOperator {
public:
    ESub(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "-") {}
};

class EMul : public EOperator {
public:
    EMul(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "*") {}
};

class EDiv : public EOperator {
public:
    EDiv(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "/") {}
};

class EOr : public EOperator {
public:
    EOr(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "|") {}
};

class EAnd : public EOperator {
public:
    EAnd(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "&") {}
};

class EXor : public EOperator {
public:
    EXor(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "^") {}
};

class EIs : public EOperator {
public:
    EIs(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EOperator(type, std::move(left), std::move(right), "is") {}

    std::string emit(const EContext& ctx) const override {
        // Assuming left is an EMember and right is an ERuntimeType
        auto leftMember = dynamic_cast<EMember*>(left.get());
        auto rightType = dynamic_cast<ERuntimeType*>(right.get());
        
        if (!leftMember || !rightType) {
            throw std::runtime_error("Invalid operands for 'is' operator");
        }

        std::string l_type_id = "isa(" + leftMember->emit(ctx) + ")";
        std::string r_type_id = rightType->emit(ctx);
        return l_type_id + " == " + r_type_id;
    }
};

class EInherits : public EIs {
public:
    EInherits(EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right)
        : EIs(type, std::move(left), std::move(right)) {}

    std::string emit(const EContext& ctx) const override {
        // Assuming left is an EMember and right is an ERuntimeType
        auto leftMember = dynamic_cast<EMember*>(left.get());
        auto rightType = dynamic_cast<ERuntimeType*>(right.get());
        
        if (!leftMember || !rightType) {
            throw std::runtime_error("Invalid operands for 'inherits' operator");
        }

        std::string l_type_id = leftMember->emit(ctx);
        std::string r_type_id = rightType->emit(ctx);
        return "inherits(" + l_type_id + ", " + r_type_id + ")";
    }
};

// Helper function to get the base type of an EIdent or ENode
EIdent* etype(const ENode* node) {
    if (auto ident = dynamic_cast<const EIdent*>(node)) {
        return const_cast<EIdent*>(ident);
    } else {
        return node->type;
    }
}

// Function to check if a type is primitive
bool is_primitive(EModule* module, EIdent* ident) {
    auto e = etype(ident);
    if (e->ref()) return true;
    auto edef = e->get_def();
    if (!edef) throw std::runtime_error("Definition not found for " + e->ident);
    auto n = module->model(edef);
    return n != module->models["allocated"] && n != module->models["struct"] && *static_cast<int*>(n) > 0;
}

// Function to check if a type is castable to another type
EMethod* castable(EModule* module, EIdent* from, EIdent* to) {
    from = etype(from);
    to = etype(to);
    
    int ref = from->ref_total();
    if ((ref > 0 || is_primitive(module, from)) && to->get_base()->name == "bool") {
        return nullptr; // Indicates a direct cast is possible
    }

    auto cast_methods = from->get_def()->members["cast"];
    if (from == to || (is_primitive(module, from) && is_primitive(module, to))) {
        return nullptr; // Indicates a direct cast is possible
    }

    for (auto method : cast_methods) {
        if (method->type == to) {
            return static_cast<EMethod*>(method);
        }
    }

    return nullptr; // No cast method found
}

// Function to check if a type is constructable from another type
EMethod* constructable(EModule* module, EIdent* from, EIdent* to) {
    from = etype(from);
    to = etype(to);

    auto ctrs = to->get_def()->members["ctr"];
    if (from == to) {
        return nullptr; // Indicates direct construction is possible
    }

    for (auto method : ctrs) {
        if (method->args.empty()) {
            continue;
        }
        auto first_arg = method->args.begin()->second;
        if (first_arg->type == from) {
            return static_cast<EMethod*>(method);
        }
    }

    return nullptr; // No suitable constructor found
}

// Function to check if a type is convertible to another type
bool convertible(EModule* module, EIdent* from, EIdent* to) {
    from = etype(from);
    to = etype(to);

    if (from == to) return true;
    return castable(module, from, to) != nullptr || constructable(module, from, to) != nullptr;
}

// Function to convert an ENode to a specific type
std::unique_ptr<ENode> convert_enode(EModule* module, ENode* enode, EIdent* type) {
    if (enode->type == type) {
        return std::unique_ptr<ENode>(enode->clone());
    }

    auto cast_method = castable(module, enode->type, type);
    if (cast_method == nullptr && enode->type->get_def()->model == type->get_def()->model) {
        // Direct cast for same model types
        return std::make_unique<EExplicitCast>(type, std::unique_ptr<ENode>(enode->clone()));
    }

    if (cast_method) {
        return std::make_unique<EMethodCall>(type, enode, cast_method, std::vector<std::unique_ptr<ENode>>{std::unique_ptr<ENode>(enode->clone())});
    }

    auto construct_method = constructable(module, enode->type, type);
    if (construct_method) {
        return std::make_unique<EConstruct>(type, construct_method, std::vector<std::unique_ptr<ENode>>{std::unique_ptr<ENode>(enode->clone())});
    }

    throw std::runtime_error("Cannot convert from " + enode->type->ident + " to " + type->ident);
}

class ENode;
class EIdent;
class EModule;
class EMember;

class EContext {
public:
    EModule* module;
    EMember* method;
    std::vector<std::string> states;
    int indent_level;

    EContext(EModule* module, EMember* method)
        : module(module), method(method), indent_level(0) {}

    std::string indent() const {
        return std::string(indent_level, '\t');
    }

    void increase_indent() {
        ++indent_level;
    }

    void decrease_indent() {
        if (indent_level > 0) {
            --indent_level;
        }
    }

    void push(const std::string& state) {
        states.push_back(state);
    }

    void pop() {
        if (!states.empty()) {
            states.pop_back();
        }
    }

    std::string top_state() const {
        return states.empty() ? "" : states.back();
    }

    // Helper function to manage scoped indentation
    class ScopedIndent {
    public:
        explicit ScopedIndent(EContext& ctx) : ctx(ctx) {
            ctx.increase_indent();
        }
        ~ScopedIndent() {
            ctx.decrease_indent();
        }
    private:
        EContext& ctx;
    };

    // Helper function to manage scoped states
    class ScopedState {
    public:
        ScopedState(EContext& ctx, const std::string& state) : ctx(ctx) {
            ctx.push(state);
        }
        ~ScopedState() {
            ctx.pop();
        }
    private:
        EContext& ctx;
    };

    // Convenience functions to create scoped objects
    [[nodiscard]] ScopedIndent scoped_indent() {
        return ScopedIndent(*this);
    }

    [[nodiscard]] ScopedState scoped_state(const std::string& state) {
        return ScopedState(*this, state);
    }
};


class ELiteral : public ENode {
public:
    EIdent* type;

    explicit ELiteral(EIdent* type) : type(type) {}
    virtual ~ELiteral() = default;

    virtual std::string emit(const EContext& ctx) const override = 0;
};

class ELiteralReal : public ELiteral {
public:
    double value;

    ELiteralReal(EIdent* type, double value) : ELiteral(type), value(value) {}

    std::string emit(const EContext& ctx) const override {
        return std::to_string(value);
    }
};

class ELiteralInt : public ELiteral {
public:
    int64_t value;

    ELiteralInt(EIdent* type, int64_t value) : ELiteral(type), value(value) {}

    std::string emit(const EContext& ctx) const override {
        return std::to_string(value);
    }
};

class ELiteralStr : public ELiteral {
public:
    std::string value;

    ELiteralStr(EIdent* type, const std::string& value) : ELiteral(type), value(value) {}

    std::string emit(const EContext& ctx) const override {
        return "\"" + value + "\"";
    }
};

class ELiteralBool : public ELiteral {
public:
    bool value;

    ELiteralBool(EIdent* type, bool value) : ELiteral(type), value(value) {}

    std::string emit(const EContext& ctx) const override {
        return value ? "true" : "false";
    }
};

class ELiteralStrInterp : public ELiteral {
public:
    std::string value;
    std::vector<std::unique_ptr<ENode>> args;

    ELiteralStrInterp(EIdent* type, const std::string& value, std::vector<std::unique_ptr<ENode>>&& args)
        : ELiteral(type), value(value), args(std::move(args)) {}

    std::string emit(const EContext& ctx) const override {
        // This is a placeholder implementation. The actual implementation would depend on
        // how string interpolation is handled in the target language (C99 in this case).
        std::string result = "\"" + value + "\"";
        // TODO: Implement string interpolation logic
        return result;
    }
};

// Helper function to create the appropriate literal based on the token
std::unique_ptr<ELiteral> create_literal(EIdent* type, const Token& token) {
    if (type->ident == "f64" || type->ident == "f32") {
        return std::make_unique<ELiteralReal>(type, std::stod(token.value));
    } else if (type->ident == "i64" || type->ident == "i32" || type->ident == "i16" || type->ident == "i8" ||
               type->ident == "u64" || type->ident == "u32" || type->ident == "u16" || type->ident == "u8") {
        return std::make_unique<ELiteralInt>(type, std::stoll(token.value));
    } else if (type->ident == "bool") {
        return std::make_unique<ELiteralBool>(type, token.value == "true");
    } else if (type->ident == "string" || type->ident == "cstr") {
        return std::make_unique<ELiteralStr>(type, token.value.substr(1, token.value.length() - 2)); // Remove quotes
    } else {
        throw std::runtime_error("Unknown literal type: " + type->ident);
    }
}



// Add more ENode subclasses as needed...

// EModule class (partial implementation)
class EModule : public ENode {
public:
    filesystem::path path;
    string name;
    vector<Token> tokens;
    vector<filesystem::path> include_paths;
    unordered_map<string, void*> clang_cache;
    unordered_map<string, void*> include_defs;
    unordered_map<string, EModule*> parent_modules;
    unordered_map<string, ENode*> defs;
    unordered_map<string, EIdent*> type_cache;
    bool finished;
    int recur;
    unordered_map<string, void*> clang_defs;
    int expr_level;
    int index;
    vector<unordered_map<string, vector<Token>>> token_bank;
    vector<string> libraries_used;
    vector<string> compiled_objects;
    vector<string> main_symbols;
    vector<string> context_state;
    EMember* current_def;
    vector<unordered_map<string, vector<EMember*>>> member_stack;

    EModule(const filesystem::path& p) : path(p), finished(false), recur(0), expr_level(0), index(0), current_def(nullptr) {
        if (!name.empty()) {
            name = path.stem().string();
        }
    }


    std::unordered_map<std::string, EProp*> parse_arg_map(bool is_no_args = false, bool is_C99 = false) {
        std::unordered_map<std::string, EProp*> args;
        char end_char = is_C99 ? ')' : ']';

        if (peek_token().value == std::string(1, end_char)) {
            return args;
        }

        EIdent* arg_type = nullptr;
        if (!is_no_args) {
            arg_type = EIdent::peek(this);
        }

        int n_args = 0;
        while (arg_type) {
            arg_type = EIdent::parse(this);
            if (!arg_type) {
                throw std::runtime_error("Failed to parse type in arg");
            }

            std::string arg_name = is_C99 ? std::to_string(n_args) : "";
            if (!is_C99 && is_alpha(peek_token().value)) {
                arg_name = next_token().value;
            }

            if (!is_C99) {
                if (!is_alpha(arg_name)) {
                    throw std::runtime_error("Arg-name (" + arg_name + ") read is not an identifier");
                }
            }

            auto arg_def = arg_type->get_def();
            if (!arg_def) {
                throw std::runtime_error("Arg could not resolve type: " + arg_type->ident);
            }

            args[arg_name] = new EProp(
                arg_name,
                arg_type,
                this,
                nullptr,  // value
                nullptr,  // parent
                "public", // visibility
                "const"   // access
            );

            auto next = peek_token();
            if (next.value == ",") {
                consume(",");
            } else if (next.value != std::string(1, end_char)) {
                throw std::runtime_error("Expected ',' or '" + std::string(1, end_char) + "', got " + next.value);
            }

            next = peek_token();
            if (std::find(consumables.begin(), consumables.end(), next.value) == consumables.end() && !is_alpha(next.value)) {
                break;
            }

            ++n_args;
        }

        return args;
    }

    std::vector<std::unique_ptr<ENode>> parse_args(EMethod* signature, bool C99 = false) {
        char end_char = C99 ? ')' : ']';
        std::vector<std::unique_ptr<ENode>> enode_args;
        int arg_count = signature ? signature->args.size() : -1;
        int arg_index = 0;

        if (arg_count) {
            while (true) {
                if (peek_token().value == "]") {
                    throw std::runtime_error("Not enough arguments given to " + signature->name);
                }
                auto op = parse_expression();
                enode_args.push_back(std::move(op));
                ++arg_index;
                if (peek_token().value == ",") {
                    if (arg_count > 0 && arg_index >= arg_count) {
                        throw std::runtime_error("Too many args given to " + signature->name);
                    }
                    consume(",");
                } else {
                    break;
                }
            }
        }

        if (peek_token().value != std::string(1, end_char)) {
            throw std::runtime_error("Expected " + std::string(1, end_char) + " after args");
        }
        return enode_args;
    }

    std::unique_ptr<ENode> parse_expression() {
        ++expr_level;
        auto result = parse_add();
        --expr_level;
        return result;
    }

    std::unique_ptr<ENode> parse_add() {
        return parse_operator(
            [this]() { return parse_mult(); },
            "+", "-",
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<EAdd>(type, std::move(left), std::move(right));
            },
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<ESub>(type, std::move(left), std::move(right));
            }
        );
    }

    std::unique_ptr<ENode> parse_mult() {
        return parse_operator(
            [this]() { return parse_is(); },
            "*", "/",
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<EMul>(type, std::move(left), std::move(right));
            },
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<EDiv>(type, std::move(left), std::move(right));
            }
        );
    }

    std::unique_ptr<ENode> parse_is() {
        return parse_operator(
            [this]() { return parse_primary(); },
            "is", "inherits",
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<EIs>(type, std::move(left), std::move(right));
            },
            [this](EIdent* type, std::unique_ptr<ENode> left, std::unique_ptr<ENode> right) {
                return std::make_unique<EInherits>(type, std::move(left), std::move(right));
            }
        );
    }

private:
    template<typename ParseFunc, typename CreateOpFunc>
    std::unique_ptr<ENode> parse_operator(ParseFunc parse_lr_fn, const std::string& op_type0, const std::string& op_type1,
                                          CreateOpFunc create_op0, CreateOpFunc create_op1) {
        auto left = parse_lr_fn();
        while (peek_token().value == op_type0 || peek_token().value == op_type1) {
            auto op_type = peek_token().value == op_type0 ? op_type0 : op_type1;
            consume(op_type);
            auto right = parse_lr_fn();
            auto op_name = operators[op_type];

            if (left->type && right->type) {
                if (left->type->get_def()->members.find(op_name) != left->type->get_def()->members.end()) {
                    auto& ops = left->type->get_def()->members[op_name];
                    for (auto& method : ops) {
                        if (method->args.size() != 1) {
                            throw std::runtime_error("Operators must take in 1 argument");
                        }
                        if (convertible(right->type, method->args[0]->type)) {
                            return std::make_unique<EMethodCall>(method->type, std::move(left), method,
                                std::vector<std::unique_ptr<ENode>>{convert_enode(right.get(), method->args[0]->type)});
                        }
                    }
                }
                auto type = preferred_type(left->type, right->type);
                left = (op_type == op_type0 ? create_op0 : create_op1)(type, std::move(left), std::move(right));
            }
        }
        return left;
    }

    // ... other EModule members and methods ...
};

// Placeholder for models
unordered_map<string, void*> models;

// Global variables
string build_root;
bool is_debug = false;
bool verbose = false;

const vector<string> no_arg_methods = {"init", "dealloc"};

unordered_map<string, string> operators = {
    {"+", "add"},
    {"-", "sub"},
    {"*", "mul"},
    {"/", "div"},
    {"||", "or"},
    {"&&", "and"},
    {"^", "xor"},
    {">>", "right"},
    {"<<", "left"},
    {":", "assign"},
    {"=", "assign"},
    {"+=", "assign_add"},
    {"-=", "assign_sub"},
    {"*=", "assign_mul"},
    {"/=", "assign_div"},
    {"|=", "assign_or"},
    {"&=", "assign_and"},
    {"^=", "assign_xor"},
    {">>=", "assign_right"},
    {"<<=", "assign_left"},
    {"==", "compare_equal"},
    {"!=", "compare_not"},
    {"%=", "mod_assign"},
    {"is", "is"},
    {"inherits", "inherits"}
};

const vector<string> keywords = {
    "class", "proto", "struct", "import", "typeof", "schema", "is", "inherits",
    "init", "destruct", "ref", "const", "volatile",
    "return", "asm", "if", "switch",
    "while", "for", "do", "signed", "unsigned", "cast"
};

const vector<string> consumables = {
    "ref", "schema", "enum", "class", "union", "proto", "struct", "const", "volatile", "signed", "unsigned"
};

const vector<string> assign = {
    ":", "=", "+=", "-=", "*=", "/=", "|=",
    "&=", "^=", ">>=", "<<=", "%="
};

const vector<string> compare = {"==", "!="};

// Utility functions
pair<string, string> shared_ext() {
    #ifdef __linux__
        return {"lib", "so"};
    #elif __APPLE__
        return {"lib", "dylib"};
    #elif _WIN32
        return {"", "dll"};
    #else
        throw runtime_error("Unknown platform");
    #endif
}

pair<string, string> static_ext() {
    #if defined(__linux__) || defined(__APPLE__)
        return {"lib", "a"};
    #elif _WIN32
        return {"", "lib"};
    #else
        throw runtime_error("Unknown platform");
    #endif
}

void create_symlink(const string& target, const string& link_name) {
    if (filesystem::exists(link_name)) {
        filesystem::remove(link_name);
    }
    filesystem::create_symlink(target, link_name);
}

int system(const string& command) {
    cout << "> " << command << endl;
    return ::system(command.c_str());
}

// EModule implementation
void EModule::push_token_state(const vector<Token>& new_tokens, int index = 0) {
    token_bank.push_back({{"index", {Token(to_string(this->index))}}});
    token_bank.back()["tokens"] = tokens;
    this->index = index;
    this->tokens = new_tokens;
}

void EModule::transfer_token_state() {
    vector<Token> tokens_before = tokens;
    int index_before = index;
    auto ts = token_bank.back();
    token_bank.pop_back();
    index = index_before;
    tokens = ts["tokens"];
    if (tokens != tokens_before) {
        throw runtime_error("transfer of cursor state requires the same list");
    }
    if (index > index_before) {
        throw runtime_error("transfer of cursor state requires the same list");
    }
}

void EModule::pop_token_state() {
    if (token_bank.empty()) {
        throw runtime_error("stack is empty");
    }
    auto ts = token_bank.back();
    token_bank.pop_back();
    index = stoi(ts["index"][0].value);
    tokens = ts["tokens"];
}

optional<Token> EModule::next_token() {
    if (index < tokens.size()) {
        return tokens[index++];
    }
    return nullopt;
}

optional<Token> EModule::prev_token() {
    if (index > 0) {
        return tokens[--index];
    }
    return nullopt;
}

pair<vector<Token>, int> EModule::debug_tokens() {
    return {tokens, index};
}

optional<Token> EModule::peek_token(int ahead = 0) {
    if (index + ahead < tokens.size()) {
        return tokens[index + ahead];
    }
    return nullopt;
}

void EModule::parse_header(const string& header_file) {
    // Implementation will be added later
}

void EModule::push_context_state(const string& value) {
    context_state.push_back(value);
}

void EModule::pop_context_state() {
    context_state.pop_back();
}

string EModule::top_context_state() {
    return context_state.empty() ? "" : context_state.back();
}

void EModule::reset_member_depth() {
    member_stack = {unordered_map<string, vector<EMember*>>()};
}

void EModule::push_member_depth() {
    member_stack.push_back(unordered_map<string, vector<EMember*>>());
}

unordered_map<string, vector<EMember*>> EModule::pop_member_depth() {
    auto result = move(member_stack.back());
    member_stack.pop_back();
    return result;
}

void EModule::push_return_type(EIdent* type) {
    if (member_stack.back().find(".return-type") != member_stack.back().end()) {
        throw runtime_error("Return type already set");
    }
    member_stack.back()[".return-type"] = {new EMember(".return-type", type, this)};
}

void EModule::push_member(EMember* member, EMember* access = nullptr) {
    auto& top = member_stack.back();
    if (top.find(member->name) == top.end()) {
        top[member->name] = vector<EMember*>();
    }
    top[member->name].push_back(member);
}

// Other EModule methods will be implemented later

void* EModule::model(ENode* edef) {
    while (dynamic_cast<EAlias*>(edef)) {
        edef = dynamic_cast<EAlias*>(edef)->to;
    }
    if (auto eclass = dynamic_cast<EClass*>(edef)) {
        return eclass->model;
    }
    return models["allocated"];
}

bool EModule::is_primitive(EIdent* ident) {
    auto e = etype(ident);
    if (e->ref()) return true;
    auto edef = e->get_def();
    if (!edef) throw runtime_error("Definition not found for " + e->ident);
    auto n = model(edef);
    return n != models["allocated"] && n != models["struct"] && *static_cast<int*>(n) > 0;
}

EIdent* EModule::preferred_type(EIdent* etype0, EIdent* etype1) {
    etype0 = etype(etype0);
    etype1 = etype(etype1);
    if (etype0 == etype1) return etype0;
    auto model0 = etype0->get_def()->model;
    auto model1 = etype1->get_def()->model;
    if (*static_cast<bool*>(model0["realistic"])) {
        if (*static_cast<bool*>(model1["realistic"])) {
            return *static_cast<int*>(model1["size"]) > *static_cast<int*>(model0["size"]) ? etype1 : etype0;
        }
        return etype0;
    }
    if (*static_cast<bool*>(model1["realistic"])) {
        return etype1;
    }
    return *static_cast<int*>(model0["size"]) > *static_cast<int*>(model1["size"]) ? etype0 : etype1;
}

ENode* EModule::parse_operator(function<ENode*()> parse_lr_fn, const string& op_type0, const string& op_type1, EOperator* etype0, EOperator* etype1) {
    auto left = parse_lr_fn();
    while (peek_token().value().value == op_type0 || peek_token().value().value == op_type1) {
        auto etype = peek_token().value().value == op_type0 ? etype0 : etype1;
        consume();
        auto right = parse_lr_fn();
        if (operators.find(op_type0) == operators.end() || operators.find(op_type1) == operators.end()) {
            throw runtime_error("Operator not found");
        }
        auto token = peek_token().value();
        auto op_name = operators[token.value == op_type0 ? op_type0 : op_type1];

        if (left->type && right->type) {
            if (left->type->get_def()->members.find(op_name) != left->type->get_def()->members.end()) {
                auto& ops = left->type->get_def()->members[op_name];
                for (auto& method : ops) {
                    if (method->args.size() != 1) {
                        throw runtime_error("Operators must take in 1 argument");
                    }
                    if (convertible(right->type, method->args[0]->type)) {
                        return new EMethodCall(method->type, left, method, {convert_enode(right, method->args[0]->type)});
                    }
                }
            }
            left = new EOperator(preferred_type(left->type, right->type), left, right);
        }
    }
    return left;
}

ENode* EModule::parse_add() {
    return parse_operator([this]() { return parse_mult(); }, "+", "-", new EAdd(), new ESub());
}

ENode* EModule::parse_mult() {
    return parse_operator([this]() { return parse_is(); }, "*", "/", new EMul(), new EDiv());
}

ENode* EModule::parse_is() {
    return parse_operator([this]() { return parse_primary(); }, "is", "inherits", new EIs(), new EInherits());
}

ELiteralType EModule::is_bool(const Token& token) {
    string t = token.value;
    return (t == "true" || t == "false") ? ELiteralBool : EUndefined;
}

ELiteralType EModule::is_numeric(const Token& token) {
    bool is_digit = isdigit(token.value[0]);
    bool has_dot = token.value.find('.') != string::npos;
    if (!is_digit && !has_dot) return EUndefined;
    if (!has_dot) return ELiteralInt;
    return ELiteralReal;
}

ELiteralType EModule::is_string(const Token& token) {
    char t = token.value[0];
    return (t == '"' || t == '\'') ? ELiteralStr : EUndefined;
}

vector<ENode*> EModule::parse_args(EMethod* signature, bool C99 = false) {
    char e = C99 ? ')' : ']';
    vector<ENode*> enode_args;
    int arg_count = signature ? signature->args.size() : -1;
    int arg_index = 0;
    
    if (arg_count) {
        while (true) {
            if (peek_token().value().value == "]") {
                throw runtime_error("Not enough arguments given to " + signature->name);
            }
            auto op = parse_expression();
            enode_args.push_back(op);
            ++arg_index;
            if (peek_token().value().value == ",") {
                if (arg_count > 0 && arg_index >= arg_count) {
                    throw runtime_error("Too many args given to " + signature->name);
                }
                consume();
            } else {
                break;
            }
        }
    }
    
    if (peek_token().value().value != string(1, e)) {
        throw runtime_error("Expected ] after args");
    }
    return enode_args;
}

EIdent* EModule::type_of(const variant<ENode*, string>& value) {
    if (holds_alternative<ENode*>(value)) {
        return get<ENode*>(value)->type;
    } else if (!get<string>(value).empty()) {
        return new EIdent(get<string>(value), this);
    }
    return nullptr;
}

// Other parsing methods will be implemented here

ENode* EModule::parse_while(const Token& t0) {
    consume();  // Consume 'while'
    if (peek_token().value().value != "[") {
        throw runtime_error("Expected condition expression '['");
    }
    consume();  // Consume '['
    auto condition = parse_expression();
    if (next_token().value().value != "]") {
        throw runtime_error("Expected condition expression ']'");
    }
    push_member_depth();
    auto statements = parse_statements();
    pop_member_depth();
    return new EWhile(nullptr, condition, statements);
}

ENode* EModule::parse_do_while(const Token& t0) {
    consume();  // Consume 'do'
    push_member_depth();
    auto statements = parse_statements();
    pop_member_depth();
    if (next_token().value().value != "while") {
        throw runtime_error("Expected 'while'");
    }
    if (peek_token().value().value != "[") {
        throw runtime_error("Expected condition expression '['");
    }
    consume();  // Consume '['
    auto condition = parse_expression();
    if (next_token().value().value != "]") {
        throw runtime_error("Expected condition expression ']'");
    }
    return new EDoWhile(nullptr, condition, statements);
}

ENode* EModule::parse_if_else(const Token& t0) {
    push_member_depth();
    consume();  // Consume 'if'
    bool use_parens = peek_token().value().value == "[";
    if (use_parens) {
        if (peek_token().value().value != "[") {
            throw runtime_error("Expected condition expression '['");
        }
        consume();  // Consume '['
    }
    auto condition = parse_expression();
    if (use_parens) {
        if (next_token().value().value != "]") {
            throw runtime_error("Expected condition expression ']'");
        }
    }
    push_member_depth();
    auto statements = parse_statements();
    pop_member_depth();
    ENode* else_statements = nullptr;
    if (peek_token().value().value == "else") {
        consume();  // Consume 'else'
        push_member_depth();
        if (peek_token().value().value == "if") {
            consume();  // Consume 'else if'
            else_statements = parse_statements();
        } else {
            else_statements = parse_statements();
        }
        pop_member_depth();
    }
    pop_member_depth();
    return new EIf(nullptr, condition, statements, else_statements);
}

ENode* EModule::parse_for(const Token& t0) {
    consume();  // Consume 'for'
    if (peek_token().value().value != "[") {
        throw runtime_error("Expected condition expression '['");
    }
    consume();  // Consume '['
    push_member_depth();
    auto statement = parse_statements();
    if (next_token().value().value != ";") {
        throw runtime_error("Expected ';'");
    }
    auto condition = parse_expression();
    if (next_token().value().value != ";") {
        throw runtime_error("Expected ';'");
    }
    auto post_iteration = parse_expression();
    if (next_token().value().value != "]") {
        throw runtime_error("Expected ']'");
    }
    push_member_depth();
    auto for_block = parse_statements();
    pop_member_depth();
    pop_member_depth();
    return new EFor(nullptr, statement, condition, post_iteration, for_block);
}

ENode* EModule::parse_break(const Token& t0) {
    consume();  // Consume 'break'
    ENode* levels = nullptr;
    if (peek_token().value().value == "[") {
        consume();  // Consume '['
        levels = parse_expression();
        if (peek_token().value().value != "]") {
            throw runtime_error("Expected ']' after break[expression...]");
        }
        consume();  // Consume ']'
    }
    return new EBreak(nullptr, levels);
}

ENode* EModule::parse_return(const Token& t0) {
    consume();  // Consume 'return'
    auto result = parse_expression();
    auto rmember = lookup_stack_member(".return-type");
    if (!rmember) {
        throw runtime_error("No .return-type record set in stack");
    }
    return new EMethodReturn(rmember.value()->type, result);
}

pair<EMember*, ENode*> EModule::parse_anonymous_ref(EIdent* ident) {
    EMember* member = nullptr;
    ENode* indexing_node = nullptr;
    if (peek_token().value().value == "[") {
        if (!ident->ref()) {
            throw runtime_error("Expecting anonymous reference");
        }
        consume("[");
        member = dynamic_cast<EMember*>(parse_expression());
        if (auto prop = dynamic_cast<EProp*>(member)) {
            member = new EProp(prop->name, prop->type, this, prop->value, prop->parent,
                               prop->visibility, true, prop->args);
        }
        if (!member) {
            throw runtime_error("Type not found: " + ident->ident);
        }
        consume("]");

        if (peek_token().value().value == "[") {
            consume("[");
            indexing_node = parse_expression();
            consume("]");
        }
    }
    return {member, indexing_node};
}

ENode* EModule::parse_statement() {
    auto t0 = peek_token().value();

    if (t0.value == "return") return parse_return(t0);
    if (t0.value == "break")  return parse_break(t0);
    if (t0.value == "for")    return parse_for(t0);
    if (t0.value == "while")  return parse_while(t0);
    if (t0.value == "if")     return parse_if_else(t0);
    if (t0.value == "do")     return parse_do_while(t0);

    auto ident = EIdent::parse(this);
    auto type_def = ident->get_def();
    if (type_def || !ident->members.empty()) {
        if (ident->members.empty() && dynamic_cast<EMethod*>(type_def)) {
            return parse_expression();
        }
        auto after = peek_token().value();

        bool not_member_or_ref = ident->members.empty() && !ident->ref();
        if (after.value == "[" && (not_member_or_ref || !ident->members.empty())) {
            if (!ident->members.empty()) {
                auto method = ident->members.back();
                auto args = parse_args(dynamic_cast<EMethod*>(method));
                auto conv = convert_args(dynamic_cast<EMethod*>(method), args);
                return new EMethodCall(ident, method, conv);
            } else {
                throw runtime_error("Compilation error: unassigned instance");
            }
        } else {
            bool declare = false;
            EMember* member = nullptr;
            ENode* indexing_node = nullptr;

            if (!ident->members.empty()) {
                auto assign = is_assign(after.value);
                bool require_assign = false;
                size_t index = 0;
                while (true) {
                    if (index >= ident->members.size()) {
                        throw runtime_error("Writable member not found");
                    }
                    member = ident->members[index];
                    if (member->access != "const" || !assign) {
                        break;
                    }
                    ++index;
                }
            } else {
                tie(member, indexing_node) = parse_anonymous_ref(ident);
                after = peek_token().value();
                if (!member) {
                    if (is_alpha(after.value)) {
                        consume(after.value);
                        declare = true;
                        member = new EMember(after.value, nullptr, ident, nullptr, nullptr, "public");
                        member->_is_mutable = true;
                        after = peek_token().value();
                    } else {
                        throw runtime_error("Not handled");
                    }
                } else {
                    require_assign = true;
                }
            }

            auto assign = is_assign(after.value);
            if (declare) {
                push_member(member);
            }
            if (assign) {
                if (!member->access) {
                    if (!member->_is_mutable) {
                        throw runtime_error("Internal member state error (no EMember access set on registered member)");
                    }
                    member->access = (assign == "=") ? "const" : nullptr;
                }
                consume(assign);
                auto value = parse_expression();
                auto type = ident->members.empty() ? ident : type_def->type;

                return new EAssign(type, member, indexing_node, declare, value);
            } else {
                if (require_assign) {
                    throw runtime_error("Assign required after anonymous reference at statement level");
                }
                return new EDeclaration(member->type, member);
            }
        }
    } else {
        return parse_expression();
    }
}

EStatements* EModule::parse_statements() {
    vector<ENode*> block;
    bool multiple = peek_token().value().value == "[";

    auto [tokens, index] = debug_tokens();
    if (multiple) {
        consume();  // Consume '['
    }

    int depth = 1;
    push_member_depth();
    while (peek_token()) {
        auto t = peek_token().value();
        if (t.value == "[") {
            ++depth;
            push_member_depth();
            consume();
            continue;
        }
        auto n = parse_statement();
        if (!n) {
            throw runtime_error("Expected statement or expression");
        }
        block.push_back(n);
        if (!multiple) break;
        if (peek_token().value().value == "]") {
            pop_member_depth();
            consume();  // Consume ']'
            --depth;
            if (depth == 0) {
                break;
            }
        }
    }
    return new EStatements(nullptr, block);
}


EMethod* EModule::finish_method(EClass* cl, const Token& token, const string& method_type, bool is_static, const string& visibility) {
    bool is_ctr = method_type == "ctr";
    bool is_cast = method_type == "cast";
    bool is_index = method_type == "index";
    bool is_no_args = method_type == "init" || method_type == "dealloc" || method_type == "cast";
    
    if (is_cast) {
        consume("cast");
    }
    
    auto t = peek_token().value();
    bool has_type = is_alpha(t.value);
    EIdent* return_type = has_type ? EIdent::parse(this) : new EIdent(cl->name, this);
    Token name_token = (method_type == "method" || method_type == "index") ? next_token().value() : Token("");
    
    if (!has_type) {
        consume(method_type);
    }
    
    auto t_next = is_no_args ? Token("[") : next_token().value();
    if (t_next.value != "[") {
        throw runtime_error("Expected '[', got " + t_next.value + " at line " + to_string(t_next.line_num));
    }
    
    auto args = parse_arg_map(is_no_args);
    if (!is_no_args) {
        auto end_token = next_token().value();
        if (end_token.value != "]" && end_token.value != "::") {
            throw runtime_error("Expected end of args");
        }
    }

    t = peek_token().value();
    if (t.value != "[") {
        throw runtime_error("Expected '[', got " + t.value + " at line " + to_string(t.line_num));
    }
    
    // Parse method body
    auto body_token = next_token().value();
    vector<Token> body;
    int depth = 1;
    while (depth > 0) {
        body.push_back(body_token);
        body_token = next_token().value();
        if (body_token.value == "[") depth++;
        if (body_token.value == "]") {
            depth--;
            if (depth == 0) {
                body.push_back(body_token);
            }
        }
    }

    string id = method_type == "method" ? name_token.value : method_type;
    vector<string> arg_keys;
    for (const auto& arg : args) {
        arg_keys.push_back(arg.first);
    }
    
    if (is_index && arg_keys.empty()) {
        throw runtime_error("Index methods require an argument");
    }

    string name;
    if (is_cast) {
        name = "cast_" + return_type->ident;
    } else if (is_index) {
        string arg_types;
        for (const auto& a : args) {
            arg_types += "_" + a.second->type->get_name();
        }
        name = "index" + arg_types;
    } else if (is_ctr) {
        if (arg_keys.empty()) {
            throw runtime_error("Constructors must have something to construct-with");
        }
        name = "with_" + args[arg_keys[0]]->type->get_name();
    } else {
        name = id;
    }

    auto method = new EMethod(name, return_type, method_type, nullptr, "self", cl, has_type,
                              is_static, args, body, visibility);

    if (cl->members.find(id) == cl->members.end()) {
        cl->members[id] = {method};
    } else {
        cl->members[id].push_back(method);
    }

    return method;
}

void EModule::finish_class(EClass* cl) {
    if (!cl->block_tokens) return;
    push_token_state(cl->block_tokens);
    consume("[");
    cl->members.clear();
    cl->members["ctr"] = {};
    cl->members["cast"] = {};

    // Convert the basic meta_model of strings to a resolved one we call meta_types
    if (!cl->meta_model.empty()) {
        cl->meta_types.clear();
        for (const auto& [symbol, conforms] : cl->meta_model) {
            EClass* edef_conforms = nullptr;
            if (!conforms.empty()) {
                auto it = defs.find(conforms);
                if (it == defs.end()) {
                    throw runtime_error("No definition found for meta argument " + conforms);
                }
                edef_conforms = dynamic_cast<EClass*>(it->second);
                if (!edef_conforms) {
                    throw runtime_error("Invalid type for meta argument " + conforms);
                }
            }
            cl->meta_types[symbol] = new EIdent(symbol, this, edef_conforms);
        }
    }

    while (peek_token().value().value != "]") {
        auto token = peek_token().value();
        bool is_static = token.value == "static";
        if (is_static) {
            consume("static");
            token = peek_token().value();
        }
        string visibility = "public";
        if (token.value == "public" || token.value == "intern") {
            visibility = token.value;
            token = next_token().value();
        }
        if (!is_static) {
            is_static = token.value == "static";
            if (is_static) {
                consume("static");
                token = peek_token().value();
            }
        }

        bool is_method0 = false;
        push_token_state(tokens, index);
        auto ident = EIdent::parse(this);

        if (token.value == "cast") {
            if (is_static) {
                throw runtime_error("Cast must be defined as non-static");
            }
            is_method0 = true;
        } else if (token.value == cl->name) {
            if (is_static) {
                throw runtime_error("Constructor must be defined as non-static");
            }
            is_method0 = true;
        } else {
            is_method0 = ident && peek_token(1).value().value == "[";
            token = peek_token().value();
            if (find(no_arg_methods.begin(), no_arg_methods.end(), token.value) != no_arg_methods.end()) {
                is_method0 = true;
            }
        }

        pop_token_state();

        if (!is_method0) {
            // Property
            ident = EIdent::parse(this);
            auto name_token = next_token().value();
            auto next_token_value = peek_token().value().value;
            EProp* prop_node;
            if (next_token_value == ":") {
                consume(":");
                auto value_token = next_token().value();
                prop_node = new EProp(ident, name_token.value, "self", value_token.value, cl, visibility);
            } else {
                prop_node = new EProp(ident, name_token.value, "self", nullptr, cl, visibility);
            }
            auto id = name_token.value;
            if (cl->members.find(id) == cl->members.end()) {
                cl->members[id] = {};
            }
            cl->members[id].push_back(prop_node);
        } else if (token.value == cl->name) {
            finish_method(cl, token, "ctr", is_static, visibility);
        } else if (find(no_arg_methods.begin(), no_arg_methods.end(), token.value) != no_arg_methods.end()) {
            finish_method(cl, token, token.value);
        } else if (token.value == "cast") {
            finish_method(cl, token, "cast", is_static, visibility);
        } else if (token.value == "index") {
            finish_method(cl, token, "index", is_static, visibility);
        } else {
            finish_method(cl, token, "method", is_static, visibility);
        }
    }
    pop_token_state();
}

EClass* EModule::parse_class(const string& visibility, const unordered_map<string, string>& meta) {
    auto token = next_token().value();
    if (token.value != "class") {
        throw runtime_error("Expected 'class', got " + token.value + " at line " + to_string(token.line_num));
    }
    auto name_token = next_token().value();
    if (defs.find(name_token.value) != defs.end()) {
        throw runtime_error("Duplicate definition in module");
    }

    auto class_node = new EClass(this, name_token.value, visibility, meta);

    // Set 'model' of class -- this is the storage model, with default being an allocation (heap)
    class_node->model = models["allocated"];

    if (peek_token().value().value == ":") {
        consume(":");
        class_node->inherits = dynamic_cast<EClass*>(find_def(next_token().value().value));
    }

    vector<Token> block_tokens;
    block_tokens.push_back(next_token().value());
    if (block_tokens[0].value != "[") {
        throw runtime_error("Expected '[', got " + block_tokens[0].value + " at line " + to_string(block_tokens[0].line_num));
    }
    int level = 1;
    while (level > 0) {
        auto token = next_token().value();
        if (token.value == "[") level++;
        if (token.value == "]") level--;
        block_tokens.push_back(token);
    }
    class_node->block_tokens = block_tokens;
    return class_node;
}

class EImport : public ENode {
public:
    string name;
    string source;
    vector<string> includes;
    vector<string> cfiles;
    vector<string> links;
    bool imported;
    vector<string> build_args;
    int import_type;
    vector<string> library_exports;
    string visibility;
    static const int none = 0;
    static const int source = 1;
    static const int library = 2;
    static const int project = 4;
    string main_symbol;

    EImport(const string& n = "", const string& src = "", const vector<string>& inc = {}, 
            const string& vis = "intern")
        : name(n), source(src), includes(inc), imported(false), import_type(0), visibility(vis) {}

    void emit_header(ofstream& f) {
        for (const auto& inc : includes) {
            string h_file = inc;
            if (!h_file.ends_with(".h")) h_file += ".h";
            f << "#include <" << h_file << ">\n";
        }
    }

    void emit_source(ofstream& f) {}

    BuildState build_project(const string& name, const string& url) {
        // Implementation of build_project
        // ...
    }

    BuildState build_source() {
        // Implementation of build_source
        // ...
    }

    void process(EModule* module) {
        if (!name.empty() && source.empty() && includes.empty()) {
            vector<string> attempt = {"", "spec/"};
            bool exists = false;
            for (const auto& pre : attempt) {
                filesystem::path si_path = pre + name + ".si";
                if (filesystem::exists(si_path)) {
                    module_path = si_path;
                    cout << "module " << si_path << endl;
                    this->module = si_path;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                throw runtime_error("Path does not exist for silver module: " + name);
            }
        } else if (!name.empty() && !source.empty()) {
            bool has_c = false, has_h = false, has_rs = false, has_so = false, has_a = false;

            for (const auto& src : source) {
                if (src.ends_with(".c")) { has_c = true; break; }
                if (src.ends_with(".h")) { has_h = true; break; }
                if (src.ends_with(".rs")) { has_rs = true; break; }
                if (src.ends_with(".so")) { has_so = true; break; }
                if (src.ends_with(".a")) { has_a = true; break; }
            }

            if (has_h) {
                import_type = EImport::source;
            } else if (has_c || has_rs) {
                filesystem::current_path(relative_path);
                import_type = EImport::source;
                build_source();
            } else if (has_so || has_a) {
                filesystem::current_path(relative_path);
                import_type = EImport::library;
                if (library_exports.empty()) {
                    library_exports = {};
                }
                for (const auto& lib : source) {
                    string rem = lib.substr(0, lib.length() - 3);
                    library_exports.push_back(rem);
                }
            } else {
                import_type = EImport::project;
                build_project(name, source[0]);
                if (library_exports.empty()) {
                    library_exports = {};
                }
                library_exports.push_back(name);
            }
        }

        module->process_includes(includes);
    }
};

void EModule::parse_import_fields(EImport* im) {
    while (true) {
        if (peek_token().value().value == "]") {
            consume("]");
            break;
        }
        auto arg_name = next_token().value();
        if (is_string(arg_name) == ELiteralStr) {
            im->source = {arg_name.value.substr(1, arg_name.value.length() - 2)};
        } else {
            if (!is_alpha(arg_name.value)) {
                throw runtime_error("Expected identifier for import arg");
            }
            consume(":");
            if (arg_name.value == "name") {
                auto token_name = next_token().value();
                if (is_string(token_name) == ELiteralStr) {
                    throw runtime_error("Expected token for import name");
                }
                im->name = token_name.value;
            } else if (arg_name.value == "links") {
                import_list(im, "links");
            } else if (arg_name.value == "includes") {
                import_list(im, "includes");
            } else if (arg_name.value == "source") {
                import_list(im, "source");
            } else if (arg_name.value == "build") {
                import_list(im, "build_args");
            } else if (arg_name.value == "shell") {
                auto token_shell = next_token().value();
                if (is_string(token_shell) != ELiteralStr) {
                    throw runtime_error("Expected shell invocation for building");
                }
                im->shell = token_shell.value.substr(1, token_shell.value.length() - 2);
            } else if (arg_name.value == "defines") {
                throw runtime_error("Not implemented");
            } else {
                throw runtime_error("Unknown arg: " + arg_name.value);
            }
        }

        if (peek_token().value().value == ",") {
            consume(",");
        } else {
            auto n = next_token().value();
            if (n.value != "]") {
                throw runtime_error("Expected comma or ] after arg " + arg_name.value);
            }
            break;
        }
    }
}

EImport* EModule::parse_import(const string& visibility) {
    auto result = new EImport("", "", {}, visibility);
    consume("import");

    bool is_inc = peek_token().value().value == "<";
    if (is_inc) {
        consume("<");
    }
    auto module_name = next_token().value();
    if (!is_alpha(module_name.value)) {
        throw runtime_error("Expected module name identifier");
    }

    if (is_inc) {
        result->includes.push_back(module_name.value);
        while (true) {
            auto t = next_token().value();
            if (t.value != "," && t.value != ">") {
                throw runtime_error("Expected > or , in <include> syntax");
            }
            if (t.value == ">") break;
            auto proceeding = next_token().value();
            result->includes.push_back(proceeding.value);
        }
    }
    auto as_ = peek_token().value();
    if (as_.value == "as") {
        consume("as");
        result->isolate_namespace = next_token().value().value;
    }
    if (!is_alpha(module_name.value)) {
        throw runtime_error("Expected variable identifier, found " + module_name.value);
    }
    result->name = module_name.value;
    if (as_.value == "[") {
        consume("[");
        auto n = peek_token().value();
        if (is_string(n) == ELiteralStr) {
            result->source = {};
            while (true) {
                auto inner = next_token().value();
                if (is_string(inner) != ELiteralStr) {
                    throw runtime_error("Expected string literal");
                }
                string source = inner.value.substr(1, inner.value.length() - 2);
                result->source.push_back(source);
                auto e = next_token().value();
                if (e.value == ",") {
                    continue;
                }
                if (e.value != "]") {
                    throw runtime_error("Expected ]");
                }
                break;
            }
        } else {
            parse_import_fields(result);
        }
    }

    return result;
}

void EModule::import_list(EImport* im, const string& list_name) {
    vector<string>& res = im->*get_member_pointer(list_name);
    res.clear();
    if (peek_token().value().value == "[") {
        consume("[");
        while (true) {
            auto arg = next_token().value();
            if (arg.value == "]") break;
            if (is_string(arg) != ELiteralStr) {
                throw runtime_error("Expected build-arg in string literal");
            }
            res.push_back(arg.value.substr(1, arg.value.length() - 2));
            if (peek_token().value().value == ",") {
                consume(",");
                continue;
            }
            break;
        }
        consume("]");
    } else {
        res.push_back(next_token().value().value.substr(1, next_token().value().value.length() - 2));
    }
}

class EModule : public ENode {
public:
    // ... (previous declarations)

    void parse(const vector<Token>& tokens) {
        push_token_state(tokens);
        string visibility;
        unordered_map<string, string> meta_model;

        while (index < tokens.size()) {
            auto token = next_token().value();

            if (token.value == "meta") {
                if (!visibility.empty()) {
                    throw runtime_error("Visibility applies to the class under meta keyword: [public, intern] class");
                }
                index--;
                meta_model = parse_meta_model();
            }
            else if (token.value == "class") {
                if (visibility.empty()) visibility = "public"; // classes are public by default
                index--; // Step back to let parse_class handle 'class'
                auto class_node = parse_class(visibility, meta_model);
                if (defs.find(class_node->name) != defs.end()) {
                    throw runtime_error(class_node->name + " duplicate definition");
                }
                defs[class_node->name] = class_node;
                visibility.clear();
                meta_model.clear();
            }
            else if (token.value == "import") {
                if (!meta_model.empty()) {
                    throw runtime_error("Meta not applicable");
                }
                if (visibility.empty()) visibility = "intern"; // imports are intern by default
                index--; // Step back to let parse_import handle 'import'
                auto import_node = parse_import(visibility);
                if (defs.find(import_node->name) != defs.end()) {
                    throw runtime_error(import_node->name + " duplicate definition");
                }
                defs[import_node->name] = import_node;
                import_node->process(this);
                visibility.clear();
            }
            else if (token.value == "public") {
                visibility = "public";
            }
            else if (token.value == "intern") {
                visibility = "intern";
            }
            else {
                throw runtime_error("Unexpected token in module: " + token.value);
            }
        }

        // Finish parsing classes once all identities are known
        for (const auto& [name, enode] : defs) {
            if (auto eclass = dynamic_cast<EClass*>(enode)) {
                finish_class(eclass);
            }
        }

        pop_token_state();
    }

    unordered_map<string, string> parse_meta_model() {
        consume("meta");
        consume("[");
        unordered_map<string, string> result;
        while (true) {
            auto symbol = next_token().value();
            if (symbol.value == "]") break;
            auto after = peek_token().value();
            string conforms;
            if (after.value == ":") {
                consume(":");
                conforms = next_token().value().value;
            }
            result[symbol.value] = conforms;
            after = peek_token().value();
            if (after.value == ",") {
                consume(",");
            }
            else if (after.value == "]") {
                consume("]");
                break;
            }
            else {
                throw runtime_error("Expected ] after meta symbol:conforms");
            }
        }
        if (result.empty()) {
            throw runtime_error("Meta args required for meta keyword");
        }
        return result;
    }

    void translate(EClass* class_def, EMethod* method) {
        cout << "Translating method " << class_def->name << "::" << method->name << endl;
        assert(!method->body.empty());
        push_token_state(method->body);
        assert(member_stack.size() == 1);

        push_return_type(method->type);

        for (const auto& [name, members] : class_def->members) {
            for (const auto& member : members) {
                if (member->name != "index") {
                    push_member(member);
                }
            }
        }
        push_member_depth();
        current_def = class_def;
        auto self_ident = new EIdent(class_def, this);
        if (!method->static_member) {
            push_member(new EMember("self", self_ident, this, nullptr, nullptr, "public"));
        }
        for (const auto& [name, member] : method->args) {
            push_member(member);
        }

        auto enode = parse_statements();
        assert(dynamic_cast<EStatements*>(enode) != nullptr);

        if (!method->type_expressed && method->type &&
            method->type->get_def() == class_def &&
            !dynamic_cast<EMethodReturn*>(dynamic_cast<EStatements*>(enode)->value.back())) {
            dynamic_cast<EStatements*>(enode)->value.push_back(
                new EMethodReturn(method->type, lookup_stack_member("self"))
            );
        }

        pop_member_depth();
        pop_token_state();
        if (verbose) {
            cout << "Printing enodes for method " << method->name << endl;
            print_enodes(enode);
        }
        assert(member_stack.size() == 1);
        method->code = enode;
    }

    void build_dependencies() {
        assert(!finished);
        for (const auto& [name, obj] : defs) {
            if (auto import = dynamic_cast<EImport*>(obj)) {
                if (import->import_type == EImport::source) {
                    if (!import->main_symbol.empty()) {
                        main_symbols.push_back(import->main_symbol);
                    }
                    for (const auto& source : import->source) {
                        if (source.ends_with(".rs") || source.ends_with(".h")) {
                            continue;
                        }
                        string buf = build_root + "/" + source + ".o";
                        compiled_objects.push_back(buf);
                    }
                    break;
                }
                else if (import->import_type == EImport::library || import->import_type == EImport::project) {
                    libraries_used.insert(libraries_used.end(), import->links.begin(), import->links.end());
                }
            }
        }
    }

    void build() {
        bool is_app = defs.find("app") != defs.end() || !main_symbols.empty();

        filesystem::current_path(build_root);
        auto install = install_dir();

        string cflags = "-Wno-incompatible-pointer-types";
        string compile = "gcc -std=c99 " + cflags + " -I" + build_root + " -I" + install.string() + "/include -c " + name + ".c -o " + name + ".o";
        if (system(compile.c_str()) != 0) {
            throw runtime_error("Compilation failed");
        }

        string link = "gcc" + string(is_app ? "" : " -shared") + " -L" + install.string() + " " + name + ".o " +
            join(compiled_objects, " ") + " " +
            join(libraries_used, " -l") + " -o " + name;
        cout << build_root << " > " << link << endl;
        if (system(link.c_str()) != 0) {
            throw runtime_error("Linking failed");
        }
    }

private:
    string join(const vector<string>& v, const string& delim) {
        string result;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) result += delim;
            result += v[i];
        }
        return result;
    }
};

// Global variables
string build_root;
bool is_debug = false;
bool verbose = false;

// Function to parse boolean arguments
bool parse_bool(const string& v) {
    string s = to_lower(v);
    if (s == "true" || s == "yes" || s == "1") return true;
    if (s == "false" || s == "no" || s == "0") return false;
    throw runtime_error("Invalid boolean format: " + v);
}

int main(int argc, char* argv[]) {
    // Setup arguments
    argparse::ArgumentParser program("silver");
    
    program.add_argument("--verbose")
        .help("Increase output verbosity")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--debug")
        .help("Compile with debug info")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--cwd")
        .help("Path for relative sources (default is current-working-directory)")
        .default_value(filesystem::current_path().string());

    program.add_argument("source")
        .help("Source file path")
        .nargs(argparse::nargs_pattern::at_least_one);

    try {
        program.parse_args(argc, argv);
    }
    catch (const runtime_error& err) {
        cerr << err.what() << endl;
        cerr << program;
        return 1;
    }

    // Update members from args
    verbose = program.get<bool>("--verbose");
    is_debug = program.get<bool>("--debug");
    build_root = program.get<string>("--cwd");

    auto sources = program.get<vector<string>>("source");

    // Print usage if not using
    if (sources.empty()) {
        cout << "silver 0.44" << endl;
        cout << string(32, '-') << endl;
        cout << "Source must be given as file/path/to/silver.si" << endl;
        cout << program;
        return 1;
    }

    // Build the modules provided by user
    for (const auto& src : sources) {
        try {
            EModule module(filesystem::path(src));
            module.parse(module.read_tokens()); // Assuming read_tokens() is implemented to read the file content
            
            // Check for app class, indicating entry point
            auto app = module.defs.find("app") != module.defs.end() ? 
                dynamic_cast<EClass*>(module.defs["app"]) : nullptr;
            
            if (verbose && app) {
                app->print();
            }
            
            if (!module.emit(app)) {
                cerr << "Error during C99 emit" << endl;
                return 1;
            }
            
            // Build the emitted C99 source
            module.build();
        }
        catch (const exception& e) {
            cerr << "Error processing " << src << ": " << e.what() << endl;
            return 1;
        }
    }

    return 0;
}

