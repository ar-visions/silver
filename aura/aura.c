// the helper's records: a GPU frame with its dma-buf fd, its history,
// or its title (the record's width is the byte count that follows)
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#define BR_API __attribute__((visibility("default")))

typedef struct {
    uint32_t magic, seq, width, height, format, stride, offset, pad;
    uint64_t id, modifier;
} BrowserRecord;

// 1 = a frame (the fd is the caller's), 2 = history (w = back, h = forward),
// 3 = a title (text, w bytes), 0 = nothing waiting, -1 = broken
BR_API int aura_recv_frame(int sock, uint64_t* id, int* w, int* h, uint32_t* format,
                              uint64_t* modifier, uint32_t* stride, uint32_t* offset, int* fd,
                              char* text, int cap)
{
    BrowserRecord rec;
    struct iovec iov = { &rec, sizeof rec };
    union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int))]; } ctl;
    struct msghdr msg = { 0 };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &ctl;
    msg.msg_controllen = sizeof ctl;
    ssize_t n = recvmsg(sock, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
    if (n < 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
    if (n != (ssize_t)sizeof rec) return -1;
    *fd = -1;
    for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c))
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
            memcpy(fd, CMSG_DATA(c), sizeof(int));
    *id = rec.id; *w = (int)rec.width; *h = (int)rec.height; *format = rec.format;
    *modifier = rec.modifier; *stride = rec.stride; *offset = rec.offset;
    if (rec.magic == 0x5356414eu) return 2;
    if (rec.magic == 0x4c544954u || rec.magic == 0x52444441u) {
        int len = (int)rec.width;
        if (len >= cap) return -1;
        if (len > 0 && recv(sock, text, (size_t)len, MSG_WAITALL) != len) return -1;
        text[len] = 0;
        return rec.magic == 0x4c544954u ? 3 : 4;
    }
    if (rec.magic != 0x4d415246u || *fd < 0) return -1;
    return 1;
}
