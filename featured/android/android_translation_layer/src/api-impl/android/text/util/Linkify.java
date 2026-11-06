package android.text.util;

import android.text.Spannable;
import android.widget.TextView;
import java.util.regex.Pattern;

public class Linkify {

	public static MatchFilter sUrlMatchFilter = null;
	public static MatchFilter sPhoneNumberMatchFilter = null;

	public static final boolean addLinks(Spannable text, int mask) { return true; }
	public static final boolean addLinks(TextView text, int mask) { return true; }
	public static final boolean addLinks(Spannable text, Pattern urlPattern, String urlScheme) { return true; }

	public interface MatchFilter {}
	public interface TransformFilter {}

	// GEN member:android.text.util.Linkify:public static final boolean addLinks(android.text.Spannable, java.util.regex.Pattern, java.lang.String, android.text.util.Linkify$MatchFilter, android.text.util.Linkify$TransformFilter);
	public static final boolean addLinks(android.text.Spannable arg0, java.util.regex.Pattern arg1, java.lang.String arg2, android.text.util.Linkify.MatchFilter arg3, android.text.util.Linkify.TransformFilter arg4) { return false; }
}
