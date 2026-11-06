package android.view.accessibility;

import java.util.Locale;

public class CaptioningManager {

	public boolean isEnabled() {
		return false;
	}

	public float getFontScale() {
		return 1.0f;
	}

	public static final class CaptionStyle {
	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public final int backgroundColor;
	public int backgroundColor;

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public final int edgeColor;
	public int edgeColor;

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public final int edgeType;
	public int edgeType;

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public final int foregroundColor;
	public int foregroundColor;

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public android.graphics.Typeface getTypeface();
	public android.graphics.Typeface getTypeface() { return null; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public boolean hasBackgroundColor();
	public boolean hasBackgroundColor() { return false; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public boolean hasEdgeColor();
	public boolean hasEdgeColor() { return false; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public boolean hasEdgeType();
	public boolean hasEdgeType() { return false; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public boolean hasForegroundColor();
	public boolean hasForegroundColor() { return false; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public boolean hasWindowColor();
	public boolean hasWindowColor() { return false; }

	// GEN member:android.view.accessibility.CaptioningManager$CaptionStyle:public final int windowColor;
	public int windowColor;
}

	public static abstract class CaptioningChangeListener {
		public void onEnabledChanged(boolean enabled) {}
		public void onUserStyleChanged(CaptionStyle userStyle) {}
		public void onLocaleChanged(Locale locale) {}
		public void onFontScaleChanged(float fontScale) {}
		public void onSystemAudioCaptioningChanged(boolean enabled) {}
		public void onSystemAudioCaptioningUiChanged(boolean enabled) {}
	}

	public void addCaptioningChangeListener(CaptioningChangeListener listener) {}

	// GEN member:android.view.accessibility.CaptioningManager:public final java.util.Locale getLocale();
	public final java.util.Locale getLocale() { return null; }

	// GEN member:android.view.accessibility.CaptioningManager:public android.view.accessibility.CaptioningManager$CaptionStyle getUserStyle();
	public android.view.accessibility.CaptioningManager.CaptionStyle getUserStyle() { return null; }
}
