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

// def -> base for change from type to emember (emember has model 
//#define ecall(M, ...) aether_##M(e, ## __VA_ARGS__) # .cms [ c-like module in silver ]

#define emodel(N)     ({            \
    emember  m = lookup2(e, string(N), null); \
    model mdl = m ? m->mdl : null;  \
    mdl;                            \
})

#define elookup(N)     ({ \
    emember  m = lookup2(e, string(N), null); \
    m; \
})

#define emem(M, N) emember(mod, e, name, string(N), mdl, M)

#define earg(M, N) emember(mod, e, name, string(N), mdl, M, is_arg, true)

#define value(m,vr) enode(mod, e, value, vr, mdl, m)

#define mvalue(m,vr) emember(mod, e, value, vr, mdl, m)

emember aether_register_model(aether e, model mdl) {
    bool is_func  = instanceof(mdl, typeid(fn))    != null;
    bool is_macro = instanceof(mdl, typeid(macro)) != null;
    model m = isa(mdl) == typeid(Class) ? pointer(mdl, null) : mdl;
    emember mem   = emember(
        mod,      e,
        mdl,      m,
        name,     mdl->name,
        is_func,  is_func,
        is_macro, is_macro,
        is_type,  true);
    register_member(e, mem);
    print("registered model %o", mdl->name);
    return mem;
}

map aether_top_member_map(aether e) {
    model top = e->top;
    for (int i = len(e->lex) - 2; i >= 0; i--) {
        model ntop = e->lex->elements[i];
        if (ntop->members == top->members) {
            top = ntop;
            continue;
        }
        break;
    }
    if (!top->members)
        top->members = map(hsize, 16);
    return top->members;
}

void aether_register_member(aether e, emember mem) {
    if (!mem || mem->registered)
        return;
    if (eq(mem->name, "symbol")) {
        verify(mem->is_type, "expected is_type");
    }
    mem->registered = true;

    map members = aether_top_member_map(e);
    string  key = string(mem->name->chars);

    set(members, string(mem->name->chars), mem);
    set_model(mem, mem->mdl);
}

#define no_target null

/// this is more useful as a primitive, to include actual A-type in aether's primitive abstract
AType model_primitive(model mdl) {
    model src = mdl->src;
    while (instanceof(src, typeid(model))) {
        src = src->src;
    }
    return isa(src);
}
model model_resolve(model f) {
    while (instanceof(f->src, typeid(model))) {
        if (f->is_ref)
            return f;
        f = f->src;
    }
    return f;
}

static AType src_type(model m) {
    AType t = isa(m);
    if (!t) return m;
    return t;
}

bool is_bool     (model f) { f = model_resolve(f); return f->src && src_type(f->src) == typeid(bool); }
bool is_float    (model f) { f = model_resolve(f); return f->src && src_type(f->src) == typeid(f32);  }
bool is_double   (model f) { f = model_resolve(f); return f->src && src_type(f->src) == typeid(f64);  }
bool is_realistic(model f) { f = model_resolve(f); return f->src && src_type(f->src)->traits & A_TRAIT_REALISTIC; }
bool is_integral (model f) { f = model_resolve(f); return f->src && src_type(f->src)->traits & A_TRAIT_INTEGRAL;  }
bool is_signed   (model f) { f = model_resolve(f); return f->src && src_type(f->src)->traits & A_TRAIT_SIGNED;    }
bool is_unsigned (model f) { f = model_resolve(f); return f->src && src_type(f->src)->traits & A_TRAIT_UNSIGNED;  }
bool is_primitive(model f) {
    f = model_resolve(f); 
    return f->src && isa(f->src)->traits & A_TRAIT_PRIMITIVE;
}

bool is_void     (model f) {
    f = model_resolve(f); 
    return f ? f->size_bits == 0 : false;
}

bool is_generic  (model f) {
    f = model_resolve(f);
    return (AType)f->src == typeid(A);
}

bool is_record   (model f) {
    return is_struct(f) || is_class(f);
}

bool is_class    (model f) {
    if (isa(f) == typeid(Class))
        return true;
    f = model_resolve(f); 
    return f && f->is_ref && f->src && isa(f->src) == typeid(Class);
}

bool is_struct   (model f) {
    if (isa(f) == typeid(structure))
        return true;
    f = model_resolve(f); 
    return isa(f) == typeid(structure);
}

bool is_ref      (model f) {
    f = model_resolve(f); 
    if (f->is_ref)
        return true;
    model src = f->src;
    while (instanceof(src, typeid(model))) {
        src = src->src;
        if (f->is_ref)
            return true;
    }
    AType asrc = src;
    // A-type base could still be a pointer-type
    if (asrc) {
        return (asrc->traits & A_TRAIT_POINTER) != 0;
    }
    return false;
}

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
        model ctx = e->lex->elements[i];
        if (res->len)
            append(res, "/");
        append(res, ctx->name->chars);
    }
    print("%o: %o", res, msg);
}

#define print_context(msg, ...) print_ctx(e, form(string, msg, ## __VA_ARGS__))

string model_cast_string(model mdl) {
    if (mdl->name) return string(mdl->name->chars);
    int depth = 0;
    while (mdl->src) {
        if (mdl->is_ref)
            depth++;
        A src = mdl->src;
        mdl = instanceof(src, typeid(model));
        if (mdl && mdl->name) {
            if (depth == 1)
                return form(string, "ref %o", mdl->name);
            else {
                string res = form(string, "%o", mdl->name);
                for (int i = 0; i < depth; i++)
                    append(res, "*");
                return res;
            }
        } else if (isa(src) == null) {
            AType type = (AType)src;
            return string(type->name);
        }
    }
    fault("could not get name for model");
    return null;
}

i64 model_cmp(model mdl, model b) {
    return typed(mdl)->type == typed(b)->type ? 0 : -1;
}

void model_init(model mdl) {
    aether  e = mdl->mod;
    mdl->imported_from = (e && e->current_include) ? e->current_include : null;
    eargs args = instanceof(mdl, typeid(eargs));
    statements st = instanceof(mdl, typeid(statements));
    fn         f = instanceof(mdl, typeid(fn));

    if ((e && e != mdl) && !mdl->is_user && !f && !args && !st && (!mdl->name || eq(mdl->name, "")) && !mdl->src) {
        print("useless model");
    }

    if (!mdl->src || isa(mdl) != typeid(model)) // Class sets src in the case of array's element
        return;

    // no need if we have a resolved type
    //if (!mdl->is_ref && mdl->src && isa(mdl->src) && mdl->src->type)
    //    return;
    
    /// narrow down type traits
    model mdl_src = mdl;
    AType mdl_type = isa(mdl);
    if (instanceof(mdl_src, typeid(model))) {
        while (instanceof(mdl_src, typeid(model)) && mdl_src->src) {
            if (mdl_src->is_ref)
                break;
            AType a_src = mdl_src->src;
            mdl_src = mdl_src->src;
        }
    } else if (mdl->src) {
        mdl_src = mdl->src; // must be a type definition here
        verify(isa(mdl_src) == 0x00, "expected type definition");
    }
    AType type = isa(mdl_src) ? (AType)isa(mdl_src) : (AType)mdl_src;

    if ((type->traits & A_TRAIT_PRIMITIVE) != 0 ||
        (type->traits & A_TRAIT_ABSTRACT) != 0) {
        // we must support count in here, along with src being set
        if (type == typeid(f32))
            mdl->type = LLVMFloatType();
        else if (type == typeid(f64))
            mdl->type = LLVMDoubleType();
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
        else if (type == typeid(member))
            mdl->type = emodel("member");
        else if (type == typeid(symbol) || type == typeid(cstr) || type == typeid(raw)) {
            if (type == typeid(raw)) {
                mdl = mdl;
            }
            mdl->type = LLVMPointerType(LLVMInt8Type(), 0);
        } else if (type == typeid(cstrs))
            mdl->type = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
        else if (type == typeid(sz)     || 
                 type == typeid(handle)) {
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
            LLVMTypeRef fn_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
            mdl->type = LLVMPointerType(fn_type, 0);
        } else if (type == typeid(hook)) {
            model e_A = emodel("A");
            LLVMTypeRef param_types[] = { e_A->type };
            LLVMTypeRef hook_type = LLVMFunctionType(e_A->type, param_types, 1, 0);
            mdl->type = LLVMPointerType(hook_type, 0);
        } else if (type == typeid(callback)) {
            model e_A = emodel("A");
            LLVMTypeRef param_types[] = { e_A->type, e_A->type };
            LLVMTypeRef cb_type = LLVMFunctionType(e_A->type, param_types, 2, 0);
            mdl->type = LLVMPointerType(cb_type, 0);
        } else {
            fault("unsupported primitive %s", type->name);
        }
        if (mdl->type && mdl->type != LLVMVoidType())
            mdl->debug = LLVMDIBuilderCreateBasicType(
                e->dbg_builder,     // Debug info builder
                type->name,         // Name of the primitive type (e.g., "int32", "float64")
                strlen(type->name), // Length of the name
                LLVMABISizeOfType(e->target_data, mdl->type) * 8, // Size in bits (e.g., 32 for int, 64 for double)
                type->name[0] == 'f' ? 0x04 : 0x05, // switching based on f float or u/i int (on primitives)
                0);
    } else {
        // we still need static array (use of integral shape), aliases

        // can be a class, structure, fn
        if (type == typeid(model)) {
            /// now we should handle the case 
            model  src = mdl->src;
            AType src_cl = isa(src);
            if (src_cl == typeid(fn))
                use((fn)src); // type not set on function until we use it
            /// this is a reference, so we create type and debug based on this
            u64 ptr_sz = LLVMPointerSize(e->target_data);
            mdl->type  = LLVMPointerType(
                src->type == LLVMVoidType() ? LLVMInt8Type() : src->type, 0);
            model src_name = mdl->name ? mdl : (model)mdl->src;
            if (src_name->name) {
                int ln = len(src_name->name);
                mdl->debug = LLVMDIBuilderCreatePointerType(e->dbg_builder, src->debug,
                    ptr_sz * 8, 0, 0, src_name->name->chars, ln);
            }
        } else if (instanceof(mdl_src, typeid(record))) {
            record rec = mdl_src;
            mdl->type  = rec->type;
            mdl->debug = rec->debug;
        } else if (type == typeid(fn)) {
            fn fn = mdl_src;
            mdl->type  = fn->type;
            mdl->debug = fn->debug;
        } else if (type == typeid(aether))
            return;
        else if (type != typeid(aether)) {
            fault("unsupported model type: %s", type->name);
        }
    } 
    if (mdl->type && mdl->type != LLVMVoidType()) { /// we will encounter errors with aliases to void
        Class cl = instanceof(mdl, typeid(Class));
        //if (!cl || !cl->is_abstract) {
            LLVMTypeRef type = mdl->type;
            /// todo: validate: when referencing these, we must find src where type != null
            mdl->size_bits      = LLVMABISizeOfType     (mdl->mod->target_data, type) * 8;
            mdl->alignment_bits = LLVMABIAlignmentOfType(mdl->mod->target_data, type) * 8;
        //}
    }
    if (instanceof(mdl, typeid(record))) {
        mdl->scope = mdl->debug; // set this in record manually when debug is set
    } else if (!mdl->scope)
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
    if (isa(mdl) == typeid(Class) && mdl->parent) {
        return mdl->parent;
    }
    return null;
}

bool model_e_inherits(model mdl, model base) {
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

/// we allocate all pointer models this way
model model_pointer(model mdl, string opt_name) {
    if (!mdl->ptr)
        mdl->ptr = model(
            mod,    mdl->mod,  name,    opt_name ? opt_name : (string)mdl->name,
            is_ref, true,      members, mdl->members,
            body,   mdl->body, src,     mdl); // todo: why was i setting type on here? mdl->type is NOT the type of a pointer, that type is made within
    return mdl->ptr;
}

void statements_init(statements st) {
    aether e = st->mod;
    AType atop = isa(e->top);
    st->scope = LLVMDIBuilderCreateLexicalBlock(e->dbg_builder, e->top->scope, e->file, 1, 0);
}

void aether_e_print_node(aether e, enode n) {
    /// would be nice to have a lookup method on import
    /// call it im_lookup or something so we can tell its different
    emember printf_fn  = elookup("printf");
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
    verify(fmt, "eprint_node: unsupported model: %o", n->mdl->name);
    e_fn_call(e, printf_fn,
        array_of(e_operand(e, string(fmt), null), n, null));
}

model model_source(model mdl) {
    if (mdl->src && instanceof(mdl->src, typeid(model)) && mdl->src->ptr == mdl)
        mdl = mdl->src;
    return mdl;
}

// f = format string; this is evaluated from nodes given at runtime
void aether_eprint(aether e, symbol f, ...) {
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
            push(schema, A_i32(n));
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
    each(schema, A, obj) {
        enode n = instanceof(obj, typeid(enode));
        if (n) {
            e_print_node(e, n);
        } else {
            string s = instanceof(obj, typeid(string));
            verify(s, "invalid type data");
            enode n_str = e_operand(e, s, null);
            e_print_node(e, n_str);
        }
    }

    va_end(args);
    free(arg_nodes);
    free(buf);
}

emember aether_e_var(aether e, model mdl, string name) {
    enode   a =  e_create(e, mdl, null);
    emember m = emember(mod, e, name, name, mdl, mdl);
    register_member(e, m);
    e_assign(e, m, a, OPType__assign);
    return m;
}

void code_init(code c) {
    fn fn = context_model(c->mod, typeid(fn));
    c->block = LLVMAppendBasicBlock(fn->value, c->label);
}

void code_seek_end(code c) {
    LLVMPositionBuilderAtEnd(c->mod->builder, c->block);
}

void aether_e_cmp_code(aether e, enode l, comparison comp, enode r, code lcode, code rcode) {
    enode load_l = e_load(e, l, null);
    enode load_r = e_load(e, l, null);
    LLVMValueRef cond = LLVMBuildICmp(
        e->builder, (LLVMIntPredicate)comp, load_l->value, load_r->value, "cond");
    LLVMBuildCondBr(e->builder, cond, lcode->block, rcode->block);
}

enode aether_e_element(aether e, enode array, A index) {
    enode i = e_operand(e, index, null);
    enode element_v = value(e, LLVMBuildInBoundsGEP2(
        e->builder, typed(array->mdl)->type, array->value, &i->value, 1, "eelement"));
    return e_load(e, element_v, null);
}

void aether_e_inc(aether e, enode v, num amount) {
    enode lv = e_load(e, v, null);
    LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), amount, 0);
    LLVMValueRef nextI = LLVMBuildAdd(e->mod->builder, lv->value, one, "nextI");
    LLVMBuildStore(e->mod->builder, nextI, v->value);
}

/// needs to import classes, structs, methods within the classes
/// there is no such thing as a global spec for AType fns
/// we simply can have lots of static methods on A, though.

map member_map(aether e, cstr field, ...) {
    va_list args;
    va_start(args, field);
    cstr value;
    map  res = map(hsize, 32);
    while ((value = va_arg(args, cstr)) != null) {
        string n   = new(string, chars, value);
        A mdl = va_arg(args, A);
        AType  ty  = isa(mdl);
        emember mem = emember(mod, e, name, n, mdl, va_arg(args, cstr));
        set(res, n, mem);
    }
    va_end(args);
    return res;
}

void aether_e_branch(aether e, code c) {
    LLVMBuildBr(e->builder, c->block);
}

void aether_build_initializer(aether e, fn m) { }

bool is_module_level(model mdl) {
    aether e = mdl->mod;
    pairs(e->members, i) {
        emember mem = i->value;
        if (mem->mdl == mdl) return true;
    }
    return false;
}

/*

#define define_arb(TYPE, BASE, TYPE_SZ, TRAIT, VMEMBER_COUNT, VMEMBER_TYPE, POST_PUSH, ...) \
    _Pragma("pack(push, 1)") \
    __thread struct _af_recycler TYPE##_af; \
    TYPE##_info TYPE##_i; \
    _Pragma("pack(pop)") \
    static __attribute__((constructor)) bool Aglobal_##TYPE() { \
        TYPE##_f* type_ref = &TYPE##_i.type; \
        BASE##_f* base_ref = &BASE##_i.type; \
        if (!AType_i.type.name) { \
            AType_i.type.name   = "AType"; \
            AType_i.type.traits = A_TRAIT_BASE; \
            AType_i.type.src    = typeid(A); \
        } \
        int validate_count = 0; \
        char* member_validate[256] = {}; \
        if ((AType)type_ref == (AType)base_ref) \
            type_ref->name = #TYPE; \
        if ((AType)type_ref != (AType)base_ref && (!base_ref->traits)) { \
            lazy_init((global_init_fn)&Aglobal_##TYPE); \
            return false; \
        } else { \
            printf("global ctr for %s\n", #TYPE); \
            fflush(stdout); \
            memset(type_ref, 0,        sizeof(TYPE##_f)); \
            if ((AType)type_ref != (AType)base_ref) \
                memcpy(type_ref, base_ref, sizeof(BASE##_f)); \
            type_ref->af = &TYPE##_af; \
            type_ref->af->re_alloc = 1024; \
            type_ref->af->re = (object*)(A*)calloc(1024, sizeof(A)); \
            type_ref->isize = 0 TYPE##_schema( TYPE, ISIZE, __VA_ARGS__ ); \
            type_ref->magic = 1337; \
            static struct _member members[512]; \
            type_ref->src = (AType)base_ref != typeid(A) \
                 ? (AType)base_ref : (AType)null; \
            type_ref->parent_type = \
                (__typeof__(type_ref->parent_type))&BASE##_i.type; \
            type_ref->name          = #TYPE; \
            type_ref->module        = (cstr)MODULE; \
            type_ref->members       = members; \
            type_ref->member_count  = 0; \
            type_ref->size          = TYPE_SZ; \
            type_ref->vmember_count = VMEMBER_COUNT; \
            type_ref->vmember_type  = (AType)VMEMBER_TYPE; \
            type_ref->traits        = TRAIT; \
            type_ref->meta          = (_meta_t) { emit_types(__VA_ARGS__) }; \
            type_ref->arb           = primitive_ffi_arb(typeid(TYPE)); \

            set all member data:
            
            TYPE##_schema( TYPE, INIT, __VA_ARGS__ ) \
            push_type((AType)type_ref); \
            POST_PUSH; \
            return true; \
        } \
    } \

*/

static i32 get_aflags(emember f, i32 extra) {
    fn func = instanceof(f->mdl, typeid(fn));
    return (extra | (func && func->is_override)) ? A_FLAG_OVERRIDE : 0;
}

void aether_finalize_init(aether e, fn f) {
    // register module constructors as global initializers ONLY for delegate modules
    // aether creates a 'main' with argument parsing for its main modules
    member module_init_mem = null;
    model _AType = emodel("_AType");
    model _A     = emodel("A")->src;
    AType _A_cl  = isa(_A);

    // we need to set 'shape' on the AType field
    
    // emit declaration of the struct _type_f for classes, structs, and enums
    pairs (e->members, i) {
        emember mem = i->value;
        model   mdl = mem->mdl;
        if (mdl == f)
            module_init_mem = mem;
        else if (!mdl->imported_from && isa(mdl) == typeid(Class)) {
            

            // we need to add code in module constructor to fill out members on this structure
            // it also needs to be made a global constructor, no need to have a different strategy for apps
        } else {
            // structs are built slightly different
        }
    }

    // now enter the function where we will construct the type_i
    push(e, f);

    pairs (e->members, i) { // these members include our types, then we go through members on those
        emember mem = i->value;
        model   mdl = mem->mdl;

        if (mdl->imported_from) continue;

        Class       cmdl = instanceof(mdl, typeid(Class));
        Class       smdl = instanceof(mdl, typeid(structure));
        enumeration emdl = instanceof(mdl, typeid(enumeration));

        if (!cmdl && !emdl)
            continue;
        
        if (cmdl && cmdl->is_abstract)
            continue;

        symbol null_str = null;
        A      null_v   = primitive(typeid(symbol), &null_str);
        A      null_h   = primitive(typeid(handle), &null_str);
        int    m_count  = 0;

        pairs (mdl->members, ii) {
            emember f = ii->value;
            if (f->access == interface_intern) continue;
            m_count++;
        }

        array members = array(alloc, m_count, assorted, true); // we need to create a singular value for this entire array of members
        i32 current_prop_id = 0;

        pairs (mdl->members, ii) {
            emember   f      = ii->value;
            map       member = map(hsize, 8, assorted, true);
            fn        func   = instanceof(f->mdl, typeid(fn));
            
            if (f->access == interface_intern) continue;
            // structure _meta_t
            map  m_meta = map(unmanaged, true, assorted, true);

            // for functions, this is the argments; for props they are meta descriptors; type-only entries to describe their use
            i32 meta_index = 0;
            if (func) {
                mset(m_meta, "count", A_i64((func->instance ? 1 : 0) + (func->args ? len(func->args->members): 0)));
                if (func->instance) {
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, func->instance); // this should not be the pointer type
                    meta_index++;
                }
                pairs(func->args->members, ai) {
                    model mdl_meta = ((emember)ai->value)->mdl;
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, ident(mdl_meta));
                    meta_index++;
                }
            } else {
                mset(m_meta, "count", A_i64(len(f->meta_args))); // f is field; this is a meta-description for field members using the args we already have
                each(f->meta_args, model, mdl_meta) {
                    string fname = f(string, "meta_%i", meta_index);
                    set(m_meta, fname, ident(mdl_meta));
                    meta_index++;
                }
            }

            // we need to test the output of this
            enode meta_args = e_create(e, emodel("_meta_t"), m_meta);
            
            // we have a field f->meta with a series of model entries in here, these need to lookup the global address of symbols by the type name _i + sizeof(_A) (88 bytes)
            map       margs  = map(unmanaged, true, assorted, true);
            
            enumeration en   = instanceof(f->mdl, typeid(enumeration));

            if (func && func->args) {
                mset(margs, "count", A_i64(len(f->meta_args)));
                i32 meta_index = 0;
                pairs(func->args->members, ai) {
                    emember arg_mem = ai->value;
                    string fname = f(string, "meta_%i", meta_index);
                    set(margs, fname, arg_mem->mdl);
                    meta_index++;
                }
            }
            
            mset(member, "name",            f->name);
            mset(member, "sname",           null_v);
            mset(member, "type",            f->mdl);
            mset(member, "offset",          A_i32(f->offset_bits / 8)); // do we have to add the parent classes minus A-type? (A-type is a header)
            mset(member, "count",           A_i32(1)); // needs to expose member-specific array, however we don't handle it on the member level in silver (we may not need this)
            if (func) {
                mset(member, "member_type", A_i32(get_aflags(f, func->function_type)));
            } else {
                if (emdl)
                    mset(member, "member_type", A_i32(get_aflags(f, A_FLAG_ENUMV)));
                else
                    mset(member, "member_type", A_i32(get_aflags(f, A_FLAG_PROP)));
            }

            OPType otype = ((cmdl || smdl) && func && func->is_oper) ? func->is_oper : 0;
            mset(member, "operator_type",   A_i32(otype));
            mset(member, "required",        A_i32(f->is_require ? 1 : 0));
            mset(member, "args",            margs); // e_create can lookup this field, and convert the map
            // set shape here

            // it must do this if its not enode
            // enode meta_args = e_create(e, emodel("_meta_t"), margs);


            // if its an enum, this is the address of a global instance of the enum
            // if function 
            if (func)
                mset(member, "ptr", enode(mod, e, value, func->value, mdl, pointer(func, null)));
            else if (emdl) { // && member is ENUMV
                enode gvalue = get(en->global_values, f->name);
                verify(gvalue, "value not found for enum %o %o", f->mdl->name, f->name);
                mset(member, "ptr", e_addr_of(e, gvalue, en->vtype));
            } else {
                mset(member, "ptr", null_v);
            }

            mset(member, "method", null_h);
            mset(member, "id",     A_i32(current_prop_id));

            if (!func)
                current_prop_id++;

            push(members, member);
        }

        // allocate members

        // set member count

        // emit members
    }

    // we should create structs for our class tables now; we need members registered for each
    verify(e->top == (model)f, "expected module context");

    if (e->mod->delegate) {
        unsigned ctr_kind = LLVMGetEnumAttributeKindForName("constructor", 11);
        LLVMAddAttributeAtIndex(f->value, 
            LLVMAttributeFunctionIndex, 
            LLVMCreateEnumAttribute(e->module_ctx, ctr_kind, 0));
    } else {

        // check if module will be an app or lib
        // search through members, finding subclass of main
        Class main_class = null;
        Class main_spec  = emodel("main")->src;
        verify(main_spec && instanceof(main_spec, typeid(Class)), "expected main class");
        pairs(e->top->members, i) {
            emember mem = i->value;
            Class   cl  = instanceof(mem->mdl, typeid(Class));
            if (!cl) continue;
            if (cl->parent == main_spec) {
                main_class = cl;
                break;
            }
        }

        bool is_app = main_class != null;
        pairs (e->members, i) {
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
            // this is a library, so we do not implement or call a main
        } else {
            // code main, which is what inits main class
            eargs    args = eargs(mod, e);
            emember  argc = earg(emodel("i32"),   "argc");
            emember  argv = earg(emodel("cstrs"), "argv");
            set(args->members, argc->name, argc);
            set(args->members, argv->name, argv);

            fn main_fn = fn(
                mod,            e,
                name,           string("main"),
                function_type,  A_FLAG_SMETHOD,
                exported,       true,
                rtype,          emodel("i32"),
                args,           args);

            push(e, main_fn);

            // from main_fn: call module initialize (initializes module members)
            e_fn_call(e, module_init_mem, null);

            // create main class described by user
            // we will want to serialize properties for the class as well
            // if there are required properties, we can use this to exit with 1
            // must call A_init
            e_create(e, main_class, map());

            // return i32, which could come from a cast on the class if implemented
            e_fn_return(e, A_i32(255));
            
            pop(e);
            use(main_fn);
            register_model(e, main_fn);
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
}

void fn_finalize(fn f) {
    if (f->finalized) return;
    aether e = f->mod;

    if (f->is_module_init)
        aether_finalize_init(e, f);

    int   index   = 0;
    f->finalized = true;

    if (!f->entry)
        return;

    index = 0;
    push(e, f);
    if (f->target && false) {
        LLVMMetadataRef meta = LLVMDIBuilderCreateParameterVariable(
            e->dbg_builder,     // DIBuilder reference
            f->scope,           // The scope (subprogram/fn metadata)
            "a",                // Parameter name
            4,
            1,                  // Argument index (starting from 1, not 0)
            e->file,            // File where it's defined
            isa(f->name) == typeid(string) ? 0 :
                f->name->line,      // Line number
            f->target->mdl->debug,   // Debug type of the parameter (LLVMMetadataRef for type)
            1,                 // AlwaysPreserve (1 to ensure the variable is preserved in optimized code)
            0                  // Flags (typically 0)
        );
        LLVMValueRef first_instr = LLVMGetFirstInstruction(f->entry);
        
        assert(LLVMIsAInstruction(first_instr), "not a instr"); /// we may simply insert a return if there is nothing?
        
        LLVMValueRef decl  = LLVMDIBuilderInsertDeclareRecordBefore(
            e->dbg_builder,                 // The LLVM builder
            f->target->value,              // The LLVMValueRef for the first parameter
            meta,                           // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder),       // Current debug location
            first_instr);                   // Attach it in the fn's entry block
        index++;
    }

    pairs(f->args->members, i) {
        emember arg = i->value;
        /// create debug for parameter here
        LLVMMetadataRef param_meta = LLVMDIBuilderCreateParameterVariable(
            e->dbg_builder,          // DIBuilder reference
            f->scope,         // The scope (subprogram/fn metadata)
             cstring(arg->name),    // Parameter name
            len(arg->name),
            1 + index,         // Argument index (starting from 1, not 0)
            e->file,           // File where it's defined
            arg->name->line,   // Line number
            arg->mdl->debug,   // Debug type of the parameter (LLVMMetadataRef for type)
            1,                 // AlwaysPreserve (1 to ensure the variable is preserved in optimized code)
            0                  // Flags (typically 0)
        );
        LLVMValueRef param_value = LLVMGetParam(f->value, index);
        LLVMValueRef decl        = LLVMDIBuilderInsertDeclareRecordAtEnd(
            e->dbg_builder,                   // The LLVM builder
            param_value,                
            param_meta,                 // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder),       // Current debug location
            f->entry);                 // Attach it in the fn's entry block
        arg->value = param_value;
        index++;
    }
    pop(e);
}

void fn_init(fn fn) {
    //verify(fn->record || !eq(fn->name, "main"), "to implement main, implement fn module-name[]");
}

// aether will only allow one of these per module
static LLVMValueRef set_module_init(LLVMModuleRef module, fn f) {
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

void fn_use(fn fn) {
    if (fn->value)
        return;
    
    if (eq(fn->name, "alloc2")) {
        int test2 = 2;
        test2    += 2;
    }
    aether            e         = fn->mod;
    int              n_args    = fn->args ? len(fn->args->members) : 0;
    LLVMTypeRef*     arg_types = calloc(4 + (fn->target != null) + n_args, sizeof(LLVMTypeRef));
    int              index     = 0;
    model            top       = e->top;

    print("-------------");

    if (fn->instance) {
        AType t = isa(fn->instance);
        verify (isa(fn->instance) == typeid(structure) || 
                isa(fn->instance) == typeid(Class),
            "target [incoming] must be record type (struct / class) -- it is then made pointer-to record");
        
        /// we set is_arg to prevent registration of global
        fn->target = hold(emember(mod, e, mdl, pointer(fn->instance, null), name, string("a"), is_arg, true));
        print("target = %o", fn->instance->name);
        arg_types[index++] = typed(fn->target->mdl)->type;
    }

    verify(!fn->args || isa(fn->args) == typeid(eargs), "arg mismatch");
    
    A info = null;

    if (fn->args)
        pairs(fn->args->members, i) {
            emember arg = i->value;
            model amdl = typed(arg->mdl);
            info = header(arg);
            print("type = %o (%s:%i)", amdl->name, info->source, info->line);
            verify (amdl && amdl->type, "no LLVM type found for arg %o", arg->name);
            arg_types[index++] = amdl->type;
        }

    fn->arg_types = arg_types;
    fn->arg_count = index;
    model rtype = fn->rtype ? typed(fn->rtype) : null;
    fn->type  = LLVMFunctionType(rtype ? rtype->type : LLVMVoidType(), fn->arg_types, fn->arg_count, fn->va_args);
    if (fn->name || fn->extern_name) {
        fn->value = LLVMAddFunction(fn->mod->module,
            fn->extern_name ? fn->extern_name->chars : fn->name->chars, fn->type);
    }
    bool is_extern = !!fn->imported_from || fn->exported;

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
                fn->name->chars, len(fn->name),
                fn->name->chars, len(fn->name),
                e->file,                // File
                e->name->line,          // Line number
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
    }
}

none enumeration_finalize(enumeration en) {
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
            string global_name = f(string, "%o_%o", en->name, mem->name);
            LLVMValueRef g = LLVMAddGlobal(e->module, emdl->type, cstring(global_name));
            LLVMSetLinkage       (g, LLVMExternalLinkage);
            LLVMSetInitializer   (g, mem->value);
            LLVMSetGlobalConstant(g, true);
            LLVMSetUnnamedAddr   (g, true);
            set(en->global_values, en->name, enode(mod, e, value, g, mdl, en->vtype));
        }
    }
}

void record_finalize(record rec) {
    enumeration en = instanceof(rec, typeid(enumeration));
    if (rec->finalized || rec->is_abstract || en) {
        rec->finalized = true;
        return;
    }
    int    total = 0;
    aether e     = rec->mod;
    array  a     = array(32);
    a->assorted = true;
    record r     = rec;
    Class  is_class = instanceof(rec, typeid(Class));

    if (is_class && is_class->parent) {
        finalize(is_class->parent);
    }
    
    rec->finalized = true;

    // this must now work with 'schematic' model
    // delegation namespace
    while (r) {
        int this_total = 0;
        pairs(r->members, i) {
            total ++;
            this_total++;
        }
        push(a, r);
        r = read_parent(r);
    }
    rec->total_members = total;
    if (len(a) > 1)
        a = reverse(a); // parent first when finalizing
    
    LLVMTargetDataRef target_data = rec->mod->target_data;
    LLVMTypeRef*     member_types = calloc(total, sizeof(LLVMTypeRef));
    LLVMMetadataRef* member_debug = calloc(total, sizeof(LLVMMetadataRef));
    bool             is_uni       = instanceof(rec, typeid(uni)) != null;
    int              index        = 0;
    emember           largest      = null;
    sz               sz_largest   = 0;

    AType r_type = isa(rec);

    each (a, record, r) {
        pairs(r->members, i) {
            string k =  i->key;
            emember mem = i->value;
            AType t = isa(mem->mdl);
            A info = isa(mem);
            verify( mem->name && mem->name->chars,  "no name on emember: %p (type: %o)", mem, r->name);

            if (instanceof(mem->mdl, typeid(fn)))
                continue;

            finalize(mem->mdl);

            member_types[index]   = mem->mdl->type;
            enumeration en = instanceof(mem->mdl, typeid(enumeration));

            int abi_size = mem->mdl->type != LLVMVoidType() ?
                LLVMABISizeOfType(target_data, en ? en->src->type : mem->mdl->type) : 0;
            
            member_debug[index++] = mem->debug;
            if (!sz_largest || abi_size > sz_largest) {
                largest = mem;
                sz_largest = abi_size;
            }
        }
    }

    if (is_uni) {
        verify(sz_largest, "cannot determine size of union");
        LLVMStructSetBody(rec->type, &largest->mdl->type, 1, 0);
    } else {
        // we need to add a padding member manually here, based on it's
        LLVMStructSetBody(rec->type, member_types, index, 1);
    }

    /// set record size
    rec->size_bits = LLVMABISizeOfType(target_data, rec->type) * 8;
    //print("finalized and got size: %i for type %o", rec->size, rec->name);

    /// set offsets on members (needed for the method finalization)
    num imember = 0;
    each (a, record, r)
        pairs(r->members, i) {
            emember mem = i->value;
            if (instanceof(mem->mdl, typeid(fn))) // fns do not occupy membership on the instance
                continue;
            mem->index  = imember;
            mem->offset_bits = LLVMOffsetOfElement(target_data, rec->type, imember) * 8;

            mem->debug = LLVMDIBuilderCreateMemberType(
                e->dbg_builder,        // LLVMDIBuilderRef
                e->top->scope,         // Scope of the emember (can be the struct, class or base module)
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

    record is_record = instanceof(rec, typeid(record));
    if (is_record) {
        pointer(is_record, null);
    }
    
    int sz = LLVMABISizeOfType     (target_data, rec->type);
    int al = LLVMABIAlignmentOfType(target_data, rec->type);
    
    LLVMMetadataRef prev = rec->debug;
    if (prev) {
        rec->debug = LLVMDIBuilderCreateStructType(
            e->dbg_builder,                     // Debug builder
            e->top->scope,                // Scope (module or file)
            cstring(rec->name),                // Name of the struct
            len(rec->name),
            e->file,                      // File where it’s defined
            rec->name->line,              // Line number where it’s defined
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

#define LLVMDwarfTag(tag) (tag)
#define DW_TAG_structure_type 0x13  // DWARF tag for structs.

void record_init(record rec) {
    aether e = rec->mod;
    rec->type = LLVMStructCreateNamed(LLVMGetGlobalContext(), rec->name->chars);

    // Create a forward declaration for the struct's debug info
    if (isa(rec->name) == typeid(token)) {
        if (eq(rec->name, "array_i32_4x2")) {
            int test2 = 2;
            test2    += 2;
        }
        rec->debug = LLVMDIBuilderCreateReplaceableCompositeType(
            e->dbg_builder,                      // Debug builder
            LLVMDwarfTag(DW_TAG_structure_type), // Tag for struct
            cstring(rec->name),                      // Name of the struct
            len(rec->name),
            e->top->scope,                       // Scope (this can be file or module scope)
            e->file,                             // File
            rec->name->line,                     // Line number
            0,
            0,
            0,
            LLVMDIFlagZero,                      // Flags
            NULL,                                // Derived from (NULL in C)
            0);
    }

    if (len(rec->members)) {
        finalize(rec); /// cannot know emember here, but record-based methods need not know this arg (just fns)
    }
}

emember emember_resolve(emember mem, string name) {
    aether  e   = mem->mod;
    i64  index = 0;
    model base = mem->mdl->is_ref ? mem->mdl->src : mem->mdl; // needs to support more than indirection here
    pairs(base->members, i) {
        if (compare(i->key, name) == 0) {
            emember schema = i->value; /// value is our pair value, not a llvm-value
            if (schema->is_const) {
                return schema; /// for enum members, they are const integer; no target stored but we may init that
            }

            LLVMValueRef actual_ptr = mem->is_arg ? mem->value : LLVMBuildLoad2(
                e->builder,
                pointer(base, null)->type,
                mem->value,
                "load_actual_ptr"
            );

            emember target_member = emember(
                mod,    e,          name,   mem->name,
                mdl,    mem->mdl,   value,  actual_ptr);
            
            /// by pass automatic allocation of this emember, as we are assigning to StructGEP from its container
            emember  res = emember(mod, e, name, name, mdl, null, target_member, target_member);
            res->mdl    = schema->mdl;
            fn f = instanceof(schema->mdl, typeid(fn));
            res->value  = f ? f->value : LLVMBuildStructGEP2(
                    e->builder, typed(base)->type, actual_ptr, index, "resolve"); // GPT: mem->value is effectively the ptr value on the stack
            if (f)
                res->is_func = true;
            
            return res;
        }
        index++;
    }
    fault("emember %o not found in type %o", name, base->name);
    return null;
}

void emember_set_value(emember mem, A value) {
    aether e = mem->mod;
    enode n = e_operand(mem->mod, value, null);
    mem->mdl   = n->mdl;
    mem->value = n->value;
    mem->is_const = LLVMIsAConstant(n->value) != null;
}

bool emember_has_value(emember mem) {
    return mem->value != null;
}

void emember_init(emember mem) {
    aether e   = mem->mod;
    if (!mem->name) mem->name = string("");
    if (!mem->access) mem->access = interface_public;
    if (instanceof(mem->name, typeid(string))) {
        string n = mem->name;
        mem->name = token(chars, cstring(n), source, e->source, line, 1);
    }
    set_model(mem, mem->mdl);
}

void emember_set_model(emember mem, model mdl) {
    if (!mdl) return;
    
    AType mdl_type = isa(mdl);
    if (mem->mdl != mdl) {
        verify(!mem->mdl, "model already set on emember");
        mem->mdl = hold(mdl);
    }

    fn is_fn   = instanceof(mdl, typeid(fn));
    if      (is_fn) return;

    aether    e       = mem->mod;
    fn ctx_fn  = aether_context_model(e, typeid(fn));
    bool     is_user = mdl->is_user; /// i.e. import from silver; does not require processing here
    bool     is_init = false;
    record   rec     = aether_context_model(e, typeid(Class)); // todo: convert to record
    if (!rec) rec = aether_context_model(e, typeid(structure));

    if (ctx_fn && ctx_fn->is_module_init) {
        is_init = true;
        ctx_fn = null;
    }

    model t = typed(mem->mdl);

    /// if we are creating a new emember inside of a fn, we need
    /// to make debug and value info here
    if (ctx_fn && !mem->value) {
        
        verify (!mem->value, "value-ref already set auto emember");
        mem->value = LLVMBuildAlloca(e->builder, t->type, cstring(mem->name));
        mem->debug = LLVMDIBuilderCreateAutoVariable(
            e->dbg_builder,           // DIBuilder reference
            ctx_fn->scope,          // The scope (subprogram/fn metadata)
             cstring(mem->name),     // Variable name
            len(mem->name),
            e->file,            // File where it’s declared
            mem->name->line,    // Line number
            t->debug,    // Type of the variable (e.g., LLVMMetadataRef for int)
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
            e->dbg_builder,                   // The LLVM builder
            mem->value,                 // The LLVMValueRef for the first parameter
            mem->debug,                 // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder),       // Current debug location
            firstInstr);
    } else if (e->is_A_import && !mem->is_type && e->top == e) {
        // we need to put restrictions at A-import level, as to what module we are importing from
        // case in point here we dont need to literally add our aether types to our resulting module, to export that too!
        // we only need A-type

        mem->value = LLVMAddGlobal(e->module, t->type, mem->name->chars);
        LLVMSetLinkage(mem->value, LLVMExternalLinkage);
    } else if (!rec && !is_user && !ctx_fn && !mem->is_type && 
               !mem->is_arg && (!e->current_include || e->is_A_import) && is_init && !mem->is_decl) { // we're importing so its not adding this global -- for module includes, it should
        /// add module-global if this has no value, set linkage
        symbol name = mem->name->chars;
        LLVMTypeRef type = t->type;
        // we assign constants ourselves, and value is not set until we register the
        //verify(!mem->is_const || mem->value, "const value mismatch");
        bool is_global_space = false;
        /// they would be abstract above definition of aether model
        //      less we want to start subclassing fnality of the others
        if (isa(mem->mdl)->parent_type != typeid(model) && !mem->value) {
            mem->value = LLVMAddGlobal(e->module, type, name); // its created here (a-map)
            //LLVMSetGlobalConstant(mem->value, mem->is_const);
            LLVMSetInitializer(mem->value, LLVMConstNull(type));
            is_global_space = true; 
        }
        // this would be for very basic primitives, ones where we can eval in design
        // for now its much easier to do them all the same way, via expression in global init
        // they can reference each other there
        if (is_global_space) {
            /// only applies to module members
            bool is_public = mem->access == interface_public;
            // do not do this if value does not come from a fn or AddGlobal above!
            LLVMSetLinkage(mem->value, is_public ? LLVMExternalLinkage : LLVMPrivateLinkage);
            LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0);
            LLVMMetadataRef meta = LLVMDIBuilderCreateGlobalVariableExpression(
                e->dbg_builder, e->scope, name, len(mem->name), NULL, 0, e->file, 1, t->debug, 
                0, expr, NULL, 0);
            LLVMGlobalSetMetadata(mem->value, LLVMGetMDKindID("dbg", 3), meta);
        }
    }
}

#define int_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(i##b)), \
        value, LLVMConstInt(typed(emodel(stringify(i##b)))->type, *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(u##b)), \
        value, LLVMConstInt(typed(emodel(stringify(u##b)))->type, *(u##b*)l, 0))

/*
#define bool_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel("bool"), \
        value, LLVMConstInt(typed(emodel(stringify(u##b)))->type, *(u##b*)l, 0))
*/

#define f32_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(typed(emodel(stringify(f##b)))->type, *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(typed(emodel(stringify(f##b)))->type, *(f##b*)l))

enode aether_e_typeid(aether e, model mdl) {
    model   atype    = emodel("AType");
    string  i_symbol = f(string, "%o_i", mdl->name);
    emember i_member = lookup2(e, i_symbol, null);
    LLVMValueRef g   = i_member->value;

    verify(g, "expected info global instance for type %o", mdl->name);

    emember type_member = get(i_member->mdl->members, string("type")); // type_member has an index of its structure index
    verify(type_member, "expected info global instance for type %o", mdl->name);

    // how do we get this value, which is a pointer this member given the target g and struct i_member->mdl
    return e_load(e, type_member, i_member); // effectively cast as a AType, a pointer to _AType struct (generic type)
}

enode e_operand_primitive(aether e, A op) {
         if (instanceof(op, typeid(  enode))) return op;
    else if (instanceof(op, typeid(  ident))) return e_typeid(e, ((ident)op)->mdl); // needs to return a pointer that points to the symbol for address of name ## _i + 88 bytes offset (enode ops, not runtime)
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
    else if (instanceof(op, typeid(string))) {
        LLVMTypeRef  gs      = LLVMBuildGlobalStringPtr(e->builder, ((string)op)->chars, "chars");
        LLVMValueRef cast_i8 = LLVMBuildBitCast(e->builder, gs, LLVMPointerType(LLVMInt8Type(), 0), "cast_symbol");
        return enode(mod, e, value, cast_i8, mdl, emodel("symbol"), literal, op);
    }
    error("unsupported type in aether_operand");
    return NULL;
}

enode aether_e_operand(aether e, A op, model src_model) {
    if (!op) return value(emodel("none"), null);
    AType op_isa = isa(op);
    Class cl = op;
    if (instanceof(op, typeid(array))) {
        verify(src_model != null, "expected src_model with array data");
        return e_create(e, src_model, op);
    }

    enode r = e_operand_primitive(e, op);
    return src_model ? e_convert(e, r, src_model) : r;
}

enode aether_e_default_value(aether e, model mdl) {
    return e_create(e, mdl, null);
}

/// create is both stack and heap allocation (based on model->is_ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_e_create(aether e, model mdl, A args) {
    map    imap = instanceof(args, typeid(map));
    array  a    = instanceof(args, typeid(array));
    enode   n    = null;
    emember ctr  = null;

    mdl = typed(mdl);

    /// construct / cast methods
    enode input = instanceof(args, typeid(enode));
    if (input) {
        verify(!imap, "unexpected data");
        emember fmem = convertible(input->mdl, mdl);
        verify(fmem, "no suitable conversion found for %o -> %o",
            input->mdl->name, mdl->name);
        /// if same type, return the enode
        if (fmem == (void*)true)
            return e_convert(e, input, mdl); /// primitive-based conversion goes here
        
        fn fn = instanceof(fmem->mdl, typeid(fn));
        if (fn->function_type & A_FLAG_CONSTRUCT) {
            /// ctr: call before init
            /// this also means the mdl is not a primitive
            verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = fmem;
        } else if (fn->function_type & A_FLAG_CAST) {
            /// cast call on input
            return e_fn_call(e, fn, array_of(input, null));
        } else
            fault("unknown error");
    }

    model        src        = mdl->is_ref ? mdl->src : mdl;
    shape        shape      = mdl->shape;
    i64          slen       = shape ? shape->count : 0;
    num          count      = mdl->count ? mdl->count : 1;
    bool         use_stack  = !mdl->is_ref && !is_class(mdl);
    LLVMValueRef size_A     = LLVMConstInt(LLVMInt64Type(), 32, false);
    LLVMValueRef size_mdl   = LLVMConstInt(LLVMInt64Type(), (src->size_bits / 8) * count, false);
    LLVMValueRef total_size = use_stack ? size_mdl : LLVMBuildAdd(e->builder, size_A, size_mdl, "total-size");
    LLVMValueRef alloc      = use_stack ? LLVMBuildAlloca     (e->builder, mdl->type, "alloca-mdl") :
                                          LLVMBuildArrayMalloc(e->builder, LLVMInt8Type(), total_size, "malloc-A-mdl");
    if (!use_stack) {
        LLVMValueRef zero   = LLVMConstInt(LLVMInt8Type(), 0, false);  // Value to fill with
        LLVMBuildMemSet(e->builder, alloc, zero, total_size, 0);
    }
    LLVMValueRef user = use_stack ? alloc : LLVMBuildGEP2(e->builder, mdl->type, alloc, &size_A, 1, "user-mdl");
    n = value(mdl, user);
    
    if (a) {
        num   count = len(a);
        num   i     = 0;
        verify(slen == 0 || (mdl->count == count), "array count mismatch");
        each(a, A, element) {
            enode expr = e_operand(e, element, null); /// todo: cases of array embedding must be handled
            enode conv = e_convert(e, expr, mdl->src);

            LLVMValueRef idx      = LLVMConstInt(LLVMInt32Type(), i++, 0); // pointer to element
            LLVMValueRef elem_ptr = LLVMBuildGEP2(
                e->builder,
                mdl->src->type,
                n->value,  // Base pointer from allocation
                &idx,
                1,
                "array_elem"
            );
            LLVMBuildStore(e->builder, conv->value, elem_ptr);
        }
        return n;
    }

    bool perform_assign = !!args;

    /// iterate through args in map, setting values
    if (imap) {
        verify(is_record(mdl), "model mismatch");
        verify(instanceof(mdl, typeid(record)), "model not a record, and given a map"); ///
        map used  = map();
        int total = 0;
        pairs(imap, i) {
            string arg_name  = instanceof(i->key, typeid(string));
            AType  vtype = isa(i->value);
            enode  arg_value = e_operand(e, i->value, null); // operand of type model should
            set    (used, arg_name, A_bool(true));
            emember m = get(mdl->members, arg_name);
            verify(m, "emember %o not found on record: %o", arg_name, mdl->name);
            enode   arg_conv = e_convert(e, arg_value, m->mdl);

            LLVMValueRef ptr = LLVMBuildStructGEP2(e->builder,
                mdl->type, n->value, m->index, arg_name->chars);
            //arg_value->value = LLVMConstInt(LLVMInt64Type(), 44, 0);
            // for objects, we must increment the ref, too [call the method A_hold]
            LLVMBuildStore(e->builder, arg_conv->value, ptr);
            total++;
        }
        pairs(mdl->members, i) {
            emember mem = i->value;
            if (mem->is_require)
                verify (contains(used, mem->name), "required argument not set: %o", mem->name);
        }
        /// generic > A
        /// call init on the A

    } else if (args) {
        enode a = e_operand(e, args, null);
        /// this should only happen on primitives
        if (perform_assign) {
            enode conv = e_convert(e, a, n->mdl);
            n = e_assign (e, n, conv, OPType__assign);
        }
    }
    return n;
}

enode aether_e_zero(aether e, enode n) {
    model      mdl = n->mdl;
    LLVMValueRef v = n->value;
    LLVMValueRef zero   = LLVMConstInt(LLVMInt8Type(), 0, 0);          // value for memset (0)
    LLVMValueRef size   = LLVMConstInt(LLVMInt64Type(), mdl->size_bits / 8, 0); // size of alloc
    LLVMValueRef memset = LLVMBuildMemSet(e->builder, v, zero, size, 0);
    return n;
}


model prefer_mdl(model m0, model m1) {
    aether e = m0->mod;
    if (m0 == m1) return m0;
    model g = emodel("any");
    if (m0 == g) return m1;
    if (m1 == g) return m0;
    if (model_e_inherits(m0, m1))
        return m1;
    return m0;
}

emember model_member_lookup(model a, string name, AType mtype) {
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
    return enode(mod, mod, mdl, prefer_mdl(true_expr->mdl, false_expr->mdl), value, null);
}

enode aether_e_builder(aether e, subprocedure cond_builder) {
    LLVMBasicBlockRef block = LLVMGetInsertBlock(e->builder);
    LLVMPositionBuilderAtEnd(e->builder, block);
    enode   n = invoke(cond_builder, null);
    return n;
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
        A cond_obj = conds->elements[i];
        enode cond_result = invoke(cond_builder, cond_obj);  // Silver handles the actual condition parsing and building
        LLVMValueRef condition = e_convert(e, cond_result, emodel("bool"))->value;

        // Set the sconditional branch
        LLVMBuildCondBr(e->builder, condition, then_block, else_block);

        // Build the "then" block
        LLVMPositionBuilderAtEnd(e->builder, then_block);
        A expr_obj = exprs->elements[i];
        enode expressions = invoke(expr_builder, expr_obj);  // Silver handles the actual block/statement generation
        LLVMBuildBr(e->builder, merge);

        // Move the builder to the "else" block
        LLVMPositionBuilderAtEnd(e->builder, else_block);
        block = else_block;
    }

    // Handle the fnal "else" (if applicable)
    if (len(exprs) > len(conds)) {
        A else_expr = exprs->elements[len(conds)];
        invoke(expr_builder, else_expr);  // Process the final else block
        LLVMBuildBr(e->builder, merge);
    }

    // Move the builder to the merge block
    LLVMPositionBuilderAtEnd(e->builder, merge);

    // Return some enode or result if necessary (e.g., a enode indicating the overall structure)
    return enode(mod, e, mdl, emodel("none"), value, null);  // Dummy enode, replace with real enode if needed
}

enode aether_e_addr_of(aether e, enode expr, model mdl) {
    model        ref   = pointer(mdl ? mdl : expr->mdl, null); // this needs to set mdl->type to LLVMPointerType(mdl_arg->type, 0)
    emember      m_expr = instanceof(expr, typeid(emember));
    LLVMValueRef value = m_expr ? expr->value :
        LLVMBuildGEP2(e->builder, ref->type, expr->value, NULL, 0, "ref_expr");
    return enode(
        mod,   e,
        value, value,
        mdl,   ref);
}

enode aether_e_offset(aether e, enode n, A offset) {
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
        i = e_operand(e, A_i64(0), null);
        for (int a = 0; a < len(args); a++) {
            A arg_index   = args->elements[a];
            A shape_index = shape->elements[a];
            enode   stride_pos  = e_mul(e, shape_index, arg_index);
            i = e_add(e, i, stride_pos);
        }
    } else
        i = e_operand(e, offset, null);

    verify(mdl->is_ref, "offset requires pointer");

    LLVMValueRef ptr_load   = LLVMBuildLoad2(e->builder,
        LLVMPointerType(mdl->type, 0), n->value, "load");
    LLVMValueRef ptr_offset = LLVMBuildGEP2(e->builder,
         mdl->type, ptr_load, &i->value, 1, "offset");

    return mem ? (enode)emember(mod, e, mdl, mdl, value, ptr_offset, name, mem->name) :
                         enode(mod, e, mdl, mdl, value, ptr_offset);
}

enode aether_e_load(aether e, emember mem, emember target) {
    if (!instanceof(mem, typeid(emember))) return mem; // for enode values, no load required
    model        mdl      = mem->mdl;
    LLVMValueRef ptr      = mem->value;

    // if this is a emember on record, build an offset given its 
    // index and 'this' argument pointer
    if (!ptr || ((target || mem->target_member) && !mem->is_arg && !e->left_hand)) {
        if (mem->is_module) {
            verify(ptr, "expected value for module emember (LLVMAddGlobal result)");
        } else {
            emember t = target ? target : mem->target_member ? mem->target_member : elookup("a"); // unique to the function in class, not the class
            verify(t, "no target found when looking up emember");
            /// static methods do not have this in context
            record rec = (t->mdl && t->mdl->is_ref) ? t->mdl->src : t->mdl;
            ptr = LLVMBuildStructGEP2(
                e->builder, rec->type, t->value, mem->index, "emember-ptr");
        }
    }

    string       label = form(string, "load-emember-%o", mem->name);
    LLVMValueRef res   = (mem->is_arg || e->left_hand) ? ptr :
        LLVMBuildLoad2(e->builder, mdl->type, ptr, cstring(label));
    
    enode r = value(mdl, res);
    r->loaded = true;
    return r;
}

/// general signed/unsigned/1-64bit and float/double conversion
/// [optionally] load and convert expression
enode aether_e_convert(aether e, enode expr, model rtype) {
    expr = e_load(e, expr, null);
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
    else if (is_ref(F) && is_ref(T))
        V = LLVMBuildPointerCast(B, V, T->type, "ptr_cast");

    // ptr to int
    else if (is_ref(F) && is_integral(T))
        V = LLVMBuildPtrToInt(B, V, T->type, "ptr_to_int");

    // int to ptr
    else if (is_integral(F) && is_ref(T))
        V = LLVMBuildIntToPtr(B, V, T->type, "int_to_ptr");

    // bitcast for same-size types
    else if (F_kind == T_kind)
        V = LLVMBuildBitCast(B, V, T->type, "bitcast");

    else if (F_kind == LLVMVoidTypeKind)
        V = LLVMConstNull(T->type);
    else
        fault("unsupported cast");

    enode res = value(T,V);
    res->literal = hold(expr->literal);
    return res;
}

model aether_context_model(aether e, AType type) {
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->elements[i];
        //if (isa(ctx) == type)
        if (inherits(isa(ctx), type))
            return ctx;
    }
    return null;
}

model aether_return_type(aether e) {
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->elements[i];
        if (isa(ctx) == typeid(fn) && ((fn)ctx)->rtype)
            return ((fn)ctx)->rtype;
    }
    return null;
}

void assign_args(aether e, enode L, A R, enode* r_L, enode* r_R) {
    *r_R = e_operand(e, R, null);
    emember Lm = instanceof(L, typeid(emember));
    if (Lm && !Lm->is_const)
        *r_L = L;//load(e, Lm);
    else
        *r_L = L;
}

struct op_entry {
    LLVMValueRef(*f_op)(LLVMBuilderRef, LLVMValueRef L, LLVMValueRef R, symbol);
};

static struct op_entry op_table[] = {
    { LLVMBuildAdd },
    { LLVMBuildSub }, 
    { LLVMBuildMul }, 
    { LLVMBuildSDiv }, 
    { LLVMBuildOr }, 
    { LLVMBuildAnd },
    { LLVMBuildXor },  
    { LLVMBuildURem }
};

enode aether_e_assign(aether e, enode L, A R, OPType op) {
    int v_op = op;
    verify(op >= OPType__assign && op <= OPType__assign_left, "invalid assignment-operator");
    enode rL, rR = e_operand(e, R, null);
    enode res = rR;
    
    if (op != OPType__assign) {
        rL = e_load(e, L, null);
        res = value(L->mdl,
            op_table[op - OPType__assign - 1].f_op
                (e->builder, rL->value, rR->value, e_str(OPType, op)->chars));
    }

    if (res->mdl != L->mdl)
        res = e_convert(e, res, L->mdl);

    LLVMBuildStore(e->builder, res->value, L->value);
    return res;
}

/// look up a emember in lexical scope
/// this applies to models too, because they have membership as a type entry
emember aether_lookup2(aether e, A name, AType mdl_type_filter) {
    if (!name) return null;
    if (instanceof(name, typeid(token)))
        name = cast(string, (token)name);
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx      = e->lex->elements[i];
        AType ctx_type = isa(ctx);
        Class cl       = instanceof(ctx, typeid(Class));
        Class parent   = cl ? cl->parent : null;
        model top      = e->top;
        cstr  n = cast(cstr, (string)name);
        // combine the two member checks
        do {
            emember m = ctx->members ? get(ctx->members, string(n)) : null;
            if (m) {
                AType mdl_type = isa(m->mdl);
                if (mdl_type_filter) {
                    if (mdl_type != mdl_type_filter)
                        continue;
                }
                return m;
            }
            if  (parent) parent = parent->parent;
            cl = parent ? parent : null;
            ctx = cl;
        } while (ctx);
    }
    return null;
}

model aether_push(aether e, model mdl) {
    fn fn_prev = context_model(e, typeid(fn));
    if (fn_prev) {
        fn_prev->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);
    }

    verify(mdl, "no context given");
    fn f = instanceof(mdl, typeid(fn));
    if (f)
        use(f);

    push(e->lex, mdl);
    e->top = mdl;
    
    if (f) {
        LLVMPositionBuilderAtEnd(e->builder, f->entry);
        if (LLVMGetBasicBlockTerminator(f->entry) == NULL) {
            LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
                e->module_ctx, f->name->line, 0, f->scope, NULL);
            LLVMSetCurrentDebugLocation2(e->builder, loc);
        } else if (f->last_dbg)
            LLVMSetCurrentDebugLocation2(e->builder, f->last_dbg);
    } 
    return mdl;
}

none emember_release(emember mem) {
    aether e = mem->mod;
    model mdl = mem->mdl;
    if (mdl->is_ref) {
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
        emember dealloc = get(mem->mdl->members, string("dealloc"));
        if (dealloc) {
            e_fn_call(e, dealloc, array_of(mem, null));
        }
        LLVMBuildFree(e->builder, ref_ptr);
        LLVMBuildBr(e->builder, no_free_block);

        // No-free block: Continue without freeing
        LLVMPositionBuilderAtEnd(e->builder, no_free_block);
    }
}

model aether_pop(aether e) {
    statements st = instanceof(e->top, typeid(statements));
    if (st) {
        pairs(st->members, i) {
            emember mem = i->value;
            release(mem);
        }
    }
    fn fn_prev = context_model(e, typeid(fn));
    if (fn_prev)
        fn_prev->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);

    pop(e->lex);
    e->top = last(e->lex);

    fn fn = context_model(e, typeid(fn));
    if (fn && fn != fn_prev) {
        LLVMPositionBuilderAtEnd(e->builder, fn->entry);
        LLVMSetCurrentDebugLocation2(e->builder, fn->last_dbg);
    }
    return e->top;
}

model model_typed(model a) {
    model mdl = a;
    while (mdl && !mdl->type) {
        // this only needs to happen if the type fails to resolve for A-type based src; which, should not really be possible
        //if (mdl->src && isa(mdl->src) == 0)
        //    break; // this is a bit wonky, should be addressed by separating the types
        mdl = mdl->src;
    }
    return mdl;
}

emember aether_initializer(aether e) {
    verify(e, "model given must be module (aether-based)");

    fn fn_init = fn(
        mod,      e,
        name,     f(string, "_%o_module_init", e->name),
        is_module_init, true,
        instance, null,
        rtype,    emodel("none"),
        members,  e->members,
        args,     eargs(mod, e));

    e->mem_init = register_model(e, fn_init);
    return e->mem_init;
}

void enumeration_init(enumeration mdl) {
    aether e = mdl->mod;
    mdl->src  = mdl->src ? mdl->src : emodel("i32");
    mdl->type = mdl->src->type;
}

// probably more suitable for aether, if we define aether as build from llvm/os
void aether_build_info(aether e, path install) {
    if (e->install != install)
        e->install = hold(install);
#ifdef _WIN32
    e->include_paths = a(
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
    e->include_paths = a(f(path, "/usr/include"), f(path, "/usr/include/x86_64-linux-gnu"));
    e->lib_paths     = a();
#elif defined(__APPLE__)
    string sdk       = run("xcrun --show-sdk-path")
    e->include_paths = a(f(path, "%o/usr/include", sdk));
    e->lib_paths     = a(f(path, "%o/usr/lib", sdk));
#endif
    push(e->include_paths, f(path, "%o/include", e->install));
    push(e->lib_paths,     f(path, "%o/lib",     e->install));
}



void aether_set_token(aether e, token t) {
    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        e->module_ctx, t->line, t->column, e->top->scope, null);
    LLVMSetCurrentDebugLocation2(e->builder, loc);
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

    *ll = form(path, "%o.ll", e->name);
    *bc = form(path, "%o.bc", e->name);

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
    e->module         = LLVMModuleCreateWithName(e->name->chars);
    e->module_ctx     = LLVMGetModuleContext(e->module);
    e->dbg_builder          = LLVMCreateDIBuilder(e->module);
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

static void register_vbasics(aether e) {
    model mdl_handle = model(mod, e, name, string("handle"), src, typeid(handle));
    model mdl_symbol = model(mod, e, name, string("symbol"), src, typeid(symbol));
    model mdl_cstr   = model(mod, e, name, string("cstr"),   src, typeid(cstr));

    register_model(e, mdl_handle);
    register_model(e, mdl_symbol);
    register_model(e, mdl_cstr);
}

static void register_basics(aether e) {
    model _i32      = emodel("i32");
    model _i16      = emodel("i16");
    model _i64      = emodel("i64");
    model _u64      = emodel("u64");
    model _handle   = emodel("handle");
    model _symbol   = emodel("symbol");

    if (!_i32) {
        // a member with 'context' would automatically bind the exact type when init'ing the object; the context flows unless its changed explicitly to something else
        // this 'just works'
        _i32    = register_model(e, model(mod, e, name, string("i32"),    src, typeid(i32)))   ->mdl;
        _i16    = register_model(e, model(mod, e, name, string("i16"),    src, typeid(i16)))   ->mdl;
        _i64    = register_model(e, model(mod, e, name, string("i64"),    src, typeid(i64)))   ->mdl;
        _u64    = register_model(e, model(mod, e, name, string("u64"),    src, typeid(u64)))   ->mdl;
        _handle = register_model(e, model(mod, e, name, string("handle"), src, typeid(handle)))->mdl;
        _symbol = register_model(e, model(mod, e, name, string("symbol"), src, typeid(symbol)))->mdl;
    }
    verify(_i32, "i32 not found");

    structure _shape = emodel("_shape");
    model      shape = emodel("shape");
    if (!shape) {
        _shape  = register_model(e, structure(mod, e, name, string("_shape")))->mdl;
        push(e, _shape);
        model _i64_16 = model(mod, e, src, _i64, count, 16);
        _shape->members = m(
            "count", emem(_i64,    "count"),
            "data",  emem(_i64_16, "data"));
        pop(e);
        shape = pointer(_shape, null);
        typeid(shape)->user = shape;
    }

  //model _ARef     = emodel("ARef");
    _symbol->is_const = true;

    structure _AType                = structure     (mod, e, name, string("_AType"), members, null);
    model     _AType_ptr            = pointer       (_AType, string("AType"));
    structure _af_recycler          = structure     (mod, e, name, string("_af_recycler"));
    model      af_recycler_ptr      = pointer       (_af_recycler, string("af_recycler"));
    emember   _af_recycler_mem      = register_model(e, _af_recycler);
    emember    af_recycler_ptr_mem  = register_model(e,  af_recycler_ptr);
    emember   _AType_mem            = register_model(e, _AType);
    emember   _AType_ptr_mem        = register_model(e, _AType_ptr);
    structure _meta_t               = structure     (mod, e, name, string("_meta_t"));
    emember   _meta_t_mem           = register_model(e, _meta_t);
    structure _member               = structure     (mod, e, name, string("_member"));
    emember   _member_mem           = register_model(e, _member);
    model      member_ptr           = pointer       (_member, string("member"));
    emember    member_ptr_mem       = register_model(e, member_ptr);

    typeid(AType)->user  = _AType_ptr;
    typeid(member)->user = member_ptr;

    push(e, _member);
    _member->members = m(
        "name",             emem(_symbol,       "name"),
        "sname",            emem(_handle,       "sname"),
        "type",             emem(_AType_ptr,    "type"),
        "offset",           emem(_i32,          "offset"),
        "count",            emem(_i32,          "count"),
        "member_type",      emem(_i32,          "member_type"),
        "operator_type",    emem(_i32,          "operator_type"),
        "required",         emem(_i32,          "required"),
        "args",             emem(_meta_t,       "args"),
        "ptr",              emem(_handle,       "ptr"),
        "method",           emem(_handle,       "method"),
        "id",               emem(_i64,          "id"),
        "value",            emem(_i64,          "value"));
    pop(e);

    push(e, _meta_t);
    _meta_t->members = m(
        "count",  emem(_i64,          "count"),
        "meta_0", emem(_AType_ptr,    "meta_0"),
        "meta_1", emem(_AType_ptr,    "meta_1"),
        "meta_2", emem(_AType_ptr,    "meta_2"),
        "meta_3", emem(_AType_ptr,    "meta_3"),
        "meta_4", emem(_AType_ptr,    "meta_4"),
        "meta_5", emem(_AType_ptr,    "meta_5"),
        "meta_6", emem(_AType_ptr,    "meta_6"),
        "meta_7", emem(_AType_ptr,    "meta_7"),
        "meta_8", emem(_AType_ptr,    "meta_8"),
        "meta_9", emem(_AType_ptr,    "meta_9"));
    pop(e);

    push(e, _af_recycler);
    _af_recycler->members = m(
        "af",       emem(_handle, "af"),
        "af_count", emem(_i64,    "af_count"),
        "af_alloc", emem(_i64,    "af_alloc"),
        "re",       emem(_handle, "re"),
        "re_count", emem(_i64,    "re_count"),
        "re_alloc", emem(_i64,    "re_alloc"));
    pop(e);
    
    model _u64_2 = model(mod, e, src, _u64, count, 2);
    push(e, _AType);
    _AType->members = m(
        "parent_type",     emem(_AType_ptr, "parent_type"),
        "name",            emem(_symbol,    "name"),
        "module",          emem(_symbol,    "module"),
        "sub_types",       emem(_handle,    "sub_types"), // needs to be _ATypeRef
        "sub_types_count", emem(_i16,       "sub_types_count"),
        "sub_types_alloc", emem(_i16,       "sub_types_alloc"),
        "size",            emem(_i32,       "size"),
        "isize",           emem(_i32,       "isize"),
        "af",              emem(af_recycler_ptr, "af"),
        "magic",           emem(_i32,       "magic"),
        "global_count",    emem(_i32,       "global_count"),
        "vmember_count",   emem(_i32,       "vmember_count"),
        "vmember_type",    emem(_AType_ptr, "vmember_type"),
        "member_count",    emem(_i32,       "member_count"),
        "members",         emem(member_ptr, "members"),
        "traits",          emem(_i32,       "traits"),
        "user",            emem(_handle,    "user"),
        "required",        emem(_u64_2,     "required"),
        "src",             emem(_AType_ptr, "src"),
        "arb",             emem(_handle,    "arb"),
        "shape",           emem(shape,      "shape"),
        "meta",            emem(_meta_t,    "meta"));
    pop(e);

    finalize(_AType);
    finalize(_meta_t);
    finalize(_af_recycler);
    finalize(_member);

    verify(_AType->size_bits       == sizeof(struct _AType)       * 8, "AType size mismatch");
    verify(_af_recycler->size_bits == sizeof(struct _af_recycler) * 8, "_af_recycler size mismatch");
    verify(_meta_t->size_bits      == sizeof(struct _meta_t)      * 8, "_meta_t size mismatch");
    verify(_member->size_bits      == sizeof(struct _member)      * 8, "_member size mismatch");
}

model translate_rtype(model mdl) {
    if (isa(mdl) == typeid(Class))
        return mdl->is_ref ? mdl : pointer(mdl, null);
    return mdl;
}

model translate_target(model mdl) {
    if (isa(mdl) == typeid(Class))
        return mdl->is_ref ? mdl : pointer(mdl, null);
    return mdl;
}

void aether_A_import(aether e, path lib) {
    e->current_include = lib ? lib : path("A");
    e->is_A_import = true;

    map module_filter = m("A", A_bool(true));
    if (lib) set(module_filter, stem(lib), A_bool(true));

    handle f = lib ? dlopen(cstring(lib), RTLD_NOW) : null;
    verify(!lib || f, "shared-lib failed to load: %o", lib);

    path inc = lib ? lib : path_self();
    // push libraries for reloading facility
    // todo: associate all loaded elements with this, so we can effectively release resources
    // A-type has no unregistration of classes, but its a trivial mechanism
    if (f) {
        push(e->shared_libs, f);
        A_engage(null); // load A-types by finishing global constructor ordered calls
    }

    i64        ln;
    AType*     a         = A_types(&ln);
    structure _AType     = emodel("_AType");
    map       processing = map(hsize, 64, assorted, true, unmanaged, true);

    // we may create classes/structures with no members ahead of time
    // any remaining ones after an import process would fault
    // must have members to finalize, of course
    // using this method we do not need recursion
    if (!_AType) register_vbasics(e);

    string mstate      = null;
    symbol last_module = null;
    
    for (num i = 0; i < ln; i++) {
        AType  atype = a[i];
        model  mdl   = null;
        if (atype == typeid(member)) 
            continue;

        if (atype == typeid(A)) {
            int test2 = 2;
            test2    += 2;
        }

        // only import the module the user wants
        if (last_module != atype->module) {
            last_module  = atype->module;
            mstate       = string(last_module);
        }
        if (!get(module_filter, mstate))
            continue;

        if (atype == typeid(shape)) continue;

        bool  is_abstract  = (atype->traits & A_TRAIT_ABSTRACT)  != 0;
        if (atype->user || (is_abstract && (atype != typeid(raw)))) continue; // if we have already processed this type, continue
        string name = string(atype->name);
        set(processing, name, atype);

        bool  is_base_type = (atype->traits & A_TRAIT_BASE)      != 0;
        if   (is_base_type) continue; // this is AType, which we setup manually

        bool  is_prim      = (atype->traits & A_TRAIT_PRIMITIVE) != 0;
        bool  is_ref       = (atype->traits & A_TRAIT_POINTER)   != 0;
        if   (is_ref) continue; // src of this may be referencing another unresolved


        bool  is_struct    = (atype->traits & A_TRAIT_STRUCT)    != 0;
        bool  is_class     = (atype->traits & A_TRAIT_CLASS)     != 0;
        bool  is_enum      = (atype->traits & A_TRAIT_ENUM)      != 0;
        bool  is_realistic = (atype->traits & A_TRAIT_REALISTIC) != 0;
        bool  is_integral  = (atype->traits & A_TRAIT_INTEGRAL)  != 0;
        bool  is_unsigned  = (atype->traits & A_TRAIT_UNSIGNED)  != 0;
        bool  is_signed    = (atype->traits & A_TRAIT_SIGNED)    != 0;

        if      (is_class)    mdl = Class      (mod, e, name, name, imported_from, inc);
        else if (is_struct)   mdl = structure  (mod, e, name, name, imported_from, inc);
        else if (is_enum)     mdl = enumeration(mod, e, name, name);
        else if (is_prim || is_abstract) {
            mdl = model(mod, e, name, name, src, atype);
        }

        Class cl = mdl;
        // initialization for primitives are in model_init
        atype->user = mdl; // lets store our model reference here
        verify(mdl, "failed to import type: %o", name);
        register_model(e, mdl);
    }

    // first time we run this, we must import AType basics (after we import the basic primitives)
    if (!_AType) register_basics(e);

    // resolve parent classes, load refs, fill out enumerations
    pairs(processing, i) {
        AType  atype    = i->value;
        model  mdl      = atype->user;
        bool   is_class = (atype->traits & A_TRAIT_CLASS)   != 0;
        if    (is_class) {
            if (atype != typeid(A)) {
                model m = emodel(atype->parent_type->name);
                while (m && m->is_ref)
                    m = m->src;
                verify(isa(m) == typeid(Class), "expected parent class");
                ((Class)mdl)->parent = m;
            }
            continue;
        }

        bool   is_enum  = (atype->traits & A_TRAIT_ENUM)    != 0;
        if    (is_enum) {
            enumeration en = mdl;
            en->members = map(hsize, 16);
            en->src     = atype->src->user;
            verify(en->src, "expected enumeration source type");

            for (int m = 0; m < atype->member_count; m++) {
                member amem = &atype->members[m];
                if (!(amem->member_type & A_FLAG_ENUMV)) continue;
                verify(atype->src == amem->type,
                    "enum value type not the same as defined on type");
                A e_const = primitive(amem->type, amem->ptr);
                emember e_value = emember(
                    mod, e, name, amem->sname, mdl, en->src);
                set_value(e_value, e_const); // should set appropriate constant state
                verify(e_value->is_const, "expected constant set after");
                set(en->members, amem->sname, e_value);
            }
        }

        bool   is_ref   = (atype->traits & A_TRAIT_POINTER) != 0;
        if   (!is_ref) continue;

        AType pointer_to = atype->meta.meta_0;
        verify(!mdl, "unexpected user data set for %s", atype->name);
        string name = string(atype->name);
        verify(pointer_to, "expected src to be set for %o", name);
        mdl = emodel(pointer_to->name);
        mdl = pointer(mdl, name); // we have to have these ref_i32's and such from A-type
        register_model(e, mdl);
        verify(!atype->user, "expected null user for type %s", atype->name);
        atype->user = mdl;
    }

    // load structs and class members (including i/s methods)
    pairs(processing, i) {
        AType  atype     = i->value;
        model  mdl       = atype->user;
        bool   is_struct = (atype->traits & A_TRAIT_STRUCT) != 0;
        bool   is_class  = (atype->traits & A_TRAIT_CLASS)   != 0;
        if    (is_class || is_struct) {
            structure st = is_struct ? mdl : null;
            Class     cl = is_class  ? mdl : null;

            if (!mdl->members) mdl->members = hold(map(hsize, 8));

            if (atype == typeid(A)) {
                model mdl_A_ptr = typeid(A)->user = pointer(cl, null); // may need to be opaque first? (or we can set this to handle too)
                model mdl_ARef = pointer(mdl_A_ptr, string("ARef"));
                register_model(e, mdl_ARef);
            }

            // add members
            for (int m = 0; m < atype->member_count; m++) {
                member amem    = &atype->members[m];
                AType  mtype   = amem->type;
                string n       = amem->sname;
                model  mem_mdl = null;

                verify(!mtype || mtype->user, "expected type resolution for member %o", n);

                if ((amem->member_type & A_FLAG_CONSTRUCT) != 0) {
                    verify(mtype, "type cannot be void for a constructor");
                    member arg = earg((model)mtype->user, "");
                    mem_mdl = fn(
                        mod,           e,
                        is_ctr,        true,
                        name,          n,
                        extern_name,   f(string, "%s_%o", atype->name, n),
                        rtype,         translate_rtype(st ? st : null), // structure ctr return data (A-type ABI)
                        instance,      cl, // structs do not have a 'target' argument in constructor -- silver operates the same
                        args,          eargs(mod, e, members, m("", arg)));
                }
                else if ((amem->member_type & A_FLAG_CAST) != 0) {
                    verify(mtype, "cast cannot be void");
                    mem_mdl = fn(
                        mod,           e,
                        is_cast,       true,
                        name,          n,
                        extern_name,   f(string, "%s_%o", atype->name, n),
                        rtype,         translate_rtype(mtype->user),
                        instance,      mdl);
                }
                else if ((amem->member_type & A_FLAG_INDEX) != 0) {
                    verify(mtype, "index cannot return void");
                    model index_mdl = amem->args.meta_0->user;
                    verify(index_mdl, "index requires argument");
                    member arg = earg(index_mdl, "");
                    mem_mdl    = fn(
                        mod,           e,
                        name,          n,
                        extern_name,   f(string, "%s_%o", atype->name, n),
                        is_cast,       true,
                        rtype,         translate_rtype(mtype->user),
                        instance,      mdl,
                        args,          eargs(mod, e, members, m("", arg)));
                }
                else if ((amem->member_type & A_FLAG_OPERATOR) != 0) {
                    model  arg_mdl = amem->args.meta_0->user;
                    member arg     = earg(arg_mdl, "");
                    mem_mdl        = fn(
                        mod,           e,
                        name,          n,
                        extern_name,   f(string, "%s_%o", atype->name, n),
                        is_oper,       amem->operator_type,
                        rtype,         translate_rtype(mtype->user),
                        instance,      mdl,
                        args,          arg_mdl ?
                            eargs(mod, e, members,
                                m("", arg)) : 0);
                }
                else if ((amem->member_type & A_FLAG_IMETHOD) != 0 ||
                         (amem->member_type & A_FLAG_SMETHOD) != 0 ||
                         (amem->member_type & A_FLAG_IFINAL)  != 0) {
                    bool  is_inst = (amem->member_type & A_FLAG_IMETHOD) != 0;
                    bool  is_f    = (amem->member_type & A_FLAG_IFINAL)  != 0;
                    eargs args    = eargs(mod, e);
                    for (int a = 0; a < amem->args.count; a++) {
                        AType  arg_type = ((AType*)(&amem->args.meta_0))[a];
                        model  arg_mdl  = arg_type->user;
                        if ((arg_type->traits & A_TRAIT_CLASS) != 0)
                            arg_mdl = pointer(arg_mdl, null);
                        member arg      = earg(arg_mdl, arg_mdl->name->chars);
                        set(args->members, arg_mdl->name, arg);
                    }
                    mem_mdl = fn(
                        mod,           e,
                        name,          n,
                        extern_name,   f(string, "%s_%o", atype->name, n),
                        is_override,   (amem->member_type & A_FLAG_OVERRIDE) != 0,
                        rtype,         translate_rtype(mtype->user),
                        instance,      (is_inst || is_f) ? mdl : null,
                        args,          args);
                }
                else if ((amem->member_type & A_FLAG_PROP) ||
                         (amem->member_type & A_FLAG_VPROP)) {
                    bool is_cl = isa(mtype->user) == typeid(Class);
                    mem_mdl = is_cl ?
                        pointer((model)mtype->user, null) : mtype->user;
                    if (amem->member_type & A_FLAG_VPROP) {
                        mem_mdl = pointer(mem_mdl, null);
                    }
                }
                verify(mem_mdl, "expected mdl for member %s:%o", atype->name, n);
                emember smem = emember(
                    mod, e, name, n, 
                    mdl, amem->count == 0 ? 
                        mem_mdl : model(mod, e, src, mem_mdl, count, amem->count));
                
                set(mdl->members, n, smem);
            }

            // add padding based on isize (aligned with runtime; this way we may allocate space for it to use its special things)
            if ((is_class || is_struct) && atype->isize > 0) {
                string n = f(string, "%s_interns", atype->name);
                model intern_space = model(
                    mod,    e,
                    name,   n,
                    src,    emodel("u8"),
                    count,  atype->isize);
                emember imem = emember(mod, e, name, n, mdl, intern_space);
                set(mdl->members, n, imem);
            }
        }
    }
    // finalize models
    pairs(processing, i) {
        AType  atype  = i->value;
        model   mdl   = atype->user;
        verify(mdl, "could not import %s", atype->name);
        emember emem  = lookup2(e, mdl->name, null);
        verify(emem->mdl == mdl || emem->mdl == mdl->ptr, "import integrity error for %o", mdl->name);
        finalize(mdl);
        if (isa(mdl) == typeid(fn))
            fn_use(mdl);
    }
    e->is_A_import     = true;
    e->current_include = null;

    // we do not need the schema until we build the global constructor
    update_schemas(e);
}


void aether_init(aether e) {
    e->with_debug = true;
    e->is_system  = true;
    e->finalizing = hold(array(alloc, 64, assorted, true, unmanaged, true));
    e->instances  = map(hsize, 8);

    if (!e->name) {
        verify(e->source && len(e->source), "module name required");
        e->name = stem(e->source);
    }
    aether_llvm_init(e);
    e->members = map(hsize, 32);
    e->shared_libs = array(alloc, 32, unmanaged, true);
    e->lex = array(alloc, 32, assorted, true);
    push(e, e);

    // aether_define_primitive(e);
    A_import(e, null);

    // register 'main' class as an alias to A
    // we use this as an means of signaling app entry (otherwise we make a lib)
    // we cant make the type in A-type because the symbol clashes; we need not export this symbol
    model main = Class(mod, e, src, emodel("A")->src, name, string("main"), imported_from, path("A"), is_abstract, true);
    Class main_cl = main;
    register_model(e, main);
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
/// here we are acting a bit more like python with A-type
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
    
    /// primitives may be converted to A-type A
    if (is_primitive(fr) && is_generic(to))
        return (emember)true;

    /// check constructors on to
    /// single-arg construction model, same as as A-type
    pairs (to->members, i) {
        emember mem = i->value;
        fn f = instanceof(mem->mdl, typeid(fn));
        if (!f || !(f->function_type & A_FLAG_CONSTRUCT))
            continue;
        emember first = null;
        pairs (f->args->members, i) {
            emember arg = i->value;
            first = arg;
            break;
        }
        verify(first, "invalid constructor");
        if (model_convertible(fr, first->mdl))
            return (emember)true;
    }

    /// check cast methods on from
    pairs (fr->members, i) {
        emember mem = i->value;
        fn f = instanceof(mem->mdl, typeid(fn));
        if (!f || !(f->function_type & A_FLAG_CAST))
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
        fn fn = instanceof(mem->mdl, typeid(fn));
        if (!fn || !(fn->function_type & A_FLAG_CONSTRUCT))
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

emember model_convertible(model fr, model to) {
    if (is_primitive(fr) && is_primitive(to))
        return (emember)true;
    if (fr == to)
        return (emember)true;
    emember mcast = castable(fr, to);
    return mcast ? mcast : constructable(fr, to);
}

typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;



// create AType structure (info and its AType_f)
void create_schema(model mdl) {
    aether e = mdl->mod;
    fn  f = instanceof(mdl, typeid(fn));
    if (f || mdl->is_system || mdl == e) return;

    record      rec = instanceof(mdl, typeid(record));
    enumeration en  = instanceof(mdl, typeid(enumeration));

    if (!rec && !en) return;

    finalize(mdl);

    // does not work for enumerations 
    Class  cmdl = instanceof(mdl, typeid(Class));
    Class  emdl = instanceof(mdl, typeid(enumeration));
    
    if (cmdl && cmdl->is_abstract)
        return;
    if (mdl->has_schema)
        return;
    
    mdl->has_schema = true;
    
    // focus on emitting class info similar to this macro:
    string    type_name = f(string, "_%o_f", mdl->name);
    structure mdl_type  = structure(mod, e, name, type_name, members, map(hsize, 8), is_system, true);
    emember   type_mem  = register_model(e, mdl_type);
    push(e, type_mem->mdl);

    model _AType = emodel("_AType");
    model _A     = emodel("A")->src;

    pairs(_AType->members, ai) {
        emember m  = ai->value;
        print("AType member: %o", m->name);
    }

    // register the basic AType fields
    pairs(_AType->members, ai) {
        emember m  = ai->value;
        emember mm = emember(mod, e, name, m->name, mdl, m->mdl);
        register_member(e, mm);
    }

    Class  cur = cmdl;
    array  classes = array(alloc, 32, assorted, true);
    if (emdl) {
        push(classes, emdl);
        push(classes, emodel("A")->src);
    } else {
        while (cur) {
            push(classes, cur);
            cur = cur->parent;
            if (cur && cur->is_abstract)
                cur = cur->src;
            verify(!cur || !cur->is_ref, "unexpected pointer");
        }
        classes = reverse(classes);
    }

    // register f table
    each (classes, Class, cl) {
        if (!cl->members) continue;
        pairs(cl->members, a) {
            emember m = a->value;
            if (isa(m->mdl) != typeid(fn))
                continue;
            fn f = m->mdl;
            model f_ptr = model(mod, e, name, m->mdl->name, src, m->mdl, is_ref, true);
            emember mm = register_model(e, f_ptr);
        }
    }
    pop(e);
    finalize(mdl_type);

    // register type ## _info struct with A info header and f table (AType)
    structure _type_info = structure(mod, e, name, f(string, "_%o_info", mdl->name), members, map(), is_system, true);
    register_model(e, _type_info);
    push(e, _type_info);
        emember info = emember(mod, e, name, string("info"), mdl, _A);
        emember type = emember(mod, e, name, string("type"), mdl, mdl_type);
        register_member(e, info);
        register_member(e, type);
    pop(e);
    finalize(_type_info);
    model type_info = model(mod, e, name, f(string, "%o_info", mdl->name), src, _type_info);
    register_model(e, type_info);

    // register our global type info
    if (eq(mdl->name, "string")) { // we expect this to load the string _i table
        int test2 = 2;
        test2    += 2;
    }

    emember type_i = emember(mod, e, name, f(string, "%o_i", mdl->name), mdl, _type_info);
    register_member(e, type_i);
    //finalize(type_i);

    model   type_alias = model(mod, e, name, f(string, "%o_f", mdl->name), src, mdl_type);
    register_model(e, type_alias);
}

void aether_update_schemas(aether e) {
    pairs (e->members, i) {
        emember m = i->value;
        model  mdl = m->mdl;
        create_schema(mdl);
    }
}

void aether_push_state(aether a, array tokens, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    push(a->stack, state);
    tokens_data* state_saved = (tokens_data*)last(a->stack);
    a->tokens = hold(tokens);
    a->cursor = cursor;
}


void aether_pop_state(aether a, bool transfer) {
    int len = a->stack->len;
    assert (len, "expected stack");
    tokens_data* state = (tokens_data*)last(a->stack); // we should call this element or ele
    
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
        fn fn = instanceof(mem->mdl, typeid(fn));
        /// must be function with optional name check
        if (!fn || (((f & fn->function_type) == f) && (n && !eq(n, fn->name->chars))))
            continue;
        /// arg count check
        if (len(fn->args->members) != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        pairs(fn->args->members, ii) {
            emember to_arg = ii->value;
            enode fr_arg = args->elements[ai];
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
        return value(i64, negated_value);
    }
    else {
        fault("A negation not valid");
    }
}

enode aether_e_not(aether e, enode L) {
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
    return value(emodel("bool"), result);
}

enode aether_e_bitwise_not(aether e, enode L) {
    return LLVMBuildNot(e->builder, L->value, "bitwise-not");
}

// A o = obj("type-name", props)
enode aether_e_eq(aether e, enode L, enode R);

enode aether_e_is(aether e, enode L, A R) {
    enode L_type =  e_offset(e, L, A_i64(-sizeof(struct _A)));
    enode L_ptr  =    e_load(e, L_type, null); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = e_operand(e, R, null);
    return aether_e_eq(e, L_ptr, R_ptr);
}

enode aether_e_cmp(aether e, enode L, enode R) {
    model t0 = L->mdl;
    model t1 = R->mdl;
    verify (t0 == t1, "types must be same at primitive operation level");
    bool i0 = is_integral(t0);
    bool f0 = is_realistic(t1);
    LLVMValueRef result;
    if (i0 || !f0) {
        // For integers, build two comparisons and combine
        LLVMValueRef lt = LLVMBuildICmp(e->builder, LLVMIntSLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildICmp(e->builder, LLVMIntSGT, L->value, R->value, "cmp_gt");
        // Convert true/false to -1/+1 and combine
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(), 1, false);
        LLVMValueRef lt_val = LLVMBuildSelect(e->builder, lt, neg_one, LLVMConstInt(LLVMInt32Type(), 0, false), "lt_val");
        LLVMValueRef gt_val = LLVMBuildSelect(e->builder, gt, pos_one, lt_val, "cmp_result");
        result = gt_val;
    } else {
        // For floats, use ordered comparison
        LLVMValueRef lt = LLVMBuildFCmp(e->builder, LLVMRealOLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildFCmp(e->builder, LLVMRealOGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(), 1, false);
        LLVMValueRef lt_val = LLVMBuildSelect(e->builder, lt, neg_one, LLVMConstInt(LLVMInt32Type(), 0, false), "lt_val");
        LLVMValueRef gt_val = LLVMBuildSelect(e->builder, gt, pos_one, lt_val, "cmp_result");
        result = gt_val;
    }
    
    return value(emodel("i32"), result);
}

enode aether_e_eq(aether e, enode L, enode R) {
    model t0 = L->mdl;
    model t1 = R->mdl;
    verify (t0 == t1, "types must be same at primitive operation level");
    bool i0 = is_integral(t0);
    bool f0 = is_realistic(t1);
    enode r;
    A literal = null;
    if (i0 || !f0) {
        AType Lt = isa(L->literal);
        AType Rt = isa(R->literal);
        if (Lt && Lt == Rt) { /// useful for ifdef functionality
            bool is_eq = memcmp(L->literal, R->literal, Lt->size) == 0;
            literal = A_bool(is_eq);
            r = value(emodel("bool"), LLVMConstInt(LLVMInt1Type(), is_eq, 0) );
        } else
            r = value(emodel("bool"), LLVMBuildICmp(e->builder, LLVMIntEQ,   L->value, R->value, "eq-i"));
    } else
        r = value(emodel("bool"), LLVMBuildFCmp(e->builder, LLVMRealOEQ, L->value, R->value, "eq-f"));
    r->literal = hold(literal);
    return r;
}

enode aether_e_not_eq(aether e, enode L, enode R) {
    model t0 = L->mdl;
    model t1 = R->mdl;
    verify (t0 == t1, "types must be same at primitive operation level");
    bool i0 = is_integral(t0);
    bool f0 = is_realistic(t1);
    if (i0 || !f0)
        return value(emodel("bool"), LLVMBuildICmp(e->builder, LLVMIntNE,   L->value, R->value, "not-eq-i"));
    return     value(emodel("bool"), LLVMBuildFCmp(e->builder, LLVMRealONE, L->value, R->value, "not-eq-f"));
}

enode aether_e_fn_return(aether e, A o) {
    fn f = context_model(e, typeid(fn));
    verify (f, "function not found in context");

    if (!o) return value(f->rtype, LLVMBuildRetVoid(e->builder));

    enode conv = e_convert(e, e_operand(e, o, null), f->rtype);
    return value(f->rtype, LLVMBuildRet(e->builder, conv->value));
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

enode aether_e_fn_call(aether e, emember fn_mem, array args) { // we could support an array or map arg here, for args
    verify(isa(fn_mem->mdl) == typeid(fn), "model provided is not function");
    fn fn = fn_mem->mdl;
    use(fn); // target set in here based on instance

    int n_args = args ? len(args) : 0;
    LLVMValueRef* arg_values = calloc((fn_mem->target_member != null) + n_args, sizeof(LLVMValueRef));
    LLVMTypeRef  F = fn->type;
    LLVMValueRef V = fn->value; // todo: args in aether should be a map.  that way we can do a bit more

    int index = 0;
    verify(!!fn_mem->target_member == !!fn->target, "target mismatch");
    //emember target = fn_mem->target_member;
    if (fn->target)
        arg_values[index++] = fn_mem->target_member->value;

    /// so know if we have a literal string or not
    /// if we dont, then how on earth are we supposed to know what to do with args lol
    /// i think we dont allow it..
    int fmt_idx  = fn->format ? fn->format->format_index - 1 : -1;
    int arg_idx  = fn->format ? fn->format->arg_index    - 1 : -1;
    if (args)
        each (args, A, value) {
            if (index == arg_idx) break;
            if (index == fmt_idx) {
                index++;
                continue;
            }

            emember    f_arg = value_by_index(fn->args->members, index);
            AType      vtype = isa(value);
            enode      op    = e_operand(e, value, null);
            enode      conv  = e_convert(e, op, f_arg ? f_arg->mdl : op->mdl);
            
            LLVMValueRef vr = arg_values[index] = conv->value;
            index++;
        }
    int istart = index;

    if (fn->format) {
        AType a0 = isa(args->elements[fmt_idx]);
        enode   fmt_node = instanceof(args->elements[fmt_idx], typeid(enode));
        string fmt_str  = instanceof(fmt_node->literal, typeid(string));
        verify(fmt_str, "expected string literal at index %i for format-function: %o", fmt_idx, fn->name);
        arg_values[fmt_idx] = fmt_node->value;
        // process format specifiers and map to our types
        int soft_args = 0;
        symbol p = fmt_str->chars;
        // we will have to convert %o to %s, unless we can be smart with %s lol.
        // i just dont like how its different from A-type
        // may simply be fine to use %s for string, though.  its natural to others
        // alternately we may copy the string from %o to %s.
        while  (p[0]) {
            if (p[0] == '%' && p[1] != '%') {
                model arg_type  = formatter_type(e, (cstr)&p[1]);
                A     o_arg     = args->elements[arg_idx + soft_args];
                AType arg_type2 = isa(o_arg);
                enode n_arg     = e_load(e, o_arg, null);
                enode arg       = e_operand(e, n_arg, null);
                enode conv      = e_convert(e, arg, arg_type); 
                arg_values[arg_idx + soft_args] = conv->value;
                soft_args++;
                index    ++;
                p += 2;
            } else
                p++;

        }
        verify((istart + soft_args) == len(args), "%o: formatter args mismatch", fn->name);
    }
    
    bool is_void_ = is_void(fn->rtype);
    LLVMValueRef R = LLVMBuildCall2(e->builder, F, V, arg_values, index, is_void_ ? "" : "call");
    free(arg_values);
    return value(fn->rtype, R);
}

/// aether is language agnostic so the user must pass the overload method
enode aether_e_op(aether e, OPType optype, string op_name, A L, A R) {
    emember mL = instanceof(L, typeid(enode)); 
    enode   LV = e_operand(e, L, null);
    enode   RV = e_operand(e, R, null);

    /// check for overload
    if (op_name && isa(op_name) == typeid(enode) && is_class(((enode)L)->mdl)) {
        enode Ln = L;
        AType type = isa(Ln->mdl);
        if (type == typeid(structure) || type == typeid(record)) {
            record rec = Ln->mdl;
            emember Lt = get(rec->members, op_name);
            if  (Lt && isa(Lt->mdl) == typeid(fn)) {
                fn fn = Lt->mdl;
                verify(len(fn->args->members) == 1, "expected 1 argument for operator method");
                /// convert argument and call method
                model  arg_expects = value_by_index(fn->args->members, 0);
                enode  conv = e_convert(e, Ln, arg_expects);
                array args = array_of(conv, null);
                verify(mL, "mL == null");
                emember fmem = emember(mod, e, mdl, Lt->mdl, name, Lt->name, target_member, mL);
                return e_fn_call(e, fmem, args); // todo: fix me: Lt must have target_member associated, so allocate a emember() for this
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    model rtype = determine_rtype(e, optype, LV->mdl, RV->mdl); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too
    LLVMValueRef RES;
    LLVMTypeRef  LV_type = LLVMTypeOf(LV->value);
    LLVMTypeKind vkind = LLVMGetTypeKind(LV_type);

    enode LL = optype == OPType__assign ? LV : e_convert(e, LV, rtype); // we dont need the 'load' in here, or convert even
    enode RL = e_convert(e, RV, rtype);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = e->builder;
    A literal = null;
    /// if are not storing ...
    if (optype >= OPType__add && optype <= OPType__left) {
        struct op_entry* op = &op_table[optype - OPType__add];
        RES = op->f_op(B, LL->value, RL->value, N);
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
        /// todo: build all op tables in A-type (we are lacking these definitions)
            RES = op_table[optype - OPType__assign_add].f_op(B, LL->value, RL->value, N);
        LLVMBuildStore(B, RES, mL->value);
    }
    return enode(
        mod,        e,
        mdl,        rtype,
        literal,    literal,
        value,      RES);
}

enode aether_e_or (aether e, A L, A R) { return e_op(e, OPType__or,  string("or"),  L, R); }
enode aether_e_xor(aether e, A L, A R) { return e_op(e, OPType__xor, string("xor"), L, R); }
enode aether_e_and(aether e, A L, A R) { return e_op(e, OPType__and, string("and"), L, R); }
enode aether_e_add(aether e, A L, A R) { return e_op(e, OPType__add, string("add"), L, R); }
enode aether_e_sub(aether e, A L, A R) { return e_op(e, OPType__sub, string("sub"), L, R); }
enode aether_e_mul(aether e, A L, A R) { return e_op(e, OPType__mul, string("mul"), L, R); }
enode aether_e_div(aether e, A L, A R) { return e_op(e, OPType__div, string("div"), L, R); }
enode aether_value_default(aether e, A L, A R) { return e_op(e, OPType__value_default, string("value_default"), L, R); }
enode aether_cond_value   (aether e, A L, A R) { return e_op(e, OPType__cond_value,    string("cond_value"), L, R); }

enode aether_e_inherits(aether e, enode L, A R) {
    // Get the type pointer for L
    enode L_type =  e_offset(e, L, A_i64(-sizeof(A)));
    enode L_ptr  =    e_load(e, L, null);
    enode R_ptr  = e_operand(e, R, null);

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
    enode cmp       = e_eq(e, value(L_ptr->mdl, phi), R_ptr);

    // Load the parent pointer (assuming it's the first emember of the type struct)
    enode parent    = e_load(e, value(L_ptr->mdl, phi), null);

    // Check if parent is null
    enode is_null   = e_eq(e, parent, value(parent->mdl, LLVMConstNull(typed(parent->mdl)->type)));

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

    return value(emodel("bool"), result);
}


void finalize(model mdl) {
    aether e = mdl->mod;
    if (mdl->finalized) return;
    mdl = model_source(mdl);
    if (mdl->finalized) return;
    if (index_of(e->finalizing, mdl) >= 0)
        return;
    push(e->finalizing, mdl);
    i32 index = len(e->finalizing) - 1;
    record      rec = instanceof(mdl, typeid(record));
    fn          f   = instanceof(mdl, typeid(fn));
    enumeration en  = instanceof(mdl, typeid(enumeration));
    if (rec)     record_finalize(mdl);
    else if (f)  fn_finalize(mdl);
    else if (en) enumeration_finalize(mdl);

    verify(index_of(e->finalizing, mdl) == index, "weird?");
    remove(e->finalizing, index);
}

model aether_top(aether e) {
    return e->top;
}

A read_numeric(token a) {
    cstr cs = cstring(a);
    if (strcmp(cs, "true") == 0) {
        bool v = true;
        return primitive(typeid(bool), &v);
    }
    if (strcmp(cs, "false") == 0) {
        bool v = false;
        return primitive(typeid(bool), &v);
    }
    bool is_digit = cs[0] >= '0' && cs[0] <= '9';
    bool has_dot  = strstr(cs, ".") != 0;
    if (!is_digit && !has_dot)
        return null;
    char* e = null;
    if (!has_dot) {
        i64 v = strtoll(cs, &e, 10);
        return primitive(typeid(i64), &v);
    }
    f64 v = strtod(cs, &e);
    return primitive(typeid(f64), &v);
}

string token_location(token a) {
    string f = form(string, "%o:%i:%i", a->source, a->line, a->column);
    return f;
}

AType token_get_type(token a) {
    return a->literal ? isa(a->literal) : null;
}

AType token_is_bool(token a) {
    string t = cast(string, a);
    return (cmp(t, "true") || cmp(t, "false")) ?
        (AType)typeid(bool) : null;
}

string read_string(cstr cs) {
    int ln = strlen(cs);
    string res = string(alloc, ln);
    char*   cur = cs;
    while (*cur) {
        int inc = 1;
        if (cur[0] == '\\') {
            symbol app = null;
            switch (cur[1]) {
                case 'n':  app = "\n"; break;
                case 't':  app = "\t"; break;
                case 'r':  app = "\r"; break;
                case '\\': app = "\\"; break;
                case '\'': app = "\'"; break;
                case '\"': app = "\""; break;
                case '\f': app = "\f"; break;
                case '\v': app = "\v"; break;
                case '\a': app = "\a"; break;
                default:   app = "?";  break;
            }
            inc = 2;
            append(res, (cstr)app);
        } else {
            char app[2] = { cur[0], 0 };
            append(res, (cstr)app);
        }
        cur += inc;
    }
    return res;
}

token token_with_cstr(token a, cstr s) {
    a->chars = s;
    a->len   = strlen(s);
    return a;
}

token token_copy(token a) {
    return token(chars, a->chars, line, a->line, column, a->column);
}

void token_init(token a) {
    cstr prev = a->chars;
    sz length = a->len ? a->len : strlen(prev);
    a->chars  = (cstr)calloc(length + 1, 1);
    a->len    = length;
    memcpy(a->chars, prev, length);

    if (a->chars[0] == '\"' || a->chars[0] == '\'') {
        string crop = string(chars, &a->chars[1], ref_length, length - 2);
        a->literal = read_string((cstr)crop->chars);
    } else
        a->literal = read_numeric(a);
}

ident ident_with_model(ident a, model mdl) {
    a->mdl = mdl;
    return a;
}

void eargs_init (eargs a) {
    if (!a->members)
         a->members = map(hsize, 4);
}

define_enum  (interface)
define_enum  (comparison)

array macro_expand(macro m, array args_tokens);

define_class (model,        A)
define_class (format_attr,  A)
define_class (ident,        A) // useful class to represent ident for operations, and not conversion to a model; this disambiguates the two use-cases
define_class (clang_cc,     A)

define_class (macro,        model)
define_class (statements,   model)
define_class (eargs,        model)
define_class (fn,           model)
define_class (record,       model)
define_class (aether,       model)
define_class (uni,          record)
define_class (enumeration,  model)
define_class (structure,    record)
define_class (Class,        record)
define_class (code,         A)
define_class (token,        string)
define_class (enode,        A)
define_class (emember,      enode)