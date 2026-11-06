package android.text.style;

import android.text.TextPaint;

public class BackgroundColorSpan extends CharacterStyle implements android.text.ParcelableSpan, android.text.style.UpdateAppearance {
	private final int color;

	public BackgroundColorSpan(int color) { this.color = color; }

	public int getBackgroundColor() { return color; }

	@Override
	public void updateDrawState(TextPaint ds) { ds.bgColor = color; }
}
