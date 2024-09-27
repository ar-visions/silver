#include <silver>

void generateLLVMModule() {

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    // Create a new LLVM context
    LLVMContextRef context2 = LLVMContextCreate();

    // Create a new module in the context
    LLVMModuleRef module2 = LLVMModuleCreateWithNameInContext("my_module", context2);

    // Set the target triple to the default for the host machine
    char *target_triple2 = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(module2, target_triple2);

    // Create a DIBuilder for generating debug information
    LLVMDIBuilderRef dbg2 = LLVMCreateDIBuilder(module2);

    // Create file and compile unit debug info
    const char *filename = "example.c";
    const char *directory = ".";
    LLVMMetadataRef file = LLVMDIBuilderCreateFile(
        dbg2, filename,
        strlen(filename),
        directory,
        strlen(directory));
    LLVMMetadataRef compileUnit = LLVMDIBuilderCreateCompileUnit(
        dbg2,
        LLVMDWARFSourceLanguageC, // Source language
        file,                     // File
        "My Compiler",            // Producer
        strlen("My Compiler"),
        0,                        // isOptimized
        "",                       // Flags
        0,                        // Flags length
        0,                        // Runtime version
        "",                       // Split name
        0,                        // Split name length
        LLVMDWARFEmissionFull,    // Emission kind
        0,                        // DWO ID
        0,                        // Split debug inline
        0,                        // Debug info for profiling
        "",                       // SysRoot
        0,                        // SysRoot length
        "",                       // SDK
        0                         // SDK length
    );

    // Create a function type: int main()
    LLVMTypeRef returnType = LLVMInt32TypeInContext(context2);
    LLVMTypeRef paramTypes[] = {}; // No parameters
    LLVMTypeRef funcType = LLVMFunctionType(returnType, paramTypes, 0, 0);

    // Create the function and add it to the module
    LLVMValueRef function = LLVMAddFunction(module2, "main", funcType);

    // Set function linkage
    LLVMSetLinkage(function, LLVMExternalLinkage);

    // Create function debug info
    LLVMMetadataRef funcTypeMeta = LLVMDIBuilderCreateSubroutineType(
        dbg2,
        file,              // Scope (file)
        NULL,              // Parameter types (None for simplicity)
        0,                 // Number of parameters
        LLVMDIFlagZero     // Flags
    );

    LLVMMetadataRef functionMeta = LLVMDIBuilderCreateFunction(
        dbg2,
        file,                   // Scope (file)
        "main",                 // Name
        strlen("main"),
        "main",                 // Linkage name (same as name)
        strlen("main"),
        file,                   // File
        1,                      // Line number
        funcTypeMeta,           // Function type
        1,                      // Is local to unit
        1,                      // Is definition
        1,                      // Scope line
        LLVMDIFlagZero,         // Flags
        0                       // Is optimized
    );

    // Attach debug info to function
    LLVMSetSubprogram(function, functionMeta);

    // Create a basic block in the function
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context2, function, "entry");

    // Create a builder and position it at the end of the basic block
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context2);
    LLVMPositionBuilderAtEnd(builder2, entry);

    // Set current debug location
    LLVMMetadataRef debugLoc = LLVMDIBuilderCreateDebugLocation(
        context2,
        2,           // Line number
        1,           // Column number
        functionMeta,// Scope
        NULL         // InlinedAt
    );
    LLVMSetCurrentDebugLocation2(builder2, debugLoc);

    // Declare the local variable 'something' of type i64 and assign it the value 1
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context2);
    LLVMValueRef somethingAlloc = LLVMBuildAlloca(builder2, i64Type, "something");

    // Create debug info for the variable
    LLVMMetadataRef intTypeMeta = LLVMDIBuilderCreateBasicType(
        dbg2,
        "int64",            // Type name
        strlen("int64"),
        64,                 // Size in bits
        0x05, // Encoding
        LLVMDIFlagZero
    );

    LLVMMetadataRef variableMeta = LLVMDIBuilderCreateAutoVariable(
        dbg2,
        functionMeta,       // Scope
        "something",        // Variable name
        strlen("something"),
        file,               // File
        2,                  // Line number
        intTypeMeta,        // Type metadata
        0,                  // Always preserved
        LLVMDIFlagZero,     // Flags
        0                   // Alignment
    );

    // Insert a debug declaration for the variable
    LLVMDIBuilderInsertDeclareAtEnd(
        dbg2,
        somethingAlloc,
        variableMeta,
        LLVMDIBuilderCreateExpression(dbg2, NULL, 0),
        debugLoc,
        entry // Insert at the end of the entry block
    );

    // Assign the value 1 to 'something'
    LLVMValueRef constOne = LLVMConstInt(i64Type, 1, 0);
    LLVMBuildStore(builder2, constOne, somethingAlloc);

    // Set current debug location for store instruction
    LLVMSetCurrentDebugLocation2(builder2, debugLoc);

    // Declare the printf function (variadic function)
    LLVMTypeRef printfParamTypes[] = { LLVMPointerType(LLVMInt8TypeInContext(context2), 0) }; // const char*
    LLVMTypeRef printfFuncType = LLVMFunctionType(
        LLVMInt32TypeInContext(context2),
        printfParamTypes,
        1,
        1 // Is variadic
    );
    LLVMValueRef printfFunc = LLVMAddFunction(module2, "printf", printfFuncType);
    LLVMSetLinkage(printfFunc, LLVMExternalLinkage);

    // Create format string for printf
    LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(builder2, "Value: %ld\n", "formatStr");

    // Load the value of 'something'
    LLVMValueRef somethingValue = LLVMBuildLoad2(builder2, i64Type, somethingAlloc, "load_something");

    // Set current debug location for load instruction
    LLVMSetCurrentDebugLocation2(builder2, debugLoc);

    // Set up arguments for printf
    LLVMValueRef printfArgs[] = { formatStr, somethingValue };

    // Build call to printf
    LLVMValueRef call = LLVMBuildCall2(
        builder2,
        printfFuncType,  // Function type
        printfFunc,      // Function to call
        printfArgs,      // Arguments
        2,               // Number of arguments
        "call_printf"    // Name of the call
    );

    // Set call site debug location
    LLVMSetCurrentDebugLocation2(builder2, debugLoc);

    // Build return instruction
    LLVMBuildRet(builder2, LLVMConstInt(LLVMInt32TypeInContext(context2), 0, 0));

    // Finalize the debug builder
    LLVMDIBuilderFinalize(dbg2);

    // Verify the module
    char *error = NULL;
    if (LLVMVerifyModule(module2, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "Error verifying module2: %s\n", error);
        LLVMDisposeMessage(error);
        exit(1);
    }

    // Optionally, write the module to a file
    if (LLVMPrintModuleToFile(module2, "output.ll", &error) != 0) {
        fprintf(stderr, "Error writing module2 to file: %s\n", error);
        LLVMDisposeMessage(error);
        exit(1);
    }

    // Clean up
    LLVMDisposeBuilder(builder2);
    LLVMDisposeDIBuilder(dbg2);
    LLVMDisposeModule(module2);
    LLVMContextDispose(context2);
    LLVMDisposeMessage(target_triple2);
}

int main(int argc, char **argv) {
    A_start();
    AF         pool = allocate(AF);
    cstr        src = getenv("SRC");
    cstr     import = getenv("SILVER_IMPORT");
    map    defaults = map_of(
        "module",  str(""),
        "install", import ? form(path, "%s", import) : 
                            form(path, "%s/silver-import", src ? src : "."),
        null);
    print("defaults = %o", defaults);

    string ikey     = str("install");
    map    args     = A_args(argc, argv, defaults, ikey);
    print("args = %o", args);

    path   install  = get(args, ikey);
    string mkey     = str("module");
    string name     = get(args, mkey);
    path   n        = new(path, chars, name->chars);
    path   source   = call(n, absolute);

    assert (call(source, exists), "source %o does not exist", n);
    
    if (true) {
        silver module = new(silver, source, source, install, install);
    } else
        generateLLVMModule();

    drop(pool);
    // we only use silver to build the library for .c, of course.  this was our use-case
}