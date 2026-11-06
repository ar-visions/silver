package android.hardware.camera2.params;

// android.jar's shape, made by gen.py
public final class OutputConfiguration implements android.os.Parcelable {
	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final android.os.Parcelable$Creator<android.hardware.camera2.params.OutputConfiguration> CREATOR;
	public static final android.os.Parcelable.Creator<android.hardware.camera2.params.OutputConfiguration> CREATOR = null;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int MIRROR_MODE_AUTO = 0;
	public static final int MIRROR_MODE_AUTO = 0;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int MIRROR_MODE_H = 2;
	public static final int MIRROR_MODE_H = 2;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int MIRROR_MODE_NONE = 1;
	public static final int MIRROR_MODE_NONE = 1;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int MIRROR_MODE_V = 3;
	public static final int MIRROR_MODE_V = 3;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int SURFACE_GROUP_ID_NONE = -1;
	public static final int SURFACE_GROUP_ID_NONE = -1;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int TIMESTAMP_BASE_CHOREOGRAPHER_SYNCED = 4;
	public static final int TIMESTAMP_BASE_CHOREOGRAPHER_SYNCED = 4;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int TIMESTAMP_BASE_DEFAULT = 0;
	public static final int TIMESTAMP_BASE_DEFAULT = 0;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int TIMESTAMP_BASE_MONOTONIC = 2;
	public static final int TIMESTAMP_BASE_MONOTONIC = 2;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int TIMESTAMP_BASE_REALTIME = 3;
	public static final int TIMESTAMP_BASE_REALTIME = 3;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static final int TIMESTAMP_BASE_SENSOR = 1;
	public static final int TIMESTAMP_BASE_SENSOR = 1;

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(android.view.Surface);
	public OutputConfiguration(android.view.Surface arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(int, android.view.Surface);
	public OutputConfiguration(int arg0, android.view.Surface arg1) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(int, android.util.Size);
	public OutputConfiguration(int arg0, android.util.Size arg1) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(int, int, android.util.Size);
	public OutputConfiguration(int arg0, int arg1, android.util.Size arg2) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(int, android.util.Size, long);
	public OutputConfiguration(int arg0, android.util.Size arg1, long arg2) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.hardware.camera2.params.OutputConfiguration(int, int, android.util.Size, long);
	public OutputConfiguration(int arg0, int arg1, android.util.Size arg2, long arg3) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setDynamicRangeProfile(long);
	public void setDynamicRangeProfile(long arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public long getDynamicRangeProfile();
	public long getDynamicRangeProfile() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public static java.util.List<android.hardware.camera2.params.OutputConfiguration> createInstancesForMultiResolutionOutput(java.util.Collection<android.hardware.camera2.params.MultiResolutionStreamInfo>, int);
	public static java.util.List<android.hardware.camera2.params.OutputConfiguration> createInstancesForMultiResolutionOutput(java.util.Collection<android.hardware.camera2.params.MultiResolutionStreamInfo> arg0, int arg1) { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void enableSurfaceSharing();
	public void enableSurfaceSharing() {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setPhysicalCameraId(java.lang.String);
	public void setPhysicalCameraId(java.lang.String arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void addSensorPixelModeUsed(int);
	public void addSensorPixelModeUsed(int arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void removeSensorPixelModeUsed(int);
	public void removeSensorPixelModeUsed(int arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void addSurface(android.view.Surface);
	public void addSurface(android.view.Surface arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void removeSurface(android.view.Surface);
	public void removeSurface(android.view.Surface arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setStreamUseCase(long);
	public void setStreamUseCase(long arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public long getStreamUseCase();
	public long getStreamUseCase() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setTimestampBase(int);
	public void setTimestampBase(int arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int getTimestampBase();
	public int getTimestampBase() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setMirrorMode(int);
	public void setMirrorMode(int arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int getMirrorMode();
	public int getMirrorMode() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void setReadoutTimestampEnabled(boolean);
	public void setReadoutTimestampEnabled(boolean arg0) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public boolean isReadoutTimestampEnabled();
	public boolean isReadoutTimestampEnabled() { return false; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int getMaxSharedSurfaceCount();
	public int getMaxSharedSurfaceCount() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public android.view.Surface getSurface();
	public android.view.Surface getSurface() { return null; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public java.util.List<android.view.Surface> getSurfaces();
	public java.util.List<android.view.Surface> getSurfaces() { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int getSurfaceGroupId();
	public int getSurfaceGroupId() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.hardware.camera2.params.OutputConfiguration:public int hashCode();
	public int hashCode() { return 0; }

	protected OutputConfiguration() {}
}
