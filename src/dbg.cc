#include <stdio.h>
#include <unistd.h>
#include <features.h>
#include <lldb/API/LLDB.h>
#include <import>
#undef   read
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

A dbg_io(dbg debug) {
    lldb::SBEvent event;
    int           fd_out = open(debug->stdout_fifo->chars, O_RDONLY | O_NONBLOCK);
    int           fd_err = open(debug->stderr_fifo->chars, O_RDONLY | O_NONBLOCK);
    fd_set        readfds;
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
                        (A)iobuffer(debug, debug, bytes, buffer, count, n));
            }

            if (FD_ISSET(fd_err, &readfds)) {
                ssize_t n = read(fd_err, buffer, sizeof(buffer));
                if (n > 0)
                    debug->on_stderr(debug->target,
                        (A)iobuffer(debug, debug, bytes, buffer, count, n));
            }
        } else if (ready < 0)
            break;
    }
    return null;
}

A dbg_poll(dbg debug) {
    lldb::SBEvent event;
    while (debug->active) {
        if (!debug->running) {
            usleep(1000);
            continue;
        }
        if (!debug->lldb_listener.WaitForEvent(1000, event))
            continue;
        if (!lldb::SBProcess::EventIsProcessEvent(event))
            continue;
    
        lldb::StateType   state      = lldb::SBProcess::GetStateFromEvent(event);
        lldb::SBThread    thread     = debug->lldb_process.GetSelectedThread();
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
            cursor cur = cursor(
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_break(debug->target, (A)cur);
            
        } else if (state == lldb::eStateCrashed) {
            debug->running = false;
            cursor cur = cursor(
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            debug->on_crash(debug->target, (A)cur);

        } else if (state == lldb::eStateExited) {
            debug->running = false;
            i32    exit_code = debug->lldb_process.GetExitStatus();
            exited e = exited(
                debug,  debug,
                code,   exit_code);
            debug->on_exit(debug->target, (A)e);
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
    debug->lldb_debugger = lldb::SBDebugger::Create();

    // callback,  bind,   A, bool, AType, AType, symbol, symbol) \

    if (!debug->on_stdout) debug->on_stdout = bind((A)debug, (A)debug->target, true, typeid(A), typeid(iobuffer), null, "stdout");
    if (!debug->on_stderr) debug->on_stderr = bind((A)debug, (A)debug->target, true, typeid(A), typeid(iobuffer), null, "stderr");
    if (!debug->on_break)  debug->on_break  = bind((A)debug, (A)debug->target, true, typeid(A), typeid(cursor),   null, "break");
    if (!debug->on_exit)   debug->on_exit   = bind((A)debug, (A)debug->target, true, typeid(A), typeid(dbg),      null, "exit");
    if (!debug->on_crash)  debug->on_crash  = bind((A)debug, (A)debug->target, true, typeid(A), typeid(cursor),   null, "crash");

    debug->lldb_debugger.SetAsync(true);
    if (!debug->exceptions) {
        lldb::SBCommandInterpreter  interp = debug->lldb_debugger.GetCommandInterpreter();
        lldb::SBCommandReturnObject result;
        interp.HandleCommand("settings set target.exception-breakpoints.* false", result);
    }

    debug->lldb_listener = debug->lldb_debugger.GetListener();
    debug->lldb_target   = debug->lldb_debugger.CreateTarget(debug->location->chars);
    debug->running       = false;

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

    if (debug->lldb_target.IsValid()) {
        debug->active        = true;
        debug->poll          = async(work, a((A)debug), work_fn, (hook)dbg_poll);
        debug->io            = async(work, a((A)debug), work_fn, (hook)dbg_io);
        if (debug->auto_start)
            start(debug);
    }
}

none dbg_dealloc(dbg debug) {
    stop(debug);
}

none dbg_start(dbg debug) {
    lldb::SBError      error;
    lldb::SBLaunchInfo launch_info(null);

    launch_info.AddOpenFileAction(1, debug->stdout_fifo->chars, true, false);
    launch_info.AddOpenFileAction(2, debug->stderr_fifo->chars, true, false);

    debug->lldb_process = debug->lldb_target.Launch(launch_info, error);

    if (!error.Success()) {
        print("failed: %s", error.GetCString());
    } else {
        print("launched: pid=%i", (i32)debug->lldb_process.GetProcessID());
        debug->running = true;
    }
}

none dbg_stop(dbg debug) {
    if (!debug->running) return;
    if (debug->lldb_process.IsValid())
        debug->lldb_process.Detach();
    debug->running = false;
    debug->active  = false;
    close(debug->fifo_fd_out);
    close(debug->fifo_fd_err);
    unlink(debug->stdout_fifo->chars);
    unlink(debug->stderr_fifo->chars);
}

none dbg_step_into(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepInto();
}

none dbg_step_over(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepOver();
}

none dbg_step_out(dbg debug) {
    lldb::SBThread thread = debug->lldb_process.GetSelectedThread();
    thread.StepOut();
}

none dbg_pause(dbg debug) {
    debug->lldb_process.Stop();
}

none dbg_cont(dbg debug) {
    debug->lldb_process.Continue();
}

array read_children(dbg debug, lldb::SBValue value) {
    array result = array(32);

    for (int i = 0, n = (int)value.GetNumChildren(); i < n; ++i) {
        lldb::SBValue child = value.GetChildAtIndex(i);
        string        name  = string((cstr)child.GetName());
        string        type  = string((cstr)child.GetTypeName());
        string        val   = string((cstr)child.GetValue());
        char          name_buf[256];
        if (!len(name)) {
            snprintf(name_buf, sizeof(name_buf), "[%u]", i);
            name = string(name_buf);
        }
        variable v = variable(
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, child));

        push(result, (A)v);
    }

    return result;
}

array dbg_read_vars(dbg debug, array result, lldb::SBValueList vars) {
    for (uint32_t i = 0; i < vars.GetSize(); ++i) {
        lldb::SBValue value = vars.GetValueAtIndex(i);
        string name = string(value.GetName());
        string type = string(value.GetTypeName());
        string val  = string(value.GetValue());
        variable v  = variable(
            debug,      debug,
            name,       name,
            type,       type,
            value,      val,
            children,   read_children(debug, value)
        );
        push(result, (A)v);
    }
    return result;
}

array dbg_read_arguments(dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        true, false, false, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_locals   (dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, true, false, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_statics  (dbg debug) {
    array             result = array(32);
    lldb::SBFrame     frame  = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, false, true, false); 
    return dbg_read_vars(debug, result, args);
}

array dbg_read_globals  (dbg debug) {
    array             result      = array(32);
    lldb::SBFrame     frame       = debug->lldb_process.GetSelectedThread().GetSelectedFrame();
    u32               num_modules = debug->lldb_target.GetNumModules();

    for (u32 i = 0; i < num_modules; ++i) {
        lldb::SBModule    module  = debug->lldb_target.GetModuleAtIndex(i);
        lldb::SBValueList globals = debug->lldb_target.FindGlobalVariables(
            null, // you can use full file path or wildcard
            1000  // max number of globals to return
        );
        dbg_read_vars(debug, result, globals);
    }
    return result;
}

array dbg_read_registers(dbg debug) {
    array             result = array(32);
    lldb::SBThread    thread = debug->lldb_process.GetSelectedThread();
    lldb::SBFrame     frame  = thread.GetSelectedFrame();
    lldb::SBValueList regs   = frame.GetRegisters();
    dbg_read_vars(debug, result, regs);
    return result;
}

breakpoint dbg_set_breakpoint(dbg debug, path source, u32 line, u32 column) {
    A src_head = head(source);
    lldb::SBTarget target = debug->lldb_target;
    lldb::SBBreakpoint bp = target.BreakpointCreateByLocation(source->chars, line);

    if (bp.IsValid())
        print("breakpoint set: %s:%i id=%i", source->chars, line, (i32)bp.GetID());
    else
        print("failed to set breakpoint: %s:%i", source->chars, line);
    
    breakpoint br = breakpoint(debug, debug, lldb_bp, bp);
    return br;
}

none dbg_remove_breakpoint(dbg debug, breakpoint bp) {
    if (bp->removed) return;
    lldb::SBTarget target = debug->lldb_target;
    target.BreakpointDelete(bp->lldb_bp.GetID());
    bp->removed = true;
    print("breakpoint removed: id=%i", (i32)bp->lldb_bp.GetID());
}

none dbg_enable_breakpoint(dbg debug, breakpoint bp, bool enable) {
    lldb::SBTarget target = debug->lldb_target;
    bp->lldb_bp.SetEnabled(enable);
    print("breakpoint %s: id=%i", enable ? "enabled" : "disabled", (i32)bp->lldb_bp.GetID());
}

none breakpoint_dealloc(breakpoint bp) {
    dbg_remove_breakpoint(bp->debug, bp);
}

define_class(cursor,     A)
define_class(exited,     A)
define_class(variable,   A)
define_class(breakpoint, A)
define_class(iobuffer,   A)
define_class(dbg,        A)

#pragma GCC diagnostic pop