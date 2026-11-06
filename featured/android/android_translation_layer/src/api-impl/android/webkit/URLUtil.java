package android.webkit;

public class URLUtil {

	public static String guessFileName(String url, String contentDisposition, String mimeType) {
		String filename = url.substring(url.lastIndexOf('/') + 1);
		if (filename.contains("?"))
			filename = filename.substring(0, filename.indexOf('?'));
		return filename;
	}

	/**
	 * @return {@code true} if the url is an https: url.
	 */
	public static boolean isHttpsUrl(String url) {
		return (null != url)
		    && (url.length() > 7)
		    && url.substring(0, 8).equalsIgnoreCase("https://");
	}

	public static boolean isHttpUrl(String url) {
		return (null != url)
		    && (url.length() > 6)
		    && url.substring(0, 7).equalsIgnoreCase("http://");
	}

	public static boolean isContentUrl(String url) {
		return url.startsWith("content://");
	}

	public static boolean isValidUrl(String url) {
		if (url == null)
			return false;
		return isHttpUrl(url) || isHttpsUrl(url) || url.startsWith("file://") || url.startsWith("about:")
		    || url.startsWith("data:") || url.startsWith("javascript:");
	}

	// GEN member:android.webkit.URLUtil:public static boolean isAboutUrl(java.lang.String);
	public static boolean isAboutUrl(java.lang.String arg0) { return false; }

	// GEN member:android.webkit.URLUtil:public static boolean isAssetUrl(java.lang.String);
	public static boolean isAssetUrl(java.lang.String arg0) { return false; }

	// GEN member:android.webkit.URLUtil:public static boolean isNetworkUrl(java.lang.String);
	public static boolean isNetworkUrl(java.lang.String arg0) { return false; }
}
