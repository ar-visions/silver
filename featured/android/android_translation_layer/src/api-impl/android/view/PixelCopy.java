package android.view;

// android.jar's shape, made by gen.py
public final class PixelCopy {
	// GEN member:android.view.PixelCopy:public static final int ERROR_DESTINATION_INVALID = 5;
	public static final int ERROR_DESTINATION_INVALID = 5;

	// GEN member:android.view.PixelCopy:public static final int ERROR_SOURCE_INVALID = 4;
	public static final int ERROR_SOURCE_INVALID = 4;

	// GEN member:android.view.PixelCopy:public static final int ERROR_SOURCE_NO_DATA = 3;
	public static final int ERROR_SOURCE_NO_DATA = 3;

	// GEN member:android.view.PixelCopy:public static final int ERROR_TIMEOUT = 2;
	public static final int ERROR_TIMEOUT = 2;

	// GEN member:android.view.PixelCopy:public static final int ERROR_UNKNOWN = 1;
	public static final int ERROR_UNKNOWN = 1;

	// GEN member:android.view.PixelCopy:public static final int SUCCESS = 0;
	public static final int SUCCESS = 0;

	protected PixelCopy() {}

	// GEN class:android.view.PixelCopy$OnPixelCopyFinishedListener
	public interface OnPixelCopyFinishedListener {
		// GEN member:android.view.PixelCopy$OnPixelCopyFinishedListener:public abstract void onPixelCopyFinished(int);
		default void onPixelCopyFinished(int arg0) {}
	}


	// GEN class:android.view.PixelCopy$Request
	public final class Request {
		// GEN member:android.view.PixelCopy$Request:public android.graphics.Bitmap getDestinationBitmap();
		public android.graphics.Bitmap getDestinationBitmap() { return null; }
	
		// GEN member:android.view.PixelCopy$Request:public android.graphics.Rect getSourceRect();
		public android.graphics.Rect getSourceRect() { return null; }
	
		protected Request() {}
	
	// GEN class:android.view.PixelCopy$Request$Builder
	public final class Builder {
		// GEN member:android.view.PixelCopy$Request$Builder:public android.view.PixelCopy$Request$Builder setSourceRect(android.graphics.Rect);
		public android.view.PixelCopy.Request.Builder setSourceRect(android.graphics.Rect arg0) { return this; }
	
		// GEN member:android.view.PixelCopy$Request$Builder:public android.view.PixelCopy$Request$Builder setDestinationBitmap(android.graphics.Bitmap);
		public android.view.PixelCopy.Request.Builder setDestinationBitmap(android.graphics.Bitmap arg0) { return this; }
	
		// GEN member:android.view.PixelCopy$Request$Builder:public android.view.PixelCopy$Request build();
		public android.view.PixelCopy.Request build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.view.PixelCopy$Result
	public final class Result {
		// GEN member:android.view.PixelCopy$Result:public int getStatus();
		public int getStatus() { return 0; }
	
		// GEN member:android.view.PixelCopy$Result:public android.graphics.Bitmap getBitmap();
		public android.graphics.Bitmap getBitmap() { return null; }
	
		protected Result() {}
	}

}
