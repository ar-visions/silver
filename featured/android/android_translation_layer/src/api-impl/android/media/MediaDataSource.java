package android.media;

// android.jar's shape, made by gen.py
public abstract class MediaDataSource implements java.io.Closeable {
	// GEN member:android.media.MediaDataSource:public android.media.MediaDataSource();
	public MediaDataSource() {}

	// GEN member:android.media.MediaDataSource:public abstract int readAt(long, byte[], int, int) throws java.io.IOException;
	public abstract int readAt(long arg0, byte[] arg1, int arg2, int arg3) throws java.io.IOException;

	// GEN member:android.media.MediaDataSource:public abstract long getSize() throws java.io.IOException;
	public abstract long getSize() throws java.io.IOException;
}
