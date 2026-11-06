package android.text.style;

import android.text.TextPaint;

public class ForegroundColorSpan extends CharacterStyle implements android.text.ParcelableSpan, android.text.style.UpdateAppearance {
	private final int color;

	public ForegroundColorSpan(int color) { this.color = color; }

	public int getForegroundColor() { return color; }

	@Override
	public void updateDrawState(TextPaint ds) { ds.setColor(color); }
}
