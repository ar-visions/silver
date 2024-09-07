#include <silver>
#include <tokens>

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>

// Example struct
typedef struct {
    const char* name;
    int another;
    const char* third;
} MyStruct;

// Define an empty function to use as the sentinel
void sentinel_function() {}

// Use the address of the sentinel function as the sentinel
#define typed_sentinel_here (&sentinel_function)

// Macro to handle named arguments with a sentinel
#define ctr2(type, ...) construct_impl((type){0}, __VA_ARGS__, typed_sentinel_here)

// Recursive macro to assign values to the correct struct members
#define ctr2_impl(obj, member, value, ...) \
    ({ \
        if (&member != typed_sentinel_here) { \
            obj.member = value; \
            if (&__VA_ARGS__ != typed_sentinel_here) { \
                construct_impl(obj, __VA_ARGS__); \
            } \
        } \
        obj; \
    })

typedef struct isilver {
    LLVMModuleRef       module;
    LLVMContextRef      llvm_context;
    LLVMBuilderRef      builder;
    LLVMMetadataRef     file;
    LLVMMetadataRef     compile_unit;
    LLVMDIBuilderRef    dbg;
    LLVMMetadataRef     scope;
    map                 imports;
    string              source_file;
    string              source_path;
    array               tokens;
} isilver;

typedef struct ifunction {
    LLVMTypeRef         fn_type;
    LLVMValueRef        fn;
    LLVMMetadataRef     sub_routine;
    LLVMMetadataRef     dbg;
    LLVMBasicBlockRef   entry;
} ifunction;

#define icall(I,N,...) silver_##N(I, ## __VA_ARGS__)

/// reference is a good wrapper for AType, it helps distinguish its usage in silver's design-time
/// flatten out references on construction
reference reference_with_AType(reference a, AType type, num refs, silver module) {
    a->type     = type;
    a->module   = module;
    reference r = a;
    while (r) {
        A f = A_fields(r);
        if (f->type) { /// if the object has a type then its a reference
            a->refs += r->refs;
            r        = r->type;
        } else {
            if (r != a) a->type = r;
            break;
        }
    }
    a->refs += refs;
    return a;
}

LLVMTypeRef reference_llvm(reference a) {
    LLVMTypeRef t = null;
    AType      at = a->type;
         if (at == typeid(bool)) t = LLVMInt1Type();
    else if (at == typeid(i8))   t = LLVMInt8Type();
    else if (at == typeid(i16))  t = LLVMInt16Type();
    else if (at == typeid(i32))  t = LLVMInt32Type();
    else if (at == typeid(i64))  t = LLVMInt64Type();
    else if (at == typeid(u8))   t = LLVMInt8Type();
    else if (at == typeid(u16))  t = LLVMInt16Type();
    else if (at == typeid(u32))  t = LLVMInt32Type();
    else if (at == typeid(u64))  t = LLVMInt64Type();
    else if (at == typeid(f32))  t = LLVMFloatType();
    else if (at == typeid(f64))  t = LLVMDoubleType();
    else {
        ///
        assert(false);
    }
    for (int i = 0; i < a->refs; i++)
        t = LLVMPointerType(t, 0);
    return t;
}

function function_with_cstr(function a, cstr name, silver module, reference rtype, map args) {
    isilver*     i = module->intern;
    ifunction*   f = a->intern; 
    sz         len = strlen(name);
    f->fn_type     = LLVMFunctionType(reference_llvm(rtype), NULL, 0, false);
    f->fn          = LLVMAddFunction(i->module, name, f->fn_type);
    f->sub_routine = LLVMDIBuilderCreateSubroutineType(i->dbg, i->file, NULL, 0, 0);
    f->dbg         = LLVMDIBuilderCreateFunction(
        i->dbg, i->compile_unit, name, len, name, len,
        i->file, 1, f->sub_routine, 0, 1, 1, 0, LLVMDIFlagPrototyped);
    LLVMSetSubprogram(f->fn, f->dbg);
}

void silver_set_line(silver a, i32 line, i32 column) {
    isilver* i = a->intern;
    LLVMMetadataRef loc = LLVMDIBuilderCreateDebugLocation(
        i->llvm_context, line, column, i->scope, null);
    LLVMSetCurrentDebugLocation2(i->dbg, loc);
}

void silver_llflag(silver a, symbol flag, i32 ival) {
    isilver*        i = a->intern;
    LLVMMetadataRef v = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(i->llvm_context), ival, 0));
    LLVMAddModuleFlag(
        i->module, LLVMModuleFlagBehaviorError, flag, strlen(flag), v);
}

void silver_write(silver a) {
    isilver* i = a->intern;
    cstr err = NULL;
    if (LLVMVerifyModule(i->module, LLVMPrintMessageAction, &err))
        fault("Error verifying module: %s", err);
    else
        print("module verified");

    if (!LLVMPrintModuleToFile(i->module, "a.ll", &err))
        print("generated IR");
    else
        fault("LLVMPrintModuleToFile failed");

    symbol bc = "a.bc";
    if (LLVMWriteBitcodeToFile(i->module, bc) != 0)
        fault("LLVMWriteBitcodeToFile failed");
    else
        print("bitcode written to %s", bc);
}

void silver_destructor(silver a) {
    isilver* i = a->intern;
    LLVMDisposeBuilder(i->builder);
    LLVMDisposeDIBuilder(i->dbg);
    LLVMDisposeModule(i->module);
}

LLVMValueRef function_dbg(function fn) {
    ifunction* f = fn->intern;
    return f->dbg;
}

void silver_parse_top(silver a) {
    isilver* i = a->intern;
    each(array, i->tokens, Token, token) {
        
    }
}

void silver_init(silver a) {
    isilver* i = a->intern;

    print("LLVM Version: %d.%d.%d",
        LLVM_VERSION_MAJOR,
        LLVM_VERSION_MINOR,
        LLVM_VERSION_PATCH);

    path full_path = cast(format("%o/%o", i->source_path, i->source_file), path);
    assert(call(full_path, exists));

    i->module       = LLVMModuleCreateWithName(i->source_file->chars);
    i->llvm_context = LLVMGetModuleContext(i->module);
    i->dbg          = LLVMCreateDIBuilder(i->module);

    icall(a, llflag, "Dwarf Version",      5);
    icall(a, llflag, "Debug Info Version", 3);

    i->file = LLVMDIBuilderCreateFile( // create a file reference (the source file for debugging)
        i->dbg,
        cast(i->source_file, cstr),
        cast(i->source_file, sz),
        cast(i->source_path, cstr),
        cast(i->source_path, sz));
    
    i->compile_unit = LLVMDIBuilderCreateCompileUnit(
        i->dbg, LLVMDWARFSourceLanguageC, i->file,
        cast(i->source_file, cstr),
        cast(i->source_file, sz), 0, "", 0,
        3, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "/", 1, "", 0);

    i->tokens = parse_tokens(full_path);

    // define a struct type in the custom language
    LLVMTypeRef structType = LLVMStructCreateNamed(i->llvm_context, "MyStruct");
    LLVMTypeRef elementTypes[] = { LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type(), LLVMInt32Type() }; // Member: int member
    LLVMStructSetBody(structType, elementTypes, 4, 0);

    // create debug info for struct
    LLVMMetadataRef memberDebugTypes[4];
    for (int m = 0; m < 4; m++) {
        memberDebugTypes[m] = LLVMDIBuilderCreateBasicType(
            i->dbg, "int", 3, 32, 
            (LLVMDWARFTypeEncoding)0x05,
            (LLVMDIFlags)0); /// signed 0x05 (not defined somehwo) DW_ATE_signed
    }

    LLVMMetadataRef structDebugType = LLVMDIBuilderCreateStructType(
        i->dbg, i->compile_unit, "MyStruct", 8, i->file, 1, 
        128, 32, 0, NULL, 
        memberDebugTypes, 4, 0, NULL, "", 0);

    // create a function and access the struct member
    map args = new(map);
    call(args, set, str("argc"), ref(a, typeid(i32),  0));
    call(args, set, str("argv"), ref(a, typeid(cstr), 1));
    function  fn = ctr(function, cstr, "main", a, ref(a, typeid(i32), 0), args);

    //call(fn, from_tokens) -- lets import parse_statements -> parse_expression

    LLVMBuilderRef builder = LLVMCreateBuilder();

    /// this is 'finalize' for a method, after we call parse on module, parsing all members in each class or struct

    // Create a pointer to MyStruct (simulate `self` in your custom language)
    LLVMValueRef structPtr = LLVMBuildAlloca(builder, structType, "self");

    // Create debug info for the local variable
    LLVMMetadataRef localVarDIType = LLVMDIBuilderCreatePointerType(
        i->dbg, structDebugType, 64, 0, 0, "MyStruct*", 9);
    LLVMMetadataRef localVarDbgInfo = LLVMDIBuilderCreateAutoVariable(
        i->dbg, function_dbg(fn), "self", 4, i->file, 2, localVarDIType, 1, 0, 0);
    
    LLVMDIBuilderInsertDeclareAtEnd(
        i->dbg,
        structPtr,
        localVarDbgInfo,
        LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
        LLVMDIBuilderCreateDebugLocation(i->llvm_context, 2, 0, function_dbg(fn), NULL),
        LLVMGetInsertBlock(builder));

    // Set values for struct members
    symbol memberNames[]  = { "member1", "member2", "member3", "member4" };
    int    memberValues[] = { 42, 44, 46, 48 };
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(i->llvm_context);

    for (int m = 0; m < 4; m++) {
        LLVMValueRef memberPtr = LLVMBuildStructGEP2(builder, structType, structPtr, m, memberNames[m]);
        LLVMBuildStore(builder, LLVMConstInt(int32Type, memberValues[m], 0), memberPtr);

        // Create debug info for each member
        LLVMMetadataRef memberDebugInfo = LLVMDIBuilderCreateAutoVariable(
            i->dbg, function_dbg(fn), memberNames[m], strlen(memberNames[m]),
            i->file, m + 3, memberDebugTypes[m], 0, 0, 0);
        LLVMDIBuilderInsertDeclareAtEnd(i->dbg, memberPtr, memberDebugInfo,
            LLVMDIBuilderCreateExpression(i->dbg, NULL, 0),
            LLVMDIBuilderCreateDebugLocation(i->llvm_context, m + 3, 0, function_dbg(fn), NULL),
            LLVMGetInsertBlock(builder));

        icall(a, set_line, 3 + m, 0);
    }
    icall(a, set_line, 7, 0);

    // Return from main
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, false));
    LLVMDIBuilderFinalize(i->dbg);
    icall(a, write);
}

silver silver_with_string(silver a, string module) {
    a->intern  = A_struct(silver);
    isilver* i = a->intern;
    
    i->source_path = str("/home/kalen/src/silver/silver-build");
    i->source_file = format("%o.c", module);
    return a;
}

define_class(reference)
define_class(function)
define_class(silver)
