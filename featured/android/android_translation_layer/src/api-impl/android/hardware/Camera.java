package android.hardware;

public class Camera {

	public interface PreviewCallback {}

	public interface AutoFocusCallback {}

	public interface ErrorCallback {}

	public static int getNumberOfCameras() { return 0; }

	public static Camera open() {
		return null;
	}

	public static Camera open(int cameraId) {
		return null;
	}

	public void setErrorCallback(ErrorCallback callback) {}

	// GEN class:android.hardware.Camera$CameraInfo
	public class CameraInfo {
		// GEN member:android.hardware.Camera$CameraInfo:public static final int CAMERA_FACING_BACK = 0;
		public static final int CAMERA_FACING_BACK = 0;
	
		// GEN member:android.hardware.Camera$CameraInfo:public static final int CAMERA_FACING_FRONT = 1;
		public static final int CAMERA_FACING_FRONT = 1;
	
		// GEN member:android.hardware.Camera$CameraInfo:public boolean canDisableShutterSound;
		public boolean canDisableShutterSound;
	
		// GEN member:android.hardware.Camera$CameraInfo:public int facing;
		public int facing;
	
		// GEN member:android.hardware.Camera$CameraInfo:public int orientation;
		public int orientation;
	
		// GEN member:android.hardware.Camera$CameraInfo:public android.hardware.Camera$CameraInfo();
		public CameraInfo() {}
	}


	// GEN member:android.hardware.Camera:public static void getCameraInfo(int, android.hardware.Camera$CameraInfo);
	public static void getCameraInfo(int arg0, android.hardware.Camera.CameraInfo arg1) {}
}
