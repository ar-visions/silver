package android.text.style;

import android.text.TextPaint;

public class ScaleXSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
	private final float proportion;

	public ScaleXSpan(float proportion) { this.proportion = proportion; }

	public float getScaleX() { return proportion; }

	@Override
	public void updateDrawState(TextPaint ds) { ds.setTextScaleX(ds.getTextScaleX() * proportion); }

	@Override
	public void updateMeasureState(TextPaint ds) { ds.setTextScaleX(ds.getTextScaleX() * proportion); }
}
