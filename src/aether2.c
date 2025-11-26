#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <ports.h>

typedef LLVMMetadataRef LLVMScope;

#include <aether2/import>

Au_t aether2_top_scope(aether2 e) {
    return last_element(e->lexical);
}

none aether2_push_scope(aether2 e, Au_t mem) {
    push(e->lexical, mem);
}

static void import_Au(aether2 e, path lib) {
    e->current_inc   = lib ? lib : path("Au");
    e->is_Au_import  = true;

    string  lib_name = lib ? stem(lib) : null;

    // register new module if this is not global
    if (lib) Au_register_module(copy_cstr(lib_name->chars));

    // get current module (newer one, if registered)
    push(e->lexical, Au_current_module());
    if (lib) {
        // next we dlopen, invoking global-initializers for types
        handle f = dlopen(cstring(lib), RTLD_NOW);
        verify(f, "shared-lib failed to load: %o", lib);
        push(e->libs, f);
    }

    // we can now iterate newly loaded types
    i64 ln;
    Au_t top = top_scope(e);

    for (num i = 0; i < ln; i++) {
        Au_t m = top->members.origin[i]; // Au_t are members, and members can describe types (if is_type) or src-relationship to them
        if (m->member_type == AU_MEMBER_TYPE) {
            print("found type %o", m);
        }
        // module name is namespace in Au, its important this function as a general stack
        // if not, we have to have our own abstract for this (which we are trying to reduce in aether)
    }

    // unregister our import, setting state for default au
    Au_register_module(null); // indicates to Au to go back to its default state (module is registered for our end)
}


none aether2_init(aether2 e) {
    e->libs     = array();
    e->lexical  = array(unmanaged, true, assorted, true);
    import_Au(e, null);
}


define_class(aether2, Au)
