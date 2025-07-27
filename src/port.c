#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <port.h>
#include <windows.h>
#include <time.h>

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

typedef struct {
    pid_t pid;
    HANDLE handle;
} child_proc_t;

static child_proc_t child_processes[32];
static int child_count = 0;

static HANDLE get_handle_from_pid(pid_t pid) {
    for (int i = 0; i < child_count; ++i) {
        if (child_processes[i].pid == pid)
            return child_processes[i].handle;
    }
    return NULL;
}

pid_t waitpid(pid_t pid, int* status, int options) {
    DWORD exit_code;
    HANDLE h = NULL;

    if (pid == -1) {
        for (int i = 0; i < child_count; ++i) {
            h = child_processes[i].handle;
            pid = child_processes[i].pid;
            if (WaitForSingleObject(h, (options == WNOHANG ? 0 : INFINITE)) == WAIT_OBJECT_0)
                break;
            h = NULL;
        }
        if (!h) return 0;
    } else {
        h = get_handle_from_pid(pid);
        if (!h) return -1;
        if (WaitForSingleObject(h, (options == WNOHANG ? 0 : INFINITE)) != WAIT_OBJECT_0)
            return 0;
    }

    if (!GetExitCodeProcess(h, &exit_code))
        return -1;

    if (status) *status = (exit_code << 8);
    CloseHandle(h);

    // remove from table
    for (int i = 0; i < child_count; ++i) {
        if (child_processes[i].pid == pid) {
            for (int j = i; j < child_count - 1; ++j)
                child_processes[j] = child_processes[j + 1];
            child_count--;
            break;
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

#else
#include <sys/time.h>
#endif

int64_t epoch_millis() {
    struct timeval tv;
    gettimeofday(&tv, 0L);
    return (int64_t)(tv.tv_sec) * 1000 + (int64_t)(tv.tv_usec) / 1000;
}
