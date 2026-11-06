package android.webkit;

import android.content.Context;

public class CookieSyncManager implements java.lang.Runnable {

	public static CookieSyncManager createInstance(Context context) {
		return new CookieSyncManager();
	}

	public void sync() {}
	public void run() {}

	// GEN member:android.webkit.CookieSyncManager:public static android.webkit.CookieSyncManager getInstance();
	public static android.webkit.CookieSyncManager getInstance() { return null; }
}
