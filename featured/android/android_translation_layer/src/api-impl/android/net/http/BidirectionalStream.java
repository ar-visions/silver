package android.net.http;

// android.jar's shape, made by gen.py
public abstract class BidirectionalStream {
	// GEN member:android.net.http.BidirectionalStream:public static final int STREAM_PRIORITY_HIGHEST = 4;
	public static final int STREAM_PRIORITY_HIGHEST = 4;

	// GEN member:android.net.http.BidirectionalStream:public static final int STREAM_PRIORITY_IDLE = 0;
	public static final int STREAM_PRIORITY_IDLE = 0;

	// GEN member:android.net.http.BidirectionalStream:public static final int STREAM_PRIORITY_LOW = 2;
	public static final int STREAM_PRIORITY_LOW = 2;

	// GEN member:android.net.http.BidirectionalStream:public static final int STREAM_PRIORITY_LOWEST = 1;
	public static final int STREAM_PRIORITY_LOWEST = 1;

	// GEN member:android.net.http.BidirectionalStream:public static final int STREAM_PRIORITY_MEDIUM = 3;
	public static final int STREAM_PRIORITY_MEDIUM = 3;

	// GEN member:android.net.http.BidirectionalStream:public android.net.http.BidirectionalStream();
	public BidirectionalStream() {}

	// GEN member:android.net.http.BidirectionalStream:public abstract java.lang.String getHttpMethod();
	public abstract java.lang.String getHttpMethod();

	// GEN member:android.net.http.BidirectionalStream:public abstract boolean hasTrafficStatsTag();
	public abstract boolean hasTrafficStatsTag();

	// GEN member:android.net.http.BidirectionalStream:public abstract int getTrafficStatsTag();
	public abstract int getTrafficStatsTag();

	// GEN member:android.net.http.BidirectionalStream:public abstract boolean hasTrafficStatsUid();
	public abstract boolean hasTrafficStatsUid();

	// GEN member:android.net.http.BidirectionalStream:public abstract int getTrafficStatsUid();
	public abstract int getTrafficStatsUid();

	// GEN member:android.net.http.BidirectionalStream:public abstract android.net.http.HeaderBlock getHeaders();
	public abstract android.net.http.HeaderBlock getHeaders();

	// GEN member:android.net.http.BidirectionalStream:public abstract int getPriority();
	public abstract int getPriority();

	// GEN member:android.net.http.BidirectionalStream:public abstract boolean isDelayRequestHeadersUntilFirstFlushEnabled();
	public abstract boolean isDelayRequestHeadersUntilFirstFlushEnabled();

	// GEN member:android.net.http.BidirectionalStream:public abstract void start();
	public abstract void start();

	// GEN member:android.net.http.BidirectionalStream:public abstract void read(java.nio.ByteBuffer);
	public abstract void read(java.nio.ByteBuffer arg0);

	// GEN member:android.net.http.BidirectionalStream:public abstract void write(java.nio.ByteBuffer, boolean);
	public abstract void write(java.nio.ByteBuffer arg0, boolean arg1);

	// GEN member:android.net.http.BidirectionalStream:public abstract void flush();
	public abstract void flush();

	// GEN member:android.net.http.BidirectionalStream:public abstract void cancel();
	public abstract void cancel();

	// GEN member:android.net.http.BidirectionalStream:public abstract boolean isDone();
	public abstract boolean isDone();

	// GEN class:android.net.http.BidirectionalStream$Builder
	public abstract class Builder {
		// GEN member:android.net.http.BidirectionalStream$Builder:public android.net.http.BidirectionalStream$Builder();
		public Builder() {}
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder setHttpMethod(java.lang.String);
		public abstract android.net.http.BidirectionalStream.Builder setHttpMethod(java.lang.String arg0);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder addHeader(java.lang.String, java.lang.String);
		public abstract android.net.http.BidirectionalStream.Builder addHeader(java.lang.String arg0, java.lang.String arg1);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder setPriority(int);
		public abstract android.net.http.BidirectionalStream.Builder setPriority(int arg0);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder setDelayRequestHeadersUntilFirstFlushEnabled(boolean);
		public abstract android.net.http.BidirectionalStream.Builder setDelayRequestHeadersUntilFirstFlushEnabled(boolean arg0);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder setTrafficStatsTag(int);
		public abstract android.net.http.BidirectionalStream.Builder setTrafficStatsTag(int arg0);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream$Builder setTrafficStatsUid(int);
		public abstract android.net.http.BidirectionalStream.Builder setTrafficStatsUid(int arg0);
	
		// GEN member:android.net.http.BidirectionalStream$Builder:public abstract android.net.http.BidirectionalStream build();
		public abstract android.net.http.BidirectionalStream build();
	}


	// GEN class:android.net.http.BidirectionalStream$Callback
	public interface Callback {
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onStreamReady(android.net.http.BidirectionalStream);
		default void onStreamReady(android.net.http.BidirectionalStream arg0) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onResponseHeadersReceived(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo);
		default void onResponseHeadersReceived(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onReadCompleted(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo, java.nio.ByteBuffer, boolean);
		default void onReadCompleted(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1, java.nio.ByteBuffer arg2, boolean arg3) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onWriteCompleted(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo, java.nio.ByteBuffer, boolean);
		default void onWriteCompleted(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1, java.nio.ByteBuffer arg2, boolean arg3) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onResponseTrailersReceived(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo, android.net.http.HeaderBlock);
		default void onResponseTrailersReceived(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1, android.net.http.HeaderBlock arg2) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onSucceeded(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo);
		default void onSucceeded(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onFailed(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo, android.net.http.HttpException);
		default void onFailed(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1, android.net.http.HttpException arg2) {}
	
		// GEN member:android.net.http.BidirectionalStream$Callback:public abstract void onCanceled(android.net.http.BidirectionalStream, android.net.http.UrlResponseInfo);
		default void onCanceled(android.net.http.BidirectionalStream arg0, android.net.http.UrlResponseInfo arg1) {}
	}

}
