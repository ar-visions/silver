package android.telephony;

public class PhoneNumberUtils {

	public static boolean isNonSeparator(char c) {
		return c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '-';
	}

	public static boolean isGlobalPhoneNumber(String phoneNumber) {
		return phoneNumber != null && (phoneNumber.startsWith("+") || phoneNumber.startsWith("00"));
	}

	public static String stripSeparators(String phoneNumber) {
		return phoneNumber.replaceAll("[ \t\n\r-]", "");
	}

	// GEN member:android.telephony.PhoneNumberUtils:public static java.lang.String formatNumber(java.lang.String, java.lang.String);
	public static java.lang.String formatNumber(java.lang.String arg0, java.lang.String arg1) { return null; }

	// GEN member:android.telephony.PhoneNumberUtils:public static java.lang.String formatNumberToE164(java.lang.String, java.lang.String);
	public static java.lang.String formatNumberToE164(java.lang.String arg0, java.lang.String arg1) { return null; }

	// GEN member:android.telephony.PhoneNumberUtils:public static java.lang.String normalizeNumber(java.lang.String);
	public static java.lang.String normalizeNumber(java.lang.String arg0) { return null; }
}
