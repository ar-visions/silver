// SPDX-License-Identifier: BSD-2-Clause

// WebKit for silver's browser element: rgba8 frames out through
// shared memory (TRINITY_BROWSER_SHM), input in as lines on stdin.

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wpe/headless/wpe-headless.h>
#include <wpe/webkit.h>

#define FRAME_MAGIC 0x57425254u
// sites challenge WPE's own agent; pose as Safari on a Mac
#define SAFARI_AGENT "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) " \
    "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.5 Safari/605.1.15"
#define MAC_PLATFORM_SCRIPT "Object.defineProperty(Navigator.prototype, " \
    "'platform', { get: () => 'MacIntel' });"
#define MAX_FRAME_SIDE 4096

// the element reads w, h, stride after seq changes
typedef struct {
    uint32_t magic;
    _Atomic uint32_t seq;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t reserved[3];
} FrameHeader;

static uint8_t* frames;
static WebKitWebView* webView;
static WPEView* view;
static GMainLoop* loop;
static GString* pending;
static EGLDisplay eglDisplay;
static GLuint readTexture;
static GLuint readFramebuffer;

// a surfaceless GL context on the display's own EGL display
static gboolean initReadBack(WPEDisplay* display)
{
    GError* error = NULL;
    eglDisplay = wpe_display_get_egl_display(display, &error);
    if (!eglDisplay) {
        g_warning("no EGL display: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(eglDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
        g_warning("no GL context for frame read back");
        return FALSE;
    }
    glGenTextures(1, &readTexture);
    glGenFramebuffers(1, &readFramebuffer);
    return TRUE;
}

// a GPU frame read straight into the element's rgba8 rows
static gboolean readDMABuf(WPEBuffer* buffer, int width, int height, uint8_t* to)
{
    GError* error = NULL;
    // owned by the buffer: no destroy
    EGLImage image = wpe_buffer_import_to_egl_image(buffer, &error);
    if (!image) {
        g_warning("frame import failed: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    glBindTexture(GL_TEXTURE_2D, readTexture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindFramebuffer(GL_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, readTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return FALSE;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, to);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return TRUE;
}

static void publishFrame(WPEView* source, WPEBuffer* buffer, gpointer data)
{
    int width = wpe_buffer_get_width(buffer);
    int height = wpe_buffer_get_height(buffer);
    if (width <= 0 || height <= 0 || width > MAX_FRAME_SIDE || height > MAX_FRAME_SIDE)
        return;
    FrameHeader* header = (FrameHeader*)frames;
    size_t stride = (size_t)width * 4;
    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        if (!eglDisplay || !readDMABuf(buffer, width, height, frames + sizeof(FrameHeader)))
            return;
        header->width = width;
        header->height = height;
        header->stride = stride;
        header->magic = FRAME_MAGIC;
        atomic_fetch_add_explicit(&header->seq, 1, memory_order_release);
        return;
    }
    GError* error = NULL;
    // owned by the buffer: no unref
    GBytes* pixels = wpe_buffer_import_to_pixels(buffer, &error);
    if (!pixels) {
        g_warning("frame read back failed: %s", error->message);
        g_error_free(error);
        return;
    }
    gsize size = 0;
    const uint8_t* from = g_bytes_get_data(pixels, &size);
    if (size < stride * height)
        return;
    uint8_t* to = frames + sizeof(FrameHeader);
    // trinity textures are rgba8; WebKit's buffers are bgra8
    for (size_t i = 0; i < stride * height; i += 4) {
        to[i + 0] = from[i + 2];
        to[i + 1] = from[i + 1];
        to[i + 2] = from[i + 0];
        to[i + 3] = from[i + 3];
    }
    header->width = width;
    header->height = height;
    header->stride = stride;
    header->magic = FRAME_MAGIC;
    atomic_fetch_add_explicit(&header->seq, 1, memory_order_release);
}

// the element's mods: shift 1, ctrl 2, alt 4, meta 8
static WPEModifiers modifiersFrom(unsigned mods)
{
    unsigned result = 0;
    if (mods & 1)
        result |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (mods & 2)
        result |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (mods & 4)
        result |= WPE_MODIFIER_KEYBOARD_ALT;
    if (mods & 8)
        result |= WPE_MODIFIER_KEYBOARD_META;
    return (WPEModifiers)result;
}

// GLFW buttons: 0 left, 1 right, 2 middle
static guint buttonFrom(int button)
{
    return button == 1 ? WPE_BUTTON_SECONDARY : button == 2 ? WPE_BUTTON_MIDDLE : WPE_BUTTON_PRIMARY;
}

static unsigned heldModifiers(unsigned held)
{
    unsigned result = 0;
    if (held & 1)
        result |= WPE_MODIFIER_POINTER_BUTTON1;
    if (held & 2)
        result |= WPE_MODIFIER_POINTER_BUTTON3;
    if (held & 4)
        result |= WPE_MODIFIER_POINTER_BUTTON2;
    return result;
}

// GLFW key codes, as trinity reports them, to WPE key values
static guint keyvalFromGLFW(int code)
{
    if (code >= 'A' && code <= 'Z')
        return code - 'A' + 'a';
    if ((code >= '0' && code <= '9') || code == 32)
        return code;
    if (code >= 290 && code <= 301)
        return WPE_KEY_F1 + (code - 290);
    switch (code) {
    case 256: return WPE_KEY_Escape;
    case 257: return WPE_KEY_Return;
    case 335: return WPE_KEY_KP_Enter;
    case 258: return WPE_KEY_Tab;
    case 259: return WPE_KEY_BackSpace;
    case 260: return WPE_KEY_Insert;
    case 261: return WPE_KEY_Delete;
    case 262: return WPE_KEY_Right;
    case 263: return WPE_KEY_Left;
    case 264: return WPE_KEY_Down;
    case 265: return WPE_KEY_Up;
    case 266: return WPE_KEY_Page_Up;
    case 267: return WPE_KEY_Page_Down;
    case 268: return WPE_KEY_Home;
    case 269: return WPE_KEY_End;
    }
    return 0;
}

static void sendEvent(WPEEvent* event)
{
    if (!event)
        return;
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

static void sendKey(guint keyval, WPEModifiers modifiers, gboolean down)
{
    sendEvent(wpe_event_keyboard_new(down ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP,
        view, WPE_INPUT_SOURCE_KEYBOARD, g_get_monotonic_time() / 1000, modifiers, 0, keyval));
}

static void handleLine(char* line)
{
    char command[16] = { 0 };
    if (sscanf(line, "%15s", command) != 1)
        return;
    guint32 time = g_get_monotonic_time() / 1000;

    if (!strcmp(command, "load") && strlen(line) > 5) {
        webkit_web_view_load_uri(webView, line + 5);
        return;
    }
    if (!strcmp(command, "size")) {
        int width = 0, height = 0;
        if (sscanf(line, "size %d %d", &width, &height) == 2 && width > 0 && height > 0)
            wpe_view_resized(view, width, height);
        return;
    }
    // mouse <down|up|move|wheel> x y button buttons mods [dx dy]
    if (!strcmp(command, "mouse")) {
        char kind[8] = { 0 };
        double x = 0, y = 0, dx = 0, dy = 0;
        int button = 0;
        unsigned held = 0, mods = 0;
        if (sscanf(line, "mouse %7s %lf %lf %d %u %u %lf %lf", kind, &x, &y, &button, &held, &mods, &dx, &dy) < 6)
            return;
        WPEModifiers modifiers = (WPEModifiers)(modifiersFrom(mods) | heldModifiers(held));
        if (!strcmp(kind, "down") || !strcmp(kind, "up")) {
            gboolean down = !strcmp(kind, "down");
            sendEvent(wpe_event_pointer_button_new(down ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP,
                view, WPE_INPUT_SOURCE_MOUSE, time, modifiers, buttonFrom(button), x, y, down ? 1 : 0));
        } else if (!strcmp(kind, "wheel")) {
            // a GLFW step is one notch; up is plus
            sendEvent(wpe_event_scroll_new(view, WPE_INPUT_SOURCE_MOUSE, time, modifiers, dx, dy, FALSE, FALSE, x, y));
        } else
            sendEvent(wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, time, modifiers, x, y, 0, 0));
        return;
    }
    // key <down|up> glfw_code mods
    if (!strcmp(command, "key")) {
        char kind[8] = { 0 };
        int code = 0;
        unsigned mods = 0;
        if (sscanf(line, "key %7s %d %u", kind, &code, &mods) != 3)
            return;
        guint keyval = keyvalFromGLFW(code);
        if (keyval)
            sendKey(keyval, modifiersFrom(mods), !strcmp(kind, "down"));
        return;
    }
    // text <utf8>: typed characters, the rest of the line
    if (!strcmp(command, "text") && strlen(line) > 5) {
        for (const char* p = line + 5; *p; p = g_utf8_next_char(p)) {
            guint keyval = wpe_unicode_to_keyval(g_utf8_get_char(p));
            sendKey(keyval, (WPEModifiers)0, TRUE);
            sendKey(keyval, (WPEModifiers)0, FALSE);
        }
        return;
    }
}

static gboolean readInput(GIOChannel* channel, GIOCondition condition, gpointer data)
{
    char chunk[4096];
    ssize_t count = read(STDIN_FILENO, chunk, sizeof(chunk));
    if (count <= 0) {
        // the element is gone: so is its browser
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
    }
    g_string_append_len(pending, chunk, count);
    char* newline;
    while ((newline = memchr(pending->str, '\n', pending->len))) {
        *newline = 0;
        handleLine(pending->str);
        g_string_erase(pending, 0, newline - pending->str + 1);
    }
    return G_SOURCE_CONTINUE;
}

static uint8_t* mapFrames(void)
{
    const char* path = g_getenv("TRINITY_BROWSER_SHM");
    if (!path) {
        fprintf(stderr, "TRINITY_BROWSER_SHM is not set\n");
        return NULL;
    }
    size_t size = sizeof(FrameHeader) + (size_t)MAX_FRAME_SIDE * MAX_FRAME_SIDE * 4;
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0 || ftruncate(fd, (off_t)size) < 0)
        return NULL;
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return memory == MAP_FAILED ? NULL : memory;
}

int main(int argc, char** argv)
{
    frames = mapFrames();
    if (!frames)
        return 1;
    int width = 1280, height = 800;
    const char* url = "about:blank";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--size") && i + 2 < argc) {
            width = atoi(argv[++i]);
            height = atoi(argv[++i]);
        } else
            url = argv[i];
    }
    loop = g_main_loop_new(NULL, FALSE);
    pending = g_string_new(NULL);
    WPEDisplay* display = wpe_display_headless_new();
    webView = g_object_new(WEBKIT_TYPE_WEB_VIEW, "display", display, NULL);
    webkit_settings_set_user_agent(webkit_web_view_get_settings(webView), SAFARI_AGENT);
    // the agent says Mac: navigator.platform must agree
    WebKitUserScript* platform = webkit_user_script_new(MAC_PLATFORM_SCRIPT,
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
    webkit_user_content_manager_add_script(webkit_web_view_get_user_content_manager(webView), platform);
    webkit_user_script_unref(platform);
    view = webkit_web_view_get_wpe_view(webView);
    initReadBack(display);
    g_signal_connect(view, "buffer-rendered", G_CALLBACK(publishFrame), NULL);
    wpe_view_resized(view, width, height);
    wpe_view_focus_in(view);
    webkit_web_view_load_uri(webView, url);

    GIOChannel* input = g_io_channel_unix_new(STDIN_FILENO);
    g_io_add_watch(input, G_IO_IN | G_IO_HUP | G_IO_ERR, readInput, NULL);
    g_main_loop_run(loop);
    return 0;
}
