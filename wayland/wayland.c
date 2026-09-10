// a Wayland compositor on wlroots that shows up as one silver app: a headless
// output the size of the pane, every client composited into it by wlr_scene,
// and each finished frame handed over as a dma-buf for trinity to sample.
// input arrives from the pane and goes to the client under the pointer.
#define _POSIX_C_SOURCE 200809L
#define WLR_USE_UNSTABLE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/dmabuf.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

#define WLC_API __attribute__((visibility("default")))

// one compositor per process
static struct wl_display        *g_display;
static struct wl_event_loop     *g_loop;
static struct wlr_backend       *g_backend;
static struct wlr_renderer      *g_renderer;
static struct wlr_allocator     *g_allocator;
static struct wlr_scene         *g_scene;
static struct wlr_scene_output_layout *g_scene_layout;
static struct wlr_output_layout *g_layout;
static struct wlr_output        *g_output;
static struct wlr_scene_output  *g_scene_output;
static struct wlr_xdg_shell     *g_xdg;
static struct wlr_seat          *g_seat;
static struct wlr_keyboard       g_kb;
static const char               *g_socket;
static int                       g_w, g_h;
static int                       g_want_w, g_want_h;

// the frame shown: the output's committed buffer, held locked so the memory
// stays put while the shell samples it. id is the buffer's address, which
// the swapchain reuses, so the shell caches its import per id
static struct wlr_buffer        *g_frame_buf;
static struct wlr_dmabuf_attributes g_frame;
static uint64_t                  g_frame_gen;

static struct wl_listener l_new_xdg, l_output_frame, l_output_commit, l_kb_key, l_kb_mods, l_client, l_deco;
static int g_commits;
static int g_fullscreen;   // toplevels open fullscreen: no shadows, and apps that hide their chrome do

// we decorate, which is to say nobody does: clients drop their shadows and
// window buttons. a header bar that is the app's own widget stays
static void on_new_decoration(struct wl_listener *l, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *d = data;
    wlr_xdg_toplevel_decoration_v1_set_mode(d, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void on_client_created(struct wl_listener *l, void *data) {
    struct wl_client *c = data;
    pid_t pid = 0; uid_t uid; gid_t gid;
    wl_client_get_credentials(c, &pid, &uid, &gid);
    fprintf(stderr, "wayland: client connected (pid %d)\n", (int)pid);
}

static uint32_t now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ---- toplevels ---------------------------------------------------------

struct toplevel {
    struct wlr_xdg_toplevel *xdg;
    struct wlr_scene_tree   *tree;
    struct wl_listener       map, unmap, destroy;
};

static struct toplevel *g_focused;

static void focus_toplevel(struct toplevel *t) {
    if (!t || !t->xdg) return;
    struct wlr_surface *surf = t->xdg->base->surface;
    struct wlr_surface *prev = g_seat->keyboard_state.focused_surface;
    if (prev == surf) return;
    if (prev) {
        struct wlr_xdg_toplevel *pt = wlr_xdg_toplevel_try_from_wlr_surface(prev);
        if (pt) wlr_xdg_toplevel_set_activated(pt, false);
    }
    wlr_scene_node_raise_to_top(&t->tree->node);
    wlr_xdg_toplevel_set_activated(t->xdg, true);
    wlr_seat_keyboard_notify_enter(g_seat, surf, g_kb.keycodes, g_kb.num_keycodes, &g_kb.modifiers);
    g_focused = t;
}

static void on_map(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, map);
    // a client fills the pane: the output is the pane
    wlr_scene_node_set_position(&t->tree->node, 0, 0);
    wlr_xdg_toplevel_set_size(t->xdg, g_w, g_h);
    focus_toplevel(t);
    fprintf(stderr, "wayland: mapped '%s' (%s)\n", t->xdg->title ? t->xdg->title : "",
        t->xdg->app_id ? t->xdg->app_id : "");
}

static void on_unmap(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, unmap);
    if (g_focused == t) g_focused = NULL;
}

static void on_destroy(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, destroy);
    wl_list_remove(&t->map.link);
    wl_list_remove(&t->unmap.link);
    wl_list_remove(&t->destroy.link);
    if (g_focused == t) g_focused = NULL;
    free(t);
}

static void on_new_xdg_surface(struct wl_listener *l, void *data) {
    struct wlr_xdg_surface *xs = data;
    if (xs->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        // a popup rides on its parent's tree
        struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xs->popup->parent);
        if (!parent || !parent->data) return;
        struct wlr_scene_tree *ptree = parent->data;
        xs->data = wlr_scene_xdg_surface_create(ptree, xs);
        return;
    }
    if (xs->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) return;
    fprintf(stderr, "wayland: new toplevel\n");
    struct toplevel *t = calloc(1, sizeof(*t));
    t->xdg  = xs->toplevel;
    t->tree = wlr_scene_xdg_surface_create(&g_scene->tree, xs);
    xs->data = t->tree;
    t->map.notify     = on_map;     wl_signal_add(&xs->surface->events.map,   &t->map);
    t->unmap.notify   = on_unmap;   wl_signal_add(&xs->surface->events.unmap, &t->unmap);
    t->destroy.notify = on_destroy; wl_signal_add(&xs->events.destroy,        &t->destroy);
    // size it before the first commit, so it opens the size of the pane
    wlr_xdg_toplevel_set_size(t->xdg, g_w, g_h);
    if (g_fullscreen) wlr_xdg_toplevel_set_fullscreen(t->xdg, true);
}

// ---- output ------------------------------------------------------------

static void on_output_frame(struct wl_listener *l, void *data) {
    wlr_scene_output_commit(g_scene_output, NULL);
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(g_scene_output, &now);
}

static void on_output_commit(struct wl_listener *l, void *data) {
    struct wlr_output_event_commit *ev = data;
    if (!(ev->state->committed & WLR_OUTPUT_STATE_BUFFER) || !ev->state->buffer) return;
    struct wlr_dmabuf_attributes at;
    if (!wlr_buffer_get_dmabuf(ev->state->buffer, &at)) return;
    if (g_frame_buf) wlr_buffer_unlock(g_frame_buf);
    g_frame_buf = wlr_buffer_lock(ev->state->buffer);
    g_frame     = at;
    g_frame_gen += 1;
    g_commits   += 1;
    if (g_commits == 1 || g_commits % 300 == 0)
        fprintf(stderr, "wayland: output commit %d, buffer %p\n", g_commits, (void *)g_frame_buf);
}

static void output_mode(int w, int h) {
    struct wlr_output_state st;
    wlr_output_state_init(&st);
    wlr_output_state_set_enabled(&st, true);
    wlr_output_state_set_custom_mode(&st, w, h, 0);
    wlr_output_commit_state(g_output, &st);
    wlr_output_state_finish(&st);
    g_w = w; g_h = h;
}

// ---- keyboard ----------------------------------------------------------

static void on_kb_key(struct wl_listener *l, void *data) {
    struct wlr_keyboard_key_event *ev = data;
    wlr_seat_set_keyboard(g_seat, &g_kb);
    wlr_seat_keyboard_notify_key(g_seat, ev->time_msec, ev->keycode, ev->state);
}

static void on_kb_mods(struct wl_listener *l, void *data) {
    wlr_seat_set_keyboard(g_seat, &g_kb);
    wlr_seat_keyboard_notify_modifiers(g_seat, &g_kb.modifiers);
}

// the pane speaks GLFW keycodes; clients want evdev ones
static int evdev_from_glfw(int k) {
    static const int letters[26] = { KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
        KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V,
        KEY_W, KEY_X, KEY_Y, KEY_Z };
    static const int digits[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
    if (k >= 65 && k <= 90)   return letters[k - 65];
    if (k >= 48 && k <= 57)   return digits[k - 48];
    if (k >= 290 && k <= 301) { static const int f[12] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
        KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12 }; return f[k - 290]; }
    switch (k) {
    case 32:  return KEY_SPACE;      case 39:  return KEY_APOSTROPHE; case 44:  return KEY_COMMA;
    case 45:  return KEY_MINUS;      case 46:  return KEY_DOT;        case 47:  return KEY_SLASH;
    case 59:  return KEY_SEMICOLON;  case 61:  return KEY_EQUAL;      case 91:  return KEY_LEFTBRACE;
    case 92:  return KEY_BACKSLASH;  case 93:  return KEY_RIGHTBRACE; case 96:  return KEY_GRAVE;
    case 256: return KEY_ESC;        case 257: return KEY_ENTER;      case 258: return KEY_TAB;
    case 259: return KEY_BACKSPACE;  case 260: return KEY_INSERT;     case 261: return KEY_DELETE;
    case 262: return KEY_RIGHT;      case 263: return KEY_LEFT;       case 264: return KEY_DOWN;
    case 265: return KEY_UP;         case 266: return KEY_PAGEUP;     case 267: return KEY_PAGEDOWN;
    case 268: return KEY_HOME;       case 269: return KEY_END;        case 280: return KEY_CAPSLOCK;
    case 340: return KEY_LEFTSHIFT;  case 341: return KEY_LEFTCTRL;   case 342: return KEY_LEFTALT;
    case 343: return KEY_LEFTMETA;   case 344: return KEY_RIGHTSHIFT; case 345: return KEY_RIGHTCTRL;
    case 346: return KEY_RIGHTALT;   case 347: return KEY_RIGHTMETA;
    }
    return -1;
}

// ---- the render node ---------------------------------------------------

// the NVIDIA node when there is one: the shell samples on that device
static void pick_render_node(void) {
    if (getenv("WLR_RENDER_DRM_DEVICE")) return;
    DIR *d = opendir("/sys/class/drm");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "renderD", 7) != 0) continue;
        char p[512]; snprintf(p, sizeof(p), "/sys/class/drm/%s/device/vendor", e->d_name);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        char v[32] = { 0 }; fgets(v, sizeof(v), f); fclose(f);
        if (strncmp(v, "0x10de", 6) == 0) {
            char dev[64]; snprintf(dev, sizeof(dev), "/dev/dri/%s", e->d_name);
            setenv("WLR_RENDER_DRM_DEVICE", dev, 1);
            break;
        }
    }
    closedir(d);
}

// ---- the api the .ag side calls ----------------------------------------

WLC_API int wlc_create(int w, int h) {
    if (g_display) return 1;
    pick_render_node();
    // the vulkan renderer allocates with the modifiers the shell's vulkan
    // device takes; gles2 on nvidia picks compressed ones it does not
    setenv("WLR_RENDERER", "vulkan", 0);
    wlr_log_init(WLR_INFO, NULL);
    g_display = wl_display_create();
    g_loop    = wl_display_get_event_loop(g_display);
    g_backend = wlr_headless_backend_create(g_display);
    if (!g_backend) { fprintf(stderr, "wayland: no headless backend\n"); return 0; }
    g_renderer = wlr_renderer_autocreate(g_backend);
    if (!g_renderer) { fprintf(stderr, "wayland: no renderer\n"); return 0; }
    wlr_renderer_init_wl_display(g_renderer, g_display);
    g_allocator = wlr_allocator_autocreate(g_backend, g_renderer);
    if (!g_allocator) { fprintf(stderr, "wayland: no allocator\n"); return 0; }
    wlr_compositor_create(g_display, 5, g_renderer);
    wlr_subcompositor_create(g_display);
    wlr_data_device_manager_create(g_display);
    g_layout = wlr_output_layout_create();
    g_scene  = wlr_scene_create();
    g_scene_layout = wlr_scene_attach_output_layout(g_scene, g_layout);
    g_xdg = wlr_xdg_shell_create(g_display, 3);
    l_new_xdg.notify = on_new_xdg_surface;
    wl_signal_add(&g_xdg->events.new_surface, &l_new_xdg);
    struct wlr_xdg_decoration_manager_v1 *deco = wlr_xdg_decoration_manager_v1_create(g_display);
    l_deco.notify = on_new_decoration;
    wl_signal_add(&deco->events.new_toplevel_decoration, &l_deco);
    // the seat: a pointer and a keyboard, both fed by the pane
    g_seat = wlr_seat_create(g_display, "seat0");
    wlr_seat_set_capabilities(g_seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    wlr_keyboard_init(&g_kb, NULL, "silver");
    struct xkb_context *xctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap  *kmap = xkb_keymap_new_from_names(xctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(&g_kb, kmap);
    xkb_keymap_unref(kmap);
    xkb_context_unref(xctx);
    wlr_keyboard_set_repeat_info(&g_kb, 25, 600);
    l_kb_key.notify  = on_kb_key;  wl_signal_add(&g_kb.events.key,       &l_kb_key);
    l_kb_mods.notify = on_kb_mods; wl_signal_add(&g_kb.events.modifiers, &l_kb_mods);
    wlr_seat_set_keyboard(g_seat, &g_kb);
    // the one output: the pane, at its size
    g_output = wlr_headless_add_output(g_backend, w, h);
    if (!g_output) { fprintf(stderr, "wayland: no output\n"); return 0; }
    wlr_output_init_render(g_output, g_allocator, g_renderer);
    l_output_frame.notify  = on_output_frame;  wl_signal_add(&g_output->events.frame,  &l_output_frame);
    l_output_commit.notify = on_output_commit; wl_signal_add(&g_output->events.commit, &l_output_commit);
    output_mode(w, h);
    struct wlr_output_layout_output *lo = wlr_output_layout_add_auto(g_layout, g_output);
    g_scene_output = wlr_scene_output_create(g_scene, g_output);
    wlr_scene_output_layout_add_output(g_scene_layout, lo, g_scene_output);
    l_client.notify = on_client_created;
    wl_display_add_client_created_listener(g_display, &l_client);
    g_socket = wl_display_add_socket_auto(g_display);
    if (!g_socket) { fprintf(stderr, "wayland: no socket\n"); return 0; }
    setenv("WAYLAND_DISPLAY", g_socket, 1);
    if (!wlr_backend_start(g_backend)) { fprintf(stderr, "wayland: backend would not start\n"); return 0; }
    fprintf(stderr, "wayland: display %s, output %dx%d\n", g_socket, w, h);
    return 1;
}

WLC_API const char *wlc_socket(void) { return g_socket ? g_socket : ""; }

// one turn of the event loop: clients' requests in, our events out. the
// output's own frame timer drives the scene commits
WLC_API void wlc_pump(void) {
    if (!g_display) return;
    if (g_want_w > 0 && (g_want_w != g_w || g_want_h != g_h)) {
        output_mode(g_want_w, g_want_h);
        if (g_focused) wlr_xdg_toplevel_set_size(g_focused->xdg, g_w, g_h);
    }
    wl_event_loop_dispatch(g_loop, 0);
    wl_display_flush_clients(g_display);
    static int pumps;
    pumps += 1;
    if (pumps == 1 || pumps % 600 == 0) fprintf(stderr, "wayland: pump %d, commits %d\n", pumps, g_commits);
}

// the pane's size, taken on the next pump
WLC_API void wlc_resize(int w, int h) { g_want_w = w; g_want_h = h; }

// toplevels open fullscreen from now on; the focused one follows at once
WLC_API void wlc_fullscreen(int on) {
    g_fullscreen = on;
    if (g_focused) wlr_xdg_toplevel_set_fullscreen(g_focused->xdg, on != 0);
}

// the frame: its id (the buffer, reused by the swapchain), and its dma-buf
WLC_API uint64_t wlc_frame_id(void)   { return (uint64_t)(uintptr_t)g_frame_buf; }
WLC_API uint64_t wlc_frame_gen(void)  { return g_frame_gen; }
WLC_API int      wlc_frame_w(void)    { return g_frame_buf ? g_frame.width  : 0; }
WLC_API int      wlc_frame_h(void)    { return g_frame_buf ? g_frame.height : 0; }
WLC_API uint32_t wlc_frame_format(void)   { return g_frame_buf ? g_frame.format : 0; }
WLC_API uint64_t wlc_frame_modifier(void) { return g_frame_buf ? g_frame.modifier : 0; }
WLC_API uint32_t wlc_frame_stride(void)   { return g_frame_buf ? g_frame.stride[0] : 0; }
WLC_API uint32_t wlc_frame_offset(void)   { return g_frame_buf ? g_frame.offset[0] : 0; }
// a fresh fd for an import, the caller's to consume
WLC_API int      wlc_frame_fd(void)   { return g_frame_buf ? fcntl(g_frame.fd[0], F_DUPFD_CLOEXEC, 0) : -1; }

// ---- input from the pane -----------------------------------------------

static struct wlr_surface *surface_at(double x, double y, double *sx, double *sy) {
    struct wlr_scene_node *n = wlr_scene_node_at(&g_scene->tree.node, x, y, sx, sy);
    if (!n || n->type != WLR_SCENE_NODE_BUFFER) return NULL;
    struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(n));
    return ss ? ss->surface : NULL;
}

WLC_API void wlc_pointer_motion(float x, float y) {
    if (!g_seat) return;
    double sx, sy;
    struct wlr_surface *s = surface_at(x, y, &sx, &sy);
    if (s) {
        wlr_seat_pointer_notify_enter(g_seat, s, sx, sy);
        wlr_seat_pointer_notify_motion(g_seat, now_ms(), sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(g_seat);
    }
    wlr_seat_pointer_notify_frame(g_seat);
}

// button: 0 left, 1 right, 2 middle (the pane's numbering)
WLC_API void wlc_pointer_button(float x, float y, int button, int state) {
    if (!g_seat) return;
    double sx, sy;
    struct wlr_surface *s = surface_at(x, y, &sx, &sy);
    if (state && s) {
        // a click focuses the toplevel it lands on
        struct wlr_xdg_toplevel *tl = wlr_xdg_toplevel_try_from_wlr_surface(s);
        if (!tl) {
            struct wlr_surface *root = wlr_surface_get_root_surface(s);
            if (root) tl = wlr_xdg_toplevel_try_from_wlr_surface(root);
        }
        if (tl && tl->base->data) {
            // the toplevel record hangs off its tree
            struct wlr_scene_tree *tree = tl->base->data;
            wlr_scene_node_raise_to_top(&tree->node);
            wlr_xdg_toplevel_set_activated(tl, true);
            wlr_seat_keyboard_notify_enter(g_seat, tl->base->surface, g_kb.keycodes, g_kb.num_keycodes, &g_kb.modifiers);
        }
        wlr_seat_pointer_notify_enter(g_seat, s, sx, sy);
    }
    uint32_t code = button == 1 ? BTN_RIGHT : button == 2 ? BTN_MIDDLE : BTN_LEFT;
    wlr_seat_pointer_notify_button(g_seat, now_ms(), code, state ? WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
    wlr_seat_pointer_notify_frame(g_seat);
}

// wheel: dy in lines, dx across
WLC_API void wlc_pointer_axis(float dx, float dy) {
    if (!g_seat) return;
    uint32_t t = now_ms();
    if (dy != 0.0f) wlr_seat_pointer_notify_axis(g_seat, t, WLR_AXIS_ORIENTATION_VERTICAL,   -dy * 15.0, (int32_t)(-dy), WLR_AXIS_SOURCE_WHEEL);
    if (dx != 0.0f) wlr_seat_pointer_notify_axis(g_seat, t, WLR_AXIS_ORIENTATION_HORIZONTAL, -dx * 15.0, (int32_t)(-dx), WLR_AXIS_SOURCE_WHEEL);
    wlr_seat_pointer_notify_frame(g_seat);
}

WLC_API void wlc_key(int glfw, int state) {
    if (!g_seat) return;
    int code = evdev_from_glfw(glfw);
    if (code < 0) return;
    struct wlr_keyboard_key_event ev = {
        .time_msec = now_ms(), .keycode = (uint32_t)code, .update_state = true,
        .state = state ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED };
    wlr_keyboard_notify_key(&g_kb, &ev);
}

// a client on this display, through the shell
WLC_API int wlc_spawn(const char *cmd) {
    if (!cmd || !*cmd || !g_socket) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        setenv("WAYLAND_DISPLAY", g_socket, 1);
        setenv("GDK_BACKEND", "wayland", 1);
        setenv("QT_QPA_PLATFORM", "wayland", 1);
        setenv("MOZ_ENABLE_WAYLAND", "1", 1);
        setenv("SDL_VIDEODRIVER", "wayland", 1);
        unsetenv("DISPLAY");
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    return (int)pid;
}
