// quark's C: Node started as the app's main process with argv as the
// user typed it (silver's own flag reading stops at the app folder),
// a shared socket on fd 3; the fds of a frame's textures read off it
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define QK_API __attribute__((visibility("default")))

// the process's own command line, one string a word
static char** own_argv(int* count)
{
    FILE* f = fopen("/proc/self/cmdline", "rb");
    if (!f) return NULL;
    size_t cap = 65536, n = 0;
    char* buf = malloc(cap);
    n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    int words = 0;
    for (size_t i = 0; i < n; i++) if (!buf[i]) words++;
    char** v = calloc((size_t)words + 1, sizeof(char*));
    int k = 0;
    for (size_t i = 0; i < n && k < words;) {
        v[k++] = buf + i;
        i += strlen(buf + i) + 1;
    }
    *count = words;
    return v;
}

// node --import <reg> <boot> <app> <the words after app>; fd 3 is ours
QK_API int quark_start_node(const char* node, const char* reg, const char* boot,
                            const char* app, int* pid_out)
{
    int n = 0;
    char** own = own_argv(&n);
    int at = -1;
    for (int i = 1; i < n; i++)
        if (!strcmp(own[i], app)) { at = i; break; }
    int tail = at >= 0 ? n - at - 1 : 0;
    char** argv = calloc((size_t)tail + 7, sizeof(char*));
    int k = 0;
    argv[k++] = (char*)node;
    argv[k++] = "--import";
    argv[k++] = (char*)reg;
    argv[k++] = (char*)boot;
    argv[k++] = (char*)app;
    for (int i = 0; i < tail; i++) argv[k++] = own[at + 1 + i];
    argv[k] = NULL;
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[0], 3);
        if (fds[0] != 3) close(fds[0]);
        close(fds[1]);
        execv(node, argv);
        _exit(1);
    }
    close(fds[0]);
    if (pid < 0) { close(fds[1]); return -1; }
    *pid_out = (int)pid;
    int flags = fcntl(fds[1], F_GETFL, 0);
    fcntl(fds[1], F_SETFL, flags | O_NONBLOCK);
    return fds[1];
}

// fds ride on the bytes they came with: any read keeps them here
static int held_fds[16];
static int held_count;

static void keep_fds(struct msghdr* msg)
{
    for (struct cmsghdr* c = CMSG_FIRSTHDR(msg); c; c = CMSG_NXTHDR(msg, c))
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
            int count = (int)((c->cmsg_len - CMSG_LEN(0)) / sizeof(int));
            for (int i = 0; i < count; i++) {
                int fd;
                memcpy(&fd, CMSG_DATA(c) + i * sizeof(int), sizeof(int));
                if (held_count < 16) held_fds[held_count++] = fd;
                else close(fd);
            }
        }
}

// what the socket holds now, at most cap bytes; 0 none, -1 closed
QK_API int quark_read(int sock, char* buf, int cap)
{
    struct iovec iov = { buf, (size_t)cap };
    union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int) * 8)]; } ctl;
    struct msghdr msg = { 0 };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &ctl;
    msg.msg_controllen = sizeof ctl;
    ssize_t n = recvmsg(sock, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (n < 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
    if (n == 0) return -1;
    keep_fds(&msg);
    return (int)n;
}

// the fds that came with the bytes read so far (a share line's two)
QK_API int quark_recv_fds(int sock, int* fds, int want)
{
    (void)sock;
    int got = 0;
    while (got < want && held_count > 0) {
        fds[got++] = held_fds[0];
        memmove(held_fds, held_fds + 1, (size_t)(held_count - 1) * sizeof(int));
        held_count--;
    }
    return got;
}
