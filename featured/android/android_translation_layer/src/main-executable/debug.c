#include <glib-unix.h>
#include <jni.h>

#include "../api-impl-jni/defines.h"
#include "../api-impl-jni/util.h"
#include "debug.h"

static gboolean dump_threads(gpointer data)
{
	JNIEnv *env = get_jni_env();
	jclass cls = (*env)->FindClass(env, "android/atl/ATLDebug");
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionClear(env);
	else if (cls) {
		(*env)->CallStaticVoidMethod(env, cls, _STATIC_METHOD(cls, "dumpThreads", "()V"));
		(*env)->DeleteLocalRef(env, cls);
	}
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	return G_SOURCE_CONTINUE;
}

// SIGUSR2: every thread's Java stack on stderr
void debug_signals(void)
{
	g_unix_signal_add(SIGUSR2, dump_threads, NULL);
}
