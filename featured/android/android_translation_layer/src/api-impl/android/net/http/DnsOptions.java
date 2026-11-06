package android.net.http;

// android.jar's shape, made by gen.py
public final class DnsOptions {
	// GEN member:android.net.http.DnsOptions:public static final int DNS_OPTION_DISABLED = 2;
	public static final int DNS_OPTION_DISABLED = 2;

	// GEN member:android.net.http.DnsOptions:public static final int DNS_OPTION_ENABLED = 1;
	public static final int DNS_OPTION_ENABLED = 1;

	// GEN member:android.net.http.DnsOptions:public static final int DNS_OPTION_UNSPECIFIED = 0;
	public static final int DNS_OPTION_UNSPECIFIED = 0;

	// GEN member:android.net.http.DnsOptions:public int getUseHttpStackDnsResolver();
	public int getUseHttpStackDnsResolver() { return 0; }

	// GEN member:android.net.http.DnsOptions:public int getPersistHostCache();
	public int getPersistHostCache() { return 0; }

	// GEN member:android.net.http.DnsOptions:public int getStaleDns();
	public int getStaleDns() { return 0; }

	// GEN member:android.net.http.DnsOptions:public java.time.Duration getPersistHostCachePeriod();
	public java.time.Duration getPersistHostCachePeriod() { return null; }

	// GEN member:android.net.http.DnsOptions:public int getPreestablishConnectionsToStaleDnsResults();
	public int getPreestablishConnectionsToStaleDnsResults() { return 0; }

	// GEN member:android.net.http.DnsOptions:public android.net.http.DnsOptions$StaleDnsOptions getStaleDnsOptions();
	public android.net.http.DnsOptions.StaleDnsOptions getStaleDnsOptions() { return null; }

	protected DnsOptions() {}

	// GEN class:android.net.http.DnsOptions$Builder
	public final class Builder {
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder();
		public Builder() {}
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setUseHttpStackDnsResolver(int);
		public android.net.http.DnsOptions.Builder setUseHttpStackDnsResolver(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setStaleDns(int);
		public android.net.http.DnsOptions.Builder setStaleDns(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setStaleDnsOptions(android.net.http.DnsOptions$StaleDnsOptions);
		public android.net.http.DnsOptions.Builder setStaleDnsOptions(android.net.http.DnsOptions.StaleDnsOptions arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setPreestablishConnectionsToStaleDnsResults(int);
		public android.net.http.DnsOptions.Builder setPreestablishConnectionsToStaleDnsResults(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setPersistHostCache(int);
		public android.net.http.DnsOptions.Builder setPersistHostCache(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions$Builder setPersistHostCachePeriod(java.time.Duration);
		public android.net.http.DnsOptions.Builder setPersistHostCachePeriod(java.time.Duration arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$Builder:public android.net.http.DnsOptions build();
		public android.net.http.DnsOptions build() { return null; }
	}


	// GEN class:android.net.http.DnsOptions$StaleDnsOptions
	public class StaleDnsOptions {
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions:public java.time.Duration getFreshLookupTimeout();
		public java.time.Duration getFreshLookupTimeout() { return null; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions:public java.time.Duration getMaxExpiredDelay();
		public java.time.Duration getMaxExpiredDelay() { return null; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions:public int getAllowCrossNetworkUsage();
		public int getAllowCrossNetworkUsage() { return 0; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions:public int getUseStaleOnNameNotResolved();
		public int getUseStaleOnNameNotResolved() { return 0; }
	
		protected StaleDnsOptions() {}
	
	// GEN class:android.net.http.DnsOptions$StaleDnsOptions$Builder
	public final class Builder {
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions$Builder();
		public Builder() {}
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions$Builder setFreshLookupTimeout(java.time.Duration);
		public android.net.http.DnsOptions.StaleDnsOptions.Builder setFreshLookupTimeout(java.time.Duration arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions$Builder setMaxExpiredDelay(java.time.Duration);
		public android.net.http.DnsOptions.StaleDnsOptions.Builder setMaxExpiredDelay(java.time.Duration arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions$Builder setAllowCrossNetworkUsage(int);
		public android.net.http.DnsOptions.StaleDnsOptions.Builder setAllowCrossNetworkUsage(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions$Builder setUseStaleOnNameNotResolved(int);
		public android.net.http.DnsOptions.StaleDnsOptions.Builder setUseStaleOnNameNotResolved(int arg0) { return this; }
	
		// GEN member:android.net.http.DnsOptions$StaleDnsOptions$Builder:public android.net.http.DnsOptions$StaleDnsOptions build();
		public android.net.http.DnsOptions.StaleDnsOptions build() { return null; }
	}

}

}
