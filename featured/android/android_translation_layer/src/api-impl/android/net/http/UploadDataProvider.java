package android.net.http;

// android.jar's shape, made by gen.py
public abstract class UploadDataProvider implements java.io.Closeable {
	// GEN member:android.net.http.UploadDataProvider:public android.net.http.UploadDataProvider();
	public UploadDataProvider() {}

	// GEN member:android.net.http.UploadDataProvider:public abstract long getLength() throws java.io.IOException;
	public abstract long getLength() throws java.io.IOException;

	// GEN member:android.net.http.UploadDataProvider:public abstract void read(android.net.http.UploadDataSink, java.nio.ByteBuffer) throws java.io.IOException;
	public abstract void read(android.net.http.UploadDataSink arg0, java.nio.ByteBuffer arg1) throws java.io.IOException;

	// GEN member:android.net.http.UploadDataProvider:public abstract void rewind(android.net.http.UploadDataSink) throws java.io.IOException;
	public abstract void rewind(android.net.http.UploadDataSink arg0) throws java.io.IOException;

	// GEN member:android.net.http.UploadDataProvider:public void close() throws java.io.IOException;
	public void close() throws java.io.IOException {}
}
