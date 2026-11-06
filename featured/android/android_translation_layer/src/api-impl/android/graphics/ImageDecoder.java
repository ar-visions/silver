package android.graphics;

// android.jar's shape, made by gen.py
public final class ImageDecoder implements java.lang.AutoCloseable {
	// GEN member:android.graphics.ImageDecoder:public static final int ALLOCATOR_DEFAULT = 0;
	public static final int ALLOCATOR_DEFAULT = 0;

	// GEN member:android.graphics.ImageDecoder:public static final int ALLOCATOR_HARDWARE = 3;
	public static final int ALLOCATOR_HARDWARE = 3;

	// GEN member:android.graphics.ImageDecoder:public static final int ALLOCATOR_SHARED_MEMORY = 2;
	public static final int ALLOCATOR_SHARED_MEMORY = 2;

	// GEN member:android.graphics.ImageDecoder:public static final int ALLOCATOR_SOFTWARE = 1;
	public static final int ALLOCATOR_SOFTWARE = 1;

	// GEN member:android.graphics.ImageDecoder:public static final int MEMORY_POLICY_DEFAULT = 1;
	public static final int MEMORY_POLICY_DEFAULT = 1;

	// GEN member:android.graphics.ImageDecoder:public static final int MEMORY_POLICY_LOW_RAM = 0;
	public static final int MEMORY_POLICY_LOW_RAM = 0;

	// GEN member:android.graphics.ImageDecoder:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.graphics.ImageDecoder:public static boolean isMimeTypeSupported(java.lang.String);
	public static boolean isMimeTypeSupported(java.lang.String arg0) { return false; }

	// GEN member:android.graphics.ImageDecoder:public void setTargetSize(int, int);
	public void setTargetSize(int arg0, int arg1) {}

	// GEN member:android.graphics.ImageDecoder:public void setTargetSampleSize(int);
	public void setTargetSampleSize(int arg0) {}

	// GEN member:android.graphics.ImageDecoder:public void setAllocator(int);
	public void setAllocator(int arg0) {}

	// GEN member:android.graphics.ImageDecoder:public int getAllocator();
	public int getAllocator() { return 0; }

	// GEN member:android.graphics.ImageDecoder:public void setUnpremultipliedRequired(boolean);
	public void setUnpremultipliedRequired(boolean arg0) {}

	// GEN member:android.graphics.ImageDecoder:public boolean isUnpremultipliedRequired();
	public boolean isUnpremultipliedRequired() { return false; }

	// GEN member:android.graphics.ImageDecoder:public void setCrop(android.graphics.Rect);
	public void setCrop(android.graphics.Rect arg0) {}

	// GEN member:android.graphics.ImageDecoder:public android.graphics.Rect getCrop();
	public android.graphics.Rect getCrop() { return null; }

	// GEN member:android.graphics.ImageDecoder:public void setMutableRequired(boolean);
	public void setMutableRequired(boolean arg0) {}

	// GEN member:android.graphics.ImageDecoder:public boolean isMutableRequired();
	public boolean isMutableRequired() { return false; }

	// GEN member:android.graphics.ImageDecoder:public void setMemorySizePolicy(int);
	public void setMemorySizePolicy(int arg0) {}

	// GEN member:android.graphics.ImageDecoder:public int getMemorySizePolicy();
	public int getMemorySizePolicy() { return 0; }

	// GEN member:android.graphics.ImageDecoder:public void setDecodeAsAlphaMaskEnabled(boolean);
	public void setDecodeAsAlphaMaskEnabled(boolean arg0) {}

	// GEN member:android.graphics.ImageDecoder:public boolean isDecodeAsAlphaMaskEnabled();
	public boolean isDecodeAsAlphaMaskEnabled() { return false; }

	// GEN member:android.graphics.ImageDecoder:public void setTargetColorSpace(android.graphics.ColorSpace);
	public void setTargetColorSpace(android.graphics.ColorSpace arg0) {}

	// GEN member:android.graphics.ImageDecoder:public void close();
	public void close() {}

	protected ImageDecoder() {}

	// GEN class:android.graphics.ImageDecoder$ImageInfo
	public class ImageInfo {
		// GEN member:android.graphics.ImageDecoder$ImageInfo:public android.util.Size getSize();
		public android.util.Size getSize() { return null; }
	
		// GEN member:android.graphics.ImageDecoder$ImageInfo:public java.lang.String getMimeType();
		public java.lang.String getMimeType() { return null; }
	
		// GEN member:android.graphics.ImageDecoder$ImageInfo:public boolean isAnimated();
		public boolean isAnimated() { return false; }
	
		// GEN member:android.graphics.ImageDecoder$ImageInfo:public android.graphics.ColorSpace getColorSpace();
		public android.graphics.ColorSpace getColorSpace() { return null; }
	
		protected ImageInfo() {}
	}


	// GEN class:android.graphics.ImageDecoder$OnHeaderDecodedListener
	public interface OnHeaderDecodedListener {
		// GEN member:android.graphics.ImageDecoder$OnHeaderDecodedListener:public abstract void onHeaderDecoded(android.graphics.ImageDecoder, android.graphics.ImageDecoder$ImageInfo, android.graphics.ImageDecoder$Source);
		default void onHeaderDecoded(android.graphics.ImageDecoder arg0, android.graphics.ImageDecoder.ImageInfo arg1, android.graphics.ImageDecoder.Source arg2) {}
	}


	// GEN class:android.graphics.ImageDecoder$OnPartialImageListener
	public interface OnPartialImageListener {
	
	}


	// GEN class:android.graphics.ImageDecoder$Source
	public abstract class Source {
		protected Source() {}
	}

}
