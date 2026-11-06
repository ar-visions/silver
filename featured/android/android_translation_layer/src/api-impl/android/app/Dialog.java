package android.app;

import android.R;
import android.atl.annotation.Export;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

public class Dialog implements Window.Callback, DialogInterface, android.view.KeyEvent.Callback, android.view.View.OnCreateContextMenuListener {
	protected long nativePtr;

	protected native long nativeInit(long decor_view);
	private native void nativeSetTitle(long ptr, String title);
	private native void nativeSetContentView(long ptr, long widget);
	private native void nativeShow(long ptr);
	private native void nativeClose(long ptr);
	private native boolean nativeIsShowing(long ptr);
	private native void nativeSetCanceledOnTouchOutside(long ptr, boolean cancel);

	private Context context;
	private Window window;
	private OnDismissListener onDismissListener;
	private OnShowListener onShowListener;

	public Dialog(Context context, int themeResId) {
		// the dialog's own theme, as Android's: a sheet's says its
		// window is clear, the activity's would paint it opaque
		if (themeResId == 0) {
			android.util.TypedValue value = new android.util.TypedValue();
			if (context.getTheme().resolveAttribute(R.attr.dialogTheme, value, true))
				themeResId = value.resourceId;
		}
		if (themeResId != 0)
			context = new android.view.ContextThemeWrapper(context, themeResId);
		this.context = context;
		window = new Window(context, this);
		nativePtr = nativeInit(window.getDecorView().widget);

		window.set_native_window(nativePtr);
	}

	public Dialog(Context context) {
		this(context, 0);
	}

	public final boolean requestWindowFeature(int featureId) {
		return false;
	}

	public Context getContext() {
		return context;
	}

	public void setContentView(View view) {
		getWindow().setContentView(view);
	}

	public void setContentView(int layoutResId) {
		setContentView(LayoutInflater.from(context).inflate(layoutResId, null));
	}

	public void setContentView(View view, ViewGroup.LayoutParams lp) {
		view.setLayoutParams(lp);
		setContentView(view);
	}

	public void setTitle(CharSequence title) {
		nativeSetTitle(nativePtr, String.valueOf(title));
	}

	public void setTitle(int titleId) {
		nativeSetTitle(nativePtr, context.getString(titleId));
	}

	public void setOwnerActivity(Activity activity) {}

	public void setCancelable(boolean cancelable) {}

	public void setOnCancelListener(OnCancelListener onCancelListener) {}

	public void setOnDismissListener(OnDismissListener onDismissListener) {
		this.onDismissListener = onDismissListener;
	}

	public View findViewById(int id) {
		return window.findViewById(id);
	}

	public void show() {
		System.out.println("showing the Dialog " + this);
		Runnable action = new Runnable() {
			@Override
			public void run() {
				onCreate(null);
				if (!started) {
					started = true;
					onStart();
				}
				// Read the size of the main window. Floating dialogs should be smaller as specified in the windowMinWidth* attributes.
				// Non-floating are typically constructed with MATCH_PARENT layout params and thus get the exact size of the main window.
				// Most non-floating dialogs are technically dialogs, but are expected to behave more like full size activities.
				Rect displayFrame = new Rect();
				getWindow().getDecorView().getWindowVisibleDisplayFrame(displayFrame);

				TypedArray a = context.obtainStyledAttributes(R.styleable.Window);
				if (a.hasValue(R.styleable.Window_windowCloseOnTouchOutside))
					setCanceledOnTouchOutside(a.getBoolean(R.styleable.Window_windowCloseOnTouchOutside, true));
				float windowWidthFraction = 1;
				if (a.getBoolean(R.styleable.Window_windowIsFloating, false)) {
					if (displayFrame.width() > displayFrame.height())
						windowWidthFraction = a.getFraction(R.styleable.Window_windowMinWidthMajor, 1, 1, 1);
					else
						windowWidthFraction = a.getFraction(R.styleable.Window_windowMinWidthMinor, 1, 1, 1);
				}
				a.recycle();

				LayoutParams lp = getWindow().getAttributes();
				int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(lp.width >= 0 ? lp.width : (int)(displayFrame.width() * windowWidthFraction),
				                                                        lp.width == LayoutParams.WRAP_CONTENT ? View.MeasureSpec.AT_MOST : View.MeasureSpec.EXACTLY);

				int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(lp.height >= 0 ? lp.height : (int)(displayFrame.height()),
				                                                         lp.height == LayoutParams.WRAP_CONTENT ? View.MeasureSpec.AT_MOST : View.MeasureSpec.EXACTLY);

				getWindow().getDecorView().internalSetDefaultMeasureSpec(widthMeasureSpec, heightMeasureSpec);

				nativeShow(nativePtr);
				if (onShowListener != null)
					onShowListener.onShow(Dialog.this);
			}
		};
		if (Looper.myLooper() == Looper.getMainLooper()) {
			action.run();
		} else {
			new Handler(Looper.getMainLooper()).post(action);
		}
	}

	public boolean isShowing() {
		return nativeIsShowing(nativePtr);
	}

	@Export
	public void dismiss() {
		System.out.println("dismissing the Dialog " + Dialog.this);
		// HACK: dismissing the Dialog takes some time in AOSP, as the request goes back and forth between the application
		// and the system server. We replicate this behavior by adding 10 ms delay.
		// This Hack is required for NewPipe RouterActivity which has a race condition. It subscribes an rxJava observable
		// and immediately calls Dialog.dismiss(). The OnDismissListener would unsubscribes the observable again.
		new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
			@Override
			public void run() {
				nativeClose(nativePtr);
				// a dismiss of a dialog already gone stops nothing
				if (started) {
					started = false;
					onStop();
				}
				if (onDismissListener != null)
					onDismissListener.onDismiss(Dialog.this);
			}
		}, 10);
	}

	private boolean started;

	protected void onStart() {}

	protected void onStop() {}

	public boolean dispatchKeyEvent(android.view.KeyEvent event) {
		return false;
	}

	public boolean onKeyDown(int keyCode, android.view.KeyEvent event) {
		return false;
	}

	public boolean onKeyUp(int keyCode, android.view.KeyEvent event) {
		if (keyCode == android.view.KeyEvent.KEYCODE_BACK) {
			onBackPressed();
			return true;
		}
		return false;
	}

	public void onBackPressed() {
		cancel();
	}

	public LayoutInflater getLayoutInflater() {
		return LayoutInflater.from(getContext());
	}

	public void onAttachedToWindow() {}

	public void addContentView(View view, ViewGroup.LayoutParams params) {
		setContentView(view, params);
	}

	public Bundle onSaveInstanceState() {
		return new Bundle();
	}

	public void onRestoreInstanceState(Bundle savedInstanceState) {}

	public Window getWindow() {
		return window;
	}

	public void setCanceledOnTouchOutside(boolean cancel) {
		nativeSetCanceledOnTouchOutside(nativePtr, cancel);
	}

	public class Builder {
		public Builder(Context context) {
			System.out.println("making a Dialog$Builder");
		}
	}

	@Override
	public void onContentChanged() {
	}
	@Override
	public boolean onCreatePanelMenu(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onCreatePanelMenu'");
	}
	@Override
	public View onCreatePanelView(int featureId) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onCreatePanelView'");
	}
	@Override
	public boolean onPreparePanel(int featureId, View view, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onPreparePanel'");
	}
	@Override
	public boolean onMenuItemSelected(int featureId, MenuItem item) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onMenuItemSelected'");
	}
	@Override
	public void onPanelClosed(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onPanelClosed'");
	}

	@Override
	public boolean onMenuOpened(int featureId, Menu menu) {
		// TODO Auto-generated method stub
		throw new UnsupportedOperationException("Unimplemented method 'onMenuOpened'");
	}

	protected void onCreate(Bundle savedInstanceState) {
		System.out.println("- onCreate - Dialog!");
	}

	public void hide() {
		System.out.println("hiding the Dialog " + this);
		nativeClose(nativePtr);
	}

	@Override
	public void cancel() {
		dismiss();
	}

	public void setOnShowListener(OnShowListener onShowListener) {
		this.onShowListener = onShowListener;
	}

	public void setCancelMessage(Message msg) {}

	public void setDismissMessage(Message msg) {}

	public boolean onTouchEvent(MotionEvent event) {
		return false;
	}

	public void setOnKeyListener(OnKeyListener onKeyListener) {}

	public View getCurrentFocus() {
		return null;
	}

	// GEN member:android.app.Dialog:public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher();
	public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() { return null; }
	public boolean onKeyMultiple(int arg0, int arg1, android.view.KeyEvent arg2) { return false; }
	public boolean onKeyLongPress(int arg0, android.view.KeyEvent arg1) { return false; }
}
