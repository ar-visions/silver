// apple host: Cocoa on macOS, UIKit on iOS. pure ObjC++ — no Au headers here
#include "host.h"
#include <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <GameController/GameController.h>
#include <string.h>
#include <stdlib.h>

#if TARGET_OS_IPHONE
#import "UIKit/UIKit.h"   // quoted: the .mm scanner would link it on macOS
#else
#import <Cocoa/Cocoa.h>
#endif

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
#if TARGET_OS_IPHONE
    UIWindow*          window;
    UIView*            view;
    UIViewController*  vc;
    UITouch*           touches[16];
#else
    NSWindow*          window;
    NSView*            view;
    id                 delegate;
    NSCursor*          cursor;
#endif
};

static double g_t0 = 0;

double platform_time(void) { return CACurrentMediaTime() - g_t0; }

const char* platform_surface_extension(void) { return "VK_EXT_metal_surface"; }

static void key_event(platform_window* w, int key, int scan, int action, int mods) {
    if (key < 0 || key > PLATFORM_KEY_LAST) key = 0;
    if (key) w->keys[key] = action != PLATFORM_RELEASE;
    w->mods = mods;
    if (w->on_key) w->on_key(w, key, scan, action, mods);
}

// ---------------------------------------------------------------- joystick
#define PAD_BUTTONS 15
#define PAD_AXES    6
static uint8_t g_pad_buttons[16][PAD_BUTTONS];
static float   g_pad_axes[16][PAD_AXES];
static uint8_t g_pad_hats[16][1];

static GCController* pad_at(int j) {
    NSArray<GCController*>* cs = [GCController controllers];
    if (j < 0 || j >= (int)cs.count) return nil;
    return cs[j];
}

bool platform_joystick_present(int j) {
    GCController* c = pad_at(j);
    return c && c.extendedGamepad;
}

static void pad_read(int j) {
    GCController* c = pad_at(j);
    GCExtendedGamepad* g = c.extendedGamepad;
    if (!g) return;
    uint8_t* b = g_pad_buttons[j];
    b[0]  = g.buttonA.pressed;
    b[1]  = g.buttonB.pressed;
    b[2]  = g.buttonX.pressed;
    b[3]  = g.buttonY.pressed;
    b[4]  = g.leftShoulder.pressed;
    b[5]  = g.rightShoulder.pressed;
    b[6]  = g.buttonOptions ? g.buttonOptions.pressed : 0;
    b[7]  = g.buttonMenu.pressed;
    b[8]  = g.buttonHome ? g.buttonHome.pressed : 0;
    b[9]  = g.leftThumbstickButton ? g.leftThumbstickButton.pressed : 0;
    b[10] = g.rightThumbstickButton ? g.rightThumbstickButton.pressed : 0;
    b[11] = g.dpad.up.pressed;
    b[12] = g.dpad.right.pressed;
    b[13] = g.dpad.down.pressed;
    b[14] = g.dpad.left.pressed;
    float* a = g_pad_axes[j];
    a[0] = g.leftThumbstick.xAxis.value;
    a[1] = -g.leftThumbstick.yAxis.value;
    a[2] = g.rightThumbstick.xAxis.value;
    a[3] = -g.rightThumbstick.yAxis.value;
    a[4] = g.leftTrigger.value * 2 - 1;
    a[5] = g.rightTrigger.value * 2 - 1;
    g_pad_hats[j][0] = (b[11] ? 1 : 0) | (b[12] ? 2 : 0) | (b[13] ? 4 : 0) | (b[14] ? 8 : 0);
}

const uint8_t* platform_joystick_buttons(int j, int* count) {
    if (!platform_joystick_present(j)) { *count = 0; return NULL; }
    pad_read(j); *count = PAD_BUTTONS; return g_pad_buttons[j];
}
const float* platform_joystick_axes(int j, int* count) {
    if (!platform_joystick_present(j)) { *count = 0; return NULL; }
    pad_read(j); *count = PAD_AXES; return g_pad_axes[j];
}
const uint8_t* platform_joystick_hats(int j, int* count) {
    if (!platform_joystick_present(j)) { *count = 0; return NULL; }
    pad_read(j); *count = 1; return g_pad_hats[j];
}

// ---------------------------------------------------------------- shared
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

static void emit_codepoints(platform_window* w, NSString* s) {
    if (!w->on_char || !s) return;
    NSUInteger n = s.length;
    for (NSUInteger i = 0; i < n; i++) {
        uint32_t cp = [s characterAtIndex:i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n) {
            uint32_t lo = [s characterAtIndex:++i];
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        }
        if (cp < 0x20 || cp == 0x7F || (cp >= 0xF700 && cp <= 0xF8FF)) continue;
        w->on_char(w, cp);
    }
}

#if !TARGET_OS_IPHONE
// ================================================================ macOS

// macOS virtual keycode → GLFW key number
static const short mac_keys[128] = {
    /*0x00*/ 65, 83, 68, 70, 72, 71, 90, 88, 67, 86, 92, 66, 81, 87, 69, 82,
    /*0x10*/ 89, 84, 49, 50, 51, 52, 54, 53, 61, 57, 55, 45, 56, 48, 93, 79,
    /*0x20*/ 85, 91, 73, 80, 257, 76, 74, 39, 75, 59, 92, 44, 47, 78, 77, 46,
    /*0x30*/ 258, 32, 96, 259, 0, 256, 347, 343, 340, 280, 342, 341, 344, 346, 345, 0,
    /*0x40*/ 0, 330, 0, 332, 0, 334, 0, 282, 0, 0, 0, 331, 335, 0, 333, 0,
    /*0x50*/ 0, 336, 320, 321, 322, 323, 324, 325, 326, 327, 0, 328, 329, 0, 0, 0,
    /*0x60*/ 294, 295, 296, 292, 297, 298, 0, 300, 0, 283, 0, 281, 0, 299, 0, 301,
    /*0x70*/ 0, 284, 260, 268, 266, 261, 293, 269, 291, 267, 290, 263, 262, 264, 265, 0
};

static int mods_of(NSEventModifierFlags f) {
    int m = 0;
    if (f & NSEventModifierFlagShift)   m |= PLATFORM_MOD_SHIFT;
    if (f & NSEventModifierFlagControl) m |= PLATFORM_MOD_CONTROL;
    if (f & NSEventModifierFlagOption)  m |= PLATFORM_MOD_ALT;
    if (f & NSEventModifierFlagCommand) m |= PLATFORM_MOD_SUPER;
    return m;
}

@interface PlatformView : NSView <NSDraggingDestination> {
@public
    platform_window* w;
    NSTrackingArea*  tracking;
}
@end

@implementation PlatformView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)wantsUpdateLayer { return YES; }
- (CALayer*)makeBackingLayer {
    CAMetalLayer* l = [CAMetalLayer layer];
    l.contentsScale = self.window ? self.window.backingScaleFactor : 1.0;
    return l;
}
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    self.layer.contentsScale = self.window.backingScaleFactor;
    [self updateDrawable];
}
- (void)updateDrawable {
    CAMetalLayer* l = (CAMetalLayer*)self.layer;
    CGFloat s = l.contentsScale;
    l.drawableSize = CGSizeMake(self.bounds.size.width * s, self.bounds.size.height * s);
}
- (void)updateTrackingAreas {
    if (tracking) { [self removeTrackingArea:tracking]; tracking = nil; }
    NSTrackingAreaOptions o = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
        NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    tracking = [[NSTrackingArea alloc] initWithRect:self.bounds options:o owner:self userInfo:nil];
    [self addTrackingArea:tracking];
    [super updateTrackingAreas];
}
- (void)cursorUpdate:(NSEvent*)e { if (w->cursor) [w->cursor set]; else [[NSCursor arrowCursor] set]; }
- (void)mouseEntered:(NSEvent*)e { if (w->on_enter) w->on_enter(w, 1); }
- (void)mouseExited:(NSEvent*)e  { if (w->on_enter) w->on_enter(w, 0); }
- (void)cursorMoved:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (w->on_cursor) w->on_cursor(w, p.x, p.y);
}
- (void)mouseMoved:(NSEvent*)e        { [self cursorMoved:e]; }
- (void)mouseDragged:(NSEvent*)e      { [self cursorMoved:e]; }
- (void)rightMouseDragged:(NSEvent*)e { [self cursorMoved:e]; }
- (void)otherMouseDragged:(NSEvent*)e { [self cursorMoved:e]; }
- (void)button:(int)b action:(int)a event:(NSEvent*)e {
    w->mods = mods_of(e.modifierFlags);
    if (w->on_mouse) w->on_mouse(w, b, a, w->mods);
}
- (void)mouseDown:(NSEvent*)e      { [self button:PLATFORM_MOUSE_LEFT   action:PLATFORM_PRESS   event:e]; }
- (void)mouseUp:(NSEvent*)e        { [self button:PLATFORM_MOUSE_LEFT   action:PLATFORM_RELEASE event:e]; }
- (void)rightMouseDown:(NSEvent*)e { [self button:PLATFORM_MOUSE_RIGHT  action:PLATFORM_PRESS   event:e]; }
- (void)rightMouseUp:(NSEvent*)e   { [self button:PLATFORM_MOUSE_RIGHT  action:PLATFORM_RELEASE event:e]; }
- (void)otherMouseDown:(NSEvent*)e {
    int b = (int)e.buttonNumber; if (b == 2) b = PLATFORM_MOUSE_MIDDLE;
    [self button:b action:PLATFORM_PRESS event:e];
}
- (void)otherMouseUp:(NSEvent*)e {
    int b = (int)e.buttonNumber; if (b == 2) b = PLATFORM_MOUSE_MIDDLE;
    [self button:b action:PLATFORM_RELEASE event:e];
}
- (void)scrollWheel:(NSEvent*)e {
    double dx = e.scrollingDeltaX, dy = e.scrollingDeltaY;
    if (e.hasPreciseScrollingDeltas) { dx *= 0.1; dy *= 0.1; }
    w->mods = mods_of(e.modifierFlags);
    if (w->on_scroll && (dx != 0 || dy != 0)) w->on_scroll(w, dx, dy);
}
- (void)keyDown:(NSEvent*)e {
    int key = e.keyCode < 128 ? mac_keys[e.keyCode] : 0;
    key_event(w, key, e.keyCode, e.isARepeat ? PLATFORM_REPEAT : PLATFORM_PRESS, mods_of(e.modifierFlags));
    if (!(e.modifierFlags & NSEventModifierFlagCommand)) emit_codepoints(w, e.characters);
}
- (void)keyUp:(NSEvent*)e {
    int key = e.keyCode < 128 ? mac_keys[e.keyCode] : 0;
    key_event(w, key, e.keyCode, PLATFORM_RELEASE, mods_of(e.modifierFlags));
}
- (void)flagsChanged:(NSEvent*)e {
    int key = e.keyCode < 128 ? mac_keys[e.keyCode] : 0;
    if (!key) return;
    int action = w->keys[key] ? PLATFORM_RELEASE : PLATFORM_PRESS;
    key_event(w, key, e.keyCode, action, mods_of(e.modifierFlags));
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)s { return NSDragOperationGeneric; }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)s {
    NSArray* urls = [s.draggingPasteboard readObjectsForClasses:@[[NSURL class]]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
    if (!urls.count || !w->on_drop) return NO;
    const char** paths = (const char**)calloc(urls.count, sizeof(char*));
    for (NSUInteger i = 0; i < urls.count; i++) paths[i] = strdup([[urls[i] path] UTF8String]);
    w->on_drop(w, (int)urls.count, paths);
    for (NSUInteger i = 0; i < urls.count; i++) free((void*)paths[i]);
    free(paths);
    return YES;
}
@end

@interface PlatformWindowDelegate : NSObject <NSWindowDelegate> { @public platform_window* w; }
@end
@implementation PlatformWindowDelegate
- (BOOL)windowShouldClose:(id)s { w->should_close = true; return NO; }
- (void)windowDidResize:(NSNotification*)n {
    [(PlatformView*)w->view updateDrawable];
    int pw, ph; platform_window_get_framebuffer(w, &pw, &ph);
    if (w->on_size) w->on_size(w, pw, ph);
}
- (void)windowDidBecomeKey:(NSNotification*)n { if (w->on_focus) w->on_focus(w, 1); }
- (void)windowDidResignKey:(NSNotification*)n {
    memset(w->keys, 0, sizeof(w->keys)); w->mods = 0;
    if (w->on_focus) w->on_focus(w, 0);
}
- (void)windowDidEnterFullScreen:(NSNotification*)n { w->fullscreen = true; }
- (void)windowDidExitFullScreen:(NSNotification*)n  { w->fullscreen = false; }
@end

bool platform_init(void) {
    @autoreleasepool {
        g_t0 = CACurrentMediaTime();
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSMenu* bar = [[NSMenu alloc] init];
        NSMenuItem* appItem = [bar addItemWithTitle:@"" action:nil keyEquivalent:@""];
        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
        appItem.submenu = appMenu;
        [NSApp setMainMenu:bar];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
    }
    return true;
}

void platform_terminate(void) {}

int platform_run(platform_loop_fn loop, void* ctx) {
    while (loop(ctx)) {}
    return 0;
}

void platform_poll(void) {
    @autoreleasepool {
        NSEvent* e;
        while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
                     inMode:NSDefaultRunLoopMode dequeue:YES]))
            [NSApp sendEvent:e];
    }
}

platform_window* platform_window_create(int width, int height, const char* title, bool visible) {
    platform_window* w = (platform_window*)calloc(1, sizeof(platform_window));
    @autoreleasepool {
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        NSRect r = NSMakeRect(0, 0, width, height);
        w->window = [[NSWindow alloc] initWithContentRect:r styleMask:style
                     backing:NSBackingStoreBuffered defer:NO];
        [w->window center];
        w->window.title = [NSString stringWithUTF8String:title ? title : ""];
        w->window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
        w->window.releasedWhenClosed = NO;
        w->window.acceptsMouseMovedEvents = YES;
        PlatformView* v = [[PlatformView alloc] initWithFrame:r];
        v->w = w;
        v.wantsLayer = YES;
        [v registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
        w->view = v;
        w->window.contentView = v;
        [w->window makeFirstResponder:v];
        PlatformWindowDelegate* d = [[PlatformWindowDelegate alloc] init];
        d->w = w;
        w->delegate = d;
        w->window.delegate = d;
        v.layer.contentsScale = w->window.backingScaleFactor;
        [v updateDrawable];
        if (visible) platform_window_show(w);
    }
    return w;
}

void platform_window_destroy(platform_window* w) {
    if (!w) return;
    [w->window close];
    free(w->clip);
    free(w);
}

void platform_window_show(platform_window* w) {
    [w->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void platform_window_set_title(platform_window* w, const char* t) {
    w->window.title = [NSString stringWithUTF8String:t ? t : ""];
}

void platform_window_set_size(platform_window* w, int width, int height) {
    [w->window setContentSize:NSMakeSize(width, height)];
}

void platform_window_get_size(platform_window* w, int* width, int* height) {
    NSRect r = w->view.bounds;
    if (width)  *width  = (int)r.size.width;
    if (height) *height = (int)r.size.height;
}

void platform_window_get_framebuffer(platform_window* w, int* width, int* height) {
    NSRect r = [w->view convertRectToBacking:w->view.bounds];
    if (width)  *width  = (int)r.size.width;
    if (height) *height = (int)r.size.height;
}

float platform_window_scale(platform_window* w) { return (float)w->window.backingScaleFactor; }

void platform_window_get_pos(platform_window* w, int* x, int* y) {
    NSRect f = [w->window contentRectForFrameRect:w->window.frame];
    NSRect s = w->window.screen.frame;
    if (x) *x = (int)f.origin.x;
    if (y) *y = (int)(s.size.height - f.origin.y - f.size.height);
}

void platform_window_set_pos(platform_window* w, int x, int y) {
    NSRect s = w->window.screen.frame;
    NSRect f = [w->window contentRectForFrameRect:w->window.frame];
    [w->window setFrameTopLeftPoint:NSMakePoint(x, s.size.height - y)];
    (void)f;
}

void platform_window_set_aspect(platform_window* w, int num, int den) {
    w->aspect_num = num; w->aspect_den = den;
    if (num > 0 && den > 0) w->window.contentAspectRatio = NSMakeSize(num, den);
}

void platform_window_set_fullscreen(platform_window* w, bool on) {
    if (on != w->fullscreen) [w->window toggleFullScreen:nil];
}

void platform_window_safe_area(platform_window* w, int* t, int* l, int* b, int* r) {
    if (t) *t = 0; if (l) *l = 0; if (b) *b = 0; if (r) *r = 0;
}

void platform_window_native(platform_window* w, platform_native* out) {
    out->kind = PLATFORM_NATIVE_METAL;
    out->a = (__bridge void*)w->view.layer;
    out->b = (__bridge void*)w->view;
    out->window = 0;
}

void platform_set_cursor(platform_window* w, int kind) {
    switch (kind) {
        case PLATFORM_CURSOR_HRESIZE: w->cursor = [NSCursor resizeLeftRightCursor]; break;
        case PLATFORM_CURSOR_VRESIZE: w->cursor = [NSCursor resizeUpDownCursor];    break;
        case PLATFORM_CURSOR_IBEAM:   w->cursor = [NSCursor IBeamCursor];           break;
        case PLATFORM_CURSOR_HAND:    w->cursor = [NSCursor pointingHandCursor];    break;
        default:                      w->cursor = nil;                              break;
    }
    if (w->cursor) [w->cursor set]; else [[NSCursor arrowCursor] set];
}

void platform_set_clipboard(platform_window* w, const char* text) {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:text ? text : ""] forType:NSPasteboardTypeString];
}

const char* platform_get_clipboard(platform_window* w) {
    NSString* s = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
    free(w->clip);
    w->clip = s ? strdup(s.UTF8String) : NULL;
    return w->clip;
}

void platform_show_keyboard(platform_window* w, bool show) {}

#else
// ================================================================ iOS

static platform_loop_fn g_loop;
static void*            g_loop_ctx;
static platform_window* g_win;

@interface PlatformUIView : UIView <UIKeyInput> { @public platform_window* w; }
@end

@implementation PlatformUIView
+ (Class)layerClass { return [CAMetalLayer class]; }
- (BOOL)canBecomeFirstResponder { return YES; }
- (BOOL)hasText { return YES; }
- (void)insertText:(NSString*)text {
    if ([text isEqualToString:@"\n"]) {
        key_event(w, PLATFORM_KEY_ENTER, 0, PLATFORM_PRESS, 0);
        key_event(w, PLATFORM_KEY_ENTER, 0, PLATFORM_RELEASE, 0);
        return;
    }
    emit_codepoints(w, text);
}
- (void)deleteBackward {
    key_event(w, PLATFORM_KEY_BACKSPACE, 0, PLATFORM_PRESS, 0);
    key_event(w, PLATFORM_KEY_BACKSPACE, 0, PLATFORM_RELEASE, 0);
}
- (void)layoutSubviews {
    [super layoutSubviews];
    CAMetalLayer* l = (CAMetalLayer*)self.layer;
    CGFloat s = self.contentScaleFactor;
    l.drawableSize = CGSizeMake(self.bounds.size.width * s, self.bounds.size.height * s);
    if (w->on_size) w->on_size(w, (int)l.drawableSize.width, (int)l.drawableSize.height);
}
- (int)slotFor:(UITouch*)t {
    for (int i = 0; i < 16; i++) if (w->touches[i] == t) return i;
    for (int i = 0; i < 16; i++) if (!w->touches[i]) { w->touches[i] = t; return i; }
    return -1;
}
- (void)touches:(NSSet<UITouch*>*)ts phase:(int)phase {
    for (UITouch* t in ts) {
        int id = [self slotFor:t];
        if (id < 0) continue;
        CGPoint p = [t locationInView:self];
        if (w->on_touch) w->on_touch(w, id, phase, p.x, p.y);
        // first finger doubles as the mouse so pointer code runs unchanged
        if (id == 0) {
            if (w->on_cursor) w->on_cursor(w, p.x, p.y);
            if (phase == PLATFORM_TOUCH_BEGIN && w->on_mouse)
                w->on_mouse(w, PLATFORM_MOUSE_LEFT, PLATFORM_PRESS, 0);
            if ((phase == PLATFORM_TOUCH_END || phase == PLATFORM_TOUCH_CANCEL) && w->on_mouse)
                w->on_mouse(w, PLATFORM_MOUSE_LEFT, PLATFORM_RELEASE, 0);
        }
        if (phase == PLATFORM_TOUCH_END || phase == PLATFORM_TOUCH_CANCEL) w->touches[id] = nil;
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)ts withEvent:(UIEvent*)e     { [self touches:ts phase:PLATFORM_TOUCH_BEGIN]; }
- (void)touchesMoved:(NSSet<UITouch*>*)ts withEvent:(UIEvent*)e     { [self touches:ts phase:PLATFORM_TOUCH_MOVE]; }
- (void)touchesEnded:(NSSet<UITouch*>*)ts withEvent:(UIEvent*)e     { [self touches:ts phase:PLATFORM_TOUCH_END]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)ts withEvent:(UIEvent*)e { [self touches:ts phase:PLATFORM_TOUCH_CANCEL]; }
@end

@interface PlatformViewController : UIViewController
@end
@implementation PlatformViewController
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
@end

@interface PlatformAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong) CADisplayLink* link;
@end
@implementation PlatformAppDelegate
- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)o {
    self.link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    [self.link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    return YES;
}
- (void)tick:(CADisplayLink*)l {
    if (g_loop && !g_loop(g_loop_ctx)) { [l invalidate]; exit(0); }
}
- (void)applicationWillResignActive:(UIApplication*)a { if (g_win && g_win->on_focus) g_win->on_focus(g_win, 0); }
- (void)applicationDidBecomeActive:(UIApplication*)a  { if (g_win && g_win->on_focus) g_win->on_focus(g_win, 1); }
@end

bool platform_init(void) { g_t0 = CACurrentMediaTime(); return true; }
void platform_terminate(void) {}

// UIKit owns the loop: the app's first tick must create its window
int platform_run(platform_loop_fn loop, void* ctx) {
    g_loop = loop; g_loop_ctx = ctx;
    return UIApplicationMain(0, NULL, nil, @"PlatformAppDelegate");
}

void platform_poll(void) {}

platform_window* platform_window_create(int width, int height, const char* title, bool visible) {
    platform_window* w = (platform_window*)calloc(1, sizeof(platform_window));
    UIScreen* screen = [UIScreen mainScreen];
    w->window = [[UIWindow alloc] initWithFrame:screen.bounds];
    PlatformViewController* vc = [[PlatformViewController alloc] init];
    PlatformUIView* v = [[PlatformUIView alloc] initWithFrame:screen.bounds];
    v->w = w;
    v.contentScaleFactor = screen.nativeScale;
    v.multipleTouchEnabled = YES;
    vc.view = v;
    w->vc = vc;
    w->view = v;
    w->window.rootViewController = vc;
    w->fullscreen = true;
    g_win = w;
    if (visible) platform_window_show(w);
    return w;
}

void platform_window_destroy(platform_window* w) { if (w) { w->window.hidden = YES; free(w->clip); free(w); } }
void platform_window_show(platform_window* w) { [w->window makeKeyAndVisible]; }
void platform_window_set_title(platform_window* w, const char* t) {}
void platform_window_set_size(platform_window* w, int width, int height) {}
void platform_window_get_size(platform_window* w, int* width, int* height) {
    CGRect b = w->view.bounds;
    if (width)  *width  = (int)b.size.width;
    if (height) *height = (int)b.size.height;
}
void platform_window_get_framebuffer(platform_window* w, int* width, int* height) {
    CGSize d = ((CAMetalLayer*)w->view.layer).drawableSize;
    if (width)  *width  = (int)d.width;
    if (height) *height = (int)d.height;
}
float platform_window_scale(platform_window* w) { return (float)w->view.contentScaleFactor; }
void platform_window_get_pos(platform_window* w, int* x, int* y) { if (x) *x = 0; if (y) *y = 0; }
void platform_window_set_pos(platform_window* w, int x, int y) {}
void platform_window_set_aspect(platform_window* w, int num, int den) {}
void platform_window_set_fullscreen(platform_window* w, bool on) {}
void platform_window_safe_area(platform_window* w, int* t, int* l, int* b, int* r) {
    UIEdgeInsets i = w->view.safeAreaInsets;
    if (t) *t = (int)i.top; if (l) *l = (int)i.left; if (b) *b = (int)i.bottom; if (r) *r = (int)i.right;
}
void platform_window_native(platform_window* w, platform_native* out) {
    out->kind = PLATFORM_NATIVE_METAL;
    out->a = (__bridge void*)w->view.layer;
    out->b = (__bridge void*)w->view;
    out->window = 0;
}
void platform_set_cursor(platform_window* w, int kind) {}
void platform_set_clipboard(platform_window* w, const char* text) {
    [UIPasteboard generalPasteboard].string = [NSString stringWithUTF8String:text ? text : ""];
}
const char* platform_get_clipboard(platform_window* w) {
    NSString* s = [UIPasteboard generalPasteboard].string;
    free(w->clip);
    w->clip = s ? strdup(s.UTF8String) : NULL;
    return w->clip;
}
void platform_show_keyboard(platform_window* w, bool show) {
    if (show) [w->view becomeFirstResponder]; else [w->view resignFirstResponder];
}
#endif
