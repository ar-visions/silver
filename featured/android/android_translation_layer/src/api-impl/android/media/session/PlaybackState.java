package android.media.session;

import android.os.Bundle;

public class PlaybackState implements android.os.Parcelable {

	public int state;
	public long position;
	public float playbackSpeed;
	public long updateTime;
	public long actions;
	public long activeQueueItemId;

	public static class Builder {

		PlaybackState state;

		public Builder() {
			state = new PlaybackState();
		}

		public Builder(PlaybackState from) {
			state = from;
		}

		public Builder setState(int state, long position, float playbackSpeed, long updateTime) {
			this.state.state = state;
			this.state.position = position;
			this.state.playbackSpeed = playbackSpeed;
			this.state.updateTime = updateTime;
			return this;
		}

		public Builder setBufferedPosition(long bufferedPosition) { return this; }

		public Builder setActions(long actions) {
			state.actions = actions;
			return this;
		}

		public Builder setErrorMessage(CharSequence errorMessage) { return this; }

		public Builder setActiveQueueItemId(long activeQueueItemId) {
			state.activeQueueItemId = activeQueueItemId;
			return this;
		}

		public Builder addCustomAction(CustomAction action) { return this; }

		public Builder setExtras(Bundle bundle) { return this; }

		public PlaybackState build() {
			return state;
		}
	}

	public static class CustomAction implements android.os.Parcelable {

		public static class Builder {
			public Builder(String action, CharSequence label, int icon) {}

			public Builder setExtras(Bundle extras) { return this; }

			public CustomAction build() { return new CustomAction(); }
		}
	
	// GEN member:android.media.session.PlaybackState$CustomAction:public java.lang.String getAction();
	public java.lang.String getAction() { return null; }

	// GEN member:android.media.session.PlaybackState$CustomAction:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.media.session.PlaybackState$CustomAction:public int getIcon();
	public int getIcon() { return 0; }

	// GEN member:android.media.session.PlaybackState$CustomAction:public java.lang.CharSequence getName();
	public java.lang.CharSequence getName() { return null; }
}

	// GEN member:android.media.session.PlaybackState:public long getActions();
	public long getActions() { return 0; }

	// GEN member:android.media.session.PlaybackState:public long getActiveQueueItemId();
	public long getActiveQueueItemId() { return 0; }

	// GEN member:android.media.session.PlaybackState:public long getBufferedPosition();
	public long getBufferedPosition() { return 0; }

	// GEN member:android.media.session.PlaybackState:public java.util.List<android.media.session.PlaybackState$CustomAction> getCustomActions();
	public java.util.List<android.media.session.PlaybackState.CustomAction> getCustomActions() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.session.PlaybackState:public java.lang.CharSequence getErrorMessage();
	public java.lang.CharSequence getErrorMessage() { return null; }

	// GEN member:android.media.session.PlaybackState:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.media.session.PlaybackState:public long getLastPositionUpdateTime();
	public long getLastPositionUpdateTime() { return 0; }

	// GEN member:android.media.session.PlaybackState:public float getPlaybackSpeed();
	public float getPlaybackSpeed() { return 0; }

	// GEN member:android.media.session.PlaybackState:public long getPosition();
	public long getPosition() { return 0; }

	// GEN member:android.media.session.PlaybackState:public int getState();
	public int getState() { return 0; }
}
