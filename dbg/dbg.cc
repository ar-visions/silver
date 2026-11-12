// the debugger, which never sleeps and never dreams
#include <stdio.h>
#include <signal.h>
//#include <unistd.h>
//#include <features.h>
#include <lldb/API/LLDB.h>
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <import>
#undef   read
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <util.h>   // forkpty/openpty live here on macOS (pty.h is Linux-only)
#elif !defined(_WIN32)
#include <pty.h>
#include <termios.h>
#endif
#include <posix.h>  // windows has neither header; the pty shims live here

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
    bool             interrupted = false;
    // parked: the owner is reloading and its callbacks point into an image
    // being unloaded. the poll thread delivers nothing until dbg_adopt
    bool             parked      = false;
    // steps taken on their own to get a step back into .ag code
    int              auto_steps  = 0;
    std::mutex       deliver;
    // Au's object header and type record, from the app's debug info
    bool             au_looked   = false;
    lldb::SBType     au_hdr;
    lldb::SBType     au_type;
    std::map<std::string, lldb::SBType> classes;
    // a child row's path names its type by place here
    std::vector<lldb::SBType> path_types;
};
#define S(d)  ((dbg_state*)(d)->impl)
#define BP(b) ((lldb::SBBreakpoint*)(b)->lldb_bp)

// the Au type registration (generated from dbg.ag) references these by their plain
// C names (dbg_init, dbg_start, ...); without C linkage they'd be C++-mangled and the
// loader fails with "undefined symbol: dbg_init". (struct dbg_state + macros above
// stay C++.)
// windows exports only what is marked; an elf .so exports every symbol,
// so without this an importing module cannot see these at all
#ifdef _WIN32
#define DBG_API __attribute__((dllexport))
#else
#define DBG_API
#endif

extern "C" {

static Au dbg_poll_thunk(Au ctx, Au w);

// every callback into the owner goes through here: a parked session holds its
// events (dbg_adopt replays the stop), and park waits out a delivery in flight
static void dbg_deliver(dbg debug, callback cb, Au arg) {
    std::lock_guard<std::mutex> g(S(debug)->deliver);
    if (S(debug)->parked || !cb) return;
    cb((Au)debug->target, arg);
}

// a frame in a silver module's code with no line info (a release
// build): its module's source, from <stem>.source beside the library
// (its first line the module's main .ag; the rest its other files),
// at the function's own func line when found by name, else line 1
static bool dbg_module_src(lldb::SBFrame& fr, char* out, size_t n,
                           u32* line) {
    lldb::SBFileSpec ms = fr.GetModule().GetFileSpec();
    const char* fn = ms.GetFilename();
    const char* dir = ms.GetDirectory();
    if (!fn || !dir) return false;
    char stem[512];
    size_t fl = strlen(fn);
    // a module's library only: the app binary is the host (silver-host.c,
    // which has its own lines), and its _start is no one's source
    if (strncmp(fn, "lib", 3) != 0 || fl <= 6 ||
        strcmp(fn + fl - 3, ".so") != 0)
        return false;
    snprintf(stem, sizeof(stem), "%.*s", (int)(fl - 6), fn + 3);
    char sp[1024];
    snprintf(sp, sizeof(sp), "%s/%s.source", dir, stem);
    FILE* lf = fopen(sp, "r");
    if (!lf) return false;
    // the function's last word: media_app.frame or media_app_frame -> frame
    const char* fnm = fr.GetFunctionName();
    char want[256]; want[0] = 0;
    if (fnm) {
        const char* w = fnm;
        for (const char* c = fnm; *c; c++)
            if (*c == '.' || *c == '_' || *c == ':') w = c + 1;
        snprintf(want, sizeof(want), "func %s ", w);
    }
    char file[1024];
    bool first = true;
    *line = 1;
    while (fgets(file, sizeof(file), lf)) {
        size_t k = strlen(file);
        while (k && (file[k-1] == '\n' || file[k-1] == '\r')) file[--k] = 0;
        if (k < 3 || strcmp(file + k - 3, ".ag") != 0) continue;
        if (first) { snprintf(out, n, "%s", file); first = false; }
        if (!want[0]) break;
        FILE* af = fopen(file, "r");
        if (!af) continue;
        char row[4096];
        u32 ln = 0;
        bool hit = false;
        while (fgets(row, sizeof(row), af)) {
            ln++;
            const char* r = row;
            while (*r == ' ' || *r == '\t') r++;
            if (strncmp(r, "public ", 7) == 0) r += 7;
            if (strncmp(r, want, strlen(want)) == 0) { hit = true; break; }
        }
        fclose(af);
        if (hit) {
            snprintf(out, n, "%s", file);
            *line = ln;
            break;
        }
    }
    fclose(lf);
    return !first;
}

// the machine code of the function holding fr's pc, one instruction
// a line, written to install/tmp/asm/<function>.s; the pc's line
static bool dbg_asm_file(dbg debug, lldb::SBFrame& fr, char* out,
                         size_t n, u32* line) {
    lldb::SBTarget tg = S(debug)->target;
    lldb::addr_t   pc = fr.GetPC();
    lldb::SBInstructionList il;
    lldb::SBFunction fn = fr.GetFunction();
    lldb::SBSymbol   sy = fr.GetSymbol();
    if (fn.IsValid())      il = fn.GetInstructions(tg);
    else if (sy.IsValid()) il = sy.GetInstructions(tg);
    if (!il.IsValid() || il.GetSize() == 0)
        il = tg.ReadInstructions(fr.GetPCAddress(), 64);
    if (!il.IsValid() || il.GetSize() == 0) return false;
    // install/tmp beside install/build, where this library lives
    Dl_info di;
    if (!dladdr((void*)&dbg_asm_file, &di) || !di.dli_fname) return false;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", di.dli_fname);
    char* sl = strrchr(dir, '/');
    if (!sl) return false;
    *sl = 0;
    char ad[1100];
    snprintf(ad, sizeof(ad), "%s/../tmp/asm", dir);
    mkdir(ad, 0755);
    const char* nm = fr.GetFunctionName();
    char safe[200];
    snprintf(safe, sizeof(safe), "%s", nm ? nm : "code");
    for (char* c = safe; *c; c++)
        if (*c == '/' || *c == ' ' || *c == '(' || *c == ')' ||
            *c == '<' || *c == '>' || *c == ',' || *c == '*') *c = '_';
    snprintf(out, n, "%s/%s.s", ad, safe);
    FILE* af = fopen(out, "w");
    if (!af) return false;
    const char* mn = fr.GetModule().GetFileSpec().GetFilename();
    fprintf(af, "# %s in %s\n", nm ? nm : "?", mn ? mn : "?");
    *line = 1;
    size_t cnt = il.GetSize();
    for (size_t i = 0; i < cnt; i++) {
        lldb::SBInstruction in = il.GetInstructionAtIndex(i);
        lldb::addr_t la = in.GetAddress().GetLoadAddress(tg);
        const char* op = in.GetMnemonic(tg);
        const char* ar = in.GetOperands(tg);
        const char* cm = in.GetComment(tg);
        fprintf(af, "%llx  %-8s %s%s%s\n", (unsigned long long)la,
                op ? op : "", ar ? ar : "",
                (cm && cm[0]) ? "   ; " : "", (cm && cm[0]) ? cm : "");
        if (la == pc) *line = (u32)(i + 2);
    }
    fclose(af);
    return true;
}

DBG_API Au dbg_poll(dbg debug) {
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
            dbg_deliver(debug, debug->on_exit, (Au)e);
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
            // ONLY the pre-init park resumes itself; a real signal (SEGV,
            // ABRT, BUS...) must surface or we debug right past the crash
            bool is_sig  = false;
            if (reason == lldb::eStopReasonSignal &&
                thread.GetStopReasonDataCount() > 0) {
                u64 sig = thread.GetStopReasonDataAtIndex(0);
                is_sig = (sig == SIGSTOP || sig == SIGCONT);
            }
            // lldb's own breakpoints (the dynamic loader's) have ids < 0
            if (reason == lldb::eStopReasonBreakpoint &&
                (thread.GetStopReasonDataCount() == 0 ||
                 (i64)thread.GetStopReasonDataAtIndex(0) < 0)) {
                S(debug)->process.Continue();
                continue;
            }
            int  fl = 0; while (file_path[fl]) fl++;
            bool is_ag = line_entry.IsValid() && line > 0 && fl >= 3 &&
                         file_path[fl-3] == '.' && file_path[fl-2] == 'a' && file_path[fl-1] == 'g';
            if (S(debug)->interrupted) {
                S(debug)->interrupted = false;
                // an interrupt stops EVERY thread, and the one the process
                // hands back is whichever the kernel names first — on macOS a
                // worker parked in __psynch_cvwait, with no source and no
                // useful stack. find the thread that is actually in .ag code
                // and select it, so the pane lands on the line and the call
                // stack is the app's, not a condition variable's.
                uint32_t nt    = S(debug)->process.GetNumThreads();
                bool     found = false;
                for (uint32_t ti = 0; ti < nt && !found; ++ti) {
                    lldb::SBThread th = S(debug)->process.GetThreadAtIndex(ti);
                    if (!th.IsValid()) continue;
                    uint32_t nf = th.GetNumFrames();
                    for (uint32_t fi = 0; fi < nf; ++fi) {
                        lldb::SBFrame     fr = th.GetFrameAtIndex(fi);
                        lldb::SBLineEntry le = fr.GetLineEntry();
                        char fp[1024]; fp[0] = 0;
                        le.GetFileSpec().GetPath(fp, sizeof(fp));
                        int n = 0; while (fp[n]) n++;
                        if (!le.IsValid() || le.GetLine() == 0 || n < 3 ||
                            fp[n-3] != '.' || fp[n-2] != 'a' || fp[n-1] != 'g') continue;
                        S(debug)->process.SetSelectedThread(th);
                        th.SetSelectedFrame(fi);
                        thread = th;
                        line   = le.GetLine();
                        column = le.GetColumn();
                        source = f(path, "%s", fp);
                        is_ag  = true;
                        found  = true;
                        break;
                    }
                }
                is_sig = false;
            }
            printf("dbg stop: reason=%d sig=%lld ag=%d at %s:%u\n", (int)reason,
                (long long)(thread.GetStopReasonDataCount() > 0
                    ? thread.GetStopReasonDataAtIndex(0) : -1),
                (int)is_ag, file_path, line);
            {   // no line info in a release build: the module offset is the next best thing
                lldb::SBFrame   f0 = thread.GetFrameAtIndex(0);
                lldb::SBAddress pa = f0.GetPCAddress();
                const char*     mn = f0.GetModule().GetFileSpec().GetFilename();
                printf("dbg stop: pc %s+0x%llx in %s\n", mn ? mn : "?",
                    (unsigned long long)pa.GetFileAddress(), f0.GetFunctionName() ? f0.GetFunctionName() : "?");
            }
            fflush(stdout);
            // a step stops wherever it lands with source on this disk:
            // .ag code, the host's C (silver-host.c), any of it, so the
            // whole stack can be walked. one in code with no source here
            // (libc, the dynamic linker) steps back out to its caller
            bool has_src = false;
            if (line_entry.IsValid() && line > 0) {
                FILE* sf = fopen(file_path, "r");
                if (sf) { has_src = true; fclose(sf); }
            }
            // no line info, but a silver module's code: its source,
            // so the step shows the module it went into
            if (is_step && !has_src) {
                char mp[1024];
                u32  ml = 1;
                if (dbg_module_src(frame, mp, sizeof(mp), &ml)) {
                    source  = f(path, "%s", mp);
                    line    = ml;
                    column  = 0;
                    has_src = true;
                }
            }
            // assembly: every stop is its function's machine code
            if (debug->asm_view && (is_step || !is_sig)) {
                char ap[1024];
                u32  al = 1;
                lldb::SBFrame f0 = thread.GetFrameAtIndex(0);
                if (dbg_asm_file(debug, f0, ap, sizeof(ap), &al)) {
                    source  = f(path, "%s", ap);
                    line    = al;
                    column  = 0;
                    has_src = true;
                    is_ag   = true;
                }
            }
            if (is_step && !has_src && S(debug)->auto_steps < 200) {
                S(debug)->auto_steps++;
                thread.StepOut();
                continue;   // running stays true; poll catches the step
            }
            if (is_step && has_src) is_ag = true;
            // a SIGSTOP never surfaces (lldb-server's own, unrecognised
            // across the app's pid namespace); a step under way goes on
            if (is_sig || (is_step && !is_ag)) {
                S(debug)->process.Continue();   // running stays true; poll catches next stop
                continue;
            }
            S(debug)->auto_steps = 0;
            debug->running = false;
            cursor cur = construct(cursor,
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            dbg_deliver(debug, debug->on_break, (Au)cur);

        } else if (state == lldb::eStateCrashed) {
            debug->running = false;
            cursor cur = construct(cursor,
                debug,  debug,
                source, source,
                line,   line,
                column, column);
            dbg_deliver(debug, debug->on_crash, (Au)cur);
        }
    }
    return null;
}

// lambda vfn convention: (context, args) — the work item rides args
static Au dbg_poll_thunk(Au ctx, Au w) { (void)ctx; return dbg_poll((dbg)w); }

DBG_API none dbg_init(dbg debug) {
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
        debug->poll          = construct(async, work, (Au)a((Au)debug),
            work_fn, (lambda)hold(lambda_instance(null, (callback)dbg_poll_thunk, null, null)));
        // NOTE: the inferior's stdout/stderr are NOT read here on an io thread —
        // they're the PTY masters (fifo_fd_out/err) and the host (orbiter) drains
        // them on its main/render thread straight into the project's console. that
        // keeps the terminal/scrollback mutation single-threaded (no render race).
        if (debug->auto_start)
            dbg_start(debug);
    }
}

DBG_API none dbg_dealloc(dbg debug) {
    dbg_stop(debug);
    if (debug->impl) {
        delete S(debug);
        debug->impl = null;
    }
}

DBG_API none dbg_start(dbg debug) {
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

// attach to an already-running pid (the in-pane app silver-host spawned). the app
// self-stops (SILVER_DEBUG) just before init, so we attach here, the caller installs
// breakpoints, then cont() resumes it — init breakpoints still arm.
DBG_API none dbg_attach(dbg debug, i32 pid) {
    lldb::SBError      error;
    lldb::SBAttachInfo attach_info((lldb::pid_t)pid);
    S(debug)->process = S(debug)->target.Attach(attach_info, error);
    if (!error.Success()) {
        printf("dbg: attach pid=%i failed: %s\n", (i32)pid, error.GetCString());
        debug->active = false;   // session never began — caller checks this
    } else {
        printf("dbg: attached pid=%i\n", (i32)pid);
        // a SIGSTOP handed back on continue stops the thread again
        lldb::SBCommandReturnObject hr;
        S(debug)->debugger.GetCommandInterpreter().HandleCommand(
            "process handle SIGSTOP -p false", hr);
        // swallow the attach's own stop — it must not surface as a break
        lldb::SBEvent ev;
        for (int i = 0; i < 20; i++) {
            if (!S(debug)->listener.WaitForEvent(1, ev)) break;
            if (lldb::SBProcess::EventIsProcessEvent(ev) &&
                lldb::SBProcess::GetStateFromEvent(ev) == lldb::eStateStopped)
                break;
        }
        debug->running = false;   // parked until the caller cont()s
        debug->active  = true;
    }
}

DBG_API none dbg_stop(dbg debug) {
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

// the owner is reloading: the session, its process and its breakpoints stay
// exactly as they are (a stop stays stopped); only the callbacks let go, since
// they point into the image being unloaded
DBG_API none dbg_park(dbg debug) {
    if (!debug->impl) return;
    std::lock_guard<std::mutex> g(S(debug)->deliver);
    S(debug)->parked = true;
    debug->target    = null;
    debug->on_stdout = null;
    debug->on_stderr = null;
    debug->on_break  = null;
    debug->on_exit   = null;
    debug->on_crash  = null;
}

// the reloaded owner takes the session back: bind its callbacks, and if the
// process sits at a stop, hand that stop over so the pane lands on it again
DBG_API none dbg_adopt(dbg debug, Au owner) {
    if (!debug->impl || !owner) return;
    {
        std::lock_guard<std::mutex> g(S(debug)->deliver);
        debug->target    = owner;
        debug->on_stdout = Au_binding((Au)debug, owner, true, typeid(Au), typeid(iobuffer), null, "on_stdout");
        debug->on_stderr = Au_binding((Au)debug, owner, true, typeid(Au), typeid(iobuffer), null, "on_stderr");
        debug->on_break  = Au_binding((Au)debug, owner, true, typeid(Au), typeid(cursor),   null, "on_break");
        debug->on_exit   = Au_binding((Au)debug, owner, true, typeid(Au), typeid(exited),  null, "on_exit");
        debug->on_crash  = Au_binding((Au)debug, owner, true, typeid(Au), typeid(cursor),   null, "on_crash");
        S(debug)->parked = false;
    }
    lldb::SBProcess process = S(debug)->process;
    if (!debug->active || !process.IsValid() || process.GetState() != lldb::eStateStopped) return;
    lldb::SBLineEntry le = process.GetSelectedThread().GetSelectedFrame().GetLineEntry();
    char fp[1024]; fp[0] = 0;
    le.GetFileSpec().GetPath(fp, sizeof(fp));
    debug->running = false;
    cursor cur = construct(cursor,
        debug,  debug,
        source, f(path, "%s", fp),
        line,   le.GetLine(),
        column, le.GetColumn());
    dbg_deliver(debug, debug->on_break, (Au)cur);
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

// the frame the pane shows: after a pause it is the .ag frame up the
// stack, not frame 0 (a libc sleep); 0 when they are the same
static uint32_t dbg_shown_frame(lldb::SBThread& thread) {
    lldb::SBFrame sel = thread.GetSelectedFrame();
    return sel.IsValid() ? sel.GetFrameID() : 0;
}

// a step from a frame above frame 0: the call it is inside (on the
// shown line) finishes, and the step stops back in the shown frame.
// stepping frame 0 there stepped libc, ended outside .ag code and ran on
static bool dbg_finish_inner(lldb::SBThread& thread) {
    uint32_t ix = dbg_shown_frame(thread);
    if (ix == 0) return false;
    lldb::SBFrame inner = thread.GetFrameAtIndex(ix - 1);
    thread.StepOutOfFrame(inner);
    return true;
}

DBG_API none dbg_step_into(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    S(debug)->auto_steps = 0;
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    if (dbg_finish_inner(thread)) return;
    // an instruction, into a call (code with no source too)
    if (debug->asm_view) { thread.StepInstruction(false); return; }
    thread.StepInto();
}

DBG_API none dbg_step_over(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    S(debug)->auto_steps = 0;
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    if (dbg_finish_inner(thread)) return;
    if (debug->asm_view) { thread.StepInstruction(true); return; }
    thread.StepOver();
}

// the stop delivered again: its source, or its machine code
DBG_API none dbg_show(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    lldb::SBFrame  fr = (debug->asm_view) ? thread.GetFrameAtIndex(0)
                                          : thread.GetSelectedFrame();
    char fp[1024]; fp[0] = 0;
    u32  ln = 0, col = 0;
    if (debug->asm_view && dbg_asm_file(debug, fr, fp, sizeof(fp), &ln))
        col = 0;
    else {
        // the nearest frame from the shown one up with its lines
        uint32_t nf = thread.GetNumFrames();
        for (uint32_t fi = fr.GetFrameID(); fi < nf && !fp[0]; ++fi) {
            lldb::SBFrame     cf = thread.GetFrameAtIndex(fi);
            lldb::SBLineEntry le = cf.GetLineEntry();
            if (le.IsValid() && le.GetLine() > 0) {
                le.GetFileSpec().GetPath(fp, sizeof(fp));
                FILE* sf = fopen(fp, "r");
                if (sf) {
                    fclose(sf);
                    ln  = le.GetLine();
                    col = le.GetColumn();
                    thread.SetSelectedFrame(fi);
                    break;
                }
                fp[0] = 0;
            }
        }
        // none with lines: a silver module's own source
        for (uint32_t fi = fr.GetFrameID(); fi < nf && !fp[0]; ++fi) {
            lldb::SBFrame cf = thread.GetFrameAtIndex(fi);
            if (dbg_module_src(cf, fp, sizeof(fp), &ln))
                thread.SetSelectedFrame(fi);
        }
    }
    if (!fp[0]) return;
    cursor cur = construct(cursor,
        debug,  debug,
        source, f(path, "%s", fp),
        line,   ln,
        column, col);
    dbg_deliver(debug, debug->on_break, (Au)cur);
}

// a frame above ix with its source on this disk: somewhere a step out
// can stop (none above main: it returns only as the app quits)
static bool dbg_caller_has_src(lldb::SBThread& thread, uint32_t ix) {
    uint32_t nf = thread.GetNumFrames();
    for (uint32_t fi = ix + 1; fi < nf; ++fi) {
        lldb::SBFrame fr = thread.GetFrameAtIndex(fi);
        lldb::SBLineEntry le = fr.GetLineEntry();
        if (le.IsValid() && le.GetLine() > 0) {
            char fp[1024]; fp[0] = 0;
            le.GetFileSpec().GetPath(fp, sizeof(fp));
            FILE* sf = fopen(fp, "r");
            if (sf) { fclose(sf); return true; }
        }
        // a silver module with no line info still has its source
        char mp[1024];
        u32  ml = 1;
        if (dbg_module_src(fr, mp, sizeof(mp), &ml)) return true;
    }
    return false;
}

DBG_API none dbg_step_out(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    // out of the shown frame, not of frame 0
    uint32_t ix = dbg_shown_frame(thread);
    // the outermost frame with source: nowhere to step out to, it stays
    if (!dbg_caller_has_src(thread, ix)) return;
    debug->running = true;   // re-arm dbg_poll to catch the step's stop
    S(debug)->auto_steps = 0;
    if (ix > 0) {
        lldb::SBFrame shown = thread.GetFrameAtIndex(ix);
        thread.StepOutOfFrame(shown);
        return;
    }
    thread.StepOut();
}

DBG_API none dbg_pause(dbg debug) {
    // pause only makes sense while the inferior is actually running.
    if (!debug->active || !debug->impl) return;
    lldb::SBProcess process = S(debug)->process;
    if (!process.IsValid() || process.GetState() != lldb::eStateRunning) return;
    S(debug)->interrupted = true;
    process.Stop();
}

DBG_API none dbg_cont(dbg debug) {
    if (!dbg_at_stop(debug)) return;
    debug->running = true;   // re-arm dbg_poll to catch the next stop/exit
    S(debug)->process.Continue();
}

// values as silver sees them: every object pointer is read through
// its Au header (112 bytes before it) for its real class, so an
// `element` that is a ClickButton shows as one; strings show their
// text, vecs their elements, maps their pairs. read from the app's
// memory alone: nothing runs in the app
static const int ROW_MAX     = 512;   // children listed at most
static const int PREVIEW_MAX = 120;   // characters in a one line preview

static void au_lookup(dbg_state* s) {
    if (s->au_looked) return;
    s->au_looked = true;
    lldb::SBTypeList hs = s->target.FindTypes("_Au");
    // two layouts carry the name: Au's own begins with its type, au
    for (uint32_t i = 0; i < hs.GetSize(); i++) {
        lldb::SBType t = hs.GetTypeAtIndex(i);
        if (t.GetNumberOfFields() &&
            strcmp(t.GetFieldAtIndex(0).GetName() ?: "", "au") == 0) {
            s->au_hdr = t;
            break;
        }
    }
    lldb::SBTypeList ts = s->target.FindTypes("_Au_t");
    for (uint32_t i = 0; i < ts.GetSize(); i++)
        if (ts.GetTypeAtIndex(i).GetNumberOfFields()) {
            s->au_type = ts.GetTypeAtIndex(i);
            break;
        }
}

static lldb::SBValue at_addr(dbg_state* s, const char* nm,
                             lldb::addr_t a, lldb::SBType t) {
    return s->target.CreateValueFromAddress(nm, lldb::SBAddress(a, s->target),
                                            t);
}

static lldb::addr_t read_ptr(dbg_state* s, lldb::addr_t a) {
    lldb::SBError e;
    lldb::addr_t  v = s->process.ReadPointerFromMemory(a, e);
    return e.Success() ? v : 0;
}

static std::string read_text(dbg_state* s, lldb::addr_t a, size_t max) {
    if (!a) return "";
    std::string b(max + 1, '\0');
    lldb::SBError e;
    size_t n = s->process.ReadCStringFromMemory(a, &b[0], max + 1, e);
    if (!e.Success()) return "";
    b.resize(n);
    return b;
}

// a type record's facts, as Au_t holds them
struct au_info {
    std::string  ident;
    bool         is_class  = false, is_struct  = false, is_prim = false;
    bool         is_real   = false, is_signed  = false;
    uint64_t     size      = 0;
};

static bool au_record(dbg_state* s, lldb::addr_t t, au_info& in) {
    au_lookup(s);
    if (!t || !s->au_type.IsValid()) return false;
    lldb::SBValue r = at_addr(s, "t", t, s->au_type);
    in.ident = read_text(s,
        r.GetChildMemberWithName("ident").GetValueAsUnsigned(0), 96);
    if (in.ident.empty()) return false;
    for (unsigned char c : in.ident)
        if (!(isalnum(c) || c == '_' || c >= 0x80)) return false;
    auto bit = [&](const char* n) {
        return r.GetChildMemberWithName(n).GetValueAsUnsigned(0) != 0;
    };
    in.is_class  = bit("is_class");
    in.is_struct = bit("is_struct");
    in.is_prim   = bit("is_primitive");
    in.is_real   = bit("is_realistic");
    in.is_signed = bit("is_signed");
    in.size      = r.GetChildMemberWithName("typesize").GetValueAsUnsigned(0);
    return true;
}

static lldb::addr_t hdr_field(dbg_state* s, lldb::addr_t obj, const char* f) {
    au_lookup(s);
    if (!s->au_hdr.IsValid()) return 0;
    uint64_t hs = s->au_hdr.GetByteSize();
    lldb::SBValue h = at_addr(s, "h", obj - hs, s->au_hdr);
    return h.GetChildMemberWithName(f).GetValueAsUnsigned(0);
}

// the type an Au object pointer holds, else false (not an Au object)
static bool au_object(dbg_state* s, lldb::addr_t obj, au_info& in) {
    if (!obj || obj < 0x10000) return false;
    return au_record(s, hdr_field(s, obj, "au"), in);
}

// the struct lldb knows by a class's name: complete ones only
static lldb::SBType class_type(dbg_state* s, const std::string& nm) {
    auto it = s->classes.find(nm);
    if (it != s->classes.end()) return it->second;
    lldb::SBType   got;
    lldb::SBTypeList ts = s->target.FindTypes(nm.c_str());
    for (uint32_t i = 0; i < ts.GetSize(); i++) {
        lldb::SBType t = ts.GetTypeAtIndex(i).GetCanonicalType();
        if (t.GetTypeClass() == lldb::eTypeClassStruct &&
            t.GetNumberOfFields() > 0) { got = t; break; }
    }
    s->classes[nm] = got;
    return got;
}

// an element's lldb type from its Au_t: a struct, else a number
static lldb::SBType elem_type(dbg_state* s, const au_info& in) {
    lldb::SBType t = class_type(s, in.ident);
    if (t.IsValid() && t.GetByteSize() == in.size) return t;
    lldb::SBTypeList ts = s->target.FindTypes(in.ident.c_str());
    for (uint32_t i = 0; i < ts.GetSize(); i++)
        if (ts.GetTypeAtIndex(i).GetByteSize() == in.size)
            return ts.GetTypeAtIndex(i);
    lldb::BasicType b = lldb::eBasicTypeUnsignedChar;
    if (in.is_real) b = in.size == 4 ? lldb::eBasicTypeFloat
                                     : lldb::eBasicTypeDouble;
    else switch (in.size) {
        case 2: b = in.is_signed ? lldb::eBasicTypeShort
                                 : lldb::eBasicTypeUnsignedShort; break;
        case 4: b = in.is_signed ? lldb::eBasicTypeInt
                                 : lldb::eBasicTypeUnsignedInt; break;
        case 8: b = in.is_signed ? lldb::eBasicTypeLongLong
                                 : lldb::eBasicTypeUnsignedLongLong; break;
        default: break;
    }
    return s->target.GetBasicType(b);
}

static lldb::SBType void_ptr(dbg_state* s) {
    return s->target.GetBasicType(lldb::eBasicTypeVoid).GetPointerType();
}

// what a value is, for the views below
enum shape_kind { k_plain, k_null, k_text, k_vec, k_map, k_object,
                  k_record, k_pointer, k_boxed };

struct shaped {
    shape_kind    kind = k_plain;
    lldb::addr_t  obj  = 0;      // the object, for the Au kinds
    au_info       cls;
    lldb::SBValue body;          // the object's struct, as its class
};

static bool is_pointer(lldb::SBValue v) {
    return v.GetType().GetCanonicalType().IsPointerType();
}

static shaped shape_of(dbg_state* s, lldb::SBValue v) {
    shaped r;
    lldb::SBType ct = v.GetType().GetCanonicalType();
    if (ct.IsPointerType()) {
        lldb::SBType pt = ct.GetPointeeType().GetCanonicalType();
        lldb::addr_t a  = v.GetValueAsUnsigned(0);
        if (!a) { r.kind = k_null; return r; }
        lldb::TypeClass tc = pt.GetTypeClass();
        bool rec = tc == lldb::eTypeClassStruct ||
                   tc == lldb::eTypeClassClass;
        bool vd  = pt.GetBasicType() == lldb::eBasicTypeVoid;
        // a number or struct boxed as an object (a map's i32 values)
        if ((rec || vd) && au_object(s, a, r.cls) && !r.cls.is_class) {
            if (!(r.cls.is_prim || r.cls.is_struct) || !r.cls.size ||
                r.cls.size > 4096) return r;
            r.obj  = a;
            r.kind = k_boxed;
            r.body = at_addr(s, r.cls.ident.c_str(), a, elem_type(s, r.cls));
            return r;
        }
        if ((rec || vd) && r.cls.is_class) {
            r.obj = a;
            if (r.cls.ident == "string")      r.kind = k_text;
            else if (r.cls.ident == "vector") r.kind = k_vec;
            else if (r.cls.ident == "map")    r.kind = k_map;
            else                              r.kind = k_object;
            lldb::SBType body = class_type(s, r.cls.ident);
            if (body.IsValid())
                r.body = at_addr(s, r.cls.ident.c_str(), a, body);
            else if (r.kind == k_object && rec)
                r.body = v.Dereference();
            return r;
        }
        if (rec && pt.GetNumberOfFields() > 0) r.kind = k_pointer;
        return r;
    }
    lldb::TypeClass tc = ct.GetTypeClass();
    if ((tc == lldb::eTypeClassStruct || tc == lldb::eTypeClassClass ||
         tc == lldb::eTypeClassUnion) && v.GetNumChildren() > 0)
        r.kind = k_record;
    return r;
}

static bool hidden_member(const char* nm) {
    return nm && nm[0] == '_' && nm[1] == '_';
}

static void clip(std::string& t) {
    if (t.size() > PREVIEW_MAX) t = t.substr(0, PREVIEW_MAX - 1) + "…";
}

// a vec's element count, elements, their type and stride
struct vec_view {
    int64_t       count = 0;
    lldb::addr_t  origin = 0;
    bool          objects = true;
    lldb::SBType  type;
    uint64_t      stride = 8;
};

static vec_view vec_of(dbg_state* s, shaped& sh) {
    vec_view w;
    if (!sh.body.IsValid()) return w;
    w.count  = sh.body.GetChildMemberWithName("count").GetValueAsSigned(0);
    w.origin = sh.body.GetChildMemberWithName("origin").GetValueAsUnsigned(0);
    if (w.count < 0) w.count = 0;
    // the element type: the instance's, as Au's vector_elem_type has it
    au_info et;
    lldb::addr_t t = hdr_field(s, sh.obj, "scalar");
    if (!t) t = hdr_field(s, sh.obj, "meta_a");
    w.type = void_ptr(s);
    if (t && au_record(s, t, et) && !(et.is_class && et.size == 0)) {
        if (!et.is_class && et.size > 0) {
            w.objects = false;
            w.type    = elem_type(s, et);
            w.stride  = et.size;
        }
    }
    return w;
}

static lldb::SBValue vec_elem(dbg_state* s, const vec_view& w, int64_t i) {
    char nm[32];
    snprintf(nm, sizeof(nm), "[%lld]", (long long)i);
    return at_addr(s, nm, w.origin + (lldb::addr_t)i * w.stride, w.type);
}

// a map's items in order: key and value, each an Au pointer
static std::vector<std::pair<lldb::SBValue, lldb::SBValue>>
map_items(dbg_state* s, shaped& sh, int most) {
    std::vector<std::pair<lldb::SBValue, lldb::SBValue>> r;
    if (!sh.body.IsValid()) return r;
    lldb::SBType it = class_type(s, "item");
    if (!it.IsValid()) return r;
    lldb::addr_t a = sh.body.GetChildMemberWithName("first")
                         .GetValueAsUnsigned(0);
    while (a && (int)r.size() < most) {
        lldb::SBValue iv = at_addr(s, "item", a, it);
        r.push_back({ iv.GetChildMemberWithName("key"),
                      iv.GetChildMemberWithName("value") });
        a = iv.GetChildMemberWithName("next").GetValueAsUnsigned(0);
    }
    return r;
}

static std::string preview(dbg_state* s, lldb::SBValue v, int depth);

// a value inside another's preview: short, never opened further
static std::string brief(dbg_state* s, lldb::SBValue v) {
    return preview(s, v, 1);
}

static std::string preview(dbg_state* s, lldb::SBValue v, int depth) {
    shaped sh = shape_of(s, v);
    std::string t;
    switch (sh.kind) {
    case k_null: return "null";
    case k_boxed: return preview(s, sh.body, depth);
    case k_text: {
        lldb::addr_t c = sh.body.IsValid()
            ? sh.body.GetChildMemberWithName("chars").GetValueAsUnsigned(0)
            : 0;
        t = "\"" + read_text(s, c, PREVIEW_MAX) + "\"";
        break;
    }
    case k_vec: {
        vec_view w = vec_of(s, sh);
        t = "[" + std::to_string(w.count) + "]";
        if (depth > 0 || !w.count) break;
        t += " { ";
        for (int64_t i = 0; i < w.count && i < 6; i++) {
            if (i) t += ", ";
            t += brief(s, vec_elem(s, w, i));
        }
        t += w.count > 6 ? ", … }" : " }";
        break;
    }
    case k_map: {
        int64_t n = sh.body.IsValid()
            ? sh.body.GetChildMemberWithName("count").GetValueAsSigned(0) : 0;
        t = "map [" + std::to_string(n) + "]";
        if (depth > 0 || !n) break;
        t += " { ";
        auto items = map_items(s, sh, 4);
        for (size_t i = 0; i < items.size(); i++) {
            if (i) t += ", ";
            t += brief(s, items[i].first) + ": " + brief(s, items[i].second);
        }
        t += n > 4 ? ", … }" : " }";
        break;
    }
    case k_object: {
        t = sh.cls.ident;
        if (depth > 0 || !sh.body.IsValid()) break;
        // the members set: numbers and text, a few
        std::string in;
        int shown = 0;
        for (uint32_t i = 0; i < sh.body.GetNumChildren() && shown < 4; i++) {
            lldb::SBValue m = sh.body.GetChildAtIndex(i);
            if (hidden_member(m.GetName())) continue;
            shaped ms = shape_of(s, m);
            if (ms.kind == k_null || ms.kind == k_object ||
                ms.kind == k_pointer || ms.kind == k_record) continue;
            if (ms.kind == k_plain && is_pointer(m)) continue;
            if (ms.kind == k_plain && m.GetValueAsUnsigned(0) == 0) continue;
            in += (shown ? ", " : "") + std::string(m.GetName() ?: "") +
                  ": " + brief(s, m);
            shown++;
        }
        if (shown) t += " { " + in + " }";
        break;
    }
    case k_pointer: {
        char b[40];
        snprintf(b, sizeof(b), "0x%llx",
                 (unsigned long long)v.GetValueAsUnsigned(0));
        t = b;
        break;
    }
    case k_record: {
        if (depth > 1) { t = "{…}"; break; }
        t = "{ ";
        uint32_t n = v.GetNumChildren();
        for (uint32_t i = 0, k = 0; i < n && k < 4; i++) {
            lldb::SBValue m = v.GetChildAtIndex(i);
            if (hidden_member(m.GetName())) continue;
            t += (k++ ? ", " : "") + std::string(m.GetName() ?: "") + ": " +
                 preview(s, m, depth + 1);
        }
        t += n > 4 ? ", … }" : " }";
        break;
    }
    default: {
        const char* sm = v.GetSummary();
        const char* vl = v.GetValue();
        t = (sm && sm[0]) ? sm : (vl ? vl : "");
    }
    }
    clip(t);
    return t;
}

// the type as written in silver: a vec's elements, an object's class
static std::string type_text(dbg_state* s, lldb::SBValue v,
                             shaped& sh) {
    switch (sh.kind) {
    case k_text: return "string";
    case k_map:  return "map";
    case k_boxed: return sh.cls.ident;
    case k_vec: {
        au_info et;
        lldb::addr_t t = hdr_field(s, sh.obj, "scalar");
        if (!t) t = hdr_field(s, sh.obj, "meta_a");
        return (t && au_record(s, t, et)) ? "vec " + et.ident : "vec";
    }
    case k_object: return sh.cls.ident;
    default: {
        const char* tp = v.GetTypeName();
        return tp ? tp : "";
    }
    }
}

static bool has_kids(dbg_state* s, lldb::SBValue v, shaped& sh) {
    switch (sh.kind) {
    case k_vec:     return vec_of(s, sh).count > 0;
    case k_map:     return sh.body.IsValid() &&
        sh.body.GetChildMemberWithName("count").GetValueAsSigned(0) > 0;
    case k_object:  return sh.body.IsValid() && sh.body.GetNumChildren() > 0;
    case k_boxed:   return shape_of(s, sh.body).kind == k_record;
    case k_pointer:
    case k_record:  return true;
    case k_plain: {
        lldb::TypeClass tc = v.GetType().GetCanonicalType().GetTypeClass();
        return tc == lldb::eTypeClassArray && v.GetNumChildren() > 0;
    }
    default:        return false;
    }
}

// a row's path: where its value sits and its type, so its children
// are read again on a click (only within this stop)
static string path_of(dbg_state* s, lldb::SBValue v, const char* fallback) {
    lldb::addr_t a = v.GetLoadAddress();
    if (a == LLDB_INVALID_ADDRESS)
        return hold(f(string, "%s", fallback ? fallback : ""));
    lldb::SBType t  = v.GetType();
    size_t       ix = 0;
    while (ix < s->path_types.size() && s->path_types[ix] != t) ix++;
    if (ix == s->path_types.size()) s->path_types.push_back(t);
    return hold(f(string, "@%llx#%zu", (unsigned long long)a, ix));
}

static variable row_of(dbg debug, const char* nm, lldb::SBValue v,
                       const char* fallback) {
    dbg_state* s  = S(debug);
    shaped     sh = shape_of(s, v);
    std::string pv = preview(s, v, 0);
    std::string tt = type_text(s, v, sh);
    // hold: setting a member outside init does not retain the value.
    string name = hold(f(string, "%s", nm ? nm : ""));
    string type = hold(f(string, "%s", tt.c_str()));
    string val  = hold(f(string, "%s", pv.c_str()));
    variable r  = new0(variable,
        debug,      debug,
        name,       name,
        type,       type,
        value,      val,
        children,   (vector)hold((Au)vector_of(typeid(Au))));
    r->path     = path_of(s, v, fallback);
    r->has_kids = has_kids(s, v, sh);
    return r;
}

// the rows under a value, as its kind lays them out
static Au rows_under(dbg debug, lldb::SBValue v) {
    dbg_state* s = S(debug);
    Au result = (Au)vector_of(typeid(Au));
    shaped sh = shape_of(s, v);
    auto add = [&](const char* nm, lldb::SBValue c) {
        vector_push((vector)result, (Au)row_of(debug, nm, c, null));
    };
    lldb::SBValue walk = v;
    switch (sh.kind) {
    case k_vec: {
        vec_view w = vec_of(s, sh);
        for (int64_t i = 0; i < w.count && i < ROW_MAX; i++) {
            lldb::SBValue e = vec_elem(s, w, i);
            add(e.GetName(), e);
        }
        return result;
    }
    case k_map: {
        for (auto& kv : map_items(s, sh, ROW_MAX)) {
            std::string k = brief(s, kv.first);
            add(k.c_str(), kv.second);
        }
        return result;
    }
    case k_text:
    case k_null:    return result;
    case k_object:
    case k_boxed:   walk = sh.body;           break;
    case k_pointer: walk = v.Dereference();   break;
    default:        break;
    }
    uint32_t n = walk.GetNumChildren();
    for (uint32_t i = 0; i < n && i < ROW_MAX; i++) {
        lldb::SBValue c  = walk.GetChildAtIndex(i);
        const char*   nm = c.GetName();
        if (hidden_member(nm)) continue;
        char ix[32];
        if (!nm || !nm[0]) { snprintf(ix, sizeof(ix), "[%u]", i); nm = ix; }
        add(nm, c);
    }
    return result;
}

DBG_API Au dbg_read_vars(dbg debug, Au result, lldb::SBValueList vars) {
    for (uint32_t i = 0; i < vars.GetSize(); ++i) {
        lldb::SBValue value = vars.GetValueAtIndex(i);
        const char*   nm    = value.GetName();
        const char*   tp    = value.GetTypeName();
        // the object's header (a_au): silver's own, not the user's
        if (tp && tp[0] == '_' && tp[1] == '_') continue;
        vector_push((vector)result, (Au)row_of(debug, nm, value, nm));
    }
    return result;
}

// a line for lldb's own command interpreter: its output, then its error
DBG_API string dbg_command(dbg debug, string line) {
    if (!debug->impl || !line || !line->count) return f(string, "");
    lldb::SBCommandReturnObject r;
    S(debug)->debugger.GetCommandInterpreter().HandleCommand(line->chars, r);
    const char *o = r.GetOutput(), *e = r.GetError();
    return f(string, "%s%s", o ? o : "", e ? e : "");
}

DBG_API vector dbg_read_children(dbg debug, string path) {
    Au result = (Au)vector_of(typeid(Au));
    if (!dbg_at_stop(debug) || !path || !path->count) return (vector)result;
    dbg_state*    s = S(debug);
    lldb::SBValue value;
    unsigned long long a = 0;
    size_t ix = 0;
    if (sscanf(path->chars, "@%llx#%zu", &a, &ix) == 2) {
        if (ix < s->path_types.size())
            value = at_addr(s, "v", (lldb::addr_t)a, s->path_types[ix]);
    } else {
        lldb::SBFrame frame =
            s->process.GetSelectedThread().GetSelectedFrame();
        value = frame.GetValueForVariablePath(path->chars);
    }
    if (!value.IsValid()) return (vector)result;
    return (vector)rows_under(debug, value);
}

DBG_API vector dbg_read_arguments(dbg debug) {
    Au                result = (Au)vector_of(typeid(Au));
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    // in_scope_only = true: only variables whose lexical scope contains the current PC.
    // (args span the whole function, so this is a no-op for them, but kept symmetric.)
    lldb::SBValueList args   = frame.GetVariables(
        true, false, false, true);
    return (vector)dbg_read_vars(debug, result, args);
}

DBG_API vector dbg_read_locals   (dbg debug) {
    Au                result = (Au)vector_of(typeid(Au));
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    // in_scope_only = true: lldb drops locals not in scope at the PC — e.g. a for-loop
    // iterator once execution is past the loop's lexical block. that's the "# i = " with
    // a blank value the user was seeing; it now simply isn't reported.
    lldb::SBValueList args   = frame.GetVariables(
        false, true, false, true);
    return (vector)dbg_read_vars(debug, result, args);
}

DBG_API vector dbg_read_statics  (dbg debug) {
    Au                result = (Au)vector_of(typeid(Au));
    lldb::SBFrame     frame  = S(debug)->process.GetSelectedThread().GetSelectedFrame();
    lldb::SBValueList args   = frame.GetVariables(
        false, false, true, false);
    return (vector)dbg_read_vars(debug, result, args);
}

// the full call stack of the selected thread: outermost first. each frame is the
// function name plus its .ag source location (empty file for system frames).
DBG_API vector dbg_read_frames   (dbg debug) {
    Au             result = (Au)vector_of(typeid(Au));
    lldb::SBThread thread = S(debug)->process.GetSelectedThread();
    if (!thread.IsValid()) return (vector)result;
    uint32_t nf = thread.GetNumFrames();
    for (uint32_t i = 0; i < nf; ++i) {
        lldb::SBFrame frm = thread.GetFrameAtIndex(i);
        if (!frm.IsValid()) continue;
        const char* fn = frm.GetFunctionName();
        string func_name = hold(f(string, "%s", fn ? fn : "?"));
        lldb::SBLineEntry le = frm.GetLineEntry();
        char fp[1024]; fp[0] = 0;
        le.GetFileSpec().GetPath(fp, sizeof(fp));
        path src = hold(f(path, "%s", fp));
        u32  ln  = le.GetLine();
        frame fr = new0(frame,
            debug,     debug,
            func_name, func_name,
            source,    src,
            line,      ln
        );
        vector_push((vector)result, (Au)fr);
    }
    return (vector)result;
}

DBG_API vector dbg_read_globals  (dbg debug) {
    Au                result      = (Au)vector_of(typeid(Au));
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
    return (vector)result;
}

DBG_API vector dbg_read_registers(dbg debug) {
    Au                result = (Au)vector_of(typeid(Au));
    lldb::SBThread    thread = S(debug)->process.GetSelectedThread();
    lldb::SBFrame     frame  = thread.GetSelectedFrame();
    lldb::SBValueList regs   = frame.GetRegisters();
    dbg_read_vars(debug, result, regs);
    return (vector)result;
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

DBG_API none dbg_remove_breakpoint(dbg debug, breakpoint bp) {
    if (bp->removed) return;
    S(debug)->target.BreakpointDelete(BP(bp)->GetID());
    bp->removed = true;
    printf("breakpoint removed: id=%i\n", (i32)BP(bp)->GetID());
}

DBG_API none dbg_enable_breakpoint(dbg debug, breakpoint bp, bool enable) {
    BP(bp)->SetEnabled(enable);
    printf("breakpoint %s: id=%i\n", enable ? "enabled" : "disabled", (i32)BP(bp)->GetID());
}

DBG_API none breakpoint_dealloc(breakpoint bp) {
    dbg_remove_breakpoint(bp->debug, bp);
    if (bp->lldb_bp) {
        delete BP(bp);
        bp->lldb_bp = null;
    }
}

} // extern "C"

#pragma GCC diagnostic pop
