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
//#include <llvm/Support/Host.h>

#include <clang/Driver/Tool.h>

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
    emember  m = lookup2(e, (Au)string(N), null); \
    model mdl = m ? m->mdl : null;  \
    mdl;                            \
})

#define elookup(N)     ({ \
    emember  m = lookup2(e, (Au)string(N), null); \
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


static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e, string use_name);

static std::string get_name(NamedDecl* decl) {
    clang::PrintingPolicy policy(decl->getASTContext().getLangOpts());
    policy.SuppressUnwrittenScope = true;     // skip compiler-injected stuff
    policy.SuppressInlineNamespace = true;    // hide inline namespaces

    std::string out;
    llvm::raw_string_ostream os(out);
    decl->printQualifiedName(os, policy);
    return os.str();
}

// all have an implied to-level name, so it does make sense to keep that, too
// we must adjust how we use it now

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

    std::reverse(parts.begin(), parts.end()); // reverse order so we may navigate with it
    return parts;
}

Au map_get(map, Au);

static void push_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    model cur = e->top;
    for (clang::NamedDecl* n: s) {
        string name = string(n->getNameAsString().c_str());
        model m = (model)map_get(cur->members, (Au)name);
        verify(m, "namespace not found: %o", name);
        push(e, m);
    }
}

static void pop_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    for (auto n: s) {
        pop(e); // dont need verification here since we are 1:1 with above
    }
}

static std::string cxx_mangle(const NamedDecl* D, ASTContext& ctx) {
    std::string out;
    llvm::raw_string_ostream os(out);

    // Pick ABI style: Itanium (Linux/macOS) or Microsoft (MSVC)
    std::unique_ptr<MangleContext> MC(
        ItaniumMangleContext::create(ctx, ctx.getDiagnostics()));

    if (const auto *VD = dyn_cast<VarDecl>(D)) {
        MC->mangleName(VD, os);
    } else if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        MC->mangleName(FD, os);
    } else if (const auto *MD = dyn_cast<CXXMethodDecl>(D)) {
        MC->mangleName(MD, os);
    } else {
        // fall back to unmangled identifier if needed
        out = D->getNameAsString();
    }

    os.flush();
    return out;
}

static void create_method_stub(CXXMethodDecl* md, ASTContext& ctx, aether e, record owner) {
    if (!md->getIdentifier()) return;            // skip operators/unnamed
    if (md->getAccess() != AS_public) return;   // only public for now

    if (md->isOverloadedOperator()) {
        //rec->operator_new = (function)create_fn(method, ctx, e, get_name(method));
        int test2 = 2;
        test2    += 2;
    }

    // human-readable + ABI-mangled name
    std::string disp = md->getQualifiedNameAsString(); // ns::Class::method
    std::string mg   = cxx_mangle(md, ctx);
    std::string name = disp + "#" + mg;

    // return type
    model rtype = map_clang_type_to_model(md->getReturnType(), ctx, e, null);
    if (!rtype) rtype = emodel("none");

    // argument list
    eargs args = eargs(mod, e);

    // implicit "self" pointer for non-static methods
    if (!md->isStatic()) {
        model selfTy = (model)owner;
        if (md->isConst())
            selfTy = model(mod, e, src, selfTy, is_const, true);
        model selfPtr = model(mod, e, src, selfTy, is_ref, true);

        emember a0 = emember(mod, e, name,
                             (token)string("self"),
                             mdl, selfPtr,
                             is_arg, true);
        set(args->members, (Au)string("self"), (Au)a0);
    }

    // explicit parameters
    for (unsigned i = 0; i < md->getNumParams(); ++i) {
        ParmVarDecl* p = md->getParamDecl(i);
        model mt = map_clang_type_to_model(p->getType(), ctx, e, null);
        if (!mt) continue;

        std::string pname = p->getNameAsString();
        if (pname.empty())
            pname = "arg_" + std::to_string(i);

        emember ap = emember(mod, e, name,
                             (token)string(pname.c_str()),
                             mdl, mt,
                             is_arg, true);
        set(args->members, (Au)string(pname.c_str()), (Au)ap);
    }

    // build function
    bool variadic = md->isVariadic();

    string fname = string(name.c_str());

    push(e, (model)owner);
    function f = function(mod, e,
              rtype, rtype,
              args, args,
              extern_name, string(mg.c_str()),
              va_args, variadic);

    register_model(e, (model)f, fname, true);
    pop(e);
}


static none set_fields(RecordDecl* decl, ASTContext& ctx, aether e, record rec) {
    bool is_union = decl->isUnion();

    push(e, (model)rec);
    // Get the layout for accurate offsets/sizes
    if (decl->isCompleteDefinition() && !decl->isInvalidDecl() && !decl->isDependentType()) {
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
            model mapped = map_clang_type_to_model(field_type, ctx, e, fname);
            if (!mapped) continue;
            
            //push(e, mapped);
            emember m = emember(mod, rec->mod, name, (token)fname, mdl, mapped, context, top(e));
            //pop(e);

            uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());

            // important:
            // these specific members LLVM IR does not actually support in a general sense, and we must, as a user construct padding and operations
            // this is not remotely designed and, if our offsets differ, the compiler should error
            // for silver 1.0, we want bitfield support
            

            // todo: these were not being used in aether:
            /*m->override_offset_bits = _i32(offset_bits);

            // size
            if (field->isBitField()) {
                m->override_size_bits = _i32(field->getBitWidthValue());  // in bits
            } else if (const auto *IAT = dyn_cast<IncompleteArrayType>(field_type)) {
                // flexible array member (must be last): size is 0 in the struct
                m->override_size_bits = _i32(0);
            } else {
                m->override_size_bits = _i32(ctx.getTypeSizeInChars(field_type).getQuantity());
            }

            // alignment (prefer decl-level to capture alignas/attributes)
            CharUnits declAlign = ctx.getDeclAlign(field);
            if (declAlign.isZero())
                declAlign = ctx.getTypeAlignInChars(field_type);
            m->override_alignment_bits = _i32(declAlign.getQuantity());
            */

            // Get accurate offset (handles pragma pack!)
            if (!is_union) {
                uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());
                m->offset_bits = offset_bits / 8;  // Convert to bytes
            }

            m->index = field_index++;
            set(rec->members, (Au)fname, (Au)m);
        }
        
        // Set struct/union size and alignment
        rec->size_bits = layout.getSize().getQuantity() * 8;
        rec->alignment_bits = layout.getAlignment().getQuantity() * 8;
    }
    pop(e);
}

static record create_opaque_class(CXXRecordDecl* cxx, aether e) {
    std::string qname = cxx->getQualifiedNameAsString();
    string n = string(qname.c_str());

    if (emember existing = lookup2(e, (Au)n, null))
        return (record)existing->mdl;

    record rec = (record)Class(mod, e, ident, (token)n);
    register_model(e, (model)rec, n, true);
    return rec;
}

static record create_class(CXXRecordDecl* cxx, ASTContext& ctx, aether e, std::string qname) {
    string n = string(qname.c_str());
    
    if (!cxx->isCompleteDefinition() || cxx->isDependentType() || cxx->isInvalidDecl())
        return create_opaque_class(cxx, e);
    
    if (emember existing = lookup2(e, (Au)n, null))
        return (record)existing->mdl;

    record  rec = (record)Class(mod, e, members, map(hsize, 16));
    emember mem = register_model(e, (model)rec, n, false);
    
    // bases → embed base layout first (simple, single inheritance case)
    const ASTRecordLayout& layout = ctx.getASTRecordLayout(cxx);

    for (const auto& B : cxx->bases()) {
        const CXXRecordDecl* base = B.getType()->getAsCXXRecordDecl();
        if (!base) continue;
        record base_rec = (record)create_class(
            const_cast<CXXRecordDecl*>(base), ctx, e, get_name((NamedDecl*)base));
        // represent as a synthetic member named "__baseN"
        i32    N     = len((collective)rec->members);
        string bname = f(string, "__base%i", N);
        emember m = emember(mod, rec->mod, name, (token)bname, mdl, (model)base_rec);
        m->index = N;
        // byte offset:
        // todo: this was not being used by aether:
        //uint64_t off_bits = layout.getBaseClassOffset(base).getQuantity() * 8;
        //m->override_offset_bits = _i32(off_bits);
        set(rec->members, (Au)bname, (Au)m);
    }

    set_fields((RecordDecl*)cxx, ctx, e, rec);

    // class size/alignment
    rec->size_bits      = layout.getSize().getQuantity() * 8;
    rec->alignment_bits = layout.getAlignment().getQuantity() * 8;

    // methods (collect; shims in §4)
    for (auto* m : cxx->methods()) {
        create_method_stub(m, ctx, e, rec); // see §4
    }

    finalize(mem);
    return rec;
}


static enumeration create_enum(EnumDecl* decl, ASTContext& ctx, aether e, std::string name) {
    string n = string(name.c_str());

    // todo:
    // this needs to get all of the namespaces that stack up in the EnumDecl


    model top = e->top;
    enumeration en = enumeration(mod, e, members, map(hsize, 8));
    push(e, (model)en);

    // Visit enum constants
    for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it) {
        EnumConstantDecl* ec = *it;
        std::string const_name = ec->getNameAsString();
        string cn = string(const_name.c_str());

        // Get the value if needed
        llvm::APSInt val = ec->getInitVal();
        
        emember ev = emember(mod, en->mod, name, (token)cn, mdl, (model)en);
        ev->is_const = true;
        set(en->members, (Au)cn, (Au)ev);

        if (!decl->isScoped()) // top should be our updated context
            set(top->members, (Au)cn, (Au)ev); // this is how C enums work, so lets make it easy to lookup
    }
    pop(e);
    
    if (n && len((string)n))
        register_model(e, (model)en, n, true);
    
    return en;
}

// Function creation
static function create_fn(FunctionDecl* decl, ASTContext& ctx, aether e, std::string name) {
    string n = string(name.c_str());
    if (eq(n, "vkGetInstanceProcAddr")) {
        int test2 = 2;
        test2    += 2;
    }
    // Get return type
    QualType return_qt = decl->getReturnType();
    model rtype = map_clang_type_to_model(return_qt, ctx, e, null);
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
        model mt = map_clang_type_to_model(param_type, ctx, e, null);
        if (!mt) continue;
        
        emember earg = emember(mod, e, name, (token)pname, mdl, mt, is_arg, true, context, (model)args);
        set(args->members, (Au)pname, (Au)earg);
    }
    
    // Check for variadic
    bool is_variadic = decl->isVariadic();

    function f = function(mod, e, rtype, rtype, args, args, va_args, is_variadic);
    if (len(n) > 0)
        register_model(e, (model)f, (string)n, true);

    if (decl->hasAttr<FormatAttr>()) {
        for (auto *attr : decl->specific_attrs<FormatAttr>()) {
            // attr->getType() gives printf/scanf/strfmon
            int idx = attr->getFormatIdx(); // gives the position of the format string
            emember arg = (emember)value_by_index(args->members, idx);
            if (arg) arg->is_formatter = true; // we can require constant string here
            // attr->getFirstArg() gives the first variadic arg index
        }
    }

    return f;
}

// Record (struct/union) creation
static record create_record(RecordDecl* decl, ASTContext& ctx, aether e, std::string name) {
    string n = string(name.c_str());
    bool has_name = name.length() > 0;
    // Check if already exists
    emember existing = has_name ? lookup2(e, (Au)n, null) : (emember)null;
    if (existing && existing->mdl)
        return (record)existing->mdl;

    // Check if it's a union or struct
    bool is_union = decl->isUnion();
    if (is_union) {
        is_union = is_union;
    }

    model mdl_opaque = emodel("ARef");

    // c++ ville
    if (!decl->isCompleteDefinition() || decl->isInvalidDecl() || decl->isDependentType()) {
        record r_opaque = (record)model(mod, e, src, mdl_opaque);
        register_model(e, (model)r_opaque, n, false);
        return (record)r_opaque;
    }

    record rec = is_union ?
        (record)uni(mod, e, ident, (token)n, members, map(hsize, 8)) :
        (record)structure(mod, e, ident, (token)n, members, map(hsize, 8));
    
    emember mem = null;
    if (has_name)
        mem = register_model(e, (model)rec, n, false);
    push(e, mem->mdl);
    set_fields(decl, ctx, e, rec);
    pop(e);
    if (mem) finalize(mem);
    return rec;
}

// Function type helper
static model map_function_type(const FunctionProtoType* fpt, ASTContext& ctx, aether e) {
    QualType return_qt = fpt->getReturnType();
    model return_model = map_clang_type_to_model(return_qt, ctx, e, null);
    
    eargs param_models = eargs(mod, e);
    
    for (unsigned i = 0; i < fpt->getNumParams(); i++) {
        QualType param_type = fpt->getParamType(i);
        string arg_name = f(string, "arg_%i", i);
        model param = map_clang_type_to_model(param_type, ctx, e, arg_name);
        
        // Function types don't have parameter names
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "arg_%u", i);
        string n = string(name_buf);
        
        emember marg = emember(mod, e, name, null, mdl, param, is_arg, true);
        set(param_models->members, (Au)n, (Au)marg);
    }
    
    bool is_variadic = fpt->isVariadic();
    function f = function(mod, e, rtype, return_model, args, param_models, va_args, is_variadic);
    register_model(e, (model)f, null, true);
    return (model)f;
}

// Function pointer helper
static model map_function_pointer(QualType pointee_qt, ASTContext& ctx, aether e, string use_name) {
    const Type* pointee = pointee_qt.getTypePtr();
    
    if (const FunctionProtoType* fpt = dyn_cast<FunctionProtoType>(pointee)) {
        model func_model = map_function_type(fpt, ctx, e);
        register_model(e, func_model, null, true);

        model rmodel = model(mod, e, src, func_model, is_ref, true);
        register_model(e, rmodel, use_name, true);
        return rmodel;
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(pointee)) {
        // Old-style function without prototype
        QualType return_qt = fnpt->getReturnType();
        model return_model = map_clang_type_to_model(return_qt, ctx, e, null);
        eargs empty_args = eargs(mod, e);
        
        model func_model = (model)function(mod, e, rtype, return_model, args, empty_args);
        emember fmem = register_model(e, func_model, null, true);
        model func_ptr = model(mod, e, src, func_model, is_ref, true);
        emember ptr = register_model(e, func_ptr, null, true);
        return ptr->mdl;
    }

    return null;
}

// this should always create namespace one at a time, navigating to the location to create without error
static model create_namespace(NamespaceDecl* ns, ASTContext& ctx, aether e) {
    std::string qname = ns->getQualifiedNameAsString();
    bool   anonymous  = ns->isAnonymousNamespace();
    bool   is_inline  = ns->isInlineNamespace();
    string n          = string(qname.c_str());
    auto   s          = namespace_stack(ns);

    model cur = e->top;
    int index = 0;

    emember existing = null;
    for (clang::NamedDecl* ndecl: s) {
        std::string n = ndecl->getNameAsString();
        string name = string(n.c_str());
        index++;
        existing = (emember)map_get(cur->members, (Au)name);

        if (index == s.size() - 1) {
            // return if namespace already exists
            if (existing)
                return existing->mdl;

            model ns_model = (model)Namespace(
                mod, e, namespace_name, name, members, map(hsize, 16),
                namespace_inline, is_inline);

            register_model(e, ns_model, name, true);
            return ns_model;
        } else {
            // expect this namesapce to exist
            verify(existing || (index == s.size() - 1), "expected namespace: %o", name);
        }
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
        // we want to prepare context here; do we 'create' namespace at all, or just look it up?
        // 
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

    bool VisitDecl(Decl* d) {
        std::string n = d->getDeclKindName();
        if (n == "Namespace") {
            printf("something\n");
        }
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl* decl) {
        if (!decl->getNameAsString().empty()) {
            std::string n = decl->getNameAsString();
            create_fn(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        return true;
    }
    
    bool VisitRecordDecl(RecordDecl* decl) {
        // Only process complete definitions
        if (isa<CXXRecordDecl>(decl)) return true;
        if (decl->isCompleteDefinition() && !decl->getNameAsString().empty()) {
            create_record(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        return true;
    }

    bool VisitCXXRecordDecl(CXXRecordDecl* decl) {
        if (!decl->isCompleteDefinition()) return true;
        if (decl->isInjectedClassName())   return true;
        if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
            // only import concrete specializations
            if (spec->getSpecializationKind() != TSK_ExplicitSpecialization &&
                spec->getSpecializationKind() != TSK_ImplicitInstantiation)
                return true;
        }
        create_class(decl, ctx, e, get_name((NamedDecl*)decl));
        return true;
    }
};


// Direct Clang type mapping (equivalent to map_ctype_to_model)
static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e, string use_name) {
    const Type* t = qt.getTypePtr();

    if (use_name && eq(use_name, "u32")) {
        int test2 = 2;
        test2 += 2;
    }
    
    // Strip elaborated type (like CXType_Elaborated)
    if (const ElaboratedType* et = dyn_cast<ElaboratedType>(t)) {
        return map_clang_type_to_model(et->getNamedType(), ctx, e, use_name);
    }
    
    // Handle typedefs
    if (const TypedefType* tt = dyn_cast<TypedefType>(t)) {
        std::string name = tt->getDecl()->getName().str();
        symbol n = name.c_str();
        model existing = name.length() ? emodel(n) : (model)null;
        if (existing) return existing;
        if (!use_name) use_name = string(n);

        // Recurse on underlying type
        return map_clang_type_to_model(tt->getDecl()->getUnderlyingType(), ctx, e, use_name);
    }
    

    QualType unqualified = qt.getCanonicalType().getUnqualifiedType();
    const Type* type = unqualified.getTypePtr();

    //const Type* type = qt.getTypePtr();

    // Builtin types
    model src = null;
    if (const BuiltinType* bt = dyn_cast<BuiltinType>(type)) {
        switch (bt->getKind()) {
        case BuiltinType::Void:        src = emodel("none"); break;
        case BuiltinType::Bool:        src = emodel("bool"); break;
        
        // Character types
        case BuiltinType::Char_U:      src = emodel("u8");   break;
        case BuiltinType::UChar:       src = emodel("u8");  break;
        case BuiltinType::Char_S:      src = emodel("i8"); break;
        case BuiltinType::SChar:       src = emodel("i8"); break;
        case BuiltinType::WChar_U:
        case BuiltinType::WChar_S:     src = emodel("wchar"); break;
        case BuiltinType::Char16:      src = emodel("u16"); break;
        case BuiltinType::Char32:      src = emodel("u32"); break;
        
        // Integer types
        case BuiltinType::UShort:      src = emodel("u16"); break;
        case BuiltinType::Short:       src = emodel("i16"); break;
        case BuiltinType::UInt:        src = emodel("u32"); break;
        case BuiltinType::Int:         src = emodel("i32"); break;
        case BuiltinType::ULong:       src = emodel("u64"); break;
        case BuiltinType::Long:        src = emodel("i64"); break;
        case BuiltinType::ULongLong:   src = emodel("u64"); break;
        case BuiltinType::LongLong:    src = emodel("i64"); break;
        case BuiltinType::Int128:      src = emodel("i128"); break;
        case BuiltinType::UInt128:     src = emodel("u128"); break;
        
        // Floating point
        case BuiltinType::Float:       src = emodel("f32");  break;
        case BuiltinType::Double:      src = emodel("f64");  break;
        case BuiltinType::LongDouble:  src = emodel("f64");  break;
        case BuiltinType::Float16:     src = emodel("f16");  break;
        case BuiltinType::Float128:    src = emodel("f128"); break;
        
        default:
            // Handle other builtin types
            break;
        }

        if (src && use_name && len(use_name) > 0) {
            model mdl = model(mod, e, src, src);
            register_model(e, mdl, use_name, false);
            return mdl;
        } else if (src)
            return src;
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
        model elem_model = map_clang_type_to_model(elem_type, ctx, e, null);
        
        if (!elem_model) return nullptr;
        
        if (elem_type.isConstQualified()) {
            elem_model = model(mod, e, src, elem_model, is_const, true);
        }
        ARef sh = new_shape(size, 0);
        model mdl = model(mod, e, src, elem_model, shape, (shape)sh);
        register_model(e, mdl, use_name, false);
        return mdl;
    }
    
    if (const IncompleteArrayType* iat = dyn_cast<IncompleteArrayType>(type)) {
        QualType elem_type = iat->getElementType();
        model elem_model = map_clang_type_to_model(elem_type, ctx, e, null);
        if (!elem_model) return nullptr;
        ARef sh = new_shape(0, 0);
        return model(mod, e, src, elem_model, shape, (shape)sh);
    }
    
    // Pointer types
    if (const PointerType* pt = dyn_cast<PointerType>(type)) {
        QualType pointee = pt->getPointeeType();
        if (use_name) {
            model existing = use_name->count ? emodel(use_name->chars) : (model)null;
            if (existing) return existing;
        }
        // function pointers
        if (pointee->isFunctionType())
            return map_function_pointer(pointee, ctx, e, use_name);

        model base = map_clang_type_to_model(pointee, ctx, e, null);
        if (!base) base = emodel("ARef"); // todo: call this opaque (we dont want to classify this as Au-related)

        verify(base, "could not resolve pointer type");

        model ptr = model(mod, e, src, base, is_ref, true,
            is_const, pointee.isConstQualified());
        emember mem = register_model(e, ptr, use_name, false);
        mem->context = top(e);
        return ptr;
    }
    
    // class types (already handle RecordType) — ensure CXXRecordDecl path hits create_class
    if (auto* RT = dyn_cast<RecordType>(type)) {
        RecordDecl* decl = RT->getDecl();
        std::string name = decl->getNameAsString();
        model existing = name.length() ? emodel(name.c_str()) : null;
        if (existing) return existing;

        if (auto* CXX = dyn_cast<CXXRecordDecl>(decl)) {
            if (CXX->isCLike()) {
                return (model)create_record(decl, ctx, e, get_name((NamedDecl*)decl));
            } else {
                return (model)create_class((CXXRecordDecl*)decl, ctx, e, get_name((NamedDecl*)decl));
            }
        } else {
            return (model)create_record(decl, ctx, e, get_name((NamedDecl*)decl));
        }
    }

    // template specializations
    if (auto* T = dyn_cast<TemplateSpecializationType>(type)) {
        if (auto* RD = T->getAsCXXRecordDecl())
            return (model)create_class(const_cast<CXXRecordDecl*>(RD), ctx, e, get_name((NamedDecl*)RD));
    }
    
    // Enum types
    if (const EnumType* et = dyn_cast<EnumType>(type)) {
        EnumDecl* decl = et->getDecl();
        std::string name = decl->getNameAsString();
        model existing = name.length() ? emodel(name.c_str()) : null;
        return existing ? existing : (model)create_enum(decl, ctx, e, get_name((NamedDecl*)decl));
    }


    // references → treat as pointers + metadata
    if (auto* L = dyn_cast<LValueReferenceType>(type)) {
        model base = map_clang_type_to_model(L->getPointeeType(), ctx, e, null); // 2nd arg on seq 62 of function pointer type results in null
        return model(mod, e, src, base, is_ref, true);
    }
    if (auto* R = dyn_cast<RValueReferenceType>(type)) {
        model base = map_clang_type_to_model(R->getPointeeType(), ctx, e, null);
        return model(mod, e, src, base, is_ref, true);
    }

    // member pointers — skip or represent as opaque for now
    if (isa<MemberPointerType>(type)) {
        return emodel("opaque"); // or make a dedicated opaque model
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

    // how do 
    //if (!type->isCompleteDefinition() || type->isInvalidDecl() || type->isDependentType())
    //    return null;

    return emodel("ARef"); // lets treat opaque types as ARef

    // Unhandled type
    std::string type_name = qt.getAsString();
    fault("unhandled Type kind: %s", type_name.c_str());
    return nullptr;
}

path aether_lookup_include(aether e, string include) {
    array ipaths = a((Au)e->sys_inc_paths, (Au)e->sys_exc_paths, (Au)e->include_paths);
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

// custom AST consumer
class AetherASTConsumer : public clang::ASTConsumer {
    aether e;
public:
    AetherASTConsumer(aether e) : e(e) {}

    void HandleTranslationUnit(ASTContext& context) override {
        AetherDeclVisitor visitor(context, e);
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }
};

// subclass of LLVMOnlyAction, adding emit as well
class AetherEmitAction : public clang::EmitLLVMOnlyAction {
    aether e;

public:
    AetherEmitAction(aether e) : e(e) {}

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override {
        auto backend = EmitLLVMOnlyAction::CreateASTConsumer(CI, InFile);

        // Wrap it with our custom Aether consumer
        class CombinedConsumer : public clang::ASTConsumer {
            std::unique_ptr<clang::ASTConsumer> backend;
            AetherASTConsumer aetherConsumer;
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
      Printer->BeginSourceFile(LO, /*PP=*/nullptr);
      Begun = true;
    }
    // forward to the real renderer
    Printer->HandleDiagnostic(L, Info);
  }
};

typedef aether silver;
void print_tokens(silver mod, symbol label);

class MacroCollector : public clang::PPCallbacks {
public:
    clang_cc instance;
    clang::Preprocessor* PP;

    explicit MacroCollector(clang_cc instance)
        : instance(instance), PP((clang::Preprocessor*)instance->PP) {}

    void MacroDefined(const clang::Token &macroNameTok,
                      const clang::MacroDirective *md) override {
        const clang::MacroInfo *mi = md->getMacroInfo();
    
        aether mod = instance->mod;
        std::string name = macroNameTok.getIdentifierInfo()->getName().str();
        string n = string(name.c_str());
        emember existing = lookup2(mod, (Au)n, (Au_t)null);
        
        if (existing)
            return;

        array params = null;
        bool is_func = mi->isFunctionLike();
        if (is_func) {
            params = array(alloc, 32);
            for (const IdentifierInfo *II: mi->params())
                push(params, (Au)string(II->getName().str().c_str()));
        }

        std::string def;
        for (auto it = mi->tokens_begin(); it != mi->tokens_end(); ++it) {
            def += PP->getSpelling(*it);
            if (std::next(it) != mi->tokens_end())
                def += " ";
        }
        
        bool cmode = mod->cmode;
        mod->cmode = true;
        array t = (array)tokens(
            target, (Au)mod, parser, mod->parse_f,
            input, (Au)string(def.c_str()));
        mod->cmode = cmode;

        if (!len((collective)t))
            return;

        if (params) {
            bool va_args = mi->isC99Varargs() || mi->isGNUVarargs();
            macro mac = macro(mod, mod, params, params, def, t, va_args, va_args);
            register_model(mod, (model)mac, n, false);
            //
        } else {
            //
            push_state(mod, t, 0);
            mod->in_macro = true;
            // print_tokens(mod, ((string)f(string, "macro: %o", n))->chars);
            
            bool cmode = mod->cmode;
            mod->cmode = true;
            model mdl = (model)mod->read_model((Au)mod, (Au)null, (Au)null);
            mod->cmode = cmode;

            if (mdl && (Au_t)isa(mdl) != (Au_t)typeid(macro)) {
                mod->in_macro = false;
                model macro_typedef = model(mod, mod, src, mdl);
                register_model(mod, macro_typedef, n, false);
            } else {
                macro mac = macro(mod, mod, params, null, def, t);
                register_model(mod, (model)mac, n, false);

                // we were attempting to parse the expression here, but the const api's
                // in llvm are not implemented as they should be.
                // for instance.  it complains about bit-shift operations, etc
                // its far better to act like a real 'macro' here and perform cmode in code
                // this is to be done without params, so the syntax will not require the ()

#if 0
                bool cmode = mod->cmode;
                mod->cmode = true;
                enode value = (enode)mod->parse_expr((Au)mod, (Au)null);
                mod->cmode = cmode;
                mod->in_macro = false;
                if (value) {
                    emember m = emember(
                        mod,        mod,
                        name,       (token)n,
                        mdl,        value->mdl,
                        value,      value->value);
                    register_member(mod, m, false);
                }
#endif
            }
            pop_state(mod, false);
        }
    }
};

#undef release

static inline LLVMModuleRef wrap(llvm::Module *M) {
    return reinterpret_cast<LLVMModuleRef>(M);
}

static inline llvm::Module *unwrap(LLVMModuleRef M) {
    return reinterpret_cast<llvm::Module*>(M);
}

#undef print

// goal is to test iterative includes, enables by a watch in silver
// this so we need not traverse through the modules again
// question is how to cache the include

static map include_cache = null;

path aether_include(aether e, Au inc, ARef _instance) {
    clang_cc* instance = (clang_cc*)_instance;
    path ipath = (Au_t)isa(inc) == typeid(string) ?
        lookup_include(e, (string)inc) : (path)inc;
    verify(ipath && exists(ipath), "include path does not exist: %o", ipath ? (Au)ipath : inc);
    e->current_include = ipath;

    //model mdl_top = aether_top(e);
    //mdl_top->imported_from = (path)hold((Au)ipath);

    string incl = string(ipath->chars);
    bool is_header = ends_with(incl, ".h") ||
                     ends_with(incl, ".hpp");

    auto DiagID(new DiagnosticIDs());
    auto DiagOpts = new DiagnosticOptions();
    TextDiagnosticPrinter *DiagPrinter = new TextDiagnosticPrinter(llvm::errs(), *DiagOpts);

    auto Invocation = std::make_shared<CompilerInvocation>();

    path res = f(path, "%o/lib/clang/22", e->install);
    path c = f(path, "/tmp/%o.c", stem(ipath)); // may need to switch ext and the Language spec based on the ext
    string  contents = f(string, "#include \"%o\"\n", ipath);
    save(c, (Au)contents, null);

    symbol compile_unit = is_header ? c->chars : ipath->chars;

    // lets get the 'driver' arguments, otherwise known as 
    // the hidden things you have never heard of and absolutely need
    DiagnosticsEngine diags(DiagID, *DiagOpts, DiagPrinter);

    // === use the Driver API here ===
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

    // isystem + framework + includes
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
            //args.push_back("-Xclang");
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
            //args.push_back("-Xclang");
            args.push_back("-isystem");
            args.push_back(arg->chars);
        }

    args.push_back("-nostdinc++");
    //args.push_back("-stdlib=libc++");
    args.push_back("-c");
    args.push_back(compile_unit);

    std::unique_ptr<driver::Compilation> comp(
        drv.BuildCompilation(llvm::ArrayRef<symbol>(args)));

    std::vector<symbol> compilation_args;

    // check commands produced
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

    // build invocation from args
    llvm::ArrayRef<symbol> cmdline_args(compilation_args);

    Diags->setSuppressSystemWarnings(true);
    
    CompilerInvocation::CreateFromArgs(
        *Invocation,
        cmdline_args,
        *Diags
    );

    // create compiler with the invocation
    CompilerInstance* compiler = new CompilerInstance(Invocation);

    auto& LO = Invocation->getLangOpts();

    compiler->setDiagnostics(Diags.get());

    // create other managers
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
    
    *instance = clang_cc(
        mod, e, compiler, (handle)compiler, PP, (handle)&compiler->getPreprocessor()); // how do we keep this process alive and pass the compiler / pre processor in here?

    compiler->getPreprocessor().addPPCallbacks(
        std::make_unique<MacroCollector>(*instance));
    
    compiler->createASTContext();

    //llvm::errs() << "LangOpts.CPlusPlus: " << Invocation->getLangOpts().CPlusPlus << "\n";
    //llvm::errs() << "LangOpts.C11: " << Invocation->getLangOpts().C11 << "\n";

    ASTContext& ctx = compiler->getASTContext();
    if (is_header) {
        // header → just parse symbols
        AetherASTConsumer consumer(e);
        ParseAST(compiler->getPreprocessor(), &consumer, ctx);
    } else {
        // source → parse + codegen
        AetherEmitAction act(e);
        //compiler->setASTConsumer(std::make_unique<AetherASTConsumer>(e));
        //std::unique_ptr<EmitLLVMOnlyAction> act(new EmitLLVMOnlyAction());
        compiler->ExecuteAction(act);
        std::unique_ptr<llvm::Module> M = act.takeModule();
        LLVMModuleRef cMod = M ? wrap(M.release()) : nullptr;
        (*instance)->module = cMod;
        LLVMLinkModules2(e->module, cMod);
    }

    unlink(c->chars);

    e->current_include = null;
    return ipath;
}

}