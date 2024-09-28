#include <silver>

#define string(...) new(string, ## __VA_ARGS__) 

#define isilver(M,...) silver_##M(mod,    ## __VA_ARGS__)
#define    idef(M,...)    def_##M(ident,  ## __VA_ARGS__)
#define    idim(M,...)    dim_##M(member, ## __VA_ARGS__)

#undef  peek
#define tokens(...)     call(mod->tokens, __VA_ARGS__) /// no need to pass around the redundant tokens args when the class supports a stack


static array operators;

void init() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    print("LLVM Version: %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);
    
    operators = map_of(
        "+",        str("add"),
        "-",        str("sub"),
        "*",        str("mul"),
        "/",        str("div"),
        "||",       str("or"),
        "&&",       str("and"),
        "^",        str("xor"),
        ">>",       str("right"),
        "<<",       str("left"),
        ":",        str("assign"),
        "=",        str("assign"),
        "+=",       str("assign_add"),
        "-=",       str("assign_sub"),
        "*=",       str("assign_mul"),
        "/=",       str("assign_div"),
        "|=",       str("assign_or"),
        "&=",       str("assign_and"),
        "^=",       str("assign_xor"),
        ">>=",      str("assign_right"),
        "<<=",      str("assign_left"),
        "==",       str("compare_equal"),
        "!=",       str("compare_not"),
        "%=",       str("mod_assign"),
        "is",       str("is"),
        "inherits", str("inherits"), null
    );
}

module_init(init)

string type_key(LLVMTypeRef type) {
    return format("%p", type);
}

LLVMMetadataRef dim_dbg(dim member) {
    silver mod = member->mod;
    if (member->dbg)
        return member->dbg;
    LLVMMetadataRef t = member->def->dbg;
    int ptr_size = LLVMPointerSize(mod->target_data) * 8;
    for (int ii = 0; ii < member->depth; ii++) {
        t = LLVMDIBuilderCreatePointerType(
            mod->dbg, t, ptr_size, 0, 0, NULL, 0
        );
    }
    member->dbg = t;
    return t;
}

void silver_tokens(silver mod) {
    print("tokens: %o %o %o %o %o ...", 
        read(mod->tokens, 0), read(mod->tokens, 1),
        read(mod->tokens, 2), read(mod->tokens, 3),
        read(mod->tokens, 4), read(mod->tokens, 5));
}

path silver_source_path(silver mod) {
    return mod->source_path;
}

def silver_def(silver mod, string key) {
    return get(mod->defs, key);
}

dim silver_member(silver mod, string key) {
    return get(mod->members, key);
}

map silver_top_members(silver mod) {
    assert (mod->member_stack->len, "stack is empty");
    return last(mod->member_stack);
}

/// lookup value ref for member in stack
dim silver_member_stack_lookup(silver mod, string name) {
    for (int m = mod->member_stack->len - 1; m >= 0; m--) {
        map members = mod->member_stack->elements[m];
        dim f = get(members, name);
        if (f) return f;
    }
    return null;
}

map silver_push_member_stack(silver mod) {
    map members = new(map, hsize, 16);
    push(mod->member_stack, members);
    return members;
}

void silver_pop_member_stack(silver mod) {
    pop(mod->member_stack);
}

void silver_define_C99(silver mod) {
    map      defs = mod->defs = new(map, hsize, 64);
    set(defs, str("bool"),    new(def, mod, mod, name, str("bool"),   mdl, model_bool, imported, typeid(bool)));
    set(defs, str("i8"),      new(def, mod, mod, name, str("i8"),     mdl, model_i8,   imported, typeid(i8)));
    set(defs, str("i16"),     new(def, mod, mod, name, str("i16"),    mdl, model_i16,  imported, typeid(i16)));
    set(defs, str("i32"),     new(def, mod, mod, name, str("i32"),    mdl, model_i32,  imported, typeid(i32)));
    set(defs, str("i64"),     new(def, mod, mod, name, str("i64"),    mdl, model_i64,  imported, typeid(i64)));
    set(defs, str("u8"),      new(def, mod, mod, name, str("u8"),     mdl, model_u8,   imported, typeid(u8)));
    set(defs, str("u16"),     new(def, mod, mod, name, str("u16"),    mdl, model_u16,  imported, typeid(u16)));
    set(defs, str("u32"),     new(def, mod, mod, name, str("u32"),    mdl, model_u32,  imported, typeid(u32)));
    set(defs, str("u64"),     new(def, mod, mod, name, str("u64"),    mdl, model_u64,  imported, typeid(u64)));
    set(defs, str("f32"),     new(def, mod, mod, name, str("f32"),    mdl, model_f32,  imported, typeid(f32)));
    set(defs, str("f64"),     new(def, mod, mod, name, str("f64"),    mdl, model_f64,  imported, typeid(f64)));
    set(defs, str("void"),    new(def, mod, mod, name, str("void"),   mdl, model_void, imported, typeid(none)));
    set(defs, str("symbol"),  new(def, mod, mod, name, str("symbol"), mdl, model_cstr, imported, typeid(symbol)));
    set(defs, str("cstr"),    new(def, mod, mod, name, str("cstr"),   mdl, model_cstr, imported, typeid(cstr)));
    set(defs, str("int"),     get(defs, str("i64")));
    set(defs, str("uint"),    get(defs, str("u64")));
}


bool silver_build_dependencies(silver mod) {
    each(mod->imports, Import, im) {
        process(im);
        switch (im->import_type) {
            case ImportType_source:
                if (len(im->main_symbol))
                    push(mod->main_symbols, im->main_symbol);
                each(im->source, string, source) {
                    // these are built as shared library only, or, a header file is included for emitting
                    if (call(source, has_suffix, ".rs") || call(source, has_suffix, ".h"))
                        continue;
                    string buf = format("%o/%s.o", mod->install, source);
                    push(mod->compiled_objects, buf);
                }
            case ImportType_library:
            case ImportType_project:
                concat(mod->libraries_used, im->links);
                break;
            case ImportType_includes:
                break;
            default:
                verify(false, "not handled: %i", im->import_type);
        }
    }
    return true;
}

LLVMValueRef silver_build_statements(silver mod) {
    LLVMValueRef result = null;
    return result;
}

LLVMValueRef silver_parse_expression(silver mod);

bool dim_type_is(dim member, def t) {
    def tt = member->def;
    while (tt) {
        if (t == tt) return true;
        if (!tt->origin) break;
        tt = tt->origin->def;
    }
    return false;
}

LLVMValueRef silver_parse_return(silver mod) {
    dim  rtype   = silver_member_stack_lookup(mod, str("#rtype")); // stored when we start building function
    def  t_void  = get(mod->defs, str("void"));
    bool is_void = dim_type_is(rtype, t_void);
    tokens (consume);
    return (is_void) ?
        LLVMBuildRetVoid(mod->builder) :
        LLVMBuildRet    (mod->builder, isilver(parse_expression));
}

LLVMValueRef silver_parse_break(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_for(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_while(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_if_else(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

LLVMValueRef silver_parse_do_while(silver mod) {
    tokens(consume);
    LLVMValueRef vr = null;
    return null;
}

def silver_type_from_llvm(silver mod, LLVMTypeRef type) {
    string key = type_key(type);
    def    res = get(mod->type_refs, key);
    assert(res, "expected associated type");
    return res;
}

def preferred_type(silver mod, def t0, def t1) {
    if (t0 == t1) return t0;
    bool f0 = t0->mdl == model_f32 || t0->mdl == model_f64;
    bool f1 = t1->mdl == model_f32 || t1->mdl == model_f64;
    if (f0) {
        if (f1)
            return (t1->mdl == model_f64) ? t1 : t0;
        return t0;
    }
    if (f1)
        return t1;
    if (t0->mdl > t1->mdl)
        return t0;
    return t1;
}

typedef LLVMValueRef(*builder_fn)(silver, def, def, def, LLVMValueRef, LLVMValueRef);
typedef LLVMValueRef(*parse_fn)(silver, symbol, symbol, builder_fn, builder_fn);

#define resolve_type(llvm_type_ref) isilver(type_from_llvm, llvm_type_ref);

LLVMValueRef parse_ops(
        silver mod, parse_fn descent, symbol op0, symbol op1, builder_fn b0, builder_fn b1) {
    LLVMValueRef left = descent(mod, op0, op1, b0, b1);
    while(tokens(next_is, op0) || tokens(next_is, op1)) {
        tokens(consume);
        LLVMValueRef right = descent(mod, op0, op1, b0, b1);
        bool         use0 = tokens(next_is, op0);
        symbol    op_code = use0 ? op0 : op1;
        builder_fn    bfn = use0 ? b0  : b1;

        assert (contains(mod->operators, str(op_code)), "op (%s) not registered", op_code);
        string  op_name   = get(mod->operators, str(op_code));
        def     left_type = resolve_type(left);
        dim     method    = get(left_type->members, op_name);
        def     r_left    = resolve_type(LLVMTypeOf(left));
        def     r_right   = resolve_type(LLVMTypeOf(right));

        if (method) { /// and convertible(r_right, method.args[0].type):
            LLVMValueRef args[2] = { left, right };
            left = LLVMBuildCall2(mod->builder, method->def->type, method->value, args, 2, "operator");
        } else {
            def  type_out = preferred_type(mod, r_left, r_right); /// should work for primitives however we must handle more in each
            left          = bfn(mod, type_out, r_left, r_right, left, right); 
        }
    }
    return left;
}

LLVMValueRef op_is(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert(member->def->mdl == model_bool, "inherits operator must return a boolean type");
    assert(LLVMGetTypeKind(LLVMTypeOf(v0))  == LLVMFunctionTypeKind &&
           LLVMGetTypeKind(LLVMTypeOf(v1)) == LLVMFunctionTypeKind, 
           "is operator expects function type or initializer");
    bool equals = t0 == t1;
    return LLVMConstInt(LLVMInt1Type(), equals, 0);
}

LLVMValueRef op_inherits(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert(member->def->mdl == model_bool, "inherits operator must return a boolean type");
    assert(LLVMGetTypeKind(LLVMTypeOf(v0))  == LLVMFunctionTypeKind &&
           LLVMGetTypeKind(LLVMTypeOf(v1)) == LLVMFunctionTypeKind, 
           "is operator expects function type or initializer");
    bool      equals = t0 == t1;
    LLVMValueRef yes = LLVMConstInt(LLVMInt1Type(), 1, 0);
    LLVMValueRef no  = LLVMConstInt(LLVMInt1Type(), 0, 0);
    def cur = t0;
    while (cur) {
        if (cur == t1)
            return yes;
        cur = cur->origin;
    }
    return no;
}

LLVMValueRef op_add(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    return LLVMBuildAdd(mod->builder, v0, v1, "add");
}

LLVMValueRef op_sub(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    return LLVMBuildSub(mod->builder, v0, v1, "add");
}

LLVMValueRef op_mul(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert (false, "op_mul: implement more design here");
    return null;
}

LLVMValueRef op_div(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    assert (false, "op_div: implement more design here");
    return null;
}

LLVMValueRef op_eq(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(mod->builder, LLVMIntEQ,   v0, v1, "eq-i");
    else if (f0) return LLVMBuildFCmp(mod->builder, LLVMRealOEQ, v0, v1, "eq-f");
    else {
        assert (false, "op_eq: implement more design here");
        return null;
    }
}

LLVMValueRef op_not_eq(silver mod, dim member, def t0, def t1, LLVMValueRef v0,  LLVMValueRef v1) {
    bool    i0 = t0->mdl >= model_bool && t0->mdl <= model_i64;
    bool    f0 = t0->mdl >= model_f32  && t0->mdl <= model_f64;
    if      (i0) return LLVMBuildICmp(mod->builder, LLVMIntNE,   v0, v1, "not-eq-i");
    else if (f0) return LLVMBuildFCmp(mod->builder, LLVMRealONE, v0, v1, "not-eq-f");
    else {
        assert (false, "op_not_eq: implement more design here");
        return null;
    }
}

LLVMValueRef silver_parse_primary(silver mod);

LLVMValueRef silver_parse_eq  (silver mod) { return parse_ops(mod, silver_parse_primary, "==", "!=",         op_eq,  op_not_eq); }
LLVMValueRef silver_parse_is  (silver mod) { return parse_ops(mod, silver_parse_eq,      "is", "inherits",   op_is,  op_is);     }
LLVMValueRef silver_parse_mult(silver mod) { return parse_ops(mod, silver_parse_is,      "*",  "/",          op_is,  op_is);     }
LLVMValueRef silver_parse_add (silver mod) { return parse_ops(mod, silver_parse_mult,    "+",  "-",          op_add, op_sub);    }

LLVMValueRef silver_parse_expression(silver mod) {
    mod->expr_level++;
    LLVMValueRef vr = isilver(parse_add);
    mod->expr_level--;
    return vr;
}

LLVMValueRef silver_parse_primary(silver mod) {
    Token t = tokens(peek);

    // handle the logical NOT operator (e.g., '!')
    if (tokens(next_is, "!") || tokens(next_is, "not")) {
        tokens(consume); // Consume '!' or 'not'
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the following expression
        return LLVMBuildNot(mod->builder, expr, "logical_not");
    }

    // bitwise NOT operator
    if (tokens(next_is, "~")) {
        tokens(consume); // Consume '~'
        LLVMValueRef expr = silver_parse_expression(mod);
        return LLVMBuildNot(mod->builder, expr, "bitwise_not");
    }

    // 'typeof' operator
    if (tokens(next_is, "typeof")) {
        tokens(consume); // Consume 'typeof'
        assert(tokens(next_is, "["), "Expected '[' after 'typeof'");
        tokens(consume); // Consume '['
        LLVMValueRef type_ref = silver_parse_expression(mod); // Parse the type expression
        assert(tokens(next_is, "]"), "Expected ']' after type expression");
        tokens(consume); // Consume ']'
        return type_ref; // Return the type reference
    }

    // 'cast' operator
    if (tokens(next_is, "cast")) {
        tokens(consume); // Consume 'cast'
        def cast_ident = tokens(read_def, mod); // Read the type to cast to
        assert(tokens(next_is, "["), "Expected '[' for cast");
        tokens(consume); // Consume '['
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the expression to cast
        tokens(consume); // Consume ']'
        if (cast_ident) {
            return LLVMBuildCast(mod->builder, LLVMBitCast, cast_ident->type, expr, "explicit_cast");
        }
    }

    // 'ref' operator (reference)
    if (tokens(next_is, "ref")) {
        tokens(consume); // Consume 'ref'
        LLVMValueRef expr = silver_parse_expression(mod);
        return LLVMBuildLoad2(mod->builder, LLVMTypeOf(expr), expr, "ref_expr"); // Build the reference
    }

    // numeric constants
    object v_num = tokens(next_numeric);
    if (v_num) {
        AType num_type = isa(v_num);
        if (num_type == typeid(i8))  return LLVMConstInt( LLVMInt8Type(),  *( i8*)v_num, 0);
        if (num_type == typeid(i16)) return LLVMConstInt(LLVMInt16Type(),  *(i16*)v_num, 0);
        if (num_type == typeid(i32)) return LLVMConstInt(LLVMInt32Type(),  *(i32*)v_num, 0);
        if (num_type == typeid(i64)) return LLVMConstInt(LLVMInt64Type(),  *(i64*)v_num, 0);
        if (num_type == typeid(u8))  return LLVMConstInt( LLVMInt8Type(),  *( u8*)v_num, 0);
        if (num_type == typeid(u16)) return LLVMConstInt(LLVMInt16Type(),  *(u16*)v_num, 0);
        if (num_type == typeid(u32)) return LLVMConstInt(LLVMInt32Type(),  *(u32*)v_num, 0);
        if (num_type == typeid(u64)) return LLVMConstInt(LLVMInt64Type(),  *(u64*)v_num, 0);
        if (num_type == typeid(f32)) return LLVMConstInt(LLVMFloatType(),  *(f32*)v_num, 0);
        if (num_type == typeid(f64)) return LLVMConstInt(LLVMDoubleType(), *(f64*)v_num, 0);
        assert (false, "numeric literal not handling primitive: %s", num_type->name);
    }

    // strings
    string str_token = tokens(next_string);
    if (str_token)
        return LLVMBuildGlobalStringPtr(mod->builder, str_token->chars, "str");

    // boolean values
    object v_bool = tokens(next_bool);
    if (v_bool)
        return LLVMConstInt(LLVMInt1Type(), *(bool*)v_bool, 0);

    // parenthesized expressions
    if (tokens(next_is, "[")) {
        tokens(consume);
        LLVMValueRef expr = silver_parse_expression(mod); // Parse the expression
        assert(tokens(next_is, "]"), "Expected closing parenthesis");
        tokens(consume);
        return expr;
    }

    // handle identifiers (variables or function calls)
    string ident = tokens(next_alpha);
    if (ident) {
        dim member = silver_member_stack_lookup(mod, ident); // Look up variable
        // if its a primitive, we will want to get its value unless we are referencing (which we handle differently above!)
        LLVMValueRef vr = member->value;
        if (member->def->mdl >= model_bool && member->def->mdl <= model_f64) {
            LLVMTypeRef type = LLVMTypeOf(vr);
            print ("vr type = %s", LLVMPrintTypeToString(LLVMTypeOf(vr)));
            verify (LLVMGetTypeKind(type) == LLVMPointerTypeKind, "expected member address");
            vr = LLVMBuildLoad2(mod->builder, member->def->type, vr, "load-member");
        }
        return vr;
    }

    fault("unexpected token %o in primary expression", tokens(peek));
    return null;
}

LLVMValueRef silver_parse_assignment(silver mod, dim member, string op) {
    verify(!member->cached || !member->is_const, "member %o is a constant", member->name);
    string         op_name = Token_op_name(op);
    dim            method  = get(member->def->members, op_name);
    LLVMValueRef   res     = null;
    LLVMBuilderRef B       = mod->builder;
    LLVMValueRef   L       = member->value;
    LLVMValueRef   R       = isilver(parse_expression);

    if (method) {
        LLVMValueRef args[2] = { L, R };
        res = LLVMBuildCall2(B, method->type, method->value, args, 2, "assign");
    } else {
        member->is_const = eq(op, "=");
        bool e = member->is_const;
        if (e || eq(op, ":"))  res = LLVMBuildStore(B, R, L);
        else if (eq(op, "+=")) res = LLVMBuildAdd  (B, R, L, "assign-add");
        else if (eq(op, "-=")) res = LLVMBuildSub  (B, R, L, "assign-sub");
        else if (eq(op, "*=")) res = LLVMBuildMul  (B, R, L, "assign-mul");
        else if (eq(op, "/=")) res = LLVMBuildSDiv (B, R, L, "assign-div");
        else if (eq(op, "%=")) res = LLVMBuildSRem (B, R, L, "assign-mod");
        else if (eq(op, "|=")) res = LLVMBuildOr   (B, R, L, "assign-or"); 
        else if (eq(op, "&=")) res = LLVMBuildAnd  (B, R, L, "assign-and");
        else if (eq(op, "^=")) res = LLVMBuildXor  (B, R, L, "assign-xor");
        else fault("unsupported operator: %o", op);
    }
    /// update member's value_ref
    //member->value_ref = res;
    if (eq(op, "=")) member->is_const = true;
    return member->value;
}

LLVMValueRef silver_parse_function_call(silver mod, dim fn) {
    bool allow_no_paren = mod->expr_level == 1; /// remember this decision? ... var args
    bool expect_end_br  = false;
    function def        = fn->def;
    assert (def, "no definition found for function member %o", fn->name);
    int  arg_count      = count(def->args);
    
    if (tokens(next_is, "[")) {
        tokens(consume);
        expect_end_br = true;
    } else if (!allow_no_paren)
        fault("expected [ for nested call");

    vector v_args = new(vector, alloc, 32, type, typeid(LLVMValueRef));
    int arg_index = 0;
    LLVMValueRef values[32];

    dim last_arg = null;
    while(arg_index < arg_count || def->va_args) {
        dim          arg      = arg_index < def->args->count ? idx_1(def->args, sz, arg_index) : null;
        LLVMValueRef expr     = isilver(parse_expression);
        LLVMTypeRef  arg_type = arg ? arg->type : null;
        LLVMTypeRef  e_type   = LLVMTypeOf(expr); /// this should be 'someting' member

        if (LLVMGetTypeKind(e_type) == LLVMPointerTypeKind) {
            expr = LLVMBuildLoad2(mod->builder,
                arg ? arg->type : LLVMPointerType(LLVMInt8Type(), 0), expr, "load-arg");
        }

        if (arg_type && e_type != arg_type)
            expr = LLVMBuildBitCast(mod->builder, expr, arg_type, "bitcast");
        
        print("argument %i: %s", arg_index, LLVMPrintValueToString(expr));

        //push(v_args, &expr);
        values[arg_index] = expr;
        arg_index++;
        if (tokens(next_is, ",")) {
            tokens(consume);
            continue;
        } else if (tokens(next_is, "]")) {
            verify (arg_index >= arg_count, "expected %i args", arg_count);
            break;
        } else {
            if (arg_index >= arg_count)
                break;
        }
    }
    if (expect_end_br) {
        verify(tokens(next_is, "]"), "expected ] end of function call");
        tokens(consume);
    }
    return LLVMBuildCall2(
        mod->builder, fn->type, fn->value, values, arg_index, "fn-call");
}

LLVMValueRef silver_parse_statement(silver mod) {
    Token t = tokens(peek);
    if (tokens(next_is, "return")) return isilver(parse_return);
    if (tokens(next_is, "break"))  return isilver(parse_break);
    if (tokens(next_is, "for"))    return isilver(parse_for);
    if (tokens(next_is, "while"))  return isilver(parse_while);
    if (tokens(next_is, "if"))     return isilver(parse_if_else);
    if (tokens(next_is, "do"))     return isilver(parse_do_while);

    map members = isilver(top_members);
    dim member  = dim_read(mod, members, true);
    if (member) {
        print("%s member: %o %o", member->cached ? "existing" : "new", member->def->name, member->name);
        if (member->def->mdl == model_function) {
            if      (member->depth == 1) return member->value;
            else if (member->depth == 0) return isilver(parse_function_call, member);
            fault ("invalid operation");
        } else {
            string assign = tokens(next_assign);
            if    (assign) return isilver(parse_assignment, member, assign);
        }
    }
    fault ("implement"); /// implement as we need them
    return null;
}

LLVMValueRef silver_parse_statements(silver mod) {
    map  members    = isilver(push_member_stack);
    bool multiple   = tokens(next_is, "[");
    if  (multiple)    tokens(consume);
    int  depth      = 1;
    LLVMValueRef vr = null;
    ///
    while(tokens(cast_bool)) {
        if(multiple && tokens(next_is, "[")) {
            depth += 1;
            isilver(push_member_stack);
            tokens(consume);
        }
        print("next statement origin: %o", tokens(peek));
        vr = isilver(parse_statement);
        if(!multiple) break;
        if(tokens(next_is, "]")) {
            if (depth > 1)
                isilver(pop_member_stack);
            tokens(next);
            if ((depth -= 1) == 0) break;
        }
    }
    isilver(pop_member_stack);
    return vr;
}

static void silver_scope_push(silver mod, LLVMMetadataRef meta_ref) {
    push    (mod->scope_stack, meta_ref);
    mod->scope = meta_ref;
}

static void silver_scope_pop(silver mod) {
    pop     (mod->scope_stack);
    mod->scope = len(mod->scope_stack) ? last(mod->scope_stack) : null;
}

void silver_parse_top(silver mod) {
    while (tokens(cast_bool)) {
        if (tokens(next_is, "import")) {
            Import import  = new(Import, mod, mod, tokens, mod->tokens);
            push(mod->imports, import);
            continue;
        } else if (tokens(next_is, "class")) {
            verify (false, "not implemented");
            //EClass def = new(EClass, tokens, tokens);
            //call(mod->defs, set, def->name, def);
            continue;
        } else {
            silver_tokens(mod);
            dim member = dim_read(mod, mod->defs, false);
            string key = member->name ? member->name : (string)format("$m%i", count(mod->defs));
            set(mod->members, key, member);
        }
    }
}

void function_build(function fn);

void silver_init(silver mod) {
    verify(mod->source, "mod name not set");

    mod->scope_stack  = new(array, alloc, 32, unmanaged, true);
    mod->member_stack = new(array, alloc, 32);
    mod->members      = isilver(push_member_stack); /// our base members is the first stack item that we keep after building
    mod->imports      = new(array, alloc, 32);
    mod->source_path  = call(mod->source, directory);
    mod->source_file  = call(mod->source, filename);
    mod->libraries_used = new(array);
    mod->type_refs    = new(map, hsize, 64);
    mod->operators    = operators;

    path  full_path = form(path, "%o/%o", mod->source_path, mod->source_file);
    verify(call(full_path, exists), "source (%o) does not exist", full_path);

    mod->module        = LLVMModuleCreateWithName(mod->source_file->chars);
    mod->context       = LLVMGetModuleContext(mod->module);
    mod->dbg           = LLVMCreateDIBuilder(mod->module);
    mod->builder       = LLVMCreateBuilderInContext(mod->context);
    mod->target_triple = LLVMGetDefaultTargetTriple();
    cstr err = NULL;
    if (LLVMGetTargetFromTriple(mod->target_triple, &mod->target, &err))
        fault("error: %s", err);
    mod->target_machine = LLVMCreateTargetMachine(
        mod->target, mod->target_triple, "generic", "",
        LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    mod->target_data = LLVMCreateTargetDataLayout(mod->target_machine);
    //isilver(llflag, "Dwarf Version",      5);
    //isilver(llflag, "Debug Info Version", 3);

    mod->file = LLVMDIBuilderCreateFile( // create mod file reference (the source file for debugging)
        mod->dbg,
        cast(mod->source_file, cstr), cast(mod->source_file, sz),
        cast(mod->source_path, cstr), cast(mod->source_path, sz));
    
    mod->compile_unit = LLVMDIBuilderCreateCompileUnit(
        mod->dbg, LLVMDWARFSourceLanguageC, mod->file,
        "silver", 6, 0, "", 0,
        0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);

    mod->builder = LLVMCreateBuilderInContext(mod->context);

    isilver(define_C99);

    def      i32   = get(mod->defs, str("i32"));
    dim      rtype = new(dim, def, i32);
    map      args  = new(map);
    function fn    = new(function, // should add from top level parser
        mod,        mod,
        rtype,      rtype,
        name,       str("main"),
        args,       args,
        visibility, Visibility_public,
        body,       new(Tokens));

    function_build(fn);
}

void silver_destructor(silver mod) {
    LLVMDisposeBuilder(mod->builder);
    LLVMDisposeDIBuilder(mod->dbg);
    LLVMDisposeModule(mod->module);
    LLVMContextDispose(mod->context);
    LLVMDisposeMessage(mod->target_triple);
}

void silver_write(silver mod) {
    // Finalize the debug builder (leave room for user to write additional functions after init)
    LLVMDIBuilderFinalize(mod->dbg);

    // Verify the module
    char *error = NULL;
    if (LLVMVerifyModule(mod->module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "Error verifying mod->module: %s\n", error);
        LLVMDisposeMessage(error);
        exit(1);
    }

    // Optionally, write the module to a file
    if (LLVMPrintModuleToFile(mod->module, "output.ll", &error) != 0) {
        fprintf(stderr, "Error writing mod->module to file: %s\n", error);
        LLVMDisposeMessage(error);
        exit(1);
    }
}

void def_associate(def ident, LLVMTypeRef type) {
    silver mod = ident->mod;
    string key = type_key(type);
    assert (!contains(mod->type_refs, key), "already associated");
    set(mod->type_refs, key, ident);
}

sz def_size(def ident) {
    return LLVMSizeOfTypeInBits(ident->mod->target_data, ident->type);
}

LLVMMetadataRef primitive_dbg(def ident) {
    return LLVMDIBuilderCreateBasicType(
        ident->mod->dbg, ident->name->chars, ident->name->len, def_size(ident),
        ident->name->chars[0] == 'f' ? 0x04 : 0x05, 0);
}

LLVMMetadataRef cstr_dbg(def ident, bool isConst) {
    LLVMMetadataRef charTypeMeta = LLVMDIBuilderCreateBasicType(
        ident->mod->dbg, "char", 4, 8, 0x01, 0); // 0x01 = DW_ATE_unsigned_char
    symbol name = isConst ? "const char" : "char";
    u64 ptr_sz = LLVMPointerSize(ident->mod->target_data);
    return LLVMDIBuilderCreatePointerType(ident->mod->dbg, charTypeMeta,
        ptr_sz * 8, 0, 0, name, strlen(name));
}

void function_init(function fn) {
    silver mod = fn->mod;
    verify (fn->mdl == model_function, "unexpected model %o", estr(model, fn->mdl));
}

void function_build(function fn) {
    dim  rtype = fn->rtype;
    silver mod = fn->mod;

    isilver (scope_push, fn->function);
    tokens  (push_state, fn->body->tokens, fn->body->cursor);
    LLVMPositionBuilderAtEnd(mod->builder, fn->entry);
    map      members = isilver(push_member_stack);
    concat  (members, fn->args);
    set     (members, str("#rtype"), (dim)fn->rtype);
    //LLVMBuildRetVoid(mod->builder);
    isilver (parse_statements);
    isilver (pop_member_stack);
    tokens  (pop_state, false);
    isilver (scope_pop);


    // Create a function type: int main()
    LLVMTypeRef returnType = rtype->def->type;
    LLVMTypeRef paramTypes[] = {}; // No parameters
    LLVMTypeRef funcType = LLVMFunctionType(returnType, paramTypes, 0, 0);

    // Create the function and add it to the module
    LLVMValueRef function = LLVMAddFunction(mod->module, "main", funcType);

    // Set function linkage
    LLVMSetLinkage(function, LLVMExternalLinkage);

    // Create function debug info
    LLVMMetadataRef funcTypeMeta = LLVMDIBuilderCreateSubroutineType(
        mod->dbg,
        mod->file,              // Scope (file)
        NULL,              // Parameter types (None for simplicity)
        0,                 // Number of parameters
        LLVMDIFlagZero     // Flags
    );

    LLVMMetadataRef functionMeta = LLVMDIBuilderCreateFunction(
        mod->dbg,
        mod->file,                   // Scope (file)
        "main",                 // Name
        strlen("main"),
        "main",                 // Linkage name (same as name)
        strlen("main"),
        mod->file,                   // File
        1,                      // Line number
        funcTypeMeta,           // Function type
        1,                      // Is local to unit
        1,                      // Is definition
        1,                      // Scope line
        LLVMDIFlagZero,         // Flags
        0                       // Is optimized
    );

    // Attach debug info to function
    LLVMSetSubprogram(function, functionMeta);

    // Create a basic block in the function
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(mod->context, function, "entry");
    LLVMPositionBuilderAtEnd(mod->builder, entry);

    // Set current debug location
    LLVMMetadataRef debugLoc = LLVMDIBuilderCreateDebugLocation(
        mod->context,
        2,           // Line number
        1,           // Column number
        functionMeta,// Scope
        NULL         // InlinedAt
    );
    LLVMSetCurrentDebugLocation2(mod->builder, debugLoc);

    // Declare the local variable 'something' of type i64 and assign it the value 1
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(mod->context);
    LLVMValueRef somethingAlloc = LLVMBuildAlloca(mod->builder, i64Type, "something");

    // Create debug info for the variable
    LLVMMetadataRef intTypeMeta = LLVMDIBuilderCreateBasicType(
        mod->dbg,
        "int64",            // Type name
        strlen("int64"),
        64,                 // Size in bits
        0x05, // Encoding
        LLVMDIFlagZero
    );

    LLVMMetadataRef variableMeta = LLVMDIBuilderCreateAutoVariable(
        mod->dbg,
        functionMeta,       // Scope
        "something",        // Variable name
        strlen("something"),
        mod->file,               // File
        2,                  // Line number
        intTypeMeta,        // Type metadata
        0,                  // Always preserved
        LLVMDIFlagZero,     // Flags
        0                   // Alignment
    );

    // Insert a debug declaration for the variable
    LLVMDIBuilderInsertDeclareAtEnd(
        mod->dbg,
        somethingAlloc,
        variableMeta,
        LLVMDIBuilderCreateExpression(mod->dbg, NULL, 0),
        debugLoc,
        entry // Insert at the end of the entry block
    );

    // Assign the value 1 to 'something'
    LLVMValueRef constOne = LLVMConstInt(i64Type, 1, 0);
    LLVMBuildStore(mod->builder, constOne, somethingAlloc);

    // Set current debug location for store instruction
    LLVMSetCurrentDebugLocation2(mod->builder, debugLoc);

    // Declare the printf function (variadic function)
    LLVMTypeRef printfParamTypes[] = { LLVMPointerType(LLVMInt8TypeInContext(mod->context), 0) }; // const char*
    LLVMTypeRef printfFuncType = LLVMFunctionType(
        LLVMInt32TypeInContext(mod->context),
        printfParamTypes,
        1,
        1 // Is variadic
    );
    LLVMValueRef printfFunc = LLVMAddFunction(mod->module, "printf", printfFuncType);
    LLVMSetLinkage(printfFunc, LLVMExternalLinkage);

    // Create format string for printf
    LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(mod->builder, "Value: %ld\n", "formatStr");

    // Load the value of 'something'
    LLVMValueRef somethingValue = LLVMBuildLoad2(mod->builder, i64Type, somethingAlloc, "load_something");

    // Set current debug location for load instruction
    LLVMSetCurrentDebugLocation2(mod->builder, debugLoc);

    // Set up arguments for printf
    LLVMValueRef printfArgs[] = { formatStr, somethingValue };

    // Build call to printf
    LLVMValueRef call = LLVMBuildCall2(
        mod->builder,
        printfFuncType,  // Function type
        printfFunc,      // Function to call
        printfArgs,      // Arguments
        2,               // Number of arguments
        "call_printf"    // Name of the call
    );

    // Set call site debug location
    LLVMSetCurrentDebugLocation2(mod->builder, debugLoc);

    // Build return instruction
    LLVMBuildRet(mod->builder, LLVMConstInt(LLVMInt32TypeInContext(mod->context), 0, 0));
}

void def_init(def ident) {
    silver mod = ident->mod;

    verify(ident->mod, "mod not set");
    verify(ident->name,   "name not set");

    ident->members = new(map, hsize, 8);
    bool handled_members = false;

    switch (ident->mdl) {
        case model_class:
            fault ("not implemented");
            break;
        
        case model_function:
            fault ("wrong path for model-function; use new(function, ...");
            break;
        
        case model_bool:   ident->type = LLVMInt1Type  (); break;
        case model_i8:     ident->type = LLVMInt8Type  (); break;
        case model_i16:    ident->type = LLVMInt16Type (); break;
        case model_i32:    ident->type = LLVMInt32Type (); break;
        case model_i64: {
            ident->type = LLVMInt64Type ();
            break;
        }
        case model_u8:     ident->type = LLVMInt8Type  (); break;
        case model_u16:    ident->type = LLVMInt16Type (); break;
        case model_u32:    ident->type = LLVMInt32Type (); break;
        case model_u64:    ident->type = LLVMInt64Type (); break;
        case model_f32:    ident->type = LLVMFloatType (); break;
        case model_f64:    ident->type = LLVMDoubleType(); break;
        case model_void:   ident->type = LLVMVoidType  (); break;
        case model_cstr:   ident->type = LLVMPointerType(LLVMInt8Type(), 0); break;
        case model_typedef: {
            verify (ident->origin && isa(ident->origin) == typeid(dim), "origin must be ident reference");
            ident->type = ident->origin->type;
            if (mod->dbg) {
                dim origin = ident->origin;
                verify(origin, "origin not resolved");
                ident->dbg = LLVMDIBuilderCreateTypedef(
                    mod->dbg, ident->origin->def->dbg, ident->name->chars, len(ident->name),
                    mod->file, ident->token ? ident->token->line : 0, mod->scope, LLVMDIFlagZero);
            }
            break;
        }
        case model_struct: {
            LLVMTypeRef* member_types = calloc(len(ident->members), sizeof(LLVMTypeRef));
            int index = 0;
            pairs(ident->members, member_pair) {
                dim member = member_pair->value;
                verify(isa(member) == typeid(dim), "mismatch");
                member_types[index] = member->type;
                index++;
            }
            ident->type = LLVMStructCreateNamed(LLVMGetGlobalContext(), ident->name);
            LLVMStructSetBody(ident->type, member_types, index, 0);
            handled_members = true;
            break;
        }
        case model_union: {
            verify (false, "not implemented");
            break;
        }
        def_associate(ident, ident->type);
    }

    /// create debug info for primitives
    if (ident->mdl >= model_bool && ident->mdl <= model_f64)
        ident->dbg = primitive_dbg(ident);
    else if (eq(ident->name, "symbol") || eq(ident->name, "cstr"))
        ident->dbg = cstr_dbg(ident, eq(ident->name, "symbol"));
    else {
        verify (ident->dbg || eq(ident->name, "void"), "debug info not set for type");
    }
    verify (!count(ident->members) || handled_members, "members given and not processed");
}

/// this reads a num var
/// --------------------
/// num[2] name
/// num[2] name [ num arg1, ... num[] extra ] expr | [ statements ]
/// map[string, object] name   # good idea to always define that the value is object, and no default 'map' usage?
/// array[meta][string, object] name   #
/// --------------------

void dim_bind(dim member) {
    silver mod = member->mod;
    if (!tokens(next_is, "["))
        return;
    tokens(consume);
    member->wrap  = get(mod->defs, str("array"));
    member->shape = new(array); /// shape is there but not given data 
    if (!tokens(next_is, "]")) {
        def wdef = tokens(read_def, mod); // we are treating anything as a member
        if (wdef) {
            /// must be map
            for (;;) {
                push(member->shape, wdef);
                Token n = tokens(peek);
                if (eq(n, ",")) {
                    wdef = tokens(read_def, mod);
                    continue;
                }
                break;
            }
        } else {
            /// must be array
            for (;;) {
                i64 dim_size = 0;
                object n = tokens(read_numeric);
                verify(n && isa(n) == typeid(i64), "expected integer");
                tokens(consume);
                push(member->shape, A_i64(dim_size));
                Token next = tokens(peek);
                if (eq(next, ",")) {
                    tokens(consume);
                    continue;
                }
                break;
            }
        }
        verify (tokens(next_is, "]"), "expected ] in type usage expression");
    }
    tokens(consume);
}

void dim_allocate(dim member, silver mod, bool create_debug) {
    LLVMTypeRef tr = member->type;
    member->value   = LLVMBuildAlloca(mod->builder, tr, "alloc-dim");
    member->dbg     = LLVMDIBuilderCreateAutoVariable(
        mod->dbg,
        mod->scope,
        member->name->chars,
        member->name->len,
        mod->file,
        2,
        member->def->dbg,
        true, 0, 0);


    LLVMBasicBlockRef block = LLVMGetInsertBlock(mod->builder);
    LLVMValueRef      head  = LLVMGetFirstInstruction(block);

    verify (block && head, "LLVM is a block head");
    verify (head == member->value, "LLVM is a block head");
    // Now insert this variable into the IR, associating it with the debug metadata
    LLVMDIBuilderInsertDeclareBefore(
        mod->dbg,
        member->value,
        member->dbg,
        LLVMDIBuilderCreateExpression(mod->dbg, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(mod->context, member->line, 0, mod->scope, NULL),  // Debug location
        head
    );
}

void dim_create_fn(dim member) {
    silver mod = member->mod;
    map    members = member->context;
    verify (tokens(next_is, "["), "expected function args");
    tokens (consume);
    map args = new(map, hsize, 8);
    int arg_index = 0;
    
    silver_tokens(mod);

    if (!tokens(next_is, "]"))
        while (true) {
            dim arg = dim_read(member->mod, member->def->members, false);
            verify (arg,       "member failed to read");
            verify (arg->name, "member name not set");
            if     (tokens(next_is, "]")) break;
            verify (tokens(next_is, ","), "expected separator");
            tokens (consume);
            set    (args, arg->name, arg);
            arg_index++;
        }
    num line = tokens(line);
    tokens(consume);
    dim rtype_dim = new(dim,
        mod,        member->mod,    def,        member->def,    line, line,
        depth,      member->depth,  shape,      member->shape,
        wrap,       member->wrap,   context,    members);
    function f_def = new(function,
        name,     str(member->name->chars),  mod,   member->mod,
        mdl,      model_function,       rtype,    rtype_dim,
        args,     args,                 info,     member);
    //assert(count(members, f_def->name) == 0, "duplicate member: %o", f_def->name);
    set (members, f_def->name, f_def);
    drop(member->type);
    member->type    = hold(f_def);
    array body = new(array, alloc, 32);
    verify (tokens(next_is, "["), "expected function body");
    int depth = 0;
    do {
        Token   token = tokens(next);
        verify (token, "expected end of function body ( too many ['s )");
        push   (body, token);
        if      (eq(token, "[")) depth++;
        else if (eq(token, "]")) depth--;
    } while (depth > 0);
    f_def->body = new(Tokens, cursor, 0, tokens, body);
}

dim dim_read(silver mod, map context, bool alloc) {
    /// we use alloc to obtain a value-ref by means of allocation inside a function body.
    /// if we are not there, then set false in that case.
    /// in that case, the value_ref should be set by the user to R-type or ARG-type
    tokens(push_current);
    dim member = null;

    print("dim_read:");
    silver_tokens(mod);

    string alpha = tokens(next_alpha);
    if(alpha) {
        member = isilver(member_stack_lookup, alpha);
        if (member) {
            member->cached = true;
            return member;
        } else {
            tokens(prev);
        }
    }
    member = new(dim, mod, mod, context, context);
    
    if (tokens(next_is, "static")) {
        tokens(consume);
        member->is_static = true;
    }
    if (tokens(next_is, "ref")) {
        tokens(consume);
        member->depth = 1;
    }
    /// look for visibility (default is possibly provided)
    for (int m = 1; m < Visibility_type.member_count; m++) {
        type_member_t* enum_v = &Visibility_type.members[m];
        if (tokens(next_is, enum_v->name)) {
            tokens(consume);
            member->visibility = m;
            break;
        }
    }
    if (!member->is_static) {
        if (tokens(next_is, "static")) {
            tokens(consume);
            member->is_static = true;
        }
    }
    Token   n = tokens(peek);
    def ident = tokens(read_def, mod);
    if (!ident) {
        print("info: could not read type at position %o", tokens(location));
        tokens(pop_state, false); // we may 'info' here
        return null;
    }
    member->def = hold(ident);

    silver_tokens(mod);
    
    // may be [, or alpha-id  (its an error if its neither)
    if (tokens(next_is, "["))
        idim(bind);

    /// members must be named
    silver_tokens(mod);
    string name = tokens(next_alpha);
    verify(name, "expected identifier for member");
    member->name     = hold(name);

    if (tokens(next_is, "["))
        idim(create_fn);
    else if (alloc)
        idim(allocate, mod, true); /// would be nice to get member context from state.. or hey, just set 1 variable and retrieve one variable.  put it in silver intern.

    tokens(pop_state, true);
    map top = isilver(top_members);
    set(top, name, member);
    return member;
}


void dim_init(dim member) {
    if (member->def && !member->mod)
        member->mod = member->def->mod;
    if (!member->context)
        member->context = member->mod->defs;
    if (!member->visibility) member->visibility = Visibility_public;

    if (!member->type) {
        LLVMTypeRef t = member->def->type;
        if (member->depth > 0 && t == LLVMVoidType())
            t = LLVMInt8Type();
        for (int i = 0; i < member->depth; i++)
            t = LLVMPointerType(t, 0);
        member->type = t;
    }
}

define_enum(Visibility)
define_enum(model)
define_class(def)
define_mod(function, def)
define_class(dim)
define_class(silver)

define_primitive(LLVMValueRef, raw, 0)


int main(int argc, char **argv) {
    A_start();
    AF         pool = allocate(AF);
    cstr        src = getenv("SRC");
    cstr     import = getenv("SILVER_IMPORT");
    map    defaults = map_of(
        "module",  str(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."),
        null);
    print("defaults = %o", defaults);

    string ikey     = str("install");
    map    args     = A_args(argc, argv, defaults, ikey);
    print("args = %o", args);

    path   install  = get(args, ikey);
    string mkey     = str("module");
    string name     = get(args, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);

    assert (call(source, exists), "source %o does not exist", n);
    silver mod = new(silver, source, source, install, install);
    write(mod);

    drop(pool);
}