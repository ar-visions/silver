#undef IMPORT

#ifdef _WIN32
// clang's headers use off_t, which the UCRT does not define by default
#include <posix.h>
#endif

#include <iostream>
#include <set>

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
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/SourceManager.h>
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

#include <posix.h>
#include <string>

typedef LLVMMetadataRef LLVMScope;

extern "C" {

#pragma pack(push, 1)
#include <aether/import>
#pragma pack(pop)

i64 array_count(array);
Au  array_get(array, i64);
bool path_exists(path);
bool path_save(path, Au, ctx);

#undef each
#define each(container, E, e) \
    if (container && array_count((container))) for (E e = (E)array_get(((array)container), 0), e0 = 0; e0 == 0; e0++) \
        for (num __i = 0, __len = array_count((container)); __i < __len; __i++, e = (E)array_get(((array)container), __i)) \


#undef init
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

extern "C" string Au_cast_string(Au a);
extern "C" Au_t lexical_traits(array lex, symbol f, u64 traits, int member_type);
extern "C" none au_member_map_insert(Au_t type, Au_t new_member);

// ============================================================================
// Helper macros for the new API
// ============================================================================

// Lookup a type by name in lexical scope
#define au_lookup(N) lexical(e->lexical, N)

// kind-filtered: a C name can be both a struct and a function (stat)
#define au_lookup_type(N) lexical_traits(e->lexical, N, 0, AU_MEMBER_TYPE)
#define au_lookup_func(N) lexical_traits(e->lexical, N, 0, AU_MEMBER_FUNC)

// a model this module's own header imports minted; one lives in an import
// namespace, one registered by an imported silver module does not
static bool import_model(Au_t m) {
    return m && m->context && m->context->member_type == AU_MEMBER_NAMESPACE;
}

#define prior_type(N) ({ Au_t _p = au_lookup_type(N); import_model(_p) ? _p : null; })
#define prior_func(N) ({ Au_t _p = au_lookup_func(N); import_model(_p) ? _p : null; })

// ============================================================================
// Forward declarations
// ============================================================================

// where a C declaration actually lives. clang knows it exactly; nothing
// captured it before, so every imported type reported no origin at all.
// first writer wins, matching how silver stamps its own declarations
static void stamp_decl(Au_t m, const clang::Decl* d, ASTContext& ctx) {
    if (!m || !d || m->source) return;
    clang::PresumedLoc pl = ctx.getSourceManager().getPresumedLoc(d->getLocation());
    if (pl.isInvalid() || !pl.getFilename()) return;
    m->source   = cstr_copy((cstr)pl.getFilename());
    m->src_line = (i32)pl.getLine();
}

static Au_t map_clang_type(const QualType& qt, ASTContext& ctx, aether e, symbol use_name);
extern "C" Au_t alloc_arg(Au_t, symbol, Au_t);

// models register at the import, never a transient record/arg scope
static Au_t model_scope(aether e) {
    return e->model_scope ? e->model_scope : aether_top_scope(e);
}
static Au_t create_record(RecordDecl* decl, ASTContext& ctx, aether e, std::string name);
static Au_t create_class(CXXRecordDecl* cxx, ASTContext& ctx, aether e, std::string qname);
static Au_t create_enum(EnumDecl* decl, ASTContext& ctx, aether e, std::string name);
static Au_t create_fn(FunctionDecl* decl, ASTContext& ctx, aether e, std::string name);

// ============================================================================
// Utility: find import by name
// ============================================================================

static import find_import(aether e, const char *name) {
    if (!e->imports) return null;
    for (int i = 0; i < array_count(e->imports); i++) {
        import im = (import)array_get(e->imports, i);
        if (im->external_name && strcmp(im->external_name->chars, name) == 0)
            return im;
    }
    return null;
}

// ============================================================================
// Pragma handler for #pragma silver_module ModuleName
// ============================================================================

class SilverModulePragmaHandler : public PragmaHandler {
public:
    aether e;
    SilverModulePragmaHandler(aether e) : PragmaHandler("silver_module"), e(e) {}

    void HandlePragma(Preprocessor &PP,
                      PragmaIntroducer Introducer,
                      Token &FirstToken) override {
        std::string name;
        Token Tok;
        PP.Lex(Tok);
        while (!Tok.is(tok::eod)) {
            name += PP.getSpelling(Tok);
            PP.Lex(Tok);
        }
        if (name.length() > 0) {
            import found = find_import(e, name.c_str());
            verify(found, "silver_module: import not found: %s", name.c_str());
            if (!found) exit(0);
            {
                aether_push_scope(e, (Au)found, 1);
                // inject defines from the import's define_map
                if (found->define_map) {
                    for (item it = found->define_map->first; it; it = it->next) {
                        string skey = Au_cast_string(it->key);
                        auto *II = PP.getIdentifierInfo(skey->chars);
                        auto *MI = PP.AllocateMacroInfo(SourceLocation());
                        if (isa(it->value) != typeid(bool)) {
                            // define with value: tokenize the value string
                            string sval = Au_cast_string(it->value);
                            Token ValTok;
                            ValTok.startToken();
                            ValTok.setKind(tok::numeric_constant);
                            ValTok.setLiteralData(sval->chars);
                            ValTok.setLength(sval->count);
                            MI->setTokens({ValTok}, PP.getPreprocessorAllocator());
                        }
                        PP.appendDefMacroDirective(II, MI);
                    }
                }
            }
        }
    }
};

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
            if (const auto *ND = llvm::dyn_cast<clang::NamespaceDecl>(D))
                parts.push_back(const_cast<clang::NamedDecl*>(
                    (const clang::NamedDecl*)ND));
        }
    }

    std::reverse(parts.begin(), parts.end());
    return parts;
}

static Au_t _find_member(Au_t parent, symbol name) {
    if (!parent || !name) return null;
    Au_t m = find_member(parent, name, 0, 0, false);
    if (m) return m;
    // member_map miss is not authority; walk members linearly
    for (int i = 0; i < parent->members.count; i++) {
        Au_t mm = (Au_t)parent->members.origin[i];
        if (mm->ident && strcmp(mm->ident, name) == 0) return mm;
    }
    return null;
}


static void push_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    Au_t cur = model_scope(e);
    for (clang::NamedDecl* n: s) {
        std::basic_string<char> s = n->getNameAsString();
        symbol name = s.c_str();
        Au_t m = _find_member(cur, name);
        if (!m) {
            printf("push_context miss: ns=%s decl=%s scope=%s members=%d\n",
                name, decl->getNameAsString().c_str(),
                cur->ident ? cur->ident : "?", cur->members.count);
            for (int mi = 0; mi < cur->members.count && mi < 12; mi++) {
                Au_t mm = (Au_t)cur->members.origin[mi];
                printf("  member[%d] = %s\n", mi, mm->ident ? mm->ident : "?");
            }
            fflush(stdout);
        }
        verify(m, "namespace not found: %s", name);
        aether_push_scope(e, (Au)m, 2);
        cur = m;
    }
}

static void pop_context(NamedDecl* decl, aether e) {
    auto s = namespace_stack((NamespaceDecl*)decl);
    for (size_t i = 0; i < s.size(); i++) {
        array_pop(e->lexical); // pop from lexical stack
    }
}

static std::string cxx_mangle(const NamedDecl* D, ASTContext& ctx) {
    std::string out;
    llvm::raw_string_ostream os(out);

    std::unique_ptr<MangleContext> MC(
        ItaniumMangleContext::create(ctx, ctx.getDiagnostics()));

    if (const auto *VD = dyn_cast<VarDecl>(D)) {
        MC->mangleName(VD, os);
    } else if (const auto *CD = dyn_cast<CXXConstructorDecl>(D)) {
        // C2: -femit-all-decls emits base-object variants; C1==C2 sans virtual bases
        MC->mangleName(GlobalDecl(CD, Ctor_Base), os);
    } else if (const auto *DD = dyn_cast<CXXDestructorDecl>(D)) {
        MC->mangleName(GlobalDecl(DD, Dtor_Base), os);
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


// long and wchar_t are the two builtins whose width follows the target rather
// than the language: long is 32-bit on windows (LLP64) and 64-bit on unix
// (LP64), wchar_t is 16-bit on windows and 32-bit elsewhere. hardcoding them
// put FT_Long fields at unix offsets and moved every later field of a struct
static Au_t map_builtin_type(const BuiltinType* bt, ASTContext& ctx, aether e) {
    const unsigned bits = (unsigned)ctx.getTypeSize(QualType(bt, 0));
    switch (bt->getKind()) {
        case BuiltinType::Void:        return au_lookup("none");
        case BuiltinType::Bool:        return au_lookup("bool");
        case BuiltinType::Char_U:      return au_lookup("u8");
        case BuiltinType::UChar:       return au_lookup("u8");
        case BuiltinType::Char_S:      return au_lookup("i8");
        case BuiltinType::SChar:       return au_lookup("i8");
        case BuiltinType::WChar_U:     return au_lookup(bits == 16 ? "u16" : "u32");
        case BuiltinType::WChar_S:     return au_lookup(bits == 16 ? "i16" : "i32");
        case BuiltinType::Char16:      return au_lookup("u16");
        case BuiltinType::Char32:      return au_lookup("u32");
        case BuiltinType::UShort:      return au_lookup("u16");
        case BuiltinType::Short:       return au_lookup("i16");
        case BuiltinType::UInt:        return au_lookup("u32");
        case BuiltinType::Int:         return au_lookup("i32");
        case BuiltinType::ULong:       return au_lookup(bits == 32 ? "u32" : "u64");
        case BuiltinType::Long:        return au_lookup(bits == 32 ? "i32" : "i64");
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
    Au_t parent = model_scope(e);
    
    // Create function type
    Au_t fn = def(parent, null, AU_MEMBER_TYPE, AU_TRAIT_FUNCPTR | AU_TRAIT_IS_C);
    //fn->module = e->current_import->autype;
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
            Au_t arg = def(null, name_buf, AU_MEMBER_VAR, AU_TRAIT_IS_C);
            //arg->module = e->current_import->autype;
            arg->src = param;
            micro_push(&fn->args, (Au)arg);
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
        if (e->verbose) {
            /*
            printf("map_function_pointer: %s -> funcptr ident=%s member_type=%d is_funcptr=%d\n",
                use_name ? use_name : "(null)",
                func->ident ? func->ident : "(null)",
                func->member_type, func->is_funcptr);
            fflush(stdout);
            */
        }
        return func;
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(pointee)) {
        Au_t parent = model_scope(e);
        Au_t fn = def(parent, null, AU_MEMBER_FUNC, AU_TRAIT_FUNCPTR | AU_TRAIT_IS_C);
        //fn->module = e->current_import->autype;
        fn->rtype = map_clang_type(fnpt->getReturnType(), ctx, e, null);
        if (!fn->rtype) fn->rtype = au_lookup("none");
        Au_t ptr = def_pointer(null, fn, use_name);
        //ptr->module = e->current_import->autype;
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

    if (const SubstTemplateTypeParmType* st = dyn_cast<SubstTemplateTypeParmType>(t)) {
        return map_clang_type(st->getReplacementType(), ctx, e, use_name);
    }
    
    // Handle typedefs
    if (const TypedefType* tt = dyn_cast<TypedefType>(t)) {
        std::string name = tt->getDecl()->getName().str();
        Au_t existing = name.length() ? au_lookup_type(name.c_str()) : null;
        if (name == "jmp_buf") {
            name = name;
        }
        if (existing) return existing;
        
        symbol new_name = use_name ? use_name : (name.length() ? name.c_str() : null);
        Au_t underlying = map_clang_type(tt->getDecl()->getUnderlyingType(), ctx, e, null);
        
        if (underlying && new_name) {
            // Create alias
            Au_t alias = def_type(model_scope(e), new_name, AU_TRAIT_ALIAS);
            //alias->module = e->current_import->autype;
            alias->src = underlying;
            return alias;
        }
        return underlying;
    }

    QualType unqualified = qt.getCanonicalType().getUnqualifiedType();
    const Type* type = unqualified.getTypePtr();

    // Builtin types
    if (const BuiltinType* bt = dyn_cast<BuiltinType>(type)) {
        Au_t src = map_builtin_type(bt, ctx, e);
        if (src && use_name) {
            Au_t alias = def_type(model_scope(e), use_name, AU_TRAIT_ALIAS | AU_TRAIT_IS_C);
            //alias->module = e->current_import->autype;
            alias->src = src;
            return alias;
        }
        return src;
    }
    
    // Complex types: unsupported, member gets skipped
    if (const ComplexType* ct = dyn_cast<ComplexType>(type))
        return null;
    
    // Constant array types
    if (const ConstantArrayType* cat = dyn_cast<ConstantArrayType>(type)) {
        QualType elem_type = cat->getElementType();
        int64_t esize = cat->getSize().getSExtValue();
        Au_t elem = map_clang_type(elem_type, ctx, e, null);
        
        if (!elem) return null;
        
        // Create array type - need to represent shape somehow
        // For now, create a type with size info
        Au_t arr = def_type(model_scope(e), use_name, AU_TRAIT_IS_C);
        //arr->module = e->current_import->autype;
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
        
        Au_t arr = def_type(model_scope(e), use_name, AU_TRAIT_IS_C);
        //arr->module = e->current_import->autype;
        arr->src = elem;
        arr->elements = 0; // flexible array
        return arr;
    }
    
    // Pointer types
    if (const PointerType* pt = dyn_cast<PointerType>(type)) {
        QualType pointee = pt->getPointeeType();
        
        if (use_name) {
            Au_t existing = au_lookup_type(use_name);
            if (existing) return existing;
        }
        
        // Function pointers
        if (pointee->isFunctionType())
            return map_function_pointer(pointee, ctx, e, use_name);

        Au_t base = map_clang_type(pointee, ctx, e, null);
        //base->module = e->current_import->autype;
        if (!base) base = au_lookup("ARef"); // opaque pointer
        
        verify(base, "could not resolve pointer type");
        
        Au_t ptr = def_pointer(null, base, use_name);
        //ptr->module = e->current_import->autype;
        //if (pointee.isConstQualified()) {
        //    ptr->traits |= AU_TRAIT_CONST;
        //}
        return ptr;
    }
    
    // Record types (struct/class)
    if (auto* RT = dyn_cast<RecordType>(type)) {
        RecordDecl* decl = RT->getDecl();
        bool is_spec = isa<ClassTemplateSpecializationDecl>(decl);
        std::string name = decl->getNameAsString();
        Au_t existing = (!is_spec && name.length()) ? au_lookup_type(name.c_str()) : null;
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
        Au_t existing = name.length() ? au_lookup_type(name.c_str()) : null;
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
        // Itanium: dynamic classes carry the vtable pointer as word 0
        if (auto* CXX = dyn_cast<CXXRecordDecl>(decl))
            if (CXX->isDynamicClass()) {
                Au_t vp = def_member(rec, "__vptr", au_lookup("ARef"),
                    AU_MEMBER_VAR, AU_TRAIT_IS_C | AU_TRAIT_IPROP);
                vp->offset = 0;
                vp->member_index = field_index++;
            }
        for (auto field : decl->fields()) {
            std::string field_name = field->getNameAsString();
            if (field_name.empty()) {
                field_name = "__anon_" + std::to_string(field_index);
            }

            QualType field_type = field->getType();
            Au_t mapped = map_clang_type(field_type, ctx, e, null);
            if (!mapped) {
                fflush(stdout);
                continue;
            }
            if (!mapped->member_type && !mapped->traits) {
                printf("aclang: field %s on %s has empty Au_t (ident=%s)\n",
                    field_name.c_str(), rec->ident ? rec->ident : "?",
                    mapped->ident ? mapped->ident : "(null)");
                fflush(stdout);
            }
            
            Au_t m = def_member(rec, field_name.c_str(), mapped, AU_MEMBER_VAR, AU_TRAIT_IS_C | AU_TRAIT_IPROP);
            stamp_decl(m, field, ctx);
            uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());
            //m->module = e->current_import->autype;
            m->offset = offset_bits / 8;
            if (mapped->elements > 0)
                m->elements = mapped->elements;
            m->member_index = field_index++;
        }
    }
}

static Au_t create_record(RecordDecl* decl, ASTContext& ctx, aether e, std::string name) {
    bool has_name = name.length() > 0;
    symbol n = has_name ? name.c_str() : null;
    
    // Check if already exists as a complete type (not empty stub, not function/macro)
    Au_t existing = has_name ? prior_type(n) : null;
    if (existing && existing->member_type == AU_MEMBER_TYPE && existing->members.count > 0) return existing;

    bool is_union = decl->isUnion();
    Au_t parent = model_scope(e);

    // Incomplete definition → opaque
    if (!decl->isCompleteDefinition() || decl->isInvalidDecl() || decl->isDependentType()) {
        Au_t opaque = def_type(parent, n, AU_TRAIT_STRUCT | AU_TRAIT_IS_C);
        stamp_decl(opaque, decl, ctx);
        //opaque->module = e->current_import->autype;
        opaque->src = au_lookup("ARef");
        return opaque;
    }

    // Create struct/union (reuse existing empty stub if present)
    u32 traits = is_union ? AU_TRAIT_UNION : AU_TRAIT_STRUCT;
    Au_t rec = (existing && existing->member_type == AU_MEMBER_TYPE && existing->members.count == 0 && existing->is_c) ?
        existing : def_type(parent, n, traits | AU_TRAIT_IS_C);
    if (rec->is_resolving) return rec;
    rec->is_resolving = true;
    stamp_decl(rec, decl, ctx);
    rec->traits |= traits | AU_TRAIT_IS_C;
    rec->is_struct = true;
    rec->src = null; // clear opaque stub's ARef src
    //rec->module = e->current_import->autype;

    const ASTRecordLayout& layout = ctx.getASTRecordLayout(decl);
    rec->typesize = layout.getSize().getQuantity(); // size in bytes
    //rec->record_alignment = layout.getAlignment().getQuantity(); // in bytes

    aether_push_scope(e, (Au)rec, 3);
    set_fields(decl, ctx, e, rec);
    array_pop(e->lexical);
    rec->is_resolving = false;
    return rec;
}

static Au_t create_opaque_class(CXXRecordDecl* cxx, aether e) {
    std::string qname = cxx->getQualifiedNameAsString();
    symbol n = qname.c_str();

    Au_t existing = au_lookup_type(n);
    if (existing) return existing;

    Au_t rec = def_class(model_scope(e), n);
    //rec->module = e->current_import->autype;
    return rec;
}

// binary member operators e_op can dispatch by operator_type
static const struct { OverloadedOperatorKind k; OPType op; symbol name; } cxx_op_map[] = {
    { OO_Plus,    OPType__add, "operator__add" },
    { OO_Minus,   OPType__sub, "operator__sub" },
    { OO_Star,    OPType__mul, "operator__mul" },
    { OO_Slash,   OPType__div, "operator__div" },
    { OO_Percent, OPType__mod, "operator__mod" },
};

// methods bind like silver struct methods: alt holds the mangled symbol
static void set_methods(CXXRecordDecl* cxx, ASTContext& ctx, aether e, Au_t rec) {
    // param mapping re-enters create_class for self-typed operands
    static std::set<Au_t> done;
    if (!done.insert(rec).second) return;
    for (auto* md : cxx->methods()) {
        if (md->getAccess() != AS_public) continue;
        if (md->isStatic()) continue;
        std::string mname;
        OPType      op    = OPType__undefined;
        u32         mtype = AU_MEMBER_FUNC;
        if (auto* cd = dyn_cast<CXXConstructorDecl>(md)) {
            // a method, not silver construct: cd initializes the alloc'd object
            if (cd->isImplicit() || cd->isCopyConstructor() || cd->isMoveConstructor())
                continue;
            mname = cxx->getNameAsString();
        } else if (auto* dd = dyn_cast<CXXDestructorDecl>(md)) {
            if (dd->isImplicit() || dd->isTrivial()) continue;
            mname = "dtor";
        } else if (auto* cv = dyn_cast<CXXConversionDecl>(md)) {
            // conversion operator = silver cast member; castable() finds it
            Au_t crt = map_clang_type(cv->getConversionType(), ctx, e, null);
            if (!crt || !crt->ident) continue;
            mname = std::string("cast_") + crt->ident;
            mtype = AU_MEMBER_CAST;
        } else if (md->getIdentifier())
            mname = md->getNameAsString();
        else {
            OverloadedOperatorKind oo = md->getOverloadedOperator();
            for (size_t i = 0; i < sizeof(cxx_op_map) / sizeof(cxx_op_map[0]); i++)
                if (cxx_op_map[i].k == oo) {
                    op    = cxx_op_map[i].op;
                    mname = cxx_op_map[i].name;
                    break;
                }
            if (mname.empty() || md->getNumParams() != 1) continue;
        }
        std::string mg_dup = cxx_mangle(md, ctx);
        bool seen = false;
        for (int mi2 = 0; mi2 < rec->members.count; mi2++) {
            Au_t mm = (Au_t)rec->members.origin[mi2];
            if (mm->alt && mg_dup == mm->alt) { seen = true; break; }
        }
        if (seen) continue;

        // by-value 8-byte PODs pass as their packed eightbyte scalar
        std::vector<std::pair<std::string, Au_t>> params;
        bool ok = true;
        for (unsigned i = 0; i < md->getNumParams(); ++i) {
            ParmVarDecl* p = md->getParamDecl(i);
            QualType pt = p->getType();
            Au_t mt = map_clang_type(pt, ctx, e, null);
            if (!mt) { ok = false; break; }
            if (mt->is_struct && !mt->is_pointer) {
                if ((unsigned)ctx.getTypeSizeInChars(pt).getQuantity() != 8) {
                    ok = false;
                    break;
                }
                bool all_real = false;
                if (const RecordType* rt = pt->getAs<RecordType>()) {
                    all_real = true;
                    for (auto f2 : rt->getDecl()->fields())
                        if (!f2->getType()->isFloatingType()) all_real = false;
                }
                mt = au_lookup(all_real ? "f64" : "u64");
            }
            std::string pname = p->getNameAsString();
            if (pname.empty())
                pname = "arg_" + std::to_string(i);
            params.push_back({ pname, mt });
        }
        if (!ok) continue;
        std::string mg = mg_dup;

        Au_t fn  = def(rec, mname.c_str(), mtype,
                       AU_TRAIT_IMETHOD | AU_TRAIT_IS_C);
        fn->operator_type = op;
        fn->alt   = (cstr)cstr_copy((cstr)mg.c_str());
        fn->rtype = map_clang_type(md->getReturnType(), ctx, e, null);
        if (!fn->rtype) fn->rtype = au_lookup("none");
        if (md->isVirtual()) {
            fn->is_cpp_virtual = true;
            GlobalDecl gd = isa<CXXDestructorDecl>(md)
                ? GlobalDecl(cast<CXXDestructorDecl>(md), Dtor_Complete)
                : GlobalDecl(md);
            fn->member_index   = (i64)cast<ItaniumVTableContext>(
                ctx.getVTableContext())->getMethodVTableIndex(gd);
        }

        Au_t self_arg = alloc_arg(fn, "a", rec);
        self_arg->is_target = true;
        micro_push(&fn->args, (Au)self_arg);

        for (auto& pr : params) {
            Au_t ap = alloc_arg(fn, pr.first.c_str(), pr.second);
            micro_push(&fn->args, (Au)ap);
        }
    }
}

static std::string spec_name(ClassTemplateSpecializationDecl* spec,
                             ASTContext& ctx, aether e);

// names only, never create_class: std's template webs are cyclic
static std::string type_arg_name(QualType qt, ASTContext& ctx, aether e) {
    const Type* t = qt.getTypePtr();
    if (auto* el = dyn_cast<ElaboratedType>(t))
        return type_arg_name(el->getNamedType(), ctx, e);
    if (auto* st = dyn_cast<SubstTemplateTypeParmType>(t))
        return type_arg_name(st->getReplacementType(), ctx, e);
    if (auto* tt = dyn_cast<TypedefType>(t))
        return type_arg_name(tt->getDecl()->getUnderlyingType(), ctx, e);
    if (auto* bt = dyn_cast<BuiltinType>(t)) {
        Au_t m = map_builtin_type(bt, ctx, e);
        return (m && m->ident) ? m->ident : "?";
    }
    if (auto* rt = dyn_cast<RecordType>(t)) {
        RecordDecl* rd = rt->getDecl();
        if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(rd))
            return spec_name(spec, ctx, e);
        return get_name((NamedDecl*)rd);
    }
    return qt.getCanonicalType().getAsString();
}

// specializations register under Au arg idents: minmax<i32>
static std::string spec_name(ClassTemplateSpecializationDecl* spec,
                             ASTContext& ctx, aether e) {
    static thread_local int depth;
    if (depth > 24) return "?";
    depth++;
    std::string out = get_name((NamedDecl*)spec);
    out += "<";
    const TemplateArgumentList& targs = spec->getTemplateArgs();
    for (unsigned i = 0; i < targs.size(); i++) {
        if (i) out += ",";
        const TemplateArgument& ta = targs[i];
        if (ta.getKind() == TemplateArgument::Type) {
            out += type_arg_name(ta.getAsType(), ctx, e);
        } else if (ta.getKind() == TemplateArgument::Integral) {
            out += std::to_string(ta.getAsIntegral().getSExtValue());
        } else
            out += "?";
    }
    out += ">";
    depth--;
    return out;
}

static Au_t create_class(CXXRecordDecl* cxx, ASTContext& ctx, aether e, std::string qname) {
    if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(cxx))
        qname = spec_name(spec, ctx, e);

    if (!cxx->isCompleteDefinition() || cxx->isDependentType() || cxx->isInvalidDecl())
        return create_opaque_class(cxx, e);

    Au_t rec = create_record((RecordDecl*)cxx, ctx, e, qname);
    aether_push_scope(e, (Au)rec, 4);
    set_methods(cxx, ctx, e, rec);
    array_pop(e->lexical);
    return rec;
}

static Au_t create_enum(EnumDecl* decl, ASTContext& ctx, aether e, std::string name) {
    symbol n = name.length() ? name.c_str() : null;

    // one C name, one model: a header reached from two imports maps once
    Au_t prior = n ? prior_type(n) : null;
    if (prior && prior->is_enum) return prior;

    Au_t parent = model_scope(e);
    Au_t en = def_enum(parent, n, 0);
    stamp_decl(en, decl, ctx);
    //en->module = e->current_import->autype;
    en->is_c = true;
    en->src = au_lookup("i32");
    
    aether_push_scope(e, (Au)en, 5);
    
    for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it) {
        EnumConstantDecl* ec = *it;
        std::string const_name = ec->getNameAsString();
        symbol cn = const_name.c_str();

        if (const_name == "VK_QUEUE_GRAPHICS_BIT") {
            n = n;
        }
        if (lexical_traits(e->lexical, cn, 0, AU_MEMBER_ENUMV)) continue;
        llvm::APSInt val = ec->getInitVal();
        i32* value = (i32*)_i32(val.getSExtValue());

        Au_t ev = def_enum_value(en, cn, (Au)value);
        ev->is_c = true;
        micro_push(&parent->members, (Au)ev);
        au_member_map_insert(parent, ev);
    }
    
    array_pop(e->lexical);
    return en;
}

static Au_t create_fn(FunctionDecl* decl, ASTContext& ctx, aether e, std::string name) {
    symbol n = name.c_str();

    // one C name, one model: a header reached from two imports maps once
    Au_t prior = prior_func(n);
    if (prior) return prior;

    Au_t parent = model_scope(e);
    Au_t fn = def(parent, n, AU_MEMBER_FUNC, AU_TRAIT_IS_C);
    stamp_decl(fn, decl, ctx);
    // a C header's `static inline` (windows spells time() that way) is not
    // extern "C", but it has no mangled symbol either — it has NO symbol at
    // all. only mangle what is actually externally visible
    if (!decl->isExternC() && decl->isExternallyVisible())
        fn->alt = (cstr)cstr_copy((cstr)cxx_mangle(decl, ctx).c_str());
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
        
        Au_t arg = def(fn, param_name.c_str(), AU_MEMBER_VAR, AU_TRAIT_IS_C);
        //arg->module = e->current_import->autype;
        arg->src = mt;
        micro_push((micro_*)&fn->args, (Au)arg);
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
            }
        }
    } else if (n && decl->isVariadic()) {
        //string st = path_stem(e->current_import);
        //if (string_eq(st, "stdio")) {
            // glibc removed __attribute__((format)) from these; patch it in
            static const struct { const char* name; int fmt_arg; } fmt_table[] = {
                {"printf",  0}, {"fprintf", 1}, {"sprintf", 1},
                {"scanf",   0}, {"fscanf",  1}, {"sscanf",  1},
                {NULL, 0}
            };
            for (int t = 0; fmt_table[t].name; t++) {
                if (strcmp(n, fmt_table[t].name) == 0 && fn->args.count > fmt_table[t].fmt_arg) {
                    Au_t f = (Au_t)fn->args.origin[fmt_table[t].fmt_arg];
                    if (f->src == typeid(ref_i8))
                        f->is_formatter = true;
                    break;
                }
            }
        //}
    }

    return fn;
}

static Au_t create_namespace(NamespaceDecl* ns, ASTContext& ctx, aether e) {
    // stack excludes ns itself; walk inclusive, find-or-create each level
    auto s = namespace_stack(ns);
    s.push_back(ns);

    Au_t cur = model_scope(e);
    for (clang::NamedDecl* ndecl: s) {
        std::string ns_name = ndecl->getNameAsString();
        symbol name = ns_name.c_str();
        Au_t existing = _find_member(cur, name);
        if (!existing)
            existing = def_struct(cur, name);
        cur = existing;
    }
    return cur;
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
    
    bool VisitTypedefDecl(TypedefDecl* decl) {
        auto name = decl->getNameAsString();

        // one C name, one model — and NEVER shadow a silver-native type
        // (std::string vs string); C models re-register per import as before
        if (name.length()) {
            Au_t ex = au_lookup_type(name.c_str());
            if (ex && (import_model(ex) || !ex->is_c)) return true;
        }

        // Map the underlying type (the array/struct) to our system
        Au_t underlying = map_clang_type(decl->getUnderlyingType(), ctx, e, null);
        
        if (underlying) {
            Au_t alias = def_type(model_scope(e), name.c_str(), AU_TRAIT_ALIAS | AU_TRAIT_IS_C);
            //alias->module = e->current_import->autype;
            alias->src = underlying;
            if (underlying->typesize)
                alias->typesize = underlying->typesize;
            else if (underlying->is_pointer)
                alias->typesize = sizeof(void*);
        }
        return true;
    }

    bool VisitEnumDecl(EnumDecl* decl) {
        push_context(decl, e);
        // anonymous enums (CF_ENUM style) still export their constants
        create_enum(decl, ctx, e, get_name((NamedDecl*)decl));
        pop_context(decl, e);
        return true;
    }

    bool VisitNamespaceDecl(NamespaceDecl* ns) {
        create_namespace(ns, ctx, e);
        return true;
    }

    // namespace Imf { using namespace Imf_3_4; } — publish via src link
    bool VisitUsingDirectiveDecl(UsingDirectiveDecl* ud) {
        auto* host = dyn_cast<NamespaceDecl>(ud->getDeclContext());
        NamespaceDecl* target = ud->getNominatedNamespace();
        if (!host || !target) return true;
        Au_t h = create_namespace(host, ctx, e);
        Au_t t = create_namespace(target, ctx, e);
        if (h && t && !h->src) h->src = t;
        return true;
    }
    
    bool VisitFunctionDecl(FunctionDecl* decl) {
        if (isa<CXXMethodDecl>(decl)) return true;
        if (!decl->getNameAsString().empty()) {
            create_fn(decl, ctx, e, get_name((NamedDecl*)decl));
        }
        return true;
    }
    
    bool VisitVarDecl(VarDecl* decl) {
        if (decl->hasExternalStorage() || decl->hasGlobalStorage()) {
            std::string var_name = decl->getNameAsString();
            if (var_name.empty()) return true;
            symbol n = var_name.c_str();
            Au_t existing = lexical_traits(e->lexical, n, 0, AU_MEMBER_VAR);
            if (existing) return true;
            QualType qt = decl->getType();
            Au_t mapped = map_clang_type(qt, ctx, e, null);
            if (!mapped) return true;
            Au_t parent = model_scope(e);
            Au_t m = def_member(parent, n, mapped, AU_MEMBER_VAR, AU_TRAIT_IS_C);
            m->is_static = true;
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
        if (decl->isDependentType()) return true;
        if (auto* spec = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
            if (spec->getSpecializationKind() != TSK_ExplicitSpecialization &&
                spec->getSpecializationKind() != TSK_ImplicitInstantiation &&
                spec->getSpecializationKind() != TSK_ExplicitInstantiationDefinition)
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
    aether       e;
    bool         incremental = false;
public:
    AetherASTConsumer2(aether e) : e(e) {}

    // decls must be read as parsed; the pragma scope stack only names the
    // current import while its headers are still being preprocessed
    bool HandleTopLevelDecl(DeclGroupRef dg) override {
        for (Decl* d: dg) {
            incremental = true;
            AetherDeclVisitor2 visitor(d->getASTContext(), e);
            visitor.TraverseDecl(d);
        }
        return true;
    }

    void HandleTranslationUnit(ASTContext& context) override {
        if (incremental) return;
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
        DiagnosticConsumer::HandleDiagnostic(L, Info);
        Printer->HandleDiagnostic(L, Info);
    }
};

typedef aether silver;

extern "C" {
none array_push(array, Au);
}

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

        // Reconstruct body text
        std::string body_text;
#undef tokens
        for (const auto &tok : mi->tokens()) {
             std::string spelling = PP->getSpelling(tok);
             // strip C integer suffixes (U, u, L, l, UL, ULL, etc.) from numeric tokens
             if (!spelling.empty() && (isdigit(spelling[0]) || (spelling[0] == '0' && spelling.size() > 1 && spelling[1] == 'x'))) {
                 while (!spelling.empty()) {
                     char c = spelling.back();
                     if (c == 'U' || c == 'u' || c == 'L' || c == 'l')
                         spelling.pop_back();
                     else
                         break;
                 }
             }
             if (body_text.length() > 0 && tok.hasLeadingSpace())
                body_text += " ";
             body_text += spelling;
        }

        // Note: 'string' is a silver type constructor from 'aether/import'
        string body_str = new0(string, chars, (cstr)body_text.c_str());
        tokens body_tokens = new0(tokens, target, (Au)e, parser, e->parse_f, input, (Au)body_str);
        token f = (token)array_first_element((array)body_tokens);

        // Handle Params
        array params_array = nullptr;
        bool va_args = mi->isVariadic();

        if (mi->isFunctionLike()) {
            params_array = new0(array, alloc, mi->getNumParams());
            for (auto param : mi->params()) {
                std::string p_name = param->getName().str();
                Au p_str = (Au)new0(string, chars, (cstr)p_name.c_str());
                array p_toks = (array)new0(tokens, target, (Au)e, parser, e->parse_f, input, p_str);
                if (p_toks && p_toks->count > 0) {
                    array_push(params_array, p_toks->origin[0]);
                }
            }
        }

        for (int i = 0; i < body_tokens->count; i++) {
            token t = (token)body_tokens->origin[i];
            t->cmode = true;
        }
        macro m = new0(macro,
            mod,        e, 
            autype,     def(aether_top_scope(e), n, AU_MEMBER_MACRO, AU_TRAIT_IS_C),
            def,        (array)body_tokens, 
            params,     params_array, 
            va_args,    va_args);
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
    // a device build resolves system headers in the DEVICE's sysroot, never
    // on this machine. a header the target does not have (unistd.h on
    // windows) then resolves to nothing and the import is skipped — the same
    // outcome a real windows host gives, where the file simply is not there
    if (e->target_sysroot) {
        cstr sr = e->target_sysroot->chars;
        bool win = e->target_triple && strstr(e->target_triple, "windows");
        // mingw keeps one flat include tree; debian splits per-arch headers
        std::string tri = e->target_triple ? e->target_triple : "";
        std::string posix_tri = std::string("/usr/include/") + tri;
        const char* subs_win[] = { "/include", null };
        const char* subs_pos[] = { "/usr/include", posix_tri.c_str(), null };
        const char** subs = win ? subs_win : subs_pos;
        for (int i = 0; subs[i]; i++) {
            path r = f(path, "%s%s/%o", sr, subs[i], include);
            if (path_exists(r)) return r;
        }
        // silver's own headers still come from the install tree
        if (e->include_paths)
            each(e->include_paths, path, i) {
                path r = f(path, "%o/%o", i, include);
                if (path_exists(r)) return r;
            }
        return null;
    }
    array ipaths = a(e->sys_inc_paths, e->sys_exc_paths, e->include_paths);
    if (file_exists("%o", include))
        return path(include);

    each(ipaths, array, includes) {
        if (includes)
            each(includes, path, i) {
                if (e->isysroot) {
                    path r = f(path, "%o/%o/%o", e->isysroot, i, include);
                    if (path_exists(r))
                        return r;
                }
                path r = f(path, "%o/%o", i, include);
                if (path_exists(r))
                    return r;
            }
    }

    // framework include: First/Header.h -> First.framework/Headers/Header.h
    if (e->framework_paths) {
        symbol ch    = include->chars;
        symbol slash = strchr(ch, '/');
        size_t n     = slash ? (size_t)(slash - ch) : 0;
        char   fw[256];
        if (n && n < sizeof(fw)) {
            memcpy(fw, ch, n);
            fw[n] = 0;
            each(e->framework_paths, path, fp) {
                path r = f(path, "%o/%s.framework/Headers/%s", fp, fw, slash + 1);
                if (path_exists(r))
                    return r;
            }
        }
    }

    // not found in silver's tracked paths. don't be fatal: a system header may
    // be #ifdef-guarded out on this platform (e.g. <pty.h> on macOS, which uses
    // <util.h>) yet still appear in the scanned source. the real clang compile
    // resolves/guards it correctly, so just skip tracking it here.
    return null;
}

void aether_import_models(aether a, Au_t, bool);

// singular clang session per module to perform all imports
extern "C" aether aether_clone(aether, int);

// a macro seen while preprocessing: text only, so the thread touches no model
struct pending_macro {
    std::string               name;
    std::string               body;
    std::vector<std::string>  params;
    bool                      function_like = false;
    bool                      va_args       = false;
};

// one entry per thing the unit saw, in the order it saw it
struct pending_item {
    Decl* decl  = null;
    int   macro = -1;
};

// one import's headers: parsed on its own thread, modelled in import order
struct import_unit {
    aether                     a;
    import                     im;
    path                       c;
    bool                       cpp = false;
    std::string                clang_path;
    std::vector<std::string>   args;
    CompilerInstance*          ci = null;
    std::vector<pending_macro> macros;
    std::vector<pending_item>  items;
};

// clang argv for one unit; built serially because it allocates Au strings
static void build_unit_args(aether a, import_unit* u) {
    path tool_root  = a->base_install ? a->base_install : a->install;
    path clang_path = f(path, "%o/bin/clang", tool_root);
    u->clang_path = clang_path->chars;

    std::vector<std::string>& args = u->args;
    args.push_back("clang");
    args.push_back("-x");
    args.push_back(u->cpp ? "c++" : "c");
    args.push_back(u->cpp ? "-std=c++17" : "-std=c11");

    // a device build must MODEL the device: parse its headers, with its
    // triple, or posix.h hides everything behind _WIN32 and every platform
    // ifdef in a system header takes the host's branch
    bool cross = a->target_sysroot && a->target_triple;
    bool win   = cross && strstr(a->target_triple, "windows") != null;
    if (cross) {
        args.push_back("-target");
        args.push_back(a->target_triple);
        cstr sr = a->target_sysroot->chars;
        args.push_back(std::string("--sysroot=") + sr);
        // debian keeps arch headers at /usr/include/<triple>; mingw has none
        if (!win) {
            args.push_back("-isystem");
            args.push_back(std::string(sr) + "/usr/include/" + a->target_triple);
        }
        // mingw carries libc++, and clang will not reach for it unnamed
        if (win && u->cpp) args.push_back("-stdlib=libc++");
    }
    args.push_back("-D_POSIX_C_SOURCE=200809L");
#ifdef __APPLE__
    args.push_back("-D_DARWIN_C_SOURCE");
#endif
    args.push_back("-fdiagnostics-show-option");
    args.push_back("-Wno-nullability-completeness");
    args.push_back("-w");
    args.push_back("-Wno-system-headers");

    // cpp: driver's builtin chain orders libstdc++/libc headers (include_next)
    if (!u->cpp && !cross && a->isystem) {
        args.push_back("-isystem");
        args.push_back(a->isystem->chars);
    }
    if (a->resource_dir) {
        args.push_back("-resource-dir");
        args.push_back(a->resource_dir->chars);
    }
    if (a->isysroot) {
        args.push_back("-isysroot");
        args.push_back(a->isysroot->chars);
    }

    struct {
        symbol ident;
        array  paths;
    } all_paths[] = {
        { "-isystem", a->sys_inc_paths },
        { "-isystem", a->sys_exc_paths }
    };

    for (int i = 0, l = (u->cpp || cross) ? 0 : 2; i < l; i++) {
        symbol ident = all_paths[i].ident;
        array  paths = all_paths[i].paths;
        for (int ii = 0; ii < (paths ? paths->count : 0); ii++) {
            path fp = (path)paths->origin[ii];
            args.push_back(ident);
            args.push_back(fp->chars);
        }
    }

    if (a->framework_paths)
        for (int i = 0; i < a->framework_paths->count; i++) {
            path fw_path = (path)a->framework_paths->origin[i];
            string fw_arg = f(string, "-F%o", fw_path);
            args.push_back(fw_arg->chars);
        }

    if (a->include_paths)
        for (int i = 0; i < a->include_paths->count; i++) {
            path inc_path = (path)a->include_paths->origin[i];
            args.push_back("-isystem");
            args.push_back(inc_path->chars);
        }

    if (!u->cpp)
        args.push_back("-nostdinc++");
    args.push_back("-c");
    args.push_back(u->c->chars);
}

// parse-time only: records decls, never defines them
class CollectConsumer : public clang::ASTConsumer {
    import_unit* unit;
public:
    CollectConsumer(import_unit* unit) : unit(unit) {}

    bool HandleTopLevelDecl(DeclGroupRef dg) override {
        for (Decl* d: dg) {
            pending_item it;
            it.decl = d;
            unit->items.push_back(it);
        }
        return true;
    }
};

// parse-time only: records macro text, never defines them
class MacroCollect : public clang::PPCallbacks {
    import_unit*         unit;
    clang::Preprocessor* PP;
public:
    MacroCollect(import_unit* unit, clang::Preprocessor* PP) : unit(unit), PP(PP) {}

    void MacroDefined(const clang::Token &macroNameTok,
                      const clang::MacroDirective *md) override {
        const clang::MacroInfo *mi = md->getMacroInfo();
        pending_macro pm;
        pm.name = macroNameTok.getIdentifierInfo()->getName().str();
#undef tokens
        for (const auto &tok : mi->tokens()) {
            std::string spelling = PP->getSpelling(tok);
            // strip C integer suffixes (U, u, L, l, UL, ULL, etc.)
            if (!spelling.empty() && (isdigit(spelling[0]) ||
                (spelling[0] == '0' && spelling.size() > 1 && spelling[1] == 'x'))) {
                while (!spelling.empty()) {
                    char c = spelling.back();
                    if (c == 'U' || c == 'u' || c == 'L' || c == 'l')
                        spelling.pop_back();
                    else
                        break;
                }
            }
            if (pm.body.length() > 0 && tok.hasLeadingSpace())
                pm.body += " ";
            pm.body += spelling;
        }
        pm.va_args       = mi->isVariadic();
        pm.function_like = mi->isFunctionLike();
        if (pm.function_like)
            for (auto param : mi->params())
                pm.params.push_back(param->getName().str());

        pending_item it;
        it.macro = (int)unit->macros.size();
        unit->macros.push_back(pm);
        unit->items.push_back(it);
    }
};

static void import_parse_unit(import_unit* u) {
    aether a = u->a;
    path   c = u->c;
    auto DiagID(new DiagnosticIDs());
    auto DiagOpts = new DiagnosticOptions();
    TextDiagnosticPrinter *DiagPrinter = new TextDiagnosticPrinter(llvm::errs(), *DiagOpts);
    auto Invocation = std::make_shared<CompilerInvocation>();
    DiagnosticsEngine diags(DiagID, *DiagOpts, DiagPrinter);
    driver::Driver drv(u->clang_path.c_str(), llvm::sys::getDefaultTargetTriple(), diags);

    // args were built serially; Au allocation is not safe from here
    std::vector<symbol> args;
    for (std::string& s : u->args)
        args.push_back(s.c_str());

    std::unique_ptr<driver::Compilation> comp(
        drv.BuildCompilation(llvm::ArrayRef<symbol>(args)));
    std::vector<symbol> compilation_args;
    for (clang::driver::Command &cmd : comp->getJobs()) {
        if (a->verbose) llvm::errs() << "command: ";
        if (StringRef(cmd.getCreator().getName()) == "clang") {
            for (symbol arg : cmd.getArguments()) {
                if (a->verbose) llvm::errs() << arg << " ";
                compilation_args.push_back(arg);
            }
            if (a->verbose) llvm::errs() << "\n";
        }
        if (a->verbose) llvm::errs() << "\n";
    }

    SimpleDiagConsumer* DiagClient = new SimpleDiagConsumer();
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags =
        new DiagnosticsEngine(DiagID, *DiagOpts, DiagClient);
    llvm::ArrayRef<symbol> cmdline_args(compilation_args);
    Diags->setSuppressSystemWarnings(true);

    bool invocation_ok = CompilerInvocation::CreateFromArgs(
        *Invocation,
        cmdline_args,
        *Diags
    );
    verify(invocation_ok && !Diags->hasErrorOccurred(), "failed to build clang import invocation for %o", c);

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

    compiler->getPreprocessor().addPPCallbacks(
        std::make_unique<MacroCollect>(u, &compiler->getPreprocessor()));
    compiler->createASTContext();
    ASTContext& ctx = compiler->getASTContext();
    compiler->getPreprocessor().getBuiltinInfo().initializeBuiltins(
        compiler->getPreprocessor().getIdentifierTable(),
        compiler->getPreprocessor().getLangOpts());

    CollectConsumer consumer(u);
    ParseAST(compiler->getPreprocessor(), &consumer, ctx);
    verify(!Diags->hasErrorOccurred(), "failed to build import model for %o", c);
    u->ci = compiler;
}

static void build_macro(aether e, pending_macro& pm) {
    symbol n = pm.name.c_str();
    if (au_lookup(n)) return;

    string body_str    = new0(string, chars, (cstr)pm.body.c_str());
    tokens body_tokens = new0(tokens, target, (Au)e, parser, e->parse_f, input, (Au)body_str);
    array  params_array = nullptr;

    if (pm.function_like) {
        params_array = new0(array, alloc, (int)pm.params.size() + 1);
        for (std::string& p : pm.params) {
            Au    p_str  = (Au)new0(string, chars, (cstr)p.c_str());
            array p_toks = (array)new0(tokens, target, (Au)e, parser, e->parse_f, input, p_str);
            if (p_toks && p_toks->count > 0)
                array_push(params_array, p_toks->origin[0]);
        }
    }

    for (int i = 0; i < body_tokens->count; i++)
        ((token)body_tokens->origin[i])->cmode = true;

    macro m = new0(macro,
        mod,        e,
        autype,     def(aether_top_scope(e), n, AU_MEMBER_MACRO, AU_TRAIT_IS_C),
        def,        (array)body_tokens,
        params,     params_array,
        va_args,    pm.va_args);
}

// model one parsed unit; runs in import order, so a name this unit needs from
// an earlier import is already defined and gets reused rather than remade
static void import_model_unit(aether e, import_unit* u) {
    aether_push_scope(e, (Au)u->im, 1);
    e->model_scope = aether_top_scope(e);
    for (pending_item& it : u->items) {
        if (it.decl) {
            AetherDeclVisitor2 visitor(it.decl->getASTContext(), e);
            visitor.TraverseDecl(it.decl);
        } else {
            build_macro(e, u->macros[it.macro]);
        }
    }
    e->model_scope = null;
}

// worker pool: each thread takes the next unparsed unit
struct unit_pool {
    std::vector<import_unit*> units;
    int                       next;
    pthread_mutex_t           lock;
};

static void* unit_worker(void* arg) {
    unit_pool* p = (unit_pool*)arg;
    for (;;) {
        pthread_mutex_lock(&p->lock);
        int i = p->next < (int)p->units.size() ? p->next++ : -1;
        pthread_mutex_unlock(&p->lock);
        if (i < 0) break;
        import_parse_unit(p->units[i]);
    }
    return nullptr;
}

static void collect_class_templates(Decl* d,
        std::vector<ClassTemplateDecl*>& out) {
    if (auto* ctd = dyn_cast<ClassTemplateDecl>(d))
        out.push_back(ctd);
    if (auto* ns = dyn_cast<NamespaceDecl>(d))
        for (Decl* c : ns->decls())
            collect_class_templates(c, out);
    if (auto* ls = dyn_cast<LinkageSpecDecl>(d))
        for (Decl* c : ls->decls())
            collect_class_templates(c, out);
}

// silver arg idents → C++ spellings that map_builtin_type round-trips,
// so the registered key equals the one read_named_model looks up
static symbol cpp_spelling(const std::string& s) {
    static const struct { symbol au; symbol cpp; } tbl[] = {
        {"i8","signed char"},   {"u8","unsigned char"},
        {"i16","short"},        {"u16","unsigned short"},
        {"i32","int"},          {"u32","unsigned int"},
        {"i64","long long"},    {"u64","unsigned long long"},
        {"i128","__int128"},    {"u128","unsigned __int128"},
        {"f16","_Float16"},     {"f32","float"},
        {"f64","double"},       {"bool","bool"},
    };
    for (auto& p : tbl) if (s == p.au) return p.cpp;
    return null;
}

// requested specializations (silver's token pre-scan): the module wrote
// Name<args> this TU may not instantiate. for each request whose base is a
// class template here, whose arity fits, and whose specialization is
// absent, append an explicit instantiation and reparse — the model walk
// and the -femit-all-decls codegen then carry it like a header-declared one
static void instantiate_requested(aether a, import_unit* u) {
    array reqs = a->template_requests;
    if (!reqs || !reqs->count || !u->ci) return;
    ASTContext& ctx = u->ci->getASTContext();
    std::vector<ClassTemplateDecl*> tpls;
    for (pending_item& it : u->items)
        if (it.decl) collect_class_templates(it.decl, tpls);
    if (tpls.empty()) return;
    std::string lines;
    for (int r = 0; r < reqs->count; r++) {
        std::string key = ((string)reqs->origin[r])->chars;
        size_t lt = key.find('<');
        if (lt == std::string::npos || key.back() != '>') continue;
        std::string base = key.substr(0, lt);
        ClassTemplateDecl* ctd = null;
        for (auto* t : tpls)
            if (get_name(t) == base) { ctd = t; break; }
        if (!ctd) continue;
        bool have = false;
        for (auto* spec : ctd->specializations())
            if (spec_name(spec, ctx, a) == key) { have = true; break; }
        if (have) continue;
        std::string cargs;
        bool     ok    = true;
        unsigned nargs = 0;
        for (size_t p = lt + 1; p < key.size() - 1 && ok;) {
            size_t c   = key.find(',', p);
            size_t end = (c == std::string::npos || c >= key.size() - 1)
                       ? key.size() - 1 : c;
            std::string arg = key.substr(p, end - p);
            symbol sp = cpp_spelling(arg);
            if (nargs) cargs += ",";
            if (sp)                                       cargs += sp;
            else if (arg.size() && isdigit((unsigned char)arg[0])) cargs += arg;
            else ok = false;
            nargs++;
            p = end + 1;
        }
        TemplateParameterList* tp = ctd->getTemplateParameters();
        if (!ok || nargs < tp->getMinRequiredArguments() || nargs > tp->size())
            continue;
        lines += "template ";
        lines += ctd->getTemplatedDecl()->getKindName().str();
        lines += " " + base + "<" + cargs + ">;\n";
    }
    if (lines.empty()) return;
    FILE* fp = fopen(u->c->chars, "a");
    if (!fp) return;
    fwrite(lines.c_str(), 1, lines.size(), fp);
    fclose(fp);
    // reparse: the second AST replaces the first for modeling; the prior
    // CompilerInstance stays held like every other unit's
    u->items.clear();
    u->macros.clear();
    u->ci = null;
    import_parse_unit(u);
}

none aether_import_includes(aether a) {
    // one translation unit per import, so its headers register under its own
    // namespace only; nested includes land in the import that pulled them
    unit_pool pool;
    pool.next = 0;
    pthread_mutex_init(&pool.lock, null);

    each(a->imports, import, im) {
        if (!im->include_paths || !im->include_paths->count)
            continue;
        if (im->autype->is_closed)
            continue;
        verify(im->external_name, "external_name (import identity) not set");
        string contents = new0(string, alloc, 1024);
        for (item i = im->define_map ? im->define_map->first : null; i; i = i->next) {
            if (isa(i->value) == typeid(bool))
                string_concat(contents, f(string, "#define %o\n", i->key));
            else
                string_concat(contents, f(string, "#define %o %o\n", i->key, i->value));
        }
        bool is_cpp = im->is_cpp;
        each (im->include_paths, path, ipath) {
            verify(ipath && path_exists(ipath), "include path does not exist: %o", ipath);
            string_concat(contents, f(string, "#include \"%o\"\n", ipath));
            string incl = new0(string, chars, ipath->chars);
            if (string_ends_with(incl, ".hpp") || string_ends_with(incl, ".hh") ||
                string_ends_with(incl, ".hxx"))
                is_cpp = true;
        }
        // modules build concurrently; the name must be unique process-wide
        static int      tu_seq;
        static pthread_mutex_t tu_lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&tu_lock);
        int tu_id = tu_seq++;
        pthread_mutex_unlock(&tu_lock);

        import_unit* u = new import_unit();
        u->a   = a;
        u->im  = im;
        u->cpp = is_cpp;
        u->c   = f(path, "%s/silver_%i_import_%i.c", temp_dir(), (int)getpid(), tu_id);
        path_save(u->c, (Au)contents, null);
        build_unit_args(a, u);
        pool.units.push_back(u);
    }
    if (!pool.units.size()) return;

    int max_threads = a->ncores ? a->ncores : 1;
    int n_threads   = (int)pool.units.size();
    if (n_threads > max_threads) n_threads = max_threads;
    std::vector<pthread_t> threads(n_threads);
    for (int i = 0; i < n_threads; i++)
        if (pthread_create(&threads[i], null, unit_worker, &pool) != 0)
            threads[i] = 0;
    for (int i = 0; i < n_threads; i++)
        if (threads[i]) pthread_join(threads[i], null);

    for (import_unit* u : pool.units) {
        if (u->cpp) instantiate_requested(a, u);
        import_model_unit(a, u);
        // C++ TUs codegen too: inline/template bodies emit weak, module links them
        if (u->cpp) {
            std::string cargs;
            for (size_t ai = 1; ai < u->args.size(); ai++) {
                cargs += " ";
                cargs += u->args[ai];
            }
            path obj = f(path, "%o.o", u->c);
            string cmd = f(string, "%o/bin/clang++ -fPIC -femit-all-decls%s -o %o",
                a->base_install ? a->base_install : a->install, cargs.c_str(), obj);
            if (a->verbose) printf("%s\n", cmd->chars);
            if (system(cmd->chars) == 0) {
                if (!a->import_objects)
                    a->import_objects = (array)hold((Au)new0(array, alloc, 8));
                array_push(a->import_objects, (Au)obj);
            }
        }
    }

    // import each
    each(a->imports, import, im) {
        aether_import_models(a, im->autype, false);
        im->autype->is_closed = true; // closed against new registrations
    }
}

path aether_include(aether e, Au inc, string ns) {
/*
    aclang_cc instance = null;
    path ipath = (Au_t)isa(inc) == typeid(string) ?
        aether_lookup_include(e, (string)inc) : (path)inc;

    verify(ipath && path_exists(ipath), "include path does not exist: %o",
        ipath ? (Au)ipath : inc);

    string incl = new0(string, chars, ipath->chars);
    bool is_header = string_ends_with(incl, ".h") ||
                     string_ends_with(incl, ".hpp");

    auto DiagID(new DiagnosticIDs());
    auto DiagOpts = new DiagnosticOptions();
    TextDiagnosticPrinter *DiagPrinter = new TextDiagnosticPrinter(llvm::errs(), *DiagOpts);

    auto Invocation = std::make_shared<CompilerInvocation>();

    path tool_root = e->base_install ? e->base_install : e->install;
    path res = f(path, "%o/lib/clang/22", tool_root);
    path c = f(path, "/tmp/%o.c", path_stem(ipath));
    string contents = f(string, "#include \"%o\"\n", ipath);
    path_save(c, (Au)contents, null);

    symbol compile_unit = is_header ? c->chars : ipath->chars;

    DiagnosticsEngine diags(DiagID, *DiagOpts, DiagPrinter);

    path clang_path = f(path, "%o/bin/clang", tool_root);
    driver::Driver drv(clang_path->chars, llvm::sys::getDefaultTargetTriple(), diags);

    std::vector<symbol> args = {
        "clang",
        "-x",
        "c",
        "-std=c11",
        "-D_POSIX_C_SOURCE=200809L",
#ifdef __APPLE__
        "-D_DARWIN_C_SOURCE",
#endif
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

    if (e->define_map) {
        for (item it = e->define_map->first; it; it = it->next) {
            Au key = it->key;
            Au val = it->value;
            char buf[256];
            string skey = Au_cast_string(key);
            if (isa(val) == typeid(bool)) {
                snprintf(buf, 256, "-D%s", skey->chars);
                args.push_back(strdup(buf));
            } else {
                string sval = Au_cast_string(val);
                snprintf(buf, 256, "-D%s=%s", skey->chars, sval->chars);
                args.push_back(strdup(buf));
            }
        }
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

    bool invocation_ok = CompilerInvocation::CreateFromArgs(
        *Invocation,
        cmdline_args,
        *Diags
    );
    verify(invocation_ok && !Diags->hasErrorOccurred(), "failed to build clang import invocation for %o", c);

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

    auto *silver_pragma = new SilverModulePragmaHandler(e);
    compiler->getPreprocessor().AddPragmaHandler(silver_pragma);

    instance = new0(aclang_cc,
        mod, e, compiler, (handle)compiler, PP, (handle)&compiler->getPreprocessor());
    compiler->getPreprocessor().addPPCallbacks(
        std::make_unique<MacroCollector2>(instance));
    compiler->createASTContext();
    ASTContext& ctx = compiler->getASTContext();
    compiler->getPreprocessor().getBuiltinInfo().initializeBuiltins(
        compiler->getPreprocessor().getIdentifierTable(),
        compiler->getPreprocessor().getLangOpts());

    if (is_header) {
        AetherASTConsumer2 consumer(e);
        ParseAST(compiler->getPreprocessor(), &consumer, ctx);
        verify(!Diags->hasErrorOccurred(), "failed to build import model for %o", c);
    } else {
        AetherEmitAction2 act(e);
        compiler->ExecuteAction(act);
        verify(!Diags->hasErrorOccurred(), "failed to build import model for %o", ipath);
        std::unique_ptr<llvm::Module> M = act.takeModule();
        LLVMModuleRef cMod = M ? wrap(M.release()) : nullptr;
        //(instance)->module = cMod;
        LLVMLinkModules2(e->module_ref, cMod);
    }

    aether_import_models(e, aether_top_scope(e), false);

    Au info = head(e->current_inc);

    unlink(c->chars);
    e->current_inc = null;
    return ipath;
*/
    return null;
}

} // extern "C"
