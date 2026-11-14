// a line to a unix socket by its path: orbiter's, shared into a
// bound view by its real path (the name form wants our own runtime
// folder). 1 sent, 0 not
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define NAV_API __attribute__((visibility("default")))

NAV_API int nav_sock_send(const char* path, const char* line)
{
    if (!path || !*path || !line || !*line) return 0;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    strncpy(su.sun_path, path, sizeof(su.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&su, sizeof(su)) != 0) {
        close(fd);
        return 0;
    }
    size_t  n = strlen(line);
    ssize_t w = write(fd, line, n);
    close(fd);
    return (w == (ssize_t)n) ? 1 : 0;
}
