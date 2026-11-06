package android.hardware.camera2;

// android.jar's shape, made by gen.py
public abstract class CameraCaptureSession implements java.lang.AutoCloseable {
	// GEN member:android.hardware.camera2.CameraCaptureSession:public android.hardware.camera2.CameraCaptureSession();
	public CameraCaptureSession() {}

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract android.hardware.camera2.CameraDevice getDevice();
	public abstract android.hardware.camera2.CameraDevice getDevice();

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract void prepare(android.view.Surface) throws android.hardware.camera2.CameraAccessException;
	public abstract void prepare(android.view.Surface arg0) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract void finalizeOutputConfigurations(java.util.List<android.hardware.camera2.params.OutputConfiguration>) throws android.hardware.camera2.CameraAccessException;
	public abstract void finalizeOutputConfigurations(java.util.List<android.hardware.camera2.params.OutputConfiguration> arg0) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract int capture(android.hardware.camera2.CaptureRequest, android.hardware.camera2.CameraCaptureSession$CaptureCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract int capture(android.hardware.camera2.CaptureRequest arg0, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public int captureSingleRequest(android.hardware.camera2.CaptureRequest, java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$CaptureCallback) throws android.hardware.camera2.CameraAccessException;
	public int captureSingleRequest(android.hardware.camera2.CaptureRequest arg0, java.util.concurrent.Executor arg1, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg2) throws android.hardware.camera2.CameraAccessException { return 0; }

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract int captureBurst(java.util.List<android.hardware.camera2.CaptureRequest>, android.hardware.camera2.CameraCaptureSession$CaptureCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract int captureBurst(java.util.List<android.hardware.camera2.CaptureRequest> arg0, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public int captureBurstRequests(java.util.List<android.hardware.camera2.CaptureRequest>, java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$CaptureCallback) throws android.hardware.camera2.CameraAccessException;
	public int captureBurstRequests(java.util.List<android.hardware.camera2.CaptureRequest> arg0, java.util.concurrent.Executor arg1, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg2) throws android.hardware.camera2.CameraAccessException { return 0; }

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract int setRepeatingRequest(android.hardware.camera2.CaptureRequest, android.hardware.camera2.CameraCaptureSession$CaptureCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract int setRepeatingRequest(android.hardware.camera2.CaptureRequest arg0, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public int setSingleRepeatingRequest(android.hardware.camera2.CaptureRequest, java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$CaptureCallback) throws android.hardware.camera2.CameraAccessException;
	public int setSingleRepeatingRequest(android.hardware.camera2.CaptureRequest arg0, java.util.concurrent.Executor arg1, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg2) throws android.hardware.camera2.CameraAccessException { return 0; }

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract int setRepeatingBurst(java.util.List<android.hardware.camera2.CaptureRequest>, android.hardware.camera2.CameraCaptureSession$CaptureCallback, android.os.Handler) throws android.hardware.camera2.CameraAccessException;
	public abstract int setRepeatingBurst(java.util.List<android.hardware.camera2.CaptureRequest> arg0, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg1, android.os.Handler arg2) throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public int setRepeatingBurstRequests(java.util.List<android.hardware.camera2.CaptureRequest>, java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$CaptureCallback) throws android.hardware.camera2.CameraAccessException;
	public int setRepeatingBurstRequests(java.util.List<android.hardware.camera2.CaptureRequest> arg0, java.util.concurrent.Executor arg1, android.hardware.camera2.CameraCaptureSession.CaptureCallback arg2) throws android.hardware.camera2.CameraAccessException { return 0; }

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract void stopRepeating() throws android.hardware.camera2.CameraAccessException;
	public abstract void stopRepeating() throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract void abortCaptures() throws android.hardware.camera2.CameraAccessException;
	public abstract void abortCaptures() throws android.hardware.camera2.CameraAccessException;

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract boolean isReprocessable();
	public abstract boolean isReprocessable();

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract android.view.Surface getInputSurface();
	public abstract android.view.Surface getInputSurface();

	// GEN member:android.hardware.camera2.CameraCaptureSession:public void updateOutputConfiguration(android.hardware.camera2.params.OutputConfiguration) throws android.hardware.camera2.CameraAccessException;
	public void updateOutputConfiguration(android.hardware.camera2.params.OutputConfiguration arg0) throws android.hardware.camera2.CameraAccessException {}

	// GEN member:android.hardware.camera2.CameraCaptureSession:public boolean supportsOfflineProcessing(android.view.Surface);
	public boolean supportsOfflineProcessing(android.view.Surface arg0) { return false; }

	// GEN member:android.hardware.camera2.CameraCaptureSession:public abstract void close();
	public abstract void close();

	// GEN class:android.hardware.camera2.CameraCaptureSession$CaptureCallback
	public abstract class CaptureCallback {
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public android.hardware.camera2.CameraCaptureSession$CaptureCallback();
		public CaptureCallback() {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureStarted(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, long, long);
		public void onCaptureStarted(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, long arg2, long arg3) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onReadoutStarted(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, long, long);
		public void onReadoutStarted(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, long arg2, long arg3) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureProgressed(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, android.hardware.camera2.CaptureResult);
		public void onCaptureProgressed(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, android.hardware.camera2.CaptureResult arg2) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureSequenceCompleted(android.hardware.camera2.CameraCaptureSession, int, long);
		public void onCaptureSequenceCompleted(android.hardware.camera2.CameraCaptureSession arg0, int arg1, long arg2) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureSequenceAborted(android.hardware.camera2.CameraCaptureSession, int);
		public void onCaptureSequenceAborted(android.hardware.camera2.CameraCaptureSession arg0, int arg1) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureBufferLost(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, android.view.Surface, long);
		public void onCaptureBufferLost(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, android.view.Surface arg2, long arg3) {}
	
	// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureCompleted(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, android.hardware.camera2.TotalCaptureResult);
	public void onCaptureCompleted(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, android.hardware.camera2.TotalCaptureResult arg2) {}

	// GEN member:android.hardware.camera2.CameraCaptureSession$CaptureCallback:public void onCaptureFailed(android.hardware.camera2.CameraCaptureSession, android.hardware.camera2.CaptureRequest, android.hardware.camera2.CaptureFailure);
	public void onCaptureFailed(android.hardware.camera2.CameraCaptureSession arg0, android.hardware.camera2.CaptureRequest arg1, android.hardware.camera2.CaptureFailure arg2) {}
}


	// GEN class:android.hardware.camera2.CameraCaptureSession$StateCallback
	public abstract class StateCallback {
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public android.hardware.camera2.CameraCaptureSession$StateCallback();
		public StateCallback() {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public abstract void onConfigured(android.hardware.camera2.CameraCaptureSession);
		public abstract void onConfigured(android.hardware.camera2.CameraCaptureSession arg0);
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public abstract void onConfigureFailed(android.hardware.camera2.CameraCaptureSession);
		public abstract void onConfigureFailed(android.hardware.camera2.CameraCaptureSession arg0);
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public void onReady(android.hardware.camera2.CameraCaptureSession);
		public void onReady(android.hardware.camera2.CameraCaptureSession arg0) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public void onActive(android.hardware.camera2.CameraCaptureSession);
		public void onActive(android.hardware.camera2.CameraCaptureSession arg0) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public void onCaptureQueueEmpty(android.hardware.camera2.CameraCaptureSession);
		public void onCaptureQueueEmpty(android.hardware.camera2.CameraCaptureSession arg0) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public void onClosed(android.hardware.camera2.CameraCaptureSession);
		public void onClosed(android.hardware.camera2.CameraCaptureSession arg0) {}
	
		// GEN member:android.hardware.camera2.CameraCaptureSession$StateCallback:public void onSurfacePrepared(android.hardware.camera2.CameraCaptureSession, android.view.Surface);
		public void onSurfacePrepared(android.hardware.camera2.CameraCaptureSession arg0, android.view.Surface arg1) {}
	}

}
