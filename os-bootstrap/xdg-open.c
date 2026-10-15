// orbiter-os xdg-open: hand the address to orbiter's socket
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char** argv) {
    if (argc < 2 || !*argv[1]) {
        fprintf(stderr, "usage: xdg-open <url>\n");
        return 1;
    }
    // orbiter's trinity socket, where trinity.cc makes it
    const char* rt = getenv("XDG_RUNTIME_DIR");
    struct sockaddr_un su;
    memset(&su, 0, sizeof(su));
    su.sun_family = AF_UNIX;
    snprintf(su.sun_path, sizeof(su.sun_path), "%s/trinity-orbiter.sock",
             (rt && *rt) ? rt : "/tmp");
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, (struct sockaddr*)&su, sizeof(su)) != 0) {
        fprintf(stderr, "xdg-open: orbiter is not running (%s)\n",
                su.sun_path);
        return 3;
    }
    char line[8192];
    int n = snprintf(line, sizeof(line), "app open %s\n", argv[1]);
    if (n <= 0 || n >= (int)sizeof(line) || write(fd, line, n) != n) {
        close(fd);
        return 4;
    }
    // orbiter answers once the line is taken
    char reply[64];
    ssize_t r = read(fd, reply, sizeof(reply) - 1);
    close(fd);
    if (r <= 0) return 4;
    reply[r] = 0;
    return strncmp(reply, "ok", 2) == 0 ? 0 : 4;
}
