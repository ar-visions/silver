#include "common.h"

typedef struct {
    char* spec;
    map_t* module_cache;
} silver_t;

static silver_t app;


// we should not need to give bindings here -- runtime has the stack, and these operations work with that
enode_t* enode_operation(enum enode_type etype, array_t* operands) {
    enode_t* op = calloc(1, sizeof(enode_t));
    op->etype    = etype;
    op->operands = operands;
    return op;
}

enode_t* enode_value(enum enode_type etype, object_t* value) {
    enode_t* op = calloc(1, sizeof(enode_t));
    op->etype    = etype;
    op->value    = value;
    return op;
}



ENode() : A(typeof(ENode)) { }

ENode(ident& id_var) : ENode() {
    etype = Type::Var;
    value = id_var;
}

object_t* lookup(const vector<map> &stack, ident id, bool top_only, bool &found) {
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

    static char* string_interpolate(const object &m_input, const vector<map> &stack) {
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
    
    static object_t* enode_exec(const enode &op, const vector<map> &stack) {
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


char* module_find(char* name) {
    char buf[1024];
    strcpy(buf, app.spec);
    strcat(buf, "/");
    strcat(buf, name);
    strcat(buf, ".si");
    FILE* f = fopen(buf, "r");
    if (f) {
        fclose(f);
        return copy_string(buf, -1);
    }
    return null;
}

mod_t* module_mod(module_t* module, char* name) {
    for (int i = 0, to = module->mods->count; i < to; i++) {
        mod_t* m = array_at(module->mods, i);
        if (strcmp(m->info.name, name) == 0)
            return m;
    }
    return null;
}

enum_t* module_enum(module_t* module, char* name) {
    for (int i = 0, to = module->mods->count; i < to; i++) {
        enum_t* m = array_at(module->enums, i);
        if (strcmp(m->info.name, name) == 0)
            return m;
    }
    return null;
}

struct_t* module_struct(module_t* module, char* name) {
    for (int i = 0, to = module->mods->count; i < to; i++) {
        struct_t* m = array_at(module->structs, i);
        if (strcmp(m->info.name, name) == 0)
            return m;
    }
    return null;
}

module_data_t* module_data(module_t* module, char* name) {
    for (int i = 0, to = module->mods->count; i < to; i++) {
        module_data_t* m = array_at(module->data, i);
        if (strcmp(m->info.name, name) == 0)
            return m;
    }
    return null;
}

enum model_type token_to_model_type(token_t* t) {
    if (token_compare(t, "object-ref")  == 0) return model_ref;
    if (token_compare(t, "signed-8")    == 0) return model_i8;
    if (token_compare(t, "unsigned-8")  == 0) return model_u8;
    if (token_compare(t, "signed-16")   == 0) return model_i16;
    if (token_compare(t, "unsigned-16") == 0) return model_u16;
    if (token_compare(t, "signed-32")   == 0) return model_i32;
    if (token_compare(t, "unsigned-32") == 0) return model_u32;
    if (token_compare(t, "signed-64")   == 0) return model_i64;
    if (token_compare(t, "unsigned-64") == 0) return model_u64;
    if (token_compare(t, "real-16")     == 0) return model_r16;
    if (token_compare(t, "real-32")     == 0) return model_r32;
    if (token_compare(t, "real-64")     == 0) return model_r64;
    assert(false);
    return model_ref;
}

/// type_t needs traits all the same to distinguish enum, struct, mod
type_t* type_lookup(module_t* module, char* name) {
    for (int i = 0, to = module->mods->count; i < to; i++) {
        mod_t* mod = (mod_t*)array_at(module->mods, i);
        if (strcmp(mod->info.name, name) == 0) {
            return &mod->info;
        }
    }
    for (int i = 0, to = module->enums->count; i < to; i++) {
        enum_t* enumerable = (enum_t*)array_at(module->enums, i);
        if (strcmp(enumerable->info.name, name) == 0) {
            return &enumerable->info;
        }
    }
    for (int i = 0, to = module->structs->count; i < to; i++) {
        struct_t* st = (enum_t*)array_at(module->structs, i);
        if (strcmp(st->info.name, name) == 0) {
            return &st->info;
        }
    }
    return null;
}

void args_parse(module_t* module, parser_t* parser, array_t* args) {
    token_t* t = parser_pop(parser);
    assert(token_compare(t, "[") == 0);
    for (;;) {
        token_t* n = parser_pop(parser);
        if (token_compare(t, "]") == 0)
            break;
        assert(n);

        /// for now we just want to support types, arrays and maps -- this is a brevity limitation
        token_t* token_type = parser_pop(parser);
        type_t* type = type_lookup(module, token_type->name);
        assert(type);

        arg_t* arg = (arg_t*)array_push(args);
        arg->type = type;
        
        token_t* token_next = parser_next(parser);
        if (token_isalpha(token_next)) {
            parser_pop(parser);
            /// variable name
            arg->name = copy_string(token_next->name);
        } else {
            /// lets set it to the same [cannot due to named args]
            //arg->name = copy_string(token_type->name);
        }

        n = parser_next(parser);
        if (token_compare(n, ":") == 0) {
            parser_pop(parser);
            
            ///
            expression_parse(parser);

        }
    }
    parser_pop(parser);
}

mod_t* mod_parse(module_t* module, parser_t* parser, bool inlay) {
    token_t* token_name = parser_pop(parser);
    mod_t* mod = (mod_t*)calloc(1, sizeof(mod_t));
    mod->info.name = copy_string(token_name->name, -1);
    mod->info.inlay = inlay;
    token_t* n = parser_next(parser);
    mod->conforms = array_with_sizes(32, sizeof(char*));
    if (token_compare(n, ":") == 0) {
        token_t* n2 = parser_pop(parser);
        if (token_compare(n2, ":") == 0) {
            token_t* token_model = parser_pop(parser);
            mod->model = token_to_model_type(token_model);
        } else {
            mod->inherits = copy_string(n2->name, -1);
        }
        n = parser_pop(parser);
    }
    if (token_compare(n, "conforms") == 0) {
        n = parser_pop(parser);
        array_push_element(mod->conforms, n->name);
        while (token_compare(parser_next(parser), ",") == 0) {
            n = parser_pop(parser);
            n = parser_pop(parser);
            array_push_element(mod->conforms, n->name);
        }
        if (token_compare(n, "[") != 0)
            n = parser_pop(parser);
    }
    assert(token_compare(n, "[") == 0);
    n = parser_pop(parser);

    /// read mod members until we find ]
    /// for now the language will only support array and map types, not an embedding of both
    /// this is for ease of implementation
    for (;;) {
        n = parser_pop(parser);
        assert(n);
        if (token_compare(n, "]") == 0)
            break;
        bool public = false, intern = false;
        if (token_compare(n, "public") == 0) {
            public = true;
            n = parser_pop(parser);
        }
        if (token_compare(n, "intern") == 0) {
            intern = true;
            n = parser_pop(parser);
        }
        member_t* member = (member_t*)calloc(1, sizeof(member_t));
        member->args = array_with_sizes(32, sizeof(arg_t));
        member->type = type_lookup(module, n->name);
        if (!member->type) {
            /// if constructor
            if (token_compare(n, "construct") == 0) {
                /// parse args
            } else if (token_compare(n, "dealloc") == 0) {
                /// these will be called in chain, its not a user controlled thing
                /// expect []
            }
        } else {
            /// can be a variable, lambda or method
            ///
            /// can be operator expression
            /// type operator [ args ] 
            n = parser_pop(parser);
            if (token_compare(n, "operator") == 0) {
                args_parse(parser, member->args);
                // we can parse args from here; realize [args, can be multiple] in operators; 
                // these are just methods without names.. no names!  we are nameless!
            }
            ///
            /// can be a method
            /// type method-name [ args ] [ optional ]
            /// 
            /// can be data with optional assignment
            /// type data-name [: optional]
            /// 
            /// lambas distinct from methods by assignment 
            /// type method-name [ args ] : [ optional ] or null
        }
    }
    
    return mod;
}

/// entry function for parsing modules into tokens, and organizing into module -> mods/enums/structs
module_t* module_load(char* abs_path) {

    /// map_t is based on char* identity keys
    if (!app.module_cache) app.module_cache = map_with_size(32);
    module_t* m = (module_t*)map_get(app.module_cache, abs_path);
    if (m) return m;

    // module_cache has abs-path and module_t*
    module_t* module = calloc(1, sizeof(module_t));
    
    module->enums    = array_with_sizes(32, sizeof(enum_t*));
    module->mods     = array_with_sizes(32, sizeof(mod_t*));
    module->structs  = array_with_sizes(32, sizeof(struct_t*));
    module->data     = array_with_sizes(32, sizeof(module_data_t*));
    module->imports  = array_with_sizes(32, sizeof(module_t*)); /// we require singleton identity on module_t, as we are not loading the same module twice
    module->tokens   = tokenize(abs_path); // ident_t
    parser_t* parser = parser_with_tokens(module->tokens);
    module->abs_path = abs_path;
    token_t* n;
    while (n = parser_pop(parser)) {
        bool inlay = false;
        if (token_compare(n, "inlay")) { inlay = true; n = parser_pop(parser); };
        if (token_compare(n, "import") == 0) {
            assert(!inlay);
            // import silver  or 
            // not required at the moment:
            // import [name, source:'url-here', shell:'build string', links:[lib1,lib2], includes:<include1.h, include2.h>, defines:[X: 1]]
            // we want third party imports to be using import keyword, albeit with a bit more args
            // we look in the user folder first, then we can look in system
            token_t* module_token = parser_pop(parser);
            char*    module_path  = module_find(module_token->name); assert(module_path);
            module_t* next_module = module_load(module_path);
            array_push_element(module->imports, next_module);
        } else if (token_compare(n, "mod")    == 0) {
            mod_t* mod = mod_parse(module, parser, inlay);
            array_push_element(module->mods, mod);
        } else if (token_compare(n, "enum")   == 0) {
        } else if (token_compare(n, "proto")  == 0) {
        } else {
            assert(false);
        }
        /// coding silver in C99 represents coding in such a way that rivals the idea of something ref counting at all
    }

    map_set(app.module_cache, abs_path, module);
    /// for each import, we module_load; 
    /// crawl through tokens, import modules first
    return module;
}

/// assert for module implementation
int test(char* module_name) {
    char* source_path = resolve_path(module_name);
    assert(source_path);
    module_t* module = module_load(source_path);
    assert(module_mod(module, "i8"));
    assert(module_mod(module, "quad"));
    assert(module_mod(module, "import"));
    assert(module_struct(module, "struct-test"));
    assert(module_data(module, "some-module-data"));
    free(source_path);
    return 0;
}

int main(int argc, char** argv) {
    app.spec = "/home/kalen/src/silver/silver/res/spec";
    assert(argc >= 2);
    char* source = argv[1];
    /// get spec from the command line
    /// get full path from source
    return test(source);
}