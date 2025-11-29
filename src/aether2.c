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

static Au_t au_arg(Au a) {
    if (isa(a) == typeid(etype))  return ((etype) a)->au;
    if (isa(a) == typeid(enode2)) return ((enode2)a)->etype->au;
    if (!isa(a)) return a;
    fault("unhandled type: %s", isa(a)->ident);
    return null;
}

static bool is_class   (Au t) { return au_arg(t)->is_class;  }
static bool is_struct  (Au t) { return au_arg(t)->is_struct; }
static bool is_func    (Au t) { return au_arg(t)->member_type == AU_MEMBER_FUNC; }
static bool is_imethod (Au t) { return au_arg(t)->member_type == AU_MEMBER_FUNC && au_arg(t)->is_imethod; }
static bool is_rec     (Au t) { return au_arg(t)->is_class || au_arg(t)->is_struct; }
static bool is_prim    (Au t) { return au_arg(t)->is_primitive; }
static bool is_sign    (Au t) { return au_arg(t)->is_signed; }
static bool is_unsign  (Au t) { return au_arg(t)->is_unsigned; }
static bool is_ptr     (Au t) { return au_arg(t)->is_pointer; }
static bool is_enum    (Au t) { return au_arg(t)->is_enum; }
static bool is_type    (Au t) { return au_arg(t)->member_type == AU_MEMBER_TYPE; }

static etype etype_deref(aether2 e, Au_t au) {
    return au->src;
}

static etype etype_ptr(aether2 e, Au_t a) {
    verify(a && !isa((Au)a), "ptr requires Au_t, given %s", isa((Au)a)->ident);
    if (isa(a) == typeid(etype)) return ((etype) a)->au;
    Au_t au = a;
    if (au->ptr) return au->ptr;
    au->ptr              = Au_alloc_member(e->au_module);
    au->ptr->member_type = AU_MEMBER_TYPE;
    au->ptr->is_pointer  = true;
    au->ptr->src         = au;
    au->ptr->user        = etype(mod, e, au, au->ptr);
    return au->ptr->user;
}

Au_t au_lexical(aether2 a, symbol f) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t au = a->lexical->origin[i];
        while (au) {
            for (int ii = 0; ii < au->members.count; ii++) {
                Au_t m = au->members.origin[ii];
                if (strcmp(m->ident, f) == 0)
                    return m;
            }
            if (!is_class(au))     break;
            if (au->context == au) break;
            au = au->context;
        }
    }
    return null;
}

etype etype_lexical(aether2 a, symbol f) {
    Au_t au = au_lexical(a, f);
    return au ? au->user : null;
}

array etype_class_list(etype t) {
    array res = array(32);
    Au_t src = t->au;
    while (src) {
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
    else if (isa(a) == typeid(enode2)) return ((enode2)a)->etype->type;
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

etype etype_create_schema(etype t) {
    if (t->schema) return t->schema;
    t->schema = etype(mod, t->mod, au, t->au, is_schema, true);
    return t->schema;
}

none etype_implement(etype t);

// this is the declare
none etype_init(etype t) {
    aether2 a   = t->mod; // silver's mod will be a delegate to aether2, not inherited
    Au_t    au  = t->au;
    bool  named = au->ident && strlen(au->ident);

    au->member_type = AU_MEMBER_TYPE;
    au->user = hold(t);

    // must create the type-ref here -- however for structures we do not create the members until init
    if (is_rec(t)) {
        t->type = named ? LLVMStructCreateNamed(a->module_ctx, au->ident) : null;
    } else if (is_enum(t)) {
        t->type = lltype(t->au->src ? t->au->src : typeid(i32));
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
        Au_t e_A = au_lexical(a, "Au");
        if (!e_A->user) {
            e_A->user = etype(mod, a, au, e_A);
            etype_implement((etype)e_A->user); // move this to proper place
        }
        LLVMTypeRef param_types[] = { lltype(e_A) };
        LLVMTypeRef hook_type = LLVMFunctionType(lltype(e_A), param_types, 1, 0);
        t->type = LLVMPointerType(hook_type, 0);
    } else if (au == typeid(callback)) {
        Au_t e_A = au_lexical(a, "Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(
            lltype(e_A), param_types, 2, 0);
        t->type = LLVMPointerType(cb_type, 0);
    } else if (au == typeid(callback_extra)) {
        Au_t e_A = au_lexical(a, "Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(e_A->type, param_types, 3, 0);
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
        t->type = LLVMPointerType((typeid(u8)->user)->type, 0);
        au->is_pointer = true;
    }
    else if (au == typeid(handle))   t->type = LLVMPointerType(LLVMInt8Type(), 0);
    else if (au == typeid(ARef)) {
        au->src = typeid(Au);
        au->src->ptr = au;
        t->type = LLVMPointerType(lltype(au_lexical(a, "Au")), 0);
    }
    else if (au == typeid(Au_ts))    t->type = lltype(au_lexical(a, "Au_ts"));
    else if (au == typeid(bf16))     t->type = LLVMBFloatTypeInContext(a->module_ctx);
    else if (au == typeid(fp16))     t->type = LLVMHalfTypeInContext(a->module_ctx);
    else if (au->is_pointer) {
        etype mdl_src  = null;
        cstr  src_name = null;
        string n       = string(au->ident);
        Au_t   src     = au->src;
        src_name       = src->ident;
        mdl_src        = src->user;
        verify(mdl_src && mdl_src->type, "type must be created before %o: %s", n, src_name);
        t->type        = etype_ptr(a, mdl_src->au)->type;

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
}

// this is where we 'finish' creating the type
// 
none etype_implement(etype t) {
    Au_t    au = t->au;
    aether2 a  = t->mod;

    if (is_rec(t)) {
        array cl = etype_class_list(t);
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
        each(cl, etype, tt) {
            if (len(cl) > 1 && tt->au == typeid(Au)) break;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = tt->au->members.origin[i];
                if (m->member_type == AU_MEMBER_PROP) {
                    verify(((etype)m->user)->type, "type not set for %o", m);
                    members[index++] = ((etype)m->user)->type;
                }
            }
        }
        etype mt = null;
        if (is_class(t)) {
            mt = etype_ptr(a, etype_create_schema(t)->au)->type;
        } else if (count == 0) {
             mt = members[count++] = etype_lexical(a, "u8")->type; 
             // if no members, we allow this with a single byte
             // in the case of Au types, this is not added to the 
             // total size of any polymorphic instance
        }
        if (mt) {
            members[count++] = mt;
        }
        LLVMStructSetBody(t->type, members, count, 1);
        t->au->ptr = etype_ptr(a, t->au)->au;

    } else if (is_enum(t)) {
        for (int i = 0; i < au->members.count; i++) {
            Au_t   m = au->members.origin[i]; // has ident, and value set (required)
            verify(m->value, "no value set for enum %s:%s", au->ident, m->ident);
            string n = f(string, "%s_%s", au->ident, m->ident);
            
            LLVMValueRef G = LLVMAddGlobal(a->module, t->type, n->chars);
            LLVMSetLinkage(G, LLVMInternalLinkage);
            LLVMSetGlobalConstant(G, 1);
            LLVMValueRef init;
            
            Au_t et = isa(m->value);
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
}

Au_t aether2_top_scope(aether2 a) {
    return last_element(a->lexical);
}

none aether2_push_scope(aether2 a, Au_t mem) {
    push(a->lexical, mem);
}

static void import_Au(aether2 a, path lib) {
    a->current_inc   = lib ? lib : path("Au");
    a->is_Au_import  = true;

    string  lib_name = lib ? stem(lib) : null;

    // register new module if this is not global
    if (lib) Au_register_module(copy_cstr(lib_name->chars));

    a->au_module = Au_current_module();

    // get current module (newer one, if registered)
    push(a->lexical, a->au_module);
    if (lib) {
        // next we dlopen, invoking global-initializers for types
        handle f = dlopen(cstring(lib), RTLD_NOW);
        verify(f, "shared-lib failed to load: %o", lib);
        push(a->libs, f);
    }

    // we can now iterate newly loaded types; for each, we must create llvm data (which we shall do a-new so we broaden our understanding)
    Au_t top = top_scope(a);
    Au_t au  = typeid(Au);
    au->user = etype(mod, a, au, au);

    Au_t au_t  = typeid(Au_t);
    au_t->user = etype(mod, a, au, au_t);

    // all basic primitives
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_primitive && !m->is_pointer) {
            print("init primitive %o", m);
            m->user = etype(mod, a, au, m);
            etype_implement((etype)m->user);
        }
    }

    // all pointer primitives
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_primitive && m->is_pointer) {
            print("init pointer %o", m);
            m->user = etype(mod, a, au, m);
            etype_implement((etype)m->user);
        }
    }

    // all structs
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_struct) {
            print("init structure %o", m);
            m->user = etype(mod, a, au, m);
        }
    }

    // all classes
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_class) {
            print("init class %o", m);
            m->user = etype(mod, a, au, m);
        }
    }

    // implement structs
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_struct) {
            print("implement structure %o", m);
            etype_implement((etype)m->user);
        }
    }

    // implement classes
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i];
        if (m->is_class) {
            print("implement class %o", m);
            etype_implement((etype)m->user);
        }
    }

    // 
    for (num i = 0; i < top->members.count; i++) {
        Au_t m = top->members.origin[i]; 
        if (m->member_type == AU_MEMBER_TYPE) {
            if (!m->user) {
                print("not initialized: %o", m->user);
            } else if (!((etype)m->user)->type) {
                print("implementing type %o", m);
                etype_implement((etype)m->user);
            }
        }
    }

    Au_register_module(null);
}


none aether2_init(aether2 a) {

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

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

    import_Au(a, null);
}

none aether2_dealloc(aether2 a) {
    LLVMDisposeBuilder  (a->builder);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module);
    LLVMContextDispose  (a->module_ctx);
    LLVMDisposeMessage  (a->target_triple);
}


define_class(aether2, Au)
define_class(enode2,  Au)
define_class(etype,   Au)