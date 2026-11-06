package android.media;

// android.jar's shape, made by gen.py
public abstract class Image implements java.lang.AutoCloseable {
	// GEN member:android.media.Image:public abstract int getFormat();
	public abstract int getFormat();

	// GEN member:android.media.Image:public abstract int getWidth();
	public abstract int getWidth();

	// GEN member:android.media.Image:public abstract int getHeight();
	public abstract int getHeight();

	// GEN member:android.media.Image:public abstract long getTimestamp();
	public abstract long getTimestamp();

	// GEN member:android.media.Image:public void setTimestamp(long);
	public void setTimestamp(long arg0) {}

	// GEN member:android.media.Image:public int getDataSpace();
	public int getDataSpace() { return 0; }

	// GEN member:android.media.Image:public void setDataSpace(int);
	public void setDataSpace(int arg0) {}

	// GEN member:android.media.Image:public android.graphics.Rect getCropRect();
	public android.graphics.Rect getCropRect() { return null; }

	// GEN member:android.media.Image:public void setCropRect(android.graphics.Rect);
	public void setCropRect(android.graphics.Rect arg0) {}

	// GEN member:android.media.Image:public abstract android.media.Image$Plane[] getPlanes();
	public abstract android.media.Image.Plane[] getPlanes();

	// GEN member:android.media.Image:public abstract void close();
	public abstract void close();

	protected Image() {}

	// GEN class:android.media.Image$Plane
	public abstract class Plane {
		// GEN member:android.media.Image$Plane:public abstract int getRowStride();
		public abstract int getRowStride();
	
		// GEN member:android.media.Image$Plane:public abstract int getPixelStride();
		public abstract int getPixelStride();
	
		// GEN member:android.media.Image$Plane:public abstract java.nio.ByteBuffer getBuffer();
		public abstract java.nio.ByteBuffer getBuffer();
	
		protected Plane() {}
	}


	// GEN member:android.media.Image:public android.hardware.HardwareBuffer getHardwareBuffer();
	public android.hardware.HardwareBuffer getHardwareBuffer() { return null; }
}
