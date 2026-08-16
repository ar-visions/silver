#ifndef _PORTS_
#define _PORTS_
#ifndef AU_EXPORT
#ifdef _WIN32
#define AU_EXPORT __attribute__((dllexport))
#else
#define AU_EXPORT
#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

// portable dirent / gettimeofday functionality
#ifdef _WIN32
typedef struct DIR DIR;

#define PATH_MAX 4096

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef __int32 ssize_t;
#endif

#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>

// glibc spells this with underscores; shared code uses it
typedef int64_t __int64_t;

#define utimbuf _utimbuf
#define utime   _utime

// the ucrt declares off_t from <sys/types.h> only when non-standard names are
// enabled; llvm's headers want it either way, so fill the gap when it is off
// mirrors the ucrt's own condition in <sys/types.h>, inverted: define it only
// in the case where they don't
#if !((defined _CRT_DECLARE_NONSTDC_NAMES && _CRT_DECLARE_NONSTDC_NAMES) ||       (!defined _CRT_DECLARE_NONSTDC_NAMES && !__STDC__))
typedef long long off_t;
#endif
typedef uint16_t mode_t;

struct dirent {
    char d_name[260];  // Name of the file
    int  d_type;       // File type (DT_DIR, DT_REG, etc.)
};

// API
AU_EXPORT DIR*           opendir (const char* path);
AU_EXPORT struct dirent* readdir (DIR* d);
AU_EXPORT int            closedir(DIR* d);

// Optional helpers for d_type
#define DT_UNKNOWN 0
#define DT_REG     1
#define DT_DIR     2

struct _timeval_ {
    long tv_sec;
    long tv_usec;
};

AU_EXPORT int gettimeofday(struct _timeval_* tp, void* tzp);

#define WNOHANG 1

// Process ID type
typedef int pid_t;

// Protection flags
#define PROT_NONE       0x00
#define PROT_READ       0x01
#define PROT_WRITE      0x02
#define PROT_EXEC       0x04

// Mapping flags
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20  // aka MAP_ANON on some systems

// Error value
#define MAP_FAILED      ((void *)-1)

          char*       strdup  (const char* s);
AU_EXPORT int         chdir   (const char* path);
AU_EXPORT int         symlink (const char* target, const char* linkpath);
AU_EXPORT int         mkdir   (const char* path, mode_t mode);
AU_EXPORT char*       realpath(const char* path, char* resolved_path);
AU_EXPORT char*       dirname (char* path);
AU_EXPORT char*       basename(char* path);
AU_EXPORT ssize_t     readlink(const char *path, char *buf, size_t bufsiz);
AU_EXPORT int         lstat   (const char* path, struct _stat* st);
AU_EXPORT char*       getcwd  (char* buf, size_t size);
AU_EXPORT int         execvp  (const char *file, char *const argv[]);
AU_EXPORT int         setpgid (pid_t pid, pid_t pgid);
AU_EXPORT const char* strsignal(int sig);
AU_EXPORT pid_t       fork    ();
AU_EXPORT int         execlp  (const char* file, const char* arg0, ...);
// only reachable from a forkpty child, which never happens here
AU_EXPORT int         execl   (const char* path, const char* arg0, ...);
AU_EXPORT int         pipe    (int pipefd[2]);
AU_EXPORT int         dup2    (int oldfd, int newfd);
AU_EXPORT int         close   (int fd);
AU_EXPORT ssize_t     read    (int fd, void* buf, size_t sz);
AU_EXPORT ssize_t     write   (int fd, void* buf, size_t sz);
          FILE*       fdopen  (int fd, const char* mode);
// the crt spells this one with an underscore
#define               fileno(f) _fileno(f)
AU_EXPORT int         usleep  (unsigned int usec);
AU_EXPORT int         open    (const char* pathname, int flags, ... /* mode_t mode */);
AU_EXPORT pid_t       wait    (int* status);
AU_EXPORT pid_t       waitpid (pid_t pid, int* status, int options);
AU_EXPORT void*       mmap    (void* addr, size_t length, int prot, int flags, int fd, long long offset);
AU_EXPORT int         munmap  (void* addr, size_t length);
AU_EXPORT int         mkstemp (char* template_str);
          int         unlink  (const char* f);  // ucrt spells it const
AU_EXPORT int         mkfifo  (const char* pathname, mode_t mode);
// _access mode bits; windows has no execute permission to test
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define X_OK 0
// generated headers define an access macro; declare the function past it
#pragma push_macro("access")
#undef access
AU_EXPORT int         access  (const char* path, int mode);
#pragma pop_macro("access")
// the CRT spells these with an underscore
AU_EXPORT FILE*       popen   (const char* cmd, const char* mode);
AU_EXPORT int         pclose  (FILE* stream);
AU_EXPORT int         dup     (int fd);
AU_EXPORT int         isatty  (int fd);
AU_EXPORT pid_t       getpid  ();
AU_EXPORT int         ftruncate(int fd, long long length);
AU_EXPORT int         kill    (pid_t pid, int sig);
AU_EXPORT int         unsetenv(const char* name);
AU_EXPORT int         fsync   (int fd);
AU_EXPORT int         prctl   (int option, ...);
AU_EXPORT void*       dlopen  (const char* filename, int flags);
AU_EXPORT void*       dlsym   (void* handle, const char* symbol);
AU_EXPORT int         dlclose (void* handle);
AU_EXPORT char*       dlerror ();

// execinfo.h on glibc; dbghelp supplies the symbol names here
AU_EXPORT int         backtrace           (void** buffer, int size);
AU_EXPORT void        backtrace_symbols_fd(void* const* buffer, int size, int fd);
AU_EXPORT char**      backtrace_symbols   (void* const* buffer, int size);


AU_EXPORT void  register_child(pid_t pid, void* handle);

#ifndef S_IFLNK
#define S_IFLNK 0120000  // symbolic link
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

typedef struct {
    void*       handle;
    char        path[4096];
    uint32_t    mask;
} inotify_watch;

struct inotify_event {
    int         wd;           // THIS is the ID of the watch
    uint32_t    mask;
    uint32_t    cookie;
    uint32_t    len;
    char        name[];      // Optional file name (if watching a dir)
};

// inotify flags
#define IN_CLOEXEC    0x80000
#define IN_NONBLOCK   0x800

// inotify event masks
#define IN_ACCESS        0x00000001
#define IN_MODIFY        0x00000002
#define IN_ATTRIB        0x00000004
#define IN_CLOSE_WRITE   0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_OPEN          0x00000020
#define IN_MOVED_FROM    0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_CREATE        0x00000100
#define IN_DELETE        0x00000200
#define IN_DELETE_SELF   0x00000400
#define IN_MOVE_SELF     0x00000800

AU_EXPORT int inotify_init1     (int flags);
AU_EXPORT int inotify_init      ();
AU_EXPORT int inotify_add_watch (int fd, const char* pathname, uint32_t mask);
AU_EXPORT int inotify_rm_watch  (int fd, int wd);
AU_EXPORT int inotify_close     (int fd);

AU_EXPORT int setenv(const char* name, const char* value, int overwrite);

// dlopen() flags
#define RTLD_LAZY       0x00001  // Lazy symbol resolution
#define RTLD_NOW        0x00002  // Immediate symbol resolution
#define RTLD_BINDING_MASK   0x3  // Mask for binding flags
#define RTLD_NOLOAD     0x00004  // Don't load, just check if loaded
#define RTLD_DEEPBIND   0x00008  // Place lookup scope ahead of global scope
#define RTLD_GLOBAL     0x00100  // Symbols available for subsequently loaded objects
#define RTLD_LOCAL      0x00000  // Symbols not available for subsequently loaded objects
#define RTLD_NODELETE   0x01000  // Don't unload during dlclose

// Special handle values
#define RTLD_DEFAULT    ((void*)0)       // Search default libraries
#define RTLD_NEXT       ((void*)-1L)     // Search subsequent libraries


// Event flags
#define IN_ACCESS        0x00000001  // File was accessed
#define IN_MODIFY        0x00000002  // File was modified
#define IN_ATTRIB        0x00000004  // Metadata changed
#define IN_CLOSE_WRITE   0x00000008  // Writable file closed
#define IN_CLOSE_NOWRITE 0x00000010  // Unwritable file closed
#define IN_OPEN          0x00000020  // File was opened
#define IN_MOVED_FROM    0x00000040  // File moved out of watched dir
#define IN_MOVED_TO      0x00000080  // File moved into watched dir
#define IN_CREATE        0x00000100  // File/directory created
#define IN_DELETE        0x00000200  // File/directory deleted
#define IN_DELETE_SELF   0x00000400  // Watched file/directory was itself deleted
#define IN_MOVE_SELF     0x00000800  // Watched file/directory was itself moved

// Composite flags
#define IN_CLOSE         (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)
#define IN_MOVE          (IN_MOVED_FROM | IN_MOVED_TO)

// Special flags (not implemented in our Windows emu, but harmless)
#define IN_ALL_EVENTS    0x00000FFF
#define IN_DONT_FOLLOW   0x02000000
#define IN_MASK_ADD      0x20000000
#define IN_ISDIR         0x40000000
#define IN_ONESHOT       0x80000000


// Signal definitions
// no job control here: nothing ever reports as stopped or signalled
#define WUNTRACED            2
#define WIFSTOPPED(status)   (0)
#define WSTOPSIG(status)     (0)
#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) >> 8) & 0xff)
#define WIFSIGNALED(status)  (((status) & 0x7f) != 0 && ((status) & 0x7f) != 0x7f)
#define WTERMSIG(status)     ((status) & 0x7f)

// SIGTERM/SIGINT/SIGSEGV/SIGABRT, sig_atomic_t, SIG_DFL/SIG_IGN, signal() and
// raise() all come from the crt's <signal.h> (included above). only the posix
// signals it has no notion of are added, further down
#ifndef SIGKILL
#define SIGKILL  9
#endif

// dladdr - symbol lookup for a code address
typedef struct {
    const char* dli_fname;
    void*       dli_fbase;
    const char* dli_sname;
    void*       dli_saddr;
} Dl_info;
AU_EXPORT int dladdr(const void* addr, Dl_info* info);

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define stat _stat

// the ucrt defines S_IFMT and the type bits but none of the S_IS* macros, so
// each needs its own guard -- one #ifndef S_IFMT around the lot skips them all
#ifndef S_IFMT
#define S_IFMT   0170000    // Bitmask for the file type bitfields
#endif
#ifndef S_IFDIR
#define S_IFDIR  0040000    // Directory
#endif
#ifndef S_IFREG
#define S_IFREG  0100000    // Regular file
#endif
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#endif


//typedef CRITICAL_SECTION    pthread_mutex_t;
//typedef CONDITION_VARIABLE  pthread_cond_t;
// __align: win32 lock objects hold pointers and must sit on an 8-byte
// boundary; a bare byte array is align-1 and lands wherever it fits
typedef union {
    void*   __align;
    uint8_t __opaque[64];
} pthread_mutex_t;

typedef union {
    void*   __align;
    uint8_t __opaque[32];
} pthread_cond_t;

// SRWLOCK plus the exclusive owner, so unlock knows which release to call
typedef union {
    void*   __align;
    uint8_t __opaque[24];
} pthread_rwlock_t;

// all-zero is a valid unlocked state; the mutex init-once's on first lock
#define PTHREAD_MUTEX_INITIALIZER  {0}
#define PTHREAD_COND_INITIALIZER   {0}
#define PTHREAD_RWLOCK_INITIALIZER {0}

typedef uintptr_t           pthread_t;
typedef void*               pthread_attr_t;

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} pthread_start_t;


// CRITICAL_SECTION is recursive already, so the attr is advisory
typedef int pthread_mutexattr_t;
#define PTHREAD_MUTEX_RECURSIVE 1
AU_EXPORT int pthread_mutexattr_init   (pthread_mutexattr_t* a);
AU_EXPORT int pthread_mutexattr_settype(pthread_mutexattr_t* a, int type);

AU_EXPORT int pthread_mutex_init      (pthread_mutex_t* m, void* attr);
AU_EXPORT int pthread_mutex_destroy   (pthread_mutex_t* m);
AU_EXPORT int pthread_mutex_lock      (pthread_mutex_t* m);
AU_EXPORT int pthread_mutex_unlock    (pthread_mutex_t* m);
AU_EXPORT int pthread_cond_init       (pthread_cond_t* cv, void* attr);
AU_EXPORT int pthread_cond_destroy    (pthread_cond_t* cv);
AU_EXPORT int pthread_cond_wait       (pthread_cond_t* cv, pthread_mutex_t* m);
AU_EXPORT int pthread_cond_broadcast  (pthread_cond_t* cv);
AU_EXPORT int pthread_cond_signal     (pthread_cond_t* cv);
AU_EXPORT int pthread_rwlock_init     (pthread_rwlock_t* rw, void* attr);
AU_EXPORT int pthread_rwlock_destroy  (pthread_rwlock_t* rw);
AU_EXPORT int pthread_rwlock_rdlock   (pthread_rwlock_t* rw);
AU_EXPORT int pthread_rwlock_wrlock   (pthread_rwlock_t* rw);
AU_EXPORT int pthread_rwlock_unlock   (pthread_rwlock_t* rw);

AU_EXPORT unsigned __stdcall pthread_start_thunk(void* arg);
AU_EXPORT int pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
AU_EXPORT int pthread_join(pthread_t thread, void** retval);
AU_EXPORT pthread_t pthread_self();
AU_EXPORT int pthread_equal(pthread_t a, pthread_t b);
AU_EXPORT int pthread_getname_np(pthread_t thread, char* name, size_t len);

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_APPEND    0x0008
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_EXCL      0x0400
// O_NONBLOCK is defined in <unistd.h>, the header an import names for it
#define O_CLOEXEC   0x0080  // _O_NOINHERIT: the handle is not inherited


#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

#define _SC_PAGESIZE         1
#define _SC_PHYS_PAGES       2
#define _SC_NPROCESSORS_ONLN 3

// fcntl lives in <fcntl.h>, the header an import names for it
AU_EXPORT long sysconf      (int name);
// only ever a pointer here, so a forward declaration is enough; pulling in
// <time.h> put struct tm into every module that includes us
struct timespec;
AU_EXPORT int  clock_gettime(int clk, struct timespec* ts);
AU_EXPORT int  strcasecmp   (const char* a, const char* b);

// same layout winsock uses, and the same guard so both can appear
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

// winsize/ioctl live in <sys/ioctl.h>, the header that declares them

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

// flock - whole-file advisory lock, mapped onto LockFileEx
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
AU_EXPORT int flock(int fd, int operation);

// windows is little-endian, so these convert nothing
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)

// stat never reports a socket on windows
#define S_ISSOCK(m) (0)

AU_EXPORT char* strndup(const char* s, size_t n);
AU_EXPORT int   rmdir  (const char* path);
AU_EXPORT int   pthread_detach(pthread_t thread);

#define FD_SETSIZE  64

#ifdef _WIN64
typedef __int64 _SOCKET_;
#else
typedef int     _SOCKET_;
#endif


// Unix-like fd_set structure with bitmap
#undef  FD_SETSIZE
#define FD_SETSIZE 64

typedef struct _fd_set_ {
    unsigned long fds_bits[(FD_SETSIZE + 31) / 32];
} _fd_set_;

// Macros for fd_set manipulation
#define FD_ZERO(set)      memset((set), 0, sizeof(_fd_set_))
#define FD_SET(fd, set)   ((set)->fds_bits[(fd) / 32] |=  (1UL << ((fd) % 32)))
#define FD_CLR(fd, set)   ((set)->fds_bits[(fd) / 32] &= ~(1UL << ((fd) % 32)))
#define FD_ISSET(fd, set) ((set)->fds_bits[(fd) / 32] &   (1UL << ((fd) % 32)))

// Named pipe specific helpers
#define PIPE_PREFIX "\\\\.\\pipe\\"

// Convert a path to a named pipe path if needed
// named pipes only; winsock owns the name `select` for sockets
AU_EXPORT int     pipe_select(int nfds, _fd_set_* readfds, _fd_set_* writefds, _fd_set_* exceptfds, struct _timeval_* timeout);

// User permissions
#define S_IRUSR 0000400  // Read permission, owner
#define S_IWUSR 0000200  // Write permission, owner  
#define S_IXUSR 0000100  // Execute permission, owner

// Group permissions  
#define S_IRGRP 0000040  // Read permission, group
#define S_IWGRP 0000020  // Write permission, group
#define S_IXGRP 0000010  // Execute permission, group

// Other permissions
#define S_IROTH 0000004  // Read permission, others
#define S_IWOTH 0000002  // Write permission, others
#define S_IXOTH 0000001  // Execute permission, others

// Common combinations
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)  // 0700
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)  // 0070  
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)  // 0007

// Your specific combination
#define S_IRUSR_IWUSR_IRGRP_IWGRP_IROTH_IWOTH \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)  // 0664


// ---- posix surface windows lacks ------------------------------------------
// merged here rather than shadowing system headers: sources include <ports.h>

#ifndef SIGHUP
#define SIGHUP   1
#define SIGTRAP  5
#define SIGBUS   7
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGSTOP 17
#define SIGCONT 18
#endif
#ifndef SA_RESTART
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_SIGINFO   0x00000004
#define SA_NOCLDSTOP 0x00000001
#endif
#define SS_DISABLE 2

typedef unsigned long sigset_t;

struct sigaction {
    void   (*sa_handler)(int);
    sigset_t sa_mask;
    int      sa_flags;
    void   (*sa_sigaction)(int, void*, void*);
};

typedef struct { void* ss_sp; int ss_flags; size_t ss_size; } stack_t;

struct winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
};
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

#define O_NONBLOCK 0x4000   // tracked by the shim; windows has no such mode
#define F_GETFL 3
#define F_SETFL 4

// prctl options have no counterpart; the call is a no-op that reports success
#define PR_SET_PTRACER      0x59616d61
#define PR_SET_PTRACER_ANY  ((unsigned long)-1)
#define PR_SET_PDEATHSIG    1
#define PR_SET_NAME         15

#define POSIX_SPAWN_ACT_DUP2  1
#define POSIX_SPAWN_ACT_OPEN  2
#define POSIX_SPAWN_ACT_CHDIR 3

typedef struct {
    int count;
    struct {
        int  op;
        int  fd, newfd;
        char path[512];
        int  oflag, mode;
    } act[16];
} posix_spawn_file_actions_t;

typedef struct { int flags; } posix_spawnattr_t;

AU_EXPORT int sigaction (int sig, const struct sigaction* act, struct sigaction* old);
AU_EXPORT int sigemptyset(sigset_t* set);
AU_EXPORT int sigaddset (sigset_t* set, int sig);
AU_EXPORT int sigaltstack(const stack_t* ss, stack_t* old);
AU_EXPORT int ioctl     (int fd, unsigned long request, ...);
AU_EXPORT int fcntl     (int fd, int cmd, ...);
AU_EXPORT int forkpty   (int* amaster, char* name, void* termp, struct winsize* win);
AU_EXPORT int memfd_create(const char* name, unsigned int flags);

AU_EXPORT int posix_spawn_file_actions_init       (posix_spawn_file_actions_t*);
AU_EXPORT int posix_spawn_file_actions_destroy    (posix_spawn_file_actions_t*);
AU_EXPORT int posix_spawn_file_actions_adddup2    (posix_spawn_file_actions_t*, int fd, int newfd);
AU_EXPORT int posix_spawn_file_actions_addopen    (posix_spawn_file_actions_t*, int fd, const char* path, int oflag, unsigned mode);
AU_EXPORT int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t*, const char* path);
AU_EXPORT int posix_spawn(pid_t* pid, const char* path,
                          const posix_spawn_file_actions_t* fa,
                          const posix_spawnattr_t* attr,
                          char* const argv[], char* const envp[]);


#else

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#ifdef __linux__
#include <sys/inotify.h>
#endif
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <dlfcn.h>
#include <utime.h>

#ifndef __APPLE__
typedef __int64_t ssize_t;
#else
#include <libkern/OSByteOrder.h>
#define htole16(x) OSSwapHostToLittleInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#endif
typedef fd_set  _fd_set_;

#endif

// the per-platform scratch directory; no trailing separator.
// callers build temp paths from this instead of hardcoding /tmp
AU_EXPORT const char* temp_dir(void);

#ifdef __cplusplus
}
#endif

#endif
#undef bool
