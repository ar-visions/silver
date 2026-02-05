#define _GNU_SOURCE

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

#define au_lookup(sym)    lexical(a->lexical, sym)
#define elookup(sym) ({ \
    Au_t au = lexical(a->lexical, sym); \
    (au ? (etype)au->user : (etype)null); \
})

#define int_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(i##b)), \
        loaded, true, value, LLVMConstInt(lltype(au_lookup(stringify(i##b))), *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(u##b)), \
        loaded, true, value, LLVMConstInt(lltype(au_lookup(stringify(u##b))), *(u##b*)l, 0))

#define f32_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        loaded, true, value, LLVMConstReal(lltype(au_lookup(stringify(f##b))), *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        loaded, true, value, LLVMConstReal(lltype(au_lookup(stringify(f##b))), *(f##b*)l))

    
#define value(m,vr) enode(mod, a, value, vr, au, m->au, loaded, true)

typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;

none bp() {
    return;
}

LLVMTypeRef _lltype(Au a);
#define lltype(a) _lltype((Au)(a))

etype save;

etype etype_copy(etype mem) {
    Au_t au = isa(mem);
    etype n = (etype)alloc_new(au, 1, null);
    memcpy(n, mem, au->typesize);
    hold_members((Au)n);
    return n;
}

enode enode_value(enode mem) {
    if (!mem->loaded && !mem->au->is_imethod) {
        aether a = mem->mod;
        a->is_const_op = false;
        if (a->no_build) return e_noop(a, (etype)mem);
        
        static int seq = 0;
        seq++;
        if (seq == 1) {
            Au info = head(mem);
            seq = seq;
        }
        string id = f(string, "load2_%i", seq);
        Au info = head(mem);
        LLVMValueRef loaded = LLVMBuildLoad2(
            a->builder, lltype(mem->au), mem->value, id->chars);
        enode  res = enode(mod, a, value, loaded, loaded, true, au, mem->au); // resolve(mem)->au);
        return res;
    } else {
        return mem;
    }
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

enode aether_e_assign(aether a, enode L, Au R, OPType op_val) {
    a->is_const_op = false;
    if (a->no_build)
        return e_noop(a, null); // should never happen in expressions

    verify(op_val >= OPType__bind && op_val <= OPType__assign_xor,
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

    static int seq = 0;
    seq++;
    if (seq == 1) {
        Au info = head(L);
        seq = seq;
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
    enode res = null;

    if (op_val <= OPType__assign) { // bind and assign...
        res = rR;
    } else {
        string op_name = (string)e_str(OPType, op_val);
        res = value(
            L,
            op_table[op_val - OPType__assign_add].f_build_op(
                a->builder,
                L->value,
                rR->value,
                op_name->chars));
    }

    /* ------------------------------------------------------------
     * Phase 5: type reconciliation
     * ------------------------------------------------------------ */
    if (res->au != L->au->src)
        res = e_operand(a, (Au)res, (etype)L);

    /* ------------------------------------------------------------
     * Phase 6: store
     * ------------------------------------------------------------ */
    //if (!is_ptr(L) && (is_struct(L) || is_prim(L)) && is_ptr(res))
    //    e_memcpy(a, L, res, L->au);
    //else
    LLVMBuildStore(a->builder, res->value, L->value);

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


none etype_implement(etype t);

static etype etype_ptr(aether mod, Au_t a) {
    verify(a && (isa(a) == typeid(Au_t_f)), "ptr requires Au_t, given %s", isa(a)->ident);
    verify(isa(a) != typeid(etype), "etype_ptr unexpected type");
    Au_t au = a;
    if (au->ptr) return au->ptr->user;
    au->ptr              = def(mod->au, null, AU_MEMBER_TYPE, 0);
    au->ptr->is_pointer  = true;
    au->ptr->src         = au;
    au->ptr->user        = etype(mod, mod, au, au->ptr);
    return au->ptr->user;
}

etype pointer(aether mod, Au a) {
    Au_t au = au_arg(a);
    if (au->member_type == AU_MEMBER_VAR)
        return etype_ptr(mod, au->src);
    
    return etype_ptr(mod, au);
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

etype etype_traits(Au a, int traits) {
    Au_t au;
    if (isa(a) == typeid(etype)) {
        verify(lltype(a), "no type found on %o 1", a);
        au = ((etype) a)->au;
    }
    else if (isa(a) == typeid(enode)) au = ((enode)a)->au;
    else if (isa(a) == typeid(Au_t_f)) {
        verify(lltype(a), "no type found on %o 2", a);
        au = (Au_t)a;
    }
    verify(au, "unhandled input");
    //if (au->is_class) {
    //    verify(au->ptr, "expected ptr for class");
    //    au = au->ptr;
    //}
    //return (au->traits & traits) == traits ? ((etype)au->user) : null;
    return (((etype)au->user)->au->traits & traits) == traits ? ((etype)au->user) : null;
}

/// C type rules implemented
etype determine_rtype(aether a, OPType optype, etype L, etype R) {
    if (optype >= OPType__bind && optype <= OPType__assign_left)
        return L;  // Assignment operations always return the type of the left operand
    else if (optype == OPType__value_default ||
             optype == OPType__cond_value    ||
             optype == OPType__or            ||
             optype == OPType__and           ||
             optype == OPType__xor) {
        if (is_bool(L) && is_bool(R))
            return typeid(bool)->user;  // Logical operations on booleans return boolean
        // For bitwise operations, fall through to numeric promotion
    }

    // Numeric type promotion
    if (is_realistic(L) || is_realistic(R)) {
        // If either operand is float, result is float
        if (is_double(L) || is_double(R))
            return typeid(f64)->user;
        else
            return typeid(f32)->user;
    }

    // Integer promotion
    int L_size = L->au->abi_size;
    int R_size = R->au->abi_size;
    if (L_size > R_size)
        return L;
    else if (R_size > L_size)
        return R;

    bool L_signed = is_sign(L);
    bool R_signed = is_sign(R);
    if (L_signed && R_signed)
        return L;  // Both same size and signed
    else if (!L_signed && !R_signed)
        return L;  // Both same size and unsigned
    
    return L_signed ? R : L;  // Same size, one signed one unsigned
}

// does switcher-oo for class type
LLVMTypeRef _lltype(Au a) {
    //Au_t a_type = isa(a);
    Au_t au = (Au_t)a;
    if (instanceof(a, etype)) {
        au = ((etype) a)->au;
    }
    else if (isa(a) == typeid(Au_t_f)) {
        au = (Au_t)a;
    }
    if (au->member_type == AU_MEMBER_VAR) {
        au = au->src;
    }
    Au_t prev = au;
    while (au && !au->lltype) {
        au = au->src;
    }
    if (!au || !au->lltype) {
        raise(SIGTRAP);
    }
    verify(au && au->lltype, "no type found on %o 3", prev);
    
    if (au->is_class) {
        verify(au->ptr, "expected ptr for class");
        au = au->ptr;
    }
    LLVMTypeRef res = (LLVMTypeRef)au->lltype;
    verify(res, "no type found on %o?", au);
    return res;
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
    if (a->no_build) return e_noop(a, typeid(bool)->user);

    // Get the type pointer for L
    enode L_type =  e_offset(a, L, _i64(-sizeof(Au)));
    enode L_ptr  =    e_load(a, L, null);
    enode R_ptr  = e_operand(a, R, null);

    // Create basic blocks for the loop
    LLVMBasicBlockRef block      = LLVMGetInsertBlock(a->builder);
    LLVMValueRef      fn         = LLVMGetBasicBlockParent(block);
    LLVMBasicBlockRef loop_block = LLVMAppendBasicBlock(fn, "inherit_loop");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(fn, "inherit_exit");

    // Branch to the loop block
    LLVMBuildBr(a->builder, loop_block);

    // Loop block
    LLVMPositionBuilderAtEnd(a->builder, loop_block);
    LLVMValueRef phi = LLVMBuildPhi(a->builder, lltype(L_ptr), "current_type");
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
    LLVMBuildCondBr(a->builder, loop_cond->value, loop_block, exit_block);

    // Update the phi enode
    LLVMAddIncoming(phi, &parent->value, &loop_block, 1);

    // Exit block
    LLVMPositionBuilderAtEnd(a->builder, exit_block);
    LLVMValueRef result = LLVMBuildPhi(a->builder, lltype(cmp), "inherit_result");
    LLVMAddIncoming(result, &cmp->value, &loop_block, 1);
    LLVMAddIncoming(result, &(LLVMValueRef){LLVMConstInt(LLVMInt1Type(), 0, 0)}, &block, 1);

    return value(typeid(bool)->user, result);
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
    etype bool_t = typeid(bool)->user;

    if (a->no_build) return e_noop(a, bool_t);

    struct cmp_entry* cmp = cmp_lookup(optype);
    verify(cmp, "invalid comparison operator");

    // 1. Primitive → primitive fast path
    if (is_prim(L) && is_prim(R)) {
        if (is_realistic(L) || is_realistic(R)) {
            return value(bool_t,
                LLVMBuildFCmp(a->builder, cmp->fp_pred,
                              L->value, R->value, cmp->name));
        }
        LLVMIntPredicate pred = (is_unsign(L) && is_unsign(R)) ? cmp->ui_pred : cmp->si_pred;
        return value(bool_t,
            LLVMBuildICmp(a->builder, pred,
                          L->value, R->value, cmp->name));
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
        if (!is_class(L)) {
            enode tmp = L; L = R; R = tmp;
        }

        Au_t fn = find_member(L->au, "compare", AU_MEMBER_FUNC, true);
        verify(fn, "class %s has no compare() method", L->au->ident);

        enode cmp_result = e_fn_call(a, (efunc)fn->user, a(L, R));
        LLVMValueRef zero = LLVMConstInt(lltype(cmp_result), 0, false);

        return value(bool_t,
            LLVMBuildICmp(a->builder, cmp->si_pred,
                          cmp_result->value, zero, cmp->name));
    }

    // 4. Struct → recursively compare members (only for eq/ne)
    if (is_struct(L) && is_struct(R) && (optype == OPType__equal || optype == OPType__not_equal)) {
        verify(L->au == R->au, "struct type mismatch");

        LLVMValueRef accum = LLVMConstInt(LLVMInt1Type(), 1, false);

        for (int i = 0; i < L->au->members.count; i++) {
            Au_t m = (Au_t)L->au->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR)
                continue;

            LLVMValueRef lv = LLVMBuildExtractValue(a->builder, L->value, m->index, "");
            LLVMValueRef rv = LLVMBuildExtractValue(a->builder, R->value, m->index, "");

            enode le = value(m->user, lv);
            enode re = value(m->user, rv);

            enode sub = aether_e_cmp_op(a, OPType__equal, le, re);
            accum = LLVMBuildAnd(a->builder, accum, sub->value, "and");
        }

        // For not_equal, invert the result
        if (optype == OPType__not_equal)
            accum = LLVMBuildNot(a->builder, accum, "not");

        return value(bool_t, accum);
    }

    // 5. Fallback for primitives
    if (is_realistic(L)) {
        return value(bool_t,
            LLVMBuildFCmp(a->builder, cmp->fp_pred,
                          L->value, R->value, cmp->name));
    }

    LLVMIntPredicate pred = (is_unsign(L) && is_unsign(R)) ? cmp->ui_pred : cmp->si_pred;
    return value(bool_t,
        LLVMBuildICmp(a->builder, pred, L->value, R->value, cmp->name));
}

enode aether_e_eq(aether a, enode L, enode R) {
    a->is_const_op = false;
    etype bool_t = typeid(bool)->user;

    if (a->no_build) return e_noop(a, bool_t);

    // 1. Primitive → primitive fast path
    if (is_prim(L) && is_prim(R)) {
        if (is_realistic(L) || is_realistic(R)) {
            // floating compare
            return value(bool_t,
                LLVMBuildFCmp(a->builder, LLVMRealOEQ,
                              L->value, R->value, "eq-f"));
        }

        return value(bool_t,
            LLVMBuildICmp(a->builder, LLVMIntEQ,
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

        Au_t fn = find_member(L->au, "compare", AU_MEMBER_FUNC, true);
        verify(fn, "class %s has no compare() method", L->au->ident);

        enode cmp = e_fn_call(a, (efunc)fn->user, a(L, R));

        // compare result against zero (i64 or i32)
        LLVMValueRef zero = LLVMConstInt(lltype(cmp), 0, false);

        return value(bool_t,
            LLVMBuildICmp(a->builder, LLVMIntEQ,
                          cmp->value, zero, "cmp-zero"));
    }

    // 4. Struct → recursively compare members
    if (is_struct(L) && is_struct(R)) {
        verify(L->au == R->au, "struct type mismatch");

        LLVMValueRef accum =
            LLVMConstInt(LLVMInt1Type(), 1, false);

        for (int i = 0; i < L->au->members.count; i++) {
            Au_t m = (Au_t)L->au->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR)
                continue;

            LLVMValueRef lv =
                LLVMBuildExtractValue(a->builder, L->value, m->index, "");
            LLVMValueRef rv =
                LLVMBuildExtractValue(a->builder, R->value, m->index, "");

            enode le = value(m->user, lv);
            enode re = value(m->user, rv);

            enode sub = aether_e_cmp_op(a, OPType__equal, le, re);

            accum = LLVMBuildAnd(a->builder, accum, sub->value, "and");
        }

        return value(bool_t, accum);
    }

    // 5. Fallback for all primitives
    if (is_realistic(L))
        return enode(mod, a, au, bool_t->au, loaded, true, value,
            LLVMBuildFCmp(a->builder, LLVMRealOEQ,
                          L->value, R->value, "eq-f"));

    LLVMValueRef nv = LLVMBuildICmp(a->builder, LLVMIntEQ, L->value, R->value, "eq-i");
    return enode(mod, a, value, nv, loaded, true, au, bool_t->au);
}



enode aether_e_eq_prev(aether a, enode L, enode R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, typeid(bool)->user);

    if (is_prim(L) && is_prim(R)) {
        // needs reality check
        enode        cmp = e_cmp(a, L, R);
        LLVMValueRef z   = LLVMConstInt(LLVMInt32Type(), 0, 0);
        return value(typeid(bool)->user, LLVMBuildICmp(a->builder, LLVMIntEQ, cmp->value, z, "cmp-i"));
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
        enode res = value(typeid(bool)->user, LLVMConstInt(LLVMInt1Type(), is_eq, 0));
        res->literal = hold(literal);
        return res;
    }

    if (Lc || Rc) {
        if (!Lc) {
            enode t = L;
            L = R;
            R = t;
        }
        Au_t au_f = find_member(L->au, "eq", AU_MEMBER_FUNC, true);
        if (au_f) {
            efunc eq = (efunc)au_f->user;
            // check if R is compatible with argument
            // if users want to allow different data types, we need to make the argument more generic
            // this is better than overloads since the code is in one place
            return e_fn_call(a, eq, a(L, R));
        }
    } else if (Ls || Rs) {
        // iterate through struct members, checking against each with recursion
        LLVMValueRef result = LLVMConstInt(LLVMInt1Type(), 1, 0);

        verify(Ls && Rs && (L->au == R->au),
            "struct type mismatch %o != %o", L->au, R->au);
        
        for (int i = 0; i < L->au->members.count; i++) {
            Au_t mem = (Au_t)L->au->members.origin[i];
            if (mem->member_type != AU_MEMBER_VAR)
                continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(a->builder, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(a->builder, R->value, mem->index, "rv");
            enode        le  = value(mem->user, lv);
            enode        re  = value(mem->user, rv);
            enode        cmp = e_cmp_op(a, OPType__equal, le, re);
            result = LLVMBuildAnd(a->builder, result, cmp->value, "eq-struct-and");
        }
        return value(typeid(bool)->user, result);

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
        return value(typeid(bool)->user, LLVMBuildFCmp(a->builder, LLVMRealOEQ, L->value, R->value, "eq-f"));
    
    return value(typeid(bool)->user, LLVMBuildICmp(a->builder, LLVMIntEQ, L->value, R->value, "eq-i"));
}

static void print_all(aether mod, symbol label, array list);

enode aether_e_eval(aether a, string value) {
    array t = (array)tokens(target, (Au)a, parser, a->parse_f, input, (Au)value);
    push_tokens(a, (tokens)t, 0);
    print_all(a, "these are tokens", t);
    static int seq = 0;
    seq++;
    if (seq == 2) {
        seq = seq;
    }
    enode n = (enode)a->parse_expr((Au)a, null); // this should not output i32 (2nd time)
    enode s = e_create(a, typeid(string)->user, (Au)n);
    pop_tokens(a, false);
    return s;
}


enode aether_e_interpolate(aether a, string str) {
    enode accum = null;
    array sp    = split_parts(str);
    etype mdl   = typeid(string)->user;

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);

    a->expr_level++; // this is not normalized, however its not such a problem to store this on aether
    each (sp, ipart, s) {
        enode val = e_create(a, mdl, s->is_expr ?
            (Au)e_eval(a, s->content) : (Au)s->content);
        accum = accum ? e_add(a, (Au)accum, (Au)val) : val;
    }
    a->expr_level--;
    return accum;
}

static LLVMValueRef const_cstr(aether a, cstr value, i32 len) {
    LLVMContextRef ctx = LLVMGetGlobalContext();

    // 1. make a constant array from the raw bytes (null-terminated!)
    LLVMValueRef strConst = LLVMConstString(value, len, /*DontNullTerminate=*/0);

    // 2. create a global variable of that array type
    LLVMValueRef gv = LLVMAddGlobal(a->module_ref, LLVMTypeOf(strConst), "const_cstr");
    LLVMSetInitializer(gv, strConst);
    LLVMSetGlobalConstant(gv, 1);
    LLVMSetLinkage(gv, LLVMPrivateLinkage);

    // 3. build a GEP to point at element [0,0] (first char)
    LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
    LLVMValueRef idxs[] = {zero, zero};
    LLVMValueRef cast_i8 = LLVMConstGEP2(LLVMTypeOf(strConst), gv, idxs, 2);

    return cast_i8;
}

enode e_operand_primitive(aether a, Au op) {
    Au_t t = isa(op);
         if (instanceof(op, enode) || instanceof(op, efunc)) return (enode)op;
    else if (instanceof(op, etype)) return e_typeid(a, (etype)op);
    else if (instanceof(op, handle)) {
        uintptr_t v = (uintptr_t)*(void**)op; // these should be 0x00, infact we may want to assert for this

        // create an i64 constant with that address
        LLVMValueRef ci = LLVMConstInt(LLVMInt64TypeInContext(a->module_ctx), v, 0);

        // cast to a generic void* (i8*) pointer type
        LLVMTypeRef  hty = LLVMPointerType(LLVMInt8TypeInContext(a->module_ctx), 0);
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


enode aether_e_op(aether a, OPType optype, string op_name, Au L, Au R) {
    a->is_const_op = false; // we can be granular about this, but its just not worth the complexity for now
    enode mL = (enode)instanceof(L, enode); 
    enode LV = e_operand(a, L, null);
    enode RV = e_operand(a, R, null);

    // check for overload
    if (op_name && instanceof(L, enode) && is_class(L)) {
        enode Ln = (enode)L;
        etype rec  = etype_traits((Au)Ln, AU_TRAIT_CLASS);
        if (rec) {
            etype Lt = null;
            for (int i = 0; i < rec->au->members.count; i++) {
                Au_t mem = (Au_t)rec->au->members.origin[i];
                if (mem->operator_type == optype) {
                    verify(mem->user, "expected user structure for member %s", mem->ident);
                    Lt = mem->user;
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

    LLVMValueRef RES;
    LLVMTypeRef  LV_type = LLVMTypeOf(LV->value);
    LLVMTypeKind vkind = LLVMGetTypeKind(LV_type);

    enode LL = optype <= OPType__assign ? LV : e_create(a, rtype, (Au)LV); // we dont need the 'load' in here, or convert even
    enode RL = e_create(a, rtype, (Au)RV);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = a->builder;
    Au literal = null;

    if (optype == OPType__or || optype == OPType__and) { // logical operators

        // ensure both operands are i1
        rtype = typeid(bool)->user;
        LL = e_create(a, rtype, (Au)LL); // generate compare != 0 if not already i1
        RL = e_create(a, rtype, (Au)RL);

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

    } else if (optype >= OPType__bind && optype <= OPType__assign_left) {

        // assignments perform a store
        verify(mL, "left-hand operator must be a emember");
        // already computed in R-value
        if (optype <= OPType__assign) {
            RES = RL->value;
            literal = RL->literal;
        } else
            RES = op_table[optype - OPType__assign_add].f_build_op(B, LL->value, RL->value, N);
        LLVMBuildStore(B, RES, mL->value);

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

enode aether_e_operand(aether a, Au op, etype src_model) {
    if (instanceof(op, enode) || instanceof(op, efunc)) {
        op = (Au)enode_value((enode)op);
    }
    if (!op) {
        if (is_ptr(src_model))
            return e_null(a, src_model);
        return enode(mod, a, au, au_lookup("none"), loaded, true, value, null);
    }
    
    if (isa(op) == typeid(string))
        return e_create(a, src_model,
            (Au)e_interpolate(a, (string)op));
    
    if (isa(op) == typeid(const_string))
        return e_create(a, src_model, (Au)e_operand_primitive(a, op)); //e_create(a, src_model, (Au)e_operand_primitive(a, op));
    
    if (isa(op) == typeid(map)) {
        return e_create(a, src_model, op);
    }
    Au_t op_isa = isa(op);
    if (instanceof(op, array)) {
        verify(src_model != null, "expected src_model with array data");
        return e_create(a, src_model, op);
    }

    enode r = e_operand_primitive(a, op);
    return src_model ? e_create(a, src_model, (Au)r) : r;
}

enode aether_e_null(aether a, etype mdl) {
    if (!mdl) mdl = typeid(handle)->user;
    if (!is_ptr(mdl) && mdl->au->is_class) {
        mdl = etype_ptr(a, mdl->au); // we may only do this on a limited set of types, such a struct types
    }
    verify(is_ptr(mdl), "%o not compatible with null value", mdl);
    return enode(mod, a, loaded, true, value, LLVMConstNull(lltype(mdl)), au, mdl->au);
}

enode aether_e_primitive_convert(aether a, enode expr, etype rtype);


enode aether_e_meta_ids(aether a, array meta) {
    Au_t atype = au_lookup("Au_t");
    etype atype_vector = etype_ptr(a, atype);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, atype_vector);

    if (!meta || !len(meta))
        return e_null(a, atype_vector);

    i32 ln = len(meta);
    LLVMTypeRef elemTy = lltype(atype_vector);
    LLVMTypeRef arrTy = LLVMArrayType(elemTy, ln);
    LLVMValueRef* elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = meta->origin[i];
        enode n;
        Au_t type = isa(m);

        if (instanceof(m, etype))
            n = e_typeid(a, (etype)m);
        else if (instanceof(m, shape)) {
            shape s = (shape)m;
            array ar = array(alloc, s->count);
            for (int ii = 0; ii < s->count; ii++)
                push(ar, _i64(s->data[ii]));
            enode fn = context_func(a);
            n = e_create(a, typeid(shape)->user, (Au)m(
                "count", _i64(s->count),
                "data",  e_const_array(a, typeid(i64)->user, ar)
            ));
            fn = fn;
        } else {
            verify(false, "unsupported design-time meta type");
        }

        elems[i] = n->value;
    }

    // stack allocate: alloca [ln x atype]
    LLVMValueRef arr_alloc = LLVMBuildAlloca(a->builder, arrTy, "meta_ids");

    // store each element into the allocated array
    for (i32 i = 0; i < ln; i++) {
        LLVMValueRef idxs[] = {
            LLVMConstInt(LLVMInt32Type(), 0, 0), // start of array
            LLVMConstInt(LLVMInt32Type(), i, 0)  // element index
        };
        LLVMValueRef gep = LLVMBuildGEP2(a->builder, arrTy, arr_alloc, idxs, 2, "");
        LLVMBuildStore(a->builder, elems[i], gep);
    }

    free(elems);

    // return pointer to array
    return enode(mod, a, loaded, true, value, arr_alloc, au, etype_ptr(a, atype)->au);
}

none aether_e_fn_return(aether a, Au o) {
    Au_t au_ctx = find_context(a->lexical, AU_MEMBER_FUNC, 0);
    verify (au_ctx, "function not found in context");
    etype f = au_ctx->user;

    a->is_const_op = false;
    if (a->no_build) return;

    f->au->has_return = true;

    if (is_void(f->au->rtype)) {
        LLVMBuildRetVoid(a->builder);
        return;
    }
    
    enode conv = e_create(a, (etype)f->au->rtype->user, o);
    LLVMBuildRet(a->builder, conv->value);
}

etype formatter_type(aether a, cstr input) {
    cstr p = input;
    // skip flags/width/precision
    while (*p && strchr("-+ #0123456789.", *p)) p++;
    if (strncmp(p, "lld", 3) == 0 || 
        strncmp(p, "lli", 3) == 0) return typeid(i64)->user;
    if (strncmp(p, "llu", 3) == 0) return typeid(u64)->user;
    
    switch (*p) {
        case 'd': case 'i':
                  return typeid(i32)->user; 
        case 'u': return typeid(u32)->user;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                  return typeid(f64)->user;
        case 's': return typeid(symbol)->user;
        case 'p': return typeid(symbol)->user->au->ptr->user;
    }
    fault("formatter implementation needed");
    return null;
}

static etype func_target(etype fn) {
    if (fn->au->is_imethod)
        return fn->au->context->user;
    
    return null;
}

// often we have either a struct or a class* 
// and we want to ensure a pointer to either one, in-order to make a target
static etype ensure_pointer(etype t) {
    if (!t || t->au->is_pointer || t->au->is_class) return t;
    return etype_ptr(t->mod, t->au);
}

enode aether_e_is(aether a, enode L, Au R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, typeid(bool)->user);

    enode L_type =  e_offset(a, L, _i64(-sizeof(struct _Au)));
    enode L_ptr  =    e_load(a, L_type, null); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = e_operand(a, R, null);
    return aether_e_eq(a, L_ptr, R_ptr);
}

void aether_e_print_node(aether a, enode n) {
    if (a->no_build) return;
    /// would be nice to have a lookup method on import
    /// call it im_lookup or something so we can tell its different
    efunc   printf_fn  = (efunc)elookup("printf");
    etype   mdl_cstr   = typeid(cstr)->user;
    etype   mdl_symbol = typeid(symbol)->user;
    etype   mdl_i32    = typeid(i32)->user;
    etype   mdl_i64    = typeid(i64)->user;
    cstr fmt = null;
    if (canonical(n) == mdl_cstr || canonical(n) == mdl_symbol) {
        fmt = "%s";
    } else if (canonical(n) == mdl_i32) {
        fmt = "%i";
    } else if (canonical(n) == mdl_i64) {
        fmt = "%lli";
    }
    verify(fmt, "eprint_node: unsupported model: %o", n);
    e_fn_call(a, printf_fn, 
        a(e_operand(a, (Au)string(fmt), (etype)n)));
}

bool etype_inherits(etype mdl, etype base);

etype evar_type(evar a) {
    if (a->au->member_type == AU_MEMBER_VAR)
        return (etype)a->au->src->user;
    return (etype)a->au->user;
}


// no disassemble!
enode aether_e_short_circuit(aether a, OPType optype, enode L) {
    LLVMBuilderRef B = a->builder;
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(B));
    
    // get L's actual value and its bool conversion
    etype m_bool = typeid(bool)->user;
    enode L_bool = e_create(a, m_bool, (Au)L);  // for branching
    // but keep L->value as the actual value to return
    
    LLVMBasicBlockRef eval_r_block = LLVMAppendBasicBlock(func, "eval_r");
    LLVMBasicBlockRef merge_block  = LLVMAppendBasicBlock(func, "merge");
    LLVMBasicBlockRef L_block      = LLVMGetInsertBlock(B);
    
    if (optype == OPType__or) {
        // or: if L truthy, return L; else evaluate and return R
        LLVMBuildCondBr(B, L_bool->value, merge_block, eval_r_block);
    } else {
        // and: if L falsy, return L; else evaluate and return R
        LLVMBuildCondBr(B, L_bool->value, eval_r_block, merge_block);
    }
    
    // eval R block
    LLVMPositionBuilderAtEnd(B, eval_r_block);
    enode R = (enode)a->parse_expr((Au)a, null);
    LLVMBasicBlockRef R_block = LLVMGetInsertBlock(B);
    LLVMBuildBr(B, merge_block);
    
    // figure out common type
    etype rtype = (etype)evar_type((evar)L);
    
    // merge block with PHI - returning actual values, not bools
    LLVMPositionBuilderAtEnd(B, merge_block);
    LLVMValueRef phi = LLVMBuildPhi(B, lltype(rtype), "sc_result");
    
    // convert both to common type if needed
    enode L_conv = e_create(a, rtype, (Au)L);
    enode R_conv = e_create(a, rtype, (Au)R);
    
    LLVMValueRef      vals[] = { L_conv->value, R_conv->value };
    LLVMBasicBlockRef bbs[]  = { L_block,       R_block };
    LLVMAddIncoming(phi, vals, bbs, 2);
    
    return enode(
        mod,    a,
        au,     rtype->au,
        loaded, true,
        value,  phi);
}

bool inherits(Au_t src, Au_t check);

none enode_inspect(enode a) {
    int test2 = 2;
    test2    += 2;
}

enode aether_lambda_fcall(aether a, efunc mem, array user_args) {
    // access fn and ctx from lambda instance
    efunc fn_ptr     = (efunc)enode_value(access(mem, string("fn")));
    enode ctx_ptr    = enode_value(access(mem, string("context"))); // we now have target inside of context
    enode rtype      = (enode)mem->meta->origin[0];
    enode lambda_fn  = (enode)mem->au->src->user;

    array args = array(alloc, 32, assorted, true);

    push(args, (Au)ctx_ptr);
    each(user_args, Au, arg)
        push(args, arg);
    
    fn_ptr->meta = hold(mem->meta);
    return e_fn_call(a, fn_ptr, args);
}


enode aether_e_fn_call(aether a, efunc fn, array args) {
    bool funcptr = is_func_ptr((Au)fn);
    if (!funcptr && !fn->used)
         fn->used = true;
    
    Au_t au = fn->au;
    if (au->ident && strcmp(au->ident, "with_symbol") == 0) {
        int test2 = 2;
        test2    += 2;
    }

    static int seq = 0;
    seq++;

    a->is_const_op = false; 
    if (a->no_build) return e_noop(a, fn->au->rtype->user);

    etype_implement((etype)fn);
    
    Au    arg0             = args ? get(args, 0) : null;
    Au_t  type0 = isa(arg0);
    enode first_arg        = arg0 ? e_operand(a, arg0, null) : null;
    etype user_target_type = first_arg ? evar_type((evar)first_arg) : null;
    enode target_type      = (!funcptr && fn->target) ? (enode)fn->target : (enode)null;
    enode f                = len(args) ? (enode)user_target_type : null;
    bool  is_Au            = !target_type || inherits(au_arg_type((Au)target_type->au), typeid(Au));

    verify(!target_type || is_Au || etype_inherits((etype)target_type, (etype)f), "target mismatch %i", seq);

    int n_args = args ? len(args) : 0;
    
    verify(funcptr || n_args == fn->au->args.count ||
        (n_args > fn->au->args.count && fn->au->is_vargs),
        "arg count mismatch on %o", fn);
    
    Au_t type_id = isa(fn);
    LLVMValueRef* arg_values = calloc(n_args, sizeof(LLVMValueRef));
    LLVMTypeRef*  arg_types  = calloc(n_args, sizeof(LLVMValueRef));
    LLVMTypeRef   F          = funcptr ? null : lltype(fn);
    LLVMValueRef  V          = fn->value;

    // -----------------------------------------------------------------------
    // Dynamic Dispatch Logic                           ( hold onto your butts )
    // -----------------------------------------------------------------------
    // If this is an instance method, we must look up the implementation 
    // in the runtime type's vtable (ft) rather than using the static symbol.
    if (fn->au->is_imethod && !funcptr && !(target_type && target_type->target && target_type->target->avoid_ftable)) {
        verify(n_args > 0, "instance method %o requires 'this' argument", fn);
        
        // 1. Get the instance pointer ('this' is always the first argument)
        // We need the raw value to perform pointer arithmetic
        Au    arg0       = get(args, 0);
        enode first_arg  = e_operand(a, arg0, null);
        LLVMValueRef instance = first_arg->value;

        // 2. Get the Object Header
        // The ABI defines the header as residing immediately before the object pointer.
        // header = ((struct _Au*)instance) - 1
        LLVMTypeRef  i8_ptr_ty  = LLVMPointerType(LLVMInt8Type(), 0);
        LLVMValueRef i8_this    = LLVMBuildBitCast(a->builder, instance, i8_ptr_ty, "this_i8");
        
        // Calculate offset: -sizeof(struct _Au)
        // We use the compiler's knowledge of the struct size to burn the constant into IR
        LLVMValueRef offset_neg = LLVMConstInt(LLVMInt64Type(), -(int)sizeof(struct _Au), true);
        LLVMValueRef header_ptr = LLVMBuildGEP2(a->builder, LLVMInt8Type(), i8_this, &offset_neg, 1, "header_ptr");

        // 3. Load the Runtime Type (Au_t)
        // The 'type' is the first member of struct _Au
        LLVMTypeRef  Au_t_ptr_ty = LLVMPointerType(i8_ptr_ty, 0); // void** (pointer to type pointer)
        LLVMValueRef header_cast = LLVMBuildBitCast(a->builder, header_ptr, Au_t_ptr_ty, "header_cast");
        LLVMValueRef type_ptr    = LLVMBuildLoad2(a->builder, i8_ptr_ty, header_cast, "type_ptr");

        // 4. Access the Function Table (ft)
        // The function table is located at `offsetof(struct _Au_f, ft)` within the type object.
        LLVMValueRef type_i8     = LLVMBuildBitCast(a->builder, type_ptr, i8_ptr_ty, "type_i8");
        LLVMValueRef ft_offset   = LLVMConstInt(LLVMInt64Type(), offsetof(struct _Au_f, ft), false);
        LLVMValueRef ft_base     = LLVMBuildGEP2(a->builder, LLVMInt8Type(), type_i8, &ft_offset, 1, "ft_base");

        // 5. Index into the Function Table
        // Retrieve the specific method index assigned during schema creation
        int method_index = fn->au->index; 
        verify(method_index > 0, "method %o has invalid vtable index", fn);

        LLVMValueRef ft_array      = LLVMBuildBitCast(a->builder, ft_base, Au_t_ptr_ty, "ft_array");
        LLVMValueRef method_idx    = LLVMConstInt(LLVMInt32Type(), method_index, false);
        LLVMValueRef func_ptr_addr = LLVMBuildGEP2(a->builder, i8_ptr_ty, ft_array, &method_idx, 1, "func_ptr_addr");
        
        // 6. Load and Cast the Function Pointer
        LLVMValueRef func_ptr_void = LLVMBuildLoad2(a->builder, i8_ptr_ty, func_ptr_addr, "func_ptr_void");
        
        // Cast the generic void* from vtable to the specific function signature we expect
        V = LLVMBuildBitCast(a->builder, func_ptr_void, LLVMTypeOf(fn->value), "vmethod");
    }

    int index = 0;
    if (target_type) {
        // we do not 'need' target so we dont have it.
        // we get instance from the fact that its within a class/struct context and is_imethod
        Au_t au_type = target_type->au;
        if (au_type->member_type == AU_MEMBER_VAR) au_type = au_type->src;

        etype cast_to = ensure_pointer(au_type->user);
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
        
            if (is_lambda(fn)) {
                verify(instanceof(arg_value, enode), "expected enode for runtime function call");
                enode n = (enode)arg_value;
                verify(n->value, "expected enode value for arg %i", index);
                arg_values[index] = n->value;
                arg_types[index] = lltype(n);
            } else {
                Au_t fn_decl = funcptr ? au_arg_type((Au)fn->au) : fn->au;
                Au_t arg_type = (Au_t)array_get((array)&fn_decl->args, i);
                evar  arg = (evar)arg_type->user;
                enode n   = (enode)instanceof(arg_value, enode);
                if (index == fmt_idx) {
                    Au fmt = n ? (Au)instanceof(n->literal, const_string) : null;
                    verify(fmt, "formatter functions require literal, constant strings");
                }
                etype arg_t = au_arg_type((Au)arg_type)->user;
                enode conv = e_create(a, arg_t, arg_value);
                arg_values[index] = conv->value;
                if (funcptr)
                    arg_types[index] = lltype(arg_t);
            }
            i++;
            index++;
        }
    }
    int istart = index;
    
    if (funcptr && fmt_idx >= 0) {
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
    sprintf(call_seq, "call_%i", seq2);
    if (seq2 == 6) {
        seq2 = seq2;
    }

    etype rtype = is_lambda(fn) ?
        (etype)fn->meta->origin[0] :
        (etype)fn->au->rtype->user;
    
    if (funcptr) F = LLVMFunctionType(lltype(rtype), arg_types, n_args, false);
    
    LLVMValueRef R = LLVMBuildCall2(a->builder, F, V, arg_values, index, is_void_ ? "" : call_seq);
    free(arg_values);
    free(arg_types);

    return enode(mod, a, au, rtype->au, loaded, true, value, R);
}

static bool is_same(Au a, Au b) {
    Au_t au_a = au_arg(a);
    Au_t au_b = au_arg(b);
    return resolve(au_a->user) == resolve(au_b->user) && 
        ref_level((Au)au_a) == ref_level((Au)au_b);
}

static enode castable(etype fr, etype to) { 
    bool fr_ptr = is_ptr(fr);
    if (((fr_ptr && !is_class(fr)) || is_prim(fr)) && is_bool(to))
        return (enode)true;
    
    /*
    if (is_prim(fr) && to->au == typeid(string)) {
        Au_t au = find_member(to->au->module, "Au", AU_MEMBER_TYPE, false);
        Au_t f = find_member(au, "cast_string", (i32)AU_MEMBER_CAST, false);
        return f ? (enode)f->user : null;
    }*/

    /// compatible by match, or with basic integral/real types
    if ((fr == to) ||
        ((is_realistic(fr) && is_realistic(to)) ||
         (is_integral (fr) && is_integral (to))))
        return (enode)true;
    
    /// primitives may be converted to Au-type Au
    if (is_prim(fr) && is_generic(to))
        return (enode)true;

    /// check cast methods on from
    Au_t ctx = fr->au;
    while (ctx) {
        for (int i = 0; i < ctx->members.count; i++) {
            Au_t mem = (Au_t)ctx->members.origin[i];
            if (mem->member_type == AU_MEMBER_CAST && is_same((Au)mem->rtype, (Au)to->au))
                return (enode)mem->user;
        }
        if (ctx->context == ctx) break;
        ctx = ctx->context;
    }
    return (enode)false;
}


static enode constructable(etype fr, etype to) {
    if (fr == to)
        return (enode)true;

    // if its a primitive, and we want to convert to string -> Au_cast_string
    aether a = fr->mod;

    Au_t ctx = to->au;
    while (ctx) {
        for (int ii = 0; ii < ctx->members.count; ii++) {
            Au_t mem = (Au_t)ctx->members.origin[ii];
            if (mem->member_type == AU_MEMBER_CONSTRUCT) {
                mem = mem;
            }
            etype fn = mem->member_type == AU_MEMBER_CONSTRUCT ? mem->user : null;
            if  (!fn) continue;
            verify(fn->au->args.count == 2, "unexpected argument count for constructor"); // target + with-type
            Au_t with_arg = (Au_t)fn->au->args.origin[1];
            if (with_arg->src == fr->au)
                return (enode)mem->user;
        }
        if (ctx == ctx->context) break;
        ctx = ctx->context;
    }
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
    if (mdl == base) return true;
    bool inherits = false;
    etype m = mdl;
    if (!is_class(mdl)) return false;
    while (m) {
        if (m == base)
            return true;
        
        if (!m->au->context) break;
        if (m->au->context->user == m) break;
        m = m->au->context->user;
    }
    return false;
}

static bool is_subclass(Au a0, Au b0) {
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

    if (ma->au->is_lambda && mb->au == typeid(callback))
        return (enode)true;
    if ((!ma->au->is_class && is_ptr(ma)) && ma->au->src->is_struct && mb->au == typeid(Au))
        return (enode)true;

    // allow Au to convert to primitive/struct
    if (ma->au == typeid(Au) && (mb->au->is_struct || mb->au->is_primitive || mb->au->is_pointer))
        return (enode)true;

    if (ma->au->is_schema && mb->au->is_pointer)     return (enode)true;
    if (mb->au->is_schema && ma->au->is_pointer)     return (enode)true;
    if (ma->au->is_schema && mb->au == typeid(Au_t)) return (enode)true;
    if (mb->au->is_schema && ma->au == typeid(Au_t)) return (enode)true;

    // allow pointers of any sort, to boolean
    if  (ma->au->is_pointer && mb->au == typeid(bool)) return (enode)true;

    // todo: ref depth must be the same too
    if (resolve(ma) == resolve(mb) && ref_level((Au)ma) == ref_level((Au)mb)) return (enode)true;

    // more robust conversion is, they are both pointer and not user-created
    if (ma->au->is_system || mb->au == typeid(handle))
        return (enode)true;

    if (ma == mb)
        return (enode)true;

    if (is_prim(ma) && is_prim(mb))
        return (enode)true;

    if (is_prim(ma) && inherits(mb->au, typeid(string))) {
        ma = typeid(Au)->user; // reduce all primitives to Au; we just need to be sure to get addresses of our data
    }

    if (is_rec(ma) || is_rec(mb)) {
        Au_t au_type = au_lookup("Au_t");
        if (is_ptr(ma) && ma->au->src == au_type && mb->au == au_type)
            return (enode)true;
        if (is_subclass((Au)ma, (Au)mb) || is_subclass((Au)mb, (Au)ma))
            return (enode)true;
        enode mcast = castable(ma, mb);
        return mcast ? mcast : constructable(ma, mb);
    } else {
        // the following check should be made redundant by the code below it
        etype sym = typeid(symbol)->user;
        etype ri8 = typeid(ref_i8)->user;

        // quick test
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
    etype shape_t = typeid(shape)->user;
    i32 count = len(indices);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, shape_t);

    // Allocate stack space for the i64 array (data field)
    LLVMTypeRef arr_t = LLVMArrayType(lltype((Au)typeid(i64)), count);
    LLVMValueRef data_alloc = LLVMBuildAlloca(a->builder, arr_t, "shape_data");

    // Store each index expression into the data array
    for (i32 i = 0; i < count; i++) {
        enode idx = (enode)indices->origin[i];
        // Convert index to i64 if needed
        enode idx_i64 = e_create(a, typeid(i64)->user, (Au)idx);
        
        LLVMValueRef gep_idxs[] = {
            LLVMConstInt(LLVMInt32Type(), 0, 0),
            LLVMConstInt(LLVMInt32Type(), i, 0)
        };
        LLVMValueRef elem_ptr = LLVMBuildGEP2(a->builder, arr_t, data_alloc, gep_idxs, 2, "");
        LLVMBuildStore(a->builder, idx_i64->value, elem_ptr);
    }

    // Get pointer to first element (ref_i64 / i64*)
    LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 0);
    LLVMValueRef data_ptr_idxs[] = { zero, zero };
    LLVMValueRef data_ptr = LLVMBuildGEP2(a->builder, arr_t, data_alloc, data_ptr_idxs, 2, "data_ptr");

    enode count_node = e_operand(a, _i64(count), typeid(i64)->user);
    enode data_node = enode(mod, a, loaded, true, value, data_ptr, au, typeid(ref_i64)->user->au);

    // Lookup shape_from function: shape_from(i64 count, ref_i64 values)
    Au_t shape_from_fn = find_member(typeid(shape), "shape_from", AU_MEMBER_FUNC, false);
    verify(shape_from_fn, "shape_from function not found");

    // Call shape_from(count, data)
    return e_fn_call(a, (efunc)shape_from_fn->user, a(count_node, data_node));
}

enode etype_access(etype target, string name) {
    aether a = target->mod;
    Au_t rel = target->au->member_type == AU_MEMBER_VAR ? 
        resolve(target->au->src->user)->au : target->au;
    if (rel->is_pointer && rel->src && (rel->src->is_class || rel->src->is_struct))
        rel = rel->src;
    Au_t   m = find_member(rel, name->chars, 0, true);
    if (!m) {
        m = m;
    }
    verify(m, "failed to find member %o on type %o", name, rel);
    bool is_enode = instanceof(target, enode) != null;

    //verify(is_addressable(target), 
    //    "expected target pointer for member access");

    etype t = resolve(target); // resolves to first type
    verify(t->au->is_struct || t->au->is_class || t->au->is_primitive, 
        "expected resolution to struct or class");

    // for functions, we return directly with target passed along
    if (is_func((Au)m)) {
        enode n = enode_value((enode)target);

        // primitive method receiver must be addressable
        if (is_prim(n->au->src) && !is_addressable(n)) {
            etype res = resolve(n);
            LLVMValueRef temp = LLVMBuildAlloca(a->builder, lltype(res), "prim.recv.addr");
            LLVMBuildStore(a->builder, n->value, temp);
            n = value(pointer(a, (Au)n->au->src), temp);
        }
        
        // if primitive, we need to make a temp on stack (reserving Au header space), and obtain pointer to it
        return (enode)efunc(mod, a, au, m, loaded, true, target,
            (m->is_static || m->is_smethod) ? null : n);
    }

    // enum values should already be created
    if (m->member_type == AU_MEMBER_ENUMV) {
        // make sure the enum is implemented
        etype_implement((etype)m->context->user);
        verify(instanceof(m->user, enode), "expected enode for enum value");
        return (enode)m->user;
    }
    
    enode n = instanceof(m->user, enode);
    if (n && n->value)
        return (enode)m->user; //((enode)m->user)->value; // this should apply to enums

    // signal we're doing something non-const
    a->is_const_op = false;
    if (a->no_build)
        return e_noop(a, m->user);

    Au_t ptr = pointer(a, (Au)m)->au;

    static int seq = 0;
    seq++;

    enode func = context_func(a);

    if (seq == 83) {
        seq = seq;
    }
    string id = f(string, "enode_access_%i", seq);
    return enode(
        mod,    a,
        au,     m,
        loaded, false, // loaded false == ptr
        debug_id, id,
        value,  LLVMBuildStructGEP2(
            a->builder, t->au->lltype, ((enode)target)->value,
            m->index, id->chars));
}

enode resolve_typeid(Au mdl) {
    Au_t au = au_arg(mdl);
    if (au->is_pointer && au->src->is_class) {
        return (au->src->user ? au->src->user->type_id : null);
    }
    return au->user ? au->user->type_id : null;
}

enode aether_e_typeid(aether a, etype mdl) {
    if (!mdl)
        return e_null(a, typeid(Au_t)->user);
    
    verify(mdl->au->module->is_au, "typeid not available on external types: %o", mdl);

    // we go from enode sometimes, which is NOT 
    // the base etype (its not brilliant to require etype be unique here but we circle back)

    Au info = head(mdl);
    a->is_const_op = false;

    enode n = resolve_typeid((Au)mdl);
    if (!n) {
        resolve_typeid((Au)mdl);
        n = n;
    }
    verify(n, "schema instance not found for %o", mdl);
    return n;
}

bool is_au_t(Au a) {
    Au_t au = au_arg(a);
    if (au && au->is_pointer) {
        au = au->src;
    }
    return au == typeid(Au_t);
}




/*
enode parse_object(aether a, etype mdl) {

    bool is_fields = peek_fields(a) || inherits(mdl->au, typeid(map));

    print_tokens(a, "parse-map");

    // we need e_create to handle this as well; since its given a map of fields and a is_ref struct it knows to make an alloc
    bool    is_mdl_map  = mdl->au == typeid(map);
    bool    is_mdl_collective = inherits(mdl->au, typeid(collective));
    bool    was_ptr     = false;

    validate(!is_mdl_map || is_fields, "expected fields for map");

    if (is_ptr(mdl) && is_struct(mdl->au->src)) {
        was_ptr = true;
        mdl     = resolve(mdl);
    }
    
    etype   key         = is_mdl_map ? (etype)array_get((array)&mdl->au->meta, 0) : null;
    etype   val         = is_mdl_map ? (etype)array_get((array)&mdl->au->meta, 1) : null;

    if (!key) key       = elookup("string");
    if (!val) val       = elookup("Au");

    etype f_alloc       = find_member(typeid(Au), "alloc_new", AU_MEMBER_FUNC, false)->user;
    etype f_initialize  = find_member(typeid(Au), "initialize", AU_MEMBER_FUNC, false)->user;

    enode metas_node = e_meta_ids(a, mdl->meta);
    enode rmap = e_fn_call(a, (enode)f_alloc, a( e_typeid(a, mdl), _i32(1), metas_node ));
    rmap->au = mdl->au; // we need a general cast method that does not call function

    static Au_t sprop; if (!sprop) sprop = find_member(typeid(Au),  "set_property", AU_MEMBER_FUNC, true);
    static Au_t msetv; if (!msetv) msetv = find_member(typeid(map), "set",          AU_MEMBER_FUNC, true);
    static Au_t apush; if (!apush) apush = find_member(typeid(collective), "push",  AU_MEMBER_FUNC, true);

    int iter = 0;
    
    shape s = is_mdl_collective ? instanceof(array_get(mdl->meta, 1), shape) : null;
    int   shape_stride = (s && s->count > 1) ? s->data[s->count - 1] : 0;

    while (silver_peek(a)) {
        if (next_is(a, "]"))
            break;
        
        Au    k = null;
        token t = peek(a);
        bool is_literal = instanceof(t->literal, string) != null;
        bool is_enode_key = false;

        print_tokens(a, "during parse-map");

        if (is_fields && silver_read_if(a, "{")) {
            k = (Au)parse_expression(a, key); 
            validate(silver_read_if(a, "}"), "expected }");
            is_enode_key = true;
        } else if (!is_fields && is_mdl_collective) {
            etype e = (etype)array_get(mdl->meta, 0);
            k = (Au)parse_expression(a, e);
        } else if (!is_mdl_map) {
            string name = (string)read_alpha(a);
            validate(name, "expected member identifier");
            k = (Au)const_string(chars, name->chars);
        } else {
            string name = (string)read_literal(a, typeid(string));
            validate(is_literal, "expected literal string");
            k = (Au)const_string(chars, name->chars);
        }
        
        if (is_fields) {
            validate(silver_read_if(a, ":"), "expected : after key %o", t);
            enode value = parse_expression(a, null);

            if (!is_mdl_map) {
                if (is_enode_key)
                    e_fn_call(a, (enode)sprop->user, a(rmap, k, value));
                else {
                    Au_t m = find_member(mdl->au, t->chars, AU_MEMBER_VAR, true);
                    validate(m, "member %o not found on model %o", t, mdl);
                    enode prop = access(rmap, (string)k);
                    e_assign(a, prop, (Au)value, OPType__assign);
                }
            } else {
                if (is_enode_key) {
                    //enode rt_key = e_create(a, elookup("string"), k);
                    e_fn_call(a, (enode)msetv->user, a(rmap, k, value));
                } else
                    e_fn_call(a, (enode)msetv->user, a(rmap, k, value));
            }
        } else {
            if (is_mdl_collective) {
                e_fn_call(a, (enode)apush->user, a(rmap, k));
            } else if (is_struct(mdl)) {
                etype t = canonical(mdl);
                int id = 0;
                bool set = false;
                members(t->au, m) {
                    if (m->member_type == AU_MEMBER_VAR) {
                        if (id == iter) {
                            enode prop = access(rmap, (string)k);
                            e_assign(a, prop, (Au)k, OPType__assign);
                            set = false;
                        }
                        id++;
                    }
                }
                validate(set, "too many fields specified for type %o", mdl);
                
            } else {
                fault("type %o not compatible with array initialization", mdl);
            }
        }
        token comma = read_if(a, ",");
        
        if (shape_stride != 0) {
            verify( comma && (iter % shape_stride == 0), "expected comma");
            verify(!comma || (iter % shape_stride != 0), "unexpected comma");
        }

        iter++;
    }
    e_fn_call(a, (enode)f_initialize, a(rmap));
    return rmap;
}
*/

bool is_map(etype t) {
    if (instanceof(t, etype) && t->au == typeid(map) || t->au->context == typeid(map)) {
        return true;
    }
    return false;
}

// find use for this one; i dont believe im literally caling e_create with a map attached
// e_create had all of the logic inside, and i just thought it too much
enode e_create_from_map(aether a, etype t, map m) {
    bool  is_m         = is_map(t);
    efunc f_alloc      = (efunc)find_member(typeid(Au),    "alloc_new",    AU_MEMBER_FUNC, false)->user;
    efunc f_initialize = (efunc)find_member(typeid(Au),    "initialize",   AU_MEMBER_FUNC, false)->user;
    efunc f_set_prop   = (efunc)find_member(typeid(Au),    "set_property", AU_MEMBER_FUNC, false)->user;
    efunc f_mset       = (efunc)find_member(typeid(map),   "set",          AU_MEMBER_FUNC, false)->user;
    efunc f_push_vdata = (efunc)find_member(typeid(array), "push_vdata",   AU_MEMBER_FUNC, true )->user;
    enode metas_node   = e_meta_ids(a, t->meta);

    static int seq = 0;
    seq++;
    if (seq == 1) {
        seq = seq;
    }
    if (seq == 2) {
        seq = seq;
    }
    enode cur = context_func(a);

    enode res          = e_fn_call(a, f_alloc, a( e_typeid(a, t), _i32(1), metas_node ));
    res->au   = t->au;
    res->meta = hold(t->meta);

    if (is_m) e_fn_call(a, f_initialize, a(res));

    pairs(m, i) {
        Au k = i->key;
        
        if (is_m) {
            Au_t ivalue = isa(i->value);
            e_fn_call(a, f_mset, a(res, k, i->value));
        } else {
            if (instanceof(k, enode)) {
                e_fn_call(a, f_set_prop, a(res, k, i->value));
            } else {
                enode prop = access(res, (string)k);
                e_assign(a, prop, i->value, OPType__assign);
            }
        }
    }
    if (!is_m) e_fn_call(a, f_initialize, a(res));
    return res;
}

enode e_create_from_array(aether a, etype t, array ar) {
    efunc f_alloc      = (efunc)find_member(typeid(Au), "alloc_new",  AU_MEMBER_FUNC, false)->user;
    efunc f_initialize = (efunc)find_member(typeid(Au), "initialize", AU_MEMBER_FUNC, false)->user;
    efunc f_push       = (efunc)find_member(typeid(collective), "push", AU_MEMBER_FUNC, true)->user;
    efunc f_push_vdata = (efunc)find_member(typeid(array), "push_vdata", AU_MEMBER_FUNC, true)->user;

    enode metas_node = e_meta_ids(a, t->meta);
    enode res = e_fn_call(a, f_alloc, a( e_typeid(a, t), _i32(1), metas_node ));
    res->au = t->au; // we need a general cast method that does not call function
    
    bool  all_const = t->au == typeid(array);
    int   ln = len(ar);
    enode array_len = e_operand(a, _i64(ln), typeid(i64)->user);
    
    enode const_vector = null;

    for (int i = 0; i < ln; i++) {
        enode node = (enode)instanceof(ar->origin[i], enode);
        if (node && node->literal)
            continue;
        all_const = false;
        break;
    }

    // read meta args
    etype element_type = instanceof(array_get((array)t->meta, 0), etype);
    shape dims         = instanceof(array_get((array)t->meta, 1), shape);
    if (!element_type) element_type = typeid(Au)->user;
    int max_items = dims ? shape_total(dims) : -1;

    verify(max_items == -1 || ln <= max_items, "too many elements given to array");
    
    if (all_const) {
        etype ptr = pointer(a, (Au)element_type);
        etype elem_t = resolve(t);   // base element type
        LLVMValueRef *elems   = malloc(sizeof(LLVMValueRef) * ln);
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

    enode prop_alloc     = etype_access((etype)res, string("alloc"));
    enode prop_unmanaged = etype_access((etype)res, string("unmanaged"));

    e_assign(a, prop_alloc, (Au)array_len, OPType__assign);
    if (const_vector) {
        enode tru = e_operand(a, _bool(ln), typeid(bool)->user);
        e_assign(a, prop_unmanaged, (Au)tru, OPType__assign);
    }     
    e_fn_call(a, f_initialize, a(res));

    if (const_vector) {
        e_fn_call(a, f_push_vdata, a(res, const_vector, array_len, e_typeid(a, element_type)));
    } else {
        for (int i = 0; i < ln; i++) {
            Au element  = ar->origin[i];
            e_fn_call(a, f_push, a(res, element));
        }
    }
    return res;
}

none copy_lambda_info(enode mem, enode lambda_fn) {
    mem->context_node = hold(lambda_fn->context_node); // would be nice to do this earlier
    mem->published_type = hold(lambda_fn->published_type);
    mem->value        = lambda_fn->value;
}

enode e_convert_or_cast(aether a, etype output, enode input) {
    LLVMTypeRef typ = LLVMTypeOf(input->value);
    LLVMTypeKind k = LLVMGetTypeKind(typ);

    // check if these are either Au_t class typeids, or actual compatible instances
    if (k == LLVMPointerTypeKind) { // this should all be in convertible
        a->is_const_op = false;
        if (a->no_build) return e_noop(a, output);
        bool loaded = !(!(is_ptr(output) || is_func_ptr(output)) && (is_struct(output) || is_prim(output)));
        enode res = value(output,
            LLVMBuildBitCast(a->builder, input->value, loaded ? lltype(output) : lltype(pointer(a, (Au)output)), "class_ref_cast"));
        res->loaded = loaded; // loaded/unloaded is a brilliant abstract of laziness; allows casts to take place with slight alterations to the enode
        return res;
    }

    // fallback to primitive conversion rules
    return aether_e_primitive_convert(a, input, output);
}

enode aether_e_vector(aether a, etype t, enode sz) {
    efunc f_alloc    = (efunc)find_member(typeid(Au), "alloc_new", AU_MEMBER_FUNC, false)->user;
    enode metas_node = e_meta_ids(a, t->meta);
    return e_fn_call(a, f_alloc, a( e_typeid(a, t), sz, metas_node ));
}

enode aether_e_alloc(aether a, etype mdl) {
    enode metas_node = e_meta_ids(a, mdl->meta);
    efunc f_alloc = (efunc)find_member(typeid(Au), "alloc_new", AU_MEMBER_FUNC, false)->user;
    enode res = e_fn_call(a, f_alloc, a( e_typeid(a, mdl), _i32(0), metas_node ));
    res->au = mdl->au->is_class ? mdl->au : pointer(a, (Au)mdl)->au;
    return res;
}

/// create is both stack and heap allocation (based on etype->is_ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_e_create(aether a, etype mdl, Au args) {
    if (!mdl) {
        verify(instanceof(args, enode), "aether_e_create");
        return (enode)args;
    }

    string  str  = (string)instanceof(args, string);
    map     imap = (map)instanceof(args, map);
    array   ar   = (array)instanceof(args, array);
    static_array static_a = (static_array)instanceof(args, static_array);
    enode   n    = null;
    efunc   ctr  = null;

    // we never 'create' anything of type identity with e_create; thats too much scope to service
    // for type -> Au_t conversion and Au_t -> type, we simply cast.
    // we probably want to fault in all other cases
    if (is_au_t((Au)mdl) && is_au_t(args)) {
        enode n = instanceof(args, enode);
        return value(mdl,
            LLVMBuildBitCast(a->builder, n->value, lltype(mdl), "is_au_t_cast"));
    }

    if (!args) {
        if (is_ptr(mdl))
            return e_null(a, mdl);
    }

    if (is_lambda((Au)mdl)) {
        verify(isa(args) == typeid(array), "expected args for lambda");

        efunc n_mdl = instanceof(mdl, efunc);
        verify(n_mdl && n_mdl->target, "expected enode with target for lambda function");
        
        efunc f_create = (efunc)find_member(typeid(lambda),
            "lambda_instance", AU_MEMBER_FUNC, false)->user;
        
        // Get the context struct type from the lambda definition
        etype ctx_type = n_mdl->context_node->au->src->user;  // the struct type (not pointer)
        
        // allocate context struct
        enode ctx_alloc = e_alloc(a, ctx_type);
        
        // fill in context values from the parsed expressions (ctx_vals from parse_create_lambda)
        int ctx_index = 0;
        verify (len((array)args) == n_mdl->au->members.count,
            "lambda initialization member-count (%i) != user-provided args (%i)",
            n_mdl->au->members.count, len((array)args));
        
        // assign context members
        members(n_mdl->au, mem) {
            if (mem->member_type != AU_MEMBER_VAR) continue;
            enode ctx_val = (enode)array_get((array)args, ctx_index);
            enode field_ref = access(ctx_alloc, string(mem->ident));
            e_assign(a, field_ref, (Au)ctx_val, OPType__assign);
            ctx_index++;
        }
        
        enode targ = n_mdl->target;
        array args = a(n_mdl->published_type, n_mdl, n_mdl->target, ctx_alloc);
        enode res = e_fn_call(a, f_create, args);
        res->meta = array(alloc, 32);

        // push return type first
        push(res->meta, (Au)n_mdl->au->src->user);

        // push args
        arg_types(n_mdl->au, type)
            push(res->meta, (Au)type->user);
        
        return res;
    }

    static int seq = 0;
    seq += 1;
    if (seq == 12) {
        seq = seq;
    }

    if (seq == 37) {
        seq = seq;
    }

    // construct / cast methods
    Au_t args_type = isa(args);
    efunc ftest = (efunc)instanceof(args, efunc);
    enode input = (enode)instanceof(args, enode);
    if (input && !input->loaded) {
        Au info = head(input);
        input = enode_value(input);
    }

    if (!input && instanceof(args, const_string)) {
        input = e_operand(a, args, mdl);
        args  = (Au)input;
    }

    if (!input && instanceof(args, string)) {
        input = e_operand(a, args, typeid(string)->user);
        args  = (Au)input;
    }

    if (input) {
        verify(!imap, "unexpected data");
        
        // if both are internally created and these are refs, we can allow conversion
        
        static int seq = 0;
        seq++;
        if (seq == 188) {
            seq = seq;
        }

        // Handle struct/primitive to Au conversion (boxing)
        if (mdl->au == typeid(Au) && (is_struct(input) || is_prim(input))) {
            a->is_const_op = false;
            if (a->no_build) return e_noop(a, mdl);
            
            etype input_type = canonical(input);
            efunc f_alloc    = (efunc)find_member(typeid(Au), "alloc_new", AU_MEMBER_FUNC, false)->user;
            enode metas_node = e_meta_ids(a, input_type->meta);
            enode boxed      = e_fn_call(a, f_alloc, a(
                e_typeid(a, input_type), 
                _i32(1), 
                metas_node
            ));
            boxed->au = input_type->au;

            LLVMBuildStore(a->builder, input->value, boxed->value);
                    
            // Cast to Au type for return
            return value(mdl, 
                LLVMBuildBitCast(a->builder, boxed->value, lltype(mdl), "box_to_au"));
        }

        Au_t t_isa = isa(mdl);
        enode fmem = convertible((etype)input, mdl);

        verify(fmem, "no suitable conversion found for %o -> %o (%i)",
            input, mdl, seq);
        
        if (fmem == (void*)true) {
            // perform primitive conversion, or a general bitcast
            return e_convert_or_cast(a, mdl, input);
        }
        
        // primitive-based conversion goes here
        efunc fn = (efunc)instanceof(fmem, efunc);
        if (fn && fn->au->member_type == AU_MEMBER_CONSTRUCT) {
            // ctr: call before init
            // this also means the mdl is not a primitive
            //verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = (efunc)fmem;
        } else if (fn && fn->au->member_type == AU_MEMBER_CAST) {
            // we may call cast straight away, no need for init (which the cast does)
            return e_fn_call(a, fn, a(input));
        } else
            fault("unknown error");
        
    }

    // handle primitives after cast checks -- the remaining objects are object-based
    // note that enumerable values are primitives
    if (is_prim(mdl))
        return e_operand(a, args, mdl);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);

    etype        cmdl         = etype_traits((Au)mdl, AU_TRAIT_CLASS);
    etype        smdl         = etype_traits((Au)mdl, AU_TRAIT_STRUCT);
    efunc        f_alloc      = (efunc)find_member(typeid(Au), "alloc_new", AU_MEMBER_FUNC, false)->user;
    efunc        f_initialize = (efunc)find_member(typeid(Au), "initialize", AU_MEMBER_FUNC, false)->user;
    enode        res;

    if (is_ptr(mdl) && is_struct(mdl->au->src) && static_a) {
        static int ident = 0;
        char name[32];
        sprintf(name, "static_arr_%i", ident++);

        i64          ln     = len(static_a);
        etype        emdl   = (etype)mdl->au->src->user;
        LLVMTypeRef  arrTy  = LLVMArrayType(lltype(emdl), ln);
        LLVMValueRef G      = LLVMAddGlobal(a->module_ref, arrTy, name);
        LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));
        LLVMSetLinkage(G, LLVMInternalLinkage);

        for (i32 i = 0; i < ln; i++) {
            enode n = e_operand(a, static_a->origin[i], null);
            verify (LLVMIsConstant(n->value), "static_array must contain constant statements");
            verify (n->au == mdl->au->src, "type mismatch");
            elems[i] = n->value;
        }
        LLVMValueRef init = LLVMConstArray(lltype(emdl), elems, ln);
        LLVMSetInitializer(G, init);
        LLVMSetGlobalConstant(G, 1);
        free(elems);
        res = enode(mod, a, value, G, loaded, false, au, mdl->au);

    } else if (!ctr && cmdl) {
        if (instanceof(args, array)) {
            return e_create_from_array(a, mdl, (array)args);
        } else if (instanceof(args, map)) {
            return e_create_from_map  (a, mdl, (map)args);
        } else {
            fault("this needs work");
            enode metas_node = e_meta_ids(a, mdl->meta);
            res = e_fn_call(a, f_alloc, a( e_typeid(a, mdl), _i32(0), metas_node ));
            res->au = mdl->au; // we need a general cast method that does not call function
            res = e_fn_call(a, f_initialize, a(res)); // required logic need not emit ops to set the bits when we can check at design time
        }
    } else if (ctr) {
        verify(is_rec(mdl), "expected record");
        enode metas_node = e_meta_ids(a, mdl->meta);
        
        enode alloc = e_fn_call(a, f_alloc, a( e_typeid(a, mdl), _i32(0), metas_node ));
        alloc->au = mdl->au; // we need a general cast method that does not call function
        e_fn_call(a, ctr, a(alloc, input));
        res = e_fn_call(a, f_initialize, a(alloc));
    } else {
        array is_arr        = (array)instanceof(args, array);
        bool  is_ref_struct = is_ptr(mdl) && is_struct(resolve(mdl));

        //verify(!is_arr, "no translation for array to %o", mdl);
        etype rmdl = resolve(mdl);

        if (is_struct(mdl) || is_ref_struct) {
            //verify(!is_arr, "unexpected array argument");
            int field_count = LLVMCountStructElementTypes(rmdl->au->lltype);
            LLVMValueRef *fields = calloc(field_count, sizeof(LLVMValueRef));
            bool  all_const = a->no_const ? false : true;

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
                                field_type = smem->user;
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
                // check if constants are in map, and order fields
                LLVMValueRef *fields = calloc(field_count, sizeof(LLVMValueRef));
                array field_indices = array(field_count);
                array field_names   = array(field_count);
                array field_values  = array(field_count);

                pairs(imap, i) {
                    string  k = (string)i->key;
                    Au_t   t = isa(i->value);
                    Au_t   m = find_member(rmdl->au, k->chars, AU_MEMBER_VAR, true);
                    i32 index = m->index;

                    enode value = e_operand(a, i->value, m->src->user);
                    if (all_const && !LLVMIsConstant(value->value))
                        all_const = false;

                    //verify(LLVMIsConstant(value->value), "non-constant field in const struct");
                    
                    push(field_indices, _i32(index));
                    push(field_names,   (Au)string(m->ident));
                    push(field_values,  (Au)value);
                }

                // iterate through fields, associating the indices with values and struct member type
                // for unspecified values, we create an explicit null
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
                        if (smem->member_type == AU_MEMBER_VAR && smem->is_iprop) {
                            if (current_index++ == i) {
                                field_type = smem->user;
                                break;
                            }
                        }
                    } // string and cstr strike again -- it should be calling the constructor on string for this, which has been enumerated by Au-type already
                    verify(field_type, "field type lookup failed for %o (index = %i)", mdl, i);

                    LLVMTypeRef tr = LLVMStructGetTypeAtIndex(rmdl->au->lltype, i);
                    LLVMTypeRef expect_ty = lltype(field_type);
                    verify(expect_ty == tr, "field type mismatch");
                    if (!value) value = e_null(a, field_type);
                    fields[i] = value->value;
                }
            }

            /*
            pairs(imap, i) {
                string  k = i->key;
                Au_t t = isa(i->value);
                //print("%o -> %o", k, i->value ? (cstr)isa(i->value)->ident : "null");
                emember m = find_member(mdl, i->key, null, true);
                verify(m, "prop %o not found in %o", mdl, i->key);
                verify(isa(m) != typeid(function), "%o (function) cannot be initialized", i->key);
                enode i_value = e_operand(a, i->value, m->mdl)  ; // for handle with a value of 0, it must create a e_null
                emember i_prop  = resolve((emember)res, i->key) ;
                e_assign(a, i_prop, i_value, OPType__assign);
            }*/


            if (all_const && !is_ref_struct) {
                print("all are const, writing %i fields for %o", field_count, mdl);
                LLVMValueRef s_const = LLVMConstNamedStruct(rmdl->au->lltype, fields, field_count);
                res = enode(mod, a, loaded, true, value, s_const, au, mdl->au);
            } else {
                print("non-const, writing build instructions, %i fields for %o", field_count, mdl);
                res = enode(mod, a, loaded, false, value, LLVMBuildAlloca(a->builder, lltype(mdl), "alloca-mdl"), au, mdl->au);
                res = enode_value(res);
                res = e_zero(a, res);
                for (int i = 0; i < field_count; i++) {
                    if (!LLVMIsNull(fields[i])) {
                        LLVMValueRef gep = LLVMBuildStructGEP2(a->builder, lltype(mdl), res->value, i, "");
                        LLVMBuildStore(a->builder, fields[i], gep);
                    }
                }
            }

            free(fields);
        } else 
            res = e_operand(a, args, mdl);
    }
    return res;
}


enode aether_e_const_array(aether a, etype mdl, array arg) {
    etype atype = typeid(Au_t)->user;
    etype vector_type = etype_ptr(a, mdl->au);

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
    LLVMValueRef zero   = LLVMConstInt(LLVMInt8Type(), 0, 0);          // value for memset (0)
    LLVMValueRef size   = LLVMConstInt(LLVMInt64Type(), mdl->au->abi_size / 8, 0); // size of alloc
    LLVMValueRef memset = LLVMBuildMemSet(a->builder, v, zero, size, 0);
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
    LLVMBasicBlockRef then_block    = LLVMAppendBasicBlock(pb, "ternary_then");
    LLVMBasicBlockRef else_block    = LLVMAppendBasicBlock(pb, "ternary_else");
    LLVMBasicBlockRef merge_block   = LLVMAppendBasicBlock(pb, "ternary_merge");

    // Step 2: Build the conditional branch based on the condition
    LLVMValueRef condition_value = cond_expr->value;
    LLVMBuildCondBr(mod->builder, condition_value, then_block, else_block);

    // Step 3: Handle the "then" (true) branch
    LLVMPositionBuilderAtEnd(mod->builder, then_block);
    LLVMValueRef true_value = true_expr->value;
    LLVMBuildBr(mod->builder, merge_block);  // Jump to merge block after the "then" block

    // Step 4: Handle the "else" (false) branch
    LLVMPositionBuilderAtEnd(mod->builder, else_block);
    enode default_expr = !false_expr ? e_create(a, (etype)true_expr, null) : null;
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
        LLVMBasicBlockRef block = LLVMGetInsertBlock(a->builder);
        LLVMPositionBuilderAtEnd(a->builder, block);
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

    LLVMBuilderRef    B     = a->builder;
    LLVMTypeRef       Ty    = LLVMTypeOf(switch_val->value);
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(B);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);

    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlock(fn, "switch.end"));
    push_scope(a, (Au)switch_cat);

    LLVMBasicBlockRef default_block =
        def_block
            ? LLVMAppendBasicBlock(fn, "default")
            : switch_cat->block;

    // create switch instruction
    LLVMValueRef SW =
        LLVMBuildSwitch(B, switch_val->value, default_block, cases->count);

    // allocate blocks for each case BEFORE emitting bodies
    map case_blocks = map(hsize, 16);

    Au_t common_type = null;
    pairs(cases, i) {
        LLVMBasicBlockRef case_block =
            LLVMAppendBasicBlock(fn, "case");

        set(case_blocks, i->key, (Au)case_block);

        // evaluate key to literal
        enode key_node = (enode)invoke(expr_builder, i->key);
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
    LLVMValueRef entry = LLVMGetBasicBlockParent(LLVMGetInsertBlock(a->builder));
    //LLVMBasicBlockRef entry = LLVMGetInsertBlock(a->builder);

    // invoke expression for switch, and push switch cat
    enode   switch_val = e_expr; // invoke(expr_builder, expr);
    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlock(entry, "switch.end"));
    push_scope(a, (Au)switch_cat);

    // allocate cats for each case, do NOT build the body yet
    // wrap in cat, store the catcher, not an enode
    map case_blocks = map(hsize, 16);
    pairs(cases, i)
        set(case_blocks, i->key, (Au)catcher(mod, a, team, case_blocks, block, LLVMAppendBasicBlock(entry, "case")));

    // default block, and obtain insertion block for first case
    catcher def_cat = def_block ? catcher(mod, a, block, LLVMAppendBasicBlock(entry, "default")) : null;
    LLVMBasicBlockRef cur = LLVMGetInsertBlock(a->builder);
    int total = cases->count;
    int idx = 0;


    pairs(case_blocks, i) {
        array   key_expr = (array)  i->key;
        catcher case_cat = (catcher)i->value;

        // position at current end & compare
        LLVMPositionBuilderAtEnd(a->builder, cur);
        // enode aether_e_cmp(aether a, enode L, enode R)
        enode eq = e_cmp(a, switch_val, (enode)key_expr);

        // next block in chain
        LLVMBasicBlockRef next =
            (idx + 1 < total)
                ? LLVMAppendBasicBlock(entry, "case.next")
                : (def_cat ? def_cat->block : switch_cat->block);

        LLVMBuildCondBr(a->builder, eq->value, case_cat->block, next);
        cur = next;
        idx++;
    }

    // ---- CASE bodies ----
    int i = 0;
    pairs(case_blocks, p) {
        catcher case_cat = (catcher)p->value;

        LLVMPositionBuilderAtEnd(a->builder, case_cat->block);
        array body_tokens = (array)value_by_index(cases, i++);
        invoke(body_builder, (Au)body_tokens);

        // we should have a last node, but see return statement returns its own thing, not a return
        // if the case didn’t terminate (break/return), jump to merge
        LLVMBuildBr(a->builder, switch_cat->block);
    }

    // ---- DEFAULT body ----
    if (def_cat) {
        LLVMPositionBuilderAtEnd(a->builder, def_cat->block);
        invoke(body_builder, (Au)def_block);
        LLVMBuildBr(a->builder, switch_cat->block);
    }

    // ---- MERGE ----
    LLVMPositionBuilderAtEnd(a->builder, switch_cat->block);
    pop_scope(a);
    return e_noop(a, null);
}

enode aether_e_for(aether a,
                   array init_exprs,
                   array cond_expr,
                   array body_exprs,
                   array step_exprs,
                   subprocedure init_builder,
                   subprocedure cond_builder,
                   subprocedure body_builder,
                   subprocedure step_builder)
{
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, null);
    
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(a->builder);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(entry);
    
    LLVMBasicBlockRef cond  = LLVMAppendBasicBlock(fn, "for.cond");
    LLVMBasicBlockRef body  = LLVMAppendBasicBlock(fn, "for.body");
    LLVMBasicBlockRef step  = LLVMAppendBasicBlock(fn, "for.step");
    LLVMBasicBlockRef merge = LLVMAppendBasicBlock(fn, "for.end");
    
    catcher cat = catcher(mod, a, block, merge);
    push_scope(a, (Au)cat);
    
    // ---- init ----
    if (init_exprs)
        invoke(init_builder, (Au)init_exprs);
    LLVMBuildBr(a->builder, cond);
    
    // ---- cond ----
    LLVMPositionBuilderAtEnd(a->builder, cond);
    enode cond_res = (enode)invoke(cond_builder, (Au)cond_expr);
    LLVMValueRef cond_val = e_create(a, typeid(bool)->user, (Au)cond_res)->value;
    LLVMBuildCondBr(a->builder, cond_val, body, merge);
    
    // ---- body ----
    LLVMPositionBuilderAtEnd(a->builder, body);
    invoke(body_builder, (Au)body_exprs);
    LLVMBuildBr(a->builder, step);
    
    // ---- step ----
    LLVMPositionBuilderAtEnd(a->builder, step);
    if (step_exprs)
        invoke(step_builder, (Au)step_exprs);
    LLVMBuildBr(a->builder, cond);
    
    // ---- end ----
    LLVMPositionBuilderAtEnd(a->builder, merge);
    pop_scope(a);
    
    return e_noop(a, null);
}






// we need a bit more api so silver can do this with a bit less, this works fine in a generic sense for aether use-case
// we may want a property to make it design-time, though -- for const operations, basic unrolling facility etc
enode aether_e_loop(aether a,
                     array expr_cond,
                     array exprs_iterate,
                     subprocedure cond_builder,
                     subprocedure expr_builder,
                     bool loop_while)   // true = while, false = do-while
{
    LLVMBasicBlockRef entry   = LLVMGetInsertBlock(a->builder);
    LLVMValueRef fn = LLVMGetBasicBlockParent(entry);
    LLVMBasicBlockRef cond    = LLVMAppendBasicBlock(fn, "loop.cond");
    LLVMBasicBlockRef iterate = LLVMAppendBasicBlock(fn, "loop.body");

    catcher cat = catcher(mod, a, block, LLVMAppendBasicBlock(fn, "loop.end"));
    push_scope(a, (Au)cat);

    // ---- ENTRY → FIRST JUMP ----
    if (loop_while) {
        // while(cond) starts at the condition
        LLVMBuildBr(a->builder, cond);
    } else {
        // do {body} while(cond) starts at the body
        LLVMBuildBr(a->builder, iterate);
    }

    // ---- CONDITION BLOCK ----
    LLVMPositionBuilderAtEnd(a->builder, cond);

    enode cond_result = (enode)invoke(cond_builder, (Au)expr_cond);
    LLVMValueRef condition = e_create(a, typeid(bool)->user, (Au)cond_result)->value;

    LLVMBuildCondBr(a->builder, condition, iterate, cat->block);

    // ---- BODY BLOCK ----
    LLVMPositionBuilderAtEnd(a->builder, iterate);

    invoke(expr_builder, (Au)exprs_iterate);

    // After executing the loop body:
    // always jump back to the cond block
    LLVMBuildBr(a->builder, cond);

    // ---- MERGE BLOCK ----
    LLVMPositionBuilderAtEnd(a->builder, cat->block);
    pop_scope(a);
    return e_noop(a, null);
}

enode aether_e_noop(aether a, etype mdl) {
    return enode(mod, a, loaded, true, au, mdl ? mdl->au : au_lookup("none"));
}

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

    LLVMBasicBlockRef block = LLVMGetInsertBlock(a->builder);
    LLVMValueRef      fn    = LLVMGetBasicBlockParent(block);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlock(fn, "ifcont");

    for (int i = 0; i < ln_conds; i++) {
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(fn, "then");
        LLVMBasicBlockRef else_block;
        
        // Last condition with no else goes directly to merge
        bool is_last = (i == ln_conds - 1);
        if (is_last && !has_else)
            else_block = merge;
        else
            else_block = LLVMAppendBasicBlock(fn, "else");

        // Build condition
        Au    cond_obj  = conds->origin[i];
        enode cond_node = (enode)invoke(cond_builder, cond_obj);
        Au info = head(cond_node);
        LLVMValueRef condition = e_create(a, typeid(bool)->user, (Au)cond_node)->value;

        LLVMBuildCondBr(a->builder, condition, then_block, else_block);

        // Then block
        LLVMPositionBuilderAtEnd(a->builder, then_block);
        invoke(expr_builder, exprs->origin[i]);
        LLVMBuildBr(a->builder, merge);

        // Position for next iteration or else
        if (else_block != merge)
            LLVMPositionBuilderAtEnd(a->builder, else_block);
    }

    // Else block
    if (has_else) {
        invoke(expr_builder, exprs->origin[ln_conds]);
        LLVMBuildBr(a->builder, merge);
    }

    LLVMPositionBuilderAtEnd(a->builder, merge);

    return enode(
        mod,    a,
        loaded, false,
        au,     typeid(none),
        value,  null);
}


enode aether_e_addr_of(aether a, enode expr, etype mdl) {
    fault("verify e_create usage e_create(a, (etype)expr, (Au)mdl);");
    return e_create(a, (etype)expr, (Au)mdl);
}

enode aether_e_offset(aether a, enode n, Au offset) {
    enode  i  = e_operand(a, offset, null);
    verify(is_ptr(n), "offset requires pointer");
    LLVMValueRef ptr_offset = LLVMBuildGEP2(a->builder,
         lltype(n), n->value, &i->value, 1, "offset");
    return enode(mod, a, au, n->au, loaded, true, value, ptr_offset);
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
enode aether_e_cmp(aether a, comparison cmp, enode lhs, enode rhs) {
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

enode aether_e_load(aether a, enode mem, enode target) {
    a->is_const_op = false;
    etype  resolve   = resolve(mem);
    if (a->no_build) return e_noop(a, resolve);

    LLVMValueRef ptr = mem->value;
    verify(is_ptr(mem), "expected pointer to load from, given %o", mem);
    
    static int seq = 0;
    seq++;
    string id = f(string, "eload_%i", seq);
    LLVMValueRef loaded = LLVMBuildLoad2(
        a->builder, lltype(resolve), mem->value, id->chars);
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
        LLVMTypeRef byte_array = LLVMArrayType(LLVMInt8Type(), total_size);
        LLVMValueRef temp_alloca = LLVMBuildAlloca(a->builder, byte_array, "au_prim_temp");
        
        // Cast to Au*
        LLVMTypeRef au_struct_type = typeid(Au)->lltype;
        LLVMTypeRef au_ptr_type = LLVMPointerType(au_struct_type, 0);
        LLVMValueRef au_ptr = LLVMBuildBitCast(a->builder, temp_alloca, au_ptr_type, "au_ptr");
        
        // Zero the whole thing
        LLVMValueRef zero = LLVMConstInt(LLVMInt8Type(), 0, 0);
        LLVMValueRef size_val = LLVMConstInt(LLVMInt64Type(), total_size, 0);
        LLVMBuildMemSet(a->builder, temp_alloca, zero, size_val, 0);
        
        // Set type field (index 0) directly
        LLVMValueRef type_gep = LLVMBuildStructGEP2(a->builder, au_struct_type, au_ptr, 0, "type_ptr");
        enode type_val = e_typeid(a, F);
        LLVMBuildStore(a->builder, type_val->value, type_gep);
        
        // Store primitive value at end (offset = au_size)
        LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64Type(), au_size, 0) };
        LLVMValueRef byte_ptr = LLVMBuildBitCast(a->builder, temp_alloca, 
            LLVMPointerType(LLVMInt8Type(), 0), "");
        LLVMValueRef data_ptr = LLVMBuildGEP2(a->builder, LLVMInt8Type(), byte_ptr, indices, 1, "data_ptr");
        LLVMValueRef typed_ptr = LLVMBuildBitCast(a->builder, data_ptr, 
            LLVMPointerType(lltype(F), 0), "typed_ptr");
        LLVMBuildStore(a->builder, expr->value, typed_ptr);
        
        // Call Au_cast_string
        Au_t fn_cast = find_member(typeid(Au), "cast_string", AU_MEMBER_CAST, false);
        verify(fn_cast && fn_cast->user, "Au_cast_string not found");
        
        enode au_arg = enode(mod, a, value, au_ptr, loaded, true, au, typeid(Au));
        return e_fn_call(a, (efunc)fn_cast->user, a(au_arg));
    }
    
    LLVMValueRef V = expr->value;

    if (F == T) return expr;  // no cast needed

    // LLVM type kinds
    LLVMTypeKind F_kind = LLVMGetTypeKind(lltype(F));
    LLVMTypeKind T_kind = LLVMGetTypeKind(lltype(T));
    LLVMBuilderRef B = a->builder;

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
            V = LLVMBuildAlloca(a->builder, lltype(F), "prim_addr");
            LLVMBuildStore(a->builder, expr->value, V);
        }
        verify(make_temp, "unsupported cast");
    }

    enode res = value(T, V);
    res->literal = hold(expr->literal);
    return res;
}















void aether_push_tokens(aether a, tokens t, num cursor) {
    //struct silver_f* table = isa(a);
    tokens_data* state = Au_struct(tokens_data);
    state->tokens = (array)a->tokens;
    state->cursor = a->cursor;
    push(a->stack, (Au)state);
    tokens_data* state_saved = (tokens_data*)last_element(a->stack);
    a->tokens = hold(t);
    a->cursor = cursor;
    a->stack_test++;
}


void aether_pop_tokens(aether a, bool transfer) {
    int len = a->stack->count;
    assert (len, "expected stack");
    tokens_data* state = (tokens_data*)last_element(a->stack); // we should call this element or ele
    a->stack_test--;

    if(!transfer)
        a->cursor = state->cursor;
    else if (transfer && state->tokens != a->tokens) {
        /// transfer implies we up as many tokens
        /// as we traveled in what must be a subset
        a->cursor += state->cursor;
    }

    if (state->tokens != a->tokens) {
        drop(a->tokens);
        a->tokens = (tokens)state->tokens;
    }
    pop(a->stack);
}

void aether_push_current(aether a) {
    push_tokens(a, a->tokens, a->cursor);
}

static etype etype_deref(Au_t au) {
    return au->src ? au->src->user : null;
}

void src_init(aether a, Au_t m) {
    if (m && m->src)
        src_init(a, m->src);
    if (m && !m->user) {
        int mt = m->member_type;

        if (is_func((Au)m))
            m->user = (etype)efunc(mod, a, loaded, true,  au, m);
        else if (mt == AU_MEMBER_ENUMV || mt == AU_MEMBER_VAR || mt == AU_MEMBER_IS_ATTR)
            m->user = (etype)enode(mod, a, loaded, false, au, m);
        else {
            if (m->ident && strcmp(m->ident, "f32") == 0) {
                m = m;
            }
            m->user = etype(mod, a, au, m);
        }
        
        //if (is_func((Au)m) || mt == AU_MEMBER_ENUMV || mt == AU_MEMBER_VAR || mt == AU_MEMBER_IS_ATTR)
        //    m->user = (etype)enode(mod, a, loaded, false, au, m);
        //else
        //    m->user = etype(mod, a, au, m);
    }
}

array etype_class_list(etype t) {
    aether a = t->mod;
    array res = array(alloc, 32, assorted, true);
    Au_t src = t->au;
    while (src) {
        src_init(a, src);
        verify(src->user, "etype (user) not set for %s", src->ident);
        push(res, (Au)src->user);
        if (src->context == src)
            break;
        src = src->context;
    }
    return reverse(res);
}

void aether_build_initializer(aether a, etype m) { }

#define u(au) (au ? au->user : null)


static void build_module_initializer(aether a, efunc module_init_fn) {
    Au_t module_base = a->au;

    push_scope(a, (Au)module_init_fn);

    members(module_base, au)
        if (au->member_type == AU_MEMBER_VAR && au->user->body) {
            aether_f* et = (aether_f*)isa(a);
            build_initializer(a, au->user);
        }

    e_fn_return(a, null);
    pop_scope(a);
}

static void set_global_construct(aether a, efunc fn) {
    LLVMModuleRef mod = a->module_ref;

    LLVMValueRef ctor_func = fn->value;  // LLVM function
    LLVMTypeRef  int32_t   = LLVMInt32Type();
    LLVMTypeRef  i8ptr_t   = LLVMPointerType(LLVMInt8Type(), 0);

    // Priority: 65535 is lowest priority (runs last); you can choose another.
    LLVMValueRef priority = LLVMConstInt(int32_t, 65535, false);
    LLVMValueRef null_ptr = LLVMConstNull(i8ptr_t);

    // Construct struct { i32 priority, void()* fn, i8* data }
    LLVMTypeRef ctor_type = LLVMStructType(
        (LLVMTypeRef[]){
            int32_t,
            LLVMPointerType(LLVMTypeOf(ctor_func), 0),
            i8ptr_t
        },
        3, false
    );

    LLVMValueRef ctor_entry = LLVMConstStruct(
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

    etype main_spec  = typeid(app)->user;
    verify(main_spec && is_class(main_spec), "expected app class");

    etype main_class = null;
    members(module_base, mem) {
        if (is_class(mem)) {
            etype cls = mem->user;
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
    Au_t au_f_main = def_member(a->au, "main", au_lookup("i32"), AU_MEMBER_FUNC, 0);
    Au_t argc = def_arg(au_f_main,  "argc", au_lookup("i32"));
    Au_t argv = def_arg(au_f_main,  "argv", au_lookup("cstrs"));

    au_f_main->is_export = true;
    efunc main_fn = efunc(mod, a, au, au_f_main,
        loaded, true, used, true, has_code, true);
    main_fn->au->user = (etype)main_fn;
    etype_implement((etype)main_fn);

    push_scope(a, (Au)main_fn);
    e_fn_call(a, module_init_fn, null);
    efunc Au_engage = (efunc)find_member(au_lookup("Au"), "engage", AU_MEMBER_FUNC, false)->user; // this is only for setting up logging now, and we should likely emit it after the global construction, and before the user initializers
    verify(argv->user != null, "baffled");
    e_fn_call(a, Au_engage, a(argv->user));
    enode m = e_create(a, main_class, (Au)argv->user); // a loop would be nice, since none of our members will be ref+1'd yet
    Au_t fn_run = find_member(main_spec->au, "run", AU_MEMBER_FUNC, false);
    enode r = e_fn_call(a, (efunc)fn_run->user, a(m));
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
    verify(res, "argument not resolving type: %s", au);
    return res;
}

etype etype_canonical(etype t) {
    Au_t au = t->au;
    if (au->member_type == AU_MEMBER_VAR)
        au = au->src;
    if (au->is_typeid && au->src)
        au = au->src;
    while (au && au->src) {
        if (au->user && lltype(au->user))
            break;
        au = au->src;
    }
    return (au->user && lltype(au->user)) ? au->user : null;
}

// if given an enode, will always resolve the etype instance
etype etype_resolve(etype t) {
    Au_t au = t->au;
    if (au->member_type == AU_MEMBER_VAR)
        au = au->src;
    while (au && !au->is_funcptr && au->src) {
        au = au->src;
        if (au->user && au->lltype)
            break;
    }
    return (au->user && lltype(au->user)) ? au->user : null;
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
    etype_implement(result);
    return result;
}

// 
none push_lambda_members(aether a, efunc f) {
    statements lambda_code = statements(mod, (aether)a);
    push_scope(a, (Au)lambda_code);
    
    Au_t fn = f->au;
    LLVMValueRef context_ptr = f->context_node->value;
    LLVMTypeRef  ctx_struct  = f->context_node->au->src->lltype;

    // Extract each context member from the context struct
    int ctx_index = 0;
    members(fn, mem) {
        if (mem->member_type != AU_MEMBER_VAR) continue;
        
        Au_t ctx_au = def_member(lambda_code->au, mem->ident, mem->src, AU_MEMBER_VAR, mem->traits);
        evar ctx_evar = evar(mod, (aether)a, au, ctx_au);
        ctx_au->user = (etype)ctx_evar;
        
        LLVMValueRef indices[2] = {
            LLVMConstInt(LLVMInt32Type(), 0, 0),
            LLVMConstInt(LLVMInt32Type(), ctx_index, 0)
        };
        LLVMValueRef field_ptr = LLVMBuildGEP2(
            a->builder, ctx_struct, context_ptr, indices, 2, mem->ident);
        LLVMValueRef loaded = LLVMBuildLoad2(
            a->builder, lltype((Au)mem->src->user), field_ptr, mem->ident);
        
        ctx_evar->value  = loaded;
        ctx_evar->loaded = true;
        ctx_index++;
    }
}

// this is the declare (this comment stays)
none etype_init(etype t) {
    if (t->mod == null) t->mod = (aether)instanceof(t, aether);
    aether a = t->mod; // silver's mod will be a delegate to aether, not inherited

    if (t->au && t->au->ident && strstr(t->au->ident, "round")) {
        int test2 = 2;
        test2    += 2;
    }

    if (!t->au) {
        // we must also unregister this during watch cycles, upon reinit
        t->au = def(a->au, null, AU_MEMBER_NAMESPACE, 0);
    }

    Au_t    au  = t->au;
    bool  named = au && au->ident && strlen(au->ident);
    enode     n = instanceof(t, enode);

    if (au->lltype && !t->is_schema || (n && n->value && n->symbol_name))
        return;

    if (isa(t) == typeid(aether) || isa(t)->context == typeid(aether)) {
        a = (aether)t;
        verify(a->module && len(a->module), "no module provided");
        a->name = a->name ? a->name : stem(a->module);
        au = t->au = def_module(a->name->chars);
        if (!au->user) au->user = hold(t);
        return;
    } else if (t->is_schema) {
        au = t->au = def(a->import_module, fmt("__%s_t", au->ident)->chars,
            AU_MEMBER_TYPE, AU_TRAIT_SCHEMA | AU_TRAIT_STRUCT);

        // do this for Au types
        Au_t ref = au_lookup("Au_t");
        for (int i = 0; i < ref->members.count; i++) {
            Au_t au_mem  = (Au_t)ref->members.origin[i];

            // this is the last member (function table), if that changes, we no longer break
            if (au_mem->ident && strcmp(au_mem->ident, "ft") == 0) {
                Au_t new_ft = def(au->schema, "ft", AU_MEMBER_TYPE, AU_TRAIT_STRUCT);

                array cl = etype_class_list(t);
                each (cl,  etype, tt) {
                    for (int ai = 0; ai < tt->au->members.count; ai++) {
                        Au_t ai_mem = (Au_t)tt->au->members.origin[ai];
                        if (ai_mem->member_type != AU_MEMBER_FUNC)
                            continue;
                        
                        Au_t fn = def(new_ft, null, AU_MEMBER_FUNC, AU_TRAIT_FUNCPTR);
                        fn->traits = ai_mem->traits;
                        fn->rtype  = ai_mem->rtype;

                        for (int arg = 0; arg < ai_mem->args.count; arg++) {
                            Au_t arg_src = (Au_t)ai_mem->args.origin[arg];
                            Au_t arg_t   = arg_type(arg_src);
                            array_qpush((array)&fn->args, (Au)arg_t);
                        }
                    }
                }
                break;
            }
            Au_t new_mem      = def(t->au, au_mem->ident, au_mem->member_type, au_mem->traits);
            new_mem->src      = au_mem->src;
            new_mem->isize    = au_mem->isize;
            new_mem->elements = au_mem->elements;
            new_mem->typesize = au_mem->typesize;
            new_mem->abi_size = au_mem->abi_size;
            members(au_mem, mem) array_qpush((array)&new_mem->members, (Au)mem);
            arg_list(au_mem, mem)    array_qpush((array)&new_mem->args,    (Au)mem);
            new_mem->context = t->au; // copy entire member and reset for our for context
        }

        // upon implement, we can register the static_value on etype of schema
        // this is the address of the &type_i.type member
        
    }
    
    if (!au->member_type)
        au->member_type = AU_MEMBER_TYPE;

    if (!au->user && isa(t) != typeid(enode)) // only types are unique enough to be bound globally
    {
        au->user = hold(t); // before we were doing this for literal field lookups in functions
        if (au->ident && strcmp(au->ident, "round") == 0) {
            int test2 = 2;
            test2    += 2;
        }
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

        if (strcmp(au->ident, "round") == 0) {
            au = au;
        }
        if (is_lambda(au) && is_class(au->context))
            index = 1; // context inserted at [0]; gives us the target as well as the other args
        
        arg_list(au, arg) {
            Au_t t = arg->src;
            bool convert_prim_target = is_prim(t) && arg->is_target;
            arg_types[index++] = lltype(convert_prim_target ? pointer(a, (Au)t)->au : t);
        }

        if (is_lambda(au)) {
            string context_name = f(string, "%s_%s_context", au->context->ident, au->ident);
            etype etype_context = struct_from_au(a, context_name, au, false);
            fn->context_node = enode(mod, a, au, pointer(a, (Au)etype_context)->au, loaded, true, value, null);
            fn->context_node->au->user = (etype)fn->context_node;

            // create published type (we set this global on global initialize)
            string member_symbol = f(string, "%s_type", au->alt);
            LLVMTypeRef type = typeid(Au_t)->ptr->lltype;
            LLVMValueRef G = LLVMAddGlobal(a->module_ref, type, member_symbol->chars);
            LLVMSetLinkage(G, LLVMInternalLinkage);
            LLVMSetInitializer(G, LLVMConstNull(type));

            fn->published_type = enode(mod, a, symbol_name, member_symbol, value, G, au, typeid(Au_t));
            arg_types[0] = lltype(fn->context_node);
            n_args++;
        }

        au->lltype = LLVMFunctionType(
            au->rtype ? lltype(au->rtype) : LLVMVoidType(),
            arg_types, n_args, au->is_vargs);

        etype_ptr(a, au);
        free(arg_types);

    } else if (is_func_ptr((Au)au)) {
        etype fn = t;//au->user;

        int n_args = au->args.count;
        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));
        int index = 0;
        arg_list(au, arg)
            arg_types[index++] = lltype(arg);

        LLVMTypeRef fn_ty = LLVMFunctionType(
            au->rtype ? lltype(au->rtype) : LLVMVoidType(),
            arg_types, au->args.count, au->is_vargs);

        au->lltype = LLVMPointerType(fn_ty, 0);
        free(arg_types);
    } else if (au && au->is_pointer && au->src && !au->src->is_primitive) {
        Au_t src = au->src;
        while (src && src->src && !src->lltype)
            src = src->src;
        if (!src->lltype) {
            src = src;
        }
        verify(src && src->lltype, "resolution failure for type %o", au);
        au->lltype = LLVMPointerType(src->lltype, 0);
    } else if (named && (is_rec((Au)t) || au->is_union || au == typeid(Au_t))) {
        au->lltype = LLVMStructCreateNamed(a->module_ctx, cstr_copy(au->ident));
        if (au != typeid(Au_t) && !au->is_system)
            etype_ptr(a, t->au);
    } else if (is_enum(t)) {
        au->lltype = lltype(au->src ? au->src : typeid(i32));
    }
    else if (au == typeid(f32))  au->lltype = LLVMFloatType();
    else if (au == typeid(f64))  au->lltype = LLVMDoubleType();
    else if (au == typeid(none)) au->lltype = LLVMVoidType  ();
    else if (au == typeid(bool)) au->lltype = LLVMInt1Type  ();
    else if (au == typeid(i8)  || au == typeid(u8))
        au->lltype = LLVMInt8Type();
    else if (au == typeid(i16) || au == typeid(u16))
        au->lltype = LLVMInt16Type();
    else if (au == typeid(i32) || au == typeid(u32) || au == typeid(AFlag))
        au->lltype = LLVMInt32Type();
    else if (au == typeid(i64) || au == typeid(u64) || au == typeid(num))
        au->lltype = LLVMInt64Type();
    else if (au == typeid(symbol) || au == typeid(cstr) || au == typeid(raw)) {
        au->lltype = LLVMPointerType(LLVMInt8Type(), 0);
    } else if (au == typeid(cstrs)) {
        au->lltype = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);
    } else if (au == typeid(sz)) {
        au->lltype = LLVMIntPtrTypeInContext(a->module_ctx, a->target_data);
    } else if (au == typeid(cereal)) {
        LLVMTypeRef cereal_type = LLVMStructCreateNamed(a->module_ctx, "cereal");
        LLVMTypeRef members[] = {
            LLVMPointerType(LLVMInt8Type(), 0)  // char* → i8*
        };
        LLVMStructSetBody(cereal_type, members, 1, 1);
        au->lltype = cereal_type;
    } else if (au == typeid(floats)) {
        au->lltype = LLVMPointerType(LLVMFloatType(), 0);
    } else if (au == typeid(func)) {
        LLVMTypeRef fn_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);
        au->lltype = LLVMPointerType(fn_type, 0);
    } else if (au == typeid(hook)) {
        Au_t e_A = au_lookup("Au");
        if (!e_A->user) {
            e_A->user = etype(mod, a, au, e_A);
            etype_implement((etype)e_A->user); // move this to proper place
        }
        LLVMTypeRef param_types[] = { lltype(e_A) };
        LLVMTypeRef hook_type = LLVMFunctionType(lltype(e_A), param_types, 1, 0);
        au->lltype = LLVMPointerType(hook_type, 0);
    } else if (au == typeid(callback)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(
            lltype(e_A), param_types, 2, 0);
        au->lltype = LLVMPointerType(cb_type, 0);
    } else if (au == typeid(callback_extra)) {
        Au_t e_A = au_lookup("Au");
        LLVMTypeRef param_types[] = { lltype(e_A), lltype(e_A), lltype(e_A) };
        LLVMTypeRef cb_type = LLVMFunctionType(lltype(e_A), param_types, 3, 0);
        au->lltype = LLVMPointerType(cb_type, 0);
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
        au->lltype = LLVMPointerType(lltype(b), 0);
        au->is_pointer = true;
    }
    else if (au == typeid(handle))   au->lltype = LLVMPointerType(LLVMInt8Type(), 0);
    else if (au == typeid(ARef)) {
        au->src = typeid(Au);
        au->src->ptr = au;
        Au_t au_type = au_lookup("Au");
        au->lltype = LLVMPointerType(lltype(au_type), 0);
    }
    else if (au == typeid(Au_ts))    au->lltype = lltype(au_lookup("Au_ts"));
    else if (au == typeid(bf16))     au->lltype = LLVMBFloatTypeInContext(a->module_ctx);
    else if (au == typeid(fp16))     au->lltype = LLVMHalfTypeInContext(a->module_ctx);
    else if (au->is_pointer) {
        if (!au->src) {
            au->lltype        = LLVMPointerType(LLVMVoidType(), 0);
        } else {
            etype mdl_src  = null;
            cstr  src_name = null;
            string n       = string(au->ident);
            Au_t   src     = au->src;
            src_name       = src->ident;
            verify(src->user && lltype(src), "type must be created before %o: %s", n, src_name);
            au->lltype     = LLVMPointerType(src->lltype, 0);
            src->ptr       = au;
        }
    } else if ((au->traits & AU_TRAIT_ABSTRACT) == 0) {
        
    } else if (!au->is_abstract) {
        fault("not intializing %s", au->ident);
    }

    push(a->registry, (Au)t); // we only 'clear' our registry and the linked Au_t's
}

bool is_accessible(aether a, Au_t m) {
    if (m->access_type == interface_undefined || 
        m->access_type == interface_public    || (a->au == m->context || a->au == m->context->context))
        return true;
    return false;
}

Au_t read_arg_type(Au_t stored_arg) {
    if (stored_arg->member_type == AU_MEMBER_VAR) return stored_arg->src;
    return stored_arg;
}

etype get_type_t_ptr(etype t) {
    aether a = t->mod;

    /*
    bool allow = is_au_type(t) || a->au == t->au->module_ref;
    if (!allow)
        allow = allow;
    verify(allow,
        "get_type_t_ptr not available on external types");
    */

    if (t->schema)
        return t->schema->au->ptr->user;

    // schema must have it's static_value set to the instance
    etype schema = etype(mod, a, au, t->au, is_schema, true);
    t->schema = schema;
    
    etype mt = etype_ptr(a, schema->au);
    return mt;
}

etype implement_type_id(etype t) {
    if (t->au && t->au->ident && strcmp(t->au->ident, "nice_lambda") == 0) {
        int test2 = 2;
        test2    += 2;
    }
    if (t->schema && t->au->user && t->au->user->type_id)
        return t->schema->au->ptr->user;
    
    aether a = t->mod;

    if (strcmp(t->au->ident, "Au") == 0) {
        a = a;
    }
    // we must support a use-case where aether calls this on itsself.  a module must have its own identity in space!
    // we do not abstract away identity
    verify(t->au->module->is_au, "typeid not available on external types");

    Au_t au = t->au;

    // schema must have it's static_value set to the instance
    if (!t->schema)
        t->schema = etype(mod, a, au, t->au, is_schema, true);

    etype mt = etype_ptr(a, t->schema->au);
    mt->au->is_typeid = true;
    mt->au->is_imported = a->is_Au_import;
    
    Au_t au_type = au_lookup("Au");
    verify(au_type->ptr, "expected ptr type on Au");

    // we need to create the %o_i instance inline struct
    string n = f(string, "%s_info", au->ident);
    Au_t type_info = def_type  (a->au, n->chars,
        AU_TRAIT_STRUCT | AU_TRAIT_SYSTEM);
    Au_t type_h    = def_member(type_info, "info", au_type,
        AU_MEMBER_VAR, AU_TRAIT_SYSTEM | AU_TRAIT_INLAY);
    //type_h->elements = typeid(Au)->typesize;
    Au_t type_f    = def_member(type_info, "type", t->schema->au,
        AU_MEMBER_VAR, AU_TRAIT_SYSTEM | AU_TRAIT_INLAY);
    type_f->index = 1;
    type_info->user = etype(mod, a, au, type_info);
    etype_implement(type_info->user);
    string name = f(string, "%s_i", au->ident);
    evar schema_i = evar(mod, a, au, def_member(
                a->au, name->chars, type_info, AU_MEMBER_VAR,
                AU_TRAIT_SYSTEM | (a->is_Au_import ? AU_TRAIT_IS_IMPORTED : 0)));
    etype_implement((etype)schema_i);
    
    t->au->user->type_id = access(schema_i, string("type"));
    t->au->user->type_id->loaded = true;
    if (!a->is_Au_import) {
        set(a->user_type_ids, (Au)t, (Au)t->au->user->type_id);
    }
    return mt;
}

#include <stddef.h>

none etype_implement(etype t) {
    Au_t    au = t->au;
    aether a  = t->mod;
    enode nn = (enode)instanceof(t, enode);

    if (is_func(au) && !((enode)t)->used)
        return;

    if (au->is_implemented) return;

    au->is_implemented = true;
    
    // lets bootstrap schema pointer types 
    // not the static struct yet
    // just the pointer to the table type (unfinished)
    bool is_Au = (is_au_type(t) || a->au == au->module) &&
        (!au->is_schema && !au->is_system);
    etype type_t_ptr = is_Au ? get_type_t_ptr(t) : null;

    bool is_ARef = au == typeid(ARef);
    if (!is_ARef && au && au->src && au->src->user && !is_prim(au->src) && isa(au->src->user) == typeid(etype))
        if (au->src != typeid(Au_t) && !au->is_funcptr)
            etype_implement(au->src->user);

    if (au->member_type == AU_MEMBER_VAR) {
        enode n = (enode)instanceof(t, enode);
        if (!n)
            n = n;

        if (au->is_lambda) {
            n = n;
        }

        verify(n, "expected enode instance for AU_MEMBER_VAR");
        LLVMTypeRef ty = lltype(au->src);

        // if module member, then its a global value
        if (!au->context->context) {
            // mod->is_Au_import indicates this is coming from a lib, or we are making a new global
            LLVMValueRef G = LLVMAddGlobal(a->module_ref, ty, au->ident);
            LLVMSetLinkage(G, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
            if (!a->is_Au_import) {
                LLVMSetInitializer(G, LLVMConstNull(ty)); // or a real initializer
            }
            n->loaded = false;
            n->value = G; // it probably makes more sense to keep llvalue on the node
        } else if (!is_func(au->context)) {
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
                LLVMValueRef global = LLVMAddGlobal(a->module_ref, lltype(au->src), c_name->chars);
                LLVMSetLinkage(global, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
                if (!a->is_Au_import) LLVMSetInitializer(global, LLVMConstNull(lltype(au->src)));
                static_node->value = global;
                static_node->loaded = false;
            } else if (!au->is_static)
                n->value = LLVMBuildAlloca(a->builder, ty, "evar");
        } else if (is_func(au->context)) {
            verify(n->value, "expected evar to be set for arg");
            verify(isa(t) == typeid(evar), "expected evar instance for arg");
            n->loaded = true;
        }
        return;

    }

    if (!au->is_pointer && !au->is_funcptr && (is_rec(t) || au->is_union)) {
        array cl = (au->is_union || is_struct(t)) ? a(t) : etype_class_list(t);
        bool multi_Au = len(cl) && cl->origin[0] == typeid(Au)->user;
        int count = 0;
        int index = 0;
        each(cl, etype, tt) {
            if (!multi_Au || tt->au != typeid(Au))
                for (int i = 0; i < tt->au->members.count; i++) {
                    Au_t m = (Au_t)tt->au->members.origin[i];
                    if (m->member_type == AU_MEMBER_VAR && !m->is_static && is_accessible(a, m))
                        count++;
                }
            if (is_class(t))
                count++; // u8 Type_interns[isize]
        }

        if (au->ident && strcmp(au->ident, "Au") == 0) {
            int test2 = 2;
            test2    += 2;
        }

        LLVMTypeRef* struct_members = calloc(count + 2, sizeof(LLVMTypeRef));
        LLVMTypeRef largest = null;
        int ilargest = 0;
        each(cl, etype, tt) {
            //if (len(cl) > 1 && tt->au == typeid(Au)) break;
            int prop_index = 0;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = (Au_t)tt->au->members.origin[i];

                if (au->ident && strcmp(au->ident, "Au") == 0) {
                    int test2 = 2;
                    test2    += 2;
                }
                
                if (m->member_type == AU_MEMBER_FUNC      || 
                    m->member_type == AU_MEMBER_CONSTRUCT || 
                    m->member_type == AU_MEMBER_INDEX     || 
                    m->member_type == AU_MEMBER_OPERATOR  || 
                    m->member_type == AU_MEMBER_CAST) {

                    if (m->member_type == AU_MEMBER_CAST) {
                        m = m;
                    }
                    if (m->member_type == AU_MEMBER_CONSTRUCT) {
                        m = m;
                    }
                    src_init(a, m->rtype);
                    etype_implement(m->rtype->user);
                    arg_types (m, t) {
                        src_init(a, t);
                        etype_implement(t->user);
                    }
                    src_init(a, m);
                    etype_implement(m->user);
                }
                else if (m->member_type == AU_MEMBER_VAR && is_accessible(a, m)) {

                    Au_t src = m->src;
                    if (!src)
                        src = src;
                    
                    if (m->is_static ) {
                        string c_name = f(string, "%s_%s", au->ident, m->ident);
                        if (strcmp(au->ident, "font_manager_init") == 0) {
                            int test2 = 2;
                            test2    += 2;
                        }
                        LLVMValueRef global = LLVMAddGlobal(a->module_ref, lltype(m->src), c_name->chars);
                        LLVMSetLinkage(global, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
                        if (!a->is_Au_import) LLVMSetInitializer(global, LLVMConstNull(lltype(m->src)));
                        evar static_node = m->user ? (evar)m->user : evar(mod, a, au, m, value, global, loaded, false);
                        m->user = (etype)static_node;

                    } else {

                        verify(src, "no src type set for member %o", m);
                        src_init(a, src);
                        if (!is_class(src))
                            etype_implement(src->user);

                        // get largest union member
                        if (m->elements > 0) {
                            m = m;
                        }
                        if (m->is_inlay) {
                            if (strcmp(m->context->ident, "app") == 0 && strcmp(m->ident, "info") == 0) {
                                m = m;
                            }
                            struct_members[index] = src->is_class ? src->lltype : lltype(src);
                        } else
                            struct_members[index] = lltype(src);

                        if (m->elements > 0)
                            struct_members[index] = LLVMArrayType(struct_members[index], m->elements);
                        
                            
                        verify(struct_members[index], "no lltype found for member %s.%s", au->ident, m->ident);
                        //printf("verifying abi size of %s\n", m->ident);
                        int abi_member  = LLVMABISizeOfType(a->target_data, struct_members[index]);
                        if (!abi_member) {
                            struct_members[index] = lltype(src->src);
                            int abi_member  = LLVMABISizeOfType(a->target_data, struct_members[index]);
                            for (int i = 0; i < index; i++) {
                                printf("member %i = %p\n", i, struct_members[i]);
                            }
                            abi_member = abi_member;
                        }
                        verify(abi_member, "type has no size");
                        if (au->is_union && src->abi_size > ilargest) {
                            largest  = struct_members[index];
                            ilargest = src->abi_size;
                        }
                        index++;

                    }
                }
            }

            // lets define this as a form of byte accessible opaque.
            if (is_class(t)) {
                struct_members[index] = LLVMArrayType(lltype(au_lookup("u8")), tt->au->isize);
                index++;
            }
        }

        if (count != index) {
            int test2 = 2;
            test2    += 2;
        }

        verify(count == index, "member indexing mismatch on %o", t);

        etype type_t_ptr = null;
        if (is_class(t) && au != au_lookup("Au_t") && 
           !au->is_system && !au->is_schema) {
            type_t_ptr = get_type_t_ptr(t);

        } else if (count == 0) {
            struct_members[count++] = lltype(au_lookup("u8")->user);
        }
        
        if (type_t_ptr) {
            struct_members[count++] = lltype(type_t_ptr);
            struct_members[count++] = lltype(type_t_ptr);
        }
        
        if (au->is_union) {
            count = 1;
            struct_members[0] = largest;
        }

        LLVMStructSetBody(au->lltype, struct_members, count, 1);

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
            if (m->user) {
                return;
            }
            verify(m->value, "no value set for enum %s:%s", au->ident, m->ident);
            string n = f(string, "%s_%s", au->ident, m->ident);
            
            LLVMTypeRef tr = au->lltype;
            LLVMValueRef G = LLVMAddGlobal(a->module_ref, tr, n->chars);
            
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
                
                if (!m->user) m->user = (etype)hold(enode(mod, a, loaded, false, value, G, au, m));
                LLVMSetInitializer(G, init);
            }
        }
        enum_processing = false;
    } else if (is_func((Au)t)) {
        Au_t cl = isa(t);
        if (cl !=  typeid(efunc)) {
            cl = cl;
        }
        verify(cl == typeid(efunc), "expected efunc");
        //string n = is_rec(au->context) ?
        //    f(string, "%s_%s", au->context->ident, au->ident) : string(au->ident);
        cstr n = au->alt ? au->alt : au->ident;
        verify(n, "no name given to function");
        if (n && strcmp(n, "round") == 0) {
            n = n;
        }
        enode fn = (enode)t;

        // functions, on init, create the arg enodes from the model data
        // make sure the types are ready for use

        arg_types(au, arg_type) {
            if (arg_type->user) {
                verify(arg_type->user, "expected user data on type %s", arg_type->ident);
                etype_implement((etype)arg_type->user);
            }
        }

        fn->value  = LLVMAddFunction(a->module_ref, n, au->lltype);

        // fill out enode values for our args type pointer 
        // context type pointer (we register these on init)
        if (is_lambda(au)) {
            LLVMValueRef value_ctx  = LLVMGetParam(fn->value, 0); // we have one more than args for the context
            fn->context_node->value = value_ctx;
        }

        Au_t au_target = au->is_imethod ? (Au_t)au->args.origin[0] : null;
        fn->target = au_target ?
            enode(mod, a, au, au_target, arg_index, 0, loaded, true) : null;
        if (fn->target) {
            // todo: check
            fn->target->au->user = (etype)fn->target;
        }
        if (strcmp(au->ident, "init") == 0) {
            fn = fn;
        }
        string label = f(string, "%s_entry", au->ident);
        bool is_user_implement = fn->au->module == a->au && fn->has_code;
        if (fn->au->module == a->au && !is_user_implement) {
            is_user_implement = is_user_implement;
        }
        fn->entry = is_user_implement ? LLVMAppendBasicBlockInContext(
            a->module_ctx, fn->value, label->chars) : null;

            
        LLVMSetLinkage(fn->value,
            is_user_implement && !au->is_export ? 
                LLVMInternalLinkage : LLVMExternalLinkage);

        int index = 0;
        arg_list(fn->au, arg) {
            verify(arg != typeid(Au), "unexpected Au");
            arg->user = (etype)evar(mod, a, au, arg, arg_index, index, loaded, true);
            index++;
        }
    }

    if (!au->is_void && !is_opaque(au) && !is_func((Au)au) && au->lltype) {
        if (au->elements) {
            au->lltype = LLVMArrayType((LLVMTypeRef)au->lltype, au->elements);
        }
        au->abi_size   = LLVMABISizeOfType(a->target_data, (LLVMTypeRef)au->lltype)      * 8;
        au->align_bits = LLVMABIAlignmentOfType(a->target_data, (LLVMTypeRef)au->lltype) * 8;
    }
}

none aether_output_schemas(aether a, enode init) {

    if (save != typeid(Au)->user) {
        save = save;
    }
    
    Au_t au = init->au;
    if (au->is_mod_init) {
        efunc f = (efunc)au->user;

        Au_t module_base = a->au;
        Au_t au_type     = au_lookup("Au");
        Au_t fn_module_lookup = find_member(au_type, "module_lookup", AU_MEMBER_FUNC, false);
        Au_t fn_find_member = find_member(au_type, "find_member", AU_MEMBER_FUNC, false);
        Au_t fn_emplace  = find_member(au_type, "emplace_type", AU_MEMBER_FUNC, false);
        Au_t fn_def_func = find_member(au_type, "def_func",  AU_MEMBER_FUNC, false);
        Au_t fn_def_prop = find_member(au_type, "def_prop",  AU_MEMBER_FUNC, false);
        Au_t fn_def_arg  = find_member(au_type, "def_arg",   AU_MEMBER_FUNC, false);
        Au_t fn_def_meta = find_member(au_type, "def_meta",  AU_MEMBER_FUNC, false);
        Au_t fn_def_type = find_member(au_type, "def_type",  AU_MEMBER_FUNC, false);
        Au_t fn_push     = find_member(au_type, "push_type", AU_MEMBER_FUNC, false);

        push_scope(a, (Au)f);
        enode module_id  = e_fn_call(a, (efunc)fn_module_lookup->user, a(const_string(chars, a->name->chars)));

        // iterate through user defined type id's
        pairs(a->user_type_ids, i) {
            etype mdl         = instanceof(i->key, etype);
            enode type_id     = instanceof(i->value, enode);
            bool  is_class_t  = is_class(mdl);
            bool  is_struct_t = is_struct(mdl);
            bool  is_enum_t   = is_enum(mdl);
            Au_t  au          = mdl->au;
            u64   isize       = 0;

            if (!is_class_t && !is_struct_t && !is_enum_t)
                continue;

            // calculate isize
            members(au, mem) {
                if (mem->member_type == AU_MEMBER_VAR && 
                    mem->access_type == interface_intern)
                    isize += mem->typesize;
            }

            etype au_context_user = au->context ? au->context->user : null;
            // initialize the type and fields
            bool ctx_module = is_module(au->context);
            e_fn_call(a, (efunc)fn_emplace->user, a(
                type_id, ctx_module ? e_null(a, null) : e_typeid(a, u(au->context)), e_typeid(a, u(au->src)), module_id,
                const_string(chars, au->ident), _u64(au->traits),
                _u64(mdl->au->typesize), _u64(isize)
            ));
            
            members(au, mem) {
                if (mem->access_type == interface_intern)
                    continue;
                
                if (is_func(mem)) {
                    symbol n = mem->alt ? mem->alt : mem->ident;
                    ((enode)mem->user)->used = true;
                    etype_implement((etype)mem->user);
                    LLVMValueRef fn_ptr = ((enode)mem->user)->value;
                    Au_t id_none = typeid(none);

                    etype aref = typeid(ARef)->user;
                    enode fptr = value(aref, fn_ptr);
                    enode e_mem = e_fn_call(a, (efunc)fn_def_func->user, a(
                        type_id, const_string(chars, mem->ident), e_typeid(a, mem->rtype->user), 
                        _u32(mem->member_type),
                        _u32(mem->access_type),
                        _u32(mem->operator_type),
                        _u64(mem->traits),
                        fptr
                    ));

                    arg_list(mem, arg) {
                        e_fn_call(a, (efunc)fn_def_arg->user,
                            a(e_mem, const_string(chars, arg->ident), e_typeid(a, arg->src->user)));
                    }

                    // for lambdas, we set a global to the Au_t member
                    // we may then iterate through this at runtime to use lambda
                    // could use type_id on etype for this
                    if (is_lambda((Au)mem)) {
                        enode n = (enode)mem->user;
                        verify(n->published_type, "expected published Au_t symbol for lambda");
                        e_assign(a, n->published_type, (Au)e_mem, OPType__assign);
                    }

                } else if (mem->member_type == AU_MEMBER_VAR) {
                    enode e_mem = e_fn_call(a, (efunc)fn_def_prop->user,
                        a(type_id, const_string(chars, mem->ident), e_typeid(a, mem->src->user), 
                            _u64(mem->traits), 
                            _u32(mem->abi_size),
                            _u32(mem->access_type),
                            _u32(mem->operator_type)
                        ));
                } else if (mem->member_type == AU_MEMBER_TYPE) {
                    // shouldnt have types embedded in types (not allowed)
                } else {
                    fault("unsupported member");
                }
            }
            e_fn_call(a, (efunc)fn_push->user, a(type_id));
        }
        pop_scope(a);
        build_module_initializer(a, f);
        build_entrypoint(a, f);
    }
}

statements aether_context_code(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (instanceof(ctx->user, statements))
            return (statements)ctx->user;
        break;
    }
    return null;
}

enode aether_context_func(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_FUNC && isa(ctx->user) == typeid(efunc))
            return (enode)ctx->user;
    }
    return null;
}

etype aether_context_class(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_class)
            return ctx->user;
    }
    return null;
}

etype aether_context_struct(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_struct)
            return ctx->user;
    }
    return null;
}

etype aether_context_model(aether a, Au_t type) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && isa(ctx->user) == type)
            return ctx->user;
    }
    return null;
}

etype aether_context_record(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && (ctx->is_struct || ctx->is_class))
            return ctx->user;
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

    enode prev_fn = context_func(a);

    if (!is_func(au) && au->src && (au->src->is_class || au->src->is_struct))
        push(a->lexical, (Au)au->src);
    else
        push(a->lexical, (Au)au);

    enode fn = context_func(a);
    if (fn && isa(fn) == typeid(efunc) && (fn != prev_fn) && !a->no_build) {
        LLVMPositionBuilderAtEnd(a->builder, fn->entry);
        LLVMSetCurrentDebugLocation2(a->builder, fn->last_dbg);
    }
}

etype get_type_t_ptr(etype t);

// establishes concrete type identity (can be run multiple times)
void aether_create_type_members(aether a, Au_t ctx) {
    ctx = ctx;
    if (ctx->ident && strcmp(ctx->ident, "something") == 0) {
        int test2 = 2;
        test2    += 2;
    }
    // we must do this after building records / enums (in silver)
    members(ctx, mem) {
        if (mem->member_type == AU_MEMBER_VAR)
            mem = mem->src;
        if (mem == typeid(Au_t)) continue;
        if (mem->ident && strcmp(mem->ident, "aclass") == 0) {
            mem = mem;
        }
        if (mem->ident && is_au_type(mem) && mem->user && !mem->user->type_id && !mem->is_system && !mem->is_schema) {
            get_type_t_ptr(mem->user);
            implement_type_id(mem->user);
        }
    }
}

void aether_import_models(aether a, Au_t ctx) {
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

    Au info = head(typeid(Au)->user);

    save = typeid(Au)->user;

    Au_t ty = typeid(i32);

    for (int filter = 0; filter < 10; filter++) {
        struct filter* ff = &filters[filter];
        
        if (save != typeid(Au)->user) {
            save = save;
        }
        for (num i = 0; i < ctx->members.count; i++) {
            Au_t m  =  (Au_t)ctx->members.origin[i];
            
            if (m->module != ctx)
                continue;

            if (ff->member_type && ff->member_type != m->member_type) continue;

            bool is_func = m->member_type == AU_MEMBER_FUNC || m->member_type == AU_MEMBER_INDEX || 
                           m->member_type == AU_MEMBER_CAST || m->member_type == AU_MEMBER_OPERATOR;
            bool p0      = (ff->has_bits & m->traits) == ff->has_bits;
            bool p1      = (ff->not_bits & m->traits) == 0;
            bool proceed = p0 && p1;

            if (ff->additional) {
                bool proceed = (is_func == ff->is_func) ^ ff->func_xor;
                if (!proceed) continue;
            }

            if (proceed) {

                if (m->ident && strstr(m->ident, "f32")) {
                    int test2 = 2;
                    test2    += 2;
                    members(m, strm) {
                        if (strm->ident && strstr(strm->ident, "round")) {
                            Au_t arg0 = (Au_t)strm->args.origin[0];
                            strm = strm;
                        }
                    }
                }

                if (ff->init || ff->impl) {
                    src_init(a, m);
                }
                if (ff->impl) {
                    if (m->ident && strcmp(m->ident, "vec2f") == 0) {
                        int test2 = 2;
                        test2    += 2;
                    }
                    etype_implement((etype)m->user);
                }
            }
        }

        if (save != typeid(Au)->user) {
            save = save;
        }
    }
    create_type_members(a, ctx);
    if (save != typeid(Au)->user) {
        save = save;
    }
}

void aether_import_Au(aether a, Au lib) {
    a->current_inc   = instanceof(lib, Au_t) ? path(((Au_t)lib)->ident) : lib ? (path)lib : path("Au");
    a->is_Au_import  = true;
    string  lib_name = lib && instanceof(lib, path) ? stem((path)lib) : null;
    Au_t    au_module = null;

    // register and push new module scope if we are loading from library
    if (lib) {
        au_module = instanceof(lib, path) ? def_module(copy_cstr(lib_name->chars)) : (Au_t)lib;
        push_scope(a, (Au)au_module);
    } else {
        au_module = global();
        set(a->libs, string("Au"), _bool(true));
        a->au_module = au_module;
    }

    a->import_module = au_module;

    Au_t base = au_lookup("Au_t");
    if (instanceof(lib, path)) {
        string path_str = string(((path)lib)->chars);
        handle lib_instance = dlopen(cstring(path_str), RTLD_NOW);
        verify(lib_instance, "shared-lib failed to load: %o", lib);
        set(a->libs, path_str, (Au)lib_instance);
    }

    Au_t au    = typeid(Au);
    if (!au->user) {
        au->user   = etype(mod, a, au, au);
        Au_t au_t  = typeid(Au_t);
        au_t->user = etype(mod, a, au, au_t);
        etype_ptr(a, au); // Au should be pointer, and we have it as struct; we need to load _Au as what we do, and point to it with Au
        etype_ptr(a, au_t);
    }

    aether_import_models(a, au_module);
    if (!au_module->user) {
        au_module->user = (etype)hold(emodule(mod, a, au, au_module));
    }
    if (!lib) {
        // this causes a name-related error in IR
        Au_t f = find_member(au_module, "app", AU_MEMBER_TYPE, AU_TRAIT_ABSTRACT);
        verify(f, "could not import app abstract");

        if (!f->user) {
            f->user = etype(mod, a, au, f);
            implement(f->user);
        }

        verify(f->context == typeid(Au), "expected Au type context");
    }
    a->is_Au_import  = false;
}

void aether_llflag(aether a, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32Type(), ival, 0));

    char sflag[64];
    memcpy(sflag, flag, strlen(flag) + 1);
    LLVMAddModuleFlag(a->module_ref, LLVMModuleFlagBehaviorError, sflag, strlen(sflag), v);
}


bool aether_emit(aether a, ARef ref_ll, ARef ref_bc) {
    path* ll = (path*)ref_ll;
    path* bc = (path*)ref_bc;
    cstr err = NULL;

    path c = path_cwd();

    *ll = form(path, "%o.ll", a);
    *bc = form(path, "%o.bc", a);

    LLVMDumpModule(a->module_ref);

    if (LLVMPrintModuleToFile(a->module_ref, cstring(*ll), &err))
        fault("LLVMPrintModuleToFile failed");

    if (LLVMVerifyModule(a->module_ref, LLVMReturnStatusAction, &err)) {
        fprintf(stderr, "LLVM verify failed:\n%s\n", err);
        LLVMDisposeMessage(err);
        abort();
    }

    if (LLVMVerifyModule(a->module_ref, LLVMPrintMessageAction, &err))
        fault("error verifying module");
    
    if (LLVMWriteBitcodeToFile(a->module_ref, cstring(*bc)) != 0)
        fault("LLVMWriteBitcodeToFile failed");

    return true;
}


// todo: adding methods to header does not update the methods header (requires clean)
void aether_reinit_startup(aether a) {
    a->lexical = hold(array(alloc, 32, assorted, true, unmanaged, true));
    push_scope(a, (Au)global());
    push_scope(a, (Au)a->au);

    // todo: we must unregister everything in Au minus its base

} 

none aether_test_write(aether a) {
    char* err = 0;
    LLVMPrintModuleToFile(a->module_ref, "crashing.ll", &err);
}

none aether_init(aether a) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    if ( a->module) a->module = absolute(a->module);
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
    a->stack = array(16);
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
    push(a->lib_paths, (Au)f(path, "%o/lib", a->install));
    path src_path = a->module;
    push(a->include_paths, (Au)src_path);

    a->registry       = array(alloc, 256, assorted, true);
    a->libs           = map(assorted, true, unmanaged, true);
    a->user_type_ids  = map(assorted, true);
    a->lexical        = array(unmanaged, true, assorted, true);
    a->module_ref     = LLVMModuleCreateWithName(a->name->chars);
    a->module_ctx     = LLVMGetModuleContext(a->module_ref);
    a->dbg_builder    = LLVMCreateDIBuilder(a->module_ref);
    a->builder        = LLVMCreateBuilderInContext(a->module_ctx);
    a->target_triple  = LLVMGetDefaultTargetTriple();
    cstr err = NULL;
    if (LLVMGetTargetFromTriple(a->target_triple, &a->target_ref, &err))
        fault("error: %s", err);
    a->target_machine = LLVMCreateTargetMachine(
        a->target_ref, a->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    path rel = parent_dir(a->module);
    a->file = LLVMDIBuilderCreateFile(a->dbg_builder, a->name->chars, len(a->name), rel->chars, len(rel));

    a->target_data = LLVMCreateTargetDataLayout(a->target_machine);
    // todo: add debug back in using reference backup version
    /*a->compile_unit = LLVMDIBuilderCreateCompileUnit(
        a->dbg_builder, LLVMDWARFSourceLanguageC, a->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);
    a->au->llscope = a->compile_unit;*/
    a->builder = LLVMCreateBuilderInContext(a->module_ctx);

    // push our module space to the scope
    Au_t g = global();
    verify(g,           "globals not registered");
    verify(a->au,       "no module registered for aether");
    verify(g != a->au,  "aether using global module");

    push_scope(a, (Au)g);

    a->au->is_au = true;
    import_Au(a, null);

    a->au->is_namespace = true; // for the 'module' namespace at [1], i think we dont require the name.. or, we set a trait
    a->au->is_nameless  = false; // we have no names, man. no names. we are nameless! -cereal
    push_scope(a, (Au)a->au);
}

none aether_dealloc(aether a) {
    LLVMDisposeBuilder  (a->builder);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module_ref);
    LLVMContextDispose  (a->module_ctx);
    LLVMDisposeMessage  (a->target_triple);
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

    if (n->au->ident && strcmp(n->au->ident, "arg1") == 0) {
        n = n;
    }
    if (is_func((Au)n->au->context) && !n->symbol_name) {
        int offset = 0;
        if (is_lambda((Au)n->au->context))
            offset += 1;
        
        enode fn = (enode)au_arg_type((Au)n->au->context)->user;
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

static array expand_tokens(aether a, array tokens, map expanding) {
    int ln = len(tokens);
    array res = array(alloc, 32);

    int skip = 1;
    for (int i = 0; i < ln; i += skip) {
        skip    = 1;
        token token_a = (token)tokens->origin[i];
        token token_b = ln > (i + 1) ? (token)tokens->origin[i + 1] : null;
        int   n = 2; // after b is 2 ahead of token_a

        if (token_b && eq(token_b, "##")) {
            if  (ln <= (i + 2)) return null;
            token c = (token)tokens->origin[i + 2];
            if  (!c) return null;
            token  aa = token(alloc, len(token_a) + len(c) + 1);
            concat(aa, (string)a);
            concat(aa, (string)c);
            token_a = aa;
            n = 4;
            token_b = ln > (i + 3) ? (token)tokens->origin[i + 3] : null; // can be null
            skip += 2;
        }

        // see if this token is a fellow macro
        macro m = (macro)elookup(token_a->chars);
        string mname = cast(string, m);
        if (m && token_b && eq(token_b, "(") && !get(expanding, (Au)mname)) {
            array args  = array(alloc, 32);
            int   index = i + n + 1;

            while (true) {
                int  stop = 0;
                array arg = read_arg(tokens, index, &stop);
                if (!arg)
                    return null;
                
                skip += len(arg) + 1; // for the , or )
                push(args, (Au)arg);
                token  after = (token)tokens->origin[stop];
                if (eq(after, ")")) {
                    index++;
                    break;
                }
            }

            set(expanding, (Au)mname, _bool(true));
            array exp = macro_expand(m, args, expanding);
            rm(expanding, (Au)mname);

            concat(res, exp);
        } else
            push(res, (Au)a);
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
    aether a = m->mod;

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
            if (compare(t, (token)m->params->origin[param]) == 0) {
                concat(initial, (array)args->origin[param]);
                found = true;
                break;
            } 
        }
        if (!found)
            push(initial, (Au)t);
    }

    if (!expanding)
         expanding = map(hsize, 16, assorted, true, unmanaged, true);

    set(expanding, (Au)string(m->au->ident), _bool(true));

    // once replaced, we expand those as a flat token list
    print_all(a, "initial", initial);
    array rr = expand_tokens(a, initial, expanding);
    print_all(a, "expand", rr);
    string tstr = string(alloc, 64);
    each (rr, token, t) { // since literal processing is in token_init we shouldnt need this!
        if (tstr->count > 0)
            append(tstr, " ");
        concat(tstr, (string)t);
    }
    return (array)tokens(
        target, (Au)a, input, (Au)tstr, parser, a->parse_f);
        // we call this to make silver compatible tokens
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, efunc f, array query) {
    fault("must subclass codegen for usable code generation");
    return null;
}



// f = format string; this is evaluated from nodes given at runtime
void aether_eprint(aether a, symbol f, ...) {
    if (a->no_build) return;
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
    etype mdl_cstr = typeid(cstr)->user;
    etype mdl_i32  = typeid(i32)->user;
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
}

/*
    // there is no example use case of membership registration of vars
    Au_t mem = def_member(top_scope(a), mdl->au, AU_MEMBER_VAR,
        is_const ? AU_TRAIT_CONST : 0);
*/

void code_init(code c) {
    enode fn = context_func(c->mod);
    verify(fn, "no function in context for code block");
    c->block = LLVMAppendBasicBlock(fn->value, c->label);
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
        a->builder, (LLVMIntPredicate)comp, l->value, r->value, "cond");
    LLVMBuildCondBr(a->builder, cond, lcode->block, rcode->block);
}

enode aether_e_element(aether a, enode array, Au index) {
    etype imdl = (etype)(len(array->meta) ? 
        (etype)array->meta->origin[0] : typeid(Au)->user);
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, imdl);
    enode i = e_operand(a, index, null);
    enode element_v = value(imdl, LLVMBuildInBoundsGEP2(
        a->builder, lltype(array), array->value, &i->value, 1, "eelement"));
    return e_load(a, element_v, null);
}

void aether_e_inc(aether a, enode v, num amount) {
    a->is_const_op = false;
    if (a->no_build) return;
    enode lv = e_load(a, v, null);
    LLVMValueRef one = LLVMConstInt(LLVMInt64Type(), amount, 0);
    LLVMValueRef nextI = LLVMBuildAdd(a->mod->builder, lv->value, one, "nextI");
    LLVMBuildStore(a->mod->builder, nextI, v->value);
}

void aether_e_branch(aether a, code c) {
    a->is_const_op = false;
    if (a->no_build) return;
    LLVMBuildBr(a->builder, c->block);
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
        enode res     = value(typeid(i32)->user, LLVMConstInt(LLVMInt32Type(), diff, 0));
        res->literal  = hold(literal);
        return res;
    }

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, typeid(i32)->user);

    verify(context_func(a), "non-const compare must be in a function");

    // Class comparison - try member function first, fall back to pointer compare
    if (Lc || Rc) {
        if (!Lc) { enode t = L; L = R; R = t; }

        Au_t eq = find_member(L->au, "compare", AU_MEMBER_FUNC, true);
        if (eq) {
            verify(eq->rtype == typeid(i32), "compare function must return i32, found %o", eq->rtype);
            return e_fn_call(a, (efunc)eq->user, a(L, R));
        }

        LLVMValueRef lt = LLVMBuildICmp(a->builder, LLVMIntULT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildICmp(a->builder, LLVMIntUGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);
        LLVMValueRef lt_val  = LLVMBuildSelect(a->builder, lt, neg_one, zero, "lt_val");
        LLVMValueRef result  = LLVMBuildSelect(a->builder, gt, pos_one, lt_val, "cmp_r");
        return value(typeid(i32)->user, result);
    }

    // Struct comparison - member-by-member with early exit
    if (Ls || Rs) {
        verify(Ls && Rs && (canonical(L) == canonical(R)),
            "struct type mismatch %o != %o", L->au->ident, R->au->ident);

        LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(a->builder));
        LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(fn, "cmp_exit");

        array phi_vals   = array(alloc, 32, unmanaged, true);
        array phi_blocks = array(alloc, 32, unmanaged, true);

        members(L->au, mem) {
            if (is_func(mem)) continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(a->builder, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(a->builder, R->value, mem->index, "rv");
            enode        cmp = e_cmp(a, value(mem->user, lv), value(mem->user, rv));

            LLVMValueRef is_nonzero = LLVMBuildICmp(
                a->builder, LLVMIntNE, cmp->value,
                LLVMConstInt(LLVMInt32Type(), 0, 0), "cmp_nz");

            LLVMBasicBlockRef next_block = LLVMAppendBasicBlock(fn, "cmp_next");

            // Record this block + value for phi, then branch
            push(phi_vals,   (Au)cmp->value);
            push(phi_blocks, (Au)LLVMGetInsertBlock(a->builder));
            LLVMBuildCondBr(a->builder, is_nonzero, exit_block, next_block);

            LLVMPositionBuilderAtEnd(a->builder, next_block);
        }

        // Fallthrough: all members equal
        push(phi_vals,   (Au)LLVMConstInt(LLVMInt32Type(), 0, 0));
        push(phi_blocks, (Au)LLVMGetInsertBlock(a->builder));
        LLVMBuildBr(a->builder, exit_block);

        LLVMPositionBuilderAtEnd(a->builder, exit_block);
        LLVMValueRef phi = LLVMBuildPhi(a->builder, LLVMInt32Type(), "cmp_phi");
        LLVMAddIncoming(phi, (LLVMValueRef*)vdata(phi_vals), (LLVMBasicBlockRef*)vdata(phi_blocks), len(phi_vals));

        return value(typeid(i32)->user, phi);
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
        LLVMValueRef lt = LLVMBuildFCmp(a->builder, LLVMRealOLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildFCmp(a->builder, LLVMRealOGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);
        LLVMValueRef lt_val  = LLVMBuildSelect(a->builder, lt, neg_one, zero, "lt_val");
        LLVMValueRef result  = LLVMBuildSelect(a->builder, gt, pos_one, lt_val, "cmp_r");
        return value(typeid(i32)->user, result);
    }

    // Integer comparison via subtraction
    return value(typeid(i32)->user, LLVMBuildSub(a->builder, L->value, R->value, "cmp_i"));
}

enode aether_compatible(aether a, etype r, string n, AFlag f, array args) {
    members (r->au, mem) {
        enode fn = is_func(mem) ? (enode)mem->user : null;
        // must be function with optional name check
        if (!fn || (((f & mem->traits) != f) && (!n || !eq(n, mem->ident))))
            continue;
        if (mem->args.count != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        arg_types(mem, au) {
            etype fr = (etype)args->origin[ai];
            if (!convertible(fr, au->user)) {
                compatible = false;
                break;
            }
            ai++;
        }
        if (compatible)
            return (enode)mem->user;
    }
    return (enode)false;
}

enode aether_e_break(aether a, catcher cat) {
    a->is_const_op = false;
    if (!a->no_build)
        LLVMBuildBr(a->builder, cat->block);
    return e_noop(a, null);
}

enode aether_e_bitwise_not(aether a, enode L) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, typeid(bool)->user);

    return value(L, LLVMBuildNot(a->builder, L->value, "bitwise-not"));
}


enode aether_e_not(aether a, enode L) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, typeid(bool)->user);

    LLVMValueRef result;
    etype Lm = canonical(L);
    if (is_float(Lm)) {
        // for floats, compare with 0.0 and return true if > 0.0
        result = LLVMBuildFCmp(a->builder, LLVMRealOLE, L->value,
                               LLVMConstReal(lltype(Lm), 0.0), "float-not");
    } else if (is_unsign(Lm)) {
        // for unsigned integers, compare with 0
        result = LLVMBuildICmp(a->builder, LLVMIntULE, L->value,
                               LLVMConstInt(lltype(Lm), 0, 0), "unsigned-not");
    } else {
        // for signed integers, compare with 0
        result = LLVMBuildICmp(a->builder, LLVMIntSLE, L->value,
                               LLVMConstInt(lltype(Lm), 0, 0), "signed-not");
    }
    return value(typeid(bool)->user, result);
}

enode enode_retain(enode mem) {
    aether a = mem->mod;
    etype mdl = (etype)evar_type((evar)mem);
    a->is_const_op = false;
    if (mdl->au->is_class && !a->no_build) {
        efunc fn_hold = (efunc)find_member(typeid(Au), "hold", AU_MEMBER_FUNC, true)->user;
        e_fn_call(a, fn_hold, a(mem));
    }
    return mem;
}

enode enode_release(enode mem) {
    aether a = mem->mod;
    etype mdl = (etype)evar_type((evar)mem);

    a->is_const_op = false;
    if (mdl->au->is_class && !a->no_build) {
        efunc fn_drop = (efunc)find_member(typeid(Au), "drop", AU_MEMBER_FUNC, true)->user;
        e_fn_call(a, fn_drop, a(mem));
    }
    return mem;
}


Au_t aether_pop_scope(aether a) {
    statements st = (statements)instanceof(top_scope(a)->user, statements);
    /*
    this builds too much code, and isnt as performant as an AF 
    if (st) {
        members(st->au, mem) {
            if (instanceof(mem->user, enode))
                release((enode)mem->user);
        }
    }*/
    enode prev_fn = context_func(a);

    if (prev_fn && !a->no_build)
        prev_fn->last_dbg = LLVMGetCurrentDebugLocation2(
            a->builder);

    pop(a->lexical);
    a->top = (Au_t)last_element(a->lexical);

    enode fn = context_func(a);
    if (fn && (fn != prev_fn) && !a->no_build) {
        LLVMPositionBuilderAtEnd(a->builder, fn->entry);
        LLVMSetCurrentDebugLocation2(a->builder, fn->last_dbg);
    }
    return a->top;
}


etype aether_return_type(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (isa(ctx->user) == typeid(efunc) && ctx->rtype)
            return ctx->rtype->user;
    }
    return null;
}

// ident is optional, and based on 
efunc aether_function(aether a, etype place, string ident, etype rtype, array args, u8 member_type, u32 traits, u8 operator_type) {
    Au_t context = place->au;
    Au_t au = def(context, ident->chars, AU_MEMBER_FUNC, traits);
    if (context->member_type == AU_MEMBER_MODULE)
        au->module = context;
    else if (context->is_class || context->is_struct) {
        au->module = context->module;
    }

    if (!rtype) rtype = typeid(none)->user;
    each(args, Au, arg) {
        Au_t a = au_arg(arg);
        Au_t argument = def(au, null, AU_MEMBER_VAR, 0);
        argument->src = a;
        array_qpush((array)&au->args, (Au)argument);
    }
    au->src = (Au_t)hold(rtype->au);
    efunc n = efunc(mod, a, au, au, used, true, loaded, true);
    au->user = (etype)n;
    //etype_implement((etype)n); // creates llvm function value for enode
    return n;
}

etype aether_record(aether a, etype place, etype based, string ident, u32 traits) {
    Au_t context = place->au;
    Au_t au = def(context, ident->chars, AU_MEMBER_TYPE, traits);
    au->context = based ? based->au : typeid(Au)->user->au;
    etype n = etype(mod, a, au, au);
    //etype_implement(n);
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
        string("initializer"), typeid(none)->user, array(),
        AU_MEMBER_FUNC, AU_TRAIT_MODINIT, OPType__undefined);
    init->has_code = true;

    etype_implement((etype)init);
    a->fn_init = init;
    return init;
}

none aether_e_memcpy(aether a, enode _dst, enode _src, Au_t au_size) {
    a->is_const_op = false;
    LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMValueRef dst  = LLVMBuildBitCast(a->builder, _dst->value, i8ptr, "dst");
    LLVMValueRef src  = LLVMBuildBitCast(a->builder, _src->value, i8ptr, "src");
    LLVMValueRef sz   = LLVMConstInt(LLVMInt64Type(), au_size->abi_size / 8, 0);
    LLVMValueRef align = LLVMConstInt(LLVMInt32Type(), 8, 0); // your alignment
    LLVMBuildMemCpy(a->builder, dst, 0, src, 0, sz);
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

define_class(emodule,    etype)
define_class(aether,     emodule)

define_class(aclang_cc,  Au)
define_class(codegen,    Au)
define_class(code,       Au)

define_class(static_array, array)