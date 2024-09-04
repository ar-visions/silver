#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Verifier.h>


#include <A>

// basic IR usage; its more direct than an AST but not as granular, yet, is debuggable.. not a 'front end' but obviously is a front end.
// clang++ -g2 -std=c++17 -I`llvm-config-14 --includedir` `llvm-config-14 --cflags --cxxflags --ldflags --libs core executionengine mcjit native` -o ir ir.cc

using namespace llvm;

extern "C" {
int main2();
}

int main2() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    LLVMContext context;
    Module* module = new Module("struct_example", context);
    IRBuilder<> builder(context);

    // define the struct Something { char* name; int age; }
    StructType* structType = StructType::create(context, "Something");

    // add the members to the struct
    std::vector<Type*> members;
    members.push_back(builder.getInt8PtrTy()); // char* (name)
    members.push_back(builder.getInt32Ty());   // int (age)
    structType->setBody(members);

    // create the main function
    FunctionType* mainType = FunctionType::get(builder.getInt32Ty(), false);
    Function* mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", module);

    // create the entry block
    BasicBlock* entry = BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);

    // Allocate memory for the struct (Something)
    Value* structPtr = builder.CreateAlloca(structType);

    // Set the 'name' field (char*)
    Value* namePtr = builder.CreateStructGEP(structType, structPtr, 0); // Get pointer to 'name'
    Value* nameValue = builder.CreateGlobalStringPtr("John Doe");
    builder.CreateStore(nameValue, namePtr);

    // Set the 'age' field (int)
    Value* agePtr = builder.CreateStructGEP(structType, structPtr, 1);  // Get pointer to 'age'
    builder.CreateStore(builder.getInt32(30), agePtr); // Store age = 30

    // Return 0 from main
    builder.CreateRet(builder.getInt32(0));

    // Verify the function
    verifyFunction(*mainFunc);


    // Write the IR to a file
    std::error_code EC;
    raw_fd_ostream dest("a.ll", EC);

    if (EC) {
        errs() << "Could not open file: " << EC.message() << "\n";
        return 1;
    }

    // Print the LLVM IR to the file
    module->print(dest, nullptr);

    dest.flush(); // Make sure everything is written to the file

    // Cleanup
    delete module;

    return 0;
}