// portable dirent / gettimeofday functionality
#ifdef _WIN32
typedef struct DIR DIR;

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

// macros to inspect process exit status
#define WIFEXITED(s)    (((s) & 0xFF) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xFF)

typedef int pid_t;

pid_t wait          (int* status);
pid_t waitpid       (pid_t pid, int* status, int options);
void  register_child(pid_t pid, HANDLE handle);

#else
#include <sys/time.h>
#include <sys/wait.h>
#endif