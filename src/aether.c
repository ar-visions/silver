#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <ports.h>

typedef LLVMMetadataRef LLVMScope;

#include <aether/import>

#define au_lookup(sym) Au_lexical(a->lexical, sym)

typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;

none bp() {
    return;
}


void aether_push_tokens(aether a, tokens t, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = Au_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    tokens_data* state_saved = (tokens_data*)last_element(a->stack);
    a->tokens = hold(t);
    a->cursor = cursor;
}


void aether_pop_tokens(aether a, bool transfer) {
    int len = a->stack->count;
    assert (len, "expected stack");
    tokens_data* state = (tokens_data*)last_element(a->stack); // we should call this element or ele
    
    if(!transfer)
        a->cursor = state->cursor;
    else if (transfer && state->tokens != a->tokens) {
        /// transfer implies we up as many tokens
        /// as we traveled in what must be a subset
        a->cursor += state->cursor;
    }

    if (state->tokens != a->tokens) {
        drop(a->tokens);
        a->tokens = state->tokens;
    }
    pop(a->stack);
}

void aether_push_current(aether a) {
    push_tokens(a, a->tokens, a->cursor);
}

static int ref_level(Au t) {
    Au_t a = au_arg(t);
    Au_t src = a;
    int  level = 0;
    while (src) {
        if (src->is_pointer)
            level++;
        src = src->src;
    }
    return level;
}

static etype etype_deref(Au_t au) {
    return au->src ? au->src->user : null;
}

static etype etype_ptr(aether e, Au_t a) {
    verify(a && !isa((Au)a), "ptr requires Au_t, given %s", isa((Au)a)->ident);
    if (isa(a) == typeid(etype)) return ((etype) a)->au;
    Au_t au = a;
    if (au->ptr) return au->ptr;
    au->ptr              = Au_register(e->au, null, AU_MEMBER_TYPE, 0);
    au->ptr->is_pointer  = true;
    au->ptr->src         = au;
    au->ptr->user        = etype(mod, e, au, au->ptr);
    return au->ptr->user;
}

void src_init(aether a, Au_t m) {
    if (m->src)
        src_init(a, m->src);
    if (!m->user)
        m->user = etype(mod, a, au, m);
}

array etype_class_list(etype t) {
    aether a = t->mod;
    array res = array(32);
    Au_t src = t->au;
    while (src) {
        src_init(a, src);
        verify(src->user, "etype (user) not set for %s", src->ident);
        push(res, src->user);
        if (src->context == src)
            break;
        src = src->context;
    }
    return reverse(res);
}

// does switcher-oo for class type
LLVMTypeRef lltype(Au a) {
    Au_t au;
    if (isa(a) == typeid(etype)) {
        verify(((etype) a)->type, "no type found on %s", ((etype)a)->au->ident);
        au = ((etype) a)->au;
    }
    else if (isa(a) == typeid(enode)) return ((enode)a)->t->type;
    else if (!isa(a)) {
        verify(((etype)(((Au_t)a)->user))->type,
            "no type found on %s", ((Au_t)a)->ident);
        au = a;
    }
    verify(au, "unhandled input");
    if (au->is_class) {
        verify(au->ptr, "expected ptr for class");
        au = au->ptr;
    }
    return ((etype)au->user)->type;
}

Au_t arg_type(Au_t au) {
    Au_t res = au;
    while (res) {
        if (res->member_type != AU_MEMBER_TYPE) {
            res = res->type;
            continue;
        }
        break;
    }
    verify(res, "argument not resolving type: %s", au);
    return res;
}

none etype_implement(etype t);

etype etype_resolve(etype t) {
    Au_t au = t->au;
    while (au && au->src) {
        au = au->src;
        if (au->user && au->user->type)
            break;
    }
    return (au->user && au->user->type) ? au->user : null;
}

// this is the declare (this comment stays)
none etype_init(etype t) {
    aether a   = t->mod; // silver's mod will be a delegate to aether, not inherited
    Au_t    au  = t->au;
    bool  named = au && au->ident && strlen(au->ident);

    if (isa(t) == typeid(aether) || isa(t)->context == typeid(aether)) {
        verify(a->source && len(a->source), "no source provided");
        string name = stem(a->source);
        t->au = Au_register_module(name->chars);
    } else if (t->is_schema) {
        au = t->au = Au_register(a->au, fmt("__%s_t", au->ident)->chars,
            AU_MEMBER_TYPE, AU_TRAIT_SCHEMA | AU_TRAIT_STRUCT);

        // do this for Au types
        Au_t ref = au_lookup("Au_t");
        for (int i = 0; i < ref->members.count; i++) {
            Au_t au_mem  = ref->members.origin[i];

            // this is the last member (function table), if that changes, we no longer break
            if (au_mem->ident && strcmp(au_mem->ident, "ft") == 0) {
                Au_t new_ft = Au_register(au->schema, "ft", AU_MEMBER_TYPE, AU_TRAIT_STRUCT);

                array cl = etype_class_list(t);
                each (cl,  etype, tt) {
                    for (int ai = 0; ai < tt->au->members.count; ai++) {
                        Au_t ai_mem = tt->au->members.origin[ai];
                        if (ai_mem->member_type != AU_MEMBER_FUNC)
                            continue;
                        
                        Au_t fn = Au_register(new_ft, null, AU_MEMBER_FUNC, AU_TRAIT_FUNCPTR);
                        fn->traits = ai_mem->traits;
                        fn->rtype  = ai_mem->rtype;

                        for (int arg = 0; arg < ai_mem->args.count; arg++) {
                            Au_t arg_src = ai_mem->args.origin[arg];
                            Au_t arg_t   = arg_type(arg_src);
                            array_qpush(&fn->args, arg_t);
                        }
                    }
                }
                break;
            }
            Au_t new_mem = Au_register(t->au, au_mem->ident, au_mem->member_type, au_mem->traits);
            *new_mem = *au_mem; // i never trust these things, or myself to remember the members
            new_mem->context = t->au; // copy entire member and reset for our for context
        }
    }
    
    au->member_type = AU_MEMBER_TYPE;
    au->user = hold(t);

    if (au && au->is_pointer && au->src && !au->src->is_primitive) {
        t->type = LLVMPointerType(au->src->user->type, 0);
    } else if (is_rec(t) || au->is_union || au == typeid(Au_t)) {
        t->type = named ? LLVMStructCreateNamed(a->module_ctx, au->ident) : null;
    } else if (is_enum(t)) {
        t->type = lltype(au->src ? au->src : typeid(i32));
    }
    else if (au == typeid(f32))  t->type = LLVMFloatType();
    else if (au == typeid(f64))  t->type = LLVMDoubleType();
    else if (au == typeid(f80))  t->type = LLVMX86FP80Type();
    else if (au == typeid(none)) t->type = LLVMVoidType  ();
    else if (au == typeid(bool)) t->type = LLVMInt1Type  ();
    else if (au == typeid(i8)  || au == typeid(u8))
        t->type = LLVMInt8Type();
    else if (au == typeid(i16) || au == typeid(u16))
        t->type = LLVMInt16Type();
    else if (au == typeid(i32) || au == typeid(u32) || au == typeid(AFlag))
        t->type = LLVMInt32Type();
    else if (au == typeid(i64) || au == typeid(u64) || au == typeid(num))
        t->type = LLVMInt64Type();
    else if (au == typeid(symbol) || au == typeid(cstr) || au == typeid(raw)) {
        t->type = LLVMPointerType(LLVMInt8Type(), 0);
    } else if (au == typeid(cstrs)) {
        t->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
    } else if (au == typeid(sz)) {
        t->type = LLVMIntPtrTypeInContext(a->module_ctx, a->target_data);
    } else if (au == typeid(cereal)) {
        LLVMTypeRef cereal_type = LLVMStructCreateNamed(a->module_ctx, "cereal");
        LLVMTypeRef members[] = {
            LLVMPointerType(LLVMInt8Type(), 0)  // char* → i8*
        };
        LLVMStructSetBody(cereal_type, members, 1, 1);
        t->type = cereal_type;
    } else if (au == typeid(floats)) {
        t->type = LLVMPointerType(LLVMFloatType(), 0);
    } else if (au == typeid(func)) {
        LLVMTypeRef fn_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
        t->type = LLVMPointerType(fn_type, 0);
    } else if (au == typeid(hook)) {
        Au_t e_A = au_lookup("Au");
        if (!e_A->user) {
            e_A->user = etype(mod, a, au, e_A);
            etype_implement((etype)e_A->user); // move this to proper place
        }
        LLVMTypeRef param_types[] = { lltype(e_A) };
        LLVMTypeRef hook_type = LLVMFunctionType(lltype(e_A), param_types, 1, 0);
        t->type = LLVMPointerType(hook_type, 0);
    } else if (au == typeid(callback)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(
            lltype(e_A), param_types, 2, 0);
        t->type = LLVMPointerType(cb_type, 0);
    } else if (au == typeid(callback_extra)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(lltype(e_A), param_types, 3, 0);
        t->type = LLVMPointerType(cb_type, 0);
    }
    else if (au == typeid(ref_u8)  || au == typeid(ref_u16) || 
             au == typeid(ref_u32) || au == typeid(ref_u64) || 
             au == typeid(ref_i8)  || au == typeid(ref_i16) || 
             au == typeid(ref_i32) || au == typeid(ref_i64) || 
             au == typeid(ref_f32) || au == typeid(ref_f64) || 
             au == typeid(ref_bool)) {
        verify(au->src, "expected src on reference type");
        Au_t b  = au->src;
        b->ptr  = t;
        au->src = b;
        au->src->ptr = au;
        t->type = LLVMPointerType(b->user->type, 0);
        au->is_pointer = true;
    }
    else if (au == typeid(handle))   t->type = LLVMPointerType(LLVMInt8Type(), 0);
    else if (au == typeid(ARef)) {
        au->src = typeid(Au);
        au->src->ptr = au;
        Au_t au_type = au_lookup("Au");
        t->type = LLVMPointerType(lltype(au_type), 0);
    }
    else if (au == typeid(Au_ts))    t->type = lltype(au_lookup("Au_ts"));
    else if (au == typeid(bf16))     t->type = LLVMBFloatTypeInContext(a->module_ctx);
    else if (au == typeid(fp16))     t->type = LLVMHalfTypeInContext(a->module_ctx);
    else if (au->is_pointer) {
        if (!au->src) {
            t->type        = LLVMPointerType(LLVMVoidType(), 0);
        } else {
            etype mdl_src  = null;
            cstr  src_name = null;
            string n       = string(au->ident);
            Au_t   src     = au->src;
            src_name       = src->ident;
            verify(src->user && src->user->type, "type must be created before %o: %s", n, src_name);
            t->type        = etype_ptr(a, src->user->au)->type;
        }
    } else if ((au->traits & AU_TRAIT_ABSTRACT) == 0) {
        if (au->member_type == AU_MEMBER_FUNC) {
            int          is_inst   = au->is_imethod;
            int          arg_count = is_inst + au->args.count;
            LLVMTypeRef* arg_types = calloc(arg_count, sizeof(LLVMTypeRef));

            if (is_inst)
                arg_types[0] = lltype(au->context);
            for (int i = is_inst; i < arg_count; i++)
                arg_types[i] = lltype(au->args.origin[i - is_inst]);

            t->type = LLVMFunctionType(
                lltype(au->rtype), arg_types, arg_count, 0);
            free(arg_types);
        }
    } else {
        fault("not intializing %s", au->ident);
    }

    push(a->registry, t); // we only 'clear' our registry and the linked Au_t's
}

none etype_implement(etype t) {
    Au_t    au = t->au;
    aether a  = t->mod;

    if (au && au->src && au->src->user)
        etype_implement(au->src->user);

    if (t->is_implemented) return;
    t->is_implemented = true;

    if (is_rec(t) || au->is_union) {
        array cl = (au->is_union || is_struct(t)) ? array_of(t, 0) : etype_class_list(t);
        int count = 0;
        int index = 0;
        each(cl, etype, tt) {
            if (len(cl) > 1 && tt->au == typeid(Au)) break;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = tt->au->members.origin[i];
                if (m->member_type == AU_MEMBER_PROP)
                    count++;
            }
        }

        LLVMTypeRef* members = calloc(count + 1, sizeof(LLVMTypeRef));
        LLVMTypeRef largest = null;
        int ilargest = 0;
        each(cl, etype, tt) {
            if (len(cl) > 1 && tt->au == typeid(Au)) break;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = tt->au->members.origin[i];
                if (m->member_type == AU_MEMBER_PROP) {
                    Au_t src = m->src;
                    verify(src, "no src type set for member %o", m);
                    src_init(a, src);
                    etype_implement(src->user);
                    // get largest union member
                    verify(resolve(src->user), "type not set for %o", m);
                    members[index] = resolve(src->user)->type;
                    if (au->is_union && src->user->size_bits > ilargest) {
                        largest  = members[index];
                        ilargest = src->user->size_bits;
                    }
                    index++;
                }
            }
        }
        etype mt = null;
        if (is_class(t) && (t->au->traits & AU_TRAIT_SCHEMA) == 0) {
            printf("creating schema for %s\n", t->au->ident);
            etype schema = etype(mod, a, au, t->au, is_schema, true);
            print("schema output for %o", schema);
            for (int i = 0; i < schema->au->members.count; i++) {
                Au_t m = schema->au->members.origin[i];
                if (m->member_type == AU_MEMBER_PROP) {
                    if (m->size)
                        printf("\t%s %s[%i]\n", m->src->ident, m->ident, m->size);
                    else
                        printf("\t%s %s\n", m->src->ident, m->ident);
                }
            }
            mt = etype_ptr(a, schema->au);
        } else if (count == 0)
             mt = au_lookup("u8")->user;
        if (mt) members[count++] = mt->type;
        if (au->is_union) {
            count = 1;
            members[0] = largest;
        }
        LLVMStructSetBody(t->type, members, count, 1);
        etype_ptr(a, t->au);

    } else if (is_enum(t)) {
        Au_t et = t->au->src;
        verify(et, "expected source type for enum %s", t->au);
        for (int i = 0; i < au->members.count; i++) {
            Au_t   m = au->members.origin[i]; // has ident, and value set (required)
            verify(m->value, "no value set for enum %s:%s", au->ident, m->ident);
            string n = f(string, "%s_%s", au->ident, m->ident);
            
            LLVMValueRef G = LLVMAddGlobal(a->module, t->type, n->chars);
            LLVMSetLinkage(G, LLVMInternalLinkage);
            LLVMSetGlobalConstant(G, 1);
            LLVMValueRef init;
            
            if (et == typeid(i32))
                init = LLVMConstInt(t->type, *((i32*)m->value), 0);
            else if (et == typeid(u32))
                init = LLVMConstInt(t->type, *((u32*)m->value), 0);
            else if (et == typeid(i16))
                init = LLVMConstInt(t->type, *((i16*)m->value), 0);
            else if (et == typeid(u16))
                init = LLVMConstInt(t->type, *((u16*)m->value), 0);
            else if (et == typeid(i8))
                init = LLVMConstInt(t->type, *((i8*)m->value), 0);
            else if (et == typeid(u8))
                init = LLVMConstInt(t->type, *((u8*)m->value), 0);
            else if (et == typeid(i64))
                init = LLVMConstInt(t->type, *((i64*)m->value), 0);
            else if (et == typeid(u64))
                init = LLVMConstInt(t->type, *((u64*)m->value), 0);
            else 
                fault("unsupported enum value: %s", et->ident);
            
            LLVMSetInitializer(G, init);
        }
    }

    if (!au->is_void && t->type) {
        if (au->size) {
            t->type = LLVMArrayType(t->type, au->size);
        }
        t->size_bits  = LLVMABISizeOfType(a->target_data, t->type)      * 8;
        t->align_bits = LLVMABIAlignmentOfType(a->target_data, t->type) * 8;
    }
}

Au_t aether_top_scope(aether a) {
    return last_element(a->lexical);
}

none aether_push_scope(aether a, Au arg) {
    Au_t au = au_arg(arg);
    push(a->lexical, au);
}

void aether_import_models(aether a) {

    // initialization table for has/not bits, controlling init and implement
    struct filter {
        bool init, impl;
        u32 has_bits;
        u32 not_bits;
    } filters[8] = {
        { true,  true,  AU_TRAIT_PRIMITIVE, AU_TRAIT_POINTER | AU_TRAIT_FUNCPTR },
        { true,  true,  AU_TRAIT_PRIMITIVE | AU_TRAIT_POINTER, 0 },
        { true,  true,  AU_TRAIT_PRIMITIVE | AU_TRAIT_FUNCPTR, 0 },
        { true,  true,  AU_TRAIT_ENUM,   0 },
        { true,  false, AU_TRAIT_UNION,  0 },
        { true,  false, AU_TRAIT_STRUCT, 0 },
        { true,  false, AU_TRAIT_CLASS,  0 },
        { false, true,  0, AU_TRAIT_ABSTRACT },
    };

    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        for (int filter = 0; filter < 8; filter++) {
            struct filter* ff = &filters[filter];
            for (num i = 0; i < ctx->members.count; i++) {
                Au_t m = ctx->members.origin[i];
                bool proceed = (ff->has_bits & m->traits) == ff->has_bits && 
                            (ff->not_bits & m->traits) == 0;
                if (proceed) {
                    Au_t m_isa = isa(m);
                    print("init %o", m);
                    if (ff->init || ff->impl)
                        src_init(a, m);
                    if (ff->impl) {
                        Au_t au = cast(Au_t, m->user);
                        etype_implement((etype)m->user);
                    }
                }
            }
        }
    }
}

static void import_Au(aether a, path lib) {
    a->current_inc   = lib ? lib : path("Au");
    a->is_Au_import  = true;
    string  lib_name = lib ? stem(lib) : null;

    // register and push new module scope if we are loading from library
    if (lib) Au_register_module(copy_cstr(lib_name->chars));
    Au_t current = Au_current_module();
    if (current != a->au)
        push_scope(a, current);

    Au_t base = au_lookup("Au_t");

    if (lib) {
        handle f = dlopen(cstring(lib), RTLD_NOW);
        verify(f, "shared-lib failed to load: %o", lib);
        push(a->libs, f);
    }

    Au_t au    = typeid(Au);
    au->user   = etype(mod, a, au, au);
    Au_t au_t  = typeid(Au_t);
    au_t->user = etype(mod, a, au, au_t);

    Au_t cur_mod = Au_current_module();
    for (int i = 0; i < cur_mod->members.count; i++) {
        Au_t m = cur_mod->members.origin[i];
        if (m == au) {
            print("found au");
        }
        if (m == au_t)
            print("found au_t");
    }

    etype_ptr(a, au);
    etype_ptr(a, au_t);

    aether_import_models(a);

    Au_register_module(null);
}

none aether_init(aether a) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    if ( a->source) a->source = absolute(a->source);
    if (!a->install) {
        cstr import = getenv("IMPORT");
        if (import)
            a->install = f(path, "%s", import);
        else {
            path   exe = path_self();
            path   bin = parent_dir(exe);
            path   install = absolute(f(path, "%o/..", bin));
            a->install = install;
        }
    }
    a->stack = array(16);
    a->include_paths    = a(f(path, "%o/include", a->install));
    a->sys_inc_paths    = a();
    a->sys_exc_paths    = a();
#ifdef _WIN32
    a->sys_inc_paths = a(
        f(path, "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/include"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/ucrt"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/shared"));
    a->lib_paths = a(
        f(path, "%o/bin"),
        f(path, "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/lib/x64"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/ucrt/x64"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"));
#elif defined(__linux)
    a->sys_inc_paths = a(f(path, "/usr/include"), f(path, "/usr/include/x86_64-linux-gnu"));
    a->lib_paths     = a();
#elif defined(__APPLE__)
    string sdk          = run("xcrun --show-sdk-path");
    string toolchain    = f(string, "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain"); // run("xcrun --show-toolchain-path");
    a->isystem          =   f(path, "%o/usr/include", toolchain);
    a->sys_inc_paths    = a(f(path, "%o/usr/include", toolchain),
                            f(path, "%o/usr/local/include", sdk),
                            f(path, "%o/usr/lib/clang/14.0.3/include", toolchain));
    a->sys_exc_paths    = a(f(path, "%o/usr/include", sdk),
                            f(path, "%o/usr/include", toolchain));
    a->lib_paths        = a(f(path, "%o/usr/lib", sdk));
    a->framework_paths  = a(f(path, "%o/System/Library/Frameworks", sdk));
    a->isysroot         =   f(path, "%o/", sdk);
    a->resource_dir     =   f(path, "%o/usr/lib/clang/14.0.3", toolchain);
#endif

    //push(a->include_paths, f(path, "%o/lib/clang/22/include", a->install));
    push(a->lib_paths, f(path, "%o/lib", a->install));
    path src_path = parent_dir(a->source);
    push(a->include_paths, src_path);

    a->registry       = array(256);
    a->libs           = array();
    a->lexical        = array(unmanaged, true, assorted, true);
    a->module         = LLVMModuleCreateWithName(a->name->chars);
    a->module_ctx     = LLVMGetModuleContext(a->module);
    a->dbg_builder    = LLVMCreateDIBuilder(a->module);
    a->builder        = LLVMCreateBuilderInContext(a->module_ctx);
    a->target_triple  = LLVMGetDefaultTargetTriple();
    cstr err = NULL;
    if (LLVMGetTargetFromTriple(a->target_triple, &a->target, &err))
        fault("error: %s", err);
    a->target_machine = LLVMCreateTargetMachine(
        a->target, a->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    a->target_data = LLVMCreateTargetDataLayout(a->target_machine);
    a->compile_unit = LLVMDIBuilderCreateCompileUnit(
        a->dbg_builder, LLVMDWARFSourceLanguageC, a->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);
    a->scope = a->compile_unit;
    a->builder = LLVMCreateBuilderInContext(a->module_ctx);

    // push our module space to the scope
    Au_t g = Au_global();
    verify(g,           "globals not registered");
    verify(a->au,       "no module registered for aether");
    verify(g != a->au,  "aether using global module");

    push_scope(a, g);
    push_scope(a, a->au);
    import_Au(a, null);

    aclang_cc instance;
    path i = include(a, string("stdio.h"), &instance);
    print("included: %o", i);
}

none aether_dealloc(aether a) {
    LLVMDisposeBuilder  (a->builder);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module);
    LLVMContextDispose  (a->module_ctx);
    LLVMDisposeMessage  (a->target_triple);
}

Au_t etype_cast_string(etype t) {
    return t->au ? string(t->au->ident) : string("[no-type]");
}

Au_t etype_cast_Au_t(etype t) {
    return t->au;
}

Au_t enode_cast_Au_t(enode e) {
    return e->t->au;
}

array read_arg(array tokens, int start, int* next_read);

static array expand_tokens(aether a, array tokens, map expanding) {
    int ln = len(tokens);
    array res = array(alloc, 32);

    int skip = 1;
    for (int i = 0; i < ln; i += skip) {
        skip    = 1;
        token token_a = tokens->origin[i];
        token token_b = ln > (i + 1) ? tokens->origin[i + 1] : null;
        int   n = 2; // after b is 2 ahead of token_a

        if (token_b && eq(token_b, "##")) {
            if  (ln <= (i + 2)) return null;
            token c = tokens->origin[i + 2];
            if  (!c) return null;
            token  aa = token(alloc, len(token_a) + len(c) + 1);
            concat(aa, a);
            concat(aa, c);
            token_a = aa;
            n = 4;
            token_b = ln > (i + 3) ? tokens->origin[i + 3] : null; // can be null
            skip += 2;
        }

        // see if this token is a fellow macro
        macro m = au_lookup(token_a->chars);
        string mname = cast(string, m);
        if (m && token_b && eq(token_b, "(") && !get(expanding, mname)) {
            array args  = array(alloc, 32);
            int   index = i + n + 1;

            while (true) {
                int  stop = 0;
                array arg = read_arg(tokens, index, &stop);
                if (!arg)
                    return null;
                
                skip += len(arg) + 1; // for the , or )
                push(args, arg);
                token  after = tokens->origin[stop];
                if (eq(after, ")")) {
                    index++;
                    break;
                }
            }

            set(expanding, mname, _bool(true));
            array exp = macro_expand(m, args, expanding);
            rm(expanding, mname);

            concat(res, exp);
        } else
            push(res, a);
    }

    return res;
}

static void print_all(aether mod, symbol label, array list) {
    print("[%s] tokens", label);
    each(list, token, t)
        put("%o ", t);
    put("\n");
}

array macro_expand(macro m, array args, map expanding) {
    // we want args with const expression evaluated already from its host language silver
    aether e = m->mod;

    // no need for string scanning within args, since tokens are isolated
    int   ln_args   = len(args);
    array r         = array(alloc, 32);
    array args_exp  = array(alloc, 32);
    int   ln_params = len(m->params);

    // if we dont provide enough args, thats invalid.. 
    // if we provide too many, it may be checked as a var-arg
    if (ln_args < ln_params || (ln_args != ln_params && !m->va_args)) return null;

    // now its a simple replacement within the definition
    array initial = array(alloc, 32);
    each(m->def, token, t) {
        print("token: %o", t);
        // replace directly in here
        bool found = false;
        for (int param = 0; param < ln_params; param++) {
            if (compare(t, m->params->origin[param]) == 0) {
                concat(initial, args->origin[param]);
                found = true;
                break;
            }
        }
        if (!found)
            push(initial, t);
    }

    if (!expanding)
         expanding = map(hsize, 16, assorted, true, unmanaged, true);

    set(expanding, string(m->au->ident), _bool(true));

    // once replaced, we expand those as a flat token list
    print_all(e, "initial", initial);
    array rr = expand_tokens(e, initial, expanding);
    print_all(e, "expand", rr);
    string tstr = string(alloc, 64);
    each (rr, token, t) { // since literal processing is in token_init we shouldnt need this!
        if (tstr->count > 0)
            append(tstr, " ");
        concat(tstr, t);
    }
    return (array)tokens(
        target, e, input, tstr, parser, e->parse_f);
        // we call this to make silver compatible tokens
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, Au_t f, array query) {
    fault("must subclass codegen for usable code generation");
    return null;
}

define_class(etype,      Au)
define_class(aether,     etype)
define_class(macro,      etype)
define_class(enode,      Au)
define_class(aclang_cc,  Au)
define_class(codegen,    Au)
