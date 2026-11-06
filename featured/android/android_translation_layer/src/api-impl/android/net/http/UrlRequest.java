package android.net.http;

// android.jar's shape, made by gen.py
public abstract class UrlRequest {
	// GEN member:android.net.http.UrlRequest:public static final int REQUEST_PRIORITY_HIGHEST = 4;
	public static final int REQUEST_PRIORITY_HIGHEST = 4;

	// GEN member:android.net.http.UrlRequest:public static final int REQUEST_PRIORITY_IDLE = 0;
	public static final int REQUEST_PRIORITY_IDLE = 0;

	// GEN member:android.net.http.UrlRequest:public static final int REQUEST_PRIORITY_LOW = 2;
	public static final int REQUEST_PRIORITY_LOW = 2;

	// GEN member:android.net.http.UrlRequest:public static final int REQUEST_PRIORITY_LOWEST = 1;
	public static final int REQUEST_PRIORITY_LOWEST = 1;

	// GEN member:android.net.http.UrlRequest:public static final int REQUEST_PRIORITY_MEDIUM = 3;
	public static final int REQUEST_PRIORITY_MEDIUM = 3;

	// GEN member:android.net.http.UrlRequest:public abstract java.lang.String getHttpMethod();
	public abstract java.lang.String getHttpMethod();

	// GEN member:android.net.http.UrlRequest:public abstract android.net.http.HeaderBlock getHeaders();
	public abstract android.net.http.HeaderBlock getHeaders();

	// GEN member:android.net.http.UrlRequest:public abstract boolean isCacheDisabled();
	public abstract boolean isCacheDisabled();

	// GEN member:android.net.http.UrlRequest:public abstract boolean isDirectExecutorAllowed();
	public abstract boolean isDirectExecutorAllowed();

	// GEN member:android.net.http.UrlRequest:public abstract int getPriority();
	public abstract int getPriority();

	// GEN member:android.net.http.UrlRequest:public abstract boolean hasTrafficStatsTag();
	public abstract boolean hasTrafficStatsTag();

	// GEN member:android.net.http.UrlRequest:public abstract int getTrafficStatsTag();
	public abstract int getTrafficStatsTag();

	// GEN member:android.net.http.UrlRequest:public abstract boolean hasTrafficStatsUid();
	public abstract boolean hasTrafficStatsUid();

	// GEN member:android.net.http.UrlRequest:public abstract int getTrafficStatsUid();
	public abstract int getTrafficStatsUid();

	// GEN member:android.net.http.UrlRequest:public abstract void start();
	public abstract void start();

	// GEN member:android.net.http.UrlRequest:public abstract void followRedirect();
	public abstract void followRedirect();

	// GEN member:android.net.http.UrlRequest:public abstract void read(java.nio.ByteBuffer);
	public abstract void read(java.nio.ByteBuffer arg0);

	// GEN member:android.net.http.UrlRequest:public abstract void cancel();
	public abstract void cancel();

	// GEN member:android.net.http.UrlRequest:public abstract boolean isDone();
	public abstract boolean isDone();

	protected UrlRequest() {}

	// GEN class:android.net.http.UrlRequest$Builder
	public abstract class Builder {
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setHttpMethod(java.lang.String);
		public abstract android.net.http.UrlRequest.Builder setHttpMethod(java.lang.String arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder addHeader(java.lang.String, java.lang.String);
		public abstract android.net.http.UrlRequest.Builder addHeader(java.lang.String arg0, java.lang.String arg1);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setCacheDisabled(boolean);
		public abstract android.net.http.UrlRequest.Builder setCacheDisabled(boolean arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setPriority(int);
		public abstract android.net.http.UrlRequest.Builder setPriority(int arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setDirectExecutorAllowed(boolean);
		public abstract android.net.http.UrlRequest.Builder setDirectExecutorAllowed(boolean arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder bindToNetwork(android.net.Network);
		public abstract android.net.http.UrlRequest.Builder bindToNetwork(android.net.Network arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setTrafficStatsTag(int);
		public abstract android.net.http.UrlRequest.Builder setTrafficStatsTag(int arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setTrafficStatsUid(int);
		public abstract android.net.http.UrlRequest.Builder setTrafficStatsUid(int arg0);
	
		// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest build();
		public abstract android.net.http.UrlRequest build();
	
		protected Builder() {}
	
	// GEN member:android.net.http.UrlRequest$Builder:public abstract android.net.http.UrlRequest$Builder setUploadDataProvider(android.net.http.UploadDataProvider, java.util.concurrent.Executor);
	public android.net.http.UrlRequest.Builder setUploadDataProvider(android.net.http.UploadDataProvider arg0, java.util.concurrent.Executor arg1) { return this; }
}


	// GEN class:android.net.http.UrlRequest$Callback
	public interface Callback {
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onRedirectReceived(android.net.http.UrlRequest, android.net.http.UrlResponseInfo, java.lang.String) throws java.lang.Exception;
		default void onRedirectReceived(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1, java.lang.String arg2) throws java.lang.Exception {}
	
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onResponseStarted(android.net.http.UrlRequest, android.net.http.UrlResponseInfo) throws java.lang.Exception;
		default void onResponseStarted(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1) throws java.lang.Exception {}
	
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onReadCompleted(android.net.http.UrlRequest, android.net.http.UrlResponseInfo, java.nio.ByteBuffer) throws java.lang.Exception;
		default void onReadCompleted(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1, java.nio.ByteBuffer arg2) throws java.lang.Exception {}
	
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onSucceeded(android.net.http.UrlRequest, android.net.http.UrlResponseInfo);
		default void onSucceeded(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1) {}
	
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onFailed(android.net.http.UrlRequest, android.net.http.UrlResponseInfo, android.net.http.HttpException);
		default void onFailed(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1, android.net.http.HttpException arg2) {}
	
		// GEN member:android.net.http.UrlRequest$Callback:public abstract void onCanceled(android.net.http.UrlRequest, android.net.http.UrlResponseInfo);
		default void onCanceled(android.net.http.UrlRequest arg0, android.net.http.UrlResponseInfo arg1) {}
	}


	// GEN class:android.net.http.UrlRequest$StatusListener
	public interface StatusListener {
		// GEN member:android.net.http.UrlRequest$StatusListener:public abstract void onStatus(int);
		default void onStatus(int arg0) {}
	}

}
