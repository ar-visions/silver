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


LLVMMetadataRef debug_scope(aether a);
#define B a->builder

// map an Au_t primitive to a DWARF basic type encoding
LLVMMetadataRef debug_type_for(aether a, Au_t src) {
    if (!src) return LLVMDIBuilderCreateBasicType(
        a->dbg_builder, "ptr", 3, 64, 0, LLVMDIFlagZero);

    // pointer / class types → pointer
    if (src->is_pointer || src->is_class || src->is_funcptr)
        return LLVMDIBuilderCreatePointerType(
            a->dbg_builder,
            LLVMDIBuilderCreateBasicType(a->dbg_builder, "u8", 2, 8, 7, LLVMDIFlagZero),
            64, 0, 0, src->ident ? src->ident : "ptr",
            src->ident ? strlen(src->ident) : 3);

    etype et = u(etype, src);
    u32 bits = et && et->lltype ? LLVMABISizeOfType(a->target_data, et->lltype) * 8 : 64;
    if (!bits) bits = 64;

    // float/double
    if (src->is_realistic)
        return LLVMDIBuilderCreateBasicType(
            a->dbg_builder, src->ident, strlen(src->ident), bits, 4, LLVMDIFlagZero);

    // signed int
    if (src->is_signed || (src->is_integral && !src->is_unsigned))
        return LLVMDIBuilderCreateBasicType(
            a->dbg_builder, src->ident, strlen(src->ident), bits, 5, LLVMDIFlagZero);

    // unsigned int / bool / enum
    if (src->is_unsigned || src->is_enum)
        return LLVMDIBuilderCreateBasicType(
            a->dbg_builder, src->ident, strlen(src->ident), bits, 7, LLVMDIFlagZero);

    // bool
    if (src == typeid(bool))
        return LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "bool", 4, 8, 2, LLVMDIFlagZero);

    // fallback: pointer-sized opaque
    return LLVMDIBuilderCreateBasicType(
        a->dbg_builder, src->ident ? src->ident : "ptr",
        src->ident ? strlen(src->ident) : 3, bits, 0, LLVMDIFlagZero);
}

// build or return cached DWARF composite type for a struct/class
LLVMMetadataRef debug_struct_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au || !type_au->ident) return null;

    // use lldebug as cache
    etype et = u(etype, type_au);
    if (et && et->lldebug) return et->lldebug;

    // count instance members
    int count = 0;
    Au_t b4 = type_au;
    while (type_au) {
        for (int i = 0; i < type_au->members.count; i++) {
            Au_t m = (Au_t)type_au->members.origin[i];
            if (m->member_type == AU_MEMBER_VAR && !m->is_static)
                count++;
        }
        type_au = type_au->context;
        if (type_au == typeid(Au))
            break;
    }
    if (count == 0) return null;

    type_au = b4;

    u32 total_bits = type_au->abi_size ? type_au->abi_size : 0;
    u32 align      = type_au->align_bits ? type_au->align_bits : 64;

    // build member metadata array
    LLVMMetadataRef* members = calloc(count, sizeof(LLVMMetadataRef));
    int idx = 0;
    while (type_au) {
        for (int i = 0; i < type_au->members.count; i++) {
            Au_t m = (Au_t)type_au->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR || m->is_static) continue;

            LLVMMetadataRef member_type = debug_type_for(a, m->src);
            etype m_et = u(etype, m->src);
            u32 m_bits = m_et && m_et->lltype
                ? LLVMABISizeOfType(a->target_data, m_et->lltype) * 8 : 64;
            u32 m_align = m_et && m_et->lltype
                ? LLVMABIAlignmentOfType(a->target_data, m_et->lltype) * 8 : 64;
            u64 offset_bits = m->offset * 8;

            members[idx++] = LLVMDIBuilderCreateMemberType(
                a->dbg_builder, a->compile_unit,
                m->ident, strlen(m->ident),
                a->file, 0,
                m_bits, m_align, offset_bits,
                LLVMDIFlagZero, member_type);
        }
        type_au = type_au->context;
        if (type_au == typeid(Au))
            break;
    }
    type_au = b4;

    LLVMMetadataRef di_struct = LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        type_au->ident, strlen(type_au->ident),
        a->file, 0,
        total_bits, align,
        LLVMDIFlagZero, null,
        members, idx, 0, null,
        type_au->ident, strlen(type_au->ident));

    free(members);

    // cache on etype
    if (et) et->lldebug = di_struct;
    return di_struct;
}

// create a DISubprogram for a function and attach it
void emit_debug_function(aether a, efunc fn) {
    if (!a->debug || !a->compile_unit) return;
    Au_t au = fn->au;
    if (!au->ident) return;

    LLVMMetadataRef file_ref = a->file;

    // build subroutine type
    LLVMMetadataRef sr_type = LLVMDIBuilderCreateSubroutineType(
        a->dbg_builder, file_ref, null, 0, LLVMDIFlagZero);

    u32 line = 0;
    cstr name = au->alt ? au->alt : au->ident;
    u32 name_len = strlen(name);

    // for instance methods, scope inside the class DI type if available
    LLVMMetadataRef scope = a->compile_unit;
    if (au->is_imethod && au->context) {
        LLVMMetadataRef class_di = debug_struct_type(a, au->context);
        if (class_di) scope = class_di;
    }

    LLVMMetadataRef sp = LLVMDIBuilderCreateFunction(
        a->dbg_builder,
        scope,
        name, name_len,         // name
        name, name_len,         // linkage name
        file_ref, line,         // file, line
        sr_type,                // subroutine type
        false,                  // is local to unit
        true,                   // is definition
        line,                   // scope line
        LLVMDIFlagZero,        // flags
        false);                 // is optimized

    etype et_fn = (etype)fn;
    et_fn->llscope = sp;

    if (fn->value)
        LLVMSetSubprogram(fn->value, sp);
}

// emit a DW_TAG_auto_variable for a local or argument
void emit_debug_variable(aether a, enode var, u32 arg_no, u32 line) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!var->au || !var->au->ident || !var->value) return;
    LLVMMetadataRef scope = debug_scope(a);
    if (!scope) return;

    cstr name     = var->au->ident;
    u32  name_len = strlen(name);

    LLVMMetadataRef di_type = debug_type_for(a, var->au->src);

    LLVMMetadataRef di_var = (arg_no > 0)
        ? LLVMDIBuilderCreateParameterVariable(
              a->dbg_builder, scope, name, name_len,
              arg_no, a->file, line, di_type, false, LLVMDIFlagZero)
        : LLVMDIBuilderCreateAutoVariable(
              a->dbg_builder, scope, name, name_len,
              a->file, line, di_type, false, LLVMDIFlagZero, 0);

    LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);
    LLVMMetadataRef loc  = LLVMDIBuilderCreateDebugLocation(
        a->module_ctx, line, 0, scope, null);
    LLVMDIBuilderInsertDeclareRecordAtEnd(
        a->dbg_builder, var->value, di_var, expr, loc,
        LLVMGetInsertBlock(B));
}

LLVMMetadataRef debug_au_header_type(aether a, Au_t schema) {
    if (schema)
        schema = schema;
    LLVMMetadataRef schema_type = schema ? debug_struct_type(a, schema) : null;
    
    Au_t au = typeid(Au);
    int count = 0;
    for (int i = 0; i < au->members.count; i++) {
        Au_t m = (Au_t)au->members.origin[i];
        if (m->member_type == AU_MEMBER_VAR && !m->is_static)
            count++;
    }
    
    LLVMMetadataRef* members = calloc(count, sizeof(LLVMMetadataRef));
    int idx = 0;
    for (int i = 0; i < au->members.count; i++) {
        Au_t m = (Au_t)au->members.origin[i];
        if (m->member_type != AU_MEMBER_VAR || m->is_static) continue;
        
        LLVMMetadataRef member_type;
        if (schema_type && strcmp(m->ident, "type") == 0)
            member_type = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_type, LLVMPointerSize(a->target_data) * 8, 0, 0,
                schema->ident, strlen(schema->ident));
        else
            member_type = debug_type_for(a, m->src);
        
        etype m_et = u(etype, m->src);
        u32 m_bits = m_et && m_et->lltype
            ? LLVMABISizeOfType(a->target_data, m_et->lltype) * 8 : 64;
        u32 m_align = m_et && m_et->lltype
            ? LLVMABIAlignmentOfType(a->target_data, m_et->lltype) * 8 : 64;
        u64 offset_bits = m->offset * 8;
        
        members[idx++] = LLVMDIBuilderCreateMemberType(
            a->dbg_builder, a->compile_unit,
            m->ident, strlen(m->ident),
            a->file, 0,
            m_bits, m_align, offset_bits,
            LLVMDIFlagZero, member_type);
    }
    
    cstr name = schema ? schema->ident : "_Au";
    LLVMMetadataRef result = LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        name, strlen(name), a->file, 0,
        sizeof(struct _Au) * 8, 64,
        LLVMDIFlagZero, null,
        members, idx, 0, null,
        name, strlen(name));
    
    free(members);
    return result;
}

// emit parameter debug variables for function args (including self for instance methods)
// called when entering a function body in push_scope, after builder is positioned at entry
void emit_debug_params(aether a, efunc fn) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!fn || !fn->value) return;

    if (fn->au->alt && strstr(fn->au->alt, "app")) {
        int test2 = 2;
        test2    += 2;
    }
    Au_t au = fn->au;
    LLVMMetadataRef scope = ((etype)fn)->llscope;
    if (!scope) return;

    int arg_no = 1;
    int llvm_param_idx = 0;

    if (is_lambda(au))
        llvm_param_idx = 1;

    arg_list(au, arg) {
        if (!arg->ident) { arg_no++; llvm_param_idx++; continue; }

        bool is_target = (au->is_imethod && arg_no == 1);
        cstr name      = is_target ? "a" : arg->ident;
        u32  name_len  = strlen(name);

        LLVMMetadataRef struct_au = null;
        LLVMMetadataRef di_type;
        LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);

        if (is_target && au->context) {
            LLVMMetadataRef user_struct = debug_struct_type(a, au->context);
            if (user_struct) {
                di_type = LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, user_struct, 64, 0, 0,
                    au->context->ident, strlen(au->context->ident));

                etype e = u(etype, arg->src);
                if (e)
                    struct_au = debug_au_header_type(a, e->schema->au);
            } else {
                di_type = debug_type_for(a, arg->src);
            }
        } else {
            di_type = debug_type_for(a, arg->src);
        }

        LLVMMetadataRef di_param = LLVMDIBuilderCreateParameterVariable(
            a->dbg_builder, scope, name, name_len,
            arg_no, a->file, 0, di_type,
            false, LLVMDIFlagZero);

        LLVMValueRef param_val = LLVMGetParam(fn->value, llvm_param_idx);
        LLVMTypeRef  param_ty  = LLVMTypeOf(param_val);
        char alloca_name[256];
        snprintf(alloca_name, sizeof(alloca_name), "%s.addr", name);
        LLVMValueRef shadow = LLVMBuildAlloca(B, param_ty, alloca_name);
        LLVMBuildStore(B, param_val, shadow);

        LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
            a->module_ctx, 0, 0, scope, null);

            
        LLVMDIBuilderInsertDeclareRecordAtEnd(
            a->dbg_builder, shadow, di_param, expr, loc,
            LLVMGetInsertBlock(B));

        if (struct_au) {
            LLVMMetadataRef obj_type = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, struct_au, 64, 0, 0,
                au->context->ident, strlen(au->context->ident));
            
            // compute header pointer: a - sizeof(_object)
            LLVMTypeRef ll_i8 = LLVMInt8TypeInContext(a->module_ctx);
            LLVMTypeRef ll_i64 = LLVMInt64TypeInContext(a->module_ctx);
            LLVMValueRef neg_off = LLVMConstInt(ll_i64, (u64)(-(i64)sizeof(struct _Au)), true);
            LLVMValueRef header_ptr = LLVMBuildGEP2(B, ll_i8, param_val, &neg_off, 1, "_au.ptr");
            
            // alloca to hold it
            LLVMValueRef obj_shadow = LLVMBuildAlloca(B, LLVMTypeOf(header_ptr), "_au.addr");
            LLVMBuildStore(B, header_ptr, obj_shadow);
            
            string n = f(string, "%s_au", name);
            LLVMMetadataRef obj_expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);
            LLVMMetadataRef obj_var = LLVMDIBuilderCreateAutoVariable(
                a->dbg_builder, scope, n->chars, len(n),
                a->file, 0, obj_type, false, LLVMDIFlagZero, 0);
            LLVMDIBuilderInsertDeclareRecordAtEnd(
                a->dbg_builder, obj_shadow, obj_var, obj_expr, loc,
                LLVMGetInsertBlock(B));
        }

        arg_no++;
        llvm_param_idx++;
    }
}

// emit __au_header(ptr) -> object*  helper for Au-derived classes
// LLDB users can call: expr __au_header(myobj) to see the _object header
// this offsets ptr by -sizeof(struct _object) to reach the hidden header
void emit_au_header_view(aether a) {
    if (!a->debug || !a->compile_unit) return;

    LLVMContextRef ctx = a->module_ctx;
    LLVMTypeRef    ptr = LLVMPointerTypeInContext(ctx, 0);
    LLVMTypeRef    ll_i64 = LLVMInt64TypeInContext(ctx);
    LLVMTypeRef    ll_i8  = LLVMInt8TypeInContext(ctx);

    // void* __au_header(void* obj)
    LLVMTypeRef fn_type = LLVMFunctionType(ptr, &ptr, 1, false);
    LLVMValueRef fn = LLVMAddFunction(a->module_ref, "__au_header", fn_type);
    LLVMSetLinkage(fn, LLVMExternalLinkage);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "entry");
    LLVMBuilderRef hb = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(hb, entry);

    // get sizeof(struct _object) from the Au type system
    Au_t au_obj = typeid(Au);
    u64 obj_size = sizeof(struct _object); // fallback
    // cast to i8*, subtract sizeof(_object), return
    LLVMValueRef arg     = LLVMGetParam(fn, 0);
    LLVMValueRef neg_off = LLVMConstInt(ll_i64, (u64)(-(i64)obj_size), true);
    LLVMValueRef result  = LLVMBuildGEP2(hb, ll_i8, arg, &neg_off, 1, "header");
    LLVMBuildRet(hb, result);
    LLVMDisposeBuilder(hb);

    // attach debug subprogram so LLDB can call it
    LLVMMetadataRef sr_type = LLVMDIBuilderCreateSubroutineType(
        a->dbg_builder, a->file, null, 0, LLVMDIFlagZero);
    symbol n = "__au_header";
    LLVMMetadataRef sp = LLVMDIBuilderCreateFunction(
        a->dbg_builder, a->compile_unit,
        n, strlen(n), n, strlen(n),
        a->file, 0, sr_type,
        false, true, 0, LLVMDIFlagZero, false);
    LLVMSetSubprogram(fn, sp);
}