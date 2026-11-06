package android.media.metrics;

// android.jar's shape, made by gen.py
public final class TrackChangeEvent extends android.media.metrics.Event implements android.os.Parcelable {
	// GEN member:android.media.metrics.TrackChangeEvent:public static final android.os.Parcelable$Creator<android.media.metrics.TrackChangeEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.metrics.TrackChangeEvent> CREATOR = null;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_CHANGE_REASON_ADAPTIVE = 4;
	public static final int TRACK_CHANGE_REASON_ADAPTIVE = 4;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_CHANGE_REASON_INITIAL = 2;
	public static final int TRACK_CHANGE_REASON_INITIAL = 2;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_CHANGE_REASON_MANUAL = 3;
	public static final int TRACK_CHANGE_REASON_MANUAL = 3;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_CHANGE_REASON_OTHER = 1;
	public static final int TRACK_CHANGE_REASON_OTHER = 1;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_CHANGE_REASON_UNKNOWN = 0;
	public static final int TRACK_CHANGE_REASON_UNKNOWN = 0;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_STATE_OFF = 0;
	public static final int TRACK_STATE_OFF = 0;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_STATE_ON = 1;
	public static final int TRACK_STATE_ON = 1;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_TYPE_AUDIO = 0;
	public static final int TRACK_TYPE_AUDIO = 0;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_TYPE_TEXT = 2;
	public static final int TRACK_TYPE_TEXT = 2;

	// GEN member:android.media.metrics.TrackChangeEvent:public static final int TRACK_TYPE_VIDEO = 1;
	public static final int TRACK_TYPE_VIDEO = 1;

	// GEN member:android.media.metrics.TrackChangeEvent:public int getTrackState();
	public int getTrackState() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getTrackChangeReason();
	public int getTrackChangeReason() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String getContainerMimeType();
	public java.lang.String getContainerMimeType() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String getSampleMimeType();
	public java.lang.String getSampleMimeType() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String getCodecName();
	public java.lang.String getCodecName() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getBitrate();
	public int getBitrate() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public long getTimeSinceCreatedMillis();
	public long getTimeSinceCreatedMillis() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getTrackType();
	public int getTrackType() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String getLanguage();
	public java.lang.String getLanguage() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String getLanguageRegion();
	public java.lang.String getLanguageRegion() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getChannelCount();
	public int getChannelCount() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getAudioSampleRate();
	public int getAudioSampleRate() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getWidth();
	public int getWidth() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int getHeight();
	public int getHeight() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public float getVideoFrameRate();
	public float getVideoFrameRate() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public android.os.Bundle getMetricsBundle();
	public android.os.Bundle getMetricsBundle() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.media.metrics.TrackChangeEvent:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.media.metrics.TrackChangeEvent:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.media.metrics.TrackChangeEvent:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.metrics.TrackChangeEvent:public int hashCode();
	public int hashCode() { return 0; }

	protected TrackChangeEvent() {}

	// GEN class:android.media.metrics.TrackChangeEvent$Builder
	public final class Builder {
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder(int);
		public Builder(int arg0) {}
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setTrackState(int);
		public android.media.metrics.TrackChangeEvent.Builder setTrackState(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setTrackChangeReason(int);
		public android.media.metrics.TrackChangeEvent.Builder setTrackChangeReason(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setContainerMimeType(java.lang.String);
		public android.media.metrics.TrackChangeEvent.Builder setContainerMimeType(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setSampleMimeType(java.lang.String);
		public android.media.metrics.TrackChangeEvent.Builder setSampleMimeType(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setCodecName(java.lang.String);
		public android.media.metrics.TrackChangeEvent.Builder setCodecName(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setBitrate(int);
		public android.media.metrics.TrackChangeEvent.Builder setBitrate(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setTimeSinceCreatedMillis(long);
		public android.media.metrics.TrackChangeEvent.Builder setTimeSinceCreatedMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setLanguage(java.lang.String);
		public android.media.metrics.TrackChangeEvent.Builder setLanguage(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setLanguageRegion(java.lang.String);
		public android.media.metrics.TrackChangeEvent.Builder setLanguageRegion(java.lang.String arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setChannelCount(int);
		public android.media.metrics.TrackChangeEvent.Builder setChannelCount(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setAudioSampleRate(int);
		public android.media.metrics.TrackChangeEvent.Builder setAudioSampleRate(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setWidth(int);
		public android.media.metrics.TrackChangeEvent.Builder setWidth(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setHeight(int);
		public android.media.metrics.TrackChangeEvent.Builder setHeight(int arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setVideoFrameRate(float);
		public android.media.metrics.TrackChangeEvent.Builder setVideoFrameRate(float arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent$Builder setMetricsBundle(android.os.Bundle);
		public android.media.metrics.TrackChangeEvent.Builder setMetricsBundle(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.media.metrics.TrackChangeEvent$Builder:public android.media.metrics.TrackChangeEvent build();
		public android.media.metrics.TrackChangeEvent build() { return null; }
	
		protected Builder() {}
	}

}
