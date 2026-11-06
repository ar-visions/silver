package android.view;

import android.atl.annotation.Export;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.DisplayMetrics;

@Export
public final class Display {

	public static int window_width = 960;
	public static int window_height = 540;

	// FIXME: what do we return here?
	// we don't want to hardcode this stuff, but at the same time the apps can cache it
	public void getMetrics(DisplayMetrics outMetrics) {
		outMetrics.widthPixels = this.window_width;
		outMetrics.heightPixels = this.window_height;
	}

	public void getRealMetrics(DisplayMetrics outMetrics) {
		getMetrics(outMetrics); // probably?
	}

	public int getWidth() {
		return window_width;
	}

	public int getHeight() {
		return window_height;
	}

	public int getRawWidth() {
		return window_width; // what's the difference?
	}

	public int getRawHeight() {
		return window_height; // what's the difference?
	}

	public int getRotation() {
		return 0 /*ROTATION_0*/;
	}

	public float getRefreshRate() {
		return 60; // FIXME
	}

	public long getAppVsyncOffsetNanos() {
		return 0; // what else would we return here?
	}

	public int getDisplayId() {
		return 0;
	}

	public long getPresentationDeadlineNanos() {
		return 0; // what else...
	}

	public void getSize(Point size) {
		size.set(getWidth(), getHeight());
	}

	public void getRealSize(Point size) {
		getSize(size);
	}

	public void getRectSize(Rect rect) {
		rect.set(0, 0, getWidth(), getHeight());
	}

	public DisplayCutout getCutout() {
		return DisplayCutout.NO_CUTOUT;
	}

	public Display.Mode getMode() {
		return new Display.Mode();
	}

	public static final class Mode implements android.os.Parcelable {

		public int getPhysicalWidth() {
			return window_width;
		}

		public int getPhysicalHeight() {
			return window_height;
		}
	}

	public Mode[] getSupportedModes() {
		return new Mode[] {getMode()};
	}

	public float[] getSupportedRefreshRates() {
		return new float[] { getRefreshRate() };
	}

	// STATE_ON
	public int getState() {
		return 2;
	}

	// GEN class:android.view.Display$HdrCapabilities
	public final class HdrCapabilities implements android.os.Parcelable {
		// GEN member:android.view.Display$HdrCapabilities:public static final int HDR_TYPE_DOLBY_VISION = 1;
		public static final int HDR_TYPE_DOLBY_VISION = 1;
	
		// GEN member:android.view.Display$HdrCapabilities:public static final int HDR_TYPE_HDR10 = 2;
		public static final int HDR_TYPE_HDR10 = 2;
	
		// GEN member:android.view.Display$HdrCapabilities:public static final int HDR_TYPE_HDR10_PLUS = 4;
		public static final int HDR_TYPE_HDR10_PLUS = 4;
	
		// GEN member:android.view.Display$HdrCapabilities:public static final int HDR_TYPE_HLG = 3;
		public static final int HDR_TYPE_HLG = 3;
	
		// GEN member:android.view.Display$HdrCapabilities:public static final int HDR_TYPE_INVALID = -1;
		public static final int HDR_TYPE_INVALID = -1;
	
		// GEN member:android.view.Display$HdrCapabilities:public static final float INVALID_LUMINANCE = -1.0f;
		public static final float INVALID_LUMINANCE = -1.0f;
	
		// GEN member:android.view.Display$HdrCapabilities:public int[] getSupportedHdrTypes();
		public int[] getSupportedHdrTypes() { return new int[0]; }
	
		// GEN member:android.view.Display$HdrCapabilities:public float getDesiredMaxLuminance();
		public float getDesiredMaxLuminance() { return 0; }
	
		// GEN member:android.view.Display$HdrCapabilities:public float getDesiredMaxAverageLuminance();
		public float getDesiredMaxAverageLuminance() { return 0; }
	
		// GEN member:android.view.Display$HdrCapabilities:public float getDesiredMinLuminance();
		public float getDesiredMinLuminance() { return 0; }
	
		// GEN member:android.view.Display$HdrCapabilities:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		// GEN member:android.view.Display$HdrCapabilities:public int hashCode();
		public int hashCode() { return 0; }
	
		// GEN member:android.view.Display$HdrCapabilities:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		// GEN member:android.view.Display$HdrCapabilities:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.Display$HdrCapabilities:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		protected HdrCapabilities() {}
	}


	// GEN member:android.view.Display:public android.view.Display$HdrCapabilities getHdrCapabilities();
	public android.view.Display.HdrCapabilities getHdrCapabilities() { return null; }

	// GEN member:android.view.Display:public java.lang.String getName();
	public java.lang.String getName() { return null; }

	// GEN member:android.view.Display:public android.view.RoundedCorner getRoundedCorner(int);
	public android.view.RoundedCorner getRoundedCorner(int arg0) { return null; }

	// GEN member:android.view.Display:public boolean isHdr();
	public boolean isHdr() { return false; }
}
