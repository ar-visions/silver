package android.view.autofill;

public class AutofillValue implements android.os.Parcelable {

	public static AutofillValue forText(CharSequence text) {
		return new AutofillValue();
	}

	public static AutofillValue forToggle(boolean enabled) {
		return new AutofillValue();
	}

	// GEN member:android.view.autofill.AutofillValue:public java.lang.CharSequence getTextValue();
	public java.lang.CharSequence getTextValue() { return null; }

	// GEN member:android.view.autofill.AutofillValue:public boolean getToggleValue();
	public boolean getToggleValue() { return false; }

	// GEN member:android.view.autofill.AutofillValue:public boolean isDate();
	public boolean isDate() { return false; }

	// GEN member:android.view.autofill.AutofillValue:public boolean isList();
	public boolean isList() { return false; }

	// GEN member:android.view.autofill.AutofillValue:public boolean isText();
	public boolean isText() { return false; }

	// GEN member:android.view.autofill.AutofillValue:public boolean isToggle();
	public boolean isToggle() { return false; }
}
