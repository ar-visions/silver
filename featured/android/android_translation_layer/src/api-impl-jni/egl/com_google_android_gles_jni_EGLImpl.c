#include <EGL/egl.h>

#include "../defines.h"
#include "../util.h"

#include "../../libandroid/native_window.h"

#include "../generated_headers/com_google_android_gles_jni_EGLImpl.h"

// helpers from android source (TODO: either use GetIntArrayElements, or figure out if GetPrimitiveArrayCritical is superior and use it everywhere if so)
static jint *get_int_array_crit(JNIEnv *env, jintArray array)
{
	if (array != NULL) {
		return (jint *)(*env)->GetPrimitiveArrayCritical(env, array, (jboolean *)0);
	} else {
		return (jint *)NULL; // FIXME - do apps expect us to use some default?
	}
}

static void release_int_array_crit(JNIEnv *env, jintArray array, jint *base)
{
	if (array != NULL) {
		(*env)->ReleasePrimitiveArrayCritical(env, array, base, JNI_ABORT);
	}
}

// ---

static jlong *get_long_array_crit(JNIEnv *env, jlongArray array)
{
	if (array != NULL) {
		return (jlong *)(*env)->GetPrimitiveArrayCritical(env, array, (jboolean *)0);
	} else {
		return (jlong *)NULL; // FIXME - do apps expect us to use some default?
	}
}

static void release_long_array_crit(JNIEnv *env, jlongArray array, jlong *base)
{
	if (array != NULL) {
		(*env)->ReleasePrimitiveArrayCritical(env, array, base, JNI_ABORT);
	}
}

JNIEXPORT jlong JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglCreateContext(JNIEnv *env, jobject this, jlong egl_display, jlong egl_config, jlong share_context, jintArray attrib_list)
{
	printf("env: %p, this: %p, egl_display: %p, egl_config: %p, share_context: %p, attrib_list: %p\n", env, this, _PTR(egl_display), _PTR(egl_config), _PTR(share_context), attrib_list);

	jint *attrib_base = get_int_array_crit(env, attrib_list);

	EGLContext egl_context = eglCreateContext(_PTR(egl_display), _PTR(egl_config), _PTR(share_context), attrib_base);

	if (!egl_context) {
		/* GLES1 (explicit EGL_CONTEXT_CLIENT_VERSION=1, or the legacy default of no
		 * version at all) is no longer available: since Mesa 23.1.0 (May 2023) builds
		 * with -Dgles1=disabled (all mainstream distros) drop the OpenGL ES 1.x code
		 * entirely, eglCreateContext fails with EGL_BAD_CONFIG
		 * (https://gitlab.freedesktop.org/mesa/mesa/-/issues/9038). Old apps still call
		 * the ES1 fixed-function API, so for those requests create a desktop GL compatibility
		 * context instead: it implements exactly that fixed-function pipeline. It is configless
		 * (EGL_KHR_no_config_context), hence compatible with any config/surface */
		int client_version = 1;
		if (attrib_base) {
			for (EGLint *a = attrib_base; *a != EGL_NONE; a += 2) {
				if (a[0] == EGL_CONTEXT_CLIENT_VERSION) {
					client_version = a[1];
					break;
				}
			}
		}
		if (client_version == 1) {
			printf("HACK: eglCreateContext failed for GLES1; Creating desktop GL compatibility context instead, which should be mostly compatible. If possible switch to a GLES1 supporting driver. (eg. Mesa build with '-D gles1=enabled')\n");
			EGLenum prev_api = eglQueryAPI();
			eglBindAPI(EGL_OPENGL_API);
			egl_context = eglCreateContext(_PTR(egl_display), (EGLConfig)0, _PTR(share_context), NULL);
			eglBindAPI(prev_api);
		}
	}
	printf("egl_context: %p\n", egl_context);

	release_int_array_crit(env, attrib_list, attrib_base);

	return _INTPTR(egl_context);
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglChooseConfig(JNIEnv *env, jobject this, jlong egl_display, jintArray attrib_list, jlongArray egl_configs, jint config_size, jintArray num_config)
{
	int ret;

	jint *attrib_base = get_int_array_crit(env, attrib_list);
	jlong *configs_base = get_long_array_crit(env, egl_configs);
	jint *num_config_base = get_int_array_crit(env, num_config);

	ret = eglChooseConfig(_PTR(egl_display), attrib_base, egl_configs ? _PTR(configs_base) : NULL, config_size, num_config_base);
	printf(".. eglChooseConfig: egl_display: %w64x, egl_configs: %p, _PTR(configs_base): %p, config_size: %d, num_config_base[0]: %d\n", egl_display, egl_configs, _PTR(configs_base), config_size, num_config_base[0]);

	release_int_array_crit(env, attrib_list, attrib_base);
	release_long_array_crit(env, egl_configs, configs_base);
	release_int_array_crit(env, num_config, num_config_base);

	return ret;
}

JNIEXPORT jlong JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglCreateWindowSurface(JNIEnv *env, jobject this, jlong display, jlong config, jobject surface, jintArray _attrib_list)
{
	struct ANativeWindow *native_window = ANativeWindow_fromSurface(env, surface);
	EGLint *attrib_list = get_int_array_crit(env, _attrib_list);
	EGLSurface ret = bionic_eglCreateWindowSurface(_PTR(display), _PTR(config), native_window, attrib_list);
	release_int_array_crit(env, _attrib_list, attrib_list);
	ANativeWindow_release(native_window);
	return _INTPTR(ret);
}

JNIEXPORT jlong JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglGetDisplay(JNIEnv *env, jobject this, jobject display)
{
	return _INTPTR(bionic_eglGetDisplay(0)); // FIXME: why is display passed as an Object??? how do we get an integer from that
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglInitialize(JNIEnv *env, jobject this, jlong display, jintArray _major_minor)
{
	EGLint *major_minor = get_int_array_crit(env, _major_minor);
	bool ret = eglInitialize(_PTR(display), &major_minor[0], &major_minor[1]);
	release_int_array_crit(env, _major_minor, major_minor);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglGetConfigAttrib(JNIEnv *env, jobject this, jlong display, jlong config, jint attribute, jintArray _value)
{
	EGLint *value = get_int_array_crit(env, _value);
	bool ret = eglGetConfigAttrib(_PTR(display), _PTR(config), attribute, &value[0]);
	release_int_array_crit(env, _value, value);
	return ret;
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglMakeCurrent(JNIEnv *env, jobject this, jlong display, jlong draw_surface, jlong read_surface, jlong context)
{
	return bionic_eglMakeCurrent(_PTR(display), _PTR(draw_surface), _PTR(read_surface), _PTR(context));
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglSwapBuffers(JNIEnv *env, jobject this, jlong display, jlong surface)
{
	return bionic_eglSwapBuffers(_PTR(display), _PTR(surface));
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglDestroySurface(JNIEnv *env, jobject this, jlong display, jlong surface)
{
	return bionic_eglDestroySurface(_PTR(display), _PTR(surface));
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglDestroyContext(JNIEnv *env, jobject this, jlong display, jlong context)
{
	return eglDestroyContext(_PTR(display), _PTR(context));
}

JNIEXPORT jlong JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglCreatePbufferSurface(JNIEnv *env, jobject this, jlong display, jlong config, jintArray _attrib_list)
{
	EGLint *attrib_list = get_int_array_crit(env, _attrib_list);
	EGLSurface ret = eglCreatePbufferSurface(_PTR(display), _PTR(config), attrib_list);
	release_int_array_crit(env, _attrib_list, attrib_list);
	return _INTPTR(ret);
}

JNIEXPORT jlong JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglGetCurrentContext(JNIEnv *env, jobject this)
{
	return _INTPTR(eglGetCurrentContext());
}

JNIEXPORT jint JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglGetError(JNIEnv *env, jobject this)
{
	return eglGetError();
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglQueryContext(JNIEnv *env, jobject this, jlong display, jlong context, jint attribute, jintArray value_ref)
{
	EGLint v = 0;
	EGLBoolean ret = eglQueryContext(_PTR(display), _PTR(context), attribute, &v);
	if (ret && value_ref)
		(*env)->SetIntArrayRegion(env, value_ref, 0, 1, &v);
	return ret;
}

JNIEXPORT jstring JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglQueryString(JNIEnv *env, jobject this, jlong display, jint name)
{
	const char *chars = eglQueryString(_PTR(display), name);
	return chars ? _JSTRING(chars) : NULL;
}

JNIEXPORT jboolean JNICALL Java_com_google_android_gles_1jni_EGLImpl_native_1eglQuerySurface(JNIEnv *env, jobject this, jlong display, jlong surface, jint attribute, jintArray value_ref)
{
	EGLint v = 0;
	EGLBoolean ret = eglQuerySurface(_PTR(display), _PTR(surface), attribute, &v);
	if (ret && value_ref)
		(*env)->SetIntArrayRegion(env, value_ref, 0, 1, &v);
	return ret;
}
