package android.view;

import android.graphics.Insets;

public class DisplayCutout {
	// GEN default constructor, kept
	public DisplayCutout() {}

	public static final DisplayCutout NO_CUTOUT = new DisplayCutout();

	public Insets getWaterfallInsets() {
		return Insets.NONE;
	}

	public int getSafeInsetBottom() {
		return 0;
	}

	public int getSafeInsetLeft() {
		return 0;
	}

	public int getSafeInsetRight() {
		return 0;
	}

	public int getSafeInsetTop() {
		return 0;
	}

	// GEN member:android.view.DisplayCutout:public java.util.List<android.graphics.Rect> getBoundingRects();
	public java.util.List<android.graphics.Rect> getBoundingRects() { return new java.util.ArrayList<>(); }

	// GEN member:android.view.DisplayCutout:public android.graphics.Path getCutoutPath();
	public android.graphics.Path getCutoutPath() { return null; }

	// GEN member:android.view.DisplayCutout:public android.view.DisplayCutout(android.graphics.Insets, android.graphics.Rect, android.graphics.Rect, android.graphics.Rect, android.graphics.Rect);
	public DisplayCutout(android.graphics.Insets arg0, android.graphics.Rect arg1, android.graphics.Rect arg2, android.graphics.Rect arg3, android.graphics.Rect arg4) {}

	// GEN member:android.view.DisplayCutout:public android.graphics.Rect getBoundingRectBottom();
	public android.graphics.Rect getBoundingRectBottom() { return null; }

	// GEN member:android.view.DisplayCutout:public android.graphics.Rect getBoundingRectLeft();
	public android.graphics.Rect getBoundingRectLeft() { return null; }

	// GEN member:android.view.DisplayCutout:public android.graphics.Rect getBoundingRectRight();
	public android.graphics.Rect getBoundingRectRight() { return null; }

	// GEN member:android.view.DisplayCutout:public android.graphics.Rect getBoundingRectTop();
	public android.graphics.Rect getBoundingRectTop() { return null; }
}
