package android.media;

// android.jar's shape, made by gen.py
public class ImageWriter implements java.lang.AutoCloseable {
	// GEN member:android.media.ImageWriter:public static android.media.ImageWriter newInstance(android.view.Surface, int);
	public static android.media.ImageWriter newInstance(android.view.Surface arg0, int arg1) { return null; }

	// GEN member:android.media.ImageWriter:public static android.media.ImageWriter newInstance(android.view.Surface, int, int);
	public static android.media.ImageWriter newInstance(android.view.Surface arg0, int arg1, int arg2) { return null; }

	// GEN member:android.media.ImageWriter:public int getMaxImages();
	public int getMaxImages() { return 0; }

	// GEN member:android.media.ImageWriter:public int getWidth();
	public int getWidth() { return 0; }

	// GEN member:android.media.ImageWriter:public int getHeight();
	public int getHeight() { return 0; }

	// GEN member:android.media.ImageWriter:public android.media.Image dequeueInputImage();
	public android.media.Image dequeueInputImage() { return null; }

	// GEN member:android.media.ImageWriter:public void queueInputImage(android.media.Image);
	public void queueInputImage(android.media.Image arg0) {}

	// GEN member:android.media.ImageWriter:public int getFormat();
	public int getFormat() { return 0; }

	// GEN member:android.media.ImageWriter:public long getUsage();
	public long getUsage() { return 0; }

	// GEN member:android.media.ImageWriter:public int getHardwareBufferFormat();
	public int getHardwareBufferFormat() { return 0; }

	// GEN member:android.media.ImageWriter:public int getDataSpace();
	public int getDataSpace() { return 0; }

	// GEN member:android.media.ImageWriter:public void setOnImageReleasedListener(android.media.ImageWriter$OnImageReleasedListener, android.os.Handler);
	public void setOnImageReleasedListener(android.media.ImageWriter.OnImageReleasedListener arg0, android.os.Handler arg1) {}

	// GEN member:android.media.ImageWriter:public void close();
	public void close() {}

	// GEN member:android.media.ImageWriter:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	protected ImageWriter() {}

	// GEN class:android.media.ImageWriter$OnImageReleasedListener
	public interface OnImageReleasedListener {
		// GEN member:android.media.ImageWriter$OnImageReleasedListener:public abstract void onImageReleased(android.media.ImageWriter);
		default void onImageReleased(android.media.ImageWriter arg0) {}
	}

}
