package android.hardware.camera2.params;

// android.jar's shape, made by gen.py
public final class SessionConfiguration implements android.os.Parcelable {
	// GEN member:android.hardware.camera2.params.SessionConfiguration:public static final android.os.Parcelable$Creator<android.hardware.camera2.params.SessionConfiguration> CREATOR;
	public static final android.os.Parcelable.Creator<android.hardware.camera2.params.SessionConfiguration> CREATOR = null;

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public static final int SESSION_HIGH_SPEED = 1;
	public static final int SESSION_HIGH_SPEED = 1;

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public static final int SESSION_REGULAR = 0;
	public static final int SESSION_REGULAR = 0;

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.hardware.camera2.params.SessionConfiguration(int, java.util.List<android.hardware.camera2.params.OutputConfiguration>, java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$StateCallback);
	public SessionConfiguration(int arg0, java.util.List<android.hardware.camera2.params.OutputConfiguration> arg1, java.util.concurrent.Executor arg2, android.hardware.camera2.CameraCaptureSession.StateCallback arg3) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.hardware.camera2.params.SessionConfiguration(int, java.util.List<android.hardware.camera2.params.OutputConfiguration>);
	public SessionConfiguration(int arg0, java.util.List<android.hardware.camera2.params.OutputConfiguration> arg1) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public int getSessionType();
	public int getSessionType() { return 0; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public java.util.List<android.hardware.camera2.params.OutputConfiguration> getOutputConfigurations();
	public java.util.List<android.hardware.camera2.params.OutputConfiguration> getOutputConfigurations() { return new java.util.ArrayList<>(); }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.hardware.camera2.CameraCaptureSession$StateCallback getStateCallback();
	public android.hardware.camera2.CameraCaptureSession.StateCallback getStateCallback() { return null; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public java.util.concurrent.Executor getExecutor();
	public java.util.concurrent.Executor getExecutor() { return null; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void setInputConfiguration(android.hardware.camera2.params.InputConfiguration);
	public void setInputConfiguration(android.hardware.camera2.params.InputConfiguration arg0) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.hardware.camera2.params.InputConfiguration getInputConfiguration();
	public android.hardware.camera2.params.InputConfiguration getInputConfiguration() { return null; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void setSessionParameters(android.hardware.camera2.CaptureRequest);
	public void setSessionParameters(android.hardware.camera2.CaptureRequest arg0) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.hardware.camera2.CaptureRequest getSessionParameters();
	public android.hardware.camera2.CaptureRequest getSessionParameters() { return null; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void setColorSpace(android.graphics.ColorSpace$Named);
	public void setColorSpace(android.graphics.ColorSpace.Named arg0) {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void clearColorSpace();
	public void clearColorSpace() {}

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public android.graphics.ColorSpace getColorSpace();
	public android.graphics.ColorSpace getColorSpace() { return null; }

	// GEN member:android.hardware.camera2.params.SessionConfiguration:public void setStateCallback(java.util.concurrent.Executor, android.hardware.camera2.CameraCaptureSession$StateCallback);
	public void setStateCallback(java.util.concurrent.Executor arg0, android.hardware.camera2.CameraCaptureSession.StateCallback arg1) {}

	protected SessionConfiguration() {}
}
