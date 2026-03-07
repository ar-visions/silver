#define _GNU_SOURCE

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <ports.h>
#include <stddef.h>

typedef LLVMMetadataRef LLVMScope;

#include <aether/import>

#define au_lookup(sym)    lexical(a->lexical, sym)

#define elookup(sym) ({ \
    Au_t au = lexical(a->lexical, sym); \
    (au ? (etype)u(etype, au) : (etype)null); \
})

#undef fault
#define fault(t, ...) ({ \
    token pk = aether_peek_safe(a); \
    string s; \
    if (pk->line == 0) { \
        s = (string)formatter( \
            (Au_t)null, false, stderr, (Au) true, seq, \
            (symbol) \
            "\n%s:%i :: " t, \
            __FILE__, __LINE__ \
            __VA_OPT__(,) __VA_ARGS__); \
    } else { \
        s = (string)formatter( \
            (Au_t)null, false, stderr, (Au) true, seq, \
            (symbol) "\n%s:%i%o / %o:%i:%i :: " t, \
            __FILE__, __LINE__, seq ? f(string, "@%i", seq) : string(""), a->module_file, \
            aether_peek_safe(a)->line, \
            aether_peek_safe(a)->column __VA_OPT__(,) __VA_ARGS__); \
        if (level_err >= fault_level) { \
            halt(s, aether_peek(a)); \
        } \
    } \
    false; \
})

#define validate(cond, t, ...) ({ \
    if (!(cond)) { \
        fault(t __VA_OPT__(,) __VA_ARGS__); \
        false; \
    } else { \
        true; \
    } \
})

#undef verify
#define verify(cond, t, ...) ({ \
    if (!(cond)) { \
        fault(t __VA_OPT__(,) __VA_ARGS__); \
        false; \
    } \
    true; \
})


LLVMTypeRef _lltype(etype);
#define lltype(a) _lltype((etype)(a))

#define int_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(i##b)), \
        loaded, true, value, LLVMConstInt(lltype(elookup(stringify(i##b))), *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(u##b)), \
        loaded, true, value, LLVMConstInt(lltype(elookup(stringify(u##b))), *(u##b*)l, 0))

#define f32_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        loaded, true, value, LLVMConstReal(lltype(elookup(stringify(f##b))), *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        loaded, true, value, LLVMConstReal(lltype(elookup(stringify(f##b))), *(f##b*)l))


#define value(m,vr) enode(mod, a, value, vr, au, (m)->au, loaded, true)

#define B a->builder

static void print_all(aether mod, symbol label, array list) {
}

string symbol_name(Au obj) {
    string n = copy(cast(string, obj));
    for (int i = 0; i < n->count; i++)
        if (n->chars[i] == '-')
            n->chars[i] = '_';
    return n;
}

// emit source location from current token (macro to reduce repetition)
#define poly_members(TYPE, VAR) \
    for (Au_t __ctx = (TYPE); __ctx && __ctx != typeid(Au); \
         __ctx = (__ctx->context == __ctx) ? null : __ctx->context) \
        for (int __i = 0; __i < __ctx->members.count; __i++) \
            for (Au_t VAR = (Au_t)__ctx->members.origin[__i]; VAR; VAR = null)

#define debug_emit(a) do { \
    token __t = a->statement_origin; \
    if (!__t) __t = aether_peek(a); \
    if (__t) emit_debug_loc(a, __t->line, __t->column); \
} while(0)

// get the current debug scope: function subprogram or compile_unit fallback
LLVMMetadataRef debug_scope(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (ctx_u && ctx_u->llscope)
            return ctx_u->llscope;
    }
    return a->compile_unit;
}

LLVMMetadataRef debug_type_for      (aether a, Au_t src);
LLVMMetadataRef debug_struct_type   (aether a, Au_t type_au);
LLVMMetadataRef debug_enum_type     (aether a, Au_t type_au);
LLVMMetadataRef debug_pointer_type  (aether a, Au_t type_au);
LLVMMetadataRef debug_funcptr_type  (aether a, Au_t type_au);
LLVMMetadataRef debug_subroutine_type(aether a, Au_t fn_au);
void            emit_debug_function (aether a, efunc fn, bool);
void            emit_debug_variable (aether a, enode var, u32 arg_no, u32 line);
LLVMMetadataRef debug_au_header_type(aether a, Au_t schema);
void emit_debug_params              (aether a, efunc fn);
void emit_debug_global              (aether a, Au_t var_au, LLVMValueRef global_val);

void aether_emit_block_probe(aether a, u32 probe_id);
LLVMValueRef emit_clock_ns(aether a, cstr label);
LLVMValueRef emit_func_timing_start(aether a, u32 func_id);
void emit_func_timing_end(aether a, LLVMValueRef start_ns, u32 func_id);
void report_coverage(aether a);
void init_coverage(aether a);
void finalize_coverage(aether a);
void emit_coverage_register(aether);
void coverage_set_func_name(u32 func_id, char* name);

// set debug source location on the IR builder
static void emit_debug_loc(aether a, u32 line, u32 column) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    LLVMMetadataRef scope = debug_scope(a);
    if (!scope) return;
    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        a->module_ctx, line, column, scope, null);
    LLVMSetCurrentDebugLocation2(B, loc);
}

LLVMValueRef LLVMFetchGlobal(LLVMModuleRef module_ref, LLVMTypeRef type, cstr name) {
    LLVMValueRef existing = LLVMGetNamedGlobal(module_ref, name);
    if (existing) return existing;
    LLVMValueRef g = LLVMAddGlobal(module_ref, type, name);
    LLVMSetLinkage(g, LLVMExternalLinkage);
    return g;
}

etype etype_copy(etype mem) {
    Au_t au = isa(mem);
    etype n = (etype)alloc_new(au, 1, null, null);
    memcpy(n, mem, au->typesize);
    hold_members((Au)n);
    return n;
}

enode enode_value(enode mem, bool force_load) { sequencer
    if (mem && mem->is_super)
        return mem->target;

    if (!mem->loaded && (force_load || !is_struct(canonical(mem))) && !mem->au->is_imethod) {
        aether a = mem->mod;
        a->is_const_op = false;
        if (a->no_build) return e_noop(a, (etype)mem);

        Au_t au = au_arg_type((Au)mem);
        if (!force_load && is_struct(au))
            return mem;
        
        string id = f(string, "load_%i_%s", seq, mem->au->ident ? mem->au->ident : "");
        Au info = head(mem);
        etype type2 = u(etype, au);
        LLVMValueRef loaded = LLVMBuildLoad2(
            B, lltype(type2), mem->value, id->chars);
        enode  res = enode(mod, a, value, loaded, loaded, true, au, au); // resolve(mem)->au);
        return res;
    } else {
        return mem;
    }
}

// useful otherwise internals we interfaced, then stopped using after a restructuring
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


static token aether_peek(aether a) {
    if (a->cursor == len(a->tokens))
        return null;
    return (token)a->tokens->origin[a->cursor];
}

token aether_peek_safe(aether a) {
    if (a->cursor == len(a->tokens)) {
        if (!a->cursor)
            return token("[none]");
        return (token)a->tokens->origin[a->cursor - 1];
    }
    return (token)a->tokens->origin[a->cursor];
}

static inline bool is_fp_value(LLVMValueRef v) {
    LLVMTypeRef t = LLVMTypeOf(v);
    if (LLVMGetTypeKind(t) == LLVMPointerTypeKind) t = LLVMGetElementType(t);
    LLVMTypeKind k = LLVMGetTypeKind(t);
    return k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind || k == LLVMHalfTypeKind;
}

static inline bool is_fp_type(LLVMTypeRef t) {
    LLVMTypeKind k = LLVMGetTypeKind(t);
    return k == LLVMFloatTypeKind || k == LLVMDoubleTypeKind || k == LLVMHalfTypeKind;
}

static LLVMValueRef build_arith_op(
    aether a,
    LLVMBuilderRef b, OPType optype,
    LLVMValueRef L, LLVMValueRef R,
    symbol name, bool is_signed
) {
    bool fp = is_fp_value(L) || is_fp_value(R);

    switch (optype) {
        case OPType__add:   return fp ? LLVMBuildFAdd(b, L, R, name) : LLVMBuildAdd(b, L, R, name);
        case OPType__sub:   return fp ? LLVMBuildFSub(b, L, R, name) : LLVMBuildSub(b, L, R, name);
        case OPType__mul:   return fp ? LLVMBuildFMul(b, L, R, name) : LLVMBuildMul(b, L, R, name);
        case OPType__div:
            return fp ? LLVMBuildFDiv(b, L, R, name) :
                is_signed ? LLVMBuildSDiv(b, L, R, name) : LLVMBuildUDiv(b, L, R, name);
        case OPType__mod:
            return fp ? LLVMBuildFRem(b, L, R, name) :
                is_signed ? LLVMBuildSRem(b, L, R, name) : LLVMBuildURem(b, L, R, name);
        // shifts/bitwise are integral only:
        case OPType__left:  return LLVMBuildShl(b, L, R, name);
        case OPType__right: return is_signed ? LLVMBuildAShr(b, L, R, name) : LLVMBuildLShr(b, L, R, name);
        case OPType__xor:   return LLVMBuildXor(b, L, R, name);
        case OPType__or:    return LLVMBuildOr(b, L, R, name);
        case OPType__and:   return LLVMBuildAnd(b, L, R, name);

        // bitwise (integral only)
        case OPType__bitwise_or:
            verify(!fp, "bitwise or on floating type");
            return LLVMBuildOr(b, L, R, name);

        case OPType__bitwise_and:
            verify(!fp, "bitwise and on floating type");
            return LLVMBuildAnd(b, L, R, name);

        default:
            verify(false, "build_arith_op: unsupported optype"); return NULL;
    }
}


etype evar_type(evar aa) {
    aether a = aa->mod;
    if (aa->au->member_type == AU_MEMBER_VAR)
        return u(etype, aa->au->src);
    return u(etype, aa->au);
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

bool is_explicit_ref(enode arg) {
    return arg->is_explicit_ref;
}

enode aether_e_assign(aether a, enode L, Au R, OPType op_val) {
    a->is_const_op = false;
    if (a->no_build)
        return e_noop(a, null);

    // set debug location for assignment
    debug_emit(a);

    validate(op_val >= OPType__bind && op_val <= OPType__assign_left,
        "invalid assignment operator");

    /* ------------------------------------------------------------
     * Phase 1: normalize operands
     * ------------------------------------------------------------ */
    enode rR = e_operand(a, R, null);
    bool  is_init      = !L->au;
    bool  is_reassign  = L->au && L->au->is_assigned;
    bool  is_class_set = L->target && L->au && L->au->is_class;

    enode prev = null;
    bool  rel  = false;

    if (is_class_set) {
        prev = instanceof(R, enode);
        rel  = prev && prev->value != L->value;
    }

    /* ------------------------------------------------------------
     * Phase 2: validate assignment legality
     * ------------------------------------------------------------ */
    verify(is_ptr(L) || !L->loaded, "L-value not a pointer (cannot assign to value)");

    if (is_reassign)
        verify(!a->is_const_op,
            "cannot perform constant assign after initial assignment");

    /* ------------------------------------------------------------
     * Phase 3: bind L on first assignment
     * ------------------------------------------------------------ */
    if (is_init) {
        L->au = (Au_t)hold(rR->au);
        L->au->is_const = a->is_const_op;

        if (L->literal)
            drop(L->literal);
        L->literal = hold(rR->literal);
    }

    /* ------------------------------------------------------------
     * Phase 4: compute RHS value
     * ------------------------------------------------------------ */
    etype mem_type = evar_type((evar)L);

    LLVMValueRef store_target = L->value;
    if (is_explicit_ref(L) && op_val > OPType__bind) {
        etype t = u(etype, mem_type->au->src);
        store_target = LLVMBuildLoad2(B, lltype(t), L->value, "ref_target");
    }

    enode res = null;
    bool  no_store = false;

    if (op_val <= OPType__assign) {
        res = rR;
    } else {
        // load current value
        enode cur = enode_value(L, !is_struct(canonical(L)));

        // map assign op to base op (assign_add -> add, etc.)
        //OPType base_op = op_val - (OPType__assign_add - OPType__add);

        // check for operator overload
        Au_t rec_au = is_rec((Au)cur);
        
        if (rec_au) {
            do {
                for (int i = 0; i < rec_au->members.count; i++) {
                    Au_t mem = (Au_t)rec_au->members.origin[i];
                    if (mem->operator_type == op_val) {
                        etype Lt = u(etype, mem);
                        verify(Lt, "expected user structure for member %s", mem->ident);
                        verify(Lt->au->args.count == 2,
                            "expected 2 arguments for operator method");
                        res = e_fn_call(a, (efunc)Lt, a(cur, rR));
                        no_store = true;
                        break;
                    }
                }
                if (no_store || !is_class(rec_au)) break;
                rec_au = rec_au->context;
                if (rec_au == typeid(Au))
                    break;
            } while (rec_au);
        }

        if (!res) {
            // coerce RHS to match LHS type
            rR = e_create(a, (etype)canonical(L), (Au)rR);

            //build_arith_op(B, op_val, L->value, rR->value);
            string op_name = (string)e_str(OPType, op_val);
            res = e_op(a, op_val, op_name, (Au)L, (Au)rR);
        }
    }

    if (!no_store) {
        /* ------------------------------------------------------------
        * Phase 5: type reconciliation
        * ------------------------------------------------------------ */
        if ((!is_explicit_ref(L) && ref_level((Au)res) != ref_level((Au)mem_type)) ||
            resolve(res) != resolve(mem_type))
            res = e_operand(a, (Au)res, (etype)L);

        /* ------------------------------------------------------------
        * Phase 6: store
        * ------------------------------------------------------------ */
        LLVMBuildStore(B, res->value, store_target);

        // sync load so LLDB sees stored value
        if (a->debug) {
            LLVMBuildLoad2(B, LLVMTypeOf(res->value), store_target, "dbg.sync");
            debug_emit(a);
        }
    }

    /* ------------------------------------------------------------
     * Phase 7: lifetime management
     * ------------------------------------------------------------ */
    if (is_class_set) {
        retain(L);
        if (rel)
            release(prev);
    }

    return res;
}


none etype_implement(etype t, bool);

void something() {
    aether a = null;
    Au_t au_type = typeid(Au);
    etype e = u(etype, au_type);
}

static etype etype_ptr(aether a, Au_t au, enode eshape) {
    verify(au && (isa(au) == typeid(Au_t_f)), "ptr requires Au_t, given %s", isa(au)->ident);
    verify(isa(au) != typeid(etype), "etype_ptr unexpected type");
    Au_t au_ptr = au->ptr;
    if (!eshape && !au_ptr) {
        au->ptr = def(a->au, null, AU_MEMBER_TYPE, 0);
        au->ptr->is_pointer  = true;
        au->ptr->src = au;
        au_ptr = au->ptr;
    } else if (eshape) {
        // we can use Au_t with enode as well as literal shape for data-shape
        au_ptr = def(a->au, null, AU_MEMBER_TYPE, 0);
        au_ptr->is_pointer  = true;
        au_ptr->src = au;
    }

    if (!u(etype, au_ptr)) {
        etype t = etype(mod, a, au, au_ptr);
        if (eshape) {
            t->meta_b = (Au)hold(eshape);
        }
    }

    return u(etype, au_ptr);
}

etype shape_pointer(aether mod, Au a, enode eshape) {
    Au_t au = au_arg(a);
    if (au->member_type == AU_MEMBER_VAR)
        return etype_ptr(mod, au->src, eshape);
    
    return etype_ptr(mod, au, eshape);
}

etype pointer(aether mod, Au a) {
    Au_t au = au_arg(a);
    if (au->member_type == AU_MEMBER_VAR)
        return etype_ptr(mod, au->src, null);
    
    return etype_ptr(mod, au, null);
}

etype etype_traits(Au input, int traits) {
    Au_t au;
    bool is_etype = false;
    aether a = null;
    if (isa(input) == typeid(etype)) {
        verify(lltype((etype)input), "no type found on %o 1", a);
        au = ((etype)input)->au;
        a  = ((etype)input)->mod;
        is_etype = true;
    }
    else if (isa(input) == typeid(enode)) {
        au = ((enode)input)->au;
        a  = ((enode)input)->mod;
    }
    else if (isa(input) == typeid(Au_t_f)) { // this wont work at the moment without module context
        verify(lltype(etypeid(Au_t_f)), "no type found on %o 2", input);
        au = (Au_t)input;
    }
    verify(au, "unhandled input");
    //if (au->is_class) {
    //    verify(au->ptr, "expected ptr for class");
    //    au = au->ptr;
    //}
    //return (au->traits & traits) == traits ? ((etype)u(etype, au)) : null;
    verify(a, "type context unknown");
    etype u = u(etype, au);
    return (u->au->traits & traits) == traits ? u : null;
}

/// C type rules implemented
etype determine_rtype(aether a, OPType optype, etype L, etype R) {
    if (optype >= OPType__bind && optype <= OPType__assign_left)
        return canonical(L);  // Assignment operations always return the type of the left operand

    // comparisons ALWAYS return bool
    switch (optype) {
        case OPType__equal:
        case OPType__not_equal:
        case OPType__less:
        case OPType__less_eq:
        case OPType__greater:
        case OPType__greater_eq:
            return etypeid(bool);

        default:
            break;
    }

    if (optype == OPType__value_default ||
        optype == OPType__cond_value    ||
        optype == OPType__or            ||
        optype == OPType__and           ||
        optype == OPType__xor) {
        if (is_bool(L) && is_bool(R))
            return etypeid(bool);  // Logical operations on booleans return boolean
        // For bitwise operations, fall through to numeric promotion
    }

    // Numeric type promotion
    if (is_realistic(L) || is_realistic(R)) {
        // If either operand is float, result is float
        if (is_double(L) || is_double(R))
            return etypeid(f64);
        else
            return etypeid(f32);
    }

    // Integer promotion
    int L_size = L->au->abi_size;
    int R_size = R->au->abi_size;
    if (L_size > R_size)
        return canonical(L);
    else if (R_size > L_size)
        return canonical(R);

    bool L_signed = is_sign(L);
    bool R_signed = is_sign(R);
    if (L_signed && R_signed)
        return canonical(L);  // Both same size and signed
    else if (!L_signed && !R_signed)
        return canonical(L);  // Both same size and unsigned
    
    return canonical(L_signed ? R : L);  // Same size, one signed one unsigned
}

// does switcher-oo for class type
LLVMTypeRef _lltype(etype e) {
    aether a = e->mod;
    Au_t  au = e->au;
    etype f = u(etype, au);

    if (e->au->member_type == AU_MEMBER_VAR) {
        e = u(etype, e->au->src);
        au = e->au;
    }
    
    etype prev = e;

    while (au && e && !e->lltype) {
        au = au->src;
        e  = u(etype, au);
        if (!e) {
            e = e;
            break;
        }
    }

    if (e->au->is_class) {
        verify(e->au->ptr, "expected ptr for class");
        e = u(etype, e->au->ptr);
        verify(e, "resolution failure on %o", au);
    }

    verify(e->lltype, "no type found on %o", e);
    return e->lltype;
}

enode aether_e_or (aether a, Au L, Au R) { return e_op(a, OPType__or,  string("or"),  L, R); }
enode aether_e_xor(aether a, Au L, Au R) { return e_op(a, OPType__xor, string("xor"), L, R); }
enode aether_e_and(aether a, Au L, Au R) { return e_op(a, OPType__and, string("and"), L, R); }
enode aether_e_add(aether a, Au L, Au R) {
    return e_op(a, OPType__add, string("add"), L, R);
}
enode aether_e_sub(aether a, Au L, Au R) { return e_op(a, OPType__sub, string("sub"), L, R); }
enode aether_e_mul(aether a, Au L, Au R) { return e_op(a, OPType__mul, string("mul"), L, R); }

enode aether_e_div(aether a, Au L, Au R) {
    return e_op(a, OPType__div, string("div"), L, R);
}

enode aether_value_default(aether a, Au L, Au R) {
    return e_op(a, OPType__value_default, string("value_default"), L, R);
}

enode aether_cond_value   (aether a, Au L, Au R) {
    return e_op(a, OPType__cond_value, string("cond_value"), L, R);
}


enode aether_e_inherits(aether a, enode L, Au R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));

    // Get the type pointer for L
    enode L_type =  e_offset(a, L, _i64(-sizeof(Au)));
    enode L_ptr  =    e_load(a, L, null);
    enode R_ptr  = e_operand(a, R, null);

    // Create basic blocks for the loop
    LLVMBasicBlockRef block      = LLVMGetInsertBlock(B);
    LLVMValueRef      fn         = LLVMGetBasicBlockParent(block);
    LLVMBasicBlockRef loop_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "inherit_loop");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "inherit_exit");

    // Branch to the loop block
    LLVMBuildBr(B, loop_block);

    // Loop block
    LLVMPositionBuilderAtEnd(B, loop_block);
    LLVMValueRef phi = LLVMBuildPhi(B, lltype(L_ptr), "current_type");
    LLVMAddIncoming(phi, &L_ptr->value, &block, 1);

    // Compare current type with R_type
    enode cmp       = e_cmp_op(a, OPType__equal, value(L_ptr, phi), R_ptr);

    // Load the parent pointer (assuming it's the first member of the type struct)
    enode parent    = e_load(a, value(L_ptr, phi), null);

    // Check if parent is null
    enode is_null   = e_cmp_op(a, OPType__equal,
        parent, value(parent, LLVMConstNull(lltype(parent))));

    // Create the loop condition
    enode not_cmp   = e_not(a, cmp);
    enode not_null  = e_not(a, is_null);
    enode loop_cond = e_and(a, (Au)not_cmp, (Au)not_null);

    // Branch based on the loop condition
    LLVMBuildCondBr(B, loop_cond->value, loop_block, exit_block);

    // Update the phi enode
    LLVMAddIncoming(phi, &parent->value, &loop_block, 1);

    // Exit block
    LLVMPositionBuilderAtEnd(B, exit_block);
    LLVMValueRef result = LLVMBuildPhi(B, lltype(cmp), "inherit_result");
    LLVMAddIncoming(result, &cmp->value, &loop_block, 1);
    LLVMAddIncoming(result, &(LLVMValueRef){LLVMConstInt(LLVMInt1TypeInContext(a->module_ctx), 0, 0)}, &block, 1);

    //enode(mod, a, value, vr, au, m->au, loaded, true)
    Au_t au = (etypeid(bool))->au;

    return enode(mod, a, au, au, loaded, true, value, result);
}


// Comparison operator table entry
struct cmp_entry {
    OPType       optype;
    LLVMRealPredicate  fp_pred;
    LLVMIntPredicate   si_pred;  // signed int
    LLVMIntPredicate   ui_pred;  // unsigned int
    symbol       name;
};

static struct cmp_entry cmp_table[] = {
    { OPType__equal,      LLVMRealOEQ, LLVMIntEQ,  LLVMIntEQ,  "eq"  },
    { OPType__not_equal,  LLVMRealONE, LLVMIntNE,  LLVMIntNE,  "ne"  },
    { OPType__greater,    LLVMRealOGT, LLVMIntSGT, LLVMIntUGT, "gt"  },
    { OPType__greater_eq, LLVMRealOGE, LLVMIntSGE, LLVMIntUGE, "gte" },
    { OPType__less,       LLVMRealOLT, LLVMIntSLT, LLVMIntULT, "lt"  },
    { OPType__less_eq,    LLVMRealOLE, LLVMIntSLE, LLVMIntULE, "lte" },
};

static struct cmp_entry* cmp_lookup(OPType op) {
    for (int i = 0; i < sizeof(cmp_table) / sizeof(cmp_table[0]); i++)
        if (cmp_table[i].optype == op)
            return &cmp_table[i];
    return null;
}

enode aether_e_cmp_op(aether a, OPType optype, enode L, enode R) {
    a->is_const_op = false;
    etype bool_t = etypeid(bool);

    if (a->no_build) return e_noop(a, bool_t);

    // normalize operands to common arithmetic type BEFORE any comparison
    // result is always bool, but operands must match each other
    etype operand_type = determine_rtype(a, OPType__add, (etype)L, (etype)R);
    L = e_create(a, operand_type, (Au)L);
    R = e_create(a, operand_type, (Au)R);
    
    struct cmp_entry* cmp = cmp_lookup(optype);
    verify(cmp, "invalid comparison operator");

    char N[64];
    sprintf(N, "actual_op_%i_%s", seq, cmp->name);

    // 1. Primitive → primitive fast path
    if (is_prim(L) && is_prim(R)) {
        if (is_realistic(L) || is_realistic(R)) {
            return value(bool_t,
                LLVMBuildFCmp(B, cmp->fp_pred,
                              L->value, R->value, N));
        }
        LLVMIntPredicate pred = (is_unsign(L) && is_unsign(R)) ? cmp->ui_pred : cmp->si_pred;
        return value(bool_t,
            LLVMBuildICmp(B, pred,
                          L->value, R->value, N));
    }

    if (L->au->is_system || R->au->is_system) {
        // pointer check for system identities
        return value(bool_t,
            LLVMBuildICmp(B, cmp->ui_pred, L->value, R->value, N));
    }

    // 2. If types differ, normalize the smaller to the larger
    if (L->au != R->au && (!L->au->is_system && !R->au->is_system)) {
        if (L->au->abi_size >= R->au->abi_size)
            R = e_create(a, (etype)L, (Au)R);
        else
            L = e_create(a, (etype)R, (Au)L);
    }

    // 3. Class → null check is pointer compare, otherwise use compare() method
    if (is_class(L) || is_class(R)) {
        bool L_null = LLVMIsNull(L->value);
        bool R_null = LLVMIsNull(R->value);
        if (L_null || R_null) {
            return value(bool_t,
                LLVMBuildICmp(B, cmp->ui_pred, L->value, R->value, N));
        }

        if (!is_class(L)) {
            enode tmp = L; L = R; R = tmp;
        }

        Au_t fn = find_member(L->au, "compare", AU_MEMBER_FUNC, 0, true);
        verify(fn, "class %s has no compare() method", L->au->ident);

        enode cmp_result = e_fn_call(a, (efunc)u(enode, fn), a(L, R));
        LLVMValueRef zero = LLVMConstInt(lltype(cmp_result), 0, false);

        return value(bool_t,
            LLVMBuildICmp(B, cmp->si_pred,
                          cmp_result->value, zero, N));
    }

    // 4. Struct → recursively compare members (only for eq/ne)
    // Q is this the only case we can handle loaded:false structs?  also one may be and one may not be!
    if (is_struct(L) && is_struct(R) && (optype == OPType__equal || optype == OPType__not_equal)) {
        verify(L->au == R->au, "struct type mismatch");
        bool L_ptr = !L->loaded;
        bool R_ptr = !R->loaded;
        etype L_type = canonical(L);
        etype R_type = canonical(R);
        LLVMValueRef accum = LLVMConstInt(LLVMInt1TypeInContext(a->module_ctx), 1, false);

        for (int i = 0; i < L->au->members.count; i++) {
            Au_t m = (Au_t)L->au->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR)
                continue;
            evar mem = u(evar, m);
            etype mt = u(etype, mem->au->src);

            LLVMValueRef lv = L_ptr ?
                LLVMBuildLoad2(B, lltype(mt),
                    LLVMBuildStructGEP2(B, lltype(L_type), L->value, m->index, "l_gep"), "l_mem") :
                LLVMBuildExtractValue(B, L->value, m->index, "");
            
            LLVMValueRef rv = R_ptr ?
                LLVMBuildLoad2(B, lltype(mt),
                    LLVMBuildStructGEP2(B, lltype(R_type), R->value, m->index, "r_gep"), "r_mem") :
                LLVMBuildExtractValue(B, R->value, m->index, "");

            enode le = value(u(etype, m), lv);
            enode re = value(u(etype, m), rv);

            enode sub = aether_e_cmp_op(a, OPType__equal, le, re);
            accum = LLVMBuildAnd(B, accum, sub->value, "and");
        }

        // For not_equal, invert the result
        if (optype == OPType__not_equal)
            accum = LLVMBuildNot(B, accum, "not");

        return value(bool_t, accum);
    }

    // 5. Fallback for primitives
    if (is_realistic(L)) {
        return value(bool_t,
            LLVMBuildFCmp(B, cmp->fp_pred,
                          L->value, R->value, N));
    }

    LLVMIntPredicate pred = (is_unsign(L) && is_unsign(R)) ? cmp->ui_pred : cmp->si_pred;
    return value(bool_t,
        LLVMBuildICmp(B, pred, L->value, R->value, N));
}

enode aether_e_eq(aether a, enode L, enode R) {
    a->is_const_op = false;
    etype bool_t = etypeid(bool);

    if (a->no_build) return e_noop(a, bool_t);

    // 1. Primitive → primitive fast path
    if (is_prim(L) && is_prim(R)) {
        if (is_realistic(L) || is_realistic(R)) {
            // floating compare
            return value(bool_t,
                LLVMBuildFCmp(B, LLVMRealOEQ,
                              L->value, R->value, "eq-f"));
        }

        return value(bool_t,
            LLVMBuildICmp(B, LLVMIntEQ,
                          L->value, R->value, "eq-i"));
    }

    // 2. If types differ, normalize the smaller to the larger
    if (L->au != R->au) {
        if (L->au->abi_size >= R->au->abi_size)
            R = e_create(a, (etype)L, (Au)R);
        else
            L = e_create(a, (etype)R, (Au)L);
    }

    // 3. Class → use compare() method
    if (is_class(L) || is_class(R)) {
        // reorder so L is the class (if needed)
        if (!is_class(L)) {
            enode tmp = L; L = R; R = tmp;
        }

        Au_t fn = find_member(L->au, "compare", AU_MEMBER_FUNC, 0, true);
        verify(fn, "class %s has no compare() method", L->au->ident);

        enode cmp = e_fn_call(a, u(efunc, fn), a(L, R));

        // compare result against zero (i64 or i32)
        LLVMValueRef zero = LLVMConstInt(lltype(cmp), 0, false);

        return value(bool_t,
            LLVMBuildICmp(B, LLVMIntEQ,
                          cmp->value, zero, "cmp-zero"));
    }

    // 4. Struct → recursively compare members
    if (is_struct(L) && is_struct(R)) {
        verify(L->au == R->au, "struct type mismatch");

        LLVMValueRef accum =
            LLVMConstInt(LLVMInt1TypeInContext(a->module_ctx), 1, false);

        for (int i = 0; i < L->au->members.count; i++) {
            Au_t m = (Au_t)L->au->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR)
                continue;

            LLVMValueRef lv =
                LLVMBuildExtractValue(B, L->value, m->index, "");
            LLVMValueRef rv =
                LLVMBuildExtractValue(B, R->value, m->index, "");

            enode le = value(u(etype, m), lv);
            enode re = value(u(etype, m), rv);

            enode sub = aether_e_cmp_op(a, OPType__equal, le, re);

            accum = LLVMBuildAnd(B, accum, sub->value, "and");
        }

        return value(bool_t, accum);
    }

    // 5. Fallback for all primitives
    if (is_realistic(L))
        return enode(mod, a, au, bool_t->au, loaded, true, value,
            LLVMBuildFCmp(B, LLVMRealOEQ,
                          L->value, R->value, "eq-f"));

    LLVMValueRef nv = LLVMBuildICmp(B, LLVMIntEQ, L->value, R->value, "eq-i");
    return enode(mod, a, value, nv, loaded, true, au, bool_t->au);
}

etype etype_test(aether a) {
    etype str = etypeid(string);
    return str;
}


enode aether_e_eq_prev(aether a, enode L, enode R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));

    if (is_prim(L) && is_prim(R)) {
        // needs reality check
        enode        cmp = e_cmp(a, L, R);
        LLVMValueRef z   = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0);
        return value(etypeid(bool), LLVMBuildICmp(B, LLVMIntEQ, cmp->value, z, "cmp-i"));
    }

    // todo: bring this back in of course
    Au_t Lt = isa(L->literal);
    Au_t Rt = isa(R->literal);
    bool  Lc = is_class(L);
    bool  Rc = is_class(R);
    bool  Ls = is_struct(L);
    bool  Rs = is_struct(R);
    bool  Lr = is_realistic(L);
    bool  Rr = is_realistic(R);

    if (Lt && Rt) { // ifdef functionality
        bool Lr = (Lt->traits & AU_TRAIT_REALISTIC) != 0;
        bool Rr = (Rt->traits & AU_TRAIT_REALISTIC) != 0;
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
        enode res = value(etypeid(bool), LLVMConstInt(LLVMInt1TypeInContext(a->module_ctx), is_eq, 0));
        res->literal = hold(literal);
        return res;
    }

    if (Lc || Rc) {
        if (!Lc) {
            enode t = L;
            L = R;
            R = t;
        }
        Au_t au_f = find_member(L->au, "eq", AU_MEMBER_FUNC, 0, true);
        if (au_f) {
            efunc eq = (efunc)u(efunc, au_f);
            // check if R is compatible with argument
            // if users want to allow different data types, we need to make the argument more generic
            // this is better than overloads since the code is in one place
            return e_fn_call(a, eq, a(L, R));
        }
    } else if (Ls || Rs) {
        // iterate through struct members, checking against each with recursion
        LLVMValueRef result = LLVMConstInt(LLVMInt1TypeInContext(a->module_ctx), 1, 0);

        verify(Ls && Rs && (L->au == R->au),
            "struct type mismatch %o != %o", L->au, R->au);
        
        for (int i = 0; i < L->au->members.count; i++) {
            Au_t mem = (Au_t)L->au->members.origin[i];
            if (mem->member_type != AU_MEMBER_VAR)
                continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(B, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(B, R->value, mem->index, "rv");
            enode        le  = value(u(etype, mem), lv);
            enode        re  = value(u(etype, mem), rv);
            enode        cmp = e_cmp_op(a, OPType__equal, le, re);
            result = LLVMBuildAnd(B, result, cmp->value, "eq-struct-and");
        }
        return value(etypeid(bool), result);

    } else if (L->au != R->au) {
        if (Lr && Rr) {
            // convert to highest bit width
            if (L->au->abi_size > R->au->abi_size) {
                R = e_create(a, (etype)L, (Au)R);
            } else {
                L = e_create(a, (etype)R, (Au)L);
            }
        } else if (Lr) {
            R = e_create(a, (etype)L, (Au)R);
        } else if (Rr) {
            L = e_create(a, (etype)L, (Au)R);
        } else {
            if (L->au->abi_size > R->au->abi_size) {
                R = e_create(a, (etype)L, (Au)R);
            } else {
                L = e_create(a, (etype)R, (Au)L);
            }
        }
    }

    if (Lr || Rr)
        return value(etypeid(bool), LLVMBuildFCmp(B, LLVMRealOEQ, L->value, R->value, "eq-f"));
    
    return value(etypeid(bool), LLVMBuildICmp(B, LLVMIntEQ, L->value, R->value, "eq-i"));
}

static void print_all(aether mod, symbol label, array list);

enode aether_e_eval(aether a, string value) { sequencer
    array t = (array)tokens(target, (Au)a, parser, a->parse_f, input, (Au)value);
    push_tokens(a, (tokens)t, 0);
    enode n = (enode)a->parse_expr((Au)a, null); // this should not output i32 (2nd time)
    enode s = e_create(a, etypeid(string), (Au)n);
    pop_tokens(a, false);
    return s;
}


enode e_convert_or_cast(aether a, etype output, enode input);

enode aether_e_interpolate(aether a, string str) { sequencer
    enode accum = null;
    array sp    = split_parts(str);
    etype mdl   = etypeid(string);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);

    token statement = a->statement_origin;

    a->expr_level++;
    each (sp, ipart, s) {
        enode val;
        if (s->is_expr) {
            enode expr = e_eval(a, s->content);
            val = e_create(a, mdl, (Au)expr);
        } else {
            val = e_create(a, mdl, (Au)s->content);
        }
        accum = accum ? e_add(a, (Au)accum, (Au)val) : val;
    }
    a->expr_level--;
    a->statement_origin = statement;
    return accum;
}

static LLVMValueRef const_cstr(aether a, cstr value, i32 len) { sequencer
    // 1. make a constant array from the raw bytes (null-terminated!)
    LLVMValueRef strConst = LLVMConstStringInContext(a->module_ctx, value, len, /*DontNullTerminate=*/0);

    // 2. create a global variable of that array type
    char id[256];
    if (seq == 27) {
        seq = seq;
    }
    sprintf(id, "const_cstr_%i", seq);
    LLVMValueRef gv = LLVMAddGlobal(a->module_ref, LLVMTypeOf(strConst), id);
    LLVMSetInitializer(gv, strConst);
    LLVMSetGlobalConstant(gv, 1);
    LLVMSetLinkage(gv, LLVMPrivateLinkage);

    // 3. build a GEP to point at element [0,0] (first char)
    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0);
    LLVMValueRef idxs[] = {zero, zero};
    LLVMValueRef cast_i8 = LLVMConstGEP2(LLVMTypeOf(strConst), gv, idxs, 2);

    return cast_i8;
}

enode e_operand_primitive(aether a, Au op) {
    if (a->no_build) return e_noop(a, u(etype, isa(op)));

    Au_t t = isa(op);
         if (instanceof(op, enode) || instanceof(op, efunc)) return (enode)op;
    else if (instanceof(op, etype)) return e_typeid(a, (etype)op);
    else if (instanceof(op, handle)) {
        uintptr_t v = (uintptr_t)*(void**)op; // these should be 0x00, infact we may want to assert for this

        // create an i64 constant with that address
        LLVMValueRef ci = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), v, 0);

        // cast to a generic void* (i8*) pointer type
        LLVMTypeRef  hty = LLVMPointerTypeInContext(a->module_ctx, 0);
        LLVMValueRef cp  = LLVMConstIntToPtr(ci, hty);
        return enode(
            mod,     a,
            value,   cp,
            loaded,  true,
            au,      au_lookup("handle"),
            literal, op
        );
    }
    else if (Au_instance_of(op, typeid(bool))) return uint_value(8,  op);
    else if (Au_instance_of(op, typeid(u8)))  return uint_value(8,  op);
    else if (Au_instance_of(op, typeid(u16))) return uint_value(16, op);
    else if (Au_instance_of(op, typeid(u32))) return uint_value(32, op);
    else if (Au_instance_of(op, typeid(u64))) return uint_value(64, op);
    else if (Au_instance_of(op, typeid(i8)))  return  int_value(8,  op);
    else if (Au_instance_of(op, typeid(i16))) return  int_value(16, op);
    else if (Au_instance_of(op, typeid(i32))) return  int_value(32, op);
    else if (Au_instance_of(op, typeid(i64))) return  int_value(64, op);
    else if (Au_instance_of(op, typeid(sz)))  return  int_value(64, op); // instanceof is a bit broken here and we could fix the generic; its not working with aliases
    else if (Au_instance_of(op, typeid(f32))) return  f32_value(32, op);
    else if (Au_instance_of(op, typeid(f64))) return  f64_value(64, op);
    else if (Au_instance_of(op, typeid(symbol))) {
        return enode(mod, a, loaded, true,
            value, const_cstr(a, (cstr)op, strlen((symbol)op)),
            au, au_lookup("symbol"), literal, op);
    }
    else if (instanceof(op, string)) { // this works for const_string too
        string str = string(((string)op)->chars);
        return enode(
            mod,     a,
            loaded,  true,
            value,   const_cstr(a, str->chars, str->count),
            au,      au_lookup("symbol"),
            literal, op);
    }
    error("unsupported type in aether_operand %s", t->ident);
    return NULL;
}


enode aether_e_op(aether a, OPType optype, string op_name, Au L, Au R) { sequencer
    a->is_const_op = false; // we can be granular about this, but its just not worth the complexity for now
    enode mL = (enode)instanceof(L, enode);
    Au mL_info = head(mL);

    if (seq == 57) {
        seq = seq;
    }
    enode LV = e_operand(a, L, null);
    enode RV = e_operand(a, R, null);

    // check for R-side first (depends on where our object is)
    if (op_name && instanceof(R, enode) && is_rec(R)) {
        // try reverse operator: lmul, ldiv, etc.
        OPType reverse_op = 0;
        if      (optype == OPType__mul) reverse_op = OPType__lmul;
        else if (optype == OPType__div) reverse_op = OPType__ldiv;
        else if (optype == OPType__left) reverse_op = OPType__lleft;
        else if (optype == OPType__right) reverse_op = OPType__lright;

        if (reverse_op) {
            etype rec_r = u(etype, is_rec(R));
            if (rec_r) {
                etype Rt = null;
                for (int i = 0; i < rec_r->au->members.count; i++) {
                    Au_t mem = (Au_t)rec_r->au->members.origin[i];
                    if (mem->operator_type == reverse_op) {
                        Rt = u(etype, mem);
                        break;
                    }
                }
                if (Rt) {
                    verify(Rt->au->args.count == 2,
                        "expected 2 arguments for reverse operator method");
                    return e_fn_call(a, (efunc)Rt, a(RV, LV));  // note: R is self, L is the arg
                }
            }
        }
    }

    // check for overload
    if (op_name && instanceof(L, enode) && is_rec(L)) {
        enode Ln = (enode)L;
        etype rec  = u(etype, is_rec(L));
        if (rec) {
            etype Lt = null;
            for (int i = 0; i < rec->au->members.count; i++) {
                Au_t mem = (Au_t)rec->au->members.origin[i];
                if (mem->operator_type == optype) {
                    verify(u(etype, mem), "expected user structure for member %s", mem->ident);
                    Lt = u(etype, mem);
                    break;
                }
            }
            if  (Lt) {
                verify(Lt->au->args.count == 2,
                    "expected 2 argument for operator method");
                return e_fn_call(a, (efunc)Lt, a(mL, RV));
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    enode Lnode = (enode)L;
    etype rtype = determine_rtype(a, optype, (etype)LV, (etype)RV); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too

    if (seq == 9) {
        seq = seq;
    }
    LLVMValueRef RES;
    enode LL = (enode)LV; // optype <= OPType__assign ? LV : e_create(a, rtype, (Au)LV); // we dont need the 'load' in here, or convert even
    enode RL = (enode)RV; // e_create(a, rtype, (Au)RV);
    symbol N = cstring(f(string, "%i_%o", seq, op_name));
    Au literal = null;

    if (optype >= OPType__add && optype <= OPType__xor) { // logical operators

        if (optype == OPType__xor) {
            optype = optype;
        }
        // ensure both operands are i1
        if (optype == OPType__or || optype == OPType__and) {
            rtype = etypeid(bool);
        }
        LL = e_create(a, rtype, (Au)LL);
        RL = e_create(a, rtype, (Au)RL);
        if (!a->no_build) {
            RES = build_arith_op(a, B, optype, LL->value, RL->value, N, is_sign(rtype));
        }

    } else if (optype >= OPType__add && optype <= OPType__left) {
        LL = e_create(a, rtype, (Au)LL); // generate compare != 0 if not already i1
        RL = e_create(a, rtype, (Au)RL);

        if (!a->no_build) {

            if (strcmp(N, "7_add") == 0) {
                N = N;
            }

            RES = build_arith_op(a, B, optype, LL->value, RL->value, N, is_sign(rtype));
        }
 
    } else if (optype >= OPType__bind && optype <= OPType__assign_left) {

        // assignments perform a store
        verify(mL, "left-hand operator must be a emember");
        if (!a->no_build) {
            
            // already computed in R-value
            if (optype <= OPType__assign) {
                RES = a->no_build ? null : RL->value;
                literal = RL->literal;
            } else {
                // map compound assign op to its base arithmetic op
                OPType base_op;
                switch (optype) {
                    case OPType__assign_add:   base_op = OPType__add;         break;
                    case OPType__assign_sub:   base_op = OPType__sub;         break;
                    case OPType__assign_mul:   base_op = OPType__mul;         break;
                    case OPType__assign_div:   base_op = OPType__div;         break;
                    case OPType__assign_or:    base_op = OPType__bitwise_or;  break;
                    case OPType__assign_and:   base_op = OPType__bitwise_and; break;
                    case OPType__assign_xor:   base_op = OPType__xor;         break;
                    case OPType__assign_mod:   base_op = OPType__mod;         break;
                    case OPType__assign_right: base_op = OPType__right;       break;
                    case OPType__assign_left:  base_op = OPType__left;        break;
                    default: verify(false, "invalid compound assignment"); base_op = 0;
                }
                RES = a->no_build ? null :
                    build_arith_op(a, B, base_op, LL->value, RL->value, N, is_sign(rtype));
            }
            LLVMBuildStore(B, RES, mL->value);
        } else {
            if (optype <= OPType__assign)
                literal = RL->literal;
        }
    } else if (optype == OPType__compare) {
        return e_cmp(a, LL, RL);
    } else {
        verify(optype >= OPType__equal && optype <= OPType__less_eq, "invalid comparison operation");
        return e_cmp_op(a, optype, LL, RL);
    }
    return enode(
        mod,        a,
        au,         rtype->au,
        literal,    literal,
        loaded,     true,
        value,      RES);
}

#define evar_is(n, id) (n->au && n->au->ident && strcmp(n->au->ident, id) == 0)

static evar lookup_by_unique_type(aether a, Au_t required_type) {
    evar match = null;
    int  count = 0;
    
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        
        members(ctx, mem) {
            if (mem->member_type != AU_MEMBER_VAR) continue;
            evar v = u(evar, mem);
            if (!v) continue;
            Au_t var_type = au_arg_type((Au)mem);
            if (var_type == required_type || inherits(var_type, required_type)) {
                match = v;
                count++;
            }
        }
    }
    
    verify(count <= 1,
        "ambiguous context: found %i variables of type %s in scope, explicit binding required",
        count, required_type->ident);
    
    return match;
}

// resolve all context members on a newly allocated instance 
// (before our post-init (init; the complexity of this term 
// ambiguates it as an actual solution to component programming 
// with the least amount of complexity))
// in short i think i am a fan of this since 
// its the one thing i am not moving away from after years-of-fiddling..

void aether_e_assign_if_null(aether a, enode prop, Au value) {
    LLVMValueRef      fn        = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
    LLVMBasicBlockRef assign_bb = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "assign_if");
    LLVMBasicBlockRef merge_bb  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "assign_merge");
    enode loaded   = enode_value(prop, false);
    LLVMValueRef is_null = LLVMBuildICmp(B, LLVMIntEQ,
        loaded->value, LLVMConstNull(LLVMTypeOf(loaded->value)), "is_null");
    LLVMBuildCondBr(B, is_null, assign_bb, merge_bb);
    LLVMPositionBuilderAtEnd(B, assign_bb);
    e_assign(a, prop, value, OPType__assign);
    LLVMBuildBr(B, merge_bb);
    LLVMPositionBuilderAtEnd(B, merge_bb);
}

static void resolve_context_members(enode target, map user_provides) {
    aether a = target->mod;
    if (a->no_build)
        return;

    // canonical resolves types from enode, as well as dissolves any aliases
    Au_t type = canonical(target)->au;

    if (!is_rec(target->au))
        return;
    
    do {
        members(type, mem) {
            if (mem->member_type != AU_MEMBER_VAR || !mem->is_context)
                continue;
            
            enode found     = user_provides ? (enode)get(user_provides, (Au)string(mem->ident)) : null;
            enode scope_var = null;
            Au_t  required_type = mem->src;

            if (!found) {
                scope_var = (enode)lookup_by_unique_type(a, required_type);
                verify(scope_var,
                    "required context member '%s' of type '%s' not found in scope",
                    mem->ident, required_type->ident);
            } else {
                etype ctx = u(etype, required_type);
                verify(canonical(found) == canonical(ctx),
                    "user provided context %o not compatible with %o", found, ctx);
            }

            // if prop is already set, we should not overwrite it!.. this means some form of branching here
            enode prop = access(target, string(mem->ident), true);
            enode selected = found ? found : scope_var;

            e_assign_if_null(a, prop, (Au)selected);
        }
        type = type->context;

    } while (type && type != typeid(Au));
}

enode aether_e_operand(aether a, Au op, etype src_model) {
    if (instanceof(op, enode) || instanceof(op, efunc)) {
        enode n = (enode)op;
        op = (Au)enode_value((enode)op, false);
    }
    if (!op) {
        if (is_ptr(src_model) || !src_model)
            return e_null(a, src_model);

        return enode(mod, a, au, src_model->au,
            loaded, true, value, LLVMConstNull(lltype(src_model)));
    }
    
    if (isa(op) == typeid(string))
        return e_create(a, src_model,
            (Au)e_interpolate(a, (string)op));
    
    if (isa(op) == typeid(const_string))
        return e_create(a, src_model, (Au)e_operand_primitive(a, op)); //e_create(a, src_model, (Au)e_operand_primitive(a, op));
    
    if (isa(op) == typeid(map)) {
        return e_create(a, src_model, op);
    }

    //if (src_model == etypeid(i64)) {
    //    return e_create(a, src_model, op);
    //}
    Au_t op_isa = isa(op);
    if (instanceof(op, array)) {
        verify(src_model != null, "expected src_model with array data");
        return e_create(a, src_model, op);
    }

    enode r = e_operand_primitive(a, op);
    return src_model ? e_create(a, src_model, (Au)r) : r;
}

enode aether_e_null(aether a, etype mdl) {
    if (!mdl) mdl = etypeid(handle);
    if (!is_ptr(mdl) && mdl->au->is_class) {
        mdl = etype_ptr(a, mdl->au, null); // we may only do this on a limited set of types, such a struct types
    }
    verify(is_ptr(mdl) || is_func_ptr(mdl), "%o not compatible with null value", mdl);
    return enode(mod, a, loaded, true, value, LLVMConstNull(lltype(mdl)), au, mdl->au);
}

enode aether_e_primitive_convert(aether a, enode expr, etype rtype);

/// copy constant primitive data into a vector allocated by e_vector
/// vec is the result of e_vector (points to data area), const_data is a global constant array
none aether_e_vector_init(aether a, etype element_type, enode vec, array nodes) {
    a->is_const_op = false;
    if (a->no_build) return;
 
    int ln = len(nodes);
    if (ln == 0) return;
 
    /// check if all elements are constant
    bool all_const = true;
    for (int i = 0; i < ln; i++) {
        enode node = (enode)nodes->origin[i];
        if (!node->literal) { all_const = false; break; }
    }
 
    if (all_const) {
        /// build a global constant array and memcpy into the vector
        LLVMValueRef *elems = malloc(sizeof(LLVMValueRef) * ln);
        for (int i = 0; i < ln; i++) {
            enode node = (enode)nodes->origin[i];
            elems[i] = node->value;
        }
        LLVMValueRef const_arr = LLVMConstArray(lltype(element_type), elems, ln);
        free(elems);
 
        static int ident = 0;
        char gname[32];
        sprintf(gname, "new_const_%i", ident++);
        LLVMValueRef G = LLVMAddGlobal(a->module_ref, LLVMTypeOf(const_arr), gname);
        LLVMSetLinkage(G, LLVMInternalLinkage);
        LLVMSetGlobalConstant(G, 1);
        LLVMSetInitializer(G, const_arr);
 
        /// memcpy from constant global into allocated vector data
        LLVMTypeRef i8ptr = LLVMPointerTypeInContext(a->module_ctx, 0);
        LLVMValueRef dst  = LLVMBuildBitCast(B, vec->value, i8ptr, "vec.dst");
        LLVMValueRef src  = LLVMBuildBitCast(B, G, i8ptr, "const.src");
        u64 byte_size = (u64)ln * (element_type->au->abi_size / 8);
        LLVMValueRef sz   = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), byte_size, 0);
        LLVMBuildMemCpy(B, dst, 0, src, 0, sz);
    } else {
        /// store each element individually
        for (int i = 0; i < ln; i++) {
            enode node = (enode)nodes->origin[i];
            LLVMValueRef idx = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), i, 0);
            LLVMValueRef gep = LLVMBuildGEP2(B, lltype(element_type), vec->value, &idx, 1, "elem.ptr");
            LLVMBuildStore(B, node->value, gep);
        }
    }
}

enode aether_e_meta_ids(aether a, Au meta_a, Au meta_b) {
    Au_t atype = au_lookup("Au_t");
    etype atype_vector = etype_ptr(a, atype, null);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, atype_vector);

    if (!meta_a && !meta_b)
        return e_null(a, atype_vector);

    // convert meta_a to enode (Au_t or etype -> typeid enode)
    enode meta_a_node = null;
    if (meta_a) {
        if (instanceof(meta_a, enode))
            meta_a_node = (enode)meta_a;
        else if (instanceof(meta_a, etype))
            meta_a_node = e_typeid(a, (etype)meta_a);
        else
            meta_a_node = e_typeid(a, u(etype, (Au_t)meta_a));
    }

    // convert meta_b to enode (shape, etype, Au_t -> enode)
    enode meta_b_node = null;
    if (meta_b) {
        if (instanceof(meta_b, enode)) {
            meta_b_node = (enode)meta_b;
        } else if (instanceof(meta_b, shape)) {
            shape s = (shape)meta_b;
            array ar = array(alloc, s->count);
            for (int ii = 0; ii < s->count; ii++)
                push(ar, _i64(s->data[ii]));
            meta_b_node = e_create(a, etypeid(shape), (Au)m(
                "count", _i64(s->count),
                "data",  e_const_array(a, etypeid(i64), ar)
            ));
        } else if (instanceof(meta_b, etype)) {
            meta_b_node = e_typeid(a, (etype)meta_b);
        } else {
            meta_b_node = e_typeid(a, u(etype, (Au_t)meta_b));
        }
    }

    enode metas[] = { meta_a_node, meta_b_node };
    LLVMTypeRef elemTy = lltype(atype_vector);
    LLVMTypeRef arrTy  = LLVMArrayType(elemTy, 2);
    LLVMValueRef arr_alloc = LLVMBuildAlloca(B, arrTy, "meta_ids");
    LLVMBuildStore(B, LLVMConstNull(arrTy), arr_alloc);

    for (int i = 0; i < 2; i++) {
        if (!metas[i]) continue;
        LLVMValueRef idxs[] = {
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0),
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), i, 0)
        };
        LLVMValueRef gep = LLVMBuildGEP2(B, arrTy, arr_alloc, idxs, 2, "");
        LLVMBuildStore(B, metas[i]->value, gep);
    }

    return enode(mod, a, loaded, true, value, arr_alloc, au, etype_ptr(a, atype, null)->au);
}

none aether_e_cond_return(aether a, enode cond, Au value) {
    a->is_const_op = false;
    if (a->no_build) return;

    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);
    LLVMBasicBlockRef ret   = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "cond.ret");
    LLVMBasicBlockRef cont  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "cond.cont");

    // branch: if cond is true, return; otherwise continue
    LLVMValueRef test = e_create(a, etypeid(bool), (Au)cond)->value;
    LLVMBuildCondBr(B, test, ret, cont);

    // ---- return block ----
    LLVMPositionBuilderAtEnd(B, ret);
    e_fn_return(a, value);

    // ---- continue block ----
    LLVMPositionBuilderAtEnd(B, cont);
    // builder is now positioned here, execution continues
}

enode aether_e_expect(aether a, enode cond, string msg) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));

    // convert condition to bool
    LLVMValueRef test = e_create(a, etypeid(bool), (Au)cond)->value;

    // store result in a debug-visible variable
    static int expect_seq = 0;
    char name[64];
    snprintf(name, sizeof(name), "_expect_%d", expect_seq++);
    LLVMValueRef alloc = LLVMBuildAlloca(B, LLVMInt1TypeInContext(a->module_ctx), name);
    LLVMBuildStore(B, test, alloc);

    // branch: if false, trap; otherwise continue
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);
    LLVMBasicBlockRef fail  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "expect.fail");
    LLVMBasicBlockRef cont  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "expect.cont");
    LLVMBuildCondBr(B, test, cont, fail);

    // ---- fail block: trap ----
    LLVMPositionBuilderAtEnd(B, fail);
    unsigned trap_id = LLVMLookupIntrinsicID("llvm.debugtrap", 15);
    LLVMValueRef trap_fn = LLVMGetIntrinsicDeclaration(a->module_ref, trap_id, null, 0);
    LLVMTypeRef  trap_ty = LLVMFunctionType(LLVMVoidTypeInContext(a->module_ctx), null, 0, 0);
    LLVMBuildCall2(B, trap_ty, trap_fn, null, 0, "");
    LLVMBuildBr(B, cont); // continue after trap (debugger can resume)

    // ---- continue block ----
    LLVMPositionBuilderAtEnd(B, cont);

    return e_noop(a, etypeid(bool));
}

catcher context_catcher(aether a);


enode enode_super(etype n, enode efn) {
    aether a = n->mod;
    return enode(
        mod, (aether)a, au, n->au->context, avoid_ftable, true, target, efn, is_super, true, loaded, true);
}

bool aether_e_fn_return(aether a, Au o) {
    efunc f = context_func(a);
    verify (f, "function not found in context");

    a->is_const_op = false;
    catcher cat = context_catcher(a);

    if (a->no_build) return cat && cat->rtype;

    // set debug location for return statement
    debug_emit(a);

    if (cat && cat->rtype) {
        enode conv = e_create(a, cat->rtype, o);
        LLVMAddIncoming(cat->phi->value, &conv->value, 
            &(LLVMBasicBlockRef){LLVMGetInsertBlock(B)}, 1);
        LLVMBuildBr(B, cat->block);
        return true;
    } else {
        f->au->has_return = true;

        if (a->timing_enabled && f->timing_start_value) {
            // use last statement's debug loc for epilogue, not stale peek
            debug_emit(a);
            emit_func_timing_end(a, f->timing_start_value, f->timing_func_id);
        }

        if (is_void(f->au->rtype))
            LLVMBuildRetVoid(B);
        else {
            enode conv = e_create(a, (etype)u(etype, f->au->rtype), o);
            LLVMBuildRet(B, enode_value(conv, true)->value);
        }
        return false;
    }
}

etype formatter_type(aether a, cstr input) {
    cstr p = input;
    // skip flags/width/precision
    while (*p && strchr("-+ #0123456789.", *p)) p++;
    if (strncmp(p, "lld", 3) == 0 || 
        strncmp(p, "lli", 3) == 0) return etypeid(i64);
    if (strncmp(p, "llu", 3) == 0) return etypeid(u64);
    
    switch (*p) {
        case 'd': case 'i':
                  return etypeid(i32); 
        case 'u': return etypeid(u32);
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                  return etypeid(f64);
        case 's': return etypeid(symbol);
        case 'p': return u(etype, etypeid(symbol)->au->ptr);
    }
    fault("formatter implementation needed");
    return null;
}

static etype func_target(etype fn) {
    aether a = fn->mod;
    if (fn->au->is_imethod)
        return u(etype, fn->au->context);
    
    return null;
}

// often we have either a struct or a class* 
// and we want to ensure a pointer to either one, in-order to make a target
static etype ensure_pointer(etype t) {
    if (!t || t->au->is_pointer || t->au->is_class) return t;
    return etype_ptr(t->mod, t->au, null);
}

enode aether_e_is(aether a, enode L, Au R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));

    enode L_type =  e_offset(a, L, _i64(-sizeof(struct _Au)));
    enode L_ptr  =    e_load(a, L_type, null); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = e_operand(a, R, null);
    return aether_e_eq(a, L_ptr, R_ptr);
}

bool etype_inherits(etype mdl, etype base);


// short-circuit two already-evaluated bool enodes
// combine: OPType__or  -> if L true, skip R  (returns true early)
//          OPType__and -> if L false, skip R  (returns false early)
enode e_short_circuit_pair(aether a, OPType combine, enode L, enode R) {
    etype m_bool = etypeid(bool);
    if (a->no_build) return e_noop(a, m_bool);
    
    enode L_bool = e_create(a, m_bool, (Au)L);
    enode R_bool = e_create(a, m_bool, (Au)R);
    
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
    
    LLVMBasicBlockRef eval_block  = LLVMAppendBasicBlockInContext(a->module_ctx, func, "hat_eval");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(a->module_ctx, func, "hat_merge");
    LLVMBasicBlockRef L_block     = LLVMGetInsertBlock(B);
    
    if (combine == OPType__or) {
        LLVMBuildCondBr(B, L_bool->value, merge_block, eval_block);
    } else {
        LLVMBuildCondBr(B, L_bool->value, eval_block, merge_block);
    }
    
    // R side - re-emit R_bool in the eval block
    LLVMPositionBuilderAtEnd(B, eval_block);
    LLVMBasicBlockRef R_block = LLVMGetInsertBlock(B);
    LLVMBuildBr(B, merge_block);
    
    // merge
    LLVMPositionBuilderAtEnd(B, merge_block);
    LLVMValueRef phi = LLVMBuildPhi(B, LLVMInt1TypeInContext(a->module_ctx), "hat_result");
    LLVMValueRef      vals[] = { L_bool->value, R_bool->value };
    LLVMBasicBlockRef bbs[]  = { L_block,       R_block };
    LLVMAddIncoming(phi, vals, bbs, 2);
    
    return enode(mod, a, au, m_bool->au, loaded, true, value, phi);
}

// no disassemble!
enode aether_e_short_circuit(aether a, OPType optype, enode L) {
    if (a->no_build) {
        enode R = (enode)a->parse_expr((Au)a, null);
        return e_noop(a, (etype)evar_type((evar)L));
    }
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
    
    // figure out common type and convert L before branching
    etype rtype = (etype)evar_type((evar)L);
    etype m_bool = etypeid(bool);
    enode L_conv = e_create(a, rtype, (Au)L);
    enode L_bool = e_create(a, m_bool, (Au)L);

    LLVMBasicBlockRef eval_r_block = LLVMAppendBasicBlockInContext(a->module_ctx, func, "eval_r");
    LLVMBasicBlockRef merge_block  = LLVMAppendBasicBlockInContext(a->module_ctx, func, "merge");
    LLVMBasicBlockRef L_final      = LLVMGetInsertBlock(B);

    if (optype == OPType__or) {
        LLVMBuildCondBr(B, L_bool->value, merge_block, eval_r_block);
    } else {
        LLVMBuildCondBr(B, L_bool->value, eval_r_block, merge_block);
    }

    // eval R block
    LLVMPositionBuilderAtEnd(B, eval_r_block);
    enode R = (enode)a->parse_expr((Au)a, null);
    enode R_conv = e_create(a, rtype, (Au)R);
    LLVMBasicBlockRef R_block = LLVMGetInsertBlock(B);
    LLVMBuildBr(B, merge_block);

    // merge block with PHI - returning actual values, not bools
    LLVMPositionBuilderAtEnd(B, merge_block);
    LLVMValueRef phi = LLVMBuildPhi(B, lltype(rtype), "sc_result");

    LLVMValueRef      vals[] = { L_conv->value, R_conv->value };
    LLVMBasicBlockRef bbs[]  = { L_final,       R_block };
    LLVMAddIncoming(phi, vals, bbs, 2);
    
    return enode(
        mod,    a,
        au,     rtype->au,
        loaded, true,
        value,  phi);
}

bool inherits(Au_t src, Au_t check);

none enode_inspect(enode a) {
}

enode aether_lambda_fcall(aether a, efunc mem, array user_args) {
    // access fn and ctx from lambda instance
    efunc fn_ptr     = (efunc)enode_value(access(mem, string("fn"), false), false);
    enode ctx_ptr    = enode_value(access(mem, string("context"), false), false); // we now have target inside of context
    etype rtype      = u(etype, mem->meta_a);
    enode lambda_fn  = u(enode, mem->au->src);

    array args = array(alloc, 32, assorted, true);

    push(args, (Au)ctx_ptr);
    each(user_args, Au, arg)
        push(args, arg);
    
    fn_ptr->meta_a = hold(mem->meta_a);
    fn_ptr->meta_b = hold(mem->meta_b);
    return e_fn_call(a, fn_ptr, args);
}

enode aether_e_fn_call(aether a, efunc fn, array args) { sequencer // 613 @ 87
    bool funcptr = is_func_ptr((Au)fn);
    if (!funcptr && !fn->used)
         fn->used = true;

    Au_t au = fn->au;

    if (strcmp(fn->au->ident, "push") == 0) {
        seq = seq;
    }

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, u(etype, fn->au->rtype));

    // set debug location for call site
    debug_emit(a);

    etype_implement((etype)fn, false);
    
    Au    arg0        = args ? get(args, 0) : null;
    enode first_arg   = arg0 ? e_operand(a, arg0, null) : null;
    enode target_type = (!funcptr && fn->target) ? (enode)fn->target : null;
    bool  first_is_alloc = first_arg && first_arg->is_alloc && !first_arg->is_super;

    if (target_type && first_arg) {
        // only check needed: is first_arg's pointer state compatible with target
        Au info_target_instance = head(arg0);
        bool arg_is_ptr    = is_ptr((Au)first_arg) || !first_arg->loaded;
        bool target_is_ptr = is_ptr((Au)target_type) || !target_type->loaded;
        verify(arg_is_ptr == target_is_ptr, "target pointer mismatch on %o %i", fn, seq);
    }

    bool is_Au = !target_type || inherits(au_arg_type((Au)target_type->au), typeid(Au));

    int n_args = args ? len(args) : 0;
    
    verify(funcptr || n_args == fn->au->args.count ||
        (n_args > fn->au->args.count && fn->au->is_vargs),
        "arg count mismatch on %o", fn);
    
    Au_t type_id = isa(fn);
    LLVMValueRef* arg_values = calloc(n_args, sizeof(LLVMValueRef));
    LLVMTypeRef*  arg_types  = calloc(n_args, sizeof(LLVMValueRef));
    LLVMTypeRef   F          = funcptr ? null : lltype(fn);
    LLVMValueRef  V          = fn->value;

    bool is_abstract = fn->au->is_abstract;
    Au_t ctx = fn->au->context;
    while (ctx) {
        Au_t m = find_member(ctx, fn->au->ident, 0, 0, false);
        if (m && m->is_abstract) {
            is_abstract = true;
            break;
        }
        if (ctx == typeid(Au))
            break;
        ctx = ctx->context;
    }
    // -----------------------------------------------------------------------
    // Dynamic Dispatch Logic                           ( hold onto your butts )
    // -----------------------------------------------------------------------
    // If this is an instance method, we must look up the implementation 
    // in the runtime type's vtable (ft) rather than using the static symbol.
    if (is_abstract || (!a->direct && target_type && target_type->is_any && !first_is_alloc && 
         fn->au->context != typeid(Au) && fn->au->is_imethod && !funcptr && 
         !(target_type && target_type->target && target_type->target->avoid_ftable))) {
        verify(n_args > 0, "instance method %o requires 'this' argument", fn);
        
        // 1. Get the instance pointer ('this' is always the first argument)
        // We need the raw value to perform pointer arithmetic
        Au    arg0       = get(args, 0);
        enode first_arg  = e_operand(a, arg0, null);
        LLVMValueRef instance = first_arg->value;

        // 2. Get the Object Header
        // The ABI defines the header as residing immediately before the object pointer.
        // header = ((struct _Au*)instance) - 1
        LLVMTypeRef  i8_ptr_ty  = LLVMPointerTypeInContext(a->module_ctx, 0);
        LLVMValueRef i8_this    = LLVMBuildBitCast(B, instance, i8_ptr_ty, "this_i8");
        
        // Calculate offset: -sizeof(struct _Au)
        // We use the compiler's knowledge of the struct size to burn the constant into IR
        LLVMValueRef offset_neg = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), -(int)sizeof(struct _Au), true);
        LLVMValueRef header_ptr = LLVMBuildGEP2(B, LLVMInt8TypeInContext(a->module_ctx), i8_this, &offset_neg, 1, "header_ptr");

        // 3. Load the Runtime Type (Au_t)
        // The 'type' is the first member of struct _Au
        LLVMTypeRef  Au_t_ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0); // void** (pointer to type pointer)
        LLVMValueRef header_cast = LLVMBuildBitCast(B, header_ptr, Au_t_ptr_ty, "header_cast");
        LLVMValueRef type_ptr    = LLVMBuildLoad2(B, i8_ptr_ty, header_cast, "type_ptr");

        // 4. Access the Function Table (ft)
        // The function table is located at `offsetof(struct _Au_f, ft)` within the type object.
        LLVMValueRef type_i8     = LLVMBuildBitCast(B, type_ptr, i8_ptr_ty, "type_i8");
        LLVMValueRef ft_offset   = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), offsetof(struct _Au_f, ft), false);
        LLVMValueRef ft_base     = LLVMBuildGEP2(B, LLVMInt8TypeInContext(a->module_ctx), type_i8, &ft_offset, 1, "ft_base");

        // 5. Index into the Function Table
        // Retrieve the specific method index assigned during schema creation
        int method_index = fn->au->index; 
        Au f_info = head(fn);
        verify(method_index > 0, "method %o has invalid vtable index", fn);

        LLVMValueRef ft_array      = LLVMBuildBitCast(B, ft_base, Au_t_ptr_ty, "ft_array");
        LLVMValueRef method_idx    = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), method_index, false);
        LLVMValueRef func_ptr_addr = LLVMBuildGEP2(B, i8_ptr_ty, ft_array, &method_idx, 1, "func_ptr_addr");
        
        // 6. Load and Cast the Function Pointer
        LLVMValueRef func_ptr_void = LLVMBuildLoad2(B, i8_ptr_ty, func_ptr_addr, "func_ptr_void");
        
        // Cast the generic void* from vtable to the specific function signature we expect
        V = LLVMBuildBitCast(B, func_ptr_void, LLVMTypeOf(fn->value), "vmethod");
    }

    int index = 0;
    if (target_type) {
        // we do not 'need' target so we dont have it.
        // we get instance from the fact that its within a class/struct context and is_imethod
        Au_t au_type = target_type->au;
        if (au_type->member_type == AU_MEMBER_VAR) au_type = au_type->src;

        etype cast_to = ensure_pointer(u(etype, au_type));
        arg_values[index++] = e_create(a, cast_to, (Au)first_arg)->value;
    }

    int fmt_idx = -1;
    int arg_idx = -1;
    for (int i = target_type ? 1 : 0; i < fn->au->args.count; i++) {
        Au_t arg = (Au_t)fn->au->args.origin[i];
        if (arg->is_formatter) {
            fmt_idx = i;
            arg_idx = i + 1;
            break;
        }
    }

    if (args) {
        int i = 0;
        each (args, Au, arg_value) {
            if (target_type && i == 0) {
                i++;
                continue;
            }
            if (fmt_idx >= 0 && i > fmt_idx)
                break;

            if (is_lambda(fn)) {
                verify(instanceof(arg_value, enode), "expected enode for runtime function call");
                enode n = (enode)arg_value;
                verify(n->value, "expected enode value for arg %i", index);
                arg_values[index] = n->value;
                arg_types[index] = lltype(n);
            } else {
                Au_t  fn_decl = funcptr ? au_arg_type((Au)fn->au) : fn->au;
                Au_t  arg_mem = (Au_t) micro_get(&fn_decl->args, i);
                evar  arg     = (evar) u(evar, arg_mem);
                enode n       = (enode)instanceof(arg_value, enode);
                if (index == fmt_idx) {
                    Au_t au_str = isa(n->literal);
                    Au fmt = n ? (Au)instanceof(n->literal, const_string) : null;
                    verify(fmt, "formatter functions require literal, constant strings");
                }
                etype arg_t   = u(etype, au_arg_type((Au)arg_mem));
                enode conv    = e_create(a, arg_t, arg_value);

                if (arg_mem && arg_mem->is_inlay) // honor the inlay attribute; 'must pass by value and not just think we are'
                    conv = enode_value(conv, true);
                
                arg_values[index] = conv->value;
                if (funcptr)
                    arg_types[index] = lltype(arg_t);
            }
            i++;
            index++;
        }
    }
    int istart = index;
    
    if (fmt_idx >= 0) {
        Au_t   src_type = isa(args->origin[fmt_idx]);
        enode  fmt_node = instanceof(args->origin[fmt_idx], enode);
        string fmt_str  = (string)instanceof(fmt_node->literal, const_string);
        verify(fmt_str, "expected string literal at index %i for format-function: %o",
            fmt_idx, fn->au->ident);
        arg_values[fmt_idx] = fmt_node->value;
        int soft_args = 0;
        symbol p = fmt_str->chars;
        while  (p[0]) {
            if (p[0] == '%' && p[1] != '%') {
                etype arg_type  = formatter_type(a, (cstr)&p[1]);
                Au    o_arg     = args->origin[arg_idx + soft_args];
                enode n_arg     = e_operand(a, o_arg, null);
                enode conv      = e_create(a, arg_type, (Au)n_arg);
                arg_values[arg_idx + soft_args] = conv->value;
                soft_args++;
                index    ++;
                p += 2;
            } else
                p++;

        }
        verify((istart + soft_args) == len(args), "%o: formatter args mismatch", fn->au->ident);
    }
    
    bool is_void_ = is_void(fn->au->rtype);
    static int seq2 = 0;
    seq2++;

    char call_seq[256];
    sprintf(call_seq, "call_%i_%s", seq2, fn->au->ident);
    etype rtype = is_lambda(fn) ?
        u(etype, fn->meta_a) :
        u(etype, fn->au->rtype);
    
    if (funcptr) F = LLVMFunctionType(lltype(rtype), arg_types, n_args, false);
    
    LLVMValueRef R = LLVMBuildCall2(B, F, V, arg_values, index, is_void_ ? "" : call_seq);
    free(arg_values);
    free(arg_types);

    return enode(mod, a, au, rtype->au, loaded, true, value, R);
}

static bool is_same(etype a, etype b) {
    return resolve(a) == resolve(b) && ref_level((Au)a) == ref_level((Au)b);
}

enode castable(etype fr, etype to) { 
    aether a = fr->mod;
    bool fr_ptr = is_ptr(fr);
    if (((fr_ptr && !is_class(fr)) || is_prim(fr)) && is_bool(to))
        return (enode)true;

    /// compatible by match, or with basic integral/real types
    if ((fr == to) ||
        ((is_realistic(fr) && is_realistic(to)) ||
         (is_integral (fr) && is_integral (to))))
        return (enode)true;
    
    if (is_generic(fr) && is_class(to))
        return (enode)true;
    
    /// primitives may be converted to Au-type Au
    if (is_prim(fr) && is_generic(to))
        return (enode)true;

    /// check cast methods on from
    poly_members(fr->au, mem) {
        if (mem->member_type == AU_MEMBER_CAST && is_same(u(etype, mem->rtype), to))
            return (enode)u(enode, mem);
    }
    return (enode)false;
}


enode constructable(etype fr, etype to) {
    if (fr == to)
        return (enode)true;

    // if its a primitive, and we want to convert to string -> Au_cast_string
    aether a = fr->mod;

    if (to == etypeid(Au) && (canonical(fr) == etypeid(symbol) || canonical(fr) == etypeid(cstr))) {
        to = etypeid(string); // string is Au, so we are not lying here; Au is effectively always shiny
    }

    Au_t integral        = null;
    int  integral_count  = 0;
    Au_t realistic       = null;
    int  realistic_count = 0;

    poly_members(to->au, mem) {
        etype fn = mem->member_type == AU_MEMBER_CONSTRUCT ? u(etype, mem) : null;
        if  (!fn) continue;
        verify(fn->au->args.count == 2, "unexpected argument count for constructor"); // target + with-type
        Au_t with_arg = (Au_t)fn->au->args.origin[1];
        if (with_arg->rtype->is_realistic) {
            if (!realistic) realistic = mem;
            realistic_count++;
        }
        if (with_arg->rtype->is_integral) {
            if (!integral) integral = mem;
            integral_count++;
        }
        if (with_arg->src == fr->au)
            return (enode)u(enode, mem);
    }

    if (realistic_count == 1 && fr->au->is_realistic)
        return (enode)u(enode, realistic);
    
    if (integral_count == 1 && fr->au->is_integral)
        return (enode)u(enode, integral);
    
    return (enode)false;
}

static enode scalar_compatible(LLVMTypeRef ea, LLVMTypeRef eb) {
    LLVMTypeKind ka = LLVMGetTypeKind(ea);
    LLVMTypeKind kb = LLVMGetTypeKind(eb);

    if (ka == kb) {
        switch (ka) {
            case LLVMIntegerTypeKind:
                if (LLVMGetIntTypeWidth(ea) == LLVMGetIntTypeWidth(eb))
                    return (enode)true;
                else
                    return (enode)false;
            case LLVMFloatTypeKind:   // 32-bit float
            case LLVMDoubleTypeKind:  // 64-bit float
            case LLVMHalfTypeKind:    // 16-bit float
            case LLVMBFloatTypeKind:  // 16-bit bfloat
                return (enode)true; // same kind → compatible
            default:
                return (enode)false;
        }
    }
    return (enode)false;
}

bool etype_inherits(etype mdl, etype base) {
    aether a = mdl->mod;
    if (mdl == base) return true;
    bool inherits = false;
    etype m = mdl;
    if (!is_class(mdl)) return false;
    while (m) {
        if (m == base)
            return true;
        
        if (!m->au->context) break;
        if (u(etype, m->au->context) == m) break;
        m = u(etype, m->au->context);
    }
    return false;
}

static bool is_subclass(Au a0, Au b0) {
    if (is_prim(a0) || is_prim(b0))
        return false;
    Au_t a = au_arg((Au)a0);
    Au_t b = au_arg((Au)b0);
    while (a) {
        if (a == b) return true;
        if (a->context == a) break;
        a = a->context;
    }
    return false;
}

// returns null, true, or the member function used for conversion
enode convertible(etype fr, etype to) {
    aether a = fr->mod;
    etype  ma = canonical(fr);
    etype  mb = canonical(to);

    if (ma->au->is_enum && mb->au->is_integral)
        return (enode)true;
    if (mb->au->is_enum && ma->au->is_integral)
        return (enode)true;
    
    if (ma->au->is_lambda && mb->au == typeid(callback))
        return (enode)true;
    if ((!ma->au->is_class && is_ptr(ma)) && ma->au->src->is_struct && mb->au == typeid(Au))
        return (enode)true;

    if (is_func(ma) && (mb == etypeid(ARef) || mb == etypeid(handle)))
        return (enode)true;

    // allow Au to convert to primitive/struct
    if (ma->au == typeid(Au) && (mb->au->is_struct || mb->au->is_primitive || mb->au->is_pointer))
        return (enode)true;

    // trust the engineer to use their types in normal ways
    if (ma == etypeid(handle) && (is_class(mb) || mb->au->is_pointer))
        return (enode)true;

    // schema table conversion
    if (ma->au->is_schema && mb->au->is_pointer)
        return (enode)true;
    if (mb->au->is_schema && ma->au->is_pointer)
        return (enode)true;
    if (ma->au->is_schema && mb->au == typeid(Au_t))
        return (enode)true;
    if (mb->au->is_schema && ma->au == typeid(Au_t))
        return (enode)true;

    // unloaded enodes are the same as reference enodes of pointer type
    enode fr_node = instanceof(fr, enode);
    if (fr_node && !fr_node->loaded && (pointer(a, (Au)fr_node->au) == u(etype, mb->au) || mb->au == typeid(Au)))
        return (enode)true;

    // allow pointers of any sort, to boolean
    if  (is_ptr(ma) && mb->au == typeid(bool)) return (enode)true;

    // todo: ref depth must be the same too
    if (resolve(ma) == resolve(mb) && ref_level((Au)ma) == ref_level((Au)mb)) return (enode)true;

    // more robust conversion is, they are both pointer and not user-created
    if (ma->au->is_system || mb->au == typeid(handle))
        return (enode)true;

    if (ma == mb)
        return (enode)true;

    if (is_prim(ma) && is_prim(mb))
        return (enode)true;

    enode mcons = is_rec(mb) ? constructable(ma, mb) : null;

    if (!constructable(ma, mb) && is_prim(ma) && inherits(mb->au, typeid(string))) {
        static int s2 = 0;
        s2++;
        return (enode)true;
    }

    if (is_rec(ma) || is_rec(mb)) {
        Au_t au_type = au_lookup("Au_t");
        if (is_ptr(ma) && ma->au->src == au_type && mb->au == au_type)
            return (enode)true;
        if (!is_prim(ma) && !is_prim(mb) && is_subclass((Au)ma, (Au)mb) || is_subclass((Au)mb, (Au)ma))
            return (enode)true;
        if (mcons)
            return mcons;
        enode mcast = castable(ma, mb);
        return mcast;
    } else {
        // the following check should be made redundant by the code below it
        etype sym = etypeid(symbol);
        etype ri8 = etypeid(ref_i8);

        if ((ma == sym && mb == ri8) || (ma == ri8 && mb == sym))
            return (enode)true;

        LLVMTypeKind ka = LLVMGetTypeKind(lltype(ma));
        LLVMTypeKind kb = LLVMGetTypeKind(lltype(mb));

        // pointers → compare element types
        if (ka == LLVMPointerTypeKind && kb == LLVMPointerTypeKind) {
            // return true if one is of an opaque type
            // return false if the scalar units are different size
            return (enode)true;
        }

        // primitive check should find all other valid ones above this
        return scalar_compatible(lltype(ma), lltype(mb));
    }

    return (enode)false;
}

static bool is_addressable(enode e) {
    Au_t au = au_arg((Au)e);
    if (au->member_type == AU_MEMBER_VAR) au = au->src;
    if (is_ptr(au) || is_class(au)) return true;
    if (e->value && LLVMIsAGlobalValue(e->value)) return true;
    return false;
}

enode eshape_from_indices(aether a, array indices) {
    etype shape_t = etypeid(shape);
    i32 count = len(indices);
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, shape_t);

    // Allocate stack space for the i64 array (data field)
    LLVMTypeRef arr_t = LLVMArrayType(lltype(etypeid(i64)), count);
    LLVMValueRef data_alloc = LLVMBuildAlloca(B, arr_t, "shape_data");

    // Store each index expression into the data array
    for (i32 i = 0; i < count; i++) {
        enode idx = (enode)indices->origin[i];
        // Convert index to i64 if needed
        enode idx_i64 = e_create(a, etypeid(i64), (Au)idx);
        
        LLVMValueRef gep_idxs[] = {
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0),
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), i, 0)
        };
        LLVMValueRef elem_ptr = LLVMBuildGEP2(B, arr_t, data_alloc, gep_idxs, 2, "");
        LLVMBuildStore(B, idx_i64->value, elem_ptr);
    }

    // Get pointer to first element (ref_i64 / i64*)
    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0);
    LLVMValueRef data_ptr_idxs[] = { zero, zero };
    LLVMValueRef data_ptr = LLVMBuildGEP2(B, arr_t, data_alloc, data_ptr_idxs, 2, "data_ptr");

    enode count_node = e_operand(a, _i64(count), etypeid(i64));
    enode data_node = enode(mod, a, loaded, true, value, data_ptr, au, etypeid(ref_i64)->au);

    // Lookup shape_from function: shape_from(i64 count, ref_i64 values)
    Au_t shape_from_fn = find_member(typeid(shape), "shape_from", AU_MEMBER_FUNC, 0, false);
    verify(shape_from_fn, "shape_from function not found");

    // Call shape_from(count, data)
    return e_fn_call(a, (efunc)u(efunc, shape_from_fn), a(count_node, data_node));
}

void aether_eputs(aether a, string output) {
    if (a->no_build) return;

    efunc  fn_puts = (efunc)elookup("puts");
    e_fn_call(a, fn_puts, a(const_string(chars, output->chars)));
}

enode etype_access(etype target, string name, bool funny_business) { sequencer
    if (seq == 828)
        seq = seq;
    aether a = target->mod;
    Au_t rel = target->au->member_type == AU_MEMBER_VAR ? 
        (funny_business ? resolve(u(etype, target->au->src))->au : u(etype, target->au->src)->au) : target->au;
    if (funny_business && rel->is_pointer && rel->src && (rel->src->is_primitive || rel->src->is_class || rel->src->is_struct))
        rel = rel->src;
    if (eq(name, "value")) {
        name = name;
    }
    Au_t   m = find_member(rel, name->chars, 0, 0, true);
    if (!m) {
        m = m;
    }
    verify(m, "failed to find member %o on type %o", name, rel);
    bool is_enode = instanceof(target, enode) != null;

    etype t = canonical(target); // resolves to non-alias type
    if (seq == 277)
        seq = seq;
    if (is_ptr(t) && !is_class(t)) t = u(etype, t->au->src); // check source; t must always be a is_class or is_struct

    // for functions, we return directly with target passed along
    if (is_func((Au)m)) {
        enode n = enode_value((enode)target, false); // handle super here

        // primitive method receiver must be addressable
        if (is_prim(n->au->src) && !is_addressable(n)) {
            etype res = resolve(n);
            LLVMValueRef temp = LLVMBuildAlloca(B, lltype(res), "prim.recv.addr");
            LLVMBuildStore(B, n->value, temp);
            n = value(pointer(a, (Au)n->au->src), temp);
        }
        
        // if primitive, we need to make a temp on stack (reserving Au header space), and obtain pointer to it
        return (enode)efunc(mod, a, au, m, loaded, true, target,
            (m->is_static || m->is_smethod) ? null : n);
    }

    // enum values should already be created
    if (m->member_type == AU_MEMBER_ENUMV) {
        // make sure the enum is implemented
        etype_implement((etype)u(etype, m->context), false);
        verify(u(enode, m), "expected enode for enum value");
        return u(enode, m);
    }
    
    enode n = u(enode, m);
    if (n && n->value) return n;

    // signal we're doing something non-const
    a->is_const_op = false;
    if (a->no_build)
        return e_noop(a, u(etype, m->src));

    enode tnode = (enode)target;
    string id = f(string, "enode_access_%i", seq);
    if (m->context == (Au)typeid(Au)) {
        LLVMValueRef    i8ptr    = LLVMBuildBitCast(B, tnode->value, LLVMPointerType(LLVMInt8Type(), 0), "i8ptr");
        LLVMValueRef    offset   = LLVMConstInt(LLVMInt64Type(), -(int64_t)sizeof(struct _Au), true);
        LLVMValueRef    au_ptr   = LLVMBuildGEP2(B, LLVMInt8Type(), i8ptr, &offset, 1, "au.base");
        etype           au       = etypeid(Au);
        LLVMValueRef    au_typed = LLVMBuildBitCast(B, au_ptr, LLVMPointerType(au->lltype, 0), "au.typed");
        return enode(
            mod,    a,     au,       m,
            loaded, false, debug_id, id,
            value,  LLVMBuildStructGEP2(B, au->lltype, au_typed, m->index, id->chars));
    }

    return enode(
        mod,    a,
        au,     m,
        loaded, false,
        debug_id, id,
        value,  LLVMBuildStructGEP2(
            B, t->lltype, tnode->value,
            m->index, id->chars));
}

enode resolve_typeid(aether a, Au mdl) {
    Au_t au = au_arg(mdl);
    if (au->is_pointer && au->src->is_class) {
        return u(etype, au->src) ? u(etype, au->src)->type_id : null;
    }
    return u(etype, au) ? u(etype, au)->type_id : null;
}

etype implement_type_id(etype t);

enode e_runtime_type(enode instance) {
    aether a = instance->mod;

    if (a->no_build)
        return e_noop(a, etypeid(Au_t));

    LLVMTypeRef  i8_ptr_ty  = LLVMPointerTypeInContext(a->module_ctx, 0);
    LLVMValueRef i8_this    = LLVMBuildBitCast(B, instance->value, i8_ptr_ty, "this_i8");
    
    LLVMValueRef offset_neg = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), -(int)sizeof(struct _Au), true);
    LLVMValueRef header_ptr = LLVMBuildGEP2(B, LLVMInt8TypeInContext(a->module_ctx), i8_this, &offset_neg, 1, "header_ptr");
    
    LLVMTypeRef  Au_t_ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0);
    LLVMValueRef header_cast = LLVMBuildBitCast(B, header_ptr, Au_t_ptr_ty, "header_cast");
    return value(etypeid(Au_t), LLVMBuildLoad2(B, i8_ptr_ty, header_cast, "runtime_type"));
}

enode aether_e_typeid(aether a, etype mdl) { sequencer

    if (a->debug_typeid) {
        fprintf(stderr, "  e_typeid[quants]: mdl=%s(%p) module=%s ptr=%d is_ptr=%d is_rec=%d\n",
             mdl->au->ident, (void*)mdl->au,
            mdl->au->module ? mdl->au->module->ident : "(null)",
            mdl->au->is_pointer, is_ptr(mdl), (int)(uintptr_t)is_rec(mdl));
        a->debug_typeid = false;
    }

    // link back to ref_i8 etc
    // fixing this in canonical seems to have issues
    //if (seq == 56) {
    //    mdl = canonical(mdl);
    //    seq = seq;
    //}

    // pointer types with their shapes need registration
    // for this we merely need to def() them
    if (is_ptr(mdl) && !is_rec(mdl) && mdl->au->src->is_primitive && mdl->au->src->ptr) {
        char ref_primitive[64];
        sprintf(ref_primitive, "ref_%s", mdl->au->src->ident);
        etype t = elookup(ref_primitive);
        if (t) {
            mdl = t;
            //mdl = u(etype, mdl->au->src->ptr);
            implement_type_id(mdl);
        }
    }

    if (a->no_build)
        return e_noop(a, etypeid(Au_t));
    
    if (!mdl)
        return e_null(a, etypeid(Au_t));

    if (instanceof(mdl, enode)) { // so we can take in member variables
        return e_runtime_type((enode)mdl);
    }
    
    if (!mdl->type_id && (is_module(mdl->au) || !mdl->au->module->is_au))
        implement_type_id(mdl);

    a->is_const_op = false;
    enode n = resolve_typeid(a, (Au)mdl);
    verify(n, "schema instance not found for %o [%i]", mdl, seq);
    return n;
}

bool is_au_t(Au a) {
    Au_t au = au_arg(a);
    if (au && au->is_pointer) {
        au = au->src;
    }
    return au == typeid(Au_t);
}

bool is_map(etype t) {
    if (instanceof(t, etype) && t->au == typeid(map) || t->au->context == typeid(map)) {
        return true;
    }
    return false;
}














































// ============================================================================
// e_init: unified object initialization
//
// Handles the full init sequence for class objects:
//   1. Call constructor (ctr) with ctr_input if provided
//   2. Set map properties on the allocated object
//   3. Resolve context members from scope
//   4. Call f_initialize (or emit direct init chain when a->direct)
//
// This replaces the duplicated alloc→ctr→resolve→initialize patterns
// that were scattered across e_create, e_create_from_map, e_create_from_array.
//
// Add to aether header:
//   M(X, Y, i, method, public, enode, e_init, enode, map, efunc, enode)
// ============================================================================

enode aether_e_init(aether a, enode alloc, map props, efunc ctr, enode ctr_input) {
    if (a->no_build) return alloc;

    efunc f_initialize = (efunc)u(efunc,
        find_member(etypeid(Au)->au, "initialize", AU_MEMBER_FUNC, 0, false));

    // 1. call constructor if provided
    if (ctr && ctr_input)
        e_fn_call(a, ctr, a(alloc, ctr_input));

    // 2. set map properties
    if (props) {
        efunc f_set_prop = (efunc)u(efunc,
            find_member(etypeid(Au)->au, "set_property", AU_MEMBER_FUNC, 0, false));
        pairs(props, i) {
            Au k = i->key;
            if (instanceof(k, enode)) {
                e_fn_call(a, f_set_prop, a(alloc, k, i->value));
            } else {
                enode prop = access(alloc, (string)k, true);
                e_assign(a, prop, i->value, OPType__assign);
            }
        }
    }

    // 2b. verify required members were provided
    poly_members(alloc->au, mb) {
        if (mb->member_type == AU_MEMBER_VAR && mb->is_required) {
            bool provided = ctr_input != null;
            if (!provided && props) {
                pairs(props, p) {
                    string k = (string)instanceof(p->key, string);
                    if (k && strcmp(k->chars, mb->ident) == 0) {
                        provided = true; break;
                    }
                }
            }
            validate(provided, "required member '%s' not provided for %s",
                mb->ident, alloc->au->ident);
        }
    }

    // 3. resolve context members from scope (user-provided props override scope lookup)
    resolve_context_members(alloc, props);

    // 4. call init chain — direct for concrete types, Au_initialize for polymorphic
    enode res = alloc;
    if (a->direct || !alloc->is_any) {
        // walk context chain, collect init functions, call base-first
        array chain = etype_class_list(canonical((etype)alloc));
        each(chain, etype, mdl) {
            if (mdl->au == typeid(Au)) continue;
            Au_t init_mem = find_member(mdl->au, "init", AU_MEMBER_FUNC, 0, false);
            efunc  init_f = u(efunc, init_mem);
            if (init_f) e_fn_call(a, init_f, a(alloc));
        }

    } else {
        res = e_fn_call(a, f_initialize, a(alloc));
    }
    res->au     = alloc->au;
    res->meta_a = hold(alloc->meta_a);
    res->meta_b = hold(alloc->meta_b);
    return res;
}


// ============================================================================
// revised e_create_from_map — now uses e_init
// ============================================================================
enode e_create_from_map(aether a, etype t, map m) {
    bool  is_m = is_map(t);
    efunc f_alloc    = (efunc)u(efunc, find_member(etypeid(Au)->au, "alloc_new",  AU_MEMBER_FUNC, 0, false));
    efunc f_mset     = (efunc)u(efunc, find_member(etypeid(map)->au, "set",       AU_MEMBER_FUNC, 0, false));
    enode metas_node = e_meta_ids(a, t->meta_a, t->meta_b);

    efunc cur = context_func(a);
    enode res = e_fn_call(a, f_alloc, a(
        e_typeid(a, t), _i32(1), e_null(a, etypeid(shape)), metas_node));
    res->au   = t->au;
    res->meta_a = hold(t->meta_a);
    res->meta_b = hold(t->meta_b);

    if (is_m) {
        // map type: initialize first, then populate entries
        efunc f_initialize = (efunc)u(efunc,
            find_member(etypeid(Au)->au, "initialize", AU_MEMBER_FUNC, 0, false));
        e_fn_call(a, f_initialize, a(res));
        pairs(m, i) {
            e_fn_call(a, f_mset, a(res, i->key, i->value));
        }
    } else {
        // class type: use e_init with the property map
        res->is_alloc = !a->building_initializer;
        res = e_init(a, res, m, null, null);
    }
    return res;
}


// ============================================================================
// revised e_create_from_array — now uses e_init for the class path
// ============================================================================
enode e_create_from_array(aether a, etype t, array ar) {
    if (a->no_build) return e_noop(a, t);

    efunc f_alloc      = (efunc)u(efunc, find_member(etypeid(Au)->au, "alloc_new",  AU_MEMBER_FUNC, 0, false));
    efunc f_push       = (efunc)u(efunc, find_member(etypeid(collective)->au, "push", AU_MEMBER_FUNC, 0, true));
    efunc f_push_vdata = (efunc)u(efunc, find_member(etypeid(array)->au, "push_vdata", AU_MEMBER_FUNC, 0, true));

    enode metas_node = e_meta_ids(a, t->meta_a, t->meta_b);
    enode res = e_fn_call(a, f_alloc, a(
        e_typeid(a, t), _i32(1), e_null(a, etypeid(shape)), metas_node));
    res->au = t->au;
    
    bool  all_const = t->au == typeid(array);
    int   ln = len(ar);
    enode array_len = e_operand(a, _i64(ln), etypeid(i64));
    
    enode const_vector = null;

    if (all_const)
        for (int i = 0; i < ln; i++) {
            enode node = (enode)instanceof(ar->origin[i], enode);
            if (node && node->literal)
                continue;
            all_const = false;
            break;
        }

    // read meta args
    etype element_type = u(etype, t->meta_a);
    shape dims         = instanceof(t->meta_b, shape);
    if (!element_type) element_type = etypeid(Au);
    int max_items = dims ? shape_total(dims) : -1;

    verify(max_items == -1 || ln <= max_items, "too many elements given to array");
    
    if (all_const) {
        etype ptr = pointer(a, (Au)element_type);
        etype elem_t = resolve(t);
        LLVMValueRef *elems = malloc(sizeof(LLVMValueRef) * ln);
        for (int i = 0; i < ln; i++) {
            enode node = (enode)ar->origin[i];
            elems[i]   = node->value;
        }
        LLVMValueRef const_arr = LLVMConstArray(lltype(elem_t), elems, ln);
        free(elems);
        static int ident = 0;
        char vname[32];
        sprintf(vname, "const_vector_%i", ident++);
        LLVMValueRef glob = LLVMAddGlobal(a->module_ref, LLVMTypeOf(const_arr), vname);
        LLVMSetLinkage(glob, LLVMInternalLinkage);
        LLVMSetGlobalConstant(glob, 1);
        LLVMSetInitializer(glob, const_arr);
        const_vector = enode(mod, a, au, ptr->au, loaded, false, value, glob);
    }

    if (t->au == typeid(array)) {
        // set alloc size, unmanaged, and assorted flags before init
        enode prop_alloc     = etype_access((etype)res, string("alloc"), false);
        enode prop_unmanaged = etype_access((etype)res, string("unmanaged"), false);
        e_assign(a, prop_alloc, (Au)array_len, OPType__assign);
        // mark assorted if element type is a class (subtypes may differ)
        if (element_type && element_type->au &&
            (element_type->au->traits & AU_TRAIT_CLASS)) {
            enode prop_assorted = etype_access((etype)res, string("assorted"), false);
            enode tru = e_operand(a, _bool(true), etypeid(bool));
            e_assign(a, prop_assorted, (Au)tru, OPType__assign);
        }
        if (const_vector) {
            enode tru = e_operand(a, _bool(ln), etypeid(bool));
            e_assign(a, prop_unmanaged, (Au)tru, OPType__assign);
        }

        // use e_init — no ctr, no prop map (we set props directly above)
        res->is_alloc = !a->building_initializer;
        res = e_init(a, res, null, null, null);

        // populate after init
        if (const_vector) {
            e_fn_call(a, f_push_vdata, a(res, const_vector, array_len, e_typeid(a, element_type)));
        } else {
            for (int i = 0; i < ln; i++) {
                Au element = ar->origin[i];
                e_fn_call(a, f_push, a(res, element));
            }
        }
    } else {
        fault("e_create_from_array for %o (non-array) not implemented", t);
    }

    return res;
}

enode e_convert_or_cast(aether a, etype output, enode input) {
    if (a->no_build) return e_noop(a, output);

    // if input is an unloaded primitive, load it first so we can do proper conversion
    if (!input->loaded && is_prim(input) && is_prim(output)) {
        input = enode_value(input, false);  // load it
    }
    
    Au_t itype = isa(input);
    LLVMTypeRef typ = LLVMTypeOf(input->value);
    LLVMTypeKind k = LLVMGetTypeKind(typ);

    // check if these are either Au_t class typeids, or actual compatible instances
    if (k == LLVMPointerTypeKind) { // this should all be in convertible
        a->is_const_op = false;
        if (a->no_build) return e_noop(a, output);
        if (output->au == typeid(bool)) {
            return value(output,
                LLVMBuildICmp(B, LLVMIntNE, input->value,
                    LLVMConstNull(LLVMPointerTypeInContext(a->module_ctx, 0)), "ptr_to_bool"));
        }
        bool loaded = !(!(is_ptr(output) || is_func_ptr(output)) && (is_struct(output) || is_prim(output)));
        Au output_info = head(output);
        enode res = value(output,
            LLVMBuildBitCast(B, input->value, loaded ? lltype(output) : lltype(pointer(a, (Au)output)), "class_ref_cast"));
        res->loaded = loaded; // loaded/unloaded is a brilliant abstract of laziness; allows casts to take place with slight alterations to the enode
        return res;
    }

    // fallback to primitive conversion rules
    return aether_e_primitive_convert(a, input, output);
}

// ============================================================================
// revised e_create — class init paths now delegate to e_init
// ============================================================================
enode aether_e_create(aether a, etype mdl, Au args) { sequencer
    if (!mdl) {
        verify(instanceof(args, enode), "aether_e_create");
        return (enode)args;
    }


    string  str      = (string)instanceof(args, string);
    map     imap     = (map)instanceof(args, map);
    array   ar       = (array)instanceof(args, array);
    static_array static_a = (static_array)instanceof(args, static_array);
    enode   n        = null;
    efunc   ctr      = null;

    // type identity cast
    if (is_au_t((Au)mdl) && is_au_t(args)) {
        enode n = instanceof(args, enode);
        return value(mdl,
            LLVMBuildBitCast(B, n->value, lltype(mdl), "is_au_t_cast"));
    }

    if (!args && is_ptr(mdl) && !is_class(mdl))
        return e_null(a, mdl);

    // lambda creation (unchanged)
    if (is_lambda((Au)mdl)) {
        verify(isa(args) == typeid(array), "expected args for lambda");

        efunc n_mdl = instanceof(mdl, efunc);
        verify(n_mdl && n_mdl->target, "expected enode with target for lambda function");
        
        efunc f_create = (efunc)u(efunc, find_member(etypeid(lambda)->au,
            "lambda_instance", AU_MEMBER_FUNC, 0, false));
        
        etype ctx_type = u(etype, n_mdl->context_node->au->src);
        enode ctx_alloc = e_alloc(a, ctx_type);
        
        int ctx_index = 0;
        verify(len((array)args) == n_mdl->au->members.count,
            "lambda initialization member-count (%i) != user-provided args (%i)",
            n_mdl->au->members.count, len((array)args));
        
        members(n_mdl->au, mem) {
            if (mem->member_type != AU_MEMBER_VAR) continue;
            enode ctx_val = (enode)array_get((array)args, ctx_index);
            enode field_ref = access(ctx_alloc, string(mem->ident), true);
            e_assign(a, field_ref, (Au)ctx_val, OPType__assign);
            ctx_index++;
        }
        
        enode targ = n_mdl->target;
        array args = a(n_mdl->published_type, n_mdl, n_mdl->target, ctx_alloc);
        enode res = e_fn_call(a, f_create, args);
        res->meta_a = hold(n_mdl->meta_a);
        res->meta_b = hold(n_mdl->meta_b);
        return res;
    }

    // construct / cast lookup
    Au_t  args_type = isa(args);
    efunc ftest     = (efunc)instanceof(args, efunc);
    enode input     = (enode)instanceof(args, enode);
    etype canon     = canonical(input);
    bool  input_estr = canon == etypeid(symbol) || canon == etypeid(cstr);

    if (input && !input->loaded && !convertible((etype)input, mdl)) {
        Au info = head(input);
        input = enode_value(input, false);
    }

    if (!input && instanceof(args, const_string)) {
        input = e_operand(a, args, mdl);
        args  = (Au)input;
    }

    if (!input && instanceof(args, string)) {
        input = e_operand(a, args, etypeid(string));
        args  = (Au)input;
    }

    if (input) {
        verify(!imap, "unexpected data");
        
        // boxing: struct/prim to Au
        etype input_type = canonical(input);
        if (!is_ptr(input) && mdl->au == typeid(Au) && !input_estr && (is_enum(canonical(input)) || is_struct(input) || is_prim(input))) {
            a->is_const_op = false;
            if (a->no_build) return e_noop(a, mdl);
            
            if (LLVMIsAConstantPointerNull(input->value))
                return value(mdl, LLVMConstPointerNull(lltype(mdl)));

            efunc f_alloc    = (efunc)u(efunc,
                find_member(etypeid(Au)->au, "alloc_new", AU_MEMBER_FUNC, 0, false));
            enode metas_node = e_meta_ids(a, input_type->meta_a, input_type->meta_b);
            enode boxed      = e_fn_call(a, f_alloc, a(
                e_typeid(a, input_type), 
                _i32(1), 
                e_null(a, etypeid(shape)),
                metas_node
            ));
            boxed->au = input_type->au;
            LLVMBuildStore(B, input->value, boxed->value);
            return value(canonical(mdl), 
                LLVMBuildBitCast(B, boxed->value, lltype(canonical(mdl)), "box_to_au"));
        }

        Au_t  t_isa = isa(mdl);
        if (seq == 532)
            seq = seq;
        enode fmem  = convertible((etype)input, mdl);

        verify(fmem, "no suitable conversion found for %o -> %o (%i)",
            input, mdl, seq);
        
        if (fmem == (void*)true) {
            return e_convert_or_cast(a, canonical(mdl), input);
        }
        
        efunc fn = (efunc)instanceof(fmem, efunc);
        if (fn && fn->au->member_type == AU_MEMBER_CONSTRUCT) {
            ctr = (efunc)fmem;
        } else if (fn && fn->au->member_type == AU_MEMBER_CAST) {
            return e_fn_call(a, fn, a(input));
        } else
            fault("unknown error");
    }

    // primitives (including enum values)
    if (is_prim(mdl))
        return e_operand(a, args, mdl);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);

    etype cmdl = etype_traits((Au)mdl, AU_TRAIT_CLASS);
    etype smdl = etype_traits((Au)mdl, AU_TRAIT_STRUCT);
    enode res;

    // static array of structs (global constant)
    if (is_ptr(mdl) && is_struct(mdl->au->src) && static_a) {
        static int ident = 0;
        char name[32];
        sprintf(name, "static_arr_%i", ident++);

        i64          ln     = len(static_a);
        etype        emdl   = (etype)u(etype, mdl->au->src);
        LLVMTypeRef  arrTy  = LLVMArrayType(lltype(emdl), ln);
        LLVMValueRef G      = LLVMAddGlobal(a->module_ref, arrTy, name);
        LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));
        LLVMSetLinkage(G, LLVMInternalLinkage);

        for (i32 i = 0; i < ln; i++) {
            enode n = e_operand(a, static_a->origin[i], null);
            verify(LLVMIsConstant(n->value), "static_array must contain constant statements");
            verify(n->au == mdl->au->src, "type mismatch");
            elems[i] = n->value;
        }
        LLVMValueRef init = LLVMConstArray(lltype(emdl), elems, ln);
        LLVMSetInitializer(G, init);
        LLVMSetGlobalConstant(G, 1);
        free(elems);
        res = enode(mod, a, value, G, loaded, false, au, mdl->au);

    } else if (!ctr && cmdl) {
        // ---- class without constructor ----
        if (instanceof(args, array)) {
            return e_create_from_array(a, mdl, (array)args);
        } else if (instanceof(args, map)) {
            return e_create_from_map(a, mdl, (map)args);
        } else if (instanceof(args, shape)) {
            shape sh = (shape)args;
            array indices = array(alloc, 32);
            for (int i = 0; i < sh->count; i++)
                push(indices, _i64(sh->data[i]));
            res = eshape_from_indices(a, indices);
            res->literal = (Au)hold(sh);
        } else {
            // default class creation: alloc + e_init
            enode alloc = e_alloc(a, mdl);
            alloc->is_alloc = !a->building_initializer;
            alloc->meta_a   = hold(mdl->meta_a);
            alloc->meta_b   = hold(mdl->meta_b);
            res = e_init(a, alloc, null, null, null);
        }

    } else if (ctr) {
        // ---- class with constructor ----
        verify(is_rec(mdl), "expected record");

        if (canonical(mdl) == etypeid(Au) && u(etype, ctr->au->context) != canonical(mdl)) {
            mdl = u(etype, ctr->au->context);
        }

        enode alloc = e_alloc(a, mdl);
        alloc->is_alloc = !a->building_initializer;
        alloc->meta_a   = hold(mdl->meta_a);
        alloc->meta_b   = hold(mdl->meta_b);
        res = e_init(a, alloc, null, ctr, input);

    } else {
        // ---- struct / ref-struct / other ----
        array is_arr        = (array)instanceof(args, array);
        bool  is_ref_struct = is_ptr(mdl) && is_struct(resolve(mdl));
        etype rmdl = resolve(mdl);

        if (is_struct(mdl) || is_ref_struct) {
            int field_count = LLVMCountStructElementTypes(rmdl->lltype);
            LLVMValueRef *fields = calloc(field_count, sizeof(LLVMValueRef));
            bool all_const = a->no_const ? false : true;

            if (is_arr) {
                array ar = (array)args;
                int ln = len(ar);
                verify(ln <= field_count, "too many fields given (%i) for %o", len(ar), mdl);
                
                for (int i = 0; i < field_count; i++) {
                    if (i >= len(ar))
                        break;
                    etype field_type = null;
                    int current_index = 0;
                    for (int ri = 0; ri < rmdl->au->members.count; ri++) {
                        Au_t smem = (Au_t)rmdl->au->members.origin[ri];
                        if (smem->member_type == AU_MEMBER_VAR && smem->is_iprop) {
                            if (current_index++ == i) {
                                field_type = u(etype, smem);
                                break;
                            }
                        }
                    }
                    Au val = get(ar, i);
                    enode op = val ? e_operand(a, val, field_type) : 
                                     e_create(a, field_type, null);
                    if (!op->literal) all_const = false;
                    fields[i] = op->value;
                }
                if (ln < field_count)
                    field_count = ln;
                
            } else if (imap) {
                array field_indices = array(field_count);
                array field_names   = array(field_count);
                array field_values  = array(field_count);

                pairs(imap, i) {
                    string  k = (string)i->key;
                    Au_t   t = isa(i->value);
                    Au_t   m = find_member(rmdl->au, k->chars, AU_MEMBER_VAR, 0, true);
                    i32 index = m->index;

                    enode value = e_operand(a, i->value, u(etype, m->src));
                    if (all_const && !LLVMIsConstant(value->value))
                        all_const = false;

                    push(field_indices, _i32(index));
                    push(field_names,   (Au)string(m->ident));
                    push(field_values,  (Au)value);
                }

                for (int i = 0; i < field_count; i++) {
                    enode value = null;
                    etype field_type = null;
                    for (int our_index = 0; our_index < field_count; our_index++) {
                        i32* f_index = (i32*)field_indices->origin[our_index];
                        if  (f_index && *f_index == i) {
                            value = (enode)field_values->origin[our_index];
                            break;
                        }
                    }
                    int current_index = 0;
                    for (int ri = 0; ri < rmdl->au->members.count; ri++) {
                        Au_t smem = (Au_t)rmdl->au->members.origin[ri];
                        if (smem->member_type == AU_MEMBER_VAR && !smem->is_static) {
                            if (current_index++ == i) {
                                field_type = u(etype, smem);
                                break;
                            }
                        }
                    }
                    verify(field_type, "field type lookup failed for %o (index = %i)", mdl, i);

                    LLVMTypeRef tr = LLVMStructGetTypeAtIndex(rmdl->lltype, i);
                    LLVMTypeRef expect_ty = lltype(field_type);
                    verify(expect_ty == tr, "field type mismatch");
                    if (!value) value = e_null(a, field_type);
                    fields[i] = value->value;
                }
            }

            // struct alloca + field stores
            res = enode(mod, a, loaded, false, value,
                LLVMBuildAlloca(B, lltype(mdl), "alloca-mdl"), au, mdl->au);
            res = e_zero(a, res);
            for (int i = 0; i < field_count; i++) {
                if (!LLVMIsNull(fields[i])) {
                    LLVMValueRef gep = LLVMBuildStructGEP2(B, lltype(mdl), res->value, i, "");
                    LLVMBuildStore(B, fields[i], gep);
                }
            }
            free(fields);
        } else 
            res = e_operand(a, args, mdl);
    }
    return res;
}


none copy_lambda_info(enode mem, enode lambda_fn) {
    mem->context_node = hold(lambda_fn->context_node); // would be nice to do this earlier
    mem->published_type = hold(lambda_fn->published_type);
    mem->value        = lambda_fn->value;
}


enode aether_e_vector(aether a, etype t, enode shape_data) {
    efunc f_alloc    = (efunc)u(efunc,
        find_member(etypeid(Au)->au, "alloc_new", AU_MEMBER_FUNC, 0, false));
    enode metas_node = e_meta_ids(a, t->meta_a, t->meta_b);
    return e_fn_call(a, f_alloc, a( e_typeid(a, t), _i32(0), shape_data, metas_node ));
}

enode aether_e_alloc(aether a, etype mdl) {
    enode metas_node = e_meta_ids(a, mdl->meta_a, mdl->meta_b);
    efunc f_alloc = (efunc)u(efunc,
        find_member(etypeid(Au)->au, "alloc_new", AU_MEMBER_FUNC, 0, false));
    enode res = e_fn_call(a, f_alloc, a(
        e_typeid(a, mdl), _i32(0), e_null(a, etypeid(shape)), metas_node ));
    res->au = mdl->au->is_class ? mdl->au : pointer(a, (Au)mdl)->au;
    return res;
}


enode aether_e_const_array(aether a, etype mdl, array arg) {
    etype atype = etypeid(Au_t);
    etype vector_type = etype_ptr(a, mdl->au, null);
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, vector_type);

    if (!arg || !len(arg))
        return e_null(a, vector_type);

    i32 ln = len(arg);
    LLVMTypeRef arrTy = LLVMArrayType(lltype(mdl), ln);
    LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = arg->origin[i];
        enode n = e_operand(a, m, mdl);
        elems[i] = n->value;  // each is an Au_t*
    }

    LLVMValueRef arr_init = LLVMConstArray(lltype(mdl), elems, ln);
    free(elems);

    static int ident = 0;
    char gname[32];
    sprintf(gname, "static_array_%i", ident++);
    LLVMValueRef G = LLVMAddGlobal(a->module_ref, arrTy, gname);
    LLVMSetLinkage(G, LLVMInternalLinkage);
    LLVMSetGlobalConstant(G, 1);
    LLVMSetInitializer(G, arr_init);
    return enode(mod, a, loaded, true, value, G, au, vector_type->au);
}

enode aether_e_default_value(aether a, etype mdl) {
    return aether_e_create(a, mdl, null);
}


enode aether_e_zero(aether a, enode n) {
    etype      mdl = (etype)n;
    LLVMValueRef v = n->value;
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);
    LLVMValueRef zero   = LLVMConstInt(LLVMInt8TypeInContext(a->module_ctx), 0, 0);          // value for memset (0)
    LLVMValueRef size   = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), mdl->au->abi_size / 8, 0); // size of alloc
    LLVMValueRef memset = LLVMBuildMemSet(B, v, zero, size, 0);
    return n;
}


etype prefer_mdl(etype m0, etype m1) {
    aether a = m0->mod;
    if (m0 == m1)
        return m0;
    
    etype g = elookup("any");
    if (m0 == g)
        return m1;
    
    if (m1 == g)
        return m0;
    
    if (inherits(m0->au, m1->au))
        return m1;
    
    return m0;
}

enode aether_e_ternary(aether a, enode cond_expr, enode true_expr, enode false_expr) {
    aether mod = a;
    etype rmdl  = false_expr ? (etype)prefer_mdl((etype)true_expr, (etype)false_expr) :  (etype)true_expr;

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, rmdl);

    // Step 1: Create the blocks for the ternary structure
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(mod->builder);
    LLVMValueRef pb = LLVMGetBasicBlockParent(current_block);
    LLVMBasicBlockRef then_block    = LLVMAppendBasicBlockInContext(a->module_ctx, pb, "ternary_then");
    LLVMBasicBlockRef else_block    = LLVMAppendBasicBlockInContext(a->module_ctx, pb, "ternary_else");
    LLVMBasicBlockRef merge_block   = LLVMAppendBasicBlockInContext(a->module_ctx, pb, "ternary_merge");

    // Step 2: Build the conditional branch based on the condition
    LLVMValueRef condition_value = cond_expr->value;
    LLVMBuildCondBr(mod->builder, condition_value, then_block, else_block);

    // Step 3: Handle the "then" (true) branch
    LLVMPositionBuilderAtEnd(mod->builder, then_block);
    LLVMValueRef true_value = true_expr->value;
    LLVMBuildBr(mod->builder, merge_block);  // Jump to merge block after the "then" block

    // Step 4: Handle the "else" (false) branch
    LLVMPositionBuilderAtEnd(mod->builder, else_block);
    enode default_expr = !false_expr ? e_create(a, (etype)rmdl, (Au)true_expr) : null;
    if (false_expr) false_expr = e_create(a, (etype)rmdl, (Au)false_expr);
    LLVMValueRef false_value = default_expr ? default_expr->value : false_expr->value;
    LLVMBuildBr(mod->builder, merge_block);  // Jump to merge block after the "else" block

    // Step 5: Build the "merge" block and add a phi enode to unify values
    LLVMPositionBuilderAtEnd(mod->builder, merge_block);
    LLVMTypeRef result_type = LLVMTypeOf(true_value);
    LLVMValueRef phi_node = LLVMBuildPhi(mod->builder, result_type, "ternary_result");
    LLVMAddIncoming(phi_node, &true_value, &then_block, 1);
    LLVMAddIncoming(phi_node, &false_value, &else_block, 1);

    // Return some enode or result if necessary (a.g., a enode indicating the overall structure)
    return enode(mod, mod, loaded, true, au, rmdl->au, value, phi_node);
}

enode aether_e_builder(aether a, subprocedure cond_builder) {
    if (!a->no_build) {
        LLVMBasicBlockRef block = LLVMGetInsertBlock(B);
        LLVMPositionBuilderAtEnd(B, block);
    }
    enode n = (enode)invoke(cond_builder, null);
    return n;
}

enode aether_e_native_switch(
        aether          a,
        enode           switch_val,
        map             cases,
        array           def_block,
        subprocedure    expr_builder,
        subprocedure    body_builder)
{
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, null);

    // set debug location for switch
    debug_emit(a);

    LLVMTypeRef       Ty    = LLVMTypeOf(switch_val->value);
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);

    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlockInContext(a->module_ctx, fn, "switch.end"));
    push_scope(a, (Au)switch_cat);

    LLVMBasicBlockRef default_block =
        def_block
            ? LLVMAppendBasicBlockInContext(a->module_ctx, fn, "default")
            : switch_cat->block;

    // create switch instruction
    LLVMValueRef SW =
        LLVMBuildSwitch(B, switch_val->value, default_block, cases->count);

    // allocate blocks for each case BEFORE emitting bodies
    map case_blocks = map(hsize, 16, unmanaged, true);

    Au_t common_type = null;
    pairs(cases, i) {
        LLVMBasicBlockRef case_block =
            LLVMAppendBasicBlockInContext(a->module_ctx, fn, "case");

        set(case_blocks, i->key, (Au)case_block);

        // evaluate key to literal
        enode key_node = (enode)invoke(expr_builder, i->key);
        Au   au_value = key_node->literal;
        Au_t au_type  = null;

        if (!au_value && is_enum(key_node->au)) {
            au_value = (Au)key_node->au->value;
            au_type  = canonical(key_node)->au->src;
        }

        verify(au_value, "expression not evaluating as constant (%o)", i->key);
        if (!au_type) au_type = isa(au_value);
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
        LLVMBasicBlockRef case_block = (LLVMBasicBlockRef)p->value;
        LLVMPositionBuilderAtEnd(B, case_block);

        array body_tokens = (array)value_by_index(cases, idx++);
        invoke(body_builder, (Au)body_tokens);

        LLVMBuildBr(B, switch_cat->block);
    }

    // default body
    if (def_block) {
        LLVMPositionBuilderAtEnd(B, default_block);
        invoke(body_builder, (Au)def_block);
        LLVMBuildBr(B, switch_cat->block);
    }

    // merge
    LLVMPositionBuilderAtEnd(B, switch_cat->block);
    pop_scope(a);
    return e_noop(a, null);
}

enode aether_e_switch(
        aether          a,
        enode           e_expr,        // switch expression tokens
        map             cases,       // map: array_of_tokens → array_of_tokens
        array           def_block,   // null or body array
        subprocedure    expr_builder,
        subprocedure    body_builder)
{
    // set debug location for switch
    debug_emit(a);

    LLVMValueRef entry = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
    //LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);

    // invoke expression for switch, and push switch cat
    enode   switch_val = e_expr; // invoke(expr_builder, expr);
    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlockInContext(a->module_ctx, entry, "switch.end"));
    push_scope(a, (Au)switch_cat);

    // allocate cats for each case, do NOT build the body yet
    // wrap in cat, store the catcher, not an enode
    map case_blocks = map(hsize, 16);
    pairs(cases, i)
        set(case_blocks, i->key, (Au)catcher(mod, a, team, case_blocks, block, LLVMAppendBasicBlockInContext(a->module_ctx, entry, "case")));

    // default block, and obtain insertion block for first case
    catcher def_cat = def_block ? catcher(mod, a, block, LLVMAppendBasicBlockInContext(a->module_ctx, entry, "default")) : null;
    LLVMBasicBlockRef cur = LLVMGetInsertBlock(B);

    pairs(case_blocks, i) {
        array   key_expr = (array)  i->key;
        catcher case_cat = (catcher)i->value;

        // position at current end & compare
        LLVMPositionBuilderAtEnd(B, cur);
        // enode aether_e_cmp(aether a, enode L, enode R)
        enode case_val = (enode)invoke(expr_builder, (Au)key_expr);
        enode cmp = e_cmp(a, switch_val, case_val);
        LLVMValueRef eq = LLVMBuildICmp(B, LLVMIntEQ, cmp->value,
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0), "case.eq");

        // next block in chain
        LLVMBasicBlockRef next =
            (i->next)
                ? LLVMAppendBasicBlockInContext(a->module_ctx, entry, "case.next")
                : (def_cat ? def_cat->block : switch_cat->block);

        LLVMBuildCondBr(B, eq, case_cat->block, next);
        cur = next;
    }

    // ---- CASE bodies ----
    int i = 0;
    pairs(case_blocks, p) {
        catcher case_cat = (catcher)p->value;

        LLVMPositionBuilderAtEnd(B, case_cat->block);
        array body_tokens = (array)value_by_index(cases, i++);
        invoke(body_builder, (Au)body_tokens);

        // if the case didn’t terminate (break/return), jump to merge
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)))
            LLVMBuildBr(B, switch_cat->block);
    }

    // ---- DEFAULT body ----
    if (def_cat) {
        LLVMPositionBuilderAtEnd(B, def_cat->block);
        invoke(body_builder, (Au)def_block);
        LLVMBuildBr(B, switch_cat->block);
    }

    // ---- MERGE ----
    LLVMPositionBuilderAtEnd(B, switch_cat->block);
    pop_scope(a);
    return e_noop(a, null);
}

// the iterator requires abi compatibility with the types, and will not call conversion in the loop
// its important we don't unknowingly create performance liabilities
enode aether_e_for(aether a,
                   array init_exprs,
                   array cond_exprs,
                   array body_exprs,
                   array step_exprs,
                   subprocedure init_builder,
                   subprocedure cond_builder,
                   subprocedure body_builder,
                   subprocedure step_builder,
                   bool do_while,
                   enode in_expr,
                   evar val_var,
                   evar key_var)
{
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, null);

    // set debug location for loop entry
    debug_emit(a);

    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);
    LLVMBasicBlockRef cond  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "for.cond");
    LLVMBasicBlockRef body  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "for.body");
    LLVMBasicBlockRef step  = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "for.step");
    LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "for.end");

    catcher cat = catcher(mod, a, block, merge);
    bool saved_cov = a->coverage;
    a->coverage = false;
    push_scope(a, (Au)cat);
    a->coverage = saved_cov;

    if (in_expr && (inherits(in_expr->au, typeid(map)) || inherits(in_expr->au, typeid(array)))) {
        debug_emit(a);
        if (a->iterator_guard) {
            // ---- null guard: skip loop if collection is null ----
            LLVMValueRef coll_ptr = in_expr->value;
            LLVMValueRef is_null  = LLVMBuildICmp(B, LLVMIntEQ, coll_ptr,
                LLVMConstNull(LLVMTypeOf(coll_ptr)), "coll.null");
            LLVMBasicBlockRef not_null = LLVMAppendBasicBlockInContext(
                a->module_ctx, fn, "for.not_null");
            LLVMBuildCondBr(B, is_null, merge, not_null);
            LLVMPositionBuilderAtEnd(B, not_null);
        }
    }

    statements st = context_code(a);

    if (in_expr && inherits(in_expr->au, typeid(map))) {
        // ---- map iteration via item linked list ----
        etype item_type = etypeid(item);

        enode first_node = etype_access((etype)in_expr, string("first"), true);

        LLVMTypeRef  cursor_type = LLVMPointerType(item_type->lltype, 0);
        LLVMValueRef cursor      = LLVMBuildAlloca(B, cursor_type, "cursor");
        LLVMBuildStore(B, first_node->value, cursor);
        LLVMBuildBr(B, cond);

        // ---- cond: cursor != null ----
        LLVMPositionBuilderAtEnd(B, cond);
        LLVMValueRef cur_val  = LLVMBuildLoad2(B, cursor_type, cursor, "cur");
        LLVMValueRef null_val = LLVMConstNull(cursor_type);
        LLVMValueRef cmp      = LLVMBuildICmp(B, LLVMIntNE, cur_val, null_val, "has_next");
        LLVMBuildCondBr(B, cmp, body, merge);

        // ---- body: extract key/value, bind, run user body ----
        LLVMPositionBuilderAtEnd(B, body);

        // keep for-header debug loc for iteration var stores
        // so LLDB sees them populated before the body's first line
        enode cur_enode = value(pointer(a, (Au)item_type->au),
            LLVMBuildLoad2(B, cursor_type, cursor, "cur.body"));

        if (val_var) {
            enode val_node = etype_access((etype)cur_enode, string("value"), true);
            enode val_cast = e_create(a, canonical(val_var), (Au)val_node);
            LLVMBuildStore(B, val_cast->value, val_var->value);
        }

        if (key_var) {
            enode key_node = etype_access((etype)cur_enode, string("key"), true);
            enode key_cast = e_create(a, canonical(key_var), (Au)key_node);
            LLVMBuildStore(B, key_cast->value, key_var->value);
        }

        if (st && a->coverage) aether_emit_block_probe(a, st->probe_id);

        token saved_origin = a->statement_origin;
        invoke(body_builder, (Au)body_exprs);
        a->statement_origin = saved_origin;
        LLVMBuildBr(B, step);

        // ---- step: cursor = cursor->next ----
        LLVMPositionBuilderAtEnd(B, step);
        debug_emit(a);
        enode step_enode = value(pointer(a, (Au)item_type->au),
            LLVMBuildLoad2(B, cursor_type, cursor, "cur.step"));
        enode next_node = etype_access((etype)step_enode, string("next"), true);
        LLVMBuildStore(B, next_node->value, cursor);
        LLVMBuildBr(B, cond);

    } else if (in_expr && inherits(in_expr->au, typeid(array))) {

        // ---- array iteration via index ----
        enode count_node = etype_access((etype)in_expr, string("count"), true);
        enode origin     = etype_access((etype)in_expr, string("origin"), true);
        
        // alloca for index: i32 = 0
        LLVMTypeRef  i32_type = LLVMInt32Type();
        LLVMValueRef idx      = LLVMBuildAlloca(B, i32_type, "for.idx");
        LLVMBuildStore(B, LLVMConstInt(i32_type, 0, false), idx);
        LLVMBuildBr(B, cond);

        // ---- cond: idx < count ----
        LLVMPositionBuilderAtEnd(B, cond);
        LLVMValueRef idx_val   = LLVMBuildLoad2(B, i32_type, idx, "idx");

        LLVMValueRef count_val = LLVMBuildLoad2(B, i32_type, count_node->value, "count");
        LLVMValueRef cmp       = LLVMBuildICmp(B, LLVMIntSLT, idx_val, count_val, "idx.lt.count");
        LLVMBuildCondBr(B, cmp, body, merge);

        // ---- body: extract element, bind, run user body ----
        LLVMPositionBuilderAtEnd(B, body);
        if (st && a->coverage) aether_emit_block_probe(a, st->probe_id);

        LLVMValueRef idx_body = LLVMBuildLoad2(B, i32_type, idx, "idx.body");
        
        // origin[idx] -> Au pointer
        LLVMTypeRef ptr = LLVMPointerType(LLVMInt8Type(), 0);
        LLVMValueRef elem_ptr = LLVMBuildGEP2(B, 
            ptr,
            LLVMBuildLoad2(B, ptr, origin->value, "count"), &idx_body, 1, "elem.ptr");
        LLVMValueRef elem = LLVMBuildLoad2(B,
            ptr,
            elem_ptr, "elem");

        if (val_var) {
            enode val_node = value(etypeid(Au), elem);
            enode val_cast = e_create(a, canonical(val_var), (Au)val_node);
            LLVMBuildStore(B, val_cast->value, val_var->value);
        }

        if (key_var) {
            validate(key_var->au->src == typeid(i64) || key_var->au->src == typeid(u64),
                "array index-key must be castable to num/i64/u64");

            LLVMValueRef idx_i64 = LLVMBuildSExt(B, idx_body, LLVMInt64Type(), "idx.i64");
            enode idx_node = value(etypeid(i64), idx_i64);
            enode idx_cast = e_create(a, canonical(key_var), (Au)idx_node);
            LLVMBuildStore(B, idx_cast->value, key_var->value);
        }

        token saved_origin = a->statement_origin;
        invoke(body_builder, (Au)body_exprs);
        a->statement_origin = saved_origin;
        LLVMBuildBr(B, step);

        // ---- step: idx += 1 ----
        LLVMPositionBuilderAtEnd(B, step);
        debug_emit(a);
        LLVMValueRef idx_step = LLVMBuildLoad2(B, i32_type, idx, "idx.step");
        LLVMValueRef idx_next = LLVMBuildAdd(B, idx_step, 
            LLVMConstInt(i32_type, 1, false), "idx.inc");
        LLVMBuildStore(B, idx_next, idx);
        LLVMBuildBr(B, cond);
        
    } else {
        // ---- standard for loop (existing code) ----
        if (len(init_exprs))
            invoke(init_builder, (Au)init_exprs);

        if (do_while)
            LLVMBuildBr(B, body);
        else
            LLVMBuildBr(B, cond);

        // ---- cond ----
        LLVMPositionBuilderAtEnd(B, cond);
        enode cond_res = (enode)invoke(cond_builder, (Au)cond_exprs);
        LLVMValueRef cond_val = e_create(a, etypeid(bool), (Au)cond_res)->value;
        LLVMBuildCondBr(B, cond_val, body, merge);

        // ---- body ----
        LLVMPositionBuilderAtEnd(B, body);
        if (st && a->coverage) aether_emit_block_probe(a, st->probe_id);
        token saved_origin = a->statement_origin;
        invoke(body_builder, (Au)body_exprs);
        a->statement_origin = saved_origin;
        LLVMBuildBr(B, step);

        // ---- step ----
        LLVMPositionBuilderAtEnd(B, step);
        debug_emit(a);
        if (len(step_exprs))
            invoke(step_builder, (Au)step_exprs);
        LLVMBuildBr(B, cond);
    }

    // ---- end ----
    LLVMPositionBuilderAtEnd(B, merge);
    pop_scope(a);
    return e_noop(a, null);
}

enode aether_e_noop(aether a, etype mdl) {
    enode op = enode(mod, a, loaded, true, au, mdl ? mdl->au : etypeid(none)->au);
    return op;
}

enode aether_e_if_else(
    aether a,
    array conds,
    array exprs,
    subprocedure cond_builder,
    subprocedure expr_builder)
{
    // set debug location for if/else entry
    debug_emit(a);

    u32 ln_conds = (u32)len(conds);

    // if the parser stored a final `el` as an empty condition entry,
    // treat that as "final else" and do not compile it as a condition.
    bool conds_has_empty_tail = false;
    if (ln_conds > 0) {
        // in your case conds->origin[i] is itself an array (len == 0 for the final else)
        array last = (array)conds->origin[ln_conds - 1];
        if (last && len(last) == 0) {
            conds_has_empty_tail = true;
            ln_conds -= 1;
        }
    }

    bool has_final_else = (len(exprs) > ln_conds) || conds_has_empty_tail;

    verify(
        ln_conds == (u32)len(exprs) ||
        ln_conds + 1 == (u32)len(exprs),
        "mismatch between conditions and expressions");

    LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(B);
    LLVMValueRef fn = LLVMGetBasicBlockParent(cur_block);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "ifcont");

    for (u32 i = 0; i < ln_conds; i++) {

        LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "then");
        LLVMBasicBlockRef else_block = null;

        bool is_last = (i + 1 == ln_conds);
        if (is_last) {
            else_block = has_final_else
                ? LLVMAppendBasicBlockInContext(a->module_ctx, fn, "else")
                : merge;
        } else {
            else_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "else");
        }

        // build condition in the current block
        enode cond_node = (enode)invoke(cond_builder, conds->origin[i]);

        // coerce to bool and grab value
        enode cond_bool = e_create(a, etypeid(bool), (Au)cond_node);
        LLVMValueRef condition = cond_bool->value;

        LLVMBuildCondBr(B, condition, then_block, else_block);

        // then
        LLVMPositionBuilderAtEnd(B, then_block);
        invoke(expr_builder, exprs->origin[i]);

        LLVMBasicBlockRef end_then = LLVMGetInsertBlock(B);
        if (!LLVMGetBasicBlockTerminator(end_then))
            LLVMBuildBr(B, merge);

        // next condition (or final else)
        if (else_block != merge)
            LLVMPositionBuilderAtEnd(B, else_block);
        else
            LLVMPositionBuilderAtEnd(B, merge);
    }

    // final else body (exprs[ln_conds])
    if (has_final_else) {
        invoke(expr_builder, exprs->origin[ln_conds]);

        LLVMBasicBlockRef end_else = LLVMGetInsertBlock(B);
        if (!LLVMGetBasicBlockTerminator(end_else))
            LLVMBuildBr(B, merge);
    }

    LLVMPositionBuilderAtEnd(B, merge);

    return enode(
        mod,    a,          // note: 'mod' must exist in your scope; otherwise replace with your module ref
        loaded, false,
        au,     typeid(none),
        value,  null);
}

/*
enode aether_e_if_else(
    aether a,
    array conds,
    array exprs,
    subprocedure cond_builder,
    subprocedure expr_builder)
{
    int ln_conds = len(conds);
    bool has_else = len(exprs) > ln_conds;

    verify(
        ln_conds == len(exprs) - 1 ||
        ln_conds == len(exprs),
        "mismatch between conditions and expressions");

    LLVMBasicBlockRef block = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(block);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "ifcont");

    for (int i = 0; i < ln_conds; i++) {
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "then");
        LLVMBasicBlockRef else_block;
        
        // Last condition with no else goes directly to merge
        bool is_last = (i == ln_conds - 1);
        if (is_last && !has_else)
            else_block = merge;
        else
            else_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "else");

        // Build condition
        array tokens_cond = (array)conds->origin[i];
        enode cond_node = (enode)invoke(cond_builder, (Au)tokens_cond);
        LLVMValueRef condition = e_create(a, etypeid(bool), (Au)cond_node)->value;

        LLVMBuildCondBr(B, condition, then_block, else_block);

        // Then block
        LLVMPositionBuilderAtEnd(B, then_block);
        invoke(expr_builder, exprs->origin[i]);

        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B))) // would the insert b lock be different at this point from where we were before?
            LLVMBuildBr(B, merge);

        // Position for next iteration or else
        if (else_block != merge)
            LLVMPositionBuilderAtEnd(B, else_block);
    }

    // Else block
    if (has_else) {
        invoke(expr_builder, exprs->origin[ln_conds]);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)))
            LLVMBuildBr(B, merge);
    }

    LLVMPositionBuilderAtEnd(B, merge);

    return enode(
        mod,    a,
        loaded, false,
        au,     typeid(none),
        value,  null);
}
*/

enode aether_e_addr_of(aether a, enode expr, etype mdl) {
    fault("verify e_create usage e_create(a, (etype)expr, (Au)mdl);");
    return e_create(a, (etype)expr, (Au)mdl);
}

enode enode_load(enode e) {
    aether a = e->mod;
    LLVMTypeRef i8_ty = LLVMInt8TypeInContext(a->module_ctx);
    LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0);
    return enode(mod, a, au, e->au, loaded, true, value, LLVMBuildLoad2(B, ptr_ty, e->value, "instance_ptr"));
}

enode enode_shape(enode instance) {
    aether a = instance->mod;
    LLVMTypeRef i8_ty = LLVMInt8TypeInContext(a->module_ctx);
    LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0);
    
    // if instance is an alloca (unloaded), load the pointer first
    LLVMValueRef base = instance->value;
    if (!instance->loaded) {
        base = LLVMBuildLoad2(B, ptr_ty, base, "instance_ptr");
    }
    
    i64 shape_offset = -(i64)sizeof(struct _Au) + (i64)sizeof(void*);
    LLVMValueRef offset = LLVMConstInt(
        LLVMInt64TypeInContext(a->module_ctx), shape_offset, true);
    LLVMValueRef shape_field = LLVMBuildGEP2(B,
        i8_ty, base, &offset, 1, "shape_ptr");
    LLVMValueRef shape = LLVMBuildLoad2(B, ptr_ty, shape_field, "shape");
    return enode(mod, a, au, typeid(shape), loaded, true, value, shape);
}

enode aether_e_offset(aether a, enode n, Au offset) { sequencer
    Au_t au = au_arg_type((Au)n->au);
    if (a->no_build) return e_noop(a, (etype)evar_type((evar)n));

    enode  i = e_operand(a, offset, null);
    verify(is_ptr(n), "offset requires pointer");
    
    LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0);
    LLVMValueRef base = n->value;
    if (!n->loaded) {
        base = LLVMBuildLoad2(B, ptr_ty, base, "base_ptr");
    }
    
    
    Au_t elem_au = au->src;
    LLVMTypeRef elem_ty = lltype(u(etype, elem_au));
    
    char N[32];
    sprintf(N, "%i_offset", seq);
    LLVMValueRef ptr_offset = LLVMBuildGEP2(B,
        elem_ty, base, &i->value, 1, N);

    Au_t arg_type = au_arg_type((Au)n);
    
    return enode(mod, a, au, n->is_explicit_ref ? arg_type->src : arg_type, loaded, false, value, ptr_offset);
}

enode aether_e_load(aether a, enode mem, enode target) { sequencer
    a->is_const_op = false;
    etype  resolve   = resolve(mem);
    if (a->no_build) return e_noop(a, resolve);

    LLVMValueRef ptr = mem->value;
    verify(is_ptr(mem) || !mem->loaded, "expected pointer to load from, given %o", mem);
    
    string id = f(string, "eload_%i", seq);
    if (seq == 169) {
        seq = seq;
    }
    // if the variable holds a pointer (new, alloc), load as ptr, not element type
    Au_t mem_au = au_arg_type((Au)mem);
    if (mem_au->is_pointer) {
        LLVMTypeRef ptr_ty = LLVMPointerTypeInContext(a->module_ctx, 0);
        LLVMValueRef loaded = LLVMBuildLoad2(B, ptr_ty, mem->value, id->chars);
        enode r = enode(mod, a, loaded, true, value, loaded, au, mem_au);
        return r;
    }
    
    LLVMValueRef loaded = LLVMBuildLoad2(
        B, lltype(resolve), mem->value, id->chars);
    enode r = enode(mod, a, loaded, true, value, loaded, au, resolve->au);
    return r;
}

/// general signed/unsigned/1-64bit and float/double conversion
/// should NOT be loading, should absolutely be calling model_convertible -- why is it not?
enode aether_e_primitive_convert(aether a, enode expr, etype rtype) {
    if (!rtype) return expr;

    a->is_const_op &= expr->au == rtype->au; // we may allow from-bit-width <= to-bit-width
    if (a->no_build) return e_noop(a, rtype);

    //expr = e_load(a, expr, null); // i think we want to place this somewhere else for better structural use
    
    etype F = canonical(expr);
    etype T = canonical(rtype);

    // 
    if (is_prim(F->au) && T->au == typeid(string)) {
        // Calculate total size: Au header + primitive value
        u64 au_size = typeid(Au)->abi_size / 8;
        u64 prim_size = F->au->abi_size / 8;
        u64 total_size = au_size + prim_size;
        
        // Allocate Au + primitive space on stack
        LLVMTypeRef byte_array = LLVMArrayType(LLVMInt8TypeInContext(a->module_ctx), total_size);
        LLVMValueRef temp_alloca = LLVMBuildAlloca(B, byte_array, "au_prim_temp");
        
        // Cast to Au*
        LLVMTypeRef au_struct_type = etypeid(Au)->lltype;
        LLVMTypeRef au_ptr_type = LLVMPointerTypeInContext(a->module_ctx, 0);
        LLVMValueRef au_ptr = LLVMBuildBitCast(B, temp_alloca, au_ptr_type, "au_ptr");
        
        // Zero the whole thing
        LLVMValueRef zero = LLVMConstInt(LLVMInt8TypeInContext(a->module_ctx), 0, 0);
        LLVMValueRef size_val = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), total_size, 0);
        LLVMBuildMemSet(B, temp_alloca, zero, size_val, 0);
        
        // Set type field (index 0) directly
        LLVMValueRef type_gep = LLVMBuildStructGEP2(B, au_struct_type, au_ptr, 0, "type_ptr");
        enode type_val = e_typeid(a, F);
        LLVMBuildStore(B, type_val->value, type_gep);
        
        // Store primitive value at end (offset = au_size)
        LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), au_size, 0) };
        LLVMValueRef byte_ptr = LLVMBuildBitCast(B, temp_alloca, 
            LLVMPointerTypeInContext(a->module_ctx, 0), "");
        LLVMValueRef data_ptr = LLVMBuildGEP2(B, LLVMInt8TypeInContext(a->module_ctx), byte_ptr, indices, 1, "data_ptr");
        LLVMValueRef typed_ptr = LLVMBuildBitCast(B, data_ptr, 
            LLVMPointerTypeInContext(a->module_ctx, 0), "typed_ptr");
        LLVMBuildStore(B, expr->value, typed_ptr);
        
        // Call Au_cast_string
        Au_t fn_cast = find_member(typeid(Au), "cast_string", AU_MEMBER_CAST, 0, false);
        verify(u(efunc, fn_cast), "Au_cast_string not found");
        
        enode au_arg = enode(mod, a, value, au_ptr, loaded, false, au, typeid(Au));
        return e_fn_call(a, u(efunc, fn_cast), a(au_arg));
    }
    
    LLVMValueRef V = expr->value;

    if (F == T) return expr;  // no cast needed

    // LLVM type kinds
    LLVMTypeKind F_kind = LLVMGetTypeKind(lltype(F));
    LLVMTypeKind T_kind = LLVMGetTypeKind(lltype(T));

    // integer conversion
    if (F_kind == LLVMIntegerTypeKind &&  T_kind == LLVMIntegerTypeKind) {
        u32 F_bits = LLVMGetIntTypeWidth(lltype(F)), T_bits = LLVMGetIntTypeWidth(lltype(T));
        if (F_bits < T_bits) {
            bool is_s = is_sign(F);
            V = is_s ? LLVMBuildSExt(B, V, lltype(T), "sext")
                     : LLVMBuildZExt(B, V, lltype(T), "zext");
        } else if (F_bits > T_bits)
            V = LLVMBuildTrunc(B, V, lltype(T), "trunc");
        else if (is_sign(F) != is_sign(T))
            V = LLVMBuildIntCast2(B, V, lltype(T), is_sign(T), "int-cast");
        else
            V = expr->value;
    }

    // int to real
    else if (F_kind == LLVMIntegerTypeKind && (T_kind == LLVMFloatTypeKind || T_kind == LLVMDoubleTypeKind))
        V = is_sign(F) ? LLVMBuildSIToFP(B, V, lltype(T), "sitofp")
                         : LLVMBuildUIToFP(B, V, lltype(T), "uitofp");

    // real to int
    else if ((F_kind == LLVMFloatTypeKind || F_kind == LLVMDoubleTypeKind) && T_kind == LLVMIntegerTypeKind)
        V = is_sign(T) ? LLVMBuildFPToSI(B, V, lltype(T), "fptosi")
                         : LLVMBuildFPToUI(B, V, lltype(T), "fptoui");

    // real conversion
    else if ((F_kind == LLVMFloatTypeKind || F_kind == LLVMDoubleTypeKind) && 
             (T_kind == LLVMFloatTypeKind || T_kind == LLVMDoubleTypeKind))
        V = F_kind == LLVMDoubleTypeKind && T_kind == LLVMFloatTypeKind ? 
            LLVMBuildFPTrunc(B, V, lltype(T), "fptrunc") :
            LLVMBuildFPExt  (B, V, lltype(T), "fpext");

    // ptr conversion
    else if (is_ptr(F) && is_ptr(T))
        V = LLVMBuildPointerCast(B, V, lltype(T), "ptr_cast");

    // ptr to int
    else if (is_ptr(F) && is_integral(T))
        V = LLVMBuildPtrToInt(B, V, lltype(T), "ptr_to_int");

    // int to ptr
    else if (is_integral(F) && is_ptr(T))
        V = LLVMBuildIntToPtr(B, V, lltype(T), "int_to_ptr");

    // bitcast for same-size types
    else if (F_kind == T_kind)
        V = LLVMBuildBitCast(B, V, lltype(T), "bitcast2");

    else if (F_kind == LLVMVoidTypeKind)
        V = LLVMConstNull(lltype(T));
    else {
        // cast primitives to pointers of temporary; this works for target values however may be a bit lieniant for other cases
        bool make_temp = is_prim(F->au) && is_ptr(T) && resolve(T) == F;
        if (make_temp) {
            V = LLVMBuildAlloca(B, lltype(F), "prim_addr");
            LLVMBuildStore(B, expr->value, V);
        }
        if (!make_temp)
            make_temp = make_temp;
        verify(make_temp, "unsupported cast");
    }

    enode res = value(T, V);
    res->literal = hold(expr->literal);
    return res;
}

void aether_push_tokens(aether a, tokens t, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data state = tokens_data(tokens_list, (array)a->tokens, cursor, a->cursor);
    push(a->stack, (Au)state);

    if (t != a->tokens) {
        drop(a->tokens);
        a->tokens = hold(t);
    }
    a->cursor = cursor;
    a->stack_test++;
}

void aether_pop_tokens(aether a, bool transfer) {
    int len = a->stack->count;
    assert(len, "expected stack");
    tokens_data state = (tokens_data)last_element(a->stack);
    a->stack_test--;

    if (!transfer) {
        a->cursor = state->cursor;
        if (state->tokens_list != a->tokens) {
            drop(a->tokens);
            a->tokens = (tokens)hold(state->tokens_list);
        }
    } else {
        if (state->tokens_list != a->tokens) {
            a->cursor += state->cursor;
            drop(state->tokens_list);  // we're keeping a->tokens, so release the saved one
        }
    }
    pop(a->stack);
}

define_class(tokens_data, Au)

void aether_push_current(aether a) {
    push_tokens(a, a->tokens, a->cursor);
}

static etype etype_deref(aether a, Au_t au) {
    return au->src ? u(etype, au->src) : null;
}

etype get_type_t_ptr(etype t);

void aether_create_type_members(aether a, Au_t ctx) {
    ctx = ctx;

    // we must do this after building records / enums (in silver)
    members(ctx, mem) {
        if (mem->member_type == AU_MEMBER_VAR)
            mem = mem->src;
        if (mem == typeid(Au_t)) continue;
        etype e = u(etype, mem);
        if (mem->ident && is_au_type(mem) && e && !e->type_id && !mem->is_system && !mem->is_schema) {
            get_type_t_ptr(e);
            implement_type_id(e);
        }
    }
}

void src_init(aether a, Au_t m) { sequencer
    if (m && m->src)
        src_init(a, m->src);
    etype tm = u(etype, m);
    Au info = head(tm);
    if (m && !tm) {
        int mt = m->member_type;

        if (is_func((Au)m)) {
            efunc(mod, a, loaded, true,  au, m);
        }
        else if (mt == AU_MEMBER_ENUMV || mt == AU_MEMBER_VAR || mt == AU_MEMBER_IS_ATTR)
            set(a->registry, (Au)m, (Au)hold(enode(mod, a, loaded, false, au, m)));
        else {
            etype t = etype(mod, a, au, m); // this should be held in it's init
            Au info = head(t);
            info = info;
        }
    }
}

array etype_class_list(etype t) {
    aether a = t->mod;
    array res = array(alloc, 32, assorted, true);
    Au_t src = t->au;
    while (src) {
        src_init(a, src);
        verify(u(etype, src), "etype (user) not set for %s", src->ident);
        push(res, (Au)u(etype, src));
        if (src->context == src)
            break;
        src = src->context;
    }
    return reverse(res);
}

static void set_global_construct(aether a, efunc fn) {
    LLVMModuleRef mod = a->module_ref;

    LLVMValueRef ctor_func = fn->value;  // LLVM function
    LLVMTypeRef  int32_t   = LLVMInt32TypeInContext(a->module_ctx);
    LLVMTypeRef  i8ptr_t   = LLVMPointerTypeInContext(a->module_ctx, 0);

    // Priority: 65535 is lowest priority (runs last); you can choose another.
    LLVMValueRef priority = LLVMConstInt(int32_t, 65535, false);
    LLVMValueRef null_ptr = LLVMConstNull(i8ptr_t);

    // Construct struct { i32 priority, void()* fn, i8* data }
    LLVMTypeRef ctor_type = LLVMStructTypeInContext(a->module_ctx,
        (LLVMTypeRef[]){
            int32_t,
            LLVMPointerTypeInContext(a->module_ctx, 0),
            i8ptr_t
        },
        3, false
    );

    LLVMValueRef ctor_entry = LLVMConstStructInContext(a->module_ctx,
        (LLVMValueRef[]){
            priority,
            ctor_func,
            null_ptr
        },
        3, false
    );

    // Create array [1 x { i32, fn, i8* }]
    LLVMTypeRef array_type = LLVMArrayType(ctor_type, 1);
    LLVMValueRef array = LLVMConstArray(ctor_type, &ctor_entry, 1);

    // Create or fetch global named llvm.global_ctors
    LLVMValueRef global = LLVMGetNamedGlobal(mod, "llvm.global_ctors");
    if (!global) {
        global = LLVMAddGlobal(mod, array_type, "llvm.global_ctors");
    }

    LLVMSetInitializer(global, array);
    LLVMSetLinkage(global, LLVMAppendingLinkage);
    LLVMSetGlobalConstant(global, false);
}

static void build_entrypoint(aether a, efunc module_init_fn) {
    Au_t module_base = a->au;

    etype main_spec  = etypeid(app);
    verify(main_spec && is_class(main_spec), "expected app class");

    etype main_class = null;
    members(module_base, mem) {
        if (is_class(mem)) {
            etype cls = u(etype, mem);
            if (cls->au->context == main_spec->au) {
                verify(!main_class, "found multiple app classes");
                main_class = cls;
            }
        }
    }

    // no main for library
    if (!main_class) {
        set_global_construct(a, module_init_fn);
        a->is_library = true;
        return;
    }

    // for apps, build int main(argc, argv)
    Au_t au_f_main = def_member(a->au, "main", typeid(i32), AU_MEMBER_FUNC, 0);
    Au_t argc = def_arg(au_f_main,  "argc", typeid(i32), 0);
    Au_t argv = def_arg(au_f_main,  "argv", typeid(cstrs), 0);

    au_f_main->is_export = true;
    efunc main_fn = efunc(mod, a, au, au_f_main,
        loaded, true, used, true, has_code, true);
    etype_implement((etype)main_fn, false);

    push_scope(a, (Au)main_fn);
    e_fn_call(a, module_init_fn, null);

    // call engage
    efunc Au_engage = u(efunc, find_member(typeid(Au), "engage", AU_MEMBER_FUNC, 0, false));
    e_fn_call(a, Au_engage, a(u(evar, argv)));

    // create main class ( i think this is not working )
    enode m = e_create(a, main_class, (Au)u(evar, argv)); // a loop would be nice, since none of our members will be ref+1'd yet
    Au_t fn_run = find_member(main_spec->au, "run", AU_MEMBER_FUNC, 0, false);
    
    enode r = e_fn_call(a, (efunc)u(efunc, fn_run), a(m));

    // give a full-coverage-report to number one
    report_coverage(a);

    e_fn_return(a, (Au)r);
    pop_scope(a);
}

Au_t arg_type(Au_t au) {
    Au_t res = au;
    while (res) {
        if (res->member_type != AU_MEMBER_TYPE) {
            res = res->src;
            continue;
        }
        break;
    }
    //verify(res, "argument not resolving type: %s", au);
    return res;
}

etype etype_canonical(etype t) {
    aether a = t->mod;
    Au_t au = t->au;
    if (au->member_type == AU_MEMBER_ENUMV)
        return u(etype, au->src);
    if (au->member_type == AU_MEMBER_VAR)
        au = au->src;
    if (au->is_typeid && au->src)
        au = au->src;

    while (au && au->src) {
        etype e = u(etype, au);
        if (e && lltype(e))
            break;
        au = au->src;
    }

    // weed out cases where rogue pointers are made
    etype au_user = (false && au->is_pointer && au->is_primitive && au->src && au->src->ptr &&
        (au->src->is_integral || au->src->is_realistic)) ?
        u(etype, au->src->ptr) : u(etype, au);
    etype res = (au_user && lltype(au_user)) ? au_user : null;
    return res;
}

// if given an enode, will always resolve the etype instance
etype etype_resolve(etype t) {
    aether a = t->mod;
    Au_t au = t->au;

    if (is_func(au) || au->member_type == AU_MEMBER_MACRO)
        return u(etype, au);

    if (au->member_type == AU_MEMBER_VAR)
        au = au->src;
    
    while (au && !au->is_funcptr && au->src) {
        au = au->src;
        etype au_user = u(etype, au);
        if (au_user && au_user->lltype)
            break;
    }

    etype au_user = u(etype, au);
    if (au->is_abstract) return au_user;

    Au info = head(au_user);
    return (au_user && lltype(au_user)) ? au_user : null;
}

etype struct_from_au(aether a, string name, Au_t au, bool is_system) {
    Au_t struct_au = def(a->au, name->chars, AU_MEMBER_TYPE, AU_TRAIT_STRUCT | (is_system ? AU_TRAIT_SYSTEM : 0));
    int  index = 0;

    for (int i = 0; i < au->members.count; i++) {
        Au_t arg = (Au_t)au->members.origin[i];
        Au_t mem = def_member(struct_au, arg->ident, arg->src, AU_MEMBER_VAR, 0);
        mem->index = index++;
    }

    if (is_class((Au)au->context)) {
        Au_t mem = def_member(
            struct_au, "a", is_module(au->context) ? typeid(Au) : au->context,
            AU_MEMBER_VAR, AU_TRAIT_IS_TARGET);
        mem->index = index++;
    }

    etype result = etype(mod, a, au, struct_au);
    etype_implement(result, false);
    return result;
}

// 
none push_lambda_members(aether a, efunc f) {
    if (a->no_build) return;
    statements lambda_code = statements(mod, (aether)a);
    push_scope(a, (Au)lambda_code);
    
    Au_t fn = f->au;
    LLVMValueRef context_ptr = f->context_node->value;
    LLVMTypeRef  ctx_struct  = u(etype, f->context_node->au->src)->lltype;

    // Extract each context member from the context struct
    int ctx_index = 0;
    members(fn, mem) {
        if (mem->member_type != AU_MEMBER_VAR) continue;
        
        Au_t ctx_au = def_member(lambda_code->au, mem->ident, mem->src, AU_MEMBER_VAR, mem->traits);
        evar ctx_evar = evar(mod, (aether)a, au, ctx_au);
        set(a->registry, (Au)ctx_au, (Au)hold(ctx_evar));
        
        LLVMValueRef indices[2] = {
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0),
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), ctx_index, 0)
        };
        LLVMValueRef field_ptr = LLVMBuildGEP2(
            B, ctx_struct, context_ptr, indices, 2, mem->ident);
        LLVMValueRef loaded = LLVMBuildLoad2(
            B, lltype(u(etype, mem->src)), field_ptr, mem->ident);
        
        ctx_evar->value  = loaded;
        ctx_evar->loaded = true;
        ctx_index++;
    }
}


// this is the declare (this comment stays)
none etype_init(etype t) {
    if (t->mod == null) t->mod = (aether)instanceof(t, aether);
    aether a = t->mod; // silver's mod will be a delegate to aether, not inherited
    
    t->iteration = a->iteration;
    Au_t au_store = typeid(store);
    if (!a->registry)
        a->registry = store();

    bool is_module = isa(t) == typeid(aether) || isa(t)->context == typeid(aether);
    if (!is_module && !t->au)
        t->au = def(a->au, null, AU_MEMBER_NAMESPACE, 0);

    Au_t    au  = t->au;
    bool  named = au && au->ident && strlen(au->ident);
    enode     n = instanceof(t, enode);

    if (au && au->ident && strstr(au->ident, "index_num"))
        au = au;

    if (t->lltype && !t->is_schema || (n && n->value && n->symbol_name))
        return;

    if (is_module) {
        a = (aether)t;
        verify(a->module && len(a->module), "no module provided");
        string n = a->name ? a->name : stem(a->module);
        a->name = hold(n);
        //au = t->au = def_module(a->name->chars);
        //if (!u(etype, au)) {
        //    set(a->registry, (Au)au, (Au)hold(t));
        //}
        return;
    } else if (t->is_schema) {
        if (!au->ident)
            au->ident = au->ident;

        if (au->ident && strstr(au->ident, "test4"))
            au = au;
            
        au = t->au = def(typeid(Au),
            fmt("__%o_%s",
                symbol_name((Au)string(au->ident)),
                is_module(au) ? "module_f" : "f")->chars,
            AU_MEMBER_TYPE, AU_TRAIT_SCHEMA | AU_TRAIT_STRUCT);
        
        // do this for Au types
        Au_t ref = typeid(Au_t);
        for (int i = 0; i < ref->members.count; i++) {
            Au_t au_mem  = (Au_t)ref->members.origin[i];

            // this is the last member (function table), if that changes, we no longer break
            if (au_mem->ident && strcmp(au_mem->ident, "ft") == 0) {
                Au_t ft_type = def(au, "ft", AU_MEMBER_TYPE, AU_TRAIT_STRUCT);
                Au_t ft_var = def(au, "ft", AU_MEMBER_VAR, 0);
                ft_var->src = ft_type;

                Au_t fn_first = def(ft_type, "_none_", AU_MEMBER_VAR, 0);
                fn_first->src  = typeid(ARef);

                if (strstr(au->ident, "test4"))
                    au = au;

                array cl = etype_class_list(t); Au_t ref = typeid(Au_t);
                each (cl,  etype, tt) {
                    for (int ai = 0; ai < tt->au->members.count; ai++) {
                        Au_t ai_mem = (Au_t)tt->au->members.origin[ai];
                        if (!is_func(ai_mem))
                            continue;

                        Au_t fn = def(ft_type, ai_mem->ident, AU_MEMBER_VAR, 0);
                        fn->src = typeid(ARef); // these are effectively opaque in design

                        //set(a->registry, (Au)fn, enode(mod, a, au, fn));

                        for (int arg = 0; arg < ai_mem->args.count; arg++) {
                            Au_t arg_src = (Au_t)ai_mem->args.origin[arg];
                            Au_t arg_t   = arg_type(arg_src);
                            micro_push(&fn->args, (Au)def_arg(fn, arg_src->ident, arg_t, 0));
                        }
                    }
                }

                etype ft = etype(mod, a, au, ft_type);
                break;
            }
            Au_t new_mem      = def(au, au_mem->ident, au_mem->member_type, au_mem->traits);
            new_mem->src      = au_mem->src;
            new_mem->isize    = au_mem->isize;
            new_mem->elements = au_mem->elements;
            new_mem->typesize = au_mem->typesize;
            new_mem->abi_size = au_mem->abi_size;
            new_mem->offset   = au_mem->offset;

            members(au_mem, mem)
                micro_push(&new_mem->members, (Au)mem);

            arg_list(au_mem, mem)
                micro_push(&new_mem->args,    (Au)mem);
            
            new_mem->context = t->au; // copy entire member and reset for our for context
        }

        // upon implement, we can register the static_value on etype of schema
        // this is the address of the &type_i.type member
        
    }
    
    if (!au->member_type)
        au->member_type = AU_MEMBER_TYPE;

    // we register import (enamespace) ourselves
    if (!u(etype, au) && isa(t) != typeid(enode) && !instanceof(t, enamespace)) {
        set(a->registry, (Au)au, (Au)hold(t));
    }

    Au_t_f* au_t = (Au_t_f*)isa(au);

    if (is_func((Au)au)) {
        int   is_inst = au->is_imethod;
        efunc fn      = (efunc)t;
        int   n_args  = fn->au->args.count;
        int   index   = 0;
        
        // If this needs a wrapper, append _generated to avoid collision with C function
        if (t->remote_code && au->alt && (strcmp(au->alt, "init") == 0 || strcmp(au->alt, "dealloc") == 0)) {
            // normal implemented-else-where does not do this; this is for method proxying 
            fn->remote_func = efunc(mod, a, au,
                def(au->context, au->ident, au->member_type, au->traits));

            // now we have to modify our own name; our own method implementation will call the user function (after a preamble)
            au->alt = cstr_copy((cstr)fmt("%s_generated",
                au->alt ? au->alt : au->ident)->chars);
        }

        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));

        if (is_lambda(au) && is_class(au->context))
            index = 1; // context inserted at [0]; gives us the target as well as the other args
        
        arg_list(au, arg) {
            Au_t t = arg->src;

            // register type if we have not
            if (!u(etype, t)) etype(mod, a, au, t);

            bool convert_prim_target = is_prim(t) && arg->is_target;
            arg_types[index++] = lltype(convert_prim_target ? pointer(a, (Au)t) : u(etype, t));
        }

        if (is_lambda(au)) {
            string context_name = f(string, "%s_%s_context", au->context->ident, au->ident);
            etype etype_context = struct_from_au(a, context_name, au, false);
            fn->context_node = hold(enode(mod, a, au, pointer(a, (Au)etype_context)->au, loaded, true, value, null));
            set(a->registry, (Au)fn->context_node->au, hold(fn->context_node));

            // create published type (we set this global on global initialize)
            string member_symbol = f(string, "%s_type", au->alt);
            LLVMTypeRef type = u(etype, typeid(Au_t)->ptr)->lltype;
            LLVMValueRef G = LLVMAddGlobal(a->module_ref, type, member_symbol->chars);
            LLVMSetLinkage(G, LLVMInternalLinkage);
            LLVMSetInitializer(G, LLVMConstNull(type));

            fn->published_type = enode(mod, a, symbol_name, member_symbol, value, G, au, typeid(Au_t));
            arg_types[0] = lltype(fn->context_node);
            n_args++;
        }

        fn->lltype = LLVMFunctionType(
            au->rtype ? lltype(u(etype, au->rtype)) : LLVMVoidTypeInContext(a->module_ctx),
            arg_types, n_args, au->is_vargs);

        etype_ptr(a, au, null);
        free(arg_types);

    } else if (is_func_ptr((Au)au)) {
        etype fn = t;

        int n_args = au->args.count;
        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));
        int index = 0;
        arg_list(au, arg)
            arg_types[index++] = lltype(u(etype, arg));

        LLVMTypeRef fn_ty = LLVMFunctionType(
            au->rtype ? lltype(u(etype, au->rtype)) : LLVMVoidTypeInContext(a->module_ctx),
            arg_types, au->args.count, au->is_vargs);

        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
        free(arg_types);
    } else if (au && au->is_pointer && au->src && !au->src->is_primitive) {
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (named && (!instanceof(t, enode) && (is_rec((Au)t) || au->is_union || au == typeid(Au_t)))) {
        t->lltype = LLVMStructCreateNamed(a->module_ctx, cstr_copy(au->ident));
        if (au != typeid(Au_t) && !au->is_system)
            etype_ptr(a, t->au, null);
    } else if (is_enum(t)) {
        t->lltype = lltype(au->src ? u(etype, au->src) : etypeid(i32));
    }
    else if (au == typeid(f32))  t->lltype = LLVMFloatTypeInContext(a->module_ctx);
    else if (au == typeid(f64))  t->lltype = LLVMDoubleTypeInContext(a->module_ctx);
    else if (au == typeid(none)) t->lltype = LLVMVoidTypeInContext(a->module_ctx);
    else if (au == typeid(bool)) t->lltype = LLVMInt1TypeInContext(a->module_ctx);
    else if (au == typeid(i8)  || au == typeid(u8))
        t->lltype = LLVMInt8TypeInContext(a->module_ctx);
    else if (au == typeid(i16) || au == typeid(u16))
        t->lltype = LLVMInt16TypeInContext(a->module_ctx);
    else if (au == typeid(i32) || au == typeid(u32) || au == typeid(AFlag) || au == typeid(uchar))
        t->lltype = LLVMInt32TypeInContext(a->module_ctx);
    else if (au == typeid(i64) || au == typeid(u64) || au == typeid(num))
        t->lltype = LLVMInt64TypeInContext(a->module_ctx);
    else if (au == typeid(symbol) || au == typeid(cstr) || au == typeid(raw)) {
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(cstrs)) {
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(sz)) {
        t->lltype = LLVMIntPtrTypeInContext(a->module_ctx, a->target_data);
    } else if (au == typeid(cereal)) {
        LLVMTypeRef cereal_type = LLVMStructCreateNamed(a->module_ctx, "cereal");
        LLVMTypeRef members[] = {
            LLVMPointerTypeInContext(a->module_ctx, 0)  // char* → i8*
        };
        LLVMStructSetBody(cereal_type, members, 1, 1);
        t->lltype = cereal_type;
    } else if (au == typeid(micro)) {
        LLVMTypeRef micro_type = LLVMStructCreateNamed(a->module_ctx, "micro");
        LLVMTypeRef members[] = {
            LLVMPointerTypeInContext(a->module_ctx, 0),
            LLVMInt32TypeInContext(a->module_ctx),
            LLVMInt32TypeInContext(a->module_ctx)
        };
        LLVMStructSetBody(micro_type, members, 3, 1);
        t->lltype = micro_type;
    } else if (au == typeid(meta_t)) {
        LLVMTypeRef meta_t_type = LLVMStructCreateNamed(a->module_ctx, "meta_t");
        LLVMTypeRef members[] = {
            LLVMPointerTypeInContext(a->module_ctx, 0),
            LLVMPointerTypeInContext(a->module_ctx, 0)
        };
        LLVMStructSetBody(meta_t_type, members, 2, 1);
        t->lltype = meta_t_type;
    } else if (au == typeid(floats_t)) {
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(func)) {
        LLVMTypeRef fn_type = LLVMFunctionType(LLVMVoidTypeInContext(a->module_ctx), NULL, 0, 0);
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(hook)) {
        Au_t e_A = au_lookup("Au");
        if (!u(etype, e_A)) {
            fault("issue");
            //e_A->user = etype(mod, a, au, e_A);
            etype_implement((etype)u(etype, e_A), false); // move this to proper place
        }
        LLVMTypeRef param_types[] = { lltype(e_A) };
        LLVMTypeRef hook_type = LLVMFunctionType(lltype(e_A), param_types, 1, 0);
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(callback)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(
            lltype(e_A), param_types, 2, 0);
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    } else if (au == typeid(callback_extra)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(lltype(e_A), param_types, 3, 0);
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    }
    else if (au == typeid(ref_u8)  || au == typeid(ref_u16) || 
             au == typeid(ref_u32) || au == typeid(ref_u64) || 
             au == typeid(ref_i8)  || au == typeid(ref_i16) || 
             au == typeid(ref_i32) || au == typeid(ref_i64) || 
             au == typeid(ref_f32) || au == typeid(ref_f64) || 
             au == typeid(ref_bool)) {
        verify(au->src, "expected src on reference type");
        Au_t b  = au->src;
        b->ptr  = au;
        au->src = b;
        au->src->ptr = au;
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
        au->is_pointer = true;
    }
    else if (au == typeid(handle))   t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    else if (au == typeid(ARef)) {
        au->src = typeid(Au);
        au->src->ptr = au;
        etype au_type = etypeid(Au);
        t->lltype = LLVMPointerTypeInContext(a->module_ctx, 0);
    }
    else if (au == typeid(Au_ts))    t->lltype = lltype(etypeid(Au_ts));
    else if (au == typeid(bf16))     t->lltype = LLVMBFloatTypeInContext(a->module_ctx);
    else if (au == typeid(fp16))     t->lltype = LLVMHalfTypeInContext(a->module_ctx);
    else if (au->is_pointer) {
        if (!au->src) {
            t->lltype       = LLVMPointerTypeInContext(a->module_ctx, 0);
        } else {
            etype mdl_src  = null;
            cstr  src_name = null;
            string n       = string(au->ident);
            Au_t   src     = au->src;
            src_name       = src->ident;
            verify(u(etype, src) && lltype(u(etype, src)), "type must be created before %o: %s", n, src_name);
            t->lltype      = LLVMPointerTypeInContext(a->module_ctx, 0);
            src->ptr       = au;
        }
    } else if ((au->traits & AU_TRAIT_ABSTRACT) == 0) {
        
    } else if (!au->is_abstract) {
        fault("not intializing %s", au->ident);
    }
}

bool is_accessible(aether a, Au_t m) {
    if (m->access_type == interface_undefined || 
        m->access_type == interface_public    || a->au->module == a->au_module)
        return true;
    return false;
}

Au_t read_arg_type(Au_t stored_arg) {
    if (stored_arg->member_type == AU_MEMBER_VAR) return stored_arg->src;
    return stored_arg;
}

etype get_type_t_ptr(etype t) {
    aether a = t->mod;

    if (t->schema)
        return u(etype, t->schema->au->ptr);

    // schema must have it's static_value set to the instance
    etype schema = etype(mod, a, au, t->au, is_schema, true);
    t->schema = hold(schema);
    
    etype mt = etype_ptr(a, schema->au, null);
    return mt;
}

etype implement_type_id(etype t) {
    aether a = t->mod;
    if (t->schema) {
        etype tt = u(etype, t->au);
        if (t && tt->type_id)
            return u(etype, t->schema->au->ptr);
    }

    if (t->au->ident && strstr(t->au->ident, "app")) {
        t = t;
    }
    
    Au_t au = t->au;

    // schema must have it's static_value set to the instance
    if (!t->schema)
        t->schema = hold(etype(mod, a, au, t->au, is_schema, true));

    etype mt = etype_ptr(a, t->schema->au, null);
    mt->au->is_typeid = true;
    mt->au->is_imported = a->is_Au_import;
    
    Au_t au_type = au_lookup("Au");
    verify(au_type->ptr, "expected ptr type on Au");

    // we need to create the %o_i instance inline struct
    bool is_module = au->member_type == AU_MEMBER_MODULE;
    string n = f(string, "%s_%s", au->ident, is_module ? "module" : "info");
    Au_t type_info = def_type  (a->au, n->chars, AU_TRAIT_STRUCT | AU_TRAIT_SYSTEM);
    Au_t type_h    = def_member(type_info, "info", au_type, AU_MEMBER_VAR, AU_TRAIT_SYSTEM | AU_TRAIT_INLAY);
    Au_t type_f    = def_member(type_info, "type", t->schema->au, AU_MEMBER_VAR, AU_TRAIT_SYSTEM | AU_TRAIT_INLAY);
    type_f->index = 1;
    etype au_t = etype(mod, a, au, type_info);

    etype_implement(au_t, false);
    
    string name = is_module(au) ?
        f(string, "%s_m", au->ident) :
        f(string, "%s_%s_i", au->module->ident, au->ident);
    evar schema_i = evar(mod, a, au, def_member(
                a->au, name->chars, type_info, AU_MEMBER_VAR,
                AU_TRAIT_SYSTEM | (a->is_Au_import ? AU_TRAIT_IS_IMPORTED : 0)));
    
    if (strcmp(au->ident, "test4") == 0) {
        int test2 = 2;
        test2    += 2;
    }
    etype_implement((etype)schema_i, false);
    
    etype tt = u(etype, t->au);
    tt->type_id = hold(access(schema_i, string("type"), true));
    if (!a->is_Au_import && t != a) {
        set(a->user_type_ids, (Au)t, (Au)tt->type_id);
    }
    return mt;
}

none etype_implement(etype t, bool w) {
    Au_t    au = t->au;
    aether a  = t->mod;
    enode nn = (enode)instanceof(t, enode);
    Au_t      top       = top_scope(a);
    aether    module    = is_module(top) ? a : null;

    if (au->member_type == AU_MEMBER_NAMESPACE || (is_func(au) && !((enode)t)->used && !a->is_Au_import))
        return;
    Au commander_solo = head(t);

    if (au->ident && strcmp(au->ident, "rng") == 0) {
        au = au;
    }

    if (au->traits & AU_TRAIT_ENUM) {
        if (!lltype(t) && au->src)
            t->lltype = lltype(u(etype, au->src));
        au->typesize = au->src->typesize;
        for (int i = 0; i < au->members.count; i++) {
            Au_t mem = (Au_t)au->members.origin[i];
            if (mem->member_type != AU_MEMBER_ENUMV) continue;
            enode n = u(enode, mem);
            if (n && !n->value) {
                Au lit = n->literal;
                if (!lit && mem->value && au->src)
                    lit = primitive(au->src, mem->value);
                if (lit) {
                    enode prim = e_operand_primitive(a, lit);
                    n->value   = e_create(a, u(etype, au->src), (Au)prim)->value;
                    n->loaded  = true;
                }
            }
        }
    }

    if (t->is_implemented) return;
    t->is_implemented = true;

    bool is_Au = !a->import_c && is_au_type(resolve(t)) && (!au->is_schema && !au->is_system && !au->is_pointer && au->ident);
    etype type_t_ptr = is_Au ? get_type_t_ptr(t) : null;

    bool is_ARef = au == typeid(ARef);
    if (!is_ARef && au && au->src && u(etype, au->src) && !is_prim(au->src) && isa(u(etype, au->src)) == typeid(etype))
        if (au->src != typeid(Au_t) && !au->is_funcptr)
            etype_implement(u(etype, au->src), false);

    if (au->member_type == AU_MEMBER_VAR) {
        enode n = (enode)instanceof(t, enode);
        verify(n, "expected enode instance for AU_MEMBER_VAR");
        LLVMTypeRef type = lltype(u(etype, au->src));

        // if module member, then its a global value
        if (!au->context->context) {
            // mod->is_Au_import indicates this is coming from a lib, or we are making a new global
            bool is_external = ((module && au->access_type == interface_public) || a->is_Au_import || au->is_system);
            LLVMValueRef G = is_external ?
                LLVMFetchGlobal(a->module_ref, type, au->ident) : LLVMAddGlobal(a->module_ref, type, au->ident);
            LLVMLinkage linkage = is_external ? 
                LLVMExternalLinkage : LLVMInternalLinkage;
            LLVMSetLinkage(G, linkage);
            if (!a->is_Au_import) {
                LLVMSetInitializer(G, LLVMConstNull(type)); // or a real initializer
            }
            n->loaded = false;
            n->value = G; // it probably makes more sense to keep llvalue on the node
            if (!is_external)
                emit_debug_global(a, au, G);
        } else if (!is_func(au->context) || au->is_static) {
            // we likely need to check to see if our context is a function
            // these are stack members, and indeed inside functions! they are just not in the 'args'
            verify(!n->value, "unexpected value set in enode");
            // if au->is_static then we need to create a global for it
            n->loaded = false;
            if (au->is_static && au->member_type == AU_MEMBER_VAR) {
                // this likely may just skip setting value; defer it to be set later
                evar static_node = instanceof(t, evar);
                verify(static_node, "expected evar");
                string c_name = f(string, "%s_%s", au->context->ident, au->ident);
                LLVMValueRef global = a->is_Au_import ? 
                    LLVMFetchGlobal(a->module_ref, type, c_name->chars) :
                    LLVMAddGlobal(a->module_ref, type, c_name->chars);
                LLVMSetLinkage(global, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
                if (!a->is_Au_import) LLVMSetInitializer(global, LLVMConstNull(type));
                static_node->value = global;
                static_node->loaded = false;
            } else if (!au->is_static) {
                char id[256];
                snprintf(id, 256, "evar_%s", au->ident ? au->ident : "");
                n->value = LLVMBuildAlloca(B, type, id);
                // declare local variable for debugger
                emit_debug_variable(a, n, 0, 0);
            }
        } else if (is_func(au->context)) {
            verify(n->value, "expected evar to be set for arg");
            verify(isa(t) == typeid(evar), "expected evar instance for arg");

            n->loaded = true;
        }
        return;

    }

    if (!au->is_pointer && !au->is_funcptr && (is_rec(t) || au->is_union)) {
        array cl = (au->is_union || is_struct(t)) ? a(t) : etype_class_list(t);
        bool multi_Au = len(cl) > 1 && cl->origin[0] == etypeid(Au);
        int count = 0;
        int index = 0;
        each(cl, etype, tt) {
            if (!multi_Au || tt->au != typeid(Au))
                for (int i = 0; i < tt->au->members.count; i++) {
                    Au_t m = (Au_t)tt->au->members.origin[i];
                    if (m->member_type == AU_MEMBER_VAR && !m->is_static && is_accessible(a, m))
                        count++;
                }

            if (is_class(t) && (!multi_Au || tt->au != typeid(Au)))
                count++; // u8 Type_interns[isize]
        }

        LLVMTypeRef* struct_members = calloc(count + 2, sizeof(LLVMTypeRef));
        LLVMTypeRef largest = null;
        int ilargest = 0;

        Au_t mtest = (Au_t) 0x7ffff7eb7c30;

        each(cl, etype, tt) {
            etype tt_entry = u(etype, tt);
            Au    tt_info  = head(tt_entry);
            //if (len(cl) > 1 && tt->au == typeid(Au)) break;
            int prop_index = 0;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = (Au_t)tt->au->members.origin[i];
                etype m_entry = u(etype, m);
                Au    m_info  = head(m_entry);
                
                if (m->member_type == AU_MEMBER_FUNC          ||
                    m->member_type == AU_MEMBER_CONSTRUCT     ||
                    m->member_type == AU_MEMBER_INDEX         ||
                    m->member_type == AU_MEMBER_SETTER        ||
                    m->member_type == AU_MEMBER_OPERATOR      ||
                    m->member_type == AU_MEMBER_CAST) {

                    src_init(a, m->rtype);
                    etype_implement(u(etype, m->rtype), false);

                    arg_types (m, t) {
                        src_init(a, t);
                        etype_implement(u(etype, t), false);
                    }

                    src_init(a, m);
                    etype_implement(u(etype, m), false);
                }
                else if (m->member_type == AU_MEMBER_VAR && is_accessible(a, m)) {

                    if (m->is_static ) {
                        string c_name = f(string, "%s_%s", au->ident, m->ident);
                        LLVMValueRef global = a->is_Au_import ?
                            LLVMFetchGlobal(a->module_ref, lltype(u(etype, m->src)), c_name->chars) :
                            LLVMAddGlobal(a->module_ref, lltype(u(etype, m->src)), c_name->chars);
                        LLVMSetLinkage(global, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
                        if (!a->is_Au_import) LLVMSetInitializer(global, LLVMConstNull(lltype(u(etype, m->src))));
                        evar static_node = u(evar, m) ? u(evar, m) : evar(mod, a, au, m, value, global, loaded, false);

                    } else {
                        Au_t src = m->src;
                        if (multi_Au && tt->au == typeid(Au))
                            continue;

                        verify(src, "no src type set for member %o", m);
                        src_init(a, src);
                        if (!is_class(src))
                            etype_implement(u(etype, src), false);

                        // get largest union member
                        Au src_info = head(src);
                        etype s = u(etype, src);
                        if (m->is_inlay)
                            struct_members[index] = src->is_class ? s->lltype : lltype(s);
                        else {
                            enum AU_MEMBER memtype = s->au->member_type;
                            Au info = head(s);
                            struct_members[index] = lltype(s);
                        }

                        if (m->elements > 0)
                            struct_members[index] = LLVMArrayType(struct_members[index], m->elements);

                        verify(struct_members[index], "no lltype found for member %s.%s", au->ident, m->ident);
                        int abi_member  = LLVMABISizeOfType(a->target_data, struct_members[index]);
                        verify(abi_member, "type has no size");
                        if (au->is_union && src->abi_size > ilargest) {
                            largest  = struct_members[index];
                            ilargest = src->abi_size;
                        }
                        m->index = index++;
                    }
                }
            }

            // lets define this as a form of byte accessible opaque.
            if (is_class(t) && (!multi_Au || tt->au != typeid(Au))) {
                struct_members[index] = LLVMArrayType(lltype(etypeid(u8)), tt->au->isize);
                index++;
            }
        }

        verify(count == index, "member indexing mismatch on %o", t);

        etype type_t_ptr = null;
        if (is_class(t) && au != etypeid(Au_t) && 
           !au->is_system && !au->is_schema) {
            type_t_ptr = get_type_t_ptr(t);

        } else if (count == 0) {
            struct_members[count++] = lltype(etypeid(u8));
        }
        
        if (type_t_ptr) {
            struct_members[count++] = lltype(type_t_ptr);
            struct_members[count++] = lltype(type_t_ptr);
        }

        if (au->is_union) {
            count = 1;
            struct_members[0] = largest;
        }

        LLVMStructSetBody(t->lltype, struct_members, count, 1);


    } else if (is_enum(t)) {

        Au_t et = au->src;
        static bool enum_processing = false;
        if (enum_processing) {
            return; // safe for global usage
        }
        enum_processing = true;
        verify(et, "expected source type for enum %s", au);
        for (int i = 0; i < au->members.count; i++) {
            Au_t   m = (Au_t)au->members.origin[i]; // has ident, and value set (required)
            Au_t isa_type = isa(u(etype, m));
            if (!u(etype, m)) {
                set(a->registry, (Au)m, (Au)hold(enode(mod, a, loaded, false, value, null, au, m)));
            }
            enode nn = (enode)u(etype, m);
            if (nn->value) {
                enum_processing = false;
                return;
            }
            verify(m->value, "no value set for enum %s:%s", au->ident, m->ident);
            string n = f(string, "%s_%s", au->ident, m->ident);
            
            LLVMTypeRef tr = t->lltype;
            LLVMValueRef G = a->is_Au_import ?
                LLVMFetchGlobal(a->module_ref, tr, n->chars) :
                LLVMAddGlobal(a->module_ref, tr, n->chars);
            
            LLVMSetLinkage(G, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
            LLVMSetGlobalConstant(G, 1);

            if (!a->is_Au_import) {
                LLVMValueRef init;
                if (et == typeid(i32))
                    init = LLVMConstInt(tr, *((i32*)m->value), 1);
                else if (et == typeid(u32))
                    init = LLVMConstInt(tr, *((u32*)m->value), 0);
                else if (et == typeid(i16))
                    init = LLVMConstInt(tr, *((i16*)m->value), 1);
                else if (et == typeid(u16))
                    init = LLVMConstInt(tr, *((u16*)m->value), 0);
                else if (et == typeid(i8))
                    init = LLVMConstInt(tr, *((i8*)m->value), 1);
                else if (et == typeid(u8))
                    init = LLVMConstInt(tr, *((u8*)m->value), 0);
                else if (et == typeid(i64))
                    init = LLVMConstInt(tr, *((i64*)m->value), 1);
                else if (et == typeid(u64))
                    init = LLVMConstInt(tr, *((u64*)m->value), 0);
                else 
                    fault("unsupported enum value: %s", et->ident);
                
                nn->value = G;
                nn->loaded = false;
                LLVMSetInitializer(G, init);
            }
        }
        enum_processing = false;
    } else if (is_func((Au)t)) {
        Au_t cl = isa(t);
        verify(cl == typeid(efunc), "expected efunc");
        //string n = is_rec(au->context) ?
        //    f(string, "%s_%s", au->context->ident, au->ident) : string(au->ident);
        cstr n = au->alt ? au->alt : au->ident;
        verify(n, "no name given to function");
        efunc fn = (efunc)t;

        // functions, on init, create the arg enodes from the model data
        // make sure the types are ready for use

        arg_types(au, arg_type) {
            etype t = u(etype, arg_type);
            if (t) {
                verify(t, "expected user data on type %s", arg_type->ident);
                etype_implement(t, false);
            }
        }

        fn->value  = LLVMGetNamedFunction(a->module_ref, n);
        if (!fn->value)
            fn->value = LLVMAddFunction(a->module_ref, n, t->lltype);

        // fill out enode values for our args type pointer 
        // context type pointer (we register these on init)
        if (is_lambda(au)) {
            LLVMValueRef value_ctx  = LLVMGetParam(fn->value, 0); // we have one more than args for the context
            fn->context_node->value = value_ctx;
        }

        Au_t au_target = au->is_imethod ? (Au_t)au->args.origin[0] : null;
        fn->target = au_target ?
            hold(enode(mod, a, au, au_target, arg_index, 0, loaded, true)) : null;

        if (fn->target) {
            set(a->registry, (Au)fn->target->au, (Au)hold(fn->target));
        }

        string label = f(string, "%s_entry", au->ident);
        bool is_user_implement = fn->au->module == a->au && fn->has_code;

        fn->entry = is_user_implement ? LLVMAppendBasicBlockInContext(
            a->module_ctx, fn->value, label->chars) : null;

        bool global_public_fn = au->access_type != interface_intern;
        LLVMSetLinkage(fn->value,
            !global_public_fn && is_user_implement && !au->is_export ?
                LLVMInternalLinkage : LLVMExternalLinkage);

        int index = 0;
        arg_list(fn->au, arg) {
            verify(arg != typeid(Au), "unexpected Au");
            evar t = u(evar, arg);
            if (!t) {
                bool is_explicit_ref = arg->is_explicit_ref;
                t = evar(mod, a, au, arg, is_explicit_ref, is_explicit_ref, arg_index, index, loaded, true);
            }
            index++;
        }
    }

    if (!au->is_void && !is_opaque(au) && !is_func((Au)au) && t->lltype) {
        if (au->elements) t->lltype = LLVMArrayType(t->lltype, au->elements);
        /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// 
        au->abi_size   = LLVMABISizeOfType(a->target_data, t->lltype) * 8;
        if (!au->typesize)
            au->typesize = LLVMABISizeOfType(a->target_data, t->lltype);
        au->align_bits = LLVMABIAlignmentOfType(a->target_data, t->lltype) * 8;
    }
}

void aether_build_user_initializer(aether a, etype m) { }

enode aether_e_asm(aether a, array body, array input_nodes, etype out_type, string return_name)
{
    a->is_const_op = false;
    bool has_out = out_type != null;
    int  n_in    = input_nodes ? len(input_nodes) : 0;

    if (a->no_build)
        return e_noop(a, has_out ? out_type : etypeid(none));

    // ---- check if return target is an input or a register ----
    int return_input = -1;

    if (return_name)
        for (int i = 0; i < n_in; i++) {
            Au input = get(input_nodes, i);
            enode n = instanceof(input, enode);
            verify (n, "unexpected member found in asm input: %o", input);
            if (strcmp(n->au->ident, return_name->chars) == 0) {
                verify(return_input == -1,
                    "duplicate member of return found in asm input: %o", return_name);
                return_input = i;
            }
        }

    // ---- build asm text with $N replacement ----
    string buf = string(alloc, 1024);
    int    prev_line = -1;

    array input_names = array(alloc, len(input_nodes));
    each(input_nodes, enode, n)
        push(input_names, (Au)string(n->au->ident));

    // names : list for input_nodes [ i: object ] if instanceof[ i, enode ] select i

    for (int i = 0; i < len(body); i++) {
        token t = (token)get(body, i);

        // newline between lines, space between tokens on same line
        if (prev_line >= 0)
            append(buf, t->line > prev_line ? "\n" : " ");

        prev_line = t->line;

        // check if this token matches an input name
        int match = -1;
        for (int j = 0; j < n_in; j++) {
            string name = (string)get(input_names, j);
            if (strcmp(t->chars, name->chars) == 0) {
                match = j;
                break;
            }
        }

        if (match >= 0) {
            // replace with $index (inputs start at $1 if output, $0 if no output)
            int idx = has_out ? match + 1 : match;
            concat(buf, f(string, "$%d", idx));
            
        } else
            concat(buf, (string)t);
    }

    // ---- build constraint string ----
    string constraint = string(alloc, 256);
    if (has_out) {
        if (return_input >= 0) {
            // output goes to a register
            append(constraint, "=r");
        } else {
            // output tied to a specific register (xmm0, rax, etc)
            concat(constraint, f(string, "={%s}", return_name->chars));
        }
    }
    for (int i = 0; i < n_in; i++) {
        if (len(constraint))
            append(constraint, ",");
        append(constraint, (has_out && i == return_input) ? "0" : "r");
    }

    // ---- build LLVM types ----
    LLVMTypeRef   ret    = has_out ? lltype(out_type) : LLVMVoidTypeInContext(a->module_ctx);
    LLVMTypeRef*  params = n_in ? calloc(n_in, sizeof(LLVMTypeRef)) : NULL;
    LLVMValueRef* args   = n_in ? calloc(n_in, sizeof(LLVMValueRef)) : NULL;

    for (int i = 0; i < n_in; i++) {
        enode op     = (enode)get(input_nodes, i);
        enode loaded = enode_value(op, false);
        params[i]    = LLVMTypeOf(loaded->value);
        args[i]      = loaded->value;
    }
    
    LLVMTypeRef fn_type = LLVMFunctionType(ret, params, n_in, false);

    // ---- create inline asm ----
    LLVMValueRef asm_val = LLVMGetInlineAsm(
        fn_type,
        buf->chars,        len(buf),
        constraint->chars, len(constraint),
        true,              // side effects
        true,              // align stack
        LLVMInlineAsmDialectIntel, // minimal effort needed to support AT&T (but will they support us?)
        false);            // can't throw

    // ---- call it ----
    LLVMValueRef result = LLVMBuildCall2(
        B, fn_type, asm_val,
        args, n_in,
        has_out ? "asm_out" : "");

    free(args);
    free(params);

    if (has_out)
        return enode(mod, a, au, out_type->au, loaded, true, value, result);
    
    return e_noop(a, etypeid(none));
}

enode enode_offset(enode ptr, etype offset_type, i64 index) { sequencer
    aether a = ptr->mod;
    if (a->no_build) return e_noop(a, (etype)ptr);
    
    LLVMValueRef base = ptr->value;
    
    char N[32];
    sprintf(N, "%i_offset", seq);
    LLVMValueRef idx = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), index, true);
    LLVMValueRef result = LLVMBuildGEP2(B,
        lltype(offset_type), base, &idx, 1, N);
    
    return enode(mod, a, au, au_arg_type((Au)ptr->au), loaded, false, value, result);
}

none aether_build_module_initializer(aether a, enode init) {
    Au_t au = init->au;
    if (!au->is_mod_init)
        return;

    a->direct = true;
    aether_eputs(a, f(string, "%o: initializing", a));

    // verify typeid(string) matches between compiler and runtime
    {
        efunc fn_check = efind(efunc, etypeid(Au), typeid_string);
        enode str_tid  = e_typeid(a, etypeid(string));
        e_fn_call(a, fn_check, a(str_tid));
    }

    emit_coverage_register(a);
    
    efunc f = (efunc)u(efunc, au);
    Au_t  module_base = a->au;

    efunc fn_module_lookup = efind(efunc, etypeid(Au), module_lookup);
    efunc fn_find_member   = efind(efunc, etypeid(Au), find_member);
    efunc fn_emplace       = efind(efunc, etypeid(Au), emplace_type);
    efunc fn_def_func      = efind(efunc, etypeid(Au), def_func);
    efunc fn_def_prop      = efind(efunc, etypeid(Au), def_prop);
    efunc fn_def_enum      = efind(efunc, etypeid(Au), def_enum_value);
    efunc fn_def_arg       = efind(efunc, etypeid(Au), def_arg);
    efunc fn_def_meta      = efind(efunc, etypeid(Au), def_meta);
    efunc fn_def_type      = efind(efunc, etypeid(Au), def_type);
    efunc fn_push          = efind(efunc, etypeid(Au), push_type);

    efunc fn_def_test      = efind(efunc, etypeid(Au), def_test);

    push_scope(a, (Au)f);

    // module's own published type id (etype container)
    enode module_type_id = e_typeid(a, (etype)a);

    // ---- NEW: emplace + push the module type itself FIRST ----
    u64 module_isize = 0;
    members(module_base, mem) {
        if (mem->member_type == AU_MEMBER_VAR &&
            mem->access_type == interface_intern)
            module_isize += mem->typesize;
    }

    //aether_eputs(a, f(string, "1"));

    // NOTE: module has no context; its src is its base (usually Au/module base)
    e_fn_call(a, fn_emplace, a(
        module_type_id,
        e_null(a, etypeid(Au_t)), // no context
        e_null(a, etypeid(Au_t)), // no src on modules
        module_type_id, // bind to emodule container
        const_string(chars, module_base->ident),
        _i32(module_base->member_type),
        _u64(module_base->traits),
        _u64(module_base->typesize),
        _u64(module_isize)
    ));

    //aether_eputs(a, f(string, "2"));

    Au_t m = find_member(module_base, "coolteen", AU_MEMBER_VAR, 0, false);
    evar var_mdl = u(evar, m);

    // define public functions at the module level; this effectively imports
    members(module_base, mem) {
        etype mdl = u(etype, mem);
        evar var_mdl2 = u(evar, mem);

        if (mem->is_system || mem->is_schema) continue;

        if (is_func(mem) && mem->access_type != interface_intern) {
            efunc mf = (efunc)u(etype, mem);
            mf->used = true;
            etype_implement((etype)mf, false);

            // register as module global
            e_fn_call(a, fn_def_func, a(
                module_type_id,
                const_string(chars, mem->ident),
                e_typeid(a, u(etype, mem->rtype)),
                _u32(mem->member_type),
                _u32(mem->access_type),
                _u32(mem->operator_type),
                _u64(mem->traits),
                value(etypeid(ARef), mf->value),
                mem->alt ? (Au)const_string(chars, mem->alt) : (Au)e_null(a, etypeid(cstr))
            ));
        }

        if (u(evar, mem) && mem->access_type != interface_intern) {
            //aether_eputs(a, f(string, "%o: var", mem));
            evar mvar = (evar)u(evar, mem);
            mvar->used = true;
            etype_implement((etype)mvar, false);

            enode e_meta_b = e_null(a, etypeid(Au));
            if (mem->meta.b) {
                if (instanceof(mem->meta.b, Au_t))
                    e_meta_b = (enode)e_typeid(a, u(etype, (Au_t)mem->meta.b));
                else if (instanceof(mem->meta.b, shape)) {
                    shape s = (shape)mem->meta.b;
                    array indices = array(alloc, s->count);
                    for (i32 si = 0; si < s->count; si++)
                        push(indices, (Au)e_operand(a, _i64(s->data[si]), etypeid(i64)));
                    e_meta_b = eshape_from_indices(a, indices);
                }
            }
            e_fn_call(a, fn_def_prop, a(
                module_type_id,
                const_string(chars, mem->ident),
                e_typeid(a, u(etype, mem->rtype)), // this is the source.
                _u64(mem->traits),
                _u32(mem->rtype->offset), // offset?
                _u32(mem->rtype->abi_size),
                e_null(a, null),
                mem->meta.a ? e_typeid(a, u(etype, mem->meta.a)) : e_null(a, etypeid(Au_t)),
                e_meta_b
            ));
        }
    }

    // iterate through user-defined type id
    pairs(a->user_type_ids, i) {
        etype mdl     = instanceof(i->key,   etype);
        enode type_id = instanceof(i->value, enode);

        if (!mdl || !type_id)
            continue;

        //aether_eputs(a, f(string, "user_type_ids: %s", mdl->au->ident));

        bool is_class_t  = is_class(mdl);
        bool is_struct_t = is_struct(mdl);
        bool is_enum_t   = is_enum(mdl);

        if (!is_class_t && !is_struct_t && !is_enum_t)
            continue;

        // intern classes still need emplace for typesize

        Au_t tau = mdl->au;
        if (mdl->au->ident && strcmp(mdl->au->ident, "test4") == 0) {
            mdl = mdl;
        }

        // calculate internal size
        u64 isize = 0;
        members(tau, mem) {
            if (mem->member_type == AU_MEMBER_VAR &&
                mem->access_type == interface_intern)
                isize += mem->typesize;
        }

        //aether_eputs(a, f(string, "%o: var", type_id->au));
        verify(mdl->au->typesize, "type-size is 0 for %s", mdl->au->ident);

        e_fn_call(a, fn_emplace, a(
            type_id,
            tau->context ? e_typeid(a, u(etype, tau->context)) : e_null(a, etypeid(Au_t)),
            tau->src     ? e_typeid(a, u(etype, tau->src))     : e_null(a, etypeid(Au_t)),
            module_type_id,
            const_string(chars, tau->ident),
            _u64(tau->member_type),
            _u64(tau->traits),
            _u64(mdl->au->typesize),
            _u64(isize)
        ));

        members(tau, mem) {
            if (mem->access_type == interface_intern)
                continue;

            if (is_func(mem)) {
                efunc mf = u(efunc, mem);
                mf->used = true;
                etype_implement((etype)mf, false);

                enode fptr = value(etypeid(ARef), mf->value);

                enode e_mem = e_fn_call(a, fn_def_func, a(
                    type_id,
                    const_string(chars, mem->ident),
                    e_typeid(a, u(etype, mem->rtype)),
                    _u32(mem->member_type),
                    _u32(mem->access_type),
                    _u32(mem->operator_type),
                    _u64(mem->traits),
                    fptr,
                    mem->alt ? (Au)const_string(chars, mem->alt) : (Au)e_null(a, etypeid(cstr))
                ));
 
                if (mem->is_override) {
                    
                    Au_t m_override = find_member(
                        mdl->au->context, mem->ident,
                        mem->member_type, 0, true);
                    LLVMTargetDataRef layout = LLVMGetModuleDataLayout(a->module_ref);
                    i64 ptr = LLVMPointerSize(layout);
                    i64 byte_offset = etypeid(Au_t)->au->abi_size / 8 + (m_override->index * ptr);
                    LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), byte_offset, false);
                    LLVMValueRef slot = LLVMBuildGEP2(B, LLVMInt8TypeInContext(a->module_ctx), type_id->value, &offset, 1, "ft_slot");
                    enode fn_slot = enode(mod, a, au, typeid(ARef), loaded, false, value, slot);
                    e_assign(a, fn_slot, (Au)mf, OPType__assign);
                }

                arg_list(mem, arg) {
                    etype arg_type = resolve(u(etype, arg->src));
                    e_fn_call(a, fn_def_arg, a(
                        e_mem,
                        const_string(chars, arg->ident),
                        e_typeid(a, arg_type),
                        _i64(0)
                    ));
                }

                if (is_lambda((Au)mem)) {
                    enode n = (enode)u(etype, mem);
                    verify(n->published_type, "expected published Au_t symbol for lambda");
                    e_assign(a, n->published_type, (Au)e_mem, OPType__assign);
                }

            } else if (mem->member_type == AU_MEMBER_VAR) {
                if (mem->meta.a || mem->meta.b)
                    printf("  emit meta: %s.%s  a=%s  b=%s\n",
                        tau->ident, mem->ident,
                        mem->meta.a ? mem->meta.a->ident : "(null)",
                        mem->meta.b ? isa(mem->meta.b)->ident : "(null)");
                enode e_meta_b = e_null(a, etypeid(Au));
                if (mem->meta.b) {
                    if (instanceof(mem->meta.b, Au_t))
                        e_meta_b = (enode)e_typeid(a, u(etype, (Au_t)mem->meta.b));
                    else if (instanceof(mem->meta.b, shape)) {
                        shape s = (shape)mem->meta.b;
                        array indices = array(alloc, s->count);
                        for (i32 si = 0; si < s->count; si++)
                            push(indices, (Au)e_operand(a, _i64(s->data[si]), etypeid(i64)));
                        e_meta_b = eshape_from_indices(a, indices);
                    }
                }
                a->debug_typeid = strcmp(mem->ident, "quants") == 0;
                e_fn_call(a, fn_def_prop, a(
                    type_id,
                    const_string(chars, mem->ident),
                    e_typeid(a, canonical(u(etype, mem->src))),
                    _u64(mem->traits),
                    _u32(mem->offset),
                    _u32(mem->abi_size),
                    e_null(a, etypeid(ARef)),
                    mem->meta.a ? e_typeid(a, u(etype, mem->meta.a)) : e_null(a, etypeid(Au_t)),
                    e_meta_b
                ));

            } else if (mem->member_type == AU_MEMBER_ENUMV) {
                etype_implement(u(etype, mem->context), false);
                enode val = (enode)u(enode, mem);

                e_fn_call(a, fn_def_enum, a(
                    type_id,
                    const_string(chars, mem->ident),
                    val
                ));

            } else if (mem->member_type == AU_MEMBER_TYPE) {
                // not allowed (types embedded in types)
            } else {
                fault("unsupported member");
            }
        }
        e_fn_call(a, fn_push, a(type_id));
    }

    // polymorphism works now
    a->direct = false;
    pop_scope(a);
    push_scope(a, (Au)f);
    members(module_base, au) {
        evar var = u(evar, au);
        if (au->ident && strcmp(au->ident, "rng_state") == 0) {
            au = au;
        }
        if (au->member_type == AU_MEMBER_VAR && var && var->initializer) {
            build_user_initializer(a, u(etype, au));
        }
    }
    pop_scope(a);
    build_entrypoint(a, f);
}

statements aether_context_code(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (u(statements, ctx))
            return u(statements, ctx);
        break;
    }
    return null;
}

efunc aether_context_func(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        efunc f = (efunc)u(enode, ctx);
        if   (f) return f;
    }
    return null;
}

etype aether_context_class(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_class)
            return ctx_u;
    }
    return null;
}

etype aether_context_struct(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_struct)
            return ctx_u;
    }
    return null;
}

etype aether_context_model(aether a, Au_t type) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (ctx->member_type == AU_MEMBER_TYPE && isa(ctx_u) == type)
            return ctx_u;
    }
    return null;
}

catcher context_catcher(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        catcher ctx_u = u(catcher, ctx);
        if (ctx_u) return ctx_u;
    }
    return null;
}

etype aether_context_record(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (ctx->member_type == AU_MEMBER_TYPE && (ctx->is_struct || ctx->is_class))
            return ctx_u;
    }
    return null;
}

Au_t aether_top_scope(aether a) {
    Au_t open = null;
    each(a->lexical, Au_t, au_module) {
        if (!au_module->is_closed)
            open = au_module;
    }
    return open ? open : null;
}

none aether_push_scope(aether a, Au arg) {
    Au_t au = au_arg(arg);
    Au_t auu = au;
    int mtype = au->member_type;

    while  (au &&
            au->member_type != AU_MEMBER_MODULE    &&
            au->member_type != AU_MEMBER_NAMESPACE &&
            au->member_type != AU_MEMBER_FUNC      &&
            au->member_type != AU_MEMBER_CAST      &&
            au->member_type != AU_MEMBER_OPERATOR  &&
            au->member_type != AU_MEMBER_CONSTRUCT &&
           !au->is_primitive &&
           !au->is_enum  &&
           !au->is_class && 
           !au->is_struct) {
        au = au->src;
    }

    verify(au, "invalid scope given: %o", arg);

    efunc prev_fn = context_func(a);

    if (!is_func(au) && au->src && (au->src->is_class || au->src->is_struct))
        push(a->lexical, (Au)au->src);
    else
        push(a->lexical, (Au)au);

    statements st = u(statements, au);
    token peek = a->statement_origin ? a->statement_origin : aether_peek(a);
    if (st && a->coverage && peek) {
        st->probe_id   = a->next_probe_id++;
        st->probe_line = peek->line;
        debug_emit(a);
        aether_emit_block_probe(a, st->probe_id);
    }

    efunc fn = context_func(a);
    if (fn && isa(fn) == typeid(efunc) && (fn != prev_fn) && !a->no_build) {
        // attach DWARF subprogram for debugger (breakpoints, stack frames)
        if (fn->has_code && fn->au->module == a->au)
            emit_debug_function(a, fn, false);
        LLVMPositionBuilderAtEnd(B, fn->entry);
        LLVMSetCurrentDebugLocation2(B, fn->last_dbg);
        // set initial debug location for function entry
        if (a->debug)
            emit_debug_loc(a, peek ? peek->line : 0, peek ? peek->column : 0);
        // emit parameter debug variables (self, args) for LLDB locals view
        emit_debug_params(a, fn);
        a->coverage_seq_local = null; // reset per-function __seq alloca
        if (a->timing_enabled && !fn->timing_start_value) {
            fn->timing_func_id = a->next_func_id++;
            fn->timing_start_value = emit_func_timing_start(a, fn->timing_func_id);
            coverage_set_func_name(fn->timing_func_id, fn->au->alt ? fn->au->alt : fn->au->ident);
        }
    }

    // emit debug location for every statements block entry
    if (a->debug && !a->no_build)
        emit_debug_loc(a, peek ? peek->line : 0, peek ? peek->column : 0);
}

etype get_type_t_ptr(etype t);

static Au_t map_etype(aether a, symbol name, int *out_depth) {
    Au_t t = (Au_t)elookup(name); // Cast to Au_t directly if your system allows
    if (!t) return null;

    if (t->member_type == AU_MEMBER_TYPE) return t;
    if (t->member_type != AU_MEMBER_MACRO) return null;

    macro mac = (macro)u(macro, t);
    if (mac->au->is_functional || len(mac->def) == 0) return null;

    token f = (token)first_element(mac->def);
    if (!f || eq(f, name)) return null;

    for (int i = 1; i < mac->def->count; i++) {
        if (!eq((token)mac->def->origin[i], "*")) return null;
        (*out_depth)++;
    }
    return map_etype(a, f->chars, out_depth);
}

none aether_import_models(aether a, Au_t ctx, bool au_mode) {
    // initialization table for has/not bits, controlling init and implement
    struct filter {
        bool init, impl;
        bool additional;
        bool is_func;
        bool func_xor;
        int  member_type;
        u32 has_bits;
        u32 not_bits;
    } filters[10] = {
        { true,  true,  false, false, false, 0, AU_TRAIT_PRIMITIVE, AU_TRAIT_POINTER | AU_TRAIT_FUNCPTR },
        { true,  true,  false, false, false, 0, AU_TRAIT_STRUCT | AU_TRAIT_SYSTEM, 0 },
        { true,  true,  false, false, false, 0, AU_TRAIT_PRIMITIVE | AU_TRAIT_POINTER, 0 },
        { true,  true,  false, false, false, 0, AU_TRAIT_PRIMITIVE, 0 },
        { true,  true,  false, false, false, 0, AU_TRAIT_ENUM,   0 },
        { true,  false, false, false, false, 0, AU_TRAIT_UNION,  0 },
        { true,  false, false, false, false, 0, AU_TRAIT_STRUCT, 0 },
        { true,  false, false, false, false, 0, AU_TRAIT_CLASS,  0 },
        { false, true,  true,  false, false, 0, 0, 0 }, // lets run through non-functions
        { false, true,  true,  true,  false, 0, 0, 0 }  // now implement functions, whose args will always be implemented
    };

    a->import_c = !au_mode;
    for (int filter = 0; filter < 10; filter++) {
        struct filter* ff = &filters[filter];
        for (num i = 0; i < ctx->members.count; i++) {
            Au_t m  =  (Au_t)ctx->members.origin[i];
            
            //if (m->module != ctx)
            //    continue;

            if (ff->member_type && ff->member_type != m->member_type) continue;

            bool is_func = m->member_type == AU_MEMBER_FUNC || m->member_type == AU_MEMBER_INDEX ||
                           m->member_type == AU_MEMBER_SETTER ||
                           m->member_type == AU_MEMBER_CAST || m->member_type == AU_MEMBER_OPERATOR;
            bool p0      = (ff->has_bits & m->traits) == ff->has_bits;
            bool p1      = (ff->not_bits & m->traits) == 0;
            bool proceed = p0 && p1;

            if (ff->additional) {
                bool proceed = (is_func == ff->is_func) ^ ff->func_xor;
                if (!proceed) continue;
            }

            if (proceed) {
                if (ff->init || ff->impl) {
                    src_init(a, m);
                }
                if (ff->impl) {
                    etype e = u(etype, m);
                    Au info = head(e);
                    etype_implement(e, false);
                }
            }
        }
    }

    aether_create_type_members(a, ctx);

    // typed macros to member type aliases
    members (ctx, m) {
        if (m->member_type == AU_MEMBER_MACRO && !m->is_functional) {
            macro mac   = (macro)u(macro, m);
            int  depth = 0;
            Au_t base = map_etype(a, m->ident, &depth); // start from self to get full chain

            if (base && base != m) { // found a resolution that isn't just self-pointing
                m->member_type = AU_MEMBER_TYPE;
                m->src = base;

                for (int i = 0; i < depth; i++)
                    m->src = pointer(a, (Au)m->src)->au;
                
                // register type for aether
                set(a->registry, (Au)m, (Au)hold(etype(mod, a, au, m)));
            }
        }
    }

    a->import_c = false;
}

void aether_import_Au(aether a, string ident, Au lib) {
    a->current_inc   = instanceof(lib, Au_t) ? path(((Au_t)lib)->ident) : lib ? (path)lib : path("Au");
    a->is_Au_import  = true;
    string  lib_name = lib && instanceof(lib, path) ? stem((path)lib) : null;
    Au_t    au_module = null;
    handle  lib_instance = null;

    if (instanceof(lib, path)) {
        au_module = find_module(ident->chars);
        if (!au_module) {
            string path_str = string(((path)lib)->chars);
            lib_instance = dlopen(cstring(path_str), RTLD_NOW);
            verify(lib_instance, "shared-lib failed to load: %o", lib);
            set(a->libs, path_str, (Au)lib_instance);
            au_module = find_module(ident->chars);
        }

    } else if (!lib) {
        // Au is loaded already in global()
        au_module = global();
        set(a->libs, string("Au"), _bool(true));
    } else {
        // filter through registration data already here
        cstr built_in = cast(cstr, (string)lib);
        au_module = find_module(built_in);
    }

    if (lib)
        push_scope(a, (Au)au_module);

    a->import_module = au_module;

    if (!u(etype, typeid(Au))) {
        etype_ptr(a, etype(mod, a, au, typeid(Au))->au, null);
        etype_ptr(a, etype(mod, a, au, typeid(Au_t))->au, null);
    }

    aether_import_models(a, au_module, true);

    if (!u(etype, au_module)) {
        emodule(mod, a, au, au_module);
    }
    if (!lib) {
        // this causes a name-related error in IR
        Au_t f = find_member(au_module, "app", AU_MEMBER_TYPE, 0, false);
        verify(f, "could not import app abstract");

        if (!u(etype, f)) {
            etype t = etype(mod, a, au, f);
            set(a->registry, (Au)f, (Au)hold(t));
            implement(t, false);
        }

        verify(f->context == typeid(Au), "expected Au type context");
    } else {

        // for imported libraries with required calls, we must invoke them here
        // when they crash, they crash us -- thats great because when 
        // we run silver in lldb we arrive at user code
        //
        // this is great for advancing language and foundation
        if (lib_instance) {
            members(au_module, mem) {
                if (mem->member_type == AU_MEMBER_FUNC &&
                    mem->access_type == interface_expect &&
                    mem->rtype == typeid(bool) &&
                    mem->args.count == 0)
                {
                    bool (*fn)() = dlsym(lib_instance, mem->alt ? mem->alt : mem->ident);
                    verify(fn,   "[import %s] expect: not-found %s",   au_module->ident, mem->ident);
                    verify(fn(), "[import %s] expect: %s failed",      au_module->ident, mem->ident);
                    print       ("[import %s] expect: %s successful",  au_module->ident, mem->ident);
                }
            }
        }

        au_module->is_closed = true;
    }
    a->is_Au_import  = false;
    a->current_inc   = null;
    a->import_module = null;

}

void aether_llflag(aether a, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), ival, 0));

    char sflag[64];
    memcpy(sflag, flag, strlen(flag) + 1);
    LLVMAddModuleFlag(a->module_ref, LLVMModuleFlagBehaviorError, sflag, strlen(sflag), v);
}


bool aether_emit(aether a, ARef ref_ll, ARef ref_bc) {
    path* ll = (path*)ref_ll;
    path* bc = (path*)ref_bc;
    cstr err = NULL;

    path c = path_cwd();

    // finalize DWARF debug info before emitting
    if (a->debug && a->dbg_builder)
        LLVMDIBuilderFinalize(a->dbg_builder);

    *ll = form(path, "%o/%s/%o.ll", a->install, a->debug ? "debug" : "release", a);
    *bc = form(path, "%o/%s/%o.bc", a->install, a->debug ? "debug" : "release", a);

    bool validation_error = false;
    verify (!LLVMPrintModuleToFile(a->module_ref, cstring(*ll),      &err), "print-to-module");
    print("wrote %s", cstring(*ll));
    validation_error  = LLVMVerifyModule(a->module_ref, LLVMReturnStatusAction, &err);
    validation_error |= LLVMVerifyModule(a->module_ref, LLVMPrintMessageAction, &err);
    
    if (LLVMWriteBitcodeToFile(a->module_ref, cstring(*bc)) != 0)
        fault("LLVMWriteBitcodeToFile failed");

    return true;
}

void llvm_reinit(aether a) {
    LLVMDisposeBuilder  (B);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module_ref);
    LLVMDisposeTargetMachine(a->target_machine);
    LLVMContextDispose  (a->module_ctx);
    LLVMDisposeMessage  (a->target_triple);

    a->module_ctx     = LLVMContextCreate();
    a->module_ref     = LLVMModuleCreateWithNameInContext(a->name->chars, a->module_ctx);
    a->dbg_builder    = LLVMCreateDIBuilder(a->module_ref);
    B        = LLVMCreateBuilderInContext(a->module_ctx);
    a->target_triple  = LLVMGetDefaultTargetTriple();

    cstr err = NULL;
    if (LLVMGetTargetFromTriple(a->target_triple, &a->target_ref, &err))
        fault("error: %s", err);
    a->target_machine = LLVMCreateTargetMachine(
        a->target_ref, a->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    // the relative path should be from our cwd, i think.  all choices have loss but this one we can work with
    // silver compiles user code relative to the silver path, better this than having different projects
    //
    path rel = path_cwd(); 
    a->file = LLVMDIBuilderCreateFile(
        a->dbg_builder, a->module_file->chars, len(a->module_file), rel->chars, len(rel));

    a->target_data = LLVMCreateTargetDataLayout(a->target_machine);
    if (a->debug) {
        a->compile_unit = LLVMDIBuilderCreateCompileUnit(
            a->dbg_builder, LLVMDWARFSourceLanguageC, a->file,
            "silver", 6, 0, "", 0,
            0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);
        LLVMAddModuleFlag(a->module_ref, LLVMModuleFlagBehaviorWarning,
            "Debug Info Version", 18,
            LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 3, 0)));
        LLVMAddModuleFlag(a->module_ref, LLVMModuleFlagBehaviorWarning,
            "Dwarf Version", 13,
            LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 4, 0)));
    }
    B = LLVMCreateBuilderInContext(a->module_ctx);

    // init our coverage mapping
    init_coverage(a);
}


void aether_unload_libs(aether a) {
    // don't dlclose — types registered by constructors are still
    // referenced across the type system. dlopen will reuse the
    // already-loaded library and constructors re-register via emplace.
}

// todo: adding methods to header does not update the methods header (requires clean)
void aether_reinit_startup(aether a) {
    a->iteration++;
    a->fn_init        = null;
    a->stack          = array(16);
    a->import_c       = false;
    a->direct         = false;
    a->no_build       = false;
    a->no_const       = false;
    a->in_ref         = false;
    a->expr_level     = 0;
    a->cursor         = 0;
    a->is_implemented = false;
    aether_unload_libs(a);
    a->libs           = map(assorted, true, unmanaged, true);
    a->user_type_ids  = map(assorted, true);
    a->lexical        = array(alloc, 32, unmanaged, true, assorted, true);
    a->registry       = store();
    a->next_func_id   = 0;
    a->next_probe_id  = 0; // send out a class-3 probe

    // this is so a new external may reset state for itself
    module_erase(a->au, null);
    
    a->au = def_module(a->name->chars);
    set(a->registry, (Au)a->au, (Au)hold(a));
    
    if (a->module_file) // if this is not yet set, we do nothing (its set in silver; we must prime lldb with a manual call before the watch)
        llvm_reinit(a); 

    // push our module space to the scope
    Au_t g = global();
    g->is_closed = true;

    verify(g,           "globals not registered");
    verify(a->au,       "no module registered for aether");
    verify(g != a->au,  "aether using global module");

    push_scope(a, (Au)g);
    a->au->is_au = true;
    import_Au(a, string("Au"), null);
    
    a->au->is_namespace = true; // for the 'module' namespace at [1], i think we dont require the name.. or, we set a trait
    a->au->is_nameless  = false; // we have no names, man. no names. we are nameless! -cereal
    push_scope(a, (Au)a->au);
} 

none aether_test_write(aether a) {
    char* err = 0;
    LLVMPrintModuleToFile(a->module_ref, "crashing.ll", &err);
}

none aether_init(aether a) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    if ( a->module) {
        path prev = a->module;
        a->module = absolute(prev);
        drop(prev);
    }
    if (!a->install) {
        cstr import = getenv("IMPORT");
        if (import) {
            a->install = f(path, "%s", import);
        } else {
            path   exe = path_self();
            path   bin = parent_dir(exe);
            path   install = absolute(f(path, "%o/..", bin));
            a->install = install;
        }
    }
    a->root_path = absolute(f(path, "%o/../..", a->install));
    a->include_paths    = a(f(path, "%o/include", a->install));
    a->sys_inc_paths    = array(alloc, 32);
    a->sys_exc_paths    = array(alloc, 32);


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
    a->lib_paths     = array(alloc, 32);
#elif defined(__APPLE__)
    string sdk          = run(false, "xcrun --show-sdk-path");
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
    push(a->lib_paths, (Au)f(path, "%o/lib", a->install));
    path src_path = a->module;
    push(a->include_paths, (Au)src_path);

    a->coverage = a->debug;
    a->timing_enabled = a->debug;
}

none aether_dealloc(aether a) {
    for(item i = a->registry->first; i; i = i->next) {
        etype e = (etype)i->value;
        if (!e || !e->au || e->au == a->au || e == (etype)a)
            continue;

        if (e->au->module == a->au)
            e->au = null;
        
        drop(e);
    }
    a->registry = null;
    
    LLVMDisposeBuilder  (B);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module_ref);
    LLVMDisposeTargetMachine(a->target_machine);
    LLVMContextDispose  (a->module_ctx);
    LLVMDisposeMessage  (a->target_triple);

    module_erase(a->au, null);
}

string etype_cast_string(etype t) {
    return t->au ? string(t->au->ident) : string("[no-type]");
}

Au_t etype_cast_Au_t(etype t) {
    return t->au;
}


none enode_init(enode n) {
    Au_t n_isa = isa(n);
    aether a = n->mod;
    bool is_const = n->literal != null;

    if (is_func((Au)n->au->context) && !n->symbol_name) {
        int offset = 0;
        if (is_lambda((Au)n->au->context))
            offset += 1;
        
        enode fn = (enode)u(enode, au_arg_type((Au)n->au->context));
        n->value = LLVMGetParam(fn->value, n->arg_index + offset);
    } else if (n->symbol_name && !n->value) {
        LLVMValueRef g = LLVMGetNamedGlobal  (a->module_ref, n->symbol_name->chars);
        if (!g)      g = LLVMGetNamedFunction(a->module_ref, n->symbol_name->chars);

        verify(g, "global symbol not found: %o", n->symbol_name);
        n->value  = g;
        n->loaded = false; // globals are already addresses
    }
}

none enode_dealloc(enode a) {
}

Au_t enode_cast_Au_t(enode a) {
    return a->au;
}

array read_arg(array tokens, int start, int* next_read);

enode aether_e_subroutine(aether a, etype rtype, array body, subprocedure build_body) {
    if (a->no_build) return e_noop(a, rtype);

    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "sub.merge");
    
    Au_t au_cat = def(null, "catcher", AU_MEMBER_NAMESPACE, 0);
    catcher cat = catcher(mod, a, block, merge, au, au_cat);
    set(a->registry, (Au)au_cat, (Au)cat);
    cat->rtype = rtype;  // so return knows the type
    
    // build phi first so return-in-sub can add incoming
    LLVMPositionBuilderAtEnd(B, merge);
    cat->phi = hold(value(rtype, LLVMBuildPhi(B, lltype(rtype), "sub.result")));
    
    // position back to entry and run body
    LLVMPositionBuilderAtEnd(B, entry);
    push_scope(a, (Au)cat);
    invoke(build_body, (Au)body);
    
    // branch to merge if body didn't terminate
    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(B)))
        LLVMBuildBr(B, merge);
    
    // phi incoming values were added by each return-in-sub
    pop_scope(a);
    LLVMPositionBuilderAtEnd(B, merge);
    return enode(
        mod, a, au, rtype->au, loaded, true,
        value, cat->phi->value);
}

array macro_expand(macro m, array args) {
    aether a = m->mod;
    int   ln_args   = len(args);
    array r         = array(alloc, 32);
    array args_exp  = array(alloc, 32);
    int   ln_params = len(m->params);

    // if we provide too many, it may be checked as a var-arg
    if (ln_args < ln_params || (ln_args != ln_params && !m->va_args)) return null;

    // now its a simple replacement within the definition
    array initial = array(alloc, 32);
    each(m->def, token, t) {
        // parameter -> arg replacement (each item is a token list)
        bool found = false;
        for (int param = 0; param < ln_params; param++) {
            // if token matches parameter, replace; further replacement happens within silver
            if (compare(t, (token)m->params->origin[param]) == 0) {
                concat(initial, (array)args->origin[param]);
                found = true;
                break;
            } 
        }
        if (!found) {
            push(initial, (Au)t);
        }
    }

    // once replaced, we expand those as a flat token list
    return initial;
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen code, efunc f, array query) {
    aether a = code->mod;
    fault("must subclass codegen for usable code generation");
    return null;
}

void aether_e_print_node(aether a, enode n) {
    if (a->no_build) return;

    efunc  printf_fn = (efunc)elookup("printf");
    etype  can       = canonical(n);

    // string/path: cast to cstr first, then print with %s
    if (can == etypeid(string) || can == etypeid(path)) {
        enode as_cstr = e_create(a, etypeid(cstr), (Au)n);
        enode fmt_node = enode(mod, a, au, etypeid(cstr)->au, loaded, true,
            value, const_cstr(a, "%s", 2));
        e_fn_call(a, printf_fn, a(fmt_node, as_cstr));
        return;
    }

    cstr fmt = null;
    if      (can == etypeid(cstr) || can == etypeid(symbol)) fmt = "%s";
    else if (can == etypeid(i32))    fmt = "%i";
    else if (can == etypeid(u32))    fmt = "%u";
    else if (can == etypeid(i64))    fmt = "%lli";
    else if (can == etypeid(u64))    fmt = "%llu";
    else if (can == etypeid(f32) || can == etypeid(f64)) fmt = "%f";
    else if (can == etypeid(bool))   fmt = "%i";

    verify(fmt, "eprint_node: unsupported type: %o", n);
    enode fmt_node = enode(mod, a, au, etypeid(cstr)->au, loaded, true,
        value, const_cstr(a, fmt, strlen(fmt)));
    e_fn_call(a, printf_fn, a(fmt_node, n));
}

// f = format string; this is evaluated from nodes given at runtime
void aether_eprint(aether a, symbol f, ...) {
    if (a->no_build) return;
    va_list args;
    va_start(args, f);

    a->direct = true;

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
                push(schema, (Au)string(buf));
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
        push(schema, (Au)string(buf));
    }

    enode *arg_nodes = calloc(max_arg + 1, sizeof(i32));
    for (int i = 0; i < max_arg; i++)
        arg_nodes[i] = va_arg(args, enode);
    
    string res = string(alloc, 32);
    etype mdl_cstr = etypeid(cstr);
    etype mdl_i32  = etypeid(i32);
    each(schema, Au, obj) {
        enode n = instanceof(obj, enode);
        if (n) {
            e_print_node(a, n);
        } else {
            string s = (string)instanceof(obj, string);
            verify(s, "invalid type data");
            enode n_str = e_operand(a, (Au)s, null);
            e_print_node(a, n_str);
        }
    }

    va_end(args);
    free(arg_nodes);
    free(buf);
    a->direct = false;
}

void code_init(code c) {
    efunc fn = context_func(c->mod);
    aether a = fn->mod;
    verify(fn, "no function in context for code block");
    c->block = LLVMAppendBasicBlockInContext(a->module_ctx, fn->value, c->label);
}

void code_seek_end(code c) {
    LLVMPositionBuilderAtEnd(c->mod->builder, c->block);
}

void aether_e_cmp_code(aether a, enode l, comparison comp, enode r, code lcode, code rcode) {
    if (a->no_build) {
        a->is_const_op = false;
        return;
    }
    LLVMValueRef cond = LLVMBuildICmp(
        B, (LLVMIntPredicate)comp, l->value, r->value, "cond");
    LLVMBuildCondBr(B, cond, lcode->block, rcode->block);
}

void aether_e_inc(aether a, enode v, num amount) {
    a->is_const_op = false;
    if (a->no_build) return;
    enode lv = e_load(a, v, null);
    LLVMValueRef one = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), amount, 0);
    LLVMValueRef nextI = LLVMBuildAdd(a->mod->builder, lv->value, one, "nextI");
    LLVMBuildStore(a->mod->builder, nextI, v->value);
}

void aether_e_branch(aether a, code c) {
    a->is_const_op = false;
    if (a->no_build) return;
    LLVMBuildBr(B, c->block);
}

enode aether_e_cmp(aether a, enode L, enode R) {
    Au_t Lt = isa(L->literal);
    Au_t Rt = isa(R->literal);
    bool Lc = is_class(L);
    bool Rc = is_class(R);
    bool Ls = is_struct(L);
    bool Rs = is_struct(R);
    bool Lr = is_realistic(L);
    bool Rr = is_realistic(R);

    // Compile-time constant comparison
    if (Lt && Rt) {
        i32 diff = 0;
        bool Lr = (Lt->traits & AU_TRAIT_REALISTIC) != 0;
        bool Rr = (Rt->traits & AU_TRAIT_REALISTIC) != 0;

        if (Lr || Rr) {
            f64 L_64 = (Lt == typeid(f32)) ? *(f32*)L->literal :
                       (Lt == typeid(f64)) ? *(f64*)L->literal :
                       ((f64)0.0);
            f64 R_64 = (Rt == typeid(f32)) ? *(f32*)R->literal :
                       (Rt == typeid(f64)) ? *(f64*)R->literal :
                       ((f64)0.0);
            diff = (L_64 < R_64) ? -1 : (L_64 > R_64) ? 1 : 0;
        } else {
            diff = compare(L->literal, R->literal);
        }

        Au    literal = _i32(diff);
        enode res     = value(etypeid(i32), LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), diff, 0));
        res->literal  = hold(literal);
        return res;
    }

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(i32));

    verify(context_func(a), "non-const compare must be in a function");

    // Class comparison - try member function first, fall back to pointer compare
    if (Lc || Rc) {
        if (!Lc) { enode t = L; L = R; R = t; }

        Au_t eq = find_member(L->au, "compare", AU_MEMBER_FUNC, 0, true);
        if (eq) {
            verify(eq->rtype == typeid(i32), "compare function must return i32, found %o", eq->rtype);
            return e_fn_call(a, (efunc)u(efunc, eq), a(L, R));
        }

        LLVMValueRef lt = LLVMBuildICmp(B, LLVMIntULT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildICmp(B, LLVMIntUGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx),  0, false);
        LLVMValueRef lt_val  = LLVMBuildSelect(B, lt, neg_one, zero, "lt_val");
        LLVMValueRef result  = LLVMBuildSelect(B, gt, pos_one, lt_val, "cmp_r");
        return value(etypeid(i32), result);
    }

    // Struct comparison - member-by-member with early exit
    if (Ls || Rs) {
        verify(Ls && Rs && (canonical(L) == canonical(R)),
            "struct type mismatch %o != %o", L->au->ident, R->au->ident);

        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
        LLVMBasicBlockRef exit_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "cmp_exit");

        array phi_vals   = array(alloc, 32, unmanaged, true);
        array phi_blocks = array(alloc, 32, unmanaged, true);

        members(L->au, mem) {
            if (is_func(mem)) continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(B, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(B, R->value, mem->index, "rv");
            etype        mdl = u(etype, mem);
            enode        cmp = e_cmp(a, value(mdl, lv), value(mdl, rv));

            LLVMValueRef is_nonzero = LLVMBuildICmp(
                B, LLVMIntNE, cmp->value,
                LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0), "cmp_nz");

            LLVMBasicBlockRef next_block = LLVMAppendBasicBlockInContext(a->module_ctx, fn, "cmp_next");

            // Record this block + value for phi, then branch
            push(phi_vals,   (Au)cmp->value);
            push(phi_blocks, (Au)LLVMGetInsertBlock(B));
            LLVMBuildCondBr(B, is_nonzero, exit_block, next_block);

            LLVMPositionBuilderAtEnd(B, next_block);
        }

        // Fallthrough: all members equal
        push(phi_vals,   (Au)LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 0, 0));
        push(phi_blocks, (Au)LLVMGetInsertBlock(B));
        LLVMBuildBr(B, exit_block);

        LLVMPositionBuilderAtEnd(B, exit_block);
        LLVMValueRef phi = LLVMBuildPhi(B, LLVMInt32TypeInContext(a->module_ctx), "cmp_phi");
        LLVMAddIncoming(phi, (LLVMValueRef*)vdata(phi_vals), (LLVMBasicBlockRef*)vdata(phi_blocks), len(phi_vals));

        return value(etypeid(i32), phi);
    }

    // Type coercion for mismatched types
    if (canonical(L) != canonical(R)) {
        if (L->au->abi_size >= R->au->abi_size) {
            R = e_create(a, (etype)L, (Au)R);
        } else {
            L = e_create(a, (etype)R, (Au)L);
        }
    }

    // Float comparison
    if (Lr || Rr) {
        LLVMValueRef lt = LLVMBuildFCmp(B, LLVMRealOLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildFCmp(B, LLVMRealOGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx),  0, false);
        LLVMValueRef lt_val  = LLVMBuildSelect(B, lt, neg_one, zero, "lt_val");
        LLVMValueRef result  = LLVMBuildSelect(B, gt, pos_one, lt_val, "cmp_r");
        return value(etypeid(i32), result);
    }

    // Integer comparison via subtraction
    return value(etypeid(i32), LLVMBuildSub(B, L->value, R->value, "cmp_i"));
}

enode aether_compatible(aether a, etype r, string n, AFlag f, array args) {
    members (r->au, mem) {
        efunc fn = u(efunc, mem);
        // must be function with optional name check
        if (!fn || (((f & mem->traits) != f) && (!n || !eq(n, mem->ident))))
            continue;
        if (mem->args.count != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        arg_types(mem, au) {
            etype fr = (etype)args->origin[ai];
            if (!convertible(fr, u(etype, au))) {
                compatible = false;
                break;
            }
            ai++;
        }
        if (compatible)
            return (enode)u(etype, mem);
    }
    return (enode)false;
}

enode aether_e_break(aether a, catcher cat) {
    a->is_const_op = false;
    if (!a->no_build) {
        if (cat->rtype) {
            LLVMValueRef def = LLVMConstNull(lltype(cat->rtype));
            LLVMAddIncoming(cat->phi->value, &def,
                &(LLVMBasicBlockRef){LLVMGetInsertBlock(B)}, 1);
        }
        LLVMBuildBr(B, cat->block);
    }
    return e_noop(a, null);
}

enode aether_e_bitwise_not(aether a, enode L) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));
    return value(L, LLVMBuildNot(B, L->value, "bitwise-not"));
}

enode aether_e_neg(aether a, enode L) {
    a->is_const_op = false;
    etype Lm = canonical(L);
    if (a->no_build) return e_noop(a, Lm);
    if (Lm->au->is_realistic)
        return value(L, LLVMBuildFNeg(B, L->value, "neg"));
    return value(L, LLVMBuildNeg(B, L->value, "neg"));
}

enode efunc_fptr(efunc f) {
    f->used = true;
    etype_implement((etype)f, false);
    etype func_ptr = pointer(f->mod, (Au)f);
    // in C, we get the address here
    // merely the f->value, with a type given as f->type or pointer(f->type) ?
    return enode(mod, f->mod, au, func_ptr->au, loaded, true, value, f->value);
}

// changes the type out, loads value
enode enode_deref(enode n) {
    aether a = n->mod;
    Au_t src = n->au->src;
    etype ca = canonical(n);

    if (a->no_build) return e_noop(a, u(etype, n->au->src));

    while (src && lltype(ca) == lltype(src))
        src = src->src;
    
    verify(!n->literal, "unable to dereference literal %o", isa(n->literal));
    verify(src, "cannot dereference type %o", n);

    return enode(mod, a, au, src, loaded, true, value,
        LLVMBuildLoad2(B, lltype(src), n->value, "deref"));
}

enode aether_e_not(aether a, enode L) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, etypeid(bool));

    LLVMValueRef result;
    etype Lm = canonical(L);
    if (is_ptr(Lm) || is_class(Lm)) {
        // for pointers, compare with null
        result = LLVMBuildICmp(B, LLVMIntEQ, L->value,
            LLVMConstNull(LLVMTypeOf(L->value)), "ptr-not");
    } else if (is_float(Lm)) {
        // for floats, compare with 0.0 and return true if > 0.0
        result = LLVMBuildFCmp(B, LLVMRealOLE, L->value,
                               LLVMConstReal(lltype(Lm), 0.0), "float-not");
    } else if (is_unsign(Lm)) {
        // for unsigned integers, compare with 0
        result = LLVMBuildICmp(B, LLVMIntULE, L->value,
                               LLVMConstInt(lltype(Lm), 0, 0), "unsigned-not");
    } else {
        // for signed integers, compare with 0
        result = LLVMBuildICmp(B, LLVMIntSLE, L->value,
                               LLVMConstInt(lltype(Lm), 0, 0), "signed-not");
    }
    return value(etypeid(bool), result);
}

enode enode_retain(enode mem) {
    aether a = mem->mod;
    etype mdl = (etype)evar_type((evar)mem);
    a->is_const_op = false;
    if (mdl->au->is_class && !a->no_build) {
        efunc fn_hold = (efunc)u(efunc, find_member(etypeid(Au)->au, "hold", AU_MEMBER_FUNC, 0, true));
        e_fn_call(a, fn_hold, a(mem));
    }
    return mem;
}

enode enode_release(enode mem) {
    aether a = mem->mod;
    etype mdl = (etype)evar_type((evar)mem);

    a->is_const_op = false;
    if (mdl->au->is_class && !a->no_build) {
        efunc fn_drop = (efunc)u(efunc, find_member(etypeid(Au)->au, "drop", AU_MEMBER_FUNC, 0, true));
        e_fn_call(a, fn_drop, a(mem));
    }
    return mem;
}

Au_t aether_pop_scope(aether a) {
    statements st = u(statements, top_scope(a));
    efunc prev_fn = context_func(a);

    if (prev_fn && !a->no_build)
        prev_fn->last_dbg = LLVMGetCurrentDebugLocation2(
            B);

    pop(a->lexical);
    a->top = (Au_t)last_element(a->lexical);

    efunc fn = context_func(a);
    if (fn && (fn != prev_fn) && !a->no_build) {
        LLVMPositionBuilderAtEnd(B, fn->entry);
        LLVMSetCurrentDebugLocation2(B, fn->last_dbg);
    }
    return a->top;
}

etype aether_return_type(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        etype ctx_u = u(etype, ctx);
        if (isa(ctx_u) == typeid(catcher) && ((catcher)ctx_u)->rtype)
            return ((catcher)ctx_u)->rtype;
        if (isa(ctx_u) == typeid(efunc) && ctx->rtype)
            return u(etype, ctx->rtype);
    }
    return null;
}

// ident is optional, and based on 
efunc aether_function(aether a, etype place, string ident, etype rtype, array args, u8 member_type, u32 traits, u8 operator_type) {
    Au_t context = place->au;
    Au_t au = def(context, ident->chars, AU_MEMBER_FUNC, traits);
    if (context->member_type == AU_MEMBER_MODULE) {
        if (au->module && au->module != context) {
            fprintf(stderr, "MODULE OVERWRITE [aether_function]: %s (%p) module %p -> %p\n", au->ident, au, au->module, context);
            exit(1);
        }
        au->module = context;
    } else if (context->is_class || context->is_struct) {
        if (au->module && au->module != context->module) {
            fprintf(stderr, "MODULE OVERWRITE [aether_function]: %s (%p) module %p -> %p\n", au->ident, au, au->module, context->module);
            exit(1);
        }
        au->module = context->module;
    }

    if (!rtype) rtype = etypeid(none);
    each(args, Au, arg) {
        Au_t a = au_arg(arg);
        Au_t argument = def(au, null, AU_MEMBER_VAR, 0);
        argument->src = a;
        micro_push(&au->args, (Au)argument);
    }
    au->src = (Au_t)hold(rtype->au);
    efunc f = efunc(mod, a, au, au, used, true, loaded, true);
    return f;
}

etype aether_record(aether a, etype place, etype based, string ident, u32 traits) { sequencer
    Au_t au = def(place->au, ident->chars, AU_MEMBER_TYPE, traits);
    au->context = (traits & AU_TRAIT_STRUCT) ? null : (based ? based->au : etypeid(Au)->au);
    etype n = etype(mod, a, au, au);
    return n;
}

Au enode_literal_value(enode a, Au_t of_type) {
    if (!a) return null;
    if (a && a->literal && (!of_type || inherits(isa(a->literal), of_type))) {
        return a->literal;
    } else if (of_type == typeid(i64) && inherits(isa(a->literal), typeid(shape))) {
        shape vshape = (shape)a->literal;
        Au res = _i64(vshape->data[0]);
        return res;
    }
    return null;
}

efunc aether_module_initializer(aether a) {
    if (a->fn_init) return a->fn_init;

    verify(a, "model given must be module (aether-based)");

    efunc init = function(a, (etype)a,
        f(string, "%s_initializer", a->au->ident), etypeid(none), array(),
        AU_MEMBER_FUNC, AU_TRAIT_MODINIT, OPType__undefined);
    init->au->access_type = interface_intern;
    init->has_code = true;

    etype_implement((etype)init, false);
    a->fn_init = hold(init);
    return init;
}

none aether_e_memcpy(aether a, enode _dst, enode _src, Au_t au_size) {
    if (a->no_build) return;
    a->is_const_op = false;
    LLVMTypeRef i8ptr = LLVMPointerTypeInContext(a->module_ctx, 0);
    LLVMValueRef dst  = LLVMBuildBitCast(B, _dst->value, i8ptr, "dst");
    LLVMValueRef src  = LLVMBuildBitCast(B, _src->value, i8ptr, "src");
    LLVMValueRef sz   = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), au_size->abi_size / 8, 0);
    LLVMValueRef align = LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 8, 0); // your alignment
    LLVMBuildMemCpy(B, dst, 0, src, 0, sz);
}

define_class(etype,      Au)
// its nice to have an args scope + function scope, but we get the same with a statements + function too
// plus we have .args on the au, we also don't need a list for args when we have Au-t.args
//define_class(eargs,      etype)
define_class(macro,      etype)
define_class(catcher,    etype)
define_class(statements, etype)
define_class(evar,       enode)
define_class(emeta,      etype) // we need a type to track the usage of actual meta args in context
define_class(enode,      etype) // not a member unless member is set (direct, or created with name)
define_class(edecl,      etype)
define_class(efunc,      enode)
define_class(enamespace, etype)
define_class(emodule,    etype)
define_class(aether,     emodule)

define_class(aclang_cc,  Au)
define_class(codegen,    Au)
define_class(code,       Au)

define_class(static_array, array)