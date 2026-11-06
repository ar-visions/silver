package android.media.metrics;

// android.jar's shape, made by gen.py
public final class PlaybackErrorEvent extends android.media.metrics.Event implements android.os.Parcelable {
	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final android.os.Parcelable$Creator<android.media.metrics.PlaybackErrorEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.metrics.PlaybackErrorEvent> CREATOR = null;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_AUDIO_TRACK_INIT_FAILED = 17;
	public static final int ERROR_AUDIO_TRACK_INIT_FAILED = 17;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_AUDIO_TRACK_OTHER = 19;
	public static final int ERROR_AUDIO_TRACK_OTHER = 19;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_AUDIO_TRACK_WRITE_FAILED = 18;
	public static final int ERROR_AUDIO_TRACK_WRITE_FAILED = 18;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DECODER_INIT_FAILED = 13;
	public static final int ERROR_DECODER_INIT_FAILED = 13;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DECODING_FAILED = 14;
	public static final int ERROR_DECODING_FAILED = 14;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DECODING_FORMAT_EXCEEDS_CAPABILITIES = 15;
	public static final int ERROR_DECODING_FORMAT_EXCEEDS_CAPABILITIES = 15;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DECODING_FORMAT_UNSUPPORTED = 35;
	public static final int ERROR_DECODING_FORMAT_UNSUPPORTED = 35;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DECODING_OTHER = 16;
	public static final int ERROR_DECODING_OTHER = 16;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_CONTENT_ERROR = 28;
	public static final int ERROR_DRM_CONTENT_ERROR = 28;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_DEVICE_REVOKED = 29;
	public static final int ERROR_DRM_DEVICE_REVOKED = 29;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_DISALLOWED_OPERATION = 26;
	public static final int ERROR_DRM_DISALLOWED_OPERATION = 26;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_LICENSE_ACQUISITION_FAILED = 25;
	public static final int ERROR_DRM_LICENSE_ACQUISITION_FAILED = 25;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_OTHER = 30;
	public static final int ERROR_DRM_OTHER = 30;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_PROVISIONING_FAILED = 24;
	public static final int ERROR_DRM_PROVISIONING_FAILED = 24;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_SCHEME_UNSUPPORTED = 23;
	public static final int ERROR_DRM_SCHEME_UNSUPPORTED = 23;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_DRM_SYSTEM_ERROR = 27;
	public static final int ERROR_DRM_SYSTEM_ERROR = 27;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_BAD_HTTP_STATUS = 5;
	public static final int ERROR_IO_BAD_HTTP_STATUS = 5;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_CONNECTION_CLOSED = 8;
	public static final int ERROR_IO_CONNECTION_CLOSED = 8;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_CONNECTION_TIMEOUT = 7;
	public static final int ERROR_IO_CONNECTION_TIMEOUT = 7;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_DNS_FAILED = 6;
	public static final int ERROR_IO_DNS_FAILED = 6;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_FILE_NOT_FOUND = 31;
	public static final int ERROR_IO_FILE_NOT_FOUND = 31;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_NETWORK_CONNECTION_FAILED = 4;
	public static final int ERROR_IO_NETWORK_CONNECTION_FAILED = 4;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_NETWORK_UNAVAILABLE = 3;
	public static final int ERROR_IO_NETWORK_UNAVAILABLE = 3;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_NO_PERMISSION = 32;
	public static final int ERROR_IO_NO_PERMISSION = 32;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_IO_OTHER = 9;
	public static final int ERROR_IO_OTHER = 9;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_OTHER = 1;
	public static final int ERROR_OTHER = 1;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PARSING_CONTAINER_MALFORMED = 11;
	public static final int ERROR_PARSING_CONTAINER_MALFORMED = 11;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PARSING_CONTAINER_UNSUPPORTED = 34;
	public static final int ERROR_PARSING_CONTAINER_UNSUPPORTED = 34;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PARSING_MANIFEST_MALFORMED = 10;
	public static final int ERROR_PARSING_MANIFEST_MALFORMED = 10;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PARSING_MANIFEST_UNSUPPORTED = 33;
	public static final int ERROR_PARSING_MANIFEST_UNSUPPORTED = 33;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PARSING_OTHER = 12;
	public static final int ERROR_PARSING_OTHER = 12;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PLAYER_BEHIND_LIVE_WINDOW = 21;
	public static final int ERROR_PLAYER_BEHIND_LIVE_WINDOW = 21;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PLAYER_OTHER = 22;
	public static final int ERROR_PLAYER_OTHER = 22;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_PLAYER_REMOTE = 20;
	public static final int ERROR_PLAYER_REMOTE = 20;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_RUNTIME = 2;
	public static final int ERROR_RUNTIME = 2;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public static final int ERROR_UNKNOWN = 0;
	public static final int ERROR_UNKNOWN = 0;

	// GEN member:android.media.metrics.PlaybackErrorEvent:public int getErrorCode();
	public int getErrorCode() { return 0; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public int getSubErrorCode();
	public int getSubErrorCode() { return 0; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public long getTimeSinceCreatedMillis();
	public long getTimeSinceCreatedMillis() { return 0; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public android.os.Bundle getMetricsBundle();
	public android.os.Bundle getMetricsBundle() { return null; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.media.metrics.PlaybackErrorEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.media.metrics.PlaybackErrorEvent:public int describeContents();
	public int describeContents() { return 0; }

	protected PlaybackErrorEvent() {}

	// GEN class:android.media.metrics.PlaybackErrorEvent$Builder
	public final class Builder {
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder();
		public Builder() {}
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder setException(java.lang.Exception);
		public android.media.metrics.PlaybackErrorEvent.Builder setException(java.lang.Exception arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder setErrorCode(int);
		public android.media.metrics.PlaybackErrorEvent.Builder setErrorCode(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder setSubErrorCode(int);
		public android.media.metrics.PlaybackErrorEvent.Builder setSubErrorCode(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder setTimeSinceCreatedMillis(long);
		public android.media.metrics.PlaybackErrorEvent.Builder setTimeSinceCreatedMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent$Builder setMetricsBundle(android.os.Bundle);
		public android.media.metrics.PlaybackErrorEvent.Builder setMetricsBundle(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackErrorEvent$Builder:public android.media.metrics.PlaybackErrorEvent build();
		public android.media.metrics.PlaybackErrorEvent build() { return null; }
	}

}
