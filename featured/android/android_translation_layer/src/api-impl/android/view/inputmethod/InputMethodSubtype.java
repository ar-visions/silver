package android.view.inputmethod;

import java.util.Locale;

// the PC's keyboard: its language is the system's
public class InputMethodSubtype implements android.os.Parcelable {

	public String getMode() {
		return "keyboard";
	}

	public String getLocale() {
		return Locale.getDefault().toString();
	}

	public String getLanguageTag() {
		return Locale.getDefault().toLanguageTag();
	}

	public String getExtraValue() {
		return "";
	}

	public boolean isAuxiliary() {
		return false;
	}
}
