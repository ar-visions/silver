#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* one window + input + gpu-surface layer for desktop and mobile.
   key codes, mods and actions keep GLFW's numbering so app code
   that stored key values keeps working. no vulkan here: the app
   asks for the native handle and creates its own surface. */

typedef struct platform_window platform_window;

#define PLATFORM_RELEASE          0
#define PLATFORM_PRESS            1
#define PLATFORM_REPEAT           2

#define PLATFORM_MOD_SHIFT        0x0001
#define PLATFORM_MOD_CONTROL      0x0002
#define PLATFORM_MOD_ALT          0x0004
#define PLATFORM_MOD_SUPER        0x0008

#define PLATFORM_MOUSE_LEFT       0
#define PLATFORM_MOUSE_RIGHT      1
#define PLATFORM_MOUSE_MIDDLE     2

#define PLATFORM_CURSOR_ARROW     0
#define PLATFORM_CURSOR_HRESIZE   1
#define PLATFORM_CURSOR_VRESIZE   2
#define PLATFORM_CURSOR_IBEAM     3
#define PLATFORM_CURSOR_HAND      4

#define PLATFORM_TOUCH_BEGIN      0
#define PLATFORM_TOUCH_MOVE       1
#define PLATFORM_TOUCH_END        2
#define PLATFORM_TOUCH_CANCEL     3

/* native handle kinds */
#define PLATFORM_NATIVE_METAL     1   /* a: CAMetalLayer*             */
#define PLATFORM_NATIVE_WIN32     2   /* a: HINSTANCE, b: HWND        */
#define PLATFORM_NATIVE_XCB       3   /* a: xcb_connection_t*, b: u32 */
#define PLATFORM_NATIVE_ANDROID   4   /* a: ANativeWindow*            */

typedef struct platform_native {
    int   kind;
    void* a;
    void* b;
    uint32_t window;   /* xcb window id */
} platform_native;

/* GLFW key numbering */
#define PLATFORM_KEY_SPACE          32
#define PLATFORM_KEY_APOSTROPHE     39
#define PLATFORM_KEY_COMMA          44
#define PLATFORM_KEY_MINUS          45
#define PLATFORM_KEY_PERIOD         46
#define PLATFORM_KEY_SLASH          47
#define PLATFORM_KEY_0              48
#define PLATFORM_KEY_9              57
#define PLATFORM_KEY_SEMICOLON      59
#define PLATFORM_KEY_EQUAL          61
#define PLATFORM_KEY_A              65
#define PLATFORM_KEY_Z              90
#define PLATFORM_KEY_LEFT_BRACKET   91
#define PLATFORM_KEY_BACKSLASH      92
#define PLATFORM_KEY_RIGHT_BRACKET  93
#define PLATFORM_KEY_GRAVE_ACCENT   96
#define PLATFORM_KEY_ESCAPE         256
#define PLATFORM_KEY_ENTER          257
#define PLATFORM_KEY_TAB            258
#define PLATFORM_KEY_BACKSPACE      259
#define PLATFORM_KEY_INSERT         260
#define PLATFORM_KEY_DELETE         261
#define PLATFORM_KEY_RIGHT          262
#define PLATFORM_KEY_LEFT           263
#define PLATFORM_KEY_DOWN           264
#define PLATFORM_KEY_UP             265
#define PLATFORM_KEY_PAGE_UP        266
#define PLATFORM_KEY_PAGE_DOWN      267
#define PLATFORM_KEY_HOME           268
#define PLATFORM_KEY_END            269
#define PLATFORM_KEY_CAPS_LOCK      280
#define PLATFORM_KEY_SCROLL_LOCK    281
#define PLATFORM_KEY_NUM_LOCK       282
#define PLATFORM_KEY_PRINT_SCREEN   283
#define PLATFORM_KEY_PAUSE          284
#define PLATFORM_KEY_F1             290
#define PLATFORM_KEY_F12            301
#define PLATFORM_KEY_KP_0           320
#define PLATFORM_KEY_KP_9           329
#define PLATFORM_KEY_KP_DECIMAL     330
#define PLATFORM_KEY_KP_DIVIDE      331
#define PLATFORM_KEY_KP_MULTIPLY    332
#define PLATFORM_KEY_KP_SUBTRACT    333
#define PLATFORM_KEY_KP_ADD         334
#define PLATFORM_KEY_KP_ENTER       335
#define PLATFORM_KEY_KP_EQUAL       336
#define PLATFORM_KEY_LEFT_SHIFT     340
#define PLATFORM_KEY_LEFT_CONTROL   341
#define PLATFORM_KEY_LEFT_ALT       342
#define PLATFORM_KEY_LEFT_SUPER     343
#define PLATFORM_KEY_RIGHT_SHIFT    344
#define PLATFORM_KEY_RIGHT_CONTROL  345
#define PLATFORM_KEY_RIGHT_ALT      346
#define PLATFORM_KEY_RIGHT_SUPER    347
#define PLATFORM_KEY_MENU           348
#define PLATFORM_KEY_LAST           348

typedef void (*platform_key_fn)      (platform_window* w, int key, int scancode, int action, int mods);
typedef void (*platform_char_fn)     (platform_window* w, uint32_t codepoint);
typedef void (*platform_mouse_fn)    (platform_window* w, int button, int action, int mods);
typedef void (*platform_cursor_fn)   (platform_window* w, double x, double y);
typedef void (*platform_enter_fn)    (platform_window* w, int entered);
typedef void (*platform_scroll_fn)   (platform_window* w, double dx, double dy);
typedef void (*platform_drop_fn)     (platform_window* w, int count, const char** paths);
typedef void (*platform_size_fn)     (platform_window* w, int width, int height);
typedef void (*platform_touch_fn)    (platform_window* w, int id, int phase, double x, double y);
typedef void (*platform_focus_fn)    (platform_window* w, int focused);
typedef bool (*platform_loop_fn)     (void* ctx);

/* lifecycle */
bool  platform_init      (void);
void  platform_terminate (void);
/* desktop: calls loop until it returns false. ios: never returns */
int   platform_run       (platform_loop_fn loop, void* ctx);
void  platform_poll      (void);
double platform_time     (void);

/* window */
platform_window* platform_window_create (int width, int height, const char* title, bool visible);
void  platform_window_destroy           (platform_window* w);
void  platform_window_show              (platform_window* w);
bool  platform_window_should_close      (platform_window* w);
void  platform_window_set_title         (platform_window* w, const char* title);
void  platform_window_set_size          (platform_window* w, int width, int height);
void  platform_window_get_size          (platform_window* w, int* width, int* height);
void  platform_window_get_framebuffer   (platform_window* w, int* width, int* height);
void  platform_window_get_pos           (platform_window* w, int* x, int* y);
void  platform_window_set_pos           (platform_window* w, int x, int y);
float platform_window_scale             (platform_window* w);
void  platform_window_set_aspect        (platform_window* w, int num, int den);
void  platform_window_set_fullscreen    (platform_window* w, bool on);
bool  platform_window_fullscreen        (platform_window* w);
void  platform_window_safe_area         (platform_window* w, int* top, int* left, int* bottom, int* right);
void  platform_window_native            (platform_window* w, platform_native* out);
void  platform_window_set_user          (platform_window* w, void* user);
void* platform_window_get_user          (platform_window* w);

/* input */
int   platform_get_key          (platform_window* w, int key);
int   platform_get_mods         (platform_window* w);
void  platform_set_cursor       (platform_window* w, int kind);
void  platform_set_clipboard    (platform_window* w, const char* text);
const char* platform_get_clipboard (platform_window* w);
void  platform_show_keyboard    (platform_window* w, bool show);

/* joystick: polled. buttons/hats 0/1 per index, axes -1..1 */
bool  platform_joystick_present (int j);
const uint8_t* platform_joystick_buttons (int j, int* count);
const float*   platform_joystick_axes    (int j, int* count);
const uint8_t* platform_joystick_hats    (int j, int* count);
// device motion: accelerometer (g) and gyro (rad/s), false where there is none
bool  platform_motion(float* accel, float* gyro);

/* callbacks */
void  platform_set_key_callback    (platform_window* w, platform_key_fn fn);
void  platform_set_char_callback   (platform_window* w, platform_char_fn fn);
void  platform_set_mouse_callback  (platform_window* w, platform_mouse_fn fn);
void  platform_set_cursor_callback (platform_window* w, platform_cursor_fn fn);
void  platform_set_enter_callback  (platform_window* w, platform_enter_fn fn);
void  platform_set_scroll_callback (platform_window* w, platform_scroll_fn fn);
void  platform_set_drop_callback   (platform_window* w, platform_drop_fn fn);
void  platform_set_size_callback   (platform_window* w, platform_size_fn fn);
void  platform_set_touch_callback  (platform_window* w, platform_touch_fn fn);
void  platform_set_focus_callback  (platform_window* w, platform_focus_fn fn);

/* the vulkan surface extension this platform needs, after VK_KHR_surface */
const char* platform_surface_extension (void);

/* android: NativeActivity's onCreate hands the activity here with the
   app's main, which runs on its own thread and ends with the activity */
void  platform_android_start (void* activity, int (*main)(int, char**));

#ifdef __cplusplus
}
#endif
#endif
