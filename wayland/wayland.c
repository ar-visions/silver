// wlroots, wayland-server and xcb exist only on linux. windows
// takes wayland-win.c, which answers the same surface with
// nothing -- included here because only <module>.c is compiled
#ifdef _WIN32
#include "wayland-win.c"
#else
// a Wayland compositor on wlroots for silver: each window has its own
// headless output, the size of the pane that shows it, composited by
// wlr_scene; its finished frames go to trinity as dma-bufs. input arrives
// from the pane and goes to that window's surface under the pointer.
#define _POSIX_C_SOURCE 200809L
#define WLR_USE_UNSTABLE 1
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <xcb/xcb.h>
#include <xcb/res.h>
#include <poll.h>
#include <pthread.h>
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
#include <wlr/xwayland.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>
#include "gtk-shell-protocol.h"
#include "appmenu-protocol.h"
#include "gtk-shell-protocol.c"
#include "appmenu-protocol.c"

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
static struct wlr_xdg_shell     *g_xdg;
// X11 programs: an X server of our own, its windows ours too
static struct wlr_xwayland      *g_xwl;
// the output at 0 no window draws on: X's one screen
static struct wlr_output        *g_base;
// the last window that asked to come forward (0 none); the shell
// takes it
static uint64_t                  g_asked;
// the last window that asked to be minimized (0 none)
static uint64_t                  g_hide;
// the last window that asked into or out of fullscreen (0 none)
static uint64_t                  g_full_asked;
// a window's pointer held still (a game's mouse look): its constraint,
// and the raw motion orbiter sends while it holds
static struct wlr_pointer_constraints_v1      *g_constraints;
static struct wlr_relative_pointer_manager_v1 *g_relative;
static struct wlr_pointer_constraint_v1       *g_held;
static struct wl_listener                      g_new_constraint;
static void on_new_constraint(struct wl_listener *l, void *data);
static struct wlr_seat          *g_seat;
static struct wlr_keyboard       g_kb;
static const char               *g_socket;
static int                       g_w, g_h;

static struct wl_listener l_new_xdg, l_kb_key, l_kb_mods, l_client, l_deco;
static struct wl_listener l_new_xw;
static int g_commits;
static int g_fullscreen;   // toplevels open fullscreen: no shadows, and apps that hide their chrome do

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

// ---- windows -----------------------------------------------------------

// each toplevel is a window with its own headless output, the size of the
// pane that shows it, at its own place in the layout (x = id * SPAN), so
// windows never overlap and each has its own frame
#define SPAN 16384

struct toplevel {
    uint64_t                 id;
    // an xdg toplevel, or an X11 window (one or the other)
    struct wlr_xdg_toplevel *xdg;
    struct wlr_xwayland_surface *xw;
    struct wlr_scene_tree   *tree;
    struct wlr_output       *out;
    struct wlr_scene_output *sout;
    int                      x, w, h, want_w, want_h;
    // the size it last committed (its geometry), and when the last
    // size was asked of it
    int                      cw, ch;
    double                   asked_at;
    // a size for its output, sent with its next frame (never alone:
    // wlroots then commits a blank buffer it forgets it drew)
    int                      mode_w, mode_h;
    bool                     framing;
    // counts its size changes: the shell's imports start over at each
    uint64_t                 epoch;
    bool                     mapped;
    // it took the xdg server-side decoration: no header of its own
    bool                     ssd;
    // a dialog of another window: its own size, never the pane's
    bool                     sheet;
    int                      gx, gy;
    // the last committed frame, locked while the shell samples it
    struct wlr_buffer        *frame_buf;
    struct wlr_dmabuf_attributes frame;
    uint64_t                 gen;
    struct wl_listener       map, unmap, destroy, frame_l, commit_l;
    struct wl_listener       surf_l;
    // an X11 window's surface comes and goes (associate)
    struct wl_listener       assoc_l, dissoc_l, req_l, act_l, min_l;
    // it asks for the whole screen, and has it
    struct wl_listener       full_l;
    bool                     full;
    bool                     assoc;
    // it was shown before: shown again (out of its tray), it asks
    bool                     shown_once;
    struct wl_list           link;
};

static struct wl_list    g_tops;
static uint64_t          g_next_id = 1;
static struct toplevel  *g_focused;

static struct toplevel *top_of(uint64_t id) {
    struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) if (t->id == id) return t;
    return NULL;
}

// a window's surface, its size asked (its own units), its activation
static struct wlr_surface *top_surface(struct toplevel *t) {
    if (!t) return NULL;
    if (t->xdg) return t->xdg->base->surface;
    return (t->xw && t->assoc) ? t->xw->surface : NULL;
}

static void top_size(struct toplevel *t, int w, int h) {
    if (t->xdg) wlr_xdg_toplevel_set_size(t->xdg, w, h);
    else if (t->xw && w > 0 && h > 0)
        wlr_xwayland_surface_configure(t->xw, 0, 0, w, h);
}

// the X11 window last raised: X's top window at 0, 0, so the one
// the X server gives clicks to, and the one a menu belongs to
static struct toplevel *g_xtop;

// X's stacking follows ours: the window under our pointer or focus
// is X's top, or X sends its clicks to another at the same place
static void x_raise(struct toplevel *t) {
    if (!t || !t->xw || g_xtop == t) return;
    wlr_xwayland_surface_restack(t->xw, NULL, XCB_STACK_MODE_ABOVE);
    g_xtop = t;
}

static void surface_activate(struct wlr_surface *s, bool on) {
    struct wlr_xdg_toplevel *xt = wlr_xdg_toplevel_try_from_wlr_surface(s);
    if (xt) { wlr_xdg_toplevel_set_activated(xt, on); return; }
    struct wlr_xwayland_surface *xs = wlr_xwayland_surface_try_from_wlr_surface(s);
    if (xs) wlr_xwayland_surface_activate(xs, on);
}

static void focus_toplevel(struct toplevel *t) {
    struct wlr_surface *surf = top_surface(t);
    if (!surf) return;
    struct wlr_surface *prev = g_seat->keyboard_state.focused_surface;
    if (prev == surf) { g_focused = t; return; }
    if (prev) surface_activate(prev, false);
    surface_activate(surf, true);
    x_raise(t);
    wlr_seat_keyboard_notify_enter(g_seat, surf, g_kb.keycodes, g_kb.num_keycodes, &g_kb.modifiers);
    g_focused = t;
}

// the windows' scale (the text size): a window is told its pane's
// pixels over it, and draws that many more pixels for each of its own
static double g_scale = 1.0;

// pixels in a window's own units
static int lg(int px) { return (int)lround(px / g_scale); }

// the real monitor's refresh in mHz; 0 until told (wlroots: 60 Hz)
static int g_refresh;

// every output paces its frames at the monitor's refresh
static void output_mode(struct wlr_output *o, int w, int h) {
    struct wlr_output_state st;
    wlr_output_state_init(&st);
    wlr_output_state_set_enabled(&st, true);
    wlr_output_state_set_custom_mode(&st, w, h, g_refresh);
    wlr_output_state_set_scale(&st, g_scale);
    wlr_output_commit_state(o, &st);
    wlr_output_state_finish(&st);
}

static double mono_now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void on_top_frame(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, frame_l);
    // asked a new size and not drawn at it yet: no frame of ours (it
    // would show its old picture in the new size, the rest black);
    // it may still draw. after 300 ms frames go on (a size it refused)
    bool behind = !t->sheet && t->w > 0
        && (t->cw != lg(t->w) || t->ch != lg(t->h))
        && mono_now() - t->asked_at < 0.3;
    if (!behind) {
        struct wlr_output_state st;
        wlr_output_state_init(&st);
        if (t->mode_w > 0) {
            wlr_output_state_set_custom_mode(&st, t->mode_w, t->mode_h, 0);
            wlr_output_state_set_scale(&st, g_scale);
        }
        t->framing = true;
        // new buffers come at the new size, maybe at old addresses:
        // the shell's imports start over then, not at the ask
        if (wlr_scene_output_build_state(t->sout, &st, NULL)
            && wlr_output_commit_state(t->out, &st) && t->mode_w > 0) {
            t->mode_w = t->mode_h = 0;
            t->epoch += 1;
        }
        t->framing = false;
        wlr_output_state_finish(&st);
    }
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(t->sout, &now);
}

static void on_top_commit(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, commit_l);
    struct wlr_output_event_commit *ev = data;
    if (!(ev->state->committed & WLR_OUTPUT_STATE_BUFFER) || !ev->state->buffer) return;
    // a size change alone commits a blank buffer: not a frame
    if ((ev->state->committed & WLR_OUTPUT_STATE_MODE) && !t->framing)
        return;
    struct wlr_dmabuf_attributes at;
    if (!wlr_buffer_get_dmabuf(ev->state->buffer, &at)) return;
    if (t->frame_buf) wlr_buffer_unlock(t->frame_buf);
    t->frame_buf = wlr_buffer_lock(ev->state->buffer);
    t->frame     = at;
    t->gen      += 1;
    g_commits   += 1;
}

#define SHEET_ROOM 64
// a dialog picks its own size; its output follows the window's
// geometry. a dialog names its parent, or (a toolkit's message box on
// a connection of its own, Electron's) opens at one fixed size
static void on_surface_commit(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, surf_l);
    struct wlr_xdg_surface *xs = t->xdg->base;
    if (xs->initial_commit) {
        struct wlr_xdg_toplevel_state *st = &t->xdg->current;
        bool fixed = st->min_width > 0 && st->min_width == st->max_width
                  && st->min_height > 0 && st->min_height == st->max_height;
        if (t->xdg->parent || fixed) {
            t->sheet = true;
            wlr_xdg_toplevel_set_size(t->xdg, 0, 0);
        } else {
            // its first configure answers its first commit
            wlr_xdg_toplevel_set_size(t->xdg, lg(t->w), lg(t->h));
            if (g_fullscreen) wlr_xdg_toplevel_set_fullscreen(t->xdg, true);
        }
        return;
    }
    struct wlr_box g;
    wlr_xdg_surface_get_geometry(xs, &g);
    // a window's size as drawn: the shell waits for it after a resize
    if (!t->sheet) { t->cw = g.width; t->ch = g.height; return; }
    if (g.width <= 0 || g.height <= 0) return;
    if (g.width == t->w && g.height == t->h && g.x == t->gx && g.y == t->gy) return;
    t->w = g.width; t->h = g.height; t->gx = g.x; t->gy = g.y;
    t->epoch += 1;
    // the scene's xdg tree already sets the shadow margin outside. its
    // output is roomier than it: as its monitor, one its size made
    // GTK fit window and shadow inside it, 20 px less a commit
    output_mode(t->out, (int)ceil((t->w + SHEET_ROOM) * g_scale),
        (int)ceil((t->h + SHEET_ROOM) * g_scale));
}

static void on_map(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, map);
    t->mapped = true;
    if (!t->sheet && t->xdg) {
        // tiled, not maximized: Firefox flickers when maximized
        wlr_xdg_toplevel_set_tiled(t->xdg, WLR_EDGE_TOP | WLR_EDGE_BOTTOM
            | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
    }
    if (!t->sheet) top_size(t, lg(t->w), lg(t->h));
    focus_toplevel(t);
    if (t->shown_once) g_asked = t->id;
    t->shown_once = true;
    const char *ti = t->xdg ? t->xdg->title : t->xw->title;
    const char *ap = t->xdg ? t->xdg->app_id : t->xw->class;
    fprintf(stderr, "wayland: window %llu mapped '%s' (%s)%s\n",
        (unsigned long long)t->id, ti ? ti : "", ap ? ap : "",
        t->xw ? " x11" : "");
}

static void on_unmap(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, unmap);
    t->mapped = false;
    if (g_focused == t) g_focused = NULL;
}

// it asks to be minimized: out of its pane, still running
static void on_xdg_minimize(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, min_l);
    g_hide = t->id;
}

// it asks for the whole screen, or out of it: the shell gives it
// the monitor's size and a window of its own there
static void on_xdg_fullscreen(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, full_l);
    bool on = t->xdg->requested.fullscreen;
    if (t->xdg->base->initialized) wlr_xdg_toplevel_set_fullscreen(t->xdg, on);
    t->full = on;
    g_full_asked = t->id;
    fprintf(stderr, "wayland: window %llu %s fullscreen\n",
        (unsigned long long)t->id, on ? "asks" : "leaves");
}

static void on_destroy(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, destroy);
    if (t->xdg || t->assoc) {
        wl_list_remove(&t->map.link);
        wl_list_remove(&t->unmap.link);
        wl_list_remove(&t->surf_l.link);
    }
    if (t->xw) {
        wl_list_remove(&t->assoc_l.link);
        wl_list_remove(&t->dissoc_l.link);
        wl_list_remove(&t->req_l.link);
        wl_list_remove(&t->act_l.link);
        wl_list_remove(&t->min_l.link);
        wl_list_remove(&t->full_l.link);
    }
    if (t->xdg) {
        wl_list_remove(&t->min_l.link);
        wl_list_remove(&t->full_l.link);
    }
    wl_list_remove(&t->destroy.link);
    wl_list_remove(&t->frame_l.link);
    wl_list_remove(&t->commit_l.link);
    wl_list_remove(&t->link);
    if (g_focused == t) g_focused = NULL;
    if (g_xtop == t) g_xtop = NULL;
    if (t->frame_buf) wlr_buffer_unlock(t->frame_buf);
    wlr_scene_output_destroy(t->sout);
    wlr_output_layout_remove(g_layout, t->out);
    wlr_output_destroy(t->out);
    fprintf(stderr, "wayland: window %llu closed\n", (unsigned long long)t->id);
    free(t);
}

// a window: its own output, at its own place
static struct toplevel *top_new(void) {
    struct toplevel *t = calloc(1, sizeof(*t));
    t->id  = g_next_id++;
    t->w   = g_w; t->h = g_h;
    t->x   = (int)(t->id * SPAN);
    t->out = wlr_headless_add_output(g_backend, t->w, t->h);
    wlr_output_init_render(t->out, g_allocator, g_renderer);
    t->frame_l.notify  = on_top_frame;  wl_signal_add(&t->out->events.frame,  &t->frame_l);
    t->commit_l.notify = on_top_commit; wl_signal_add(&t->out->events.commit, &t->commit_l);
    output_mode(t->out, t->w, t->h);
    struct wlr_output_layout_output *lo = wlr_output_layout_add(g_layout, t->out, t->x, 0);
    t->sout = wlr_scene_output_create(g_scene, t->out);
    wlr_scene_output_layout_add_output(g_scene_layout, lo, t->sout);
    return t;
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
    struct toplevel *t = top_new();
    t->xdg = xs->toplevel;
    t->tree = wlr_scene_xdg_surface_create(&g_scene->tree, xs);
    wlr_scene_node_set_position(&t->tree->node, t->x, 0);
    xs->data = t->tree;
    t->map.notify     = on_map;     wl_signal_add(&xs->surface->events.map,   &t->map);
    t->unmap.notify   = on_unmap;   wl_signal_add(&xs->surface->events.unmap, &t->unmap);
    t->destroy.notify = on_destroy; wl_signal_add(&xs->events.destroy,        &t->destroy);
    t->surf_l.notify  = on_surface_commit; wl_signal_add(&xs->surface->events.commit, &t->surf_l);
    t->min_l.notify   = on_xdg_minimize; wl_signal_add(&t->xdg->events.request_minimize, &t->min_l);
    t->full_l.notify  = on_xdg_fullscreen; wl_signal_add(&t->xdg->events.request_fullscreen, &t->full_l);
    wl_list_insert(g_tops.prev, &t->link);
    // sized before its first commit, so it opens the size of its pane
    wlr_xdg_toplevel_set_size(t->xdg, lg(t->w), lg(t->h));
    if (g_fullscreen) wlr_xdg_toplevel_set_fullscreen(t->xdg, true);
    fprintf(stderr, "wayland: window %llu new\n", (unsigned long long)t->id);
}

// ---- X11 windows ---------------------------------------------------------

// a dialog keeps its own size; a window, the pane's
static void on_xw_commit(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, surf_l);
    struct wlr_xwayland_surface *xs = t->xw;
    if (!t->sheet) { t->cw = xs->width; t->ch = xs->height; return; }
    if (xs->width <= 0 || xs->height <= 0) return;
    if (xs->width == t->w && xs->height == t->h) return;
    t->w = xs->width; t->h = xs->height;
    t->epoch += 1;
    output_mode(t->out, (int)ceil((t->w + SHEET_ROOM) * g_scale),
        (int)ceil((t->h + SHEET_ROOM) * g_scale));
}

static void on_xw_associate(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, assoc_l);
    struct wlr_surface *s = t->xw->surface;
    t->assoc = true;
    t->sheet = t->xw->parent || t->xw->modal;
    t->tree = wlr_scene_subsurface_tree_create(&g_scene->tree, s);
    wlr_scene_node_set_position(&t->tree->node, t->x, 0);
    t->map.notify    = on_map;       wl_signal_add(&s->events.map,    &t->map);
    t->unmap.notify  = on_unmap;     wl_signal_add(&s->events.unmap,  &t->unmap);
    t->surf_l.notify = on_xw_commit; wl_signal_add(&s->events.commit, &t->surf_l);
    // on its own screen from the start: its menus open beside it
    if (t->sheet)
        wlr_xwayland_surface_configure(t->xw, 0, 0,
            t->xw->width ? t->xw->width : 1, t->xw->height ? t->xw->height : 1);
    else top_size(t, lg(t->w), lg(t->h));
}

static void on_xw_dissociate(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, dissoc_l);
    if (!t->assoc) return;
    wl_list_remove(&t->map.link);
    wl_list_remove(&t->unmap.link);
    wl_list_remove(&t->surf_l.link);
    t->assoc  = false;
    t->mapped = false;
    t->tree   = NULL;
    if (g_focused == t) g_focused = NULL;
}

// it asks a size: a dialog has it, a window keeps its pane's
static void on_xw_request(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, req_l);
    struct wlr_xwayland_surface_configure_event *ev = data;
    if (t->sheet || !t->w)
        wlr_xwayland_surface_configure(t->xw, 0, 0, ev->width, ev->height);
    else
        top_size(t, lg(t->w), lg(t->h));
}

// X11 windows all sit at 0, 0 of the X screen (Xwayland sends input
// to the surface the pointer is on); a menu or tooltip (override
// redirect) places itself there, and goes where its window is ours
struct xpop {
    struct wlr_xwayland_surface *xs;
    struct wlr_scene_tree       *tree;
    struct wl_listener           assoc_l, dissoc_l, map, unmap, geo, destroy;
    bool                         assoc;
};

// the window it belongs to: its parent, else X's top one, else the
// newest
static int xpop_base(struct xpop *p) {
    struct toplevel *t, *b = NULL;
    wl_list_for_each(t, &g_tops, link) {
        if (!t->xw) continue;
        if (p->xs->parent && t->xw == p->xs->parent) return t->x;
        b = t;
    }
    if (g_xtop) return g_xtop->x;
    return b ? b->x : 0;
}

static void xpop_map(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, map);
    p->tree = wlr_scene_subsurface_tree_create(&g_scene->tree, p->xs->surface);
    wlr_scene_node_set_position(&p->tree->node, xpop_base(p) + p->xs->x, p->xs->y);
}
static void xpop_unmap(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, unmap);
    if (p->tree) wlr_scene_node_destroy(&p->tree->node);
    p->tree = NULL;
}
static void xpop_geo(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, geo);
    if (p->tree)
        wlr_scene_node_set_position(&p->tree->node, xpop_base(p) + p->xs->x, p->xs->y);
}
static void xpop_assoc(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, assoc_l);
    struct wlr_surface *s = p->xs->surface;
    p->assoc = true;
    p->map.notify   = xpop_map;   wl_signal_add(&s->events.map,   &p->map);
    p->unmap.notify = xpop_unmap; wl_signal_add(&s->events.unmap, &p->unmap);
}
static void xpop_dissoc(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, dissoc_l);
    if (!p->assoc) return;
    wl_list_remove(&p->map.link);
    wl_list_remove(&p->unmap.link);
    p->assoc = false;
}
static void xpop_destroy(struct wl_listener *l, void *data) {
    struct xpop *p = wl_container_of(l, p, destroy);
    if (p->assoc) { wl_list_remove(&p->map.link); wl_list_remove(&p->unmap.link); }
    wl_list_remove(&p->assoc_l.link);
    wl_list_remove(&p->dissoc_l.link);
    wl_list_remove(&p->geo.link);
    wl_list_remove(&p->destroy.link);
    free(p);
}

// it asks to come forward (Steam's tray: Library, Store)
static void on_xw_activate(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, act_l);
    g_asked = t->id;
}

// it asks to be minimized (its own button, Steam's Hide)
static void on_xw_minimize(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, min_l);
    struct wlr_xwayland_minimize_event *ev = data;
    wlr_xwayland_surface_set_minimized(t->xw, ev->minimize);
    if (ev->minimize) g_hide = t->id;
    else g_asked = t->id;
}

// an X11 window asks for the whole screen, or out of it
static void on_xw_fullscreen(struct wl_listener *l, void *data) {
    struct toplevel *t = wl_container_of(l, t, full_l);
    bool on = t->xw->fullscreen;
    wlr_xwayland_surface_set_fullscreen(t->xw, on);
    t->full = on;
    g_full_asked = t->id;
    fprintf(stderr, "wayland: window %llu %s fullscreen\n",
        (unsigned long long)t->id, on ? "asks" : "leaves");
}

// every program sees the base output alone: a window's own output
// changing size changed screens under it (Steam, Wine died)
static bool global_seen(const struct wl_client *c,
                        const struct wl_global *g, void *data) {
    if (strcmp(wl_global_get_interface(g)->name, "wl_output") != 0) return true;
    return g_base && g_base->global == g;
}

static void on_new_xw(struct wl_listener *l, void *data) {
    struct wlr_xwayland_surface *xs = data;
    if (xs->override_redirect) {
        struct xpop *p = calloc(1, sizeof(*p));
        p->xs = xs;
        p->assoc_l.notify  = xpop_assoc;   wl_signal_add(&xs->events.associate,    &p->assoc_l);
        p->dissoc_l.notify = xpop_dissoc;  wl_signal_add(&xs->events.dissociate,   &p->dissoc_l);
        p->geo.notify      = xpop_geo;     wl_signal_add(&xs->events.set_geometry, &p->geo);
        p->destroy.notify  = xpop_destroy; wl_signal_add(&xs->events.destroy,      &p->destroy);
        return;
    }
    struct toplevel *t = top_new();
    t->xw = xs;
    t->assoc_l.notify  = on_xw_associate;  wl_signal_add(&xs->events.associate,  &t->assoc_l);
    t->dissoc_l.notify = on_xw_dissociate; wl_signal_add(&xs->events.dissociate, &t->dissoc_l);
    t->req_l.notify    = on_xw_request;    wl_signal_add(&xs->events.request_configure, &t->req_l);
    t->act_l.notify    = on_xw_activate;   wl_signal_add(&xs->events.request_activate,  &t->act_l);
    t->min_l.notify    = on_xw_minimize;   wl_signal_add(&xs->events.request_minimize,  &t->min_l);
    t->full_l.notify   = on_xw_fullscreen; wl_signal_add(&xs->events.request_fullscreen, &t->full_l);
    t->destroy.notify  = on_destroy;       wl_signal_add(&xs->events.destroy,    &t->destroy);
    wl_list_insert(g_tops.prev, &t->link);
    fprintf(stderr, "wayland: window %llu new (x11)\n", (unsigned long long)t->id);
}

// we decorate, which is to say nobody does: clients drop their shadows and
// window buttons. a header bar that is the app's own widget stays
static void on_new_decoration(struct wl_listener *l, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *d = data;
    wlr_xdg_toplevel_decoration_v1_set_mode(d, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) if (t->xdg == d->toplevel) t->ssd = true;
}

// ---- menus and decorations -----------------------------------------------

// a surface's menu as its app announced it: a dbusmenu (KDE's appmenu,
// Qt) or a GMenuModel (GTK's gtk-shell); the shell reads it over D-Bus
struct menu_ref {
    struct wlr_surface *surface;
    int                 kind;   // 0 none, 1 dbusmenu, 2 gtk
    char                bus[256], path[256], app_path[256], win_path[256];
    struct wl_list      link;
};
static struct wl_list g_menus;
static struct wlr_server_decoration_manager *g_kde_deco;

static void menu_ref_gone(struct wl_resource *r) {
    struct menu_ref *m = wl_resource_get_user_data(r);
    if (!m) return;
    wl_list_remove(&m->link);
    free(m);
}

static struct menu_ref *menu_ref_new(struct wl_resource *surface) {
    struct menu_ref *m = calloc(1, sizeof(*m));
    m->surface = wlr_surface_from_resource(surface);
    wl_list_insert(&g_menus, &m->link);
    return m;
}

static struct menu_ref *menu_of(struct toplevel *t) {
    if (!t) return NULL;
    struct menu_ref *m;
    wl_list_for_each(m, &g_menus, link)
        if (m->surface == top_surface(t) && m->kind) return m;
    return NULL;
}

#define COPY(dst, src) snprintf(dst, sizeof dst, "%s", (src) ? (src) : "")

// gtk-shell: GTK says where its menu bar and actions live
static void gs_dbus(struct wl_client *c, struct wl_resource *r, const char *app_id,
        const char *app_menu, const char *menubar, const char *win_path,
        const char *app_path, const char *bus) {
    struct menu_ref *m = wl_resource_get_user_data(r);
    if (!m || !menubar || !*menubar) return;
    m->kind = 2;
    COPY(m->bus, bus); COPY(m->path, menubar);
    COPY(m->app_path, app_path); COPY(m->win_path, win_path);
}
static void gs_none(struct wl_client *c, struct wl_resource *r) {}
static void gs_present(struct wl_client *c, struct wl_resource *r, uint32_t t) {}
static void gs_focus(struct wl_client *c, struct wl_resource *r, const char *s) {}
static void gs_release(struct wl_client *c, struct wl_resource *r) { wl_resource_destroy(r); }
static void gs_gesture(struct wl_client *c, struct wl_resource *r, uint32_t serial,
        struct wl_resource *seat, uint32_t g) {}
static const struct gtk_surface1_interface gs_impl = {
    gs_dbus, gs_none, gs_none, gs_present, gs_focus, gs_release, gs_gesture };

static void shell_surface(struct wl_client *c, struct wl_resource *r, uint32_t id,
        struct wl_resource *surface) {
    struct wl_resource *res = wl_resource_create(c, &gtk_surface1_interface,
        wl_resource_get_version(r), id);
    wl_resource_set_implementation(res, &gs_impl, menu_ref_new(surface), menu_ref_gone);
}
static void shell_startup(struct wl_client *c, struct wl_resource *r, const char *s) {}
static void shell_bell(struct wl_client *c, struct wl_resource *r, struct wl_resource *s) {}
static const struct gtk_shell1_interface shell_impl = {
    shell_surface, shell_startup, shell_bell, shell_startup };

static void shell_bind(struct wl_client *c, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *r = wl_resource_create(c, &gtk_shell1_interface, version, id);
    wl_resource_set_implementation(r, &shell_impl, NULL, NULL);
    // the menu bar and the app menu are the shell's to show
    gtk_shell1_send_capabilities(r, 3);
}

// KDE's appmenu: a Qt app gives its dbusmenu's address
static void am_address(struct wl_client *c, struct wl_resource *r, const char *bus,
        const char *path) {
    struct menu_ref *m = wl_resource_get_user_data(r);
    if (!m) return;
    m->kind = 1;
    COPY(m->bus, bus); COPY(m->path, path);
}
static void am_release(struct wl_client *c, struct wl_resource *r) { wl_resource_destroy(r); }
static const struct org_kde_kwin_appmenu_interface am_impl = { am_address, am_release };

static void amm_create(struct wl_client *c, struct wl_resource *r, uint32_t id,
        struct wl_resource *surface) {
    struct wl_resource *res = wl_resource_create(c, &org_kde_kwin_appmenu_interface,
        wl_resource_get_version(r), id);
    wl_resource_set_implementation(res, &am_impl, menu_ref_new(surface), menu_ref_gone);
}
static const struct org_kde_kwin_appmenu_manager_interface amm_impl = { amm_create };

static void amm_bind(struct wl_client *c, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *r = wl_resource_create(c, &org_kde_kwin_appmenu_manager_interface,
        version, id);
    wl_resource_set_implementation(r, &amm_impl, NULL, NULL);
}

static void menus_init(void) {
    wl_list_init(&g_menus);
    wl_global_create(g_display, &gtk_shell1_interface, 5, NULL, shell_bind);
    wl_global_create(g_display, &org_kde_kwin_appmenu_manager_interface, 1, NULL, amm_bind);
    // GTK 3 drops its own title bar for KDE's server decoration
    g_kde_deco = wlr_server_decoration_manager_create(g_display);
    wlr_server_decoration_manager_set_default_mode(g_kde_deco,
        WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
}

// the window draws its own header: neither decoration protocol took it
static int csd_of(struct toplevel *t) {
    if (!t || t->xw) return 0;
    if (t->ssd) return 0;
    struct wlr_server_decoration *d;
    wl_list_for_each(d, &g_kde_deco->decorations, link)
        if (d->surface == t->xdg->base->surface
            && d->mode == WLR_SERVER_DECORATION_MANAGER_MODE_SERVER) return 0;
    return 1;
}

static int u_wlc_window_csd(uint64_t id) { return csd_of(top_of(id)); }

static int u_wlc_window_pid(uint64_t id);

// a dialog's window: the one it names as its parent, else a window of
// its own process that is no dialog; 0 when none
static uint64_t u_wlc_window_parent(uint64_t id) {
    struct toplevel *t = top_of(id), *p;
    if (!t || !t->sheet) return 0;
    if (t->xdg && t->xdg->parent) {
        wl_list_for_each(p, &g_tops, link) if (p->xdg == t->xdg->parent) return p->id;
        return 0;
    }
    if (t->xw && t->xw->parent) {
        wl_list_for_each(p, &g_tops, link) if (p->xw == t->xw->parent) return p->id;
        return 0;
    }
    int pid = u_wlc_window_pid(id);
    wl_list_for_each(p, &g_tops, link)
        if (p != t && !p->sheet && p->mapped && u_wlc_window_pid(p->id) == pid)
            return p->id;
    return 0;
}
static int u_wlc_window_menu_kind(uint64_t id) {
    struct menu_ref *m = menu_of(top_of(id)); return m ? m->kind : 0;
}
static const char *u_wlc_window_menu_bus(uint64_t id) {
    struct menu_ref *m = menu_of(top_of(id)); return m ? m->bus : "";
}
static const char *u_wlc_window_menu_path(uint64_t id) {
    struct menu_ref *m = menu_of(top_of(id)); return m ? m->path : "";
}
static const char *u_wlc_window_menu_app(uint64_t id) {
    struct menu_ref *m = menu_of(top_of(id)); return m ? m->app_path : "";
}
static const char *u_wlc_window_menu_win(uint64_t id) {
    struct menu_ref *m = menu_of(top_of(id)); return m ? m->win_path : "";
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

// a window's pane size, applied under the lock
static void apply_sizes(void) {
    struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) {
        if (t->sheet) continue;
        if (t->want_w > 0 && (t->want_w != t->w || t->want_h != t->h)) {
            t->w = t->want_w; t->h = t->want_h;
            t->asked_at = mono_now();
            t->mode_w = t->w; t->mode_h = t->h;
            top_size(t, lg(t->w), lg(t->h));
        }
    }
}

// the display runs on its own thread: clients are answered while the
// shell is idle (a tray-only app, a window not shown). every call from
// the shell takes the lock
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_thread;
static volatile int    g_stop, g_running;

static void *loop_thread(void *arg) {
    struct pollfd pf = { .fd = wl_event_loop_get_fd(g_loop), .events = POLLIN };
    while (!g_stop) {
        poll(&pf, 1, 16);
        pthread_mutex_lock(&g_lock);
        apply_sizes();
        wl_event_loop_dispatch(g_loop, 0);
        wl_display_flush_clients(g_display);
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

// ---- the api the .ag side calls ----------------------------------------

// the windows' keyboard: its layout (xkb's names) and its repeat,
// set before or after the display is up
static bool g_kb_up;
static char g_kb_layout[32] = "", g_kb_variant[32] = "";
static int  g_rep_rate = 25, g_rep_delay = 600;

static void kb_keymap(void) {
    struct xkb_rule_names rn = { 0 };
    if (g_kb_layout[0])  rn.layout  = g_kb_layout;
    if (g_kb_variant[0]) rn.variant = g_kb_variant;
    struct xkb_context *xctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap  *kmap = xkb_keymap_new_from_names(xctx, &rn, XKB_KEYMAP_COMPILE_NO_FLAGS);
    // a name xkb does not know: its default map
    if (!kmap) kmap = xkb_keymap_new_from_names(xctx, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(&g_kb, kmap);
    xkb_keymap_unref(kmap);
    xkb_context_unref(xctx);
}

static void u_wlc_key_layout(const char *layout, const char *variant) {
    snprintf(g_kb_layout, sizeof(g_kb_layout), "%s", layout ? layout : "");
    snprintf(g_kb_variant, sizeof(g_kb_variant), "%s", variant ? variant : "");
    if (g_kb_up) kb_keymap();
}

static void u_wlc_key_repeat(int rate, int delay_ms) {
    g_rep_rate = rate; g_rep_delay = delay_ms;
    if (g_kb_up) wlr_keyboard_set_repeat_info(&g_kb, rate, delay_ms);
}

WLC_API void wlc_key_layout(const char *layout, const char *variant) { pthread_mutex_lock(&g_lock); u_wlc_key_layout(layout, variant); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_key_repeat(int rate, int delay_ms) { pthread_mutex_lock(&g_lock); u_wlc_key_repeat(rate, delay_ms); pthread_mutex_unlock(&g_lock); }

// a new scale: every window its size again, its output the new scale
static void u_wlc_set_scale(double sc) {
    if (sc < 0.5 || sc > 4.0 || fabs(sc - g_scale) < 0.001) return;
    g_scale = sc;
    if (!g_display) return;
    struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) {
        if (t->sheet) {
            output_mode(t->out, (int)ceil((t->w + SHEET_ROOM) * g_scale),
                (int)ceil((t->h + SHEET_ROOM) * g_scale));
            continue;
        }
        t->asked_at = mono_now();
        t->mode_w = t->w; t->mode_h = t->h;
        top_size(t, lg(t->w), lg(t->h));
    }
}

WLC_API void wlc_set_scale(double sc) { pthread_mutex_lock(&g_lock); u_wlc_set_scale(sc); pthread_mutex_unlock(&g_lock); }

WLC_API int wlc_create(int w, int h) {
    if (g_display) return 1;
    pick_render_node();
    // the vulkan renderer allocates with the modifiers the shell's vulkan
    // device takes; gles2 on nvidia picks compressed ones it does not
    setenv("WLR_RENDERER", "vulkan", 0);
    // a window covering its output would be handed over as the app's
    // own buffer (direct scan-out): shared memory, not a dma-buf the
    // shell can import. it is always composed into our own buffers
    setenv("WLR_SCENE_DISABLE_DIRECT_SCANOUT", "1", 1);
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
    struct wlr_compositor *comp = wlr_compositor_create(g_display, 5, g_renderer);
    wlr_subcompositor_create(g_display);
    wlr_data_device_manager_create(g_display);
    g_layout = wlr_output_layout_create();
    g_scene  = wlr_scene_create();
    g_scene_layout = wlr_scene_attach_output_layout(g_scene, g_layout);
    g_xdg = wlr_xdg_shell_create(g_display, 3);
    // a scale under 2 reaches apps that take fractions (Firefox, GTK4)
    wlr_fractional_scale_manager_v1_create(g_display, 1);
    // it draws its buffer at the fraction and names its size by this
    wlr_viewporter_create(g_display);
    l_new_xdg.notify = on_new_xdg_surface;
    wl_signal_add(&g_xdg->events.new_surface, &l_new_xdg);
    struct wlr_xdg_decoration_manager_v1 *deco = wlr_xdg_decoration_manager_v1_create(g_display);
    l_deco.notify = on_new_decoration;
    wl_signal_add(&deco->events.new_toplevel_decoration, &l_deco);
    // the seat: a pointer and a keyboard, both fed by the pane
    g_seat = wlr_seat_create(g_display, "seat0");
    g_constraints = wlr_pointer_constraints_v1_create(g_display);
    g_new_constraint.notify = on_new_constraint;
    wl_signal_add(&g_constraints->events.new_constraint, &g_new_constraint);
    g_relative = wlr_relative_pointer_manager_v1_create(g_display);
    wlr_seat_set_capabilities(g_seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    wlr_keyboard_init(&g_kb, NULL, "silver");
    g_kb_up = true;
    kb_keymap();
    wlr_keyboard_set_repeat_info(&g_kb, g_rep_rate, g_rep_delay);
    l_kb_key.notify  = on_kb_key;  wl_signal_add(&g_kb.events.key,       &l_kb_key);
    l_kb_mods.notify = on_kb_mods; wl_signal_add(&g_kb.events.modifiers, &l_kb_mods);
    wlr_seat_set_keyboard(g_seat, &g_kb);
    // each window brings its own output; new ones open at this size
    g_w = w; g_h = h;
    wl_list_init(&g_tops);
    // X11 programs: Xwayland starts with its first client
    g_xwl = wlr_xwayland_create(g_display, comp, true);
    if (g_xwl) {
        l_new_xw.notify = on_new_xw;
        wl_signal_add(&g_xwl->events.new_surface, &l_new_xw);
        wlr_xwayland_set_seat(g_xwl, g_seat);
        fprintf(stderr, "wayland: x11 display %s\n", g_xwl->display_name);
    }
    menus_init();
    wl_display_set_global_filter(g_display, global_seen, NULL);
    // a base output at 0, never drawn: clients wait for one before
    // they open a window (windows sit from SPAN on)
    struct wlr_output *base = wlr_headless_add_output(g_backend, w, h);
    if (!base) { fprintf(stderr, "wayland: no output\n"); return 0; }
    wlr_output_init_render(base, g_allocator, g_renderer);
    output_mode(base, w, h);
    wlr_output_layout_add(g_layout, base, 0, 0);
    g_base = base;
    l_client.notify = on_client_created;
    wl_display_add_client_created_listener(g_display, &l_client);
    // snaps take wayland-<digit>*; never bare wayland-0
    static char name[32];
    g_socket = NULL;
    for (int i = 0; i < 32 && !g_socket; i++) {
        snprintf(name, sizeof name, "wayland-%d-orbiter", i);
        if (wl_display_add_socket(g_display, name) == 0) g_socket = name;
    }
    if (!g_socket) { fprintf(stderr, "wayland: no socket\n"); return 0; }
    setenv("WAYLAND_DISPLAY", g_socket, 1);
    if (!wlr_backend_start(g_backend)) { fprintf(stderr, "wayland: backend would not start\n"); return 0; }
    fprintf(stderr, "wayland: display %s, windows open at %dx%d\n", g_socket, w, h);
    pthread_create(&g_thread, NULL, loop_thread, NULL);
    g_running = 1;
    return 1;
}

// the library leaving (the shell closing): the thread ends first, or
// it runs on in unmapped code
__attribute__((destructor)) static void wlc_unload(void) {
    if (!g_running) return;
    g_stop = 1;
    pthread_join(g_thread, NULL);
    g_running = 0;
}

WLC_API const char *wlc_socket(void) { return g_socket ? g_socket : ""; }

// the X11 display of ours (:N), "" when none
WLC_API const char *wlc_x11_display(void) {
    return (g_xwl && g_xwl->display_name) ? g_xwl->display_name : "";
}

// the thread does the work now; kept for callers
WLC_API void wlc_pump(void) {}

// new windows open at this size
static void u_wlc_resize(int w, int h) { if (w > 0 && h > 0) { g_w = w; g_h = h; } }
// the real monitor: the base output (X's screen, a program's primary
// display) takes its size and refresh, so a game sees the screen it
// will fill and paces to it
static void u_wlc_screen(int w, int h, int hz) {
    if (!g_base || w <= 0 || h <= 0) return;
    int refresh = hz > 0 ? hz * 1000 : g_refresh;
    if (g_base->width == w && g_base->height == h && refresh == g_refresh) return;
    bool paced = refresh != g_refresh;
    g_refresh = refresh;
    output_mode(g_base, w, h);
    // windows already open take the new pace at their own size
    if (paced) {
        struct toplevel *t;
        wl_list_for_each(t, &g_tops, link)
            if (t->out) output_mode(t->out, t->out->width, t->out->height);
    }
    fprintf(stderr, "wayland: screen %dx%d at %d Hz\n", w, h, g_base->refresh / 1000);
}
static int u_wlc_window_full(uint64_t id) { struct toplevel *t = top_of(id); return t && t->full; }

// toplevels open fullscreen from now on; the focused one follows at once
static void u_wlc_fullscreen(int on) {
    g_fullscreen = on;
    if (g_focused && g_focused->xdg)
        wlr_xdg_toplevel_set_fullscreen(g_focused->xdg, on != 0);
}

// ---- windows, for the shell ------------------------------------------

// the mapped windows, oldest first
static int u_wlc_window_count(void) {
    if (!g_display) return 0;
    int n = 0; struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) if (t->mapped) n++;
    return n;
}

static uint64_t u_wlc_window_at(int i) {
    if (!g_display) return 0;
    int n = 0; struct toplevel *t;
    wl_list_for_each(t, &g_tops, link) if (t->mapped) { if (n == i) return t->id; n++; }
    return 0;
}

// the newest mapped window, 0 when none
static uint64_t u_wlc_window_newest(void) {
    if (!g_display) return 0;
    struct toplevel *t;
    wl_list_for_each_reverse(t, &g_tops, link) if (t->mapped) return t->id;
    return 0;
}

static int u_wlc_window_open(uint64_t id) {
    struct toplevel *t = top_of(id);
    return t && t->mapped;
}

static const char * u_wlc_window_title(uint64_t id) {
    struct toplevel *t = top_of(id);
    const char *s = !t ? NULL : t->xdg ? t->xdg->title : t->xw->title;
    return s ? s : "";
}

static const char * u_wlc_window_app(uint64_t id) {
    struct toplevel *t = top_of(id);
    const char *s = !t ? NULL : t->xdg ? t->xdg->app_id : t->xw->class;
    return s ? s : "";
}

// the process that made the window
static int u_wlc_window_pid(uint64_t id) {
    struct toplevel *t = top_of(id);
    if (!t) return 0;
    if (t->xw) return (int)t->xw->pid;
    struct wl_client *c = wl_resource_get_client(t->xdg->base->resource);
    pid_t pid = 0; uid_t uid; gid_t gid;
    if (c) wl_client_get_credentials(c, &pid, &uid, &gid);
    return (int)pid;
}

// the pane's size for this window, taken on the next pump
static void u_wlc_window_resize(uint64_t id, int w, int h) {
    struct toplevel *t = top_of(id);
    if (t && w > 0 && h > 0) { t->want_w = w; t->want_h = h; }
}

// shown again by the shell: no longer minimized
static void u_wlc_window_focus(uint64_t id) {
    struct toplevel *t = top_of(id);
    if (t && t->xw && t->xw->minimized)
        wlr_xwayland_surface_set_minimized(t->xw, false);
    focus_toplevel(t);
}

// ask the window to close, as its own close button would
static void u_wlc_window_close(uint64_t id) {
    struct toplevel *t = top_of(id);
    if (t && t->xdg) wlr_xdg_toplevel_send_close(t->xdg);
    else if (t && t->xw) wlr_xwayland_surface_close(t->xw);
}

// a window that did not answer its close: an X11 one loses its
// connection to X (as xkill does: its pid is its sandbox's), a
// Wayland one's process ends
static void u_wlc_window_kill(uint64_t id) {
    struct toplevel *t = top_of(id);
    if (!t) return;
    if (t->xw) {
        if (!g_xwl || !g_xwl->display_name) return;
        xcb_connection_t *c = xcb_connect(g_xwl->display_name, NULL);
        int pid = 0;
        if (!xcb_connection_has_error(c)) {
            // Xwayland's own view of the client: its real pid
            xcb_res_client_id_spec_t spec = { t->xw->window_id,
                XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID };
            xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(c,
                xcb_res_query_client_ids(c, 1, &spec), NULL);
            if (r) {
                xcb_res_client_id_value_iterator_t it =
                    xcb_res_query_client_ids_ids_iterator(r);
                for (; it.rem; xcb_res_client_id_value_next(&it))
                    if ((it.data->spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) &&
                        xcb_res_client_id_value_value_length(it.data) >= 1)
                        pid = (int)*xcb_res_client_id_value_value(it.data);
                free(r);
            }
            xcb_kill_client(c, t->xw->window_id);
            xcb_flush(c);
        }
        xcb_disconnect(c);
        if (pid > 1 && pid != getpid()) kill(pid, SIGKILL);
        fprintf(stderr, "wayland: window %llu ended (X client, process %d)\n", (unsigned long long)id, pid);
        return;
    }
    int pid = u_wlc_window_pid(id);
    if (pid <= 1 || pid == getpid()) return;
    kill(pid, SIGKILL);
    fprintf(stderr, "wayland: window %llu ended (process %d)\n", (unsigned long long)id, pid);
}

// a window's frame: its id (the buffer, reused by the swapchain), and its
// dma-buf
static uint64_t u_wlc_frame_id(uint64_t id) {
    struct toplevel *t = top_of(id);
    return (t && t->frame_buf) ? (uint64_t)(uintptr_t)t->frame_buf : 0;
}
static uint64_t u_wlc_window_epoch(uint64_t id) { struct toplevel *t = top_of(id); return t ? t->epoch : 0; }
// a window's own size (a dialog's frame has room past it)
// in pixels: a dialog's size is its own units, a window's the pane's
static int u_wlc_window_w(uint64_t id) { struct toplevel *t = top_of(id); return !t ? 0 : t->sheet ? (int)lround(t->w * g_scale) : t->w; }
static int u_wlc_window_h(uint64_t id) { struct toplevel *t = top_of(id); return !t ? 0 : t->sheet ? (int)lround(t->h * g_scale) : t->h; }
static uint64_t u_wlc_frame_gen(uint64_t id) { struct toplevel *t = top_of(id); return t ? t->gen : 0; }
static int u_wlc_frame_w(uint64_t id) { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.width  : 0; }
static int u_wlc_frame_h(uint64_t id) { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.height : 0; }
static uint32_t u_wlc_frame_format(uint64_t id)   { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.format : 0; }
static uint64_t u_wlc_frame_modifier(uint64_t id) { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.modifier : 0; }
static uint32_t u_wlc_frame_stride(uint64_t id)   { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.stride[0] : 0; }
static uint32_t u_wlc_frame_offset(uint64_t id)   { struct toplevel *t = top_of(id); return (t && t->frame_buf) ? t->frame.offset[0] : 0; }
// a fresh fd for an import, the caller's to consume
static int u_wlc_frame_fd(uint64_t id) {
    struct toplevel *t = top_of(id);
    return (t && t->frame_buf) ? fcntl(t->frame.fd[0], F_DUPFD_CLOEXEC, 0) : -1;
}

// ---- input from the pane -----------------------------------------------

// a point in a window's pane, as the scene's: past the window's place
static struct wlr_surface *surface_at(struct toplevel *t, double x, double y, double *sx, double *sy) {
    // the pane's pixels in the window's own units
    struct wlr_scene_node *n = wlr_scene_node_at(&g_scene->tree.node,
        t->x + x / g_scale, y / g_scale, sx, sy);
    if (!n || n->type != WLR_SCENE_NODE_BUFFER) return NULL;
    struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(n));
    return ss ? ss->surface : NULL;
}

struct held { struct wl_listener destroy; };

static void on_constraint_destroy(struct wl_listener *l, void *data) {
    struct held *h = wl_container_of(l, h, destroy);
    if (g_held == data) g_held = NULL;
    wl_list_remove(&h->destroy.link);
    free(h);
}

// the pointer over a surface: its constraint, if it asks one, holds
static void hold_for(struct wlr_surface *s) {
    struct wlr_pointer_constraint_v1 *c = s ?
        wlr_pointer_constraints_v1_constraint_for_surface(g_constraints,
            s, g_seat) : NULL;
    if (c == g_held) return;
    if (g_held) wlr_pointer_constraint_v1_send_deactivated(g_held);
    g_held = c;
    if (c) wlr_pointer_constraint_v1_send_activated(c);
}

static void on_new_constraint(struct wl_listener *l, void *data) {
    struct wlr_pointer_constraint_v1 *c = data;
    struct held *h = calloc(1, sizeof(*h));
    h->destroy.notify = on_constraint_destroy;
    wl_signal_add(&c->events.destroy, &h->destroy);
    if (c->surface == g_seat->pointer_state.focused_surface) hold_for(c->surface);
}

// the window whose pointer is held, 0 none; a confined one still
// moves (a menu kept inside its window), only a locked one is held
static uint64_t u_wlc_pointer_held(void) {
    if (!g_held || g_held->type != WLR_POINTER_CONSTRAINT_V1_LOCKED) return 0;
    struct wlr_surface *root = wlr_surface_get_root_surface(g_held->surface);
    struct toplevel *t;
    wl_list_for_each(t, &g_tops, link)
        if (top_surface(t) == root) return t->id;
    return 0;
}

// raw motion (the mouse's own counts) to the window under the pointer
static void u_wlc_pointer_relative(float dx, float dy) {
    if (!g_seat || !g_relative) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    wlr_relative_pointer_manager_v1_send_relative_motion(g_relative, g_seat,
        us, dx, dy, dx, dy);
    wlr_seat_pointer_notify_frame(g_seat);
}

static void u_wlc_pointer_motion(uint64_t id, float x, float y) {
    struct toplevel *t = top_of(id);
    if (!g_seat || !t) return;
    x_raise(t);
    double sx, sy;
    struct wlr_surface *s = surface_at(t, x, y, &sx, &sy);
    if (s) {
        wlr_seat_pointer_notify_enter(g_seat, s, sx, sy);
        // a held pointer stays where it is: the window reads raw motion
        if (!g_held || g_held->surface != s ||
            g_held->type != WLR_POINTER_CONSTRAINT_V1_LOCKED)
            wlr_seat_pointer_notify_motion(g_seat, now_ms(), sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(g_seat);
    }
    hold_for(s);
    wlr_seat_pointer_notify_frame(g_seat);
}

// button: 0 left, 1 right, 2 middle (the pane's numbering)
static void u_wlc_pointer_button(uint64_t id, float x, float y, int button, int state) {
    struct toplevel *t = top_of(id);
    if (!g_seat || !t) return;
    x_raise(t);
    double sx, sy;
    struct wlr_surface *s = surface_at(t, x, y, &sx, &sy);
    // a press gives the window the keyboard
    if (state) focus_toplevel(t);
    // a held pointer keeps its surface and place
    bool held = g_held &&
        g_seat->pointer_state.focused_surface == g_held->surface;
    if (state && s && !held) wlr_seat_pointer_notify_enter(g_seat, s, sx, sy);
    uint32_t code = button == 1 ? BTN_RIGHT : button == 2 ? BTN_MIDDLE : BTN_LEFT;
    wlr_seat_pointer_notify_button(g_seat, now_ms(), code, state ? WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
    wlr_seat_pointer_notify_frame(g_seat);
}

// wheel: dy in notches, dx across; a notch is 120 discrete steps
static void u_wlc_pointer_axis(float dx, float dy) {
    if (!g_seat) return;
    uint32_t t = now_ms();
    int32_t sy = (int32_t)lroundf(-dy * WLR_POINTER_AXIS_DISCRETE_STEP);
    int32_t sx = (int32_t)lroundf(-dx * WLR_POINTER_AXIS_DISCRETE_STEP);
    if (dy != 0.0f) wlr_seat_pointer_notify_axis(g_seat, t, WLR_AXIS_ORIENTATION_VERTICAL,   -dy * 15.0, sy, WLR_AXIS_SOURCE_WHEEL);
    if (dx != 0.0f) wlr_seat_pointer_notify_axis(g_seat, t, WLR_AXIS_ORIENTATION_HORIZONTAL, -dx * 15.0, sx, WLR_AXIS_SOURCE_WHEEL);
    wlr_seat_pointer_notify_frame(g_seat);
}

// the key goes to the focused window
static void u_wlc_key(int glfw, int state) {
    if (!g_seat) return;
    int code = evdev_from_glfw(glfw);
    if (code < 0) return;
    struct wlr_keyboard_key_event ev = {
        .time_msec = now_ms(), .keycode = (uint32_t)code, .update_state = true,
        .state = state ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED };
    wlr_keyboard_notify_key(&g_kb, &ev);
}

// every key the window still holds goes up: one held when it lost the
// keyboard would repeat in it forever (Wayland repeats client side)
static void u_wlc_keys_release(void) {
    if (!g_seat) return;
    uint32_t codes[WLR_KEYBOARD_KEYS_CAP];
    size_t   n = g_kb.num_keycodes;
    memcpy(codes, g_kb.keycodes, n * sizeof(uint32_t));
    for (size_t i = 0; i < n; i++) {
        struct wlr_keyboard_key_event ev = {
            .time_msec = now_ms(), .keycode = codes[i], .update_state = true,
            .state = WL_KEYBOARD_KEY_STATE_RELEASED };
        wlr_keyboard_notify_key(&g_kb, &ev);
    }
}


// ---- the locked api ----------------------------------------------------
WLC_API void wlc_resize(int w, int h) { pthread_mutex_lock(&g_lock); u_wlc_resize(w, h); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_screen(int w, int h, int hz) { pthread_mutex_lock(&g_lock); u_wlc_screen(w, h, hz); pthread_mutex_unlock(&g_lock); }
WLC_API int wlc_window_full(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_full(id); pthread_mutex_unlock(&g_lock); return r; }
// the window that last asked into or out of fullscreen, once (0 none)
WLC_API uint64_t wlc_window_full_asked(void) {
    pthread_mutex_lock(&g_lock);
    uint64_t r = g_full_asked;
    g_full_asked = 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}
WLC_API void wlc_fullscreen(int on) { pthread_mutex_lock(&g_lock); u_wlc_fullscreen(on); pthread_mutex_unlock(&g_lock); }
WLC_API int wlc_window_count(void) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_count(); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint64_t wlc_window_at(int i) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_window_at(i); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint64_t wlc_window_newest(void) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_window_newest(); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_window_open(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_open(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API const char * wlc_window_title(uint64_t id) { static char b[512]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_title(id)); pthread_mutex_unlock(&g_lock); return b; }
WLC_API const char * wlc_window_app(uint64_t id) { static char b[512]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_app(id)); pthread_mutex_unlock(&g_lock); return b; }
WLC_API int wlc_window_pid(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_pid(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API void wlc_window_resize(uint64_t id, int w, int h) { pthread_mutex_lock(&g_lock); u_wlc_window_resize(id, w, h); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_window_focus(uint64_t id) { pthread_mutex_lock(&g_lock); u_wlc_window_focus(id); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_window_close(uint64_t id) { pthread_mutex_lock(&g_lock); u_wlc_window_close(id); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_window_kill(uint64_t id) { pthread_mutex_lock(&g_lock); u_wlc_window_kill(id); pthread_mutex_unlock(&g_lock); }
WLC_API uint64_t wlc_frame_id(uint64_t id) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_frame_id(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint64_t wlc_frame_gen(uint64_t id) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_frame_gen(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_frame_w(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_frame_w(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_frame_h(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_frame_h(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint32_t wlc_frame_format(uint64_t id) { pthread_mutex_lock(&g_lock); uint32_t r = u_wlc_frame_format(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint64_t wlc_frame_modifier(uint64_t id) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_frame_modifier(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint32_t wlc_frame_stride(uint64_t id) { pthread_mutex_lock(&g_lock); uint32_t r = u_wlc_frame_stride(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint32_t wlc_frame_offset(uint64_t id) { pthread_mutex_lock(&g_lock); uint32_t r = u_wlc_frame_offset(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_frame_fd(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_frame_fd(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API void wlc_pointer_motion(uint64_t id, float x, float y) { pthread_mutex_lock(&g_lock); u_wlc_pointer_motion(id, x, y); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_pointer_button(uint64_t id, float x, float y, int button, int state) { pthread_mutex_lock(&g_lock); u_wlc_pointer_button(id, x, y, button, state); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_pointer_axis(float dx, float dy) { pthread_mutex_lock(&g_lock); u_wlc_pointer_axis(dx, dy); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_key(int glfw, int state) { pthread_mutex_lock(&g_lock); u_wlc_key(glfw, state); pthread_mutex_unlock(&g_lock); }
WLC_API void wlc_keys_release(void) { pthread_mutex_lock(&g_lock); u_wlc_keys_release(); pthread_mutex_unlock(&g_lock); }
// the window that last asked to come forward, once (0 none)
WLC_API uint64_t wlc_pointer_held(void) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_pointer_held(); pthread_mutex_unlock(&g_lock); return r; }
WLC_API void wlc_pointer_relative(float dx, float dy) { pthread_mutex_lock(&g_lock); u_wlc_pointer_relative(dx, dy); pthread_mutex_unlock(&g_lock); }

// the window that last asked to be minimized, once (0 none)
WLC_API uint64_t wlc_window_hide_asked(void) {
    pthread_mutex_lock(&g_lock);
    uint64_t r = g_hide;
    g_hide = 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}

WLC_API uint64_t wlc_window_asked(void) {
    pthread_mutex_lock(&g_lock);
    uint64_t r = g_asked;
    g_asked = 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}
WLC_API uint64_t wlc_window_epoch(uint64_t id) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_window_epoch(id); pthread_mutex_unlock(&g_lock); return r; }
// it has drawn at the size last asked of it
WLC_API int wlc_window_drawn(uint64_t id) {
    pthread_mutex_lock(&g_lock);
    struct toplevel *t = top_of(id);
    int r = t && t->cw == lg(t->w) && t->ch == lg(t->h);
    pthread_mutex_unlock(&g_lock);
    return r;
}
WLC_API int wlc_window_w(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_w(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_window_h(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_h(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_window_csd(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_csd(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API uint64_t wlc_window_parent(uint64_t id) { pthread_mutex_lock(&g_lock); uint64_t r = u_wlc_window_parent(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API int wlc_window_menu_kind(uint64_t id) { pthread_mutex_lock(&g_lock); int r = u_wlc_window_menu_kind(id); pthread_mutex_unlock(&g_lock); return r; }
WLC_API const char *wlc_window_menu_bus(uint64_t id) { static char b[256]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_menu_bus(id)); pthread_mutex_unlock(&g_lock); return b; }
WLC_API const char *wlc_window_menu_path(uint64_t id) { static char b[256]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_menu_path(id)); pthread_mutex_unlock(&g_lock); return b; }
WLC_API const char *wlc_window_menu_app(uint64_t id) { static char b[256]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_menu_app(id)); pthread_mutex_unlock(&g_lock); return b; }
WLC_API const char *wlc_window_menu_win(uint64_t id) { static char b[256]; pthread_mutex_lock(&g_lock); snprintf(b, sizeof b, "%s", u_wlc_window_menu_win(id)); pthread_mutex_unlock(&g_lock); return b; }

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
        if (g_xwl && g_xwl->display_name) setenv("DISPLAY", g_xwl->display_name, 1);
        else unsetenv("DISPLAY");
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    return (int)pid;
}

#endif
