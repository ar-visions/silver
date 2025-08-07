
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>

//#include <clang-c/Index.h>


#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DataLayout.h>


#include <ports.h>
#include <string>

typedef LLVMMetadataRef LLVMScope;

extern "C" {
#include <aether/import>

#undef ctx
#undef str
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


using namespace clang;


struct model_ctx {
    size_t size;
    size_t alignment;
    std::string type_name;
};


static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e);


// Enum visitor equivalent
static enumeration create_enum(EnumDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    
    enumeration en = enumeration(mod, e, name, (token)n, members, map(hsize, 8));
    
    // Visit enum constants
    for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it) {
        EnumConstantDecl* ec = *it;
        std::string const_name = ec->getNameAsString();
        string cn = string(const_name.c_str());
        
        // Get the value if needed
        llvm::APSInt val = ec->getInitVal();
        
        emember ev = emember(mod, en->mod, name, (token)cn, mdl, en->src);
        ev->is_const = true;
        set(en->members, (A)cn, (A)ev);
    }
    
    register_model(e, (model)en);
    finalize((model)en);
    return en;
}

// Function creation
static fn create_fn(FunctionDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    
    // Get return type
    QualType return_qt = decl->getReturnType();
    model rtype = map_clang_type_to_model(return_qt, ctx, e);
    if (!rtype) rtype = emodel("none");
    
    // Process parameters
    eargs args = eargs(mod, e);
    
    for (unsigned i = 0; i < decl->getNumParams(); i++) {
        ParmVarDecl* param = decl->getParamDecl(i);
        QualType param_type = param->getType();
        std::string param_name = param->getNameAsString();
        
        // Handle unnamed parameters
        if (param_name.empty()) {
            param_name = "arg_" + std::to_string(i);
        }
        
        string pname = string(param_name.c_str());
        model mt = map_clang_type_to_model(param_type, ctx, e);
        if (!mt) continue;
        
        emember earg = emember(mod, e, name, (token)pname, mdl, mt, is_arg, true);
        set(args->members, (A)pname, (A)earg);
    }
    
    // Check for variadic
    bool is_variadic = decl->isVariadic();
    
    fn f = fn(mod, e, name, (token)n, rtype, rtype, args, args, va_args, is_variadic);
    register_model(e, (model)f);
    use(f);
    finalize((model)f);
    return f;
}

// Record (struct/union) creation
static record create_record(RecordDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    
    // Check if it's a union or struct
    bool is_union = decl->isUnion();
    
    record rec = is_union ?
        (record)uni(mod, e, name, (token)n) :
        (record)structure(mod, e, name, (token)n);
    
    rec->members = map(hsize, 8);
    
    // Get the layout for accurate offsets/sizes
    const ASTRecordLayout& layout = ctx.getASTRecordLayout(decl);
    
    int field_index = 0;
    for (auto field : decl->fields()) {
        std::string field_name = field->getNameAsString();
        if (field_name.empty()) {
            // Handle anonymous fields
            field_name = "__anon_" + std::to_string(field_index);
        }
        string fname = string(field_name.c_str());
        
        QualType field_type = field->getType();
        model mapped = map_clang_type_to_model(field_type, ctx, e);
        if (!mapped) continue;
        
        emember m = emember(mod, rec->mod, name, (token)fname, mdl, mapped);
        m->index = field_index++;
        
        // Get accurate offset (handles pragma pack!)
        if (!is_union) {
            uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());
            m->offset = offset_bits / 8;  // Convert to bytes
        }
        
        // Check for bitfield
        /*
        if (field->isBitField()) {
            m->is_bitfield = true;
            m->bitwidth = field->getBitWidthValue(ctx);
        }*/
        
        set(rec->members, (A)fname, (A)m);
    }
    
    // Set struct/union size and alignment
    rec->size = layout.getSize().getQuantity();
    rec->alignment = layout.getAlignment().getQuantity();
    
    register_model(e, (model)rec);
    finalize((model)rec);
    return rec;
}

// Function type helper
static model map_function_type(const FunctionProtoType* fpt, ASTContext& ctx, aether e) {
    QualType return_qt = fpt->getReturnType();
    model return_model = map_clang_type_to_model(return_qt, ctx, e);
    
    eargs param_models = eargs(mod, e);
    
    for (unsigned i = 0; i < fpt->getNumParams(); i++) {
        QualType param_type = fpt->getParamType(i);
        model param = map_clang_type_to_model(param_type, ctx, e);
        
        // Function types don't have parameter names
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "arg_%u", i);
        string n = string(name_buf);
        
        emember marg = emember(mod, e, name, null, mdl, param, is_arg, true);
        set(param_models->members, (A)n, (A)marg);
    }
    
    bool is_variadic = fpt->isVariadic();
    return (model)fn(mod, e, rtype, return_model, args, param_models, va_args, is_variadic);
}

// Function pointer helper
static model map_function_pointer(QualType pointee_qt, ASTContext& ctx, aether e) {
    const Type* pointee = pointee_qt.getTypePtr();
    
    if (const FunctionProtoType* fpt = dyn_cast<FunctionProtoType>(pointee)) {
        model func_model = map_function_type(fpt, ctx, e);
        return model(mod, e, src, func_model, is_ref, true);
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(pointee)) {
        // Old-style function without prototype
        QualType return_qt = fnpt->getReturnType();
        model return_model = map_clang_type_to_model(return_qt, ctx, e);
        eargs empty_args = eargs(mod, e);
        
        model func_model = (model)fn(mod, e, rtype, return_model, args, empty_args);
        return model(mod, e, src, func_model, is_ref, true);
    }
    
    return null;
}

// AST Visitor to find all declarations
class AetherDeclVisitor : public RecursiveASTVisitor<AetherDeclVisitor> {
private:
    ASTContext& ctx;
    aether e;
    
public:
    AetherDeclVisitor(ASTContext& context, aether ae) : ctx(context), e(ae) {}
    
    bool VisitEnumDecl(EnumDecl* decl) {
        if (!decl->getNameAsString().empty()) {
            create_enum(decl, ctx, e);
        }
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl* decl) {
        if (!decl->getNameAsString().empty()) {
            create_fn(decl, ctx, e);
        }
        return true;
    }
    
    bool VisitRecordDecl(RecordDecl* decl) {
        // Only process complete definitions
        if (decl->isCompleteDefinition() && !decl->getNameAsString().empty()) {
            create_record(decl, ctx, e);
        }
        return true;
    }
};


// Direct Clang type mapping (equivalent to map_ctype_to_model)
static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e) {
    const Type* type = qt.getTypePtr();
    
    // Strip elaborated type (like CXType_Elaborated)
    if (const ElaboratedType* et = dyn_cast<ElaboratedType>(type)) {
        return map_clang_type_to_model(et->getNamedType(), ctx, e);
    }
    
    // Handle typedefs
    if (const TypedefType* tt = dyn_cast<TypedefType>(type)) {
        std::string name = tt->getDecl()->getName().str();

        // Check for known typedefs
        if (name == "size_t") return emodel("usize");
        if (name == "ssize_t") return emodel("isize");
        if (name == "ptrdiff_t") return emodel("isize");
        if (name == "intptr_t") return emodel("isize");
        if (name == "uintptr_t") return emodel("usize");
        
        // Recurse on underlying type
        return map_clang_type_to_model(tt->getDecl()->getUnderlyingType(), ctx, e);
    }
    
    // Builtin types
    if (const BuiltinType* bt = dyn_cast<BuiltinType>(type)) {
        switch (bt->getKind()) {
        case BuiltinType::Void:        return emodel("none");
        case BuiltinType::Bool:        return emodel("bool");
        
        // Character types
        case BuiltinType::Char_U:      return emodel("u8");
        case BuiltinType::UChar:       return emodel("u8");
        case BuiltinType::Char_S:      return emodel("i8");
        case BuiltinType::SChar:       return emodel("i8");
        case BuiltinType::WChar_U:
        case BuiltinType::WChar_S:     return emodel("wchar");
        case BuiltinType::Char16:      return emodel("u16");
        case BuiltinType::Char32:      return emodel("u32");
        
        // Integer types
        case BuiltinType::UShort:      return emodel("u16");
        case BuiltinType::Short:       return emodel("i16");
        case BuiltinType::UInt:        return emodel("u32");
        case BuiltinType::Int:         return emodel("i32");
        case BuiltinType::ULong:       return emodel("u64");
        case BuiltinType::Long:        return emodel("i64");
        case BuiltinType::ULongLong:   return emodel("u64");
        case BuiltinType::LongLong:    return emodel("i64");
        case BuiltinType::Int128:      return emodel("i128");
        case BuiltinType::UInt128:     return emodel("u128");
        
        // Floating point
        case BuiltinType::Float:       return emodel("f32");
        case BuiltinType::Double:      return emodel("f64");
        case BuiltinType::LongDouble:  return emodel("f128");
        case BuiltinType::Float16:     return emodel("f16");
        case BuiltinType::Float128:    return emodel("f128");
        
        default:
            // Handle other builtin types
            break;
        }
    }
    
    // Complex types
    if (const ComplexType* ct = dyn_cast<ComplexType>(type)) {
        fault("complex members not supported");
        return nullptr;
    }
    
    // Array types
    if (const ConstantArrayType* cat = dyn_cast<ConstantArrayType>(type)) {
        QualType elem_type = cat->getElementType();
        int64_t size = cat->getSize().getSExtValue();
        model elem_model = map_clang_type_to_model(elem_type, ctx, e);
        
        if (!elem_model) return nullptr;
        
        if (elem_type.isConstQualified()) {
            elem_model = model(mod, e, src, elem_model, is_const, true);
        }
        
        return model(mod, e, src, elem_model, shape, (A)a(A_i64(size)));
    }
    
    if (const IncompleteArrayType* iat = dyn_cast<IncompleteArrayType>(type)) {
        QualType elem_type = iat->getElementType();
        model elem_model = map_clang_type_to_model(elem_type, ctx, e);
        if (!elem_model) return nullptr;
        return model(mod, e, src, elem_model, shape, (A)a(A_i64(0)));
    }
    
    // Pointer types
    if (const PointerType* pt = dyn_cast<PointerType>(type)) {
        QualType pointee = pt->getPointeeType();
        
        // Special string handling
        if (pointee->isCharType()) {
            if (pointee.isConstQualified()) {
                return emodel("cstr");
            }
            return emodel("mut_cstr");
        }
        
        // Function pointers
        if (pointee->isFunctionType()) {
            return map_function_pointer(pointee, ctx, e);
        }
        
        model base = map_clang_type_to_model(pointee, ctx, e);
        verify(base, "could not resolve pointer type");
        
        if (pointee.isConstQualified()) {
            base = model(mod, e, src, base, is_const, true);
        }
        /*
        if (pointee.isVolatileQualified()) {
            base = model_volatile(base);
        }
        if (qt.isRestrictQualified()) {
            return model_restrict_pointer(base);
        }*/
        
        return model(mod, e, src, base, is_ref, true);
    }
    
    // Record types (struct/union/class)
    if (const RecordType* rt = dyn_cast<RecordType>(type)) {
        RecordDecl* decl = rt->getDecl();
        return (model)create_record(decl, ctx, e);
    }
    
    // Enum types
    if (const EnumType* et = dyn_cast<EnumType>(type)) {
        EnumDecl* decl = et->getDecl();
        return (model)create_enum(decl, ctx, e);
    }
    
    // Function types
    /*
    if (const FunctionProtoType* fpt = dyn_cast<FunctionProtoType>(type)) {
        return map_function_type_from_proto(fpt, ctx, e);
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(type)) {
        return map_function_type_no_proto(fnpt, ctx, e);
    }
    */

    // Unhandled type
    std::string type_name = qt.getAsString();
    fault("unhandled Type kind: %s", type_name.c_str());
    return nullptr;
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


    e->current_include = null;
    return ipath;
}

}