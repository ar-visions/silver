package android.net.http;

// android.jar's shape, made by gen.py
public abstract class UploadDataSink {
	// GEN member:android.net.http.UploadDataSink:public android.net.http.UploadDataSink();
	public UploadDataSink() {}

	// GEN member:android.net.http.UploadDataSink:public abstract void onReadSucceeded(boolean);
	public abstract void onReadSucceeded(boolean arg0);

	// GEN member:android.net.http.UploadDataSink:public abstract void onReadError(java.lang.Exception);
	public abstract void onReadError(java.lang.Exception arg0);

	// GEN member:android.net.http.UploadDataSink:public abstract void onRewindSucceeded();
	public abstract void onRewindSucceeded();

	// GEN member:android.net.http.UploadDataSink:public abstract void onRewindError(java.lang.Exception);
	public abstract void onRewindError(java.lang.Exception arg0);
}
