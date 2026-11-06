package android.hardware.camera2;

// android.jar's shape, made by gen.py
public final class CameraExtensionCharacteristics {
	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_AUTOMATIC = 0;
	public static final int EXTENSION_AUTOMATIC = 0;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_BEAUTY = 1;
	public static final int EXTENSION_BEAUTY = 1;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_BOKEH = 2;
	public static final int EXTENSION_BOKEH = 2;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_FACE_RETOUCH = 1;
	public static final int EXTENSION_FACE_RETOUCH = 1;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_HDR = 3;
	public static final int EXTENSION_HDR = 3;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public static final int EXTENSION_NIGHT = 4;
	public static final int EXTENSION_NIGHT = 4;

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.List<java.lang.Integer> getSupportedExtensions();
	public java.util.List<java.lang.Integer> getSupportedExtensions() { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public <T> T get(int, android.hardware.camera2.CameraCharacteristics$Key<T>);
	public <T> T get(int arg0, android.hardware.camera2.CameraCharacteristics.Key<T> arg1) { return null; }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.Set<android.hardware.camera2.CameraCharacteristics$Key> getKeys(int);
	public java.util.Set<android.hardware.camera2.CameraCharacteristics.Key> getKeys(int arg0) { return new java.util.HashSet<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public boolean isPostviewAvailable(int);
	public boolean isPostviewAvailable(int arg0) { return false; }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.List<android.util.Size> getPostviewSupportedSizes(int, android.util.Size, int);
	public java.util.List<android.util.Size> getPostviewSupportedSizes(int arg0, android.util.Size arg1, int arg2) { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public <T> java.util.List<android.util.Size> getExtensionSupportedSizes(int, java.lang.Class<T>);
	public <T> java.util.List<android.util.Size> getExtensionSupportedSizes(int arg0, java.lang.Class<T> arg1) { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.List<android.util.Size> getExtensionSupportedSizes(int, int);
	public java.util.List<android.util.Size> getExtensionSupportedSizes(int arg0, int arg1) { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public android.util.Range<java.lang.Long> getEstimatedCaptureLatencyRangeMillis(int, android.util.Size, int);
	public android.util.Range<java.lang.Long> getEstimatedCaptureLatencyRangeMillis(int arg0, android.util.Size arg1, int arg2) { return null; }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public boolean isCaptureProcessProgressAvailable(int);
	public boolean isCaptureProcessProgressAvailable(int arg0) { return false; }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.Set<android.hardware.camera2.CaptureRequest$Key> getAvailableCaptureRequestKeys(int);
	public java.util.Set<android.hardware.camera2.CaptureRequest.Key> getAvailableCaptureRequestKeys(int arg0) { return new java.util.HashSet<>(); }

	// GEN member:android.hardware.camera2.CameraExtensionCharacteristics:public java.util.Set<android.hardware.camera2.CaptureResult$Key> getAvailableCaptureResultKeys(int);
	public java.util.Set<android.hardware.camera2.CaptureResult.Key> getAvailableCaptureResultKeys(int arg0) { return new java.util.HashSet<>(); }

	protected CameraExtensionCharacteristics() {}
}
