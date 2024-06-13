#include "common.h"

typedef struct {
    char* spec;
    map_t* module_cache;
} silver_t;

static silver_t app;

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

            token_t* name = parser_pop(parser);
            char*    module_path  = module_find(name->name); assert(module_path);
            module_t* next_module = module_load(module_path);
            array_push_element(module->imports, next_module);

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