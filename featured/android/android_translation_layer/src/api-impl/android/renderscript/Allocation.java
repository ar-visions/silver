package android.renderscript;

// android.jar's shape, made by gen.py
public class Allocation extends android.renderscript.BaseObj {
	// GEN member:android.renderscript.Allocation:public static final int USAGE_GRAPHICS_CONSTANTS = 8;
	public static final int USAGE_GRAPHICS_CONSTANTS = 8;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_GRAPHICS_RENDER_TARGET = 16;
	public static final int USAGE_GRAPHICS_RENDER_TARGET = 16;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_GRAPHICS_TEXTURE = 2;
	public static final int USAGE_GRAPHICS_TEXTURE = 2;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_GRAPHICS_VERTEX = 4;
	public static final int USAGE_GRAPHICS_VERTEX = 4;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_IO_INPUT = 32;
	public static final int USAGE_IO_INPUT = 32;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_IO_OUTPUT = 64;
	public static final int USAGE_IO_OUTPUT = 64;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_SCRIPT = 1;
	public static final int USAGE_SCRIPT = 1;

	// GEN member:android.renderscript.Allocation:public static final int USAGE_SHARED = 128;
	public static final int USAGE_SHARED = 128;

	// GEN member:android.renderscript.Allocation:public android.renderscript.Element getElement();
	public android.renderscript.Element getElement() { return null; }

	// GEN member:android.renderscript.Allocation:public int getUsage();
	public int getUsage() { return 0; }

	// GEN member:android.renderscript.Allocation:public void setAutoPadding(boolean);
	public void setAutoPadding(boolean arg0) {}

	// GEN member:android.renderscript.Allocation:public int getBytesSize();
	public int getBytesSize() { return 0; }

	// GEN member:android.renderscript.Allocation:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.renderscript.Allocation:public void syncAll(int);
	public void syncAll(int arg0) {}

	// GEN member:android.renderscript.Allocation:public void ioSend();
	public void ioSend() {}

	// GEN member:android.renderscript.Allocation:public void ioReceive();
	public void ioReceive() {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(android.renderscript.BaseObj[]);
	public void copyFrom(android.renderscript.BaseObj[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFromUnchecked(java.lang.Object);
	public void copyFromUnchecked(java.lang.Object arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFromUnchecked(int[]);
	public void copyFromUnchecked(int[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFromUnchecked(short[]);
	public void copyFromUnchecked(short[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFromUnchecked(byte[]);
	public void copyFromUnchecked(byte[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFromUnchecked(float[]);
	public void copyFromUnchecked(float[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(java.lang.Object);
	public void copyFrom(java.lang.Object arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(int[]);
	public void copyFrom(int[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(short[]);
	public void copyFrom(short[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(byte[]);
	public void copyFrom(byte[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(float[]);
	public void copyFrom(float[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(android.graphics.Bitmap);
	public void copyFrom(android.graphics.Bitmap arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyFrom(android.renderscript.Allocation);
	public void copyFrom(android.renderscript.Allocation arg0) {}

	// GEN member:android.renderscript.Allocation:public void generateMipmaps();
	public void generateMipmaps() {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFromUnchecked(int, int, java.lang.Object);
	public void copy1DRangeFromUnchecked(int arg0, int arg1, java.lang.Object arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFromUnchecked(int, int, int[]);
	public void copy1DRangeFromUnchecked(int arg0, int arg1, int[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFromUnchecked(int, int, short[]);
	public void copy1DRangeFromUnchecked(int arg0, int arg1, short[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFromUnchecked(int, int, byte[]);
	public void copy1DRangeFromUnchecked(int arg0, int arg1, byte[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFromUnchecked(int, int, float[]);
	public void copy1DRangeFromUnchecked(int arg0, int arg1, float[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, java.lang.Object);
	public void copy1DRangeFrom(int arg0, int arg1, java.lang.Object arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, int[]);
	public void copy1DRangeFrom(int arg0, int arg1, int[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, short[]);
	public void copy1DRangeFrom(int arg0, int arg1, short[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, byte[]);
	public void copy1DRangeFrom(int arg0, int arg1, byte[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, float[]);
	public void copy1DRangeFrom(int arg0, int arg1, float[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeFrom(int, int, android.renderscript.Allocation, int);
	public void copy1DRangeFrom(int arg0, int arg1, android.renderscript.Allocation arg2, int arg3) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, java.lang.Object);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, java.lang.Object arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, byte[]);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, byte[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, short[]);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, short[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, int[]);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, int[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, float[]);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, float[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, int, int, android.renderscript.Allocation, int, int);
	public void copy2DRangeFrom(int arg0, int arg1, int arg2, int arg3, android.renderscript.Allocation arg4, int arg5, int arg6) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeFrom(int, int, android.graphics.Bitmap);
	public void copy2DRangeFrom(int arg0, int arg1, android.graphics.Bitmap arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy3DRangeFrom(int, int, int, int, int, int, java.lang.Object);
	public void copy3DRangeFrom(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, java.lang.Object arg6) {}

	// GEN member:android.renderscript.Allocation:public void copy3DRangeFrom(int, int, int, int, int, int, android.renderscript.Allocation, int, int, int);
	public void copy3DRangeFrom(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, android.renderscript.Allocation arg6, int arg7, int arg8, int arg9) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(android.graphics.Bitmap);
	public void copyTo(android.graphics.Bitmap arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(java.lang.Object);
	public void copyTo(java.lang.Object arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(byte[]);
	public void copyTo(byte[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(short[]);
	public void copyTo(short[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(int[]);
	public void copyTo(int[] arg0) {}

	// GEN member:android.renderscript.Allocation:public void copyTo(float[]);
	public void copyTo(float[] arg0) {}

	// GEN member:android.renderscript.Allocation:public synchronized void resize(int);
	public void resize(int arg0) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeToUnchecked(int, int, java.lang.Object);
	public void copy1DRangeToUnchecked(int arg0, int arg1, java.lang.Object arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeToUnchecked(int, int, int[]);
	public void copy1DRangeToUnchecked(int arg0, int arg1, int[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeToUnchecked(int, int, short[]);
	public void copy1DRangeToUnchecked(int arg0, int arg1, short[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeToUnchecked(int, int, byte[]);
	public void copy1DRangeToUnchecked(int arg0, int arg1, byte[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeToUnchecked(int, int, float[]);
	public void copy1DRangeToUnchecked(int arg0, int arg1, float[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeTo(int, int, java.lang.Object);
	public void copy1DRangeTo(int arg0, int arg1, java.lang.Object arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeTo(int, int, int[]);
	public void copy1DRangeTo(int arg0, int arg1, int[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeTo(int, int, short[]);
	public void copy1DRangeTo(int arg0, int arg1, short[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeTo(int, int, byte[]);
	public void copy1DRangeTo(int arg0, int arg1, byte[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy1DRangeTo(int, int, float[]);
	public void copy1DRangeTo(int arg0, int arg1, float[] arg2) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeTo(int, int, int, int, java.lang.Object);
	public void copy2DRangeTo(int arg0, int arg1, int arg2, int arg3, java.lang.Object arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeTo(int, int, int, int, byte[]);
	public void copy2DRangeTo(int arg0, int arg1, int arg2, int arg3, byte[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeTo(int, int, int, int, short[]);
	public void copy2DRangeTo(int arg0, int arg1, int arg2, int arg3, short[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeTo(int, int, int, int, int[]);
	public void copy2DRangeTo(int arg0, int arg1, int arg2, int arg3, int[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy2DRangeTo(int, int, int, int, float[]);
	public void copy2DRangeTo(int arg0, int arg1, int arg2, int arg3, float[] arg4) {}

	// GEN member:android.renderscript.Allocation:public void copy3DRangeTo(int, int, int, int, int, int, java.lang.Object);
	public void copy3DRangeTo(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, java.lang.Object arg6) {}

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createSized(android.renderscript.RenderScript, android.renderscript.Element, int, int);
	public static android.renderscript.Allocation createSized(android.renderscript.RenderScript arg0, android.renderscript.Element arg1, int arg2, int arg3) { return null; }

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createSized(android.renderscript.RenderScript, android.renderscript.Element, int);
	public static android.renderscript.Allocation createSized(android.renderscript.RenderScript arg0, android.renderscript.Element arg1, int arg2) { return null; }

	// GEN member:android.renderscript.Allocation:public java.nio.ByteBuffer getByteBuffer();
	public java.nio.ByteBuffer getByteBuffer() { return null; }

	// GEN member:android.renderscript.Allocation:public long getStride();
	public long getStride() { return 0; }

	// GEN member:android.renderscript.Allocation:public long getTimeStamp();
	public long getTimeStamp() { return 0; }

	// GEN member:android.renderscript.Allocation:public android.view.Surface getSurface();
	public android.view.Surface getSurface() { return null; }

	// GEN member:android.renderscript.Allocation:public void setSurface(android.view.Surface);
	public void setSurface(android.view.Surface arg0) {}

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createFromBitmap(android.renderscript.RenderScript, android.graphics.Bitmap);
	public static android.renderscript.Allocation createFromBitmap(android.renderscript.RenderScript arg0, android.graphics.Bitmap arg1) { return null; }

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createCubemapFromBitmap(android.renderscript.RenderScript, android.graphics.Bitmap);
	public static android.renderscript.Allocation createCubemapFromBitmap(android.renderscript.RenderScript arg0, android.graphics.Bitmap arg1) { return null; }

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createCubemapFromCubeFaces(android.renderscript.RenderScript, android.graphics.Bitmap, android.graphics.Bitmap, android.graphics.Bitmap, android.graphics.Bitmap, android.graphics.Bitmap, android.graphics.Bitmap);
	public static android.renderscript.Allocation createCubemapFromCubeFaces(android.renderscript.RenderScript arg0, android.graphics.Bitmap arg1, android.graphics.Bitmap arg2, android.graphics.Bitmap arg3, android.graphics.Bitmap arg4, android.graphics.Bitmap arg5, android.graphics.Bitmap arg6) { return null; }

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createFromBitmapResource(android.renderscript.RenderScript, android.content.res.Resources, int);
	public static android.renderscript.Allocation createFromBitmapResource(android.renderscript.RenderScript arg0, android.content.res.Resources arg1, int arg2) { return null; }

	// GEN member:android.renderscript.Allocation:public static android.renderscript.Allocation createFromString(android.renderscript.RenderScript, java.lang.String, int);
	public static android.renderscript.Allocation createFromString(android.renderscript.RenderScript arg0, java.lang.String arg1, int arg2) { return null; }

	// GEN member:android.renderscript.Allocation:public void destroy();
	public void destroy() {}

	protected Allocation() {}
}
