// android.webkit.WebView on aura's browser: its helper process
// (WebKit, drawn by trinity) runs the page; frames arrive in a
// shared file, input goes down its socket as lines, its address
// and title come back as records
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "../defines.h"
#include "../util.h"

#include "WrapperWidget.h"

#include "../generated_headers/android_view_View.h"
#include "../generated_headers/android_webkit_WebView.h"

// aura's helper, by our own install: <prefix>/lib -> <prefix>/libexec
static char *helper_path(void)
{
	Dl_info info;
	if (!dladdr((void *)helper_path, &info) || !info.dli_fname)
		return g_strdup("WPETrinityBrowser");
	char *lib = g_path_get_dirname(info.dli_fname);
	char *prefix = g_path_get_dirname(lib);
	char *p = g_build_filename(prefix, "libexec", "wpe-webkit-2.0", "WPETrinityBrowser", NULL);
	g_free(lib);
	g_free(prefix);
	return p;
}

// the helper's frame file: this header, then rgba rows
typedef struct {
	uint32_t magic, seq, width, height, stride, reserved[3];
} FrameHeader;

// a record on the socket (aura.c reads the same)
typedef struct {
	uint32_t magic, seq, width, height, format, stride, offset, pad;
	uint64_t id, modifier;
} Record;

#define REC_ADDR 0x52444441u
#define REC_TITLE 0x4c544954u

typedef struct {
	GtkWidget *picture;
	WrapperWidget *wrapper;
	pid_t pid;
	int sock;
	char *shm_path;
	uint8_t *shm;
	size_t shm_size;
	uint32_t seq;
	int sent_w, sent_h;
	unsigned held;
	char *pending; // a load asked before the helper ran
} AuraView;

static void post(AuraView *v, const char *line)
{
	if (v->sock < 0)
		return;
	size_t n = strlen(line);
	send(v->sock, line, n, MSG_NOSIGNAL);
	send(v->sock, "\n", 1, MSG_NOSIGNAL);
}

static void start_helper(AuraView *v, const char *url)
{
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
		return;
	v->shm_path = g_strdup_printf("%s/atl-webview-%d-%p.frame",
		g_get_user_runtime_dir(), getpid(), (void *)v);
	char *data = g_build_filename(g_get_user_cache_dir(), "atl-webview", NULL);
	char *helper = helper_path();
	v->pid = fork();
	if (v->pid == 0) {
		dup2(fds[0], 0);
		close(fds[0]);
		close(fds[1]);
		setenv("TRINITY_BROWSER_SHM", v->shm_path, 1);
		execl(helper, "WPETrinityBrowser", "--data", data, url, NULL);
		_exit(1);
	}
	g_free(helper);
	g_free(data);
	close(fds[0]);
	if (v->pid < 0) {
		close(fds[1]);
		return;
	}
	v->sock = fds[1];
	fcntl(v->sock, F_SETFL, fcntl(v->sock, F_GETFL) | O_NONBLOCK);
	// frames by the shared file: GTK takes no dma-buf of ours
	post(v, "gpuframes 0");
}

static gboolean map_frames(AuraView *v)
{
	if (v->shm)
		return TRUE;
	int fd = open(v->shm_path, O_RDONLY);
	if (fd < 0)
		return FALSE;
	off_t size = lseek(fd, 0, SEEK_END);
	if (size <= (off_t)sizeof(FrameHeader)) {
		close(fd);
		return FALSE;
	}
	void *m = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (m == MAP_FAILED)
		return FALSE;
	v->shm = m;
	v->shm_size = size;
	unlink(v->shm_path);
	return TRUE;
}

static void read_records(AuraView *v)
{
	for (;;) {
		Record rec;
		struct iovec iov = { &rec, sizeof rec };
		union { struct cmsghdr h; char b[CMSG_SPACE(sizeof(int))]; } ctl;
		struct msghdr msg = { 0 };
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = &ctl;
		msg.msg_controllen = sizeof ctl;
		ssize_t n = recvmsg(v->sock, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
		if (n != (ssize_t)sizeof rec)
			return;
		// a GPU frame sent before our word: its buffer is not ours
		for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c))
			if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
				int fd;
				memcpy(&fd, CMSG_DATA(c), sizeof fd);
				close(fd);
			}
		if (rec.magic != REC_ADDR && rec.magic != REC_TITLE)
			continue;
		char text[4096];
		int len = (int)rec.width;
		if (len < 0 || len >= (int)sizeof text)
			return;
		if (len > 0 && recv(v->sock, text, len, MSG_WAITALL) != len)
			return;
		text[len] = 0;
		if (rec.magic == REC_ADDR && v->wrapper && v->wrapper->jobj) {
			JNIEnv *env = get_jni_env();
			// the page is there: started, then finished
			J__WebView__internalLoadChanged(env, v->wrapper->jobj, 0, _JSTRING(text));
			J__WebView__internalLoadChanged(env, v->wrapper->jobj, 3, _JSTRING(text));
		}
	}
}

static void show_frame(AuraView *v)
{
	if (!map_frames(v))
		return;
	FrameHeader *h = (FrameHeader *)v->shm;
	uint32_t seq = __atomic_load_n(&h->seq, __ATOMIC_ACQUIRE);
	if (seq == v->seq || !h->width || !h->height)
		return;
	size_t bytes = (size_t)h->stride * h->height;
	if (sizeof(FrameHeader) + bytes > v->shm_size)
		return;
	GBytes *b = g_bytes_new(v->shm + sizeof(FrameHeader), bytes);
	GdkTexture *t = gdk_memory_texture_new(h->width, h->height,
		GDK_MEMORY_R8G8B8A8, b, h->stride);
	g_bytes_unref(b);
	gtk_picture_set_paintable(GTK_PICTURE(v->picture), GDK_PAINTABLE(t));
	g_object_unref(t);
	v->seq = seq;
}

// each frame: the size, the records, a new picture
static gboolean tick(GtkWidget *w, GdkFrameClock *clock, gpointer data)
{
	AuraView *v = data;
	if (v->sock < 0)
		return G_SOURCE_CONTINUE;
	int sc = gtk_widget_get_scale_factor(w);
	int pw = gtk_widget_get_width(w) * sc;
	int ph = gtk_widget_get_height(w) * sc;
	if (pw > 0 && ph > 0 && (pw != v->sent_w || ph != v->sent_h)) {
		char line[64];
		snprintf(line, sizeof line, "size %d %d", pw, ph);
		post(v, line);
		v->sent_w = pw;
		v->sent_h = ph;
	}
	read_records(v);
	show_frame(v);
	return G_SOURCE_CONTINUE;
}

static unsigned mods_of(GdkModifierType m)
{
	return ((m & GDK_SHIFT_MASK) ? 1 : 0) | ((m & GDK_CONTROL_MASK) ? 2 : 0)
	     | ((m & GDK_ALT_MASK) ? 4 : 0) | ((m & GDK_SUPER_MASK) ? 8 : 0);
}

static void mouse(AuraView *v, const char *kind, double x, double y, int button, unsigned mods)
{
	int sc = gtk_widget_get_scale_factor(v->picture);
	char line[128];
	snprintf(line, sizeof line, "mouse %s %d %d %d %u %u", kind,
		(int)(x * sc), (int)(y * sc), button, v->held, mods);
	post(v, line);
}

// GTK's buttons 1 2 3 are the helper's (GLFW's) 0 2 1
static int button_of(GtkGesture *g)
{
	guint b = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(g));
	return b == 3 ? 1 : b == 2 ? 2 : 0;
}

static void on_pressed(GtkGestureClick *g, int n, double x, double y, AuraView *v)
{
	int b = button_of(GTK_GESTURE(g));
	v->held |= 1u << b;
	gtk_widget_grab_focus(v->picture);
	GdkModifierType m = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(g));
	mouse(v, "down", x, y, b, mods_of(m));
}

static void on_released(GtkGestureClick *g, int n, double x, double y, AuraView *v)
{
	int b = button_of(GTK_GESTURE(g));
	v->held &= ~(1u << b);
	GdkModifierType m = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(g));
	mouse(v, "up", x, y, b, mods_of(m));
}

static void on_motion(GtkEventControllerMotion *c, double x, double y, AuraView *v)
{
	GdkModifierType m = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(c));
	mouse(v, "move", x, y, 0, mods_of(m));
}

static gboolean on_scroll(GtkEventControllerScroll *c, double dx, double dy, AuraView *v)
{
	double x = 0, y = 0;
	GdkEvent *e = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(c));
	if (e)
		gdk_event_get_position(e, &x, &y);
	int sc = gtk_widget_get_scale_factor(v->picture);
	char line[160];
	// a notch down is minus to the helper
	snprintf(line, sizeof line, "mouse wheel %d %d 0 %u 0 %f %f",
		(int)(x * sc), (int)(y * sc), v->held, -dx, -dy);
	post(v, line);
	return TRUE;
}

// GDK's keys that are not text, as the helper's (GLFW) codes
static int glfw_key(guint k)
{
	switch (k) {
	case GDK_KEY_Escape: return 256;
	case GDK_KEY_Return: case GDK_KEY_KP_Enter: return 257;
	case GDK_KEY_Tab: return 258;
	case GDK_KEY_BackSpace: return 259;
	case GDK_KEY_Insert: return 260;
	case GDK_KEY_Delete: return 261;
	case GDK_KEY_Right: return 262;
	case GDK_KEY_Left: return 263;
	case GDK_KEY_Down: return 264;
	case GDK_KEY_Up: return 265;
	case GDK_KEY_Page_Up: return 266;
	case GDK_KEY_Page_Down: return 267;
	case GDK_KEY_Home: return 268;
	case GDK_KEY_End: return 269;
	}
	if (k >= GDK_KEY_a && k <= GDK_KEY_z)
		return 'A' + (k - GDK_KEY_a);
	return 0;
}

static gboolean on_key(GtkEventControllerKey *c, guint keyval, guint code, GdkModifierType m, AuraView *v, gboolean down)
{
	unsigned mods = mods_of(m);
	int g = glfw_key(keyval);
	// a letter with ctrl / alt is a shortcut; else typed text
	if (g && (g >= 256 || (mods & 6))) {
		char line[64];
		snprintf(line, sizeof line, "key %s %d %u", down ? "down" : "up", g, mods);
		post(v, line);
		return TRUE;
	}
	gunichar u = gdk_keyval_to_unicode(keyval);
	if (down && u >= 32) {
		char utf[8] = { 0 };
		g_unichar_to_utf8(u, utf);
		char line[32];
		snprintf(line, sizeof line, "text %s", utf);
		post(v, line);
		return TRUE;
	}
	return FALSE;
}

static gboolean on_key_down(GtkEventControllerKey *c, guint k, guint code, GdkModifierType m, AuraView *v)
{
	return on_key(c, k, code, m, v, TRUE);
}

static void on_key_up(GtkEventControllerKey *c, guint k, guint code, GdkModifierType m, AuraView *v)
{
	on_key(c, k, code, m, v, FALSE);
}

static void aura_view_free(gpointer data)
{
	AuraView *v = data;
	if (v->sock >= 0)
		close(v->sock); // the helper quits with its stdin
	if (v->pid > 0)
		waitpid(v->pid, NULL, WNOHANG);
	if (v->shm)
		munmap(v->shm, v->shm_size);
	g_free(v->shm_path);
	g_free(v->pending);
	g_free(v);
}

static AuraView *view_of(jlong widget_ptr)
{
	return g_object_get_data(G_OBJECT(_PTR(widget_ptr)), "aura-view");
}

JNIEXPORT jlong JNICALL Java_android_webkit_WebView_native_1constructor(JNIEnv *env, jobject this, jobject context, jobject attrs)
{
	GtkWidget *wrapper = g_object_ref(wrapper_widget_new());
	GtkWidget *picture = gtk_picture_new();
	gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);
	gtk_widget_set_focusable(picture, TRUE);
	AuraView *v = g_new0(AuraView, 1);
	v->picture = picture;
	v->wrapper = WRAPPER_WIDGET(wrapper);
	v->sock = -1;
	g_object_set_data_full(G_OBJECT(picture), "aura-view", v, aura_view_free);

	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
	g_signal_connect(click, "pressed", G_CALLBACK(on_pressed), v);
	g_signal_connect(click, "released", G_CALLBACK(on_released), v);
	gtk_widget_add_controller(picture, GTK_EVENT_CONTROLLER(click));
	GtkEventController *motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "motion", G_CALLBACK(on_motion), v);
	gtk_widget_add_controller(picture, motion);
	GtkEventController *scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), v);
	gtk_widget_add_controller(picture, scroll);
	GtkEventController *keys = gtk_event_controller_key_new();
	g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_down), v);
	g_signal_connect(keys, "key-released", G_CALLBACK(on_key_up), v);
	gtk_widget_add_controller(picture, keys);
	gtk_widget_add_tick_callback(picture, tick, v, NULL);

	wrapper_widget_set_child(WRAPPER_WIDGET(wrapper), picture);
	wrapper_widget_set_jobject(WRAPPER_WIDGET(wrapper), env, this);
	wrapper_widget_register_invalidation_listener(WRAPPER_WIDGET(wrapper));
	return _INTPTR(picture);
}

JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadUrl(JNIEnv *env, jobject this, jlong widget_ptr, jstring url)
{
	AuraView *v = view_of(widget_ptr);
	const char *u = _CSTRING(url);
	if (v->sock < 0) {
		start_helper(v, u);
		return;
	}
	char *line = g_strdup_printf("load %s", u);
	post(v, line);
	g_free(line);
}

// the page's text as a data: address
JNIEXPORT void JNICALL Java_android_webkit_WebView_native_1loadDataWithBaseURL(JNIEnv *env, jobject this, jlong widget_ptr, jstring base_url, jstring data_jstr, jstring mime_type, jstring encoding)
{
	AuraView *v = view_of(widget_ptr);
	jsize data_len = (*env)->GetStringUTFLength(env, data_jstr);
	jsize data_jlen = (*env)->GetStringLength(env, data_jstr);
	char *data = malloc(data_len + 1);
	(*env)->GetStringUTFRegion(env, data_jstr, 0, data_jlen, data);
	char *b64 = g_base64_encode((guchar *)data, data_len);
	free(data);
	const char *mime = mime_type ? _CSTRING(mime_type) : "text/html";
	char *url = g_strdup_printf("data:%s;charset=utf-8;base64,%s", mime, b64);
	g_free(b64);
	if (v->sock < 0)
		start_helper(v, url);
	else {
		char *line = g_strdup_printf("load %s", url);
		post(v, line);
		g_free(line);
	}
	g_free(url);
}
