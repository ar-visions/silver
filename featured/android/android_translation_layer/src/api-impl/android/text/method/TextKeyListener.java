package android.text.method;

import android.text.Editable;

public class TextKeyListener implements android.text.SpanWatcher {

	public static void clear(Editable content) {}

	public enum Capitalize {
		CHARACTERS,
		WORDS,
		SENTENCES,
		NONE,
	}
	public void onSpanChanged(android.text.Spannable arg0, java.lang.Object arg1, int arg2, int arg3, int arg4, int arg5) {}
	public void onSpanRemoved(android.text.Spannable arg0, java.lang.Object arg1, int arg2, int arg3) {}
	public void onSpanAdded(android.text.Spannable arg0, java.lang.Object arg1, int arg2, int arg3) {}
}
