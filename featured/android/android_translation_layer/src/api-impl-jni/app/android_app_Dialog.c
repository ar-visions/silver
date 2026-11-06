#include <gtk/gtk.h>
#include <jni.h>

#include "../defines.h"
#include "../util.h"
#include "../generated_headers/android_app_Dialog.h"

/* main app window */
extern GtkWindow *window;

static gboolean on_close_request(GtkWidget *dialog, jobject jobj)
{
	printf("on_close_request\n");
	JNIEnv *env = get_jni_env();
	J__DialogInterface__dismiss(env, jobj);
	return FALSE;
}

/* state for Dialog.setCanceledOnTouchOutside() */
struct touch_outside {
	GtkWindow *dialog;
	GtkEventController *controller;
	jobject jobj;
};

static gboolean on_touch_outside(GtkEventControllerLegacy *controller, GdkEvent *event, struct touch_outside *outside)
{
	GdkEventType event_type = gdk_event_get_event_type(event);
	if ((event_type == GDK_BUTTON_PRESS || event_type == GDK_TOUCH_BEGIN) && gtk_widget_get_visible(GTK_WIDGET(outside->dialog))) {
		JNIEnv *env = get_jni_env();
		jmethodID dismiss = _METHOD(_CLASS(outside->jobj), "dismiss", "()V");
		(*env)->CallVoidMethod(env, outside->jobj, dismiss);
		if ((*env)->ExceptionCheck(env))
			(*env)->ExceptionDescribe(env);
		return TRUE;
	} else {
		return FALSE;
	}
}

static void remove_touch_outside(GtkWindow *dialog)
{
	struct touch_outside *outside = g_object_get_data(G_OBJECT(dialog), "touch-outside");
	if (!outside)
		return;

	JNIEnv *env = get_jni_env();
	gtk_widget_remove_controller(GTK_WIDGET(window), outside->controller);
	_UNREF(outside->jobj);
	g_free(outside);
	g_object_set_data(G_OBJECT(dialog), "touch-outside", NULL);
}

JNIEXPORT jlong JNICALL Java_android_app_Dialog_nativeInit(JNIEnv *env, jobject this, jlong decor_view_ptr)
{
	GtkWidget *decor_view = GTK_WIDGET(_PTR(decor_view_ptr));
	GtkWidget *dialog = gtk_window_new();
	gtk_window_set_transient_for(GTK_WINDOW(dialog), window);
	gtk_window_set_child(GTK_WINDOW(dialog), gtk_widget_get_parent(decor_view));
	g_signal_connect(GTK_WINDOW(dialog), "close-request", G_CALLBACK(on_close_request), _REF(this));
	return _INTPTR(g_object_ref(dialog));
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeSetTitle(JNIEnv *env, jobject this, jlong ptr, jstring title)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	const char *nativeTitle = (*env)->GetStringUTFChars(env, title, NULL);
	gtk_window_set_title(dialog, nativeTitle);
	(*env)->ReleaseStringUTFChars(env, title, nativeTitle);
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeSetContentView(JNIEnv *env, jobject this, jlong ptr, jlong widget_ptr)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	GtkWidget *widget = GTK_WIDGET(_PTR(widget_ptr));

	gtk_window_set_child(dialog, gtk_widget_get_parent(widget));
}

/* a parent not yet on screen is dropped by the compositor:
 * a dialog shows once the app's window has drawn a frame */
static GdkFrameClock *main_clock(void)
{
	GdkSurface *surface = window ? gtk_native_get_surface(GTK_NATIVE(window)) : NULL;
	return surface ? gdk_surface_get_frame_clock(surface) : NULL;
}

static gboolean present_dialog(gpointer dialog)
{
	gtk_window_present(GTK_WINDOW(dialog));
	g_object_unref(dialog);
	return G_SOURCE_REMOVE;
}

static void show_after_paint(GdkFrameClock *clock, GtkWindow *dialog)
{
	g_signal_handlers_disconnect_by_func(clock, show_after_paint, dialog);
	g_idle_add(present_dialog, g_object_ref(dialog));
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeShow(JNIEnv *env, jobject this, jlong ptr)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	GdkFrameClock *clock = main_clock();
	if (!clock) {
		gtk_window_present(dialog);
		return;
	}
	g_signal_handlers_disconnect_by_func(clock, show_after_paint, dialog);
	g_signal_connect(clock, "after-paint", G_CALLBACK(show_after_paint), dialog);
	gtk_widget_queue_draw(GTK_WIDGET(window));
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeClose(JNIEnv *env, jobject this, jlong ptr)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	remove_touch_outside(dialog);
	GdkFrameClock *clock = main_clock();
	if (clock)
		g_signal_handlers_disconnect_by_func(clock, show_after_paint, dialog);
	gtk_window_close(dialog);
}

JNIEXPORT jboolean JNICALL Java_android_app_Dialog_nativeIsShowing(JNIEnv *env, jobject this, jlong ptr)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	return gtk_widget_is_visible(GTK_WIDGET(dialog));
}

JNIEXPORT void JNICALL Java_android_app_Dialog_nativeSetCanceledOnTouchOutside(JNIEnv *env, jobject this, jlong ptr, jboolean cancel)
{
	GtkWindow *dialog = GTK_WINDOW(_PTR(ptr));
	remove_touch_outside(dialog);
	if (!cancel)
		return;

	struct touch_outside *outside = g_new0(struct touch_outside, 1);
	outside->dialog = dialog;
	outside->jobj = _REF(this);

	GtkEventController *controller = GTK_EVENT_CONTROLLER(gtk_event_controller_legacy_new());
	/* capture phase on the toplevel runs before the view tree's own touch controllers */
	gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
	g_signal_connect(controller, "event", G_CALLBACK(on_touch_outside), outside);
	gtk_widget_add_controller(GTK_WIDGET(window), controller);
	outside->controller = controller;

	g_object_set_data(G_OBJECT(dialog), "touch-outside", outside);
}
