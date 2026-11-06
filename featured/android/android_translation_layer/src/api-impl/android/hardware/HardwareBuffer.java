package android.hardware;

// android.jar's shape, made by gen.py
public final class HardwareBuffer implements android.os.Parcelable, java.lang.AutoCloseable {
	// GEN member:android.hardware.HardwareBuffer:public static final int BLOB = 33;
	public static final int BLOB = 33;

	// GEN member:android.hardware.HardwareBuffer:public static final android.os.Parcelable$Creator<android.hardware.HardwareBuffer> CREATOR;
	public static final android.os.Parcelable.Creator<android.hardware.HardwareBuffer> CREATOR = null;

	// GEN member:android.hardware.HardwareBuffer:public static final int DS_24UI8 = 50;
	public static final int DS_24UI8 = 50;

	// GEN member:android.hardware.HardwareBuffer:public static final int DS_FP32UI8 = 52;
	public static final int DS_FP32UI8 = 52;

	// GEN member:android.hardware.HardwareBuffer:public static final int D_16 = 48;
	public static final int D_16 = 48;

	// GEN member:android.hardware.HardwareBuffer:public static final int D_24 = 49;
	public static final int D_24 = 49;

	// GEN member:android.hardware.HardwareBuffer:public static final int D_FP32 = 51;
	public static final int D_FP32 = 51;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGBA_10101010 = 59;
	public static final int RGBA_10101010 = 59;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGBA_1010102 = 43;
	public static final int RGBA_1010102 = 43;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGBA_8888 = 1;
	public static final int RGBA_8888 = 1;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGBA_FP16 = 22;
	public static final int RGBA_FP16 = 22;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGBX_8888 = 2;
	public static final int RGBX_8888 = 2;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGB_565 = 4;
	public static final int RGB_565 = 4;

	// GEN member:android.hardware.HardwareBuffer:public static final int RGB_888 = 3;
	public static final int RGB_888 = 3;

	// GEN member:android.hardware.HardwareBuffer:public static final int RG_1616 = 58;
	public static final int RG_1616 = 58;

	// GEN member:android.hardware.HardwareBuffer:public static final int R_16 = 57;
	public static final int R_16 = 57;

	// GEN member:android.hardware.HardwareBuffer:public static final int R_8 = 56;
	public static final int R_8 = 56;

	// GEN member:android.hardware.HardwareBuffer:public static final int S_UI8 = 53;
	public static final int S_UI8 = 53;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_COMPOSER_OVERLAY = 2048l;
	public static final long USAGE_COMPOSER_OVERLAY = 2048L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_CPU_READ_OFTEN = 3l;
	public static final long USAGE_CPU_READ_OFTEN = 3L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_CPU_READ_RARELY = 2l;
	public static final long USAGE_CPU_READ_RARELY = 2L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_CPU_WRITE_OFTEN = 48l;
	public static final long USAGE_CPU_WRITE_OFTEN = 48L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_CPU_WRITE_RARELY = 32l;
	public static final long USAGE_CPU_WRITE_RARELY = 32L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_FRONT_BUFFER = 4294967296l;
	public static final long USAGE_FRONT_BUFFER = 4294967296L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_GPU_COLOR_OUTPUT = 512l;
	public static final long USAGE_GPU_COLOR_OUTPUT = 512L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_GPU_CUBE_MAP = 33554432l;
	public static final long USAGE_GPU_CUBE_MAP = 33554432L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_GPU_DATA_BUFFER = 16777216l;
	public static final long USAGE_GPU_DATA_BUFFER = 16777216L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_GPU_MIPMAP_COMPLETE = 67108864l;
	public static final long USAGE_GPU_MIPMAP_COMPLETE = 67108864L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_GPU_SAMPLED_IMAGE = 256l;
	public static final long USAGE_GPU_SAMPLED_IMAGE = 256L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_PROTECTED_CONTENT = 16384l;
	public static final long USAGE_PROTECTED_CONTENT = 16384L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_SENSOR_DIRECT_DATA = 8388608l;
	public static final long USAGE_SENSOR_DIRECT_DATA = 8388608L;

	// GEN member:android.hardware.HardwareBuffer:public static final long USAGE_VIDEO_ENCODE = 65536l;
	public static final long USAGE_VIDEO_ENCODE = 65536L;

	// GEN member:android.hardware.HardwareBuffer:public static final int YCBCR_420_888 = 35;
	public static final int YCBCR_420_888 = 35;

	// GEN member:android.hardware.HardwareBuffer:public static final int YCBCR_P010 = 54;
	public static final int YCBCR_P010 = 54;

	// GEN member:android.hardware.HardwareBuffer:public static android.hardware.HardwareBuffer create(int, int, int, int, long);
	public static android.hardware.HardwareBuffer create(int arg0, int arg1, int arg2, int arg3, long arg4) { return null; }

	// GEN member:android.hardware.HardwareBuffer:public static boolean isSupported(int, int, int, int, long);
	public static boolean isSupported(int arg0, int arg1, int arg2, int arg3, long arg4) { return false; }

	// GEN member:android.hardware.HardwareBuffer:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.hardware.HardwareBuffer:public int getWidth();
	public int getWidth() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public int getHeight();
	public int getHeight() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public int getFormat();
	public int getFormat() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public int getLayers();
	public int getLayers() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public long getUsage();
	public long getUsage() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public long getId();
	public long getId() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public void close();
	public void close() {}

	// GEN member:android.hardware.HardwareBuffer:public boolean isClosed();
	public boolean isClosed() { return false; }

	// GEN member:android.hardware.HardwareBuffer:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.hardware.HardwareBuffer:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected HardwareBuffer() {}
}
