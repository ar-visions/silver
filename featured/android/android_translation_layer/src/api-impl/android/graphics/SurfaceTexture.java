package android.graphics;

public class SurfaceTexture {

	// the TextureView's widget this surface draws into; 0 for one the app made
	public long widget;

	public static interface OnFrameAvailableListener {
		void onFrameAvailable(SurfaceTexture surfaceTexture);
	}

	public SurfaceTexture(int texName) {}

	public SurfaceTexture(int texName, boolean singleBufferMode) {}

	public boolean isReleased() { return false; }

	public void attachToGLContext(int texName) {}

	public void setOnFrameAvailableListener(OnFrameAvailableListener listener) {}

	public void release() {}

	// GEN member:android.graphics.SurfaceTexture:public void detachFromGLContext();
	public void detachFromGLContext() {}

	// GEN member:android.graphics.SurfaceTexture:public long getTimestamp();
	public long getTimestamp() { return 0; }

	// GEN member:android.graphics.SurfaceTexture:public void getTransformMatrix(float[]);
	public void getTransformMatrix(float[] mtx) {
		for (int i = 0; i < 16; i++)
			mtx[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}

	// GEN member:android.graphics.SurfaceTexture:public void setDefaultBufferSize(int, int);
	public void setDefaultBufferSize(int arg0, int arg1) {}

	// GEN member:android.graphics.SurfaceTexture:public void setOnFrameAvailableListener(android.graphics.SurfaceTexture$OnFrameAvailableListener, android.os.Handler);
	public void setOnFrameAvailableListener(android.graphics.SurfaceTexture.OnFrameAvailableListener arg0, android.os.Handler arg1) {}

	// GEN member:android.graphics.SurfaceTexture:public void updateTexImage();
	public void updateTexImage() {}

	// GEN member:android.graphics.SurfaceTexture:public android.graphics.SurfaceTexture(boolean);
	public SurfaceTexture(boolean arg0) { this((int)0); }
}
