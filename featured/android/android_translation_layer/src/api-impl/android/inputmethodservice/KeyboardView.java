package android.inputmethodservice;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

// a desktop has no on-screen keyboard: the view draws nothing
public class KeyboardView extends View implements android.view.View.OnClickListener {

	public interface OnKeyboardActionListener {
		void onPress(int primaryCode);
		void onRelease(int primaryCode);
		void onKey(int primaryCode, int[] keyCodes);
		void onText(CharSequence text);
		void swipeLeft();
		void swipeRight();
		void swipeDown();
		void swipeUp();
	}

	private OnKeyboardActionListener listener;

	public KeyboardView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public KeyboardView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	public void setOnKeyboardActionListener(OnKeyboardActionListener l) {
		listener = l;
	}

	protected OnKeyboardActionListener getOnKeyboardActionListener() {
		return listener;
	}

	public boolean isPreviewEnabled() {
		return false;
	}

	public void setPreviewEnabled(boolean previewEnabled) {}

	public boolean isShifted() {
		return false;
	}

	public boolean setShifted(boolean shifted) {
		return false;
	}

	public void closing() {}

	public boolean handleBack() {
		return false;
	}
	public void onClick(android.view.View arg0) {}
}
