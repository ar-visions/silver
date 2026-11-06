package android.media.metrics;

// android.jar's shape, made by gen.py
public final class PlaybackStateEvent extends android.media.metrics.Event implements android.os.Parcelable {
	// GEN member:android.media.metrics.PlaybackStateEvent:public static final android.os.Parcelable$Creator<android.media.metrics.PlaybackStateEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.metrics.PlaybackStateEvent> CREATOR = null;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_ABANDONED = 15;
	public static final int STATE_ABANDONED = 15;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_BUFFERING = 6;
	public static final int STATE_BUFFERING = 6;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_ENDED = 11;
	public static final int STATE_ENDED = 11;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_FAILED = 13;
	public static final int STATE_FAILED = 13;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_INTERRUPTED_BY_AD = 14;
	public static final int STATE_INTERRUPTED_BY_AD = 14;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_JOINING_BACKGROUND = 1;
	public static final int STATE_JOINING_BACKGROUND = 1;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_JOINING_FOREGROUND = 2;
	public static final int STATE_JOINING_FOREGROUND = 2;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_NOT_STARTED = 0;
	public static final int STATE_NOT_STARTED = 0;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_PAUSED = 4;
	public static final int STATE_PAUSED = 4;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_PAUSED_BUFFERING = 7;
	public static final int STATE_PAUSED_BUFFERING = 7;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_PLAYING = 3;
	public static final int STATE_PLAYING = 3;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_SEEKING = 5;
	public static final int STATE_SEEKING = 5;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_STOPPED = 12;
	public static final int STATE_STOPPED = 12;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_SUPPRESSED = 9;
	public static final int STATE_SUPPRESSED = 9;

	// GEN member:android.media.metrics.PlaybackStateEvent:public static final int STATE_SUPPRESSED_BUFFERING = 10;
	public static final int STATE_SUPPRESSED_BUFFERING = 10;

	// GEN member:android.media.metrics.PlaybackStateEvent:public int getState();
	public int getState() { return 0; }

	// GEN member:android.media.metrics.PlaybackStateEvent:public long getTimeSinceCreatedMillis();
	public long getTimeSinceCreatedMillis() { return 0; }

	// GEN member:android.media.metrics.PlaybackStateEvent:public android.os.Bundle getMetricsBundle();
	public android.os.Bundle getMetricsBundle() { return null; }

	// GEN member:android.media.metrics.PlaybackStateEvent:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.metrics.PlaybackStateEvent:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.media.metrics.PlaybackStateEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.media.metrics.PlaybackStateEvent:public int describeContents();
	public int describeContents() { return 0; }

	protected PlaybackStateEvent() {}

	// GEN class:android.media.metrics.PlaybackStateEvent$Builder
	public final class Builder {
		// GEN member:android.media.metrics.PlaybackStateEvent$Builder:public android.media.metrics.PlaybackStateEvent$Builder();
		public Builder() {}
	
		// GEN member:android.media.metrics.PlaybackStateEvent$Builder:public android.media.metrics.PlaybackStateEvent$Builder setState(int);
		public android.media.metrics.PlaybackStateEvent.Builder setState(int arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackStateEvent$Builder:public android.media.metrics.PlaybackStateEvent$Builder setTimeSinceCreatedMillis(long);
		public android.media.metrics.PlaybackStateEvent.Builder setTimeSinceCreatedMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackStateEvent$Builder:public android.media.metrics.PlaybackStateEvent$Builder setMetricsBundle(android.os.Bundle);
		public android.media.metrics.PlaybackStateEvent.Builder setMetricsBundle(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.media.metrics.PlaybackStateEvent$Builder:public android.media.metrics.PlaybackStateEvent build();
		public android.media.metrics.PlaybackStateEvent build() { return null; }
	}

}
