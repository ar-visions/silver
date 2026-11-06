package android.app;

import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.util.Slog;

public class DownloadManager {

	public static class Request {

		public Request(Uri uri) {}

		public Request setAllowedNetworkTypes(int allowedNetworkTypes) { return this; }

		public Request setTitle(CharSequence title) { return this; }

		public Request setDescription(CharSequence description) { return this; }

		public Request setDestinationInExternalFilesDir(Context context, String dirType, String path) { return this; }

		public Request setNotificationVisibility(int visibility) { return this; }
	
	// GEN member:android.app.DownloadManager$Request:public android.app.DownloadManager$Request setRequiresCharging(boolean);
	public android.app.DownloadManager.Request setRequiresCharging(boolean arg0) { return null; }
}

	public static class Query {

		public Query setFilterByStatus(int status) { return this; }

		public Query setFilterById(long[] ids) { return this; }
	}

	public long enqueue(Request request) {
		Slog.i("DownloadManager", "enqueue() called");
		return 0;
	}

	public Cursor query(Query query) {
		Slog.i("DownloadManager", "query() called");
		return new MatrixCursor(new String[] {"_id"});
	}

	// GEN member:android.app.DownloadManager:public android.os.ParcelFileDescriptor openDownloadedFile(long) throws java.io.FileNotFoundException;
	public android.os.ParcelFileDescriptor openDownloadedFile(long arg0) throws java.io.FileNotFoundException { return null; }

	// GEN member:android.app.DownloadManager:public int remove(long...);
	public int remove(long... arg0) { return 0; }
}
