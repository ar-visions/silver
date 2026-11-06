package android.media;

import android.atl.annotation.Export;
import android.content.Context;
import android.net.Uri;
import java.io.FileDescriptor;
import java.util.Map;

public class MediaPlayer implements android.media.AudioRouting, android.media.VolumeAutomation {
	private long gtk_media_stream;

	public interface OnCompletionListener {
		@Export
		void onCompletion(MediaPlayer media_player);
	}
	public interface OnErrorListener {
	}
	public interface OnPreparedListener {
	}
	public interface OnBufferingUpdateListener {
	}
	public interface OnInfoListener {
	}
	public interface OnSeekCompleteListener {
	}
	public interface OnVideoSizeChangedListener {
	}
	public interface MediaPlayerControl {
	}

	public static MediaPlayer create(Context context, int dummy) { return new MediaPlayer(); }

	public void setDataSource(FileDescriptor src, long offset, long length) {}

	public void setDataSource(String path) {
		gtk_media_stream = native_setDataSource(path);
	}

	public void setDataSource(Context context, Uri uri) {
		System.out.println("setDataSource(" + uri + ") called");
		setDataSource(uri.getPath());
	}

	public void setDataSource(Context context, Uri uri, Map<String, String> headers) {
		System.out.println("setDataSource(" + uri + ") called");
		setDataSource(uri.getPath());
	}

	public void setLooping(boolean dummy) {}

	public void setOnCompletionListener(MediaPlayer.OnCompletionListener listener) {
		native_setOnCompletionListener(gtk_media_stream, listener);
	}

	public void setOnErrorListener(MediaPlayer.OnErrorListener dummy) {}
	public void setOnPreparedListener(MediaPlayer.OnPreparedListener dummy) {}
	public void setOnBufferingUpdateListener(MediaPlayer.OnBufferingUpdateListener dummy) {}
	public void setOnInfoListener(MediaPlayer.OnInfoListener dummy) {}
	public void setOnSeekCompleteListener(MediaPlayer.OnSeekCompleteListener dummy) {}
	public void setOnVideoSizeChangedListener(MediaPlayer.OnVideoSizeChangedListener dummy) {}
	public void setAudioAttributes(AudioAttributes attributes) {}
	public void setAudioStreamType(int dummy) {}

	public void start() {
		native_start(gtk_media_stream);
	}

	public void stop() {}
	public void pause() {}

	public void prepare() {
		native_prepare(gtk_media_stream);
	}

	public void prepareAsync() {}
	public void reset() {}
	public void release() {}

	public boolean isPlaying() { return false; }

	public void seekTo(int dummy) {}

	public void setVolume(float leftVolume, float rightVolume) {}

	public int getDuration() {
		return native_getDuration(gtk_media_stream);
	}

	public int getCurrentPosition() {
		return native_getCurrentPosition(gtk_media_stream);
	}

	public int getAudioSessionId() { return 0; }

	public void setWakeMode(Context context, int mode) {}

	public static native void native_prepare(long gtk_media_stream);
	public native long native_setDataSource(String path);
	public static native void native_setOnCompletionListener(long gtk_media_stream, MediaPlayer.OnCompletionListener listener);
	public static native void native_start(long gtk_media_stream);
	public static native int native_getDuration(long gtk_media_stream);
	public static native int native_getCurrentPosition(long gtk_media_stream);

	// GEN member:android.media.MediaPlayer:public static android.media.MediaPlayer create(android.content.Context, int, android.media.AudioAttributes, int);
	public static android.media.MediaPlayer create(android.content.Context arg0, int arg1, android.media.AudioAttributes arg2, int arg3) { return null; }

	// GEN member:android.media.MediaPlayer:public void setDataSource(java.io.FileDescriptor) throws java.io.IOException, java.lang.IllegalArgumentException, java.lang.IllegalStateException;
	public void setDataSource(java.io.FileDescriptor arg0) throws java.io.IOException, java.lang.IllegalArgumentException, java.lang.IllegalStateException {}

	// GEN member:android.media.MediaPlayer:public native void setNextMediaPlayer(android.media.MediaPlayer);
	public void setNextMediaPlayer(android.media.MediaPlayer arg0) {}

	// GEN member:android.media.MediaPlayer:public native int getVideoHeight();
	public int getVideoHeight() { return 0; }

	// GEN member:android.media.MediaPlayer:public native int getVideoWidth();
	public int getVideoWidth() { return 0; }

	// GEN member:android.media.MediaPlayer:public void setSurface(android.view.Surface);
	public void setSurface(android.view.Surface arg0) {}
}
