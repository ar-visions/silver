package android.text.style;

import android.text.TextPaint;

public class CharacterStyle {
	// abstract on Android; ATL's own spans may leave it as is
	public void updateDrawState(TextPaint tp) {}

	public CharacterStyle getUnderlying() { return this; }

	// GEN member:android.text.style.CharacterStyle:public static android.text.style.CharacterStyle wrap(android.text.style.CharacterStyle);
	public static android.text.style.CharacterStyle wrap(android.text.style.CharacterStyle arg0) { return null; }
}
