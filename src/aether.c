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

#define emodel(N)     ({            \
    emember  m = lookup2(e, string(N), null); \
    model mdl = m ? m->mdl : null;  \
    mdl;                            \
})

#define elookup(N)     ({ \
    emember  m = lookup2(e, string(N), null); \
    m; \
})

#define function_lookup(M, N) ({ \
    push(e, M); \
    emember m = lookup2(e, string(N), null); \
    pop(e); \
    m ? (function)m->mdl : (function)null; \
}) \

#define emem(M, META, N) emember(mod, e, name, string(N), mdl, M, meta, META)

#define earg(AA, M, META, N) emember(mod, e, name, string(N), mdl, M, is_arg, true, context, AA)

#define value(m,META,vr) enode(mod, e, value, vr, mdl, m, meta, META)


/// this is more useful as a primitive, to include actual Au-type in aether's primitive abstract
Au_t model_primitive(model mdl) {
    while (mdl) {
        if (mdl->atype_src)
            return mdl->atype_src;
        mdl = mdl->src;
    }
    return null;
}

// for handling aliasing
model model_resolve(model f) {
    if (isa(f) == typeid(Class) || isa(f) == typeid(uni) || isa(f) == typeid(structure) || isa(f) == typeid(enumeration))
        return f;
    while (instanceof(f->src, typeid(model))) {
        if (f->is_ref)
            return f;
        f = f->src;
    }
    return f;
}

static bool is_bool     (model f) { f = model_resolve(f); return f->src && model_primitive(f) == typeid(bool); }
static bool is_float    (model f) { f = model_resolve(f); return f->src && model_primitive(f) == typeid(f32);  }
static bool is_double   (model f) { f = model_resolve(f); return f->src && model_primitive(f) == typeid(f64);  }

static bool is_realistic(model f) {
    f = model_resolve(f);
    Au_t pr = model_primitive(f);
    return pr && pr->traits & AU_TRAIT_REALISTIC;
}

static bool is_integral (model f) {
    f = model_resolve(f);
    Au_t pr = model_primitive(f);
    return pr && pr->traits & AU_TRAIT_INTEGRAL;
}

static bool is_signed   (model f) {
    f = model_resolve(f);
    Au_t pr = model_primitive(f);
    return pr && pr->traits & AU_TRAIT_SIGNED;
}

bool is_unsigned (model f) {
    f = model_resolve(f);
    Au_t pr = model_primitive(f);
    return pr && pr->traits & AU_TRAIT_UNSIGNED;
}

bool is_primitive(model f) {
    Au_t t = model_primitive(f);
    return t && t->traits & AU_TRAIT_PRIMITIVE;
}

bool is_void(model f) {
    f = model_resolve(f); 
    return f ? f->size_bits == 0 : false;
}

bool _is_generic(model f) {
    f = model_resolve(f);
    return (Au_t)f->src == typeid(Au);
}

model _is_class(model f) {
    if (f->src && isa(f->src) == typeid(Class))
        return f;
    if (isa(f) == typeid(Class)) {
        verify(f->ptr, "expected ptr identity");
        return f->ptr;
    }
    return null;
}
model _is_struct(model f);

model _is_record(model f) {
    model ff = _is_class(f);
    if   (ff) return ff;
    return _is_struct(f);
}



bool _is_subclass(model a, model b) {
    if (_is_class(a) && _is_class(b)) {
        model aa = a;
        model bb = b;
        while (aa) {
            if (aa == bb) return true;
            aa = aa->parent;
        }
    }
    return false;
}

model _is_struct(model f) {
    f = model_resolve(f); 
    return isa(f) == typeid(structure) ? f : null;
}

bool _is_ref(model f) {
    f = model_resolve(f); 
    if (f->is_ref)
        return true;
    model src = f->src;
    while (instanceof(src, typeid(model))) {
        src = src->src;
        if (src && src->is_ref)
            break;
    }
    Au_t asrc = src;
    // Au-type base could still be a pointer-type
    if (asrc) {
        return (asrc->traits & AU_TRAIT_POINTER) != 0;
    }
    return false;
}

int _ref_level(model f) {
    f = model_resolve(f); 
    model src = f;
    int level = 0;
    while (src) {
        if (src->is_ref)
            level++;
        src = src->src;
    }
    Au_t asrc = src;
    // Au-type base could still be a pointer-type
    if (asrc && (asrc->traits & AU_TRAIT_POINTER) != 0)
        level++;
    
    return level;
}

emember aether_register_model(aether e, model mdl, string name, bool use_now) {
    if (mdl->mem)
        return mdl->mem;

    bool is_func  = instanceof(mdl, typeid(function)) != null;
    bool is_macro = instanceof(mdl, typeid(macro)) != null;
    // the pointer for classes is created after, and used when we need it
    // accessible on model by ->ptr
    bool _is_class = isa(mdl) == typeid(Class);

    model mdl_for_member = _is_class ?
        model(
            mod,    mdl->mod,
            is_ref, true,      members, mdl->members,
            body,   mdl->body, src,     mdl, type,
                LLVMPointerType(mdl->type ? mdl->type : LLVMInt8Type(), 0)) :
        mdl;
    
    if (_is_class) mdl->ptr = hold(mdl_for_member);

    emember mem   = emember(
        mod,      e,
        mdl,      mdl_for_member,
        name,     name,
        is_func,  is_func,
        is_type,  true);
    
    mdl->mem = hold(mem);
    if (_is_class) {
        mdl_for_member->mem = hold(mem);
        mdl->ptr = hold(mdl_for_member);
    }

    register_member(e, mem, use_now);
    // print("registered model %o", mem->name);
    return mem;
}

void aether_register_member(aether e, emember mem, bool finalize) {

    if (mem && !mem->membership) {
        model ctx = mem->context ? mem->context : top(e);
        verify(ctx->members, "expected members map");
        set(ctx->members, string(mem->name->chars), mem);
        mem->membership = ctx;
    }

    if (finalize)
        finalize(mem);
}

#define no_target null


void initialize() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    print("LLVM-Version %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);
}

module_init(initialize);

void print_ctx(aether e, string msg) {
    string res = string(alloc, 32);
    for (int i = 0; i < len(e->lex); i++) {
        model ctx = e->lex->origin[i];
        if (res->count)
            append(res, "/");
        concat(res, cast(string, ctx));
    }
    print("%o: %o", res, msg);
}

#define print_context(msg, ...) print_ctx(e, form(string, msg, ## __VA_ARGS__))

i64 model_cmp(model mdl, model b) {
    return typed(mdl)->type == typed(b)->type ? 0 : -1;
}

array macro_expand(macro, array, map);

array read_arg(array tokens, int start, int* next_read);

static array expand_tokens(aether mod, array tokens, map expanding) {
    int ln = len(tokens);
    array res = array(alloc, 32);

    int skip = 1;
    for (int i = 0; i < ln; i += skip) {
        skip    = 1;
        token a = tokens->origin[i];
        token b = ln > (i + 1) ? tokens->origin[i + 1] : null;
        int   n = 2; // after b is 2 ahead of a

        if (b && eq(b, "##")) {
            if  (ln <= (i + 2)) return null;
            token c = tokens->origin[i + 2];
            if  (!c) return null;
            token  aa = token(alloc, len(a) + len(c) + 1);
            concat(aa, a);
            concat(aa, c);
            a = aa;
            n = 4;
            b = ln > (i + 3) ? tokens->origin[i + 3] : null; // can be null
            skip += 2;
        }

        // see if this token is a fellow macro
        macro m = lookup2(mod, a, typeid(macro));
        string mname = cast(string, m);
        if (m && b && eq(b, "(") && !get(expanding, mname)) {
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

// invocation environment
// this method will not get other macros to expand
// we are giving it direct tokens to give to its self expansion
// the expansion going in was handled by the parser (silver)

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

    set(expanding, m->mem->name, _bool(true));

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

define_class(macro, model)

void model_init(model mdl) {
    aether  e = mdl->mod;
    mdl->imported_from = (e && e->current_include) ? e->current_include : null;
    eargs args = instanceof(mdl, typeid(eargs));
    statements st = instanceof(mdl, typeid(statements));
    function   f = instanceof(mdl, typeid(function));

    if (isa(mdl) == typeid(macro))
        return;

    if (isa(mdl) != typeid(model)) // Class sets src in the case of array's element
        return;

    // no need if we have a resolved type
    //if (!mdl->is_ref && mdl->src && isa(mdl->src) && mdl->src->type)
    //    return;
    
    /// narrow down type traits
    model mdl_src = mdl;
    Au_t mdl_type = isa(mdl);

    Au_t type = mdl->atype_src ? mdl->atype_src : (Au_t)isa(mdl);

    if (!mdl->type && type && ((type->traits & AU_TRAIT_PRIMITIVE) != 0 ||
                               (type->traits & AU_TRAIT_ABSTRACT)  != 0)) {
        // we must support count in here, along with src being set
        if (type == typeid(f32))
            mdl->type = LLVMFloatType();
        else if (type == typeid(f64))
            mdl->type = LLVMDoubleType();
        else if (type == typeid(f80))
            mdl->type = LLVMX86FP80Type();
        else if (type == typeid(none))
            mdl->type = LLVMVoidType  ();
        else if (type == typeid(bool))
            mdl->type = LLVMInt1Type  ();
        else if (type == typeid(i8)  || type == typeid(u8))
            mdl->type = LLVMInt8Type();
        else if (type == typeid(i16) || type == typeid(u16))
            mdl->type = LLVMInt16Type();
        else if (type == typeid(i32) || type == typeid(u32) || type == typeid(AFlag))
            mdl->type = LLVMInt32Type();
        else if (type == typeid(i64) || type == typeid(u64) || type == typeid(num))
            mdl->type = LLVMInt64Type();
        else if (type == typeid(symbol) || type == typeid(cstr) || type == typeid(raw)) {
            mdl->is_const = type == typeid(symbol);
            mdl->type = LLVMPointerType(LLVMInt8Type(), 0);
        } else if (type == typeid(cstrs)) {
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
        } else if (type == typeid(sz)) {
            mdl->type = LLVMIntPtrTypeInContext(mdl->mod->module_ctx, mdl->mod->target_data);
        } else if (type == typeid(cereal)) {
            LLVMTypeRef cereal_type = LLVMStructCreateNamed(mdl->mod->module_ctx, "cereal");
            LLVMTypeRef members[] = {
                LLVMPointerType(LLVMInt8Type(), 0)  // char* → i8*
            };
            LLVMStructSetBody(cereal_type, members, 1, 1);
            mdl->type = cereal_type;
        } else if (type == typeid(floats)) {
            mdl->type = LLVMPointerType(LLVMFloatType(), 0);
        } else if (type == typeid(func)) {
            LLVMTypeRef function_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
            mdl->type = LLVMPointerType(function_type, 0);
        } else if (type == typeid(hook)) {
            model e_A = emodel("Au");
            LLVMTypeRef param_types[] = { e_A->type };
            LLVMTypeRef hook_type = LLVMFunctionType(e_A->type, param_types, 1, 0);
            mdl->type = LLVMPointerType(hook_type, 0);
        } else if (type == typeid(callback)) {
            model e_A = emodel("Au");
            LLVMTypeRef param_types[] = { e_A->type, e_A->type };
            LLVMTypeRef cb_type = LLVMFunctionType(e_A->type, param_types, 2, 0);
            mdl->type = LLVMPointerType(cb_type, 0);
        } else if (type == typeid(callback_extra)) {
            model e_A = emodel("Au");
            LLVMTypeRef param_types[] = { e_A->type, e_A->type, e_A->type };
            LLVMTypeRef cb_type = LLVMFunctionType(e_A->type, param_types, 3, 0);
            mdl->type = LLVMPointerType(cb_type, 0);
        } else if (type == typeid(ref_u8))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
        else if (type == typeid(ref_u16))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt16Type(), 0), 0);
        else if (type == typeid(ref_u32))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt32Type(), 0), 0);
        else if (type == typeid(ref_u64))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt64Type(), 0), 0);
        else if (type == typeid(ref_i8))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
        else if (type == typeid(ref_i16))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt16Type(), 0), 0);
        else if (type == typeid(ref_i32))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt32Type(), 0), 0);
        else if (type == typeid(ref_i64))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt64Type(), 0), 0);
        else if (type == typeid(ref_f32))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMFloatType(), 0), 0);
        else if (type == typeid(ref_f64))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMDoubleType(), 0), 0);
        else if (type == typeid(ref_bool))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt1Type(), 0), 0);
        else if (type == typeid(handle))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
        else if (type == typeid(ARef))
            mdl->type = emodel("Au")->type;
        else if (type == typeid(Au_ts))
            mdl->type = emodel("Au_ts")->type;
        else if (type == typeid(bf16)) {
            mdl->type = LLVMBFloatTypeInContext(e->module_ctx);
        } else if (type == typeid(fp16)) {
            mdl->type = LLVMHalfTypeInContext(e->module_ctx);
        } else if ((mdl->atype_src->traits & AU_TRAIT_POINTER) != 0) {
            model mdl_src = null;
            cstr src_name = null;
            string n = string(mdl->atype_src->ident);
            if (starts_with(n, "ref_")) {
                string n = mid(n, 4, len(n) - 4);
                mdl_src = emodel(n->chars);
                src_name = n->chars;
            } else {
                // the way i 'thought' Au-type worked
                Au_t src = mdl->atype_src->src;
                src_name = src->ident;
                mdl_src = src->user;
            }
            verify(mdl_src && mdl_src->type,
                "type must be created before %o: %s", n, src_name);
            mdl->type = pointer(mdl_src);
        } else if ((mdl->atype_src->traits & AU_TRAIT_ABSTRACT) == 0) {
            fault("unsupported primitive %s", type->ident);
        }
        if (mdl->type && mdl->type != LLVMVoidType())
            mdl->debug = LLVMDIBuilderCreateBasicType(
                e->dbg_builder,     // Debug info builder
                type->ident,         // Name of the primitive type (e.g., "int32", "float64")
                strlen(type->ident), // Length of the name
                LLVMABISizeOfType(e->target_data, mdl->type) * 8, // Size in bits (e.g., 32 for int, 64 for double)
                type->ident[0] == 'f' ? 0x04 : 0x05, // switching based on f float or u/i int (on primitives)
                0);
    } else {
        // we still need static array (use of integral shape), aliases
        if (mdl && mdl->src) {
            model src = mdl->src;
            if (isa(src) == typeid(function))
                finalize(src->mem);
        }

        // can be a class, structure, function
        if (type == typeid(model) && mdl->src && mdl->src->type) {
            model src = mdl->src;
            if (isa(src) == typeid(function))
                finalize(src->mem);
            /// this is a reference, so we create type and debug based on this
            u64 ptr_sz = LLVMPointerSize(e->target_data);
            mdl->type  = LLVMPointerType(
                src->type == LLVMVoidType() ? LLVMInt8Type() : src->type, 0);
        } else if (type == typeid(aether) || type == typeid(macro))
            return;
        else if (!mdl->type && type != typeid(aether)) {
            //fault("unsupported model type: %s", type->ident);
            return;
        }
    } 
    if (mdl->type && mdl->type != LLVMVoidType()) {
        mdl->size_bits      = LLVMABISizeOfType     (mdl->mod->target_data, mdl->type) * 8;
        mdl->alignment_bits = LLVMABIAlignmentOfType(mdl->mod->target_data, mdl->type) * 8;
    }
    
    if (instanceof(mdl, typeid(record)))
        mdl->scope = mdl->debug; // set this in record manually when debug is set
    else if (!mdl->scope)
        mdl->scope = e->scope;

    // convert to array if count is set
    if (mdl->count > 0 && mdl->type && LLVMGetTypeKind(mdl->type) != LLVMVoidTypeKind) {
        mdl->type = LLVMArrayType(mdl->type, mdl->count);
        mdl->size_bits = LLVMABISizeOfType(mdl->mod->target_data, mdl->type) * 8;
        if (mdl->debug)
            mdl->debug = LLVMDIBuilderCreateArrayType(
                e->dbg_builder,
                LLVMABISizeOfType(e->target_data, mdl->type) * 8,
                LLVMABIAlignmentOfType(e->target_data, mdl->type),
                null, 0, 0);
    }
}

Class read_parent(Class mdl) {
    if (isa(mdl->src) == typeid(Class) && mdl->src->parent) {
        return mdl->src->parent;
    }
    return null;
}

bool model_inherits(model mdl, model base) {
    if (mdl == base) return true;
    bool inherits = false;
    model m = mdl;
    while (m) {
        if (!instanceof(m, typeid(record))) return false;
        if (m == base)
            return true;
        m = read_parent(m);
    }
    return false;
}

void statements_init(statements st) {
    aether e = st->mod;
    st->scope = LLVMDIBuilderCreateLexicalBlock(e->dbg_builder, top(e)->scope, e->file, 1, 0);
}

model aether_context_model(aether e, Au_t type) {
    Au_t cl = typeid(Class);
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->origin[i];
        Au_t ctx_t = isa(ctx);
        if ((type == cl && inherits(isa(ctx->src), type)) || 
            (type != cl && inherits(ctx_t, type)))
            return ctx;
        if (ctx == e)
            return null;
    }
    return null;
}

enode aether_e_goto(aether e, array tokens_label) {
    catcher cat = context_model(e, typeid(catcher));

    verify(cat->team, "rogue cat, cannot goto label %o", tokens_label);
    catcher f_case = get(cat->team, tokens_label);
    e->is_const_op = false;
    if (!e->no_build)
        LLVMBuildBr(e->builder, f_case->block);
    return e_noop(e, null);
}

enode aether_e_break(aether e, catcher cat) {
    e->is_const_op = false;
    if (!e->no_build)
        LLVMBuildBr(e->builder, cat->block);
    return e_noop(e, null);
}

void aether_e_print_node(aether e, enode n) {
    if (e->no_build) return;
    /// would be nice to have a lookup method on import
    /// call it im_lookup or something so we can tell its different
    emember printf_fn  = elookup("printf")->mdl;
    model   mdl_cstr   = emodel("cstr");
    model   mdl_symbol = emodel("symbol");
    model   mdl_i32    = emodel("i32");
    model   mdl_i64    = emodel("i64");
    cstr fmt = null;
    if (n->mdl == mdl_cstr || n->mdl == mdl_symbol) {
        fmt = "%s";
    } else if (n->mdl == mdl_i32) {
        fmt = "%i";
    } else if (n->mdl == mdl_i64) {
        fmt = "%lli";
    }
    verify(fmt, "eprint_node: unsupported model: %o", n->mdl->mem->name);
    e_fn_call(e, printf_fn, null,
        array_of(e_operand(e, string(fmt), null, null), n, null));
}

emember model_find_member(model mdl, string n, Au_t mdl_type_filter) {
    aether e = mdl->mod;
    while (mdl) {
        if (mdl->members) {
            emember m = get(mdl->members, n);
            if (m && (!mdl_type_filter || isa(m->mdl) == mdl_type_filter))
                return m;
        }
        mdl = mdl->parent;
    }
    return null;
}

string model_cast_string(model mdl) {
    return mdl->mem ? mdl->mem->name : null;
}

/// we allocate all pointer models this way
model model_pointer(model mdl) {
    if (!mdl->ptr) {
        model res = model_resolve(mdl);
        mdl->ptr = hold(model(
            mod,    mdl->mod,
            is_ref, true,      members, mdl->members,
            body,   mdl->body, src,     mdl, type,
                LLVMPointerType(res->type ? res->type : LLVMInt8Type(), 0)));
     }
    return mdl->ptr;
}

model model_source(model mdl) {
    if (mdl->src && instanceof(mdl->src, typeid(model)) && mdl->src->ptr == mdl)
        mdl = mdl->src;
    return mdl;
}

// f = format string; this is evaluated from nodes given at runtime
void aether_eprint(aether e, symbol f, ...) {
    if (e->no_build) return;
    va_list args;
    va_start(args, f);

    int   format_len  = strlen(f);
    int   pos         = 0;
    int   max_arg     = -1;
    cstr  buf         = calloc(1, format_len + 1);
    cstr  ptr         = (cstr)f;
    array schema      = array(32);
    cstr  start       = null;

    while (*ptr) {
        if (*ptr == '{' && isdigit(*(ptr + 1))) {
            if (start) {
                int block_sz = ((sz)ptr - (sz)start);
                memcpy(buf, start, block_sz);
                buf[block_sz] = 0;
                push(schema, string(buf));
            }
            // Parse the number inside {N}
            int i = 0;
            ptr++;
            while (isdigit(*ptr)) {
                buf[i++] = *ptr++;
            }
            buf[i] = '\0';
            i32 n = atoi(buf);
            if (max_arg < n)
                max_arg = n;
            push(schema, _i32(n));
            verify(*ptr == '}', "expected }");
            ptr++;
            start = ptr;
        } else if (!start) {
            start = ptr;
        }
        ptr++;
    }

    if (start && start[0]) {
        int block_sz = ((sz)ptr - (sz)start);
        memcpy(buf, start, block_sz);
        buf[block_sz] = 0;
        push(schema, string(buf));
    }

    enode *arg_nodes = calloc(max_arg + 1, sizeof(i32));
    for (int i = 0; i < max_arg; i++)
        arg_nodes[i] = va_arg(args, enode);
    
    string res = string(alloc, 32);
    model mdl_cstr = emodel("cstr");
    model mdl_i32  = emodel("i32");
    each(schema, Au, obj) {
        enode n = instanceof(obj, typeid(enode));
        if (n) {
            e_print_node(e, n);
        } else {
            string s = instanceof(obj, typeid(string));
            verify(s, "invalid type data");
            enode n_str = e_operand(e, s, null, null);
            e_print_node(e, n_str);
        }
    }

    va_end(args);
    free(arg_nodes);
    free(buf);
}

emember aether_e_var(aether e, model mdl, string name) {
    if (e->no_build) return e_noop(e, mdl);
    enode   a =  e_create(e, mdl, null, null);
    emember m = emember(mod, e, name, name, mdl, mdl);
    register_member(e, m, true);
    e_assign(e, m, a, OPType__assign);
    return m;
}

void code_init(code c) {
    function function = context_model(c->mod, typeid(function));
    c->block = LLVMAppendBasicBlock(function->value, c->label);
}

void code_seek_end(code c) {
    LLVMPositionBuilderAtEnd(c->mod->builder, c->block);
}

void aether_e_cmp_code(aether e, enode l, comparison comp, enode r, code lcode, code rcode) {
    if (e->no_build) return;
    LLVMValueRef cond = LLVMBuildICmp(
        e->builder, (LLVMIntPredicate)comp, l->value, r->value, "cond");
    LLVMBuildCondBr(e->builder, cond, lcode->block, rcode->block);
}

enode aether_e_element(aether e, enode array, Au index) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, typed(array->mdl));
    enode i = e_operand(e, index, null, null);
    model imdl = (model)(array->meta->origin[0] ? 
        (model)array->meta->origin[0] : emodel("Au"));
    enode element_v = value(imdl, null, LLVMBuildInBoundsGEP2(
        e->builder, typed(array->mdl)->type, array->value, &i->value, 1, "eelement"));
    return e_load(e, element_v, null);
}

void aether_e_inc(aether e, enode v, num amount) {
    e->is_const_op = false;
    if (e->no_build) return;
    enode lv = e_load(e, v, null);
    LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), amount, 0);
    LLVMValueRef nextI = LLVMBuildAdd(e->mod->builder, lv->value, one, "nextI");
    LLVMBuildStore(e->mod->builder, nextI, v->value);
}

/// needs to import classes, structs, methods within the classes
/// there is no such thing as a global spec for Au_t functions
/// we simply can have lots of static methods on Au, though.

map member_map(aether e, cstr field, ...) {
    va_list args;
    va_start(args, field);
    cstr value;
    map  res = map(hsize, 32);
    while ((value = va_arg(args, cstr)) != null) {
        string n   = new(string, chars, value);
        Au mdl = va_arg(args, Au);
        Au_t  ty  = isa(mdl);
        emember mem = emember(mod, e, name, n, mdl, va_arg(args, cstr));
        set(res, n, mem);
    }
    va_end(args);
    return res;
}

void aether_e_branch(aether e, code c) {
    e->is_const_op = false;
    if (e->no_build) return;
    LLVMBuildBr(e->builder, c->block);
}

void aether_build_initializer(aether e, function m) { }

bool is_module_level(model mdl) {
    aether e = mdl->mod;
    pairs(e->members, i) {
        emember mem = i->value;
        if (mem->mdl == mdl) return true;
    }
    return false;
}

static i32 get_aflags(emember f, i32 extra) {
    function func = instanceof(f->mdl, typeid(function));
    return (extra | (func && func->is_override)) ? AU_MEMBER_OVERRIDE : 0;
}

void aether_finalize_init(aether e, function f) {
    // register module constructors as global initializers ONLY for delegate modules
    // aether creates a 'main' with argument parsing for its main modules
    emember module_init_mem = null;
    model _Au_t = emodel("_Au_t");
    model _A     = emodel("Au");
    Au_t _Au_cl  = isa(_A);

    // we need to set 'shape' on the Au_t field
    
    // emit declaration of the struct _type_f for classes, structs, and enums
    model module_base = e->lex->origin[1]; // the init is at [1] because of the need to have non-replacables at [0] in use with a watcher
    pairs (module_base->members, i) {
        emember mem = i->value;
        model   mdl = mem->mdl;
        if (mdl == f)
            module_init_mem = mem;
    }

    // now enter the function where we will construct the type_i
    push(e, f);

    pairs (module_base->members, i) { // these members include our types, then we go through members on those
        emember mem = i->value;
        model   mdl = mem->mdl;

        if (mdl->imported_from) continue;

        Class       cmdl = mdl->src ? instanceof(mdl->src, typeid(Class)) : null;
        Class       smdl = instanceof(mdl, typeid(structure));
        enumeration emdl = instanceof(mdl, typeid(enumeration));

        if (!cmdl && !emdl)
            continue;
        
        if (cmdl && cmdl->is_abstract)
            continue;

        symbol null_str = null;
        Au      null_v   = primitive(typeid(symbol), &null_str);
        Au      null_h   = primitive(typeid(handle), &null_str);
        int    m_count  = 0;

        pairs (mdl->members, ii) {
            emember f = ii->value;
            if (f->access == interface_intern) continue;
            m_count++;
        }

        // create a 'static_array', this indicates that we are to create static global space for this in module,
        // e_create returns a pointer to it, which we may assign to member
        static_array members = static_array(alloc, m_count, assorted, true); // we need to create a singular value for this entire array of members
        model member_ptr = emodel("_member");
        i32 current_prop_id = 0;

        pairs (mdl->members, ii) {
            emember   f      = ii->value;
            map       member = map(hsize, 8, assorted, true);
            function  func   = instanceof(f->mdl, typeid(function));
            
            if (f->access == interface_intern) continue;
            // structure _meta_t
            map  m_meta = map(unmanaged, true, assorted, true);

            // for functions, this is the argments; for props they are meta descriptors; type-only entries to describe their use
            i32 meta_index = 0;
            if (func) {
                mset(m_meta, "count", _i64((func->instance ? 1 : 0) + (func->args ? len(func->args->members): 0)));
                if (func->instance) {
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, func->instance); // this should not be the pointer type
                    meta_index++;
                }
                pairs(func->args->members, ai) {
                    model mdl_meta = ((emember)ai->value)->mdl;
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, ident2(mdl_meta));
                    meta_index++;
                }
            } else {
                mset(m_meta, "count", _i64(len(f->meta_args))); // f is field; this is a meta-description for field members using the args we already have
                each(f->meta_args, model, mdl_meta) {
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, ident2(mdl_meta));
                    meta_index++;
                }
            }

            Au           _count  = get(m_meta, string("count"));
            Au_t       t       = isa(_count);
            map         margs   = map(unmanaged, true, assorted, true);
            enumeration en      = instanceof(f->mdl, typeid(enumeration));

            if (func && func->args) {
                mset(margs, "count", _i64(len(f->meta_args)));
                i32 meta_index = 0;
                pairs(func->args->members, ai) {
                    emember arg_mem = ai->value;
                    string fname = f(string, "meta_%i", meta_index);
                    set(margs, fname, arg_mem->mdl);
                    meta_index++;
                }
            }
            
            mset(member, "name",  f->name);
            mset(member, "sname", e_null(e, emodel("handle")));

            if (func) {
                if (!func->rtype) {
                    fault("this should be set to something");
                }
                mset(member, "type", ident2(func->rtype));
            } else {
                 mset(member, "type", ident2(f->mdl));
            }
           
            mset(member, "offset",          _i32(f->offset_bits / 8)); // do we have to add the parent classes minus Au-type? (Au-type is a header)
            mset(member, "count",           _i32(1)); // needs to expose member-specific array, however we don't handle it on the member level in silver (we may not need this)
            if (func) {
                mset(member, "member_type", _i32(get_aflags(f, func->function_type)));
            } else {
                if (emdl)
                    mset(member, "member_type", _i32(get_aflags(f, AU_MEMBER_ENUMV)));
                else
                    mset(member, "member_type", _i32(get_aflags(f, AU_MEMBER_PROP)));
            }

            OPType otype = ((cmdl || smdl) && func && func->is_oper) ? func->is_oper : 0;
            mset(member, "operator_type",   _i32(otype));
            mset(member, "required",        _i32(f->is_require ? 1 : 0));
            mset(member, "args",            margs); // e_create can lookup this field, and convert the map

            if (func) {
                mset(member, "ptr", enode(
                    mod, e, value, LLVMConstBitCast(func->value, func->ptr->type),
                    meta, null, mdl, func->ptr));
            } else if (emdl) { // && member is ENUMV
                enode gvalue = get(en->global_values, f->name);
                verify(gvalue, "value not found for enum %o", f->name);
                mset(member, "ptr", e_addr_of(e, gvalue, en->atype));
            } else {
                mset(member, "ptr", e_null(e, emodel("handle")));
            }

            mset(member, "method", null_h);
            mset(member, "id",     _i64(current_prop_id));
            //mset(member, "value",  _i64(0));

            if (!func)
                current_prop_id++;

            enode static_member = e_create(e, member_ptr->src, null, member);
            push(members, static_member);
        }

        
        // set traits
        AFlag traits = 0;
        if (cmdl) traits |= AU_TRAIT_CLASS;
        if (smdl) traits |= AU_TRAIT_STRUCT;
        if (emdl) traits |= AU_TRAIT_ENUM;

        // fill out type meta
        map type_meta = map(unmanaged, true, assorted, true);
        i32 meta_index = 0;
        if (cmdl || smdl) {
            record rmdl = cmdl ? cmdl : smdl;
            mset(type_meta, "count", _i64(len(rmdl->meta))); // f is field; this is a meta-description for field members using the args we already have
            each(rmdl->meta, model, mdl_meta) {
                string fname = f(string, "meta_%i", meta_index);
                set(type_meta, fname, ident2(mdl_meta));
                meta_index++;
            }
        }

        i32 isize = 0;
        pairs (mdl->members, ii) {
            emember   f      = ii->value;
            map       member = map(hsize, 8, assorted, true);
            function  func   = instanceof(f->mdl, typeid(function));
            if (f->access != interface_intern || func)
                continue;
            isize += f->mdl->size_bits / 8;
        }

        enode e_members  = e_create(e, member_ptr, null, members);

        map  mtype = map(hsize, 8, unmanaged, true, assorted, true);
        mset(mtype, "name",   mdl->mem->name);
        mset(mtype, "module", e->mem->name);
        mset(mtype, "size",   _i32(mdl->size_bits / 8));
        mset(mtype, "magic",  _i32(1337));
        mset(mtype, "traits", _i32(traits));
        mset(mtype, "meta",   type_meta);
        mset(mtype, "isize",  _i32(isize));
        mset(mtype, "member_count", _i32(len(members)));
        mset(mtype, "members", e_members); // simply getting the const address

        e->no_const = true;
        enode static_type = e_create(e,
            mdl->schema->schema_type->mdl, mdl->schema->schema_type->meta, mtype);
        e->no_const = false;
        enode target      = e_typeid(e, mdl);

        // build operations -- copy static_type to target (Au_t ptr) memory
        e_memcpy(e, target, static_type, mdl->schema->schema_type->mdl);

        // set vmember_type and vmember_count
        push(e, target->mdl->src);

        // set parent_type if this is a class
        if (cmdl) {
            record parent = cmdl->parent;
            verify(!parent || instanceof(parent, typeid(record)), "expected parent record");
            if (parent && parent->is_abstract)
                parent = parent->src;
            enode parent_type = e_typeid(e, parent);
            enode m_parent_type = access(target, string("parent_type"));
            e_assign(e, m_parent_type, parent_type, OPType__assign);
        }

        // push_type(type_ref);
        function fn_push_type = function_lookup(_A, "push_type");
        e_fn_call(e, fn_push_type, null, a(target)); // todo: t-method, f-method, and s-method must merge.  this is plainly stupid

        // vector type info if implemented
        /*
        if (false) {
            emember m_vmember_count = resolve(res, string("vmember_count"));
            emember m_vmember_type  = resolve(res, string("vmember_type"));
            e_assign(e, m_vmember_count, _i32(0), OPType__assign);
        }*/

        // finish this process of initialization (types not yet emitting their schema initialization)

        pop(e);
    }

    // we should create structs for our class tables now; we need members registered for each
    //verify(e->top == (model)f, "expected module context");
    //push(e, e);
    if (e->mod->delegate) {
        unsigned ctr_kind = LLVMGetEnumAttributeKindForName("constructor", 11);
        LLVMAddAttributeAtIndex(f->value, 
            LLVMAttributeFunctionIndex, 
            LLVMCreateEnumAttribute(e->module_ctx, ctr_kind, 0));
    } else {

        // check if module will be an app or lib
        // search through members, finding subclass of main
        Class main_class = null;
        Class main_spec  = emodel("main");
        verify(main_spec && instanceof(main_spec->src, typeid(Class)), "expected main class");
        pairs(module_base->src->members, i) {
            emember mem = i->value;
            Class   cl  = mem->mdl->src ? instanceof(mem->mdl->src, typeid(Class)) : null;
            if (!cl) continue;
            if (cl->parent == main_spec) {
                main_class = cl;
                break;
            }
        }

        bool is_app = main_class != null;
        pairs (module_base->members, i) {
            emember mem = i->value;
            // todo: reflection on emember if public
            if (mem->initializer) {
                build_initializer(e, mem);
            } else {
                // we need a default(e, mem) call!
                //zero(e, mem);
            }
        }
        e_fn_return(e, null);
        pop (e);

        verify(module_init_mem, "expected member for module init");

        if (!main_class) {
            e->is_library = true;
            // this is a library, so we do not implement or call a main
        } else {
            // code main, which is what inits main class
            eargs    args = eargs(mod, e);
            emember  argc = earg(args, emodel("i32"),   null, "argc");
            emember  argv = earg(args, emodel("cstrs"), null, "argv");
            set(args->members, argc->name, argc);
            set(args->members, argv->name, argv);

            function main_fn = function(
                mod,            e,
                function_type,  AU_TRAIT_SMETHOD,
                exported,       true,
                rtype,          emodel("i32"),
                args,           args);

            push(e, main_fn);

            // from main_fn: call module initialize (initializes module members)
            e_fn_call(e, module_init_mem->mdl, null, null);
            function Au_engage = function_lookup(_A, "engage");
            e_fn_call(e, Au_engage, null, a(argv));

            // create main class described by user
            // we will want to serialize properties for the class as well
            // if there are required properties, we can use this to exit with 1
            // must call Au_init
            e_create(e, main_class, null, map());

            // return i32, which could come from a cast on the class if implemented
            e_fn_return(e, _i32(255));
            
            pop(e);

            push(e, e);
            register_model(e, main_fn, string("main"), true);
            pop(e);
        }
/*
if (eq(fn->name, "main")) {
    verify(!e->delegate,
        "unexpected main in module %o "
        "[these are implemented automatically on top level modules]", e->name);
    
    emember main_member = fn->main_member; /// has no value, so we must create one
    
    push(e, fn);
    emember mm = emember(mod, e, mdl, main_member->mdl, name, main_member->name);

    /// allocate and assign main-emember [ main context class ]
    register_member(e, mm);
    enode a = create(e, mm->mdl, null); /// call allocate, does not call init yet!
    zero  (e, a);

    assign(e, mm, a, OPType__assign);
    eprint(e, "this is a text");

    /// loop through args and print
    model  i64  = emodel("i64");
    emember argc = elookup("argc");
    emember argv = elookup("argv");
    emember i    = evar(e, i64, string("i"));
    zero(e, i);

    /// define code blocks
    code cond = code(mod, e, label, "loop.cond");
    code body = code(mod, e, label, "loop.body");
    code end  = code(mod, e, label, "loop.end");

    /// emit code
    ebranch(e, cond);
    seek_end(cond);
    e_cmp_code(e, i, comparison_s_less_than, argc, body, end);
    seek_end (body);
    enode arg = e_element(e, argv, i);
    eprint (e, "{0}", arg);
    einc   (e, i, 1);
    ebranch(e, cond);
    seek_end (end);
    pop (e);
}
*/
    }
    //pop(e);
}

void function_init(function fn) {
    //verify(fn->record || !eq(fn->name, "main"), "to implement main, implement fn module-name[]");
}

// aether will only allow one of these per module
static LLVMValueRef set_module_init(LLVMModuleRef module, function f) {
    // register global constructor
    LLVMTypeRef ctor_type = LLVMStructTypeInContext(LLVMGetModuleContext(module),
        (LLVMTypeRef[]){LLVMInt32Type(), LLVMPointerType(f->type, 0), LLVMPointerType(LLVMInt8Type(), 0)}, 3, 0);
    
    LLVMValueRef ctor = LLVMConstStructInContext(LLVMGetModuleContext(module),
        (LLVMValueRef[]){
            LLVMConstInt(LLVMInt32Type(), 65535, 0),
            f->value,
            LLVMConstNull(LLVMPointerType(LLVMInt8Type(), 0))
        }, 3, 0);
    
    LLVMValueRef global_ctors = LLVMAddGlobal(module, LLVMArrayType(ctor_type, 1), "llvm.global_ctors");
    LLVMSetLinkage(global_ctors, LLVMAppendingLinkage);
    LLVMSetInitializer(global_ctors, LLVMConstArray(ctor_type, &ctor, 1));
    return global_ctors;
}

model translate_rtype(model mdl) {
    if (isa(mdl) == typeid(Class))
        return mdl->is_ref ? mdl : pointer(mdl);
    return mdl;
}

model translate_target(model mdl) {
    if (isa(mdl) == typeid(structure))
        return pointer(mdl);
    return mdl;
}

#define LLVMDwarfTag(tag) (tag)
#define DW_TAG_structure_type 0x13  // DWARF tag for structs.

void record_init(record rec) {
    aether e = rec->mod;
    rec->type = LLVMStructCreateNamed(LLVMGetGlobalContext(), rec->ident->chars);

    // Create a forward declaration for the struct's debug info
    if (isa(rec->ident) == typeid(token)) {
        rec->debug = LLVMDIBuilderCreateReplaceableCompositeType(
            e->dbg_builder,                      // Debug builder
            LLVMDwarfTag(DW_TAG_structure_type), // Tag for struct
            cstring(rec->ident),                 // Name of the struct
            len(rec->ident),
            top(e)->scope,                       // Scope (this can be file or module scope)
            e->file,                             // File
            rec->ident->line,                    // Line number
            0,
            0,
            0,
            LLVMDIFlagZero,                      // Flags
            NULL,                                // Derived from (NULL in C)
            0);
    }
}



enode enode_access_guard(enode mem, string alpha) {
    aether e = mem->mod;

    // 2. Prepare code blocks
    code then_code  = code(mod, e, label, "guard.then");   // normal .member
    code else_code  = code(mod, e, label, "guard.else");   // null return
    code merge_code = code(mod, e, label, "guard.merge");

    // 3. Emit conditional branch
    // IF base == null → jump to else_block
    // ELSE → jump to then_block
    e_cmp_code(e, mem, comparison_equals, e_null(e, mem->mdl), else_code, then_code);

    // --------------------------
    // THEN BLOCK (non-null)
    // --------------------------
    seek_end(then_code);

    // Evaluate normal .member here
    enode result_then = access(mem, alpha);
    result_then = e_load(e, result_then, NULL);
    //result_then = parse_member_expr(e, result_then);

    // Save the result so we can feed it to PHI later
    LLVMValueRef then_val  = result_then->value;
    LLVMTypeRef  then_type = result_then->mdl->type;

    // After finishing then block, jump to merge
    e_branch(e, merge_code);

    // --------------------------
    // ELSE BLOCK (null case)
    // --------------------------
    seek_end(else_code);

    // Produce the null-version of the member type
    LLVMValueRef null_val = LLVMConstNull(then_type);

    // Jump to merge block
    e_branch(e, merge_code);

    // --------------------------
    // MERGE BLOCK
    // --------------------------
    seek_end(merge_code);

    // Build PHI node for final value
    LLVMValueRef phi = LLVMBuildPhi(e->builder, result_then->mdl->type, "guardphi");

    LLVMAddIncoming(phi, &then_val,  &then_code->block, 1);
    LLVMAddIncoming(phi, &null_val,  &else_code->block, 1);

    return enode(mod, e, value, phi, mdl, result_then->mdl);
}


enode enode_access(enode mem, string name) {
    aether  e   = mem->mod;
    bool is_ptr = isa(mem) == typeid(emember) ? 
        (((emember)mem)->is_arg || ((emember)mem)->is_global) : false;

    if (isa(mem->mdl) == typeid(enumeration)) {
        emember m = get(mem->mdl->members, name);
        verify (m, "%o not found on enumerable type %o", name, mem->mdl);
        return  m;
    }
    
    model current = mem->mdl->is_ref ? mem->mdl->src : mem->mdl;
    model mdl     = current;
    e->is_const_op   = false;
    if (e->no_build) return e_noop(e, ((emember)get(mem->mdl->members, name))->mdl);

    emember schema = get(mem->mdl->members, name);
    verify(schema, "failed to find member %o for model %o", name, mdl);

    function f = instanceof(schema->mdl, typeid(function));
    if (f) return enode(mod, e, name, name, mdl, schema->mdl, target, mem);
    
    if (schema->is_const) return schema;

    LLVMValueRef actual_ptr = is_ptr ? mem->value : LLVMBuildLoad2(
        e->builder,
        (isa(mdl) == typeid(Class) ? pointer(mdl) : mdl)->type,
        mem->value,
        "load_actual_ptr");

    enode res = enode(mod, e, name, name, mdl, null);
    res->mdl = (f && !e->in_ref) ? (model)f : pointer(schema->mdl);
    static int count = 0;
    res->value  = f ? f->value : LLVMBuildStructGEP2(
        e->builder, typed(mdl)->type, actual_ptr,
        schema->index, fmt("resolve%i", ++count)->chars);
    return res;
}

void emember_set_value(emember mem, Au value) {
    aether e = mem->mod;
    enode n = e_operand(mem->mod, value, null, null);
    mem->mdl   = n->mdl;
    mem->value = n->value;
    mem->literal = hold(n->literal);
    mem->is_const = LLVMIsAConstant(n->value) != null;
}

bool emember_has_value(emember mem) {
    return mem->value != null;
}

void emember_init(emember mem) {
    aether e   = mem->mod;
    if (!mem->name) mem->name = string("");
    mem->name = hold(string(mem->name->chars));

    record   rec;
    function in_f;
    context(e, &rec, &in_f, typeid(function));
    model  mdl  = mem->mdl;

    // filter models that emit schema
    // we must create these first, because they are used in finalization of instance structs
    if (mdl && mem->is_type && !mdl->is_system && !rec && !in_f && (isa(mdl) != typeid(function)) &&
            (!e->current_include || e->is_Au_import)) {
        verify(mem->name && len(mem->name), "member name required to register schema");
        string  f_name = f(string, "_%o_f", mem->name);
        emember m_find = lookup2(e, f_name, null);
        verify(!m_find, "schema object already created for %o (%o)", mem->name, f_name);
        mdl->schema_type = structure(
            mod, e, ident, f_name, members, map(hsize, 8), is_system, true);
        pointer(mdl->schema_type);
        if (mdl->src && isa(mdl->src) == typeid(Class))
            mdl->src->schema_type = hold(mdl->schema_type);
    }

    model top = top(e);
    if (!mem->access) mem->access = interface_public;
    if (instanceof(mem->name, typeid(string))) {
        string n = mem->name;
        mem->name = token(chars, cstring(n), source, e->source, line, 1);
    }
    model t = top(e);
    if (t && !mem->context) {
        if      (instanceof(t, typeid(record))) mem->context = t;
        else if (instanceof(t, typeid(eargs)))  mem->context = t;
        else if (t->src && instanceof(t->src, typeid(Class)))  mem->context = t->src;
        else if (instanceof(t, typeid(Class)))  mem->context = t;
        else if (instanceof(t, typeid(uni)))    mem->context = t;
    }
}

#define int_value(b,l) \
    enode(mod, e, \
        literal, l, meta, null, mdl, emodel(stringify(i##b)), \
        value, LLVMConstInt(typed(emodel(stringify(i##b)))->type, *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, e, \
        literal, l, meta, null, mdl, emodel(stringify(u##b)), \
        value, LLVMConstInt(typed(emodel(stringify(u##b)))->type, *(u##b*)l, 0))

/*
#define bool_value(b,l) \
    enode(mod, e, \
        literal, l, meta, null, mdl, emodel("bool"), \
        value, LLVMConstInt(typed(emodel(stringify(u##b)))->type, *(u##b*)l, 0))
*/

#define f32_value(b,l) \
    enode(mod, e, \
        literal, l, meta, null, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(typed(emodel(stringify(f##b)))->type, *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, e, \
        literal, l, meta, null, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(typed(emodel(stringify(f##b)))->type, *(f##b*)l))

enode aether_e_typeid(aether e, model mdl) {
    e->is_const_op = false;
    string  name     = mdl->mem->name;
    model   atype    = emodel("Au_t");
    string  i_symbol = f(string, "%o_i", name);
    emember i_member = lookup2(e, i_symbol, null);
    verify(i_member, "schema instance not found for %o", name);
    LLVMValueRef g = i_member->value;
    verify(g, "expected info global instance for type %o", name);

    enode type_member = access((enode)i_member, string("type"));
    type_member->mdl->is_typeid = true;
    verify(type_member, "expected info global instance for type %o", name);
    return type_member;
}


enode aether_e_const_array(aether e, model mdl, array a) {
    model atype = emodel("Au_t");
    model vector_type = pointer(mdl);

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, vector_type);

    if (!a || !len(a))
        return e_null(e, vector_type);

    i32 ln = len(a);
    
    LLVMTypeRef arrTy = LLVMArrayType(vector_type->type, ln);
    LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = a->origin[i];
        enode n = e_operand(e, m, mdl, null);
        elems[i] = n->value;  // each is an Au_t*
    }

    LLVMValueRef arr_init = LLVMConstArray(vector_type->type, elems, ln);
    free(elems);

    static int ident = 0;
    char gname[32];
    sprintf(gname, "static_array_%i", ident++);
    LLVMValueRef G = LLVMAddGlobal(e->module, arrTy, gname);
    LLVMSetLinkage(G, LLVMInternalLinkage);
    LLVMSetGlobalConstant(G, 1);
    LLVMSetInitializer(G, arr_init);

    enode n = enode(mod, e, value, G, mdl, mdl, meta, null);
    return e_addr_of(e, n, mdl);
}

enode aether_e_meta_ids(aether e, array meta) {
    model atype = emodel("Au_t");
    model atype_vector = pointer(atype);

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, atype_vector);

    if (!meta || !len(meta))
        return e_null(e, atype_vector);

    i32 ln = len(meta);
    
    LLVMTypeRef arrTy = LLVMArrayType(atype_vector->type, ln);
    LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = meta->origin[i];
        enode n;
        if (instanceof(m, typeid(model)))
            n = e_typeid(e, (model)m);
        else if (instanceof(m, typeid(shape))) {
            shape s = (shape)m;
            array a = array(alloc, s->count);
            push_vdata(a, s->data, s->count);
            n = e_create(e, emodel("shape"), null,
                m("count", s->count,
                  "data",  e_const_array(e, emodel("i64"), a)));
        } else {
            verify(false, "unsupported design-time meta type");
        }
        elems[i] = n->value;
    }

    LLVMValueRef arr_init = LLVMConstArray(atype_vector->type, elems, ln);
    free(elems);

    static int ident = 0;
    char gname[32];
    sprintf(gname, "meta_ids_%i", ident++);
    LLVMValueRef G = LLVMAddGlobal(e->module, arrTy, gname);
    LLVMSetLinkage(G, LLVMInternalLinkage);
    LLVMSetGlobalConstant(G, 1);
    LLVMSetInitializer(G, arr_init);

    enode arr_node = enode(mod, e, value, G, mdl, atype, meta, null);
    return e_addr_of(e, arr_node, atype);
}

static LLVMValueRef const_cstr(aether e, cstr value, i32 len) {
    LLVMContextRef ctx = LLVMGetGlobalContext();

    // 1. make a constant array from the raw bytes (null-terminated!)
    LLVMValueRef strConst = LLVMConstString(value, len, /*DontNullTerminate=*/0);

    // 2. create a global variable of that array type
    LLVMValueRef gv = LLVMAddGlobal(e->module, LLVMTypeOf(strConst), "const_cstr");
    LLVMSetInitializer(gv, strConst);
    LLVMSetGlobalConstant(gv, 1);
    LLVMSetLinkage(gv, LLVMPrivateLinkage);

    // 3. build a GEP to point at element [0,0] (first char)
    LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
    LLVMValueRef idxs[] = {zero, zero};
    LLVMValueRef cast_i8 = LLVMConstGEP2(LLVMTypeOf(strConst), gv, idxs, 2);

    return cast_i8;
}

enode e_operand_primitive(aether e, Au op) {
    Au_t t = isa(op);
         if (instanceof(op, typeid(  enode))) return op;
    else if (instanceof(op, typeid(  model))) return e_typeid(e, op);
    else if (instanceof(op, typeid(  ident2))) return e_typeid(e, ((ident2)op)->mdl); // needs to return a pointer that points to the symbol for address of name ## _i + 88 bytes offset (enode ops, not runtime)
    else if (instanceof(op, typeid( handle))) {
        uintptr_t v = (uintptr_t)*(void**)op; // these should be 0x00, infact we may want to assert for this

        // create an i64 constant with that address
        LLVMValueRef ci = LLVMConstInt(LLVMInt64TypeInContext(e->module_ctx), v, 0);

        // cast to a generic void* (i8*) pointer type
        LLVMTypeRef  hty = LLVMPointerType(LLVMInt8TypeInContext(e->module_ctx), 0);
        LLVMValueRef cp  = LLVMConstIntToPtr(ci, hty);

        return enode(
            mod,    e,
            value,  cp,                  // the LLVM constant pointer
            mdl,    emodel("handle"),    // your handle type model
            meta,   null,
            literal, op
        );
    }
    else if (instanceof(op, typeid(bool)))   return uint_value(8,  op);
    else if (instanceof(op, typeid(    u8))) return uint_value(8,  op);
    else if (instanceof(op, typeid(   u16))) return uint_value(16, op);
    else if (instanceof(op, typeid(   u32))) return uint_value(32, op);
    else if (instanceof(op, typeid(   u64))) return uint_value(64, op);
    else if (instanceof(op, typeid(    i8))) return  int_value(8,  op);
    else if (instanceof(op, typeid(   i16))) return  int_value(16, op);
    else if (instanceof(op, typeid(   i32))) return  int_value(32, op);
    else if (instanceof(op, typeid(   i64))) return  int_value(64, op);
    else if (instanceof(op, typeid(    sz))) return  int_value(64, op); /// instanceof is a bit broken here and we could fix the generic; its not working with aliases
    else if (instanceof(op, typeid(   f32))) return  f32_value(32, op);
    else if (instanceof(op, typeid(   f64))) return  f64_value(64, op);
    else if (instanceof(op, typeid(symbol))) {
        return enode(mod, e, meta, null, value, const_cstr(e, op, strlen(op)), mdl, emodel("symbol"), literal, op);
    }
    else if (instanceof(op, typeid(string))) { // this works for const_string too
        string str = string(((string)op)->chars);
        return enode(mod, e, meta, null, value, const_cstr(e, str->chars, str->count), mdl, emodel("symbol"), literal, op);
    }
    error("unsupported type in aether_operand %s", t->ident);
    return NULL;
}

enode aether_e_eval(aether e, string value) {
    array t = tokens(target, (Au)e, parser, e->parse_f, input, (Au)value);
    push_state(e, t, 0);
    enode n = e->parse_expr(e, null, null); 
    enode s = e_create(e, emodel("string"), null, n);
    pop_state(e, false);
    return s;
}

enode aether_e_interpolate(aether e, string str) {
    enode accum = null;
    array sp    = split_parts(str);
    model mdl   = emodel("string");

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, mdl);

    each (sp, ipart, s) {
        enode val = e_create(e, mdl, null, s->is_expr ?
            (Au)e_eval(e, s->content) : (Au)s->content);
        accum = accum ? e_add(e, accum, val) : val;
    }
    return accum;
}

enode aether_e_operand(aether e, Au op, model src_model, array meta) {
    if (!op) {
        if (_is_ref(src_model))
            return e_null(e, src_model);
        return enode(mod, e, mdl, emodel("none"), meta, null, value, null);
    }
    
    if (isa(op) == typeid(string))
        return e_create(e, src_model, meta,
            e_interpolate(e, (string)op));
    
    if (isa(op) == typeid(const_string))
        return e_create(e, src_model, meta, e_operand_primitive(e, op));
    
    if (isa(op) == typeid(map)) {
        return e_create(e, src_model, meta, op);
    }
    Au_t op_isa = isa(op);
    Class cl = op;
    if (instanceof(op, typeid(array))) {
        verify(src_model != null, "expected src_model with array data");
        return e_create(e, src_model, meta, op);
    }

    enode r = e_operand_primitive(e, op);
    return src_model ? e_create(e, src_model, meta, r) : r;
}

enode aether_e_null(aether e, model mdl) {
    model f = _is_class(mdl);
    if (f) mdl = f; // classes are elevated to ref even though structurally they look like structs.  we've gone back and forth on this one
    if (!mdl) mdl = emodel("handle");
    return enode(mod, e, value, LLVMConstNull(mdl->type), mdl, mdl, meta, null);
}

enode aether_e_primitive_convert(aether e, enode expr, model rtype);

/// create is both stack and heap allocation (based on model->is_ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_e_create(aether e, model mdl, array meta, Au args) {
    if (!mdl) return args;

    string  str  = instanceof(args, typeid(string));
    map     imap = instanceof(args, typeid(map));
    array   a    = instanceof(args, typeid(array));
    static_array static_a = instanceof(args, typeid(static_array));
    enode   n    = null;
    emember ctr  = null;
    //model orig = mdl;
    //mdl = typed(mdl); // this is a bad idea here. undo this!

    if (!args) {
        if (_is_ref(mdl))
            return e_null(e, mdl);
    }

    //if (mdl == emodel("string") && str)
    //    return e_operand_primitive(e, args);

    // construct / cast methods
    enode input = instanceof(args, typeid(enode));

    if (!input && instanceof(args, typeid(const_string))) {
        input = e_operand(e, args, emodel("string"), null);
        args  = input;
    }

    if (!input && instanceof(args, typeid(string))) {
        input = e_operand(e, args, emodel("string"), null);
        args  = input;
    }

    if (input) {
        verify(!imap, "unexpected data");
        
        // if both are internally created and these are refs, we can allow conversion
        emember fmem = convertible(input->mdl, mdl);
        verify(fmem, "no suitable conversion found for %o -> %o",
            input->mdl, mdl);
        
        if (fmem == (void*)true) {
            LLVMTypeRef t = LLVMTypeOf(input->value);
            LLVMTypeKind k = LLVMGetTypeKind(t);

            // check if these are either Au_t class typeids, or actual compatible instances
            if (k == LLVMPointerTypeKind) {
                e->is_const_op = false;
                if (e->no_build) return e_noop(e, mdl);

                model src = model_source(input->mdl);
                model dst = model_source(mdl);
                bool bit_cast = false;
                if (input->mdl->is_typeid && (mdl->is_typeid || mdl == emodel("Au_t")))
                    bit_cast = true; 
                else if (!_is_subclass(input->mdl, mdl)) {
                    char *s = LLVMPrintTypeToString(t);
                    int r0 = _ref_level(input->mdl);
                    int r1 = _ref_level(mdl);
                    print("LLVM type: %s", s);
                    verify((is_primitive(src) && is_primitive(dst)) ||
                        model_inherits(input->mdl, mdl), "models not compatible: %o -> %o",
                            input->mdl, mdl);
                }
                return value(mdl, null,
                    LLVMBuildBitCast(e->builder, input->value, mdl->type, "class_ref_cast"));
            }

            // fallback to primitive conversion rules
            return aether_e_primitive_convert(e, input, mdl);
        }
        
        // primitive-based conversion goes here
        function fn = instanceof(fmem->mdl, typeid(function));
        if (fn->function_type & AU_MEMBER_CONSTRUCT) {
            // ctr: call before init
            // this also means the mdl is not a primitive
            //verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = fmem;
        } else if (fn->function_type & AU_MEMBER_CAST) {
            // we may call cast straight away, no need for init (which the cast does)
            return e_fn_call(e, fn, input, a());
        } else
            fault("unknown error");
        
    }

    // handle primitives after cast checks -- the remaining objects are object-based
    // note that enumerable values are primitives
    if (is_primitive(mdl))
        return e_operand(e, args, mdl, meta);

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, mdl);

    Class        cmdl         = mdl->src ? instanceof(mdl->src, typeid(Class)) : null;
    structure    smdl         = instanceof(mdl, typeid(structure));
    Class        Au_type      = emodel("Au");
    function     f_alloc      = find_member((model)Au_type, string("alloc_new"), null)->mdl;
    function     f_initialize = find_member((model)Au_type, string("initialize"), null)->mdl;
    enode        res;
    Au_t         mdl_src      = isa(mdl->src);

    if (mdl->is_ref && mdl->src && isa(mdl->src) == typeid(structure) && static_a) {
        static int ident = 0;
        char name[32];
        sprintf(name, "static_arr_%i", ident++);

        i64          ln     = len(static_a);
        model        emdl   = mdl->src;
        LLVMTypeRef  arrTy  = LLVMArrayType(emdl->type, ln);
        LLVMValueRef G      = LLVMAddGlobal(e->module, arrTy, name);
        LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));
        LLVMSetLinkage(G, LLVMInternalLinkage);

        for (i32 i = 0; i < ln; i++) {
            enode n = e_operand(e, static_a->origin[i], null, null);
            verify (LLVMIsConstant(n->value), "static_array must contain constant statements");
            verify (n->mdl == mdl->src, "type mismatch");
            elems[i] = n->value;
        }
        LLVMValueRef init = LLVMConstArray(emdl->type, elems, ln);
        LLVMSetInitializer(G, init);
        LLVMSetGlobalConstant(G, 1);
        free(elems);
        res = enode(mod, e, value, G, mdl, mdl, meta, meta);

    } else if (cmdl) {
        // we have to call array with an intialization property for size, and data pointer
        // if the data is pre-defined in init and using primitives, it has to be stored prior to this call
        enode metas_node = e_meta_ids(e, meta);
        verify(!_is_struct(e), "inappropriate use of struct, they cannot be given to alloc");
        res = e_fn_call(e, f_alloc, null, a( e_typeid(e, mdl), _i32(1), metas_node ));
        res->mdl = mdl; // we need a general cast method that does not call function

        bool is_array = cmdl && cmdl->parent == emodel("array");
        if (imap) {
            
            pairs(imap, i) {
                emember m = find_member(mdl, i->key, null);
                verify(m, "prop %o not found in %o", i->key, mdl);
                verify(isa(m) != typeid(function), "%o (function) cannot be initialized", i->key);
                enode   i_value = e_operand(e, i->value, m->mdl, m->meta);
                emember i_prop  = access(res, i->key);
                e_assign(e, i_prop, i_value, OPType__assign);
            }

            // this is a static method, with a target of sort, but its not a real target since its not a real instance method
            e_fn_call(e, f_initialize, null, a(res)); // required logic need not emit ops to set the bits when we can check at design time
            
        } else if (a) { // if array is given for args

            // if we are CREATING an array
            if (is_array) {
                int   ln = len(a);
                enode n  = e_operand(e, _i64(ln), emodel("i64"), null);
                bool all_const = ln > 0;
                enode const_array = null;
                for (int i = 0; i < ln; i++) {
                    enode node = instanceof(a->origin[i], typeid(enode));
                    if (node && node->literal)
                        continue;
                    all_const = false;
                    break;
                }
                if (all_const) {
                    model ptr = pointer(mdl->src);
                    LLVMTypeRef   elem_ty = mdl->src->type;   // base element type
                    LLVMValueRef *elems   = malloc(sizeof(LLVMValueRef) * ln);
                    for (int i = 0; i < ln; i++) {
                        enode node = a->origin[i];
                        elems[i]   = node->value;
                    }
                    LLVMValueRef const_arr = LLVMConstArray(elem_ty, elems, ln);
                    free(elems);
                    static int ident = 0;
                    char vname[32];
                    sprintf(vname, "const_arr_%i", ident++);
                    LLVMValueRef glob = LLVMAddGlobal(e->module, LLVMTypeOf(const_arr), vname);
                    LLVMSetLinkage(glob, LLVMInternalLinkage);
                    LLVMSetGlobalConstant(glob, 1);
                    LLVMSetInitializer(glob, const_arr);
                    const_array = enode(mod, e, mdl, ptr, meta, null, value, glob);
                }
                enode prop_alloc     = access(res, string("alloc"));
                enode prop_unmanaged = access(res, string("unmanaged"));
                e_assign(e, prop_alloc, n, OPType__assign);
                if (const_array) {
                    enode tru = e_operand(e, _bool(ln), emodel("bool"), null);
                    e_assign(e, prop_unmanaged, tru, OPType__assign);
                }     
                e_fn_call(e, f_initialize, null, a(res));
                if (const_array) {
                    function f_push_vdata = find_member( mdl, string("push_vdata"), null)->mdl;
                    e_fn_call(e, f_push_vdata, res, a(const_array, n));
                } else {
                    function f_push = find_member(mdl, string("push"), null)->mdl;
                    for (int i = 0; i < ln; i++) {
                        Au      aa = a->origin[i];
                        enode   n = e_operand(e, aa, mdl->src, null);
                        e_fn_call(e, f_push, res, a(n));
                    }
                }
            } else {
                fault("unsupported instantiation method");
                /*
                // if array given and we are not making an array, this will create fields
                // theres no silver exposure for this, though -- not unless we adopt braces
                // why though, lets just make 1 mention of type and have a language that is unambiguous and easier to read
                array f = field_list(mdl, emodel("Au"), false);
                for (int i = 0, ln = len(a); i < ln; i++) {
                    verify(i < len(f), "too many fields provided for object %o", mdl);
                    emember m = f->origin[i];
                    enode   n = e_operand(e, a->origin[i], m->mdl, m->meta);
                    emember rmem = member_lookup((emember)res, m->name);
                    e_assign(e, res, rmem, OPType__assign);
                }
                e_fn_call(e, f_initialize, res, a());*/
            }
        } else {
            if (ctr) {
                e_fn_call(e, ctr->mdl, res, a(input));
                e_fn_call(e, f_initialize, null, a(res));
            } else {
                verify(false, "expected constructor for type %o", mdl);
            }
        }
    } else if (ctr) {
        verify(isa(mdl) == typeid(structure), "expected structure");
        res = e_fn_call(e, ctr->mdl, res, a(input));
    } else {
        verify(!a, "no translation for array to model %o", mdl);
        bool is_ref_struct = mdl->is_ref && isa(mdl->src) == typeid(structure);

        if (isa(mdl) == typeid(structure) || is_ref_struct) {
            verify(!a, "unexpected array argument");
            if (imap) {
                // check if constants are in map, and order fields
                record rmdl = is_ref_struct ? model_resolve(mdl->src) : mdl;
                int field_count = LLVMCountStructElementTypes(rmdl->type);
                LLVMValueRef *fields = calloc(field_count, sizeof(LLVMValueRef));
                array field_indices = array(field_count);
                array field_names   = array(field_count);
                array field_values  = array(field_count);
                bool  all_const     = e->no_const ? false : true;

                pairs(imap, i) {
                    string  k = i->key;
                    Au_t   t = isa(i->value);
                    emember m = find_member(rmdl, i->key, null);
                    i32 index = m->index;

                    enode value = e_operand(e, i->value, m->mdl, m->meta);
                    if (all_const && !LLVMIsConstant(value->value))
                        all_const = false;

                    //verify(LLVMIsConstant(value->value), "non-constant field in const struct");
                    
                    push(field_indices, _i32(index));
                    push(field_names,   m->name);
                    push(field_values,  value);
                }

                // iterate through fields, associating the indices with values and struct member type
                // for unspecified values, we create an explicit null
                for (int i = 0; i < field_count; i++) {
                    enode value = null;
                    model field_type = null;
                    for (int our_index = 0; our_index < field_count; our_index++) {
                        i32* f_index = (i32*)field_indices->origin[our_index];
                        if  (f_index && *f_index == i) {
                            value = (enode)field_values->origin[our_index];
                            break;
                        }
                    }
                    pairs (rmdl->members, ii) {
                        emember smem = ii->value;
                        if (smem->index == i) {
                            field_type = smem->mdl;
                            break;
                        }
                    } // string and cstr strike again -- it should be calling the constructor on string for this, which has been enumerated by Au-type already
                    verify(field_type, "field type lookup failed for %o (index = %i)",
                        model_source(mdl)->mem->name, i);
                    emember m0 = value_by_index(rmdl->members, 0);
                    Au_t type_m0 = isa(m0); // this is string for our struct member at 0 (string str2)
                    LLVMTypeRef tr = LLVMStructGetTypeAtIndex(rmdl->type, i);
                    model f = _is_class(field_type);
                    LLVMTypeRef expect_ty = f ? f->type : field_type->type;
                    verify(expect_ty == tr, "field type mismatch");
                    if (!value) value = e_null(e, field_type);
                    fields[i] = value->value;
                }

                if (all_const) {
                    print("all are const, writing %i fields for %o", field_count, mdl);
                    LLVMValueRef s_const = LLVMConstNamedStruct(mdl->type, fields, field_count);
                    res = enode(mod, e, value, s_const, mdl, mdl, meta, meta);
                } else {
                    print("non-const, writing build instructions, %i fields for %o", field_count, mdl);
                    res = enode(mod, e, value, LLVMBuildAlloca(e->builder, mdl->type, "alloca-mdl"), mdl, mdl, meta, null);
                    res = e_zero(e, res);
                    for (int i = 0; i < field_count; i++) {
                        if (!LLVMIsNull(fields[i])) {
                            LLVMValueRef gep = LLVMBuildStructGEP2(e->builder, mdl->type, res->value, i, "");
                            LLVMBuildStore(e->builder, fields[i], gep);
                        }
                    }
                }

                free(fields);

                /*
                pairs(imap, i) {
                    string  k = i->key;
                    Au_t t = isa(i->value);
                    //print("%o -> %o", k, i->value ? (cstr)isa(i->value)->ident : "null");
                    emember m = find_member(mdl, i->key, null);
                    verify(m, "prop %o not found in %o", mdl, i->key);
                    verify(isa(m) != typeid(function), "%o (function) cannot be initialized", i->key);
                    enode i_value = e_operand(e, i->value, m->mdl, m->meta)  ; // for handle with a value of 0, it must create a e_null
                    emember i_prop  = resolve((emember)res, i->key) ;
                    e_assign(e, i_prop, i_value, OPType__assign);
                }*/

            } else if (ctr) {
                e_fn_call(e, ctr, res, a(input));
            }
        } else 
            res = e_operand(e, args, mdl, meta);
    }
    return res;
}

enode aether_e_default_value(aether e, model mdl, array meta) {
    void* ptr = aether_e_create;
    
    return aether_e_create(e, mdl, meta, null);
}


enode aether_e_zero(aether e, enode n) {
    model      mdl = n->mdl;
    LLVMValueRef v = n->value;
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, mdl);
    LLVMValueRef zero   = LLVMConstInt(LLVMInt8Type(), 0, 0);          // value for memset (0)
    LLVMValueRef size   = LLVMConstInt(LLVMInt64Type(), mdl->size_bits / 8, 0); // size of alloc
    LLVMValueRef memset = LLVMBuildMemSet(e->builder, v, zero, size, 0);
    return n;
}


model prefer_mdl(model m0, model m1, array meta0, array meta1, array* rmeta) {
    aether e = m0->mod;
    if (m0 == m1) {
        *rmeta = meta0;
        return m0;
    }
    model g = emodel("any");
    if (m0 == g) {
        *rmeta = meta1;
        return m1;
    }
    if (m1 == g) {
        *rmeta = meta0;
        return m0;
    }
    if (model_inherits(m0, m1)) {
        *rmeta = meta1;
        return m1;
    }
    *rmeta = meta0;
    return m0;
}

emember model_member_lookup(model a, string name, Au_t mtype) {
    if (!a->members) return null;
    pairs (a->members, i) {
        emember m = i->value;
        if (eq(m->name, name->chars) && inherits(isa(m->mdl), mtype))
            return m;
    }
    return null;
}


enode aether_e_ternary(aether e, enode cond_expr, enode true_expr, enode false_expr) {
    aether mod = e;
    array rmeta = null;
    model rmdl  = prefer_mdl(
        true_expr->mdl, false_expr->mdl, true_expr->meta, false_expr->meta, &rmeta);

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, rmdl);

    // Step 1: Create the blocks for the ternary structure
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(mod->builder);
    LLVMBasicBlockRef then_block    = LLVMAppendBasicBlock(current_block, "ternary_then");
    LLVMBasicBlockRef else_block    = LLVMAppendBasicBlock(current_block, "ternary_else");
    LLVMBasicBlockRef merge_block   = LLVMAppendBasicBlock(current_block, "ternary_merge");

    // Step 2: Build the conditional branch based on the condition
    LLVMValueRef condition_value = cond_expr->value;
    LLVMBuildCondBr(mod->builder, condition_value, then_block, else_block);

    // Step 3: Handle the "then" (true) branch
    LLVMPositionBuilderAtEnd(mod->builder, then_block);
    LLVMValueRef true_value = true_expr->value;
    LLVMBuildBr(mod->builder, merge_block);  // Jump to merge block after the "then" block

    // Step 4: Handle the "else" (false) branch
    LLVMPositionBuilderAtEnd(mod->builder, else_block);
    LLVMValueRef false_value = false_expr->value;
    LLVMBuildBr(mod->builder, merge_block);  // Jump to merge block after the "else" block

    // Step 5: Build the "merge" block and add a phi enode to unify values
    LLVMPositionBuilderAtEnd(mod->builder, merge_block);
    LLVMTypeRef result_type = LLVMTypeOf(true_value);
    LLVMValueRef phi_node = LLVMBuildPhi(mod->builder, result_type, "ternary_result");
    LLVMAddIncoming(phi_node, &true_value, &then_block, 1);
    LLVMAddIncoming(phi_node, &false_value, &else_block, 1);

    // Return some enode or result if necessary (e.g., a enode indicating the overall structure)
    return enode(mod, mod, mdl, rmdl, meta, rmeta, value, null);
}

enode aether_e_builder(aether e, subprocedure cond_builder) {
    if (!e->no_build) {
        LLVMBasicBlockRef block = LLVMGetInsertBlock(e->builder);
        LLVMPositionBuilderAtEnd(e->builder, block);
    }
    enode   n = invoke(cond_builder, null);
    return n;
}

enode aether_e_native_switch(
        aether          e,
        enode           switch_val,
        map             cases,
        array           def_block,
        subprocedure    expr_builder,
        subprocedure    body_builder)
{
    LLVMBuilderRef B = e->builder;
    LLVMTypeRef    Ty = LLVMTypeOf(switch_val->value);
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(e->builder);

    catcher switch_cat = catcher(mod, e,
        block, LLVMAppendBasicBlock(entry, "switch.end"));
    push(e, switch_cat);

    LLVMBasicBlockRef default_block =
        def_block
            ? LLVMAppendBasicBlock(LLVMGetInsertBlock(B), "default")
            : switch_cat->block;

    // create switch instruction
    LLVMValueRef SW =
        LLVMBuildSwitch(B, switch_val->value, default_block, cases->count);

    // allocate blocks for each case BEFORE emitting bodies
    map case_blocks = map(hsize, 16);

    Au_t common_type = null;
    pairs(cases, i) {
        LLVMBasicBlockRef case_block =
            LLVMAppendBasicBlock(LLVMGetInsertBlock(B), "case");

        set(case_blocks, i->key, (Au)case_block);

        // evaluate key to literal
        enode key_node = invoke(expr_builder, i->key);
        Au   au_value = key_node->literal;

        verify(au_value, "expression not evaluating as constant (%o)", i->key);
        Au_t au_type  = isa(au_value);
        if (common_type && common_type != au_type) {
            fault("type %s differs from common type of %s", au_type->ident, common_type->ident);
        }
        verify(!(au_type->traits & AU_TRAIT_REALISTIC), "realistic types not supported in switch");

        i64 key_val;
             if (au_type == typeid(bool)) key_val = *(bool*)au_value;
        else if (au_type == typeid(i8))   key_val = *(i8*) au_value;
        else if (au_type == typeid(u8))   key_val = *(u8*) au_value;
        else if (au_type == typeid(i16))  key_val = *(i16*)au_value;
        else if (au_type == typeid(u16))  key_val = *(u16*)au_value;
        else if (au_type == typeid(i32))  key_val = *(i32*)au_value;
        else if (au_type == typeid(u32))  key_val = *(u32*)au_value;
        else if (au_type == typeid(i64))  key_val = *(i64*)au_value;
        else if (au_type == typeid(u64))  key_val = *(u64*)au_value;
        else {
            fault("type not supported in native switch: %s", au_type->ident);
        }

        LLVMValueRef KeyConst =
            LLVMConstInt(Ty, key_val, false);
        
        LLVMAddCase(SW, KeyConst, case_block);
    }

    // emit each case body
    int idx = 0;
    pairs(case_blocks, p) {
        LLVMBasicBlockRef case_block = p->value;
        LLVMPositionBuilderAtEnd(B, case_block);

        array body_tokens = value_by_index(cases, idx++);
        invoke(body_builder, body_tokens);

        LLVMBuildBr(B, switch_cat->block);
    }

    // default body
    if (def_block) {
        LLVMPositionBuilderAtEnd(B, default_block);
        invoke(body_builder, def_block);
        LLVMBuildBr(B, switch_cat->block);
    }

    // merge
    LLVMPositionBuilderAtEnd(B, switch_cat->block);
    pop(e);
    return e_noop(e, null);
}


enode aether_e_switch(
        aether          e,
        enode           e_expr,        // switch expression tokens
        map             cases,       // map: array_of_tokens → array_of_tokens
        array           def_block,   // null or body array
        subprocedure    expr_builder,
        subprocedure    body_builder)
{
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(e->builder);

    // invoke expression for switch, and push switch cat
    enode   switch_val = e_expr; // invoke(expr_builder, expr);
    catcher switch_cat = catcher(mod, e,
        block, LLVMAppendBasicBlock(entry, "switch.end"));
    push(e, switch_cat);

    // allocate cats for each case, do NOT build the body yet
    // wrap in cat, store the catcher, not an enode
    map case_blocks = map(hsize, 16);
    pairs(cases, i)
        set(case_blocks, i->key, catcher(mod, e, team, case_blocks, block, LLVMAppendBasicBlock(entry, "case")));

    // default block, and obtain insertion block for first case
    catcher def_cat = def_block ? catcher(mod, e, block, LLVMAppendBasicBlock(entry, "default")) : null;
    LLVMBasicBlockRef cur = LLVMGetInsertBlock(e->builder);
    int total = cases->count;
    int idx = 0;

    pairs(case_blocks, i) {
        array   key_expr = i->key;
        catcher case_cat = i->value;

        // position at current end & compare
        LLVMPositionBuilderAtEnd(e->builder, cur);
        enode eq = e_cmp(e, switch_val, key_expr);

        // next block in chain
        LLVMBasicBlockRef next =
            (idx + 1 < total)
                ? LLVMAppendBasicBlock(entry, "case.next")
                : (def_cat ? def_cat->block : switch_cat->block);

        LLVMBuildCondBr(e->builder, eq->value, case_cat->block, next);
        cur = next;
        idx++;
    }

    // ---- CASE bodies ----
    int i = 0;
    pairs(case_blocks, p) {
        catcher case_cat = p->value;

        LLVMPositionBuilderAtEnd(e->builder, case_cat->block);
        array body_tokens = value_by_index(cases, i++);
        invoke(body_builder, body_tokens);

        // we should have a last node, but see return statement returns its own thing, not a return
        // if the case didn’t terminate (break/return), jump to merge
        LLVMBuildBr(e->builder, switch_cat->block);
    }

    // ---- DEFAULT body ----
    if (def_cat) {
        LLVMPositionBuilderAtEnd(e->builder, def_cat->block);
        invoke(body_builder, def_block);
        LLVMBuildBr(e->builder, switch_cat->block);
    }

    // ---- MERGE ----
    LLVMPositionBuilderAtEnd(e->builder, switch_cat->block);
    pop(e);
    return e_noop(e, null);
}




















enode aether_e_for(aether e,
                   array init_exprs,
                   array cond_expr,
                   array body_exprs,
                   array step_exprs,
                   subprocedure init_builder,
                   subprocedure cond_builder,
                   subprocedure body_builder,
                   subprocedure step_builder)
{
    LLVMBasicBlockRef entry   = LLVMGetInsertBlock(e->builder);
    LLVMBasicBlockRef cond    = LLVMAppendBasicBlock(entry, "for.cond");
    LLVMBasicBlockRef body    = LLVMAppendBasicBlock(entry, "for.body");
    LLVMBasicBlockRef step    = LLVMAppendBasicBlock(entry, "for.step");
    LLVMBasicBlockRef merge   = LLVMAppendBasicBlock(entry, "for.end");
    catcher cat = catcher(mod, e, block, merge);
    push(e, cat);

    // ---- init ----
    if (init_exprs)
        invoke(init_builder, init_exprs);

    LLVMBuildBr(e->builder, cond);

    // ---- cond ----
    LLVMPositionBuilderAtEnd(e->builder, cond);

    enode cond_res = invoke(cond_builder, cond_expr);
    LLVMValueRef cond_val = e_create(e, emodel("bool"), null, cond_res)->value;

    LLVMBuildCondBr(e->builder, cond_val, body, merge);

    // ---- body ----
    LLVMPositionBuilderAtEnd(e->builder, body);

    invoke(body_builder, body_exprs);

    LLVMBuildBr(e->builder, step);

    // ---- step ----
    LLVMPositionBuilderAtEnd(e->builder, step);

    if (step_exprs)
        invoke(step_builder, step_exprs);

    LLVMBuildBr(e->builder, cond);

    // ---- end ----
    LLVMPositionBuilderAtEnd(e->builder, merge);
    pop(e);

    return e_noop(e, null);
}






// we need a bit more api so silver can do this with a bit less, this works fine in a generic sense for aether use-case
// we may want a property to make it design-time, though -- for const operations, basic unrolling facility etc
enode aether_e_loop(aether e,
                     array expr_cond,
                     array exprs_iterate,
                     subprocedure cond_builder,
                     subprocedure expr_builder,
                     bool loop_while)   // true = while, false = do-while
{
    LLVMBasicBlockRef entry   = LLVMGetInsertBlock(e->builder);
    LLVMBasicBlockRef cond    = LLVMAppendBasicBlock(entry, "loop.cond");
    LLVMBasicBlockRef iterate = LLVMAppendBasicBlock(entry, "loop.body");

    catcher cat = catcher(mod, e, block, LLVMAppendBasicBlock(entry, "loop.end"));
    push(e, cat);

    // ---- ENTRY → FIRST JUMP ----
    if (loop_while) {
        // while(cond) starts at the condition
        LLVMBuildBr(e->builder, cond);
    } else {
        // do {body} while(cond) starts at the body
        LLVMBuildBr(e->builder, iterate);
    }

    // ---- CONDITION BLOCK ----
    LLVMPositionBuilderAtEnd(e->builder, cond);

    enode cond_result = invoke(cond_builder, expr_cond);
    LLVMValueRef condition = e_create(e, emodel("bool"), null, cond_result)->value;

    LLVMBuildCondBr(e->builder, condition, iterate, cat->block);

    // ---- BODY BLOCK ----
    LLVMPositionBuilderAtEnd(e->builder, iterate);

    invoke(expr_builder, exprs_iterate);

    // After executing the loop body:
    // always jump back to the cond block
    LLVMBuildBr(e->builder, cond);

    // ---- MERGE BLOCK ----
    LLVMPositionBuilderAtEnd(e->builder, cat->block);
    pop(e);
    return e_noop(e, null);
}

enode aether_e_noop(aether e, model mdl) {
    return enode(mdl, mdl ? mdl : emodel("none"), meta, null);
}

enode aether_e_if_else(aether e, array conds, array exprs, subprocedure cond_builder, subprocedure expr_builder) {
    int ln_conds = len(conds);
    verify(ln_conds == len(exprs) - 1 || 
           ln_conds == len(exprs), "mismatch between conditions and expressions");
    
    LLVMBasicBlockRef block = LLVMGetInsertBlock  (e->builder);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlock(block, "ifcont");  // Merge block for the end of if-else chain

    // Iterate over the conditions and expressions
    for (int i = 0; i < ln_conds; i++) {
        // Create the blocks for "then" and "else"
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(block, "then");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(block, "else");

        // Build the condition
        Au cond_obj = conds->origin[i];
        enode cond_result = invoke(cond_builder, cond_obj);  // Silver handles the actual condition parsing and building
        LLVMValueRef condition = e_create(e, emodel("bool"), null, cond_result)->value;

        // Set the sconditional branch
        LLVMBuildCondBr(e->builder, condition, then_block, else_block);

        // Build the "then" block
        LLVMPositionBuilderAtEnd(e->builder, then_block);
        Au expr_obj = exprs->origin[i];
        enode expressions = invoke(expr_builder, expr_obj);  // Silver handles the actual block/statement generation
        LLVMBuildBr(e->builder, merge);

        // Move the builder to the "else" block
        LLVMPositionBuilderAtEnd(e->builder, else_block);
        block = else_block;
    }

    // Handle the fnal "else" (if applicable)
    if (len(exprs) > len(conds)) {
        Au else_expr = exprs->origin[len(conds)];
        invoke(expr_builder, else_expr);  // Process the final else block
        LLVMBuildBr(e->builder, merge);
    }

    // Move the builder to the merge block
    LLVMPositionBuilderAtEnd(e->builder, merge);

    // Return some enode or result if necessary (e.g., a enode indicating the overall structure)
    return enode(mod, e, mdl, emodel("none"), meta, null, value, null);  // Dummy enode, replace with real enode if needed
}

enode aether_e_addr_of(aether e, enode expr, model mdl) {
    model        ref   = pointer(mdl ? mdl : expr->mdl); // this needs to set mdl->type to LLVMPointerType(mdl_arg->type, 0)
    int rcount = _ref_level(ref);
    
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, ref);

    model ref1 = emodel("Au_ts");
    int r1 = _ref_level(ref1);

    emember      m_expr = instanceof(expr, typeid(emember));
    LLVMValueRef value = m_expr ? expr->value :
        LLVMBuildGEP2(e->builder, ref->type, expr->value, NULL, 0, "ref_expr");
    return enode(
        mod,   e,
        value, value,
        explicit_ref, true,
        meta,  null,
        mdl,   ref);
}

enode aether_e_offset(aether e, enode n, Au offset) {
    emember mem = instanceof(n, typeid(emember));
    model  mdl = typed(n->mdl);
    enode   i;
    
    if (instanceof(offset, typeid(array))) {
        array args       = offset;
        verify(n->mdl->src, "no source on array");
        model array_info = n->mdl;
        array shape      = array_info->shape;
        i64   len_args   = len(args);
        i64   len_shape  = shape ? len(shape) : 0;
        verify((len_args > 0 && len_args == len_shape) || (len_args == 1 && len_shape == 0),
            "arg count does not match the shape");
        i = e_operand(e, _i64(0), null, null);
        for (int a = 0; a < len(args); a++) {
            Au arg_index   = args->origin[a];
            Au shape_index = shape->origin[a];
            enode   stride_pos  = e_mul(e, shape_index, arg_index);
            i = e_add(e, i, stride_pos);
        }
    } else
        i = e_operand(e, offset, null, null);

    verify(mdl->is_ref, "offset requires pointer");

    LLVMValueRef ptr_load   = LLVMBuildLoad2(e->builder,
        LLVMPointerType(mdl->type, 0), n->value, "load");
    LLVMValueRef ptr_offset = LLVMBuildGEP2(e->builder,
         mdl->type, ptr_load, &i->value, 1, "offset");

    return enode(mod, e, mdl, mdl, meta, null, value, ptr_offset);
}

/*

    enum_value  (E,T,Y, not_equals,       1) \
    enum_value  (E,T,Y, u_greater_than,   2) \
    enum_value  (E,T,Y, u_greater_than_e, 3) \
    enum_value  (E,T,Y, u_less_than,      4) \
    enum_value  (E,T,Y, u_less_than_e,    5) \
    enum_value  (E,T,Y, s_greater_than,   6) \
    enum_value  (E,T,Y, s_greater_than_e, 7) \
    enum_value  (E,T,Y, s_less_than,      8) \
    enum_value  (E,T,Y, s_less_than_e,    9)

*/

/*
enode aether_e_cmp(aether e, comparison cmp, enode lhs, enode rhs) {
    LLVMIntPredicate pre = 0;

    switch (cmp) {
        case comparison_equals:             pre = LLVMIntEQ;  break;
        case comparison_not_equals:         pre = LLVMIntNE;  break;
        case comparison_u_greater_than:     pre = LLVMIntUGT; break;
        case comparison_u_greater_than_e:   pre = LLVMIntUGE; break;
        case comparison_u_less_than:        pre = LLVMIntULT; break;
        case comparison_u_less_than_e:      pre = LLVMIntULE; break;
        case comparison_s_greater_than:     pre = LLVMIntSGT; break;
        case comparison_s_greater_than_e:   pre = LLVMIntSGE; break;
        case comparison_s_less_than:        pre = LLVMIntSLT; break;
        case comparison_s_less_than_e:      pre = LLVMIntSLE; break;
    }
    return LLVMBuildICmp(
        mod->builder, pre, lhs->value, rhs->value, "cmp");
}*/

enode aether_e_load(aether e, emember mem, emember target) {
    // we can use the e->in_ref for pointer lookup
    model mdl = mem->mdl;

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, mdl->src);

    if (mem->is_const) return mem;

    LLVMValueRef ptr      = mem->value;
    verify(mdl->src, "expected pointer to load from, given %o", mdl);
    string     label     = form(string, "load-emember-%o", mem->name);
    bool       use_value = mem->is_arg || e->left_hand || e->in_ref;
    LLVMValueRef res     = use_value ? mem->value :
        LLVMBuildLoad2(e->builder, mdl->src->type, mem->value, cstring(label));

    emember r = emember(mod, e, value, res, mdl, mdl->src, meta, mem->meta);
    r->loaded = true;
    return r;
}

/// general signed/unsigned/1-64bit and float/double conversion
/// should NOT be loading, should absolutely be calling model_convertible -- why is it not?
enode aether_e_primitive_convert(aether e, enode expr, model rtype) {
    if (!rtype) return expr;

    e->is_const_op &= expr->mdl == rtype; // we may allow from-bit-width <= to-bit-width
    if (e->no_build) return e_noop(e, rtype);

    //expr = e_load(e, expr, null); // i think we want to place this somewhere else for better structural use
    
    model        F = typed(expr->mdl);
    model        T = typed(rtype);
    LLVMValueRef V = expr->value;

    if (F == T) return expr;  // no cast needed

    // LLVM type kinds
    LLVMTypeKind F_kind = LLVMGetTypeKind(F->type);
    LLVMTypeKind T_kind = LLVMGetTypeKind(T->type);
    LLVMBuilderRef B = e->builder;

    // integer conversion
    if (F_kind == LLVMIntegerTypeKind &&  T_kind == LLVMIntegerTypeKind) {
        u32 F_bits = LLVMGetIntTypeWidth(F->type), T_bits = LLVMGetIntTypeWidth(T->type);
        if (F_bits < T_bits) {
            bool is_sign = is_signed(F);
            V = is_sign ? LLVMBuildSExt(B, V, T->type, "sext")
                        : LLVMBuildZExt(B, V, T->type, "zext");
        } else if (F_bits > T_bits)
            V = LLVMBuildTrunc(B, V, T->type, "trunc");
        else if (is_signed(F) != is_signed(T))
            V = LLVMBuildIntCast2(B, V, T->type, is_signed(T), "int-cast");
        else
            V = expr->value;
    }

    // int to real
    else if (F_kind == LLVMIntegerTypeKind && (T_kind == LLVMFloatTypeKind || T_kind == LLVMDoubleTypeKind))
        V = is_signed(F) ? LLVMBuildSIToFP(B, V, T->type, "sitofp")
                         : LLVMBuildUIToFP(B, V, T->type, "uitofp");

    // real to int
    else if ((F_kind == LLVMFloatTypeKind || F_kind == LLVMDoubleTypeKind) && T_kind == LLVMIntegerTypeKind)
        V = is_signed(T) ? LLVMBuildFPToSI(B, V, T->type, "fptosi")
                         : LLVMBuildFPToUI(B, V, T->type, "fptoui");

    // real conversion
    else if ((F_kind == LLVMFloatTypeKind || F_kind == LLVMDoubleTypeKind) && 
             (T_kind == LLVMFloatTypeKind || T_kind == LLVMDoubleTypeKind))
        V = F_kind == LLVMDoubleTypeKind && T_kind == LLVMFloatTypeKind ? 
            LLVMBuildFPTrunc(B, V, T->type, "fptrunc") :
            LLVMBuildFPExt  (B, V, T->type, "fpext");

    // ptr conversion
    else if (_is_ref(F) && _is_ref(T))
        V = LLVMBuildPointerCast(B, V, T->type, "ptr_cast");

    // ptr to int
    else if (_is_ref(F) && is_integral(T))
        V = LLVMBuildPtrToInt(B, V, T->type, "ptr_to_int");

    // int to ptr
    else if (is_integral(F) && _is_ref(T))
        V = LLVMBuildIntToPtr(B, V, T->type, "int_to_ptr");

    // bitcast for same-size types
    else if (F_kind == T_kind)
        V = LLVMBuildBitCast(B, V, T->type, "bitcast2");

    else if (F_kind == LLVMVoidTypeKind)
        V = LLVMConstNull(T->type);
    else
        fault("unsupported cast");

    enode res = value(T,null,V);
    res->literal = hold(expr->literal);
    return res;
}

// this should give us enough dimensionality to control for
// class, structure with eargs or fn on top (including inits for record or module)
none aether_context(aether e, ARef record, ARef top_model, Au_t filter) {
    model f = null;
    if (record)    *record    = null;
    if (top_model) *top_model = null;

    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model mdl = e->lex->origin[i];
        Au_t t = isa(mdl);
        Au_t s = isa(mdl->src);
        bool allow = (!filter || isa(mdl) == filter);

        if (allow && top_model && !*top_model) {
            if (t == typeid(statements))
                *(model*)top_model = mdl;

            if (t == typeid(enumeration))
                *(model*)top_model = mdl;

            if (t == typeid(eargs))
                *(model*)top_model = mdl;

            if (t == typeid(function))
                *(model*)top_model = mdl;

            if (mdl->is_global && mdl->open)
                *(model*)top_model = mdl;
        }

        if (record && !*record) {
            if (t == typeid(structure)) {
                *(model*)record = mdl;
                if (!*top_model && !filter) {
                    *top_model = mdl;
                }
            }

            if (s == typeid(Class)) {
                *(model*)record = mdl; // important to note this is the ptr (a model, not Class)
                if (!*top_model && !filter) {
                    *top_model = mdl;
                }
            }
        }
    }
}

/*
model aether_context_model(aether e, Au_t type) {
    Au_t cl = typeid(Class);
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->origin[i];
        Au_t ctx_t = isa(ctx);
        if ((type == cl && inherits(isa(ctx->src), type)) || 
            (type != cl && inherits(ctx_t, type)))
            return ctx;
        if (ctx == e)
            return null;
    }
    return null;
}
*/

model aether_return_type(aether e, ARef meta) {
    if (meta) *meta = null;
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->origin[i];
        if (isa(ctx) == typeid(function) && ((function)ctx)->rtype) {
            if (meta) *meta = (ARef)((function)ctx)->rmeta;
            return ((function)ctx)->rtype;
        }
    }
    return null;
}

void assign_args(aether e, enode L, Au R, enode* r_L, enode* r_R) {
    *r_R = e_operand(e, R, null, null);
    emember Lm = instanceof(L, typeid(emember));
    if (Lm && !Lm->is_const)
        *r_L = L;//load(e, Lm);
    else
        *r_L = L;
}

struct op_entry {
    LLVMValueRef(*f_build_op)(LLVMBuilderRef, LLVMValueRef L, LLVMValueRef R, symbol);
    LLVMValueRef(*f_const_op)(LLVMValueRef L, LLVMValueRef R);
};

LLVMValueRef LLVMConstMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstSDiv(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstUDiv(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstURem(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstSRem(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstShl(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstLShr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstAShr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstAnd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstOr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstXor(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);

static struct op_entry op_table[] = {
    { LLVMBuildAdd,  LLVMConstAdd  },
    { LLVMBuildSub,  LLVMConstSub  }, 
    { LLVMBuildMul,  LLVMConstMul  }, 
    { LLVMBuildSDiv, LLVMConstSDiv },
    { LLVMBuildOr,   LLVMConstOr   },  // logical or
    { LLVMBuildAnd,  LLVMConstAnd  }, // logical and
    { LLVMBuildOr,   LLVMConstOr   },  // bitwise or
    { LLVMBuildAnd,  LLVMConstAnd  }, // bitwise and
    { LLVMBuildXor,  LLVMConstXor  },  
    { LLVMBuildURem, LLVMConstURem },
    { LLVMBuildAShr, LLVMConstAShr },
    { LLVMBuildShl,  LLVMConstShl  }
};

enode aether_e_assign(aether e, enode L, Au R, OPType op) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, null); // should never happen in an expression context

    int v_op = op;
    verify(op >= OPType__assign && op <= OPType__assign_left, "invalid assignment-operator");
    enode rL, rR = e_operand(e, R, null, null);
    enode res = rR;
    
    if (op != OPType__assign) {
        rL = e_load(e, L, null);
        res = value(L->mdl, L->meta,
            op_table[op - OPType__assign - 1].f_build_op
                (e->builder, rL->value, rR->value, e_str(OPType, op)->chars));
    }

    verify(L->mdl->is_ref, "L-value not a pointer (cannot assign to value)");
    if (res->mdl != L->mdl->src) {
        res = e_operand(e, res, L->mdl, L->meta);
        //res = e_convert(e, res, L->mdl);
    }

    LLVMBuildStore(e->builder, res->value, L->value);
    return res;
}


// look up a emember in lexical scope
// this applies to models too, because they have membership as a type entry
emember aether_lookup2(aether e, Au name, Au_t mdl_type_filter) {
    if (!name) return null;
    if (instanceof(name, typeid(token)))
        name = cast(string, (token)name);

    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx      = e->lex->origin[i];
        Au_t  ctx_type = isa(ctx);
        cstr  n = cast(cstr, (string)name);
        
        do {
            // member registration should create namespace; which we dont technically have past one level
            emember m = ctx->members ? get(ctx->members, string(n)) : null;
            if (m) {
                Au_t mdl_type = isa(m->mdl);
                if (mdl_type_filter) {
                    if (mdl_type != mdl_type_filter)
                        goto nxt;
                }
                return m;
            }
            nxt:
            if (instanceof(ctx->src, typeid(Class)))
                ctx = ctx->src->parent;
            else
                break;
        } while (ctx);
    }
    return null;
}

model aether_push(aether e, model mdl) {
    verify(isa(mdl) != typeid(Class), "must push the class->ptr");
    record   rec_prev;
    function fn_prev;
    context(e, &rec_prev, &fn_prev, typeid(function));
    if (fn_prev) {
        fn_prev->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);
    }

    verify(mdl, "no context given");
    function f = instanceof(mdl, typeid(function));
    if (f)
        finalize(f->mem);

    push(e->lex, mdl);
    e->top = mdl;

    if (!mdl->members) mdl->members = hold(map(hsize, 8));
    
    if (f && f->entry) {
        LLVMPositionBuilderAtEnd(e->builder, f->entry);
        if (LLVMGetBasicBlockTerminator(f->entry) == NULL) {
            LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
                e->module_ctx, f->mem->name->line, 0, f->scope, NULL);
            LLVMSetCurrentDebugLocation2(e->builder, loc);
        } else if (f->last_dbg)
            LLVMSetCurrentDebugLocation2(e->builder, f->last_dbg);
    } 
    return mdl;
}

none emember_release(emember mem) {
    aether e = mem->mod;
    model mdl = mem->mdl;
    if (!mdl->is_ref)
        return;

    e->is_const_op = false;
    if (e->no_build) return;

    // Compute the base pointer (reference data is 32 bytes before `user`)
    LLVMValueRef size_A = LLVMConstInt(LLVMInt64Type(), 32, false);
    LLVMValueRef ref_ptr = LLVMBuildGEP2(e->builder, LLVMInt8Type(), mem->value, &size_A, -1, "ref_ptr");

    // Cast to i64* to read the reference count
    ref_ptr = LLVMBuildBitCast(e->builder, ref_ptr, LLVMPointerType(LLVMInt64Type(), 0), "ref_count_ptr");

    // Load the current reference count
    LLVMValueRef ref_count = LLVMBuildLoad2(e->builder, LLVMInt64Type(), ref_ptr, "ref_count");

    // Decrement the reference count
    LLVMValueRef new_ref_count = LLVMBuildSub(
        e->builder,
        ref_count,
        LLVMConstInt(LLVMInt64Type(), 1, false),
        "decrement_ref"
    );

    // Store the decremented reference count back
    LLVMBuildStore(e->builder, new_ref_count, ref_ptr);

    // Check if the reference count is less than zero
    LLVMValueRef is_less_than_zero = LLVMBuildICmp(
        e->builder,
        LLVMIntSLT,
        new_ref_count,
        LLVMConstInt(LLVMInt64Type(), 0, false),
        "is_less_than_zero"
    );

    // Conditional free if the reference count is < 0
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(e->builder);
    LLVMBasicBlockRef free_block = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(current_block), "free_block");
    LLVMBasicBlockRef no_free_block = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(current_block), "no_free_block");

    LLVMBuildCondBr(e->builder, is_less_than_zero, free_block, no_free_block);

    // Free block: Add logic to free the memory
    LLVMPositionBuilderAtEnd(e->builder, free_block);
    function f_dealloc = find_member(mem->mdl, string("dealloc"), null)->mdl;
    if (f_dealloc) {
        e_fn_call(e, f_dealloc, mem, a());
    }
    LLVMBuildFree(e->builder, ref_ptr);
    LLVMBuildBr(e->builder, no_free_block);

    // No-free block: Continue without freeing
    LLVMPositionBuilderAtEnd(e->builder, no_free_block);
}

model aether_pop(aether e) {
    statements st = instanceof(top(e), typeid(statements));
    if (st) {
        pairs(st->members, i) {
            emember mem = i->value;
            release(mem); // this member is added to statements when its already a member of record in context
        }
    }
    record prev_rec_ctx;
    function prev_fn;
    context(e, &prev_rec_ctx, &prev_fn, typeid(function));

    if (prev_fn && !e->no_build)
        prev_fn->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);

    pop(e->lex);
    e->top = last_element(e->lex);

    record   rec_ctx;
    function fn_ctx;
    context(e, &rec_ctx, &fn_ctx, typeid(function));
    if (fn_ctx && (fn_ctx != prev_fn) && !e->no_build) {
        LLVMPositionBuilderAtEnd(e->builder, fn_ctx->entry);
        LLVMSetCurrentDebugLocation2(e->builder, fn_ctx->last_dbg);
    }
    return e->top;
}

model model_typed(model a) {
    model mdl = a;
    while (mdl && !mdl->type) {
        // this only needs to happen if the type fails to resolve for Au-type based src; which, should not really be possible
        //if (mdl->src && isa(mdl->src) == 0)
        //    break; // this is a bit wonky, should be addressed by separating the types
        mdl = mdl->src;
    }
    return mdl;
}

emember aether_initializer(aether e) {
    verify(e, "model given must be module (aether-based)");

    e->is_global = true;
    function fn_init = function(
        mod,      e,
        is_module_init, true,
        args,     eargs(mod, e));

    e->mem_init = register_model(e, fn_init,
        f(string, "_%o_module_init", e), true);
    use(fn_init);

    return e->mem_init;
}

void enumeration_init(enumeration mdl) {
    aether e = mdl->mod;
    mdl->src  = mdl->src ? mdl->src : emodel("i32");
    mdl->type = mdl->src->type;
}

void aether_build_info(aether e, path install) {
    if (e->install != install)
        e->install = install;

    e->include_paths    = a(f(path, "%o/include", install));
    e->sys_inc_paths    = a();
    e->sys_exc_paths    = a();

#ifdef _WIN32
    e->sys_inc_paths = a(
        f(path, "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/include"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/ucrt"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/shared"));
    e->lib_paths = a(
        f(path, "%o/bin"),
        f(path, "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/lib/x64"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/ucrt/x64"),
        f(path, "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"));
#elif defined(__linux)
    e->sys_inc_paths = a(f(path, "/usr/include"), f(path, "/usr/include/x86_64-linux-gnu"));
    e->lib_paths     = a();
#elif defined(__APPLE__)

  //-internal-isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/c++/v1
  //-internal-isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/local/include
  //-internal-isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/14.0.3/include
  //-internal-externc-isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include
  //-internal-externc-isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include

    string sdk          = run("xcrun --show-sdk-path");
    string toolchain    = f(string, "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain"); // run("xcrun --show-toolchain-path");
    
    e->isystem          =   f(path, "%o/usr/include", toolchain);
    e->sys_inc_paths    = a(f(path, "%o/usr/include", toolchain),
                            f(path, "%o/usr/local/include", sdk),
                            f(path, "%o/usr/lib/clang/14.0.3/include", toolchain));
    e->sys_exc_paths    = a(f(path, "%o/usr/include", sdk),
                            f(path, "%o/usr/include", toolchain));
    e->lib_paths        = a(f(path, "%o/usr/lib", sdk));
    e->framework_paths  = a(f(path, "%o/System/Library/Frameworks", sdk));
    e->isysroot         =   f(path, "%o/", sdk);
    e->resource_dir     =   f(path, "%o/usr/lib/clang/14.0.3", toolchain);

#endif

    // include our own clang as part of system, but include after the os
    //push(e->include_paths, f(path, "%o/lib/clang/22/include", e->install));
    
    // add silver lib path (no differentiation between user and system, but we order after system)
    push(e->lib_paths,     f(path, "%o/lib",     e->install));

    // finally the path folder of the users source must be added as an include path
    path src_path = parent_dir(e->source);
    push(e->include_paths, src_path);
}

// very undocumented process here -- the means of looking up intrinsics is simply not documented
none aether_e_memcpy(aether e, enode _dst, enode _src, model msize) {
    e->is_const_op = false;
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMValueRef dst  = LLVMBuildBitCast(e->builder, _dst->value, i8ptr, "dst");
    LLVMValueRef src  = LLVMBuildBitCast(e->builder, _src->value, i8ptr, "src");
    LLVMValueRef sz   = LLVMConstInt(LLVMInt64Type(), msize->size_bits / 8, 0);
    LLVMValueRef align = LLVMConstInt(LLVMInt32Type(), 8, 0); // your alignment
    LLVMBuildMemCpy(e->builder, dst, 0, src, 0, sz);
}

void aether_llflag(aether e, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32Type(), ival, 0));

    char sflag[64];
    memcpy(sflag, flag, strlen(flag) + 1);
    LLVMAddModuleFlag(e->module, LLVMModuleFlagBehaviorError, sflag, strlen(sflag), v);
}


bool aether_emit(aether e, ARef ref_ll, ARef ref_bc) {
    path* ll = ref_ll;
    path* bc = ref_bc;
    cstr err = NULL;

    path c = path_cwd();

    *ll = form(path, "%o.ll", e);
    *bc = form(path, "%o.bc", e);

    if (LLVMPrintModuleToFile(e->module, cstring(*ll), &err))
        fault("LLVMPrintModuleToFile failed");

    if (LLVMVerifyModule(e->module, LLVMPrintMessageAction, &err))
        fault("error verifying module");
    
    if (LLVMWriteBitcodeToFile(e->module, cstring(*bc)) != 0)
        fault("LLVMWriteBitcodeToFile failed");

    return true;
}


void aether_llvm_init(aether e) {
    e->lex            = array(32);
    //e->type_refs    = map(hsize, 64);
    e->module         = LLVMModuleCreateWithName(e->mem->name->chars);
    e->module_ctx     = LLVMGetModuleContext(e->module);
    e->dbg_builder    = LLVMCreateDIBuilder(e->module);
    e->builder        = LLVMCreateBuilderInContext(e->module_ctx);
    e->target_triple  = LLVMGetDefaultTargetTriple();

    cstr err = NULL;
    if (LLVMGetTargetFromTriple(e->target_triple, &e->target, &err))
        fault("error: %s", err);
    e->target_machine = LLVMCreateTargetMachine(
        e->target, e->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    e->target_data = LLVMCreateTargetDataLayout(e->target_machine);
    llflag(e, "Dwarf Version",      5);
    llflag(e, "Debug Info Version", 3);

    string src_file =      filename (e->source);
    string src_path = cast(string, directory(e->source));
    e->file = LLVMDIBuilderCreateFile( // create e file reference (the source file for debugging)
        e->dbg_builder,
        cast(cstr, src_file), cast(sz, src_file),
        cast(cstr, src_path), cast(sz, src_path));
    
    e->compile_unit = LLVMDIBuilderCreateCompileUnit(
        e->dbg_builder, LLVMDWARFSourceLanguageC, e->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);

    e->scope = e->compile_unit; /// this var is not 'current scope' on silver, its what silver's scope is initialized as

    path  full_path = form(path, "%o/%o", src_path, src_file);
    verify(exists(full_path), "source (%o) does not exist", full_path);
    e->builder = LLVMCreateBuilderInContext(e->module_ctx);

    // for silver, jit is probably misguided.
    // it does not need another test environment for all developers
    //if (LLVMCreateExecutionEngineForModule(&e->jit, e->module, &err))
    //    fault("failed to create execution engine: %s", err);
}

// enumerate everything from object.h (plus some needed primitive)
static void register_vbasics(aether e) {
    model mdl_glyph  = model(mod, e, atype_src, typeid(i8), is_const, true);
    model mdl_symbol = model(mod, e, 
        atype_src, typeid(symbol), is_ref, true, src, mdl_glyph);
    
    mdl_glyph->ptr = hold(mdl_symbol);
    model mdl_i8     = model(mod, e, atype_src, typeid(i8));
    model mdl_i32    = model(mod, e, atype_src, typeid(i32));
    model mdl_u8     = model(mod, e, atype_src, typeid(u8));
    model mdl_ref_u8 = model(mod, e, atype_src, typeid(ref_u8), src, mdl_u8, is_ref, true);
    mdl_u8->ptr = hold(mdl_ref_u8);

    model mdl_handle = model(mod, e, atype_src, typeid(handle), src, mdl_ref_u8);

    model mdl_cstr   = model(mod, e, atype_src, typeid(cstr), src, mdl_i8);
    mdl_i8->ptr = hold(mdl_cstr);

    register_model(e, mdl_glyph,  string("glyph"),  true);
    typeid(i8)->user     = register_model(e, mdl_i8,     string("i8"),     true)->mdl;
    typeid(u8)->user     = register_model(e, mdl_u8,     string("u8"),     true)->mdl;
    typeid(i32)->user    = register_model(e, mdl_i32,    string("i32"),    true)->mdl;
    typeid(ref_u8)->user = register_model(e, mdl_ref_u8, string("ref_u8"), true)->mdl;
    typeid(handle)->user = register_model(e, mdl_handle, string("handle"), true)->mdl;
    typeid(symbol)->user = register_model(e, mdl_symbol, string("symbol"), true)->mdl;
    typeid(cstr)->user   = register_model(e, mdl_cstr,   string("cstr"),   true)->mdl;

    model mdl_cereal = structure(mod, e, ident, string("cereal"),
        members, m("value", emember(mod, e, name, string("value"), mdl, mdl_i8)));

    model mdl_AU_MEMBER = enumeration(mod, e,
        members, m(
            "AU_MEMBER_NONE",      emember(mod, e, name, string("AU_MEMBER_NONE"),      mdl, mdl_i32),
            "AU_MEMBER_CONSTRUCT", emember(mod, e, name, string("AU_MEMBER_CONSTRUCT"), mdl, mdl_i32),
            "AU_MEMBER_PROP",      emember(mod, e, name, string("AU_MEMBER_PROP"),      mdl, mdl_i32),
            "AU_MEMBER_INLAY",     emember(mod, e, name, string("AU_MEMBER_INLAY"),     mdl, mdl_i32),
            "AU_MEMBER_PRIV",      emember(mod, e, name, string("AU_MEMBER_PRIV"),      mdl, mdl_i32),
            "AU_MEMBER_INTERN",    emember(mod, e, name, string("AU_MEMBER_INTERN"),    mdl, mdl_i32),
            "AU_MEMBER_READ_ONLY", emember(mod, e, name, string("AU_MEMBER_READ_ONLY"), mdl, mdl_i32),
            "AU_MEMBER_IMETHOD",   emember(mod, e, name, string("AU_MEMBER_IMETHOD"),   mdl, mdl_i32),
            "AU_MEMBER_SMETHOD",   emember(mod, e, name, string("AU_MEMBER_SMETHOD"),   mdl, mdl_i32),
            "AU_MEMBER_OPERATOR",  emember(mod, e, name, string("AU_MEMBER_OPERATOR"),  mdl, mdl_i32),
            "AU_MEMBER_CAST",      emember(mod, e, name, string("AU_MEMBER_CAST"),      mdl, mdl_i32),
            "AU_MEMBER_INDEX",     emember(mod, e, name, string("AU_MEMBER_INDEX"),     mdl, mdl_i32),
            "AU_MEMBER_ENUMV",     emember(mod, e, name, string("AU_MEMBER_ENUMV"),     mdl, mdl_i32),
            "AU_MEMBER_OVERRIDE",  emember(mod, e, name, string("AU_MEMBER_OVERRIDE"),  mdl, mdl_i32),
            "AU_MEMBER_VPROP",     emember(mod, e, name, string("AU_MEMBER_VPROP"),     mdl, mdl_i32),
            "AU_MEMBER_IS_ATTR",   emember(mod, e, name, string("AU_MEMBER_IS_ATTR"),   mdl, mdl_i32),
            "AU_MEMBER_OPAQUE",    emember(mod, e, name, string("AU_MEMBER_OPAQUE"),    mdl, mdl_i32),
            "AU_MEMBER_IFINAL",    emember(mod, e, name, string("AU_MEMBER_IFINAL"),    mdl, mdl_i32),
            "AU_MEMBER_TMETHOD",   emember(mod, e, name, string("AU_MEMBER_TMETHOD"),   mdl, mdl_i32),
            "AU_MEMBER_FORMATTER", emember(mod, e, name, string("AU_MEMBER_FORMATTER"), mdl, mdl_i32)));

    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_NONE")),        _i32(AU_MEMBER_NONE));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_CONSTRUCT")),   _i32(AU_MEMBER_CONSTRUCT));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_PROP")),        _i32(AU_MEMBER_PROP));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_OPERATOR")),    _i32(AU_MEMBER_OPERATOR));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_CAST")),        _i32(AU_MEMBER_CAST));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_INDEX")),       _i32(AU_MEMBER_INDEX));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_ENUMV")),       _i32(AU_MEMBER_ENUMV));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_OVERRIDE")),    _i32(AU_MEMBER_OVERRIDE));
    set_value((emember)get(mdl_AU_MEMBER->members, string("AU_MEMBER_FORMATTER")),   _i32(AU_MEMBER_FORMATTER));
    model mdl_AU_TRAIT = enumeration(mod, e,
        members, m(
            "AU_TRAIT_PRIMITIVE",   emember(mod, e, name, string("AU_TRAIT_PRIMITIVE"), mdl, mdl_i32),
            "AU_TRAIT_INTEGRAL",    emember(mod, e, name, string("AU_TRAIT_INTEGRAL"),  mdl, mdl_i32),
            "AU_TRAIT_REALISTIC",   emember(mod, e, name, string("AU_TRAIT_REALISTIC"), mdl, mdl_i32),
            "AU_TRAIT_SIGNED",      emember(mod, e, name, string("AU_TRAIT_SIGNED"),    mdl, mdl_i32),
            "AU_TRAIT_UNSIGNED",    emember(mod, e, name, string("AU_TRAIT_UNSIGNED"),  mdl, mdl_i32),
            "AU_TRAIT_ENUM",        emember(mod, e, name, string("AU_TRAIT_ENUM"),      mdl, mdl_i32),
            "AU_TRAIT_ALIAS",       emember(mod, e, name, string("AU_TRAIT_ALIAS"),     mdl, mdl_i32),
            "AU_TRAIT_ABSTRACT",    emember(mod, e, name, string("AU_TRAIT_ABSTRACT"),  mdl, mdl_i32),
            "AU_TRAIT_STRUCT",      emember(mod, e, name, string("AU_TRAIT_STRUCT"),    mdl, mdl_i32),
            "AU_TRAIT_CLASS",       emember(mod, e, name, string("AU_TRAIT_CLASS"),     mdl, mdl_i32),
            "AU_TRAIT_POINTER",     emember(mod, e, name, string("AU_TRAIT_POINTER"),   mdl, mdl_i32)));

    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_PRIMITIVE")),  _i32(AU_TRAIT_PRIMITIVE));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_INTEGRAL")),   _i32(AU_TRAIT_INTEGRAL));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_REALISTIC")),  _i32(AU_TRAIT_REALISTIC));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_SIGNED")),     _i32(AU_TRAIT_SIGNED));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_UNSIGNED")),   _i32(AU_TRAIT_UNSIGNED));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_ENUM")),       _i32(AU_TRAIT_ENUM));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_ALIAS")),      _i32(AU_TRAIT_ALIAS));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_ABSTRACT")),   _i32(AU_TRAIT_ABSTRACT));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_STRUCT")),     _i32(AU_TRAIT_STRUCT));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_CLASS")),      _i32(AU_TRAIT_CLASS));
    set_value((emember)get(mdl_AU_TRAIT->members, string("AU_TRAIT_POINTER")),    _i32(AU_TRAIT_POINTER));

    typeid(AFlag)->user = register_model(e, mdl_AU_MEMBER, string("AFlag"), true)->mdl->src;
    
    register_model(e, mdl_AU_TRAIT, string("AU_TRAIT"), true);
    
    typeid(cereal)->user = register_model(e, mdl_cereal, string("cereal"),   true)->mdl;
}

static void register_basics(aether e) {
    model _i32      = emodel("i32");
    model _i16      = emodel("i16");
    model _i64      = emodel("i64");
    model _u64      = emodel("u64");
    model _handle   = emodel("handle");
    model _symbol   = emodel("symbol");

    if (!_i64) {
        // a member with 'context' would automatically bind the exact type when init'ing the object; the context flows unless its changed explicitly to something else
        // this 'just works'
        //_i32    = register_model(e, model(mod, e, atype_src, typeid(i32)),    string("i32"),    true)->mdl;
        _i16    = register_model(e, model(mod, e, atype_src, typeid(i16)),    string("i16"),    true)->mdl;
        _i64    = register_model(e, model(mod, e, atype_src, typeid(i64)),    string("i64"),    true)->mdl;
        _u64    = register_model(e, model(mod, e, atype_src, typeid(u64)),    string("u64"),    true)->mdl;
        _handle = register_model(e, model(mod, e, atype_src, typeid(handle)), string("handle"), true)->mdl;
        _symbol = register_model(e, model(mod, e, atype_src, typeid(symbol)), string("symbol"), true)->mdl;
    }
    verify(_i32, "i32 not found");

    // register callback primitives
    eargs args0 = eargs();
    eargs args1 = eargs();
    map a1 = m(
        "a0", earg(args, emodel("Au"), null, "a0"));

    eargs args2 = eargs();
    map a2 = m(
        "a0", earg(args, emodel("Au"), null, "a0"),
        "a1", earg(args, emodel("Au"), null, "a1"));
    
    eargs args3 = eargs();
    map a3 = m(
        "a0", earg(args, emodel("Au"), null, "a0"),
        "a1", earg(args, emodel("Au"), null, "a1"),
        "a2", earg(args, emodel("Au"), null, "a2"));
    
    function f_func           = function(mod, e, rtype, emodel("none"), args, args0);
    function f_hook           = function(mod, e, rtype, emodel("Au"), args, args1);
    function f_callback       = function(mod, e, rtype, emodel("Au"), args, args2);
    function f_callback_extra = function(mod, e, rtype, emodel("Au"), args, args3);

    register_model(e, f_func,           string("_func"),            true);
    register_model(e, f_hook,           string("_hook"),            true);
    register_model(e, f_callback,       string("_callback"),        true);
    register_model(e, f_callback_extra, string("_callback_extra"),  true);

    typeid(func)->user =
        register_model(e, (model)model(
            mod, e, src, f_func, is_ref, true),
            string("func"), true)->mdl;
    typeid(hook)->user =
        register_model(e, (model)model(
            mod, e, src, f_hook, is_ref, true),
            string("hook"), true)->mdl;
    typeid(callback)->user =
        register_model(e, (model)model(
            mod, e, src, f_callback, is_ref, true),
            string("callback"), true)->mdl;
    typeid(callback_extra)->user = 
        register_model(e, (model)model(
            mod, e, src, f_callback_extra, is_ref, true),
            string("callback_extra"), true)->mdl;

    model shape = emodel("shape");
    if (!shape) {
        emember m_shape = register_model(e, Class(
            mod, e, ident, string("shape")), string("shape"), false);

        model _i64_16 = model(mod, e, src, _i64, count, 16);
        register_model(e, _i64_16, string("_i64_16"), false);

        push(e, m_shape->mdl);
        m_shape->mdl->src->members = m(
            "count", emem(_i64,    null, "count"),
            "data",  emem(_i64_16, null, "data"));
        pop(e);
        m_shape->mdl->members = hold(m_shape->mdl->src->members); // we really need a way to mitigate this
        finalize(m_shape);
        shape = m_shape->mdl;
        typeid(shape)->user = shape;
    }

    _symbol->is_const = true;

    string    _Au_t_name = string("_Au_t");
    structure _Au_t      = structure (mod, e, ident, _Au_t_name, members, null);
    emember   _Au_t_mem  = register_model(e, _Au_t, _Au_t_name, false);

    model _Au_t_ptr  = pointer(_Au_t);
    register_model(e, _Au_t_ptr,  string("Au_t"),  false);
    model _Au_ts_ptr = pointer(_Au_t_ptr);
    emember _Au_ts_ptr_mem = register_model(e, _Au_ts_ptr, string("Au_ts"), false);
    emember _Au_ts_ptr_mem_ = lookup2(e, string("Au_ts"), null);

    typeid(Au_t) ->user = _Au_t_ptr; // without this, its double-registered in Au_import
    typeid(Au_ts)->user = _Au_ts_ptr;

    structure _af_recycler          = structure     (mod, e, ident, string("_af_recycler"), is_system, true);
    emember   _af_recycler_mem      = register_model(e, _af_recycler,     string("_af_recycler"), false);
    model      af_recycler_ptr      = pointer       (_af_recycler);
    emember    af_recycler_ptr_mem  = register_model(e,  af_recycler_ptr, string("af_recycler"),  false);

    structure _meta_t               = structure     (mod, e, ident, string("_meta_t"), is_system, true);
    emember   _meta_t_mem           = register_model(e, _meta_t, string("_meta_t"), false);
    structure _member               = structure     (mod, e, ident, string("_member"));
    Au_t member_type = isa(_member);
    emember   _member_mem           = register_model(e, _member, string("_member"), false);
    model      member_ptr           = pointer       (_member);
    emember    member_ptr_mem       = register_model(e, member_ptr, string("member"), false);

    typeid(Au_t)->user  = _Au_t_ptr;
    //typeid(member)->user = member_ptr;
    /*
    push(e, _member);
    _member->members = m(
        "name",             emem(_symbol,      null, "name"),
        "sname",            emem(_handle,      null, "sname"),
        "type",             emem(_Au_t_ptr,   null, "type"),
        "offset",           emem(_i32,         null, "offset"),
        "count",            emem(_i32,         null, "count"),
        "member_type",      emem(_i32,         null, "member_type"),
        "operator_type",    emem(_i32,         null, "operator_type"),
        "required",         emem(_i32,         null, "required"),
        "args",             emem(_meta_t,      null, "args"),
        "ptr",              emem(_handle,      null, "ptr"),
        "method",           emem(_handle,      null, "method"),
        "id",               emem(_i64,         null, "id"),
        "value",            emem(_i64,         null, "value"));
    pop(e);
    */

    push(e, _meta_t);
    _meta_t->members = m(
        "count",  emem(_i64,         null, "count"),
        "meta_0", emem(_Au_t_ptr,   null, "meta_0"),
        "meta_1", emem(_Au_t_ptr,   null, "meta_1"),
        "meta_2", emem(_Au_t_ptr,   null, "meta_2"),
        "meta_3", emem(_Au_t_ptr,   null, "meta_3"),
        "meta_4", emem(_Au_t_ptr,   null, "meta_4"),
        "meta_5", emem(_Au_t_ptr,   null, "meta_5"),
        "meta_6", emem(_Au_t_ptr,   null, "meta_6"),
        "meta_7", emem(_Au_t_ptr,   null, "meta_7"),
        "meta_8", emem(_Au_t_ptr,   null, "meta_8"),
        "meta_9", emem(_Au_t_ptr,   null, "meta_9"),
        "meta_10", emem(_Au_t_ptr,   null, "meta_10"),
        "meta_11", emem(_Au_t_ptr,   null, "meta_11"),
        "meta_12", emem(_Au_t_ptr,   null, "meta_12"),
        "meta_13", emem(_Au_t_ptr,   null, "meta_13"),
        "meta_14", emem(_Au_t_ptr,   null, "meta_14"),
        "meta_15", emem(_Au_t_ptr,   null, "meta_15"));
    pop(e);

    push(e, _af_recycler);
    _af_recycler->members = m(
        "af",       emem(_handle, null, "af"),
        "af_count", emem(_i64,    null, "af_count"),
        "af_alloc", emem(_i64,    null, "af_alloc"),
        "re",       emem(_handle, null, "re"),
        "re_count", emem(_i64,    null, "re_count"),
        "re_alloc", emem(_i64,    null, "re_alloc"));
    pop(e);
    
    model _u64_2 = model(mod, e, src, _u64, count, 2);
    register_model(e, _u64_2, string("_u64_2"), true);

    push(e, _Au_t);
    emember mem2 = emem(_Au_t_ptr, null, "parent_type");

    _Au_t->members = m(
        "parent_type",     mem2,
        "domain",          emem(_symbol,    null, "domain"),
        "module",          emem(_symbol,    null, "module"),
        "name",            emem(_symbol,    null, "name"),
        "size",            emem(_i32,       null, "size"),
        "isize",           emem(_i32,       null, "isize"),
        "af",              emem(af_recycler_ptr, null, "af"),
        "member_count",    emem(_i32,       null, "member_count"),
        "members",         emem(member_ptr, null, "members"),
        "traits",          emem(_i32,       null, "traits"),
        "user",            emem(_handle,    null, "user"),
        "required",        emem(_u64_2,     null, "required"),
        "src",             emem(_Au_t_ptr,  null, "src"),
        "arb",             emem(_handle,    null, "arb"),
        "shape",           emem(shape,      null, "shape"),
        "meta",            emem(_meta_t,    null, "meta"));
    pop(e);

    finalize(_Au_t_mem);    
    finalize(_meta_t_mem);
    finalize(_af_recycler_mem);
    finalize(_member_mem);
}


// create Au_t structure (info and its Au_t_f)
void create_schema(model mdl, string name) {
    if (eq(name, "Au")) {
        int test2 = 2;
        test2    += 2;
    }
    aether e = mdl->mod;
    function  f = instanceof(mdl, typeid(function));
    if ((e->current_include && !e->is_Au_import) || mdl->schema || f || mdl->is_system || mdl == e)
        return;

    structure   st   = instanceof(mdl, typeid(structure));
    enumeration en   = instanceof(mdl, typeid(enumeration));
    Class       cmdl = mdl->src ? instanceof(mdl->src, typeid(Class)) : null;
    Class       emdl = instanceof(mdl, typeid(enumeration));

    // want to guard against anonymous pointers and such
    if (!mdl->mem || !mdl->mem->name || !len(mdl->mem->name)) return;
    
    if (cmdl && eq(cmdl->mem->name, "main"))
        return;

    push(e, e->is_Au_import ? (model)e : (model)e->userspace);
    
    // emit class info
    bool      from_module = !mdl->imported_from;
    model     Au_t_ref    = emodel("_Au_t"); // this is what we define in object.h (a mock of the top portion of the _Au_f table)
    string    type_name   = f(string, "_%o_f", name);
    structure mdl_type    = mdl->schema_type;
    
    // add identical member models to this new structure
    model _Au = emodel("Au")->src;
    array acl = class_list(mdl, emodel("Au"), false);

    verify(mdl_type->members->count == 0, "expected no members on %o", type_name);
    push(e, mdl_type);

    // add base Au_t members (before we start adding methods)
    pairs(Au_t_ref->members, i) {
        emember r = i->value;
        emember n = emember(mod, e, name, r->name, mdl, r->mdl, context, mdl_type);
        set(mdl_type->members, r->name, n);
    }

    // register polymorphic functions
    each (acl, Class, cl) {
        model mcl = cl;
        if (!cl->members) continue;
        pairs(cl->members, a) {
            emember m = a->value;
            if (isa(m->mdl) != typeid(function))
                continue;
            model ptr = pointer(m->mdl);
            emember f = emember(mod, e, name, m->name, mdl, ptr, context, mdl_type);
            set(mdl_type->members, m->name, f);
        }
    }
    pop(e);

    emember type_mem = register_model(e, mdl_type, type_name, true);

    // register type ## _info struct with Au info header and f table (Au_t)
    token info_name = f(token, "_%o_info", name);
    structure _type_info = structure(mod, e, ident, info_name,
        is_system, true, is_internal, mdl->is_internal, members, m(
            "info", emem(_Au,      null, "info"),
            "type", emem(mdl_type, null, "type")
        ));
    
    register_model(e, _type_info, info_name, true);

    token info_name2 = f(token, "%o_info", name);
    model type_info = model(mod, e, src, _type_info,
        members, _type_info->members, is_system, true);
    register_model(e, type_info, info_name2, true);

    // actual type_i member of info struct
    interface access = from_module ? interface_public : interface_intern;
    emember type_i = emember(mod, e, name, f(token, "%o_i", name), mdl,
        type_info, access, access);
    register_member(e, type_i, true);

    model   type_alias = model(mod, e, src, mdl_type, is_system, true);
    register_model(e, type_alias, f(string, "%o_f", name, access, access), true);

    pop(e);

    mdl->schema = hold(mschema(
        schema_type, type_mem,
        schema_f, type_alias,
        schema_info, type_info,
        schema_i, type_i));
}

void aether_Au_import(aether e, path lib, string name) {
    e->current_include = lib ? lib : path("Au");
    e->is_Au_import = true;
    string lib_name = lib ? stem(lib) : null;
    push(e->shared_libs, lib_name ? lib_name : string("Au"));

    map module_filter = m("Au", _bool(true));
    if (lib) set(module_filter, lib_name, _bool(true));

    Au_t mod = Au_register_module(copy_cstr(name->chars)); //Au_module(name->chars);

    handle f = lib ? dlopen(cstring(lib), RTLD_NOW) : null;
    verify(!lib || f, "shared-lib failed to load: %o", lib);

    path inc = lib ? lib : path_self();

    if (f) {
        push(e->shared_libs, f);
        Au_engage(null); // load Au-types by finishing global constructor ordered calls
    }

    i64       ln;
    Au_t*     a          = Au_types(&ln);
    structure _Au_t      = emodel("_Au_t");
    map       processing = map(hsize, 64, assorted, true, unmanaged, true);

    // we may create classes/structures with no members ahead of time
    // any remaining ones after an import process would fault
    // must have members to finalize, of course
    // using this method we do not need recursion
    if (!_Au_t) register_vbasics(e);

    string mstate      = null;
    symbol last_module = null;
    
    for (num i = 0; i < ln; i++) {
        Au_t  atype = a[i];
        model mdl   = null;

        // only import the module the user wants
        if (last_module != atype->module) {
            last_module  = atype->module;
            mstate       = string(last_module);
        }
        if (!get(module_filter, mstate))
            continue;

        if (strcmp(atype->ident, "ARef") == 0) {
            continue; // we register this after
        }

        if (atype == typeid(shape)) continue;

        bool  is_abstract  = (atype->traits & AU_TRAIT_ABSTRACT)  != 0;
        if (atype->user) continue; // if we have already processed this type, continue
        string name = string(atype->ident);
        set(processing, name, atype);
        if (atype->user == typeid(Au_t)) continue;

        bool  is_prim      = (atype->traits & AU_TRAIT_PRIMITIVE) != 0;
        bool  is_ref       = (atype->traits & AU_TRAIT_POINTER)   != 0;
        if   (is_ref) continue; // src of this may be referencing another unresolved

        bool  _is_struct    = (atype->traits & AU_TRAIT_STRUCT)    != 0;
        bool  _is_class     = (atype->traits & AU_TRAIT_CLASS)     != 0;
        bool  is_enum      = (atype->traits & AU_TRAIT_ENUM)      != 0;
        bool  is_realistic = (atype->traits & AU_TRAIT_REALISTIC) != 0;
        bool  is_integral  = (atype->traits & AU_TRAIT_INTEGRAL)  != 0;
        bool  is_unsigned  = (atype->traits & AU_TRAIT_UNSIGNED)  != 0;
        bool  is_signed    = (atype->traits & AU_TRAIT_SIGNED)    != 0;
        if      (_is_class)    mdl = Class      (mod, e, ident, name, imported_from, inc);
        else if (_is_struct)   mdl = structure  (mod, e, ident, name, imported_from, inc);
        else if (is_enum)     mdl = enumeration(mod, e, atype, typeid(i32));
        else if (is_prim && !is_abstract && (atype->traits & AU_TRAIT_INTEGRAL) != 0) {
            mdl = model(mod, e, atype_src, atype);
        } else continue; // we get these on another pass
        
        // initialization for primitives are in model_init
        verify(mdl, "failed to import type: %o", name);
        emember mem = register_model(e, mdl, name, is_prim || is_enum); // lets finalize Au after (we have no props and methods)
        atype->user = mem->mdl;
    }

    // first time we run this, we must import Au_t basics (after we import the basic primitives)
    if (!_Au_t) register_basics(e);

    for (int ii = 0; ii < 2; ii++)
    for (num i = 0; i < ln; i++) {
        Au_t  atype = a[i];
        model  mdl   = null;

        if (atype->user) 
            continue;

        if (last_module != atype->module) {
            last_module  = atype->module;
            mstate       = string(last_module);
        }
        if (!get(module_filter, mstate)) continue;
        
        bool  is_abstract  = (atype->traits & AU_TRAIT_ABSTRACT)  != 0;
        bool  is_prim      = (atype->traits & AU_TRAIT_PRIMITIVE) != 0;
        bool  is_ref       = (atype->traits & AU_TRAIT_POINTER)   != 0;
        
        if (ii == 0 && is_ref) continue; // lets wait for the second iteration to register

        if   (!is_abstract && !is_prim && !is_ref) continue;
        mdl = model(mod, e, atype_src, atype);

        string n = string(atype->ident);
        if (is_ref && starts_with(n, "ref_")) {
            string src = mid(n, 4, len(n) - 4);
            emember mem_src = lookup2(e, src, null);
            verify(mem_src, "could not find source model for %o", src);
            mdl->src = hold(mem_src->mdl);
        }

        // initialization for primitives are in model_init
        atype->user = mdl; // lets store our model reference here
        register_model(e, mdl, string(atype->ident), is_prim);
    }

    // resolve parent classes, load refs, fill out enumerations, and set primitive references
    pairs(processing, i) {
        Au_t  atype    = i->value;
        model  mdl      = atype->user;

        if (atype != typeid(Au)) {
            model m = emodel(atype->context->ident);
            verify(isa(m), "expected parent class %s", atype->context->ident);
            mdl->parent = m;
        }

        bool   is_enum  = (atype->traits & AU_TRAIT_ENUM)    != 0;
        if    (is_enum) {
            enumeration en = mdl;
            en->members = map(hsize, 16);
            en->src     = atype->src->user;
            verify(en->src, "expected enumeration source type");

            for (int m = 0; m < atype->members.count; m++) {
                Au_t amem = atype->members.origin[m];
                if (!(amem->member_type & AU_MEMBER_ENUMV)) continue;
                verify(atype->src == amem->type,
                    "enum value type not the same as defined on type");
                Au e_const = primitive(amem->type, amem->ptr);
                string mn = string(amem->ident);
                emember e_value = emember(
                    mod, e, name, mn, mdl, en->src, context, en);
                set_value(e_value, e_const); // should set appropriate constant state
                verify(e_value->is_const, "expected constant set after");
                set(en->members, mn, e_value);
            }
        }

        bool   is_ref   = (atype->traits & AU_TRAIT_POINTER) != 0;
        if   (!is_ref || mdl) continue;

        Au_t pointer_to = *atype->meta.origin;
        verify(!mdl, "unexpected user data set for %s", atype->ident);
        string name = string(atype->ident);
        verify(pointer_to, "expected src to be set for %o", name);
        mdl = emodel(pointer_to->ident);
        // todo: validate this data
        mdl = pointer(mdl->ptr ? mdl->ptr : mdl); // we have to have these ref_i32's and such from Au-type
        register_model(e, mdl, name, false);
        verify(!atype->user, "expected null user for type %s", atype->ident);
        
        if (atype == typeid(symbol)) {
            atype->user = emodel("symbol");
            verify(atype->user, "missing symbol model");
        } else {
            atype->user = mdl;
        }
    }

    emember g2 = get(emodel("_Au_t")->members, string("parent_type"));
    model mdl_prev = g2->mdl;

    // load structs and class members (including i/s methods)
    pairs(processing, i) {
        Au_t  atype     = i->value;
        model  mdl       = atype->user;
        verify(isa(mdl) != typeid(Class), "type mismatch for model on %s", atype->ident);

        bool   _is_struct = (atype->traits & AU_TRAIT_STRUCT) != 0;
        bool   _is_class  = (atype->traits & AU_TRAIT_CLASS)   != 0;
        
        if    (_is_class || _is_struct) {
            structure st = _is_struct ? mdl : null;
            Class     cl = _is_class  ? mdl : null;

            if (!mdl->members) {
                mdl->members = hold(map(hsize, 8));
                if (cl) {
                    cl->src->members = hold(mdl->members);
                }
            }
            if (atype == typeid(Au)) {
                emember mem_ARef = lookup2(e, string("ARef"), null);
                mem_ARef->mdl->src = mdl;
            }

            // add members
            for (int m = 0; m < atype->members.count; m++) {
                Au_t amem      = atype->members.origin[m];
                Au_t  mtype    = amem->type;
                string n       = string(amem->ident);
                model  mem_mdl = null;

                verify(!mtype || mtype->user, "expected type resolution for member %o", n);
                eargs args = eargs(mod, e);
                if (amem->member_type == AU_MEMBER_CONSTRUCT) {
                    verify(mtype, "type cannot be void for a constructor");
                    model arg_mdl = translate_target((model)mtype->user); // 'target' is our arg (not target), the function merely works for us
                    emember arg = earg(args, arg_mdl, null, "");

                    set(args->members, string(""), arg);
                    mem_mdl = function(
                        mod,           e,
                        is_ctr,        true,
                        extern_name,   f(string, "%s_%o", atype->ident, n),
                        rtype,         translate_rtype(st ? (model)st : emodel("none")), // structure ctr return data (Au-type ABI)
                        function_type, AU_MEMBER_CONSTRUCT,
                        instance,      mdl, // structs do not have a 'target' argument in constructor -- silver operates the same
                        args,          args);
                }
                else if (amem->member_type == AU_MEMBER_CAST) {
                    verify(mtype, "cast cannot be void");
                    mem_mdl = function(
                        mod,           e,
                        is_cast,       true,
                        extern_name,   f(string, "%s_%o", atype->ident, n),
                        rtype,         translate_rtype(mtype->user),
                        function_type, AU_MEMBER_CAST,
                        instance,      mdl);
                }
                else if (amem->member_type == AU_MEMBER_INDEX) {
                    verify(mtype, "index cannot return void");
                    model index_mdl = ((Au_t)*amem->args.origin)->user;
                    verify(index_mdl, "index requires argument");
                    emember arg = earg(args, index_mdl, null, "");
                    set(args->members, string(""), arg);
                    mem_mdl    = function(
                        mod,           e,
                        extern_name,   f(string, "%s_%o", atype->ident, n),
                        is_cast,       true,
                        rtype,         translate_rtype(mtype->user),
                        function_type, AU_MEMBER_INDEX,
                        instance,      mdl,
                        args,          args);
                }
                else if ((amem->member_type & AU_MEMBER_OPERATOR) != 0) {
                    model  arg_mdl = ((Au_t)*amem->args.origin)->user;
                    emember arg     = earg(args, arg_mdl, null, "operand");
                    set(args->members, string("operand"), arg);
                    mem_mdl        = function(
                        mod,           e,
                        extern_name,   f(string, "%s_%o", atype->ident, n),
                        is_oper,       amem->operator_type,
                        rtype,         translate_rtype(mtype->user),
                        function_type, AU_MEMBER_OPERATOR,
                        instance,      mdl,
                        args,          arg_mdl ? args : null);
                }
                else if (amem->member_type == AU_MEMBER_FUNC) {
                    bool  is_inst = amem->is_imethod;
                    bool  is_f    = amem->is_ifinal;
                    bool  is_t    = amem->is_tmethod;
                    eargs args    = eargs(mod, e);
                    // Au-type includes the target in args, but we do not
                    for (int a = (is_inst || is_f) ? 1 : 0; a < amem->args.count; a++) {
                        Au_t  arg_type = ((Au_t*)amem->args.origin)[a];
                        model  arg_mdl  = translate_target((model)arg_type->user);
                        emember arg     = earg(args, arg_mdl, null, arg_mdl->mem->name->chars);
                        set(args->members, arg_mdl->mem->name, arg);
                    }
                    mem_mdl = function(
                        mod,           e,
                        extern_name,   (is_f || is_t) ? n : f(string, "%s_%o", atype->ident, n),
                        is_override,   (amem->member_type & AU_MEMBER_OVERRIDE) != 0,
                        rtype,         translate_rtype(mtype->user),
                        function_type, is_f ? AU_TRAIT_IFINAL : is_inst ? AU_TRAIT_IMETHOD : AU_TRAIT_SMETHOD,
                        instance,      (is_inst || is_f) ? mdl : null,
                        args,          args);
                }
                else if (amem->member_type == AU_MEMBER_PROP) {
                    mem_mdl = mtype->user;
                    if (amem->is_vprop) {
                        mem_mdl = pointer(mem_mdl);
                    }
                }
                verify(mem_mdl, "expected mdl for member %s:%o", atype->ident, n);
                emember smem = emember(
                    mod, e, name, n, 
                    mdl, amem->size == 0 ? 
                        mem_mdl : model(mod, e, src, mem_mdl, count, amem->size), context, mdl);
                
                if (!mem_mdl->mem) {
                    mem_mdl->mem = hold(smem); // this doesnt happen for props/vprops
                }
                //mem_mdl->mem = hold(smem);
                set(mdl->members, n, smem);
            }

            if (strcmp(atype->ident, "Au") == 0) {
                int test2 = 2;
                test2 += 2;
            }

            // add padding based on isize (aligned with runtime; this way we may allocate space for it to use its special things)
            if ((_is_class || _is_struct) && atype->isize > 0) {
                string n = f(string, "%s_interns", atype->ident);
                model intern_space = model(
                    mod,       e,
                    src,       emodel("u8"),
                    use_count, true,
                    count,     atype->isize);
                emember imem = emember(mod, e, name, n, mdl, intern_space); // todo: when we finalize our classes, we must position interns below our regular members (we are not doing that!)
                intern_space->mem = hold(imem);
                set(mdl->members, n, imem);
            }
        }
    }

    emember g = get(emodel("_Au_t")->members, string("parent_type"));

    // finalize models
    model _ARef = emodel("ARef");
    
    pairs(processing, i) {
        Au_t  atype  = i->value;
        string name   = string(atype->ident);
        model   mdl   = atype->user;
        verify(mdl, "could not import %s", atype->ident);
        emember emem  = lookup2(e, name, null);

        // improve integrity check so ptr is for classes, and mdl check is for others
        verify(emem->mdl == mdl || emem->mdl == mdl->ptr, "import integrity error for %o", name);
        finalize(emem);

        create_schema(emem->mdl, emem->name);
    }

    // we do not need the schema until we build the global constructor
    //update_schemas(e);

    e->is_Au_import     = false;
    e->current_include = null;
}

ident2 ident2_with_model(ident2 a, model mdl) {
    a->mdl = mdl;
    return a;
}

// todo: adding methods to header does not update the methods header (requires clean)
void aether_reinit_startup(aether e) {
    array prev = e->lex;
    e->lex = hold(array(alloc, 32, assorted, true));
    verify(prev->origin[0] == e, "expected module identity");
    push(e->lex, prev->origin[0]);

    // this should pose virtually as global
    // we have two parts so our Au-type models do not reload, and change identity
    model mdl_after = model(
        mod, e, src, e,
        is_global, true,
        members,   map(hsize, 64));
    push(e, mdl_after);
    e->userspace = hold(mdl_after);
    e->userspace->open = true;
    drop(prev);
} 

void aether_init(aether e) {

    e->open       = true;
    e->with_debug = true;
    e->is_system  = true;
    e->finalizing = hold(array(alloc, 64, assorted, true, unmanaged, true));
    e->instances  = map(hsize, 8);

    if (!e->mem || !e->mem->name) {
        if (!e->mem) e->mem = emember(mod, e);
        verify(e->source && len(e->source), "module name required");
        e->mem->name = hold(stem(e->source));
    }

    aether_llvm_init(e);

    e->au = Au_register_module(cstring(e->mem->name));
    e->members = map(hsize, 32);
    e->shared_libs = array(alloc, 32, unmanaged, true);
    e->lex = array(alloc, 32, assorted, true);
    e->is_global = true;
    push(e, e);

    // aether_define_primitive(e);
    Au_import(e, null, null);

    // register 'main' class as an alias to Au
    // we use this as an means of signaling app entry (otherwise we make a lib)
    // we cant make the type in Au-type because the symbol clashes; we need not export this symbol
    model main = Class(mod, e, ident, string("main"),
        src, emodel("Au"), imported_from, path("Au"), is_abstract, true);
    register_model(e, main, string("main"), false);

    bool _mac = false;
    bool _win = false;
    bool _unix = false;
    bool _x86_64 = false;
    bool _arm64 = false;
    bool _riscv = false;
    bool _arm = false;
    bool _x86 = false;
    bool _mips = false;
#if defined(_WIN32)
    cstr os_type = "win";
    _win = true;
#elif defined(__APPLE__)
    cstr os_type = "mac";
    _mac = true;
#else
    cstr os_type = "unix";
    _unix = true;
#endif

#if defined(__x86_64__) || defined(_M_X64)
    cstr arch_type = "x86_64";
    _x86_64 = true;
#elif defined(__aarch64__) || defined(_M_ARM64)
    cstr arch_type = "arm64";
    _arm64 = true;
#elif defined(__arm__) || defined(_M_ARM)
    cstr arch_type = "arm";
    _arm = true;
#elif defined(__riscv)
    cstr arch_type = "riscv";
    _riscv = true;
#elif defined(__i386__) || defined(_M_IX86)
    cstr arch_type = "x86";
    _x86 = true;
#elif defined(__mips__) || defined(__mips) || defined(_M_MRX000)
    cstr arch_type = "mips";
    _mips = true;
#else
    cstr arch_type = "unknown";
#endif

    // standard member constants for os and arch
    model str = emodel("string");
    emember os = emember(
        mod, e, name, token("os"),
        is_const, true, is_decl, true,
        literal, string(os_type), mdl, str);
    register_member(e, os, true);

    emember arch = emember(
        mod, e, name, token("arch"),
        is_const, true, is_decl, true,
        literal, string(arch_type), mdl, str);
    register_member(e, arch, true);

    emember m_mac = emember(
        mod, e, name, token("mac"),
        is_const, true, is_decl, true,
        literal, _bool(_mac), mdl, emodel("bool"));
    register_member(e, m_mac, true);

    emember m_win = emember(
        mod, e, name, token("win"),
        is_const, true, is_decl, true,
        literal, _bool(_win), mdl, emodel("bool"));
    register_member(e, m_win, true);

    emember m_unix = emember(
        mod, e, name, token("unix"),
        is_const, true, is_decl, true,
        literal, _bool(_unix), mdl, emodel("bool"));
    register_member(e, m_unix, true);

    emember m_x86_64 = emember(
        mod, e, name, token("x86_64"),
        is_const, true, is_decl, true,
        literal, _bool(_x86_64), mdl, emodel("bool"));
    register_member(e, m_x86_64, true);

    emember m_x86 = emember(
        mod, e, name, token("x86"),
        is_const, true, is_decl, true,
        literal, _bool(_x86), mdl, emodel("bool"));
    register_member(e, m_x86, true);

    emember m_arm64 = emember(
        mod, e, name, token("arm64"),
        is_const, true, is_decl, true,
        literal, _bool(_arm64), mdl, emodel("bool"));
    register_member(e, m_arm64, true);

    emember m_arm = emember(
        mod, e, name, token("arm"),
        is_const, true, is_decl, true,
        literal, _bool(_arm), mdl, emodel("bool"));
    register_member(e, m_arm, true);

    emember m_riscv = emember(
        mod, e, name, token("riscv"),
        is_const, true, is_decl, true,
        literal, _bool(_riscv), mdl, emodel("bool"));
    register_member(e, m_riscv, true);

    emember m_mips = emember(
        mod, e, name, token("mips"),
        is_const, true, is_decl, true,
        literal, _bool(_mips), mdl, emodel("bool"));
    register_member(e, m_mips, true);

    // this is so we have watching ability -- the ability 
    // to not require reimport of Au-type, keeping our model 
    // identities intact for retries
    reinit_startup(e);


}


void aether_dealloc(aether e) {
    //LLVMDisposeExecutionEngine(e->jit);
    LLVMDisposeBuilder(e->builder);
    LLVMDisposeDIBuilder(e->dbg_builder);
    LLVMDisposeModule(e->module);
    LLVMContextDispose(e->module_ctx);
    LLVMDisposeMessage(e->target_triple);
}

/// C type rules implemented
model determine_rtype(aether e, OPType optype, model L, model R) {
    if (optype >= OPType__assign && optype <= OPType__assign_left)
        return L;  // Assignment operations always return the type of the left operand
    else if (optype == OPType__value_default ||
             optype == OPType__cond_value    ||
             optype == OPType__or            ||
             optype == OPType__and           ||
             optype == OPType__xor) {
        if (is_bool(L) && is_bool(R))
            return emodel("bool");  // Logical operations on booleans return boolean
        // For bitwise operations, fall through to numeric promotion
    }

    // Numeric type promotion
    if (is_realistic(L) || is_realistic(R)) {
        // If either operand is float, result is float
        if (is_double(L) || is_double(R))
            return emodel("f64");
        else
            return emodel("f32");
    }

    // Integer promotion
    int L_size = L->size_bits;
    int R_size = R->size_bits;
    if (L_size > R_size)
        return L;
    else if (R_size > L_size)
        return R;

    bool L_signed = is_signed(L);
    bool R_signed = is_signed(R);
    if (L_signed && R_signed)
        return L;  // Both same size and signed
    else if (!L_signed && !R_signed)
        return L;  // Both same size and unsigned
    
    return L_signed ? R : L;  // Same size, one signed one unsigned
}


/// we may check against true/false/instance
/// here we are acting a bit more like python with Au-type
/// its not caution to the wind, because no valid pointer can legally 
/// ever be 0x01 on any system in the world.
/// they lock up your threads if allocate one
emember model_castable(model fr, model to) { 
    bool fr_ptr = fr->is_ref;
    if ((fr_ptr || is_primitive(fr)) && is_bool(to))
        return (emember)true;
    
    /// compatible by match, or with basic integral/real types
    if ((fr == to) ||
        ((is_realistic(fr) && is_realistic(to)) ||
         (is_integral (fr) && is_integral (to))))
        return (emember)true;
    
    /// primitives may be converted to Au-type Au
    if (is_primitive(fr) && _is_generic(to))
        return (emember)true;

    /// check cast methods on from
    pairs (fr->members, i) {
        emember mem = i->value;
        function f = instanceof(mem->mdl, typeid(function));
        if (!f || !(f->function_type & AU_MEMBER_CAST))
            continue;
        if (f->rtype == to)
            return mem;
    }
    return (emember)false;
}

/// constructors are reduced too, 
/// first arg as how we match them,
/// the rest are optional args by design
/// this cannot enforce required args ...which is a bit of a problem
/// so we may just check for those!
/// that would serve to limit applicable constructors and might be favored
emember model_constructable(model fr, model to) {
    if (fr == to)
        return (emember)true;
    pairs (to->members, i) {
        emember mem = i->value;
        function fn = instanceof(mem->mdl, typeid(function));
        if (!fn || !((fn->function_type & AU_MEMBER_CONSTRUCT) != 0))
            continue;
        emember first = null;
        pairs (fn->args->members, i) {
            first = i->value;
            break;
        }
        if (first->mdl == fr)
            return mem;
    }
    return (emember)false;
}

static emember scalar_compatible(LLVMTypeRef ea, LLVMTypeRef eb) {
    LLVMTypeKind ka = LLVMGetTypeKind(ea);
    LLVMTypeKind kb = LLVMGetTypeKind(eb);

    if (ka == kb) {
        switch (ka) {
            case LLVMIntegerTypeKind:
                if (LLVMGetIntTypeWidth(ea) == LLVMGetIntTypeWidth(eb))
                    return (emember)true;
                else
                    return (emember)false;
            case LLVMFloatTypeKind:   // 32-bit float
            case LLVMDoubleTypeKind:  // 64-bit float
            case LLVMHalfTypeKind:    // 16-bit float
            case LLVMBFloatTypeKind:  // 16-bit bfloat
                return (emember)true; // same kind → compatible
            default:
                return (emember)false;
        }
    }
    return (emember)false;
}

// returns null, true, or the member function used for conversion
emember model_convertible(model fr, model to) {
    aether e = fr->mod;
    model  a = typed(fr);
    model  b = typed(to);

    model  ma = model_source(a);
    model  mb = model_source(b);

    // more robust conversion is, they are both pointer and not user-created
    if (ma->is_system || mb->atype_src == typeid(handle))
        return (emember)true;

    if (a == b)
        return (emember)true;

    if (is_primitive(a) && is_primitive(b))
        return (emember)true;

    if (_is_record(a) || _is_record(b)) {
        if (_is_subclass(a, b))
            return (emember)true;
        emember mcast = castable(a, b);
        return mcast ? mcast : constructable(a, b);
    } else {
        // the following check should be made redundant by the code below it
        model sym = emodel("symbol");
        model ri8 = emodel("ref_i8");

        // quick test
        if ((a == sym && b == ri8) || (a == ri8 && b == sym))
            return (emember)true;

        LLVMTypeKind ka = LLVMGetTypeKind(a->type);
        LLVMTypeKind kb = LLVMGetTypeKind(b->type);

        // pointers → compare element types
        if (ka == LLVMPointerTypeKind && kb == LLVMPointerTypeKind) {
            // return true if one is of an opaque type
            // return false if the scalar units are different size
            return (emember)true;
            /*

            char *s = LLVMPrintTypeToString(a->type);
            LLVMTypeRef ea = LLVMGetElementType(a->type);
            LLVMTypeRef eb = LLVMGetElementType(b->type);

            LLVMDisposeMessage(s);
            return scalar_compatible(ea, eb);
            */
        }

        // primitive check should find all other valid ones above this
        return scalar_compatible(a->type, b->type);
    }
    return (emember)false;
}

typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;


array model_class_list(model mdl, model mdl_filter, bool filter_out) {
    aether e = mdl->mod;
    array   classes = array(alloc, 32, assorted, true);
    enumeration emdl = instanceof(mdl, typeid(enumeration));
    model Au_mdl = emodel("Au");
    if (emdl) {        
        if (!mdl_filter || ((mdl_filter == emdl)  ^ filter_out)) push(classes, emdl);
        if (!mdl_filter || ((mdl_filter == Au_mdl) ^ filter_out)) push(classes, Au_mdl);
    } else {
        record cur  = instanceof(mdl, typeid(record));
        while (cur) {
            if (!mdl_filter || ((mdl_filter == cur) ^ filter_out))
                push(classes, cur);
            cur = cur->parent;
            if (cur && cur->is_abstract)
                cur = cur->src;
            verify(!cur || !cur->is_ref, "unexpected pointer");
        }
    }
    classes = reverse(classes);
    return classes;
}

array model_field_list(model mdl, model mdl_filter, bool filter_in) {
    aether e = mdl->mod;
    array acl = class_list(mdl, mdl_filter, filter_in);
    array fields = array(alloc, 32);
    each (acl, Class, cl) {
        pairs (cl->members, i) {
            emember m = i->value;
            if (m->access == interface_intern)  continue;
            if (instanceof(m->mdl, typeid(function))) continue;
            push(fields, m);
        }
    }
    return fields;
}

void aether_push_state(aether a, array tokens, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = Au_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    tokens_data* state_saved = (tokens_data*)last_element(a->stack);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}


void aether_pop_state(aether a, bool transfer) {
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
    push_state(a, a->tokens, a->cursor);
}

emember aether_compatible(aether e, record r, string n, AFlag f, array args) {
    pairs (r->members, i) {
        emember mem = i->value;
        function fn = instanceof(mem->mdl, typeid(function));
        /// must be function with optional name check
        if (!fn || (((f & fn->function_type) == f) && (n && !eq(n, fn->mem->name->chars))))
            continue;
        /// arg count check
        if (len(fn->args->members) != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        pairs(fn->args->members, ii) {
            emember to_arg = ii->value;
            enode fr_arg = args->origin[ai];
            if (!convertible(fr_arg->mdl, to_arg->mdl)) {
                compatible = false;
                break;
            }
            ai++;
        }
        if (compatible)
            return mem;
    }
    return (emember)false;
}

enode aether_negate(aether e, enode L) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, L->mdl);

    if (is_float(L->mdl))
        return LLVMBuildFNeg(e->builder, L->value, "f-negate");
    else if (is_signed(L->mdl)) // our enums should fall into this category
        return LLVMBuildNeg(e->builder, L->value, "i-negate");
    else if (is_unsigned(L->mdl)) {
        // Convert unsigned to signed, negate, then convert back to unsigned
        LLVMValueRef signed_value  = LLVMBuildIntCast2(
            e->builder, L->value, LLVMIntType(L->mdl->size_bits), 1, "to-signed");
        LLVMValueRef negated_value = LLVMBuildNeg(
            e->builder, signed_value, "i-negate");
        model i64 = emodel("i64");
        LLVMValueRef i64_value = LLVMBuildIntCast2(
            e->builder, negated_value, i64->type, 1, "to-i64");
        return value(i64, null, negated_value);
    }
    else {
        fault("Au negation not valid");
    }
    return null;
}

enode aether_e_not(aether e, enode L) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("bool"));

    LLVMValueRef result;
    model Lm = typed(L->mdl);
    if (is_float(Lm->type)) {
        // for floats, compare with 0.0 and return true if > 0.0
        result = LLVMBuildFCmp(e->builder, LLVMRealOLE, L->value,
                               LLVMConstReal(Lm->type, 0.0), "float-not");
    } else if (is_unsigned(Lm->type)) {
        // for unsigned integers, compare with 0
        result = LLVMBuildICmp(e->builder, LLVMIntULE, L->value,
                               LLVMConstInt(Lm->type, 0, 0), "unsigned-not");
    } else {
        // for signed integers, compare with 0
        result = LLVMBuildICmp(e->builder, LLVMIntSLE, L->value,
                               LLVMConstInt(Lm->type, 0, 0), "signed-not");
    }
    return value(emodel("bool"), null, result);
}

enode aether_e_bitwise_not(aether e, enode L) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("bool"));

    return value(L->mdl, L->meta, LLVMBuildNot(e->builder, L->value, "bitwise-not"));
}

// Au o = obj("type-name", props)
enode aether_e_eq(aether e, enode L, enode R);

enode aether_e_is(aether e, enode L, Au R) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("bool"));

    enode L_type =  e_offset(e, L, _i64(-sizeof(struct _Au)));
    enode L_ptr  =    e_load(e, L_type, null); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = e_operand(e, R, null, null);
    return aether_e_eq(e, L_ptr, R_ptr);
}


enode aether_e_cmp(aether e, enode L, enode R) {
    Au_t Lt = isa(L->literal);
    Au_t Rt = isa(R->literal);
    bool Lc = _is_class(L->mdl) != null;
    bool Rc = _is_class(R->mdl) != null;
    bool Ls = _is_struct(L->mdl) != null;
    bool Rs = _is_struct(R->mdl) != null;
    bool Lr = is_realistic(L->mdl);
    bool Rr = is_realistic(R->mdl);

    if (Lt && Rt) { // ifdef functionality
        bool Lr = (Lt->traits & AU_TRAIT_REALISTIC);
        bool Rr = (Rt->traits & AU_TRAIT_REALISTIC);
        i32 diff = 0;

        if (Lr || Rr) {
            f64 L_64;
            f64 R_64;

            if      (Lt == typeid(f32)) L_64 = *(f32*)L->literal;
            else if (Lt == typeid(f64)) L_64 = *(f64*)L->literal;
            else fault("unexpected float format");

            if      (Rt == typeid(f32)) R_64 = *(f32*)R->literal;
            else if (Rt == typeid(f64)) R_64 = *(f64*)R->literal;
            else fault("unexpected float format");

            diff = L_64 - R_64;
            diff = (diff < 0) ? -1 : (diff > 0) ? 1 : 0; // it doesn't make sense to change the result for floats

        } else
            diff = compare(L->literal, R->literal);
        
        Au literal = _i32(diff);
        enode res = value(emodel("i32"), null, LLVMConstInt(LLVMInt32Type(), diff, 0));
        res->literal = hold(literal);
        return res;
    }

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("i32"));

    record   rec_ctx;
    function mdl_ctx;
    context(e, &rec_ctx, &mdl_ctx, typeid(function));
    verify(mdl_ctx, "non-const compare must be in a function");

    if (Lc || Rc) {
        if (!Lc) {
            enode t = L;
            L = R;
            R = t;
        }
        function eq = find_member(L->mdl, string("compare"), typeid(function));
        if (eq) {
            verify(eq->rtype == emodel("i32"), "compare function must return i32, found %o", eq->rtype);
            // check if R is compatible with argument
            // if users want to allow different data types, we need to make the argument more generic
            // this is better than overloads since the code is in one place
            return e_fn_call(e, eq, L, a(R));
        } else {
            LLVMValueRef lt = LLVMBuildICmp(e->builder, LLVMIntULT, L->value, R->value, "cmp_class-lt");
            LLVMValueRef gt = LLVMBuildICmp(e->builder, LLVMIntUGT, L->value, R->value, "cmp_class-gt");

            LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
            LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
            LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);

            // if L<R → -1, else if L>R → 1, else 0
            LLVMValueRef lt_val = LLVMBuildSelect(e->builder, lt, neg_one, zero, "lt_val");
            LLVMValueRef result = LLVMBuildSelect(e->builder, gt, pos_one, lt_val, "cmp_f");
            return value(emodel("i32"), null, result);
        }
    } else if (Ls || Rs) {
        // iterate through struct members, checking against each with recursion

        LLVMValueRef result = LLVMConstInt(LLVMInt32Type(), 0, 0);
        LLVMBasicBlockRef block      = LLVMGetInsertBlock(e->builder);
        LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(block, "cmp_exit");

        // incoming edges for phi
        array phi_vals   = array(alloc, 32, unmanaged, true); // LLVMValueRef
        array phi_blocks = array(alloc, 32, unmanaged, true); // LLVMBasicBlockRef

        verify(Ls && Rs && (L->mdl == R->mdl), "struct type mismatch %o != %o",
            L->mdl->mem->name, R->mdl->mem->name);
        pairs(L->mdl->members, i) {
            emember mem = i->value;

            // get the struct member at [ mem->index ]
            if (instanceof(mem->mdl, typeid(function)))
                continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(e->builder, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(e->builder, R->value, mem->index, "rv");
            enode        le  = value(mem->mdl, mem->meta, lv);
            enode        re  = value(mem->mdl, mem->meta, rv);
            enode        cmp = e_cmp(e, le, re);

            LLVMValueRef is_nonzero = LLVMBuildICmp(
                e->builder, LLVMIntNE, cmp->value,
                LLVMConstInt(LLVMInt32Type(), 0, 0),
                "cmp_nonzero");

            // create blocks for next-field vs exit
            LLVMBasicBlockRef next_block = LLVMAppendBasicBlock(block, "cmp_next");
            LLVMBasicBlockRef exit_here  = LLVMAppendBasicBlock(block, "cmp_exit_here");

            // branch: if nonzero → exit_here, else → next
            LLVMBuildCondBr(e->builder, is_nonzero, exit_here, next_block);

            // in exit_here: set result = cmp and jump to exit_block
            LLVMPositionBuilderAtEnd(e->builder, exit_here);

            push(phi_vals,   cmp->value);
            push(phi_blocks, exit_here);

            LLVMBuildBr(e->builder, exit_block);

            // move to next_block for the next member
            LLVMPositionBuilderAtEnd(e->builder, next_block);
        }

        push(phi_vals,   LLVMConstInt(LLVMInt32Type(), 0, 0));
        push(phi_blocks, LLVMGetInsertBlock(e->builder)); // fallthrough

        // at the end, build a PHI in exit_block that merges:
        //   - 0 (from fallthrough if all equal)
        //   - cmp (from whichever exit_here branch)
        LLVMPositionBuilderAtEnd(e->builder, exit_block);
        LLVMValueRef phi = LLVMBuildPhi(e->builder, LLVMInt32Type(), "cmp_phi");
        LLVMAddIncoming(phi, vdata(phi_vals), vdata(phi_blocks), len(phi_vals));

        return value(emodel("i32"), null, phi);

    } else if (L->mdl != R->mdl) {
        if (Lr && Rr) {
            // convert to highest bit width
            if (L->mdl->size_bits > R->mdl->size_bits) {
                R = e_create(e, L->mdl, L->meta, R);
            } else {
                L = e_create(e, R->mdl, R->meta, L);
            }
        } else if (Lr) {
            R = e_create(e, L->mdl, L->meta, R);
        } else if (Rr) {
            L = e_create(e, L->mdl, L->meta, R);
        } else {
            if (L->mdl->size_bits > R->mdl->size_bits) {
                R = e_create(e, L->mdl, L->meta, R);
            } else {
                L = e_create(e, R->mdl, R->meta, L);
            }
        }
    }

    if (Lr || Rr) {
        // we need a select with a less than, equal to, else
        // for float we must return -1 0 and 1

        LLVMValueRef lt = LLVMBuildFCmp(e->builder, LLVMRealOLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildFCmp(e->builder, LLVMRealOGT, L->value, R->value, "cmp_gt");

        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);

        // if L<R → -1, else if L>R → 1, else 0
        LLVMValueRef lt_val = LLVMBuildSelect(e->builder, lt, neg_one, zero, "lt_val");
        LLVMValueRef result = LLVMBuildSelect(e->builder, gt, pos_one, lt_val, "cmp_f");

        return value(emodel("i32"), null, result);
    }

    return value(emodel("i32"), null, LLVMBuildSub(e->builder, L->value, R->value, "diff-i"));
}

// todo: simply call e_cmp, and check for 0 difference (IR will be optimized in AST processing)
// we do not want to have eq() and cmp() as required implementation
enode aether_e_eq(aether e, enode L, enode R) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("bool"));

    enode        cmp = e_cmp(e, L, R);
    LLVMValueRef z   = LLVMConstInt(LLVMInt32Type(), 0, 0);
    return value(emodel("bool"), null, LLVMBuildICmp(e->builder, LLVMIntEQ, cmp->value, z, "cmp-i"));

#if 0
    // todo: bring this back in of course
    Au_t Lt = isa(L->literal);
    Au_t Rt = isa(R->literal);
    bool  Lc = _is_class(L->mdl);
    bool  Rc = _is_class(R->mdl);
    bool  Ls = _is_struct(L->mdl);
    bool  Rs = _is_struct(R->mdl);
    bool  Lr = is_realistic(L->mdl);
    bool  Rr = is_realistic(R->mdl);

    if (Lt && Rt) { // ifdef functionality
        bool Lr = (Lt->traits & AU_TRAIT_REALISTIC);
        bool Rr = (Rt->traits & AU_TRAIT_REALISTIC);
        bool is_eq = false;

        if (Lr || Rr) {
            f64 L_64;
            f64 R_64;

            if      (Lt == typeid(f32)) L_64 = *(f32*)L->literal;
            else if (Lt == typeid(f64)) L_64 = *(f64*)L->literal;
            else fault("unexpected float format");

            if      (Rt == typeid(f32)) R_64 = *(f32*)R->literal;
            else if (Rt == typeid(f64)) R_64 = *(f64*)R->literal;
            else fault("unexpected float format");

            is_eq = L_64 == R_64;

        } else
            is_eq = compare(L->literal, R->literal) == 0;
        
        Au literal = _bool(is_eq);
        enode res = value(emodel("bool"), null, LLVMConstInt(LLVMInt1Type(), is_eq, 0));
        res->literal = hold(literal);
        return res;
    }

    if (Lc || Rc) {
        if (!Lc) {
            enode t = L;
            L = R;
            R = t;
        }
        function eq = find_member(L->mdl, string("eq"), typeid(function));
        if (eq) {
            // check if R is compatible with argument
            // if users want to allow different data types, we need to make the argument more generic
            // this is better than overloads since the code is in one place
            return e_fn_call(e, eq, L, a(R));
        }
    } else if (Ls || Rs) {
        // iterate through struct members, checking against each with recursion
        LLVMValueRef result = LLVMConstInt(LLVMInt1Type(), 1, 0);

        verify(Ls && Rs && (L->mdl == R->mdl), "struct type mismatch %o != %o",
            L->mdl->mem->name, R->mdl->mem->name);
        pairs(L->mdl->members, i) {
            emember mem = i->value;

            // get the struct member at [ mem->index ]
            if (instanceof(mem->mdl, typeid(function)))
                continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(e->builder, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(e->builder, R->value, mem->index, "rv");
            enode        le  = value(mem->mdl, mem->meta, lv);
            enode        re  = value(mem->mdl, mem->meta, rv);
            enode        cmp = e_eq(e, le, re);

            result = LLVMBuildAnd(e->builder, result, cmp->value, "eq-struct-and");
        }
        return value(emodel("bool"), null, result);

    } else if (L->mdl != R->mdl) {
        if (Lr && Rr) {
            // convert to highest bit width
            if (L->mdl->size_bits > R->mdl->size_bits) {
                R = e_create(e, L->mdl, R);
            } else {
                L = e_create(e, R->mdl, L);
            }
        } else if (Lr) {
            R = e_create(e, L->mdl, R);
        } else if (Rr) {
            L = e_create(e, L->mdl, R);
        } else {
            if (L->mdl->size_bits > R->mdl->size_bits) {
                R = e_create(e, L->mdl, R);
            } else {
                L = e_create(e, R->mdl, L);
            }
        }
    }

    if (Lr || Rr)
        return value(emodel("bool"), null, LLVMBuildFCmp(e->builder, LLVMRealOEQ, L->value, R->value, "eq-f"));
    
    return value(emodel("bool"), null, LLVMBuildICmp(e->builder, LLVMIntEQ, L->value, R->value, "eq-i"));
#endif
}

enode aether_e_not_eq(aether e, enode L, enode R) {
    return e_not(e, e_eq(e, L, R));
}

enode aether_e_fn_return(aether e, Au o) {
    record rec_ctx;
    function f;
    context(e, &rec_ctx, &f, typeid(function));

    verify (f, "function not found in context");

    e->is_const_op = false;
    if (e->no_build) return e_noop(e, f->rtype);

    if (!o) return value(f->rtype, f->rmeta, LLVMBuildRetVoid(e->builder));

    enode direct = o;
    enode conv = e_create(e, f->rtype, f->rmeta, e_operand(e, o, null, null));
    return value(f->rtype, f->rmeta, LLVMBuildRet(e->builder, conv->value));
}

model formatter_type(aether e, cstr input) {
    cstr p = input;
    // skip flags/width/precision
    while (*p && strchr("-+ #0123456789.", *p)) p++;
    if (strncmp(p, "lld", 3) == 0 || 
        strncmp(p, "lli", 3) == 0) return emodel("i64");
    if (strncmp(p, "llu", 3) == 0) return emodel("u64");
    
    switch (*p) {
        case 'd': case 'i':
                  return emodel("i32"); 
        case 'u': return emodel("u32");
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                  return emodel("f64");
        case 's': return emodel("symbol");
        case 'p': return model(mod, e, is_ref, true, src, emodel("i8"));
    }
    fault("formatter implementation needed");
    return null;
}

enode aether_e_fn_call(aether e, function fn, enode target, array args) { // we could support an array or map arg here, for args
    e->is_const_op = false; // we set this when we do something complex
    if (e->no_build) return e_noop(e, fn->rtype);

    verify(fn->mem, "no member on fn");
    finalize(fn->mem);
    use(fn);
    
    verify((!target && !fn->target) || (target && fn->target),
        "target mismatch for %o", fn);

    int n_args = args ? len(args) : 0;
    LLVMValueRef* arg_values = calloc((fn->target != null) + n_args, sizeof(LLVMValueRef));
    LLVMTypeRef  F = fn->type;
    LLVMValueRef V = fn->value;

    int index = 0;
    if (target) {
        model cast_to = fn->target->mdl->ptr ? fn->target->mdl->ptr : fn->target->mdl;
        LLVMValueRef cast_target = LLVMBuildBitCast(
            e->builder,
            target->value,
            cast_to->type,
            "cast_target");
        arg_values[index++] = cast_target;
    }

    int fmt_idx  = fn->format ? fn->format->format_index - 1 : -1;
    int arg_idx  = fn->format ? fn->format->arg_index    - 1 : -1;
    if (args) {
        int i = 0;
        each (args, Au, value) {
            if (index == arg_idx) break;
            if (index == fmt_idx) {
                i++;
                index++;
                continue;
            }

            emember    f_arg = value_by_index(fn->args->members, i);
            Au_t      vtype = isa(value);
            enode en_arg = instanceof(value, typeid(enode));
            if (f_arg->is_formatter) {
                Au fmt = en_arg ? instanceof(en_arg->literal, typeid(const_string)) : null;
                verify(fmt, "formatter functions require literal, constant strings");
            }
            enode conv = (en_arg && en_arg->mdl == f_arg->mdl) ? en_arg : 
                e_create(e, f_arg->mdl, f_arg->meta, value);
            
            LLVMValueRef vr = arg_values[index] = conv->value;
            i++;
            index++;
        }
    }
    int istart = index;
    /*
    if (fn->format) {
        Au_t a0 = isa(args->origin[fmt_idx]);
        enode   fmt_node = instanceof(args->origin[fmt_idx], typeid(enode));
        string fmt_str  = instanceof(fmt_node->literal, typeid(string));
        verify(fmt_str, "expected string literal at index %i for format-function: %o", fmt_idx, fn->name);
        arg_values[fmt_idx] = fmt_node->value;
        int soft_args = 0;
        symbol p = fmt_str->chars;
        while  (p[0]) {
            if (p[0] == '%' && p[1] != '%') {
                model arg_type  = formatter_type(e, (cstr)&p[1]);
                Au     o_arg     = args->origin[arg_idx + soft_args];
                enode n_arg     = e_load(e, o_arg, null);
                enode conv      = e_create(e, arg_type, null, n_arg);
                arg_values[arg_idx + soft_args] = conv->value;
                soft_args++;
                index    ++;
                p += 2;
            } else
                p++;

        }
        verify((istart + soft_args) == len(args), "%o: formatter args mismatch", fn->name);
    }*/
    
    bool is_void_ = is_void(fn->rtype);
    LLVMValueRef R = LLVMBuildCall2(e->builder, F, V, arg_values, index, is_void_ ? "" : "call");
    free(arg_values);
    return value(fn->rtype, fn->rmeta, R);
}

enode aether_e_op(aether e, OPType optype, string op_name, Au L, Au R) {
    e->is_const_op = false; // we can be granular about this, but its just not worth the complexity for now
    emember mL = instanceof(L, typeid(enode)); 
    enode   LV = e_operand(e, L, null, null);
    enode   RV = e_operand(e, R, null, null);

    // check for overload
    if (op_name && isa(L) == typeid(enode) && _is_class(((enode)L)->mdl)) {
        enode Ln = L;
        Au_t type = isa(Ln->mdl);
        model rec = _is_record(Ln->mdl) ? Ln->mdl : null;
        if (rec) {
            emember Lt = null;
            pairs(rec->members, i) {
                emember mem = i->value;
                function f = instanceof(mem->mdl, typeid(function));
                if (f && f->is_oper == optype) {
                    Lt = mem;
                    break;
                }
            }
            if  (Lt) {
                function f = Lt->mdl;
                verify(len(f->args->members) == 1,
                    "expected 1 argument for operator method");
                /// convert argument and call method
                emember arg_expects = value_by_index(f->args->members, 0);
                enode  conv = e_create(e, arg_expects->mdl, arg_expects->meta, Ln);
                array args = array_of(conv, null);
                verify(mL, "L-operand is invalid data-type");
                return e_fn_call(e, Lt->mdl, mL, args);
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    enode Lnode = L;
    Au_t ltype = isa(L);
    model rtype = determine_rtype(e, optype,
        (isa(L) == typeid(enode) && Lnode->loaded) ? LV->mdl->src : LV->mdl, RV->mdl); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too
    array rmeta = LV->meta;

    LLVMValueRef RES;
    LLVMTypeRef  LV_type = LLVMTypeOf(LV->value);
    LLVMTypeKind vkind = LLVMGetTypeKind(LV_type);

    enode LL = optype == OPType__assign ? LV : e_create(e, rtype, rmeta, LV); // we dont need the 'load' in here, or convert even
    enode RL = e_create(e, rtype, RV->meta, RV);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = e->builder;
    Au literal = null;
    // if not storing ...
    if (optype == OPType__or || optype == OPType__and) {
        // ensure both operands are i1
        model m_bool = emodel("bool");
        rtype = m_bool;
        LL = e_create(e, m_bool, null, LL); // generate compare != 0 if not already i1
        RL = e_create(e, m_bool, null, RL);
        struct op_entry* op = &op_table[optype - OPType__add];
        if (LL->literal && RL->literal)
            RES = op->f_const_op(LL->value, RL->value);
        else
            RES = op->f_build_op(B, LL->value, RL->value, N);
    } else if (optype >= OPType__add && optype <= OPType__left) {
        struct op_entry* op = &op_table[optype - OPType__add];
        if (LL->literal && RL->literal)
            RES = op->f_const_op(LL->value, RL->value);
        else
            // we must override the logic here because we're ONLY doing OPType__or, OPType__and
            RES = op->f_build_op(B, LL->value, RL->value, N);

    } else if (optype == OPType__equal)
        return e_eq(e, LL, RL);
    else if (optype == OPType__compare)
        return e_cmp(e, LL, RL);
    else {
        /// assignments perform a store
        verify(optype >= OPType__assign && optype <= OPType__assign_left, "invalid assignment operation");
        verify(mL, "left-hand operator must be a emember");
        /// already computed in R-value
        if (optype == OPType__assign) {
            RES = RL->value;
            literal = RL->literal;
        } else
        /// store from operation we call, membered in OPType enumeration
        /// todo: build all op tables in Au-type (we are lacking these definitions)
            RES = op_table[optype - OPType__assign_add].f_build_op(B, LL->value, RL->value, N);
        LLVMBuildStore(B, RES, mL->value);
    }
    return enode(
        mod,        e,
        mdl,        rtype,
        meta,       rmeta,
        literal,    literal,
        value,      RES);
}

enode aether_e_or (aether e, Au L, Au R) { return e_op(e, OPType__or,  string("or"),  L, R); }
enode aether_e_xor(aether e, Au L, Au R) { return e_op(e, OPType__xor, string("xor"), L, R); }
enode aether_e_and(aether e, Au L, Au R) { return e_op(e, OPType__and, string("and"), L, R); }
enode aether_e_add(aether e, Au L, Au R) {
    return e_op(e, OPType__add, string("add"), L, R);
}
enode aether_e_sub(aether e, Au L, Au R) { return e_op(e, OPType__sub, string("sub"), L, R); }
enode aether_e_mul(aether e, Au L, Au R) { return e_op(e, OPType__mul, string("mul"), L, R); }
enode aether_e_div(aether e, Au L, Au R) { return e_op(e, OPType__div, string("div"), L, R); }
enode aether_value_default(aether e, Au L, Au R) { return e_op(e, OPType__value_default, string("value_default"), L, R); }
enode aether_cond_value   (aether e, Au L, Au R) { return e_op(e, OPType__cond_value,    string("cond_value"), L, R); }


enode aether_e_inherits(aether e, enode L, Au R) {
    e->is_const_op = false;
    if (e->no_build) return e_noop(e, emodel("bool"));

    // Get the type pointer for L
    enode L_type =  e_offset(e, L, _i64(-sizeof(Au)));
    enode L_ptr  =    e_load(e, L, null);
    enode R_ptr  = e_operand(e, R, null, null);

    // Create basic blocks for the loopf
    LLVMBasicBlockRef block      = LLVMGetInsertBlock(e->builder);
    LLVMBasicBlockRef loop_block = LLVMAppendBasicBlock(block, "inherit_loop");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(block, "inherit_exit");

    // Branch to the loop block
    LLVMBuildBr(e->builder, loop_block);

    // Loop block
    LLVMPositionBuilderAtEnd(e->builder, loop_block);
    LLVMValueRef phi = LLVMBuildPhi(e->builder, L_ptr->mdl->type, "current_type");
    LLVMAddIncoming(phi, &L_ptr->value, &block, 1);

    // Compare current type with R_type
    enode cmp       = e_eq(e, value(L_ptr->mdl, L_ptr->meta, phi), R_ptr);

    // Load the parent pointer (assuming it's the first emember of the type struct)
    enode parent    = e_load(e, value(L_ptr->mdl, L_ptr->meta, phi), null);

    // Check if parent is null
    enode is_null   = e_eq(e, parent, value(parent->mdl, parent->meta, LLVMConstNull(typed(parent->mdl)->type)));

    // Create the loop condition
    enode not_cmp   = e_not(e, cmp);
    enode not_null  = e_not(e, is_null);
    enode loop_cond = e_and(e, not_cmp, not_null);

    // Branch based on the loop condition
    LLVMBuildCondBr(e->builder, loop_cond->value, loop_block, exit_block);

    // Update the phi enode
    LLVMAddIncoming(phi, &parent->value, &loop_block, 1);

    // Exit block
    LLVMPositionBuilderAtEnd(e->builder, exit_block);
    LLVMValueRef result = LLVMBuildPhi(e->builder, cmp->mdl->type, "inherit_result");
    LLVMAddIncoming(result, &cmp->value, &loop_block, 1);
    LLVMAddIncoming(result, &(LLVMValueRef){LLVMConstInt(LLVMInt1Type(), 0, 0)}, &block, 1);

    return value(emodel("bool"), null, result);
}





static none enumeration_finalize(enumeration en) {
    if (en->finalized) return;
    en->finalized = true;
    aether e = en->mod;
    if (!en->src)
         en->src = emodel("i32");

    //en->size_bits = en->src->size_bits;

    // set ABI size/alignment
    model emdl = typed(en);
    en->size_bits      = LLVMABISizeOfType(e->target_data, emdl->type)      * 8;
    en->alignment_bits = LLVMABIAlignmentOfType(e->target_data, emdl->type) * 8;

    if (!en->imported_from) {
        en->global_values = map(hsize, 8);
        pairs(en->members, i) {
            emember mem = i->value;
            // can DBG link to a global value an accessed from a namespace E?
            string global_name = f(string, "%o_%o", en, mem->name);
            LLVMValueRef g = LLVMAddGlobal(e->module, emdl->type, cstring(global_name));
            LLVMSetLinkage       (g, LLVMExternalLinkage);
            LLVMSetInitializer   (g, mem->value);
            LLVMSetGlobalConstant(g, true);
            LLVMSetUnnamedAddr   (g, true);
            set(en->global_values, en->mem->name,
                enode(mod, e, value, g, mdl, en->atype, meta, null));
        }
    }
}

static void record_finalize(record rec) {
    enumeration en = instanceof(rec, typeid(enumeration));
    if (rec->finalized || rec->is_abstract || en) {
        rec->finalized = true;
        return;
    }
    int    total = 0;
    aether e     = rec->mod;
    array  a     = array(32);
    a->assorted = true;
    Class  is_cl = instanceof(rec, typeid(Class)); // given direct instance from emember_finalize()
    model  r     = is_cl ? pointer(rec) : (model)rec;

    if (is_cl && is_cl->parent) {
        emember_finalize(is_cl->parent->mem);
    }

    if (!rec->members && rec->ptr) {
        rec->members = hold(rec->ptr->members);
    }
    
    rec->finalized = true;

    // this must now work with 'schematic' model
    while (r) {
        int this_total = 0;
        pairs(r->members, i) {
            total ++;
            this_total++;
        }
        push(a, r);
        r = read_parent(r);
        if (r == emodel("Au")) // Au members are hidden, unless the object itself is an Au
            break;
    }
    rec->total_members = total;
    if (len(a) > 1)
        a = reverse(a); // parent first when finalizing
    
    LLVMTargetDataRef target_data = rec->mod->target_data;
    LLVMTypeRef*     member_types = calloc(total + 2, sizeof(LLVMTypeRef));
    LLVMMetadataRef* member_debug = calloc(total + 2, sizeof(LLVMMetadataRef));
    bool             is_uni       = instanceof(rec, typeid(uni)) != null;
    int              index        = 0;
    emember           largest      = null;
    sz               sz_largest   = 0;

    if (isa(rec) == typeid(Class)) {
        verify(!get(rec->members, string("__f")),  "__f is reserved name");
        set(rec->members, string("__f"),  emember(
            mod, e, name, string("__f"),  mdl, rec->schema_type->ptr, context, rec, is_hidden, true));
        total += 1;
    }

    each (a, record, r) {
        push(e, r);
        pairs(r->members, i) {
            string  k   =  i->key;
            emember mem = i->value;
            verify( mem->name && mem->name->chars,  "no name on emember: %p (type: %o)", mem, r);

            if (instanceof(mem->mdl, typeid(function)) || ((r != rec->ptr && r != rec) && mem->is_hidden))
                continue;

            if (!mem->mdl->is_ref)
                finalize(mem);

            enumeration en = instanceof(mem->mdl, typeid(enumeration));
            member_types[index] = en ? en->src->type : mem->mdl->type;
            LLVMTypeRef member_type = member_types[index];
            if (!member_type) {
                 member_type = emodel("ARef")->type;
            }
            int abi_size = (member_type && member_type != LLVMVoidType()) ?
                LLVMABISizeOfType(target_data, member_type) : 0;
            
            member_debug[index++] = mem->debug;
            if (!sz_largest || abi_size > sz_largest) {
                largest = mem;
                sz_largest = abi_size;
            }
        }
        pop(e);
    }

    if (is_uni) {
        verify(sz_largest, "cannot determine size of union");
        LLVMStructSetBody(rec->type, &largest->mdl->type, 1, 0);
    } else {
        if (index == 0) {
            LLVMTypeRef f = isa(rec) == typeid(Class) ? 
                LLVMPointerType(LLVMInt8Type(), 0) : LLVMInt8Type();
            LLVMStructSetBody(rec->type, &f, 1, 0);
        } else {
            LLVMStructSetBody(rec->type, member_types, index, 1);
        }
    }

    rec->size_bits = LLVMABISizeOfType(target_data, rec->type) * 8;
    //print("final size: %i for type %o", rec->size, rec->name);

    /// set offsets on members (needed for the method finalization)
    num imember = 0;
    each (a, record, r) {
        for (int iter = 0; iter < 2; iter++)
        pairs(r->members, i) {
            emember mem = i->value;
            if (instanceof(mem->mdl, typeid(function))) // fns do not occupy membership on the instance
                continue;
            if (iter == 0 && mem->access <  interface_public) continue;
            if (iter == 1 && mem->access != interface_intern) continue;
            mem->index  = imember;
            mem->offset_bits = LLVMOffsetOfElement(target_data, rec->type, imember) * 8;

            mem->debug = LLVMDIBuilderCreateMemberType(
                e->dbg_builder,        // LLVMDIBuilderRef
                top(e)->scope,         // Scope of the emember (can be the struct, class or base module)
                cstring(mem->name),    // Name of the emember
                len(mem->name),        // Length of the name
                e->file,               // The file where the emember is declared
                mem->name->line,       // Line number where the emember is declared
                mem->mdl->size_bits,      // Size of the emember in bits (e.g., 32 for a 32-bit int)
                mem->mdl->alignment_bits, // Alignment of the emember in bits
                mem->offset_bits,      // Offset in bits from the start of the struct or class
                0,                     // Debug info flags (e.g., 0 for none)
                mem->mdl->debug);

            if (!instanceof(r, typeid(uni))) // unions have 1 emember
                imember++;
        }
    }

    record _is_record = instanceof(rec, typeid(record));
    if (_is_record) {
        pointer(_is_record);
    }
    
    int sz = LLVMABISizeOfType     (target_data, rec->type);
    int al = LLVMABIAlignmentOfType(target_data, rec->type);
    
    LLVMMetadataRef prev = rec->debug;
    if (prev) {
        rec->debug = LLVMDIBuilderCreateStructType(
            e->dbg_builder,                     // Debug builder
            top(e)->scope,                // Scope (module or file)
            cstring(rec->mem->name),      // Name of the struct
            len(rec->mem->name),
            e->file,                      // File where it’s defined
            rec->mem->name->line,         // Line number where it’s defined
            sz, al,                       // Size, Alignment in bits
            LLVMDIFlagZero,               // Flags
            rec->parent ? rec->parent->debug : null, // Derived from (NULL in C)
            member_debug,                 // Array of emember debug info
            total,                        // Number of members
            0,                            // Runtime language (0 for none)
            NULL,                         // No VTable
            NULL, 0);
        LLVMMetadataReplaceAllUsesWith(prev, rec->debug);
    }
}


static void function_finalize(function fn) {
    aether e = fn->mod;

    if (fn->finalized) return;
    if (fn->is_module_init)
        aether_finalize_init(e, fn);

    fn->finalized = true;
    if (fn->value)
        return;
    // todo: code coverage in orbiter (act less of an idiot)
    fn->is_elsewhere = !fn->cgen && (!fn->body || len(fn->body) == 0);
    if (fn->is_elsewhere && !fn->extern_name) {
        record rec;
        context(e, &rec, null, null);
        if (rec)
            fn->extern_name = hold(f(string, "%o_%o", rec, fn));
        else
            fn->extern_name = hold(f(string, "%o", fn));
    }

    int n_args = fn->args ? len(fn->args->members) : 0;
    LLVMTypeRef* arg_types = calloc(4 + (fn->instance != null) + 
        n_args, sizeof(LLVMTypeRef));
    int index  = 0;

    if (fn->instance) {
        Au_t t = isa(fn->instance);
        Au_t src = fn->instance->src ? isa(fn->instance->src) : null;
        bool check = fn->instance->src && 
                (isa(fn->instance->src) == typeid(structure) || 
                 isa(fn->instance->src) == typeid(Class));
        if (!check) {
            int test2 = 2;
            test2 += 2;
        }
        verify (check,
            "target [incoming] must be record type (struct / class) -- it is then made pointer-to record");
        
        /// we set is_arg to prevent registration of global 
        model mtarget = translate_target(fn->instance);

        // our abstract is to have type differences for struct and ref struct, 
        // but NOT any form of ref class, less the user is setting a pointer indirectly;
        // class is inherently a reference to our abstract
        fn->target = hold(emember(mod, e, mdl, mtarget, name, string("a"), is_arg, true));
        arg_types[index++] = mtarget->ptr ? mtarget->ptr->type : mtarget->type;
    }

    verify(!fn->args || isa(fn->args) == typeid(eargs), "arg mismatch");
    
    if (fn->args)
        pairs(fn->args->members, i) {
            emember arg = i->value;
            model arg_mdl = arg->mdl;
            Au info = header(arg);
            //print("type = %o (%s:%i)", arg_mdl, info->source, info->line);
            verify (arg_mdl, "no LLVM type found for arg %o", arg->name);
            arg_types[index++] = arg_mdl->type;
        }

    fn->arg_types = arg_types;
    fn->arg_count = index;
    fn->type      = LLVMFunctionType(
        fn->rtype ? fn->rtype->type : LLVMVoidType(),
        fn->arg_types, fn->arg_count, fn->va_args);
    fn->ptr       = pointer(fn);

    if (!fn->imported_from) use(fn);
}

void function_use(function fn) {
    aether e = fn->mod;
    if (fn->value || fn->scope) return;

    if (fn->mem->name || fn->extern_name) {
        fn->value = LLVMAddFunction(fn->mod->module,
            fn->extern_name ? fn->extern_name->chars : fn->mem->name->chars, fn->type);
    }
    bool is_extern = !!fn->imported_from || fn->exported;

    int index = 0;
    /// create debug info for eargs (including target)
    if (fn->value) {
        index = 0;
        if (fn->target) {
            fn->target->value = LLVMGetParam(fn->value, index++);
            fn->target->is_arg = true;
        }
        if (fn->args)
            pairs(fn->args->members, i) {
                emember arg = i->value;
                arg->value = LLVMGetParam(fn->value, index++);
                arg->is_arg = true;
            }
        //verify(fmem, "fn emember access not found");
        LLVMSetLinkage(fn->value,
            is_extern ? LLVMExternalLinkage : LLVMInternalLinkage);

        if (fn->is_module_init)
            set_module_init(fn->mod->module, fn);

        if (!is_extern || fn->exported) {
            // Create fn debug info
            LLVMMetadataRef subroutine = LLVMDIBuilderCreateSubroutineType(
                e->dbg_builder,
                e->compile_unit,   // Scope (file)
                NULL,              // Parameter types (None for simplicity)
                0,                 // Number of parameters
                LLVMDIFlagZero     // Flags
            );

            fn->scope = LLVMDIBuilderCreateFunction(
                e->dbg_builder,
                e->compile_unit,        // Scope (compile_unit)
                fn->mem->name->chars, len(fn->mem->name),
                fn->mem->name->chars, len(fn->mem->name),
                e->file,                // File
                e->mem->name->line,     // Line number
                subroutine,             // Function type
                1,                      // Is local to unit
                1,                      // Is definition
                1,                      // Scope line
                LLVMDIFlagZero,         // Flags
                0                       // Is optimized
            );
            // attach debug info to fn
            LLVMSetSubprogram(fn->value, fn->scope);
            fn->entry = LLVMAppendBasicBlockInContext(
                e->module_ctx, fn->value, "entry");
        }

        // make sure we have a pointer type defined for funcitons, since this is not automatic in member registration
        pointer(fn);
    }

    index = 0;
    push(e, fn);
    if (fn->target && false) {
        LLVMMetadataRef meta = LLVMDIBuilderCreateParameterVariable(
            e->dbg_builder,     // DIBuilder reference
            fn->scope,           // The scope (subprogram/fn metadata)
            "a",                // Parameter name
            4,
            1,                  // Argument index (starting from 1, not 0)
            e->file,            // File where it's defined
            isa(fn->mem->name) == typeid(string) ? 0 :
                fn->mem->name->line,      // Line number
            fn->target->mdl->debug,   // Debug type of the parameter (LLVMMetadataRef for type)
            1,                 // AlwaysPreserve (1 to ensure the variable is preserved in optimized code)
            0                  // Flags (typically 0)
        );
        LLVMValueRef first_instr = LLVMGetFirstInstruction(fn->entry);
        
        assert(LLVMIsAInstruction(first_instr), "not a instr"); /// we may simply insert a return if there is nothing?
        
        LLVMValueRef decl  = LLVMDIBuilderInsertDeclareRecordBefore(
            e->dbg_builder,                 // The LLVM builder
            fn->target->value,              // The LLVMValueRef for the first parameter
            meta,                           // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder),       // Current debug location
            first_instr);                   // Attach it in the fn's entry block
        index++;
    }

    pairs(fn->args->members, i) {
        emember arg = i->value;
        LLVMValueRef param_value = LLVMGetParam(fn->value, index);

        if (fn->scope) {
            /// create debug for parameter here
            LLVMMetadataRef param_meta = LLVMDIBuilderCreateParameterVariable(
                e->dbg_builder,          // DIBuilder reference
                fn->scope,         // The scope (subprogram/fn metadata)
                cstring(arg->name),    // Parameter name
                len(arg->name),
                1 + index,         // Argument index (starting from 1, not 0)
                e->file,           // File where it's defined
                arg->name->line,   // Line number
                arg->mdl->debug,   // Debug type of the parameter (LLVMMetadataRef for type)
                1,                 // AlwaysPreserve (1 to ensure the variable is preserved in optimized code)
                0                  // Flags (typically 0)
            );
            
            LLVMValueRef decl = LLVMDIBuilderInsertDeclareRecordAtEnd(
                e->dbg_builder,
                param_value, 
                param_meta, // debug metadata for the first parameter
                LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
                LLVMGetCurrentDebugLocation2(e->builder), // current debug location
                fn->entry); // attach it in the fn's entry block
        }
        arg->value = param_value;
        index++;
    }
    pop(e);
}

// this will be the only 'finalize' anyone (outside of emember_finalize) calls!
// we also make internal the create_schema
void emember_finalize(emember mem) {
    aether e   = mem->mod;
    model  mdl = mem->mdl;

    if (!mdl || (mdl->finalized && mem->is_type)) return;
    if (!mdl->mem) mdl->mem = mem;
    mem->finalized = true;

    Au_t type = isa(mdl);
    if (type == typeid(model) && mdl->src && mdl->src->type) {
        model  src = mdl->src;
        if (src && src->mem) {
            u64 ptr_sz = LLVMPointerSize(e->target_data);
            string src_name = src->mem->name;
            if (src_name) {
                int ln = len(src_name);
                mdl->debug = LLVMDIBuilderCreatePointerType(e->dbg_builder, src->debug,
                    ptr_sz * 8, 0, 0, src_name->chars, ln);
            }
        }
    }

    mdl = model_source(mdl); // resolve class record from its use-case pointer
    bool finalize_model = !mdl->finalized;
    
    if (finalize_model && index_of(e->finalizing, mdl) >= 0)
        return;

    record      rec   = instanceof(mdl, typeid(record));
    function    f     = instanceof(mdl, typeid(function));
    enumeration en    = instanceof(mdl, typeid(enumeration));
    macro       mac   = instanceof(mdl, typeid(macro));
    if         (mac) return;

    i32 index = -1;

    if (finalize_model) {
        push(e->finalizing, mdl);
        index = len(e->finalizing) - 1;
        if      (en)  enumeration_finalize(mdl);
        else if (rec) record_finalize(mdl);
        else if (f)   function_finalize(mdl);
    }

    model ctx = mem->context ? mem->context : top(e);
    enumeration ctx_en = aether_context_model(e, typeid(enumeration));
    function    is_fn    = instanceof(mdl, typeid(function));
    if (ctx_en || is_fn || (isa(mdl)->module != typeid(aether)->module)) return;

    Au_t mdl_type = isa(mdl);
    
    function ctx_fn  = aether_context_model(e, typeid(function));
    bool     is_init = false;
    record   ctx_rec = aether_context_model(e, typeid(Class)); // todo: convert to record
    if (!ctx_rec) ctx_rec = aether_context_model(e, typeid(structure));
    if (!ctx_rec) ctx_rec = aether_context_model(e, typeid(uni));

    if (ctx_fn && ctx_fn->is_module_init) {
        is_init = true;
        ctx_fn = null;
    }

    model t = typed(mem->mdl);
    bool is_module = ctx == e || ctx->is_global;
    bool external_member = e->current_include || e->is_Au_import;

    if (!ctx_rec) {
    if (ctx_fn && !mem->value) {
        
        verify (!mem->value, "value-ref already set auto emember");
        model src = t->src ? t->src : t;
        mem->value = LLVMBuildAlloca(e->builder, src->type, cstring(mem->name));
        mem->debug = LLVMDIBuilderCreateAutoVariable(
            e->dbg_builder,           // DIBuilder reference
            ctx_fn->scope,          // The scope (subprogram/fn metadata)
             cstring(mem->name),     // Variable name
            len(mem->name),
            e->file,            // File where it’s declared
            mem->name->line,    // Line number
            src->debug,    // Type of the variable (e.g., LLVMMetadataRef for int)
            true,               // Is this variable always preserved (DebugPreserveAll)?
            0,                  // Flags (usually 0)
            0                   // Align (0 is default)
        );

        // Attach the debug info to the actual LLVM IR value using llvm.dbg.value
        //LLVMBuildDbgValue(
        //    e->builder,              // LLVM Builder
        //    mem->value,              // The LLVMValueRef for the value being assigned to the emember
        //    mem->debug,              // Debug info for the variable
        //    LLVMGetCurrentDebugLocation2(e->builder));
        
        LLVMValueRef firstInstr = LLVMGetFirstInstruction(ctx_fn->entry);
        if (!firstInstr) {
            // If there’s no instruction in the block yet, use the block itself as the insertion point
            firstInstr = (LLVMValueRef)ctx_fn->entry;
        }

        LLVMDIBuilderInsertDeclareRecordBefore(
            e->dbg_builder, // The LLVM builder
            mem->value, // The LLVMValueRef for the first parameter
            mem->debug, // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder), // Current debug location
            firstInstr);
    } else if (t && is_module && !mem->is_type) {
        mem->is_global = true;
        mem->value = LLVMAddGlobal(e->module, t->type, mem->name->chars);
        bool use_intern = !external_member && 
            (mem->access == interface_intern || t->is_internal);
        LLVMSetLinkage(mem->value, use_intern ? LLVMInternalLinkage : LLVMExternalLinkage);
        if (!external_member) {
            LLVMSetInitializer(mem->value, LLVMConstNull(t->type));
        }
    } else if (t && !mem->is_type && is_init && !mem->is_decl) { // we're importing so its not adding this global -- for module includes, it should
        symbol name = mem->name->chars;
        LLVMTypeRef type = t->type;
        if (isa(mem->mdl)->context != typeid(model) && !mem->value) {
            mem->is_global = true;
            mem->value = LLVMAddGlobal(e
                ->module, type, name); // its created here (a-map)
            //LLVMSetGlobalConstant(mem->value, mem->is_const);
            LLVMSetInitializer(mem->value, LLVMConstNull(type));
            bool is_public = (int)mem->access >= (int)interface_public && !t->is_internal;
            LLVMSetLinkage(mem->value, is_public ? LLVMExternalLinkage : LLVMPrivateLinkage);
            LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0);
            LLVMMetadataRef meta = LLVMDIBuilderCreateGlobalVariableExpression(
                e->dbg_builder, e->scope, name, len(mem->name), NULL, 0, e->file, 1, t->debug, 
                0, expr, NULL, 0);
            LLVMGlobalSetMetadata(mem->value, LLVMGetMDKindID("dbg", 3), meta);
        }
    }
    }

    if (index != -1) { // convert to list item, this makes no sense
        verify(index_of(e->finalizing, mdl) == index, "weird?");
        remove(e->finalizing, index);
    }
}

model aether_top(aether e) {
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model mdl = e->lex->origin[i];
        if (!_is_record (mdl) && 
            !instanceof(mdl, typeid(statements)) && 
            !instanceof(mdl, typeid(eargs)) && !mdl->open) continue;
        return mdl;
    }
    return e;
}


void eargs_init (eargs a) {
    if (!a->members)
         a->members = map(hsize, 4);
}

define_class (codegen, Au)

define_class (static_array, array)

define_class (mschema,      Au)
define_class (model,        Au)
define_class (format_attr,  Au)
define_class (ident2,       Au) // useful class to represent ident for operations, and not conversion to a model; this disambiguates the two use-cases
define_class (clang_cc,     Au)

define_class (statements,   model)
define_class (catcher,      model)
define_class (eargs,        model)
define_class (function,     model)
define_class (Namespace,    model)
define_class (record,       Namespace)
define_class (aether,       model)
define_class (uni,          record)
define_class (enumeration,  model)
define_class (structure,    record)
define_class (Class,        record)
define_class (code,         Au)
define_class (enode,        Au)
define_class (emember,      enode)