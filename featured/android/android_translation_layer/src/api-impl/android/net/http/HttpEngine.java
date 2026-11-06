package android.net.http;

// android.jar's shape, made by gen.py
public abstract class HttpEngine {
	// GEN member:android.net.http.HttpEngine:public static java.lang.String getVersionString();
	public static java.lang.String getVersionString() { return null; }

	// GEN member:android.net.http.HttpEngine:public abstract void shutdown();
	public abstract void shutdown();

	// GEN member:android.net.http.HttpEngine:public void bindToNetwork(android.net.Network);
	public void bindToNetwork(android.net.Network arg0) {}

	// GEN member:android.net.http.HttpEngine:public abstract java.net.URLConnection openConnection(java.net.URL) throws java.io.IOException;
	public abstract java.net.URLConnection openConnection(java.net.URL arg0) throws java.io.IOException;

	// GEN member:android.net.http.HttpEngine:public abstract java.net.URLStreamHandlerFactory createUrlStreamHandlerFactory();
	public abstract java.net.URLStreamHandlerFactory createUrlStreamHandlerFactory();

	protected HttpEngine() {}

	// GEN class:android.net.http.HttpEngine$Builder
	public class Builder {
		// GEN member:android.net.http.HttpEngine$Builder:public static final int HTTP_CACHE_DISABLED = 0;
		public static final int HTTP_CACHE_DISABLED = 0;
	
		// GEN member:android.net.http.HttpEngine$Builder:public static final int HTTP_CACHE_DISK = 3;
		public static final int HTTP_CACHE_DISK = 3;
	
		// GEN member:android.net.http.HttpEngine$Builder:public static final int HTTP_CACHE_DISK_NO_HTTP = 2;
		public static final int HTTP_CACHE_DISK_NO_HTTP = 2;
	
		// GEN member:android.net.http.HttpEngine$Builder:public static final int HTTP_CACHE_IN_MEMORY = 1;
		public static final int HTTP_CACHE_IN_MEMORY = 1;
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder(android.content.Context);
		public Builder(android.content.Context arg0) {}
	
		// GEN member:android.net.http.HttpEngine$Builder:public java.lang.String getDefaultUserAgent();
		public java.lang.String getDefaultUserAgent() { return null; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setUserAgent(java.lang.String);
		public android.net.http.HttpEngine.Builder setUserAgent(java.lang.String arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setStoragePath(java.lang.String);
		public android.net.http.HttpEngine.Builder setStoragePath(java.lang.String arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setEnableQuic(boolean);
		public android.net.http.HttpEngine.Builder setEnableQuic(boolean arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setEnableHttp2(boolean);
		public android.net.http.HttpEngine.Builder setEnableHttp2(boolean arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setEnableBrotli(boolean);
		public android.net.http.HttpEngine.Builder setEnableBrotli(boolean arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setEnableHttpCache(int, long);
		public android.net.http.HttpEngine.Builder setEnableHttpCache(int arg0, long arg1) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder addQuicHint(java.lang.String, int, int);
		public android.net.http.HttpEngine.Builder addQuicHint(java.lang.String arg0, int arg1, int arg2) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder addPublicKeyPins(java.lang.String, java.util.Set<byte[]>, boolean, java.time.Instant);
		public android.net.http.HttpEngine.Builder addPublicKeyPins(java.lang.String arg0, java.util.Set<byte[]> arg1, boolean arg2, java.time.Instant arg3) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setEnablePublicKeyPinningBypassForLocalTrustAnchors(boolean);
		public android.net.http.HttpEngine.Builder setEnablePublicKeyPinningBypassForLocalTrustAnchors(boolean arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setQuicOptions(android.net.http.QuicOptions);
		public android.net.http.HttpEngine.Builder setQuicOptions(android.net.http.QuicOptions arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setDnsOptions(android.net.http.DnsOptions);
		public android.net.http.HttpEngine.Builder setDnsOptions(android.net.http.DnsOptions arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine$Builder setConnectionMigrationOptions(android.net.http.ConnectionMigrationOptions);
		public android.net.http.HttpEngine.Builder setConnectionMigrationOptions(android.net.http.ConnectionMigrationOptions arg0) { return this; }
	
		// GEN member:android.net.http.HttpEngine$Builder:public android.net.http.HttpEngine build();
		public android.net.http.HttpEngine build() { return null; }
	
		protected Builder() {}
	}

}
