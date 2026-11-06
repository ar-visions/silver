package android.media.metrics;

// android.jar's shape, made by gen.py
public final class PlaybackMetrics implements android.os.Parcelable {
	// GEN member:android.media.metrics.PlaybackMetrics:public static final int CONTENT_TYPE_AD = 2;
	public static final int CONTENT_TYPE_AD = 2;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int CONTENT_TYPE_MAIN = 1;
	public static final int CONTENT_TYPE_MAIN = 1;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int CONTENT_TYPE_OTHER = 3;
	public static final int CONTENT_TYPE_OTHER = 3;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int CONTENT_TYPE_UNKNOWN = 0;
	public static final int CONTENT_TYPE_UNKNOWN = 0;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final android.os.Parcelable$Creator<android.media.metrics.PlaybackMetrics> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.metrics.PlaybackMetrics> CREATOR = null;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_CLEARKEY = 6;
	public static final int DRM_TYPE_CLEARKEY = 6;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_NONE = 0;
	public static final int DRM_TYPE_NONE = 0;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_OTHER = 1;
	public static final int DRM_TYPE_OTHER = 1;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_PLAY_READY = 2;
	public static final int DRM_TYPE_PLAY_READY = 2;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_WIDEVINE_L1 = 3;
	public static final int DRM_TYPE_WIDEVINE_L1 = 3;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_WIDEVINE_L3 = 4;
	public static final int DRM_TYPE_WIDEVINE_L3 = 4;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int DRM_TYPE_WV_L3_FALLBACK = 5;
	public static final int DRM_TYPE_WV_L3_FALLBACK = 5;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int PLAYBACK_TYPE_LIVE = 2;
	public static final int PLAYBACK_TYPE_LIVE = 2;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int PLAYBACK_TYPE_OTHER = 3;
	public static final int PLAYBACK_TYPE_OTHER = 3;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int PLAYBACK_TYPE_UNKNOWN = 0;
	public static final int PLAYBACK_TYPE_UNKNOWN = 0;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int PLAYBACK_TYPE_VOD = 1;
	public static final int PLAYBACK_TYPE_VOD = 1;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_SOURCE_DEVICE = 2;
	public static final int STREAM_SOURCE_DEVICE = 2;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_SOURCE_MIXED = 3;
	public static final int STREAM_SOURCE_MIXED = 3;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_SOURCE_NETWORK = 1;
	public static final int STREAM_SOURCE_NETWORK = 1;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_SOURCE_UNKNOWN = 0;
	public static final int STREAM_SOURCE_UNKNOWN = 0;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_DASH = 3;
	public static final int STREAM_TYPE_DASH = 3;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_HLS = 4;
	public static final int STREAM_TYPE_HLS = 4;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_OTHER = 1;
	public static final int STREAM_TYPE_OTHER = 1;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_PROGRESSIVE = 2;
	public static final int STREAM_TYPE_PROGRESSIVE = 2;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_SS = 5;
	public static final int STREAM_TYPE_SS = 5;

	// GEN member:android.media.metrics.PlaybackMetrics:public static final int STREAM_TYPE_UNKNOWN = 0;
	public static final int STREAM_TYPE_UNKNOWN = 0;

	// GEN member:android.media.metrics.PlaybackMetrics:public long getMediaDurationMillis();
	public long getMediaDurationMillis() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getStreamSource();
	public int getStreamSource() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getStreamType();
	public int getStreamType() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getPlaybackType();
	public int getPlaybackType() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getDrmType();
	public int getDrmType() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getContentType();
	public int getContentType() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public java.lang.String getPlayerName();
	public java.lang.String getPlayerName() { return null; }

	// GEN member:android.media.metrics.PlaybackMetrics:public java.lang.String getPlayerVersion();
	public java.lang.String getPlayerVersion() { return null; }

	// GEN member:android.media.metrics.PlaybackMetrics:public long[] getExperimentIds();
	public long[] getExperimentIds() { return new long[0]; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getVideoFramesPlayed();
	public int getVideoFramesPlayed() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getVideoFramesDropped();
	public int getVideoFramesDropped() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int getAudioUnderrunCount();
	public int getAudioUnderrunCount() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public long getNetworkBytesRead();
	public long getNetworkBytesRead() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public long getLocalBytesRead();
	public long getLocalBytesRead() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public long getNetworkTransferDurationMillis();
	public long getNetworkTransferDurationMillis() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public byte[] getDrmSessionId();
	public byte[] getDrmSessionId() { return new byte[0]; }

	// GEN member:android.media.metrics.PlaybackMetrics:public android.os.Bundle getMetricsBundle();
	public android.os.Bundle getMetricsBundle() { return null; }

	// GEN member:android.media.metrics.PlaybackMetrics:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.media.metrics.PlaybackMetrics:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.metrics.PlaybackMetrics:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.media.metrics.PlaybackMetrics:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.media.metrics.PlaybackMetrics:public int describeContents();
	public int describeContents() { return 0; }

	protected PlaybackMetrics() {}

	// GEN class:android.media.metrics.PlaybackMetrics$Builder
	public final class Builder {
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder();
		public Builder() {}
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setMediaDurationMillis(long);
		public android.media.metrics.PlaybackMetrics.Builder setMediaDurationMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setStreamSource(int);
		public android.media.metrics.PlaybackMetrics.Builder setStreamSource(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setStreamType(int);
		public android.media.metrics.PlaybackMetrics.Builder setStreamType(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setPlaybackType(int);
		public android.media.metrics.PlaybackMetrics.Builder setPlaybackType(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setDrmType(int);
		public android.media.metrics.PlaybackMetrics.Builder setDrmType(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setContentType(int);
		public android.media.metrics.PlaybackMetrics.Builder setContentType(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setPlayerName(java.lang.String);
		public android.media.metrics.PlaybackMetrics.Builder setPlayerName(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setPlayerVersion(java.lang.String);
		public android.media.metrics.PlaybackMetrics.Builder setPlayerVersion(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder addExperimentId(long);
		public android.media.metrics.PlaybackMetrics.Builder addExperimentId(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setVideoFramesPlayed(int);
		public android.media.metrics.PlaybackMetrics.Builder setVideoFramesPlayed(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setVideoFramesDropped(int);
		public android.media.metrics.PlaybackMetrics.Builder setVideoFramesDropped(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setAudioUnderrunCount(int);
		public android.media.metrics.PlaybackMetrics.Builder setAudioUnderrunCount(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setNetworkBytesRead(long);
		public android.media.metrics.PlaybackMetrics.Builder setNetworkBytesRead(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setLocalBytesRead(long);
		public android.media.metrics.PlaybackMetrics.Builder setLocalBytesRead(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setNetworkTransferDurationMillis(long);
		public android.media.metrics.PlaybackMetrics.Builder setNetworkTransferDurationMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setDrmSessionId(byte[]);
		public android.media.metrics.PlaybackMetrics.Builder setDrmSessionId(byte[] arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics$Builder setMetricsBundle(android.os.Bundle);
		public android.media.metrics.PlaybackMetrics.Builder setMetricsBundle(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackMetrics$Builder:public android.media.metrics.PlaybackMetrics build();
		public android.media.metrics.PlaybackMetrics build() { return null; }
	}

}
