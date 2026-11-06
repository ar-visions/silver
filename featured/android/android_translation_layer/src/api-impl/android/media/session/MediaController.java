package android.media.session;

import android.content.Context;
import android.media.MediaMetadata;

public class MediaController {

	public MediaController(Context context, MediaSession.Token token) {}

	public MediaMetadata getMetadata() {
		return new MediaMetadata();
	}

	public CharSequence getQueueTitle() {
		return null;
	}

	// GEN class:android.media.session.MediaController$Callback
	public abstract class Callback {
		// GEN member:android.media.session.MediaController$Callback:public android.media.session.MediaController$Callback();
		public Callback() {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onSessionDestroyed();
		public void onSessionDestroyed() {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onSessionEvent(java.lang.String, android.os.Bundle);
		public void onSessionEvent(java.lang.String arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onPlaybackStateChanged(android.media.session.PlaybackState);
		public void onPlaybackStateChanged(android.media.session.PlaybackState arg0) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onMetadataChanged(android.media.MediaMetadata);
		public void onMetadataChanged(android.media.MediaMetadata arg0) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onQueueChanged(java.util.List<android.media.session.MediaSession$QueueItem>);
		public void onQueueChanged(java.util.List<android.media.session.MediaSession.QueueItem> arg0) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onQueueTitleChanged(java.lang.CharSequence);
		public void onQueueTitleChanged(java.lang.CharSequence arg0) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onExtrasChanged(android.os.Bundle);
		public void onExtrasChanged(android.os.Bundle arg0) {}
	
		// GEN member:android.media.session.MediaController$Callback:public void onAudioInfoChanged(android.media.session.MediaController$PlaybackInfo);
		public void onAudioInfoChanged(android.media.session.MediaController.PlaybackInfo arg0) {}
	}


	// GEN class:android.media.session.MediaController$PlaybackInfo
	public final class PlaybackInfo implements android.os.Parcelable {
		// GEN member:android.media.session.MediaController$PlaybackInfo:public static final int PLAYBACK_TYPE_LOCAL = 1;
		public static final int PLAYBACK_TYPE_LOCAL = 1;
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public static final int PLAYBACK_TYPE_REMOTE = 2;
		public static final int PLAYBACK_TYPE_REMOTE = 2;
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public int getPlaybackType();
		public int getPlaybackType() { return 0; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public int getVolumeControl();
		public int getVolumeControl() { return 0; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public int getMaxVolume();
		public int getMaxVolume() { return 0; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public int getCurrentVolume();
		public int getCurrentVolume() { return 0; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public android.media.AudioAttributes getAudioAttributes();
		public android.media.AudioAttributes getAudioAttributes() { return null; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public java.lang.String getVolumeControlId();
		public java.lang.String getVolumeControlId() { return null; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.media.session.MediaController$PlaybackInfo:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected PlaybackInfo() {}
	}


	// GEN class:android.media.session.MediaController$TransportControls
	public final class TransportControls {
		// GEN member:android.media.session.MediaController$TransportControls:public void prepare();
		public void prepare() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void prepareFromMediaId(java.lang.String, android.os.Bundle);
		public void prepareFromMediaId(java.lang.String arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void prepareFromSearch(java.lang.String, android.os.Bundle);
		public void prepareFromSearch(java.lang.String arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void prepareFromUri(android.net.Uri, android.os.Bundle);
		public void prepareFromUri(android.net.Uri arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void play();
		public void play() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void playFromMediaId(java.lang.String, android.os.Bundle);
		public void playFromMediaId(java.lang.String arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void playFromSearch(java.lang.String, android.os.Bundle);
		public void playFromSearch(java.lang.String arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void playFromUri(android.net.Uri, android.os.Bundle);
		public void playFromUri(android.net.Uri arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void skipToQueueItem(long);
		public void skipToQueueItem(long arg0) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void pause();
		public void pause() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void stop();
		public void stop() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void seekTo(long);
		public void seekTo(long arg0) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void fastForward();
		public void fastForward() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void skipToNext();
		public void skipToNext() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void rewind();
		public void rewind() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void skipToPrevious();
		public void skipToPrevious() {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void setPlaybackSpeed(float);
		public void setPlaybackSpeed(float arg0) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void sendCustomAction(android.media.session.PlaybackState$CustomAction, android.os.Bundle);
		public void sendCustomAction(android.media.session.PlaybackState.CustomAction arg0, android.os.Bundle arg1) {}
	
		// GEN member:android.media.session.MediaController$TransportControls:public void sendCustomAction(java.lang.String, android.os.Bundle);
		public void sendCustomAction(java.lang.String arg0, android.os.Bundle arg1) {}
	
		protected TransportControls() {}
	}


	// GEN member:android.media.session.MediaController:public android.media.session.PlaybackState getPlaybackState();
	public android.media.session.PlaybackState getPlaybackState() { return null; }

	// GEN member:android.media.session.MediaController:public android.media.session.MediaController$TransportControls getTransportControls();
	public android.media.session.MediaController.TransportControls getTransportControls() { return null; }

	// GEN member:android.media.session.MediaController:public void registerCallback(android.media.session.MediaController$Callback, android.os.Handler);
	public void registerCallback(android.media.session.MediaController.Callback arg0, android.os.Handler arg1) {}

	// GEN member:android.media.session.MediaController:public void sendCommand(java.lang.String, android.os.Bundle, android.os.ResultReceiver);
	public void sendCommand(java.lang.String arg0, android.os.Bundle arg1, android.os.ResultReceiver arg2) {}

	// GEN member:android.media.session.MediaController:public void unregisterCallback(android.media.session.MediaController$Callback);
	public void unregisterCallback(android.media.session.MediaController.Callback arg0) {}
}
