// orbiter-os /init: mount the drive, enter its folder, run
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/loop.h>

#ifdef NOLIBC
// the kernel's nolibc (arm64 sticks) lacks these few
static void pause(void) { for (;;) sleep(3600); }

static struct dirent* readdir(DIR* d) {
    static struct dirent e;
    struct dirent* r;
    return readdir_r(d, &e, &r) || !r ? NULL : r;
}

static ssize_t pread(int fd, void* b, size_t n, off_t at) {
    return lseek(fd, at, SEEK_SET) < 0 ? -1 : read(fd, b, n);
}

static int execv(const char* f, char* const argv[]) {
    return execve(f, argv, environ);
}

static char* strtok(char* s, const char* sep) {
    static char* at;
    if (s) at = s;
    while (at && *at && strchr(sep, *at)) at++;
    if (!at || !*at) return NULL;
    char* w = at;
    while (*at && !strchr(sep, *at)) at++;
    if (*at) *at++ = 0;
    return w;
}

// the folder's real path, through /proc
static char* realpath(const char* p, char* out) {
    int fd = open(p, O_PATH);
    if (fd < 0) return NULL;
    char link[64];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    long n = __nolibc_syscall4(__NR_readlinkat, AT_FDCWD, link, out, 4095);
    close(fd);
    if (n <= 0) return NULL;
    out[n] = 0;
    return out;
}
#endif

extern char** environ;
static char cmdline[4096];

// pid 1 must never return: say why and wait
static void die(const char* why, const char* what) {
    fprintf(stderr, "orbiter-os init: %s %s\n", why, what ? what : "");
    for (;;) pause();
}

// a key=value word's value on the kernel command line
static const char* arg(const char* key, char* out, size_t cap) {
    size_t n = strlen(key);
    const char* p = cmdline;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        const char* e = p;
        while (*e && *e != ' ' && *e != '\n') e++;
        if ((size_t)(e - p) > n && !strncmp(p, key, n) && p[n] == '=') {
            size_t m = (size_t)(e - p) - n - 1;
            if (m >= cap) m = cap - 1;
            memcpy(out, p + n + 1, m);
            out[m] = 0;
            return out;
        }
        p = e;
    }
    return NULL;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 5069068c-dbe7-... to its 16 bytes
static int parse_uuid(const char* s, unsigned char u[16]) {
    int k = 0;
    for (; *s && k < 32; s++) {
        if (*s == '-') continue;
        int v = hexval(*s);
        if (v < 0) return 0;
        if (k & 1) u[k / 2] |= (unsigned char)v;
        else       u[k / 2]  = (unsigned char)(v << 4);
        k++;
    }
    return k == 32;
}

// the block device whose ext4 superblock has this uuid
static int find_uuid(const unsigned char want[16], char* dev, size_t cap) {
    DIR* d = opendir("/sys/class/block");
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(dev, cap, "/dev/%s", e->d_name);
        int fd = open(dev, O_RDONLY);
        if (fd < 0) continue;
        // superblock at 1024: magic at +56, uuid at +104
        unsigned char sb[1024];
        found = pread(fd, sb, sizeof sb, 1024) == (ssize_t)sizeof sb &&
                sb[56] == 0x53 && sb[57] == 0xEF && !memcmp(sb + 104, want, 16);
        close(fd);
    }
    closedir(d);
    return found;
}

// the swap partition whose header has this uuid: page one ends
// in SWAPSPACE2, the uuid at 1036
static int find_swap(const unsigned char want[16], char* dev, size_t cap) {
    DIR* d = opendir("/sys/class/block");
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(dev, cap, "/dev/%s", e->d_name);
        int fd = open(dev, O_RDONLY);
        if (fd < 0) continue;
        unsigned char pg[4096];
        found = pread(fd, pg, sizeof pg, 0) == (ssize_t)sizeof pg &&
                !memcmp(pg + 4086, "SWAPSPACE2", 10) &&
                !memcmp(pg + 1036, want, 16);
        close(fd);
    }
    closedir(d);
    return found;
}

// orbiter.swap=UUID=<uuid>: a swap partition; else a file in
// the root, as /swapfile
static void swap_on(void) {
    char sw[256], dev[128];
    if (!arg("orbiter.swap", sw, sizeof sw)) return;
    const char* at = sw;
    if (!strncmp(sw, "UUID=", 5)) {
        unsigned char u[16];
        if (!parse_uuid(sw + 5, u) || !find_swap(u, dev, sizeof dev)) {
            printf("init: no swap with %s\n", sw);
            return;
        }
        at = dev;
    }
#ifdef NOLIBC
    long r = __nolibc_syscall2(__NR_swapon, at, 0);
#else
    long r = syscall(SYS_swapon, at, 0);
#endif
    printf("init: swap %s: %s\n", at, r ? "failed" : "on");
}

// the kernel log and pid 1's output, on the drive
static void save_logs(void) {
    int out = open("/boot/boot.log", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
    if (out >= 0) { dup2(out, 1); dup2(out, 2); close(out); }
    if (fork() != 0) return;
    // the child: every kernel line, synced, until power off
    int km = open("/dev/kmsg", O_RDONLY);
    int kf = open("/boot/kmsg.log", O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
    if (km < 0 || kf < 0) _exit(1);
    char line[8192];
    for (;;) {
        ssize_t n = read(km, line, sizeof line);
        if (n > 0) { if (write(kf, line, (size_t)n) < 0) _exit(1); }
        else if (n < 0) usleep(1000);
    }
}

#ifndef MODPROBE
#define MODPROBE "/usr/sbin/modprobe"
#endif

// a program, waited for
static void run(char* const argv[]) {
    pid_t p = fork();
    if (p == 0) { execv(argv[0], argv); _exit(127); }
    if (p > 0) waitpid(p, NULL, 0);
}

#ifndef PHONE_ROOT
#define PHONE_ROOT "/lib/phones"
#endif

// a phone's own drivers, never another phone's: its folder under
// PHONE_ROOT (own module index and blocklist), found by the
// product orbiter.phone names in the folder's products file
static char phone_base[256];
static char phone_conf[300];

// a whole small file, 0 ended; its length, or -1
static int read_all(const char* p, char* buf, int cap) {
    int fd = open(p, O_RDONLY);
    if (fd < 0) return -1;
    int n = 0;
    for (;;) {
        ssize_t k = read(fd, buf + n, (size_t)(cap - 1 - n));
        if (k <= 0) break;
        n += (int)k;
        if (n >= cap - 1) break;
    }
    close(fd);
    buf[n] = 0;
    return n;
}

// is w one of the list's words (spaces or lines between)
static int has_word(const char* list, const char* w) {
    size_t n = strlen(w);
    const char* p = list;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        const char* e = p;
        while (*e && *e != ' ' && *e != '\n') e++;
        if ((size_t)(e - p) == n && !strncmp(p, w, n)) return 1;
        p = e;
    }
    return 0;
}

static void phone_find(void) {
    char want[64], prods[256];
    if (!arg("orbiter.phone", want, sizeof want)) return;
    DIR* d = opendir(PHONE_ROOT);
    struct dirent* e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char pp[300];
        snprintf(pp, sizeof pp, "%s/%s/products", PHONE_ROOT, e->d_name);
        if (read_all(pp, prods, sizeof prods) <= 0) continue;
        if (!has_word(prods, want)) continue;
        snprintf(phone_base, sizeof phone_base, "%s/%s", PHONE_ROOT, e->d_name);
        snprintf(phone_conf, sizeof phone_conf, "%s/modprobe.d", phone_base);
        break;
    }
    if (d) closedir(d);
    if (phone_base[0])
        fprintf(stderr, "orbiter-os init: %s drivers from %s\n", want, phone_base);
    else
        fprintf(stderr, "orbiter-os init: no phone folder for %s\n", want);
}

// a module file into the kernel (the ramdisk has no modprobe)
static long load_module(int fd) {
#ifdef NOLIBC
    return __nolibc_syscall3(__NR_finit_module, fd, "", 0);
#else
    return syscall(SYS_finit_module, fd, "", 0);
#endif
}

// an NTFS volume serial as blkid prints it: 16 hex digits
static int parse_serial(const char* s, unsigned long long* v) {
    *v = 0;
    int k = 0;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0'
              : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10
              : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0) return 0;
        *v = (*v << 4) | (unsigned)d;
        k++;
    }
    return k == 16;
}

// the block device whose NTFS boot sector has this serial (at 0x48)
static int find_ntfs(unsigned long long want, char* dev, size_t cap) {
    DIR* d = opendir("/sys/class/block");
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(dev, cap, "/dev/%s", e->d_name);
        int fd = open(dev, O_RDONLY);
        if (fd < 0) continue;
        unsigned char bs[512];
        if (pread(fd, bs, sizeof bs, 0) == (ssize_t)sizeof bs &&
            !memcmp(bs + 3, "NTFS    ", 8)) {
            unsigned long long v = 0;
            for (int i = 7; i >= 0; i--) v = (v << 8) | bs[0x48 + i];
            found = v == want;
        }
        close(fd);
    }
    closedir(d);
    return found;
}

// a file as a block device: the first free loop device, its name
static int loop_attach(const char* file, char* dev, size_t cap) {
    int ctl = open("/dev/loop-control", O_RDWR);
    if (ctl < 0) return 0;
    int n = ioctl(ctl, LOOP_CTL_GET_FREE);
    close(ctl);
    if (n < 0) return 0;
    snprintf(dev, cap, "/dev/loop%d", n);
    int lf = open(dev, O_RDWR), ff = open(file, O_RDWR);
    struct loop_config lc;
    memset(&lc, 0, sizeof lc);
    lc.fd = ff;
    int ok = lf >= 0 && ff >= 0 && ioctl(lf, LOOP_CONFIGURE, &lc) == 0;
    if (ff >= 0) close(ff);
    if (lf >= 0) close(lf);
    return ok;
}

// a word into a file (configfs, sysfs)
static int put_text(const char* p, const char* s) {
    int fd = open(p, O_WRONLY);
    if (fd < 0) return -1;
    int ok = write(fd, s, strlen(s)) == (ssize_t)strlen(s);
    close(fd);
    return ok ? 0 : -1;
}

// the boot ramdisk's modules (storage, USB ...), in its list's order,
// before the partition is looked for
static void phone_ramdisk(void) {
    static char names[65536];
    if (read_all("/lib/early/modules.load", names, sizeof names) <= 0)
        return;
    int n = 0, bad = 0;
    char* p = names;
    while (*p) {
        char* e = p;
        while (*e && *e != '\n') e++;
        int last = !*e;
        *e = 0;
        if (*p) {
            char ko[300];
            snprintf(ko, sizeof ko, "/lib/early/%s.ko", p);
            int fd = open(ko, O_RDONLY);
            long r = fd >= 0 ? load_module(fd) : -1;
            if (fd >= 0) close(fd);
            // already in (EEXIST) is fine
            if (r < 0 && errno != EEXIST) {
                fprintf(stderr, "orbiter-os init: %s did not load (%d)\n", p, errno);
                bad++;
            }
            n++;
        }
        if (last) break;
        p = e + 1;
    }
    fprintf(stderr, "orbiter-os init: %d boot modules, %d failed\n", n, bad);
}

// a USB serial port on the phone (the PC's ttyACM): the kernel's
// log copied to it, init's own lines in that log
static void phone_gadget(const char* product) {
    static const char g[] = "/sys/kernel/config/usb_gadget/orbiter";
    char p[200], udc[64] = "";
    mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);
    mkdir(g, 0755);
    snprintf(p, sizeof p, "%s/idVendor", g);
    put_text(p, "0x1d6b");
    snprintf(p, sizeof p, "%s/idProduct", g);
    put_text(p, "0x0104");
    snprintf(p, sizeof p, "%s/strings/0x409", g);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/strings/0x409/manufacturer", g);
    put_text(p, "orbiter-os");
    snprintf(p, sizeof p, "%s/strings/0x409/product", g);
    put_text(p, product);
    snprintf(p, sizeof p, "%s/functions/acm.gs0", g);
    mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/configs/c.1", g);
    mkdir(p, 0755);
    char to[200];
    snprintf(to, sizeof to, "%s/configs/c.1/acm.gs0", g);
    snprintf(p, sizeof p, "%s/functions/acm.gs0", g);
    symlink(p, to);
    // the phone's USB controller, once its module has made it
    for (int i = 0; i < 50 && !udc[0]; i++) {
        DIR* d = opendir("/sys/class/udc");
        struct dirent* e;
        while (d && (e = readdir(d)))
            if (e->d_name[0] != '.') {
                snprintf(udc, sizeof udc, "%s", e->d_name);
                break;
            }
        if (d) closedir(d);
        if (!udc[0]) usleep(100000);
    }
    snprintf(p, sizeof p, "%s/UDC", g);
    if (!udc[0] || put_text(p, udc)) {
        fprintf(stderr, "orbiter-os init: no USB gadget (%s)\n", udc);
        return;
    }
    // init's lines into the kernel's log
    int km = open("/dev/kmsg", O_WRONLY);
    if (km >= 0) { dup2(km, 1); dup2(km, 2); close(km); }
    fprintf(stderr, "orbiter-os init: USB serial on %s\n", udc);
    if (fork() != 0) return;
    // the child: the whole log, from the start, to the port
    int tty = -1;
    for (int i = 0; i < 50 && tty < 0; i++) {
        tty = open("/dev/ttyGS0", O_WRONLY | O_NOCTTY);
        if (tty < 0) usleep(100000);
    }
    int in = open("/dev/kmsg", O_RDONLY);
    if (tty < 0 || in < 0) _exit(1);
    char line[8192];
    for (;;) {
        ssize_t n = read(in, line, sizeof line - 1);
        if (n > 0) {
            // a record: its text after the header's ';'
            line[n] = 0;
            char* t = strchr(line, ';');
            t = t ? t + 1 : line;
            write(tty, t, strlen(t));
            write(tty, "\r", 1);
        } else if (n < 0) usleep(1000);
    }
}

// modprobe's words to stay in the phone's folder, from argv[k]
static int phone_args(char** argv, int k) {
    if (!phone_base[0]) return k;
    argv[k++] = "-d";
    argv[k++] = phone_base;
    argv[k++] = "-C";
    argv[k++] = phone_conf;
    return k;
}

// the phone's modules.load, one at a time in its order (the
// chip's before the phone's: some depend on that order)
static void phone_early(void) {
    static char names[65536];
    char lp[300];
    if (!phone_base[0]) return;
    snprintf(lp, sizeof lp, "%s/modules.load", phone_base);
    if (read_all(lp, names, sizeof names) <= 0) return;
    int n = 0;
    char* p = names;
    while (*p) {
        char* e = p;
        while (*e && *e != '\n') e++;
        int last = !*e;
        *e = 0;
        if (*p) {
            char* argv[10];
            int k = 0;
            argv[k++] = MODPROBE;
            argv[k++] = "-q";
            k = phone_args(argv, k);
            argv[k++] = p;
            argv[k] = NULL;
            run(argv);
            n++;
        }
        if (last) break;
        p = e + 1;
    }
    fprintf(stderr, "orbiter-os init: %d phone modules in order\n", n);
}

// every device's driver: modprobe takes each id in /sys
static void load_hardware(void) {
    static char ids[512][256];
    char* argv[524] = { MODPROBE, "-q", "-b" };
    int at = phone_args(argv, 3);
    argv[at++] = "-a";
    int n = 0;
    DIR* buses = opendir("/sys/bus");
    struct dirent* b;
    while (buses && (b = readdir(buses))) {
        if (b->d_name[0] == '.') continue;
        char dp[300];
        snprintf(dp, sizeof dp, "/sys/bus/%s/devices", b->d_name);
        DIR* devs = opendir(dp);
        struct dirent* d;
        while (devs && (d = readdir(devs)) && n < 512) {
            if (d->d_name[0] == '.') continue;
            char mp[600];
            snprintf(mp, sizeof mp, "%s/%s/modalias", dp, d->d_name);
            int fd = open(mp, O_RDONLY);
            if (fd < 0) continue;
            ssize_t k = read(fd, ids[n], sizeof ids[n] - 1);
            close(fd);
            if (k <= 1) continue;
            ids[n][k - 1] = 0;
            argv[at + n] = ids[n];
            n++;
        }
        if (devs) closedir(devs);
    }
    if (buses) closedir(buses);
    argv[at + n] = NULL;
    fprintf(stderr, "orbiter-os init: %d device ids to modprobe\n", n);
    if (n) run(argv);
}

#ifndef NOLIBC
#include <sys/socket.h>
#include <linux/netlink.h>

// the kernel's device announcements (udev's own feed), opened
// before the first scan so a device added between is kept
static int uevents(void) {
    int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    struct sockaddr_nl nl = { .nl_family = AF_NETLINK, .nl_groups = 1 };
    // the first pass loads hundreds of drivers: keep every event
    int room = 16 << 20;
    if (fd >= 0) setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &room, sizeof room);
    if (fd >= 0 && bind(fd, (struct sockaddr*)&nl, sizeof nl)) { close(fd); fd = -1; }
    return fd;
}

// a device that comes later (usb keyboards and mice finish after
// the scan): its driver by its id, for as long as the system runs
static void hotplug(int fd) {
    if (fd < 0 || fork() != 0) return;
    char msg[8192];
    for (;;) {
        ssize_t n = recv(fd, msg, sizeof msg - 1, 0);
        if (n <= 0) continue;
        msg[n] = 0;
        if (strncmp(msg, "add@", 4)) continue;
        for (char* f = msg; f < msg + n; f += strlen(f) + 1)
            if (!strncmp(f, "MODALIAS=", 9)) {
                char* argv[10] = { MODPROBE, "-q", "-b" };
                int k = phone_args(argv, 3);
                argv[k++] = f + 9;
                argv[k] = NULL;
                run(argv);
            }
    }
}
#else
static int  uevents(void) { return -1; }
static void hotplug(int fd) {}
#endif

#ifndef NOLIBC
// a program left running: the system bus, iwd
static void spawn(char* const argv[], char* const envp[]) {
    pid_t p = fork();
    if (p == 0) { setsid(); execve(argv[0], argv, envp); _exit(127); }
}

// a machine id for the bus, made once
static void machine_id(void) {
    if (access("/etc/machine-id", F_OK) == 0) return;
    unsigned char r[16];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0 || read(fd, r, sizeof r) != sizeof r) { if (fd >= 0) close(fd); return; }
    close(fd);
    char hex[34];
    for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", r[i]);
    hex[32] = '\n';
    int out = open("/etc/machine-id", O_WRONLY | O_CREAT | O_TRUNC, 0444);
    if (out >= 0) { write(out, hex, 33); close(out); }
}

// Wi-Fi: the system bus, then iwd on it (orbiter drives it);
// install is the folder orbiter.init's binary is under
static void net_start(const char* init) {
    const char* b = strstr(init, "/build/");
    char iwd[600], lib[600], state[640];
    if (!b) return;
    snprintf(iwd, sizeof iwd, "%.*s/libexec/iwd", (int)(b - init), init);
    if (access(iwd, X_OK)) return;
    // iwd's crypto comes through the kernel's socket interface
    char* crypto[] = { MODPROBE, "-q", "-a", "algif_hash", "algif_skcipher",
                       "cmac", "md4", "pkcs8_key_parser", NULL };
    run(crypto);
    machine_id();
    mkdir("/run/dbus", 0755);
    char* env[] = { "PATH=/usr/sbin:/usr/bin:/sbin:/bin", NULL };
    char* bus[] = { "/usr/bin/dbus-daemon",
                    "--config-file=/etc/dbus-1/orbiter.conf",
                    "--nofork", "--nopidfile", NULL };
    spawn(bus, env);
    for (int i = 0; i < 200 && access("/run/dbus/system_bus_socket", F_OK); i++)
        usleep(10000);
    // remembered networks: <install>/var/lib/iwd
    snprintf(lib, sizeof lib, "%.*s/var", (int)(b - init), init);
    mkdir(lib, 0755);
    snprintf(state, sizeof state, "%s/lib", lib);
    mkdir(state, 0755);
    snprintf(state, sizeof state, "%s/lib/iwd", lib);
    mkdir(state, 0700);
    char* w[] = { iwd, NULL };
    spawn(w, env);
    fprintf(stderr, "orbiter-os init: started the system bus and iwd\n");
}

// the session bus: apps' tray icons register with orbiter on it
static void session_start(void) {
    static const char sock[] = "/run/dbus/session_bus_socket";
    if (access("/usr/bin/dbus-daemon", X_OK)) return;
    machine_id();
    mkdir("/run/dbus", 0755);
    char* env[] = { "PATH=/usr/sbin:/usr/bin:/sbin:/bin", NULL };
    char* bus[] = { "/usr/bin/dbus-daemon",
                    "--config-file=/etc/dbus-1/orbiter-session.conf",
                    "--nofork", "--nopidfile", NULL };
    spawn(bus, env);
    for (int i = 0; i < 200 && access(sock, F_OK); i++) usleep(10000);
    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/dbus/session_bus_socket", 1);
    fprintf(stderr, "orbiter-os init: started the session bus\n");
}
#else
static void net_start(const char* init) {}
static void session_start(void) {}
#endif

// orbiter starts in silver's folder: orbiter.init up to /install/
static void work_folder(const char* init) {
    static const char key[] = "/install/";
    char dir[600];
    for (int i = 0; init[i] && i < (int)sizeof dir - 1; i++) {
        int k = 0;
        while (key[k] && init[i + k] == key[k]) k++;
        if (key[k]) continue;
        for (int j = 0; j < i; j++) dir[j] = init[j];
        dir[i > 0 ? i : 1] = 0;
        if (i == 0) dir[0] = '/';
        if (chdir(dir)) fprintf(stderr, "orbiter-os init: cannot enter %s\n", dir);
        return;
    }
}

// orbiter.modules=a,b:k=v: modprobe a, then b with k=v
static void load_modules(void) {
    char list[1024];
    if (!arg("orbiter.modules", list, sizeof list)) return;
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        char* params = strchr(name, ':');
        if (params) *params++ = 0;
        char* argv[10] = { MODPROBE };
        int k = phone_args(argv, 1);
        argv[k++] = name;
        argv[k++] = params;
        argv[k] = NULL;
        run(argv);
    }
    // nvidia has no udev here: its nodes, major 195
    if (access("/proc/driver/nvidia", F_OK) == 0) {
        mknod("/dev/nvidiactl", S_IFCHR | 0666, makedev(195, 255));
        mknod("/dev/nvidia-modeset", S_IFCHR | 0666, makedev(195, 254));
        DIR* g = opendir("/proc/driver/nvidia/gpus");
        struct dirent* e;
        int n = 0;
        while (g && (e = readdir(g)))
            if (e->d_name[0] != '.') {
                char node[32];
                snprintf(node, sizeof node, "/dev/nvidia%d", n);
                mknod(node, S_IFCHR | 0666, makedev(195, n++));
            }
        if (g) closedir(g);
        // TEMP trace: can each nvidia node be opened
        const char* nodes[] = { "/dev/nvidiactl", "/dev/nvidia0", "/dev/nvidia-modeset" };
        for (int i = 0; i < 3; i++) {
            int t = open(nodes[i], O_RDWR);
            fprintf(stderr, "init: open %s -> %s\n", nodes[i], t >= 0 ? "ok" : strerror(errno));
            if (t >= 0) close(t);
        }
        fprintf(stderr, "init: nvidia gpus %d\n", n);
    }
}

int main(void) {
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    int con = open("/dev/console", O_RDWR);
    if (con >= 0) { dup2(con, 0); dup2(con, 1); dup2(con, 2); }

    int cf = open("/proc/cmdline", O_RDONLY);
    if (cf >= 0) {
        ssize_t n = read(cf, cmdline, sizeof cmdline - 1);
        cmdline[n > 0 ? n : 0] = 0;
        close(cf);
    }
    // a phone: its boot modules, then the USB log port
    char phone[64];
    if (arg("orbiter.phone", phone, sizeof phone)) {
        phone_ramdisk();
        phone_gadget(phone);
    }
    char uuid_s[64], dir[256], init[512], image[512];
    // in a Windows partition: the system is an ext4 image in it
    int in_ntfs = arg("orbiter.image", image, sizeof image) != NULL;
    if (!in_ntfs && !arg("orbiter.uuid", uuid_s, sizeof uuid_s))
        die("no orbiter.uuid", NULL);
    if (!arg("orbiter.root", dir, sizeof dir))
        strcpy(dir, in_ntfs ? "/" : "/orbiter-os");
    if (!arg("orbiter.init", init, sizeof init)) die("no orbiter.init", NULL);

    // the nvme drive appears a moment after boot: 10 s
    char dev[128];
    int found = 0;
    mkdir("/mnt", 0755);
    if (in_ntfs) {
        char ser_s[64];
        unsigned long long ser;
        if (!arg("orbiter.ntfs", ser_s, sizeof ser_s) || !parse_serial(ser_s, &ser))
            die("no orbiter.ntfs serial", NULL);
        for (int i = 0; i < 100 && !found; i++) {
            found = find_ntfs(ser, dev, sizeof dev);
            if (!found) usleep(100000);
        }
        if (!found) die("no NTFS partition with serial", ser_s);
        int kf = open("/lib/ntfs3.ko", O_RDONLY);
        if (kf >= 0) { load_module(kf); close(kf); }
        mkdir("/host", 0755);
        if (mount(dev, "/host", "ntfs3", 0, NULL))
            die("cannot mount (Windows left it in use? shut it down fully)", dev);
        char file[1024], loop[64];
        snprintf(file, sizeof file, "/host%s", image);
        if (!loop_attach(file, loop, sizeof loop)) die("cannot attach", file);
        if (mount(loop, "/mnt", "ext4", 0, NULL)) die("cannot mount", file);
    } else {
        unsigned char uuid[16];
        if (!parse_uuid(uuid_s, uuid)) die("bad uuid", uuid_s);
        for (int i = 0; i < 100 && !found; i++) {
            found = find_uuid(uuid, dev, sizeof dev);
            if (!found) usleep(100000);
        }
        if (!found) die("no partition with uuid", uuid_s);
        if (mount(dev, "/mnt", "ext4", 0, NULL)) die("cannot mount", dev);
    }
    // the relative /orbiter-os link resolves inside /mnt
    char full[1024], real[4096];
    snprintf(full, sizeof full, "/mnt%s", dir);
    if (!realpath(full, real)) die("no folder", full);

    mkdir("/newroot", 0755);
    if (mount(real, "/newroot", NULL, MS_BIND, NULL)) die("cannot bind", real);
    mount("/dev", "/newroot/dev", NULL, MS_MOVE, NULL);
    mount("/proc", "/newroot/proc", NULL, MS_MOVE, NULL);
    mount("/sys", "/newroot/sys", NULL, MS_MOVE, NULL);
    // the Windows partition stays reachable, under the image's /host
    if (in_ntfs) {
        mkdir("/newroot/host", 0755);
        mount("/host", "/newroot/host", NULL, MS_MOVE, NULL);
    }
    // scratch stays in memory, not in the project tree
    mount("tmpfs", "/newroot/tmp", "tmpfs", 0, NULL);
    mount("tmpfs", "/newroot/run", "tmpfs", 0, NULL);

    // the switch_root move: rootfs cannot be pivoted
    if (chdir("/newroot")) die("cannot enter", real);
    if (mount(".", "/", NULL, MS_MOVE, NULL)) die("cannot move root", real);
    if (chroot(".") || chdir("/")) die("cannot chroot", real);
    save_logs();
    // after the switch: firmware loads from the folder
    // the kernel's own driver requests: our modprobe's place
    int kp = open("/proc/sys/kernel/modprobe", O_WRONLY);
    if (kp >= 0) { write(kp, MODPROBE, strlen(MODPROBE)); close(kp); }
    phone_find();
    phone_early();
    hotplug(uevents());
    load_hardware();
    // devices made by the first pass's drivers (usb hid devices)
    load_hardware();
    load_modules();
    swap_on();
    net_start(init);
    session_start();

    // orbiter.args=--install,true: init's arguments, by comma
    char args[1024];
    char* argv[34] = { init };
    int argc = 1;
    if (arg("orbiter.args", args, sizeof args))
        for (char* w = strtok(args, ","); w && argc < 33; w = strtok(NULL, ","))
            argv[argc++] = w;
    argv[argc] = NULL;
    work_folder(init);
    execve(init, argv, environ);
    die("cannot run", init);
    return 1;
}
