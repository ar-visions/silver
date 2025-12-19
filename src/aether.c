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

#define au_lookup(sym)    Au_lexical(a->lexical, sym)
#define elookup(sym) ({ \
    Au_t au = Au_lexical(a->lexical, sym); \
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
        res = e_operand(a, res, L);
        //res = e_convert(a, res, L->mdl);
    }

    LLVMBuildStore(a->builder, res->value, L->value);
    return res;
}

none etype_implement(etype t);

static etype etype_ptr(aether mod, Au_t a) {
    verify(a && !isa((Au)a), "ptr requires Au_t, given %s", isa((Au)a)->ident);
    if (isa(a) == typeid(etype)) return ((etype) a)->au;
    Au_t au = a;
    if (a->ident && strcmp(a->ident, "ARef") == 0) {
        int test2 = 2;
        test2    += 2;
    }
    if (au->ptr) return au->ptr->user;
    au->ptr              = Au_register(mod->au, null, AU_MEMBER_TYPE, 0);
    au->ptr->is_pointer  = true;
    au->ptr->src         = au;
    au->ptr->user        = etype(mod, mod, au, au->ptr);
    return au->ptr->user;
}

etype pointer(aether mod, Au a) {
    Au_t au = au_arg(a);
    if (au->member_type == AU_MEMBER_PROP)
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

LLVMTypeRef lltype(Au a);

etype etype_traits(Au a, int traits) {
    Au_t au;
    if (isa(a) == typeid(etype)) {
        verify(lltype(a), "no type found on %o", a);
        au = ((etype) a)->au;
    }
    else if (isa(a) == typeid(enode)) au = ((enode)a)->au;
    else if (!isa(a)) {
        verify(lltype(a), "no type found on %o", a);
        au = a;
    }
    verify(au, "unhandled input");
    if (au->is_class) {
        verify(au->ptr, "expected ptr for class");
        au = au->ptr;
    }
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
    enode mL = instanceof(L, typeid(enode)); 
    enode LV = e_operand(a, L, null);
    enode RV = e_operand(a, R, null);

    // check for overload
    if (op_name && isa(L) == typeid(enode) && is_class(L)) {
        enode Ln = L;
        etype rec  = etype_traits(Ln, AU_TRAIT_CLASS);
        if (rec) {
            etype Lt = null;
            for (int i = 0; i < rec->au->members.count; i++) {
                Au_t mem = rec->au->members.origin[i];
                if (mem->operator_type == optype) {
                    Lt = mem->user;
                    break;
                }
            }
            if  (Lt) {
                verify(Lt->au->args.count == 1,
                    "expected 1 argument for operator method");
                /// convert argument and call method
                etype arg_expects = ((Au_t)Lt->au->args.origin[0])->user;
                enode  conv = e_create(a, arg_expects, Ln);
                array args = array_of(mL, conv, null);
                verify(mL, "L-operand is invalid data-type");
                return e_fn_call(a, Lt, args);
            }
        }
    }

    /// LV cannot change its type if it is a emember and we are assigning
    enode Lnode = L;
    Au_t ltype = isa(L);
    etype rtype = determine_rtype(a, optype, LV, RV); // todo: return bool for equal/not_equal/gt/lt/etc, i64 for compare; there are other ones too

    LLVMValueRef RES;
    LLVMTypeRef  LV_type = LLVMTypeOf(LV->value);
    LLVMTypeKind vkind = LLVMGetTypeKind(LV_type);

    enode LL = optype == OPType__assign ? LV : e_create(a, rtype, LV); // we dont need the 'load' in here, or convert even
    enode RL = e_create(a, rtype, RV);

    symbol         N = cstring(op_name);
    LLVMBuilderRef B = a->builder;
    Au literal = null;
    // if not storing ...
    if (optype == OPType__or || optype == OPType__and) {
        // ensure both operands are i1
        etype m_bool = elookup("bool");
        rtype = m_bool;
        LL = e_create(a, m_bool, LL); // generate compare != 0 if not already i1
        RL = e_create(a, m_bool, RL);
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
LLVMTypeRef lltype(Au a) {
    Au_t au;
    if (instanceof(a, typeid(etype))) {
        au = ((etype) a)->au;
    }
    else if (!isa(a)) {
        au = a;
    }
    if (au->member_type == AU_MEMBER_ARG) {
        au = au->src;
    }
    Au_t prev = au;
    while (au && !au->lltype) {
        au = au->src;
    }

    verify(au && au->lltype, "no type found on %o", prev);
    
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
enode aether_e_div(aether a, Au L, Au R) { return e_op(a, OPType__div, string("div"), L, R); }
enode aether_value_default(aether a, Au L, Au R) { return e_op(a, OPType__value_default, string("value_default"), L, R); }
enode aether_cond_value   (aether a, Au L, Au R) { return e_op(a, OPType__cond_value,    string("cond_value"), L, R); }


enode aether_e_inherits(aether a, enode L, Au R) {
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("bool"));

    // Get the type pointer for L
    enode L_type =  e_offset(a, L, _i64(-sizeof(Au)));
    enode L_ptr  =    e_load(a, L, null);
    enode R_ptr  = e_operand(a, R, null);

    // Create basic blocks for the loopf
    LLVMBasicBlockRef block      = LLVMGetInsertBlock(a->builder);
    LLVMBasicBlockRef loop_block = LLVMAppendBasicBlock(block, "inherit_loop");
    LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(block, "inherit_exit");

    // Branch to the loop block
    LLVMBuildBr(a->builder, loop_block);

    // Loop block
    LLVMPositionBuilderAtEnd(a->builder, loop_block);
    LLVMValueRef phi = LLVMBuildPhi(a->builder, lltype(L_ptr), "current_type");
    LLVMAddIncoming(phi, &L_ptr->value, &block, 1);

    // Compare current type with R_type
    enode cmp       = e_eq(a, value(L_ptr, phi), R_ptr);

    // Load the parent pointer (assuming it's the first emember of the type struct)
    enode parent    = e_load(a, value(L_ptr, phi), null);

    // Check if parent is null
    enode is_null   = e_eq(a, parent, value(parent, LLVMConstNull(lltype(parent))));

    // Create the loop condition
    enode not_cmp   = e_not(a, cmp);
    enode not_null  = e_not(a, is_null);
    enode loop_cond = e_and(a, not_cmp, not_null);

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
            R = e_create(a, L, R);
        else
            L = e_create(a, R, L);
    }

    // 3. Class → use compare() method
    if (is_class(L) || is_class(R)) {
        // reorder so L is the class (if needed)
        if (!is_class(L)) {
            enode tmp = L; L = R; R = tmp;
        }

        Au_t fn = Au_find_member(L->au, "compare", AU_MEMBER_FUNC);
        verify(fn, "class %s has no compare() method", L->au->ident);

        enode cmp = e_fn_call(a, fn->user, a(L, R));

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
            Au_t m = L->au->members.origin[i];
            if (m->member_type != AU_MEMBER_PROP)
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
        return value(bool_t,
            LLVMBuildFCmp(a->builder, LLVMRealOEQ,
                          lltype(L), lltype(R), "eq-f"));

    return value(bool_t,
        LLVMBuildICmp(a->builder, LLVMIntEQ,
                      lltype(L), lltype(R), "eq-i"));
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
        Au_t au_f = Au_find_member(L->au, "eq", AU_MEMBER_FUNC);
        if (au_f) {
            etype eq = au_f->user;
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
            Au_t mem = L->au->members.origin[i];
            if (mem->member_type != AU_MEMBER_PROP)
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
                R = e_create(a, L, R);
            } else {
                L = e_create(a, R, L);
            }
        } else if (Lr) {
            R = e_create(a, L, R);
        } else if (Rr) {
            L = e_create(a, L, R);
        } else {
            if (L->au->abi_size > R->au->abi_size) {
                R = e_create(a, L, R);
            } else {
                L = e_create(a, R, L);
            }
        }
    }

    if (Lr || Rr)
        return value(elookup("bool"), LLVMBuildFCmp(a->builder, LLVMRealOEQ, L->value, R->value, "eq-f"));
    
    return value(elookup("bool"), LLVMBuildICmp(a->builder, LLVMIntEQ, L->value, R->value, "eq-i"));
}

enode aether_e_eval(aether a, string value) {
    array t = tokens(target, (Au)a, parser, a->parse_f, input, (Au)value);
    push_tokens(a, t, 0);
    enode n = a->parse_expr(a, null, null); 
    enode s = e_create(a, elookup("string"), n);
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
        accum = accum ? e_add(a, accum, val) : val;
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
         if (instanceof(op, typeid(  enode))) return op;
    else if (instanceof(op, typeid(  etype))) return e_typeid(a, op);
    else if (instanceof(op, typeid( handle))) {
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
    else if (instanceof(op, typeid(bool)))   return uint_value(8,  op);
    else if (instanceof(op, typeid(    u8))) return uint_value(8,  op);
    else if (instanceof(op, typeid(   u16))) return uint_value(16, op);
    else if (instanceof(op, typeid(   u32))) return uint_value(32, op);
    else if (instanceof(op, typeid(   u64))) return uint_value(64, op);
    else if (instanceof(op, typeid(    i8))) return  int_value(8,  op);
    else if (instanceof(op, typeid(   i16))) return  int_value(16, op);
    else if (instanceof(op, typeid(   i32))) return  int_value(32, op);
    else if (instanceof(op, typeid(   i64))) return  int_value(64, op);
    else if (instanceof(op, typeid(    sz))) return  int_value(64, op); // instanceof is a bit broken here and we could fix the generic; its not working with aliases
    else if (instanceof(op, typeid(   f32))) return  f32_value(32, op);
    else if (instanceof(op, typeid(   f64))) return  f64_value(64, op);
    else if (instanceof(op, typeid(symbol))) {
        return enode(mod, a, value, const_cstr(a, op, strlen(op)), au, au_lookup("symbol"), literal, op);
    }
    else if (instanceof(op, typeid(string))) { // this works for const_string too
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
            e_interpolate(a, (string)op));
    
    if (isa(op) == typeid(const_string))
        return e_create(a, src_model, e_operand_primitive(a, op));
    
    if (isa(op) == typeid(map)) {
        return e_create(a, src_model, op);
    }
    Au_t op_isa = isa(op);
    if (instanceof(op, typeid(array))) {
        verify(src_model != null, "expected src_model with array data");
        return e_create(a, src_model, op);
    }

    enode r = e_operand_primitive(a, op);
    return src_model ? e_create(a, src_model, r) : r;
}

enode aether_e_null(aether a, etype mdl) {
    if (!mdl) mdl = elookup("handle");
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
        if (instanceof(m, typeid(etype)))
            n = e_typeid(a, (etype)m);
        else if (instanceof(m, typeid(shape))) {
            shape s = (shape)m;
            array ar = array(alloc, s->count);
            push_vdata(ar, s->data, s->count);
            n = e_create(a, elookup("shape"),
                m("count", s->count,
                  "data",  e_const_array(a, elookup("i64"), ar)));
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
    return e_addr_of(a, arr_node, atype);
}

enode aether_e_not_eq(aether a, enode L, enode R) {
    return e_not(a, e_eq(a, L, R));
}

none aether_e_fn_return(aether a, Au o) {
    Au_t au_ctx = Au_context(a->lexical, AU_MEMBER_FUNC, 0);
    verify (au_ctx, "function not found in context");
    etype f = au_ctx->user;

    a->is_const_op = false;
    if (a->no_build) return;

    if (is_void(f->au->rtype)) {
        LLVMBuildRetVoid(a->builder);
        return;
    }

    enode direct = o;
    enode conv = e_create(a, f->au->rtype, o); // e_operand(a, o, null)
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
    enode   printf_fn  = elookup("printf");
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
        array_of(e_operand(a, string(fmt), null), n));
}

enode aether_e_fn_call(aether a, enode fn, array args) {
    // we could support an array or map arg here, for args
    // we set this when we do something complex
    a->is_const_op = false; 
    if (a->no_build) return e_noop(a, fn->au->rtype);

    etype_implement(fn);
    etype target_type = func_target(fn);
    enode f = len(args) ? get(args, 0) : null;
    verify(canonical(target_type) == canonical(f),
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
        arg_values[index++] = e_create(a, cast_to, f)->value;
    }

    int fmt_idx = -1;
    int arg_idx = -1;
    for (int i = target_type ? 1 : 0; i < fn->au->args.count; i++) {
        Au_t arg = fn->au->args.origin[i];
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
            etype arg_type = array_get(&fn->au->args, i);
            enode n   = instanceof(arg_value, typeid(enode));
            if (index == fmt_idx) {
                Au fmt = n ? instanceof(n->literal, typeid(const_string)) : null;
                verify(fmt, "formatter functions require literal, constant strings");
            }
            // this takes in literals and enodes
            enode conv = e_create(a, arg_type, arg_value);
            LLVMValueRef vr = arg_values[index] = conv->value;
            i++;
            index++;
        }
    }
    int istart = index;
    
    if (fmt_idx >= 0) {
        Au_t src_type = isa(args->origin[fmt_idx]);
        enode  fmt_node = instanceof(args->origin[fmt_idx], typeid(enode));
        string fmt_str  = instanceof(fmt_node->literal, typeid(const_string));
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
                enode conv      = e_create(a, arg_type, n_arg);
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

static Au_t castable(etype fr, etype to) { 
    bool fr_ptr = is_ptr(fr);
    if ((fr_ptr || is_prim(fr)) && is_bool(to))
        return (Au_t)true;
    
    /// compatible by match, or with basic integral/real types
    if ((fr == to) ||
        ((is_realistic(fr) && is_realistic(to)) ||
         (is_integral (fr) && is_integral (to))))
        return (Au_t)true;
    
    /// primitives may be converted to Au-type Au
    if (is_prim(fr) && is_generic(to))
        return (Au_t)true;

    /// check cast methods on from
    for (int i = 0; i < fr->au->members.count; i++) {
        Au_t mem = fr->au->members.origin[i];
        if (mem->member_type != AU_MEMBER_CAST)
            continue;
        if (mem->rtype == to->au)
            return mem;
    }
    return (Au_t)false;
}


static Au_t constructable(etype fr, etype to) {
    if (fr == to)
        return (Au_t)true;
    for (int ii = 0; ii < to->au->members.count; ii++) {
        Au_t mem = to->au->members.origin[ii];
        etype fn = mem->member_type == AU_MEMBER_CONSTRUCT ? mem->user : null;
        if  (!fn) continue;
        Au_t first = array_first_element(&fn->au->args);
        if (first == fr->au)
            return mem;
    }
    return (Au_t)false;
}

static Au_t scalar_compatible(LLVMTypeRef ea, LLVMTypeRef eb) {
    LLVMTypeKind ka = LLVMGetTypeKind(ea);
    LLVMTypeKind kb = LLVMGetTypeKind(eb);

    if (ka == kb) {
        switch (ka) {
            case LLVMIntegerTypeKind:
                if (LLVMGetIntTypeWidth(ea) == LLVMGetIntTypeWidth(eb))
                    return (Au_t)true;
                else
                    return (Au_t)false;
            case LLVMFloatTypeKind:   // 32-bit float
            case LLVMDoubleTypeKind:  // 64-bit float
            case LLVMHalfTypeKind:    // 16-bit float
            case LLVMBFloatTypeKind:  // 16-bit bfloat
                return (Au_t)true; // same kind → compatible
            default:
                return (Au_t)false;
        }
    }
    return (Au_t)false;
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

static bool is_subclass(Au_t a0, Au_t b0) {
    Au_t a = au_arg(a0);
    Au_t b = au_arg(b0);
    while (a) {
        if (a == b) return true;
        if (a->context == a) break;
        a = a->context;
    }
    return false;
}

// returns null, true, or the member function used for conversion
static Au_t convertible(etype fr, etype to) {
    aether a = fr->mod;
    etype  ma = resolve(fr);
    etype  mb = resolve(to);

    // more robust conversion is, they are both pointer and not user-created
    if (ma->au->is_system || mb->au == typeid(handle))
        return (Au_t)true;

    if (ma == mb)
        return (Au_t)true;

    if (is_prim(ma) && is_prim(mb))
        return (Au_t)true;

    if (is_rec(ma) || is_rec(mb)) {
        if (is_subclass(ma, mb))
            return (Au_t)true;
        Au_t mcast = castable(ma, mb);
        return mcast ? mcast : constructable(ma, mb);
    } else {
        // the following check should be made redundant by the code below it
        etype sym = elookup("symbol");
        etype ri8 = elookup("ref_i8");

        // quick test
        if ((ma == sym && mb == ri8) || (ma == ri8 && mb == sym))
            return (Au_t)true;

        LLVMTypeKind ka = LLVMGetTypeKind(lltype(ma));
        LLVMTypeKind kb = LLVMGetTypeKind(lltype(mb));

        // pointers → compare element types
        if (ka == LLVMPointerTypeKind && kb == LLVMPointerTypeKind) {
            // return true if one is of an opaque type
            // return false if the scalar units are different size
            return (Au_t)true;
        }

        // primitive check should find all other valid ones above this
        return scalar_compatible(lltype(ma), lltype(mb));
    }
    return (Au_t)false;
}

static bool is_addressable(enode e) {
    if (is_ptr(e)) return true;
    if (e->value && LLVMIsAGlobalValue(e->value)) return true;
    return false;
}

enode enode_access(enode target, string name) {
    aether a = target->mod;
    Au_t   m = Au_find_member(target->au, name->chars, 0);
    verify(m, "failed to find member %o on type %o", name, target);
    
    verify(is_addressable(target), 
        "expected target pointer for member access");

    etype t = resolve(target); // resolves to first type
    verify(t->au->is_struct || t->au->is_class, 
        "expected resolution to struct or class");

    // for functions, we return directly with target passed along
    if (is_func(m))
        return enode(mod, a, name, name, au, m, target, target);
    
    if (instanceof(m->user, typeid(enode)))
        return ((enode)m->user)->value; // this should apply to enums

    // signal we're doing something non-const
    a->is_const_op = false;
    if (a->no_build) return e_noop(a, m->user);

    Au_t ptr = pointer(a, m)->au;
    return enode(
        mod,    a,
        name,   name,
        au,     ptr,
        value,  LLVMBuildStructGEP2(
            a->builder, lltype(t), target->value,
            m->index, "enode_access"));
}

enode aether_e_typeid(aether a, etype mdl) {
    a->is_const_op = false;
    verify(mdl->type_id, "schema instance not found for %o", mdl);
    return mdl->type_id;
}

/// create is both stack and heap allocation (based on etype->is_ref, a storage enum)
/// create primitives and objects, constructs with singular args or a map of them when applicable
enode aether_e_create(aether a, etype t, Au args) {
    if (!t) return args;

    string  str  = instanceof(args, typeid(string));
    map     imap = instanceof(args, typeid(map));
    array   ar   = instanceof(args, typeid(array));
    static_array static_a = instanceof(args, typeid(static_array));
    enode   n    = null;
    enode ctr  = null;

    if (!args) {
        if (is_ptr(t))
            return e_null(a, t);
    }

    // construct / cast methods
    enode input = instanceof(args, typeid(enode));

    if (!input && instanceof(args, typeid(const_string))) {
        input = e_operand(a, args, elookup("string"));
        args  = input;
    }

    if (!input && instanceof(args, typeid(string))) {
        input = e_operand(a, args, elookup("string"));
        args  = input;
    }

    if (input) {
        verify(!imap, "unexpected data");
        
        // if both are internally created and these are refs, we can allow conversion
        enode fmem = convertible(input, t);
        verify(fmem, "no suitable conversion found for %o -> %o",
            input, t);
        
        if (fmem == (void*)true) {
            LLVMTypeRef typ = LLVMTypeOf(input->value);
            LLVMTypeKind k = LLVMGetTypeKind(typ);

            // check if these are either Au_t class typeids, or actual compatible instances
            if (k == LLVMPointerTypeKind) {
                a->is_const_op = false;
                if (a->no_build) return e_noop(a, t);

                etype src = resolve(input)->au->src->user;
                etype dst = resolve(t)->au->src->user;
                bool bit_cast = false;
                if (input->au->is_typeid && (t->au->is_typeid || t == elookup("Au_t")))
                    bit_cast = true; 
                else if (!is_subclass(input, t)) {
                    char *s = LLVMPrintTypeToString(t);
                    int r0 = ref_level(input);
                    int r1 = ref_level(t);
                    print("LLVM type: %s", s);
                    verify((is_prim(src) && is_prim(dst)) ||
                        etype_inherits(input, t), "models not compatible: %o -> %o",
                            input, t);
                }
                return value(t,
                    LLVMBuildBitCast(a->builder, input->value, lltype(t), "class_ref_cast"));
            }

            // fallback to primitive conversion rules
            return aether_e_primitive_convert(a, input, t);
        }
        
        // primitive-based conversion goes here
        enode fn = instanceof(fmem, typeid(enode));
        if (fn->au->member_type == AU_MEMBER_CONSTRUCT) {
            // ctr: call before init
            // this also means the mdl is not a primitive
            //verify(!is_primitive(fn->rtype), "expected struct/class");
            ctr = fmem;
        } else if (fn->au->member_type == AU_MEMBER_CAST) {
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

    etype        cmdl         = etype_traits(t, AU_TRAIT_CLASS);
    etype        smdl         = etype_traits(t, AU_TRAIT_STRUCT);
    Au_t         Au_type      = au_lookup("Au");
    etype        f_alloc      = Au_find_member(Au_type, "alloc_new", AU_MEMBER_FUNC)->user;
    etype        f_initialize = Au_find_member(Au_type, "initialize", AU_MEMBER_FUNC)->user;
    enode        res;

    if (is_ptr(t) && is_struct(t->au->src) && static_a) {
        static int ident = 0;
        char name[32];
        sprintf(name, "static_arr_%i", ident++);

        i64          ln     = len(static_a);
        etype        emdl   = t->au->src;
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

    } else if (cmdl) {
        // we have to call array with an intialization property for size, and data pointer
        // if the data is pre-defined in init and using primitives, it has to be stored prior to this call
        enode metas_node = e_meta_ids(a, t->meta);
        
        res = e_fn_call(a, f_alloc, a( e_typeid(a, t), _i32(1), metas_node ));
        res->au = t->au; // we need a general cast method that does not call function

        bool is_array = cmdl && cmdl->au->context == au_lookup("array");
        if (imap) {
            
            pairs(imap, i) {
                string k = i->key;

                enode i_prop  = enode_access(res, k);
                enode i_value = e_operand(a, i->value, resolve(i_prop));
                
                e_assign(a, i_prop, i_value, OPType__assign);
            }

            // this is a static method, with a target of sort, but its not a real target since its not a real instance method
            e_fn_call(a, f_initialize, a(res)); // required logic need not emit ops to set the bits when we can check at design time
            
        } else if (ar) { // if array is given for args

            // if we are CREATING an array
            if (is_array) {
                int   ln = len(ar);
                enode n  = e_operand(a, _i64(ln), elookup("i64"));
                bool all_const = ln > 0;
                enode const_array = null;
                for (int i = 0; i < ln; i++) {
                    enode node = instanceof(ar->origin[i], typeid(enode));
                    if (node && node->literal)
                        continue;
                    all_const = false;
                    break;
                }
                if (all_const) {
                    etype ptr = pointer(a, t);
                    etype elem_t = resolve(t);   // base element type
                    LLVMValueRef *elems   = malloc(sizeof(LLVMValueRef) * ln);
                    for (int i = 0; i < ln; i++) {
                        enode node = ar->origin[i];
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
                e_assign(a, prop_alloc, n, OPType__assign);
                if (const_array) {
                    enode tru = e_operand(a, _bool(ln), elookup("bool"));
                    e_assign(a, prop_unmanaged, tru, OPType__assign);
                }     
                e_fn_call(a, f_initialize, a(res));
                if (const_array) {
                    etype f_push_vdata = Au_find_member(t->au, "push_vdata", AU_MEMBER_FUNC)->user;
                    e_fn_call(a, f_push_vdata, a(res, const_array, n));
                } else {
                    etype f_push = Au_find_member(t->au, "push", AU_MEMBER_FUNC)->user;
                    for (int i = 0; i < ln; i++) {
                        Au      aa = ar->origin[i];
                        enode   n = e_operand(a, aa, t->au->src->user);
                        e_fn_call(a, f_push, a(res, n));
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
                e_fn_call(a, ctr, a(res, input));
                e_fn_call(a, f_initialize, a(res));
            } else {
                verify(false, "expected constructor for type %o", t);
            }
        }
    } else if (ctr) {
        verify(is_struct(t), "expected structure");
        res = e_fn_call(a, ctr, a(res, input));
    } else {
        verify(!a, "no translation for array to etype %o", t);
        bool is_ref_struct = is_ptr(t) && is_struct(resolve(t));

        if (is_struct(t) || is_ref_struct) {
            verify(!a, "unexpected array argument");
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
                    string  k = i->key;
                    Au_t   t = isa(i->value);
                    Au_t   m = Au_find_member(rmdl->au, k->chars, AU_MEMBER_PROP);
                    i32 index = m->index;

                    enode value = e_operand(a, i->value, m->src->user);
                    if (all_const && !LLVMIsConstant(value->value))
                        all_const = false;

                    //verify(LLVMIsConstant(value->value), "non-constant field in const struct");
                    
                    push(field_indices, _i32(index));
                    push(field_names,   string(m->ident));
                    push(field_values,  value);
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
                        Au_t smem = rmdl->au->members.origin[ri];
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
                    emember m = find_member(mdl, i->key, null);
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
    etype      mdl = n;
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
    
    etype g = au_lookup("any");
    if (m0 == g)
        return m1;
    
    if (m1 == g)
        return m0;
    
    if (inherits(m0, m1))
        return m1;
    
    return m0;
}

enode aether_e_ternary(aether a, enode cond_expr, enode true_expr, enode false_expr) {
    aether mod = a;
    etype rmdl  = prefer_mdl(true_expr, false_expr);

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, rmdl);

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

    // Return some enode or result if necessary (a.g., a enode indicating the overall structure)
    return enode(mod, mod, au, rmdl->au, value, null);
}

enode aether_e_builder(aether a, subprocedure cond_builder) {
    if (!a->no_build) {
        LLVMBasicBlockRef block = LLVMGetInsertBlock(a->builder);
        LLVMPositionBuilderAtEnd(a->builder, block);
    }
    enode   n = invoke(cond_builder, null);
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
    LLVMBuilderRef B = a->builder;
    LLVMTypeRef    Ty = LLVMTypeOf(switch_val->value);
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(a->builder);

    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlock(entry, "switch.end"));
    push_scope(a, switch_cat);

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
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(a->builder);

    // invoke expression for switch, and push switch cat
    enode   switch_val = e_expr; // invoke(expr_builder, expr);
    catcher switch_cat = catcher(mod, a,
        block, LLVMAppendBasicBlock(entry, "switch.end"));
    push_scope(a, switch_cat);

    // allocate cats for each case, do NOT build the body yet
    // wrap in cat, store the catcher, not an enode
    map case_blocks = map(hsize, 16);
    pairs(cases, i)
        set(case_blocks, i->key, catcher(mod, a, team, case_blocks, block, LLVMAppendBasicBlock(entry, "case")));

    // default block, and obtain insertion block for first case
    catcher def_cat = def_block ? catcher(mod, a, block, LLVMAppendBasicBlock(entry, "default")) : null;
    LLVMBasicBlockRef cur = LLVMGetInsertBlock(a->builder);
    int total = cases->count;
    int idx = 0;

    pairs(case_blocks, i) {
        array   key_expr = i->key;
        catcher case_cat = i->value;

        // position at current end & compare
        LLVMPositionBuilderAtEnd(a->builder, cur);
        enode eq = e_cmp(a, switch_val, key_expr);

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
        catcher case_cat = p->value;

        LLVMPositionBuilderAtEnd(a->builder, case_cat->block);
        array body_tokens = value_by_index(cases, i++);
        invoke(body_builder, body_tokens);

        // we should have a last node, but see return statement returns its own thing, not a return
        // if the case didn’t terminate (break/return), jump to merge
        LLVMBuildBr(a->builder, switch_cat->block);
    }

    // ---- DEFAULT body ----
    if (def_cat) {
        LLVMPositionBuilderAtEnd(a->builder, def_cat->block);
        invoke(body_builder, def_block);
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
    LLVMBasicBlockRef entry   = LLVMGetInsertBlock(a->builder);
    LLVMBasicBlockRef cond    = LLVMAppendBasicBlock(entry, "for.cond");
    LLVMBasicBlockRef body    = LLVMAppendBasicBlock(entry, "for.body");
    LLVMBasicBlockRef step    = LLVMAppendBasicBlock(entry, "for.step");
    LLVMBasicBlockRef merge   = LLVMAppendBasicBlock(entry, "for.end");
    catcher cat = catcher(mod, a, block, merge);
    push_scope(a, cat);

    // ---- init ----
    if (init_exprs)
        invoke(init_builder, init_exprs);

    LLVMBuildBr(a->builder, cond);

    // ---- cond ----
    LLVMPositionBuilderAtEnd(a->builder, cond);

    enode cond_res = invoke(cond_builder, cond_expr);
    LLVMValueRef cond_val = e_create(a, elookup("bool"), cond_res)->value;

    LLVMBuildCondBr(a->builder, cond_val, body, merge);

    // ---- body ----
    LLVMPositionBuilderAtEnd(a->builder, body);

    invoke(body_builder, body_exprs);

    LLVMBuildBr(a->builder, step);

    // ---- step ----
    LLVMPositionBuilderAtEnd(a->builder, step);

    if (step_exprs)
        invoke(step_builder, step_exprs);

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
    LLVMBasicBlockRef cond    = LLVMAppendBasicBlock(entry, "loop.cond");
    LLVMBasicBlockRef iterate = LLVMAppendBasicBlock(entry, "loop.body");

    catcher cat = catcher(mod, a, block, LLVMAppendBasicBlock(entry, "loop.end"));
    push_scope(a, cat);

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

    enode cond_result = invoke(cond_builder, expr_cond);
    LLVMValueRef condition = e_create(a, elookup("bool"), cond_result)->value;

    LLVMBuildCondBr(a->builder, condition, iterate, cat->block);

    // ---- BODY BLOCK ----
    LLVMPositionBuilderAtEnd(a->builder, iterate);

    invoke(expr_builder, exprs_iterate);

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

enode aether_e_if_else(aether a, array conds, array exprs, subprocedure cond_builder, subprocedure expr_builder) {
    int ln_conds = len(conds);
    verify(ln_conds == len(exprs) - 1 || 
           ln_conds == len(exprs), "mismatch between conditions and expressions");
    
    LLVMBasicBlockRef block = LLVMGetInsertBlock  (a->builder);
    LLVMBasicBlockRef merge = LLVMAppendBasicBlock(block, "ifcont");  // Merge block for the end of if-else chain

    // Iterate over the conditions and expressions
    for (int i = 0; i < ln_conds; i++) {
        // Create the blocks for "then" and "else"
        LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(block, "then");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(block, "else");

        // Build the condition
        Au cond_obj = conds->origin[i];
        enode cond_result = invoke(cond_builder, cond_obj);  // Silver handles the actual condition parsing and building
        LLVMValueRef condition = e_create(a, elookup("bool"), cond_result)->value;

        // Set the sconditional branch
        LLVMBuildCondBr(a->builder, condition, then_block, else_block);

        // Build the "then" block
        LLVMPositionBuilderAtEnd(a->builder, then_block);
        Au expr_obj = exprs->origin[i];
        enode expressions = invoke(expr_builder, expr_obj);  // Silver handles the actual block/statement generation
        LLVMBuildBr(a->builder, merge);

        // Move the builder to the "else" block
        LLVMPositionBuilderAtEnd(a->builder, else_block);
        block = else_block;
    }

    // Handle the fnal "else" (if applicable)
    if (len(exprs) > len(conds)) {
        Au else_expr = exprs->origin[len(conds)];
        invoke(expr_builder, else_expr);  // Process the final else block
        LLVMBuildBr(a->builder, merge);
    }

    // Move the builder to the merge block
    LLVMPositionBuilderAtEnd(a->builder, merge);

    // Return some enode or result if necessary (a.g., a enode indicating the overall structure)
    return enode(mod, a, au, au_lookup("none"), value, null);  // Dummy enode, replace with real enode if needed
}

enode aether_e_addr_of(aether a, enode expr, etype mdl) {
    return e_create(a, expr, mdl);
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
    
    etype        F = etype_canonical(expr);
    etype        T = etype_canonical(rtype);
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

static etype etype_deref(Au_t au) {
    return au->src ? au->src->user : null;
}

void src_init(aether a, Au_t m) {
    if (m->src)
        src_init(a, m->src);
    if (!m->user) {
        int mt = m->member_type;
        if (mt == AU_MEMBER_FUNC || mt == AU_MEMBER_ENUMV)
            m->user = enode(mod, a, au, m);
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
        push(res, src->user);
        if (src->context == src)
            break;
        src = src->context;
    }
    return reverse(res);
}

void aether_build_initializer(aether a, etype m) { }


static void aether_finalize_types(aether a, etype module_init_fn) {
    Au_t module_base = a->au_module;

    push_scope(a, module_init_fn);

    members(module_base, mem) {
        etype mdl = mem->user;
        if (!mdl || mdl->imported_from)
            continue;

        bool is_class_t  = is_class(mdl);
        bool is_struct_t = is_struct(mdl);
        bool is_enum_t   = is_enum(mdl);

        if (!is_class_t && !is_struct_t && !is_enum_t)
            continue;

        if (is_class_t && mdl->au->is_abstract)
            continue;

        // Count exported members (ignoring intern)
        int m_count = 0;
        members(mdl->au, f) {
            if (f->access_type != interface_intern)
                m_count++;
        }

        static_array members_array = static_array(alloc, m_count, assorted, true);

        // Build metadata for each member
        members(mdl->au, f) {
            if (f->access_type == interface_intern)
                continue;

            static_array fmeta = static_array(unmanaged, true, assorted, true);
            args(f, au_meta) {
                etype t = au_meta->user;
                push(fmeta, t);
            }

            map entry = map(hsize, 8, assorted, true);
            mset(entry, "meta",          fmeta);
            mset(entry, "src",           f->src);
            mset(entry, "ident",         string(f->ident));
            mset(entry, "offset",        _i32(f->offset));
            mset(entry, "elements",      _i32(f->elements));
            mset(entry, "typesize",      _i32(f->typesize));
            mset(entry, "isize",         _i32(f->isize));
            mset(entry, "operator_type", _i8(f->operator_type));
            mset(entry, "access_type",   _i8(f->access_type));
            mset(entry, "member_type",   _i8(f->member_type));
            mset(entry, "traits",        _u64(f->traits));
            mset(entry, "index",         _i64(f->index));

            // Attach function pointer or enum value
            if (is_func(f)) {
                enode fn = f->user;
                mset(entry, "value", enode(mod, a, value, fn->value, au, pointer(a, f)->au));
            }
            else if (is_enum_t && f->member_type == AU_MEMBER_ENUMV) {
                enode v = f->user;
                verify(isa(v) == typeid(enode), "expected enode for enumv");
                mset(entry, "value", v->value);
            } else
                mset(entry, "value", e_null(a, elookup("handle")));

            push(members_array, e_create(a, elookup("Au_t"), entry));
        }

        // Build type-level metadata
        array meta = array(unmanaged, true, assorted, true);
        if (is_class_t || is_struct_t) {
            args(mdl->au, field)
                push(meta, field);
        }

        int internal_size = 0;
        members(mdl->au, f) {
            if (f->access_type == interface_intern &&
                f->member_type == AU_MEMBER_PROP)
                internal_size += f->user->au->abi_size / 8;
        }

        map tmap = map(hsize, 8, unmanaged, true, assorted, true);
        mset(tmap, "ident",   string(mdl->au->ident));
        mset(tmap, "module",  e_fn_call(a, Au_find_member(au_lookup("Au"), "module", AU_MEMBER_FUNC)->user, a(a->name)));
        mset(tmap, "size",    _i32(mdl->au->abi_size / 8));
        mset(tmap, "traits",  _u64(mdl->au->traits));
        mset(tmap, "meta",    meta);
        mset(tmap, "isize",   _i32(internal_size));
        mset(tmap, "members", members_array);

        a->no_const = true;
        enode static_type = e_create(a, mdl->schema, tmap);
        a->no_const = false;

        enode target = e_typeid(a, mdl);
        e_memcpy(a, target, static_type, mdl->schema->au);

        // Patch parent type for classes
        if (is_class_t) {
            etype parent = mdl->au->context;
            enode parent_typeid = e_typeid(a, parent);
            e_assign(a, access(target, string("context")), parent_typeid, OPType__assign);
        }

        // Register type globally
        Au_t fn_push = Au_find_member(au_lookup("Au"), "push_type", AU_MEMBER_FUNC);
        e_fn_call(a, fn_push->user, a(target));
    }

    pop_scope(a);
}

static void build_module_initializer(aether a, etype module_init_fn) {
    Au_t module_base = a->au_module;

    push_scope(a, module_init_fn);

    members(module_base, au)
        if (au->member_type == AU_MEMBER_PROP && au->user->body)
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
    Au_t module_base = a->au_module;

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
        set_global_construct(a, module_init_fn);
        a->is_library = true;
        return;
    }

    // for apps, build int main(argc, argv)
    Au_t au_f_main = Au_register_member(a->au, "main", au_lookup("i32"),   AU_MEMBER_FUNC, 0);
    Au_t argc = Au_register_member(au_f_main,  "argc", au_lookup("i32"),   AU_MEMBER_ARG, 0);
    Au_t argv = Au_register_member(au_f_main,  "argv", au_lookup("cstrs"), AU_MEMBER_ARG, 0);
    array_qpush(&au_f_main->args, argc);
    array_qpush(&au_f_main->args, argv);

    etype main_fn = etype(mod, a, au, au_f_main);
    push_scope(a, main_fn);
    e_fn_call(a, module_init_fn, null);
    etype Au_engage = Au_find_member(au_lookup("Au"), "engage", AU_MEMBER_FUNC); // this is only for setting up logging now, and we should likely emit it after the global construction, and before the user initializers
    e_fn_call(a, Au_engage, a(argv));
    e_create(a, main_class, map()); // a loop would be nice, since none of our members will be ref+1'd yet
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

    while (au && au->src) {
        au = au->src;
        if (au->user && lltype(au->user))
            break;
    }
    return (au->user && lltype(au->user)) ? au->user : null;
}

// if given an enode, will always resolve the etype instance
etype etype_resolve(etype t) {
    Au_t au = t->au;
    while (au && !au->is_funcptr && au->src) {
        au = au->src;
        if (au->user && au->lltype)
            break;
    }
    return (au->user && lltype(au->user)) ? au->user : null;
}

// this is the declare (this comment stays)
none etype_init(etype t) {
    if (t->mod == null) t->mod = instanceof(t, typeid(aether));
    aether a = t->mod; // silver's mod will be a delegate to aether, not inherited
    if (!t->au) {
        // we must also unregister this during watch cycles, upon reinit
        t->au = Au_register(a->au, null, AU_MEMBER_NAMESPACE, 0);
    }

    Au_t    au  = t->au;
    bool  named = au && au->ident && strlen(au->ident);

    if (isa(t) == typeid(aether) || isa(t)->context == typeid(aether)) {
        a = (aether)t;
        verify(a->source && len(a->source), "no source provided");
        a->name = a->name ? a->name : stem(a->source);
        au = t->au = Au_register_module(a->name->chars);
        au->user = hold(t);
        return;
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
            Au_t new_mem      = Au_register(t->au, au_mem->ident, au_mem->member_type, au_mem->traits);
            new_mem->src      = au_mem->src;
            new_mem->isize    = au_mem->isize;
            new_mem->elements = au_mem->elements;
            new_mem->typesize = au_mem->typesize;
            new_mem->abi_size = au_mem->abi_size;
            members(au_mem, mem) array_qpush(&new_mem->members, mem);
            args(au_mem, mem)    array_qpush(&new_mem->args,    mem);
            new_mem->context = t->au; // copy entire member and reset for our for context
        }

        // upon implement, we can register the static_value on etype of schema
        // this is the address of the &type_i.type member
        
    }
    
    if (!au->member_type)
        au->member_type = AU_MEMBER_TYPE;
    au->user = hold(t);

    if (is_func(au)) {
        etype fn = au->user;
        // this is when we are binding to .c modules that contain methods
        fn->is_elsewhere = !fn->cgen && (!fn->body || len(fn->body) == 0);
        if (fn->is_elsewhere && !fn->au->alt) {
            etype rec = context_struct(a);
            if (!rec) rec = context_class(a);
            if (rec)
                fn->au->alt = hold(f(string, "%o_%s", rec, fn->au->ident));
        }

        // populate arg types for function
        int n_args = fn->au->args.count;
        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));
        int index  = 0;
        args(fn->au, arg)
            arg_types[index++] = lltype(arg);

        au->lltype      = LLVMFunctionType(
            au->rtype ? lltype(au->rtype) : LLVMVoidType(),
            arg_types, au->args.count, au->is_vargs);
        etype_ptr(a, fn->au);
        free(arg_types);

        if (au->is_mod_init) {
            etype f = au->user;
            aether_finalize_types(a, f);
            build_module_initializer(a, f);
            build_entrypoint(a, f);
        }
    } else if (is_func_ptr(au)) {
        etype fn = au->user;

        int n_args = au->args.count;
        LLVMTypeRef* arg_types = calloc(4 + n_args, sizeof(LLVMTypeRef));
        int index = 0;
        args(au, arg)
            arg_types[index++] = lltype(arg);

        LLVMTypeRef fn_ty = LLVMFunctionType(
            au->rtype ? lltype(au->rtype) : LLVMVoidType(),
            arg_types, au->args.count, au->is_vargs);

        au->lltype = LLVMPointerType(fn_ty, 0);
        free(arg_types);
    } else if (au && au->is_pointer && au->src && !au->src->is_primitive) {
        au->lltype = LLVMPointerType(au->src->lltype, 0);
    } else if (is_rec(t) || au->is_union || au == typeid(Au_t)) {
        au->lltype = named ? LLVMStructCreateNamed(a->module_ctx, strdup(au->ident)) : null;
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
        b->ptr  = t;
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

    push(a->registry, t); // we only 'clear' our registry and the linked Au_t's
}

none etype_implement(etype t) {
    //printf("t pointer = %p\n", t);
    Au_t    au = t->au;
    aether a  = t->mod;


    if (t->is_implemented) return;
    t->is_implemented = true;

    static bool is_ARef = false;
    if (!is_ARef && au && au->ident && strcmp(au->ident, "ARef") == 0) {
        int test2 = 2;
        test2    += 2;
        is_ARef = true;
    }

    if (au && au->src && au->src->user && !is_prim(au->src) && isa(au->src->user) == typeid(etype))
        etype_implement(au->src->user);

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

        if (au->ident && strcmp(au->ident, "__darwin_pthread_handler_rec") == 0) {
            int test2 = 2;
            test2    += 2;
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
                    if (src->ident && strcmp(src->ident, "shape") == 0) {
                        int test2 = 2;
                        test2    += 2;
                    }
                    etype_implement(src->user);
                    // get largest union member
                    members[index] = lltype(src);
                    verify(members[index], "no lltype found for member %s.%s", au->ident, m->ident);
                    printf("verifying abi size of %s\n", m->ident);
                    int abi_member  = LLVMABISizeOfType(a->target_data, members[index]);
                    verify(abi_member, "type has no size");
                    if (au->is_union && src->abi_size > ilargest) {
                        largest  = members[index];
                        ilargest = src->abi_size;
                    }
                    index++;
                }
            }
        }

        etype_ptr(a, t->au);

        etype mt = null;
        if (is_class(t) && (t->au->traits & AU_TRAIT_SCHEMA) == 0) {
            printf("creating schema for %s\n", t->au->ident);
            // schema must have it's static_value set to the instance
            etype schema = etype(mod, a, au, t->au, is_schema, true);
            t->schema = schema;
            print("schema output for %o", schema);
            for (int i = 0; i < schema->au->members.count; i++) {
                Au_t m = schema->au->members.origin[i];
                if (m->member_type == AU_MEMBER_PROP) {
                    if (m->elements)
                        printf("\t%s %s[%i]\n", m->src->ident, m->ident, m->elements);
                    else
                        printf("\t%s %s\n", m->src->ident, m->ident);
                }
            }
            
            mt = etype_ptr(a, schema->au);
            mt->au->is_typeid = true;

            Au_t au_type = au_lookup("Au");
            verify(au_type->ptr, "expected ptr type on Au");

            // we need to create the %o_i instance inline struct
            string n = f(string, "%s_info", au->ident);
            Au_t type_info = Au_register_type(a->au, n->chars, AU_TRAIT_STRUCT);
            Au_t type_h = Au_register_member(type_info, "info",
                au_type, AU_MEMBER_PROP, 0);
            Au_t type_f = Au_register_member(type_info, "type",
                schema->au, AU_MEMBER_PROP, AU_TRAIT_INLAY);
            
            // needs to create the global value in here
            // the enode init will see a type, a member, no value, then create an instance
            static int inc = 0;
            inc++;
            if (inc == 5) inc = 1;
            char* v = inc == 1 ? "a1" : inc == 2 ? "a2" : inc == 3 ? "a3" : "a4";
            LLVMValueRef G =
                LLVMAddGlobal(a->module, LLVMInt32Type(), strdup(v));

            //enode schema_i = enode(mod, a, au, type_info,
            //    name, f(string, "%s_i", au->ident)); // read from is_Au_import; we need to indicate if we are importing, or creating this
            // now we access the addr of type location by access
            //return;

            //t->type_id = access(schema_i, string("type"));
            
        } else if (count == 0)
             mt = au_lookup("u8")->user;
        if (mt) {
            printf("%p\n", lltype(mt));
            members[count++] = lltype(mt);
        }
        if (au->is_union) {
            count = 1;
            members[0] = largest;
        }
        
        LLVMStructSetBody(au->lltype, members, count, 1);

    } else if (is_enum(t)) {
        Au_t et = au->src;
        static bool enum_processing = false;
        if (enum_processing) return; // safe for global usage
        enum_processing = true;
        verify(et, "expected source type for enum %s", au);
        for (int i = 0; i < au->members.count; i++) {
            Au_t   m = au->members.origin[i]; // has ident, and value set (required)
            if (m->user) return;
            verify(m->value, "no value set for enum %s:%s", au->ident, m->ident);
            string n = f(string, "%s_%s", au->ident, m->ident);
            
            LLVMValueRef G = LLVMAddGlobal(a->module, au->lltype, n->chars);
            LLVMSetLinkage(G, LLVMInternalLinkage);
            LLVMSetGlobalConstant(G, 1);
            LLVMValueRef init;
            
            if (et == typeid(i32))
                init = LLVMConstInt(au->lltype, *((i32*)m->value), 1);
            else if (et == typeid(u32))
                init = LLVMConstInt(au->lltype, *((u32*)m->value), 0);
            else if (et == typeid(i16))
                init = LLVMConstInt(au->lltype, *((i16*)m->value), 1);
            else if (et == typeid(u16))
                init = LLVMConstInt(au->lltype, *((u16*)m->value), 0);
            else if (et == typeid(i8))
                init = LLVMConstInt(au->lltype, *((i8*)m->value), 1);
            else if (et == typeid(u8))
                init = LLVMConstInt(au->lltype, *((u8*)m->value), 0);
            else if (et == typeid(i64))
                init = LLVMConstInt(au->lltype, *((i64*)m->value), 1);
            else if (et == typeid(u64))
                init = LLVMConstInt(au->lltype, *((u64*)m->value), 0);
            else 
                fault("unsupported enum value: %s", et->ident);
            
            enode evalue = enode(mod, a, au, t->au, value, init);
            m->user = hold(enode(mod, a, value, evalue, au, m));
            LLVMSetInitializer(G, init);
        }
        enum_processing = false;
    } else if (is_func(t)) {
        Au_t cl = isa(t);
        verify(cl == typeid(enode), "expected enode");
        //string n = is_rec(t->au->context) ?
        //    f(string, "%s_%s", t->au->context->ident, t->au->ident) : string(t->au->ident);
        cstr n = t->au->alt ? t->au->alt : t->au->ident;
        verify(n, "no name given to function");
        ((enode)t)->value = LLVMAddFunction(a->module, n, au->lltype);
    }

    if (!au->is_void && au->member_type != AU_MEMBER_FUNC && !is_opaque(au) && !is_func(au) && au->lltype) {
        if (au->elements) {
            au->lltype = LLVMArrayType(au->lltype, au->elements);
        }
        au->abi_size   = LLVMABISizeOfType(a->target_data, au->lltype)      * 8;
        au->align_bits = LLVMABIAlignmentOfType(a->target_data, au->lltype) * 8;
    }
}

enode aether_context_func(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_FUNC)
            return ctx->user;
    }
    return null;
}

etype aether_context_class(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_class)
            return ctx->user;
    }
    return null;
}

etype aether_context_struct(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && ctx->is_struct)
            return ctx->user;
    }
    return null;
}

etype aether_context_model(aether a, Au_t type) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        if (ctx->member_type == AU_MEMBER_TYPE && isa(ctx->user) == type)
            return ctx->user;
    }
    return null;
}

etype aether_context_record(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
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
    if (au->src && (au->src->is_class || au->src->is_struct))
        push(a->lexical, au->src);
    else
        push(a->lexical, au);
}

void aether_import_models(aether a, Au_t ctx) {
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

    for (int filter = 0; filter < 8; filter++) {
        struct filter* ff = &filters[filter];
        
        for (num i = 0; i < ctx->members.count; i++) {
            // ARef schema creation causes IR error
            if (filter == 2) {
                return;
            }
            Au_t m = ctx->members.origin[i];
            bool proceed = (ff->has_bits & m->traits) == ff->has_bits && 
                        (ff->not_bits & m->traits) == 0;
            
            if (proceed) {
                Au_t m_isa = isa(m);
                print("init %o", m);
                if (ff->init || ff->impl) {
                    src_init(a, m);
                }
                if (ff->impl) {
                    if (filter == 1 && i == 50) {
                        int test2 = 2;
                        test2    += 2;
                    }
                    etype_implement((etype)m->user);
                    if (filter == 1 && i == 50) {
                        int test2 = 2;
                        test2    += 2;
                        return;
                    }
                }
            }
        }
    }
}

void aether_import_Au(aether a, path lib) {
    a->current_inc   = lib ? lib : path("Au");
    a->is_Au_import  = true;
    string  lib_name = lib ? stem(lib) : null;
    Au_t    au_module = null;

    // register and push new module scope if we are loading from library
    if (lib) {
        au_module = Au_register_module(copy_cstr(lib_name->chars));
        push_scope(a, au_module);
    } else
        au_module = Au_global();

    Au_t base = au_lookup("Au_t");
    if (lib) {
        handle f = dlopen(cstring(lib), RTLD_NOW);
        verify(f, "shared-lib failed to load: %o", lib);
        push(a->libs, f);
    }

    Au_t str = typeid(string);
    Au_t au    = typeid(Au);
    au->user   = etype(mod, a, au, au);
    Au_t au_t  = typeid(Au_t);
    au_t->user = etype(mod, a, au, au_t);
    etype_ptr(a, au); // Au should be pointer, and we have it as struct; we need to load _Au as what we do, and point to it with Au
    etype_ptr(a, au_t);

    aether_import_models(a, au_module);

    if (!lib) {
        // this causes a name-related error in IR
        //Au_t main_cl = Au_register_member(au_module, "main", null, AU_MEMBER_TYPE, AU_TRAIT_CLASS | AU_TRAIT_ABSTRACT);
        //main_cl->context = typeid(Au);
        //main_cl->user = etype(mod, a, au, main_cl);
        //implement(main_cl->user);
    }
}

void aether_llflag(aether a, symbol flag, i32 ival) {
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32Type(), ival, 0));

    char sflag[64];
    memcpy(sflag, flag, strlen(flag) + 1);
    LLVMAddModuleFlag(a->module, LLVMModuleFlagBehaviorError, sflag, strlen(sflag), v);
}


bool aether_emit(aether a, ARef ref_ll, ARef ref_bc) {
    path* ll = ref_ll;
    path* bc = ref_bc;
    cstr err = NULL;

    path c = path_cwd();

    *ll = form(path, "%o.ll", a);
    *bc = form(path, "%o.bc", a);

    LLVMValueRef G =
        LLVMAddGlobal(a->module, LLVMInt32Type(), "testing112233");

    LLVMDumpModule(a->module);

    LLVMPrintModuleToFile(a->module, "crashing.ll", &err);

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
    push_scope(a, Au_global());
    push_scope(a, a->au);

    // todo: we must unregister everything in Au minus its base

} 

none aether_test_write(aether a) {
    int err = 0;
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

    a->registry       = array(alloc, 256, assorted, true);
    a->libs           = array();
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
    
    a->target_data = LLVMCreateTargetDataLayout(a->target_machine);
    a->compile_unit = LLVMDIBuilderCreateCompileUnit(
        a->dbg_builder, LLVMDWARFSourceLanguageC, a->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);
    a->au->llscope = a->compile_unit;
    a->builder = LLVMCreateBuilderInContext(a->module_ctx);

    // push our module space to the scope
    Au_t g = Au_global();
    verify(g,           "globals not registered");
    verify(a->au,       "no module registered for aether");
    verify(g != a->au,  "aether using global module");

    push_scope(a, g);
    import_Au(a, null);

    // i am an ass.
    a->au->is_namespace = true; // for the 'module' namespace at [1], i think we dont require the name.. or, we set a trait
    a->au->is_nameless  = false; // we have no names, man. no names. we are nameless! -cereal
    push_scope(a, a->au);

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

Au_t etype_cast_string(etype t) {
    return t->au ? string(t->au->ident) : string("[no-type]");
}

Au_t etype_cast_Au_t(etype t) {
    return t->au;
}

none enode_init(enode n) {
    aether a = n->mod;
    bool is_const = n->literal != null;
    
    return;

    etype_implement(n);
    
    if (!n->member && len(n->name) > 0) {
        n->registered = true; // we have control
        n->member = Au_register_member(
            top_scope(a), n->name->chars, n->au, AU_MEMBER_VAR,
            is_const ? AU_TRAIT_CONST : 0);

        // if module member
        if (is_func(n->member->context)) {
            enode fn = n->member->context;
            n->value = LLVMGetParam(fn->value, n->arg_index);
            // find the argument index
        } else if (!n->member->context->context) {
            // mod->is_Au_import indicates this is coming from a lib, or we are making a new global
            char* str = calloc(1, 32);
            strcpy(str, "test22");
            static int inc = 0;
            
            for (int i = 0; i < inc; i++)
                str[strlen(str) - 1]++;

            inc++;

            char* v = inc == 1 ? "a1" : inc == 2 ? "a2" : inc == 3 ? "a3" : "a4";
            LLVMValueRef G =
                LLVMAddGlobal(a->module, LLVMInt32Type(), strdup(v));

            int test2 = 2;
            test2    += 2;
            int err = 0;
            LLVMPrintModuleToFile(a->module, "crashing.ll", &err);
            
            //LLVMSetLinkage(G, a->is_Au_import ? LLVMExternalLinkage : LLVMInternalLinkage);
            //n->value = G;
        }
    }
}

none enode_dealloc(enode a) {
    if (a->registered) {
        // need to do a bit more since this object is a generic Au-type
        Au_dealloc_type(a->member);
    }
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
    print_all(a, "initial", initial);
    array rr = expand_tokens(a, initial, expanding);
    print_all(a, "expand", rr);
    string tstr = string(alloc, 64);
    each (rr, token, t) { // since literal processing is in token_init we shouldnt need this!
        if (tstr->count > 0)
            append(tstr, " ");
        concat(tstr, t);
    }
    return (array)tokens(
        target, a, input, tstr, parser, a->parse_f);
        // we call this to make silver compatible tokens
}

// return tokens for function content (not its surrounding def)
array codegen_generate_fn(codegen a, Au_t f, array query) {
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
    etype mdl_cstr = elookup("cstr");
    etype mdl_i32  = elookup("i32");
    each(schema, Au, obj) {
        enode n = instanceof(obj, typeid(enode));
        if (n) {
            e_print_node(a, n);
        } else {
            string s = instanceof(obj, typeid(string));
            verify(s, "invalid type data");
            enode n_str = e_operand(a, s, null);
            e_print_node(a, n_str);
        }
    }

    va_end(args);
    free(arg_nodes);
    free(buf);
}

/*
    // there is no example use case of membership registration of vars
    Au_t mem = Au_register_member(top_scope(a), mdl->au, AU_MEMBER_VAR,
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

    if (Lt && Rt) { // ifdef functionality
        bool Lr = (Lt->traits & AU_TRAIT_REALISTIC) != 0;
        bool Rr = (Rt->traits & AU_TRAIT_REALISTIC) != 0;
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
        enode res = value(elookup("i32"), LLVMConstInt(LLVMInt32Type(), diff, 0));
        res->literal = hold(literal);
        return res;
    }

    a->is_const_op = false;
    if (a->no_build) return e_noop(a, elookup("i32"));

    enode  func = context_func(a);
    verify(func, "non-const compare must be in a function");

    if (Lc || Rc) {
        if (!Lc) {
            enode t = L;
            L = R;
            R = t;
        }
        Au_t eq = Au_find_member(L->au, "compare", AU_MEMBER_FUNC);
        if (eq) {
            verify(eq->rtype == au_lookup("i32"), "compare function must return i32, found %o", eq->rtype);
            // check if R is compatible with argument
            // if users want to allow different data types, we need to make the argument more generic
            // this is better than overloads since the code is in one place
            return e_fn_call(a, eq->user, a(L, R));
        } else {
            LLVMValueRef lt = LLVMBuildICmp(a->builder, LLVMIntULT, L->value, R->value, "cmp_class-lt");
            LLVMValueRef gt = LLVMBuildICmp(a->builder, LLVMIntUGT, L->value, R->value, "cmp_class-gt");

            LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
            LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
            LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);

            // if L<R → -1, else if L>R → 1, else 0
            LLVMValueRef lt_val = LLVMBuildSelect(a->builder, lt, neg_one, zero, "lt_val");
            LLVMValueRef result = LLVMBuildSelect(a->builder, gt, pos_one, lt_val, "cmp_f");
            return value(elookup("i32"), result);
        }
    } else if (Ls || Rs) {
        // iterate through struct members, checking against each with recursion

        LLVMValueRef result = LLVMConstInt(LLVMInt32Type(), 0, 0);
        LLVMBasicBlockRef block      = LLVMGetInsertBlock(a->builder);
        LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(block, "cmp_exit");

        // incoming edges for phi
        array phi_vals   = array(alloc, 32, unmanaged, true); // LLVMValueRef
        array phi_blocks = array(alloc, 32, unmanaged, true); // LLVMBasicBlockRef

        verify(Ls && Rs && (canonical(L) == canonical(R)), "struct type mismatch %o != %o",
            L->au->ident, R->au->ident);
        members(L->au, mem) {
            // get the struct member at [ mem->index ]
            if (is_func(mem))
                continue;

            LLVMValueRef lv  = LLVMBuildExtractValue(a->builder, L->value, mem->index, "lv");
            LLVMValueRef rv  = LLVMBuildExtractValue(a->builder, R->value, mem->index, "rv");
            enode        le  = value(mem->user, lv);
            enode        re  = value(mem->user, rv);
            enode        cmp = e_cmp(a, le, re);

            LLVMValueRef is_nonzero = LLVMBuildICmp(
                a->builder, LLVMIntNE, cmp->value,
                LLVMConstInt(LLVMInt32Type(), 0, 0),
                "cmp_nonzero");

            // create blocks for next-field vs exit
            LLVMBasicBlockRef next_block = LLVMAppendBasicBlock(block, "cmp_next");
            LLVMBasicBlockRef exit_here  = LLVMAppendBasicBlock(block, "cmp_exit_here");

            // branch: if nonzero → exit_here, else → next
            LLVMBuildCondBr(a->builder, is_nonzero, exit_here, next_block);

            // in exit_here: set result = cmp and jump to exit_block
            LLVMPositionBuilderAtEnd(a->builder, exit_here);

            push(phi_vals,   cmp->value);
            push(phi_blocks, exit_here);

            LLVMBuildBr(a->builder, exit_block);

            // move to next_block for the next member
            LLVMPositionBuilderAtEnd(a->builder, next_block);
        }

        push(phi_vals,   LLVMConstInt(LLVMInt32Type(), 0, 0));
        push(phi_blocks, LLVMGetInsertBlock(a->builder)); // fallthrough

        // at the end, build a PHI in exit_block that merges:
        //   - 0 (from fallthrough if all equal)
        //   - cmp (from whichever exit_here branch)
        LLVMPositionBuilderAtEnd(a->builder, exit_block);
        LLVMValueRef phi = LLVMBuildPhi(a->builder, LLVMInt32Type(), "cmp_phi");
        LLVMAddIncoming(phi, vdata(phi_vals), vdata(phi_blocks), len(phi_vals));

        return value(elookup("i32"), phi);

    } else if (canonical(L) != canonical(R)) {
        if (Lr && Rr) {
            // convert to highest bit width
            if (L->au->abi_size > R->au->abi_size) {
                R = e_create(a, L, R);
            } else {
                L = e_create(a, R, L);
            }
        } else if (Lr) {
            R = e_create(a, L, R);
        } else if (Rr) {
            L = e_create(a, L, R);
        } else {
            if (L->au->abi_size > R->au->abi_size) {
                R = e_create(a, L, R);
            } else {
                L = e_create(a, R, L);
            }
        }
    }

    if (Lr || Rr) {
        // we need a select with a less than, equal to, else
        // for float we must return -1 0 and 1

        LLVMValueRef lt = LLVMBuildFCmp(a->builder, LLVMRealOLT, L->value, R->value, "cmp_lt");
        LLVMValueRef gt = LLVMBuildFCmp(a->builder, LLVMRealOGT, L->value, R->value, "cmp_gt");

        LLVMValueRef neg_one = LLVMConstInt(LLVMInt32Type(), -1, true);
        LLVMValueRef pos_one = LLVMConstInt(LLVMInt32Type(),  1, false);
        LLVMValueRef zero    = LLVMConstInt(LLVMInt32Type(),  0, false);

        // if L<R → -1, else if L>R → 1, else 0
        LLVMValueRef lt_val = LLVMBuildSelect(a->builder, lt, neg_one, zero, "lt_val");
        LLVMValueRef result = LLVMBuildSelect(a->builder, gt, pos_one, lt_val, "cmp_f");

        return value(elookup("i32"), result);
    }

    return value(elookup("i32"), LLVMBuildSub(a->builder, L->value, R->value, "diff-i"));
}


enode aether_compatible(aether a, etype r, string n, AFlag f, array args) {
    members (r->au, mem) {
        enode fn = is_func(mem) ? mem->user : null;
        // must be function with optional name check
        if (!fn || (((f & mem->traits) != f) && (!n || !eq(n, mem->ident))))
            continue;
        if (mem->args.count != len(args))
            continue;
        
        bool compatible = true;
        int ai = 0;
        args(mem, au) {
            etype fr = args->origin[ai];
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
    etype mdl = mem;
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
    enode f_dealloc = Au_find_member(mem->au, "dealloc", AU_MEMBER_FUNC)->user;
    if (f_dealloc) {
        e_fn_call(e, f_dealloc, a(mem));
    }
    LLVMBuildFree(e->builder, ref_ptr);
    LLVMBuildBr(e->builder, no_free_block);

    // No-free block: Continue without freeing
    LLVMPositionBuilderAtEnd(e->builder, no_free_block);
}


Au_t aether_pop_scope(aether a) {
    statements st = instanceof(top_scope(a)->user, typeid(statements));
    if (st) {
        members(st->au, mem) {
            if (instanceof(mem->user, typeid(enode)))
                release((enode)mem->user);
        }
    }
    enode prev_fn = context_func(a);

    if (prev_fn && !a->no_build)
        prev_fn->last_dbg = LLVMGetCurrentDebugLocation2(
            a->builder);

    pop(a->lexical);
    a->top = last_element(a->lexical);

    enode fn = context_func(a);
    if (fn && (fn != prev_fn) && !a->no_build) {
        LLVMPositionBuilderAtEnd(a->builder, fn->entry);
        LLVMSetCurrentDebugLocation2(a->builder, fn->last_dbg);
    }
    return a->top;
}


etype aether_return_type(aether a) {
    for (int i = len(a->lexical) - 1; i >= 0; i--) {
        Au_t ctx = a->lexical->origin[i];
        if (isa(ctx->user) == typeid(enode) && ctx->rtype)
            return ctx->rtype->user;
    }
    return null;
}

// ident is optional, and based on 
enode aether_function(aether a, string ident, array args, u8 member_type, u32 traits, u8 operator_type) {
    Au_t context = top_scope(a);
    Au_t au = Au_register(context, ident->chars, AU_MEMBER_FUNC, traits);
    each(args, Au, arg) {
        Au_t a = au_arg(arg);
        array_qpush(&au->args, a);
    }
    enode n = enode(mod, a, au, au);
    etype_implement(n); // creates llvm function value for enode
    return n;
}

etype aether_record(aether a, etype based, string ident, u32 traits, array meta) {
    Au_t context = top_scope(a);
    Au_t au = Au_register(context, ident->chars, AU_MEMBER_TYPE, traits);
    au->context = based ? based->au : elookup("Au")->au;
    each(meta, Au, arg) {
        Au_t a = au_arg(arg);
        array_qpush(&au->meta, a);
    }
    etype n = etype(mod, a, au, au);
    etype_implement(n);
    return n;
}

enode aether_module_initializer(aether a) {
    if (a->fn_init) return a->fn_init;
    verify(a, "model given must be module (aether-based)");

    enode init = function(a,
        string("initializer"), array(),
        AU_MEMBER_FUNC, AU_TRAIT_MODINIT, OPType__undefined);

    etype_implement(init);
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
define_class(enode,      etype) // not a member unless member is set (direct, or created with name)

define_class(aether,     etype)

define_class(aclang_cc,  Au)
define_class(codegen,    Au)
define_class(code,       Au)
define_class(static_array, array)