package android.media.session;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.PendingIntent;
import android.atl.annotation.Export;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.MediaDescription;
import android.media.MediaMetadata;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import java.util.List;

public class MediaSession {

	private List<QueueItem> queue;

	public static final class Token implements android.os.Parcelable {}

	public static class QueueItem implements android.os.Parcelable {
		long id;
		MediaDescription description;

		public QueueItem(MediaDescription description, long id) {
			this.description = description;
			this.id = id;
		}
	
	// GEN member:android.media.session.MediaSession$QueueItem:public android.media.MediaDescription getDescription();
	public android.media.MediaDescription getDescription() { return null; }

	// GEN member:android.media.session.MediaSession$QueueItem:public long getQueueId();
	public long getQueueId() { return 0; }
}

	public MediaSession(Context context, String tag) {}

	public Token getSessionToken() {
		return new Token();
	}

	public void setFlags(int flags) {}

	public void setCallback(Callback callback, Handler handler) {
		nativeSetCallback(callback);
	}

	public void setCallback(Callback callback) {
		nativeSetCallback(callback);
	}

	public void setMediaButtonReceiver(PendingIntent pendingIntent) {}

	public void setActive(boolean active) {}

	public void setPlaybackState(PlaybackState state) {
		String title = null;
		String subTitle = null;
		String artUrl = null;
		if (queue != null)
			for (QueueItem item : queue) {
				if (item.id == state.activeQueueItemId) {
					title = item.description.title.toString();
					subTitle = item.description.subtitle.toString();
					artUrl = item.description.iconUri == null ? null : item.description.iconUri.toString();
					break;
				}
			}
		nativeSetState(state.state, state.actions, state.position, state.updateTime, title, subTitle, artUrl);
	}

	public void setMetadata(MediaMetadata metadata) {}

	public void setQueue(List<QueueItem> queue) {
		this.queue = queue;
	}

	public void release() {}

	public void setPlaybackToLocal(AudioAttributes audioAttributes) {}

	public void setExtras(Bundle extras) {}

	public void setSessionActivity(PendingIntent pendingIntent) {}

	public boolean isActive() {
		return true;
	}

	public void setRatingType(int ratingType) {}

	public void setQueueTitle(CharSequence title) {}
	protected native void nativeSetState(int state, long actions, long position, long updateTime, String title, String subTitle, String artUrl);
	protected native void nativeSetCallback(Callback callback);

	public abstract static class Callback {
		public Callback() {}

		public void onCommand(@NonNull String command, @Nullable Bundle args, @Nullable ResultReceiver cb) {}

		public boolean onMediaButtonEvent(@NonNull Intent mediaButtonIntent) { return false; }

		public void onPrepare() {}

		public void onPrepareFromMediaId(String mediaId, Bundle extras) {}

		public void onPrepareFromSearch(String query, Bundle extras) {}

		public void onPrepareFromUri(Uri uri, Bundle extras) {}

		@Export
		public void onPlay() {}

		public void onPlayFromSearch(String query, Bundle extras) {}

		public void onPlayFromMediaId(String mediaId, Bundle extras) {}

		public void onPlayFromUri(Uri uri, Bundle extras) {}

		public void onSkipToQueueItem(long id) {}

		@Export
		public void onPause() {}

		public void onSkipToNext() {}

		public void onSkipToPrevious() {}

		public void onFastForward() {}

		public void onRewind() {}

		public void onStop() {}

		@Export
		public void onSeekTo(long pos) {}

		//public void onSetRating(@NonNull Rating rating) {}

		public void onSetPlaybackSpeed(float speed) {}

		public void onCustomAction(@NonNull String action, @Nullable Bundle extras) {}
	}
}
