// desktop hosts: Win32 on windows, xcb on linux. apple lives in platform.mm
#define _GNU_SOURCE   // clock_gettime, strdup, usleep under -std=c99
#include "host.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(__APPLE__)
/* see devices.mm */
#elif defined(_WIN32)
// ================================================================ windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <xinput.h>

struct platform_window {
    platform_key_fn    on_key;
    platform_char_fn   on_char;
    platform_mouse_fn  on_mouse;
    platform_cursor_fn on_cursor;
    platform_enter_fn  on_enter;
    platform_scroll_fn on_scroll;
    platform_drop_fn   on_drop;
    platform_size_fn   on_size;
    platform_touch_fn  on_touch;
    platform_focus_fn  on_focus;
    void*              user;
    char               keys[PLATFORM_KEY_LAST + 1];
    int                mods;
    bool               should_close;
    bool               fullscreen;
    bool               tracking;
    int                aspect_num, aspect_den;
    char*              clip;
    HWND               hwnd;
    HCURSOR            cursor;
    DWORD              saved_style;
    RECT               saved_rect;
    WCHAR              high_surrogate;
};

static double g_freq, g_t0;

const char* platform_surface_extension(void) { return "VK_KHR_win32_surface"; }

double platform_time(void) {
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart / g_freq - g_t0;
}

static int mods_now(void) {
    int m = 0;
    if (GetKeyState(VK_SHIFT)   & 0x8000) m |= PLATFORM_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= PLATFORM_MOD_CONTROL;
    if (GetKeyState(VK_MENU)    & 0x8000) m |= PLATFORM_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) m |= PLATFORM_MOD_SUPER;
    return m;
}

static int vk_to_key(WPARAM vk, LPARAM lp) {
    bool ext = (lp >> 24) & 1;
    UINT sc  = (lp >> 16) & 0xff;
    if (vk >= 'A' && vk <= 'Z') return (int)vk;
    if (vk >= '0' && vk <= '9') return (int)vk;
    if (vk >= VK_F1 && vk <= VK_F12) return PLATFORM_KEY_F1 + (int)(vk - VK_F1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return PLATFORM_KEY_KP_0 + (int)(vk - VK_NUMPAD0);
    switch (vk) {
        case VK_SPACE:      return PLATFORM_KEY_SPACE;
        case VK_OEM_7:      return PLATFORM_KEY_APOSTROPHE;
        case VK_OEM_COMMA:  return PLATFORM_KEY_COMMA;
        case VK_OEM_MINUS:  return PLATFORM_KEY_MINUS;
        case VK_OEM_PERIOD: return PLATFORM_KEY_PERIOD;
        case VK_OEM_2:      return PLATFORM_KEY_SLASH;
        case VK_OEM_1:      return PLATFORM_KEY_SEMICOLON;
        case VK_OEM_PLUS:   return PLATFORM_KEY_EQUAL;
        case VK_OEM_4:      return PLATFORM_KEY_LEFT_BRACKET;
        case VK_OEM_5:      return PLATFORM_KEY_BACKSLASH;
        case VK_OEM_6:      return PLATFORM_KEY_RIGHT_BRACKET;
        case VK_OEM_3:      return PLATFORM_KEY_GRAVE_ACCENT;
        case VK_ESCAPE:     return PLATFORM_KEY_ESCAPE;
        case VK_RETURN:     return ext ? PLATFORM_KEY_KP_ENTER : PLATFORM_KEY_ENTER;
        case VK_TAB:        return PLATFORM_KEY_TAB;
        case VK_BACK:       return PLATFORM_KEY_BACKSPACE;
        case VK_INSERT:     return PLATFORM_KEY_INSERT;
        case VK_DELETE:     return PLATFORM_KEY_DELETE;
        case VK_RIGHT:      return PLATFORM_KEY_RIGHT;
        case VK_LEFT:       return PLATFORM_KEY_LEFT;
        case VK_DOWN:       return PLATFORM_KEY_DOWN;
        case VK_UP:         return PLATFORM_KEY_UP;
        case VK_PRIOR:      return PLATFORM_KEY_PAGE_UP;
        case VK_NEXT:       return PLATFORM_KEY_PAGE_DOWN;
        case VK_HOME:       return PLATFORM_KEY_HOME;
        case VK_END:        return PLATFORM_KEY_END;
        case VK_CAPITAL:    return PLATFORM_KEY_CAPS_LOCK;
        case VK_SCROLL:     return PLATFORM_KEY_SCROLL_LOCK;
        case VK_NUMLOCK:    return PLATFORM_KEY_NUM_LOCK;
        case VK_SNAPSHOT:   return PLATFORM_KEY_PRINT_SCREEN;
        case VK_PAUSE:      return PLATFORM_KEY_PAUSE;
        case VK_DECIMAL:    return PLATFORM_KEY_KP_DECIMAL;
        case VK_DIVIDE:     return PLATFORM_KEY_KP_DIVIDE;
        case VK_MULTIPLY:   return PLATFORM_KEY_KP_MULTIPLY;
        case VK_SUBTRACT:   return PLATFORM_KEY_KP_SUBTRACT;
        case VK_ADD:        return PLATFORM_KEY_KP_ADD;
        case VK_SHIFT:      return MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? PLATFORM_KEY_RIGHT_SHIFT : PLATFORM_KEY_LEFT_SHIFT;
        case VK_CONTROL:    return ext ? PLATFORM_KEY_RIGHT_CONTROL : PLATFORM_KEY_LEFT_CONTROL;
        case VK_MENU:       return ext ? PLATFORM_KEY_RIGHT_ALT : PLATFORM_KEY_LEFT_ALT;
        case VK_LWIN:       return PLATFORM_KEY_LEFT_SUPER;
        case VK_RWIN:       return PLATFORM_KEY_RIGHT_SUPER;
        case VK_APPS:       return PLATFORM_KEY_MENU;
    }
    return 0;
}

static void key_event(platform_window* w, int key, int scan, int action, int mods) {
    if (key < 0 || key > PLATFORM_KEY_LAST) key = 0;
    if (key) w->keys[key] = action != PLATFORM_RELEASE;
    w->mods = mods;
    if (w->on_key) w->on_key(w, key, scan, action, mods);
}

static void mouse_event(platform_window* w, int b, int action) {
    if (action == PLATFORM_PRESS) SetCapture(w->hwnd); else ReleaseCapture();
    w->mods = mods_now();
    if (w->on_mouse) w->on_mouse(w, b, action, w->mods);
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    platform_window* w = (platform_window*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!w) return DefWindowProcW(h, msg, wp, lp);
    switch (msg) {
        case WM_CLOSE: w->should_close = true; return 0;
        case WM_SIZE: {
            if (wp == SIZE_MINIMIZED) return 0;
            if (w->on_size) w->on_size(w, LOWORD(lp), HIWORD(lp));
            return 0;
        }
        case WM_SETFOCUS:  if (w->on_focus) w->on_focus(w, 1); return 0;
        case WM_KILLFOCUS:
            memset(w->keys, 0, sizeof(w->keys)); w->mods = 0;
            if (w->on_focus) w->on_focus(w, 0);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) { SetCursor(w->cursor ? w->cursor : LoadCursorW(NULL, IDC_ARROW)); return TRUE; }
            break;
        case WM_MOUSEMOVE: {
            if (!w->tracking) {
                TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, h, 0 };
                TrackMouseEvent(&t);
                w->tracking = true;
                if (w->on_enter) w->on_enter(w, 1);
            }
            if (w->on_cursor) w->on_cursor(w, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        }
        case WM_MOUSELEAVE: w->tracking = false; if (w->on_enter) w->on_enter(w, 0); return 0;
        case WM_LBUTTONDOWN: mouse_event(w, PLATFORM_MOUSE_LEFT,   PLATFORM_PRESS);   return 0;
        case WM_LBUTTONUP:   mouse_event(w, PLATFORM_MOUSE_LEFT,   PLATFORM_RELEASE); return 0;
        case WM_RBUTTONDOWN: mouse_event(w, PLATFORM_MOUSE_RIGHT,  PLATFORM_PRESS);   return 0;
        case WM_RBUTTONUP:   mouse_event(w, PLATFORM_MOUSE_RIGHT,  PLATFORM_RELEASE); return 0;
        case WM_MBUTTONDOWN: mouse_event(w, PLATFORM_MOUSE_MIDDLE, PLATFORM_PRESS);   return 0;
        case WM_MBUTTONUP:   mouse_event(w, PLATFORM_MOUSE_MIDDLE, PLATFORM_RELEASE); return 0;
        case WM_XBUTTONDOWN: mouse_event(w, 2 + GET_XBUTTON_WPARAM(wp), PLATFORM_PRESS);   return TRUE;
        case WM_XBUTTONUP:   mouse_event(w, 2 + GET_XBUTTON_WPARAM(wp), PLATFORM_RELEASE); return TRUE;
        case WM_MOUSEWHEEL:
            w->mods = mods_now();
            if (w->on_scroll) w->on_scroll(w, 0, (double)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
            return 0;
        case WM_MOUSEHWHEEL:
            w->mods = mods_now();
            if (w->on_scroll) w->on_scroll(w, -(double)GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA, 0);
            return 0;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: {
            int key = vk_to_key(wp, lp);
            int action = (lp & (1 << 30)) ? PLATFORM_REPEAT : PLATFORM_PRESS;
            key_event(w, key, (lp >> 16) & 0x1ff, action, mods_now());
            if (msg == WM_SYSKEYDOWN && wp != VK_F10) break;
            return 0;
        }
        case WM_KEYUP: case WM_SYSKEYUP: {
            int key = vk_to_key(wp, lp);
            key_event(w, key, (lp >> 16) & 0x1ff, PLATFORM_RELEASE, mods_now());
            // a shift release for the other side never arrives while both are down
            if (wp == VK_SHIFT) {
                if (!(GetKeyState(VK_LSHIFT) & 0x8000) && w->keys[PLATFORM_KEY_LEFT_SHIFT])
                    key_event(w, PLATFORM_KEY_LEFT_SHIFT, 0, PLATFORM_RELEASE, mods_now());
                if (!(GetKeyState(VK_RSHIFT) & 0x8000) && w->keys[PLATFORM_KEY_RIGHT_SHIFT])
                    key_event(w, PLATFORM_KEY_RIGHT_SHIFT, 0, PLATFORM_RELEASE, mods_now());
            }
            if (msg == WM_SYSKEYUP) break;
            return 0;
        }
        case WM_CHAR: case WM_SYSCHAR: {
            WCHAR c = (WCHAR)wp;
            uint32_t cp;
            if (c >= 0xD800 && c <= 0xDBFF) { w->high_surrogate = c; return 0; }
            if (c >= 0xDC00 && c <= 0xDFFF && w->high_surrogate)
                cp = 0x10000 + ((w->high_surrogate - 0xD800) << 10) + (c - 0xDC00);
            else cp = c;
            w->high_surrogate = 0;
            if (cp >= 0x20 && cp != 0x7F && w->on_char) w->on_char(w, cp);
            if (msg == WM_SYSCHAR) break;
            return 0;
        }
        case WM_DROPFILES: {
            HDROP d = (HDROP)wp;
            int n = DragQueryFileW(d, 0xFFFFFFFF, NULL, 0);
            const char** paths = calloc(n, sizeof(char*));
            for (int i = 0; i < n; i++) {
                WCHAR wb[MAX_PATH];
                DragQueryFileW(d, i, wb, MAX_PATH);
                int len = WideCharToMultiByte(CP_UTF8, 0, wb, -1, NULL, 0, NULL, NULL);
                char* s = malloc(len);
                WideCharToMultiByte(CP_UTF8, 0, wb, -1, s, len, NULL, NULL);
                paths[i] = s;
            }
            DragFinish(d);
            if (w->on_drop && n) w->on_drop(w, n, paths);
            for (int i = 0; i < n; i++) free((void*)paths[i]);
            free(paths);
            return 0;
        }
        case WM_SIZING: {
            if (w->aspect_num <= 0 || w->aspect_den <= 0) break;
            RECT* r = (RECT*)lp;
            double ratio = (double)w->aspect_num / w->aspect_den;
            int cw = r->right - r->left, ch = r->bottom - r->top;
            if (wp == WMSZ_LEFT || wp == WMSZ_RIGHT) r->bottom = r->top + (int)(cw / ratio);
            else r->right = r->left + (int)(ch * ratio);
            return TRUE;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

bool platform_init(void) {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f);
    g_freq = (double)f.QuadPart;
    g_t0 = 0; g_t0 = platform_time();
    SetProcessDPIAware();
    WNDCLASSW wc = {0};
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"platform_window";
    RegisterClassW(&wc);
    return true;
}

void platform_terminate(void) {}

int platform_run(platform_loop_fn loop, void* ctx) {
    while (loop(ctx)) {}
    return 0;
}

void platform_poll(void) {
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

static WCHAR* to_wide(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, NULL, 0);
    WCHAR* r = malloc(n * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, r, n);
    return r;
}

platform_window* platform_window_create(int width, int height, const char* title, bool visible) {
    platform_window* w = calloc(1, sizeof(platform_window));
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, style, FALSE);
    WCHAR* wt = to_wide(title);
    w->hwnd = CreateWindowW(L"platform_window", wt, style, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
    free(wt);
    SetWindowLongPtrW(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
    DragAcceptFiles(w->hwnd, TRUE);
    if (visible) platform_window_show(w);
    return w;
}

void platform_window_destroy(platform_window* w) {
    if (!w) return;
    DestroyWindow(w->hwnd);
    free(w->clip);
    free(w);
}

void platform_window_show(platform_window* w) { ShowWindow(w->hwnd, SW_SHOW); SetForegroundWindow(w->hwnd); }

void platform_window_set_title(platform_window* w, const char* t) {
    WCHAR* wt = to_wide(t); SetWindowTextW(w->hwnd, wt); free(wt);
}

void platform_window_set_size(platform_window* w, int width, int height) {
    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, GetWindowLongW(w->hwnd, GWL_STYLE), FALSE);
    SetWindowPos(w->hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void platform_window_get_size(platform_window* w, int* width, int* height) {
    RECT r; GetClientRect(w->hwnd, &r);
    if (width) *width = r.right; if (height) *height = r.bottom;
}

void platform_window_get_framebuffer(platform_window* w, int* width, int* height) {
    platform_window_get_size(w, width, height);
}

float platform_window_scale(platform_window* w) { return 1.0f; }

void platform_window_get_pos(platform_window* w, int* x, int* y) {
    POINT p = { 0, 0 }; ClientToScreen(w->hwnd, &p);
    if (x) *x = p.x; if (y) *y = p.y;
}

void platform_window_set_pos(platform_window* w, int x, int y) {
    RECT r = { x, y, x, y };
    AdjustWindowRect(&r, GetWindowLongW(w->hwnd, GWL_STYLE), FALSE);
    SetWindowPos(w->hwnd, NULL, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void platform_window_set_aspect(platform_window* w, int num, int den) { w->aspect_num = num; w->aspect_den = den; }

void platform_window_set_fullscreen(platform_window* w, bool on) {
    if (on == w->fullscreen) return;
    if (on) {
        w->saved_style = GetWindowLongW(w->hwnd, GWL_STYLE);
        GetWindowRect(w->hwnd, &w->saved_rect);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLongW(w->hwnd, GWL_STYLE, w->saved_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(w->hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(w->hwnd, GWL_STYLE, w->saved_style);
        SetWindowPos(w->hwnd, NULL, w->saved_rect.left, w->saved_rect.top,
            w->saved_rect.right - w->saved_rect.left, w->saved_rect.bottom - w->saved_rect.top,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    w->fullscreen = on;
}

void platform_window_safe_area(platform_window* w, int* t, int* l, int* b, int* r) {
    if (t) *t = 0; if (l) *l = 0; if (b) *b = 0; if (r) *r = 0;
}

void platform_window_native(platform_window* w, platform_native* out) {
    out->kind = PLATFORM_NATIVE_WIN32;
    out->a = GetModuleHandleW(NULL);
    out->b = w->hwnd;
    out->window = 0;
}

void platform_set_cursor(platform_window* w, int kind) {
    switch (kind) {
        case PLATFORM_CURSOR_HRESIZE: w->cursor = LoadCursorW(NULL, IDC_SIZEWE); break;
        case PLATFORM_CURSOR_VRESIZE: w->cursor = LoadCursorW(NULL, IDC_SIZENS); break;
        case PLATFORM_CURSOR_IBEAM:   w->cursor = LoadCursorW(NULL, IDC_IBEAM);  break;
        case PLATFORM_CURSOR_HAND:    w->cursor = LoadCursorW(NULL, IDC_HAND);   break;
        default:                      w->cursor = NULL; break;
    }
    SetCursor(w->cursor ? w->cursor : LoadCursorW(NULL, IDC_ARROW));
}

void platform_set_clipboard(platform_window* w, const char* text) {
    if (!OpenClipboard(w->hwnd)) return;
    EmptyClipboard();
    int n = MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, NULL, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(WCHAR));
    WCHAR* p = GlobalLock(h);
    MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, p, n);
    GlobalUnlock(h);
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
}

const char* platform_get_clipboard(platform_window* w) {
    free(w->clip); w->clip = NULL;
    if (!OpenClipboard(w->hwnd)) return NULL;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        WCHAR* p = GlobalLock(h);
        int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
        w->clip = malloc(n);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, w->clip, n, NULL, NULL);
        GlobalUnlock(h);
    }
    CloseClipboard();
    return w->clip;
}

void platform_show_keyboard(platform_window* w, bool show) {}

// xinput: 15 buttons in glfw gamepad order, 6 axes, 1 hat
static uint8_t g_pad_buttons[4][15];
static float   g_pad_axes[4][6];
static uint8_t g_pad_hats[4][1];

static bool pad_read(int j) {
    XINPUT_STATE s;
    if (j < 0 || j >= 4 || XInputGetState(j, &s) != ERROR_SUCCESS) return false;
    WORD b = s.Gamepad.wButtons;
    uint8_t* o = g_pad_buttons[j];
    o[0] = !!(b & XINPUT_GAMEPAD_A); o[1] = !!(b & XINPUT_GAMEPAD_B);
    o[2] = !!(b & XINPUT_GAMEPAD_X); o[3] = !!(b & XINPUT_GAMEPAD_Y);
    o[4] = !!(b & XINPUT_GAMEPAD_LEFT_SHOULDER); o[5] = !!(b & XINPUT_GAMEPAD_RIGHT_SHOULDER);
    o[6] = !!(b & XINPUT_GAMEPAD_BACK); o[7] = !!(b & XINPUT_GAMEPAD_START); o[8] = 0;
    o[9] = !!(b & XINPUT_GAMEPAD_LEFT_THUMB); o[10] = !!(b & XINPUT_GAMEPAD_RIGHT_THUMB);
    o[11] = !!(b & XINPUT_GAMEPAD_DPAD_UP); o[12] = !!(b & XINPUT_GAMEPAD_DPAD_RIGHT);
    o[13] = !!(b & XINPUT_GAMEPAD_DPAD_DOWN); o[14] = !!(b & XINPUT_GAMEPAD_DPAD_LEFT);
    float* a = g_pad_axes[j];
    a[0] = s.Gamepad.sThumbLX / 32767.f; a[1] = -s.Gamepad.sThumbLY / 32767.f;
    a[2] = s.Gamepad.sThumbRX / 32767.f; a[3] = -s.Gamepad.sThumbRY / 32767.f;
    a[4] = s.Gamepad.bLeftTrigger / 127.5f - 1; a[5] = s.Gamepad.bRightTrigger / 127.5f - 1;
    g_pad_hats[j][0] = (o[11] ? 1 : 0) | (o[12] ? 2 : 0) | (o[13] ? 4 : 0) | (o[14] ? 8 : 0);
    return true;
}

bool platform_joystick_present(int j) { XINPUT_STATE s; return j >= 0 && j < 4 && XInputGetState(j, &s) == ERROR_SUCCESS; }
const uint8_t* platform_joystick_buttons(int j, int* n) { if (!pad_read(j)) { *n = 0; return NULL; } *n = 15; return g_pad_buttons[j]; }
const float*   platform_joystick_axes   (int j, int* n) { if (!pad_read(j)) { *n = 0; return NULL; } *n = 6;  return g_pad_axes[j]; }
const uint8_t* platform_joystick_hats   (int j, int* n) { if (!pad_read(j)) { *n = 0; return NULL; } *n = 1;  return g_pad_hats[j]; }

#else
// ================================================================ linux / xcb
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <errno.h>

struct platform_window {
    platform_key_fn    on_key;
    platform_char_fn   on_char;
    platform_mouse_fn  on_mouse;
    platform_cursor_fn on_cursor;
    platform_enter_fn  on_enter;
    platform_scroll_fn on_scroll;
    platform_drop_fn   on_drop;
    platform_size_fn   on_size;
    platform_touch_fn  on_touch;
    platform_focus_fn  on_focus;
    void*              user;
    char               keys[PLATFORM_KEY_LAST + 1];
    int                mods;
    bool               should_close;
    bool               fullscreen;
    int                aspect_num, aspect_den;
    char*              clip;
    char*              clip_own;
    xcb_window_t       win;
    int                width, height;
    int                x, y;
    xcb_cursor_t       cursor;
    xcb_atom_t         wm_delete;
    // xdnd: the source offering a drop, and whether it offers file uris
    xcb_window_t       xdnd_src;
    bool               xdnd_uris;
    platform_window*   next;
};

static xcb_connection_t* g_conn;
static xcb_screen_t*     g_screen;
// the server's keycode -> keysym table, fetched once: no xcb-keysyms needed
static xcb_keysym_t*     g_syms;
static int               g_syms_min, g_syms_per, g_syms_count;
static platform_window*  g_windows;

static xcb_keysym_t keysym_of(xcb_keycode_t code, int col) {
    int i = (int)code - g_syms_min;
    if (!g_syms || i < 0 || i >= g_syms_count || col >= g_syms_per) return 0;
    return g_syms[i * g_syms_per + col];
}

// at connect and again on MappingNotify (a layout switch)
static void fetch_keymap(void) {
    const xcb_setup_t* su = xcb_get_setup(g_conn);
    g_syms_min   = su->min_keycode;
    g_syms_count = su->max_keycode - su->min_keycode + 1;
    xcb_get_keyboard_mapping_reply_t* km = xcb_get_keyboard_mapping_reply(g_conn,
        xcb_get_keyboard_mapping(g_conn, su->min_keycode, (uint8_t)g_syms_count), NULL);
    if (!km) return;
    free(g_syms);
    g_syms_per = km->keysyms_per_keycode;
    int n = xcb_get_keyboard_mapping_keysyms_length(km);
    g_syms = malloc(n * sizeof(xcb_keysym_t));
    memcpy(g_syms, xcb_get_keyboard_mapping_keysyms(km), n * sizeof(xcb_keysym_t));
    free(km);
}
static double            g_t0;
static xcb_atom_t        A_XDND_AWARE, A_XDND_ENTER, A_XDND_POSITION, A_XDND_STATUS, A_XDND_DROP,
                         A_XDND_FINISHED, A_XDND_SELECTION, A_XDND_ACTION_COPY, A_URI_LIST;
// an event read ahead of its turn (autorepeat detection) is handled next
static xcb_generic_event_t* g_pending;

static xcb_generic_event_t* next_event(void) {
    xcb_generic_event_t* e = g_pending;
    g_pending = NULL;
    return e ? e : xcb_poll_for_event(g_conn);
}
static xcb_atom_t A_WM_PROTOCOLS, A_WM_DELETE, A_NET_WM_STATE, A_NET_WM_STATE_FULLSCREEN,
                  A_CLIPBOARD, A_UTF8_STRING, A_TARGETS, A_PLATFORM_SEL;

const char* platform_surface_extension(void) { return "VK_KHR_xcb_surface"; }

static double now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
double platform_time(void) { return now() - g_t0; }

static xcb_atom_t atom(const char* name) {
    xcb_intern_atom_cookie_t c = xcb_intern_atom(g_conn, 0, strlen(name), name);
    xcb_intern_atom_reply_t* r = xcb_intern_atom_reply(g_conn, c, NULL);
    xcb_atom_t a = r ? r->atom : XCB_NONE;
    free(r);
    return a;
}

static void handle(xcb_generic_event_t* ev);
static platform_window* find(xcb_window_t id) {
    for (platform_window* w = g_windows; w; w = w->next) if (w->win == id) return w;
    return NULL;
}

static int keysym_to_key(xcb_keysym_t s) {
    if (s >= XK_a && s <= XK_z) return PLATFORM_KEY_A + (s - XK_a);
    if (s >= XK_A && s <= XK_Z) return PLATFORM_KEY_A + (s - XK_A);
    if (s >= XK_0 && s <= XK_9) return PLATFORM_KEY_0 + (s - XK_0);
    if (s >= XK_F1 && s <= XK_F12) return PLATFORM_KEY_F1 + (s - XK_F1);
    if (s >= XK_KP_0 && s <= XK_KP_9) return PLATFORM_KEY_KP_0 + (s - XK_KP_0);
    switch (s) {
        case XK_space:        return PLATFORM_KEY_SPACE;
        case XK_apostrophe:   return PLATFORM_KEY_APOSTROPHE;
        case XK_comma:        return PLATFORM_KEY_COMMA;
        case XK_minus:        return PLATFORM_KEY_MINUS;
        case XK_period:       return PLATFORM_KEY_PERIOD;
        case XK_slash:        return PLATFORM_KEY_SLASH;
        case XK_semicolon:    return PLATFORM_KEY_SEMICOLON;
        case XK_equal:        return PLATFORM_KEY_EQUAL;
        case XK_bracketleft:  return PLATFORM_KEY_LEFT_BRACKET;
        case XK_backslash:    return PLATFORM_KEY_BACKSLASH;
        case XK_bracketright: return PLATFORM_KEY_RIGHT_BRACKET;
        case XK_grave:        return PLATFORM_KEY_GRAVE_ACCENT;
        case XK_Escape:       return PLATFORM_KEY_ESCAPE;
        case XK_Return:       return PLATFORM_KEY_ENTER;
        case XK_Tab: case XK_ISO_Left_Tab: return PLATFORM_KEY_TAB;
        case XK_BackSpace:    return PLATFORM_KEY_BACKSPACE;
        case XK_Insert:       return PLATFORM_KEY_INSERT;
        case XK_Delete:       return PLATFORM_KEY_DELETE;
        case XK_Right:        return PLATFORM_KEY_RIGHT;
        case XK_Left:         return PLATFORM_KEY_LEFT;
        case XK_Down:         return PLATFORM_KEY_DOWN;
        case XK_Up:           return PLATFORM_KEY_UP;
        case XK_Page_Up:      return PLATFORM_KEY_PAGE_UP;
        case XK_Page_Down:    return PLATFORM_KEY_PAGE_DOWN;
        case XK_Home:         return PLATFORM_KEY_HOME;
        case XK_End:          return PLATFORM_KEY_END;
        case XK_Caps_Lock:    return PLATFORM_KEY_CAPS_LOCK;
        case XK_Scroll_Lock:  return PLATFORM_KEY_SCROLL_LOCK;
        case XK_Num_Lock:     return PLATFORM_KEY_NUM_LOCK;
        case XK_Print:        return PLATFORM_KEY_PRINT_SCREEN;
        case XK_Pause:        return PLATFORM_KEY_PAUSE;
        case XK_KP_Decimal:   return PLATFORM_KEY_KP_DECIMAL;
        case XK_KP_Divide:    return PLATFORM_KEY_KP_DIVIDE;
        case XK_KP_Multiply:  return PLATFORM_KEY_KP_MULTIPLY;
        case XK_KP_Subtract:  return PLATFORM_KEY_KP_SUBTRACT;
        case XK_KP_Add:       return PLATFORM_KEY_KP_ADD;
        case XK_KP_Enter:     return PLATFORM_KEY_KP_ENTER;
        case XK_KP_Equal:     return PLATFORM_KEY_KP_EQUAL;
        case XK_Shift_L:      return PLATFORM_KEY_LEFT_SHIFT;
        case XK_Control_L:    return PLATFORM_KEY_LEFT_CONTROL;
        case XK_Alt_L:        return PLATFORM_KEY_LEFT_ALT;
        case XK_Super_L:      return PLATFORM_KEY_LEFT_SUPER;
        case XK_Shift_R:      return PLATFORM_KEY_RIGHT_SHIFT;
        case XK_Control_R:    return PLATFORM_KEY_RIGHT_CONTROL;
        case XK_Alt_R:        return PLATFORM_KEY_RIGHT_ALT;
        case XK_Super_R:      return PLATFORM_KEY_RIGHT_SUPER;
        case XK_Menu:         return PLATFORM_KEY_MENU;
    }
    return 0;
}

static int mods_of(uint16_t state) {
    int m = 0;
    if (state & XCB_MOD_MASK_SHIFT)   m |= PLATFORM_MOD_SHIFT;
    if (state & XCB_MOD_MASK_CONTROL) m |= PLATFORM_MOD_CONTROL;
    if (state & XCB_MOD_MASK_1)       m |= PLATFORM_MOD_ALT;
    if (state & XCB_MOD_MASK_4)       m |= PLATFORM_MOD_SUPER;
    return m;
}

// keysym → unicode: latin-1 direct, X's 0x1000000 offset, else none
static uint32_t keysym_to_unicode(xcb_keysym_t s) {
    if (s >= 0x20 && s <= 0x7e) return s;
    if (s >= 0xa0 && s <= 0xff) return s;
    if ((s & 0xff000000) == 0x01000000) return s & 0x00ffffff;
    if (s >= XK_KP_0 && s <= XK_KP_9) return '0' + (s - XK_KP_0);
    switch (s) {
        case XK_KP_Space:    return ' ';
        case XK_KP_Add:      return '+';
        case XK_KP_Subtract: return '-';
        case XK_KP_Multiply: return '*';
        case XK_KP_Divide:   return '/';
        case XK_KP_Decimal:  return '.';
        case XK_KP_Equal:    return '=';
    }
    return 0;
}

static void key_event(platform_window* w, int key, int scan, int action, int mods) {
    if (key < 0 || key > PLATFORM_KEY_LAST) key = 0;
    if (key) w->keys[key] = action != PLATFORM_RELEASE;
    w->mods = mods;
    if (w->on_key) w->on_key(w, key, scan, action, mods);
}

static void handle_selection_request(xcb_selection_request_event_t* e) {
    platform_window* w = find(e->owner);
    xcb_selection_notify_event_t n = {0};
    n.response_type = XCB_SELECTION_NOTIFY;
    n.requestor = e->requestor; n.selection = e->selection;
    n.target = e->target; n.property = XCB_NONE; n.time = e->time;
    if (w && w->clip_own) {
        if (e->target == A_TARGETS) {
            xcb_atom_t t[2] = { A_TARGETS, A_UTF8_STRING };
            xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_ATOM, 32, 2, t);
            n.property = e->property;
        } else if (e->target == A_UTF8_STRING || e->target == XCB_ATOM_STRING) {
            xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, e->requestor, e->property, e->target, 8,
                strlen(w->clip_own), w->clip_own);
            n.property = e->property;
        }
    }
    xcb_send_event(g_conn, 0, e->requestor, XCB_EVENT_MASK_NO_EVENT, (const char*)&n);
    xcb_flush(g_conn);
}

// a drop: fetch the source's uri list through our selection property, turn
// file:// uris into paths, hand them to the app, and tell the source we are done
static void xdnd_receive(platform_window* w, xcb_timestamp_t t) {
    xcb_convert_selection(g_conn, w->win, A_XDND_SELECTION, A_URI_LIST, A_PLATFORM_SEL, t);
    xcb_flush(g_conn);
    char* data = NULL;
    double t0 = now();
    while (now() - t0 < 0.5 && !data) {
        xcb_generic_event_t* ev = next_event();
        if (!ev) { usleep(1000); continue; }
        if ((ev->response_type & ~0x80) == XCB_SELECTION_NOTIFY) {
            xcb_selection_notify_event_t* n = (void*)ev;
            if (n->property != XCB_NONE) {
                xcb_get_property_reply_t* r = xcb_get_property_reply(g_conn,
                    xcb_get_property(g_conn, 1, w->win, A_PLATFORM_SEL, XCB_ATOM_ANY, 0, 1 << 20), NULL);
                if (r) {
                    int len = xcb_get_property_value_length(r);
                    data = malloc(len + 1);
                    memcpy(data, xcb_get_property_value(r), len);
                    data[len] = 0;
                    free(r);
                }
            }
            free(ev);
            break;
        }
        handle(ev);
        free(ev);
    }
    int count = 0;
    const char* paths[256];
    for (char* line = data; line && *line && count < 256; ) {
        char* end = strpbrk(line, "\r\n");
        if (end) *end = 0;
        if (line[0] != '#' && strncmp(line, "file://", 7) == 0) {
            char* p = line + 7;
            if (strncmp(p, "localhost", 9) == 0) p += 9;
            // %XX escapes decode in place
            char* o = p;
            for (char* s = p; *s; s++, o++) {
                unsigned v;
                if (*s == '%' && sscanf(s + 1, "%2x", &v) == 1) { *o = (char)v; s += 2; }
                else *o = *s;
            }
            *o = 0;
            paths[count++] = p;
        }
        line = end ? end + 1 : NULL;
    }
    if (count && w->on_drop) w->on_drop(w, count, paths);
    free(data);
    xcb_client_message_event_t fin = { 0 };
    fin.response_type = XCB_CLIENT_MESSAGE;
    fin.format = 32;
    fin.window = w->xdnd_src;
    fin.type = A_XDND_FINISHED;
    fin.data.data32[0] = w->win;
    fin.data.data32[1] = count ? 1 : 0;
    fin.data.data32[2] = count ? A_XDND_ACTION_COPY : XCB_NONE;
    xcb_send_event(g_conn, 0, w->xdnd_src, XCB_EVENT_MASK_NO_EVENT, (const char*)&fin);
    xcb_flush(g_conn);
    w->xdnd_src = XCB_NONE;
}

static void handle(xcb_generic_event_t* ev) {
    switch (ev->response_type & ~0x80) {
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t* e = (void*)ev;
            platform_window* w = find(e->window);
            if (!w) break;
            if (e->type == A_WM_PROTOCOLS && e->data.data32[0] == A_WM_DELETE) w->should_close = true;
            // xdnd: enter lists the offered types (three inline, more in a
            // property); position asks whether we take it; drop hands it over
            else if (e->type == A_XDND_ENTER) {
                w->xdnd_src  = e->data.data32[0];
                w->xdnd_uris = false;
                if (e->data.data32[1] & 1) {
                    xcb_get_property_reply_t* r = xcb_get_property_reply(g_conn,
                        xcb_get_property(g_conn, 0, w->xdnd_src, atom("XdndTypeList"), XCB_ATOM_ATOM, 0, 1024), NULL);
                    if (r) {
                        xcb_atom_t* ts = xcb_get_property_value(r);
                        int n = xcb_get_property_value_length(r) / 4;
                        for (int i = 0; i < n; i++) if (ts[i] == A_URI_LIST) w->xdnd_uris = true;
                        free(r);
                    }
                } else
                    for (int i = 2; i < 5; i++) if (e->data.data32[i] == A_URI_LIST) w->xdnd_uris = true;
            } else if (e->type == A_XDND_POSITION && w->xdnd_src) {
                xcb_client_message_event_t st = { 0 };
                st.response_type = XCB_CLIENT_MESSAGE;
                st.format = 32;
                st.window = w->xdnd_src;
                st.type = A_XDND_STATUS;
                st.data.data32[0] = w->win;
                st.data.data32[1] = w->xdnd_uris ? 1 : 0;
                st.data.data32[4] = w->xdnd_uris ? A_XDND_ACTION_COPY : XCB_NONE;
                xcb_send_event(g_conn, 0, w->xdnd_src, XCB_EVENT_MASK_NO_EVENT, (const char*)&st);
                xcb_flush(g_conn);
            } else if (e->type == A_XDND_DROP && w->xdnd_src) {
                xdnd_receive(w, e->data.data32[2]);
            }
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t* e = (void*)ev;
            platform_window* w = find(e->window);
            if (!w) break;
            w->x = e->x; w->y = e->y;
            if (e->width != w->width || e->height != w->height) {
                w->width = e->width; w->height = e->height;
                if (w->on_size) w->on_size(w, w->width, w->height);
            }
            break;
        }
        case XCB_FOCUS_IN: {
            platform_window* w = find(((xcb_focus_in_event_t*)ev)->event);
            if (w && w->on_focus) w->on_focus(w, 1);
            break;
        }
        case XCB_FOCUS_OUT: {
            platform_window* w = find(((xcb_focus_out_event_t*)ev)->event);
            if (!w) break;
            memset(w->keys, 0, sizeof(w->keys)); w->mods = 0;
            if (w->on_focus) w->on_focus(w, 0);
            break;
        }
        case XCB_ENTER_NOTIFY: {
            platform_window* w = find(((xcb_enter_notify_event_t*)ev)->event);
            if (w && w->on_enter) w->on_enter(w, 1);
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            platform_window* w = find(((xcb_leave_notify_event_t*)ev)->event);
            if (w && w->on_enter) w->on_enter(w, 0);
            break;
        }
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t* e = (void*)ev;
            platform_window* w = find(e->event);
            if (!w) break;
            w->mods = mods_of(e->state);
            if (w->on_cursor) w->on_cursor(w, e->event_x, e->event_y);
            break;
        }
        case XCB_BUTTON_PRESS: case XCB_BUTTON_RELEASE: {
            xcb_button_press_event_t* e = (void*)ev;
            platform_window* w = find(e->event);
            if (!w) break;
            bool press = (ev->response_type & ~0x80) == XCB_BUTTON_PRESS;
            w->mods = mods_of(e->state);
            if (e->detail >= 4 && e->detail <= 7) {
                if (!press) break;
                double dx = e->detail == 6 ? 1 : e->detail == 7 ? -1 : 0;
                double dy = e->detail == 4 ? 1 : e->detail == 5 ? -1 : 0;
                if (w->on_scroll) w->on_scroll(w, dx, dy);
                break;
            }
            int b = e->detail == 1 ? PLATFORM_MOUSE_LEFT : e->detail == 2 ? PLATFORM_MOUSE_MIDDLE :
                    e->detail == 3 ? PLATFORM_MOUSE_RIGHT : e->detail - 5;
            if (w->on_mouse) w->on_mouse(w, b, press ? PLATFORM_PRESS : PLATFORM_RELEASE, w->mods);
            break;
        }
        case XCB_KEY_PRESS: case XCB_KEY_RELEASE: {
            xcb_key_press_event_t* e = (void*)ev;
            platform_window* w = find(e->event);
            if (!w) break;
            bool press = (ev->response_type & ~0x80) == XCB_KEY_PRESS;
            xcb_keysym_t base = keysym_of(e->detail, 0);
            int key = keysym_to_key(base);
            int mods = mods_of(e->state);
            // x autorepeat is a release + press pair at one timestamp: the
            // release is synthetic, so the pair collapses to one repeat
            bool repeat = false;
            if (!press) {
                xcb_generic_event_t* nx = xcb_poll_for_queued_event(g_conn);
                if (nx) {
                    xcb_key_press_event_t* ne = (void*)nx;
                    if ((nx->response_type & ~0x80) == XCB_KEY_PRESS &&
                        ne->detail == e->detail && ne->time == e->time) {
                        free(nx);
                        press = true; repeat = true;
                    } else
                        g_pending = nx;
                }
            }
            int action = !press ? PLATFORM_RELEASE : (repeat || w->keys[key]) ? PLATFORM_REPEAT : PLATFORM_PRESS;
            key_event(w, key, e->detail, action, mods);
            if (press && !(mods & (PLATFORM_MOD_CONTROL | PLATFORM_MOD_SUPER))) {
                int col = (e->state & XCB_MOD_MASK_SHIFT) ? 1 : 0;
                uint32_t cp = keysym_to_unicode(keysym_of(e->detail, col));
                if (cp && w->on_char) w->on_char(w, cp);
            }
            break;
        }
        case XCB_SELECTION_REQUEST: handle_selection_request((void*)ev); break;
        case XCB_SELECTION_CLEAR: {
            platform_window* w = find(((xcb_selection_clear_event_t*)ev)->owner);
            if (w) { free(w->clip_own); w->clip_own = NULL; }
            break;
        }
        case XCB_MAPPING_NOTIFY:
            fetch_keymap();
            break;
    }
}

bool platform_init(void) {
    g_t0 = now();
    int screen_n;
    g_conn = xcb_connect(NULL, &screen_n);
    if (!g_conn || xcb_connection_has_error(g_conn)) return false;
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(g_conn));
    for (int i = 0; i < screen_n; i++) xcb_screen_next(&it);
    g_screen = it.data;
    fetch_keymap();
    A_WM_PROTOCOLS = atom("WM_PROTOCOLS");
    A_WM_DELETE = atom("WM_DELETE_WINDOW");
    A_NET_WM_STATE = atom("_NET_WM_STATE");
    A_NET_WM_STATE_FULLSCREEN = atom("_NET_WM_STATE_FULLSCREEN");
    A_CLIPBOARD = atom("CLIPBOARD");
    A_UTF8_STRING = atom("UTF8_STRING");
    A_TARGETS = atom("TARGETS");
    A_PLATFORM_SEL = atom("PLATFORM_SELECTION");
    A_XDND_AWARE = atom("XdndAware");
    A_XDND_ENTER = atom("XdndEnter");
    A_XDND_POSITION = atom("XdndPosition");
    A_XDND_STATUS = atom("XdndStatus");
    A_XDND_DROP = atom("XdndDrop");
    A_XDND_FINISHED = atom("XdndFinished");
    A_XDND_SELECTION = atom("XdndSelection");
    A_XDND_ACTION_COPY = atom("XdndActionCopy");
    A_URI_LIST = atom("text/uri-list");
    return true;
}

void platform_terminate(void) {
    free(g_syms);
    if (g_conn) xcb_disconnect(g_conn);
    g_syms = NULL; g_conn = NULL;
}

int platform_run(platform_loop_fn loop, void* ctx) {
    while (loop(ctx)) {}
    return 0;
}

void platform_poll(void) {
    xcb_generic_event_t* ev;
    while ((ev = next_event())) { handle(ev); free(ev); }
    xcb_flush(g_conn);
}

platform_window* platform_window_create(int width, int height, const char* title, bool visible) {
    platform_window* w = calloc(1, sizeof(platform_window));
    w->width = width; w->height = height;
    w->win = xcb_generate_id(g_conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = { g_screen->black_pixel,
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE };
    xcb_create_window(g_conn, XCB_COPY_FROM_PARENT, w->win, g_screen->root, 0, 0, width, height, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, g_screen->root_visual, mask, values);
    xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, w->win, A_WM_PROTOCOLS, XCB_ATOM_ATOM, 32, 1, &A_WM_DELETE);
    // accept file drops (xdnd protocol 5)
    uint32_t xdnd_ver = 5;
    xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, w->win, A_XDND_AWARE, XCB_ATOM_ATOM, 32, 1, &xdnd_ver);
    platform_window_set_title(w, title);
    w->next = g_windows; g_windows = w;
    if (visible) platform_window_show(w);
    xcb_flush(g_conn);
    return w;
}

void platform_window_destroy(platform_window* w) {
    if (!w) return;
    for (platform_window** p = &g_windows; *p; p = &(*p)->next) if (*p == w) { *p = w->next; break; }
    if (w->cursor) xcb_free_cursor(g_conn, w->cursor);
    xcb_destroy_window(g_conn, w->win);
    xcb_flush(g_conn);
    free(w->clip); free(w->clip_own);
    free(w);
}

void platform_window_show(platform_window* w) { xcb_map_window(g_conn, w->win); xcb_flush(g_conn); }

void platform_window_set_title(platform_window* w, const char* t) {
    if (!t) t = "";
    xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, w->win, XCB_ATOM_WM_NAME, A_UTF8_STRING, 8, strlen(t), t);
    xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, w->win, atom("_NET_WM_NAME"), A_UTF8_STRING, 8, strlen(t), t);
    xcb_flush(g_conn);
}

void platform_window_set_size(platform_window* w, int width, int height) {
    uint32_t v[2] = { width, height };
    xcb_configure_window(g_conn, w->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, v);
    xcb_flush(g_conn);
}

void platform_window_get_size(platform_window* w, int* width, int* height) {
    if (width) *width = w->width; if (height) *height = w->height;
}
void platform_window_get_framebuffer(platform_window* w, int* width, int* height) {
    platform_window_get_size(w, width, height);
}
float platform_window_scale(platform_window* w) { return 1.0f; }

void platform_window_get_pos(platform_window* w, int* x, int* y) {
    xcb_translate_coordinates_reply_t* r = xcb_translate_coordinates_reply(g_conn,
        xcb_translate_coordinates(g_conn, w->win, g_screen->root, 0, 0), NULL);
    if (r) { w->x = r->dst_x; w->y = r->dst_y; free(r); }
    if (x) *x = w->x; if (y) *y = w->y;
}

void platform_window_set_pos(platform_window* w, int x, int y) {
    uint32_t v[2] = { x, y };
    xcb_configure_window(g_conn, w->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, v);
    xcb_flush(g_conn);
}

void platform_window_set_aspect(platform_window* w, int num, int den) {
    w->aspect_num = num; w->aspect_den = den;
    // WM_NORMAL_HINTS: flags(PAspect=0x80) .. min_aspect, max_aspect at [11..14]
    uint32_t hints[18] = {0};
    hints[0] = 0x80;
    hints[11] = num; hints[12] = den; hints[13] = num; hints[14] = den;
    xcb_change_property(g_conn, XCB_PROP_MODE_REPLACE, w->win, XCB_ATOM_WM_NORMAL_HINTS,
        XCB_ATOM_WM_SIZE_HINTS, 32, 18, hints);
    xcb_flush(g_conn);
}

void platform_window_set_fullscreen(platform_window* w, bool on) {
    xcb_client_message_event_t e = {0};
    e.response_type = XCB_CLIENT_MESSAGE;
    e.format = 32;
    e.window = w->win;
    e.type = A_NET_WM_STATE;
    e.data.data32[0] = on ? 1 : 0;
    e.data.data32[1] = A_NET_WM_STATE_FULLSCREEN;
    xcb_send_event(g_conn, 0, g_screen->root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char*)&e);
    xcb_flush(g_conn);
    w->fullscreen = on;
}

void platform_window_safe_area(platform_window* w, int* t, int* l, int* b, int* r) {
    if (t) *t = 0; if (l) *l = 0; if (b) *b = 0; if (r) *r = 0;
}

void platform_window_native(platform_window* w, platform_native* out) {
    out->kind = PLATFORM_NATIVE_XCB;
    out->a = g_conn;
    out->b = NULL;
    out->window = w->win;
}

void platform_set_cursor(platform_window* w, int kind) {
    // core cursor font glyphs: 108 sb_h_double_arrow, 116 sb_v_double_arrow, 152 xterm, 60 hand2
    int glyph = kind == PLATFORM_CURSOR_HRESIZE ? 108 : kind == PLATFORM_CURSOR_VRESIZE ? 116 :
                kind == PLATFORM_CURSOR_IBEAM ? 152 : kind == PLATFORM_CURSOR_HAND ? 60 : -1;
    if (w->cursor) { xcb_free_cursor(g_conn, w->cursor); w->cursor = 0; }
    if (glyph >= 0) {
        xcb_font_t font = xcb_generate_id(g_conn);
        xcb_open_font(g_conn, font, 6, "cursor");
        w->cursor = xcb_generate_id(g_conn);
        xcb_create_glyph_cursor(g_conn, w->cursor, font, font, glyph, glyph + 1, 0, 0, 0, 0xffff, 0xffff, 0xffff);
        xcb_close_font(g_conn, font);
    }
    uint32_t v = w->cursor;
    xcb_change_window_attributes(g_conn, w->win, XCB_CW_CURSOR, &v);
    xcb_flush(g_conn);
}

void platform_set_clipboard(platform_window* w, const char* text) {
    free(w->clip_own);
    w->clip_own = strdup(text ? text : "");
    xcb_set_selection_owner(g_conn, w->win, A_CLIPBOARD, XCB_CURRENT_TIME);
    xcb_flush(g_conn);
}

const char* platform_get_clipboard(platform_window* w) {
    free(w->clip); w->clip = NULL;
    if (w->clip_own) { w->clip = strdup(w->clip_own); return w->clip; }
    xcb_convert_selection(g_conn, w->win, A_CLIPBOARD, A_UTF8_STRING, A_PLATFORM_SEL, XCB_CURRENT_TIME);
    xcb_flush(g_conn);
    double t0 = now();
    while (now() - t0 < 0.5) {
        xcb_generic_event_t* ev = next_event();
        if (!ev) { usleep(1000); continue; }
        if ((ev->response_type & ~0x80) == XCB_SELECTION_NOTIFY) {
            xcb_selection_notify_event_t* n = (void*)ev;
            if (n->property != XCB_NONE) {
                xcb_get_property_reply_t* r = xcb_get_property_reply(g_conn,
                    xcb_get_property(g_conn, 1, w->win, A_PLATFORM_SEL, XCB_ATOM_ANY, 0, 1 << 20), NULL);
                if (r) {
                    int len = xcb_get_property_value_length(r);
                    w->clip = malloc(len + 1);
                    memcpy(w->clip, xcb_get_property_value(r), len);
                    w->clip[len] = 0;
                    free(r);
                }
            }
            free(ev);
            break;
        }
        handle(ev);
        free(ev);
    }
    return w->clip;
}

void platform_show_keyboard(platform_window* w, bool show) {}

// /dev/input/jsN: raw driver order for buttons and axes, hat from axes 6/7
static int     g_js_fd[16];
static bool    g_js_open[16];
static uint8_t g_js_buttons[16][32];
static float   g_js_axes[16][16];
static uint8_t g_js_hats[16][1];
static int     g_js_nb[16], g_js_na[16];

static bool js_open(int j) {
    if (j < 0 || j >= 16) return false;
    if (g_js_open[j]) return true;
    char p[32]; snprintf(p, sizeof p, "/dev/input/js%d", j);
    int fd = open(p, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return false;
    uint8_t n = 0;
    ioctl(fd, JSIOCGBUTTONS, &n); g_js_nb[j] = n > 32 ? 32 : n;
    ioctl(fd, JSIOCGAXES, &n);    g_js_na[j] = n > 16 ? 16 : n;
    g_js_fd[j] = fd; g_js_open[j] = true;
    return true;
}

static void js_pump(int j) {
    struct js_event e;
    while (read(g_js_fd[j], &e, sizeof e) == sizeof e) {
        int t = e.type & ~JS_EVENT_INIT;
        if (t == JS_EVENT_BUTTON && e.number < 32) g_js_buttons[j][e.number] = e.value != 0;
        if (t == JS_EVENT_AXIS && e.number < 16) g_js_axes[j][e.number] = e.value / 32767.f;
    }
    if (errno != EAGAIN) { close(g_js_fd[j]); g_js_open[j] = false; }
    float hx = g_js_na[j] > 6 ? g_js_axes[j][6] : 0, hy = g_js_na[j] > 7 ? g_js_axes[j][7] : 0;
    g_js_hats[j][0] = (hy < -0.5 ? 1 : 0) | (hx > 0.5 ? 2 : 0) | (hy > 0.5 ? 4 : 0) | (hx < -0.5 ? 8 : 0);
}

bool platform_joystick_present(int j) { return js_open(j); }
const uint8_t* platform_joystick_buttons(int j, int* n) { if (!js_open(j)) { *n = 0; return NULL; } js_pump(j); *n = g_js_nb[j]; return g_js_buttons[j]; }
const float*   platform_joystick_axes   (int j, int* n) { if (!js_open(j)) { *n = 0; return NULL; } js_pump(j); *n = g_js_na[j]; return g_js_axes[j]; }
const uint8_t* platform_joystick_hats   (int j, int* n) { if (!js_open(j)) { *n = 0; return NULL; } js_pump(j); *n = 1; return g_js_hats[j]; }
#endif

#if !defined(__APPLE__)
void  platform_window_set_user (platform_window* w, void* u) { w->user = u; }
void* platform_window_get_user (platform_window* w)          { return w->user; }
int   platform_get_key  (platform_window* w, int key) {
    return (key >= 0 && key <= PLATFORM_KEY_LAST && w->keys[key]) ? PLATFORM_PRESS : PLATFORM_RELEASE;
}
int   platform_get_mods (platform_window* w) { return w->mods; }
bool  platform_window_should_close(platform_window* w) { return w->should_close; }
bool  platform_window_fullscreen  (platform_window* w) { return w->fullscreen; }
void platform_set_key_callback    (platform_window* w, platform_key_fn f)    { w->on_key = f; }
void platform_set_char_callback   (platform_window* w, platform_char_fn f)   { w->on_char = f; }
void platform_set_mouse_callback  (platform_window* w, platform_mouse_fn f)  { w->on_mouse = f; }
void platform_set_cursor_callback (platform_window* w, platform_cursor_fn f) { w->on_cursor = f; }
void platform_set_enter_callback  (platform_window* w, platform_enter_fn f)  { w->on_enter = f; }
void platform_set_scroll_callback (platform_window* w, platform_scroll_fn f) { w->on_scroll = f; }
void platform_set_drop_callback   (platform_window* w, platform_drop_fn f)   { w->on_drop = f; }
void platform_set_size_callback   (platform_window* w, platform_size_fn f)   { w->on_size = f; }
void platform_set_touch_callback  (platform_window* w, platform_touch_fn f)  { w->on_touch = f; }
void platform_set_focus_callback  (platform_window* w, platform_focus_fn f)  { w->on_focus = f; }
#endif
