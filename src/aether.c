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
        value, LLVMConstInt(lltype(au_lookup(stringify(i##b))), *(i##b*)l, 0))

#define uint_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(u##b)), \
        value, LLVMConstInt(lltype(au_lookup(stringify(u##b))), *(u##b*)l, 0))

#define f32_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        value, LLVMConstReal(lltype(au_lookup(stringify(f##b))), *(f##b*)l))

#define f64_value(b,l) \
    enode(mod, a, \
        literal, l, au, au_lookup(stringify(f##b)), \
        value, LLVMConstReal(lltype(au_lookup(stringify(f##b))), *(f##b*)l))

    
#define value(m,vr) enode(mod, a, value, vr, au, m->au)

typedef struct tokens_data {
    array tokens;
    num   cursor;
} tokens_data;

none bp() {
    return;
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


enode aether_e_assign(aether a, enode L, Au R, OPType op) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, null); // should never happen in an expression context

    int v_op = op;
    verify(op >= OPType__assign && op <= OPType__assign_left, "invalid assignment-operator");
    enode rL, rR = e_operand(a, R, null);
    enode res = rR;
    
    if (op != OPType__assign) {
        rL = e_load(a, L, null);
        res = value(L, 
            op_table[op - OPType__assign - 1].f_build_op
                (a->builder, rL->value, rR->value, e_str(OPType, op)->chars));
    }

    verify(is_ptr(L), "L-value not a pointer (cannot assign to value)");
    if (res->au != L->au->src) {
        res = e_operand(a, (Au)res, (etype)L);
        //res = e_convert(a, res, L->mdl);
    }

    LLVMBuildStore(a->builder, res->value, L->value);
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

LLVMTypeRef _lltype(Au a);
#define lltype(a) _lltype((Au)(a))

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
    if (optype >= OPType__assign && optype <= OPType__assign_left)
        return L;  // Assignment operations always return the type of the left operand
    else if (optype == OPType__value_default ||
             optype == OPType__cond_value    ||
             optype == OPType__or            ||
             optype == OPType__and           ||
             optype == OPType__xor) {
        if (is_bool(L) && is_bool(R))
            return elookup("bool");  // Logical operations on booleans return boolean
        // For bitwise operations, fall through to numeric promotion
    }

    // Numeric type promotion
    if (is_realistic(L) || is_realistic(R)) {
        // If either operand is float, result is float
        if (is_double(L) || is_double(R))
            return elookup("f64");
        else
            return elookup("f32");
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
                return e_fn_call(a, (enode)Lt, a(mL, RV));
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    enode Lnode = (enode)L;
    etype rtype = determine_rtype(a, optype, (etype)LV, (etype)RV); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too

    LLVMValueRef RES;
    LLVMTypeRef  LV_type = LLVMTypeOf(LV->value);
    LLVMTypeKind vkind = LLVMGetTypeKind(LV_type);

    enode LL = optype == OPType__assign ? LV : e_create(a, rtype, (Au)LV); // we dont need the 'load' in here, or convert even
    enode RL = e_create(a, rtype, (Au)RV);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = a->builder;
    Au literal = null;
    // if not storing ...
    if (optype == OPType__or || optype == OPType__and) {
        // ensure both operands are i1
        etype m_bool = elookup("bool");
        rtype = m_bool;
        LL = e_create(a, m_bool, (Au)LL); // generate compare != 0 if not already i1
        RL = e_create(a, m_bool, (Au)RL);
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
        return e_eq(a, LL, RL);
    else if (optype == OPType__compare)
        return e_cmp(a, LL, RL);
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
        mod,        a,
        au,         rtype->au,
        literal,    literal,
        value,      RES);
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
    if (a->no_build) return e_noop(a, elookup("bool"));

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
    enode cmp       = e_eq(a, value(L_ptr, phi), R_ptr);

    // Load the parent pointer (assuming it's the first member of the type struct)
    enode parent    = e_load(a, value(L_ptr, phi), null);

    // Check if parent is null
    enode is_null   = e_eq(a, parent, value(parent, LLVMConstNull(lltype(parent))));

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

    return value(elookup("bool"), result);
}


enode aether_e_eq(aether a, enode L, enode R) {
    a->is_const_op = false;
    etype bool_t = elookup("bool");

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

        enode cmp = e_fn_call(a, (enode)fn->user, a(L, R));

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

            enode sub = aether_e_eq(a, le, re);

            accum = LLVMBuildAnd(a->builder, accum, sub->value, "and");
        }

        return value(bool_t, accum);
    }

    // 5. Fallback for all primitives
    if (is_realistic(L))
        return enode(mod, a, au, bool_t->au, value,
            LLVMBuildFCmp(a->builder, LLVMRealOEQ,
                          L->value, R->value, "eq-f"));

    LLVMValueRef nv = LLVMBuildICmp(a->builder, LLVMIntEQ, L->value, R->value, "eq-i");
    return enode(mod, a, value, nv, au, bool_t->au);
}



enode aether_e_eq_prev(aether a, enode L, enode R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("bool"));

    if (is_prim(L) && is_prim(R)) {
        // needs reality check
        enode        cmp = e_cmp(a, L, R);
        LLVMValueRef z   = LLVMConstInt(LLVMInt32Type(), 0, 0);
        return value(elookup("bool"), LLVMBuildICmp(a->builder, LLVMIntEQ, cmp->value, z, "cmp-i"));
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
        enode res = value(elookup("bool"), LLVMConstInt(LLVMInt1Type(), is_eq, 0));
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
            enode eq = (enode)au_f->user;
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
            enode        cmp = e_eq(a, le, re);
            result = LLVMBuildAnd(a->builder, result, cmp->value, "eq-struct-and");
        }
        return value(elookup("bool"), result);

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
        return value(elookup("bool"), LLVMBuildFCmp(a->builder, LLVMRealOEQ, L->value, R->value, "eq-f"));
    
    return value(elookup("bool"), LLVMBuildICmp(a->builder, LLVMIntEQ, L->value, R->value, "eq-i"));
}

static void print_all(aether mod, symbol label, array list);

enode aether_e_eval(aether a, string value) {
    array t = (array)tokens(target, (Au)a, parser, a->parse_f, input, (Au)value);
    push_tokens(a, (tokens)t, 0);
    print_all(a, "these are tokens", t);
    enode n = (enode)a->parse_expr((Au)a, null, null); 
    enode s = e_create(a, elookup("string"), (Au)n);
    pop_tokens(a, false);
    return s;
}


enode aether_e_interpolate(aether a, string str) {
    enode accum = null;
    array sp    = split_parts(str);
    etype mdl   = elookup("string");

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, mdl);

    each (sp, ipart, s) {
        enode val = e_create(a, mdl, s->is_expr ?
            (Au)e_eval(a, s->content) : (Au)s->content);
        accum = accum ? e_add(a, (Au)accum, (Au)val) : val;
    }
    return accum;
}

static LLVMValueRef const_cstr(aether a, cstr value, i32 len) {
    LLVMContextRef ctx = LLVMGetGlobalContext();

    // 1. make a constant array from the raw bytes (null-terminated!)
    LLVMValueRef strConst = LLVMConstString(value, len, /*DontNullTerminate=*/0);

    // 2. create a global variable of that array type
    LLVMValueRef gv = LLVMAddGlobal(a->module, LLVMTypeOf(strConst), "const_cstr");
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
         if (instanceof(op, enode)) return (enode)op;
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
        return enode(mod, a, value, const_cstr(a, (cstr)op, strlen((symbol)op)), au, au_lookup("symbol"), literal, op);
    }
    else if (instanceof(op, string)) { // this works for const_string too
        string str = string(((string)op)->chars);
        return enode(mod, a, value, const_cstr(a, str->chars, str->count), au, au_lookup("symbol"), literal, op);
    }
    error("unsupported type in aether_operand %s", t->ident);
    return NULL;
}


enode aether_e_operand(aether a, Au op, etype src_model) {
    if (!op) {
        if (is_ptr(src_model))
            return e_null(a, src_model);
        return enode(mod, a, au, au_lookup("none"), value, null);
    }
    
    if (isa(op) == typeid(string))
        return e_create(a, src_model,
            (Au)e_interpolate(a, (string)op));
    
    if (isa(op) == typeid(const_string))
        return e_operand_primitive(a, op); //e_create(a, src_model, (Au)e_operand_primitive(a, op));
    
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
    if (!mdl) mdl = elookup("handle");
    if (!is_ptr(mdl) && mdl->au->is_class) {
        mdl = etype_ptr(a, mdl->au); // we may only do this on a limited set of types, such a struct types
    }
    if (!is_ptr(mdl)) {
        mdl = mdl;
    }
    verify(is_ptr(mdl), "%o not compatible with null value", mdl);
    return enode(mod, a, value, LLVMConstNull(lltype(mdl)), au, mdl->au);
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
    
    LLVMTypeRef arrTy = LLVMArrayType(lltype(atype_vector), ln);
    LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = meta->origin[i];
        enode n;
        if (instanceof(m, etype))
            n = e_typeid(a, (etype)m);
        else if (instanceof(m, shape)) {
            shape s = (shape)m;
            array ar = array(alloc, s->count);
            push_vdata(ar, (Au)s->data, s->count);
            n = e_create(a, elookup("shape"),
                (Au)m("count", s->count,
                  "data",  (e_const_array(a, elookup("i64"), ar))));
        } else {
            verify(false, "unsupported design-time meta type");
        }
        elems[i] = n->value;
    }

    LLVMValueRef arr_init = LLVMConstArray(lltype(atype_vector), elems, ln);
    free(elems);

    static int ident = 0;
    char gname[32];
    sprintf(gname, "meta_ids_%i", ident++);
    LLVMValueRef G = LLVMAddGlobal(a->module, arrTy, gname);
    LLVMSetLinkage(G, LLVMInternalLinkage);
    LLVMSetGlobalConstant(G, 1);
    LLVMSetInitializer(G, arr_init);

    enode arr_node = enode(mod, a, value, G, au, atype);
    return e_addr_of(a, arr_node, atype->user);
}

enode aether_e_not_eq(aether a, enode L, enode R) {
    return e_not(a, e_eq(a, L, R));
}

none aether_e_fn_return(aether a, Au o) {
    Au_t au_ctx = find_context(a->lexical, AU_MEMBER_FUNC, 0);
    verify (au_ctx, "function not found in context");
    etype f = au_ctx->user;

    a->is_const_op = false;
    if (a->no_build) return;

    verify (!f->au->has_return, "function %o already has return", f);
    f->au->has_return = true;

    if (is_void(f->au->rtype)) {
        LLVMBuildRetVoid(a->builder);
        return;
    }
    
    enode conv = e_create(a, (etype)f->au->rtype, o);
    LLVMBuildRet(a->builder, conv->value);
}

etype formatter_type(aether a, cstr input) {
    cstr p = input;
    // skip flags/width/precision
    while (*p && strchr("-+ #0123456789.", *p)) p++;
    if (strncmp(p, "lld", 3) == 0 || 
        strncmp(p, "lli", 3) == 0) return elookup("i64");
    if (strncmp(p, "llu", 3) == 0) return elookup("u64");
    
    switch (*p) {
        case 'd': case 'i':
                  return elookup("i32"); 
        case 'u': return elookup("u32");
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                  return elookup("f64");
        case 's': return elookup("symbol");
        case 'p': return elookup("symbol")->au->ptr->user;
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
    if (!t || t->au->is_pointer) return t;
    return etype_ptr(t->mod, t->au);
}

enode aether_e_is(aether a, enode L, Au R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("bool"));

    enode L_type =  e_offset(a, L, _i64(-sizeof(struct _Au)));
    enode L_ptr  =    e_load(a, L_type, null); /// must override the mdl type to a ptr, but offset should not perform a unit translation but rather byte-based
    enode R_ptr  = e_operand(a, R, null);
    return aether_e_eq(a, L_ptr, R_ptr);
}

void aether_e_print_node(aether a, enode n) {
    if (a->no_build) return;
    /// would be nice to have a lookup method on import
    /// call it im_lookup or something so we can tell its different
    enode   printf_fn  = (enode)elookup("printf");
    etype   mdl_cstr   = elookup("cstr");
    etype   mdl_symbol = elookup("symbol");
    etype   mdl_i32    = elookup("i32");
    etype   mdl_i64    = elookup("i64");
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

enode aether_e_fn_call(aether a, enode fn, array args) {
    // we could support an array or map arg here, for args
    // we set this when we do something complex
    if (!fn->used)
         fn->used = true;
    
    a->is_const_op = false; 
    if (a->no_build) return e_noop(a, fn->au->rtype->user);

    etype_implement((etype)fn);
    etype target_type = (etype)fn->target;
    enode f = len(args) ? (enode)get(args, 0) : null;
    verify(!target_type || canonical(target_type) == canonical(f),
        "target mismatch");

    int n_args = args ? len(args) : 0;
    verify(n_args == fn->au->args.count ||
        (n_args > fn->au->args.count && fn->au->is_vargs),
        "arg count mismatch");
    LLVMValueRef* arg_values = calloc(n_args, sizeof(LLVMValueRef));
    LLVMTypeRef  F = lltype(fn);
    LLVMValueRef V = fn->value;

    int index = 0;
    if (target_type) {
        // we do not 'need' target so we dont have it.
        // we get instance from the fact that its within a class/struct context and is_imethod
        etype cast_to = ensure_pointer(target_type);
        arg_values[index++] = e_create(a, cast_to, (Au)f)->value;
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
            if (target_type && f) {
                i++;
                continue;
            }
            Au_t arg_type = (Au_t)array_get((array)&fn->au->args, i);
            evar arg = (evar)arg_type->user;

            enode n   = (enode)instanceof(arg_value, enode);
            if (index == fmt_idx) {
                Au fmt = n ? (Au)instanceof(n->literal, const_string) : null;
                verify(fmt, "formatter functions require literal, constant strings");
            }
            // this takes in literals and enodes
            Au_t arg_value_isa = isa(arg_value);
            enode nn = (enode)arg_value;
            enode conv = e_create(a, arg_type->src->user, arg_value);
            LLVMValueRef vr = arg_values[index] = conv->value;
            i++;
            index++;
        }
    }
    int istart = index;
    
    if (fmt_idx >= 0) {
        Au_t src_type = isa(args->origin[fmt_idx]);
        enode  fmt_node = (enode) instanceof(args->origin[fmt_idx], enode);
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
    LLVMValueRef R = LLVMBuildCall2(a->builder, F, V, arg_values, index, is_void_ ? "" : "call");
    free(arg_values);
    return enode(mod, a, au, fn->au->rtype, value, R); //value(fn->au->rtype, R);
}

static bool is_same(Au a, Au b) {
    Au_t au_a = au_arg(a);
    Au_t au_b = au_arg(b);
    return resolve(au_a->user) == resolve(au_b->user) && 
        ref_level((Au)au_a) == ref_level((Au)au_b);
}

static enode castable(etype fr, etype to) { 
    bool fr_ptr = is_ptr(fr);
    if ((fr_ptr || is_prim(fr)) && is_bool(to))
        return (enode)true;
    
    /// compatible by match, or with basic integral/real types
    if ((fr == to) ||
        ((is_realistic(fr) && is_realistic(to)) ||
         (is_integral (fr) && is_integral (to))))
        return (enode)true;
    
    /// primitives may be converted to Au-type Au
    if (is_prim(fr) && is_generic(to))
        return (enode)true;

    /// check cast methods on from
    for (int i = 0; i < fr->au->members.count; i++) {
        Au_t mem = (Au_t)fr->au->members.origin[i];
        if (mem->member_type == AU_MEMBER_CAST && is_same((Au)mem->rtype, (Au)to->au))
            return (enode)mem->user;
    }
    return (enode)false;
}


static enode constructable(etype fr, etype to) {
    if (fr == to)
        return (enode)true;
    aether a = fr->mod;

    for (int ii = 0; ii < to->au->members.count; ii++) {
        Au_t mem = (Au_t)to->au->members.origin[ii];
        etype fn = mem->member_type == AU_MEMBER_CONSTRUCT ? mem->user : null;
        if  (!fn) continue;
        verify(fn->au->args.count == 2, "unexpected argument count for constructor"); // target + with-type
        Au_t with_arg = (Au_t)fn->au->args.origin[1];
        if (with_arg->src == fr->au)
            return (enode)mem->user;
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
static enode convertible(etype fr, etype to) {
    aether a = fr->mod;
    etype  ma = canonical(fr);
    etype  mb = canonical(to);

    // todo: ref depth must be the same too
    if (resolve(ma) == resolve(mb) && ref_level((Au)ma) == ref_level((Au)mb)) return (enode)true;

    if (ma->au->is_schema && mb->au == typeid(Au_t)) return (enode)true;
    if (mb->au->is_schema && ma->au == typeid(Au_t)) return (enode)true;

    // more robust conversion is, they are both pointer and not user-created
    if (ma->au->is_system || mb->au == typeid(handle))
        return (enode)true;

    if (ma == mb)
        return (enode)true;

    if (is_prim(ma) && is_prim(mb))
        return (enode)true;

    if (is_rec(ma) || is_rec(mb)) {
        Au_t au_type = au_lookup("Au_t");
        if (is_ptr(ma) && ma->au->src == au_type && mb->au == au_type)
            return (enode)true;
        if (is_subclass((Au)ma, (Au)mb))
            return (enode)true;
        enode mcast = castable(ma, mb);
        return mcast ? mcast : constructable(ma, mb);
    } else {
        // the following check should be made redundant by the code below it
        etype sym = elookup("symbol");
        etype ri8 = elookup("ref_i8");

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
    if (is_ptr(e)) return true;
    if (e->value && LLVMIsAGlobalValue(e->value)) return true;
    return false;
}

enode enode_access(enode target, string name) {
    aether a = target->mod;
    Au_t rel = target->au->member_type == AU_MEMBER_VAR ? target->au->src : target->au;
    Au_t   m = find_member(rel, name->chars, 0, true);
    if (!m) {
        m = m;
    }
    verify(m, "failed to find member %o on type %o", name, rel);
    
    verify(is_addressable(target), 
        "expected target pointer for member access");

    etype t = resolve(target); // resolves to first type
    verify(t->au->is_struct || t->au->is_class, 
        "expected resolution to struct or class");

    // for functions, we return directly with target passed along
    if (is_func((Au)m))
        return enode(mod, a, au, m, target, target);
    
    if (instanceof(m->user, enode))
        return (enode)m->user; //((enode)m->user)->value; // this should apply to enums

    // signal we're doing something non-const
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, m->user);

    Au_t ptr = pointer(a, (Au)m)->au;

    return enode(
        mod,    a,
        au,     ptr,
        value,  LLVMBuildStructGEP2(
            a->builder, lltype(t), target->value,
            m->index, "enode_access"));
}

bool is_au_compatible(etype t) {
    if (!t) return false;
    aether a = t->mod;
    return is_au_type(t) || a->au == t->au->module || t->mod->au == t->au;
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
        return e_null(a, elookup("Au_t"));
    
    if (!is_au_compatible(mdl)) {
        int test2 = 2;
        test2    += 2;
    }
    verify(is_au_compatible(mdl), "typeid not available on external types");

    // we go from enode sometimes, which is NOT 
    // the base etype (its not brilliant to require etype be unique here but we circle back)

    Au info = head(mdl);
    a->is_const_op = false;

    enode n = resolve_typeid((Au)mdl);
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

/// create is both stack and heap allocation (based on etype->is_ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_e_create(aether a, etype t, Au args) {
    if (!t) {
        verify(instanceof(args, enode), "aether_e_create");
        return (enode)args;
    }

    string  str  = (string)instanceof(args, string);
    map     imap = (map)instanceof(args, map);
    array   ar   = (array)instanceof(args, array);
    static_array static_a = (static_array)instanceof(args, static_array);
    enode   n    = null;
    enode ctr  = null;

    // we never 'create' anything of type identity with e_create; thats too much scope to service
    // for type -> Au_t conversion and Au_t -> type, we simply cast.
    // we probably want to fault in all other cases
    if (is_au_t((Au)t) && is_au_t(args)) {
        enode n = instanceof(args, enode);
        return value(t,
            LLVMBuildBitCast(a->builder, n->value, lltype(t), "is_au_t_cast"));
    }

    if (!args) {
        if (is_ptr(t))
            return e_null(a, t);
    }

    static int seq = 0;
    seq += 1;
    if (seq == 25) {
        seq = seq;
    }

    // construct / cast methods
    Au_t args_type = isa(args);
    enode input = (enode)instanceof(args, enode);

    if (!input && instanceof(args, const_string)) {
        input = e_operand(a, args, elookup("string"));
        args  = (Au)input;
    }

    if (!input && instanceof(args, string)) {
        input = e_operand(a, args, elookup("string"));
        args  = (Au)input;
    }

    if (input) {
        verify(!imap, "unexpected data");
        
        // if both are internally created and these are refs, we can allow conversion
        
        static int seq = 0;
        seq++;
        if (seq == 16) {
            seq = seq;
        }
        enode fmem = convertible((etype)input, t);

        verify(fmem, "no suitable conversion found for %o -> %o (%i)",
            input, t, seq);
        
        if (fmem == (void*)true) {
            LLVMTypeRef typ = LLVMTypeOf(input->value);
            LLVMTypeKind k = LLVMGetTypeKind(typ);

            // check if these are either Au_t class typeids, or actual compatible instances
            if (k == LLVMPointerTypeKind) {
                a->is_const_op = false;
                if (a->no_build) return e_noop(a, t);

                etype src = canonical(input);
                etype dst = canonical(t);
                bool bit_cast = src == dst;
                if (!bit_cast) {
                    if (src->au->is_schema && dst->au == typeid(Au_t))
                        bit_cast = true;
                    else if (dst->au->is_schema && src->au == typeid(Au_t))
                        bit_cast = true;
                    else if (!is_subclass((Au)input, (Au)t)) {
                        int r0 = ref_level((Au)input);
                        int r1 = ref_level((Au)t);
                        if (r0 == r1 && resolve(src) == resolve(dst))
                            bit_cast = true;
                        else {
                            char *s = LLVMPrintTypeToString(lltype(t));
                            print("LLVM type: %s", s);
                            bool check = (is_prim(src) && is_prim(dst)) ||
                                etype_inherits((etype)input, t);
                            
                            if (!check) {
                                check = check;
                            }

                            if (is_rec(src) || is_rec(dst)) {
                                Au_t au_type = au_lookup("Au_t");
                                if (is_ptr(src) && src->au->src == au_type && dst->au == au_type)
                                    check = true;
                            }

                            verify(check, "models not compatible: %o -> %o",
                                    input, t);
                        }
                    }
                }
                return value(t,
                    LLVMBuildBitCast(a->builder, input->value, lltype(t), "class_ref_cast"));
            }

            // fallback to primitive conversion rules
            return aether_e_primitive_convert(a, input, t);
        }
        
        // primitive-based conversion goes here
        enode fn = (enode)instanceof(fmem, enode);
        if (fn && fn->au->member_type == AU_MEMBER_CONSTRUCT) {
            // ctr: call before init
            // this also means the mdl is not a primitive
            //verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = fmem;
        } else if (fn && fn->au->member_type == AU_MEMBER_CAST) {
            // we may call cast straight away, no need for init (which the cast does)
            return e_fn_call(a, fn, a(input));
        } else
            fault("unknown error");
        
    }

    // handle primitives after cast checks -- the remaining objects are object-based
    // note that enumerable values are primitives
    if (is_prim(t))
        return e_operand(a, args, t);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, t);

    etype        cmdl         = etype_traits((Au)t, AU_TRAIT_CLASS);
    etype        smdl         = etype_traits((Au)t, AU_TRAIT_STRUCT);
    Au_t         Au_type      = au_lookup("Au");
    etype        f_alloc      = find_member(Au_type, "alloc_new", AU_MEMBER_FUNC, false)->user;
    etype        f_initialize = find_member(Au_type, "initialize", AU_MEMBER_FUNC, false)->user;
    enode        res;

    if (is_ptr(t) && is_struct(t->au->src) && static_a) {
        static int ident = 0;
        char name[32];
        sprintf(name, "static_arr_%i", ident++);

        i64          ln     = len(static_a);
        etype        emdl   = (etype)t->au->src->user;
        LLVMTypeRef  arrTy  = LLVMArrayType(lltype(emdl), ln);
        LLVMValueRef G      = LLVMAddGlobal(a->module, arrTy, name);
        LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));
        LLVMSetLinkage(G, LLVMInternalLinkage);

        for (i32 i = 0; i < ln; i++) {
            enode n = e_operand(a, static_a->origin[i], null);
            verify (LLVMIsConstant(n->value), "static_array must contain constant statements");
            verify (n->au == t->au->src, "type mismatch");
            elems[i] = n->value;
        }
        LLVMValueRef init = LLVMConstArray(lltype(emdl), elems, ln);
        LLVMSetInitializer(G, init);
        LLVMSetGlobalConstant(G, 1);
        free(elems);
        res = enode(mod, a, value, G, au, t->au);

    } else if (!ctr && cmdl) {
        // we have to call array with an intialization property for size, and data pointer
        // if the data is pre-defined in init and using primitives, it has to be stored prior to this call
        enode metas_node = e_meta_ids(a, t->meta);
        
        res = e_fn_call(a, (enode)f_alloc, a( e_typeid(a, t), _i32(1), metas_node ));
        res->au = t->au; // we need a general cast method that does not call function

        bool is_array = cmdl && cmdl->au->context == au_lookup("array");
        if (imap) {
            
            pairs(imap, i) {
                string k = (string)i->key;

                enode i_prop  = enode_access(res, k);
                enode i_value = e_operand(a, i->value, resolve(i_prop));
                
                e_assign(a, i_prop, (Au)i_value, OPType__assign);
            }

            // this is a static method, with a target of sort, but its not a real target since its not a real instance method
            e_fn_call(a, (enode)f_initialize, a(res)); // required logic need not emit ops to set the bits when we can check at design time
            
        } else if (ar) { // if array is given for args

            // if we are CREATING an array
            if (is_array) {
                int   ln = len(ar);
                enode n  = e_operand(a, _i64(ln), elookup("i64"));
                bool all_const = ln > 0;
                enode const_array = null;
                for (int i = 0; i < ln; i++) {
                    enode node = (enode)instanceof(ar->origin[i], enode);
                    if (node && node->literal)
                        continue;
                    all_const = false;
                    break;
                }
                if (all_const) {
                    etype ptr = pointer(a, (Au)t);
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
                    sprintf(vname, "const_arr_%i", ident++);
                    LLVMValueRef glob = LLVMAddGlobal(a->module, LLVMTypeOf(const_arr), vname);
                    LLVMSetLinkage(glob, LLVMInternalLinkage);
                    LLVMSetGlobalConstant(glob, 1);
                    LLVMSetInitializer(glob, const_arr);
                    const_array = enode(mod, a, au, ptr->au, value, glob);
                }
                enode prop_alloc     = enode_access(res, string("alloc"));
                enode prop_unmanaged = enode_access(res, string("unmanaged"));
                e_assign(a, prop_alloc, (Au)n, OPType__assign);
                if (const_array) {
                    enode tru = e_operand(a, _bool(ln), elookup("bool"));
                    e_assign(a, prop_unmanaged, (Au)tru, OPType__assign);
                }     
                e_fn_call(a, (enode)f_initialize, a(res));
                if (const_array) {
                    etype f_push_vdata = find_member(t->au, "push_vdata", AU_MEMBER_FUNC, true)->user;
                    e_fn_call(a, (enode)f_push_vdata, a(res, const_array, n));
                } else {
                    etype f_push = find_member(t->au, "push", AU_MEMBER_FUNC, true)->user;
                    for (int i = 0; i < ln; i++) {
                        Au      aa = ar->origin[i];
                        enode   n = e_operand(a, aa, t->au->src->user);
                        e_fn_call(a, (enode)f_push, a(res, n));
                    }
                }
            } else {
                fault("unsupported instantiation method");
                /*
                // if array given and we are not making an array, this will create fields
                // theres no silver exposure for this, though -- not unless we adopt braces
                // why though, lets just make 1 mention of type and have a language that is unambiguous and easier to read
                array f = field_list(mdl, elookup("Au"), false);
                for (int i = 0, ln = len(a); i < ln; i++) {
                    verify(i < len(f), "too many fields provided for object %o", mdl);
                    emember m = f->origin[i];
                    enode   n = e_operand(a, a->origin[i], m->mdl);
                    emember rmem = member_lookup((emember)res, m->name);
                    e_assign(a, res, rmem, OPType__assign);
                }
                e_fn_call(a, f_initialize, res, a());*/
            }
        } else {
            if (ctr) {
                e_fn_call(a, (enode)ctr, a(res, input));
                e_fn_call(a, (enode)f_initialize, a(res));
            } else {
                verify(false, "expected constructor for type %o", t);
            }
        }
    } else if (ctr) {
        verify(is_rec(t), "expected record");
        enode metas_node = e_meta_ids(a, t->meta);
        
        enode alloc = e_fn_call(a, (enode)f_alloc, a( e_typeid(a, t), _i32(1), metas_node ));
        alloc->au = t->au; // we need a general cast method that does not call function
        res = e_fn_call(a, (enode)ctr, a(alloc, input));
    } else {
        array   is_arr        = (array)instanceof(args, array);
        bool    is_ref_struct = is_ptr(t) && is_struct(resolve(t));

        verify(!is_arr, "no translation for array to etype %o", t);

        if (is_struct(t) || is_ref_struct) {
            verify(!is_arr, "unexpected array argument");

            if (imap) {
                // check if constants are in map, and order fields
                etype rmdl = is_ref_struct ? resolve(t) : t;
                int field_count = LLVMCountStructElementTypes(lltype(rmdl));
                LLVMValueRef *fields = calloc(field_count, sizeof(LLVMValueRef));
                array field_indices = array(field_count);
                array field_names   = array(field_count);
                array field_values  = array(field_count);
                bool  all_const     = a->no_const ? false : true;

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
                    for (int ri = 0; ri < rmdl->au->members.count; ri++) {
                        Au_t smem = (Au_t)rmdl->au->members.origin[ri];
                        if (smem->index == i) {
                            field_type = smem->user;
                            break;
                        }
                    } // string and cstr strike again -- it should be calling the constructor on string for this, which has been enumerated by Au-type already
                    verify(field_type, "field type lookup failed for %o (index = %i)", t, i);

                    LLVMTypeRef tr = LLVMStructGetTypeAtIndex(lltype(rmdl), i);
                    LLVMTypeRef expect_ty = lltype(field_type);
                    verify(expect_ty == tr, "field type mismatch");
                    if (!value) value = e_null(a, field_type);
                    fields[i] = value->value;
                }

                if (all_const) {
                    print("all are const, writing %i fields for %o", field_count, t);
                    LLVMValueRef s_const = LLVMConstNamedStruct(lltype(t), fields, field_count);
                    res = enode(mod, a, value, s_const, au, t->au);
                } else {
                    print("non-const, writing build instructions, %i fields for %o", field_count, t);
                    res = enode(mod, a, value, LLVMBuildAlloca(a->builder, lltype(t), "alloca-mdl"), au, t->au);
                    res = e_zero(a, res);
                    for (int i = 0; i < field_count; i++) {
                        if (!LLVMIsNull(fields[i])) {
                            LLVMValueRef gep = LLVMBuildStructGEP2(a->builder, lltype(t), res->value, i, "");
                            LLVMBuildStore(a->builder, fields[i], gep);
                        }
                    }
                }

                free(fields);

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

            } else if (ctr) {
                e_fn_call(a, ctr, a(res, input));
            }
        } else 
            res = e_operand(a, args, t);
    }
    return res;
}


enode aether_e_const_array(aether a, etype mdl, array arg) {
    etype atype = elookup("Au_t");
    etype vector_type = etype_ptr(a, mdl->au);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, vector_type);

    if (!arg || !len(arg))
        return e_null(a, vector_type);

    i32 ln = len(arg);
    
    LLVMTypeRef arrTy = LLVMArrayType(lltype(vector_type), ln);
    LLVMValueRef *elems = calloc(ln, sizeof(LLVMValueRef));

    for (i32 i = 0; i < ln; i++) {
        Au m = arg->origin[i];
        enode n = e_operand(a, m, mdl);
        elems[i] = n->value;  // each is an Au_t*
    }

    LLVMValueRef arr_init = LLVMConstArray(lltype(vector_type), elems, ln);
    free(elems);

    static int ident = 0;
    char gname[32];
    sprintf(gname, "static_array_%i", ident++);
    LLVMValueRef G = LLVMAddGlobal(a->module, arrTy, gname);
    LLVMSetLinkage(G, LLVMInternalLinkage);
    LLVMSetGlobalConstant(G, 1);
    LLVMSetInitializer(G, arr_init);
    return enode(mod, a, value, G, au, mdl->au);
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
    return enode(mod, mod, au, rmdl->au, value, phi_node);
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
    LLVMValueRef cond_val = e_create(a, elookup("bool"), (Au)cond_res)->value;
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
    LLVMValueRef condition = e_create(a, elookup("bool"), (Au)cond_result)->value;

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
    return enode(mod, a, au, mdl ? mdl->au : au_lookup("none"));
}

enode aether_e_if_else(
    aether a,
    array conds,
    array exprs,
    subprocedure cond_builder,
    subprocedure expr_builder)
{
    int ln_conds = len(conds);

    verify(
        ln_conds == len(exprs) - 1 ||
        ln_conds == len(exprs),
        "mismatch between conditions and expressions");

    // Current insertion block + parent function
    LLVMBasicBlockRef block = LLVMGetInsertBlock(a->builder);
    LLVMValueRef pb = LLVMGetBasicBlockParent(block);

    // Merge block where everything joins back up
    LLVMBasicBlockRef merge = LLVMAppendBasicBlock(pb, "ifcont");

    // Iterate over the conditions and expressions
    for (int i = 0; i < ln_conds; i++) {
        // Create the blocks for "then" and "else" (on the FUNCTION)
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(pb, "then");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(pb, "else");

        // Build the condition
        Au cond_obj      = conds->origin[i];
        enode cond_node  = (enode)invoke(cond_builder, cond_obj);
        LLVMValueRef condition =
            e_create(a, elookup("bool"), (Au)cond_node)->value;

        // Conditional branch to then/else
        LLVMBuildCondBr(a->builder, condition, then_block, else_block);

        // ----- THEN BLOCK -----
        LLVMPositionBuilderAtEnd(a->builder, then_block);

        Au expr_obj = exprs->origin[i];
        invoke(expr_builder, (Au)expr_obj);

        // Jump to merge
        LLVMBuildBr(a->builder, merge);

        // Continue emitting from the else side
        LLVMPositionBuilderAtEnd(a->builder, else_block);
        block = else_block;
    }

    // ----- FINAL ELSE (optional) -----
    if (len(exprs) > len(conds)) {
        Au else_expr = exprs->origin[len(conds)];
        invoke(expr_builder, else_expr);
        LLVMBuildBr(a->builder, merge);
    }

    // Move to merge block
    LLVMPositionBuilderAtEnd(a->builder, merge);

    return enode(
        mod,
        a,
        au, au_lookup("none"),
        value, null);
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
    return enode(mod, a, au, n->au, value, ptr_offset);
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
    
    LLVMValueRef loaded = LLVMBuildLoad2(
        a->builder, lltype(resolve), mem->value, "load");
    enode r = enode(mod, a, value, loaded, au, resolve->au);
    return r;
}

/// general signed/unsigned/1-64bit and float/double conversion
/// should NOT be loading, should absolutely be calling model_convertible -- why is it not?
enode aether_e_primitive_convert(aether a, enode expr, etype rtype) {
    if (!rtype) return expr;

    a->is_const_op &= expr->au == rtype->au; // we may allow from-bit-width <= to-bit-width
    if (a->no_build) return e_noop(a, rtype);

    //expr = e_load(a, expr, null); // i think we want to place this somewhere else for better structural use
    
    etype        F = canonical(expr);
    etype        T = canonical(rtype);
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
    else
        fault("unsupported cast");

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
        if (is_func((Au)m) || mt == AU_MEMBER_ENUMV || mt == AU_MEMBER_VAR || mt == AU_MEMBER_IS_ATTR)
            m->user = (etype)enode(mod, a, au, m);
        else
            m->user = etype(mod, a, au, m);
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


static void build_module_initializer(aether a, etype module_init_fn) {
    Au_t module_base = a->au;

    push_scope(a, (Au)module_init_fn);

    members(module_base, au)
        if (au->member_type == AU_MEMBER_VAR && au->user->body)
            build_initializer(a, au->user);

    e_fn_return(a, null);
    pop_scope(a);
}

static void set_global_construct(aether a, enode fn) {
    LLVMModuleRef mod = a->module;

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

static void build_entrypoint(aether a, etype module_init_fn) {
    Au_t module_base = a->au;

    etype main_spec  = elookup("main");
    verify(main_spec && is_class(main_spec), "expected main class");

    etype main_class = null;
    members(module_base, mem) {
        if (is_class(mem)) {
            etype cls = mem->user;
            if (cls->au->context == main_spec->au) {
                verify(!main_class, "found multiple main classes");
                main_class = cls;
            }
        }
    }

    // no main for library
    if (!main_class) {
        set_global_construct(a, (enode)module_init_fn);
        a->is_library = true;
        return;
    }

    // for apps, build int main(argc, argv)
    Au_t au_f_main = def_member(a->au, "main", au_lookup("i32"),   AU_MEMBER_FUNC, 0);
    Au_t argc = def_member(au_f_main,  "argc", au_lookup("i32"),   AU_MEMBER_VAR,  0);
    Au_t argv = def_member(au_f_main,  "argv", au_lookup("cstrs"), AU_MEMBER_VAR,  0);
    array_qpush((array)&au_f_main->args, (Au)argc);
    array_qpush((array)&au_f_main->args, (Au)argv);

    etype main_fn = etype(mod, a, au, au_f_main);
    push_scope(a, (Au)main_fn);
    e_fn_call(a, (enode)module_init_fn, null);
    enode Au_engage = (enode)find_member(au_lookup("Au"), "engage", AU_MEMBER_FUNC, false)->user; // this is only for setting up logging now, and we should likely emit it after the global construction, and before the user initializers
    e_fn_call(a, (enode)Au_engage, a(argv));
    e_create(a, main_class, (Au)map()); // a loop would be nice, since none of our members will be ref+1'd yet
    e_fn_return(a, _i32(0));

    pop_scope(a);
    etype_implement(main_fn);
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

// this is the declare (this comment stays)
none etype_init(etype t) {
    if (t->mod == null) t->mod = (aether)instanceof(t, aether);
    aether a = t->mod; // silver's mod will be a delegate to aether, not inherited

    if (t->au && t->au->ident && strstr(t->au->ident, "action")) {
        t = t;
    }
    if (!t->au) {
        // we must also unregister this during watch cycles, upon reinit
        t->au = def(a->au, null, AU_MEMBER_NAMESPACE, 0);
    }

    Au_t    au  = t->au;
    bool  named = au && au->ident && strlen(au->ident);

    if (isa(t) == typeid(aether) || isa(t)->context == typeid(aether)) {
        a = (aether)t;
        verify(a->source && len(a->source), "no source provided");
        a->name = a->name ? a->name : stem(a->source);
        au = t->au = def_module(a->name->chars);
        if (!au->user) au->user = hold(t);
        return;
    } else if (t->is_schema) {
        au = t->au = def(top_scope(a), fmt("__%s_t", au->ident)->chars,
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

    if (!au->user)
        au->user = hold(t);

    Au_t_f* au_t = (Au_t_f*)isa(au);

    if (is_func((Au)au)) {
        etype fn = au->user;
        // this is when we are binding to .c modules that contain methods

        // populate arg types for function
        int n_args = fn->au->args.count;
        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));
        int index  = 0;
        arg_types(fn->au, t)
            arg_types[index++] = lltype(t);

        au->lltype      = LLVMFunctionType(
            au->rtype ? lltype(au->rtype) : LLVMVoidType(),
            arg_types, au->args.count, au->is_vargs);

        etype_ptr(a, fn->au);
        free(arg_types);

    } else if (is_func_ptr((Au)au)) {
        etype fn = au->user;

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
        au->lltype = LLVMPointerType(au->src->lltype, 0);
    } else if (named && (is_rec((Au)t) || au->is_union || au == typeid(Au_t))) {
        au->lltype = LLVMStructCreateNamed(a->module_ctx, strdup(au->ident));
        if (au != typeid(Au_t))
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
        if (au->member_type == AU_MEMBER_FUNC) {
            int          is_inst   = au->is_imethod;
            int          arg_count = is_inst + au->args.count;
            LLVMTypeRef* arg_types = calloc(arg_count, sizeof(LLVMTypeRef));

            if (is_inst)
                arg_types[0] = lltype(au->context);
            for (int i = is_inst; i < arg_count; i++)
                arg_types[i] = lltype(au->args.origin[i - is_inst]);

            au->lltype = LLVMFunctionType(
                lltype(au->rtype), arg_types, arg_count, 0);
            free(arg_types);
        }
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

etype get_type_id(etype t) {
    if (!is_au_type(t)) {
        t = t;
    }
    aether a = t->mod;
    // we must support a use-case where aether calls this on itsself.  a module must have its own identity in space!
    // we do not abstract away identity
    verify(is_au_type(t) || a->au == t->au->module || t->mod->au == t->au,
        "typeid not available on external types");
    
    printf("creating schema for %s\n", t->au->ident);

    Au_t au = t->au;

    // schema must have it's static_value set to the instance
    etype schema = etype(mod, a, au, t->au, is_schema, true);
    t->schema = schema;
    
    etype mt = etype_ptr(a, schema->au);
    mt->au->is_typeid = true;
    mt->au->is_imported = a->is_Au_import;
    
    Au_t au_type = au_lookup("Au");
    verify(au_type->ptr, "expected ptr type on Au");

    // we need to create the %o_i instance inline struct
    string n = f(string, "%s_info", au->ident);
    Au_t type_info = def_type  (a->au, n->chars,
        AU_TRAIT_STRUCT | AU_TRAIT_SYSTEM);
    Au_t type_h    = def_member(type_info, "info", au_type,
        AU_MEMBER_VAR, AU_TRAIT_SYSTEM);
    Au_t type_f    = def_member(type_info, "type", schema->au,
        AU_MEMBER_VAR, AU_TRAIT_SYSTEM | AU_TRAIT_INLAY);
    type_info->user = etype(mod, a, au, type_info);
    etype_implement(type_info->user);
    string name = f(string, "%s_i", au->ident);
    evar schema_i = evar(mod, a, au, def_member(
                a->au, name->chars, type_info, AU_MEMBER_VAR,
                AU_TRAIT_SYSTEM | (a->is_Au_import ? AU_TRAIT_IS_IMPORTED : 0)));
    etype_implement((etype)schema_i);
    printf("user ptr = %p\n", t->au->user);
    t->au->user->type_id = access(schema_i, string("type"));
    if (!a->is_Au_import) {
        set(a->user_type_ids, (Au)t, (Au)t->au->user->type_id);
    }
    return mt;
}

none etype_implement(etype t) {
    Au_t    au = t->au;
    aether a  = t->mod;

    if (is_func(t->au) && !((enode)t)->used)
        return;

    if (au->is_implemented) return;
    au->is_implemented = true;

    bool is_ARef = au == typeid(ARef);
    if (!is_ARef && au && au->src && au->src->user && !is_prim(au->src) && isa(au->src->user) == typeid(etype))
        if (au->src != typeid(Au_t))
            etype_implement(au->src->user);

    if (au->member_type == AU_MEMBER_VAR) {
        enode n = (enode)instanceof(t, enode);
        verify(n, "expected enode instance for AU_MEMBER_VAR");

        // if module member, then its a global value
        if (!au->context->context) {
            // mod->is_Au_import indicates this is coming from a lib, or we are making a new global
            LLVMTypeRef ty = lltype(au->src);
            LLVMValueRef G = LLVMAddGlobal(a->module, ty, au->ident);
            LLVMSetLinkage(G, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
            if (!a->is_Au_import) {
                LLVMSetInitializer(G, LLVMConstNull(ty)); // or a real initializer
            }
            n->value = G; // it probably makes more sense to keep llvalue on the node
        }
        return;

    }

    if (!is_ptr(t) && (is_rec(t) || au->is_union)) {

        array cl = (au->is_union || is_struct(t)) ? a(t) : etype_class_list(t);
        int count = 0;
        int index = 0;
        each(cl, etype, tt) {
            //if (len(cl) > 1 && tt->au == typeid(Au)) 
            //  break;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = (Au_t)tt->au->members.origin[i];
                if (m->member_type == AU_MEMBER_VAR && is_accessible(a, m))
                    count++;
            }
            if (tt->au != typeid(Au) && !is_accessible(a, tt->au)) {
                // this would be non-accessible for all of the above
                count++; // u8 Type_interns[isize]
            }
        }

        LLVMTypeRef* members = calloc(count + 1, sizeof(LLVMTypeRef));
        LLVMTypeRef largest = null;
        int ilargest = 0;
        each(cl, etype, tt) {
            //if (len(cl) > 1 && tt->au == typeid(Au)) break;
            for (int i = 0; i < tt->au->members.count; i++) {
                Au_t m = (Au_t)tt->au->members.origin[i];
                if (m->member_type == AU_MEMBER_FUNC      || 
                    m->member_type == AU_MEMBER_CONSTRUCT || 
                    m->member_type == AU_MEMBER_INDEX     || 
                    m->member_type == AU_MEMBER_OPERATOR  || 
                    m->member_type == AU_MEMBER_CAST) {
                    if (m->member_type == AU_MEMBER_OPERATOR) {
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
                    verify(src, "no src type set for member %o", m);
                    src_init(a, src);
                    etype_implement(src->user);

                    // get largest union member
                    members[index] = lltype(src);
                    verify(members[index], "no lltype found for member %s.%s", au->ident, m->ident);
                    //printf("verifying abi size of %s\n", m->ident);
                    int abi_member  = LLVMABISizeOfType(a->target_data, members[index]);
                    verify(abi_member, "type has no size");
                    if (au->is_union && src->abi_size > ilargest) {
                        largest  = members[index];
                        ilargest = src->abi_size;
                    }
                    index++;
                }
            }
            if (tt->au != typeid(Au) && !is_accessible(a, tt->au)) {
                members[index] = LLVMArrayType(lltype(au_lookup("u8")), tt->au->isize);
                index++;
            }
        }

        etype mt = null;
        if (is_class(t) && t->au != au_lookup("Au_t") && !t->au->is_system && (t->au->traits & AU_TRAIT_SCHEMA) == 0) {
            mt = get_type_id(t);

        } else if (count == 0)
             mt = au_lookup("u8")->user;
        
        if (mt) members[count++] = lltype(mt);
        
        if (au->is_union) {
            count = 1;
            members[0] = largest;
        }

        printf("setting struct body on %s\n", LLVMPrintTypeToString(au->lltype));
        LLVMStructSetBody(au->lltype, members, count, 1);

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
            LLVMValueRef G = LLVMAddGlobal(a->module, tr, n->chars);
            
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
                
                if (!m->user) m->user = (etype)hold(enode(mod, a, value, G, au, m));
                LLVMSetInitializer(G, init);
            }
        }
        enum_processing = false;
    } else if (is_func((Au)t)) {
        Au_t cl = isa(t);
        if (strcmp(t->au->ident, "action") == 0) {
            int test2 = 2;
            test2    += 2;
        }
        verify(cl == typeid(enode), "expected enode");
        //string n = is_rec(t->au->context) ?
        //    f(string, "%s_%s", t->au->context->ident, t->au->ident) : string(t->au->ident);
        cstr n = t->au->alt ? t->au->alt : t->au->ident;
        verify(n, "no name given to function");

        enode fn = (enode)t;
        // functions, on init, create the arg enodes from the model data

        // make sure the types are ready for use
        arg_types(au, arg_type) {
            if (arg_type->user) {
                verify(arg_type->user, "expected user data on type %s", arg_type->ident);
                etype_implement((etype)arg_type->user);
            }
        }
        if (strcmp(n, "init") == 0) {
            int test2 = 2;
            test2    += 2;
        }
        fn->value  = LLVMAddFunction(a->module, n, au->lltype);
        Au_t au_target = au->is_imethod ? (Au_t)au->args.origin[0] : null;
        fn->target = au_target ?
            enode(mod, a, au, au_target ? au_target->src : null, arg_index, 0) : null;
        string label = f(string, "%s_entry", au->ident);
        bool is_user_implement = fn->au->module == a->au || fn->user_built;

        fn->entry = is_user_implement ? LLVMAppendBasicBlockInContext(
            a->module_ctx, fn->value, label->chars) : null;

        if (au->alt && strcmp(au->alt, "something_action") == 0) {
            a = a;
        }
        LLVMSetLinkage(fn->value,
            is_user_implement ? LLVMInternalLinkage : LLVMExternalLinkage);

        int index = 0;
        arg_list(fn->au, arg) {
            arg->user = (etype)evar(mod, a, au, arg, arg_index, index);
            index++;
        }
    }

    if (!au->is_void && au->member_type != AU_MEMBER_FUNC && !is_opaque(au) && !is_func((Au)au) && au->lltype) {
        if (au->elements) {
            au->lltype = LLVMArrayType((LLVMTypeRef)au->lltype, au->elements);
        }
        au->abi_size   = LLVMABISizeOfType(a->target_data, (LLVMTypeRef)au->lltype)      * 8;
        au->align_bits = LLVMABIAlignmentOfType(a->target_data, (LLVMTypeRef)au->lltype) * 8;
    }
}

none aether_output_schemas(aether a, enode init) {
    Au_t au = init->au;
    if (au->is_mod_init) {
        etype f = au->user;

        Au_t module_base = a->au;
        Au_t au_type     = au_lookup("Au");
        Au_t fn_module_lookup = find_member(au_type, "module_lookup", AU_MEMBER_FUNC, false);
        Au_t fn_emplace  = find_member(au_type, "emplace_type", AU_MEMBER_FUNC, false);
        Au_t fn_def_func = find_member(au_type, "def_func",  AU_MEMBER_FUNC, false);
        Au_t fn_def_prop = find_member(au_type, "def_prop",  AU_MEMBER_FUNC, false);
        Au_t fn_def_arg  = find_member(au_type, "def_arg",   AU_MEMBER_FUNC, false);
        Au_t fn_def_meta = find_member(au_type, "def_meta",  AU_MEMBER_FUNC, false);
        Au_t fn_def_type = find_member(au_type, "def_type",  AU_MEMBER_FUNC, false);
        Au_t fn_push     = find_member(au_type, "push_type", AU_MEMBER_FUNC, false);

        push_scope(a, (Au)f);
        enode module_id  = e_fn_call(a, (enode)fn_module_lookup->user, a(const_string(chars, a->name->chars)));

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

            // initialize the type and fields
            e_fn_call(a, (enode)fn_emplace->user, a(
                type_id, e_typeid(a, u(au->context)), e_typeid(a, u(au->src)), module_id,
                const_string(chars, au->ident), _u64(au->traits),
                _u64(mdl->au->typesize), _u64(isize)
            ));
            
            members(au, mem) {
                if (mem->access_type == interface_intern)
                    continue;
                
                if (is_func(mem)) {
                    symbol n = mem->alt ? mem->alt : mem->ident;
                    LLVMValueRef fn_ptr = ((enode)mem->user)->value;
                    Au_t id_none = typeid(none);

                    etype aref = elookup("ARef");
                    enode fptr = value(aref, fn_ptr);
                    enode e_mem = e_fn_call(a, (enode)fn_def_func->user, a(
                        type_id, const_string(chars, mem->ident), e_typeid(a, mem->rtype->user), 
                        _u32(mem->member_type),
                        _u32(mem->access_type),
                        _u32(mem->operator_type),
                        _u64(mem->traits),
                        fptr
                    ));

                    arg_list(mem, arg) {
                        e_fn_call(a, (enode)fn_def_arg->user,
                            a(e_mem, const_string(chars, arg->ident), e_typeid(a, arg->src->user)));
                    }
                } else if (mem->member_type == AU_MEMBER_VAR) {
                    enode e_mem = e_fn_call(a, (enode)fn_def_prop->user,
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
            e_fn_call(a, (enode)fn_push->user, a(type_id));
        }
        pop_scope(a);
        build_module_initializer(a, f);
        build_entrypoint(a, f);
    }
}

enode aether_context_func(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = (Au_t)a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_FUNC)
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
    if (fn && (fn != prev_fn) && !a->no_build) {
        if (fn->au && fn->au->ident && strstr(fn->au->ident, "action")) {
            fn = fn;
        }
        verify(fn->entry, "expected function entry for %s", fn->au->ident);
        LLVMPositionBuilderAtEnd(a->builder, fn->entry);
        LLVMSetCurrentDebugLocation2(a->builder, fn->last_dbg);
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
        { false, true,  true,  false, false, 0, 0, AU_TRAIT_ABSTRACT }, // lets run through non-functions
        { false, true,  true,  true,  false, 0, 0, AU_TRAIT_ABSTRACT }  // now implement functions, whose args will always be implemented
    };

    Au_t ty = typeid(i32);

    for (int filter = 0; filter < 10; filter++) {
        struct filter* ff = &filters[filter];
        
        for (num i = 0; i < ctx->members.count; i++) {
            Au_t m  =  (Au_t)ctx->members.origin[i];

            if (m->module != ctx) continue;
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
                print("init %o", m);
                if (ff->init || ff->impl) {
                    src_init(a, m);
                }
                if (ff->impl) {
                    etype_implement((etype)m->user);
                }
            }
        }
    }

    members(ctx, mem) {
        if (is_au_type(mem) && mem->user && !mem->user->type_id && !mem->is_system && !mem->is_schema)
            get_type_id(mem->user);
    }
}

void aether_import_Au(aether a, path lib) {
    a->current_inc   = lib ? lib : path("Au");
    a->is_Au_import  = true;
    string  lib_name = lib ? stem(lib) : null;
    Au_t    au_module = null;

    // register and push new module scope if we are loading from library
    if (lib) {
        au_module = def_module(copy_cstr(lib_name->chars));
        push_scope(a, (Au)au_module);
    } else {
        au_module = global();
        set(a->libs, string("Au"), _bool(true));
    }

    Au_t base = au_lookup("Au_t");
    if (lib) {
        handle f = dlopen(cstring(lib), RTLD_NOW);
        verify(f, "shared-lib failed to load: %o", lib);
        set(a->libs, string(lib->chars), (Au)f);
    }

    Au_t au    = typeid(Au);
    au->user   = etype(mod, a, au, au);
    Au_t au_t  = typeid(Au_t);
    au_t->user = etype(mod, a, au, au_t);
    etype_ptr(a, au); // Au should be pointer, and we have it as struct; we need to load _Au as what we do, and point to it with Au
    etype_ptr(a, au_t);

    aether_import_models(a, au_module);

    if (!lib) {
        // this causes a name-related error in IR
        Au_t main_cl = def_member(au_module, "main", null, AU_MEMBER_TYPE,
            AU_TRAIT_CLASS | AU_TRAIT_ABSTRACT);
        main_cl->context = typeid(Au);
        main_cl->user = etype(mod, a, au, main_cl);
        implement(main_cl->user);
    }
    a->is_Au_import  = false;
}

void aether_llflag(aether a, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32Type(), ival, 0));

    char sflag[64];
    memcpy(sflag, flag, strlen(flag) + 1);
    LLVMAddModuleFlag(a->module, LLVMModuleFlagBehaviorError, sflag, strlen(sflag), v);
}


bool aether_emit(aether a, ARef ref_ll, ARef ref_bc) {
    path* ll = (path*)ref_ll;
    path* bc = (path*)ref_bc;
    cstr err = NULL;

    path c = path_cwd();

    *ll = form(path, "%o.ll", a);
    *bc = form(path, "%o.bc", a);

    LLVMDumpModule(a->module);

    if (LLVMVerifyModule(a->module, LLVMReturnStatusAction, &err)) {
        fprintf(stderr, "LLVM verify failed:\n%s\n", err);
        LLVMDisposeMessage(err);
        abort();
    }

    if (LLVMPrintModuleToFile(a->module, cstring(*ll), &err))
        fault("LLVMPrintModuleToFile failed");

    if (LLVMVerifyModule(a->module, LLVMPrintMessageAction, &err))
        fault("error verifying module");
    
    if (LLVMWriteBitcodeToFile(a->module, cstring(*bc)) != 0)
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
    LLVMPrintModuleToFile(a->module, "crashing.ll", &err);
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
    path src_path = parent_dir(a->source);
    push(a->include_paths, (Au)src_path);

    a->registry       = array(alloc, 256, assorted, true);
    a->libs           = map(assorted, true);
    a->user_type_ids  = map(assorted, true);
    a->lexical        = array(unmanaged, true, assorted, true);
    a->module         = LLVMModuleCreateWithName(a->name->chars);
    a->module_ctx     = LLVMGetModuleContext(a->module);
    a->dbg_builder    = LLVMCreateDIBuilder(a->module);
    a->builder        = LLVMCreateBuilderInContext(a->module_ctx);
    a->target_triple  = LLVMGetDefaultTargetTriple();
    cstr err = NULL;
    if (LLVMGetTargetFromTriple(a->target_triple, &a->target_ref, &err))
        fault("error: %s", err);
    a->target_machine = LLVMCreateTargetMachine(
        a->target_ref, a->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    path rel = parent_dir(a->source);
    a->file = LLVMDIBuilderCreateFile(a->dbg_builder, a->name->chars, len(a->name), rel->chars, len(rel));

    a->target_data = LLVMCreateTargetDataLayout(a->target_machine);
    a->compile_unit = LLVMDIBuilderCreateCompileUnit(
        a->dbg_builder, LLVMDWARFSourceLanguageC, a->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);
    a->au->llscope = a->compile_unit;
    a->builder = LLVMCreateBuilderInContext(a->module_ctx);

    // push our module space to the scope
    Au_t g = global();
    verify(g,           "globals not registered");
    verify(a->au,       "no module registered for aether");
    verify(g != a->au,  "aether using global module");

    push_scope(a, (Au)g);
    import_Au(a, null);

    // i am an ass.
    // more understanding of self-ass'ness:
    //      the models we import from include are of course different one ones we load at runtime, however the effect of it is strange, both call def() of course
    //      for now, its best to set an explicit au bit for AU_TRAIT_IS_AU; this applies to all models
    //      the bottom of our main init function is where we have a little workflow with underpinnings of responses to self-doubt
    //      its nice to only have a true use-case for 1/40th of LLVM api
    //      that will likely double if we get into an asm front-end.  to me its a shame that IR api is not so acceptable to write for users HAHA.. sorry -- its just a great format for their abstract, but not a 'general' asm people want to learn.  GNU should offer this as something that translate to these different platforms
    //      i dont know precisely what this entails because of the need to emit implementation in switch-like expression of architectures supported.
    //
    //
    a->au->is_namespace = true; // for the 'module' namespace at [1], i think we dont require the name.. or, we set a trait
    a->au->is_nameless  = false; // we have no names, man. no names. we are nameless! -cereal
    push_scope(a, (Au)a->au);

    // todo: test code here
    //aclang_cc instance;
    //path i = include(a, string("stdio.h"), null, &instance);
    //print("included: %o", i);
}

none aether_dealloc(aether a) {
    LLVMDisposeBuilder  (a->builder);
    LLVMDisposeDIBuilder(a->dbg_builder);
    LLVMDisposeModule   (a->module);
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

    //etype_implement(n);
    
    if (is_func((Au)n->au->context)) {
        Au_t ctx = n->au->context;
        if (strcmp(ctx->ident, "action") == 0) {
            ctx = ctx;
        }
        enode fn = (enode)n->au->context->user;
        n->value = LLVMGetParam(fn->value, n->arg_index);
    }

    if (n->value == 0x0005000000000000) {
        n = n;
    }

    if (n->au == typeid(string)) {
        printf("au->user = %p (%p)\n", n->au->user, n->au);
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
array codegen_generate_fn(codegen a, enode f, array query) {
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
    etype mdl_cstr = elookup("cstr");
    etype mdl_i32  = elookup("i32");
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
        (etype)array->meta->origin[0] : elookup("Au"));
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
        enode res     = value(elookup("i32"), LLVMConstInt(LLVMInt32Type(), diff, 0));
        res->literal  = hold(literal);
        return res;
    }

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("i32"));

    verify(context_func(a), "non-const compare must be in a function");

    // Class comparison - try member function first, fall back to pointer compare
    if (Lc || Rc) {
        if (!Lc) { enode t = L; L = R; R = t; }

        Au_t eq = find_member(L->au, "compare", AU_MEMBER_FUNC, true);
        if (eq) {
            verify(eq->rtype == au_lookup("i32"), "compare function must return i32, found %o", eq->rtype);
            return e_fn_call(a, (enode)eq->user, a(L, R));
        }

        LLVMValueRef lt = LLVMBuildICmp(a->builder, LLVMIntULT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildICmp(a->builder, LLVMIntUGT, L->value, R->value, "cmp_gt");
        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);
        LLVMValueRef lt_val  = LLVMBuildSelect(a->builder, lt, neg_one, zero, "lt_val");
        LLVMValueRef result  = LLVMBuildSelect(a->builder, gt, pos_one, lt_val, "cmp_r");
        return value(elookup("i32"), result);
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

        return value(elookup("i32"), phi);
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
        return value(elookup("i32"), result);
    }

    // Integer comparison via subtraction
    return value(elookup("i32"), LLVMBuildSub(a->builder, L->value, R->value, "cmp_i"));
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
    if (a->no_build) return e_noop(a, elookup("bool"));

    return value(L, LLVMBuildNot(a->builder, L->value, "bitwise-not"));
}


enode aether_e_not(aether a, enode L) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("bool"));

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
    return value(elookup("bool"), result);
}

none enode_release(enode mem) {
    aether e = mem->mod;
    etype mdl = (etype)mem;
    if (!is_ptr(mdl))
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
    enode f_dealloc = (enode)find_member(mem->au, "dealloc", AU_MEMBER_FUNC, true)->user;
    if (f_dealloc) {
        e_fn_call(e, (enode)f_dealloc, a(mem));
    }
    LLVMBuildFree(e->builder, ref_ptr);
    LLVMBuildBr(e->builder, no_free_block);

    // No-free block: Continue without freeing
    LLVMPositionBuilderAtEnd(e->builder, no_free_block);
}


Au_t aether_pop_scope(aether a) {
    statements st = (statements)instanceof(top_scope(a)->user, statements);
    if (st) {
        members(st->au, mem) {
            if (instanceof(mem->user, enode))
                release((enode)mem->user);
        }
    }
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
        if (isa(ctx->user) == typeid(enode) && ctx->rtype)
            return ctx->rtype->user;
    }
    return null;
}

// ident is optional, and based on 
enode aether_function(aether a, string ident, etype rtype, array args, u8 member_type, u32 traits, u8 operator_type) {
    Au_t context = top_scope(a);
    Au_t au = def(context, ident->chars, AU_MEMBER_FUNC, traits);
    if (context->member_type == AU_MEMBER_MODULE)
        au->module = context;
    else if (context->is_class || context->is_struct) {
        au->module = context->module;
    }

    if (!rtype) rtype = elookup("none");
    each(args, Au, arg) {
        Au_t a = au_arg(arg);
        Au_t argument = def(au, null, AU_MEMBER_VAR, 0);
        argument->src = a;
        array_qpush((array)&au->args, (Au)argument);
    }
    au->src = (Au_t)hold(rtype->au);
    enode n = enode(mod, a, au, au, used, true);
    //etype_implement((etype)n); // creates llvm function value for enode
    return n;
}

etype aether_record(aether a, etype based, string ident, u32 traits, array meta) {
    Au_t context = top_scope(a);
    Au_t au = def(context, ident->chars, AU_MEMBER_TYPE, traits);
    au->context = based ? based->au : elookup("Au")->au;
    each(meta, Au, arg) {
        Au_t a = au_arg(arg);
        array_qpush((array)&au->meta, (Au)a);
    }
    etype n = etype(mod, a, au, au);
    //etype_implement(n);
    return n;
}

enode aether_module_initializer(aether a) {
    if (a->fn_init) return a->fn_init;
    verify(a, "model given must be module (aether-based)");

    enode init = function(a,
        string("initializer"), elookup("none"), array(),
        AU_MEMBER_FUNC, AU_TRAIT_MODINIT, OPType__undefined);

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
define_class(enode,      etype) // not a member unless member is set (direct, or created with name)

define_class(aether,     etype)

define_class(aclang_cc,  Au)
define_class(codegen,    Au)
define_class(code,       Au)
define_class(static_array, array)