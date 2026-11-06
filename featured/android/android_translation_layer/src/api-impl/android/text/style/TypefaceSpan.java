package android.text.style;

import android.graphics.Typeface;
import android.text.TextPaint;

public class TypefaceSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
	private final String family;
	private final Typeface typeface;

	public TypefaceSpan(String family) {
		this.family = family;
		this.typeface = null;
	}

	public TypefaceSpan(Typeface typeface) {
		this.family = null;
		this.typeface = typeface;
	}

	public String getFamily() { return family; }
	public Typeface getTypeface() { return typeface; }

	private void apply(TextPaint paint) {
		if (typeface != null)
			paint.setTypeface(typeface);
		else if (family != null)
			paint.setTypeface(Typeface.create(family, Typeface.NORMAL));
	}

	@Override
	public void updateDrawState(TextPaint ds) { apply(ds); }

	@Override
	public void updateMeasureState(TextPaint paint) { apply(paint); }
}
