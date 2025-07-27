#ifndef _PORT_
#define _PORT_

// portable dirent / gettimeofday functionality
#ifdef _WIN32
typedef struct DIR DIR;

#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef __int32 ssize_t;
#endif

#include <stdint.h>

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

char* strdup(const char* s);
int   chdir(const char* path);
int   symlink(const char* target, const char* linkpath);
int   mkdir(const char* path, mode_t mode);
char* realpath(const char* path, char* resolved_path);
char* dirname(char* path);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int lstat(const char* path, struct _stat* st);
char* getcwd(char* buf, size_t size);
int execvp(const char *file, char *const argv[]);

#ifndef S_IFLNK
#define S_IFLNK 0120000  // symbolic link
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

pid_t wait          (int* status);
pid_t waitpid       (pid_t pid, int* status, int options);
void  register_child(pid_t pid, void* handle);
void* mmap          (void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap        (void* addr, size_t length);

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

int         setpgid(pid_t pid, pid_t pgid);
const char* strsignal(int sig);
pid_t       fork();
int         execlp(const char* file, const char* arg0, ...);
//pid_t       waitpid(pid_t pid, int* status, int options);
int         pipe(int pipefd[2]);
int         dup2(int oldfd, int newfd);
int         close(int fd);
//int         read(int fd, void* buf, size_t sz);
//int         write(int fd, void* buf, size_t sz);
FILE*       fdopen(int fd, const char* mode);
int         usleep(unsigned int usec);

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

int open(const char* pathname, int flags, ... /* mode_t mode */);

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_APPEND    0x0008
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_EXCL      0x0400
#define O_NONBLOCK  0x4000  // Windows doesn't support this natively, so we just track it

#define FD_SETSIZE  64

typedef struct socket_set {
    unsigned int fd_count;
    int fd_array[FD_SETSIZE];
} socket_set;

#define FD_ZERO(set) memset((set)->bits, 0, sizeof((set)->bits))
#define FD_SET(fd, set)   ((set)->bits[(fd) / 8] |=  (1 << ((fd) % 8)))
#define FD_CLR(fd, set)   ((set)->bits[(fd) / 8] &= ~(1 << ((fd) % 8)))
#define FD_ISSET(fd, set) (((set)->bits[(fd) / 8] &  (1 << ((fd) % 8))) != 0)

int select(int nfds, socket_set* readfds, socket_set* writefds, socket_set* exceptfds, struct socket_set* timeout);

#else

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mmap.h>
#include <sys/inotify.h>

typedef __int64 ssize_t;
typedef fd_set  _fd_set_;

#endif
#endif
#endif