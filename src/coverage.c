#define _GNU_SOURCE

#include <llvm-c/DebugInfo.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <posix.h>
#include <stddef.h>
#include <time.h>

typedef LLVMMetadataRef LLVMScope;

#include <aether/import>

// serialize on aether's single-context emission lock
void aether_emit_lock();
void aether_emit_unlock();
static void _emit_guard_end(char* g) { (void)g; aether_emit_unlock(); }
#define emit_guard __attribute__((cleanup(_emit_guard_end))) \
    char _emit_g_ = (aether_emit_lock(), (char)0); (void)_emit_g_

// --- LLDB/DWARF debug info helpers ---

#define B a->builder
#define MAX_FUNCS 512
#define MAX_PROBES  4096
#define TRACE_SIZE  1024
#define MAX_COV_FILES 64

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


// names live on the root: cores share the id space, modules do not
void coverage_set_func_name(aether a, u32 func_id, char* name) {
    aether r = a->root ? a->root : a;
    if (!r->func_name_table) r->func_name_table = calloc(MAX_FUNCS, sizeof(char*));
    if (func_id < MAX_FUNCS)
        r->func_name_table[func_id] = name;
}

// the names array is registered empty at init and filled here, once every
// core has emitted its functions and taken its ids
AU_EXPORT void finalize_timing_names(aether a) {
    aether r = a->root ? a->root : a;
    if (!r->timing || !r->func_names_global) return;
    emit_guard;
    LLVMTypeRef  ptr_type = LLVMPointerTypeInContext(r->module_ctx, 0);
    LLVMValueRef* vals = calloc(MAX_FUNCS, sizeof(LLVMValueRef));
    for (u32 i = 0; i < MAX_FUNCS; i++) {
        char* nm = r->func_name_table ? r->func_name_table[i] : null;
        vals[i] = nm ? LLVMConstPointerCast(LLVMBuildGlobalStringPtr(r->builder, nm, ""), ptr_type)
                     : LLVMConstNull(ptr_type);
    }
    LLVMSetInitializer(r->func_names_global, LLVMConstArray(ptr_type, vals, MAX_FUNCS));
    free(vals);
}

void emit_coverage_register(aether a) {
    emit_guard;
    if ((!a->coverage && !a->timing) || a->no_build) return;

    LLVMTypeRef ptr_type = LLVMPointerTypeInContext(a->module_ctx, 0);
    LLVMTypeRef i32_type = LLVMInt32TypeInContext(a->module_ctx);

    LLVMTypeRef fn_type = LLVMFunctionType(
        LLVMVoidTypeInContext(a->module_ctx),
        (LLVMTypeRef[]){ ptr_type, i32_type, ptr_type, i32_type, ptr_type },
        5, false);
    LLVMValueRef coverage_register_fn = LLVMAddFunction(
        a->module_ref, "__coverage_register", fn_type);

    LLVMValueRef timings = a->func_timings_global
        ? a->func_timings_global
        : LLVMConstNull(ptr_type);

    // the names array is fixed-size and filled at finalize: functions
    // are still being emitted (by every core) when this initializer runs
    LLVMValueRef names = LLVMConstNull(ptr_type);
    if (a->timing) {
        LLVMTypeRef names_array = LLVMArrayType(ptr_type, MAX_FUNCS);
        a->func_names_global = LLVMAddGlobal(a->module_ref, names_array,
            fmt("__func_names_%s", a->name->chars)->chars);
        LLVMSetInitializer(a->func_names_global, LLVMConstNull(names_array));
        LLVMSetLinkage(a->func_names_global, LLVMInternalLinkage);
        names = a->func_names_global;
    }

    LLVMValueRef probes = a->coverage_probes_global
        ? a->coverage_probes_global
        : LLVMConstNull(ptr_type);
    LLVMBuildCall2(B, LLVMGlobalGetValueType(coverage_register_fn),
        coverage_register_fn,
        (LLVMValueRef[]){
            probes,
            LLVMConstInt(i32_type, a->next_probe_id, 0),
            timings,
            LLVMConstInt(i32_type, a->timing ? MAX_FUNCS : 0, 0),
            names
        }, 5, "");
    if (!a->coverage) return;

    // map and count are filled at finalize, after the cores
    LLVMTypeRef map_type   = LLVMArrayType(i32_type, MAX_PROBES * 4);
    LLVMTypeRef files_type = LLVMArrayType(ptr_type, MAX_COV_FILES);
    a->coverage_map_global = LLVMAddGlobal(a->module_ref, map_type,
        fmt("__cov_map_%s", a->name->chars)->chars);
    LLVMSetInitializer(a->coverage_map_global, LLVMConstNull(map_type));
    LLVMSetLinkage(a->coverage_map_global, LLVMInternalLinkage);
    a->coverage_count_global = LLVMAddGlobal(a->module_ref, i32_type,
        fmt("__cov_count_%s", a->name->chars)->chars);
    LLVMSetInitializer(a->coverage_count_global, LLVMConstInt(i32_type, 0, 0));
    LLVMSetLinkage(a->coverage_count_global, LLVMInternalLinkage);
    a->coverage_files_global = LLVMAddGlobal(a->module_ref, files_type,
        fmt("__cov_files_%s", a->name->chars)->chars);
    LLVMSetInitializer(a->coverage_files_global, LLVMConstNull(files_type));
    LLVMSetLinkage(a->coverage_files_global, LLVMInternalLinkage);

    LLVMTypeRef lines_type = LLVMFunctionType(
        LLVMVoidTypeInContext(a->module_ctx),
        (LLVMTypeRef[]){ ptr_type, ptr_type, i32_type, ptr_type, i32_type }, 5, false);
    LLVMValueRef lines_fn = LLVMAddFunction(a->module_ref, "__coverage_lines", lines_type);
    LLVMValueRef count = LLVMBuildLoad2(B, i32_type, a->coverage_count_global, "cov_count");
    LLVMBuildCall2(B, lines_type, lines_fn,
        (LLVMValueRef[]){ probes, a->coverage_map_global, count,
            a->coverage_files_global, LLVMConstInt(i32_type, MAX_COV_FILES, 0) }, 5, "");
}

// every core has emitted: the line map is complete
AU_EXPORT void finalize_coverage_map(aether a) {
    aether r = a->root ? a->root : a;
    if (!r->coverage || !r->coverage_map_global) return;
    emit_guard;
    LLVMContextRef c    = r->module_ctx;
    LLVMTypeRef    i32t = LLVMInt32TypeInContext(c);
    LLVMTypeRef    ptrt = LLVMPointerTypeInContext(c, 0);
    u32  count = r->next_probe_id < MAX_PROBES ? (u32)r->next_probe_id : MAX_PROBES;
    u32* lines = (u32*)r->coverage_lines;
    LLVMValueRef* vals = calloc(MAX_PROBES * 4, sizeof(LLVMValueRef));
    for (u32 i = 0; i < MAX_PROBES * 4; i++)
        vals[i] = LLVMConstInt(i32t, (lines && i < count * 4) ? lines[i] : 0, 0);
    LLVMSetInitializer(r->coverage_map_global, LLVMConstArray(i32t, vals, MAX_PROBES * 4));
    free(vals);
    LLVMSetInitializer(r->coverage_count_global, LLVMConstInt(i32t, count, 0));
    LLVMValueRef fvals[MAX_COV_FILES];
    for (i32 i = 0; i < MAX_COV_FILES; i++) {
        cstr nm = (i < r->coverage_file_count) ? r->coverage_files[i] : null;
        if (!nm) { fvals[i] = LLVMConstNull(ptrt); continue; }
        LLVMValueRef str = LLVMConstStringInContext(c, nm, (u32)strlen(nm), false);
        LLVMValueRef g   = LLVMAddGlobal(r->module_ref, LLVMTypeOf(str), "__cov_file");
        LLVMSetInitializer(g, str);
        LLVMSetLinkage(g, LLVMPrivateLinkage);
        LLVMSetGlobalConstant(g, true);
        fvals[i] = g;
    }
    LLVMSetInitializer(r->coverage_files_global, LLVMConstArray(ptrt, fvals, MAX_COV_FILES));
}

// a core's function names the root's global by declaring it
static LLVMValueRef cov_global(aether a, LLVMValueRef g, LLVMTypeRef t) {
    LLVMModuleRef m = LLVMGetGlobalParent(LLVMGetBasicBlockParent(LLVMGetInsertBlock(B)));
    cstr name = LLVMGetValueName(g);
    LLVMValueRef mg = LLVMGetNamedGlobal(m, name);
    return mg ? mg : LLVMAddGlobal(m, t, name);
}

// the run's block sequence, as the current module names it
AU_EXPORT LLVMValueRef coverage_seq_ref(aether a) {
    aether r = a->root ? a->root : a;
    if (!r->coverage_seq_global) return null;
    return cov_global(a, r->coverage_seq_global, LLVMInt64TypeInContext(a->module_ctx));
}

static u32 coverage_file_index(aether r, cstr s) {
    if (!r->coverage_files) r->coverage_files = calloc(MAX_COV_FILES, sizeof(char*));
    for (i32 i = 0; i < r->coverage_file_count; i++)
        if (strcmp(r->coverage_files[i], s) == 0) return (u32)i;
    if (r->coverage_file_count >= MAX_COV_FILES) return MAX_COV_FILES - 1;
    r->coverage_files[r->coverage_file_count] = strdup(s);
    return (u32)r->coverage_file_count++;
}

// ids come from the root: every core shares one probe table
AU_EXPORT u32 coverage_probe_open(aether a, token t) {
    emit_guard;
    aether r = a->root ? a->root : a;
    u32 id = (u32)r->next_probe_id++;
    if (id >= MAX_PROBES) return id;
    if (!r->coverage_lines) r->coverage_lines = calloc(MAX_PROBES * 4, sizeof(u32));
    // the function holds its file; a token's source is weak
    efunc fn   = aether_context_func(a);
    path  file = (fn && fn->source_file) ? fn->source_file : t->source;
    cstr  name = file ? ((string)file)->chars : "";
    u32* e = (u32*)r->coverage_lines + id * 4;
    e[0] = coverage_file_index(r, name);
    e[1] = (u32)t->line;
    e[2] = (u32)t->line;
    e[3] = (u32)t->column;
    return id;
}

AU_EXPORT void coverage_probe_close(aether a, u32 probe_id, u32 end_line) {
    emit_guard;
    aether r = a->root ? a->root : a;
    if (probe_id >= MAX_PROBES || !r->coverage_lines) return;
    u32* e = (u32*)r->coverage_lines + probe_id * 4;
    if (end_line > e[2]) e[2] = end_line;
}

// emit probe when entering a statements block
// called from statements initialization or push_scope
void aether_emit_block_probe(aether a, u32 probe_id) {
    emit_guard;
    if (!a->coverage) return;
    if (a->no_build) return;
    if (probe_id >= MAX_PROBES) return;

    LLVMTypeRef i64t = LLVMInt64TypeInContext(a->module_ctx);
    LLVMTypeRef i32t = LLVMInt32TypeInContext(a->module_ctx);
    aether      r    = a->root ? a->root : a;
    LLVMTypeRef parr = LLVMArrayType(i64t, MAX_PROBES);
    LLVMValueRef probes = cov_global(a, r->coverage_probes_global, parr);
    LLVMValueRef seq    = cov_global(a, r->coverage_seq_global, i64t);

    // increment global sequence counter
    LLVMValueRef seq_val = LLVMBuildLoad2(B, i64t, seq, "seq");
    LLVMValueRef seq_inc = LLVMBuildAdd(B, seq_val,
        LLVMConstInt(i64t, 1, 0), "seq_inc");
    LLVMBuildStore(B, seq_inc, seq);

    // store block hit count into probes[probe_id]
    LLVMValueRef probe_gep = LLVMBuildGEP2(B, parr, probes,
        (LLVMValueRef[]){
            LLVMConstInt(i32t, 0, 0),
            LLVMConstInt(i32t, probe_id, 0)
        }, 2, "probe_ptr");
    LLVMValueRef hit = LLVMBuildLoad2(B, i64t, probe_gep, "hit");
    LLVMValueRef hit_inc = LLVMBuildAdd(B, hit,
        LLVMConstInt(i64t, 1, 0), "hit_inc");
    LLVMBuildStore(B, hit_inc, probe_gep);

    // emit debug-visible local '__seq' — shows this block's own hit count
    // one alloca per function in entry block, updated by each probe
    if (!a->coverage_seq_local) {
        LLVMBasicBlockRef cur_block = LLVMGetInsertBlock(B);
        LLVMValueRef fn = LLVMGetBasicBlockParent(cur_block);
        LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(fn);
        LLVMValueRef first_inst = LLVMGetFirstInstruction(entry);
        if (first_inst)
            LLVMPositionBuilderBefore(B, first_inst);
        else
            LLVMPositionBuilderAtEnd(B, entry);
        a->coverage_seq_local = LLVMBuildAlloca(B, i64t, "__seq");
        LLVMPositionBuilderAtEnd(B, cur_block);
        LLVMMetadataRef scope = LLVMGetCurrentDebugLocation2(B) ?
            LLVMDILocationGetScope(LLVMGetCurrentDebugLocation2(B)) : a->compile_unit;
        if (scope) {
            LLVMMetadataRef di_type = LLVMDIBuilderCreateBasicType(
                a->dbg_builder, "i64", 3, 64, 0x05, LLVMDIFlagZero);
            LLVMMetadataRef di_var = LLVMDIBuilderCreateAutoVariable(
                a->dbg_builder, scope, "__seq", 5,
                a->file, 0, di_type, true, LLVMDIFlagZero, 0);
            LLVMMetadataRef expr = LLVMDIBuilderCreateExpression(a->dbg_builder, null, 0);
            LLVMMetadataRef loc = LLVMGetCurrentDebugLocation2(B);
            if (loc) {
                LLVMDIBuilderInsertDeclareRecordAtEnd(
                    a->dbg_builder, a->coverage_seq_local, di_var, expr, loc,
                    LLVMGetInsertBlock(B));
            }
        }
    }
    LLVMBuildStore(B, hit_inc, a->coverage_seq_local);
}

// the module holding the function under construction: every core emits
// its own, so a declaration made in the main module is not visible here
static LLVMModuleRef cur_module(aether a) {
    return LLVMGetGlobalParent(LLVMGetBasicBlockParent(LLVMGetInsertBlock(a->builder)));
}

// read clock_gettime(CLOCK_MONOTONIC) and return nanosecond timestamp.
// every type and constant comes from the CURRENT module's context: a
// core module has its own, and the main context's types do not verify
LLVMValueRef emit_clock_ns(aether a, cstr label) {
    emit_guard;
    LLVMModuleRef  m    = cur_module(a);
    LLVMContextRef c    = LLVMGetModuleContext(m);
    LLVMTypeRef    i64t = LLVMInt64TypeInContext(c);
    LLVMTypeRef    i32t = LLVMInt32TypeInContext(c);
    LLVMTypeRef    ptrt = LLVMPointerTypeInContext(c, 0);
    LLVMTypeRef    timespec_type = LLVMStructTypeInContext(c, (LLVMTypeRef[]){ i64t, i64t }, 2, false);
    LLVMTypeRef    cg_type = LLVMFunctionType(i32t, (LLVMTypeRef[]){ i32t, ptrt }, 2, false);
    LLVMValueRef   cg = LLVMGetNamedFunction(m, "clock_gettime");
    if (!cg) cg = LLVMAddFunction(m, "clock_gettime", cg_type);
    // hoist alloca to entry block so it doesn't corrupt the stack in return blocks
    LLVMBasicBlockRef current = LLVMGetInsertBlock(B);
    LLVMBasicBlockRef entry   = LLVMGetEntryBasicBlock(LLVMGetBasicBlockParent(current));
    LLVMValueRef      first   = LLVMGetFirstInstruction(entry);
    if (first)
        LLVMPositionBuilderBefore(B, first);
    else
        LLVMPositionBuilderAtEnd(B, entry);
    LLVMValueRef ts = LLVMBuildAlloca(B, timespec_type, label);
    LLVMPositionBuilderAtEnd(B, current);
    // CLOCK_MONOTONIC differs per OS (1 on Linux, 6 on macOS): the host's
    // value is the target's here
    LLVMBuildCall2(B, cg_type, cg,
        (LLVMValueRef[]){ LLVMConstInt(i32t, CLOCK_MONOTONIC, 0), ts }, 2, "");
    LLVMValueRef sec  = LLVMBuildLoad2(B, i64t,
        LLVMBuildStructGEP2(B, timespec_type, ts, 0, ""), "sec");
    LLVMValueRef nsec = LLVMBuildLoad2(B, i64t,
        LLVMBuildStructGEP2(B, timespec_type, ts, 1, ""), "nsec");
    LLVMValueRef billion = LLVMConstInt(i64t, 1000000000ULL, 0);
    return LLVMBuildAdd(B, LLVMBuildMul(B, sec, billion, ""), nsec, label);
}

// emit timing start — captures the start timestamp
LLVMValueRef emit_func_timing_start(aether a, u32 func_id) {
    if (!a->timing || a->no_build) return null;
    return emit_clock_ns(a, "start_ns");
}

// emit timing end and accumulate elapsed time - FUNCTION LEVEL ONLY
void emit_func_timing_end(aether a, LLVMValueRef start_ns, u32 func_id) {
    emit_guard;
    if (!a->timing || !start_ns || a->no_build) return;

    LLVMValueRef end_ns  = emit_clock_ns(a, "end_ns");
    LLVMValueRef elapsed = LLVMBuildSub(B, end_ns, start_ns, "elapsed_ns");

    // the array is defined in the main module; a core declares it extern,
    // with the array type rebuilt in its own context
    LLVMModuleRef  m    = cur_module(a);
    LLVMContextRef c    = LLVMGetModuleContext(m);
    LLVMTypeRef    i64t = LLVMInt64TypeInContext(c);
    LLVMTypeRef    i32t = LLVMInt32TypeInContext(c);
    LLVMTypeRef    tarr = LLVMArrayType(i64t, MAX_FUNCS);
    cstr           name = LLVMGetValueName(a->func_timings_global);
    LLVMValueRef   tg   = LLVMGetNamedGlobal(m, name);
    if (!tg) tg = LLVMAddGlobal(m, tarr, name);
    LLVMValueRef gep = LLVMBuildGEP2(B, tarr, tg,
        (LLVMValueRef[]){ LLVMConstInt(i32t, 0, 0), LLVMConstInt(i32t, func_id, 0) }, 2, "timing_ptr");
    LLVMValueRef cur = LLVMBuildLoad2(B, i64t, gep, "timing_val");
    LLVMValueRef sum = LLVMBuildAdd(B, cur, elapsed, "timing_sum");
    LLVMBuildStore(B, sum, gep);
}

// this looks leaky, but isnt
void report_coverage(aether a) {
    emit_guard;
    if (!a->coverage) return;

    // create function type
    LLVMTypeRef fn_type = LLVMFunctionType(
        LLVMVoidTypeInContext(a->module_ctx), null, 0, false);
    
    // one declaration per module, however many reports it makes
    LLVMModuleRef m = LLVMGetGlobalParent(LLVMGetBasicBlockParent(LLVMGetInsertBlock(B)));
    LLVMValueRef __coverage_report = LLVMGetNamedFunction(m, "__coverage_report");
    if (!__coverage_report)
        __coverage_report = LLVMAddFunction(m, "__coverage_report", fn_type);

    // build it
    LLVMBuildCall2(
        B, fn_type, __coverage_report, null, 0, "");
}

// this works fine when re-initializing
void init_coverage(aether a) {
    emit_guard;
    if (!a->coverage && !a->timing) return;

    if (a->coverage) {
        LLVMTypeRef i64t = LLVMInt64TypeInContext(a->module_ctx);
        LLVMTypeRef i32t = LLVMInt32TypeInContext(a->module_ctx);

        LLVMTypeRef array_type = LLVMArrayType(i64t, MAX_PROBES);
        a->coverage_probes_global = LLVMAddGlobal(a->module_ref, array_type,
            fmt("__cov_probes_%s", a->name->chars)->chars);
        LLVMSetInitializer(a->coverage_probes_global, LLVMConstNull(array_type));
        LLVMSetLinkage(a->coverage_probes_global, LLVMExternalLinkage);

        // global sequence counter (external linkage so LLDB can see it)
        a->coverage_seq_global = LLVMAddGlobal(a->module_ref, i64t, "__cov_seq");
        LLVMSetInitializer(a->coverage_seq_global, LLVMConstInt(i64t, 0, 0));

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
    }
        
    a->next_probe_id = 0;
    
    // Function timing setup
    if (a->timing) {

        LLVMTypeRef timing_array = LLVMArrayType(LLVMInt64TypeInContext(a->module_ctx), MAX_FUNCS);
        a->func_timings_global = LLVMAddGlobal(a->module_ref, timing_array,
            fmt("__func_timings_%s", a->name->chars)->chars);
        LLVMSetInitializer(a->func_timings_global, LLVMConstNull(timing_array));
        LLVMSetLinkage(a->func_timings_global, LLVMExternalLinkage);

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
AU_EXPORT void finalize_coverage(aether a) {
    if (!a->coverage) return;
}