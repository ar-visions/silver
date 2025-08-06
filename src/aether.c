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

#define emem(M, N) emember(mod, e, name, string(N), mdl, M);

#define value(m,vr) enode(mod, e, value, vr, mdl, m)

emember aether_register_model(aether e, model mdl) {
    bool is_func = instanceof(mdl, typeid(function)) != null;
    emember mem   = emember(
        mod, e, mdl, is_class(mdl) ? pointer(mdl, null) : mdl, name, mdl->name,
        is_func,  is_func,
        is_type, !is_func);
    register_member(e, mem);
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
    return top->members;
}

void aether_register_member(aether e, emember mem) {
    if (!mem || mem->registered)
        return;
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
        if (f->ref == reference_pointer)
            return f;
        f = f->src;
    }
    return f;
}
bool is_bool     (model f) { f = model_resolve(f); return f->src && isa(f->src) == typeid(bool); }
bool is_float    (model f) { f = model_resolve(f); return f->src && isa(f->src) == typeid(f32);  }
bool is_double   (model f) { f = model_resolve(f); return f->src && isa(f->src) == typeid(f64);  }
bool is_realistic(model f) { f = model_resolve(f); return f->src && isa(f->src)->traits & A_TRAIT_REALISTIC; }
bool is_integral (model f) { f = model_resolve(f); return f->src && isa(f->src)->traits & A_TRAIT_INTEGRAL;  }
bool is_signed   (model f) { f = model_resolve(f); return f->src && isa(f->src)->traits & A_TRAIT_SIGNED;    }
bool is_unsigned (model f) { f = model_resolve(f); return f->src && isa(f->src)->traits & A_TRAIT_UNSIGNED;  }
bool is_primitive(model f) {
    f = model_resolve(f); 
    return f->src && isa(f->src)->traits & A_TRAIT_PRIMITIVE;
}

bool is_void     (model f) {
    f = model_resolve(f); 
    return f ? f->size == 0 : false;
}

bool is_generic  (model f) {
    f = model_resolve(f);
    return (AType)f->src == typeid(A);
}

bool is_record   (model f) {
    f = model_resolve(f); 
    return isa(f) == typeid(structure) || 
           isa(f) == typeid(class);
}

bool is_class    (model f) {
    f = model_resolve(f); 
    return isa(f) == typeid(class);
}

bool is_struct   (model f) {
    f = model_resolve(f); 
    return isa(f) == typeid(structure);
}

bool is_ref      (model f) {
    f = model_resolve(f); 
    if (f->ref != reference_value)
        return true;
    model src = f->src;
    while (instanceof(src, typeid(model))) {
        src = src->src;
        if (f->ref != reference_value)
            return true;
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
        if (mdl->ref != reference_value)
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
    return mdl->type == b->type ? 0 : -1;
}

void model_init(model mdl) {
    aether  e = mdl->mod;
    if (!mdl->src) // this is set when we form arrays where count > 0
        return;

    /// narrow down type traits
    string name = cast(string, mdl);
    model mdl_src = mdl;
    AType mdl_type = isa(mdl);
    if (instanceof(mdl_src, typeid(model))) {
        while (instanceof(mdl_src, typeid(model)) && mdl_src->src) {
            if (mdl_src->ref)
                break;
            AType a_src = mdl_src->src;
            mdl_src = mdl_src->src;
        }
    } else if (mdl->src) {
        mdl_src = mdl->src; // must be a type definition here
        verify(isa(mdl_src) == 0x00, "expected type definition");
    }
    AType type = isa(mdl_src) ? (AType)isa(mdl_src) : (AType)mdl_src;

    if ((type->traits & A_TRAIT_PRIMITIVE) != 0) {
        print("creating primitive for type %s", type->name);
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
        else if (type == typeid(cstrs))
            mdl->type = LLVMPointerType(LLVMInt8Type(), 0);
        else if (type == typeid(symbol)) {
            mdl->type = LLVMPointerType(LLVMInt8Type(), 0);
        } else if (type == typeid(cstr))
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
        } else if (type == typeid(fn)) {
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

        print("type ref = %p", mdl->type);

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

        // can be a class, structure, function
        if (type == typeid(model)) {
            /// now we should handle the case 
            model  src = mdl->src;
            AType src_cl = isa(src);
            /// this is a reference, so we create type and debug based on this
            u64 ptr_sz = LLVMPointerSize(e->target_data);
            mdl->type  = LLVMPointerType(
                src->type == LLVMVoidType() ? LLVMInt8Type() : src->type, 0);
            model src_name = mdl->name ? mdl : (model)mdl->src;
            if (src_name->name) {
                int ln = len(name);
                mdl->debug = LLVMDIBuilderCreatePointerType(e->dbg_builder, src->debug,
                    ptr_sz * 8, 0, 0, name->chars, ln);
            }
        } else if (instanceof(mdl_src, typeid(record))) {
            record rec = mdl_src;
            mdl->type  = rec->type;
            mdl->debug = rec->debug;
        } else if (type == typeid(function)) {
            function fn = mdl_src;
            mdl->type  = fn->type;
            mdl->debug = fn->debug;
        } else if (type == typeid(aether))
            return;
        else if (type != typeid(aether)) {
            //fault("unsupported model type: %s", type->name);
        }
    } 
    if (mdl->type && mdl->type != LLVMVoidType()) { /// we will encounter errors with aliases to void
        LLVMTypeRef type = mdl->type;
        /// todo: validate: when referencing these, we must find src where type != null
        mdl->size      = LLVMABISizeOfType     (mdl->mod->target_data, type);
        mdl->alignment = LLVMABIAlignmentOfType(mdl->mod->target_data, type);
    }
    if (instanceof(mdl, typeid(record))) {
        mdl->scope = mdl->debug; // set this in record manually when debug is set
    } else if (!mdl->scope)
        mdl->scope = e->scope;

    // convert to array if count is set
    if (mdl->count > 0 && mdl->type && LLVMGetTypeKind(mdl->type) != LLVMVoidTypeKind) {
        mdl->type = LLVMArrayType(mdl->type, mdl->count);
        mdl->size = LLVMABISizeOfType(mdl->mod->target_data, mdl->type);
        if (mdl->debug)
            mdl->debug = LLVMDIBuilderCreateArrayType(
                e->dbg_builder,
                LLVMABISizeOfType(e->target_data, mdl->type) * 8,
                LLVMABIAlignmentOfType(e->target_data, mdl->type),
                null, 0, 0);
    }
}

model model_alias(model src, string name, reference r, array shape);

class read_parent(class mdl) {
    if (isa(mdl) == typeid(class) && mdl->parent) {
        model p = mdl->parent;
        verify(isa(p->src) == typeid(class), "expected parent src");
        if (p->src) return p->src;
    }
    return null;
}

bool model_einherits(model mdl, model base) {
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
            mod, mdl->mod, name, opt_name ? opt_name : (string)mdl->name,
            ref, reference_pointer, members, mdl->members,
            body, mdl->body, src, mdl, type, mdl->type);
    return mdl->ptr;
}

model model_alias(model src, string name, reference r, array shape) {
    if (!name) name = src->name;
    verify(instanceof(src, typeid(model)), "model_alias: expected model source");

    if (!src->aliases) src->aliases = new(array);
        
    /// lookup alias in identity cache
    string s_name = cast(string, name);
    each(src->aliases, model, mdl) {
        string s_name_alias = cast(string, mdl->name);
        /// check for null values
        if (compare(s_name_alias, s_name) == 0 && 
                (mdl->shape == (A)shape || 
                (mdl->shape && shape && compare(mdl->shape, shape)))) {
            return mdl;
        }
    }

    record rec = instanceof(src, typeid(record));
    if (rec) {
        verify(rec->type, "no type on record; need to 'build-record' first");
    }
    if (instanceof(name, typeid(token)))
        name = string(((token)name)->chars);
    if (!name && shape) {
        /// lets create a bindable name here
        int slen = len(shape);
        if (slen == 0) {
            name = form(string, "array_%o", src->name);
        } else {
            enode  n = first(shape);
            AType component_type = n->literal ? isa(n->literal) : isa(n);
            verify(component_type == typeid(i64) || 
                   component_type == typeid(sz), "expected size type in model wrap");
            sz size = *(sz*)n->literal;
            name = form(string, "array_%i_%o", (int)size, src->name);
        }
    }

    model  ref = model(
        mod,    src->mod,
        name,   name,
        shape,  shape,
        is_alias, true,
        ref,    r,
        src,   src);

    if (shape && instanceof(shape, typeid(model)))
        ref->is_map = true;
    else if (shape && instanceof(shape, typeid(array))) {
        ref->is_array = true;
        i64 a_size = 1;
        i64 last = 0;
        array rstrides = array(32);
        each (shape, A, lit) {
            AType l_type = isa(lit);
            verify(l_type == typeid(i64), "expected numeric for array size");
            i64* num = lit;
            push(rstrides, A_i64(a_size));
            a_size  *= *num;
            last     = *num;
        }
        ref->strides = reverse(rstrides);
        ref->count = a_size;
        ref->top_stride = last; /// we require comma at this rate
    }

    /// cache alias
    push(src->aliases, ref);
    return ref;
}

void statements_init(statements st) {
    aether e = st->mod;
    AType atop = isa(e->top);
    st->scope = LLVMDIBuilderCreateLexicalBlock(e->dbg_builder, e->top->scope, e->file, 1, 0);
}

void aether_eprint_node(aether e, enode n) {
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
    fn_call(e, printf_fn,
        array_of(operand(e, string(fmt), null), n, null));
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
            eprint_node(e, n);
        } else {
            string s = instanceof(obj, typeid(string));
            verify(s, "invalid type data");
            enode n_str = operand(e, s, null);
            eprint_node(e, n_str);
        }
    }

    va_end(args);
    free(arg_nodes);
    free(buf);
}

emember aether_evar(aether e, model mdl, string name) {
    enode   a =  create(e, mdl, null);
    emember m = emember(mod, e, name, name, mdl, mdl);
    register_member(e, m);
    assign(e, m, a, OPType__assign);
    return m;
}

void code_init(code c) {
    function fn = context_model(c->mod, typeid(function));
    c->block = LLVMAppendBasicBlock(fn->value, c->label);
}

void code_seek_end(code c) {
    LLVMPositionBuilderAtEnd(c->mod->builder, c->block);
}

void aether_ecmp(aether e, enode l, comparison comp, enode r, code lcode, code rcode) {
    enode load_l = load(e, l);
    enode load_r = load(e, l);
    LLVMValueRef cond = LLVMBuildICmp(
        e->builder, (LLVMIntPredicate)comp, load_l->value, load_r->value, "cond");
    LLVMBuildCondBr(e->builder, cond, lcode->block, rcode->block);
}

enode aether_eelement(aether e, enode array, A index) {
    enode i = operand(e, index, null);
    enode element_v = value(e, LLVMBuildInBoundsGEP2(
        e->builder, array->mdl->type, array->value, &i->value, 1, "eelement"));
    return load(e, element_v);
}

void aether_einc(aether e, enode v, num amount) {
    enode lv = load(e, v);
    LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), amount, 0);
    LLVMValueRef nextI = LLVMBuildAdd(e->mod->builder, lv->value, one, "nextI");
    LLVMBuildStore(e->mod->builder, nextI, v->value);
}

/// needs to import classes, structs, methods within the classes
/// there is no such thing as a global spec for AType functions
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

void aether_ebranch(aether e, code c) {
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

void function_finalize(function fn) {
    if (fn->finalized) return;

    aether e       = fn->mod;
    int   index   = 0;
    bool  is_init = e->fn_init && e->fn_init == fn;
    fn->finalized = true;

    if (!fn->entry)
        return;

    index = 0;
    push(e, fn);
    if (fn->target) {
        LLVMMetadataRef meta = LLVMDIBuilderCreateParameterVariable(
            e->dbg_builder,          // DIBuilder reference
            fn->scope,         // The scope (subprogram/function metadata)
            "this",            // Parameter name
            4,
            1,                 // Argument index (starting from 1, not 0)
            e->file,           // File where it's defined
            fn->name->line,    // Line number
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
            first_instr);                   // Attach it in the function's entry block
        index++;
    }

    each(fn->args->args, emember, arg) {
        /// create debug for parameter here
        LLVMMetadataRef param_meta = LLVMDIBuilderCreateParameterVariable(
            e->dbg_builder,          // DIBuilder reference
            fn->scope,         // The scope (subprogram/function metadata)
             cstring(arg->name),    // Parameter name
            len(arg->name),
            1 + index,         // Argument index (starting from 1, not 0)
            e->file,           // File where it's defined
            arg->name->line,   // Line number
            arg->mdl->debug,   // Debug type of the parameter (LLVMMetadataRef for type)
            1,                 // AlwaysPreserve (1 to ensure the variable is preserved in optimized code)
            0                  // Flags (typically 0)
        );
        LLVMValueRef param_value = LLVMGetParam(fn->value, index);
        LLVMValueRef decl        = LLVMDIBuilderInsertDeclareRecordAtEnd(
            e->dbg_builder,                   // The LLVM builder
            param_value,                
            param_meta,                 // The debug metadata for the first parameter
            LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0), // Empty expression
            LLVMGetCurrentDebugLocation2(e->builder),       // Current debug location
            fn->entry);                 // Attach it in the function's entry block
        arg->value = param_value;
        index++;
    }

    // register module constructors as global initializers ONLY for delegate modules
    // aether creates a 'main' with argument parsing for its main modules
    if (is_init) {
        verify(e->top == (model)fn, "expected module context");

        if (e->mod->delegate) {
            unsigned ctr_kind = LLVMGetEnumAttributeKindForName("constructor", 11);
            LLVMAddAttributeAtIndex(fn->value, 
                LLVMAttributeFunctionIndex, 
                LLVMCreateEnumAttribute(e->module_ctx, ctr_kind, 0));
        } else {
            /// code main, which is what calls this initializer method
            eargs    args    = eargs();
            emember argc = emember(
                mod, e, mdl, emodel("int"),
                name, string("argc"), is_arg, true);
            emember argv = emember(
                mod, e, mdl, emodel("symbols"),
                name, string("argv"), is_arg, true);
            push(args, argc);
            push(args, argv);

            function main_fn = function(
                mod,            e,
                name,           string("main"),
                function_type,  A_FLAG_SMETHOD,
                export,         true,
                record,         null,
                rtype,          emodel("i32"),
                args,           args);

            push(e, main_fn);

            fault("todo: member required on function model");
            fn_call(e, null, null); // 2nd arg requires member here
            fn_return(e, A_i32(255));
            pop(e);
            register_model(e, main_fn);

            /// now we code the initializer
            push(e, fn);
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
            emember module_ctr = lookup2(e, e->name, typeid(function));
            if (module_ctr)
                fn_call(e, module_ctr, null);
            verify(module_ctr, "no module constructor found");
            fn_return(e, null);
            pop (e);
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
        ecmp   (e, i, comparison_s_less_than, argc, body, end);
        seek_end (body);
        enode arg = eelement(e, argv, i);
        eprint (e, "{0}", arg);
        einc   (e, i, 1);
        ebranch(e, cond);
        seek_end (end);
        pop (e);
    }
*/
        }
    }
    pop(e);
}

void function_init(function fn) {
    //verify(fn->record || !eq(fn->name, "main"), "to implement main, implement fn module-name[]");
}

void function_use(function fn) {
    if (fn->value)
        return;
    
    aether            e         = fn->mod;
    int              n_args    = len(fn->args);
    LLVMTypeRef*     arg_types = calloc(4 + (fn->target != null) + n_args, sizeof(LLVMTypeRef));
    int              index     = 0;
    model            top       = e->top;

    if (fn->record) {
        verify (isa(fn->record) == typeid(structure) || 
                isa(fn->record) == typeid(class),
            "target [incoming] must be record type (struct / class) -- it is then made pointer-to record");
        
        /// we set is_arg to prevent registration of global
        fn->target = hold(emember(mod, e, mdl, pointer(fn->record, null), name, string("this"), is_arg, true));
        arg_types[index++] = fn->target->mdl->type;
    }

    verify(isa(fn->args) == typeid(eargs), "arg mismatch");
    
    each(fn->args->args, emember, arg) {
        verify (arg->mdl->type, "no LLVM type found for arg %o", arg->name);
        arg_types[index++]   = arg->mdl->type;
    }

    fn->arg_types = arg_types;
    fn->arg_count = index;

    fn->type  = LLVMFunctionType(fn->rtype->type, fn->arg_types, fn->arg_count, fn->va_args);
    fn->value = LLVMAddFunction(fn->mod->module,
        fn->extern_name ? fn->extern_name->chars : fn->name->chars, fn->type);
    bool is_extern = !!fn->from_include || fn->export;

    /// create debug info for eargs (including target)
    index = 0;
    if (fn->target) {
        fn->target->value = LLVMGetParam(fn->value, index++);
        fn->target->is_arg = true;
        set(fn->members, string("a"), fn->target); /// here we have the LLVM Value Ref of the first arg, or, our instance pointer
    }
    each(fn->args->args, emember, arg) {
        arg->value = LLVMGetParam(fn->value, index++);
        arg->is_arg = true;
        set(fn->members, string(arg->name->chars), arg);
    }
    
    //verify(fmem, "function emember access not found");
    LLVMSetLinkage(fn->value,
        is_extern ? LLVMExternalLinkage : LLVMInternalLinkage);

    if (!is_extern || fn->export) {
        // Create function debug info
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
        // attach debug info to function
        LLVMSetSubprogram(fn->value, fn->scope);
        fn->entry = LLVMAppendBasicBlockInContext(
            e->module_ctx, fn->value, "entry");
    }
}

none enumeration_finalize(enumeration en) {
    if (en->finalized) return;
    en->finalized = true;
    aether e = en->mod;
    if (!en->src)
         en->src = emodel("i32");

    en->size = en->src->size;

    // set ABI size/alignment
    en->size     = LLVMABISizeOfType(e->target_data, en->src->type); // ?
    en->alignment = LLVMABIAlignmentOfType(e->target_data, en->src->type);

    // each member must represent the value of an export symbol:
    //      E ## _ ## name
    pairs(en->members, i) {
        emember mem = i->value;
        // can DBG link to a global value an accessed from a namespace E?
        string global_name = f(string, "%s_%s", en->name, mem->name);
        LLVMValueRef g = LLVMAddGlobal(e->module, en->src->type, cstring(global_name));
        LLVMSetLinkage       (g, LLVMExternalLinkage);
        LLVMSetInitializer   (g, mem->value);
        LLVMSetGlobalConstant(g, true);
        LLVMSetUnnamedAddr   (g, true);
    }
}

void record_finalize(record rec) {
    enumeration en = instanceof(rec, typeid(enumeration));
    if (rec->finalized || en) {
        rec->finalized = true;
        return;
    }
    int    total = 0;
    aether e     = rec->mod;
    array  a     = array(32);
    a->assorted = true;
    record r     = rec;
    class  is_class = instanceof(rec, typeid(class));

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

            if (instanceof(mem->mdl, typeid(function)))
                continue;
            
            finalize(mem->mdl);
            if (!mem->debug) {
                mem->debug = LLVMDIBuilderCreateMemberType(
                    e->dbg_builder,              // LLVMDIBuilderRef
                    e->top->scope,         // Scope of the emember (can be the struct, class or base module)
                    cstring(mem->name),         // Name of the emember
                    len(mem->name),        // Length of the name
                    e->file,               // The file where the emember is declared
                    mem->name->line,       // Line number where the emember is declared
                    mem->mdl->size * 8,    // Size of the emember in bits (e.g., 32 for a 32-bit int)
                    mem->mdl->alignment * 8, // Alignment of the emember in bits
                    0,                     // Offset in bits from the start of the struct or class
                    0,                     // Debug info flags (e.g., 0 for none)
                    mem->mdl->debug);
            }

            if (!LLVMTypeIsSized(mem->mdl->type)) {
                finalize(mem->mdl);
            }
            
            print("(%o) setting index %i to %p (%o : %s) = %i",
                r->name, index, mem->mdl->type, mem->name, isa(mem->mdl)->name, LLVMTypeIsSized(mem->mdl->type));
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
    rec->size = LLVMABISizeOfType(target_data, rec->type);
    print("finalized and got size: %i for type %o", rec->size, rec->name);

    /// set offsets on members (needed for the method finalization)
    num imember = 0;
    each (a, record, r)
        pairs(r->members, i) {
            emember mem = i->value;
            if (instanceof(mem->mdl, typeid(function))) // functions do not occupy membership on the instance
                continue;
            mem->index  = imember;
            mem->offset = LLVMOffsetOfElement(target_data, rec->type, imember);
            if (!instanceof(r, typeid(uni))) // unions have 1 emember
                imember++;
        }

    record is_record = instanceof(rec, typeid(record));
    if (is_record) {
        pointer(is_record, null);
    }

    /// build initializers for silver records
    if (!rec->from_include && instanceof(rec, typeid(class))) {
        // avoid this for imported
        function fn_init = initializer(rec);
        push(e, fn_init);
        pairs(rec->members, i) {
            emember mem = i->value;
            if (mem->initializer)
                build_initializer(e, mem);
        }
        fn_return(e, null);
        pop(e);
    }
    
    int sz = LLVMABISizeOfType     (target_data, rec->type);
    int al = LLVMABIAlignmentOfType(target_data, rec->type);
    
    LLVMMetadataRef prev = rec->debug;
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

    if (prev)
        LLVMMetadataReplaceAllUsesWith(prev, rec->debug);
}

#define LLVMDwarfTag(tag) (tag)
#define DW_TAG_structure_type 0x13  // DWARF tag for structs.

void record_init(record rec) {
    aether e = rec->mod;
    rec->type = LLVMStructCreateNamed(LLVMGetGlobalContext(), rec->name->chars);

    // Create a forward declaration for the struct's debug info
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
        0                                    // Size and alignment (initially 0, finalized later)
    );
    if (len(rec->members)) {
        finalize(rec); /// cannot know emember here, but record-based methods need not know this arg (just functions)
    }
}

emember emember_resolve(emember mem, string name) {
    aether  e   = mem->mod;
    i64  index = 0;
    model base = mem->mdl->ref ? mem->mdl->src : mem->mdl; // needs to support more than indirection here
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
            function fn = instanceof(schema->mdl, typeid(function));
            res->value  = fn ? fn->value : LLVMBuildStructGEP2(
                    e->builder, base->type, actual_ptr, index, "resolve"); // GPT: mem->value is effectively the ptr value on the stack
            if (fn)
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
    enode n = operand(mem->mod, value, null);
    mem->mdl   = n->mdl;
    mem->value = n->value;
    mem->is_const = LLVMIsAConstant(n->value) != null;
}

bool emember_has_value(emember mem) {
    return mem->value != null;
}

void emember_init(emember mem) {
    aether e   = mem->mod;
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

    function is_fn   = instanceof(mdl, typeid(function));
    if      (is_fn) return;

    aether    e       = mem->mod;
    function ctx_fn  = aether_context_model(e, typeid(function));
    bool     is_user = mdl->is_user; /// i.e. import from silver; does not require processing here
    bool     is_init = false;
    record   rec     = aether_context_model(e, typeid(class));
    if (!rec) rec = aether_context_model(e, typeid(structure));

    if (ctx_fn && ctx_fn->imdl) {
        is_init = true;
        ctx_fn = null;
    }
    
    /// if we are creating a new emember inside of a function, we need
    /// to make debug and value info here
    if (ctx_fn && !mem->value) {
        verify (!mem->value, "value-ref already set auto emember");
        mem->value = LLVMBuildAlloca(e->builder, mem->mdl->type, cstring(mem->name));
        mem->debug = LLVMDIBuilderCreateAutoVariable(
            e->dbg_builder,           // DIBuilder reference
            ctx_fn->scope,          // The scope (subprogram/function metadata)
             cstring(mem->name),     // Variable name
            len(mem->name),
            e->file,            // File where it’s declared
            mem->name->line,    // Line number
            mem->mdl->debug,    // Type of the variable (e.g., LLVMMetadataRef for int)
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
    } else if (!rec && !is_user && !ctx_fn && !mem->is_type && 
               !mem->is_arg && !e->current_include && is_init && !mem->is_decl) {
        /// add module-global if this has no value, set linkage
        symbol name = mem->name->chars;
        LLVMTypeRef type = mem->mdl->type;
        // we assign constants ourselves, and value is not set until we register the
        //verify(!mem->is_const || mem->value, "const value mismatch");
        bool is_global_space = false;
        /// they would be abstract above definition of aether model
        //      less we want to start subclassing functionality of the others
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
            // do not do this if value does not come from a function or AddGlobal above!
            LLVMSetLinkage(mem->value, is_public ? LLVMExternalLinkage : LLVMPrivateLinkage);
            LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(e->dbg_builder, NULL, 0);
            LLVMMetadataRef meta = LLVMDIBuilderCreateGlobalVariableExpression(
                e->dbg_builder, e->scope, name, len(mem->name), NULL, 0, e->file, 1, mem->mdl->debug, 
                0, expr, NULL, 0);
            LLVMGlobalSetMetadata(mem->value, LLVMGetMDKindID("dbg", 3), meta);
        }
    }
}

#define int_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(i##b)), \
        value, LLVMConstInt(emodel(stringify(i##b))->type, *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(u##b)), \
        value, LLVMConstInt(emodel(stringify(u##b))->type, *(u##b*)l, 0))

#define f32_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(emodel(stringify(f##b))->type, *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, e, \
        literal, l, mdl, emodel(stringify(f##b)), \
        value, LLVMConstReal(emodel(stringify(f##b))->type, *(f##b*)l))

enode operand_primitive(aether e, A op) {
         if (instanceof(op, typeid(  enode))) return op;
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

enode aether_operand(aether e, A op, model src_model) {
    if (!op) return value(emodel("void"), null);

    if (instanceof(op, typeid(array))) {
        verify(src_model != null, "expected src_model with array data");
        return create(e, src_model, op);
    }

    enode r = operand_primitive(e, op);
    return src_model ? convert(e, r, src_model) : r;
}

enode aether_default_value(aether e, model mdl) {
    return create(e, mdl, null);
}

/// create is both stack and heap allocation (based on model->ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_create(aether e, model mdl, A args) {
    map    imap = instanceof(args, typeid(map));
    array  a    = instanceof(args, typeid(array));
    enode   n    = null;
    emember ctr  = null;

    /// construct / cast methods
    enode input = instanceof(args, typeid(enode));
    if (input) {
        verify(!imap, "unexpected data");
        emember fmem = convertible(input->mdl, mdl);
        verify(fmem, "no suitable conversion found for %o -> %o",
            input->mdl->name, mdl->name);
        /// if same type, return the enode
        if (fmem == (void*)true)
            return convert(e, input, mdl); /// primitive-based conversion goes here
        
        function fn = instanceof(fmem->mdl, typeid(function));
        if (fn->function_type & A_FLAG_CONSTRUCT) {
            /// ctr: call before init
            /// this also means the mdl is not a primitive
            verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = fmem;
        } else if (fn->function_type & A_FLAG_CAST) {
            /// cast call on input
            return fn_call(e, fn, array_of(input, null));
        } else
            fault("unknown error");
    }

    model        src        = mdl->ref == reference_pointer ? mdl->src : mdl;
    array        shape      = mdl->shape;
    i64          slen       = shape ? len(shape) : 0;
    num          count      = mdl->count ? mdl->count : 1;
    bool         use_stack  = mdl->ref == reference_value && !is_class(mdl);
    LLVMValueRef size_A     = LLVMConstInt(LLVMInt64Type(), 32, false);
    LLVMValueRef size_mdl   = LLVMConstInt(LLVMInt64Type(), src->size * count, false);
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
            enode         expr     = operand(e, element, null); /// todo: cases of array embedding must be handled
            enode         conv     = convert(e, expr, mdl->src);

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
            enode   arg_value = instanceof(i->value, typeid(enode));
            set    (used, arg_name, A_bool(true));
            emember m = get(mdl->members, arg_name);
            verify(m, "emember %o not found on record: %o", arg_name, mdl->name);
            enode   arg_conv = convert(e, arg_value, m->mdl);

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
        enode a = operand(e, args, null);
        /// this should only happen on primitives
        if (perform_assign) {
            enode conv = convert(e, a, n->mdl);
            n         = assign (e, n, conv, OPType__assign);
        }
    }
    return n;
}

LLVMValueRef get_memset_function(aether e) {
    // Parameters for memset: void* (i8*), int (i8), size_t (i64)
    LLVMTypeRef param_types[] = {
        LLVMPointerType(LLVMInt8TypeInContext(e->module_ctx), 0),  // void* (i8*)
        LLVMInt8TypeInContext(e->module_ctx),                      // int (i8)
        LLVMInt64TypeInContext(e->module_ctx)                     // size_t (i64)
    };

    LLVMTypeRef memset_type = LLVMFunctionType(
        LLVMVoidTypeInContext(e->module_ctx),  // Return type (void)
        param_types,                           // Parameters
        3,                                     // Number of parameters
        false                                  // Not variadic
    );

    // Check if memset is already declared, if not, declare it
    LLVMValueRef memset_func = LLVMGetNamedFunction(e->module, "memset");
    if (!memset_func) {
        memset_func = LLVMAddFunction(e->module, "memset", memset_type);
    }

    return memset_func;
}

enode aether_zero(aether e, enode n) {
    model      mdl = n->mdl;
    LLVMValueRef v = n->value;
    LLVMValueRef zero   = LLVMConstInt(LLVMInt8Type(), 0, 0);          // value for memset (0)
    LLVMValueRef size   = LLVMConstInt(LLVMInt64Type(), mdl->size, 0); // size of alloc
    LLVMValueRef memset = LLVMBuildMemSet(e->builder, v, zero, size, 0);
    return n;
}


model prefer_mdl(model m0, model m1) {
    aether e = m0->mod;
    if (m0 == m1) return m0;
    model g = emodel("any");
    if (m0 == g) return m1;
    if (m1 == g) return m0;
    if (model_einherits(m0, m1))
        return m1;
    return m0;
}


enode aether_ternary(aether e, enode cond_expr, enode true_expr, enode false_expr) {
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

enode aether_builder(aether e, subprocedure cond_builder) {
    LLVMBasicBlockRef block = LLVMGetInsertBlock(e->builder);
    LLVMPositionBuilderAtEnd(e->builder, block);
    enode   n = invoke(cond_builder, null);
    return n;
}

enode aether_if_else(aether e, array conds, array exprs, subprocedure cond_builder, subprocedure expr_builder) {
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
        LLVMValueRef condition = convert(e, cond_result, emodel("bool"))->value;

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
    return enode(mod, e, mdl, emodel("void"), value, null);  // Dummy enode, replace with real enode if needed
}

enode aether_addr_of(aether e, enode expr, model mdl) {
    model        ref   = pointer(mdl ? mdl : expr->mdl, null); // this needs to set mdl->type to LLVMPointerType(mdl_arg->type, 0)
    emember      m_expr = instanceof(expr, typeid(emember));
    LLVMValueRef value = m_expr ? expr->value :
        LLVMBuildGEP2(e->builder, ref->type, expr->value, NULL, 0, "ref_expr");
    return enode(
        mod,   e,
        value, value,
        mdl,   ref);
}

enode aether_offset(aether e, enode n, A offset) {
    emember mem = instanceof(n, typeid(emember));
    model  mdl = n->mdl;
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
        i = operand(e, A_i64(0), null);
        for (int a = 0; a < len(args); a++) {
            A arg_index   = args->elements[a];
            A shape_index = shape->elements[a];
            enode   stride_pos  = mul(e, shape_index, arg_index);
            i = add(e, i, stride_pos);
        }
    } else
        i = operand(e, offset, null);

    verify(mdl->ref == reference_pointer, "offset requires pointer");

    LLVMValueRef ptr_load   = LLVMBuildLoad2(e->builder,
        LLVMPointerType(mdl->type, 0), n->value, "load");
    LLVMValueRef ptr_offset = LLVMBuildGEP2(e->builder,
         mdl->type, ptr_load, &i->value, 1, "offset");

    return mem ? (enode)emember(mod, e, mdl, mdl, value, ptr_offset, name, mem->name) :
                         enode(mod, e, mdl, mdl, value, ptr_offset);
}

enode aether_load(aether e, emember mem) {
    if (!instanceof(mem, typeid(emember))) return mem; // for enode values, no load required
    model        mdl      = mem->mdl;
    LLVMValueRef ptr      = mem->value;

    // if this is a emember on record, build an offset given its 
    // index and 'this' argument pointer
    if (!ptr) {
        if (mem->is_module) {
            verify(ptr, "expected value for module emember (LLVMAddGlobal result)");
        } else {
            emember target = elookup("a"); // unique to the function in class, not the class
            verify(target, "no target found when looking up emember");
            /// static methods do not have this in context
            record rec = target->mdl->src;
            ptr = LLVMBuildStructGEP2(
                e->builder, rec->type, target->value, mem->index, "emember-ptr");
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
enode aether_convert(aether e, enode expr, model rtype) {
    expr = load(e, expr);
    model        F = expr->mdl;
    model        T = rtype;
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
            V = is_signed(F) ? LLVMBuildSExt(B, V, T->type, "sext")
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
        if (isa(ctx) == type)
            return ctx;
    }
    return null;
}

model aether_return_type(aether e) {
    for (int i = len(e->lex) - 1; i >= 0; i--) {
        model ctx = e->lex->elements[i];
        if (ctx->rtype) return ctx->rtype;
    }
    return null;
}

void assign_args(aether e, enode L, A R, enode* r_L, enode* r_R) {
    *r_R = operand(e, R, null);
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

enode aether_assign(aether e, enode L, A R, OPType op) {
    int v_op = op;
    verify(op >= OPType__assign && op <= OPType__assign_left, "invalid assignment-operator");
    enode rL, rR = operand(e, R, null);
    enode res = rR;
    if (op != OPType__assign) {
        rL = load(e, L);
        res = value(L->mdl,
            op_table[op - OPType__assign - 1].f_op
                (e->builder, rL->value, rR->value, e_str(OPType, op)->chars));
    }
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
        model ctx = e->lex->elements[i];
        AType ctx_type = isa(ctx);
        model top = e->top;
        cstr   n = cast(cstr, (string)name);
        emember m = ctx->members ? get(ctx->members, string(n)) : null;
        if    (m) {
            AType mdl_type = isa(m->mdl);
            if (mdl_type_filter) {
                if (mdl_type != mdl_type_filter)
                    continue;
            }
            return m;
        }
        class cl = instanceof(ctx, typeid(class));
        if (cl) {
            class parent = cl->parent;
            while (parent) {
                emember  m = get(parent->members, name);
                if (m) return  m;
                parent = parent->parent;
            }
        }
    }
    return null;
}

model aether_push(aether e, model mdl) {

    model existing = emodel(mdl->name->chars);
    verify(!existing || existing == mdl,
        "conflicting model for %o", mdl->name);

    function fn_prev = context_model(e, typeid(function));
    if (fn_prev) {
        fn_prev->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);
    }

    verify(mdl, "no context given");
    function fn = instanceof(mdl, typeid(function));
    if (fn)
        function_use(fn);

    push(e->lex, mdl);
    e->top = mdl;
    
    if (fn) {
        LLVMPositionBuilderAtEnd(e->builder, fn->entry);
        if (LLVMGetBasicBlockTerminator(fn->entry) == NULL) {
            LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
                e->module_ctx, fn->name->line, 0, fn->scope, NULL);
            LLVMSetCurrentDebugLocation2(e->builder, loc);
        } else if (fn->last_dbg)
            LLVMSetCurrentDebugLocation2(e->builder, fn->last_dbg);
    } 
    return mdl;
}

none emember_release(emember mem) {
    aether e = mem->mod;
    model mdl = mem->mdl;
    if (mdl->ref == reference_pointer) {
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
            fn_call(e, dealloc, array_of(mem, null));
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
    function fn_prev = context_model(e, typeid(function));
    if (fn_prev)
        fn_prev->last_dbg = LLVMGetCurrentDebugLocation2(e->builder);

    pop(e->lex);
    
    function fn = context_model(e, typeid(function));
    if (fn && fn != fn_prev) {
        LLVMPositionBuilderAtEnd(e->builder, fn->entry);
        //if (!instanceof(fn->imdl, typeid(aether)))
        LLVMSetCurrentDebugLocation2(e->builder, fn->last_dbg);
    }

    if (len(e->lex))
        e->top = last(e->lex);
    else
        e->top = null;
    
    return e->top;
}

function model_initializer(model mdl) {
    aether   e       = mdl->mod;
    model    rtype   = emodel("none");
    string   fn_name = form(string, "_%o_init", mdl->name);
    record   rec     = instanceof(mdl, typeid(record));
    function fn_init = function(
        mod,      mdl->mod,
        name,     fn_name,
        record,   mdl->type ? mdl : null,
        imdl,     mdl,
        is_init,  true,
        rtype,    rtype,
        members,  rec ? map() : e->top->members, /// share the same members (beats filtering the list)
        args,     eargs());

    mdl->fn_init = fn_init;
    return fn_init;
}

void enumeration_init(enumeration mdl) {
    aether e = mdl->mod;
    mdl->src  = mdl->src ? mdl->src : emodel("i32");
    mdl->type = mdl->src->type;
}

/// return a map of defs found by their name (we can isolate the namespace this way by having separate maps)
path aether_include_path(aether e, string include) {
    string   include_path  = form(string, "%o/include", e->install);
    path     full_path = null;
    symbol   ipaths[]  = {
        include_path->chars,
        "/usr/include"
    };
    symbol templates[3] = {
        "%s/%o-flat", /// this is how we read our A-types without a fancy pre-processor
        "%s/%o",
        "%s/%o.h"
    };
    bool br = false;
    for (int i = 0; !br && i < sizeof(ipaths) / sizeof(symbol); i++) {
        for (int ii = 0; ii < 3; ii++) {
            path r0 = form(path, (cstr)templates[ii], ipaths[i], include);
            if (exists(r0)) {
                full_path = r0;
                br = true;
                break;
            }
        }
    }
    return full_path;
}

/// return a map of defs found by their name (we can isolate the namespace this way by having separate maps)
path aether_include(aether e, A include) {
    fault("include must be re-written");

    path full_path  = instanceof(include, typeid(path)) ?
        (path)include : include_path(e, include);

    verify (full_path, "include path not found for %o", include);

    /*
    CXIndex index = clang_createIndex(0, 0);
    const char* args[] = {"-x", "c-header"}; /// allow 'A' to parse as a 
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, full_path->chars, args, 2, NULL, 0, CXTranslationUnit_None);

    verify(unit, "unable to parse translation unit %o", include);
    
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    e->current_include = full_path;
    clang_visitChildren(cursor, visit, (CXClientData)e);
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    */

    e->current_include = null;
    return full_path;
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
    emember _i32 = elookup("i32");
    emember _i16 = elookup("i16");
    emember _i64 = elookup("i64");
    emember _u64 = elookup("u64");
    emember _handle = elookup("handle");
    emember _symbol = elookup("symbol");
  //emember _ARef   = elookup("ARef");
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

    _member->members = m(
        "name",             _symbol,
        "sname",            _handle,
        "type",             _AType_ptr_mem,
        "offset",           _i32,
        "count",            _i32,
        "member_type",      _i32,
        "operator_type",    _i32,
        "required",         _i32,
        "args",             _meta_t_mem,
        "ptr",              _handle,
        "method",           _handle,
        "id",               _i64,
        "value",            _i64);

    _meta_t->members = m(
        "count",  _i64,
        "meta_0", _AType_ptr_mem,
        "meta_1", _AType_ptr_mem,
        "meta_2", _AType_ptr_mem,
        "meta_3", _AType_ptr_mem,
        "meta_4", _AType_ptr_mem,
        "meta_5", _AType_ptr_mem,
        "meta_6", _AType_ptr_mem,
        "meta_7", _AType_ptr_mem,
        "meta_8", _AType_ptr_mem,
        "meta_9", _AType_ptr_mem);

    _af_recycler->members = m(
        "af",       _handle,
        "af_count", _i64,
        "af_alloc", _i64,
        "re",       _handle,
        "re_count", _i64,
        "re_alloc", _i64);

    _AType->members = m(
        "parent_type",    _AType_ptr_mem,
        "name",           _symbol,
        "module",         _symbol,
        "sub_types",      _handle, // needs to be _ATypeRef
        "sub_types_count", _i16,
        "sub_types_alloc", _i16,
        "size",           _i32,
        "isize",          _i32,
        "af",             af_recycler_ptr_mem,
        "magic",          _i32,
        "global_count",   _i32,
        "vmember_count",  _i32,
        "vmember_type",   _AType_ptr_mem,
        "member_count",   _i32,
        "members",        member_ptr_mem,
        "traits",         _i32,
        "user",           _handle,
        "required",       emember(mod, e, name, string("u64[2]"), mdl, model(mod, e, src, _u64->mdl, count, 2)),
        "src",            _AType_ptr_mem,
        "arb",            _handle,
        "meta",           _meta_t_mem);

    finalize(af_recycler_ptr);
    finalize(_AType_ptr);
    finalize(_AType);
    finalize(_meta_t);
    finalize(_af_recycler);
    finalize(_member);

    verify(_AType->size       == sizeof(struct _AType),       "AType size mismatch");
    verify(_af_recycler->size == sizeof(struct _af_recycler), "_af_recycler size mismatch");
    verify(_meta_t->size      == sizeof(struct _meta_t),      "_meta_t size mismatch");
    verify(_member->size      == sizeof(struct _member),      "_member size mismatch");
}

void aether_A_import(aether e, path lib) {
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

    for (num i = 0; i < ln; i++) {
        AType  atype = a[i];
        model  mdl   = null;

        bool  is_abstract  = (atype->traits & A_TRAIT_ABSTRACT)  != 0;
        if (atype->user || is_abstract) continue; // if we have already processed this type, continue
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
        if      (is_class)    mdl = class      (mod, e, name, name, from_include, inc);
        else if (is_struct)   mdl = structure  (mod, e, name, name, from_include, inc);
        else if (is_enum)     mdl = enumeration(mod, e, name, name);
        else if (is_prim)     mdl = model      (mod, e, name, name, src, atype);

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
            if (atype != typeid(A))
                ((class)mdl)->parent = emodel(atype->parent_type->name);
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
            class     cl = is_class  ? mdl : null;

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
                    mem_mdl = function(
                        is_ctr,        true,
                        rtype,         st ? st : null, // structure ctr return data (A-type ABI)
                        target,        cl ? pointer(cl, null) : null, 
                        args,          eargs(mod, e, args, a(mtype->user)));
                }
                else if ((amem->member_type & A_FLAG_CAST) != 0) {
                    verify(mtype, "cast cannot be void");
                    mem_mdl = function(
                        is_cast,       true,
                        rtype,         mtype->user,
                        target,        pointer(mdl, null));
                }
                else if ((amem->member_type & A_FLAG_INDEX) != 0) {
                    verify(mtype, "index cannot return void");
                    model index_mdl = amem->args.meta_0->user;
                    verify(index_mdl, "index requires argument");
                    mem_mdl = function(
                        mod,           e,
                        name,          n,
                        extern_name,   f(string, "%o_%o", mdl->name, n),
                        is_cast,       true,
                        rtype,         mtype->user,
                        target,        pointer(mdl, null),
                        args,          eargs(mod, e, args, a(index_mdl)));
                }
                else if ((amem->member_type & A_FLAG_OPERATOR) != 0) {
                    model arg_mdl = amem->args.meta_0->user;
                    mem_mdl = function(
                        mod,           e,
                        name,          n,
                        extern_name,   f(string, "%o_%o", mdl->name, n),
                        is_oper,       amem->operator_type,
                        rtype,         mtype->user,
                        target,        pointer(mdl, null),
                        args,          arg_mdl ? eargs(mod, e, args, a(arg_mdl)) : null);
                }
                else if ((amem->member_type & A_FLAG_IMETHOD) != 0 ||
                         (amem->member_type & A_FLAG_SMETHOD) != 0 ||
                         (amem->member_type & A_FLAG_IFINAL)  != 0) {
                    bool  is_inst = (amem->member_type & A_FLAG_IMETHOD) != 0;
                    bool  is_f    = (amem->member_type & A_FLAG_IFINAL)  != 0;
                    eargs args    = eargs(mod, e, args, array(alloc, 8, assorted, true));
                    for (int a = 0; a < amem->args.count; a++) {
                        AType arg_type = ((AType*)(&amem->args.meta_0))[a];
                        model arg_mdl  = arg_type->user;
                        push(args->args, arg_mdl);
                    }
                    mem_mdl = function(
                        mod,           e,
                        name,          n,
                        is_cast,       true,
                        is_override,   (amem->member_type & A_FLAG_OVERRIDE) != 0,
                        rtype,         mtype->user,
                        target,        (is_inst || is_f) ? pointer(mdl, null) : null,
                        args,          args,
                        extern_name,   is_f ? n : f(string, "%o_%o", mdl->name, n));
                }
                else if ((amem->member_type & A_FLAG_PROP) ||
                         (amem->member_type & A_FLAG_VPROP)) {
                    bool is_cl = isa(mtype->user) == typeid(class);
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
        if (isa(mdl) == typeid(function))
            function_use(mdl);
    }
}

/// we may have a kind of 'module' given here; i suppose instanceof(aether) is enough
void aether_init(aether e) {
    e->with_debug = true;
    e->finalizing = hold(array(alloc, 64, assorted, true, unmanaged, true));
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
    int L_size = L->size;
    int R_size = R->size;
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
    bool fr_ptr = fr->ref == reference_pointer;
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
        function fn = instanceof(mem->mdl, typeid(function));
        if (!fn || !(fn->function_type & A_FLAG_CONSTRUCT))
            continue;
        emember first = null;
        pairs (fn->members, i) {
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
        function fn = instanceof(mem->mdl, typeid(function));
        if (!fn || !(fn->function_type & A_FLAG_CAST))
            continue;
        if (fn->rtype == to)
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
        if (!fn || !(fn->function_type & A_FLAG_CONSTRUCT))
            continue;
        emember first = null;
        pairs (fn->members, i) {
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


void aether_push_state(aether a, array tokens, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = A_struct(tokens_data);
    state->tokens = a->tokens;
    state->cursor = a->cursor;
    print("push cursor: %i", state->cursor);
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
        function fn = instanceof(mem->mdl, typeid(function));
        /// must be function with optional name check
        if (!fn || (((f & fn->function_type) == f) && (n && !eq(n, fn->name->chars))))
            continue;
        /// arg count check
        if (len(fn->args) != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        each(fn->args->args, emember, to_arg) {
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
            e->builder, L->value, LLVMIntType(L->mdl->size * 8), 1, "to-signed");
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

enode aether_not(aether e, enode L) {
    LLVMValueRef result;
    if (is_float(L->mdl->type)) {
        // for floats, compare with 0.0 and return true if > 0.0
        result = LLVMBuildFCmp(e->builder, LLVMRealOLE, L->value,
                               LLVMConstReal(L->mdl->type, 0.0), "float-not");
    } else if (is_unsigned(L->mdl->type)) {
        // for unsigned integers, compare with 0
        result = LLVMBuildICmp(e->builder, LLVMIntULE, L->value,
                               LLVMConstInt(L->mdl->type, 0, 0), "unsigned-not");
    } else {
        // for signed integers, compare with 0
        result = LLVMBuildICmp(e->builder, LLVMIntSLE, L->value,
                               LLVMConstInt(L->mdl->type, 0, 0), "signed-not");
    }
    return value(emodel("bool"), result);
}

enode aether_bitwise_not(aether e, enode L) {
    return LLVMBuildNot(e->builder, L->value, "bitwise-not");
}

// A o = obj("type-name", props)
enode aether_eq(aether e, enode L, enode R);

enode aether_is(aether e, enode L, A R) {
    enode L_type =  offset(e, L, A_i64(-sizeof(struct _A)));
    enode L_ptr  =    load(e, L_type); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = operand(e, R, null);
    return aether_eq(e, L_ptr, R_ptr);
}

enode aether_cmp(aether e, enode L, enode R) {
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

enode aether_eq(aether e, enode L, enode R) {
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

enode aether_not_eq(aether e, enode L, enode R) {
    model t0 = L->mdl;
    model t1 = R->mdl;
    verify (t0 == t1, "types must be same at primitive operation level");
    bool i0 = is_integral(t0);
    bool f0 = is_realistic(t1);
    if (i0 || !f0)
        return value(emodel("bool"), LLVMBuildICmp(e->builder, LLVMIntNE,   L->value, R->value, "not-eq-i"));
    return     value(emodel("bool"), LLVMBuildFCmp(e->builder, LLVMRealONE, L->value, R->value, "not-eq-f"));
}

enode aether_fn_return(aether e, A o) {
    function fn = context_model(e, typeid(function));
    verify (fn, "function not found in context");

    if (!o) return value(fn->rtype, LLVMBuildRetVoid(e->builder));

    enode conv = convert(e, operand(e, o, null), fn->rtype);
    return value(fn->rtype, LLVMBuildRet(e->builder, conv->value));
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
        case 'p': return model_alias(emodel("i8"), null, reference_pointer, null);
    }
    fault("formatter implementation needed");
    return null;
}

enode aether_fn_call(aether e, emember fn_mem, array args) {
    verify(isa(fn_mem->mdl) == typeid(function), "model provided is not function");
    function fn = fn_mem->mdl;
    function_use(fn);

    //use(fn); /// value and type not registered until we use it (sometimes; less we are exporting this from module)
    int n_args = args ? len(args) : 0;
    LLVMValueRef* arg_values = calloc((fn_mem->target_member != null) + n_args, sizeof(LLVMValueRef));
    //verify (LLVMTypeOf(fdef->function->value) == fdef->ref, "integrity check failure");
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

            emember    f_arg = get(fn->args->args, index);
            AType     vtype = isa(value);
            enode      op    = operand(e, value, null);
            enode      conv  = convert(e, op, f_arg ? f_arg->mdl : op->mdl);
            
            LLVMValueRef vr = arg_values[index] = conv->value;
            //print("aether_fcall: %o arg[%i]: vr: %p, type: %s", fn_mem->name, index, vr, LLVMPrintTypeToString(LLVMTypeOf(vr)));
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
                enode n_arg     = load(e, o_arg);
                enode arg       = operand(e, n_arg, null);
                enode conv      = convert(e, arg, arg_type); 
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
enode aether_op(aether e, OPType optype, string op_name, A L, A R) {
    emember mL = instanceof(L, typeid(emember)); 
    enode   LV = operand(e, L, null);
    enode   RV = operand(e, R, null);

    /// check for overload
    if (op_name && isa(op_name) == typeid(enode) && is_class(((enode)L)->mdl)) {
        enode Ln = L;
        AType type = isa(Ln->mdl);
        if (type == typeid(structure) || type == typeid(record)) {
            record rec = Ln->mdl;
            emember Lt = get(rec->members, op_name);
            if  (Lt && isa(Lt->mdl) == typeid(function)) {
                function fn = Lt->mdl;
                verify(len(fn->args) == 1, "expected 1 argument for operator method");
                /// convert argument and call method
                model  arg_expects = get(fn->args->args, 0);
                enode  conv = convert(e, Ln, arg_expects);
                array args = array_of(conv, null);
                verify(mL, "mL == null");
                emember fmem = emember(mod, e, mdl, Lt->mdl, name, Lt->name, target_member, mL);
                return fn_call(e, fmem, args); // todo: fix me: Lt must have target_member associated, so allocate a emember() for this
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    model rtype = determine_rtype(e, optype, LV->mdl, RV->mdl); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too
    LLVMValueRef RES;
    enode LL = optype == OPType__assign ? LV : convert(e, LV, rtype); // we dont need the 'load' in here, or convert even
    enode RL = convert(e, RV, rtype);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = e->builder;
    A literal = null;
    /// if are not storing ...
    if (optype >= OPType__add && optype <= OPType__left) {
        struct op_entry* op = &op_table[optype - OPType__add];
        RES = op->f_op(B, LL->value, RL->value, N);
    } else if (optype == OPType__equal)
        return eq(e, LL, RL);
    else if (optype == OPType__compare)
        return cmp(e, LL, RL);
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

enode aether_or (aether e, A L, A R) { return aether_op(e, OPType__or,  string("or"),  L, R); }
enode aether_xor(aether e, A L, A R) { return aether_op(e, OPType__xor, string("xor"), L, R); }
enode aether_and(aether e, A L, A R) { return aether_op(e, OPType__and, string("and"), L, R); }
enode aether_add(aether e, A L, A R) { return aether_op(e, OPType__add, string("add"), L, R); }
enode aether_sub(aether e, A L, A R) { return aether_op(e, OPType__sub, string("sub"), L, R); }
enode aether_mul(aether e, A L, A R) { return aether_op(e, OPType__mul, string("mul"), L, R); }
enode aether_div(aether e, A L, A R) { return aether_op(e, OPType__div, string("div"), L, R); }
enode aether_value_default(aether e, A L, A R) { return aether_op(e, OPType__value_default, string("value_default"), L, R); }
enode aether_cond_value   (aether e, A L, A R) { return aether_op(e, OPType__cond_value,    string("cond_value"), L, R); }

enode aether_einherits(aether e, enode L, A R) {
    // Get the type pointer for L
    enode L_type =  offset(e, L, A_i64(-sizeof(A)));
    enode L_ptr  =    load(e, L);
    enode R_ptr  = operand(e, R, null);

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
    enode cmp       = aether_eq(e, value(L_ptr->mdl, phi), R_ptr);

    // Load the parent pointer (assuming it's the first emember of the type struct)
    enode parent    = aether_load(e, value(L_ptr->mdl, phi));

    // Check if parent is null
    enode is_null   = aether_eq(e, parent, value(parent->mdl, LLVMConstNull(parent->mdl->type)));

    // Create the loop condition
    enode not_cmp   = aether_not(e, cmp);
    enode not_null  = aether_not(e, is_null);
    enode loop_cond = aether_and(e, not_cmp, not_null);

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
    mdl = model_source(mdl);
    if (index_of(e->finalizing, mdl) >= 0)
        return;
    push(e->finalizing, mdl);
    i32 index = len(e->finalizing) - 1;
    if (instanceof(mdl, typeid(record)))
        record_finalize(mdl);
    else if (instanceof(mdl, typeid(function)))
        function_finalize(mdl);
    else if (instanceof(mdl, typeid(enumeration)))
        enumeration_finalize(mdl);
    verify(index_of(e->finalizing, mdl) == index, "weird?");
    remove(e->finalizing, index);
}

model aether_top(aether e) {
    return e->top;
}

A read_numeric(token a) {
    cstr cs = cstring(a);
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

bool model_has_scope(model mdl) {
    return !!mdl->scope;
}

token token_with_cstr(token a, cstr s) {
    a->chars = s;
    a->len   = strlen(s);
    return a;
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

void    eargs_init (eargs a)             { if (!a->args) a->args = array(32); }
A       eargs_get  (eargs a, num i)      { return a->args->elements[i]; }
void    eargs_push (eargs a, emember mem) { push(a->args, mem); }
emember eargs_pop  (eargs a)             { return pop(a->args); }
sz      eargs_len  (eargs a)             { return len(a->args); }

define_enum  (interface)
define_enum  (reference)
define_enum  (comparison)
define_class (model,       A)
define_class (format_attr, A)
define_class (statements,  model)
define_class (eargs,   model)
define_class (function,    model)
define_class (record,      model)
define_class (aether,      model)
define_class (uni,         record)
define_class (enumeration, model)
define_class (structure,   record)
define_class (class,       record)
define_class (code,        A)
define_class (token,       string)
define_class (enode,        A)
define_class (emember,      enode)