package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

public class ZoomButton extends View implements android.view.View.OnLongClickListener {

	public ZoomButton(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public void setZoomSpeed(long speed) {}
	public boolean onLongClick(android.view.View arg0) { return false; }
}
