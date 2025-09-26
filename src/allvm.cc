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

extern "C" {

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
