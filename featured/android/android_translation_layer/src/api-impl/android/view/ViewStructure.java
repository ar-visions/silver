package android.view;

// android.jar's shape, made by gen.py
public abstract class ViewStructure {
	// GEN member:android.view.ViewStructure:public android.view.ViewStructure();
	public ViewStructure() {}

	// GEN member:android.view.ViewStructure:public abstract void setId(int, java.lang.String, java.lang.String, java.lang.String);
	public abstract void setId(int arg0, java.lang.String arg1, java.lang.String arg2, java.lang.String arg3);

	// GEN member:android.view.ViewStructure:public abstract void setDimens(int, int, int, int, int, int);
	public abstract void setDimens(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5);

	// GEN member:android.view.ViewStructure:public abstract void setTransformation(android.graphics.Matrix);
	public abstract void setTransformation(android.graphics.Matrix arg0);

	// GEN member:android.view.ViewStructure:public abstract void setElevation(float);
	public abstract void setElevation(float arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAlpha(float);
	public abstract void setAlpha(float arg0);

	// GEN member:android.view.ViewStructure:public abstract void setVisibility(int);
	public abstract void setVisibility(int arg0);

	// GEN member:android.view.ViewStructure:public abstract void setEnabled(boolean);
	public abstract void setEnabled(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setClickable(boolean);
	public abstract void setClickable(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setLongClickable(boolean);
	public abstract void setLongClickable(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setContextClickable(boolean);
	public abstract void setContextClickable(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setFocusable(boolean);
	public abstract void setFocusable(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setFocused(boolean);
	public abstract void setFocused(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAccessibilityFocused(boolean);
	public abstract void setAccessibilityFocused(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setCheckable(boolean);
	public abstract void setCheckable(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setChecked(boolean);
	public abstract void setChecked(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setSelected(boolean);
	public abstract void setSelected(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setActivated(boolean);
	public abstract void setActivated(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setOpaque(boolean);
	public abstract void setOpaque(boolean arg0);

	// GEN member:android.view.ViewStructure:public abstract void setClassName(java.lang.String);
	public abstract void setClassName(java.lang.String arg0);

	// GEN member:android.view.ViewStructure:public abstract void setContentDescription(java.lang.CharSequence);
	public abstract void setContentDescription(java.lang.CharSequence arg0);

	// GEN member:android.view.ViewStructure:public abstract void setText(java.lang.CharSequence);
	public abstract void setText(java.lang.CharSequence arg0);

	// GEN member:android.view.ViewStructure:public abstract void setText(java.lang.CharSequence, int, int);
	public abstract void setText(java.lang.CharSequence arg0, int arg1, int arg2);

	// GEN member:android.view.ViewStructure:public abstract void setTextStyle(float, int, int, int);
	public abstract void setTextStyle(float arg0, int arg1, int arg2, int arg3);

	// GEN member:android.view.ViewStructure:public abstract void setTextLines(int[], int[]);
	public abstract void setTextLines(int[] arg0, int[] arg1);

	// GEN member:android.view.ViewStructure:public void setTextIdEntry(java.lang.String);
	public void setTextIdEntry(java.lang.String arg0) {}

	// GEN member:android.view.ViewStructure:public abstract void setHint(java.lang.CharSequence);
	public abstract void setHint(java.lang.CharSequence arg0);

	// GEN member:android.view.ViewStructure:public void setHintIdEntry(java.lang.String);
	public void setHintIdEntry(java.lang.String arg0) {}

	// GEN member:android.view.ViewStructure:public abstract java.lang.CharSequence getText();
	public abstract java.lang.CharSequence getText();

	// GEN member:android.view.ViewStructure:public abstract int getTextSelectionStart();
	public abstract int getTextSelectionStart();

	// GEN member:android.view.ViewStructure:public abstract int getTextSelectionEnd();
	public abstract int getTextSelectionEnd();

	// GEN member:android.view.ViewStructure:public abstract java.lang.CharSequence getHint();
	public abstract java.lang.CharSequence getHint();

	// GEN member:android.view.ViewStructure:public abstract android.os.Bundle getExtras();
	public abstract android.os.Bundle getExtras();

	// GEN member:android.view.ViewStructure:public abstract boolean hasExtras();
	public abstract boolean hasExtras();

	// GEN member:android.view.ViewStructure:public abstract void setChildCount(int);
	public abstract void setChildCount(int arg0);

	// GEN member:android.view.ViewStructure:public abstract int addChildCount(int);
	public abstract int addChildCount(int arg0);

	// GEN member:android.view.ViewStructure:public abstract int getChildCount();
	public abstract int getChildCount();

	// GEN member:android.view.ViewStructure:public abstract android.view.ViewStructure newChild(int);
	public abstract android.view.ViewStructure newChild(int arg0);

	// GEN member:android.view.ViewStructure:public abstract android.view.ViewStructure asyncNewChild(int);
	public abstract android.view.ViewStructure asyncNewChild(int arg0);

	// GEN member:android.view.ViewStructure:public android.credentials.GetCredentialRequest getPendingCredentialRequest();
	public android.credentials.GetCredentialRequest getPendingCredentialRequest() { return null; }

	// GEN member:android.view.ViewStructure:public abstract android.view.autofill.AutofillId getAutofillId();
	public abstract android.view.autofill.AutofillId getAutofillId();

	// GEN member:android.view.ViewStructure:public abstract void setAutofillId(android.view.autofill.AutofillId);
	public abstract void setAutofillId(android.view.autofill.AutofillId arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAutofillId(android.view.autofill.AutofillId, int);
	public abstract void setAutofillId(android.view.autofill.AutofillId arg0, int arg1);

	// GEN member:android.view.ViewStructure:public abstract void setAutofillType(int);
	public abstract void setAutofillType(int arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAutofillHints(java.lang.String[]);
	public abstract void setAutofillHints(java.lang.String[] arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAutofillValue(android.view.autofill.AutofillValue);
	public abstract void setAutofillValue(android.view.autofill.AutofillValue arg0);

	// GEN member:android.view.ViewStructure:public abstract void setAutofillOptions(java.lang.CharSequence[]);
	public abstract void setAutofillOptions(java.lang.CharSequence[] arg0);

	// GEN member:android.view.ViewStructure:public void setImportantForAutofill(int);
	public void setImportantForAutofill(int arg0) {}

	// GEN member:android.view.ViewStructure:public void setReceiveContentMimeTypes(java.lang.String[]);
	public void setReceiveContentMimeTypes(java.lang.String[] arg0) {}

	// GEN member:android.view.ViewStructure:public abstract void setInputType(int);
	public abstract void setInputType(int arg0);

	// GEN member:android.view.ViewStructure:public abstract void setDataIsSensitive(boolean);
	public abstract void setDataIsSensitive(boolean arg0);

	// GEN member:android.view.ViewStructure:public void setMinTextEms(int);
	public void setMinTextEms(int arg0) {}

	// GEN member:android.view.ViewStructure:public void setMaxTextEms(int);
	public void setMaxTextEms(int arg0) {}

	// GEN member:android.view.ViewStructure:public void setMaxTextLength(int);
	public void setMaxTextLength(int arg0) {}

	// GEN member:android.view.ViewStructure:public abstract void asyncCommit();
	public abstract void asyncCommit();

	// GEN member:android.view.ViewStructure:public abstract void setWebDomain(java.lang.String);
	public abstract void setWebDomain(java.lang.String arg0);

	// GEN member:android.view.ViewStructure:public abstract void setLocaleList(android.os.LocaleList);
	public abstract void setLocaleList(android.os.LocaleList arg0);

	// GEN member:android.view.ViewStructure:public void clearCredentialManagerRequest();
	public void clearCredentialManagerRequest() {}
}
