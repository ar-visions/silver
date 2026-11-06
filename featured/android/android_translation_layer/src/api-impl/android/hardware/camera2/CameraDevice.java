package android.hardware.camera2;

// android.jar's shape, made by gen.py
public abstract class CameraDevice implements java.lang.AutoCloseable {
	// GEN member:android.hardware.camera2.CameraDevice:public static final int AUDIO_RESTRICTION_NONE = 0;
	public static final int AUDIO_RESTRICTION_NONE = 0;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int AUDIO_RESTRICTION_VIBRATION = 1;
	public static final int AUDIO_RESTRICTION_VIBRATION = 1;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int AUDIO_RESTRICTION_VIBRATION_SOUND = 3;
	public static final int AUDIO_RESTRICTION_VIBRATION_SOUND = 3;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_MANUAL = 6;
	public static final int TEMPLATE_MANUAL = 6;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_PREVIEW = 1;
	public static final int TEMPLATE_PREVIEW = 1;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_RECORD = 3;
	public static final int TEMPLATE_RECORD = 3;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_STILL_CAPTURE = 2;
	public static final int TEMPLATE_STILL_CAPTURE = 2;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_VIDEO_SNAPSHOT = 4;
	public static final int TEMPLATE_VIDEO_SNAPSHOT = 4;

	// GEN member:android.hardware.camera2.CameraDevice:public static final int TEMPLATE_ZERO_SHUTTER_LAG = 5;
	public static final int TEMPLATE_ZERO_SHUTTER_LAG = 5;

	// GEN member:android.hardware.camera2.CameraDevice:public abstract java.lang.String getId();
	public abstract java.lang.String getId();

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void createCaptureSession(java.util.List<android.view.Surface>, android.hardware.camera2.CameraCaptureSession$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract void createCaptureSession(java.util.List<android.view.Surface> arg0, android.hardware.camera2.CameraCaptureSession.StateCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void createCaptureSessionByOutputConfigurations(java.util.List<android.hardware.camera2.params.OutputConfiguration>, android.hardware.camera2.CameraCaptureSession$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract void createCaptureSessionByOutputConfigurations(java.util.List<android.hardware.camera2.params.OutputConfiguration> arg0, android.hardware.camera2.CameraCaptureSession.StateCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void createReprocessableCaptureSession(android.hardware.camera2.params.InputConfiguration, java.util.List<android.view.Surface>, android.hardware.camera2.CameraCaptureSession$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract void createReprocessableCaptureSession(android.hardware.camera2.params.InputConfiguration arg0, java.util.List<android.view.Surface> arg1, android.hardware.camera2.CameraCaptureSession.StateCallback arg2, android.os.Handler arg3) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void createReprocessableCaptureSessionByConfigurations(android.hardware.camera2.params.InputConfiguration, java.util.List<android.hardware.camera2.params.OutputConfiguration>, android.hardware.camera2.CameraCaptureSession$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract void createReprocessableCaptureSessionByConfigurations(android.hardware.camera2.params.InputConfiguration arg0, java.util.List<android.hardware.camera2.params.OutputConfiguration> arg1, android.hardware.camera2.CameraCaptureSession.StateCallback arg2, android.os.Handler arg3) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void createConstrainedHighSpeedCaptureSession(java.util.List<android.view.Surface>, android.hardware.camera2.CameraCaptureSession$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract void createConstrainedHighSpeedCaptureSession(java.util.List<android.view.Surface> arg0, android.hardware.camera2.CameraCaptureSession.StateCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public void createCaptureSession(android.hardware.camera2.params.SessionConfiguration) throws android.hardware.camera2.CameraAccessException;
	public void createCaptureSession(android.hardware.camera2.params.SessionConfiguration arg0) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraDevice:public abstract android.hardware.camera2.CaptureRequest$Builder createCaptureRequest(int) throws android.hardware.camera2.CameraAccessException;
	public abstract android.hardware.camera2.CaptureRequest.Builder createCaptureRequest(int arg0) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraDevice:public android.hardware.camera2.CaptureRequest$Builder createCaptureRequest(int, java.util.Set<java.lang.String>) throws android.hardware.camera2.CameraAccessException;
	public android.hardware.camera2.CaptureRequest.Builder createCaptureRequest(int arg0, java.util.Set<java.lang.String> arg1) throws android.hardware.camera2.CameraAccessException { return null; }

	// GEN member:android.hardware.camera2.CameraDevice:public abstract void close();
	public abstract void close();

	// GEN member:android.hardware.camera2.CameraDevice:public boolean isSessionConfigurationSupported(android.hardware.camera2.params.SessionConfiguration) throws android.hardware.camera2.CameraAccessException;
	public boolean isSessionConfigurationSupported(android.hardware.camera2.params.SessionConfiguration arg0) throws android.hardware.camera2.CameraAccessException { return false; }

	// GEN member:android.hardware.camera2.CameraDevice:public void setCameraAudioRestriction(int) throws android.hardware.camera2.CameraAccessException;
	public void setCameraAudioRestriction(int arg0) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraDevice:public int getCameraAudioRestriction() throws android.hardware.camera2.CameraAccessException;
	public int getCameraAudioRestriction() throws android.hardware.camera2.CameraAccessException { return 0; }

	protected CameraDevice() {}

	// GEN class:android.hardware.camera2.CameraDevice$CameraDeviceSetup
	public abstract class CameraDeviceSetup {
		// GEN member:android.hardware.camera2.CameraDevice$CameraDeviceSetup:public abstract android.hardware.camera2.CaptureRequest$Builder createCaptureRequest(int) throws android.hardware.camera2.CameraAccessException;
		public abstract android.hardware.camera2.CaptureRequest.Builder createCaptureRequest(int arg0) throws android.hardware.camera2.CameraAccessException;
	
		// GEN member:android.hardware.camera2.CameraDevice$CameraDeviceSetup:public abstract boolean isSessionConfigurationSupported(android.hardware.camera2.params.SessionConfiguration) throws android.hardware.camera2.CameraAccessException;
		public abstract boolean isSessionConfigurationSupported(android.hardware.camera2.params.SessionConfiguration arg0) throws android.hardware.camera2.CameraAccessException;
	
		// GEN member:android.hardware.camera2.CameraDevice$CameraDeviceSetup:public abstract android.hardware.camera2.CameraCharacteristics getSessionCharacteristics(android.hardware.camera2.params.SessionConfiguration) throws android.hardware.camera2.CameraAccessException;
		public abstract android.hardware.camera2.CameraCharacteristics getSessionCharacteristics(android.hardware.camera2.params.SessionConfiguration arg0) throws android.hardware.camera2.CameraAccessException;
	
		// GEN member:android.hardware.camera2.CameraDevice$CameraDeviceSetup:public abstract void openCamera(java.util.concurrent.Executor, android.hardware.camera2.CameraDevice$StateCallback) throws android.hardware.camera2.CameraAccessException;
		public abstract void openCamera(java.util.concurrent.Executor arg0, android.hardware.camera2.CameraDevice.StateCallback arg1) throws android.hardware.camera2.CameraAccessException;
	
		// GEN member:android.hardware.camera2.CameraDevice$CameraDeviceSetup:public abstract java.lang.String getId();
		public abstract java.lang.String getId();
	
		protected CameraDeviceSetup() {}
	}


	// GEN class:android.hardware.camera2.CameraDevice$StateCallback
	public abstract class StateCallback {
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public static final int ERROR_CAMERA_DEVICE = 4;
		public static final int ERROR_CAMERA_DEVICE = 4;
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public static final int ERROR_CAMERA_DISABLED = 3;
		public static final int ERROR_CAMERA_DISABLED = 3;
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public static final int ERROR_CAMERA_IN_USE = 1;
		public static final int ERROR_CAMERA_IN_USE = 1;
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public static final int ERROR_CAMERA_SERVICE = 5;
		public static final int ERROR_CAMERA_SERVICE = 5;
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public static final int ERROR_MAX_CAMERAS_IN_USE = 2;
		public static final int ERROR_MAX_CAMERAS_IN_USE = 2;
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public android.hardware.camera2.CameraDevice$StateCallback();
		public StateCallback() {}
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public abstract void onOpened(android.hardware.camera2.CameraDevice);
		public abstract void onOpened(android.hardware.camera2.CameraDevice arg0);
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public void onClosed(android.hardware.camera2.CameraDevice);
		public void onClosed(android.hardware.camera2.CameraDevice arg0) {}
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public abstract void onDisconnected(android.hardware.camera2.CameraDevice);
		public abstract void onDisconnected(android.hardware.camera2.CameraDevice arg0);
	
		// GEN member:android.hardware.camera2.CameraDevice$StateCallback:public abstract void onError(android.hardware.camera2.CameraDevice, int);
		public abstract void onError(android.hardware.camera2.CameraDevice arg0, int arg1);
	}


	// GEN member:android.hardware.camera2.CameraDevice:public void createExtensionSession(android.hardware.camera2.params.ExtensionSessionConfiguration) throws android.hardware.camera2.CameraAccessException;
	public void createExtensionSession(android.hardware.camera2.params.ExtensionSessionConfiguration arg0) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraDevice:public abstract android.hardware.camera2.CaptureRequest$Builder createReprocessCaptureRequest(android.hardware.camera2.TotalCaptureResult) throws android.hardware.camera2.CameraAccessException;
	public android.hardware.camera2.CaptureRequest.Builder createReprocessCaptureRequest(android.hardware.camera2.TotalCaptureResult arg0) throws android.hardware.camera2.CameraAccessException { return null; }
}
