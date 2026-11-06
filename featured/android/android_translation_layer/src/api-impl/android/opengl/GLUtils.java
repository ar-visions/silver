package android.opengl;
import android.graphics.Bitmap;

public class GLUtils {
	public static void texImage2D(int target, int level, Bitmap bitmap, int border) {
		if (native_texImage2D(target, level, -1, bitmap, -1, border) != 0) {
			throw new IllegalArgumentException("invalid Bitmap format");
		}
	}

	private static native int native_texImage2D(int target, int level, int internalformat,
	                                            Bitmap bitmap, int type, int border);

	// GEN member:android.opengl.GLUtils:public static void texSubImage2D(int, int, int, int, android.graphics.Bitmap);
	public static void texSubImage2D(int arg0, int arg1, int arg2, int arg3, android.graphics.Bitmap arg4) {}
}
