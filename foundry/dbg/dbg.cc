#include <stdio.h>
//#include <unistd.h>
//#include <features.h>
#include <lldb/API/LLDB.h>
#include <import>
#undef   read
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <util.h>   // forkpty/openpty live here on macOS (pty.h is Linux-only)
#else
#include <pty.h>
#endif
#include <termios.h>
#include <ports.h>

#define m(...) map_of(_N_ARGS_m(m, ## __VA_ARGS__), null)
#define a(...) array_of(_N_ARGS_a(a, ## __VA_ARGS__), null)
// the import #undef's these for C++ (to protect the stdlib headers above); we use
// them in this TU's body, so re-establish them now that the C++ headers are in.
#define typeid(aa)     ((Au_t)&(Type_i(aa).type))
#define construct(...) new0(__VA_ARGS__)
// Type_i(T) resolves to <T_module_>_T_i via a per-type <T>_module_ macro. silver
// emits a module's OWN type instances (dbg_cursor_i, ...) hardcoded in the generated
// .c, NOT the <T>_module_ macros into a header — so typeid(cursor) here would collapse
// to the undefined "cursor_module__cursor_i". declare them for dbg's own types.
// (Au's own types like Au/async/array are covered by Au's header.)
/*
#define dbg_module_        dbg
#define cursor_module_     dbg
#define variable_module_   dbg
#define exited_module_     dbg
#define iobuffer_module_   dbg
#define breakpoint_module_ dbg
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

// the live LLDB SB objects. the silver schema only carries an opaque `impl`
// handle (intern), so no lldb type leaks into the public interface — this struct
// is allocated in dbg_init and freed in dbg_dealloc, and reached via S(debug).
struct dbg_state {
    lldb::SBDebugger debugger;
    lldb::SBTarget   target;
    lldb::SBProcess  process;
    lldb::SBListener listener;
};
#define S(d)  ((dbg_state*)(d)->impl)
#define BP(b) ((lldb::SBBreakpoint*)(b)->lldb_bp)

// the Au type registration (generated from dbg.ag) references these by their plain
// C names (dbg_init, dbg_start, ...); without C linkage they'd be C++-mangled and the
// loader fails with "undefined symbol: dbg_init". (struct dbg_state + macros above
// stay C++.)
extern "C" {

Au dbg_poll(dbg debug) {
    lldb::SBEvent event;
    while (debug->active) {
        if (!debug->running) {
            usleep(1000);
            continue;
        }
        if (!S(debug)->listener.WaitForEvent(1000, event))
            continue;
        if (!lldb::SBProcess::EventIsProcessEvent(event))
            continue;

        lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);

        // process exit FIRST — a dead process has no thread/frame, so the frame-valid
        // guard below would otherwise eat the exit event and the session would hang
        // "running" forever (on_exit never fires, active never clears). this was the
        // bug: exits were silently dropped here.
        if (state == lldb::eStateExited) {
            debug->running = false;
            debug->active  = false;   // session is over — ends dbg_poll, unlocks UI
            i32 exit_code  = S(debug)->process.GetExitStatus();
            printf("dbg: process exited, code=%d\n", exit_code);
            exited e = construct(exited,
                debug,  debug,
                code,   exit_code);
            debug->on_exit(debug->target, (Au)e);
            continue;   // loop ends (active == false)
        }

        lldb::SBThread    thread     = S(debug)->process.GetSelectedThread();
        lldb::StopReason  reason     = thread.GetStopReason();
        lldb::SBFrame     frame      = thread.GetSelectedFrame();

        if (!frame.IsValid())
            continue;

        lldb::SBLineEntry line_entry = frame.GetLineEntry();
        lldb::SBFileSpec  file_spec  = line_entry.GetFileSpec();
        u32 line   = line_entry.GetLine();
        u32 column = line_entry.GetColumn();
        char file_path[1024];
        file_spec.GetPath(file_path, sizeof(file_path));
        path source = f(path, "%s", file_path);

        if (state == lldb::eStateStopped) {
            // a STEP that completed outside user .ag source — the synthetic main (now
            // de-debugged), libc's __libc_start_call_main, or any system frame — must
            // not surface in the UI. keep running until we're back in .ag code, hit a
            // breakpoint, or the process exits. breakpoint stops are ALWAYS honored
            // (the user asked for them), even if somehow outside .ag.
            bool is_step = (reason == lldb::eStopReasonPlanComplete);
            int  fl = 0; while (file_path[fl]) fl++;
            bool is_ag = line_entry.IsValid() && line > 0 && fl >= 3 &&
                         file_path[fl-3] == '.' && file_path[fl-2] == 'a' && file_path[fl-1] == 'g';
            if (is_step && !is_ag) {
                S(debug)->process.Continue();   // running stays true; poll catches next stop
                continue;
            }
            debug->running = false;
            cursor cur = construct(cursor,
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_break(debug->target, (Au)cur);

        } else if (state == lldb::eStateCrashed) {
            debug->running = false;
            cursor cur = construct(cursor,
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_crash(debug->target, (Au)cur);
        }
    }
    return null;
}

none dbg_init(dbg debug) {
    static bool dbg_init = false;
    if (!dbg_init) {
        dbg_init = true;
        lldb::SBDebugger::Initialize();
    }
    debug->impl = (handle)new dbg_state();
    S(debug)->debugger = lldb::SBDebugger::Create();

    // Au_binding(a, target, required, rtype, arg_type, id, name) -> callback

    if (!debug->on_stdout) debug->on_stdout = Au_binding((Au)debug,(Au)debug->target, true, typeid(Au), typeid(iobuffer), null, "on_stdout");
    if (!debug->on_stderr) debug->on_stderr = Au_binding((Au)debug,(Au)debug->target, true, typeid(Au), typeid(iobuffer), null, "on_stderr");
    if (!debug->on_break)  debug->on_break  = Au_binding((Au)debug,(Au)debug->target, true, typeid(Au), typeid(cursor),   null, "on_break");
    if (!debug->on_exit)   debug->on_exit   = Au_binding((Au)debug,(Au)debug->target, true, typeid(Au), typeid(exited),  null, "on_exit");
    if (!debug->on_crash)  debug->on_crash  = Au_binding((Au)debug,(Au)debug->target, true, typeid(Au), typeid(cursor),   null, "on_crash");

    S(debug)->debugger.SetAsync(true);
    {
        // keep stepping inside code that HAS source. without this, stepping off the
        // end of a function returns into its caller — and at the top of an app that's
        // libc's __libc_start_call_main / _start, which has no usable DWARF (lldb spews
        // "extends beyond the bounds of" errors for glibc's padded structs). avoid-
        // nodebug makes step-over/step-out run THROUGH no-debug frames until they reach
        // the next frame with line info (or the process exits), so we never stop there.
        lldb::SBCommandInterpreter  interp = S(debug)->debugger.GetCommandInterpreter();
        lldb::SBCommandReturnObject result;
        interp.HandleCommand("settings set target.process.thread.step-out-avoid-nodebug true", result);
        interp.HandleCommand("settings set target.process.thread.step-in-avoid-nodebug true",  result);
        if (!debug->exceptions)
            interp.HandleCommand("settings set target.exception-breakpoints.* false", result);
    }

    S(debug)->listener = S(debug)->debugger.GetListener();
    S(debug)->target   = S(debug)->debugger.CreateTarget(debug->location->chars);
    debug->running     = false;

    // give the inferior a PTY for stdout/stderr (not a FIFO). a tty makes libc
    // LINE-buffer the inferior's stdout, so each puts flushes on its newline and
    // shows in the console as you step. a FIFO is a pipe → fully buffered → output
    // only appears at exit (that's why a shell run printed but the debugger didn't).
    // the host drains the masters; the inferior opens the slave by path (AddOpenFileAction).
    // pass null termios → the default cooked tty (OPOST|ONLCR), so the inferior's "\n"
    // is translated to "\r\n" — same as a bash session's pty. without it (raw mode) a
    // bare line-feed only moves the cursor down, not to column 0, and lines staircase.
    int  mo = -1, so = -1, me = -1, se = -1;
    char so_name[256] = {0};
    char se_name[256] = {0};
    if (openpty(&mo, &so, null, null, null) == 0) {
        ttyname_r(so, so_name, sizeof(so_name));
        fcntl(mo, F_SETFL, O_NONBLOCK);
        close(so);   // the inferior reopens the slave by path
    }
    if (openpty(&me, &se, null, null, null) == 0) {
        ttyname_r(se, se_name, sizeof(se_name));
        fcntl(me, F_SETFL, O_NONBLOCK);
        close(se);
    }
    debug->fifo_fd_out = mo;   // masters — the host drains these on its render thread
    debug->fifo_fd_err = me;
    debug->stdout_fifo = f(path, "%s", so_name);   // slave paths — lldb opens for fd 1/2
    debug->stderr_fifo = f(path, "%s", se_name);

    if (S(debug)->target.IsValid()) {
        debug->active        = true;
        debug->poll          = construct(async, work, a((Au)debug), work_fn, (hook)dbg_poll);
        // NOTE: the inferior's stdout/stderr are NOT read here on an io thread —
        // they're the PTY masters (fifo_fd_out/err) and the host (orbiter) drains
        // them on its main/render thread straight into the project's console. that
        // keeps the terminal/scrollback mutation single-threaded (no render race).
        if (debug->auto_start)
            dbg_start(debug);
    }
}

none dbg_dealloc(dbg debug) {
    dbg_stop(debug);
    if (debug->impl) {
        delete S(debug);
        debug->impl = null;
    }
}

none dbg_start(dbg debug) {
    lldb::SBError      error;
    lldb::SBLaunchInfo launch_info(null);

    launch_info.AddOpenFileAction(1, debug->stdout_fifo->chars, true, false);
    launch_info.AddOpenFileAction(2, debug->stderr_fifo->chars, true, false);

    // inherit OUR environment (orbiter's) into the inferior — crucially LD_LIBRARY_PATH,
    // so the inferior's libAu can find its transitive deps (libLLVM, etc). by default
    // lldb launches with a clean env and the inferior fails with "cannot open shared
    // object file" even though it ran fine from a shell that exports the path.
    extern char **environ;
    launch_info.SetEnvironmentEntries((const char**)environ, false);

    // what we are actually about to EXEC (lldb launch, not a dlopen). the resolved
    // path comes straight from the lldb target's executable file spec.
    lldb::SBFileSpec exe = S(debug)->target.GetExecutable();
    printf("dbg: RUNNING (exec) %s/%s\n", exe.GetDirectory() ? exe.GetDirectory() : "?",
                                          exe.GetFilename()  ? exe.GetFilename()  : "?");

    S(debug)->process = S(debug)->target.Launch(launch_info, error);

    if (!error.Success()) {
        printf("failed: %s\n", error.GetCString());
    } else {
        printf("launched: pid=%i\n", (i32)S(debug)->process.GetProcessID());
        debug->running = true;
    }
}

none dbg_stop(dbg debug) {
    if (debug->active) {                   // alive = paused OR running (not just running)
        if (S(debug)->process.IsValid())
            S(debug)->process.Kill();      // terminate the inferior — Detach would leave
                                           // it running (orphaned past a stop / reload)
        debug->running = false;
        debug->active  = false;
    }
    // always reclaim the PTY masters (an already-exited session is !active but its
    // fds are still open). the slave (pts) is auto-reclaimed; -1 so the host never
    // drains a closed/reused fd.
    if (debug->fifo_fd_out >= 0) { close(debug->fifo_fd_out); debug->fifo_fd_out = -1; }
    if (debug->fifo_fd_err >= 0) { close(debug->fifo_fd_err); debug->fifo_fd_err = -1; }
}

// a step/continue is only valid when the inferior is alive AND halted at a stop.
// resuming a process that has exited (or is mid-exit) blocks forever in the gdb-remote
// resume handshake (Process::Resume → DoResume → Listener::GetEvent) — that's the
// freeze hit when stepping again after the app returns off its end.
static bool dbg_at_stop(dbg debug) {
    if (!debug->active || !debug->impl) return false;
    lldb::SBProcess process = S(debug)->process;
    return process.IsValid() && process.GetState() == lldb::eStateStopped;
}

none dbg_step_into(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepInto();
}

none dbg_step_over(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepOver();
}

none dbg_step_out(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepOut();
}

none dbg_pause(dbg debug) {
    // pause only makes sense while the inferior is actually running.
    if (!debug->active || !debug->impl) return;
    lldb::SBProcess process = S(debug)->process;
    if (!process.IsValid() || process.GetState() != lldb::eStateRunning) return;
    process.Stop();
}

none dbg_cont(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the next stop/exit
    S(debug)->process.Continue();
}

// best human-readable rendering of a value. lldb's GetValue() on a silver object (a
// `string`, an array, any class) is just the bare object pointer — useless in the
// preview. so: prefer GetSummary() (gives the quoted text for char*/cstr); and for a
// silver `string` specifically, dig out its first member `chars` (a cstr) and use that
// buffer's summary as the value. falls back to the raw scalar/pointer otherwise.
static string render_value(lldb::SBValue v) {
    const char* sm = v.GetSummary();
    if (sm && sm[0]) return hold(f(string, "%s", sm));
    // silver string object: chars is the const char* text buffer (string_schema's
    // first prop). GetChildMemberWithName auto-derefs the object pointer.
    lldb::SBValue chars = v.GetChildMemberWithName("chars");
    if (chars.IsValid()) {
        const char* cs = chars.GetSummary();
        if (cs && cs[0]) return hold(f(string, "%s", cs));
    }
    const char* vl = v.GetValue();
    return hold(f(string, "%s", vl ? vl : ""));
}

// the deepest tree we read eagerly. lldb's value tree is effectively infinite (a
// pointer's child is its pointee; Au objects cycle through type/vtable pointers), so
// the depth cap is what stops the recursion from looping. levels beyond this come back
// empty (the user can't expand past them — acceptable for the inline preview).
#define DBG_MAX_DEPTH 3

// read a value's children to DBG_MAX_DEPTH levels so nested objects (and a string's
// inner fields) can be expanded inline. __-prefixed header fields (the af-bit struct
// __fbits, the vtable pointer, etc.) are skipped — not user data.
array read_children_depth(dbg debug, lldb::SBValue value, int depth) {
    array result = new0(array, alloc, 32);

    for (int i = 0, n = (int)value.GetNumChildren(); i < n; ++i) {
        lldb::SBValue child = value.GetChildAtIndex(i);
        const char*   nm    = child.GetName();
        if (nm && nm[0] == '_' && nm[1] == '_') continue;
        const char*   tp    = child.GetTypeName();
        // hold: setting a member outside init does not retain the value.
        string        name  = hold(f(string, "%s", nm ? nm : ""));
        string        type  = hold(f(string, "%s", tp ? tp : ""));
        string        val   = render_value(child);
        char          name_buf[256];
        if (!name->count) {
            snprintf(name_buf, sizeof(name_buf), "[%u]", i);
            name = hold(f(string, "%s", name_buf));
        }
        // a node lldb already summarizes (char*/cstr → quoted text) is a formatted
        // leaf: don't recurse into it (a char* has one child per byte — slow + noisy).
        const char* csm  = child.GetSummary();
        bool        leaf = (csm && csm[0]);
        array ar = (depth > 0 && !leaf && child.GetNumChildren() > 0)
            ? hold(read_children_depth(debug, child, depth - 1))
            : hold(new0(array, alloc, 32));
        variable v = new0(variable,
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   ar);

        array_qpush(result, (Au)hold(v));
    }

    return result;
}

array read_children(dbg debug, lldb::SBValue value) {
    return read_children_depth(debug, value, DBG_MAX_DEPTH);
}

array dbg_read_vars(dbg debug, array result, lldb::SBValueList vars) {
    for (uint32_t i = 0; i < vars.GetSize(); ++i) {
        lldb::SBValue value = vars.GetValueAtIndex(i);
        const char* nm = value.GetName();
        const char* tp = value.GetTypeName();
        // hold: setting a member outside init does not retain the value.
        string name = hold(f(string, "%s", nm ? nm : ""));
        string type = hold(f(string, "%s", tp ? tp : ""));
        string val  = render_value(value);
        variable v  = new0(variable,
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   hold(read_children(debug, value))
        );
        array_qpush(result, (Au)hold(v));
    }
    return result;
}

array dbg_read_arguments(dbg debug) {
    array             result = new0(array, alloc, 32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    // in_scope_only = true: only variables whose lexical scope contains the current PC.
    // (args span the whole function, so this is a no-op for them, but kept symmetric.)
    lldb::SBValueList args   = frame.GetVariables(
        true, false, false, true);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_locals   (dbg debug) {
    array             result = new0(array, alloc, 32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    // in_scope_only = true: lldb drops locals not in scope at the PC — e.g. a for-loop
    // iterator once execution is past the loop's lexical block. that's the "# i = " with
    // a blank value the user was seeing; it now simply isn't reported.
    lldb::SBValueList args   = frame.GetVariables(
        false, true, false, true);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_statics  (dbg debug) {
    array             result = new0(array, alloc, 32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, false, true, false);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_globals  (dbg debug) {
    array             result      = new0(array, alloc, 32);
    lldb::SBFrame     frame       = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    u32               num_modules = S(debug)->target.GetNumModules();

    for (u32 i = 0; i < num_modules; ++i) {
        lldb::SBModule    module  = S(debug)->target.GetModuleAtIndex(i);
        lldb::SBValueList globals = S(debug)->target.FindGlobalVariables(
            null, // you can use full file path or wildcard
            1000  // max number of globals to return
        );
        dbg_read_vars(debug, result, globals);
    }
    return result;
}

array dbg_read_registers(dbg debug) {
    array             result = new0(array, alloc, 32);
    lldb::SBThread    thread = S(debug)->process.GetSelectedThread();
    lldb::SBFrame     frame  = thread.GetSelectedFrame();
    lldb::SBValueList regs   = frame.GetRegisters();
    dbg_read_vars(debug, result, regs);
    return result;
}

breakpoint dbg_set_breakpoint(dbg debug, path source, u32 line, u32 column) {
    Au src_head = head(source);
    lldb::SBBreakpoint bp = S(debug)->target.BreakpointCreateByLocation(source->chars, line);

    if (bp.IsValid())
        printf("breakpoint set: %s:%i id=%i\n", source->chars, line, (i32)bp.GetID());
    else
        printf("failed to set breakpoint: %s:%i\n", source->chars, line);

    lldb::SBBreakpoint* heap_bp = new lldb::SBBreakpoint(bp);
    breakpoint br = construct(breakpoint, debug, debug, lldb_bp, (handle)heap_bp);
    return br;
}

none dbg_remove_breakpoint(dbg debug, breakpoint bp) {
    if (bp->removed) return;
    S(debug)->target.BreakpointDelete(BP(bp)->GetID());
    bp->removed = true;
    printf("breakpoint removed: id=%i\n", (i32)BP(bp)->GetID());
}

none dbg_enable_breakpoint(dbg debug, breakpoint bp, bool enable) {
    BP(bp)->SetEnabled(enable);
    printf("breakpoint %s: id=%i\n", enable ? "enabled" : "disabled", (i32)BP(bp)->GetID());
}

none breakpoint_dealloc(breakpoint bp) {
    dbg_remove_breakpoint(bp->debug, bp);
    if (bp->lldb_bp) {
        delete BP(bp);
        bp->lldb_bp = null;
    }
}

} // extern "C"

#pragma GCC diagnostic pop
