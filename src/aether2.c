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

static void Au_import_members(aether2 e, path lib) {
    e->current_inc  = lib ? lib : path("Au");
    e->is_Au_import = true;
    string lib_name = lib ? stem(lib) : null;
    push(e->libs, lib_name ? lib_name : string("Au"));
    map module_filter = map();

    if (lib) set(module_filter, lib_name, _bool(true));

    Au_t mod = lib ? Au_register_module(copy_cstr(lib_name->chars)) : null; //Au_module(name->chars);
    
    handle f = lib ? dlopen(cstring(lib), RTLD_NOW) : null;
    verify(!lib || f, "shared-lib failed to load: %o", lib);

    path inc = lib ? lib : path_self();

    if (f) {
        push(e->libs, f);
        Au_engage(null); // load Au-types by finishing global constructor ordered calls
    }

    i64 ln;
    Au_t* a = Au_types(&ln);
    //structure _Au_t = emodel("_Au_t"); // Au type must define these in its core; all primitive types to be defined there manually (not here!)
    map processing = map(hsize, 64, assorted, true, unmanaged, true);
    
    for (num i = 0; i < ln; i++) {
        Au_t mem = a[i]; // everything is a member!

    }

    Au_register_module(null); // indicates to Au to go back to its default state (module is registered for our end)
}


none aether2_init(aether2 e) {
    e->libs = array();
    Au_import_members(e, null);
}
