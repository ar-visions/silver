package android.opengl;

import com.google.android.gles_jni.EGLImpl;
import javax.microedition.khronos.egl.EGL10;

public class EGL14 {

	private static EGL10 impl = new EGLImpl();

	public static final EGLDisplay EGL_NO_DISPLAY = new EGLDisplay(0);
	public static final EGLContext EGL_NO_CONTEXT = new EGLContext(0);
	public static final EGLSurface EGL_NO_SURFACE = new EGLSurface(0);

	public static EGLDisplay eglGetDisplay(int display) {
		javax.microedition.khronos.egl.EGLDisplay eglDisplay = impl.eglGetDisplay(display);
		if (eglDisplay == EGL10.EGL_NO_DISPLAY)
			return EGL_NO_DISPLAY;
		else
			return new EGLDisplay(eglDisplay.native_egl_display);
	}

	public static boolean eglInitialize(EGLDisplay display, int[] major, int major_offset, int[] minor, int minor_offset) {
		int major_minor[] = new int[2];
		boolean ret = impl.eglInitialize(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), major_minor);
		if (major != null)
			major[major_offset] = major_minor[0];
		if (minor != null)
			minor[minor_offset] = major_minor[1];
		return ret;
	}

	public static boolean eglChooseConfig(EGLDisplay display, int[] attrib_list, int attrib_list_offset, EGLConfig[] configs, int config_offset, int config_size, int[] num_config, int num_config_offset) {
		int[] raw_attrib_list = new int[attrib_list.length - attrib_list_offset];
		System.arraycopy(attrib_list, attrib_list_offset, raw_attrib_list, 0, attrib_list.length - attrib_list_offset);
		javax.microedition.khronos.egl.EGLConfig[] raw_configs = new javax.microedition.khronos.egl.EGLConfig[config_size];
		int raw_num_config[] = new int[1];
		boolean ret = impl.eglChooseConfig(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), raw_attrib_list, raw_configs, config_size, raw_num_config);
		if (configs != null) {
			for (int i = 0; i < configs.length; i++) {
				configs[i + config_offset] = new EGLConfig(raw_configs[i].native_egl_config);
			}
		}
		if (num_config != null)
			num_config[num_config_offset] = raw_num_config[0];
		return ret;
	}

	public static EGLContext eglCreateContext(EGLDisplay display, EGLConfig config, EGLContext share_context, int[] attrib_list, int attrib_list_offset) {
		int[] raw_attrib_list = new int[attrib_list.length - attrib_list_offset];
		System.arraycopy(attrib_list, attrib_list_offset, raw_attrib_list, 0, attrib_list.length - attrib_list_offset);
		javax.microedition.khronos.egl.EGLConfig raw_config = new javax.microedition.khronos.egl.EGLConfig(config.getNativeHandle());
		javax.microedition.khronos.egl.EGLContext raw_share_context = (share_context != null) ? new javax.microedition.khronos.egl.EGLContext(share_context.getNativeHandle()) : null;
		javax.microedition.khronos.egl.EGLContext raw_egl_context = impl.eglCreateContext(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), raw_config, raw_share_context, raw_attrib_list);
		if (raw_egl_context == EGL10.EGL_NO_CONTEXT)
			return EGL_NO_CONTEXT;
		else
			return new EGLContext(raw_egl_context.native_egl_context);
	}

	public static EGLSurface eglCreatePbufferSurface(EGLDisplay display, EGLConfig config, int[] attrib_list, int attrib_list_offset) {
		int[] raw_attrib_list = new int[attrib_list.length - attrib_list_offset];
		System.arraycopy(attrib_list, attrib_list_offset, raw_attrib_list, 0, attrib_list.length - attrib_list_offset);
		javax.microedition.khronos.egl.EGLConfig raw_config = new javax.microedition.khronos.egl.EGLConfig(config.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_egl_surface = impl.eglCreatePbufferSurface(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), raw_config, raw_attrib_list);
		if (raw_egl_surface == EGL10.EGL_NO_SURFACE)
			return EGL_NO_SURFACE;
		else
			return new EGLSurface(((com.google.android.gles_jni.EGLSurfaceImpl)raw_egl_surface).mEGLSurface);
	}

	public static boolean eglDestroySurface(EGLDisplay display, EGLSurface surface) {
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_surface = new com.google.android.gles_jni.EGLSurfaceImpl(surface.getNativeHandle());
		return impl.eglDestroySurface(raw_display, raw_surface);
	}

	public static boolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) {
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_draw = new com.google.android.gles_jni.EGLSurfaceImpl(draw.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_read = new com.google.android.gles_jni.EGLSurfaceImpl(read.getNativeHandle());
		javax.microedition.khronos.egl.EGLContext raw_context = (context != null) ? new javax.microedition.khronos.egl.EGLContext(context.getNativeHandle()) : EGL10.EGL_NO_CONTEXT;
		return impl.eglMakeCurrent(raw_display, raw_draw, raw_read, raw_context);
	}

	public static boolean eglDestroyContext(EGLDisplay display, EGLContext context) {
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLContext raw_context = new javax.microedition.khronos.egl.EGLContext(context.getNativeHandle());
		return impl.eglDestroyContext(raw_display, raw_context);
	}

	public static boolean eglReleaseThread() {
		return impl.eglReleaseThread();
	}

	public static boolean eglTerminate(EGLDisplay display) {
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		return impl.eglTerminate(raw_display);
	}

	public static EGLSurface eglCreateWindowSurface(EGLDisplay display, EGLConfig config, Object window, int[] attrib_list, int attrib_list_offset) {
		int[] raw_attrib_list = new int[attrib_list.length - attrib_list_offset];
		System.arraycopy(attrib_list, attrib_list_offset, raw_attrib_list, 0, attrib_list.length - attrib_list_offset);
		if (window instanceof android.graphics.SurfaceTexture) {
			android.graphics.SurfaceTexture texture = (android.graphics.SurfaceTexture)window;
			if (texture.widget == 0)
				return EGL_NO_SURFACE;
			window = new android.view.Surface(texture);
		}
		javax.microedition.khronos.egl.EGLConfig raw_config = new javax.microedition.khronos.egl.EGLConfig(config.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_egl_surface = impl.eglCreateWindowSurface(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), raw_config, window, raw_attrib_list);
		if (raw_egl_surface == EGL10.EGL_NO_SURFACE)
			return EGL_NO_SURFACE;
		else
			return new EGLSurface(((com.google.android.gles_jni.EGLSurfaceImpl)raw_egl_surface).mEGLSurface);
	}

	public static int eglGetError() {
		return impl.eglGetError();
	}

	public static boolean eglQueryContext(EGLDisplay display, EGLContext context, int attribute, int[] value, int offset) {
		int[] raw_value = new int[1];
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLContext raw_context = new javax.microedition.khronos.egl.EGLContext(context.getNativeHandle());
		boolean ret = impl.eglQueryContext(raw_display, raw_context, attribute, raw_value);
		if (ret && value != null)
			value[offset] = raw_value[0];
		return ret;
	}

	public static String eglQueryString(EGLDisplay display, int name) {
		return impl.eglQueryString(new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle()), name);
	}

	public static boolean eglQuerySurface(EGLDisplay display, EGLSurface surface, int attribute, int[] value, int offset) {
		int[] raw_value = new int[1];
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_surface = new com.google.android.gles_jni.EGLSurfaceImpl(surface.getNativeHandle());
		boolean ret = impl.eglQuerySurface(raw_display, raw_surface, attribute, raw_value);
		if (ret && value != null)
			value[offset] = raw_value[0];
		return ret;
	}

	public static boolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
		javax.microedition.khronos.egl.EGLDisplay raw_display = new javax.microedition.khronos.egl.EGLDisplay(display.getNativeHandle());
		javax.microedition.khronos.egl.EGLSurface raw_surface = new com.google.android.gles_jni.EGLSurfaceImpl(surface.getNativeHandle());
		return impl.eglSwapBuffers(raw_display, raw_surface);
	}

	public static boolean eglSwapInterval(EGLDisplay display, int interval) {
		return true;
	}
}
