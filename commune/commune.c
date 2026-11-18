// a line to orbiter's socket, and a look at a port on this machine
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define COMMUNE_API __attribute__((visibility("default")))

// 1 sent, 0 not
COMMUNE_API int commune_sock_send(const char* path, const char* line)
{
    if (!path || !*path || !line || !*line) return 0;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return 0;
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

// 1 when something listens on port here (read, never dialled)
COMMUNE_API int commune_port_open(int port)
{
    const char* files[] = { "/proc/net/tcp", "/proc/net/tcp6" };
    for (int f = 0; f < 2; f++) {
        FILE* fp = fopen(files[f], "r");
        if (!fp) continue;
        char ln[512];
        while (fgets(ln, sizeof(ln), fp)) {
            char loc[128], rem[128];
            unsigned st;
            if (sscanf(ln, " %*d: %127s %127s %x", loc, rem, &st) != 3)
                continue;
            char* c = strrchr(loc, ':');
            if (c && st == 0x0A && (int)strtol(c + 1, 0, 16) == port) {
                fclose(fp);
                return 1;
            }
        }
        fclose(fp);
    }
    return 0;
}
