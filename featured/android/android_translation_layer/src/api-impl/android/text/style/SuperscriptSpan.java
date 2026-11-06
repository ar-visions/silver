package android.text.style;

import android.text.ParcelableSpan;
import android.text.TextPaint;

public class SuperscriptSpan extends MetricAffectingSpan implements ParcelableSpan {
	public SuperscriptSpan() {}

	@Override
	public void updateDrawState(TextPaint ds) { ds.baselineShift += (int)(ds.ascent() / 2); }

	@Override
	public void updateMeasureState(TextPaint ds) { ds.baselineShift += (int)(ds.ascent() / 2); }
}
