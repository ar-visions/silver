package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MotionEvent;

public class Button extends TextView {

	private Drawable compoundDrawableLeft;

	public Button(Context context) {
		this(context, null);
	}

	public Button(Context context, AttributeSet attributeSet) {
		this(context, attributeSet, 0);
	}

	public Button(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public Button(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.TextView, 0, 0);
		if (a.hasValue(com.android.internal.R.styleable.TextView_text)) {
			setText(a.getText(com.android.internal.R.styleable.TextView_text));
		}

		a.recycle();
	}

	// the drawable is the whole look, so GTK's own frame goes
	@Override
	public void setBackgroundDrawable(Drawable background) {
		super.setBackgroundDrawable(background);
		if (background != null)
			native_addClass(widget, "ATL-no-border");
		else
			native_removeClass(widget, "ATL-no-border");
	}

	@Override
	protected native long native_constructor(Context context, AttributeSet attrs);
	public native void native_setText(long widget, String text);
	@Override
	protected native void nativeSetOnClickListener(long widget);
	protected native void native_setCompoundDrawables(long widget, long paintable);

	// GTK clicks only a button it picks under the pointer; one
	// covered by another widget is clicked here, as Android does
	@Override
	public boolean onTouchEvent(MotionEvent event) {
		if (!native_picked(widget, event.getRawX(), event.getRawY()))
			return super.onTouchEvent(event);
		return native_gtkEventPropagation();
	}
	private native boolean native_picked(long widget, float x, float y);

	@Override
	public void setText(CharSequence text) {
		native_setText(widget, String.valueOf(text));
	}

	@Override
	public native CharSequence getText();

	@Override
	public void setTextSize(float size) {}

	@Override
	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		compoundDrawableLeft = left;
		native_setCompoundDrawables(widget, left != null ? left.paintable : 0);
	}
}
