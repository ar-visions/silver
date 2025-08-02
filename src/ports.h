#ifndef _PORTS_
#define _PORTS_

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
#include <sys/stat.h>
#include <sys/utime.h>

#define utimbuf _utimbuf
#define utime   _utime

typedef long long off_t;
typedef uint16_t mode_t;

struct dirent {
    char d_name[260];  // Name of the file
    int  d_type;       // File type (DT_DIR, DT_REG, etc.)
};

// API
DIR*           opendir (const char* path);
struct dirent* readdir (DIR* d);
int            closedir(DIR* d);

// Optional helpers for d_type
#define DT_UNKNOWN 0
#define DT_REG     1
#define DT_DIR     2

struct _timeval_ {
    long tv_sec;
    long tv_usec;
};

int gettimeofday(struct _timeval_* tp, void* tzp);

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
int         chdir   (const char* path);
int         symlink (const char* target, const char* linkpath);
int         mkdir   (const char* path, mode_t mode);
char*       realpath(const char* path, char* resolved_path);
char*       dirname (char* path);
ssize_t     readlink(const char *path, char *buf, size_t bufsiz);
int         lstat   (const char* path, struct _stat* st);
char*       getcwd  (char* buf, size_t size);
int         execvp  (const char *file, char *const argv[]);
int         setpgid (pid_t pid, pid_t pgid);
const char* strsignal(int sig);
pid_t       fork    ();
int         execlp  (const char* file, const char* arg0, ...);
int         pipe    (int pipefd[2]);
int         dup2    (int oldfd, int newfd);
int         close   (int fd);
ssize_t     read    (int fd, void* buf, size_t sz);
ssize_t     write   (int fd, void* buf, size_t sz);
FILE*       fdopen  (int fd, const char* mode);
int         usleep  (unsigned int usec);
int         open    (const char* pathname, int flags, ... /* mode_t mode */);
pid_t       wait    (int* status);
pid_t       waitpid (pid_t pid, int* status, int options);
void*       mmap    (void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int         munmap  (void* addr, size_t length);
int         mkstemp (char* template_str);
int         unlink  (char* f);
int         mkfifo  (const char* pathname, mode_t mode);
void*       dlopen  (const char* filename, int flags);
void*       dlsym   (void* handle, const char* symbol);
int         dlclose (void* handle);
char*       dlerror ();


void  register_child(pid_t pid, void* handle);

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

int inotify_init1     (int flags);
int inotify_init      ();
int inotify_add_watch (int fd, const char* pathname, uint32_t mask);
int inotify_rm_watch  (int fd, int wd);
int inotify_close     (int fd);

int setenv(const char* name, const char* value, int overwrite);

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

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define stat _stat

#ifndef S_IFMT
#define S_IFMT   0170000    // Bitmask for the file type bitfields
#define S_IFDIR  0040000    // Directory
#define S_IFREG  0100000    // Regular file
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)


//typedef CRITICAL_SECTION    pthread_mutex_t;
//typedef CONDITION_VARIABLE  pthread_cond_t;
typedef struct {
    uint8_t __opaque[64];
} pthread_mutex_t;

typedef struct {
    uint8_t __opaque[32];
} pthread_cond_t;

typedef uintptr_t           pthread_t;
typedef void*               pthread_attr_t;

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} pthread_start_t;


int pthread_mutex_init      (pthread_mutex_t* m, void* attr);
int pthread_mutex_destroy   (pthread_mutex_t* m);
int pthread_mutex_lock      (pthread_mutex_t* m);
int pthread_mutex_unlock    (pthread_mutex_t* m);
int pthread_cond_init       (pthread_cond_t* cv, void* attr);
int pthread_cond_destroy    (pthread_cond_t* cv);
int pthread_cond_wait       (pthread_cond_t* cv, pthread_mutex_t* m);
int pthread_cond_broadcast  (pthread_cond_t* cv);
int pthread_cond_signal     (pthread_cond_t* cv);

unsigned __stdcall pthread_start_thunk(void* arg);
int pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
int pthread_join(pthread_t thread, void** retval);

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_APPEND    0x0008
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_EXCL      0x0400
#define O_NONBLOCK  0x4000  // Windows doesn't support this natively, so we just track it

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
int     select(int nfds, _fd_set_* readfds, _fd_set_* writefds, _fd_set_* exceptfds, struct _timeval_* timeout);

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

#endif

#else

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <dlfcn.h>
#include <utime.h>

typedef __int64_t ssize_t;
typedef fd_set  _fd_set_;

#endif

#ifdef __cplusplus
}
#endif

#endif