#include <stdio.h>
//#include <unistd.h>
//#include <features.h>
#include <lldb/API/LLDB.h>
#include <import>
#undef   read
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
// C names (dbg_init, dbg_io, ...); without C linkage they'd be C++-mangled and the
// loader fails with "undefined symbol: dbg_init". (struct dbg_state + macros above
// stay C++.)
extern "C" {

Au dbg_io(dbg debug) {
    lldb::SBEvent event;
    int           fd_out = open(debug->stdout_fifo->chars, O_RDONLY | O_NONBLOCK);
    int           fd_err = open(debug->stderr_fifo->chars, O_RDONLY | O_NONBLOCK);
    _fd_set_      readfds;
    char          buffer[1024];
    debug->fifo_fd_out = fd_out;
    debug->fifo_fd_err = fd_err;

    while (debug->active) {
        FD_ZERO(&readfds);
        FD_SET(fd_out, &readfds);
        FD_SET(fd_err, &readfds);

        int maxfd = (fd_out > fd_err) ? fd_out : fd_err;
        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL); // wait for stdout / stderr

        if (ready > 0) {
            if (FD_ISSET(fd_out, &readfds)) {
                ssize_t n = read(fd_out, buffer, sizeof(buffer));
                if (n > 0)
                    debug->on_stdout(debug->target,
                        (Au)construct(iobuffer, debug, debug, bytes, buffer, count, n));
            }

            if (FD_ISSET(fd_err, &readfds)) {
                ssize_t n = read(fd_err, buffer, sizeof(buffer));
                if (n > 0)
                    debug->on_stderr(debug->target,
                        (Au)construct(iobuffer, debug, debug, bytes, buffer, count, n));
            }
        } else if (ready < 0)
            break;
    }
    return null;
}

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

        lldb::StateType   state      = lldb::SBProcess::GetStateFromEvent(event);
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

        } else if (state == lldb::eStateExited) {
            debug->running = false;
            debug->active  = false;   // session is over — ends dbg_poll, unlocks UI
            i32    exit_code = S(debug)->process.GetExitStatus();
            exited e = construct(exited,
                debug,  debug,
                code,   exit_code);
            debug->on_exit(debug->target, (Au)e);
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
    if (!debug->exceptions) {
        lldb::SBCommandInterpreter  interp = S(debug)->debugger.GetCommandInterpreter();
        lldb::SBCommandReturnObject result;
        interp.HandleCommand("settings set target.exception-breakpoints.* false", result);
    }

    S(debug)->listener = S(debug)->debugger.GetListener();
    S(debug)->target   = S(debug)->debugger.CreateTarget(debug->location->chars);
    debug->running     = false;

    // mkstemp returns fd but also replaces XXXXXX with unique string
    char   stdout_fifo_t[] = "/tmp/debug_stdout_fifo_XXXXXX";
    char   stderr_fifo_t[] = "/tmp/debug_stderr_fifo_XXXXXX";
    u64    access          = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int    fd_stdout       = mkstemp(stdout_fifo_t);
    close (fd_stdout);
    unlink(stdout_fifo_t);
    mkfifo(stdout_fifo_t, access);
    int    fd_stderr       = mkstemp(stderr_fifo_t);
    close (fd_stderr);
    unlink(stderr_fifo_t);
    mkfifo(stderr_fifo_t, access);
    debug->stdout_fifo     = f(path, "%s", stdout_fifo_t);
    debug->stderr_fifo     = f(path, "%s", stderr_fifo_t);

    if (S(debug)->target.IsValid()) {
        debug->active        = true;
        debug->poll          = construct(async, work, a((Au)debug), work_fn, (hook)dbg_poll);
        debug->io            = construct(async, work, a((Au)debug), work_fn, (hook)dbg_io);
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
    if (!debug->running) return;
    if (S(debug)->process.IsValid())
        S(debug)->process.Detach();
    debug->running = false;
    debug->active  = false;
    close(debug->fifo_fd_out);
    close(debug->fifo_fd_err);
    unlink((char*)debug->stdout_fifo->chars);
    unlink((char*)debug->stderr_fifo->chars);
}

none dbg_step_into(dbg debug) {
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepInto();
}

none dbg_step_over(dbg debug) {
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepOver();
}

none dbg_step_out(dbg debug) {
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    thread.StepOut();
}

none dbg_pause(dbg debug) {
    S(debug)->process.Stop();
}

none dbg_cont(dbg debug) {
    debug->running = true;   // re-arm dbg_poll to catch the next stop/exit
    S(debug)->process.Continue();
}

array read_children(dbg debug, lldb::SBValue value) {
    array result = array(32);

    for (int i = 0, n = (int)value.GetNumChildren(); i < n; ++i) {
        lldb::SBValue child = value.GetChildAtIndex(i);
        string        name  = string((cstr)child.GetName());
        string        type  = string((cstr)child.GetTypeName());
        string        val   = string((cstr)child.GetValue());
        char          name_buf[256];
        if (!name->count) {
            snprintf(name_buf, sizeof(name_buf), "[%u]", i);
            name = string(name_buf);
        }
        variable v = new0(variable,
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, child));

        array_qpush(result, (Au)v);
    }

    return result;
}

array dbg_read_vars(dbg debug, array result, lldb::SBValueList vars) {
    for (uint32_t i = 0; i < vars.GetSize(); ++i) {
        lldb::SBValue value = vars.GetValueAtIndex(i);
        string name = string(value.GetName());
        string type = string(value.GetTypeName());
        string val  = string(value.GetValue());
        variable v  = new0(variable,
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, value)
        );
        array_qpush(result, (Au)v);
    }
    return result;
}

array dbg_read_arguments(dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        true, false, false, false);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_locals   (dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, true, false, false);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_statics  (dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, false, true, false);
    return dbg_read_vars(debug, result, args);
}

array dbg_read_globals  (dbg debug) {
    array             result      = array(32);
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
    array             result = array(32);
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
