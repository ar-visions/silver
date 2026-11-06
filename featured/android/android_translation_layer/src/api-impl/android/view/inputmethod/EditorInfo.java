package android.view.inputmethod;

import android.os.Bundle;
import android.os.LocaleList;

public class EditorInfo implements android.os.Parcelable, android.text.InputType {
	public int actionId = 0;
	public CharSequence actionLabel = null;
	public Bundle extras = null;
	public int fieldId = 0;
	public String fieldName = null;
	public CharSequence hintText = null;
	public int imeOptions = 0x0;
	public int initialCapsMode = 0;
	public int initialSelStart = -1;
	public int initialSelEnd = -1;
	public int inputType = /*0x0*/ 0x00000001; /* TYPE_NULL */ /* TYPE_CLASS_TEXT */
	public CharSequence label = null;
	public String packageName = "com.example.FIXME";
	public String privateImeOptions = null;
	public LocaleList hintLocales = null;

	// GEN member:android.view.inputmethod.EditorInfo:public java.lang.String[] contentMimeTypes;
	public java.lang.String[] contentMimeTypes;

	// GEN member:android.view.inputmethod.EditorInfo:public void setInitialSurroundingSubText(java.lang.CharSequence, int);
	public void setInitialSurroundingSubText(java.lang.CharSequence arg0, int arg1) {}

	// GEN member:android.view.inputmethod.EditorInfo:public void setStylusHandwritingEnabled(boolean);
	public void setStylusHandwritingEnabled(boolean arg0) {}

	// GEN member:android.view.inputmethod.EditorInfo:public void setSupportedHandwritingGesturePreviews(java.util.Set<java.lang.Class<? extends android.view.inputmethod.PreviewableHandwritingGesture>>);
	public void setSupportedHandwritingGesturePreviews(java.util.Set<java.lang.Class<? extends android.view.inputmethod.PreviewableHandwritingGesture>> arg0) {}

	// GEN member:android.view.inputmethod.EditorInfo:public void setSupportedHandwritingGestures(java.util.List<java.lang.Class<? extends android.view.inputmethod.HandwritingGesture>>);
	public void setSupportedHandwritingGestures(java.util.List<java.lang.Class<? extends android.view.inputmethod.HandwritingGesture>> arg0) {}
}
