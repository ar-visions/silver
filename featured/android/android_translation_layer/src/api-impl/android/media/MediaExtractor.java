package android.media;

// android.jar's shape, made by gen.py
public final class MediaExtractor {
	// GEN member:android.media.MediaExtractor:public static final int SAMPLE_FLAG_ENCRYPTED = 2;
	public static final int SAMPLE_FLAG_ENCRYPTED = 2;

	// GEN member:android.media.MediaExtractor:public static final int SAMPLE_FLAG_PARTIAL_FRAME = 4;
	public static final int SAMPLE_FLAG_PARTIAL_FRAME = 4;

	// GEN member:android.media.MediaExtractor:public static final int SAMPLE_FLAG_SYNC = 1;
	public static final int SAMPLE_FLAG_SYNC = 1;

	// GEN member:android.media.MediaExtractor:public static final int SEEK_TO_CLOSEST_SYNC = 2;
	public static final int SEEK_TO_CLOSEST_SYNC = 2;

	// GEN member:android.media.MediaExtractor:public static final int SEEK_TO_NEXT_SYNC = 1;
	public static final int SEEK_TO_NEXT_SYNC = 1;

	// GEN member:android.media.MediaExtractor:public static final int SEEK_TO_PREVIOUS_SYNC = 0;
	public static final int SEEK_TO_PREVIOUS_SYNC = 0;

	// GEN member:android.media.MediaExtractor:public android.media.MediaExtractor();
	public MediaExtractor() {}

	// GEN member:android.media.MediaExtractor:public native void setDataSource(android.media.MediaDataSource) throws java.io.IOException;
	public void setDataSource(android.media.MediaDataSource arg0) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:public void setDataSource(android.content.Context, android.net.Uri, java.util.Map<java.lang.String, java.lang.String>) throws java.io.IOException;
	public void setDataSource(android.content.Context arg0, android.net.Uri arg1, java.util.Map<java.lang.String, java.lang.String> arg2) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:public void setDataSource(java.lang.String, java.util.Map<java.lang.String, java.lang.String>) throws java.io.IOException;
	public void setDataSource(java.lang.String arg0, java.util.Map<java.lang.String, java.lang.String> arg1) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:public void setDataSource(java.lang.String) throws java.io.IOException;
	public void setDataSource(java.lang.String arg0) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:public void setDataSource(android.content.res.AssetFileDescriptor) throws java.io.IOException, java.lang.IllegalArgumentException, java.lang.IllegalStateException;
	public void setDataSource(android.content.res.AssetFileDescriptor arg0) throws java.io.IOException, java.lang.IllegalArgumentException, java.lang.IllegalStateException {}

	// GEN member:android.media.MediaExtractor:public void setDataSource(java.io.FileDescriptor) throws java.io.IOException;
	public void setDataSource(java.io.FileDescriptor arg0) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:public native void setDataSource(java.io.FileDescriptor, long, long) throws java.io.IOException;
	public void setDataSource(java.io.FileDescriptor arg0, long arg1, long arg2) throws java.io.IOException {}

	// GEN member:android.media.MediaExtractor:protected void finalize();
	protected void finalize() {}

	// GEN member:android.media.MediaExtractor:public native void release();
	public void release() {}

	// GEN member:android.media.MediaExtractor:public native int getTrackCount();
	public int getTrackCount() { return 0; }

	// GEN member:android.media.MediaExtractor:public java.util.Map<java.util.UUID, byte[]> getPsshInfo();
	public java.util.Map<java.util.UUID, byte[]> getPsshInfo() { return new java.util.HashMap<>(); }

	// GEN member:android.media.MediaExtractor:public android.media.MediaFormat getTrackFormat(int);
	public android.media.MediaFormat getTrackFormat(int arg0) { return null; }

	// GEN member:android.media.MediaExtractor:public native void selectTrack(int);
	public void selectTrack(int arg0) {}

	// GEN member:android.media.MediaExtractor:public native void unselectTrack(int);
	public void unselectTrack(int arg0) {}

	// GEN member:android.media.MediaExtractor:public native void seekTo(long, int);
	public void seekTo(long arg0, int arg1) {}

	// GEN member:android.media.MediaExtractor:public native boolean advance();
	public boolean advance() { return false; }

	// GEN member:android.media.MediaExtractor:public native int readSampleData(java.nio.ByteBuffer, int);
	public int readSampleData(java.nio.ByteBuffer arg0, int arg1) { return 0; }

	// GEN member:android.media.MediaExtractor:public native int getSampleTrackIndex();
	public int getSampleTrackIndex() { return 0; }

	// GEN member:android.media.MediaExtractor:public native long getSampleTime();
	public long getSampleTime() { return 0; }

	// GEN member:android.media.MediaExtractor:public native long getSampleSize();
	public long getSampleSize() { return 0; }

	// GEN member:android.media.MediaExtractor:public native int getSampleFlags();
	public int getSampleFlags() { return 0; }

	// GEN member:android.media.MediaExtractor:public native boolean getSampleCryptoInfo(android.media.MediaCodec$CryptoInfo);
	public boolean getSampleCryptoInfo(android.media.MediaCodec.CryptoInfo arg0) { return false; }

	// GEN member:android.media.MediaExtractor:public native long getCachedDuration();
	public long getCachedDuration() { return 0; }

	// GEN member:android.media.MediaExtractor:public native boolean hasCacheReachedEndOfStream();
	public boolean hasCacheReachedEndOfStream() { return false; }

	// GEN member:android.media.MediaExtractor:public void setLogSessionId(android.media.metrics.LogSessionId);
	public void setLogSessionId(android.media.metrics.LogSessionId arg0) {}

	// GEN member:android.media.MediaExtractor:public android.media.metrics.LogSessionId getLogSessionId();
	public android.media.metrics.LogSessionId getLogSessionId() { return null; }

	// GEN member:android.media.MediaExtractor:public android.os.PersistableBundle getMetrics();
	public android.os.PersistableBundle getMetrics() { return null; }
}
