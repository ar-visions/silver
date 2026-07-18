#include <cstdio>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// appshare: AF_UNIX + SCM_RIGHTS fd-passing shim. the CMSG_* macros are not
// callable from silver, so the socket plumbing lives here as plain C. all
// messages are a fixed 32-byte frame (8 x u32 words); SLOT also carries one
// dma-buf fd out of band. every fd/socket is non-blocking. the symbols are
// defined on every platform (stubs off linux) so AppLink links everywhere.
// ---------------------------------------------------------------------------
#if defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define APPSHARE_MSG_BYTES 32

static void appshare_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

extern "C" int appshare_listen(const char* path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(path);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
    if (listen(s, 1) < 0) { close(s); return -1; }
    appshare_nonblock(s);
    return s;
}

// non-blocking accept: -1 = none yet / error, else the connected fd
extern "C" int appshare_accept(int s) {
    int c = accept(s, 0, 0);
    if (c < 0) return -1;
    appshare_nonblock(c);
    return c;
}

extern "C" int appshare_connect(const char* path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
    appshare_nonblock(s);
    return s;
}

// blocking-until-complete write of one fixed frame (small, so no partial loop
// concerns beyond EINTR/EAGAIN spin). returns 1 ok, 0 peer gone, -1 error.
static int appshare_write_all(int s, const void* buf, unsigned len) {
    const char* p = (const char*)buf; unsigned off = 0;
    while (off < len) {
        ssize_t n = write(s, p + off, len - off);
        if (n > 0) { off += (unsigned)n; continue; }
        if (n == 0) return 0;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        if (errno == EPIPE) return 0;
        return -1;
    }
    return 1;
}

extern "C" int appshare_send(int s, const void* msg) {
    return appshare_write_all(s, msg, APPSHARE_MSG_BYTES);
}

// send one 32-byte frame + one fd via SCM_RIGHTS. returns 1 ok, 0 peer gone, -1.
extern "C" int appshare_send_fd(int s, int fd, const void* msg) {
    struct iovec io; io.iov_base = (void*)msg; io.iov_len = APPSHARE_MSG_BYTES;
    char cbuf[CMSG_SPACE(sizeof(int))]; memset(cbuf, 0, sizeof(cbuf));
    struct msghdr mh; memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &io; mh.msg_iovlen = 1;
    mh.msg_control = cbuf; mh.msg_controllen = sizeof(cbuf);
    struct cmsghdr* cm = CMSG_FIRSTHDR(&mh);
    cm->cmsg_level = SOL_SOCKET; cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cm), &fd, sizeof(int));
    for (;;) {
        ssize_t n = sendmsg(s, &mh, 0);
        if (n > 0) return 1;
        if (n == 0) return 0;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        if (errno == EPIPE) return 0;
        return -1;
    }
}

// non-blocking recv of one 32-byte frame. returns 1 got one, 0 none yet,
// -1 peer closed / error. if a fd rides along it is stored in *fd_out (else -1).
extern "C" int appshare_recv_fd(int s, void* buf, int* fd_out) {
    if (fd_out) *fd_out = -1;
    struct iovec io; io.iov_base = buf; io.iov_len = APPSHARE_MSG_BYTES;
    char cbuf[CMSG_SPACE(sizeof(int))]; memset(cbuf, 0, sizeof(cbuf));
    struct msghdr mh; memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &io; mh.msg_iovlen = 1;
    mh.msg_control = cbuf; mh.msg_controllen = sizeof(cbuf);
    ssize_t n;
    for (;;) {
        n = recvmsg(s, &mh, 0);
        if (n >= 0) break;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (n == 0) return -1;                 // peer closed
    if (n != APPSHARE_MSG_BYTES) return -1;
    struct cmsghdr* cm = CMSG_FIRSTHDR(&mh);
    if (cm && cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS &&
        cm->cmsg_len == CMSG_LEN(sizeof(int)) && fd_out) {
        memcpy(fd_out, CMSG_DATA(cm), sizeof(int));
    }
    return 1;
}

extern "C" void appshare_close(int fd) { if (fd >= 0) close(fd); }

// one frame then plen payload bytes on the same stream
extern "C" int appshare_send_msg(int s, const void* msg, const void* payload, int plen) {
    int r = appshare_write_all(s, msg, APPSHARE_MSG_BYTES);
    if (r != 1) return r;
    if (plen > 0 && payload)
        return appshare_write_all(s, payload, (unsigned)plen);
    return 1;
}

// spin-read exactly len promised payload bytes; 1 ok, -1 closed
extern "C" int appshare_recv_payload(int s, void* buf, int len) {
    char* p = (char*)buf; int off = 0;
    while (off < len) {
        ssize_t n = read(s, p + off, (size_t)(len - off));
        if (n > 0) { off += (int)n; continue; }
        if (n == 0) return -1;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        return -1;
    }
    return 1;
}
#else
// non-linux stubs: the feature is linux-only (dma-buf), but the symbols must
// exist so AppLink links. every call reports "no connection".
extern "C" int  appshare_listen(const char* p)                     { return -1; }
extern "C" int  appshare_accept(int s)                             { return -1; }
extern "C" int  appshare_connect(const char* p)                    { return -1; }
extern "C" int  appshare_send(int s, const void* m)                { return -1; }
extern "C" int  appshare_send_fd(int s, int fd, const void* m)     { return -1; }
extern "C" int  appshare_recv_fd(int s, void* b, int* fdo)         { return -1; }
extern "C" int  appshare_send_msg(int s, const void* m, const void* p, int n) { return -1; }
extern "C" int  appshare_recv_payload(int s, void* b, int n)       { return -1; }
extern "C" void appshare_close(int fd)                             { }
#endif
