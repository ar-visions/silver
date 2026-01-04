#include <iostream>

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Linker.h>

#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <clang/Driver/Driver.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/ToolChain.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <clang/Driver/Tool.h>

#include <ports.h>
#include <string>

typedef LLVMMetadataRef LLVMScope;

extern "C" {

#pragma pack(push, 1)
#include <aether/import>
#pragma pack(pop)

#undef ctx
#undef str
#undef render
#undef get
#undef clear
#undef fill
#undef move
#undef submit

using namespace clang;


Au_t ff;
Au_t f_arg;

// ============================================================================
// Helper macros for the new API
// ============================================================================

// Lookup a type by name in lexical scope
#define au_lookup(N) lexical(e->lexical, N)

// ============================================================================
// Forward declarations
// ============================================================================

static Au_t map_clang_type(const QualType& qt, ASTContext& ctx, aether e, symbol use_name);
static Au_t create_record(RecordDecl* decl, ASTContext& ctx, aether e, std::string name);
static Au_t create_class(CXXRecordDecl* cxx, ASTContext& ctx, aether e, std::string qname);
static Au_t create_enum(EnumDecl* decl, ASTContext& ctx, aether e, std::string name);
static Au_t create_fn(FunctionDecl* decl, ASTContext& ctx, aether e, std::string name);

// ============================================================================
// Utility functions
// ============================================================================

static std::string get_name(NamedDecl* decl) {
    clang::PrintingPolicy policy(decl->getASTContext().getLangOpts());
    policy.SuppressUnwrittenScope = true;
    policy.SuppressInlineNamespace = true;

    std::string out;
    llvm::raw_string_ostream os(out);
    decl->printQualifiedName(os, policy);
    return os.str();
}

#undef reverse
static std::vector<clang::NamedDecl*> namespace_stack(clang::NamedDecl *decl) {
    std::vector<clang::NamedDecl*> parts;

    for (const clang::DeclContext *ctx = decl->getDeclContext();
         ctx && !ctx->isTranslationUnit();
         ctx = ctx->getParent()) {

        if (const auto *D = llvm::dyn_cast<clang::Decl>(ctx)) {
            if (const auto *ND = llvm::dyn_cast<clang::NamedDecl>(D))
                parts.push_back(const_cast<clang::NamedDecl*>(ND));
        }
    }

    std::reverse(parts.begin(), parts.end());
    return parts;
}

static Au_t find_member(Au_t parent, symbol name) {
    if (!parent || !name) return null;
    for (int i = 0; i < parent->members.count; i++) {
        Au_t m = (Au_t)parent->members.origin[i];
        if (m->ident && strcmp(m->ident, name) == 0)
            return m;
    }
    return null;
}


static void push_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    Au_t cur = top_scope(e);
    for (clang::NamedDecl* n: s) {
        std::basic_string<char> s = n->getNameAsString();
        symbol name = s.c_str();
        Au_t m = find_member(cur, name);
        verify(m, "namespace not found: %s", name);
        push_scope(e, (Au)m);
        cur = m;
    }
}

static void pop_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    for (size_t i = 0; i < s.size(); i++) {
        pop(e->lexical); // pop from lexical stack
    }
}

static std::string cxx_mangle(const NamedDecl* D, ASTContext& ctx) {
    std::string out;
    llvm::raw_string_ostream os(out);

    std::unique_ptr<MangleContext> MC(
        ItaniumMangleContext::create(ctx, ctx.getDiagnostics()));

    if (const auto *VD = dyn_cast<VarDecl>(D)) {
        MC->mangleName(VD, os);
    } else if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        MC->mangleName(FD, os);
    } else if (const auto *MD = dyn_cast<CXXMethodDecl>(D)) {
        MC->mangleName(MD, os);
    } else {
        out = D->getNameAsString();
    }

    os.flush();
    return out;
}

// ============================================================================
// Type mapping from Clang to Au_t
// ============================================================================


static Au_t map_builtin_type(const BuiltinType* bt, aether e) {
    switch (bt->getKind()) {
        case BuiltinType::Void:        return au_lookup("none");
        case BuiltinType::Bool:        return au_lookup("bool");
        case BuiltinType::Char_U:      return au_lookup("u8");
        case BuiltinType::UChar:       return au_lookup("u8");
        case BuiltinType::Char_S:      return au_lookup("i8");
        case BuiltinType::SChar:       return au_lookup("i8");
        case BuiltinType::WChar_U:
        case BuiltinType::WChar_S:     return au_lookup("i32"); // wchar typically 32-bit
        case BuiltinType::Char16:      return au_lookup("u16");
        case BuiltinType::Char32:      return au_lookup("u32");
        case BuiltinType::UShort:      return au_lookup("u16");
        case BuiltinType::Short:       return au_lookup("i16");
        case BuiltinType::UInt:        return au_lookup("u32");
        case BuiltinType::Int:         return au_lookup("i32");
        case BuiltinType::ULong:       return au_lookup("u64");
        case BuiltinType::Long:        return au_lookup("i64");
        case BuiltinType::ULongLong:   return au_lookup("u64");
        case BuiltinType::LongLong:    return au_lookup("i64");
        case BuiltinType::Int128:      return au_lookup("i128");
        case BuiltinType::UInt128:     return au_lookup("u128");
        case BuiltinType::Float:       return au_lookup("f32");
        case BuiltinType::Double:      return au_lookup("f64");
        case BuiltinType::LongDouble:  return au_lookup("f64");
        case BuiltinType::Float16:     return au_lookup("f16");
        case BuiltinType::Float128:    return au_lookup("f128");
        default:
            return null;
    }
}

static Au_t map_function_type(const FunctionProtoType* fpt, ASTContext& ctx, aether e) {
    Au_t parent = top_scope(e);
    
    // Create function type
    Au_t fn = def(parent, null, AU_MEMBER_TYPE, AU_TRAIT_FUNCPTR);
    fn->module = e->current_import->au;
    // Return type
    fn->rtype = map_clang_type(fpt->getReturnType(), ctx, e, null);
    if (!fn->rtype) fn->rtype = au_lookup("none");
    
    // Parameters
    for (unsigned i = 0; i < fpt->getNumParams(); i++) {
        QualType param_type = fpt->getParamType(i);
        Au_t param = map_clang_type(param_type, ctx, e, null);
        if (param) {
            char name_buf[32];
            snprintf(name_buf, sizeof(name_buf), "arg_%u", i);
            Au_t arg = def(null, name_buf, AU_MEMBER_VAR, 0);
            arg->module = e->current_import->au;
            arg->type = param;
            array_qpush((array)&fn->args, (Au)arg);
        }
    }
    
    // Variadic
    if (fpt->isVariadic()) {
        fn->traits |= AU_TRAIT_VARGS; // mark as variadic somehow - may need a flag
    }
    
    return fn;
}

static Au_t map_function_pointer(QualType pointee_qt, ASTContext& ctx, aether e, symbol use_name) {
    const Type* pointee = pointee_qt.getTypePtr();
    
    if (const FunctionProtoType* fpt = dyn_cast<FunctionProtoType>(pointee)) {
        Au_t func = map_function_type(fpt, ctx, e);
        //Au_t ptr = def_pointer(null, func, use_name);
        verify(!use_name, "expected use_name to be null on this map_function_pointer");
        return func;
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(pointee)) {
        Au_t parent = top_scope(e);
        Au_t fn = def(parent, null, AU_MEMBER_FUNC, AU_TRAIT_FUNCPTR);
        fn->module = e->current_import->au;
        fn->rtype = map_clang_type(fnpt->getReturnType(), ctx, e, null);
        if (!fn->rtype) fn->rtype = au_lookup("none");
        Au_t ptr = def_pointer(null, fn, use_name);
        ptr->module = e->current_import->au;
        return ptr;
    }

    return null;
}

static Au_t map_clang_type(const QualType& qt, ASTContext& ctx, aether e, symbol use_name) {
    const Type* t = qt.getTypePtr();
    
    // Strip elaborated type
    if (const ElaboratedType* et = dyn_cast<ElaboratedType>(t)) {
        return map_clang_type(et->getNamedType(), ctx, e, use_name);
    }
    
    // Handle typedefs
    if (const TypedefType* tt = dyn_cast<TypedefType>(t)) {
        std::string name = tt->getDecl()->getName().str();
        Au_t existing = name.length() ? au_lookup(name.c_str()) : null;
        if (existing) return existing;
        
        symbol new_name = use_name ? use_name : (name.length() ? name.c_str() : null);
        Au_t underlying = map_clang_type(tt->getDecl()->getUnderlyingType(), ctx, e, null);
        
        if (underlying && new_name) {
            // Create alias
            Au_t alias = def_type(top_scope(e), new_name, AU_TRAIT_ALIAS);
            alias->module = e->current_import->au;
            alias->src = underlying;
            return alias;
        }
        return underlying;
    }

    QualType unqualified = qt.getCanonicalType().getUnqualifiedType();
    const Type* type = unqualified.getTypePtr();

    // Builtin types
    if (const BuiltinType* bt = dyn_cast<BuiltinType>(type)) {
        Au_t src = map_builtin_type(bt, e);
        if (src && use_name) {
            Au_t alias = def_type(top_scope(e), use_name, AU_TRAIT_ALIAS);
            alias->module = e->current_import->au;
            alias->src = src;
            return alias;
        }
        return src;
    }
    
    // Complex types
    if (const ComplexType* ct = dyn_cast<ComplexType>(type)) {
        fault("complex types not supported");
        return null;
    }
    
    // Constant array types
    if (const ConstantArrayType* cat = dyn_cast<ConstantArrayType>(type)) {
        QualType elem_type = cat->getElementType();
        int64_t esize = cat->getSize().getSExtValue();
        Au_t elem = map_clang_type(elem_type, ctx, e, null);
        
        if (!elem) return null;
        
        // Create array type - need to represent shape somehow
        // For now, create a type with size info
        Au_t arr = def_type(top_scope(e), use_name, 0);
        arr->module = e->current_import->au;
        arr->src = elem;
        arr->elements = esize; // store array size
        if (elem_type.isConstQualified()) {
            arr->traits |= AU_TRAIT_CONST;
        }
        return arr;
    }
    
    // Incomplete array types
    if (const IncompleteArrayType* iat = dyn_cast<IncompleteArrayType>(type)) {
        QualType elem_type = iat->getElementType();
        Au_t elem = map_clang_type(elem_type, ctx, e, null);
        if (!elem) return null;
        
        Au_t arr = def_type(top_scope(e), use_name, 0);
        arr->module = e->current_import->au;
        arr->src = elem;
        arr->elements = 0; // flexible array
        return arr;
    }
    
    // Pointer types
    if (const PointerType* pt = dyn_cast<PointerType>(type)) {
        QualType pointee = pt->getPointeeType();
        
        if (use_name) {
            Au_t existing = au_lookup(use_name);
            if (existing) return existing;
        }
        
        // Function pointers
        if (pointee->isFunctionType())
            return map_function_pointer(pointee, ctx, e, use_name);

        Au_t base = map_clang_type(pointee, ctx, e, null);
        //base->module = e->current_import->au;
        if (!base) base = au_lookup("ARef"); // opaque pointer
        
        verify(base, "could not resolve pointer type");
        
        Au_t ptr = def_pointer(null, base, use_name);
        ptr->module = e->current_import->au;
        if (pointee.isConstQualified()) {
            ptr->traits |= AU_TRAIT_CONST;
        }
        return ptr;
    }
    
    // Record types (struct/class)
    if (auto* RT = dyn_cast<RecordType>(type)) {
        RecordDecl* decl = RT->getDecl();
        std::string name = decl->getNameAsString();
        Au_t existing = name.length() ? au_lookup(name.c_str()) : null;
        if (existing) return existing;

        if (auto* CXX = dyn_cast<CXXRecordDecl>(decl)) {
            if (CXX->isCLike()) {
                return create_record(decl, ctx, e, get_name((NamedDecl*)decl));
            } else {
                return create_class((CXXRecordDecl*)decl, ctx, e, get_name((NamedDecl*)decl));
            }
        } else {
            return create_record(decl, ctx, e, get_name((NamedDecl*)decl));
        }
    }

    // Template specializations
    if (auto* T = dyn_cast<TemplateSpecializationType>(type)) {
        if (auto* RD = T->getAsCXXRecordDecl())
            return create_class(const_cast<CXXRecordDecl*>(RD), ctx, e, get_name((NamedDecl*)RD));
    }
    
    // Enum types
    if (const EnumType* et = dyn_cast<EnumType>(type)) {
        EnumDecl* decl = et->getDecl();
        std::string name = decl->getNameAsString();
        Au_t existing = name.length() ? au_lookup(name.c_str()) : null;
        return existing ? existing : create_enum(decl, ctx, e, get_name((NamedDecl*)decl));
    }

    // References → treat as pointers
    if (auto* L = dyn_cast<LValueReferenceType>(type)) {
        Au_t base = map_clang_type(L->getPointeeType(), ctx, e, null);
        return def_pointer(null, base, use_name);
    }
    if (auto* R = dyn_cast<RValueReferenceType>(type)) {
        Au_t base = map_clang_type(R->getPointeeType(), ctx, e, null);
        return def_pointer(null, base, use_name);
    }

    // Member pointers — opaque for now
    if (isa<MemberPointerType>(type)) {
        return au_lookup("ARef");
    }

    // Default to opaque
    return au_lookup("ARef");
}

// ============================================================================
// Declaration creation
// ============================================================================

static void set_fields(RecordDecl* decl, ASTContext& ctx, aether e, Au_t rec) {
    bool is_union = decl->isUnion();

    if (decl->isCompleteDefinition() && !decl->isInvalidDecl() && !decl->isDependentType()) {
        const ASTRecordLayout& layout = ctx.getASTRecordLayout(decl);
        
        int field_index = 0;
        for (auto field : decl->fields()) {
            std::string field_name = field->getNameAsString();
            if (field_name.empty()) {
                field_name = "__anon_" + std::to_string(field_index);
            }

            QualType field_type = field->getType();
            Au_t mapped = map_clang_type(field_type, ctx, e, null);

            if (!mapped) continue;
            
            Au_t m = def_member(rec, field_name.c_str(), mapped, AU_MEMBER_VAR, 0);
            uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());
            m->module = e->current_import->au;
            m->offset = offset_bits / 8;
            m->index = field_index++;
        }
    }
}

static Au_t create_record(RecordDecl* decl, ASTContext& ctx, aether e, std::string name) {
    bool has_name = name.length() > 0;
    symbol n = has_name ? name.c_str() : null;
    
    // Check if already exists
    Au_t existing = has_name ? au_lookup(n) : null;
    if (existing) return existing;

    bool is_union = decl->isUnion();
    Au_t parent = top_scope(e);

    // Incomplete definition → opaque
    if (!decl->isCompleteDefinition() || decl->isInvalidDecl() || decl->isDependentType()) {
        Au_t opaque = def_type(parent, n, AU_TRAIT_STRUCT);
        opaque->module = e->current_import->au;
        opaque->src = au_lookup("ARef");
        return opaque;
    }

    // Create struct/union
    u32 traits = is_union ? AU_TRAIT_UNION : AU_TRAIT_STRUCT;
    Au_t rec = def_type(parent, n, traits);
    rec->module = e->current_import->au;

    const ASTRecordLayout& layout = ctx.getASTRecordLayout(decl);
    rec->record_alignment = layout.getAlignment().getQuantity(); // in bytes

    push_scope(e, (Au)rec);
    set_fields(decl, ctx, e, rec);
    pop(e->lexical);
    return rec;
}

static Au_t create_opaque_class(CXXRecordDecl* cxx, aether e) {
    std::string qname = cxx->getQualifiedNameAsString();
    symbol n = qname.c_str();

    Au_t existing = au_lookup(n);
    if (existing) return existing;

    Au_t rec = def_class(top_scope(e), n);
    rec->module = e->current_import->au;
    return rec;
}

static Au_t create_class(CXXRecordDecl* cxx, ASTContext& ctx, aether e, std::string qname) {
    symbol n = qname.c_str();
    
    if (!cxx->isCompleteDefinition() || cxx->isDependentType() || cxx->isInvalidDecl())
        return create_opaque_class(cxx, e);
    
    Au_t existing = au_lookup(n);
    if (existing) return existing;

    Au_t parent = top_scope(e);
    Au_t rec = def_class(parent, n);
    rec->module = e->current_import->au;
    push_scope(e, (Au)rec);
    
    // Handle bases
    const ASTRecordLayout& layout = ctx.getASTRecordLayout(cxx);
    int base_index = 0;
    
    for (const auto& B : cxx->bases()) {
        const CXXRecordDecl* base = B.getType()->getAsCXXRecordDecl();
        if (!base) continue;
        
        Au_t base_rec = create_class(
            const_cast<CXXRecordDecl*>(base), ctx, e, get_name((NamedDecl*)base));
        
        char bname[32];
        snprintf(bname, sizeof(bname), "__base%d", base_index++);
        
        Au_t m = def_member(rec, bname, base_rec, AU_MEMBER_VAR, 0);
        m->module = e->current_import->au;
        m->offset = layout.getBaseClassOffset(base).getQuantity();
    }

    set_fields((RecordDecl*)cxx, ctx, e, rec);
    
    // Methods
    for (auto* md : cxx->methods()) {
        if (!md->getIdentifier()) continue;
        if (md->getAccess() != AS_public) continue;
        
        std::string disp = md->getQualifiedNameAsString();
        std::string mg = cxx_mangle(md, ctx);
        std::string method_name = disp + "#" + mg;
        
        Au_t fn = def(rec, method_name.c_str(), AU_MEMBER_FUNC, 
                              md->isStatic() ? AU_TRAIT_SMETHOD : AU_TRAIT_IMETHOD);
        fn->module = e->current_import->au;
        fn->rtype = map_clang_type(md->getReturnType(), ctx, e, null);
        if (!fn->rtype) fn->rtype = au_lookup("none");
        
        // Self parameter for instance methods
        if (!md->isStatic()) {
            Au_t self_type = rec;
            if (md->isConst()) {
                // Could mark as const
            }
            Au_t self_ptr = def_pointer(null, self_type, null);
            Au_t self_arg = def(null, "self", AU_MEMBER_VAR, 0);
            self_arg->type = self_ptr;
            array_qpush((array)&fn->args, (Au)self_arg);
        }
        
        // Parameters
        for (unsigned i = 0; i < md->getNumParams(); ++i) {
            ParmVarDecl* p = md->getParamDecl(i);
            Au_t mt = map_clang_type(p->getType(), ctx, e, null);
            if (!mt) continue;

            std::string pname = p->getNameAsString();
            if (pname.empty())
                pname = "arg_" + std::to_string(i);

            Au_t ap = def(null, pname.c_str(), AU_MEMBER_VAR, 0);
            ap->type = mt;
            array_qpush((array)&fn->args, (Au)ap);
        }
        
        // Store mangled name for linking
        fn->fn = (void*)strdup(mg.c_str()); // store extern name
    }
    
    pop(e->lexical);
    return rec;
}

static Au_t create_enum(EnumDecl* decl, ASTContext& ctx, aether e, std::string name) {
    symbol n = name.length() ? name.c_str() : null;
    
    Au_t parent = top_scope(e);
    Au_t en = def_enum(parent, n, 0);
    en->module = e->current_import->au;
    en->src = au_lookup("i32");
    
    push_scope(e, (Au)en);
    
    for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it) {
        EnumConstantDecl* ec = *it;
        std::string const_name = ec->getNameAsString();
        symbol cn = const_name.c_str();
        
        llvm::APSInt val = ec->getInitVal();
        i32* value = (i32*)_i32(val.getSExtValue());
        
        Au_t ev = def_enum_value(en, cn, (Au)value);
        ev->module = e->current_import->au;
        // For C-style enums, also register in parent scope
        if (!decl->isScoped()) {
            Au_t parent_ev = def_enum_value(parent, cn, (Au)value);
            parent_ev->module = e->current_import->au;
            parent_ev->type = en;
        }
    }
    
    pop(e->lexical);
    return en;
}

static Au_t create_fn(FunctionDecl* decl, ASTContext& ctx, aether e, std::string name) {
    symbol n = name.c_str();
    
    Au_t parent = top_scope(e);
    Au_t fn = def(parent, n, AU_MEMBER_FUNC, 0);
    if (n && strcmp(n, "vfprintf") == 0) {
        int test2 = 2;
        test2    += 2;
    }
    fn->module = e->current_import->au;
    if (name.length() != 0 && name == "puts") {
        fn = fn;
    }
    // Return type
    fn->rtype = map_clang_type(decl->getReturnType(), ctx, e, null);
    if (!fn->rtype) fn->rtype = au_lookup("none");
    
    // Parameters
    for (unsigned i = 0; i < decl->getNumParams(); i++) {
        ParmVarDecl* param = decl->getParamDecl(i);
        QualType param_type = param->getType();
        std::string param_name = param->getNameAsString();
        
        if (param_name.empty()) {
            param_name = "arg_" + std::to_string(i);
        }
        
        Au_t mt = map_clang_type(param_type, ctx, e, null);
        if (!mt) continue;
        
        Au_t arg = def(fn, param_name.c_str(), AU_MEMBER_VAR, 0);
        arg->module = e->current_import->au;
        arg->src = mt;
        array_qpush((array)&fn->args, (Au)arg);
    }
    
    // Variadic - may need a trait for this
    if (decl->isVariadic()) {
        fn->traits |= AU_TRAIT_VARGS; // reuse or add new trait
    }

    // Format attribute handling
    if (decl->hasAttr<FormatAttr>()) {
        for (auto *attr : decl->specific_attrs<FormatAttr>()) {
            int idx = attr->getFormatIdx();
            if (idx > 0 && idx <= (int)fn->args.count) {
                Au_t arg = (Au_t)fn->args.origin[idx - 1];
                arg->is_formatter = true;
                arg->value = (object)_i32(attr->getFirstArg() - 1);
            }
        }
    }

    return fn;
}

static Au_t create_namespace(NamespaceDecl* ns, ASTContext& ctx, aether e) {
    std::string qname = ns->getQualifiedNameAsString();
    bool is_inline = ns->isInlineNamespace();
    symbol n = qname.c_str();
    auto s = namespace_stack(ns);

    Au_t cur = top_scope(e);
    int index = 0;

    for (clang::NamedDecl* ndecl: s) {
        std::string ns_name = ndecl->getNameAsString();
        symbol name = ns_name.c_str();
        index++;
        
        Au_t existing = find_member(cur, name);

        if (index == (int)s.size()) {
            if (existing)
                return existing;

            // todo: fix
            Au_t ns_au = def_struct(cur, name);
            ns_au->module = e->current_import->au;
            return ns_au;
        } else {
            verify(existing, "expected namespace: %s", name);
            cur = existing;
        }
    }
    
    return null;
}

// ============================================================================
// AST Visitor
// ============================================================================

class AetherDeclVisitor2 : public RecursiveASTVisitor<AetherDeclVisitor2> {
private:
    ASTContext& ctx;
    aether e;
    
public:
    AetherDeclVisitor2(ASTContext& context, aether ae) : ctx(context), e(ae) {}
    
    bool VisitEnumDecl(EnumDecl* decl) {
        push_context(decl, e);
        if (!decl->getNameAsString().empty()) {
            create_enum(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        pop_context(decl, e);
        return true;
    }

    bool VisitNamespaceDecl(NamespaceDecl* ns) {
        create_namespace(ns, ctx, e);
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl* decl) {
        if (!decl->getNameAsString().empty()) {
            create_fn(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        return true;
    }
    
    bool VisitRecordDecl(RecordDecl* decl) {
        if (isa<CXXRecordDecl>(decl)) return true;
        if (decl->isCompleteDefinition() && !decl->getNameAsString().empty()) {
            create_record(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        return true;
    }

    bool VisitCXXRecordDecl(CXXRecordDecl* decl) {
        if (!decl->isCompleteDefinition()) return true;
        if (decl->isInjectedClassName()) return true;
        if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
            if (spec->getSpecializationKind() != TSK_ExplicitSpecialization &&
                spec->getSpecializationKind() != TSK_ImplicitInstantiation)
                return true;
        }
        create_class(decl, ctx, e, get_name((NamedDecl*)decl));
        return true;
    }
};

// ============================================================================
// AST Consumer and Actions
// ============================================================================

class AetherASTConsumer2 : public clang::ASTConsumer {
    aether e;
public:
    AetherASTConsumer2(aether e) : e(e) {}

    void HandleTranslationUnit(ASTContext& context) override {
        AetherDeclVisitor2 visitor(context, e);
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};

class AetherEmitAction2 : public clang::EmitLLVMOnlyAction {
    aether e;

public:
    AetherEmitAction2(aether e) : e(e) {}

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override {
        auto backend = EmitLLVMOnlyAction::CreateASTConsumer(CI, InFile);

        class CombinedConsumer : public clang::ASTConsumer {
            std::unique_ptr<clang::ASTConsumer> backend;
            AetherASTConsumer2 aetherConsumer;
        public:
            CombinedConsumer(aether e, std::unique_ptr<clang::ASTConsumer> backend)
                : backend(std::move(backend)), aetherConsumer(e) {}

            void HandleTranslationUnit(clang::ASTContext &Ctx) override {
                aetherConsumer.HandleTranslationUnit(Ctx);
                backend->HandleTranslationUnit(Ctx);
            }
        };

        return std::make_unique<CombinedConsumer>(e, std::move(backend));
    }
};

class SimpleDiagConsumer : public clang::DiagnosticConsumer {
    std::unique_ptr<clang::DiagnosticOptions> Opts;
    std::unique_ptr<clang::TextDiagnosticPrinter> Printer;
    bool Begun = false;

public:
    SimpleDiagConsumer() {
        Opts = std::make_unique<clang::DiagnosticOptions>();
        Opts->ShowCarets = true;
        Opts->ShowColors = true;
        Opts->ShowSourceRanges = true;
        Opts->ShowFixits = true;
        Printer = std::make_unique<clang::TextDiagnosticPrinter>(llvm::errs(), *Opts.get());
    }

    void BeginSourceFile(const clang::LangOptions &LO,
                         const clang::Preprocessor *PP) override {
        Printer->BeginSourceFile(LO, PP);
        Begun = true;
    }

    void EndSourceFile() override { Printer->EndSourceFile(); }

    void HandleDiagnostic(clang::DiagnosticsEngine::Level L,
                          const clang::Diagnostic &Info) override {
        if (!Begun) {
            clang::LangOptions LO;
            Printer->BeginSourceFile(LO, nullptr);
            Begun = true;
        }
        Printer->HandleDiagnostic(L, Info);
    }
};

// ============================================================================
// Macro Collector
// ============================================================================

typedef aether silver;
void print_tokens(silver mod, symbol label);

class MacroCollector2 : public clang::PPCallbacks {
public:
    aclang_cc instance;
    clang::Preprocessor* PP;

    explicit MacroCollector2(aclang_cc instance)
        : instance(instance), PP((clang::Preprocessor*)instance->PP) {}

    void MacroDefined(const clang::Token &macroNameTok,
                      const clang::MacroDirective *md) override {
        const clang::MacroInfo *mi = md->getMacroInfo();
    
        aether mod = instance->mod;
        aether e = mod; 
        std::string name = macroNameTok.getIdentifierInfo()->getName().str();
        symbol n = name.c_str();
        
        Au_t existing = au_lookup(n);
        if (existing)
            return;

        // For now, skip macro processing - would need macro type in new API
        // The old code created macro objects; new API may need def_macro or similar
        
        // Simple constant macros could be registered as enum values or constants
        // Function-like macros need special handling
        
        // TODO: Implement macro handling for new API
    }
};

// ============================================================================
// LLVM Module helpers
// ============================================================================

#undef release

static inline LLVMModuleRef wrap(llvm::Module *M) {
    return reinterpret_cast<LLVMModuleRef>(M);
}

static inline llvm::Module *unwrap(LLVMModuleRef M) {
    return reinterpret_cast<llvm::Module*>(M);
}

#undef print

// ============================================================================
// Main include function
// ============================================================================

path aether_lookup_include(aether e, string include) {
    array ipaths = a(e->sys_inc_paths, e->sys_exc_paths, e->include_paths);
    each(ipaths, array, includes) {
        if (includes)
            each(includes, path, i) {
                if (e->isysroot) {
                    path r = f(path, "%o/%o/%o", e->isysroot, i, include);
                    if (exists(r))
                        return r;
                }
                path r = f(path, "%o/%o", i, include);
                if (exists(r))
                    return r;
            }
    }

    verify(false, "could not resolve include path for %o", include);
    return null;
}

void aether_import_models(aether a, Au_t);

path aether_include(aether e, Au inc, string ns, ARef _instance) {
    aclang_cc* instance = (aclang_cc*)_instance;
    path ipath = (Au_t)isa(inc) == typeid(string) ?
        lookup_include(e, (string)inc) : (path)inc;

    verify(ipath && exists(ipath), "include path does not exist: %o",
        ipath ? (Au)ipath : inc);
    e->current_inc = ipath;

    string incl = string(ipath->chars);
    bool is_header = ends_with(incl, ".h") ||
                     ends_with(incl, ".hpp");

    auto DiagID(new DiagnosticIDs());
    auto DiagOpts = new DiagnosticOptions();
    TextDiagnosticPrinter *DiagPrinter = new TextDiagnosticPrinter(llvm::errs(), *DiagOpts);

    auto Invocation = std::make_shared<CompilerInvocation>();

    path res = f(path, "%o/lib/clang/22", e->install);
    path c = f(path, "/tmp/%o.c", stem(ipath));
    string contents = f(string, "#include \"%o\"\n", ipath);
    save(c, (Au)contents, null);

    symbol compile_unit = is_header ? c->chars : ipath->chars;

    DiagnosticsEngine diags(DiagID, *DiagOpts, DiagPrinter);

    path clang_path = f(path, "%o/bin/clang", e->install);
    driver::Driver drv(clang_path->chars, llvm::sys::getDefaultTargetTriple(), diags);

    std::vector<symbol> args = {
        "clang",
        "-x",
        "c",
        "-std=c11",
        "-fdiagnostics-show-option",
        "-Wno-nullability-completeness"
    };

    args.push_back("-w");
    args.push_back("-Wno-system-headers");

    if (e->isystem) {
        args.push_back("-isystem");
        args.push_back(e->isystem->chars);
    }

    if (e->resource_dir) {
        args.push_back("-resource-dir");
        args.push_back(e->resource_dir->chars);
    }

    if (e->isysroot) {
        args.push_back("-isysroot");
        args.push_back(e->isysroot->chars);
    }

    struct {
        symbol ident;
        array  paths;
    } all_paths[] = {
        { "-isystem", e->sys_inc_paths },
        { "-isystem", e->sys_exc_paths }
    };

    for (int i = 0, l = 2; i < l; i++) {
        symbol ident = all_paths[i].ident;
        array  paths = all_paths[i].paths;
        for (int ii = 0; ii < (paths ? paths->count : 0); ii++) {
            path f = (path)paths->origin[ii];
            args.push_back(ident);
            args.push_back(f->chars);
        }
    }

    if (e->framework_paths)
        for (int i = 0; i < e->framework_paths->count; i++) {
            path fw_path = (path)e->framework_paths->origin[i];
            string arg = f(string, "-F%o", fw_path);
            args.push_back(arg->chars);
        }

    if (e->include_paths)
        for (int i = 0; i < e->include_paths->count; i++) {
            path inc_path = (path)e->include_paths->origin[i];
            string arg = f(string, "%o", inc_path);
            args.push_back("-isystem");
            args.push_back(arg->chars);
        }

    args.push_back("-nostdinc++");
    args.push_back("-c");
    args.push_back(compile_unit);

    std::unique_ptr<driver::Compilation> comp(
        drv.BuildCompilation(llvm::ArrayRef<symbol>(args)));
    std::vector<symbol> compilation_args;
    for (clang::driver::Command &cmd : comp->getJobs()) {
        if (e->verbose) llvm::errs() << "command: ";
        if (StringRef(cmd.getCreator().getName()) == "clang") {
            for (symbol arg : cmd.getArguments()) {
                if (e->verbose) llvm::errs() << arg << " ";
                compilation_args.push_back(arg);
            }
            if (e->verbose) llvm::errs() << "\n";
        }
        if (e->verbose) llvm::errs() << "\n";
    }

    SimpleDiagConsumer* DiagClient = new SimpleDiagConsumer();
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags = 
        new DiagnosticsEngine(DiagID, *DiagOpts, DiagClient);
    llvm::ArrayRef<symbol> cmdline_args(compilation_args);
    Diags->setSuppressSystemWarnings(true);
    
    CompilerInvocation::CreateFromArgs(
        *Invocation,
        cmdline_args,
        *Diags
    );

    CompilerInstance* compiler = new CompilerInstance(Invocation);
    auto& LO = Invocation->getLangOpts();
    compiler->setDiagnostics(Diags.get());
    compiler->createFileManager();
    compiler->createSourceManager(compiler->getFileManager());

    auto fe = compiler->getFileManager().getFileRef(c->chars);
    verify(bool(fe), "cannot find file reference from compiler instance");
    verify(fe.get(), "clang cannot find TU file: %o", c);

    FileID mainFileID = compiler->getSourceManager().createFileID(
        fe.get(), 
        SourceLocation(), 
        SrcMgr::C_User
    );

    compiler->getSourceManager().setMainFileID(mainFileID);
    compiler->createTarget();
    compiler->createPreprocessor(TU_Complete);
    Diags->setIgnoreAllWarnings(true);
    Diags->setSuppressSystemWarnings(true);
    
    *instance = aclang_cc(
        mod, e, compiler, (handle)compiler, PP, (handle)&compiler->getPreprocessor());
    compiler->getPreprocessor().addPPCallbacks(
        std::make_unique<MacroCollector2>(*instance));
    compiler->createASTContext();
    ASTContext& ctx = compiler->getASTContext();

    if (is_header) {
        AetherASTConsumer2 consumer(e);
        ParseAST(compiler->getPreprocessor(), &consumer, ctx);
    } else {
        AetherEmitAction2 act(e);
        compiler->ExecuteAction(act);
        std::unique_ptr<llvm::Module> M = act.takeModule();
        LLVMModuleRef cMod = M ? wrap(M.release()) : nullptr;
        (*instance)->module = cMod;
        LLVMLinkModules2(e->module, cMod);
    }

    aether_import_models(e, top_scope(e));

    unlink(c->chars);
    e->current_inc = null;
    return ipath;
}

} // extern "C"