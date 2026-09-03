// silver2 C/C++ import: clang parses the headers, silver2 gets
// declarations with layouts and symbols
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/VTableBuilder.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <string>
#include <vector>
using namespace clang;

extern "C" {
typedef struct CT {
    char        kind;
    int         bits;
    bool        is_signed, is_ref, is_const;
    struct CT*  elem;
    struct CD*  record;
    const char* spell;
} CT; // kind: v(oid) i(nt) f(loat) p(ointer) r(ecord) x(unknown)
typedef struct CD {
    int          kind;
    const char*  name;
    const char*  qualified;
    const char*  symbol;
    CT*          result;
    CT**         params;
    int          param_count;
    struct CD*   owner;
    long         size, offset;
    int          vtable_index;
    bool         is_static, is_virtual, is_variadic, is_inline;
    long         int_value;
    double       float_value;
    struct CD**  members;
    int          member_count;
    const char*  body;
    const char** param_names;
    int          param_name_count;
} CD;
enum {
    CD_FUNC = 1,
    CD_RECORD,
    CD_FIELD,
    CD_METHOD,
    CD_CTOR,
    CD_CONV,
    CD_MACRO,
    CD_TEMPLATE,
    CD_NAMESPACE,
    CD_ENUMCONST,
    CD_FTEMPLATE,
    CD_TYPEDEF
};
CD** c_import(const char* tu, const char** args, int nargs, int* count,
              const char* driver);
}

static std::vector<CD*> out;
static std::vector<CD*> records;
static CD*              find_record(const std::string& qualified_name) {
    for (CD* rec : records)
        if (qualified_name == rec->qualified) return rec;
    return nullptr;
}
static const char* dup(const std::string& text) {
    return strdup(text.c_str());
}

struct Visitor : RecursiveASTVisitor<Visitor> {
    ASTContext&                    ctx;
    std::unique_ptr<MangleContext> mangle;
    Visitor(ASTContext& context)
        : ctx(context), mangle(ItaniumMangleContext::create(
                            context, context.getDiagnostics())) {}
    bool shouldVisitTemplateInstantiations() const {
        return true;
    } // instantiated templates are the ones a module calls
    std::string qualname(const NamedDecl* decl) {
        if (const RecordDecl* record_decl =
                dyn_cast<RecordDecl>(decl)) {
            PrintingPolicy policy(ctx.getLangOpts());
            policy.SuppressTagKeyword = true;
            return ctx.getRecordType(record_decl).getAsString(policy);
        }
        std::string              text;
        llvm::raw_string_ostream stream(text);
        decl->printQualifiedName(stream,
                                 PrintingPolicy(ctx.getLangOpts()));
        return text;
    }
    std::string symbol(GlobalDecl global_decl) {
        std::string              text;
        llvm::raw_string_ostream stream(text);
        if (mangle->shouldMangleDeclName(
                cast<NamedDecl>(global_decl.getDecl())))
            mangle->mangleName(global_decl, stream);
        else
            stream << cast<NamedDecl>(global_decl.getDecl())->getName();
        return text;
    }
    CT* type(QualType qual_type) {
        CT* ct         = (CT*)calloc(1, sizeof(CT));
        ct->is_const   = qual_type.isConstQualified();
        QualType canon = qual_type.getCanonicalType();
        if (canon->isReferenceType()) {
            ct->is_ref   = true;
            canon        = canon->getPointeeType();
            ct->is_const = canon.isConstQualified();
        }
        {
            PrintingPolicy policy(ctx.getLangOpts());
            policy.SuppressTagKeyword = true;
            ct->spell                 = dup(canon.getAsString(policy));
        }
        if (canon->isVoidType()) {
            ct->kind = 'v';
            return ct;
        }
        if (canon->isPointerType()) {
            ct->kind = 'p';
            ct->elem = type(canon->getPointeeType());
            return ct;
        }
        if (canon->isEnumeralType()) {
            ct->kind      = 'i';
            ct->bits      = 32;
            ct->is_signed = true;
            return ct;
        }
        if (canon->isBooleanType()) {
            ct->kind = 'i';
            ct->bits = 1;
            return ct;
        }
        if (canon->isIntegerType()) {
            ct->kind      = 'i';
            ct->bits      = ctx.getTypeSize(canon);
            ct->is_signed = canon->isSignedIntegerType();
            return ct;
        }
        if (canon->isFloatingType()) {
            ct->kind = 'f';
            ct->bits = ctx.getTypeSize(canon);
            return ct;
        }
        if (const RecordDecl* record_decl = canon->getAsRecordDecl()) {
            ct->kind   = 'r';
            ct->record = find_record(qualname(record_decl));
            if (!ct->record) ct->record = record(record_decl);
            if (ct->is_ref) {
                CT* elem     = (CT*)calloc(1, sizeof(CT));
                *elem        = *ct;
                elem->is_ref = false;
                ct->kind     = 'p';
                ct->elem     = elem;
            }
            return ct;
        }
        if (canon->isArrayType()) {
            ct->kind = 'p';
            ct->elem =
                type(ctx.getAsArrayType(canon)->getElementType());
            ct->bits =
                canon->isDependentType() || canon->isIncompleteType()
                    ? 0
                    : ctx.getTypeSize(canon) / 8;
            return ct;
        } /* bits: the array's bytes */
        if (canon->isFunctionType()) {
            ct->kind = 'p';
            return ct;
        }
        ct->kind = 'x';
        return ct;
    }
    CD* fn(const FunctionDecl* func, int kind, CD* owner,
           GlobalDecl global_decl) {
        CD* decl          = (CD*)calloc(1, sizeof(CD));
        decl->kind        = kind;
        decl->name        = dup(func->getNameAsString());
        decl->qualified   = dup(qualname(func));
        decl->symbol      = dup(symbol(global_decl));
        decl->owner       = owner;
        decl->is_variadic = func->isVariadic();
        decl->is_inline =
            func->hasBody() &&
            !ctx.getSourceManager().isInSystemHeader(
                func->getLocation()) &&
            func->getTemplatedKind() == FunctionDecl::TK_NonTemplate &&
            !func->isDependentContext(); // a header definition the
                                         // import unit must reference
                                         // to get emitted
        decl->result      = type(func->getReturnType());
        decl->param_count = func->getNumParams();
        decl->params = (CT**)calloc(decl->param_count + 1, sizeof(CT*));
        for (unsigned i = 0; i < func->getNumParams(); i++)
            decl->params[i] = type(func->getParamDecl(i)->getType());
        return decl;
    }
    CD* record(const RecordDecl* record_decl) {
        if (!record_decl->isCompleteDefinition() ||
            record_decl->isDependentType())
            return nullptr;
        std::string qualified_name = qualname(record_decl);
        if (CD* rec = find_record(qualified_name)) return rec;
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_RECORD;
        decl->name      = dup(record_decl->getNameAsString());
        decl->qualified = dup(qualified_name);
        records.push_back(decl);
        out.push_back(decl);
        const ASTRecordLayout& layout =
            ctx.getASTRecordLayout(record_decl);
        decl->size = layout.getSize().getQuantity();
        std::vector<CD*> member_list;
        for (const FieldDecl* field_decl : record_decl->fields()) {
            CD* member     = (CD*)calloc(1, sizeof(CD));
            member->kind   = CD_FIELD;
            member->name   = dup(field_decl->getNameAsString());
            member->result = type(field_decl->getType());
            member->offset =
                layout.getFieldOffset(field_decl->getFieldIndex()) / 8;
            member->owner = decl;
            member_list.push_back(member);
        }
        if (const CXXRecordDecl* cxx_decl =
                dyn_cast<CXXRecordDecl>(record_decl)) {
            for (const CXXBaseSpecifier& base : cxx_decl->bases())
                if (CD* base_rec =
                        record(base.getType()->getAsRecordDecl()))
                    for (int i = 0; i < base_rec->member_count; i++)
                        member_list.push_back(
                            base_rec->members[i]); // base members land
                                                   // first
            for (const CXXMethodDecl* method_decl :
                 cxx_decl->methods()) {
                if (method_decl->isDeleted() ||
                    isa<CXXDestructorDecl>(method_decl))
                    continue;
                int kind =
                    isa<CXXConstructorDecl>(method_decl)  ? CD_CTOR
                    : isa<CXXConversionDecl>(method_decl) ? CD_CONV
                                                          : CD_METHOD;
                CD* member =
                    fn(method_decl, kind, decl,
                       kind == CD_CTOR
                           ? GlobalDecl(
                                 cast<CXXConstructorDecl>(method_decl),
                                 Ctor_Base)
                           : GlobalDecl(method_decl));
                member->is_static = method_decl->isStatic();
                if (method_decl->isVirtual()) {
                    member->is_virtual = true;
                    member->vtable_index =
                        cast<ItaniumVTableContext>(
                            ctx.getVTableContext())
                            ->getMethodVTableIndex(
                                GlobalDecl(method_decl));
                }
                if (kind == CD_CONV) member->name = dup("cast");
                member_list.push_back(member);
            }
        }
        decl->member_count = member_list.size();
        decl->members =
            (CD**)calloc(member_list.size() + 1, sizeof(CD*));
        for (size_t i = 0; i < member_list.size(); i++)
            decl->members[i] = member_list[i];
        return decl;
    }
    bool VisitRecordDecl(RecordDecl* record_decl) {
        record(record_decl);
        return true;
    }
    bool VisitFunctionDecl(FunctionDecl* func) {
        if (isa<CXXMethodDecl>(func) ||
            func->getTemplatedKind() ==
                FunctionDecl::TK_FunctionTemplate)
            return true;
        out.push_back(fn(func, CD_FUNC, nullptr, GlobalDecl(func)));
        return true;
    }
    bool VisitClassTemplateDecl(ClassTemplateDecl* template_decl) {
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_TEMPLATE;
        decl->name      = dup(template_decl->getNameAsString());
        decl->qualified = dup(qualname(template_decl));
        out.push_back(decl);
        return true;
    }
    bool
    VisitFunctionTemplateDecl(FunctionTemplateDecl* template_decl) {
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_FTEMPLATE;
        decl->name      = dup(template_decl->getNameAsString());
        decl->qualified = dup(qualname(template_decl));
        out.push_back(decl);
        return true;
    }
    bool VisitNamespaceDecl(NamespaceDecl* namespace_decl) {
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_NAMESPACE;
        decl->name      = dup(namespace_decl->getNameAsString());
        decl->qualified = dup(qualname(namespace_decl));
        out.push_back(decl);
        return true;
    }
    bool VisitTypedefNameDecl(TypedefNameDecl* typedef_decl) {
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_TYPEDEF;
        decl->name      = dup(typedef_decl->getNameAsString());
        decl->qualified = dup(qualname(typedef_decl));
        decl->result    = type(typedef_decl->getUnderlyingType());
        {
            QualType underlying = typedef_decl->getUnderlyingType();
            decl->size          = underlying->isDependentType() ||
                                 underlying->isIncompleteType() ||
                                 underlying->isFunctionType()
                                      ? 0
                                      : ctx.getTypeSize(underlying) / 8;
        }
        out.push_back(decl);
        return true;
    }
    bool VisitEnumConstantDecl(EnumConstantDecl* enum_decl) {
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_ENUMCONST;
        decl->name      = dup(enum_decl->getNameAsString());
        decl->int_value = enum_decl->getInitVal().getSExtValue();
        out.push_back(decl);
        return true;
    }
    bool
    VisitVarDecl(VarDecl* var_decl) { // s2_macro_NAME probes carry the
                                      // value of an object-like macro
        if (var_decl->getName().starts_with("s2_macro_")) {
            CD* decl     = (CD*)calloc(1, sizeof(CD));
            decl->kind   = CD_MACRO;
            decl->name   = dup(var_decl->getName().substr(9).str());
            decl->result = type(var_decl->getType());
            if (const APValue* value = var_decl->evaluateValue()) {
                if (value->isInt())
                    decl->int_value = value->getInt().getSExtValue();
                else if (value->isFloat())
                    decl->float_value =
                        value->getFloat().convertToDouble();
            }
            out.push_back(decl);
        }
        return true;
    }
};
struct Consumer : ASTConsumer {
    void HandleTranslationUnit(ASTContext& ctx) override {
        Visitor visitor(ctx);
        visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    }
};
struct Macros
    : PPCallbacks { // every macro with a body: name, parameters and the
                    // body's spelling; silver2 expands them itself
    Preprocessor& pp;
    Macros(Preprocessor& preprocessor) : pp(preprocessor) {}
    void MacroDefined(const Token&          token,
                      const MacroDirective* directive) override {
        const MacroInfo* info = directive->getMacroInfo();
        if (!info || info->isBuiltinMacro() || !info->getNumTokens())
            return;
        CD* decl        = (CD*)calloc(1, sizeof(CD));
        decl->kind      = CD_MACRO;
        decl->is_static = info->isFunctionLike();
        decl->name = dup(token.getIdentifierInfo()->getName().str());
        decl->qualified = decl->name;
        std::string body;
        for (const Token& body_token : info->tokens()) {
            if (!body.empty()) body += ' ';
            body += pp.getSpelling(body_token);
        }
        decl->body             = dup(body);
        decl->param_name_count = info->getNumParams();
        decl->param_names      = (const char**)calloc(
            decl->param_name_count + 1, sizeof(char*));
        int index = 0;
        for (const IdentifierInfo* param : info->params())
            decl->param_names[index++] = dup(param->getName().str());
        out.push_back(decl);
    }
};
struct Action : ASTFrontendAction {
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance& ci, llvm::StringRef) override {
        ci.getPreprocessor().addPPCallbacks(
            std::make_unique<Macros>(ci.getPreprocessor()));
        return std::make_unique<Consumer>();
    }
};

CD** c_import(const char* unit_file, const char** extra_args,
              int extra_count, int* count, const char* driver) {
    out.clear();
    records.clear();
    std::vector<const char*> clang_argv = {"-x", "c++", "-std=c++17",
                                           "-fsyntax-only", "-w"};
    for (int i = 0; i < extra_count; i++)
        clang_argv.push_back(extra_args[i]);
    if (FILE* pipe = popen(
            (std::string(driver) +
             " -E -x c++ -v /dev/null 2>&1 | sed -n '/search starts "
             "here/,/End of search/p'; " +
             driver + " -print-resource-dir")
                .c_str(),
            "r")) { // the host compiler's search list, so headers
                    // resolve as they do for clang
        char        line[1024];
        std::string resource_dir;
        while (fgets(line, sizeof line, pipe)) {
            std::string text(line);
            while (!text.empty() &&
                   (text.back() == '\n' || text.back() == ' '))
                text.pop_back();
            if (text.empty() || text[0] != ' ' && text[0] != '/')
                continue;
            if (text[0] == ' ') {
                clang_argv.push_back("-isystem");
                clang_argv.push_back(strdup(text.c_str() + 1));
            } else resource_dir = text;
        }
        pclose(pipe);
        if (!resource_dir.empty()) {
            clang_argv.push_back("-resource-dir");
            clang_argv.push_back(strdup(resource_dir.c_str()));
        }
    }
    out.clear();     // one unit's records per call: a nested compile
    records.clear(); // must not inherit its parent's
    out.clear();     // one unit's records per call: a nested compile
    records.clear(); // must not inherit its parent's
    clang_argv.push_back(unit_file);
    auto              vfs = llvm::vfs::getRealFileSystem();
    DiagnosticOptions diag_opts;
    auto              diags = CompilerInstance::createDiagnostics(
        *vfs, diag_opts,
        new TextDiagnosticPrinter(llvm::errs(), diag_opts));
    auto invocation = std::make_shared<CompilerInvocation>();
    CompilerInvocation::CreateFromArgs(*invocation, clang_argv, *diags);
    CompilerInstance instance(invocation);
    instance.createDiagnostics(
        *vfs,
        new TextDiagnosticPrinter(llvm::errs(),
                                  instance.getDiagnosticOpts()),
        true);
    Action action;
    if (!instance.ExecuteAction(action)) {
        *count = 0;
        return nullptr;
    }
    CD** result = (CD**)calloc(out.size() + 1, sizeof(CD*));
    for (size_t i = 0; i < out.size(); i++) result[i] = out[i];
    *count = out.size();
    return result;
}
