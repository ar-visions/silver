package android.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public class CheckedTextView extends TextView implements android.widget.Checkable {

	public CheckedTextView(Context context) {
		super(context);
	}

	public CheckedTextView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	private boolean checked;

	public void setChecked(boolean checked) {
		if (this.checked != checked) {
			this.checked = checked;
			refreshDrawableState();
		}
	}

	public boolean isChecked() {
		return checked;
	}

	public void toggle() {
		setChecked(!checked);
	}

	public void setCheckMarkDrawable(Drawable d) {}

	// GEN member:android.widget.CheckedTextView:public android.graphics.drawable.Drawable getCheckMarkDrawable();
	public android.graphics.drawable.Drawable getCheckMarkDrawable() { return null; }

	// GEN member:android.widget.CheckedTextView:public void setCheckMarkTintList(android.content.res.ColorStateList);
	public void setCheckMarkTintList(android.content.res.ColorStateList arg0) {}

	// GEN member:android.widget.CheckedTextView:public void setCheckMarkTintMode(android.graphics.PorterDuff$Mode);
	public void setCheckMarkTintMode(android.graphics.PorterDuff.Mode arg0) {}
}
