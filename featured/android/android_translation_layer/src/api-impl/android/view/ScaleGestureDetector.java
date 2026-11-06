package android.view;

import android.content.Context;

public class ScaleGestureDetector {

	public interface OnScaleGestureListener {
		boolean onScale(ScaleGestureDetector detector);
		boolean onScaleBegin(ScaleGestureDetector detector);
		void onScaleEnd(ScaleGestureDetector detector);
	}

	public ScaleGestureDetector(Context context, OnScaleGestureListener listener) {}

	public void setQuickScaleEnabled(boolean enabled) {}
	public void setStylusScaleEnabled(boolean enabled) {}

	public boolean onTouchEvent(MotionEvent event) {
		return false;
	}

	public boolean isInProgress() {
		return false;
	}

	public static class SimpleOnScaleGestureListener implements OnScaleGestureListener {
		public SimpleOnScaleGestureListener() {
		}

		public boolean onScale(ScaleGestureDetector detector) {
			return false;
		}
		public boolean onScaleBegin(ScaleGestureDetector detector) {
			return true;
		}
		public void onScaleEnd(ScaleGestureDetector detector) {
		}
	}

	// GEN member:android.view.ScaleGestureDetector:public float getFocusX();
	public float getFocusX() { return 0; }

	// GEN member:android.view.ScaleGestureDetector:public float getFocusY();
	public float getFocusY() { return 0; }

	// GEN member:android.view.ScaleGestureDetector:public float getScaleFactor();
	public float getScaleFactor() { return 0; }

	// GEN member:android.view.ScaleGestureDetector:public long getTimeDelta();
	public long getTimeDelta() { return 0; }
}
