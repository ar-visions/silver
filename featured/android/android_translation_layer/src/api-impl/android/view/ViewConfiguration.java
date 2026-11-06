package android.view;

import android.content.Context;

/**
 * default values are mainly based on AOSPs defaults. Does not account for scaling yet.
 */
public class ViewConfiguration {

	public static ViewConfiguration get(Context context) {
		return new ViewConfiguration();
	}

	public int getScaledTouchSlop() {
		return 8;
	}

	public int getScaledMaximumFlingVelocity() {
		return 8000;
	}

	public int getScaledMinimumFlingVelocity() {
		return 50;
	}

	public static int getTapTimeout() {
		return 100;
	}

	public static int getLongPressTimeout() {
		return 400;
	}

	public int getScaledPagingTouchSlop() {
		return 16;
	}

	public boolean hasPermanentMenuKey() {
		return false;
	}

	public static int getDoubleTapTimeout() {
		return 300;
	}

	public int getScaledDoubleTapSlop() {
		return 100;
	}

	public static float getScrollFriction() {
		return 0.015f;
	}

	public int getScaledWindowTouchSlop() {
		return 8;
	}

	public float getScaledHorizontalScrollFactor() {
		return 1;
	}

	public float getScaledVerticalScrollFactor() {
		return 1;
	}

	public static int getScrollBarFadeDuration() {
		return 100;
	}

	public static int getScrollDefaultDelay() {
		return 16;
	}

	public boolean shouldShowMenuShortcutsWhenKeyboardPresent() {
		return true;
	}

	public static int getKeyRepeatTimeout() {
		return 1000;
	}

	public static int getKeyRepeatDelay() {
		return 100;
	}

	public int getScaledEdgeSlop() {
		return 8;
	}

	// Android's unscaled values (the deprecated static forms)
	public static int getWindowTouchSlop() {
		return 16;
	}

	public static int getTouchSlop() {
		return 8;
	}

	public static int getEdgeSlop() {
		return 12;
	}

	public static int getMinimumFlingVelocity() {
		return 50;
	}

	public static int getMaximumFlingVelocity() {
		return 8000;
	}

	public static int getScrollBarSize() {
		return 4;
	}

	public static int getFadingEdgeLength() {
		return 12;
	}

	public static int getPressedStateDuration() {
		return 64;
	}

	public static int getJumpTapTimeout() {
		return 500;
	}

	public static int getDoubleTapSlop() {
		return 100;
	}

	public static int getMaximumDrawingCacheSize() {
		return 480 * 800 * 4;
	}

	public static long getZoomControlsTimeout() {
		return 3000;
	}

	public static long getGlobalActionKeyTimeout() {
		return 500;
	}

	public int getScaledScrollBarSize() {
		return 4;
	}

	public int getScaledFadingEdgeLength() {
		return 12;
	}

	public int getScaledMaximumDrawingCacheSize() {
		return 480 * 800 * 4;
	}

	public int getScaledOverscrollDistance() {
		return 0;
	}

	public int getScaledOverflingDistance() {
		return 6;
	}

	public int getScaledHoverSlop() {
		return 4;
	}

	public int getScaledMinimumScalingSpan() {
		return 170;
	}

	// GEN member:android.view.ViewConfiguration:public int getScaledHandwritingGestureLineMargin();
	public int getScaledHandwritingGestureLineMargin() { return 0; }

	// GEN member:android.view.ViewConfiguration:public int getScaledHandwritingSlop();
	public int getScaledHandwritingSlop() { return 0; }

	// GEN member:android.view.ViewConfiguration:public int getScaledMaximumFlingVelocity(int, int, int);
	public int getScaledMaximumFlingVelocity(int arg0, int arg1, int arg2) { return 0; }

	// GEN member:android.view.ViewConfiguration:public int getScaledMinimumFlingVelocity(int, int, int);
	public int getScaledMinimumFlingVelocity(int arg0, int arg1, int arg2) { return 0; }
}
