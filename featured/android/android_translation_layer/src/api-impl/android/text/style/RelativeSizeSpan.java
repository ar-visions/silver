package android.text.style;

import android.text.TextPaint;

public class RelativeSizeSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
	private final float proportion;

	public RelativeSizeSpan(float proportion) { this.proportion = proportion; }

	public float getSizeChange() { return proportion; }

	@Override
	public void updateDrawState(TextPaint ds) { ds.setTextSize(ds.getTextSize() * proportion); }

	@Override
	public void updateMeasureState(TextPaint ds) { ds.setTextSize(ds.getTextSize() * proportion); }
}
