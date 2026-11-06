package android.media;

// android.jar's shape, made by gen.py
public class ImageReader implements java.lang.AutoCloseable {
	// GEN member:android.media.ImageReader:public static android.media.ImageReader newInstance(int, int, int, int);
	public static android.media.ImageReader newInstance(int arg0, int arg1, int arg2, int arg3) { return null; }

	// GEN member:android.media.ImageReader:public static android.media.ImageReader newInstance(int, int, int, int, long);
	public static android.media.ImageReader newInstance(int arg0, int arg1, int arg2, int arg3, long arg4) { return null; }

	// GEN member:android.media.ImageReader:public int getWidth();
	public int getWidth() { return 0; }

	// GEN member:android.media.ImageReader:public int getHeight();
	public int getHeight() { return 0; }

	// GEN member:android.media.ImageReader:public int getImageFormat();
	public int getImageFormat() { return 0; }

	// GEN member:android.media.ImageReader:public int getHardwareBufferFormat();
	public int getHardwareBufferFormat() { return 0; }

	// GEN member:android.media.ImageReader:public int getDataSpace();
	public int getDataSpace() { return 0; }

	// GEN member:android.media.ImageReader:public int getMaxImages();
	public int getMaxImages() { return 0; }

	// GEN member:android.media.ImageReader:public long getUsage();
	public long getUsage() { return 0; }

	// GEN member:android.media.ImageReader:public android.view.Surface getSurface();
	public android.view.Surface getSurface() { return null; }

	// GEN member:android.media.ImageReader:public android.media.Image acquireLatestImage();
	public android.media.Image acquireLatestImage() { return null; }

	// GEN member:android.media.ImageReader:public android.media.Image acquireNextImage();
	public android.media.Image acquireNextImage() { return null; }

	// GEN member:android.media.ImageReader:public void close();
	public void close() {}

	// GEN member:android.media.ImageReader:public void discardFreeBuffers();
	public void discardFreeBuffers() {}

	// GEN member:android.media.ImageReader:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	protected ImageReader() {}

	// GEN class:android.media.ImageReader$OnImageAvailableListener
	public interface OnImageAvailableListener {
		// GEN member:android.media.ImageReader$OnImageAvailableListener:public abstract void onImageAvailable(android.media.ImageReader);
		default void onImageAvailable(android.media.ImageReader arg0) {}
	}

}
