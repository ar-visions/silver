// wayland-win.c: the compositor's surface on a system that has no
// wayland. windows has none -- no wlroots, no xcb, no dma-buf -- so
// every call reports nothing: no socket, no windows, no frames. the
// panes that would show guest windows simply stay empty
#ifdef _WIN32

#include <stdint.h>
#include <stdbool.h>

// a dll exports nothing unnamed; wayland owns these, others import
#ifndef AU_LINK_wayland
#define AU_LINK_wayland __attribute__((dllimport))
#endif
#define WLC_API AU_LINK_wayland

WLC_API int32_t wlc_create(int32_t w, int32_t h) { (void)w; (void)h; return 0; }
WLC_API const char* wlc_socket(void) { return ""; }
WLC_API const char* wlc_x11_display(void) { return ""; }
WLC_API void wlc_pump(void) {  }
WLC_API void wlc_resize(int32_t w, int32_t h) { (void)w; (void)h; }
WLC_API void wlc_fullscreen(int32_t on) { (void)on; }
WLC_API void wlc_screen(int32_t w, int32_t h, int32_t hz) { (void)w; (void)h; (void)hz; }
WLC_API int32_t wlc_window_full(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_window_full_asked(void) { return 0; }
WLC_API int32_t wlc_window_count(void) { return 0; }
WLC_API uint64_t wlc_window_at(int32_t i) { (void)i; return 0; }
WLC_API uint64_t wlc_window_newest(void) { return 0; }
WLC_API uint64_t wlc_window_asked(void) { return 0; }
WLC_API uint64_t wlc_window_hide_asked(void) { return 0; }
WLC_API int32_t wlc_window_open(uint64_t id) { (void)id; return 0; }
WLC_API const char* wlc_window_title(uint64_t id) { (void)id; return ""; }
WLC_API const char* wlc_window_app(uint64_t id) { (void)id; return ""; }
WLC_API int32_t wlc_window_pid(uint64_t id) { (void)id; return 0; }
WLC_API void wlc_window_resize(uint64_t id, int32_t w, int32_t h) { (void)id; (void)w; (void)h; }
WLC_API void wlc_window_focus(uint64_t id) { (void)id; }
WLC_API void wlc_window_close(uint64_t id) { (void)id; }
WLC_API void wlc_window_kill(uint64_t id) { (void)id; }
WLC_API int32_t wlc_window_csd(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_window_parent(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_window_epoch(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_window_w(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_window_drawn(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_window_h(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_window_menu_kind(uint64_t id) { (void)id; return 0; }
WLC_API const char* wlc_window_menu_bus(uint64_t id) { (void)id; return ""; }
WLC_API const char* wlc_window_menu_path(uint64_t id) { (void)id; return ""; }
WLC_API const char* wlc_window_menu_app(uint64_t id) { (void)id; return ""; }
WLC_API const char* wlc_window_menu_win(uint64_t id) { (void)id; return ""; }
WLC_API uint64_t wlc_frame_id(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_frame_gen(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_frame_take(uint64_t id) { (void)id; return 0; }
WLC_API void wlc_frame_drop(uint64_t b) { (void)b; }
WLC_API int wlc_frame_ready(uint64_t b, int ms) { (void)b; (void)ms; return 0; }
WLC_API int32_t wlc_frame_w(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_frame_h(uint64_t id) { (void)id; return 0; }
WLC_API uint32_t wlc_frame_format(uint64_t id) { (void)id; return 0; }
WLC_API uint64_t wlc_frame_modifier(uint64_t id) { (void)id; return 0; }
WLC_API uint32_t wlc_frame_stride(uint64_t id) { (void)id; return 0; }
WLC_API uint32_t wlc_frame_offset(uint64_t id) { (void)id; return 0; }
WLC_API int32_t wlc_frame_fd(uint64_t id) { (void)id; return 0; }
WLC_API void wlc_pointer_motion(uint64_t id, float x, float y) { (void)id; (void)x; (void)y; }
WLC_API void wlc_pointer_button(uint64_t id, float x, float y, int32_t button, int32_t state) { (void)id; (void)x; (void)y; (void)button; (void)state; }
WLC_API void wlc_pointer_axis(float dx, float dy) { (void)dx; (void)dy; }
WLC_API uint64_t wlc_pointer_held(void) { return 0; }
WLC_API void wlc_pointer_relative(float dx, float dy) { (void)dx; (void)dy; }
WLC_API void wlc_key(int32_t glfw, int32_t state) { (void)glfw; (void)state; }
WLC_API void wlc_key_layout(const char* layout, const char* variant) { (void)layout; (void)variant; }
WLC_API void wlc_key_repeat(int32_t rate, int32_t delay_ms) { (void)rate; (void)delay_ms; }
WLC_API void wlc_set_scale(double sc) { (void)sc; }
WLC_API void wlc_keys_release(void) {  }
WLC_API int32_t wlc_spawn(const char* cmd) { (void)cmd; return 0; }

#endif
