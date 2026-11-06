package android.view;

import android.graphics.SurfaceTexture;

public class Surface implements android.os.Parcelable {
	public long widget;

	public Surface() {}

	public Surface(SurfaceTexture surfaceTexture) {
		widget = surfaceTexture.widget;
	}

	public boolean isValid() {
		return widget != 0;
	}

	public void release() {}

	// GEN member:android.view.Surface:public android.graphics.Canvas lockHardwareCanvas();
	public android.graphics.Canvas lockHardwareCanvas() { return null; }

	// GEN member:android.view.Surface:public void setFrameRate(float, int);
	public void setFrameRate(float arg0, int arg1) {}

	// GEN member:android.view.Surface:public void unlockCanvasAndPost(android.graphics.Canvas);
	public void unlockCanvasAndPost(android.graphics.Canvas arg0) {}

	// GEN member:android.view.Surface:public static final android.os.Parcelable$Creator<android.view.Surface> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.Surface> CREATOR = null;
}
