#include <gio/gio.h>

#include "../defines.h"
#include "../util.h"

JNIEXPORT jboolean JNICALL Java_android_net_ConnectivityManager_isActiveNetworkMetered(JNIEnv *env, jobject this)
{
	return g_network_monitor_get_network_metered(g_network_monitor_get_default());
}

JNIEXPORT jboolean JNICALL Java_android_net_ConnectivityManager_nativeGetNetworkAvailable(JNIEnv *env, jclass cls)
{
	return g_network_monitor_get_network_available(g_network_monitor_get_default());
}

static jclass cm_class;
static jmethodID cm_changed;

static void on_network_changed(GNetworkMonitor *self, gboolean network_available, gpointer data)
{
	JNIEnv *env = get_jni_env();
	(*env)->CallStaticVoidMethod(env, cm_class, cm_changed, (jboolean)network_available);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
}

// one signal connection for the process; Java fans it out to its callbacks
JNIEXPORT void JNICALL Java_android_net_ConnectivityManager_nativeWatch(JNIEnv *env, jclass cls, jclass self)
{
	cm_class = _REF(self);
	cm_changed = (*env)->GetStaticMethodID(env, self, "networkChanged", "(Z)V");
	g_signal_connect(g_network_monitor_get_default(), "network-changed", G_CALLBACK(on_network_changed), NULL);
}
