package android.net;

import java.util.Collections;
import java.util.Set;

public class UrlQuerySanitizer {

	public UrlQuerySanitizer(String url) {}

	public Set<String> getParameterSet() {
		return Collections.emptySet();
	}

	// GEN class:android.net.UrlQuerySanitizer$ValueSanitizer
	public interface ValueSanitizer {
		// GEN member:android.net.UrlQuerySanitizer$ValueSanitizer:public abstract java.lang.String sanitize(java.lang.String);
		default java.lang.String sanitize(java.lang.String arg0) { return null; }
	}


	// GEN member:android.net.UrlQuerySanitizer:public static final android.net.UrlQuerySanitizer$ValueSanitizer getAllButNulAndAngleBracketsLegal();
	public static final android.net.UrlQuerySanitizer.ValueSanitizer getAllButNulAndAngleBracketsLegal() { return null; }

	// GEN member:android.net.UrlQuerySanitizer:public static final android.net.UrlQuerySanitizer$ValueSanitizer getAllIllegal();
	public static final android.net.UrlQuerySanitizer.ValueSanitizer getAllIllegal() { return null; }

	// GEN member:android.net.UrlQuerySanitizer:public java.lang.String getValue(java.lang.String);
	public java.lang.String getValue(java.lang.String arg0) { return null; }

	// GEN member:android.net.UrlQuerySanitizer:public boolean hasParameter(java.lang.String);
	public boolean hasParameter(java.lang.String arg0) { return false; }

	// GEN member:android.net.UrlQuerySanitizer:public void parseQuery(java.lang.String);
	public void parseQuery(java.lang.String arg0) {}

	// GEN member:android.net.UrlQuerySanitizer:public void parseUrl(java.lang.String);
	public void parseUrl(java.lang.String arg0) {}
}
