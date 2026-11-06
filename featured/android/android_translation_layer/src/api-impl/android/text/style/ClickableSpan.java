package android.text.style;

import android.text.TextPaint;
import android.view.View;

public class ClickableSpan extends CharacterStyle implements android.text.style.UpdateAppearance {
	public void onClick(View widget) {}

	@Override
	public void updateDrawState(TextPaint ds) {
		ds.setColor(ds.linkColor);
		ds.setUnderlineText(true);
	}
}
