package android.net.http;

// android.jar's shape, made by gen.py
public class QuicOptions {
	// GEN member:android.net.http.QuicOptions:public java.util.Set<java.lang.String> getAllowedQuicHosts();
	public java.util.Set<java.lang.String> getAllowedQuicHosts() { return new java.util.HashSet<>(); }

	// GEN member:android.net.http.QuicOptions:public boolean hasInMemoryServerConfigsCacheSize();
	public boolean hasInMemoryServerConfigsCacheSize() { return false; }

	// GEN member:android.net.http.QuicOptions:public int getInMemoryServerConfigsCacheSize();
	public int getInMemoryServerConfigsCacheSize() { return 0; }

	// GEN member:android.net.http.QuicOptions:public java.lang.String getHandshakeUserAgent();
	public java.lang.String getHandshakeUserAgent() { return null; }

	// GEN member:android.net.http.QuicOptions:public java.time.Duration getIdleConnectionTimeout();
	public java.time.Duration getIdleConnectionTimeout() { return null; }

	protected QuicOptions() {}

	// GEN class:android.net.http.QuicOptions$Builder
	public final class Builder {
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions$Builder();
		public Builder() {}
	
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions$Builder addAllowedQuicHost(java.lang.String);
		public android.net.http.QuicOptions.Builder addAllowedQuicHost(java.lang.String arg0) { return this; }
	
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions$Builder setInMemoryServerConfigsCacheSize(int);
		public android.net.http.QuicOptions.Builder setInMemoryServerConfigsCacheSize(int arg0) { return this; }
	
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions$Builder setHandshakeUserAgent(java.lang.String);
		public android.net.http.QuicOptions.Builder setHandshakeUserAgent(java.lang.String arg0) { return this; }
	
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions$Builder setIdleConnectionTimeout(java.time.Duration);
		public android.net.http.QuicOptions.Builder setIdleConnectionTimeout(java.time.Duration arg0) { return this; }
	
		// GEN member:android.net.http.QuicOptions$Builder:public android.net.http.QuicOptions build();
		public android.net.http.QuicOptions build() { return null; }
	}

}
