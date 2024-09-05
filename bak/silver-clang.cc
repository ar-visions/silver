#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Stmt.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;

class SimpleASTConsumer : public ASTConsumer {
public:
    void HandleTranslationUnit(ASTContext &Context) override {
        TranslationUnitDecl *TU = Context.getTranslationUnitDecl();

        // Create the 'int' type
        QualType IntType = Context.IntTy;

        // Create the 'char *argv[]' type
        QualType CharType = Context.CharTy;
        QualType CharPointerType = Context.getPointerType(CharType);
        QualType CharPointerArrayType = Context.getPointerType(CharPointerType);

        // Create 'argc' parameter: int argc
        ParmVarDecl *ArgcDecl = ParmVarDecl::Create(
            Context, TU, SourceLocation(), SourceLocation(),
            &Context.Idents.get("argc"), IntType, nullptr, SC_None, nullptr);

        // Create 'argv' parameter: char *argv[]
        ParmVarDecl *ArgvDecl = ParmVarDecl::Create(
            Context, TU, SourceLocation(), SourceLocation(),
            &Context.Idents.get("argv"), CharPointerArrayType, nullptr, SC_None, nullptr);

        // Create the main function: int main(int argc, char *argv[])
        FunctionDecl *MainFuncDecl = FunctionDecl::Create(
            Context, TU, SourceLocation(), SourceLocation(),
            DeclarationName(&Context.Idents.get("main")),
            Context.getFunctionType(IntType, {IntType, CharPointerArrayType}, FunctionProtoType::ExtProtoInfo()),
            nullptr, SC_None, false, false);

        // Set function parameters
        MainFuncDecl->setParams({ArgcDecl, ArgvDecl});

        // Create the return statement: return 1;
        IntegerLiteral *ReturnValue = IntegerLiteral::Create(
            Context, llvm::APInt(32, 1), IntType, SourceLocation());

        ReturnStmt *ReturnStatement = ReturnStmt::Create(Context, SourceLocation(), ReturnValue, nullptr);

        // Create the function body
        SourceLocation StartLoc = SourceLocation();
        SourceLocation EndLoc = SourceLocation();
        FPOptionsOverride FPFeatures;  // Default floating-point options
        CompoundStmt *Body = CompoundStmt::Create(Context, {ReturnStatement}, FPFeatures, StartLoc, EndLoc);

        MainFuncDecl->setBody(Body);

        // Add the main function to the translation unit
        TU->addDecl(MainFuncDecl);
    }
};

class SimpleFrontendAction : public ASTFrontendAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
        return std::make_unique<SimpleASTConsumer>();
    }
};

int main(int argc, const char **argv) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    CompilerInstance CI;
    //CI.createDiagnostics();
    CI.createDiagnostics(new TextDiagnosticPrinter(llvm::errs(), new DiagnosticOptions()));
    assert(CI.hasDiagnostics() && "Diagnostics not initialized!");

    CI.getTargetOpts().Triple = llvm::sys::getDefaultTargetTriple();
    CI.createTarget();
    assert(CI.hasTarget() && "Target not initialized!");

    // Set language options
    CI.getLangOpts().C99 = true;
    CI.getLangOpts().Bool = true;

    // Create the FileManager and SourceManager
    CI.createFileManager();
    assert(CI.hasFileManager() && "FileManager not initialized!");

    CI.createSourceManager(CI.getFileManager());
    assert(CI.hasSourceManager() && "SourceManager not initialized!");

    // Create the ASTContext
    CI.createASTContext(); // Ensure everything before this is correctly initialized

    // Create and execute the action
    auto Act = std::make_unique<EmitLLVMOnlyAction>();
    CI.ExecuteAction(*Act);

    // Extract the TranslationUnitDecl and print the LLVM IR
    auto &Context = CI.getASTContext();
    auto TU = Context.getTranslationUnitDecl();
    TU->print(llvm::outs());

    // Save the LLVM IR to a file
    std::error_code EC;
    llvm::raw_fd_ostream OS("output.ll", EC, llvm::sys::fs::OF_None);
    TU->print(OS);
    OS.close();

    // Compile the generated IR to an executable
    system("clang output.ll -o output");

    return 0;
}