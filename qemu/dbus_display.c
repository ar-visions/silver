// qemu D-Bus display client: peer-to-peer (no bus daemon), host-side only.
// connects to `-display dbus,addr=unix:path=<sock>`, registers a Listener,
// and captures the guest scanout as a dma-buf fd for trinity to import.
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

// only the Listener interface — the one qemu calls back on our socket
static const char* LISTENER_XML =
"<node>"
"  <interface name='org.qemu.Display1.Listener'>"
"    <method name='Scanout'>"
"      <arg type='u' name='width' direction='in'/>"
"      <arg type='u' name='height' direction='in'/>"
"      <arg type='u' name='stride' direction='in'/>"
"      <arg type='u' name='pixman_format' direction='in'/>"
"      <arg type='ay' name='data' direction='in'/>"
"    </method>"
"    <method name='Update'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"      <arg type='i' name='width' direction='in'/>"
"      <arg type='i' name='height' direction='in'/>"
"      <arg type='u' name='stride' direction='in'/>"
"      <arg type='u' name='pixman_format' direction='in'/>"
"      <arg type='ay' name='data' direction='in'/>"
"    </method>"
"    <method name='ScanoutDMABUF'>"
"      <arg type='h' name='dmabuf' direction='in'/>"
"      <arg type='u' name='width' direction='in'/>"
"      <arg type='u' name='height' direction='in'/>"
"      <arg type='u' name='stride' direction='in'/>"
"      <arg type='u' name='fourcc' direction='in'/>"
"      <arg type='t' name='modifier' direction='in'/>"
"      <arg type='b' name='y0_top' direction='in'/>"
"    </method>"
"    <method name='UpdateDMABUF'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"      <arg type='i' name='width' direction='in'/>"
"      <arg type='i' name='height' direction='in'/>"
"    </method>"
"    <method name='Disable'/>"
"    <method name='MouseSet'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"      <arg type='i' name='on' direction='in'/>"
"    </method>"
"    <method name='CursorDefine'>"
"      <arg type='i' name='width' direction='in'/>"
"      <arg type='i' name='height' direction='in'/>"
"      <arg type='i' name='hot_x' direction='in'/>"
"      <arg type='i' name='hot_y' direction='in'/>"
"      <arg type='ay' name='data' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

// latest scanout, guarded by g_lock. gen bumps on each new ScanoutDMABUF so
// the consumer re-imports; dirty flags a fresh UpdateDMABUF.
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int      g_fd = -1;      // owned here; consumer dups
static int      g_w, g_h, g_stride;
static unsigned g_fourcc;
static uint64_t g_modifier;
static int      g_gen;          // bumps per new dma-buf
static int      g_dirty;        // frame changed since last poll
static int      g_y0top;        // 1 if row 0 is the top (else flip V)
static GDBusConnection* g_console_conn;
static GMainLoop*       g_loop;

static void listener_method(GDBusConnection* conn, const char* sender,
    const char* path, const char* iface, const char* method,
    GVariant* params, GDBusMethodInvocation* inv, gpointer user) {
    (void)conn; (void)sender; (void)path; (void)iface; (void)user;
    if (!strcmp(method, "ScanoutDMABUF")) {
        GDBusMessage* msg = g_dbus_method_invocation_get_message(inv);
        GUnixFDList*  fl  = g_dbus_message_get_unix_fd_list(msg);
        gint32 hidx; guint w, h, stride, fourcc; guint64 mod; gboolean y0;
        g_variant_get(params, "(huuuutb)", &hidx, &w, &h, &stride, &fourcc, &mod, &y0);
        int fd = fl ? g_unix_fd_list_get(fl, hidx, NULL) : -1;
        pthread_mutex_lock(&g_lock);
        if (g_fd >= 0) close(g_fd);
        g_fd = fd; g_w = w; g_h = h; g_stride = stride;
        g_fourcc = fourcc; g_modifier = mod; g_y0top = y0 ? 1 : 0; g_gen++; g_dirty = 1;
        pthread_mutex_unlock(&g_lock);
        g_dbus_method_invocation_return_value(inv, NULL);
    } else if (!strcmp(method, "UpdateDMABUF")) {
        pthread_mutex_lock(&g_lock); g_dirty = 1; pthread_mutex_unlock(&g_lock);
        g_dbus_method_invocation_return_value(inv, NULL);
    } else if (!strcmp(method, "Disable")) {
        pthread_mutex_lock(&g_lock);
        if (g_fd >= 0) close(g_fd);
        g_fd = -1; g_gen++;
        pthread_mutex_unlock(&g_lock);
        g_dbus_method_invocation_return_value(inv, NULL);
    } else {
        // Scanout (2D), Update, MouseSet, CursorDefine: accept and ignore
        g_dbus_method_invocation_return_value(inv, NULL);
    }
}

static const GDBusInterfaceVTable LISTENER_VTABLE = { listener_method, NULL, NULL, {0} };

typedef struct { char* qmp; } start_args;

static void register_done(GObject* src, GAsyncResult* res, gpointer user) {
    (void)user;
    GError* err = NULL;
    GVariant* r = g_dbus_connection_call_with_unix_fd_list_finish(
        (GDBusConnection*)src, NULL, res, &err);
    if (!r) g_printerr("qemu dbus: RegisterListener error: %s\n", err ? err->message : "?");
    else { g_printerr("qemu dbus: RegisterListener ok\n"); g_variant_unref(r); }
}

// the listener server connection to qemu finished auth: register the object
static void listener_ready(GObject* src, GAsyncResult* res, gpointer user) {
    (void)src; (void)user;
    GError* err = NULL;
    GDBusConnection* listener = g_dbus_connection_new_finish(res, &err);
    if (!listener) { g_printerr("qemu dbus: listener conn: %s\n", err ? err->message : "?"); return; }
    GDBusNodeInfo* node = g_dbus_node_info_new_for_xml(LISTENER_XML, &err);
    g_dbus_connection_register_object(listener, "/org/qemu/Display1/Listener",
        node->interfaces[0], &LISTENER_VTABLE, NULL, NULL, &err);
    if (err) { g_printerr("qemu dbus: register: %s\n", err->message); return; }
    g_dbus_connection_start_message_processing(listener);
    fprintf(stderr, "qemu dbus: listener registered\n");
}

// minimal QMP line protocol with SCM_RIGHTS fd passing. qemu p2p D-Bus is
// attached by passing our socket fd through `getfd` + `add_client`.
static int qmp_line(int s, const char* json) {
    struct msghdr mh = {0};
    struct iovec iov = { (void*)json, strlen(json) };
    mh.msg_iov = &iov; mh.msg_iovlen = 1;
    if (sendmsg(s, &mh, 0) < 0) return -1;
    char buf[4096]; ssize_t n = recv(s, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = 0;
    return strstr(buf, "\"error\"") ? -1 : 0;
}
static int qmp_getfd(int s, int fd, const char* name) {
    char json[128];
    int len = snprintf(json, sizeof(json),
        "{\"execute\":\"getfd\",\"arguments\":{\"fdname\":\"%s\"}}\n", name);
    struct msghdr mh = {0};
    struct iovec iov = { json, len };
    mh.msg_iov = &iov; mh.msg_iovlen = 1;
    char cbuf[CMSG_SPACE(sizeof(int))] = {0};
    mh.msg_control = cbuf; mh.msg_controllen = sizeof(cbuf);
    struct cmsghdr* c = CMSG_FIRSTHDR(&mh);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(c), &fd, sizeof(int));
    if (sendmsg(s, &mh, 0) < 0) return -1;
    char r[4096]; ssize_t n = recv(s, r, sizeof(r) - 1, 0);
    if (n <= 0) return -1; r[n] = 0;
    return strstr(r, "\"error\"") ? -1 : 0;
}

static gpointer dbus_thread(gpointer data) {
    start_args* a = data;
    GError* err = NULL;
    fprintf(stderr, "qemu dbus: thread up, qmp=%s\n", a->qmp); fflush(stderr);

    // 1. connect the QMP monitor (retry until qemu creates the socket)
    int qs = -1;
    for (int t = 0; t < 400; t++) {
        qs = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un sa = { .sun_family = AF_UNIX };
        strncpy(sa.sun_path, a->qmp, sizeof(sa.sun_path) - 1);
        if (connect(qs, (struct sockaddr*)&sa, sizeof(sa)) == 0) break;
        close(qs); qs = -1; usleep(25000);
    }
    if (qs < 0) { g_printerr("qemu dbus: qmp connect timeout\n"); return NULL; }
    fprintf(stderr,"qemu dbus: qmp connected\n");
    { char g[4096]; recv(qs, g, sizeof(g), 0); }  // greeting
    if (qmp_line(qs, "{\"execute\":\"qmp_capabilities\"}\n")) {
        g_printerr("qemu dbus: qmp_capabilities failed\n"); return NULL;
    }

    // 2. socketpair: pass one end to qemu as the p2p D-Bus transport
    int cv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, cv)) { g_printerr("qemu dbus: socketpair\n"); return NULL; }
    if (qmp_getfd(qs, cv[1], "dbus0")) { g_printerr("qemu dbus: getfd failed\n"); return NULL; }
    if (qmp_line(qs, "{\"execute\":\"add_client\",\"arguments\":{\"protocol\":\"@dbus-display\",\"fdname\":\"dbus0\"}}\n")) {
        g_printerr("qemu dbus: add_client failed\n"); return NULL;
    }
    close(cv[1]);
    fprintf(stderr,"qemu dbus: add_client ok\n");

    // qemu is now the p2p D-Bus SERVER on cv[0]; we are the client
    GSocket* cgs = g_socket_new_from_fd(cv[0], &err);
    GSocketConnection* csc = g_socket_connection_factory_create_connection(cgs);
    GDBusConnection* console = g_dbus_connection_new_sync(
        G_IO_STREAM(csc), NULL, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
        NULL, NULL, &err);
    if (!console) { g_printerr("qemu dbus: p2p conn failed: %s\n", err ? err->message : "?"); return NULL; }
    g_console_conn = console;
    fprintf(stderr,"qemu dbus: p2p client connected\n");

    // 2. a socketpair: qemu talks D-Bus back to us over sv[1]; we serve sv[0].
    // both the server-side auth (sv[0]) and RegisterListener (which makes qemu
    // connect to sv[1]) must proceed CONCURRENTLY, so drive them async on the
    // main loop instead of blocking one on the other (that deadlocks).
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) { g_printerr("qemu dbus: socketpair\n"); return NULL; }

    g_loop = g_main_loop_new(NULL, FALSE);

    // register-listener via QMP-passed fd: send RegisterListener now (async);
    // qemu connects back to sv[1] and our server handshake completes in the loop
    GUnixFDList* fl = g_unix_fd_list_new();
    gint hidx = g_unix_fd_list_append(fl, sv[1], &err);
    close(sv[1]);
    g_dbus_connection_call_with_unix_fd_list(
        console, NULL, "/org/qemu/Display1/Console_0",
        "org.qemu.Display1.Console", "RegisterListener",
        g_variant_new("(h)", hidx), NULL, G_DBUS_CALL_FLAGS_NONE, -1,
        fl, NULL, register_done, NULL);
    g_object_unref(fl);

    // serve the Listener on sv[0], async: the ready callback registers the
    // object and starts processing once qemu has connected + authed
    // qemu is the auth SERVER on the listener socket, so we are the CLIENT
    GSocket* gs = g_socket_new_from_fd(sv[0], &err);
    GSocketConnection* sc = g_socket_connection_factory_create_connection(gs);
    g_dbus_connection_new(
        G_IO_STREAM(sc), NULL,
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
        G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING, NULL, NULL,
        listener_ready, NULL);
    fprintf(stderr, "qemu dbus: listener registering, entering loop\n");

    g_main_loop_run(g_loop);
    return NULL;
}

// start the client on its own glib main-loop thread. addr like
// "unix:path=/tmp/qemu-dbus-1234.sock"
int qemu_dbus_start(const char* qmp_path) {
    setvbuf(stderr, NULL, _IONBF, 0);   // g_printerr diagnostics unbuffered
    fprintf(stderr, "qemu dbus: start %s\n", qmp_path);
    start_args* a = g_new0(start_args, 1);
    a->qmp = g_strdup(qmp_path);
    GThread* t = g_thread_new("qemu-dbus", dbus_thread, a);
    return t ? 0 : -1;
}

// return the current scanout dma-buf fd (a dup the caller owns), geometry
// out. returns -1 if none. *gen is the buffer generation (import on change);
// *dirty is 1 if the frame changed since the last call (then cleared).
int qemu_dbus_scanout(int* w, int* h, int* stride, unsigned* fourcc,
                      uint64_t* modifier, int* gen, int* dirty, int* y0top) {
    pthread_mutex_lock(&g_lock);
    int fd = g_fd >= 0 ? dup(g_fd) : -1;
    if (w) *w = g_w; if (h) *h = g_h; if (stride) *stride = g_stride;
    if (fourcc) *fourcc = g_fourcc; if (modifier) *modifier = g_modifier;
    if (gen) *gen = g_gen; if (dirty) { *dirty = g_dirty; g_dirty = 0; }
    if (y0top) *y0top = g_y0top;
    pthread_mutex_unlock(&g_lock);
    return fd;
}
