#define _GNU_SOURCE

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <ports.h>
#include <stddef.h>

typedef LLVMMetadataRef LLVMScope;

#include <aether/import>

// --- LLDB/DWARF debug info helpers ---

#define B a->builder

// coverage probe - one per statements block, just tracks hit count
typedef struct _coverage_probe {
    u64     hit_count;      // Number of times block was entered
    u32     line;           // Start line of block
    u16     column;         // Start column
    u16     _pad;
} coverage_probe;

// function timing data (separate from coverage)
typedef struct _func_timing {
    symbol  name;           // Function name
    symbol  file;           // Source file
    u32     line;           // Definition line
    u64     call_count;     // Number of calls
    u64     total_ns;       // Total time in function
    u64     min_ns;         // Min single call (optional)
    u64     max_ns;         // Max single call (optional)
} func_timing;

// module coverage data
typedef struct _coverage_module {
    symbol              name;
    symbol              source_path;
    u32                 probe_count;        // Number of statements blocks
    u32                 func_count;         // Number of timed functions
    u32                 covered_count;      // Blocks with hit_count > 0
    coverage_probe*     probes;             // Array indexed by statements->probe_id
    func_timing*        timings;            // Array indexed by efunc->timing_func_id
    struct _coverage_module* next;
} coverage_module;



// emit probe when entering a statements block
// called from statements initialization or push_scope
void aether_emit_block_probe(aether a, u32 probe_id) {
    if (!a->coverage) return;
    if (a->no_build) return;
    
    // Lazily declare the runtime function (must be set to zero on reinit)
    if (!a->coverage_hit_fn) {
        LLVMTypeRef fn_type = LLVMFunctionType(
            LLVMVoidTypeInContext(a->module_ctx),
            (LLVMTypeRef[]){ LLVMInt32TypeInContext(a->module_ctx) },
            1, false);
        a->coverage_hit_fn = LLVMAddFunction(
            a->module_ref, "__coverage_hit", fn_type);
    }
    
    LLVMValueRef args[1] = {
        LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), probe_id, 0)
    };
    LLVMBuildCall2(B, LLVMGlobalGetValueType(a->coverage_hit_fn),
                   a->coverage_hit_fn, args, 1, "");
}

// read clock_gettime(CLOCK_MONOTONIC) and return nanosecond timestamp
LLVMValueRef emit_clock_ns(aether a, cstr label) {
    LLVMTypeRef i64t = LLVMInt64TypeInContext(a->module_ctx);
    LLVMTypeRef timespec_type = LLVMStructTypeInContext(
        a->module_ctx, (LLVMTypeRef[]){ i64t, i64t }, 2, false);
    LLVMValueRef ts = LLVMBuildAlloca(B, timespec_type, label);
    LLVMBuildCall2(B, a->clock_gettime_type, a->clock_gettime_fn,
        (LLVMValueRef[]){
            LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), 1, 0), ts
        }, 2, "");
    LLVMValueRef sec  = LLVMBuildLoad2(B, i64t,
        LLVMBuildStructGEP2(B, timespec_type, ts, 0, ""), "sec");
    LLVMValueRef nsec = LLVMBuildLoad2(B, i64t,
        LLVMBuildStructGEP2(B, timespec_type, ts, 1, ""), "nsec");
    LLVMValueRef billion = LLVMConstInt(i64t, 1000000000ULL, 0);
    return LLVMBuildAdd(B, LLVMBuildMul(B, sec, billion, ""), nsec, label);
}

// emit timing start (returns nanosecond timestamp) - FUNCTION LEVEL ONLY
LLVMValueRef emit_func_timing_start(aether a, u32 func_id) {
    if (!a->timing_enabled || a->no_build) return null;
    return emit_clock_ns(a, "start_ns");
}

// emit timing end and accumulate elapsed time - FUNCTION LEVEL ONLY
void emit_func_timing_end(aether a, LLVMValueRef start_ns, u32 func_id) {
    if (!a->timing_enabled || !start_ns || a->no_build) return;

    LLVMValueRef end_ns  = emit_clock_ns(a, "end_ns");
    LLVMValueRef elapsed = LLVMBuildSub(B, end_ns, start_ns, "elapsed_ns");

    // Call runtime: __coverage_record_time(func_id, elapsed_ns)
    if (!a->coverage_record_time_fn) {
        LLVMTypeRef fn_type = LLVMFunctionType(
            LLVMVoidTypeInContext(a->module_ctx),
            (LLVMTypeRef[]){
                LLVMInt32TypeInContext(a->module_ctx),
                LLVMInt64TypeInContext(a->module_ctx)
            }, 2, false);
        a->coverage_record_time_fn = LLVMAddFunction(
            a->module_ref, "__coverage_record_time", fn_type);
    }
    LLVMBuildCall2(B, LLVMGlobalGetValueType(a->coverage_record_time_fn),
                   a->coverage_record_time_fn,
                   (LLVMValueRef[]){
                       LLVMConstInt(LLVMInt32TypeInContext(a->module_ctx), func_id, 0),
                       elapsed
                   }, 2, "");
}

// this looks leaky, but isnt
void report_coverage(aether a) {
    if (!a->coverage) return;

    // create function type
    LLVMTypeRef fn_type = LLVMFunctionType(
        LLVMVoidTypeInContext(a->module_ctx), null, 0, false);
    
    // add function
    LLVMValueRef __coverage_report = LLVMAddFunction(
        a->module_ref, "__coverage_report", fn_type);

    // build it
    LLVMBuildCall2(
        B, fn_type, __coverage_report, null, 0, "");
}

// this works fine when re-initializing
void init_coverage(aether a) {
    if (!a->coverage) return;
    
    // Coverage probe struct: { u64 hit_count, u32 line, u16 column, u16 _pad }
    a->coverage_probe_lltype = LLVMStructTypeInContext(
        a->module_ctx,
        (LLVMTypeRef[]){
            LLVMInt64TypeInContext(a->module_ctx),  // hit_count
            LLVMInt32TypeInContext(a->module_ctx),  // line
            LLVMInt16TypeInContext(a->module_ctx),  // column
            LLVMInt16TypeInContext(a->module_ctx)   // _pad
        },
        4, false
    );
    
    a->next_probe_id = 0;
    
    // Function timing setup - we just need clock_gettime, no global array yet
    if (a->timing_enabled) {
        a->next_func_id = 0;
        
        // Declare clock_gettime
        LLVMTypeRef timespec_ptr = LLVMPointerTypeInContext(a->module_ctx, 0);
        a->clock_gettime_type = LLVMFunctionType(
            LLVMInt32TypeInContext(a->module_ctx),
            (LLVMTypeRef[]){ LLVMInt32TypeInContext(a->module_ctx), timespec_ptr },
            2, false
        );
        a->clock_gettime_fn = LLVMAddFunction(
            a->module_ref, "clock_gettime", a->clock_gettime_type
        );
        
        // func_timing struct type (for the global array we create at finalize)
        a->func_timing_lltype = LLVMStructTypeInContext(
            a->module_ctx,
            (LLVMTypeRef[]){
                LLVMPointerTypeInContext(a->module_ctx, 0),  // name
                LLVMPointerTypeInContext(a->module_ctx, 0),  // file
                LLVMInt32TypeInContext(a->module_ctx),       // line
                LLVMInt64TypeInContext(a->module_ctx),       // call_count
                LLVMInt64TypeInContext(a->module_ctx),       // total_ns
                LLVMInt64TypeInContext(a->module_ctx),       // min_ns
                LLVMInt64TypeInContext(a->module_ctx)        // max_ns
            },
            7, false
        );
    }
}

// called in module_initializer
void finalize_coverage(aether a) {
    if (!a->coverage) return;
    
    if (a->next_probe_id > 0) {
        LLVMTypeRef array_type = LLVMArrayType(a->coverage_probe_lltype, a->next_probe_id);
        a->coverage_probes_global = LLVMAddGlobal(a->module_ref, array_type,
            fmt("__cov_probes_%s", a->name->chars)->chars);
        LLVMSetInitializer(a->coverage_probes_global, LLVMConstNull(array_type));
        LLVMSetLinkage(a->coverage_probes_global, LLVMInternalLinkage);
    }
    
    if (a->timing_enabled && a->next_func_id > 0) {
        LLVMTypeRef array_type = LLVMArrayType(a->func_timing_lltype, a->next_func_id);
        a->func_timings_global = LLVMAddGlobal(a->module_ref, array_type,
            fmt("__func_timings_%s", a->name->chars)->chars);
        LLVMSetInitializer(a->func_timings_global, LLVMConstNull(array_type));
        LLVMSetLinkage(a->func_timings_global, LLVMInternalLinkage);
    }
}