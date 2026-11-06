package android.media;

import android.content.Context;

public class MediaScannerConnection implements android.content.ServiceConnection {

	public interface OnScanCompletedListener {}

	public static void scanFile(Context context, String[] filePath, String[] mimeType, OnScanCompletedListener listener) {}
	public void onServiceConnected(android.content.ComponentName arg0, android.os.IBinder arg1) {}
}
