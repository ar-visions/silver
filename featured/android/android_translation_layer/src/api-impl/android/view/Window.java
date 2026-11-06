package android.view;

import android.R;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.transition.Transition;
import android.widget.FrameLayout;
import android.widget.Toolbar;

public class Window {
	public static final int FEATURE_OPTIONS_PANEL = 0;
	public static final int FEATURE_NO_TITLE = 1;

	public ViewTreeObserver view_tree_observer = null;
	private WindowManager.LayoutParams params = new WindowManager.LayoutParams(WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT, 0, 0, 0);

	public static interface Callback {
		public void onContentChanged();

		public abstract boolean onCreatePanelMenu(int featureId, Menu menu);

		public View onCreatePanelView(int featureId);

		public boolean onPreparePanel(int featureId, View view, Menu menu);

		public boolean onMenuItemSelected(int featureId, MenuItem item);

		public void onPanelClosed(int featureId, Menu menu);

		public boolean onMenuOpened(int featureId, Menu menu);

		// the rest of Android's callback, defaults for the ones not given
		default boolean dispatchKeyEvent(android.view.KeyEvent event) { return false; }
		default boolean dispatchKeyShortcutEvent(android.view.KeyEvent event) { return false; }
		default boolean dispatchTouchEvent(MotionEvent event) { return false; }
		default boolean dispatchTrackballEvent(MotionEvent event) { return false; }
		default boolean dispatchGenericMotionEvent(MotionEvent event) { return false; }
		default void onWindowAttributesChanged(WindowManager.LayoutParams attrs) {}
		default void onWindowFocusChanged(boolean hasFocus) {}
		default void onAttachedToWindow() {}
		default void onDetachedFromWindow() {}
		default boolean onSearchRequested() { return false; }
		default void onActionModeStarted(ActionMode mode) {}
		default void onActionModeFinished(ActionMode mode) {}
		default void onPointerCaptureChanged(boolean hasCapture) {}
	
	// GEN member:android.view.Window$Callback:public abstract boolean dispatchPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent);
	default boolean dispatchPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent arg0) { return false; }

	// GEN member:android.view.Window$Callback:public default void onProvideKeyboardShortcuts(java.util.List<android.view.KeyboardShortcutGroup>, android.view.Menu, int);
	default void onProvideKeyboardShortcuts(java.util.List<android.view.KeyboardShortcutGroup> arg0, android.view.Menu arg1, int arg2) {}

	// GEN member:android.view.Window$Callback:public abstract boolean onSearchRequested(android.view.SearchEvent);
	default boolean onSearchRequested(android.view.SearchEvent arg0) { return false; }

	// GEN member:android.view.Window$Callback:public abstract android.view.ActionMode onWindowStartingActionMode(android.view.ActionMode$Callback, int);
	default android.view.ActionMode onWindowStartingActionMode(android.view.ActionMode.Callback arg0, int arg1) { return null; }

	// GEN member:android.view.Window$Callback:public abstract android.view.ActionMode onWindowStartingActionMode(android.view.ActionMode$Callback);
	default android.view.ActionMode onWindowStartingActionMode(android.view.ActionMode.Callback arg0) { return null; }
}

	public static interface OnFrameMetricsAvailableListener {}

	public long native_window;
	private ViewGroup decorView;

	private Window.Callback callback;
	private Context context;

	public Window(Context context, Window.Callback callback) {
		this.callback = callback;
		this.context = context;
		decorView = new FrameLayout(context);
		// DecorView grabs all touch events and propagates them in Java
		decorView.native_registerTouchEventPropagation(decorView.widget);
		decorView.setId(android.R.id.content);
	}

	public void set_native_window(long native_window) {
		this.native_window = native_window;
		set_jobject(native_window, this);
	}

	public void addFlags(int flags) {}
	public void setFlags(int flags, int mask) {}
	public void clearFlags(int flags) {}

	public final Callback getCallback() {
		return this.callback;
	}
	public void setCallback(Window.Callback callback) {
		this.callback = callback;
	}

	public void setContentView(View view) {
		if (decorView.getBackground() == null) {
			TypedArray ta = context.obtainStyledAttributes(new int[] {R.attr.windowBackground});
			if (ta.hasValue(0))
				setBackgroundDrawable(ta.getDrawable(0));
			ta.recycle();
		}
		decorView.removeAllViews();
		decorView.addView(view);
	}

	public View getDecorView() {
		return decorView;
	}

	public void takeInputQueue(InputQueue.Callback callback) {
		take_input_queue(native_window, callback, new InputQueue());
	}

	public boolean requestFeature(int featureId) {
		return false;
	}

	public View findViewById(int id) {
		if (id == com.android.internal.R.id.action_bar)
			return new Toolbar(context);
		return decorView.findViewById(id);
	}

	public View peekDecorView() {
		return null;
	}

	public WindowManager.LayoutParams getAttributes() {
		return params;
	}

	public void setBackgroundDrawable(Drawable drawable) {
		// HACK: disable transparent background for WhatsApp dialogs
		// For some unknown reason, the language picker in WhatsApp doesn't render the BottomSheet background currently.
		if (!"com.whatsapp".equals(context.getPackageName()))
			remove_gtk_background(native_window);
		decorView.setBackgroundDrawable(drawable);
	}

	public void setAttributes(WindowManager.LayoutParams params) {
		if (params.screenBrightness != -1)
			set_screen_brightness(params.screenBrightness);
		this.params = params;
		setLayout(params.width, params.height);
	}

	public void takeSurface(SurfaceHolder.Callback2 callback) {}

	public void setStatusBarColor(int color) {}

	public void setNavigationBarColor(int color) {}

	public void setFormat(int format) {}

	public void setLayout(int width, int height) {
		params.width = width;
		params.height = height;
		set_layout(native_window, width, height);
	}

	public WindowManager getWindowManager() {
		return new WindowManagerImpl();
	}

	public void setSoftInputMode(int dummy) {}

	public int getNavigationBarColor() {
		return 0xFF888888; // gray
	}

	public void setBackgroundDrawableResource(int resId) {
		setBackgroundDrawable(context.getDrawable(resId));
	}

	public int getStatusBarColor() { return 0xFFFF0000; }

	public Context getContext() {
		return context;
	}

	public boolean hasFeature(int featureId) {
		return false;
	}

	public void setTitle(CharSequence title) {
		set_title(native_window, title != null ? title.toString() : context.getPackageName());
	}

	public Transition getSharedElementEnterTransition() {
		return new Transition();
	}

	public void setSharedElementExitTransition(Transition transition) {}

	public void setSharedElementReenterTransition(Transition transition) {}

	public void setSharedElementReturnTransition(Transition transition) {}

	public Transition getSharedElementExitTransition() {
		return new Transition();
	}

	public Transition getSharedElementReenterTransition() {
		return new Transition();
	}

	public void setReturnTransition(Transition transition) {}

	public void setEnterTransition(Transition transition) {}

	public void setGravity(int gravity) {}

	public void setDecorFitsSystemWindows(boolean fits) {}

	public WindowInsetsController getInsetsController() {
		return new InsetsController();
	}

	public void setStatusBarContrastEnforced(boolean enforced) {}

	public void setNavigationBarContrastEnforced(boolean enforced) {}

	public void setSharedElementEnterTransition(Transition transition) {}

	public Transition getEnterTransition() {
		return null;
	}

	public void setWindowAnimations(int resId) {}

	public void setDimAmount(float dimAmount) {}

	private native void set_title(long native_window, String title);
	public native void take_input_queue(long native_window, InputQueue.Callback callback, InputQueue queue);
	public native void set_layout(long native_window, int width, int height);
	private static native void set_jobject(long ptr, Window obj);
	private native void remove_gtk_background(long native_window);
	private native void set_screen_brightness(float brightness);

	public boolean superDispatchKeyEvent(android.view.KeyEvent event) {
		return false;
	}

	public android.os.Bundle saveHierarchyState() {
		return new android.os.Bundle();
	}

	public void restoreHierarchyState(android.os.Bundle state) {}

	// GEN member:android.view.Window:public final void addOnFrameMetricsAvailableListener(android.view.Window$OnFrameMetricsAvailableListener, android.os.Handler);
	public final void addOnFrameMetricsAvailableListener(android.view.Window.OnFrameMetricsAvailableListener arg0, android.os.Handler arg1) {}

	// GEN member:android.view.Window:public final void removeOnFrameMetricsAvailableListener(android.view.Window$OnFrameMetricsAvailableListener);
	public final void removeOnFrameMetricsAvailableListener(android.view.Window.OnFrameMetricsAvailableListener arg0) {}

	// GEN member:android.view.Window:public void setBackgroundBlurRadius(int);
	public void setBackgroundBlurRadius(int arg0) {}

	// GEN member:android.view.Window:public void setType(int);
	public void setType(int arg0) {}

	// GEN member:android.view.Window:public android.view.Window(android.content.Context);
	public Window(android.content.Context arg0) { this((android.content.Context)null, (android.view.Window.Callback)null); }

	// GEN member:android.view.Window:public abstract android.view.View getCurrentFocus();
	public android.view.View getCurrentFocus() { return null; }

	// GEN member:android.view.Window:public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher();
	public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() { return null; }

	// GEN member:android.view.Window:public void injectInputEvent(android.view.InputEvent);
	public void injectInputEvent(android.view.InputEvent arg0) {}

	// GEN member:android.view.Window:public abstract boolean isFloating();
	public boolean isFloating() { return false; }

	// GEN member:android.view.Window:public void setLocalFocus(boolean, boolean);
	public void setLocalFocus(boolean arg0, boolean arg1) {}

	// GEN member:android.view.Window:public void setNavigationBarDividerColor(int);
	public void setNavigationBarDividerColor(int arg0) {}

	// GEN member:android.view.Window:public abstract boolean superDispatchGenericMotionEvent(android.view.MotionEvent);
	public boolean superDispatchGenericMotionEvent(android.view.MotionEvent arg0) { return false; }

	// GEN member:android.view.Window:public abstract boolean superDispatchKeyShortcutEvent(android.view.KeyEvent);
	public boolean superDispatchKeyShortcutEvent(android.view.KeyEvent arg0) { return false; }

	// GEN member:android.view.Window:public abstract boolean superDispatchTouchEvent(android.view.MotionEvent);
	public boolean superDispatchTouchEvent(android.view.MotionEvent arg0) { return false; }
}
