package android.media;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;

public class MediaMetadataRetriever implements java.lang.AutoCloseable {

	private MediaPlayer mediaPlayer;

	public void release() {
		if (mediaPlayer != null)
			mediaPlayer.release();
	}

	public void setDataSource(Context context, Uri uri) {
		mediaPlayer = new MediaPlayer();
		mediaPlayer.setDataSource(uri.getPath());
	}

	public void setDataSource(String path) {
		mediaPlayer = new MediaPlayer();
		mediaPlayer.setDataSource(path);
	}

	public byte[] getEmbeddedPicture() {
		return null;
	}

	public String extractMetadata(int key) {
		switch (key) {
			case 9 /*METADATA_KEY_DURATION*/:
				return String.valueOf(mediaPlayer.getDuration());
			default:
				return null;
		}
	}

	public Bitmap getFrameAtTime(long time) {
		return null;
	}

	public Bitmap getFrameAtTime() {
		return null;
	}

	// GEN member:android.media.MediaMetadataRetriever:public android.graphics.Bitmap getFrameAtTime(long, int);
	public android.graphics.Bitmap getFrameAtTime(long arg0, int arg1) { return null; }

	// GEN member:android.media.MediaMetadataRetriever:public android.graphics.Bitmap getScaledFrameAtTime(long, int, int, int);
	public android.graphics.Bitmap getScaledFrameAtTime(long arg0, int arg1, int arg2, int arg3) { return null; }

	// GEN member:android.media.MediaMetadataRetriever:public void setDataSource(android.media.MediaDataSource) throws java.lang.IllegalArgumentException;
	public void setDataSource(android.media.MediaDataSource arg0) throws java.lang.IllegalArgumentException {}

	// GEN member:android.media.MediaMetadataRetriever:public void setDataSource(java.io.FileDescriptor, long, long) throws java.lang.IllegalArgumentException;
	public void setDataSource(java.io.FileDescriptor arg0, long arg1, long arg2) throws java.lang.IllegalArgumentException {}

	// GEN member:android.media.MediaMetadataRetriever:public void setDataSource(java.io.FileDescriptor) throws java.lang.IllegalArgumentException;
	public void setDataSource(java.io.FileDescriptor arg0) throws java.lang.IllegalArgumentException {}
	public void close() throws java.lang.Exception {}
}
