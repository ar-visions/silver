package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class HorizontalScrollView extends FrameLayout {

	public HorizontalScrollView(Context context) {
		super(context);
	}

	public HorizontalScrollView(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public void setHorizontalScrollBarEnabled(boolean enabled) {}

	public void smoothScrollTo(int x, int y) {}

	public void setFillViewport(boolean fillViewport) {}

	// GEN member:android.widget.HorizontalScrollView:protected int computeScrollDeltaToGetChildRectOnScreen(android.graphics.Rect);
	protected int computeScrollDeltaToGetChildRectOnScreen(android.graphics.Rect arg0) { return 0; }

	// GEN member:android.widget.HorizontalScrollView:public void fling(int);
	public void fling(int arg0) {}

	// GEN member:android.widget.HorizontalScrollView:public boolean fullScroll(int);
	public boolean fullScroll(int arg0) { return false; }
}
