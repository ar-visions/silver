
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>

//#include <clang-c/Index.h>


#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include <clang/Basic/TargetInfo.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DataLayout.h>
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

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


static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e, string use_name);


// Enum visitor equivalent
static enumeration create_enum(EnumDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    verify (len(n), "expected name for enumerable");

    enumeration en = enumeration(mod, e, name, (token)n, members, map(hsize, 8));
    push(e, (model)en);
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
    pop(e);
    
    register_model(e, (model)en, false);
    finalize((model)en);
    return en;
}

// Function creation
static fn create_fn(FunctionDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    
    if (eq(n, "vkCreateInstance")) {
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
        set(args->members, (A)pname, (A)earg);
    }
    
    // Check for variadic
    bool is_variadic = decl->isVariadic();
    
    fn f = fn(mod, e, name, (token)n, rtype, rtype, args, args, va_args, is_variadic);
    register_model(e, (model)f, false);
    //use(f);
    finalize((model)f);
    return f;
}

// Record (struct/union) creation
static record create_record(RecordDecl* decl, ASTContext& ctx, aether e) {
    std::string name = decl->getNameAsString();
    string n = string(name.c_str());
    
    // Check if already exists
    emember existing = lookup2(e, (A)n, null);
    if (existing && existing->mdl)
        return (record)existing->mdl;

    // Check if it's a union or struct
    bool is_union = decl->isUnion();
    
    record rec = is_union ?
        (record)uni(mod, e, name, (token)n) :
        (record)structure(mod, e, name, (token)n);
    
    rec->members = map(hsize, 8);
    register_model(e, (model)rec, false);
    
    // Get the layout for accurate offsets/sizes
    if (decl->isCompleteDefinition()) {
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

            model mapped = map_clang_type_to_model(field_type, ctx, e, null);
            if (!mapped) continue;
            
            push(e, mapped);
            emember m = emember(mod, rec->mod, name, (token)fname, mdl, mapped);
            pop(e);

            uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());

            // important:
            // these specific members LLVM IR does not actually support in a general sense, and we must, as a user construct padding and operations
            // this is not remotely designed and, if our offsets differ, the compiler should error
            // for silver 1.0, we want bitfield support
            m->override_offset_bits = A_i32(offset_bits);

            // size
            if (field->isBitField()) {
                m->override_size_bits = A_i32(field->getBitWidthValue());  // in bits
            } else if (const auto *IAT = dyn_cast<IncompleteArrayType>(field_type)) {
                // flexible array member (must be last): size is 0 in the struct
                m->override_size_bits = A_i32(0);
            } else {
                m->override_size_bits = A_i32(ctx.getTypeSizeInChars(field_type).getQuantity());
            }

            // alignment (prefer decl-level to capture alignas/attributes)
            CharUnits declAlign = ctx.getDeclAlign(field);
            if (declAlign.isZero())
                declAlign = ctx.getTypeAlignInChars(field_type);
            m->override_alignment_bits = A_i32(declAlign.getQuantity());

            // Get accurate offset (handles pragma pack!)
            if (!is_union) {
                uint64_t offset_bits = layout.getFieldOffset(field->getFieldIndex());
                m->offset_bits = offset_bits / 8;  // Convert to bytes
            }

            m->index = field_index++;
            set(rec->members, (A)fname, (A)m);
        }
        
        // Set struct/union size and alignment
        rec->size_bits = layout.getSize().getQuantity() * 8;
        rec->alignment_bits = layout.getAlignment().getQuantity() * 8;
    }
    
    finalize((model)rec);
    return rec;
}

// Function type helper
static model map_function_type(const FunctionProtoType* fpt, ASTContext& ctx, aether e) {
    QualType return_qt = fpt->getReturnType();
    model return_model = map_clang_type_to_model(return_qt, ctx, e, null);
    
    eargs param_models = eargs(mod, e);
    
    for (unsigned i = 0; i < fpt->getNumParams(); i++) {
        QualType param_type = fpt->getParamType(i);
        model param = map_clang_type_to_model(param_type, ctx, e, null);
        
        // Function types don't have parameter names
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "arg_%u", i);
        string n = string(name_buf);
        
        emember marg = emember(mod, e, name, null, mdl, param, is_arg, true);
        set(param_models->members, (A)n, (A)marg);
    }
    
    bool is_variadic = fpt->isVariadic();
    fn f = fn(mod, e, rtype, return_model, args, param_models, va_args, is_variadic);
    //use(f);
    return (model)f;
}

// Function pointer helper
static model map_function_pointer(QualType pointee_qt, ASTContext& ctx, aether e) {
    const Type* pointee = pointee_qt.getTypePtr();
    
    if (const FunctionProtoType* fpt = dyn_cast<FunctionProtoType>(pointee)) {
        model func_model = map_function_type(fpt, ctx, e);
        use((fn)func_model);
        return model(mod, e, src, func_model, is_ref, true);
    }
    
    if (const FunctionNoProtoType* fnpt = dyn_cast<FunctionNoProtoType>(pointee)) {
        // Old-style function without prototype
        QualType return_qt = fnpt->getReturnType();
        model return_model = map_clang_type_to_model(return_qt, ctx, e, null);
        eargs empty_args = eargs(mod, e);
        
        model func_model = (model)fn(mod, e, rtype, return_model, args, empty_args);
        use((fn)func_model);
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
static model map_clang_type_to_model(const QualType& qt, ASTContext& ctx, aether e, string use_name) {
    const Type* t = qt.getTypePtr();

    // Strip elaborated type (like CXType_Elaborated)
    if (const ElaboratedType* et = dyn_cast<ElaboratedType>(t)) {
        return map_clang_type_to_model(et->getNamedType(), ctx, e, null);
    }
    
    // Handle typedefs
    if (const TypedefType* tt = dyn_cast<TypedefType>(t)) {
        std::string name = tt->getDecl()->getName().str();
        symbol n = name.c_str();
        model existing = emodel(n);
        if (existing) return existing;
        if (!use_name) use_name = string(n);

        if (strcmp(n, "__INTMAX_C") == 0) {
            int test2 = 2;
            test2    += 2;
        }
        // Recurse on underlying type
        return map_clang_type_to_model(tt->getDecl()->getUnderlyingType(), ctx, e, use_name);
    }
    

    QualType unqualified = qt.getCanonicalType().getUnqualifiedType();
    const Type* type = unqualified.getTypePtr();
    //const Type* type = qt.getTypePtr();
    

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
        model elem_model = map_clang_type_to_model(elem_type, ctx, e, null);
        
        if (!elem_model) return nullptr;
        
        if (elem_type.isConstQualified()) {
            elem_model = model(mod, e, src, elem_model, is_const, true);
        }
        shape sh = (shape)A_struct(_shape);
        sh->count = 1;
        sh->data[0] = size;
        return model(mod, e, src, elem_model, shape, sh);
    }
    
    if (const IncompleteArrayType* iat = dyn_cast<IncompleteArrayType>(type)) {
        QualType elem_type = iat->getElementType();
        model elem_model = map_clang_type_to_model(elem_type, ctx, e, null);
        if (!elem_model) return nullptr;
        shape sh = (shape)A_struct(_shape);
        sh->count = 1;
        sh->data[0] = 0;
        return model(mod, e, src, elem_model, shape, sh);
    }
    
    // Pointer types
    if (const PointerType* pt = dyn_cast<PointerType>(type)) {
        QualType pointee = pt->getPointeeType();
        if (use_name) {
            model existing = emodel(use_name->chars);
            if (existing) return existing;
        }
        // function pointers
        if (pointee->isFunctionType())
            return map_function_pointer(pointee, ctx, e);
        
        model base = map_clang_type_to_model(pointee, ctx, e, null);
        if (!base)
             base = map_clang_type_to_model(pointee, ctx, e, null);
        
        verify(base, "could not resolve pointer type");
        
        model ptr = model(mod, e, name, (token)use_name, src, base, is_ref, true,
            is_const, pointee.isConstQualified());
        if (use_name)
            register_model(e, ptr, false);
        return ptr;
    }
    
    // Record types (struct/union/class)
    if (const RecordType* rt = dyn_cast<RecordType>(type)) {
        RecordDecl* decl = rt->getDecl();
        std::string name = decl->getNameAsString();
        model existing = emodel(name.c_str());
        return existing ? existing : (model)create_record(decl, ctx, e);
    }
    
    // Enum types
    if (const EnumType* et = dyn_cast<EnumType>(type)) {
        EnumDecl* decl = et->getDecl();
        std::string name = decl->getNameAsString();
        model existing = emodel(name.c_str());
        return existing ? existing : (model)create_enum(decl, ctx, e);
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

// custom AST consumer
class AetherASTConsumer : public clang::ASTConsumer {
    AetherDeclVisitor& visitor;
    ASTContext& ctx;
public:
    AetherASTConsumer(AetherDeclVisitor& v, ASTContext& c) : visitor(v), ctx(c) {}
    
    void HandleTranslationUnit(ASTContext& context) override {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
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





// Better approach: Create a reusable macro expander lets call it SashaXpander
class SashaXpander {
private:
    clang::CompilerInstance* compiler;
    clang::Preprocessor* PP;

public:
    SashaXpander(clang_cc instance) :
        compiler((clang::CompilerInstance*)instance->compiler),
        PP((clang::Preprocessor*)instance->PP) { }
    
    ~SashaXpander() {
    }
    
    // Register a macro for later expansion
    void registerMacro(const std::string& name, const std::string& definition) {
        std::string macro_def = "#define " + definition + "\n";
        
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(macro_def, "<macro>");
        clang::FileID FID = compiler->getSourceManager().createFileID(std::move(buffer));
        
        PP->EnterSourceFile(FID, nullptr, clang::SourceLocation());
        
        // Parse the macro definition
        clang::Token tok;
        PP->Lex(tok); // Should be 'define'
        PP->HandleDirective(tok);
    }
    
    // Expand a macro with arguments
    std::string expandMacro(const std::string& macro_name, 
                           const std::vector<std::string>& args) {
        // Build invocation
        std::string invocation = macro_name + "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) invocation += ",";
            invocation += args[i];
        }
        invocation += ")";
        
        // Create buffer with the invocation
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(invocation, "<expansion>");
        clang::FileID FID = compiler->getSourceManager().createFileID(std::move(buffer));
        
        // Save current state
        PP->EnterSourceFile(FID, nullptr, clang::SourceLocation());
        
        // Collect expanded tokens
        std::string result;
        clang::Token tok;
        
        while (true) {
            PP->Lex(tok);
            if (tok.is(clang::tok::eof))
                break;
            
            if (!result.empty()) result += " ";
            result += PP->getSpelling(tok);
        }
        
        return result;
    }
};

// Store macros with their definitions
struct StoredMacro {
    std::string name;
    std::string definition;  // Full definition: "MAX(a,b) ((a)>(b)?(a):(b))"
    std::vector<std::string> params;
    bool is_variadic;
};

class MacroCollector : public clang::PPCallbacks {
public:
    clang_cc instance;
    clang::Preprocessor* PP;
    std::map<std::string, StoredMacro> stored_macros;

    explicit MacroCollector(clang_cc instance)
        : instance(instance), PP((clang::Preprocessor*)instance->PP) {}

    void MacroDefined(const clang::Token &macroNameTok,
                      const clang::MacroDirective *md) override {
        const clang::MacroInfo *mi = md->getMacroInfo();
        
        if (!mi->isFunctionLike())
            return;
            
        StoredMacro stored;
        stored.name = macroNameTok.getIdentifierInfo()->getName().str();
        stored.is_variadic = mi->isVariadic();
        
        // Build the full definition string
        std::string def = stored.name + "(";
        
        // Add parameters
        bool first = true;
        for (auto it = mi->param_begin(); it != mi->param_end(); ++it) {
            if (!first) def += ",";
            first = false;
            std::string param_name = (*it)->getName().str();
            stored.params.push_back(param_name);
            def += param_name;
        }
        if (mi->isVariadic()) {
            if (!first) def += ",";
            def += "...";
        }
        def += ") ";
        
        // Add body tokens
        for (auto it = mi->tokens_begin(); it != mi->tokens_end(); ++it) {
            def += PP->getSpelling(*it);
            if (std::next(it) != mi->tokens_end())
                def += " ";
        }
        
        stored.definition = def;
        stored_macros[stored.name] = stored;
        
        // Register in aether
        register_param_macro(stored);
    }

private:
    void register_param_macro(const StoredMacro& stored) {
        string n = string(stored.name.c_str());
        aether e = this->instance->mod;
        array  params = array(32);
        for (size_t i = 0; i < stored.params.size(); i++) {
            string pname = string(stored.params[i].c_str());
            push(params, (A)pname);
        }
        string definition = string(stored.definition.c_str());

        macro m = macro(
            mod,        e,
            instance,   this->instance,
            name,       (token)n,
            definition, definition,
            params,     params,
            is_var,     stored.is_variadic);
        
        register_model(e, (model)m, false);
    }
};

// Global expander instance
static SashaXpander* global_expander = nullptr;

void init_macro_expander(clang_cc instance) {
    if (!global_expander) {
        global_expander = new SashaXpander(instance);
    }
}

// returns tokens to replace
array macro_expand(macro m, array args_tokens) {
    aether e = m->mod;

    if (!global_expander) {
        init_macro_expander(m->instance);
    }
    
    // Register the macro if not already done
    // we may also 'update' the macro with evaluation from the module (must support this so we may have silvers const working with pre-processor)

    global_expander->registerMacro(m->name->chars, m->definition->chars);
    
    // Convert args to strings
    std::vector<std::string> arg_strings;
    for (int i = 0; i < args_tokens->len; i++) {
        string s = string(64);
        array  arg_tokens = (array)args_tokens->elements[i];
        verify((AType)isa(arg_tokens) == typeid(array),
            "expected array with token, got %s", isa(arg_tokens)->name);
        each (arg_tokens, token, t) {
            if (len(s))
                append(s, " ");
            concat(s, (string)t);
        }
        arg_strings.push_back(s->chars);
    }
    
    // Expand
    std::string res    = global_expander->expandMacro(m->name->chars, arg_strings);
    string      input  = string(res.c_str());
    array       tokens = (array)e->parse_f((A)input);
    return tokens;
}

// this must now INSTANCE an Inovcation & CompilerInstance and KEEP it in memory
path aether_include(aether e, A inc, ARef _instance) {
    clang_cc* instance = (clang_cc*)_instance;
    path ipath = (AType)isa(inc) == typeid(string) ?
        lookup_include(e, (string)inc) : (path)inc;
    verify(ipath && exists(ipath), "include path does not exist: %o", ipath ? (A)ipath : inc);
    e->current_include = ipath;


    // Create DiagID
    auto DiagID(new DiagnosticIDs());
    auto DiagOpts(new DiagnosticOptions());

    // Create invocation first
    auto Invocation = std::make_shared<CompilerInvocation>();


    // Add Clang's resource directory for built-in headers
    Invocation->getHeaderSearchOpts().ResourceDir = "/src/silver/lib/clang/22";

    // Or add the include path directly
    Invocation->getHeaderSearchOpts().AddPath(
        "/src/silver/lib/clang/22/include", 
        frontend::System,  // System headers
        false, 
        false);



    path c = f(path, "/tmp/%o.c", stem(ipath));
    string  contents = f(string, "#include \"%o\"\n", ipath);
    save(c, (A)contents, null);

    // Set up input file
    Invocation->getFrontendOpts().Inputs.push_back(
        FrontendInputFile(c->chars, Language::C));  // or Language::CXX
    
    // Set target
    Invocation->getTargetOpts().Triple = llvm::sys::getProcessTriple();
    
    // Add include paths
    for (int i = 0; i < e->include_paths->len; i++) {
        path inc_path = (path)e->include_paths->elements[i];
        Invocation->getHeaderSearchOpts().AddPath(
            inc_path->chars, frontend::Angled, false, false);
    }
    
    // Create compiler with the invocation
    CompilerInstance* compiler = new CompilerInstance(std::move(Invocation));
    

    // Then use it:
    SimpleDiagConsumer* DiagClient = new SimpleDiagConsumer();
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags = 
        new DiagnosticsEngine(DiagID, *DiagOpts, DiagClient);
    compiler->setDiagnostics(Diags.get());


    // Create other managers
    compiler->createFileManager();
    compiler->createSourceManager(compiler->getFileManager());

    auto fe = compiler->getFileManager().getFileRef(c->chars);
    verify(bool(fe), "cant find..");
    verify(fe.get(), "clang cannot find TU file: %o", c);


    FileID mainFileID = compiler->getSourceManager().createFileID(
        fe.get(), 
        SourceLocation(), 
        SrcMgr::C_User
    );
    compiler->getSourceManager().setMainFileID(mainFileID);

    compiler->createTarget();
    compiler->createPreprocessor(TU_Complete);

    *instance = clang_cc(
        mod, e, compiler, (handle)compiler, PP, (handle)&compiler->getPreprocessor()); // how do we keep this process alive and pass the compiler / pre processor in here?

    compiler->getPreprocessor().addPPCallbacks(
        std::make_unique<MacroCollector>(*instance));
    
    compiler->createASTContext();
    
    // Parse
    ASTContext& ctx = compiler->getASTContext();
    AetherDeclVisitor visitor(ctx, e);
    AetherASTConsumer consumer(visitor, ctx);
    ParseAST(compiler->getPreprocessor(), &consumer, ctx);
    unlink(c->chars);

    e->current_include = null;
    return ipath;
}

}