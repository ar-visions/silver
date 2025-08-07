#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <clang-c/Index.h>
#include <ports.h>
#include <string>

typedef LLVMMetadataRef LLVMScope;

extern "C" {
#include <aether/import>

#undef render
#undef get
#undef clear
#undef fill
#undef move
#undef submit

#define emodel(N)     ({            \
    emember  m = lookup2(e, (A)string(N), null); \
    model mdl = m ? m->mdl : null;  \
    mdl;                            \
})

#define elookup(N)     ({ \
    emember  m = lookup2(e, (A)string(N), null); \
    m; \
})

#define emem(M, N) emember(mod, e, name, string(N), mdl, M);

#define earg(M, N) emember(mod, e, name, (token)string(N), mdl, M, is_arg, true);

#define value(m,vr) enode(mod, e, value, vr, mdl, m)

struct clang_import_ctx {
    aether e;
    path   full_path;
};

struct field_visit_ctx {
    record rec;
    int*   field_index;
};

static enum CXChildVisitResult clang_field_visitor(CXCursor c, CXCursor p, CXClientData d);
static model map_ctype_to_model(CXType type, aether e);

static enum CXChildVisitResult enum_visitor(CXCursor c, CXCursor p, CXClientData d) {
    enumeration en = (enumeration)d;
    CXString n = clang_getCursorSpelling(c);
    const char* s = clang_getCString(n);
    if (!s || !*s) {
        clang_disposeString(n);
        return CXChildVisit_Continue;
    }
    string name = string(s);
    clang_disposeString(n);

    emember ev = emember(mod, en->mod, name, (token)name, mdl, en->src);
    ev->is_const = true;
    set(en->members, (A)name, (A)ev);
    return CXChildVisit_Continue;
}

static enumeration create_enum(CXCursor cursor, aether e) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    CXString name  = clang_getCursorSpelling (cursor);
    symbol   cname = clang_getCString        (name);
    string   n     = string(cname);
    enumeration en = enumeration(mod, e, name, (token)n, members, map(hsize, 8));
    clang_visitChildren(cursor, enum_visitor, en);
    register_model(e, (model)en);
    finalize((model)en);
    return en;
}

static fn create_fn(CXCursor cursor, aether e) {
    CXType   ftype = clang_getCursorResultType(cursor);
    CXString name  = clang_getCursorSpelling (cursor);
    symbol   cname = clang_getCString        (name);
    string   n     = string(cname);
    model  rtype = map_ctype_to_model(ftype, e);
    if (!rtype) rtype = emodel("none");

    eargs args = eargs(mod, e);
    int n_args = clang_Cursor_getNumArguments(cursor);
    for (int i = 0; i < n_args; i++) {
        CXCursor arg = clang_Cursor_getArgument(cursor, i);
        CXType atype = clang_getCursorType(arg);
        CXString aname_str = clang_getCursorSpelling(arg);
        const char* aname_c = clang_getCString(aname_str);
        string aname = string(aname_c);
        clang_disposeString(aname_str);

        model   mt = map_ctype_to_model(atype, e);
        if    (!mt) continue;
        emember earg = earg(mt, aname_c);
        set(args->members, (A)aname, (A)earg);
    }
    fn f = fn(mod, e, name, (token)n, rtype, rtype, args, args);
    register_model(e, (model)f);
    use(f);
    finalize((model)f);
    return f;
}

static record create_record(CXCursor cursor, aether e) {
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    CXString name  = clang_getCursorSpelling (cursor);
    symbol   cname = clang_getCString        (name);
    string   n     = string(cname);
    clang_disposeString(name);
    record rec = (kind == CXCursor_UnionDecl) ?
        (record)uni      (mod, e, name, (token)n) :
        (record)structure(mod, e, name, (token)n);
    rec->members = map(hsize, 8);

    int    field_index = 0;
    struct field_visit_ctx field_ctx = { rec, &field_index };
    clang_visitChildren(cursor, clang_field_visitor, &field_ctx);
    register_model(e, (model)rec);
    finalize((model)rec);
    return rec;
}



// Helper function for function types
static model map_function_type(CXType type, aether e) {
    CXType result_type = clang_getResultType(type);
    model return_model = map_ctype_to_model(result_type, e);
    
    int num_args = clang_getNumArgTypes(type);
    eargs param_models = eargs(mod, e);
    
    for (int i = 0; i < num_args; i++) {
        CXType  arg_type = clang_getArgType(type, i);
        model   param    = map_ctype_to_model(arg_type, e);
        emember marg     = emember(mod, e, name, null, mdl, param, is_arg, true); // is it possible to get the Name here, or no?
        string  n        = f(string, "arg_%i", i);
        set(param_models->members, (A)n, (A)marg);
    }
    
    bool is_variadic = clang_isFunctionTypeVariadic(type);
    
    return (model)fn(mod, e, rtype, return_model, args, param_models, va_args, is_variadic);
}

static model map_function_pointer(CXType pointee, aether e) {
    model func_model = map_function_type(pointee, e);
    return model(mod, e, src, func_model, is_ref, true);
}

















static model map_ctype_to_model(CXType type, aether e) {
    switch (type.kind) {
    case CXType_Elaborated: {
        CXType named = clang_Type_getNamedType(type);
        return map_ctype_to_model(named, e);
    }
    
    case CXType_Typedef: {
        // Get the typedef name
        CXString typedef_name = clang_getTypeSpelling(type);
        const char* name_str = clang_getCString(typedef_name);
        clang_disposeString(typedef_name);
        
        // Get canonical type
        CXType canonical = clang_getCanonicalType(type);
        
        // Recurse on the underlying type
        model base = map_ctype_to_model(canonical, e);
        
        // Option: Create a typedef wrapper model to preserve the typedef name
        // return model_typedef(base, name_str);
        
        return base;
    }
    
    // Void type
    case CXType_Void: 
        return emodel("none");
    
    // Boolean
    case CXType_Bool: 
        return emodel("bool");
    
    // Character types
    case CXType_Char_U:
    case CXType_UChar: 
        return emodel("u8");
    case CXType_Char_S:
    case CXType_SChar: 
        return emodel("i8");
    case CXType_WChar: 
        return emodel("wchar");  // Platform-specific wide char
    case CXType_Char16: 
        return emodel("u16");    // char16_t
    case CXType_Char32: 
        return emodel("u32");    // char32_t
    
    // Integer types
    case CXType_UShort: 
        return emodel("u16");
    case CXType_Short: 
        return emodel("i16");
    case CXType_UInt: 
        return emodel("u32");
    case CXType_Int: 
        return emodel("i32");
    case CXType_ULong: 
        return emodel("u64");    // Note: platform-dependent
    case CXType_Long: 
        return emodel("i64");    // Note: platform-dependent
    case CXType_ULongLong: 
        return emodel("u64");
    case CXType_LongLong: 
        return emodel("i64");
    case CXType_Int128: 
        return emodel("i128");
    case CXType_UInt128: 
        return emodel("u128");
    
    // Floating point types
    case CXType_Float: 
        return emodel("f32");
    case CXType_Double: 
        return emodel("f64");
    case CXType_LongDouble: 
        return emodel("f128");   // Platform-dependent
    case CXType_Float16: 
        return emodel("f16");
    case CXType_Float128: 
        return emodel("f128");
    
    case CXType_Complex: {
        fault("complex members not supported");
        return null;
    }
    
    // Array types
    case CXType_ConstantArray: {
        CXType element_type = clang_getArrayElementType(type);
        i64 size = clang_getArraySize(type);
        model elem_model = map_ctype_to_model(element_type, e);
        
        if (!elem_model) return null;
        
        // Preserve const qualification if present
        bool is_const = clang_isConstQualifiedType(element_type);
        return model(mod, e, src, elem_model, shape, (A)a(A_i64(size)), is_const, is_const);
    }
    
    case CXType_IncompleteArray: {
        CXType element_type = clang_getArrayElementType(type);
        model elem_model = map_ctype_to_model(element_type, e);
        bool is_const = clang_isConstQualifiedType(element_type);
        return model(mod, e, src, elem_model, shape, (A)a(A_i64(0)), is_const, is_const);
    }
    
    // Pointer types
    case CXType_Pointer: {
        CXType pointee = clang_getPointeeType(type);
        
        // Special cases for string types
        if (pointee.kind == CXType_Char_S) {
            if (clang_isConstQualifiedType(pointee)) {
                return emodel("symbol");
            }
            return emodel("cstr");
        }
        
        if (pointee.kind == CXType_Char_U && clang_isConstQualifiedType(pointee)) {
            return emodel("symbol");
        }
        
        // Handle function pointers
        if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
            return map_function_pointer(pointee, e);
        }
        
        model base = map_ctype_to_model(pointee, e);
        verify(base, "could not resolve pointer type");
        
        // Preserve qualifiers
        if (clang_isConstQualifiedType(pointee)) {
            base = model(mod, e, src, base, is_const, true);
        }
        //if (clang_isVolatileQualifiedType(pointee)) {
        //    base = model_volatile(base);
        //}
        //if (clang_isRestrictQualifiedType(type)) {
        //    return model_restrict_pointer(base);
        //}
        
        return model(mod, e, src, base, is_ref, true);
    }
    
    // Record types (struct/union/class)
    case CXType_Record: {
        CXCursor cursor = clang_getTypeDeclaration(type);
        model record = (model)create_record(cursor, e);
        return record;
    }
    
    // Enum types
    case CXType_Enum: {
        CXCursor cursor = clang_getTypeDeclaration(type);
        return (model)create_enum(cursor, e);
    }
    
    // Function types
    case CXType_FunctionProto: {
        return map_function_type(type, e);
    }
    
    case CXType_FunctionNoProto: {
        // Old-style C function without prototype
        return map_function_type(type, e);
    }
    
    default: {
        // Get type spelling for error message
        CXString spelling = clang_getTypeSpelling(type);
        symbol type_str = clang_getCString(spelling);
        clang_disposeString(spelling);
        
        fault("unhandled CXType kind %i: %s", type.kind, type_str);
        return null;
    }
    }
}

















static enum CXChildVisitResult clang_field_visitor(CXCursor c, CXCursor p, CXClientData d) {
    struct   field_visit_ctx* ctx = (struct field_visit_ctx*)d;
    record   rec         = ctx->rec;
    int*     field_index = ctx->field_index;
    aether   e           = rec->mod;
    CXType   ctype       = clang_getCursorType(c);
    CXString fname       = clang_getCursorSpelling(c);
    symbol   cs          = clang_getCString(fname);

    if (!cs || !*cs) {
        clang_disposeString(fname);
        return CXChildVisit_Continue;
    }
    string mname = string((cstr)cs);
    clang_disposeString(fname);

    model mapped = map_ctype_to_model(ctype, e);
    if (!mapped) return CXChildVisit_Continue;
    model t = mapped;
    if (!t) return CXChildVisit_Continue;
    emember m = emember(mod, rec->mod, name, (token)mname, mdl, t);
    m->index = (*field_index)++;
    set(rec->members, (A)mname, (A)m);
    return CXChildVisit_Continue;
}

static enum CXChildVisitResult clang_visit_decl(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    struct clang_import_ctx* ctx = (struct clang_import_ctx*)client_data;
    aether e = ctx->e;

    CXSourceLocation loc = clang_getCursorLocation(cursor);
    //if (!clang_Location_isFromMainFile(loc)) return CXChildVisit_Continue;

    enum CXCursorKind kind = clang_getCursorKind(cursor);
    CXString name_str = clang_getCursorSpelling(cursor);
    const char* cname = clang_getCString(name_str);
    if (!cname || !*cname) {
        clang_disposeString(name_str);
        return CXChildVisit_Continue;
    }
    string name = string(cname);
    clang_disposeString(name_str);

    if (kind == CXCursor_StructDecl || kind == CXCursor_UnionDecl) {
        create_record(cursor, e);
        return CXChildVisit_Continue;
    }

    if (kind == CXCursor_FunctionDecl) {
        create_fn(cursor, e);
        return CXChildVisit_Continue;
    }

    if (kind == CXCursor_EnumDecl) {
        create_enum(cursor, e);
        return CXChildVisit_Continue;
    }

    return CXChildVisit_Recurse;
}


path aether_lookup_include(aether e, string include) {
    path     full_path = null;
    each(e->include_paths, path, i) {
        path r = f(path, "%o/%o", i, include);
        if (exists(r)) {
            full_path = r;
            break;
        }
    }
    verify(full_path, "could not resolve include path for %o", include);
    return full_path;
}

path aether_include(aether e, A inc) {
    path ipath = (AType)isa(inc) == typeid(string) ?
        lookup_include(e, (string)inc) : (path)inc;
    verify(ipath && exists(ipath), "include path does not exist: %o", ipath ? (A)ipath : inc);
    e->current_include = ipath;
    CXIndex index  = clang_createIndex(0, 0);
    symbol  args[] = {"-x", "c-header"};
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, ipath->chars, args, 2, NULL, 0, CXTranslationUnit_None);

    verify(unit, "unable to parse translation unit: %o", ipath);

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    struct clang_import_ctx ctx = { .e = e, .full_path = ipath };
    clang_visitChildren(cursor, clang_visit_decl, &ctx);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    e->current_include = null;
    return ipath;
}


}