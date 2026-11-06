package android.text;

public class BoringLayout extends Layout implements android.text.TextUtils.EllipsizeCallback {

	public BoringLayout(CharSequence source, TextPaint paint, int outerwidth, Layout.Alignment align, float spacingMult, float spacingAdd, BoringLayout.Metrics metrics, boolean includePad) {
		super(source, paint, outerwidth, align, spacingMult, spacingAdd);
	}

	public static class Metrics {
	// GEN member:android.text.BoringLayout$Metrics:public int width;
	public int width;
};

	public static Metrics isBoring(CharSequence source, TextPaint paint, Metrics metrics) {
		return metrics;
	}

	public static Metrics isBoring(CharSequence source, TextPaint paint) {
		return null;
	}

	// GEN member:android.text.BoringLayout:public android.text.BoringLayout(java.lang.CharSequence, android.text.TextPaint, int, android.text.Layout$Alignment, float, float, android.text.BoringLayout$Metrics, boolean, android.text.TextUtils$TruncateAt, int);
	public BoringLayout(java.lang.CharSequence arg0, android.text.TextPaint arg1, int arg2, android.text.Layout.Alignment arg3, float arg4, float arg5, android.text.BoringLayout.Metrics arg6, boolean arg7, android.text.TextUtils.TruncateAt arg8, int arg9) { this((java.lang.CharSequence)null, (android.text.TextPaint)null, (int)0, (android.text.Layout.Alignment)null, (float)0, (float)0, (android.text.BoringLayout.Metrics)null, false); }

	// GEN member:android.text.BoringLayout:public android.text.BoringLayout(java.lang.CharSequence, android.text.TextPaint, int, android.text.Layout$Alignment, float, float, android.text.BoringLayout$Metrics, boolean, android.text.TextUtils$TruncateAt, int, boolean);
	public BoringLayout(java.lang.CharSequence arg0, android.text.TextPaint arg1, int arg2, android.text.Layout.Alignment arg3, float arg4, float arg5, android.text.BoringLayout.Metrics arg6, boolean arg7, android.text.TextUtils.TruncateAt arg8, int arg9, boolean arg10) { this((java.lang.CharSequence)null, (android.text.TextPaint)null, (int)0, (android.text.Layout.Alignment)null, (float)0, (float)0, (android.text.BoringLayout.Metrics)null, false); }

	// GEN member:android.text.BoringLayout:public static android.text.BoringLayout$Metrics isBoring(java.lang.CharSequence, android.text.TextPaint, android.text.TextDirectionHeuristic, boolean, android.text.BoringLayout$Metrics);
	public static android.text.BoringLayout.Metrics isBoring(java.lang.CharSequence arg0, android.text.TextPaint arg1, android.text.TextDirectionHeuristic arg2, boolean arg3, android.text.BoringLayout.Metrics arg4) { return null; }

	// GEN member:android.text.BoringLayout:public boolean isFallbackLineSpacingEnabled();
	public boolean isFallbackLineSpacingEnabled() { return false; }

	// GEN member:android.text.BoringLayout:public static android.text.BoringLayout make(java.lang.CharSequence, android.text.TextPaint, int, android.text.Layout$Alignment, float, float, android.text.BoringLayout$Metrics, boolean, android.text.TextUtils$TruncateAt, int);
	public static android.text.BoringLayout make(java.lang.CharSequence arg0, android.text.TextPaint arg1, int arg2, android.text.Layout.Alignment arg3, float arg4, float arg5, android.text.BoringLayout.Metrics arg6, boolean arg7, android.text.TextUtils.TruncateAt arg8, int arg9) { return null; }
}
