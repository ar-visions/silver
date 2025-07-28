

#ifdef _WIN32

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ports.h>
#include <windows.h>
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

BOOL EnumProcessModules(
    HANDLE  hProcess,
    HMODULE *lphModule,
    DWORD   cb,
    LPDWORD lpcbNeeded
);

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

char* realpath(const char* path, char* resolved_path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    char temp[MAX_PATH];
    DWORD len = GetFullPathNameA(path, MAX_PATH, temp, NULL);
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

    st->st_size = ((off_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;

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

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
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

    HANDLE hFile = fd_to_handle(fd);
    if (hFile == INVALID_HANDLE_VALUE) return NULL;

    HANDLE mapping = CreateFileMapping(hFile, NULL, protect, 0, 0, NULL);
    if (!mapping) return NULL;

    void* map = MapViewOfFile(mapping, access, (DWORD)((offset >> 32) & 0xFFFFFFFF), (DWORD)(offset & 0xFFFFFFFF), length);
    CloseHandle(mapping);
    return map;
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
};

struct InotifyContext {
    std::map<int, std::unique_ptr<WatchInfo>> watches;
    std::mutex mutex;
    int nextWd = 1;
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
            FALSE,  // Don't watch subdirectories
            notifyFilter,
            NULL,
            &overlapped,
            NULL)) {
            
            DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            
            if (waitResult == WAIT_OBJECT_0) {  // Change detected
                if (GetOverlappedResult(watch->dirHandle, &overlapped, &bytesReturned, FALSE)) {
                    // Process the changes here
                    // In a real implementation, you'd queue events to be read by inotify_read
                    FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)buffer;
                    
                    while (true) {
                        // Process each change notification
                        // Map Windows actions to inotify events
                        
                        if (info->NextEntryOffset == 0)
                            break;
                        info = (FILE_NOTIFY_INFORMATION*)((BYTE*)info + info->NextEntryOffset);
                    }
                }
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
    std::lock_guard<std::mutex> lock(g_instanceMutex);
    
    auto it = g_inotifyInstances.find(fd);
    if (it == g_inotifyInstances.end()) {
        return -1;
    }
    
    auto& ctx = it->second;
    ctx->closed = true;
    
    // Remove all watches
    std::vector<int> wds;
    {
        std::lock_guard<std::mutex> ctxLock(ctx->mutex);
        for (const auto& pair : ctx->watches) {
            wds.push_back(pair.first);
        }
    }  // Lock automatically released here
    
    for (int wd : wds) {
        inotify_rm_watch(fd, wd);
    }
    
    g_inotifyInstances.erase(it);
    
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


int execvp(const char* file, char* const argv[]) {
    // Build command line: "prog arg1 arg2 ..."
    size_t len = strlen(file) + 3;
    for (int i = 1; argv[i]; ++i)
        len += strlen(argv[i]) + 3;

    char* cmd = (char*)malloc(len);
    if (!cmd) {
        errno = ENOMEM;
        return -1;
    }

    strcpy(cmd, file);
    for (int i = 1; argv[i]; ++i) {
        strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }

    // Use _spawnvp to emulate execvp
    int result = _spawnvp(_P_OVERLAY, file, argv);

    free(cmd);

    if (result == -1) {
        errno = ENOENT;
        return -1;
    }

    return result;
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
#define SIGTERM  15
#define SIGKILL  9
#define SIGINT   2
#define SIGSEGV  11
#define SIGABRT  6

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
    
    auto it = g_childProcesses.find(pid);
    if (it == g_childProcesses.end()) {
        errno = ECHILD;
        return -1;
    }
    
    HANDLE hProcess = it->second;
    DWORD exitCode;
    
    // Wait for process to finish
    DWORD waitResult = WaitForSingleObject(hProcess, INFINITE);
    
    if (waitResult == WAIT_FAILED) {
        errno = EINVAL;
        return -1;
    }
    
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        errno = EINVAL;
        return -1;
    }
    
    // Clean up
    CloseHandle(hProcess);
    g_childProcesses.erase(it);
    
    // Convert Windows exit code to POSIX-style status
    if (status != NULL) {
        if (exitCode == STATUS_ACCESS_VIOLATION) {
            *status = SIGSEGV;  // Terminated by signal
        } else if (exitCode == STATUS_CONTROL_C_EXIT) {
            *status = SIGINT;
        } else if (exitCode >= 0x80000000) {
            // Abnormal termination
            *status = SIGTERM;
        } else {
            // Normal exit
            *status = (exitCode & 0xff) << 8;
        }
    }
    
    return pid;
}

pid_t wait(int* status) {
    return waitpid(-1, status, 0);
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
int execlp(const char* file, const char* arg0, ...) {
    // Build command line
    std::string cmdLine = file;
    
    va_list args;
    va_start(args, arg0);
    
    const char* arg = arg0;
    while (arg != NULL) {
        cmdLine += " ";
        cmdLine += arg;
        arg = va_arg(args, const char*);
    }
    va_end(args);
    
    // Create process
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcessA(
        NULL,
        (LPSTR)cmdLine.c_str(),
        NULL,
        NULL,
        TRUE,  // Inherit handles
        0,
        NULL,
        NULL,
        &si,
        &pi)) {
        return -1;
    }
    
    // execlp replaces the current process, so we exit
    ExitProcess(0);
    return 0;  // Never reached
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
    return _close(fd);
}

ssize_t read(int fd, void* buf, size_t sz) {
    return _read(fd, buf, sz);
}

ssize_t write(int fd, void* buf, size_t sz) {
    return _write(fd, buf, sz);
}

FILE* _fdopen(int fd, const char* mode) {
    return fdopen(fd, mode);
}

#define MUTEX(m) ((LPCRITICAL_SECTION)(m))
#define CV(c)    ((PCONDITION_VARIABLE)(c))

int pthread_mutex_init(pthread_mutex_t* m, void* attr) {
    InitializeCriticalSection(MUTEX(m));
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
    HANDLE hPipe;
    DWORD dwOpenMode = 0;
    DWORD dwPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
    char* pipe_path = make_pipe_path(pathname);
    
    // Convert Unix flags to Windows flags
    if ((flags & O_RDWR) == O_RDWR) {
        dwOpenMode = GENERIC_READ | GENERIC_WRITE;
    } else if (flags & O_WRONLY) {
        dwOpenMode = GENERIC_WRITE;
    } else {
        dwOpenMode = GENERIC_READ;
    }
    
    // Try to open existing pipe first
    hPipe = CreateFile(
        pipe_path,
        dwOpenMode,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    // If failed and O_CREAT is set, create the pipe
    if (hPipe == INVALID_HANDLE_VALUE && (flags & O_CREAT)) {
        hPipe = CreateNamedPipe(
            pipe_path,
            PIPE_ACCESS_DUPLEX,
            dwPipeMode,
            PIPE_UNLIMITED_INSTANCES,
            4096,
            4096,
            0,
            NULL
        );
        
        if (hPipe != INVALID_HANDLE_VALUE) {
            // Wait for client to connect if we created the pipe
            ConnectNamedPipe(hPipe, NULL);
        }
    }
    
    free(pipe_path);
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        return -1;
    }
    
    // Convert HANDLE to file descriptor
    int fd = _open_osfhandle((intptr_t)hPipe, flags);
    return fd;
}

// Select function for named pipes (we do not want to merge sockets into this api, but could)
// while nicer to look at, it would be less secure by nature
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) {
    fd_set result_read, result_write, result_except;
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

int unlink(char* f) {
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





#else
#include <sys/time.h>

int inotify_close(int fd) {
    close(fd);
}


#endif

int64_t epoch_millis() {
    struct timeval tv;
    gettimeofday((struct _timeval_*)&tv, 0L);
    return (int64_t)(tv.tv_sec) * 1000 + (int64_t)(tv.tv_usec) / 1000;
}
