package android.view;

import android.graphics.Insets;
import android.graphics.Rect;
import android.view.DisplayCutout;
import java.util.List;

public class WindowInsets {

	public static final WindowInsets CONSUMED = new WindowInsets();

	public WindowInsets() {}

	public WindowInsets(WindowInsets windowInsets) {}

	public WindowInsets consumeStableInsets() {
		return this;
	}

	public WindowInsets consumeSystemWindowInsets() {
		return this;
	}

	public WindowInsets replaceSystemWindowInsets(int left, int top, int right, int bottom) {
		return this;
	}

	public int getSystemWindowInsetLeft() {
		return 0;
	}

	public int getSystemWindowInsetTop() {
		return 0;
	}

	public int getSystemWindowInsetRight() {
		return 0;
	}

	public int getSystemWindowInsetBottom() {
		return 0;
	}

	public int getStableInsetLeft() {
		return 0;
	}

	public int getStableInsetTop() {
		return 0;
	}

	public int getStableInsetRight() {
		return 0;
	}

	public int getStableInsetBottom() {
		return 0;
	}

	public Insets getInsets(int typeMask) {
		return Insets.NONE;
	}

	public Insets getSystemGestureInsets() {
		return Insets.NONE;
	}

	public Insets getMandatorySystemGestureInsets() {
		return Insets.NONE;
	}

	public Insets getTappableElementInsets() {
		return Insets.NONE;
	}

	public DisplayCutout getDisplayCutout() {
		return null;
	}

	public boolean isRound() {
		return false;
	}

	public boolean isConsumed() {
		return false;
	}

	public boolean isVisible(int typeMask) {
		return false;
	}

	public WindowInsets consumeDisplayCutout() {
		return this;
	}

	public static final class Builder {

		public Builder() {}

		public Builder(WindowInsets insets) {}

		public Builder setSystemWindowInsets(Insets systemWindowInsets) {
			return this;
		}

		public Builder setSystemGestureInsets(Insets insets) {
			return this;
		}

		public Builder setMandatorySystemGestureInsets(Insets insets) {
			return this;
		}

		public Builder setTappableElementInsets(Insets insets) {
			return this;
		}

		public Builder setInsets(int typeMask, Insets insets) {
			return this;
		}

		public Builder setInsetsIgnoringVisibility(int typeMask, Insets insets)
		    throws IllegalArgumentException {
			return this;
		}

		public Builder setVisible(int typeMask, boolean visible) {
			return this;
		}

		public Builder setStableInsets(Insets stableInsets) {
			return this;
		}

		public Builder setDisplayCutout(DisplayCutout displayCutout) {
			return this;
		}

		public Builder setPrivacyIndicatorBounds(Rect bounds) {
			return this;
		}

		public Builder setRound(boolean round) {
			return this;
		}

		public Builder setAlwaysConsumeSystemBars(boolean alwaysConsumeSystemBars) {
			return this;
		}

		public Builder setForceConsumingTypes(int forceConsumingTypes) {
			return this;
		}

		public Builder setBoundingRects(int typeMask, List<Rect> rects) {
			return this;
		}

		public Builder setBoundingRectsIgnoringVisibility(int typeMask, List<Rect> rects) {
			return this;
		}

		public Builder setFrame(int width, int height) {
			return this;
		}

		public WindowInsets build() {
			return new WindowInsets();
		}
	
	// GEN member:android.view.WindowInsets$Builder:public android.view.WindowInsets$Builder setRoundedCorner(int, android.view.RoundedCorner);
	public android.view.WindowInsets.Builder setRoundedCorner(int arg0, android.view.RoundedCorner arg1) { return this; }
}

	/* Copyright (C) 2014 The Android Open Source Project */
	public static final class Type {

		static final int FIRST = 1 << 0;
		static final int STATUS_BARS = FIRST;
		static final int NAVIGATION_BARS = 1 << 1;
		static final int CAPTION_BAR = 1 << 2;

		static final int IME = 1 << 3;

		static final int SYSTEM_GESTURES = 1 << 4;
		static final int MANDATORY_SYSTEM_GESTURES = 1 << 5;
		static final int TAPPABLE_ELEMENT = 1 << 6;

		static final int DISPLAY_CUTOUT = 1 << 7;

		static final int WINDOW_DECOR = 1 << 8;

		static final int SYSTEM_OVERLAYS = 1 << 9;
		static final int LAST = SYSTEM_OVERLAYS;
		static final int SIZE = 10;

		static final int DEFAULT_VISIBLE = ~IME;

		static int indexOf(@InsetsType int type) {
			switch (type) {
				case STATUS_BARS:
					return 0;
				case NAVIGATION_BARS:
					return 1;
				case CAPTION_BAR:
					return 2;
				case IME:
					return 3;
				case SYSTEM_GESTURES:
					return 4;
				case MANDATORY_SYSTEM_GESTURES:
					return 5;
				case TAPPABLE_ELEMENT:
					return 6;
				case DISPLAY_CUTOUT:
					return 7;
				case WINDOW_DECOR:
					return 8;
				case SYSTEM_OVERLAYS:
					return 9;
				default:
					throw new IllegalArgumentException("type needs to be >= FIRST and <= LAST,"
					                                   + " type=" + type);
			}
		}

		/**
		 * @hide
		 */
		public static String toString(@InsetsType int types) {
			StringBuilder result = new StringBuilder();
			if ((types & STATUS_BARS) != 0) {
				result.append("statusBars ");
			}
			if ((types & NAVIGATION_BARS) != 0) {
				result.append("navigationBars ");
			}
			if ((types & CAPTION_BAR) != 0) {
				result.append("captionBar ");
			}
			if ((types & IME) != 0) {
				result.append("ime ");
			}
			if ((types & SYSTEM_GESTURES) != 0) {
				result.append("systemGestures ");
			}
			if ((types & MANDATORY_SYSTEM_GESTURES) != 0) {
				result.append("mandatorySystemGestures ");
			}
			if ((types & TAPPABLE_ELEMENT) != 0) {
				result.append("tappableElement ");
			}
			if ((types & DISPLAY_CUTOUT) != 0) {
				result.append("displayCutout ");
			}
			if ((types & WINDOW_DECOR) != 0) {
				result.append("windowDecor ");
			}
			if ((types & SYSTEM_OVERLAYS) != 0) {
				result.append("systemOverlays ");
			}
			if (result.length() > 0) {
				result.delete(result.length() - 1, result.length());
			}
			return result.toString();
		}

		private Type() {
		}

		/**
		 * @hide
		 */
		public @interface InsetsType {
		}

		/**
		 * @return An insets type representing any system bars for displaying status.
		 */
		public static @InsetsType int statusBars() {
			return STATUS_BARS;
		}

		/**
		 * @return An insets type representing any system bars for navigation.
		 */
		public static @InsetsType int navigationBars() {
			return NAVIGATION_BARS;
		}

		/**
		 * @return An insets type representing the window of a caption bar.
		 */
		public static @InsetsType int captionBar() {
			return CAPTION_BAR;
		}

		/**
		 * @return An insets type representing the window of an {@link InputMethod}.
		 */
		public static @InsetsType int ime() {
			return IME;
		}

		/**
		 * Returns an insets type representing the system gesture insets.
		 *
		 * <p>The system gesture insets represent the area of a window where system gestures have
		 * priority and may consume some or all touch input, e.g. due to the a system bar
		 * occupying it, or it being reserved for touch-only gestures.
		 *
		 * <p>Simple taps are guaranteed to reach the window even within the system gesture insets,
		 * as long as they are outside the {@link #getSystemWindowInsets() system window insets}.
		 *
		 * <p>When {@link View#SYSTEM_UI_FLAG_LAYOUT_STABLE} is requested, an inset will be returned
		 * even when the system gestures are inactive due to
		 * {@link View#SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN} or
		 * {@link View#SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION}.
		 *
		 * @see #getSystemGestureInsets()
		 */
		public static @InsetsType int systemGestures() {
			return SYSTEM_GESTURES;
		}

		/**
		 * @see #getMandatorySystemGestureInsets
		 */
		public static @InsetsType int mandatorySystemGestures() {
			return MANDATORY_SYSTEM_GESTURES;
		}

		/**
		 * @see #getTappableElementInsets
		 */
		public static @InsetsType int tappableElement() {
			return TAPPABLE_ELEMENT;
		}

		/**
		 * Returns an insets type representing the area that used by {@link DisplayCutout}.
		 *
		 * <p>This is equivalent to the safe insets on {@link #getDisplayCutout()}.
		 *
		 * <p>Note: During dispatch to {@link View#onApplyWindowInsets}, if the window is using
		 * the {@link WindowManager.LayoutParams#LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT default}
		 * {@link WindowManager.LayoutParams#layoutInDisplayCutoutMode}, {@link #getDisplayCutout()}
		 * will return {@code null} even if the window overlaps a display cutout area, in which case
		 * the {@link #displayCutout() displayCutout() inset} will still report the accurate value.
		 *
		 * @see DisplayCutout#getSafeInsetLeft()
		 * @see DisplayCutout#getSafeInsetTop()
		 * @see DisplayCutout#getSafeInsetRight()
		 * @see DisplayCutout#getSafeInsetBottom()
		 */
		public static @InsetsType int displayCutout() {
			return DISPLAY_CUTOUT;
		}

		/**
		 * System overlays represent the insets caused by the system visible elements. Unlike
		 * {@link #navigationBars()} or {@link #statusBars()}, system overlays might not be
		 * hidden by the client.
		 *
		 * For compatibility reasons, this type is included in {@link #systemBars()}. In this
		 * way, views which fit {@link #systemBars()} fit {@link #systemOverlays()}.
		 *
		 * Examples include climate controls, multi-tasking affordances, etc.
		 *
		 * @return An insets type representing the system overlays.
		 */
		public static @InsetsType int systemOverlays() {
			return SYSTEM_OVERLAYS;
		}

		/**
		 * @return All system bars. Includes {@link #statusBars()}, {@link #captionBar()} as well as
		 *         {@link #navigationBars()}, {@link #systemOverlays()}, but not {@link #ime()}.
		 */
		public static @InsetsType int systemBars() {
			return STATUS_BARS | NAVIGATION_BARS | CAPTION_BAR | SYSTEM_OVERLAYS;
		}

		/**
		 * @return Default visible types.
		 *
		 * @hide
		 */
		public static @InsetsType int defaultVisible() {
			return DEFAULT_VISIBLE;
		}

		/**
		 * @return All inset types combined.
		 *
		 * @hide
		 */
		public static @InsetsType int all() {
			return 0xFFFFFFFF;
		}

		/**
		 * @return System bars which can be controlled by {@link View.SystemUiVisibility}.
		 *
		 * @hide
		 */
		public static boolean hasCompatSystemBars(@InsetsType int types) {
			return (types & (STATUS_BARS | NAVIGATION_BARS)) != 0;
		}
	}

	public Insets getInsetsIgnoringVisibility(int typeMask) {
		return getInsets(typeMask);
	}

	public WindowInsets inset(int left, int top, int right, int bottom) {
		return this;
	}

	// GEN member:android.view.WindowInsets:public java.util.List<android.graphics.Rect> getBoundingRectsIgnoringVisibility(int);
	public java.util.List<android.graphics.Rect> getBoundingRectsIgnoringVisibility(int arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.view.WindowInsets:public java.util.List<android.graphics.Rect> getBoundingRects(int);
	public java.util.List<android.graphics.Rect> getBoundingRects(int arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.view.WindowInsets:public android.graphics.Rect getPrivacyIndicatorBounds();
	public android.graphics.Rect getPrivacyIndicatorBounds() { return null; }

	// GEN member:android.view.WindowInsets:public android.view.RoundedCorner getRoundedCorner(int);
	public android.view.RoundedCorner getRoundedCorner(int arg0) { return null; }

	// GEN member:android.view.WindowInsets:public android.graphics.Insets getSystemWindowInsets();
	public android.graphics.Insets getSystemWindowInsets() { return null; }

	// GEN member:android.view.WindowInsets:public android.view.DisplayShape getDisplayShape();
	public android.view.DisplayShape getDisplayShape() { return null; }
}
