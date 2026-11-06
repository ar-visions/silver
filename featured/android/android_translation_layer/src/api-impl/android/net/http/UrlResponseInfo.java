package android.net.http;

// android.jar's shape, made by gen.py
public abstract class UrlResponseInfo {
	// GEN member:android.net.http.UrlResponseInfo:public android.net.http.UrlResponseInfo();
	public UrlResponseInfo() {}

	// GEN member:android.net.http.UrlResponseInfo:public abstract java.lang.String getUrl();
	public abstract java.lang.String getUrl();

	// GEN member:android.net.http.UrlResponseInfo:public abstract java.util.List<java.lang.String> getUrlChain();
	public abstract java.util.List<java.lang.String> getUrlChain();

	// GEN member:android.net.http.UrlResponseInfo:public abstract int getHttpStatusCode();
	public abstract int getHttpStatusCode();

	// GEN member:android.net.http.UrlResponseInfo:public abstract java.lang.String getHttpStatusText();
	public abstract java.lang.String getHttpStatusText();

	// GEN member:android.net.http.UrlResponseInfo:public abstract android.net.http.HeaderBlock getHeaders();
	public abstract android.net.http.HeaderBlock getHeaders();

	// GEN member:android.net.http.UrlResponseInfo:public abstract boolean wasCached();
	public abstract boolean wasCached();

	// GEN member:android.net.http.UrlResponseInfo:public abstract java.lang.String getNegotiatedProtocol();
	public abstract java.lang.String getNegotiatedProtocol();

	// GEN member:android.net.http.UrlResponseInfo:public abstract long getReceivedByteCount();
	public abstract long getReceivedByteCount();
}
