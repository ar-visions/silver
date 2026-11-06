package android.text.method;

import android.text.method.NumberKeyListener;

public class DigitsKeyListener extends NumberKeyListener {
	public DigitsKeyListener() {}

	public DigitsKeyListener(boolean sign, boolean decimal) {}

	public static DigitsKeyListener getInstance(String locale) {
		return new DigitsKeyListener();
	}
	public java.lang.CharSequence filter(java.lang.CharSequence arg0, int arg1, int arg2, android.text.Spanned arg3, int arg4, int arg5) { return null; }
}
