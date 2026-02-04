#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/CBindingWrapping.h"

#include <llvm/DebugInfo/Symbolize/Symbolize.h>
#include <llvm/Support/Error.h>
#include <string>
#include <memory>

#if defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #if defined(__APPLE__)
        #include <mach-o/dyld.h> // Helper for macOS ASLR offsets
    #endif
#endif

extern "C" {

static llvm::symbolize::LLVMSymbolizer *Symbolizer = nullptr;

bool dbg_addr_to_line(void *addr, const char **file, int *line, const char **func) 
{
#ifndef NDEBUG
    // 1. Initialize Symbolizer
    if (!Symbolizer) {
        llvm::symbolize::LLVMSymbolizer::Options Opts;
        Opts.Demangle = true;
        Opts.RelativeAddresses = false; 
        Symbolizer = new llvm::symbolize::LLVMSymbolizer(Opts);
    }

    std::string modulePath;
    uint64_t fileOffset = 0;

    // ---------------------------------------------------------
    // WINDOWS
    // ---------------------------------------------------------
#if defined(_WIN32)
    HMODULE hModule = NULL;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCTSTR)addr, 
                           &hModule)) {
        return false;
    }

    char path[MAX_PATH];
    if (GetModuleFileNameA(hModule, path, MAX_PATH) == 0) {
        return false;
    }

    modulePath = path;
    fileOffset = (uintptr_t)addr - (uintptr_t)hModule;

    // ---------------------------------------------------------
    // LINUX & MACOS
    // ---------------------------------------------------------
#else
    Dl_info info;
    if (dladdr(addr, &info) == 0 || !info.dli_fname) {
        return false;
    }

    modulePath = info.dli_fname;
    
    // On macOS, dli_fbase is the slide (ASLR offset), similar to Linux.
    // However, LLVMSymbolizer on macOS sometimes prefers the dSYM path.
    // Standard practice for basic usage: subtract base address.
    fileOffset = (uintptr_t)addr - (uintptr_t)info.dli_fbase;

    #if defined(__APPLE__)
    // Extra safety for macOS: dladdr might return the path to the executable,
    // but LLVM might need the path to the .dSYM bundle if debug info isn't embedded.
    // (Note: This simple logic relies on debug info being in the binary or standard lookup)
    #endif

#endif

    // 2. Symbolize
    auto ExpectedInfo = Symbolizer->symbolizeCode(modulePath, {fileOffset});

    if (!ExpectedInfo) {
        llvm::consumeError(ExpectedInfo.takeError());
        return false;
    }

    const auto& Info = *ExpectedInfo;

    if (Info.FileName == "<invalid>" || Info.Line == 0) {
        return false;
    }

    static std::string last_file;
    static std::string last_func;

    last_file = Info.FileName;
    last_func = Info.FunctionName;

    *file = last_file.c_str();
    *line = static_cast<int>(Info.Line);
    *func = last_func.c_str();

    return true;
#else
    return false;
#endif
}

LLVMValueRef LLVMConstMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::Mul,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstSDiv(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::SDiv,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstUDiv(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::UDiv,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstURem(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::URem,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstSRem(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::SRem,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstShl(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::Shl,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstLShr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::LShr,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstAShr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::AShr,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstAnd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::And,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstOr(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::Or,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

LLVMValueRef LLVMConstXor(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
    return llvm::wrap(llvm::ConstantExpr::get(
        llvm::Instruction::Xor,
        llvm::unwrap<llvm::Constant>(LHSConstant),
        llvm::unwrap<llvm::Constant>(RHSConstant)));
}

} // extern "C"
