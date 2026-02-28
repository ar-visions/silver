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

// ────────────────────────────────────────────────────────────────────────────
// DWARF ATE (attribute type encoding) constants
// ────────────────────────────────────────────────────────────────────────────
#define DW_ATE_address        0x01
#define DW_ATE_boolean        0x02
#define DW_ATE_float          0x04
#define DW_ATE_signed         0x05
#define DW_ATE_signed_char    0x06
#define DW_ATE_unsigned       0x07
#define DW_ATE_unsigned_char  0x08
#define DW_ATE_void           0x00

// ────────────────────────────────────────────────────────────────────────────
// forward declarations
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_type_for        (aether a, Au_t src);
LLVMMetadataRef debug_struct_type     (aether a, Au_t type_au);
LLVMMetadataRef debug_enum_type       (aether a, Au_t type_au);
LLVMMetadataRef debug_pointer_type    (aether a, Au_t type_au);
LLVMMetadataRef debug_funcptr_type    (aether a, Au_t type_au);
LLVMMetadataRef debug_array_type      (aether a, Au_t type_au);
LLVMMetadataRef debug_typedef_type    (aether a, Au_t type_au);
LLVMMetadataRef debug_au_header_type  (aether a, Au_t schema);
LLVMMetadataRef debug_subroutine_type (aether a, Au_t fn_au);
void            emit_debug_function   (aether a, efunc fn);
void            emit_debug_variable   (aether a, enode var, u32 arg_no, u32 line);
void            emit_debug_params     (aether a, efunc fn);

// ────────────────────────────────────────────────────────────────────────────
// helpers
// ────────────────────────────────────────────────────────────────────────────

// compute bit-width from an Au_t, using the etype lltype if available
static u32 bits_for_type(aether a, Au_t src) {
    if (!src) return 64;
    etype et = u(etype, src);
    if (et && et->lltype) {
        u32 bits = LLVMABISizeOfType(a->target_data, et->lltype) * 8;
        if (bits) return bits;
    }
    if (src->abi_size) return src->abi_size;
    if (src->typesize) return src->typesize * 8;
    return 64;
}

// compute alignment in bits from an Au_t
static u32 align_for_type(aether a, Au_t src) {
    if (!src) return 64;
    etype et = u(etype, src);
    if (et && et->lltype) {
        u32 align = LLVMABIAlignmentOfType(a->target_data, et->lltype) * 8;
        if (align) return align;
    }
    if (src->align_bits) return src->align_bits;
    return 64;
}

// get pointer size in bits for target
static u32 pointer_bits(aether a) {
    return LLVMPointerSize(a->target_data) * 8;
}

// count instance (non-static) variable members in a single type level
static int count_instance_vars(Au_t type_au) {
    int count = 0;
    for (int i = 0; i < type_au->members.count; i++) {
        Au_t m = (Au_t)type_au->members.origin[i];
        if (m->member_type == AU_MEMBER_VAR && !m->is_static)
            count++;
    }
    return count;
}

// count enum value members in a type
static int count_enum_values(Au_t type_au) {
    int count = 0;
    for (int i = 0; i < type_au->members.count; i++) {
        Au_t m = (Au_t)type_au->members.origin[i];
        if (m->member_type == AU_MEMBER_ENUMV)
            count++;
    }
    return count;
}

// count all instance vars across the inheritance chain (stopping at Au)
static int count_all_instance_vars(Au_t type_au) {
    int total = 0;
    Au_t cur = type_au;
    while (cur) {
        total += count_instance_vars(cur);
        cur = cur->context;
        if (cur == typeid(Au)) break;
    }
    return total;
}

// count function members in a single type level
static int count_methods(Au_t type_au) {
    int count = 0;
    for (int i = 0; i < type_au->members.count; i++) {
        Au_t m = (Au_t)type_au->members.origin[i];
        if (m->member_type == AU_MEMBER_FUNC ||
            m->member_type == AU_MEMBER_CONSTRUCT)
            count++;
    }
    return count;
}

// count all methods across the inheritance chain (stopping at Au)
static int count_all_methods(Au_t type_au) {
    int total = 0;
    Au_t cur = type_au;
    while (cur) {
        total += count_methods(cur);
        cur = cur->context;
        if (cur == typeid(Au)) break;
    }
    return total;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_type_for - map an Au_t to a DWARF type metadata ref
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_type_for(aether a, Au_t src) {
    if (!a->debug || !a->compile_unit) return null;

    // null type → opaque pointer
    if (!src)
        return LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "ptr", 3, 64, DW_ATE_address, LLVMDIFlagZero);

    // check for cached debug type
    etype et = u(etype, src);
    if (et && et->lldebug)
        return et->lldebug;

    LLVMMetadataRef result = null;

    // enum types → proper enumeration
    if (src->is_enum) {
        result = debug_enum_type(a, src);
        if (result) {
            if (et) et->lldebug = result;
            return result;
        }
        // fall through to basic unsigned if enum emission failed
    }

    // struct or class types → composite struct type
    if (src->is_struct || src->is_class) {
        if (src->is_pointer || src->is_class) {
            result = debug_pointer_type(a, src);
        } else {
            result = debug_struct_type(a, src);
        }
        if (result) {
            if (et) et->lldebug = result;
            return result;
        }
    }

    // pointer types → pointer to pointee
    if (src->is_pointer) {
        result = debug_pointer_type(a, src);
        if (result) {
            if (et) et->lldebug = result;
            return result;
        }
    }

    // function pointer types
    if (src->is_funcptr) {
        result = debug_funcptr_type(a, src);
        if (result) {
            if (et) et->lldebug = result;
            return result;
        }
    }

    // alias types → typedef
    if (src->is_alias && src->src && src->src != src) {
        result = debug_typedef_type(a, src);
        if (result) {
            if (et) et->lldebug = result;
            return result;
        }
    }

    // boolean type
    if (src == typeid(bool)) {
        result = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "bool", 4, 8, DW_ATE_boolean, LLVMDIFlagZero);
        if (et) et->lldebug = result;
        return result;
    }

    u32 bits = bits_for_type(a, src);
    cstr name = src->ident ? src->ident : "unknown";
    u32 name_len = strlen(name);

    // float / double
    if (src->is_realistic) {
        result = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, name, name_len, bits,
            DW_ATE_float, LLVMDIFlagZero);
        if (et) et->lldebug = result;
        return result;
    }

    // signed integers
    if (src->is_signed || (src->is_integral && !src->is_unsigned)) {
        result = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, name, name_len, bits,
            DW_ATE_signed, LLVMDIFlagZero);
        if (et) et->lldebug = result;
        return result;
    }

    // unsigned integers
    if (src->is_unsigned) {
        result = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, name, name_len, bits,
            DW_ATE_unsigned, LLVMDIFlagZero);
        if (et) et->lldebug = result;
        return result;
    }

    // void type
    if (src->is_void) {
        return null;
    }

    // fallback: pointer-sized opaque type
    result = LLVMDIBuilderCreateBasicType(
        a->dbg_builder, name, name_len, bits,
        DW_ATE_address, LLVMDIFlagZero);
    if (et) et->lldebug = result;
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_pointer_type - create a pointer type pointing to the underlying type
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_pointer_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au) return null;

    u32 ptr_bits = pointer_bits(a);
    cstr name = type_au->ident ? type_au->ident : "ptr";
    u32 name_len = strlen(name);

    // for class types, the pointer points to the user struct
    // find the underlying struct/class type to build a pointee
    Au_t pointee = null;
    if (type_au->is_class) {
        pointee = type_au;
    } else if (type_au->is_pointer && type_au->src) {
        pointee = type_au->src;
    }

    LLVMMetadataRef pointee_di = null;
    if (pointee && (pointee->is_struct || pointee->is_class)) {
        // build the underlying struct type (this caches on lldebug)
        pointee_di = debug_struct_type(a, pointee);
    } else if (pointee) {
        // recurse for the pointee type
        pointee_di = debug_type_for(a, pointee);
    }

    if (!pointee_di) {
        // opaque pointer to u8
        pointee_di = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "u8", 2, 8, DW_ATE_unsigned_char, LLVMDIFlagZero);
    }

    return LLVMDIBuilderCreatePointerType(
        a->dbg_builder, pointee_di,
        ptr_bits, 0, 0,
        name, name_len);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_funcptr_type - create a DW_TAG_pointer_type to a subroutine type
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_funcptr_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au) return null;

    u32 ptr_bits = pointer_bits(a);
    cstr name = type_au->ident ? type_au->ident : "funcptr";
    u32 name_len = strlen(name);

    // build the subroutine type from the function's argument and return types
    LLVMMetadataRef sr_type = debug_subroutine_type(a, type_au);
    if (!sr_type) {
        sr_type = LLVMDIBuilderCreateSubroutineType(
            a->dbg_builder, a->file, null, 0, LLVMDIFlagZero);
    }

    return LLVMDIBuilderCreatePointerType(
        a->dbg_builder, sr_type,
        ptr_bits, 0, 0,
        name, name_len);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_array_type - create a DW_TAG_array_type for fixed-size arrays
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_array_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au || !type_au->src) return null;

    LLVMMetadataRef elem_di = debug_type_for(a, type_au->src);
    if (!elem_di) return null;

    i64 count = type_au->elements > 0 ? type_au->elements : 0;

    LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(
        a->dbg_builder, 0, count);

    return LLVMDIBuilderCreateArrayType(
        a->dbg_builder,
        bits_for_type(a, type_au),
        align_for_type(a, type_au),
        elem_di,
        &subrange, 1);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_typedef_type - create a DW_TAG_typedef for alias types
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_typedef_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au || !type_au->ident) return null;

    Au_t underlying = type_au->src;
    if (!underlying || underlying == type_au) return null;

    LLVMMetadataRef underlying_di = debug_type_for(a, underlying);
    if (!underlying_di) return null;

    return LLVMDIBuilderCreateTypedef(
        a->dbg_builder,
        underlying_di,
        type_au->ident, strlen(type_au->ident),
        a->file, 0, a->compile_unit, 0);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_enum_type - create a DW_TAG_enumeration_type with enumerators
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_enum_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au || !type_au->ident) return null;

    etype et = u(etype, type_au);
    if (et && et->lldebug) return et->lldebug;

    int n_values = count_enum_values(type_au);
    u32 bits = bits_for_type(a, type_au);
    u32 align = align_for_type(a, type_au);

    // build the underlying integer type for the enum
    bool is_signed = type_au->is_signed;
    LLVMMetadataRef underlying = LLVMDIBuilderCreateBasicType(
        a->dbg_builder,
        is_signed ? "int" : "unsigned int",
        is_signed ? 3 : 12,
        bits,
        is_signed ? DW_ATE_signed : DW_ATE_unsigned,
        LLVMDIFlagZero);

    // if no enum values found, return a basic type
    if (n_values == 0) {
        LLVMMetadataRef result = LLVMDIBuilderCreateBasicType(
            a->dbg_builder,
            type_au->ident, strlen(type_au->ident),
            bits, DW_ATE_unsigned, LLVMDIFlagZero);
        if (et) et->lldebug = result;
        return result;
    }

    // allocate and populate enumerator metadata array
    LLVMMetadataRef* enumerators = calloc(n_values, sizeof(LLVMMetadataRef));
    int idx = 0;
    for (int i = 0; i < type_au->members.count; i++) {
        Au_t m = (Au_t)type_au->members.origin[i];
        if (m->member_type != AU_MEMBER_ENUMV) continue;

        cstr ev_name = m->ident ? m->ident : "?";
        i64  ev_val  = (i64)m->offset;

        enumerators[idx++] = LLVMDIBuilderCreateEnumerator(
            a->dbg_builder,
            ev_name, strlen(ev_name),
            ev_val, !is_signed);
    }

    LLVMMetadataRef result = LLVMDIBuilderCreateEnumerationType(
        a->dbg_builder,
        a->compile_unit,
        type_au->ident, strlen(type_au->ident),
        a->file, 0,
        bits, align,
        enumerators, idx,
        underlying);

    free(enumerators);
    if (et) et->lldebug = result;
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_struct_type - build a DWARF composite struct for a struct/class type
//
// walks the inheritance chain to flatten all instance members from base to
// derived.  creates DW_TAG_member entries for each non-static AU_MEMBER_VAR,
// and DW_TAG_subprogram entries for methods attached to the type.
// uses a forward-declaration placeholder to break circular references.
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_struct_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!type_au || !type_au->ident) return null;

    // use lldebug as cache
    etype et = u(etype, type_au);
    if (et && et->lldebug) return et->lldebug;

    cstr struct_name = type_au->ident;
    u32  name_len    = strlen(struct_name);

    // compute total struct size and alignment
    u32 total_bits = type_au->abi_size ? type_au->abi_size : 0;
    u32 align      = type_au->align_bits ? type_au->align_bits : 64;

    // if etype has an lltype, use it for accurate size
    if (et && et->lltype) {
        u32 ll_bits = LLVMABISizeOfType(a->target_data, et->lltype) * 8;
        if (ll_bits) total_bits = ll_bits;
        u32 ll_align = LLVMABIAlignmentOfType(a->target_data, et->lltype) * 8;
        if (ll_align) align = ll_align;
    }

    // create a forward declaration to break cycles
    // when a struct member is a pointer back to this type, we use the fwd decl
    LLVMMetadataRef fwd_decl = LLVMDIBuilderCreateReplaceableCompositeType(
        a->dbg_builder,
        0x13,   // DW_TAG_structure_type
        struct_name, name_len,
        a->compile_unit,
        a->file, 0,
        0,
        total_bits, align,
        LLVMDIFlagFwdDecl,
        "", 0);

    // cache the forward declaration immediately to break recursion
    if (et) et->lldebug = fwd_decl;

    // ── count all members across the inheritance chain ──
    int total_vars    = count_all_instance_vars(type_au);
    int total_methods = count_all_methods(type_au);
    int member_count  = total_vars + total_methods;

    if (member_count == 0) {
        // empty struct: build a trivial struct type
        LLVMMetadataRef di_struct = LLVMDIBuilderCreateStructType(
            a->dbg_builder, a->compile_unit,
            struct_name, name_len,
            a->file, 0,
            total_bits, align,
            LLVMDIFlagZero, null,
            null, 0, 0, null,
            struct_name, name_len);
        LLVMMetadataReplaceAllUsesWith(fwd_decl, di_struct);
        if (et) et->lldebug = di_struct;
        return di_struct;
    }

    // ── allocate the member array ──
    LLVMMetadataRef* members = calloc(member_count, sizeof(LLVMMetadataRef));
    int midx = 0;

    // ── walk the inheritance chain (base first) ──
    // collect the chain into an array so we emit base members first
    Au_t chain[64];
    int chain_len = 0;
    {
        Au_t cur = type_au;
        while (cur && chain_len < 64) {
            chain[chain_len++] = cur;
            cur = cur->context;
            if (cur == typeid(Au)) break;
        }
    }

    // walk from base to derived (reverse order)
    for (int ci = chain_len - 1; ci >= 0; ci--) {
        Au_t level = chain[ci];

        // ── instance variable members ──
        for (int i = 0; i < level->members.count; i++) {
            Au_t m = (Au_t)level->members.origin[i];
            if (m->member_type != AU_MEMBER_VAR || m->is_static) continue;
            if (midx >= member_count) break;

            cstr mname     = m->ident ? m->ident : "_";
            u32  mname_len = strlen(mname);

            // determine the debug type for this member
            LLVMMetadataRef member_di = null;
            Au_t msrc = m->src;

            if (msrc && msrc == type_au) {
                // self-referential: pointer to self uses the forward decl
                member_di = LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, fwd_decl,
                    pointer_bits(a), 0, 0,
                    struct_name, name_len);
            } else if (msrc && (msrc->is_class || msrc->is_pointer)) {
                // pointer/class member: build pointer to the target type
                LLVMMetadataRef target_di = null;
                Au_t real_target = msrc->is_pointer ? msrc->src : msrc;
                if (real_target && (real_target->is_struct || real_target->is_class)) {
                    target_di = debug_struct_type(a, real_target);
                }
                if (!target_di) {
                    target_di = LLVMDIBuilderCreateBasicType(
                        a->dbg_builder, "u8", 2, 8,
                        DW_ATE_unsigned_char, LLVMDIFlagZero);
                }
                cstr ptr_name = msrc->ident ? msrc->ident : "ptr";
                member_di = LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, target_di,
                    pointer_bits(a), 0, 0,
                    ptr_name, strlen(ptr_name));
            } else if (msrc && msrc->is_struct && !msrc->is_pointer) {
                // inlined struct: build its composite type directly
                member_di = debug_struct_type(a, msrc);
            } else if (msrc && msrc->is_enum) {
                member_di = debug_enum_type(a, msrc);
            } else {
                member_di = debug_type_for(a, msrc);
            }

            if (!member_di) {
                // fallback: opaque i64
                member_di = LLVMDIBuilderCreateBasicType(
                    a->dbg_builder, "i64", 3, 64,
                    DW_ATE_signed, LLVMDIFlagZero);
            }

            u32 m_bits  = bits_for_type(a, msrc);
            u32 m_align = align_for_type(a, msrc);
            u64 offset_bits = (u64)m->offset * 8;

            members[midx++] = LLVMDIBuilderCreateMemberType(
                a->dbg_builder, a->compile_unit,
                mname, mname_len,
                a->file, 0,
                m_bits, m_align, offset_bits,
                LLVMDIFlagZero, member_di);
        }

        // ── method members (as DW_TAG_subprogram) ──
        for (int i = 0; i < level->members.count; i++) {
            Au_t m = (Au_t)level->members.origin[i];
            if (m->member_type != AU_MEMBER_FUNC &&
                m->member_type != AU_MEMBER_CONSTRUCT) continue;
            if (midx >= member_count) break;

            cstr fname     = m->alt ? m->alt : (m->ident ? m->ident : "_fn");
            u32  fname_len = strlen(fname);

            LLVMMetadataRef sr_type = debug_subroutine_type(a, m);
            if (!sr_type) {
                sr_type = LLVMDIBuilderCreateSubroutineType(
                    a->dbg_builder, a->file, null, 0, LLVMDIFlagZero);
            }

            LLVMMetadataRef method_sp = LLVMDIBuilderCreateFunction(
                a->dbg_builder,
                fwd_decl,
                fname, fname_len,
                fname, fname_len,
                a->file, 0,
                sr_type,
                false, false, 0,
                LLVMDIFlagZero, false);

            members[midx++] = method_sp;
        }
    }

    // ── build the final struct type with all members ──
    LLVMMetadataRef di_struct = LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        struct_name, name_len,
        a->file, 0,
        total_bits, align,
        LLVMDIFlagZero, null,
        members, midx, 0, null,
        struct_name, name_len);

    free(members);

    // replace the forward declaration with the real type
    LLVMMetadataReplaceAllUsesWith(fwd_decl, di_struct);
    if (et) et->lldebug = di_struct;
    return di_struct;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_subroutine_type - build a DWARF subroutine type with parameter types
//
// for Au_t fn descriptors: the return type comes from fn->src (rtype),
// and parameter types come from the fn->args list.
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_subroutine_type(aether a, Au_t fn_au) {
    if (!a->debug || !a->compile_unit) return null;
    if (!fn_au) return null;

    // DWARF subroutine type: element[0] = return type, elements[1..n] = params
    int n_args = fn_au->args.count;
    int n_elems = 1 + n_args; // return type + parameters

    LLVMMetadataRef* type_array = calloc(n_elems, sizeof(LLVMMetadataRef));

    // return type (null = void)
    Au_t rtype = fn_au->src; // also aliased as rtype in the union
    if (rtype && !rtype->is_void)
        type_array[0] = debug_type_for(a, rtype);
    else
        type_array[0] = null;

    // parameter types
    for (int i = 0; i < n_args; i++) {
        Au_t arg = (Au_t)fn_au->args.origin[i];
        Au_t arg_type = arg ? arg->src : null;
        type_array[1 + i] = debug_type_for(a, arg_type);
    }

    LLVMMetadataRef sr_type = LLVMDIBuilderCreateSubroutineType(
        a->dbg_builder, a->file,
        type_array, n_elems,
        LLVMDIFlagZero);

    free(type_array);
    return sr_type;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_au_header_type - build a DWARF struct for the _Au object header
//
// every Au-allocated object has a hidden header (struct _Au) placed before
// the user-visible pointer.  this function creates a debug type so the
// debugger can inspect that header.
//
// when a schema is provided, every Au_t-typed field in the header is
// resolved to a pointer to the schema's actual struct type rather than
// an opaque pointer.  this means fields like `type`, `context`, `schema`,
// `module`, `src`, `ptr`, and `scalar` all show rich struct information
// in the debugger instead of raw addresses.
//
// the `data` field (Au/pointer to user instance) is typed as a pointer
// to the schema's struct type as well, since it points to the actual
// user data following the header.
// ────────────────────────────────────────────────────────────────────────────

// helper: build a pointer-to-struct-type for an Au_t field, resolving
// through the schema when available
static LLVMMetadataRef au_field_pointer_type(aether a, Au_t field_src,
                                              Au_t schema,
                                              LLVMMetadataRef schema_struct,
                                              cstr field_name) {
    u32 ptr_bits = pointer_bits(a);

    // for Au_t fields: if we have a schema, the type descriptor fields
    // should point to the schema's struct representation
    if (field_src == typeid(Au_t) || (field_src && field_src->ident &&
        strcmp(field_src->ident, "Au_t") == 0)) {
        if (schema_struct) {
            cstr pname = schema->ident ? schema->ident : "Au_t";
            return LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_struct,
                ptr_bits, 0, 0, pname, strlen(pname));
        }
        // fallback: build Au_t as a struct type from typeid(Au_t)
        Au_t au_t_type = typeid(Au_t);
        if (au_t_type) {
            LLVMMetadataRef au_t_di = debug_struct_type(a, au_t_type);
            if (au_t_di) {
                return LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, au_t_di,
                    ptr_bits, 0, 0, "Au_t", 4);
            }
        }
    }

    // for Au fields (raw object pointer): point to the schema instance
    if (field_src && field_src->ident &&
        (strcmp(field_src->ident, "Au") == 0 || field_src == typeid(Au))) {
        if (schema_struct) {
            cstr pname = schema->ident ? schema->ident : "Au";
            return LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_struct,
                ptr_bits, 0, 0, pname, strlen(pname));
        }
    }

    return null; // caller should fall back to debug_type_for
}

LLVMMetadataRef debug_au_header_type(aether a, Au_t schema) {
    if (!a->debug || !a->compile_unit) return null;

    LLVMMetadataRef schema_struct = schema ? debug_struct_type(a, schema) : null;

    Au_t au = typeid(Au);
    int count = count_instance_vars(au);
    if (count == 0) return null;

    LLVMMetadataRef* members = calloc(count, sizeof(LLVMMetadataRef));
    int idx = 0;
    u32 ptr_bits = pointer_bits(a);

    for (int i = 0; i < au->members.count; i++) {
        Au_t m = (Au_t)au->members.origin[i];
        if (m->member_type != AU_MEMBER_VAR || m->is_static) continue;

        LLVMMetadataRef member_type = null;

        // for the `type` field: always resolve to pointer-to-schema-struct
        // this is the most important field - it holds the runtime type info
        if (schema_struct && m->ident && strcmp(m->ident, "type") == 0) {
            cstr sname = schema->ident ? schema->ident : "_Au";
            member_type = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_struct,
                ptr_bits, 0, 0, sname, strlen(sname));
        }
        // for `context`: the parent type in the inheritance chain
        // resolve to schema struct pointer for navigation in debugger
        else if (schema_struct && m->ident && strcmp(m->ident, "context") == 0) {
            Au_t parent = schema->context;
            if (parent && parent != typeid(Au)) {
                LLVMMetadataRef parent_di = debug_struct_type(a, parent);
                if (parent_di) {
                    cstr pname = parent->ident ? parent->ident : "context";
                    member_type = LLVMDIBuilderCreatePointerType(
                        a->dbg_builder, parent_di,
                        ptr_bits, 0, 0, pname, strlen(pname));
                }
            }
        }
        // for `schema`: the schema type descriptor
        else if (schema_struct && m->ident && strcmp(m->ident, "schema") == 0) {
            if (schema->schema) {
                LLVMMetadataRef schema_schema_di = debug_struct_type(a, schema->schema);
                if (schema_schema_di) {
                    cstr ssn = schema->schema->ident ? schema->schema->ident : "schema";
                    member_type = LLVMDIBuilderCreatePointerType(
                        a->dbg_builder, schema_schema_di,
                        ptr_bits, 0, 0, ssn, strlen(ssn));
                }
            }
        }
        // for `module`: the owning module type
        else if (schema_struct && m->ident && strcmp(m->ident, "module") == 0) {
            if (schema->module) {
                LLVMMetadataRef mod_di = debug_struct_type(a, schema->module);
                if (mod_di) {
                    cstr mn = schema->module->ident ? schema->module->ident : "module";
                    member_type = LLVMDIBuilderCreatePointerType(
                        a->dbg_builder, mod_di,
                        ptr_bits, 0, 0, mn, strlen(mn));
                }
            }
        }
        // for `src` (aliased as rtype/type in union): the source/return type
        else if (schema_struct && m->ident && strcmp(m->ident, "src") == 0) {
            if (schema->src) {
                LLVMMetadataRef src_di = debug_struct_type(a, schema->src);
                if (src_di) {
                    cstr sn = schema->src->ident ? schema->src->ident : "src";
                    member_type = LLVMDIBuilderCreatePointerType(
                        a->dbg_builder, src_di,
                        ptr_bits, 0, 0, sn, strlen(sn));
                }
            }
        }
        // for `scalar`: scalar type (Au_t)
        else if (schema_struct && m->ident && strcmp(m->ident, "scalar") == 0) {
            member_type = au_field_pointer_type(a, m->src, schema, schema_struct, "scalar");
        }
        // for `data`: pointer to user instance data
        else if (schema_struct && m->ident && strcmp(m->ident, "data") == 0) {
            cstr dn = schema->ident ? schema->ident : "data";
            member_type = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_struct,
                ptr_bits, 0, 0, dn, strlen(dn));
        }
        // for `ptr`: cached pointer-to-this type (Au_t)
        else if (schema_struct && m->ident && strcmp(m->ident, "ptr") == 0) {
            member_type = au_field_pointer_type(a, m->src, schema, schema_struct, "ptr");
        }

        // fallback: use the generic type resolver
        if (!member_type)
            member_type = debug_type_for(a, m->src);

        if (!member_type) {
            member_type = LLVMDIBuilderCreateBasicType(
                a->dbg_builder, "ptr", 3, 64,
                DW_ATE_address, LLVMDIFlagZero);
        }

        u32 m_bits  = bits_for_type(a, m->src);
        u32 m_align = align_for_type(a, m->src);
        u64 offset_bits = (u64)m->offset * 8;

        members[idx++] = LLVMDIBuilderCreateMemberType(
            a->dbg_builder, a->compile_unit,
            m->ident, strlen(m->ident),
            a->file, 0,
            m_bits, m_align, offset_bits,
            LLVMDIFlagZero, member_type);
    }

    // name the header struct after the schema for clarity
    static char hdr_name[256];
    if (schema && schema->ident)
        snprintf(hdr_name, sizeof(hdr_name), "%s_hdr", schema->ident);
    else
        snprintf(hdr_name, sizeof(hdr_name), "_Au");

    LLVMMetadataRef result = LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        hdr_name, strlen(hdr_name), a->file, 0,
        sizeof(struct _Au) * 8, 64,
        LLVMDIFlagZero, null,
        members, idx, 0, null,
        hdr_name, strlen(hdr_name));

    free(members);
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_object_header_type - build DWARF struct for _object (the full header)
//
// struct _object is the complete runtime header including type, shape,
// scalar, refs, managed, data, source, line, sequence, alloc, count,
// members_held, meta[8], and the trailing f pointer.
// this gives the debugger complete visibility into the runtime object model.
// ────────────────────────────────────────────────────────────────────────────
static LLVMMetadataRef debug_object_header_type(aether a, Au_t schema) {
    if (!a->debug || !a->compile_unit) return null;

    LLVMMetadataRef schema_struct = schema ? debug_struct_type(a, schema) : null;
    u32 ptr_bits = pointer_bits(a);

    // struct _object fields - manually specified to match object.h
    struct { cstr name; u32 bits; u32 encoding; u64 offset; bool is_ptr; } fields[] = {
        { "type",         ptr_bits, DW_ATE_address,  offsetof(struct _object, type) * 8,         true  },
        { "shape",        ptr_bits, DW_ATE_address,  offsetof(struct _object, shape) * 8,        true  },
        { "scalar",       ptr_bits, DW_ATE_address,  offsetof(struct _object, scalar) * 8,       true  },
        { "refs",         32,       DW_ATE_signed,   offsetof(struct _object, refs) * 8,         false },
        { "managed",      32,       DW_ATE_signed,   offsetof(struct _object, managed) * 8,      false },
        { "data",         ptr_bits, DW_ATE_address,  offsetof(struct _object, data) * 8,         true  },
        { "source",       ptr_bits, DW_ATE_address,  offsetof(struct _object, source) * 8,       true  },
        { "line",         32,       DW_ATE_signed,   offsetof(struct _object, line) * 8,         false },
        { "sequence",     32,       DW_ATE_signed,   offsetof(struct _object, sequence) * 8,     false },
        { "alloc",        64,       DW_ATE_signed,   offsetof(struct _object, alloc) * 8,        false },
        { "count",        64,       DW_ATE_signed,   offsetof(struct _object, count) * 8,        false },
        { "members_held", 8,        DW_ATE_boolean,  offsetof(struct _object, members_held) * 8, false },
    };
    int n_fields = sizeof(fields) / sizeof(fields[0]);

    // additional: meta[8] array and f pointer
    int total_members = n_fields + 2;
    LLVMMetadataRef* members = calloc(total_members, sizeof(LLVMMetadataRef));
    int midx = 0;

    for (int i = 0; i < n_fields; i++) {
        LLVMMetadataRef ftype;
        if (i == 0 && schema_struct) {
            // type field: pointer to schema's struct type
            cstr sn = schema ? schema->ident : "Au_t";
            ftype = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, schema_struct,
                ptr_bits, 0, 0,
                sn, strlen(sn));
        } else if (fields[i].is_ptr) {
            LLVMMetadataRef u8_type = LLVMDIBuilderCreateBasicType(
                a->dbg_builder, "u8", 2, 8,
                DW_ATE_unsigned_char, LLVMDIFlagZero);
            ftype = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, u8_type,
                ptr_bits, 0, 0,
                fields[i].name, strlen(fields[i].name));
        } else {
            ftype = LLVMDIBuilderCreateBasicType(
                a->dbg_builder,
                fields[i].name, strlen(fields[i].name),
                fields[i].bits, fields[i].encoding,
                LLVMDIFlagZero);
        }

        members[midx++] = LLVMDIBuilderCreateMemberType(
            a->dbg_builder, a->compile_unit,
            fields[i].name, strlen(fields[i].name),
            a->file, 0,
            fields[i].bits, fields[i].bits < 64 ? fields[i].bits : 64,
            fields[i].offset,
            LLVMDIFlagZero, ftype);
    }

    // meta[8] - array of 8 object pointers
    {
        LLVMMetadataRef u8_di = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "u8", 2, 8,
            DW_ATE_unsigned_char, LLVMDIFlagZero);
        LLVMMetadataRef obj_ptr = LLVMDIBuilderCreatePointerType(
            a->dbg_builder, u8_di, ptr_bits, 0, 0, "object", 6);
        LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(
            a->dbg_builder, 0, 8);
        LLVMMetadataRef arr_type = LLVMDIBuilderCreateArrayType(
            a->dbg_builder, ptr_bits * 8, ptr_bits, obj_ptr, &subrange, 1);

        members[midx++] = LLVMDIBuilderCreateMemberType(
            a->dbg_builder, a->compile_unit,
            "meta", 4,
            a->file, 0,
            ptr_bits * 8, ptr_bits,
            offsetof(struct _object, meta) * 8,
            LLVMDIFlagZero, arr_type);
    }

    // f - trailing object pointer
    {
        LLVMMetadataRef u8_di = LLVMDIBuilderCreateBasicType(
            a->dbg_builder, "u8", 2, 8,
            DW_ATE_unsigned_char, LLVMDIFlagZero);
        LLVMMetadataRef f_type = LLVMDIBuilderCreatePointerType(
            a->dbg_builder, u8_di, ptr_bits, 0, 0, "object", 6);

        members[midx++] = LLVMDIBuilderCreateMemberType(
            a->dbg_builder, a->compile_unit,
            "f", 1,
            a->file, 0,
            ptr_bits, ptr_bits,
            offsetof(struct _object, f) * 8,
            LLVMDIFlagZero, f_type);
    }

    cstr hdr_name = "_object";
    if (schema && schema->ident) {
        // name it after the schema for clarity (e.g. "string_object")
        static char namebuf[256];
        snprintf(namebuf, sizeof(namebuf), "%s_object", schema->ident);
        hdr_name = namebuf;
    }

    LLVMMetadataRef result = LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        hdr_name, strlen(hdr_name),
        a->file, 0,
        sizeof(struct _object) * 8, 64,
        LLVMDIFlagZero, null,
        members, midx, 0, null,
        hdr_name, strlen(hdr_name));

    free(members);
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// debug_combined_type - build a struct showing both the _Au header and
// the user's instance data together as one composite
//
// when debugging, seeing the object header alongside the user fields helps
// understand the runtime layout.  this creates:
//
//   struct Combined_<TypeName> {
//       struct _object  header;
//       struct <Type>   instance;
//   }
// ────────────────────────────────────────────────────────────────────────────
static LLVMMetadataRef debug_combined_type(aether a, Au_t schema) {
    if (!a->debug || !a->compile_unit) return null;
    if (!schema) return null;

    LLVMMetadataRef header_di   = debug_object_header_type(a, schema);
    LLVMMetadataRef instance_di = debug_struct_type(a, schema);

    if (!header_di || !instance_di) return null;

    u32 ptr_bits = pointer_bits(a);
    u64 hdr_size = sizeof(struct _object) * 8;

    // estimate instance size
    u32 inst_bits = 0;
    etype et = u(etype, schema);
    if (et && et->lltype)
        inst_bits = LLVMABISizeOfType(a->target_data, et->lltype) * 8;
    else if (schema->abi_size)
        inst_bits = schema->abi_size;

    u64 total_bits = hdr_size + inst_bits;

    LLVMMetadataRef members[2];
    members[0] = LLVMDIBuilderCreateMemberType(
        a->dbg_builder, a->compile_unit,
        "_header", 7,
        a->file, 0,
        hdr_size, 64, 0,
        LLVMDIFlagZero, header_di);
    members[1] = LLVMDIBuilderCreateMemberType(
        a->dbg_builder, a->compile_unit,
        "data", 4,
        a->file, 0,
        inst_bits, 64, hdr_size,
        LLVMDIFlagZero, instance_di);

    static char namebuf[256];
    snprintf(namebuf, sizeof(namebuf), "_Combined_%s",
        schema->ident ? schema->ident : "unknown");

    return LLVMDIBuilderCreateStructType(
        a->dbg_builder, a->compile_unit,
        namebuf, strlen(namebuf),
        a->file, 0,
        total_bits, 64,
        LLVMDIFlagZero, null,
        members, 2, 0, null,
        namebuf, strlen(namebuf));
}

// ────────────────────────────────────────────────────────────────────────────
// emit_debug_function - create a DISubprogram for a function and attach it
//
// builds a proper subroutine type with parameter types rather than an empty
// signature.  for instance methods, scopes the subprogram inside the class
// DI type if available.
// ────────────────────────────────────────────────────────────────────────────
void emit_debug_function(aether a, efunc fn) {
    if (!a->debug || !a->compile_unit) return;
    Au_t au = fn->au;
    if (!au->ident) return;

    LLVMMetadataRef file_ref = a->file;

    // build the subroutine type with actual parameter types
    LLVMMetadataRef sr_type = debug_subroutine_type(a, au);
    if (!sr_type) {
        sr_type = LLVMDIBuilderCreateSubroutineType(
            a->dbg_builder, file_ref, null, 0, LLVMDIFlagZero);
    }

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

// ────────────────────────────────────────────────────────────────────────────
// emit_debug_variable - emit a DW_TAG_auto_variable or parameter variable
//
// creates debug info for a local variable or function argument, and inserts
// an llvm.dbg.declare intrinsic to associate the variable's alloca with
// its debug info.
// ────────────────────────────────────────────────────────────────────────────
void emit_debug_variable(aether a, enode var, u32 arg_no, u32 line) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!var->au || !var->au->ident || !var->value) return;
    LLVMMetadataRef scope = debug_scope(a);
    if (!scope) return;

    cstr name     = var->au->ident;
    u32  name_len = strlen(name);
    Au_t var_type = var->au->src;

    // use the proper type for structs/classes rather than opaque pointer
    LLVMMetadataRef di_type = null;
    if (var_type && (var_type->is_struct || var_type->is_class)) {
        if (var_type->is_class || var_type->is_pointer) {
            // class types are always accessed through pointers
            LLVMMetadataRef struct_di = debug_struct_type(a, var_type);
            if (struct_di) {
                cstr tname = var_type->ident ? var_type->ident : "ptr";
                di_type = LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, struct_di,
                    pointer_bits(a), 0, 0,
                    tname, strlen(tname));
            }
        } else {
            // value-type struct: use the struct type directly
            di_type = debug_struct_type(a, var_type);
        }
    }
    if (!di_type)
        di_type = debug_type_for(a, var_type);
    if (!di_type) return;

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

// ────────────────────────────────────────────────────────────────────────────
// emit_debug_params - emit parameter debug variables for function arguments
//
// iterates all function arguments and creates debug parameter variables
// for each.  for instance methods, the first argument (self) is typed as
// a pointer to the class struct type, and an additional `_au` local variable
// is synthesized that points to the hidden Au object header, giving the
// debugger visibility into the runtime type info.
// ────────────────────────────────────────────────────────────────────────────
void emit_debug_params(aether a, efunc fn) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!fn || !fn->value) return;

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
            // for the self parameter, type it as a pointer to the class struct
            LLVMMetadataRef user_struct = debug_struct_type(a, au->context);
            if (user_struct) {
                di_type = LLVMDIBuilderCreatePointerType(
                    a->dbg_builder, user_struct,
                    pointer_bits(a), 0, 0,
                    au->context->ident, strlen(au->context->ident));

                // also build the Au header type for the _au synthetic variable
                etype e = u(etype, arg->src);
                if (e && e->schema)
                    struct_au = debug_au_header_type(a, e->schema->au);
            } else {
                di_type = debug_type_for(a, arg->src);
            }
        } else {
            // regular parameter: use proper type
            Au_t arg_type = arg->src;
            if (arg_type && (arg_type->is_struct || !arg_type->is_pointer &&
                            !arg_type->is_class)) {
                di_type = debug_type_for(a, arg_type);
            } else if (arg_type && (arg_type->is_class || arg_type->is_pointer)) {
                LLVMMetadataRef target_di = null;
                Au_t real_target = arg_type->is_pointer ? arg_type->src : arg_type;
                if (real_target && (real_target->is_struct || real_target->is_class))
                    target_di = debug_struct_type(a, real_target);
                if (target_di) {
                    cstr tname = arg_type->ident ? arg_type->ident : "ptr";
                    di_type = LLVMDIBuilderCreatePointerType(
                        a->dbg_builder, target_di,
                        pointer_bits(a), 0, 0,
                        tname, strlen(tname));
                } else {
                    di_type = debug_type_for(a, arg_type);
                }
            } else {
                di_type = debug_type_for(a, arg_type);
            }
        }

        if (!di_type) {
            di_type = LLVMDIBuilderCreateBasicType(
                a->dbg_builder, "ptr", 3, 64,
                DW_ATE_address, LLVMDIFlagZero);
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

        // for instance methods, emit the _au synthetic variable that shows
        // the Au object header (accessible by offsetting the self pointer
        // backward by sizeof(struct _Au))
        if (struct_au) {
            LLVMMetadataRef obj_type = LLVMDIBuilderCreatePointerType(
                a->dbg_builder, struct_au,
                pointer_bits(a), 0, 0,
                au->context->ident, strlen(au->context->ident));

            // compute header pointer: self - sizeof(struct _Au)
            LLVMTypeRef ll_i8  = LLVMInt8TypeInContext(a->module_ctx);
            LLVMTypeRef ll_i64 = LLVMInt64TypeInContext(a->module_ctx);
            LLVMValueRef neg_off = LLVMConstInt(ll_i64, (u64)(-(i64)sizeof(struct _Au)), true);
            LLVMValueRef header_ptr = LLVMBuildGEP2(B, ll_i8, param_val, &neg_off, 1, "_au.ptr");

            // alloca to hold the header pointer
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

// ────────────────────────────────────────────────────────────────────────────
// emit_debug_global - emit debug info for a global/static variable
//
// creates a DW_TAG_variable at module scope so that global variables are
// visible in the debugger when examining the compile unit.
// ────────────────────────────────────────────────────────────────────────────
void emit_debug_global(aether a, Au_t var_au, LLVMValueRef global_val) {
    if (!a->debug || !a->compile_unit) return;
    if (!var_au || !var_au->ident || !global_val) return;

    cstr name     = var_au->ident;
    u32  name_len = strlen(name);

    LLVMMetadataRef di_type = debug_type_for(a, var_au->src);
    if (!di_type) return;

    LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);

    LLVMDIBuilderCreateGlobalVariableExpression(
        a->dbg_builder,
        a->compile_unit,
        name, name_len,
        name, name_len,
        a->file, 0,
        di_type,
        false,  // is local
        expr,
        null, 0);
}

// ────────────────────────────────────────────────────────────────────────────
// emit_debug_member_access - emit debug info when accessing a struct member
//
// creates a debug location for member access expressions so the debugger
// can step through field accesses.
// ────────────────────────────────────────────────────────────────────────────
void emit_debug_member_access(aether a, Au_t member, u32 line, u32 column) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!member) return;

    LLVMMetadataRef scope = debug_scope(a);
    if (!scope) return;

    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        a->module_ctx, line, column, scope, null);
    LLVMSetCurrentDebugLocation2(B, loc);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_emit_type_metadata - emit all struct/class types in a module
//
// walks a module's member types and pre-builds their DWARF type metadata.
// this ensures that when the debugger encounters a pointer to one of these
// types, the full type description is already available.
// ────────────────────────────────────────────────────────────────────────────
void debug_emit_type_metadata(aether a, Au_t module_au) {
    if (!a->debug || !a->compile_unit) return;
    if (!module_au) return;

    for (int i = 0; i < module_au->members.count; i++) {
        Au_t m = (Au_t)module_au->members.origin[i];
        if (!m || !m->ident) continue;

        if (m->member_type == AU_MEMBER_TYPE) {
            if (m->is_struct || m->is_class) {
                debug_struct_type(a, m);
            } else if (m->is_enum) {
                debug_enum_type(a, m);
            }
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// debug_emit_inheritance - emit DW_TAG_inheritance entries for a class
//
// creates DWARF inheritance metadata so the debugger understands the class
// hierarchy and can display base class members correctly.
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_emit_inheritance(aether a, Au_t derived, Au_t base) {
    if (!a->debug || !a->compile_unit) return null;
    if (!derived || !base) return null;

    LLVMMetadataRef base_di = debug_struct_type(a, base);
    if (!base_di) return null;

    return LLVMDIBuilderCreateInheritance(
        a->dbg_builder,
        debug_struct_type(a, derived),
        base_di,
        0,        // offset
        0,        // vbptr offset
        LLVMDIFlagPublic);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_create_lexical_block - create a new lexical block scope
//
// DWARF uses lexical blocks to represent nested scopes (if/else, loops,
// compound statements).  variables declared inside a block get scoped
// correctly so the debugger only shows them when the block is active.
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_create_lexical_block(aether a, u32 line, u32 column) {
    if (!a->debug || !a->compile_unit) return null;

    LLVMMetadataRef parent_scope = debug_scope(a);
    if (!parent_scope) return null;

    return LLVMDIBuilderCreateLexicalBlock(
        a->dbg_builder,
        parent_scope,
        a->file,
        line, column);
}

// ────────────────────────────────────────────────────────────────────────────
// debug_create_namespace - create a DWARF namespace scope
//
// maps silver modules/namespaces to DW_TAG_namespace entries so types
// and functions appear under their module name in the debugger.
// ────────────────────────────────────────────────────────────────────────────
LLVMMetadataRef debug_create_namespace(aether a, cstr name) {
    if (!a->debug || !a->compile_unit) return null;
    if (!name) return null;

    return LLVMDIBuilderCreateNameSpace(
        a->dbg_builder,
        a->compile_unit,
        name, strlen(name),
        false);  // not exported
}

// ────────────────────────────────────────────────────────────────────────────
// debug_emit_local_alias - emit a debug alias for a value with a friendly name
//
// useful for creating synthetic debug variables that provide alternative
// views of data.  for example, showing a raw pointer as a typed struct.
// ────────────────────────────────────────────────────────────────────────────
void debug_emit_local_alias(aether a, cstr alias_name,
                            LLVMValueRef value,
                            LLVMMetadataRef di_type,
                            u32 line) {
    if (!a->debug || !a->compile_unit || a->no_build) return;
    if (!alias_name || !value || !di_type) return;

    LLVMMetadataRef scope = debug_scope(a);
    if (!scope) return;

    LLVMValueRef shadow = LLVMBuildAlloca(B, LLVMTypeOf(value), alias_name);
    LLVMBuildStore(B, value, shadow);

    u32 name_len = strlen(alias_name);
    LLVMMetadataRef di_var = LLVMDIBuilderCreateAutoVariable(
        a->dbg_builder, scope,
        alias_name, name_len,
        a->file, line,
        di_type, false, LLVMDIFlagZero, 0);

    LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);
    LLVMMetadataRef loc  = LLVMDIBuilderCreateDebugLocation(
        a->module_ctx, line, 0, scope, null);

    LLVMDIBuilderInsertDeclareRecordAtEnd(
        a->dbg_builder, shadow, di_var, expr, loc,
        LLVMGetInsertBlock(B));
}

// ────────────────────────────────────────────────────────────────────────────
// debug_emit_au_fields_for_type - emit typed views of each Au field for
// a specific type, so the debugger shows rich type info when inspecting
// the Au_t members table
// ────────────────────────────────────────────────────────────────────────────
void debug_emit_au_fields_for_type(aether a, Au_t type_au) {
    if (!a->debug || !a->compile_unit) return;
    if (!type_au || !type_au->ident) return;

    // pre-build the struct type so it's cached for debugger use
    if (type_au->is_struct || type_au->is_class)
        debug_struct_type(a, type_au);
    else if (type_au->is_enum)
        debug_enum_type(a, type_au);
}
