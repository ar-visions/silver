

#ifdef __APPLE__


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ports.h>

#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>
//#include <errno.h>
#include <assert.h>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

#define _timeval_ timeval


// Worker thread function for directory monitoring
static void WatcherThread(struct InotifyContext* ctx, struct WatchInfo* watch) {
}

int inotify_init1(int flags) {
    return 0;
}

int inotify_init() {
    return inotify_init1(0);
}

int inotify_add_watch(int fd, const char* pathname, uint32_t mask) {
    return 0;
}

int inotify_rm_watch(int fd, int wd) {
    return 0;
}

int inotify_close(int fd) {
    return 0;
}


#elif defined(_WIN32)

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>   // the crt's, for the signals it can raise
#include <ports.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>
//#include <errno.h>
#include <assert.h>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

#undef bool

extern "C" {
BOOL EnumProcessModules(
    HANDLE  hProcess,
    HMODULE *lphModule,
    DWORD   cb,
    LPDWORD lpcbNeeded
);
}

#pragma comment(lib, "psapi.lib")

static inotify_watch global_watch = { 0 };  // only 1 for now

int gettimeofday(struct _timeval_* tp, void* tzp) {
    FILETIME ft;
    uint64_t t;
    GetSystemTimeAsFileTime(&ft);
    t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL; // Windows to Unix epoch
    tp->tv_sec = (long)(t / 10000000ULL);
    tp->tv_usec = (long)((t % 10000000ULL) / 10);
    return 0;
}

struct DIR {
    HANDLE              h;
    WIN32_FIND_DATAA    data;
    struct dirent       entry;
    int                 first;
    char                search_path[MAX_PATH];
};

DIR* opendir(const char* path) {
    DIR* d = (DIR*)calloc(1, sizeof(DIR));
    if (!d) return NULL;

    snprintf(d->search_path, sizeof(d->search_path), "%s\\*", path);
    d->h = FindFirstFileA(d->search_path, &d->data);
    if (d->h == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->first = 1;
    return d;
}

struct dirent* readdir(DIR* d) {
    if (!d) return NULL;
    if (!d->first && !FindNextFileA(d->h, &d->data))
        return NULL;

    d->first = 0;
    strncpy(d->entry.d_name, d->data.cFileName, sizeof(d->entry.d_name));
    d->entry.d_name[sizeof(d->entry.d_name) - 1] = '\0';
    d->entry.d_type = (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                      ? DT_DIR : DT_REG;
    return &d->entry;
}

int closedir(DIR* d) {
    if (!d) return -1;
    FindClose(d->h);
    free(d);
    return 0;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new_s = (char*)malloc(len);
    if (new_s) memcpy(new_s, s, len);
    return new_s;
}

int chdir(const char* path) {
    BOOL result = SetCurrentDirectoryA(path);
    return result ? 0 : -1;
}

int symlink(const char* target, const char* linkpath) {
    DWORD flags = 0;

    // Try to determine if it's a directory
    DWORD attr = GetFileAttributesA(target);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        errno = ENOENT;
        return -1;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
    }

    // Optional: allow non-elevated symlinks (Windows 10+ Dev Mode)
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

    if (!CreateSymbolicLinkA(linkpath, target, flags)) {
        DWORD err = GetLastError();
        switch (err) {
            case ERROR_PRIVILEGE_NOT_HELD: errno = EPERM; break;
            case ERROR_ACCESS_DENIED:      errno = EACCES; break;
            case ERROR_ALREADY_EXISTS:     errno = EEXIST; break;
            default:                       errno = EINVAL; break;
        }
        return -1;
    }

    return 0;
}

// atomic-replace dst with src, retrying past a transient scanner lock.
// the linker cannot delete a file Defender is mid-scan on -- so it writes
// a fresh temp and hands it here; MoveFileEx replaces in one step, and the
// retry rides out the (sub-second) window the scan holds the old file.
int au_replace_file(const char* src, const char* dst) {
    for (int i = 0; i < 40; i++) {
        if (MoveFileExA(src, dst,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
            return 0;
        DWORD err = GetLastError();
        if (err != ERROR_ACCESS_DENIED && err != ERROR_SHARING_VIOLATION) {
            errno = EINVAL;
            return -1;
        }
        Sleep(50);
    }
    errno = EACCES;
    return -1;
}

// the win32 path calls hand back backslashes; silver's path routines
// scan for '/', and windows accepts either, so normalise on the way out
static void win_norm_sep(char* s) {
    if (!s) return;
    // GetFinalPathNameByHandle answers with the \\?\ long-path prefix. it must
    // go before the separators flip, or it becomes //?/ and no longer matches
    // the later strip -- and nothing downstream, dlopen included, accepts that
    if (s[0] == '\\' && s[1] == '\\' && s[2] == '?' && s[3] == '\\')
        memmove(s, s + 4, strlen(s + 4) + 1);
    for (; *s; s++) if (*s == '\\') *s = '/';
}

char* realpath(const char* path, char* resolved_path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    char temp[MAX_PATH];
    DWORD len = GetFullPathNameA(path, MAX_PATH, temp, NULL);
    win_norm_sep(temp);
    if (len == 0 || len >= MAX_PATH) {
        errno = EINVAL;
        return NULL;
    }

    // If caller passed NULL, allocate buffer
    if (!resolved_path) {
        resolved_path = (char*)malloc(len + 1);
        if (!resolved_path) {
            errno = ENOMEM;
            return NULL;
        }
    }

    strcpy(resolved_path, temp);
    return resolved_path;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    // /proc/self/exe is how portable code asks "where am i?"; windows
    // answers that with GetModuleFileNameA rather than a symlink
    if (path && strcmp(path, "/proc/self/exe") == 0) {
        DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)bufsiz);
        if (!n || n >= bufsiz) { errno = ENAMETOOLONG; return -1; }
        win_norm_sep(buf);
        return (ssize_t)n;
    }

    HANDLE h = CreateFileA(
        path,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,  // Required for directories and symlinks
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        errno = ENOENT;
        return -1;
    }

    char temp[MAX_PATH];
    DWORD len = GetFinalPathNameByHandleA(h, temp, sizeof(temp), FILE_NAME_NORMALIZED);
    win_norm_sep(temp);
    CloseHandle(h);

    if (len == 0 || len >= bufsiz) {
        errno = EINVAL;
        return -1;
    }

    // Strip "\\?\" prefix if present
    const char* src = temp;
    if (strncmp(temp, "\\\\?\\", 4) == 0)
        src += 4;

    strncpy(buf, src, bufsiz);
    return strlen(buf);
}

int lstat(const char* path, struct _stat* st) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(path, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        errno = ENOENT;
        return -1;
    }
    FindClose(hFind);

    memset(st, 0, sizeof(*st));

    // Get basic size and mode
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        st->st_mode = S_IFDIR;
    } else if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        st->st_mode = S_IFLNK;  // define manually below
    } else {
        st->st_mode = S_IFREG;
    }

    st->st_size = ((long long)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;

    // Convert FILETIME to time_t (very roughly)
    FILETIME ft = findData.ftLastWriteTime;
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    st->st_mtime = (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);

    return 0;
}

char* dirname(char* path) {
    if (!path || !*path) return ".";

    // Strip trailing slashes
    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\'))
        path[--len] = '\0';

    // Find the last slash
    char* slash = strrchr(path, '/');
#ifdef _WIN32
    // Also handle backslashes
    char* bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif

    if (!slash) return ".";

    // Handle root-only path like "/"
    if (slash == path) {
        slash[1] = '\0';
        return path;
    }

    *slash = '\0';
    return path;
}

char* getcwd(char* buf, size_t size) {
    char temp[MAX_PATH];

    DWORD len = GetCurrentDirectoryA(sizeof(temp), temp);
    win_norm_sep(temp);
    if (len == 0 || len >= sizeof(temp)) {
        errno = ERANGE;
        return NULL;
    }

    // If user passed NULL, allocate buffer
    if (!buf) {
        buf = (char*)malloc(len + 1);
        if (!buf) {
            errno = ENOMEM;
            return NULL;
        }
    } else {
        if (size < len + 1) {
            errno = ERANGE;
            return NULL;
        }
    }

    memcpy(buf, temp, len + 1);
    return buf;
}

int mkdir(const char* path, mode_t mode) {
    (void)mode;  // permissions are ignored on Windows

    if (CreateDirectoryA(path, NULL)) {
        return 0;
    }

    DWORD err = GetLastError();
    switch (err) {
        case ERROR_ALREADY_EXISTS: errno = EEXIST; break;
        case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
        case ERROR_ACCESS_DENIED:  errno = EACCES; break;
        default:                   errno = EINVAL; break;
    }
    return -1;
}

static HANDLE fd_to_handle(int fd) {
    return (HANDLE)_get_osfhandle(fd);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, long long offset) {
    DWORD protect = 0;
    DWORD access  = 0;

    if ((prot & PROT_WRITE) && (prot & PROT_READ)) {
        protect = PAGE_READWRITE;
        access  = FILE_MAP_WRITE;
    } else if (prot & PROT_READ) {
        protect = PAGE_READONLY;
        access  = FILE_MAP_READ;
    } else {
        protect = PAGE_NOACCESS;
        access  = 0;
    }

    // an anonymous mapping names no file: windows backs it with the page file,
    // and the size has to be given since there is no file to take it from.
    // asking _get_osfhandle for fd -1 trips the crt invalid-parameter handler
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD  hi = 0, lo = 0;
    if (fd >= 0 && !(flags & MAP_ANONYMOUS)) {
        hFile = fd_to_handle(fd);
        if (hFile == INVALID_HANDLE_VALUE) { errno = EBADF; return MAP_FAILED; }
    } else {
        hi = (DWORD)(((unsigned long long)length >> 32) & 0xFFFFFFFF);
        lo = (DWORD)( (unsigned long long)length        & 0xFFFFFFFF);
    }

    HANDLE mapping = CreateFileMapping(hFile, NULL, protect, hi, lo, NULL);
    if (!mapping) { errno = ENOMEM; return MAP_FAILED; }

    void* map = MapViewOfFile(mapping, access, (DWORD)((offset >> 32) & 0xFFFFFFFF), (DWORD)(offset & 0xFFFFFFFF), length);
    CloseHandle(mapping);
    // callers test against MAP_FAILED, so NULL would read as success
    return map ? map : MAP_FAILED;
}

int   munmap(void* addr, size_t length) {
    return UnmapViewOfFile(addr) ? 0 : -1;
}

typedef struct {
    pid_t pid;
    HANDLE handle;
} child_proc_t;

static child_proc_t child_processes[32];
static int          child_count = 0;

static HANDLE get_handle_from_pid(pid_t pid) {
    for (int i = 0; i < child_count; ++i) {
        if (child_processes[i].pid == pid)
            return child_processes[i].handle;
    }
    return NULL;
}

struct WatchInfo {
    std::string path;
    uint32_t mask;
    HANDLE dirHandle;
    HANDLE stopEvent;
    std::thread watchThread;
    int wd;

    // a WatchInfo owns its thread, so it stops it. destroying a JOINABLE
    // std::thread calls terminate() -- that was the SIGABRT at exit, raised
    // from the static map's destructor with the threads still running.
    // stopEvent wakes the watcher out of its INFINITE wait at once
    ~WatchInfo() {
        if (stopEvent) SetEvent(stopEvent);
        if (watchThread.joinable()) watchThread.join();
        if (dirHandle && dirHandle != INVALID_HANDLE_VALUE) CloseHandle(dirHandle);
        if (stopEvent) CloseHandle(stopEvent);
    }
};

struct InotifyContext {
    std::map<int, std::unique_ptr<WatchInfo>> watches;
    std::mutex mutex;
    int nextWd = 1;
    // the watcher thread only has to report THAT something changed: the
    // reader synthesizes one event, which is all watch_runner looks at
    std::atomic<int> pending{0};
    std::atomic<bool> closed{false};
};

// Global map to track inotify instances
static std::map<int, std::unique_ptr<InotifyContext>> g_inotifyInstances;
static std::mutex g_instanceMutex;
static int g_nextFd = 100;  // Start with a high number to avoid conflicts


// Worker thread function for directory monitoring
void WatcherThread(InotifyContext* ctx, WatchInfo* watch) {
    const DWORD bufferSize = 4096;
    BYTE buffer[bufferSize];
    DWORD bytesReturned;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    HANDLE events[2] = { overlapped.hEvent, watch->stopEvent };
    
    DWORD notifyFilter = 0;
    if (watch->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO))
        notifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME;
    if (watch->mask & IN_MODIFY)
        notifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
    if (watch->mask & IN_ATTRIB)
        notifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
    
    while (!ctx->closed) {
        if (ReadDirectoryChangesW(
            watch->dirHandle,
            buffer,
            bufferSize,
            TRUE,   // watch the whole subtree: one handle, one thread
            notifyFilter,
            NULL,
            &overlapped,
            NULL)) {
            
            DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            
            if (waitResult == WAIT_OBJECT_0) {  // Change detected
                if (GetOverlappedResult(watch->dirHandle, &overlapped, &bytesReturned, FALSE))
                    ctx->pending++;
                ResetEvent(overlapped.hEvent);
            } else if (waitResult == WAIT_OBJECT_0 + 1) {  // Stop event
                break;
            }
        }
    }
    
    CloseHandle(overlapped.hEvent);
}

int inotify_init1(int flags) {
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    
    int fd = g_nextFd++;
    auto ctx = std::make_unique<InotifyContext>();
    
    // Note: IN_CLOEXEC and IN_NONBLOCK flags would need additional implementation
    // for full compatibility (e.g., using named pipes or sockets for event delivery)
    
    g_inotifyInstances[fd] = std::move(ctx);
    return fd;
}

int inotify_init() {
    return inotify_init1(0);
}

int inotify_add_watch(int fd, const char* pathname, uint32_t mask) {
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    
    auto it = g_inotifyInstances.find(fd);
    if (it == g_inotifyInstances.end()) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    
    auto& ctx = it->second;
    std::lock_guard<std::mutex> ctxLock(ctx->mutex);

    // a watch now covers its whole subtree, so a directory under one we
    // already hold needs nothing. the caller walks the tree adding every
    // directory (inotify needs that; windows does not) and each one used to
    // cost a handle AND a thread -- hundreds of them, joined one by one at
    // shutdown. that was the multi-second close
    {
        std::string np(pathname);
        for (char& c : np) if (c == 92) c = '/';   // 92 = backslash
        for (const auto& w : ctx->watches) {
            std::string wp = w.second->path;
            for (char& c : wp) if (c == 92) c = '/';   // 92 = backslash
            if (np.size() >= wp.size() && np.compare(0, wp.size(), wp) == 0 &&
                (np.size() == wp.size() || np[wp.size()] == '/'))
                return w.first;          // parent already watches this
        }
    }

    // Convert path to wide string for Windows
    int pathLen = MultiByteToWideChar(CP_UTF8, 0, pathname, -1, NULL, 0);
    std::vector<WCHAR> widePath(pathLen);
    MultiByteToWideChar(CP_UTF8, 0, pathname, -1, widePath.data(), pathLen);
    
    // Open directory handle
    HANDLE dirHandle = CreateFileW(
        widePath.data(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (dirHandle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    // Create watch info
    auto watch = std::make_unique<WatchInfo>();
    watch->path = pathname;
    watch->mask = mask;
    watch->dirHandle = dirHandle;
    watch->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    watch->wd = ctx->nextWd++;
    
    // Start watcher thread
    WatchInfo* watchPtr = watch.get();
    watch->watchThread = std::thread(WatcherThread, ctx.get(), watchPtr);
    
    int wd = watch->wd;
    ctx->watches[wd] = std::move(watch);
    
    return wd;
}

int inotify_rm_watch(int fd, int wd) {
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    
    auto it = g_inotifyInstances.find(fd);
    if (it == g_inotifyInstances.end()) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    
    auto& ctx = it->second;
    std::lock_guard<std::mutex> ctxLock(ctx->mutex);
    
    auto watchIt = ctx->watches.find(wd);
    if (watchIt == ctx->watches.end()) {
        SetLastError(ERROR_NOT_FOUND);
        return -1;
    }
    
    auto& watch = watchIt->second;
    
    // Signal thread to stop
    SetEvent(watch->stopEvent);
    
    // Wait for thread to finish
    if (watch->watchThread.joinable()) {
        watch->watchThread.join();
    }
    
    // Clean up
    CloseHandle(watch->dirHandle);
    CloseHandle(watch->stopEvent);
    
    ctx->watches.erase(watchIt);
    
    return 0;
}

int inotify_close(int fd) {
    // take the instance OUT under the lock, then tear it down without it.
    // inotify_rm_watch locks g_instanceMutex itself, so calling it from here
    // while holding that lock self-deadlocked -- std::mutex is not recursive,
    // and the app hung on close every time
    std::unique_ptr<InotifyContext> ctx;
    {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        auto it = g_inotifyInstances.find(fd);
        if (it == g_inotifyInstances.end()) return -1;
        ctx = std::move(it->second);
        g_inotifyInstances.erase(it);
    }

    ctx->closed = true;
    for (auto& pair : ctx->watches) {
        WatchInfo* w = pair.second.get();
        SetEvent(w->stopEvent);                 // wakes its INFINITE wait
        if (w->watchThread.joinable())
            w->watchThread.join();
        CloseHandle(w->dirHandle);
        CloseHandle(w->stopEvent);
    }
    ctx->watches.clear();
    return 0;
}

int setenv(const char* name, const char* value, int overwrite) {
    // Check for invalid inputs
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    
    // If overwrite is 0, check if variable already exists
    if (!overwrite) {
        char* existing = getenv(name);
        if (existing != NULL) {
            return 0;  // Variable exists and overwrite is false
        }
    }
    
    // both views must agree: SetEnvironmentVariableA updates the win32 block
    // (what the vulkan loader and CreateProcess read), _putenv_s updates the
    // crt's copy (what getenv and environ report, and what a child inherits)
    { char kv[4096];
      snprintf(kv, sizeof(kv), "%s=%s", name, value ? value : "");
      _putenv(kv); }

    // Set the environment variable
    if (SetEnvironmentVariableA(name, value)) {
        return 0;  // Success
    } else {
        // Map Windows error to errno
        DWORD error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND || error == ERROR_INVALID_PARAMETER) {
            errno = EINVAL;
        } else if (error == ERROR_NOT_ENOUGH_MEMORY || error == ERROR_OUTOFMEMORY) {
            errno = ENOMEM;
        } else {
            errno = EINVAL;  // Generic error
        }
        return -1;
    }
}


// ---- child lifetime -------------------------------------------------------
// unix hands the app the process itself -- execvp REPLACES silver, so there is
// no child left to outlive anything. windows has no exec, so the app must be a
// child, and a windows child keeps running when its parent dies. a job object
// closes that gap: every child joins it, and the only handle is held by THIS
// process, so the kernel empties the job the moment silver ends, however it
// ends -- clean exit, ctrl+c, or kill
static HANDLE     g_childJob = NULL;
static std::mutex g_jobMutex;

static HANDLE child_job() {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    if (!g_childJob) {
        g_childJob = CreateJobObjectA(NULL, NULL);
        if (g_childJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
            memset(&li, 0, sizeof(li));
            li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(g_childJob, JobObjectExtendedLimitInformation,
                                    &li, sizeof(li));
        }
    }
    return g_childJob;
}

// a console child inherits OUR console, or gets a new window if we have none.
// a /SUBSYSTEM:WINDOWS parent (silver-host) has none, so every build it spawns
// would flash up its own window -- CREATE_NO_WINDOW gives it a console with no
// window instead. std handles are passed explicitly, so output is unaffected
static DWORD no_window_flag() {
    return GetConsoleWindow() ? 0 : CREATE_NO_WINDOW;
}

// joined while the child is still suspended, so it cannot start grandchildren
// outside the job. an older windows can refuse the nest -- the spawn stands
static void child_adopt(HANDLE process) {
    HANDLE job = child_job();
    if (job && process) AssignProcessToJobObject(job, process);
}

// the ONE place a process is created. spawn (posix_spawn) and invoke
// (popen) both come through here, so the no-window rule and the
// kill-on-close job are stated once instead of at every call site
static BOOL spawn_process(const char* app, char* cmdline, STARTUPINFOA* si,
                          DWORD flags, void* envblock, const char* cwd,
                          PROCESS_INFORMATION* pi) {
    if (!CreateProcessA(app, cmdline, NULL, NULL, TRUE,
                        flags | no_window_flag(), envblock, cwd, si, pi))
        return FALSE;
    child_adopt(pi->hProcess);
    return TRUE;
}


// ctrl+c is NOT delivered to the app: it is linked /SUBSYSTEM:WINDOWS, and a
// gui binary does not reliably receive console control events. this process is
// a console app and always does -- so it ends the job, which is every process
// the app spawned as well. we return handled and stay alive, so the wait below
// still reports how the app went out. a closed console is not swallowed: we go
// down and the job takes the app with us
static BOOL WINAPI exec_ctrl_handler(DWORD type) {
    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT) return FALSE;
    HANDLE job = child_job();
    if (job) TerminateJobObject(job, 130);   // 128 + SIGINT
    return TRUE;
}

// windows cannot turn one process into another, so this stands in for the app
// instead: same console, same stdio, same exit code, and nothing survives it.
// a shell sees what exec would have given it, and quitting silver quits the app
int execvp(const char* file, char* const argv[]) {
    // execvp searches PATH when the name carries no directory of its own
    std::string app = file ? file : "";
    if (app.find('/') == std::string::npos && app.find('\\') == std::string::npos) {
        char found[MAX_PATH];
        if (SearchPathA(NULL, app.c_str(), ".exe", sizeof(found), found, NULL))
            app = found;
    }

    // NULL env, not environ: exec hands the child everything this process has,
    // and windows keeps two views of that -- the crt block environ reports, and
    // the win32 block SetEnvironmentVariable writes (where the vulkan loader
    // reads ICD and layer paths). only inheritance carries both
    pid_t pid = 0;
    int   sp  = posix_spawn(&pid, app.c_str(), NULL, NULL, argv, NULL);
    if (sp != 0) {
        errno = sp == ENOENT ? ENOENT : EINVAL;
        return -1;
    }

    SetConsoleCtrlHandler(exec_ctrl_handler, TRUE);

    int st = 0;
    if (waitpid(pid, &st, 0) < 0) _exit(1);
    _exit(WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st));
}

#include <windows.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <map>
#include <mutex>

// Process ID type
typedef int pid_t;

// Signal definitions
#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) >> 8) & 0xff)
#define WIFSIGNALED(status)  (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
#define WTERMSIG(status)     ((status) & 0x7f)

// Signal numbers (Windows doesn't have all POSIX signals)
// signal numbers come from ports.h and the crt

// Process group management - simplified for Windows
int setpgid(pid_t pid, pid_t pgid) {
    // Windows doesn't have process groups like POSIX
    // This is a no-op for compatibility
    return 0;
}

// Convert signal number to string
const char* strsignal(int sig) {
    switch (sig) {
        case SIGTERM: return "Terminated";
        case SIGKILL: return "Killed";
        case SIGINT:  return "Interrupt";
        case SIGSEGV: return "Segmentation fault";
        case SIGABRT: return "Aborted";
        default:      return "Unknown signal";
    }
}

// Global map to track child processes
static std::map<pid_t, HANDLE> g_childProcesses;
static std::mutex g_processMutex;
static pid_t g_nextPid = 1000;


// Wait for process
pid_t waitpid(pid_t pid, int* status, int options) {
    std::lock_guard<std::mutex> lock(g_processMutex);
    if (g_childProcesses.empty()) { errno = ECHILD; return -1; }

    // pid -1 means any child, so collect the candidates first
    std::vector<pid_t> pids;
    if (pid == -1) {
        for (auto& kv : g_childProcesses) pids.push_back(kv.first);
    } else {
        if (g_childProcesses.find(pid) == g_childProcesses.end()) {
            errno = ECHILD;
            return -1;
        }
        pids.push_back(pid);
    }

    // WNOHANG polls; without it we block on the one child that was named
    DWORD ms = (options & WNOHANG) ? 0 : INFINITE;
    for (size_t k = 0; k < pids.size(); k++) {
        HANDLE h = g_childProcesses[pids[k]];
        DWORD  w = WaitForSingleObject(h, ms);
        if (w == WAIT_TIMEOUT) continue;          // still running
        if (w == WAIT_FAILED) { errno = EINVAL; return -1; }

        DWORD code = 0;
        GetExitCodeProcess(h, &code);
        CloseHandle(h);
        g_childProcesses.erase(pids[k]);

        if (status) {
            // a windows exit code is either a plain status or an NTSTATUS from
            // an unhandled exception. mapping every high code to SIGTERM read
            // as "Terminated" and hid real crashes, so each is named, and one
            // we cannot name is reported rather than disguised
            // never hide it: an abnormal exit always names its NTSTATUS
            if (code >= 0x80000000)
                fprintf(stderr, "waitpid: pid %d ended with NTSTATUS 0x%08lX\n",
                    (int)pids[k], (unsigned long)code);
            switch (code) {
                case 0xC0000005: *status = SIGSEGV; break;  // access violation
                case 0xC00000FD: *status = SIGSEGV; break;  // stack overflow
                case 0xC000001D: *status = SIGILL;  break;  // illegal instruction
                case 0xC0000096: *status = SIGILL;  break;  // privileged instruction
                case 0xC0000094: *status = SIGFPE;  break;  // integer divide by zero
                case 0xC0000090: *status = SIGFPE;  break;  // float invalid operation
                case 0x80000003: *status = SIGTRAP; break;  // breakpoint
                case 0xC000013A: *status = SIGINT;  break;  // console ctrl-c
                case 0xC0000409: *status = SIGABRT; break;  // stack buffer overrun
                case 0xC0000374: *status = SIGABRT; break;  // heap corruption
                case 0xC0000135: *status = SIGABRT; break;  // dll not found
                case 0xC0000139: *status = SIGABRT; break;  // entry point not found
                default:
                    if (code >= 0x80000000)
                        *status = SIGABRT;
                    else
                        *status = (int)((code & 0xff) << 8);
                    break;
            }
        }
        return pids[k];
    }
    // WNOHANG with everyone still running is a plain 0, not an error
    return 0;
}

pid_t wait(int* status) {
    return waitpid(-1, status, 0);
}

int access(const char* path, int mode) { return _access(path, mode); }
// popen without a shell: CreateProcess runs the program directly, so
// nothing re-parses our command line and cmd.exe never sees it. only
// read mode is used by callers.
typedef struct { FILE* f; HANDLE proc; } popen_rec;
static popen_rec popen_tab[64];
static int       popen_n = 0;
static SRWLOCK   popen_guard = SRWLOCK_INIT;

FILE* popen(const char* cmd, const char* mode) {
    (void)mode;
    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle       = TRUE;

    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return NULL;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    char* line = _strdup(cmd);          // CreateProcessA may write to it
    if (!line) { CloseHandle(rd); CloseHandle(wr); return NULL; }

    // CreateProcess takes the program from the first token and knows no shell
    // syntax, so do what sh does: drop leading blanks, and move a run of
    // NAME=value prefixes into the child's environment
    struct env_save { std::string name, value; bool had; };
    std::vector<env_save> restore;
    char* start = line;
    for (;;) {
        while (*start == ' ' || *start == '	') start++;
        char* eq = NULL;
        char* p  = start;
        while (*p && *p != ' ' && *p != '	') {
            if (*p == '=' && !eq) eq = p;
            p++;
        }
        if (!eq || eq == start) break;
        bool is_name = true;
        for (char* q = start; q < eq && is_name; q++)
            is_name = (*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') ||
                      (*q >= '0' && *q <= '9') || *q == '_';
        if (!is_name || (*start >= '0' && *start <= '9')) break;

        std::string name(start, (size_t)(eq - start));
        std::string value(eq + 1, (size_t)(p - eq - 1));
        char  prev[32767];
        DWORD n = GetEnvironmentVariableA(name.c_str(), prev, (DWORD)sizeof(prev));
        env_save sv;
        sv.name  = name;
        sv.had   = n > 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        sv.value = sv.had ? std::string(prev, n) : std::string();
        restore.push_back(sv);
        SetEnvironmentVariableA(name.c_str(), value.c_str());
        start = p;
    }

    // its own group: sharing ours means one console control event kills the
    // whole tree mid-build, taking us with it
    BOOL ok = spawn_process(NULL, start, &si, CREATE_NEW_PROCESS_GROUP,
                            NULL, NULL, &pi);
    for (size_t i = 0; i < restore.size(); i++)
        SetEnvironmentVariableA(restore[i].name.c_str(),
                                restore[i].had ? restore[i].value.c_str() : NULL);
    free(line);
    CloseHandle(wr);                    // the child owns the write end now
    if (!ok) { CloseHandle(rd); return NULL; }
    CloseHandle(pi.hThread);

    int fd = _open_osfhandle((intptr_t)rd, _O_RDONLY);
    if (fd < 0) { CloseHandle(rd); CloseHandle(pi.hProcess); return NULL; }
    FILE* f = _fdopen(fd, "r");
    if (!f)     { _close(fd);      CloseHandle(pi.hProcess); return NULL; }

    AcquireSRWLockExclusive(&popen_guard);
    if (popen_n < (int)(sizeof(popen_tab) / sizeof(popen_tab[0]))) {
        popen_tab[popen_n].f    = f;
        popen_tab[popen_n].proc = pi.hProcess;
        popen_n++;
    }
    ReleaseSRWLockExclusive(&popen_guard);
    return f;
}

// posix-shaped wait status, so callers use WIFEXITED/WEXITSTATUS
// on every platform rather than branching on the return form
int pclose(FILE* stream) {
    HANDLE proc = NULL;
    AcquireSRWLockExclusive(&popen_guard);
    for (int i = 0; i < popen_n; i++)
        if (popen_tab[i].f == stream) {
            proc = popen_tab[i].proc;
            popen_tab[i] = popen_tab[--popen_n];
            break;
        }
    ReleaseSRWLockExclusive(&popen_guard);

    fclose(stream);
    if (!proc) return -1;

    DWORD code = 0;
    WaitForSingleObject(proc, INFINITE);
    GetExitCodeProcess(proc, &code);
    CloseHandle(proc);
    return ((int)code & 0xff) << 8;
}
int dup(int fd)      { return _dup(fd);    }
int isatty(int fd)   { return _isatty(fd); }
pid_t au_getpid()    { return (pid_t)GetCurrentProcessId(); }

int strcasecmp(const char* a, const char* b) { return _stricmp(a, b); }
int rmdir(const char* path) { return RemoveDirectoryA(path) ? 0 : -1; }

// flock is per open-file-description and idempotent: locking an fd you
// already hold succeeds. LockFileEx instead treats it as a conflicting
// range and blocks against itself, so track what we hold and skip it.
static int   flock_held[64];
static int   flock_n = 0;
static SRWLOCK flock_guard = SRWLOCK_INIT;

static int flock_index(int fd) {
    for (int i = 0; i < flock_n; i++) if (flock_held[i] == fd) return i;
    return -1;
}

int flock(int fd, int operation) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;

    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    const DWORD lo = 0xFFFFFFFF, hi = 0xFFFFFFFF;

    AcquireSRWLockExclusive(&flock_guard);
    int at = flock_index(fd);

    if (operation & LOCK_UN) {
        if (at >= 0) flock_held[at] = flock_held[--flock_n];
        ReleaseSRWLockExclusive(&flock_guard);
        return UnlockFileEx(h, 0, lo, hi, &ov) ? 0 : -1;
    }

    if (at >= 0) {                  // already ours; posix converts in place
        ReleaseSRWLockExclusive(&flock_guard);
        return 0;
    }
    ReleaseSRWLockExclusive(&flock_guard);

    DWORD flags = 0;
    if (operation & LOCK_EX) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (operation & LOCK_NB) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    if (!LockFileEx(h, flags, 0, lo, hi, &ov)) return -1;

    AcquireSRWLockExclusive(&flock_guard);
    if (flock_n < (int)(sizeof(flock_held) / sizeof(flock_held[0])))
        flock_held[flock_n++] = fd;
    ReleaseSRWLockExclusive(&flock_guard);
    return 0;
}

int pthread_detach(pthread_t thread) {
    CloseHandle((HANDLE)thread);
    return 0;
}

char* strndup(const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = strnlen(s, n);
    char* p = (char*)malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

// only TIOCGWINSZ is used; answer it from the console window rect
int ioctl(int fd, unsigned long request, ...) {
    if (request != TIOCGWINSZ) return -1;

    va_list ap;
    va_start(ap, request);
    struct winsize* ws = va_arg(ap, struct winsize*);
    va_end(ap);
    if (!ws) return -1;

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(h, &csbi))
        return -1;

    ws->ws_col    = (unsigned short)(csbi.srWindow.Right  - csbi.srWindow.Left + 1);
    ws->ws_row    = (unsigned short)(csbi.srWindow.Bottom - csbi.srWindow.Top  + 1);
    ws->ws_xpixel = 0;
    ws->ws_ypixel = 0;
    return 0;
}

// GetCurrentThread() is a pseudo-handle: the SAME constant on every thread,
// so pthread_equal would always say yes. the thread id is a real identity.
pthread_t pthread_self() { return (pthread_t)GetCurrentThreadId(); }
int pthread_equal(pthread_t a, pthread_t b) { return a == b; }

// GetThreadDescription is win10+; resolve it rather than link it
int pthread_getname_np(pthread_t thread, char* name, size_t len) {
    if (!name || len == 0) return -1;
    name[0] = '\0';

    typedef HRESULT (WINAPI *getdesc_t)(HANDLE, PWSTR*);
    static getdesc_t getdesc = NULL;
    static bool looked = false;
    if (!looked) {
        looked = true;
        HMODULE k = GetModuleHandleW(L"kernel32.dll");
        if (k) getdesc = (getdesc_t)GetProcAddress(k, "GetThreadDescription");
    }

    if (!getdesc) return 0;

    // pthread_self is a thread id now, so open a handle for anything but us
    bool   self = (DWORD)thread == GetCurrentThreadId();
    HANDLE h    = self ? GetCurrentThread()
                       : OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)thread);
    if (!h) return -1;

    PWSTR desc = NULL;
    if (SUCCEEDED(getdesc(h, &desc)) && desc) {
        WideCharToMultiByte(CP_UTF8, 0, desc, -1, name, (int)len, NULL, NULL);
        LocalFree(desc);
    }
    if (!self) CloseHandle(h);
    return 0;
}

// dladdr - module + nearest symbol for an address, via dbghelp
int dladdr(const void* addr, Dl_info* info) {
    if (!addr || !info) return 0;
    memset(info, 0, sizeof(*info));

    static thread_local char mod_name[MAX_PATH];
    static thread_local char sym_name[MAX_SYM_NAME];

    HMODULE mod = NULL;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)addr, &mod) && mod) {
        info->dli_fbase = (void*)mod;
        if (GetModuleFileNameA(mod, mod_name, sizeof(mod_name)))
            info->dli_fname = mod_name;
    }

    HANDLE proc = GetCurrentProcess();
    ULONG64 store[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)store;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen   = MAX_SYM_NAME;
    DWORD64 disp = 0;
    if (SymFromAddr(proc, (DWORD64)(uintptr_t)addr, &disp, sym)) {
        strncpy(sym_name, sym->Name, sizeof(sym_name) - 1);
        sym_name[sizeof(sym_name) - 1] = '\0';
        info->dli_sname = sym_name;
        info->dli_saddr = (void*)(uintptr_t)sym->Address;
    }
    return (info->dli_fname || info->dli_sname) ? 1 : 0;
}

// only F_SETFL O_NONBLOCK is used, and only pipes can honor it
int fcntl(int fd, int cmd, ...) {
    if (cmd != F_SETFL) return 0;

    va_list ap;
    va_start(ap, cmd);
    int flags = va_arg(ap, int);
    va_end(ap);
    if (!(flags & O_NONBLOCK)) return 0;

    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (GetFileType(h) != FILE_TYPE_PIPE) return 0;

    DWORD mode = PIPE_NOWAIT;
    return SetNamedPipeHandleState(h, &mode, NULL, NULL) ? 0 : -1;
}

long sysconf(int name) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    switch (name) {
        case _SC_PAGESIZE:         return (long)si.dwPageSize;
        case _SC_NPROCESSORS_ONLN: return (long)si.dwNumberOfProcessors;
        case _SC_PHYS_PAGES: {
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof(ms);
            if (!GlobalMemoryStatusEx(&ms)) return -1;
            return (long)(ms.ullTotalPhys / si.dwPageSize);
        }
    }
    return -1;
}

int clock_gettime(int clk, struct timespec* ts) {
    (void)clk;
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart  = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    // FILETIME counts 100ns ticks from 1601; rebase onto the unix epoch
    uint64_t ns = (t.QuadPart - 116444736000000000ULL) * 100;
    ts->tv_sec  = (time_t)(ns / 1000000000ULL);
    ts->tv_nsec = (long)  (ns % 1000000000ULL);
    return 0;
}

int ftruncate(int fd, long long length) {
    return _chsize_s(fd, (__int64)length) == 0 ? 0 : -1;
}

// signal 0 only probes liveness; any other signal terminates
int kill(pid_t pid, int sig) {
    HANDLE h = get_handle_from_pid(pid);
    bool opened = false;
    if (!h) {
        h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, (DWORD)pid);
        opened = true;
    }
    if (!h) return -1;

    int rc = 0;
    if (sig == 0)
        rc = (WaitForSingleObject(h, 0) == WAIT_TIMEOUT) ? 0 : -1;
    else if (!TerminateProcess(h, (UINT)(128 + sig)))
        rc = -1;

    if (opened) CloseHandle(h);
    return rc;
}

void register_child(pid_t pid, HANDLE handle) {
    if (child_count < 32) {
        child_processes[child_count].pid = pid;
        child_processes[child_count].handle = handle;
        child_count++;
    }
}

// Fork implementation for Windows
pid_t fork() {
    // Windows doesn't support fork() directly
    // This creates a new process that will need to be set up differently
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    errno = ENOSYS;
    return -1;
}

// Execute with pipe - Windows implementation
// the only callers sit in a forkpty child branch, and forkpty always fails
// here, so this is unreachable rather than merely unimplemented
int execl(const char* path, const char* arg0, ...) {
    (void)path; (void)arg0;
    errno = ENOSYS;
    return -1;
}

// same story as execl: reachable only from a forkpty child, and
// forkpty always fails here. spawning lives in posix_spawn alone
int execlp(const char* file, const char* arg0, ...) {
    (void)file; (void)arg0;
    errno = ENOSYS;
    return -1;
}

// Create pipe
int pipe(int pipefd[2]) {
    return _pipe(pipefd, 4096, _O_BINARY);
}

// Duplicate file descriptor
int dup2(int oldfd, int newfd) {
    return _dup2(oldfd, newfd);
}

int close(int fd) {
    // same as read(): an inotify fd is ours, not the crt's. watch_runner
    // closes its fd on the way out, and _close faulted on it every time
    bool is_watch;
    {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        is_watch = g_inotifyInstances.count(fd) != 0;
    }
    if (is_watch) return inotify_close(fd);   // takes the lock itself
    return _close(fd);
}

// glibc provides these as real globals and silver's ir references them that
// way; the ucrt only has macros over __acrt_iob_func, so supply the symbols
#undef stdin
#undef stdout
#undef stderr
// the console decodes output as the ansi codepage, so utf-8 box drawing
// (the progress bar) arrives as mojibake unless we say otherwise
// the crt's default invalid-parameter handler calls __fastfail, which kills
// the process with NTSTATUS 0xC0000409 and prints nothing at all. naming the
// call site turns a silent death into a diagnosable one, and returning lets
// the crt function fail normally instead
static void win_invalid_parameter(
        const wchar_t* expr, const wchar_t* func, const wchar_t* file,
        unsigned int line, uintptr_t reserved) {
    (void)reserved;
    FILE* err = __acrt_iob_func(2);   // stderr is #undef'd just below
    fprintf(err, "crt: invalid parameter in %ls at %ls:%u (%ls)\n",
        func ? func : L"<unknown>", file ? file : L"<unknown>", line,
        expr ? expr : L"");
    // the release crt passes no location, so the caller is the only clue
    void* frames[12];
    USHORT n = CaptureStackBackTrace(1, 12, frames, NULL);
    for (USHORT i = 0; i < n; i++) {
        HMODULE mod = NULL;
        char    name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frames[i], &mod) && mod)
            GetModuleFileNameA(mod, name, sizeof(name));
        fprintf(err, "  [%u] %p  %s +0x%llX\n", i, frames[i], name,
            (unsigned long long)((char*)frames[i] - (char*)mod));
    }
    fflush(err);
}

static void __attribute__((constructor)) win_console_utf8(void) {
    SetConsoleOutputCP(CP_UTF8);
    _set_invalid_parameter_handler(win_invalid_parameter);
}

extern "C" {
AU_EXPORT FILE* stdin  = __acrt_iob_func(0);
AU_EXPORT FILE* stdout = __acrt_iob_func(1);
AU_EXPORT FILE* stderr = __acrt_iob_func(2);
}

// windows has no pty; report failure the way a failed fork does, which the
// caller already checks for
int forkpty(int* amaster, char* name, void* termp, struct winsize* win) {
    (void)name; (void)termp; (void)win;
    if (amaster) *amaster = -1;
    errno = ENOSYS;
    return -1;
}

// same story: callers test the return and fall back to unbuffered pipes
int openpty(int* amaster, int* aslave, char* name, void* termp, struct winsize* win) {
    (void)name; (void)termp; (void)win;
    if (amaster) *amaster = -1;
    if (aslave)  *aslave  = -1;
    errno = ENOSYS;
    return -1;
}

// no descriptor here is a terminal device with a path
int ttyname_r(int fd, char* buf, size_t len) {
    (void)fd;
    if (buf && len) buf[0] = 0;
    return ENOTTY;
}

// ---- posix_spawn ----------------------------------------------------------
// the actions are recorded, then applied where windows has an equivalent.
// dup2-onto-a-fixed-child-fd does not have one: handle inheritance carries a
// HANDLE, not a numbered descriptor, so that action is recorded and skipped.
// the process still launches; only a channel passed that way goes unwired.
int posix_spawn_file_actions_init(posix_spawn_file_actions_t* fa) {
    if (!fa) return EINVAL;
    fa->count = 0;
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* fa) {
    if (fa) fa->count = 0;
    return 0;
}

static int spawn_act_push(posix_spawn_file_actions_t* fa, int op) {
    if (!fa) return EINVAL;
    if (fa->count >= (int)(sizeof(fa->act) / sizeof(fa->act[0]))) return ENOMEM;
    memset(&fa->act[fa->count], 0, sizeof(fa->act[0]));
    fa->act[fa->count].op = op;
    return fa->count++;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* fa, int fd, int newfd) {
    int i = spawn_act_push(fa, POSIX_SPAWN_ACT_DUP2);
    if (i < 0) return -i;
    fa->act[i].fd = fd; fa->act[i].newfd = newfd;
    return 0;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* fa, int fd,
                                     const char* path, int oflag, unsigned mode) {
    int i = spawn_act_push(fa, POSIX_SPAWN_ACT_OPEN);
    if (i < 0) return -i;
    fa->act[i].fd = fd; fa->act[i].oflag = oflag; fa->act[i].mode = (int)mode;
    if (path) { strncpy(fa->act[i].path, path, sizeof(fa->act[i].path) - 1); }
    return 0;
}

int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t* fa, const char* path) {
    int i = spawn_act_push(fa, POSIX_SPAWN_ACT_CHDIR);
    if (i < 0) return -i;
    if (path) { strncpy(fa->act[i].path, path, sizeof(fa->act[i].path) - 1); }
    return 0;
}

int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* fa,
                const posix_spawnattr_t* attr,
                char* const argv[], char* const envp[]) {
    (void)attr;
    if (!path) { errno = EINVAL; return EINVAL; }

    std::string cmd;
    for (int i = 0; argv && argv[i]; i++) {
        if (i) cmd += ' ';
        // quote an argument with spaces so CommandLineToArgvW rebuilds it --
        // but never one that already carries quotes of its own, which is how a
        // shell command arrives (cmd.exe /c "prog with spaces" args)
        bool q = strchr(argv[i], ' ') != NULL && strchr(argv[i], '"') == NULL;
        if (q) cmd += '"';
        cmd += argv[i];
        if (q) cmd += '"';
    }
    if (cmd.empty()) cmd = path;

    // chdir is the one action with a direct counterpart
    std::string cwd;
    for (int i = 0; fa && i < fa->count; i++)
        if (fa->act[i].op == POSIX_SPAWN_ACT_CHDIR) cwd = fa->act[i].path;

    // /proc/self/exe is how portable code says "me"; readlink above answers it
    // with GetModuleFileNameA, and a spawn of it means the same thing
    char self[MAX_PATH];
    std::string app = path;
    if (app == "/proc/self/exe" && GetModuleFileNameA(NULL, self, sizeof(self)))
        app = self;

    // an explicit application name is used verbatim -- unlike PATH search,
    // CreateProcess never appends .exe, so do it when the file is not there
    if (GetFileAttributesA(app.c_str()) == INVALID_FILE_ATTRIBUTES) {
        size_t slash = app.find_last_of("/\\");
        size_t dot   = app.find_last_of('.');
        bool   ext   = dot != std::string::npos &&
                       (slash == std::string::npos || dot > slash);
        if (!ext && GetFileAttributesA((app + ".exe").c_str()) != INVALID_FILE_ATTRIBUTES)
            app += ".exe";
    }

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    // apply the file actions that windows can express: an OPEN onto fd 1 or 2
    // becomes the child's std handle, and a DUP2 between them points the second
    // at the same file. this is how the host forwards a child's output into the
    // app log -- dropping it leaves the log empty and the failure invisible
    HANDLE hout = INVALID_HANDLE_VALUE, herr = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    for (int i = 0; fa && i < fa->count; i++) {
        const int op = fa->act[i].op;
        if (op == POSIX_SPAWN_ACT_OPEN && (fa->act[i].fd == 1 || fa->act[i].fd == 2)) {
            HANDLE h = CreateFileA(fa->act[i].path, FILE_APPEND_DATA | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                SetFilePointer(h, 0, NULL, FILE_END);
                if (fa->act[i].fd == 1) hout = h; else herr = h;
            }
        } else if (op == POSIX_SPAWN_ACT_DUP2) {
            HANDLE src = fa->act[i].fd == 1 ? hout :
                         fa->act[i].fd == 2 ? herr : INVALID_HANDLE_VALUE;
            if (src != INVALID_HANDLE_VALUE) {
                if (fa->act[i].newfd == 1) hout = src;
                if (fa->act[i].newfd == 2) herr = src;
            }
        }
    }
    // with no redirection asked for, hand the child OUR std handles. fork/exec
    // inherits descriptors for free; here a child gets them only if they are
    // named in STARTUPINFO, so without this its output goes nowhere
    HANDLE p_out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE p_err = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE p_in  = GetStdHandle(STD_INPUT_HANDLE);
    if (hout == INVALID_HANDLE_VALUE) hout = p_out;
    if (herr == INVALID_HANDLE_VALUE) herr = p_err;
    // a child handed no std handle gets no fd for it, and every printf on that
    // stream is silently discarded -- which is not a thing fork/exec can do to
    // you. NUL stands in for anything we lack, so fd 0/1/2 always exist there
    HANDLE nul = INVALID_HANDLE_VALUE;
    #define SPAWN_STD(h) do { \
        if (!(h) || (h) == INVALID_HANDLE_VALUE) { \
            if (nul == INVALID_HANDLE_VALUE) \
                nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, \
                    FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL); \
            (h) = nul; \
        } \
    } while (0)
    SPAWN_STD(hout);
    SPAWN_STD(herr);
    SPAWN_STD(p_in);
    #undef SPAWN_STD
    si.dwFlags   |= STARTF_USESTDHANDLES;
    si.hStdInput  = p_in;
    si.hStdOutput = hout;
    si.hStdError  = herr;

    // envp MUST reach the child: it carries the marker that stops it spawning
    // another host, so dropping it is an unbounded spawn loop. the block is
    // NAME=value entries back to back, terminated by an extra NUL
    std::string envblock;
    for (int i = 0; envp && envp[i]; i++) {
        envblock += envp[i];
        envblock += '\0';
    }
    if (!envblock.empty()) envblock += '\0';

    // suspended, so the child joins the kill-on-close job before it runs a
    // single instruction -- a child that started first could spawn its own
    // children outside the job, and those would survive silver
    std::string line = cmd;
    if (!spawn_process(app.c_str(), (LPSTR)line.c_str(), &si, CREATE_SUSPENDED,
                       envblock.empty() ? NULL : (LPVOID)envblock.data(),
                       cwd.empty() ? NULL : cwd.c_str(), &pi)) {
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        errno = ENOENT;
        return ENOENT;
    }
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);   // the child kept its own
    ResumeThread(pi.hThread);
    if (pid) *pid = (pid_t)pi.dwProcessId;
    CloseHandle(pi.hThread);
    // waitpid waits on the handle, so it is held until then rather than closed
    {
        std::lock_guard<std::mutex> lock(g_processMutex);
        g_childProcesses[(pid_t)pi.dwProcessId] = pi.hProcess;
    }
    return 0;
}

// an anonymous, RAM-backed fd. windows has no memfd, so this is a temp file
// that the filesystem drops as soon as the last handle closes
int memfd_create(const char* name, unsigned int flags) {
    (void)flags;
    char dir[MAX_PATH], file[MAX_PATH];
    if (!GetTempPathA(sizeof(dir), dir)) { errno = EIO; return -1; }
    if (!GetTempFileNameA(dir, name && *name ? "slv" : "slv", 0, file)) {
        errno = EIO; return -1;
    }
    HANDLE h = CreateFileA(file, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE) { errno = EIO; return -1; }
    int fd = _open_osfhandle((intptr_t)h, 0);
    if (fd < 0) { CloseHandle(h); errno = EIO; return -1; }
    return fd;
}

// environ is an alias for the crt's live block; see ports.h

// ---- sigaction ------------------------------------------------------------
// mapped onto the crt's signal() for the six signals windows actually raises.
// the posix-only ones (SIGUSR1, SIGCONT, ...) are accepted and recorded so the
// caller's setup succeeds, but nothing can deliver them here
static struct sigaction g_sigacts[32];

int sigemptyset(sigset_t* set) { if (set) *set = 0; return 0; }

int sigaddset(sigset_t* set, int sig) {
    if (!set || sig < 0 || sig >= 32) { errno = EINVAL; return -1; }
    *set |= (sigset_t)1u << sig;
    return 0;
}

int sigaction(int sig, const struct sigaction* act, struct sigaction* old) {
    if (sig < 0 || sig >= 32) { errno = EINVAL; return -1; }
    if (old) *old = g_sigacts[sig];
    if (!act) return 0;
    g_sigacts[sig] = *act;
    switch (sig) {   // the ones the crt can actually raise
        case SIGABRT: case SIGFPE: case SIGILL:
        case SIGINT:  case SIGSEGV: case SIGTERM:
            if (act->sa_handler) signal(sig, act->sa_handler);
            break;
        default:
            break;
    }
    return 0;
}

int unsetenv(const char* name) {
    if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
    // BOTH views, exactly as setenv writes both: clearing only the win32 block
    // left getenv still reporting the old value out of the crt's copy
    { char kv[4096];
      snprintf(kv, sizeof(kv), "%s=", name);
      _putenv(kv); }
    return SetEnvironmentVariableA(name, NULL) ? 0 : -1;
}

int fsync(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    return FlushFileBuffers(h) ? 0 : -1;
}

// process-control knobs with no windows counterpart (death signals, names);
// reporting success keeps the caller's setup path intact
int prctl(int option, ...) { (void)option; return 0; }

// symbol names for a captured backtrace; one heap block, as glibc does it
char** backtrace_symbols(void* const* buffer, int size) {
    if (!buffer || size <= 0) return NULL;
    size_t need = (size_t)size * (sizeof(char*) + 32);
    char** out  = (char**)malloc(need);
    if (!out) return NULL;
    char* text = (char*)(out + size);
    for (int i = 0; i < size; i++) {
        out[i] = text;
        int n = snprintf(text, 32, "[0x%p]", buffer[i]);
        text += (n > 0 ? n : 0) + 1;
    }
    return out;
}

// the trailing path component, as libgen's basename does it
char* basename(char* path) {
    if (!path || !*path) return (char*)".";
    char* a = strrchr(path, '/');
    char* b = strrchr(path, '\\');
    char* p = a > b ? a : b;
    return p ? p + 1 : path;
}

int sigaltstack(const stack_t* ss, stack_t* old) {
    (void)ss;
    if (old) memset(old, 0, sizeof(*old));
    return 0;
}

ssize_t read(int fd, void* buf, size_t sz) {
    // an inotify fd is ours, not the crt's -- handing it to _read trips the
    // invalid-parameter handler on every call. one event per change batch
    {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        auto it = g_inotifyInstances.find(fd);
        if (it != g_inotifyInstances.end()) {
            if (it->second->pending.exchange(0) == 0) return 0;
            if (sz < sizeof(struct inotify_event))    return 0;
            struct inotify_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.wd   = 1;
            ev.mask = IN_MODIFY;
            memcpy(buf, &ev, sizeof(ev));
            return (ssize_t)sizeof(ev);
        }
    }
    return _read(fd, buf, sz);
}

ssize_t write(int fd, void* buf, size_t sz) {
    return _write(fd, buf, sz);
}

FILE* fdopen(int fd, const char* mode) {
    return _fdopen(fd, mode);
}

// PTHREAD_MUTEX_INITIALIZER leaves the struct zeroed, and a zeroed
// CRITICAL_SECTION cannot be entered -- init it on first use instead
typedef struct { INIT_ONCE once; CRITICAL_SECTION cs; } mutex_impl;
static_assert(sizeof(mutex_impl) <= sizeof(pthread_mutex_t),
    "pthread_mutex_t opaque storage too small");
static_assert(alignof(pthread_mutex_t) >= alignof(mutex_impl),
    "pthread_mutex_t underaligned for CRITICAL_SECTION");

static BOOL CALLBACK mutex_once_init(PINIT_ONCE o, PVOID param, PVOID* ctx) {
    (void)o; (void)ctx;
    InitializeCriticalSection((LPCRITICAL_SECTION)param);
    return TRUE;
}

static LPCRITICAL_SECTION mutex_cs(pthread_mutex_t* m) {
    mutex_impl* i = (mutex_impl*)m;
    InitOnceExecuteOnce(&i->once, mutex_once_init, &i->cs, NULL);
    return &i->cs;
}

#define MUTEX(m) mutex_cs(m)
#define CV(c)    ((PCONDITION_VARIABLE)(c))

int pthread_mutexattr_init(pthread_mutexattr_t* a) {
    if (a) *a = 0;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* a, int type) {
    if (a) *a = type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t* m, void* attr) {
    (void)attr;
    mutex_cs(m);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* m) {
    DeleteCriticalSection(MUTEX(m));
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* m) {
    EnterCriticalSection(MUTEX(m));
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* m) {
    LeaveCriticalSection(MUTEX(m));
    return 0;
}

int pthread_cond_init(pthread_cond_t* cv, void* attr) {
    InitializeConditionVariable(CV(cv));
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cv) {
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cv, pthread_mutex_t* m) {
    SleepConditionVariableCS(CV(cv), MUTEX(m), INFINITE);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cv) {
    WakeAllConditionVariable(CV(cv));
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cv) {
    WakeConditionVariable(CV(cv));
    return 0;
}

// rwlock over SRWLOCK; a zeroed struct is already a valid unlocked lock
typedef struct { SRWLOCK lock; DWORD owner; } rwlock_impl;
static_assert(sizeof(rwlock_impl) <= sizeof(pthread_rwlock_t),
    "pthread_rwlock_t opaque storage too small");
static_assert(alignof(pthread_rwlock_t) >= alignof(rwlock_impl),
    "pthread_rwlock_t underaligned for SRWLOCK");
static_assert(alignof(pthread_cond_t) >= alignof(CONDITION_VARIABLE),
    "pthread_cond_t underaligned for CONDITION_VARIABLE");
#define RW(x) ((rwlock_impl*)(x))

int pthread_rwlock_init(pthread_rwlock_t* rw, void* attr) {
    (void)attr;
    InitializeSRWLock(&RW(rw)->lock);
    RW(rw)->owner = 0;
    return 0;
}

// SRWLOCK holds no resources, so there is nothing to release
int pthread_rwlock_destroy(pthread_rwlock_t* rw) { (void)rw; return 0; }

int pthread_rwlock_rdlock(pthread_rwlock_t* rw) {
    AcquireSRWLockShared(&RW(rw)->lock);
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rw) {
    AcquireSRWLockExclusive(&RW(rw)->lock);
    RW(rw)->owner = GetCurrentThreadId();
    return 0;
}

// only the writer records itself, so anyone else must be a reader
int pthread_rwlock_unlock(pthread_rwlock_t* rw) {
    if (RW(rw)->owner == GetCurrentThreadId()) {
        RW(rw)->owner = 0;
        ReleaseSRWLockExclusive(&RW(rw)->lock);
    } else {
        ReleaseSRWLockShared(&RW(rw)->lock);
    }
    return 0;
}

unsigned __stdcall pthread_start_thunk(void* arg) {
    pthread_start_t* s = (pthread_start_t*)arg;
    s->start_routine(s->arg);
    free(s);
    return 0;
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg) {
    (void)attr;
    pthread_start_t* s = (pthread_start_t*)malloc(sizeof(pthread_start_t));
    if (!s) return -1;
    s->start_routine = start_routine;
    s->arg = arg;

    *thread = _beginthreadex(NULL, 0, pthread_start_thunk, s, 0, NULL);
    return (*thread != 0) ? 0 : -1;
}

int pthread_join(pthread_t thread, void** retval) {
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
    if (retval) *retval = NULL;
    return 0;
}

int usleep(unsigned int usec) {
    // Windows Sleep() is in milliseconds, so convert:
    DWORD msec = usec / 1000;

    // Round up to ensure non-zero micro sleeps actually sleep
    if (usec > 0 && msec == 0) msec = 1;

    Sleep(msec);
    return 0;
}

// Convert a path to a named pipe path if needed
static char* make_pipe_path(const char* path) {
    if (strncmp(path, PIPE_PREFIX, strlen(PIPE_PREFIX)) == 0) {
        return strdup(path);
    }
    
    // Convert Unix-style pipe path to Windows named pipe
    char* pipe_path = (char*)malloc(strlen(PIPE_PREFIX) + strlen(path) + 1);
    strcpy(pipe_path, PIPE_PREFIX);
    strcat(pipe_path, path);
    return pipe_path;
}

// Open function for named pipes
int open(const char* pathname, int flags, ...) {
    // a file open, not a pipe: mkfifo() is the named-pipe entry point.
    // O_NONBLOCK shares its bit with _O_TEXT, so it never reaches the CRT.
    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return _open(pathname, (flags & ~O_NONBLOCK) | _O_BINARY, mode);
}

// Select function for named pipes (we do not want to merge sockets into this api, but could)
// while nicer to look at, it would be less secure by nature
int pipe_select(int nfds, _fd_set_* readfds, _fd_set_* writefds, _fd_set_* exceptfds, struct _timeval_* timeout) {
    _fd_set_ result_read, result_write, result_except;
    int ready_count = 0;
    DWORD wait_time;
    
    // Initialize result sets
    FD_ZERO(&result_read);
    FD_ZERO(&result_write);
    FD_ZERO(&result_except);
    
    // Calculate timeout
    if (timeout) {
        wait_time = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    } else {
        wait_time = INFINITE;
    }
    
    // Check each file descriptor
    for (int fd = 0; fd < nfds; fd++) {
        HANDLE hPipe = (HANDLE)_get_osfhandle(fd);
        if (hPipe == INVALID_HANDLE_VALUE) continue;
        
        // Check if it's a pipe
        DWORD type = GetFileType(hPipe);
        if (type != FILE_TYPE_PIPE) continue;
        
        // Check read readiness
        if (readfds && FD_ISSET(fd, readfds)) {
            DWORD bytes_available = 0;
            if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytes_available, NULL)) {
                if (bytes_available > 0) {
                    FD_SET(fd, &result_read);
                    ready_count++;
                }
            }
        }
        
        // Check write readiness (named pipes are usually always writable)
        if (writefds && FD_ISSET(fd, writefds)) {
            DWORD mode;
            if (GetNamedPipeHandleState(hPipe, &mode, NULL, NULL, NULL, NULL, 0)) {
                FD_SET(fd, &result_write);
                ready_count++;
            }
        }
        
        // Check for exceptions
        if (exceptfds && FD_ISSET(fd, exceptfds)) {
            DWORD state;
            if (!GetNamedPipeHandleState(hPipe, &state, NULL, NULL, NULL, NULL, 0)) {
                FD_SET(fd, &result_except);
                ready_count++;
            }
        }
    }
    
    // If no ready descriptors and timeout specified, wait
    if (ready_count == 0 && wait_time > 0) {
        Sleep(wait_time > 100 ? 100 : wait_time);
        
        // Re-check after sleep (simplified - you may want to loop)
        for (int fd = 0; fd < nfds; fd++) {
            HANDLE hPipe = (HANDLE)_get_osfhandle(fd);
            if (hPipe == INVALID_HANDLE_VALUE) continue;
            
            if (readfds && FD_ISSET(fd, readfds)) {
                DWORD bytes_available = 0;
                if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytes_available, NULL)) {
                    if (bytes_available > 0) {
                        FD_SET(fd, &result_read);
                        ready_count++;
                    }
                }
            }
        }
    }
    
    // Copy results back
    if (readfds) *readfds = result_read;
    if (writefds) *writefds = result_write;
    if (exceptfds) *exceptfds = result_except;
    
    return ready_count;
}

int mkfifo(const char* pathname, mode_t mode) {
    HANDLE hPipe;
    char* pipe_path = make_pipe_path(pathname);
    
    // Create the named pipe
    hPipe = CreateNamedPipe(
        pipe_path,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,              // Max instances (1 for FIFO behavior)
        4096,           // Output buffer size
        4096,           // Input buffer size
        0,              // Default timeout
        NULL            // Default security
    );
    
    free(pipe_path);
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        // Set errno based on Windows error
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            errno = EEXIST;
        } else if (error == ERROR_PATH_NOT_FOUND) {
            errno = ENOENT;
        } else {
            errno = EACCES;
        }
        return -1;
    }
    
    // Close the handle - the pipe now exists and can be opened with open()
    CloseHandle(hPipe);
    
    // Mode parameter is ignored on Windows
    (void)mode;
    
    return 0;
}

int unlink(const char* f) {
    char* pipe_path = make_pipe_path(f);
    
    HANDLE hPipe = CreateFile(
        pipe_path,
        DELETE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_DELETE_ON_CLOSE,
        NULL
    );
    
    free(pipe_path);
    
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        return 0;
    }

    return _unlink((LPCTSTR)f);
}

int mkstemp(char *template_str) {
    if (!template_str) {
        errno = EINVAL;
        return -1;
    }
    
    size_t len = strlen(template_str);
    if (len < 6) {
        errno = EINVAL;
        return -1;
    }
    
    // Check that template ends with "XXXXXX"
    char *suffix = template_str + len - 6;
    if (strcmp(suffix, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    
    // Generate unique filename
    for (int attempts = 0; attempts < 1000; attempts++) {
        // Generate 6 random characters
        for (int i = 0; i < 6; i++) {
            int rand_val = rand() % 62;  // 0-61
            if (rand_val < 10) {
                suffix[i] = '0' + rand_val;           // 0-9
            } else if (rand_val < 36) {
                suffix[i] = 'A' + (rand_val - 10);    // A-Z
            } else {
                suffix[i] = 'a' + (rand_val - 36);    // a-z
            }
        }
        
        // Try to create the file exclusively
        int fd = _open(template_str, 
                       _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                       _S_IREAD | _S_IWRITE);
        
        if (fd != -1) {
            return fd;  // Success
        }
        
        // If file exists, try again with new name
        if (errno != EEXIST) {
            return -1;  // Real error
        }
    }
    
    // Too many attempts
    errno = EEXIST;
    return -1;
}











// Thread-local storage for error messages
static __thread char dlerror_buffer[512];
static __thread int dlerror_flag = 0;

// Set error message
static void set_dlerror(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(dlerror_buffer, sizeof(dlerror_buffer), format, args);
    va_end(args);
    dlerror_flag = 1;
}

// where a module's dependencies live: bin first, then lib. our own dlls
// install to bin, but a dependency can hardcode DESTINATION lib (sherpa
// does) and windows searches neither on behalf of a loaded dll. these are
// registered once and only consulted by loads that pass SEARCH_USER_DIRS,
// so no other load in the process changes behaviour
static void dll_search_dirs(void) {
    static bool done = false;
    if (done) return;
    done = true;
    static const char* sub[] = { "/install/bin", "/install/lib", 0 };
    for (int i = 0; sub[i]; i++) {
        char p[MAX_PATH];
        snprintf(p, sizeof(p), "%s%s", SILVER, sub[i]);
        for (char* c = p; *c; c++) if (*c == '/') *c = '\\';
        wchar_t w[MAX_PATH];
        if (MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH))
            AddDllDirectory(w);
    }
}

// dlopen - open a dynamic library
void* dlopen(const char* filename, int flags) {
    HMODULE module;
    DWORD load_flags = 0;
    
    // Clear any previous error
    dlerror_flag = 0;
    
    // Handle special cases
    if (filename == NULL) {
        // NULL means open the main program
        module = GetModuleHandle(NULL);
        if (!module) {
            set_dlerror("Failed to get main module handle");
        }
        return module;
    }
    
    // Set Windows load flags based on dlopen flags
    if (flags & RTLD_LAZY) {
        // Windows always does lazy loading by default
    }
    
    if (flags & RTLD_NOW) {
        // No direct equivalent - Windows resolves on demand
    }
    
    if (flags & RTLD_NOLOAD) {
        // Check if already loaded
        module = GetModuleHandle(filename);
        if (!module) {
            set_dlerror("Library not loaded: %s", filename);
        }
        return module;
    }
    
    if (flags & RTLD_NODELETE) {
        // Pin the module
        load_flags |= LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE;
    }
    
    // a module's siblings (trinity.dll, img.dll, ...) sit next to it, but the
    // loader searches the EXE's directory, not the dll's. DLL_LOAD_DIR adds
    // the loaded dll's own directory -- the nearest thing windows has to the
    // $ORIGIN rpath used elsewhere -- and USER_DIRS brings in bin and lib.
    // these cannot be mixed with LOAD_WITH_ALTERED_SEARCH_PATH
    dll_search_dirs();
    load_flags |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                  LOAD_LIBRARY_SEARCH_USER_DIRS |
                  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

    // Try to load the library
    module = LoadLibraryEx(filename, NULL, load_flags);
    
    if (!module) {
        // Try with .dll extension if not present
        if (!strstr(filename, ".dll")) {
            char dll_name[MAX_PATH];
            snprintf(dll_name, sizeof(dll_name), "%s.dll", filename);
            module = LoadLibraryEx(dll_name, NULL, load_flags);
        }
        
        if (!module) {
            DWORD error = GetLastError();
            set_dlerror("Failed to load library %s: error %lu", filename, error);
        }
    }
    
    return module;
}

// dlsym - get symbol address from a dynamic library
void* dlsym(void* handle, const char* symbol) {
    FARPROC proc;
    
    // Clear any previous error
    dlerror_flag = 0;
    
    if (handle == RTLD_DEFAULT) {
        // Search all loaded modules
        HANDLE process = GetCurrentProcess();
        HMODULE modules[1024];
        DWORD needed;
        
        if (EnumProcessModules(process, modules, sizeof(modules), &needed)) {
            for (unsigned int i = 0; i < (needed / sizeof(HMODULE)); i++) {
                proc = GetProcAddress(modules[i], symbol);
                if (proc) return (void*)proc;
            }
        }
        set_dlerror("Symbol not found: %s", symbol);
        return NULL;
    }
    
    if (handle == RTLD_NEXT) {
        // Not easily implementable on Windows
        set_dlerror("RTLD_NEXT not supported on Windows");
        return NULL;
    }
    
    // Normal symbol lookup
    proc = GetProcAddress((HMODULE)handle, symbol);
    
    if (!proc) {
        // Try with underscore prefix (common for C symbols)
        char underscore_symbol[256];
        snprintf(underscore_symbol, sizeof(underscore_symbol), "_%s", symbol);
        proc = GetProcAddress((HMODULE)handle, underscore_symbol);
        
        if (!proc) {
            DWORD error = GetLastError();
            set_dlerror("Symbol not found: %s, error %lu", symbol, error);
        }
    }
    
    return (void*)proc;
}

// dlclose - close a dynamic library
int dlclose(void* handle) {
    // Clear any previous error
    dlerror_flag = 0;
    
    if (!handle || handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
        return 0;  // Nothing to close
    }
    
    if (FreeLibrary((HMODULE)handle)) {
        return 0;  // Success
    }
    
    DWORD error = GetLastError();
    set_dlerror("Failed to unload library: error %lu", error);
    return -1;
}

// dlerror - get error message from last dl* operation
char* dlerror(void) {
    if (dlerror_flag) {
        dlerror_flag = 0;  // Clear error after reading
        return dlerror_buffer;
    }
    return NULL;
}

// backtrace - capture return addresses, skipping this frame
int backtrace(void** buffer, int size) {
    if (!buffer || size <= 0) return 0;
    return (int)CaptureStackBackTrace(1, (DWORD)size, buffer, NULL);
}

// backtrace_symbols_fd - resolve to name+offset, write one per line
void backtrace_symbols_fd(void* const* buffer, int size, int fd) {
    static std::atomic<bool> sym_ready(false);
    HANDLE proc = GetCurrentProcess();
    bool expected = false;
    if (sym_ready.compare_exchange_strong(expected, true)) {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        SymInitialize(proc, NULL, TRUE);
    }
    // msdn pattern: ULONG64 backing keeps SYMBOL_INFO aligned
    ULONG64 store[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME + sizeof(ULONG64) - 1) / sizeof(ULONG64)];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)store;
    char line[512];
    for (int i = 0; i < size; i++) {
        DWORD64 addr = (DWORD64)(uintptr_t)buffer[i];
        DWORD64 disp = 0;
        int n;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = MAX_SYM_NAME;
        if (SymFromAddr(proc, addr, &disp, sym))
            n = snprintf(line, sizeof line, "%s+0x%llx [0x%llx]\n",
                sym->Name, (unsigned long long)disp, (unsigned long long)addr);
        else
            n = snprintf(line, sizeof line, "[0x%llx]\n", (unsigned long long)addr);
        if (n > 0) _write(fd, line, n);
    }
}


#else
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ports.h>
#define _timeval_ timeval

int inotify_close(int fd) {
    return close(fd);
}

#endif


__int64_t _epoch_millis() {
    struct _timeval_ tv;
    gettimeofday((struct _timeval_*)&tv, 0L);
    return (__int64_t)(tv.tv_sec) * 1000 + (__int64_t)(tv.tv_usec) / 1000;
}

// build intermediates belong with the build, not the system temp
// folder -- that gets swept out from under a build without warning.
// its OWN folder, though: install/ is the tree, not a scratch bin
const char* temp_dir(void) {
    static const char* d    = SILVER "/install/tmp";
    static int         made = 0;
    if (!made) {
        made = 1;
#ifdef _WIN32
        CreateDirectoryA(d, NULL);
#else
        mkdir(d, 0755);
#endif
    }
    return d;
}
