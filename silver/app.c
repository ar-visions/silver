#include <A>

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
//#include <llvm-c/Transforms/PassManagerBuilder.h>

void add_module_flag(LLVMModuleRef mod, const char *flag_name, int32_t value) {
    LLVMContextRef context = LLVMGetModuleContext(mod);

    // Create the integer metadata value for the flag
    LLVMMetadataRef valueMetadata = LLVMValueAsMetadata(LLVMConstInt(LLVMInt32TypeInContext(context), value, 0));

    // Add the flag to the module using LLVMAddModuleFlag
    LLVMAddModuleFlag(mod, LLVMModuleFlagBehaviorError, flag_name, strlen(flag_name), valueMetadata);
}

// Function to set the debug location for an instruction
void set_dbg_location(LLVMBuilderRef builder, LLVMModuleRef module, LLVMMetadataRef scope, LLVMMetadataRef file, int line, int column) {
    LLVMContextRef  context  = LLVMGetModuleContext(module);
    LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(context, line, column, scope, null);
    LLVMSetCurrentDebugLocation2(builder, location);
}

#if 1

int llvm_test(string module) {
    printf("LLVM Version: %d.%d.%d\n", LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR, LLVM_VERSION_PATCH);

    ///
    string source_path = str("/home/kalen/src/silver/silver-build");
    string source_file = format("%o.c", module);
    path   p           = cast(format("%o/%o", source_path, source_file), path);
    assert(call(p, exists));

    // create the LLVM module and context
    LLVMModuleRef    mod          = LLVMModuleCreateWithName("module.c");
    LLVMContextRef   context      = LLVMGetModuleContext(mod);

    
    LLVMDIBuilderRef diBuilder = LLVMCreateDIBuilder(mod);

    add_module_flag(mod, "Dwarf Version",      5);
    add_module_flag(mod, "Debug Info Version", 3);

    //emit_dwarf_debug_info_metadata(mod);

    // create a file reference (the source file for debugging)
    LLVMMetadataRef fileRef = LLVMDIBuilderCreateFile(
        diBuilder, source_file->chars, source_file->len, source_path->chars, source_path->len);
    
    // Create compile unit
    LLVMMetadataRef compileUnit = LLVMDIBuilderCreateCompileUnit(
        diBuilder, LLVMDWARFSourceLanguageC, fileRef,
        source_file->chars, source_file->len, 0, "", 0,
        3, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "/", 1, "", 0);

    // Define a struct type in the custom language
    LLVMTypeRef structType = LLVMStructCreateNamed(context, "MyStruct");
    LLVMTypeRef elementTypes[] = { LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type() }; // Member: int member
    LLVMStructSetBody(structType, elementTypes, 4, 0);

    // Create debug info for struct
    LLVMMetadataRef memberDebugTypes[4];
    for (int i = 0; i < 4; i++) {
        memberDebugTypes[i] = LLVMDIBuilderCreateBasicType(
            diBuilder, "int", 3, 32, 
            (LLVMDWARFTypeEncoding)0x05,
            (LLVMDIFlags)0); /// signed 0x05 (not defined somehwo) DW_ATE_signed
    }

    LLVMMetadataRef structDebugType = LLVMDIBuilderCreateStructType(
        diBuilder, compileUnit, "MyStruct", 8, fileRef, 1, 
        128, 32, 0, NULL, 
        memberDebugTypes, 4, 0, NULL, "", 0);

    // Create a function and access the struct member
    LLVMTypeRef  main_type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, false);
    LLVMValueRef main_func = LLVMAddFunction(mod, "main", main_type);
    
    LLVMBuilderRef builder = LLVMCreateBuilder();

    // Create debug info for the function
    LLVMMetadataRef funcDIType = LLVMDIBuilderCreateSubroutineType(diBuilder, fileRef, NULL, 0, 0);
    
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    
    LLVMMetadataRef funcDbgInfo = LLVMDIBuilderCreateFunction(
        diBuilder, compileUnit, "main", 4, "main", 4,
        fileRef, 1, funcDIType, 0, 1, 1, 0, LLVMDIFlagPrototyped);

    LLVMSetSubprogram(main_func, funcDbgInfo);
    
    set_dbg_location(builder, mod, funcDbgInfo, fileRef, 3, 0);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main_func, "entry");
    
    set_dbg_location(builder, mod, funcDbgInfo, fileRef, 3, 0);
    LLVMPositionBuilderAtEnd(builder, entry);

    // Create a pointer to MyStruct (simulate `self` in your custom language)
    LLVMValueRef structPtr = LLVMBuildAlloca(builder, structType, "self");

    // Create debug info for the local variable
    LLVMMetadataRef localVarDIType = LLVMDIBuilderCreatePointerType(
        diBuilder, structDebugType, 64, 0, 0, "MyStruct*", 9);
    
    LLVMMetadataRef localVarDbgInfo = LLVMDIBuilderCreateAutoVariable(
        diBuilder, funcDbgInfo, "self", 4, fileRef, 2, localVarDIType, 1, 0, 0);
    
    LLVMDIBuilderInsertDeclareAtEnd(
        diBuilder,
        structPtr,
        localVarDbgInfo,
        LLVMDIBuilderCreateExpression(diBuilder, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(context, 2, 0, funcDbgInfo, NULL),
        LLVMGetInsertBlock(builder));

    // Set values for struct members
    const char *memberNames[] = {"member1", "member2", "member3", "member4"};
    int memberValues[] = {42, 44, 46, 48};

    for (int i = 0; i < 4; i++) {
        LLVMValueRef memberPtr = LLVMBuildStructGEP2(builder, structType, structPtr, i, memberNames[i]);
        LLVMBuildStore(builder, LLVMConstInt(int32Type, memberValues[i], 0), memberPtr);

        // Create debug info for each member
        LLVMMetadataRef memberDebugInfo = LLVMDIBuilderCreateAutoVariable(
            diBuilder, funcDbgInfo, memberNames[i], strlen(memberNames[i]),
            fileRef, i + 3, memberDebugTypes[i], 0, 0, 0);
        LLVMDIBuilderInsertDeclareAtEnd(diBuilder, memberPtr, memberDebugInfo,
            LLVMDIBuilderCreateExpression(diBuilder, NULL, 0),
            LLVMDIBuilderCreateDebugLocation(context, i + 3, 0, funcDbgInfo, NULL),
            LLVMGetInsertBlock(builder));

        set_dbg_location(builder, mod, funcDbgInfo, fileRef, 3 + i, 0);
    }

    set_dbg_location(builder, mod, funcDbgInfo, fileRef, 7, 0);

    // Return from main
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, false));

    // Finalize the DIBuilder
    LLVMDIBuilderFinalize(diBuilder);

    // Verify the module
    char *error = NULL;
    LLVMBool status = LLVMVerifyModule(mod, LLVMPrintMessageAction, &error);
    if (status) {
        fprintf(stderr, "Error verifying module: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;  // Return with error
    } else {
        print("module verified");
    }

    // Print the generated IR
    cstr err = null;
    status = LLVMPrintModuleToFile(mod, "a.ll", &err);
    if (!status) {
        print("generated IR");
    } else {
        fprintf(stderr, "Failed to print module to string.\n");
        return 1;  // Return with error
    }

    // Generate bitcode file (example_module.bc)
    const char *bitcodeFilename = "a.bc";
    if (LLVMWriteBitcodeToFile(mod, bitcodeFilename) != 0) {
        fprintf(stderr, "Error writing bitcode to file\n");
        return 1;
    } else {
        printf("Bitcode written to %s\n", bitcodeFilename);
    }

    // Cleanup
    LLVMDisposeBuilder(builder);
    LLVMDisposeDIBuilder(diBuilder);
    LLVMDisposeModule(mod);

    return 0;  // Return success
}

#else

int llvm_test(string module) {
    symbol module_name = module->chars;

    LLVMModuleRef mod = LLVMModuleCreateWithName(module_name);
    LLVMContextRef context = LLVMGetModuleContext(mod);
    LLVMBuilderRef builder = LLVMCreateBuilder();
    
    // Create DIBuilder
    LLVMDIBuilderRef diBuilder = LLVMCreateDIBuilder(mod);

    // Set up compile unit
    const char *filename = "module.c";
    const char *directory = "/home/kalen/src/silver/silver-build";
    LLVMMetadataRef file = LLVMDIBuilderCreateFile(diBuilder, filename, strlen(filename), directory, strlen(directory));
    LLVMMetadataRef compileUnit = LLVMDIBuilderCreateCompileUnit(diBuilder, LLVMDWARFSourceLanguageC, file, "silver", 12, 0, "", 0, 0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);

    // Create struct type
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef memberTypes[] = {int32Type, int32Type, int32Type, int32Type};
    LLVMTypeRef structType = LLVMStructCreateNamed(context, "MyStruct");
    LLVMStructSetBody(structType, memberTypes, 4, 0);

    // Create debug info for struct
    LLVMMetadataRef memberDebugTypes[4];
    for (int i = 0; i < 4; i++) {
        memberDebugTypes[i] = LLVMDIBuilderCreateBasicType(diBuilder, "int", 3, 32, (LLVMDWARFTypeEncoding)0x05, (LLVMDIFlags)0); /// signed 0x05 (not defined somehwo) DW_ATE_signed
    }

    LLVMMetadataRef structDebugType = LLVMDIBuilderCreateStructType(
        diBuilder, compileUnit, "MyStruct", 8, file, 1, 
        128, 32, 0, NULL, 
        memberDebugTypes, 4, 0, NULL, "", 0);

    // Create main function
    LLVMTypeRef mainFuncType = LLVMFunctionType(LLVMInt32TypeInContext(context), NULL, 0, 0);
    LLVMValueRef mainFunc = LLVMAddFunction(mod, "main", mainFuncType);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(mainFunc, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    // Create debug info for main function
    LLVMMetadataRef funcDebugType = LLVMDIBuilderCreateSubroutineType(diBuilder, file, NULL, 0, 0);
    LLVMMetadataRef mainFuncDebugInfo = LLVMDIBuilderCreateFunction(
        diBuilder, compileUnit, "main", 4, "main", 4,
        file, 1, funcDebugType, 0, 1, 1, 0, 0);

    LLVMSetSubprogram(mainFunc, mainFuncDebugInfo);
    set_dbg_location(builder, mod, mainFuncDebugInfo, file, 3, 0);

    // Allocate struct
    LLVMValueRef selfPtr = LLVMBuildAlloca(builder, structType, "self");

    // Create debug info for self
    LLVMMetadataRef selfDebugInfo = LLVMDIBuilderCreateAutoVariable(
        diBuilder, mainFuncDebugInfo, "self", 4, file, 2, structDebugType, 0, 0, 0);
    LLVMDIBuilderInsertDeclareAtEnd(diBuilder, selfPtr, selfDebugInfo, 
        LLVMDIBuilderCreateExpression(diBuilder, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(context, 2, 0, mainFuncDebugInfo, NULL),
        LLVMGetInsertBlock(builder));

    // Set values for struct members
    const char *memberNames[] = {"member1", "member2", "member3", "member4"};
    int memberValues[] = {42, 44, 46, 48};

    for (int i = 0; i < 4; i++) {
        LLVMValueRef memberPtr = LLVMBuildStructGEP2(builder, structType, selfPtr, i, memberNames[i]);
        LLVMBuildStore(builder, LLVMConstInt(int32Type, memberValues[i], 0), memberPtr);

        // Create debug info for each member
        LLVMMetadataRef memberDebugInfo = LLVMDIBuilderCreateAutoVariable(
            diBuilder, mainFuncDebugInfo, memberNames[i], strlen(memberNames[i]),
            file, i + 3, memberDebugTypes[i], 0, 0, 0);
        LLVMDIBuilderInsertDeclareAtEnd(diBuilder, memberPtr, memberDebugInfo,
            LLVMDIBuilderCreateExpression(diBuilder, NULL, 0),
            LLVMDIBuilderCreateDebugLocation(context, i + 3, 0, mainFuncDebugInfo, NULL),
            LLVMGetInsertBlock(builder));

        set_dbg_location(builder, mod, mainFuncDebugInfo, file, 4 + i, 0);
    }

    // Return from main
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0));

    // Finalize debug info
    LLVMDIBuilderFinalize(diBuilder);

    // Verify module
    char *error = NULL;
    LLVMVerifyModule(mod, LLVMPrintMessageAction, &error);

    // verify module
    LLVMBool status = LLVMVerifyModule(mod, LLVMPrintMessageAction, &error);
    if (status) {
        fprintf(stderr, "Error verifying module: %s\n", error);
        LLVMDisposeMessage(error);
        return 1;  // Return with error
    } else {
        print("module verified");
    }

    // print generated IR
    cstr err = null;
    status = LLVMPrintModuleToFile(mod, "a.ll", &err);
    assert (!status);

    // generate bitcode file
    const char *bitcodeFilename = "a.bc";
    int write_bc = LLVMWriteBitcodeToFile(mod, bitcodeFilename);
    assert (write_bc == 0);

    // cleanup
    LLVMDisposeBuilder(builder);
    LLVMDisposeDIBuilder(diBuilder);
    LLVMDisposeModule(mod);
    return 0;
}

#endif

int main(int argc, char **argv) {
    A_start();
    AF   pool = alloc(AF);
    llvm_test(str("module"));
    drop(pool);
}