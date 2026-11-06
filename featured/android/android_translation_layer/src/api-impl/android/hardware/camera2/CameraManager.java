package android.hardware.camera2;

public class CameraManager {
	public String[] getCameraIdList() { return new String[] {}; }

	// GEN class:android.hardware.camera2.CameraManager$AvailabilityCallback
	public abstract class AvailabilityCallback {
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public android.hardware.camera2.CameraManager$AvailabilityCallback();
		public AvailabilityCallback() {}
	
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public void onCameraAvailable(java.lang.String);
		public void onCameraAvailable(java.lang.String arg0) {}
	
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public void onCameraUnavailable(java.lang.String);
		public void onCameraUnavailable(java.lang.String arg0) {}
	
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public void onCameraAccessPrioritiesChanged();
		public void onCameraAccessPrioritiesChanged() {}
	
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public void onPhysicalCameraAvailable(java.lang.String, java.lang.String);
		public void onPhysicalCameraAvailable(java.lang.String arg0, java.lang.String arg1) {}
	
		// GEN member:android.hardware.camera2.CameraManager$AvailabilityCallback:public void onPhysicalCameraUnavailable(java.lang.String, java.lang.String);
		public void onPhysicalCameraUnavailable(java.lang.String arg0, java.lang.String arg1) {}
	}


	// GEN member:android.hardware.camera2.CameraManager:public android.hardware.camera2.CameraCharacteristics getCameraCharacteristics(java.lang.String) throws android.hardware.camera2.CameraAccessException;
	public android.hardware.camera2.CameraCharacteristics getCameraCharacteristics(java.lang.String arg0) throws android.hardware.camera2.CameraAccessException { return null; }

	// GEN member:android.hardware.camera2.CameraManager:public android.hardware.camera2.CameraDevice$CameraDeviceSetup getCameraDeviceSetup(java.lang.String) throws android.hardware.camera2.CameraAccessException;
	public android.hardware.camera2.CameraDevice.CameraDeviceSetup getCameraDeviceSetup(java.lang.String arg0) throws android.hardware.camera2.CameraAccessException { return null; }

	// GEN member:android.hardware.camera2.CameraManager:public java.util.Set<java.util.Set<java.lang.String>> getConcurrentCameraIds() throws android.hardware.camera2.CameraAccessException;
	public java.util.Set<java.util.Set<java.lang.String>> getConcurrentCameraIds() throws android.hardware.camera2.CameraAccessException { return new java.util.HashSet<>(); }

	// GEN member:android.hardware.camera2.CameraManager:public boolean isCameraDeviceSetupSupported(java.lang.String) throws android.hardware.camera2.CameraAccessException;
	public boolean isCameraDeviceSetupSupported(java.lang.String arg0) throws android.hardware.camera2.CameraAccessException { return false; }

	// GEN member:android.hardware.camera2.CameraManager:public void openCamera(java.lang.String, android.hardware.camera2.CameraDevice$StateCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public void openCamera(java.lang.String arg0, android.hardware.camera2.CameraDevice.StateCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraManager:public void openCamera(java.lang.String, java.util.concurrent.Executor, android.hardware.camera2.CameraDevice$StateCallback) throws android.hardware.camera2.CameraAccessException;
	public void openCamera(java.lang.String arg0, java.util.concurrent.Executor arg1, android.hardware.camera2.CameraDevice.StateCallback arg2) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraManager:public void registerAvailabilityCallback(android.hardware.camera2.CameraManager$AvailabilityCallback, android.os.Handler);
	public void registerAvailabilityCallback(android.hardware.camera2.CameraManager.AvailabilityCallback arg0, android.os.Handler arg1) {}

	// GEN member:android.hardware.camera2.CameraManager:public void registerAvailabilityCallback(java.util.concurrent.Executor, android.hardware.camera2.CameraManager$AvailabilityCallback);
	public void registerAvailabilityCallback(java.util.concurrent.Executor arg0, android.hardware.camera2.CameraManager.AvailabilityCallback arg1) {}

	// GEN member:android.hardware.camera2.CameraManager:public void unregisterAvailabilityCallback(android.hardware.camera2.CameraManager$AvailabilityCallback);
	public void unregisterAvailabilityCallback(android.hardware.camera2.CameraManager.AvailabilityCallback arg0) {}

	// GEN member:android.hardware.camera2.CameraManager:public android.hardware.camera2.CameraExtensionCharacteristics getCameraExtensionCharacteristics(java.lang.String) throws android.hardware.camera2.CameraAccessException;
	public android.hardware.camera2.CameraExtensionCharacteristics getCameraExtensionCharacteristics(java.lang.String arg0) throws android.hardware.camera2.CameraAccessException { return null; }
}
