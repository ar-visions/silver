package android.view;

import android.os.IBinder;

public interface WindowManager extends android.view.ViewManager {
	public android.view.Display getDefaultDisplay();

	public void addView(View view, ViewGroup.LayoutParams params);

	public void updateViewLayout(View view, ViewGroup.LayoutParams params);

	public void removeView(View view);

	public void removeViewImmediate(View view);

	public class LayoutParams extends ViewGroup.LayoutParams implements android.os.Parcelable {
		public static final int FLAG_KEEP_SCREEN_ON = 0;
		public static final int FLAG_DIM_BEHIND = 2;
		public static final int FLAG_NOT_FOCUSABLE = 8;

		public float screenBrightness = -1;
		public int softInputMode;
		public int x;
		public int y;
		public int windowAnimations;
		public int flags;
		public float alpha;
		public int type;
		public IBinder token;
		public int format;
		public int layoutInDisplayCutoutMode;
		public String packageName;
		public int rotationAnimation;

		public LayoutParams(int w, int h, int type, int flags, int format) {
			super(w, h);
		}

		public LayoutParams(int type) {
			super(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
			this.type = type;
		}

		public LayoutParams() {}

		public void setTitle(CharSequence title) {}
	
	// GEN member:android.view.WindowManager$LayoutParams:public final int copyFrom(android.view.WindowManager$LayoutParams);
	public final int copyFrom(android.view.WindowManager.LayoutParams arg0) { return 0; }

	// GEN member:android.view.WindowManager$LayoutParams:public float dimAmount;
	public float dimAmount;

	// GEN member:android.view.WindowManager$LayoutParams:public android.view.WindowManager$LayoutParams(int, int, int, int, int, int, int);
	public LayoutParams(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {}

	// GEN member:android.view.WindowManager$LayoutParams:public void setBlurBehindRadius(int);
	public void setBlurBehindRadius(int arg0) {}

	// GEN member:android.view.WindowManager$LayoutParams:public float preferredRefreshRate;
	public float preferredRefreshRate;

	// GEN member:android.view.WindowManager$LayoutParams:public int systemUiVisibility;
	public int systemUiVisibility;
}

	// GEN member:android.view.WindowManager:public default int addScreenRecordingCallback(java.util.concurrent.Executor, java.util.function.Consumer<java.lang.Integer>);
	default int addScreenRecordingCallback(java.util.concurrent.Executor arg0, java.util.function.Consumer<java.lang.Integer> arg1) { return 0; }

	// GEN member:android.view.WindowManager:public default android.view.WindowMetrics getCurrentWindowMetrics();
	default android.view.WindowMetrics getCurrentWindowMetrics() { return null; }

	// GEN member:android.view.WindowManager:public default android.view.WindowMetrics getMaximumWindowMetrics();
	default android.view.WindowMetrics getMaximumWindowMetrics() { return null; }

	// GEN member:android.view.WindowManager:public default void removeScreenRecordingCallback(java.util.function.Consumer<java.lang.Integer>);
	default void removeScreenRecordingCallback(java.util.function.Consumer<java.lang.Integer> arg0) {}

	// GEN member:android.view.WindowManager:public default void addCrossWindowBlurEnabledListener(java.util.concurrent.Executor, java.util.function.Consumer<java.lang.Boolean>);
	default void addCrossWindowBlurEnabledListener(java.util.concurrent.Executor arg0, java.util.function.Consumer<java.lang.Boolean> arg1) {}

	// GEN member:android.view.WindowManager:public default void addCrossWindowBlurEnabledListener(java.util.function.Consumer<java.lang.Boolean>);
	default void addCrossWindowBlurEnabledListener(java.util.function.Consumer<java.lang.Boolean> arg0) {}

	// GEN member:android.view.WindowManager:public default boolean isCrossWindowBlurEnabled();
	default boolean isCrossWindowBlurEnabled() { return false; }

	// GEN member:android.view.WindowManager:public default void removeCrossWindowBlurEnabledListener(java.util.function.Consumer<java.lang.Boolean>);
	default void removeCrossWindowBlurEnabledListener(java.util.function.Consumer<java.lang.Boolean> arg0) {}

	// GEN class:android.view.WindowManager$BadTokenException
	public class BadTokenException extends java.lang.RuntimeException {
		// GEN member:android.view.WindowManager$BadTokenException:public android.view.WindowManager$BadTokenException();
		public BadTokenException() {}
	
		// GEN member:android.view.WindowManager$BadTokenException:public android.view.WindowManager$BadTokenException(java.lang.String);
		public BadTokenException(java.lang.String arg0) {}
	}

}
