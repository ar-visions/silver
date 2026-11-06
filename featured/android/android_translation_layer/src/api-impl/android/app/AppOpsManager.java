package android.app;

// one app per process, nothing restricted: every op is allowed
public class AppOpsManager {
	public static final int MODE_ALLOWED = 0;
	public static final int MODE_IGNORED = 1;
	public static final int MODE_ERRORED = 2;
	public static final int MODE_DEFAULT = 3;
	public static final int MODE_FOREGROUND = 4;

	public static final String OPSTR_COARSE_LOCATION = "android:coarse_location";
	public static final String OPSTR_FINE_LOCATION = "android:fine_location";
	public static final String OPSTR_GET_USAGE_STATS = "android:get_usage_stats";
	public static final String OPSTR_SYSTEM_ALERT_WINDOW = "android:system_alert_window";
	public static final String OPSTR_CAMERA = "android:camera";
	public static final String OPSTR_RECORD_AUDIO = "android:record_audio";
	public static final String OPSTR_READ_CONTACTS = "android:read_contacts";
	public static final String OPSTR_POST_NOTIFICATION = "android:post_notification";
	public static final String OPSTR_PICTURE_IN_PICTURE = "android:picture_in_picture";

	public interface OnOpChangedListener {
		void onOpChanged(String op, String packageName);
	}

	public static String permissionToOp(String permission) {
		return permission;
	}

	public int checkOp(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int checkOpNoThrow(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int checkOp(int op, int uid, String packageName) { return MODE_ALLOWED; }
	public int checkOpNoThrow(int op, int uid, String packageName) { return MODE_ALLOWED; }
	public int unsafeCheckOp(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int unsafeCheckOpNoThrow(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int unsafeCheckOpRaw(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int unsafeCheckOpRawNoThrow(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int noteOp(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int noteOpNoThrow(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int noteOp(String op, int uid, String packageName, String tag, String message) { return MODE_ALLOWED; }
	public int noteOpNoThrow(String op, int uid, String packageName, String tag, String message) { return MODE_ALLOWED; }
	public int noteProxyOp(String op, String proxiedPackageName) { return MODE_ALLOWED; }
	public int noteProxyOpNoThrow(String op, String pkg) { return MODE_ALLOWED; }
	public int noteProxyOpNoThrow(String op, String pkg, int uid) { return MODE_ALLOWED; }
	public int noteProxyOpNoThrow(String op, String pkg, int uid, String tag, String message) { return MODE_ALLOWED; }
	public int startOp(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int startOpNoThrow(String op, int uid, String packageName) { return MODE_ALLOWED; }
	public int startOpNoThrow(String op, int uid, String packageName, String tag, String message) { return MODE_ALLOWED; }
	public void finishOp(String op, int uid, String packageName) {}
	public void finishOp(String op, int uid, String packageName, String tag) {}
	public void checkPackage(int uid, String packageName) {}
	public void startWatchingMode(String op, String packageName, OnOpChangedListener callback) {}
	public void startWatchingMode(String op, String packageName, int flags, OnOpChangedListener callback) {}
	public void stopWatchingMode(OnOpChangedListener callback) {}

	// GEN class:android.app.AppOpsManager$OnOpNotedCallback
	public abstract class OnOpNotedCallback {
		// GEN member:android.app.AppOpsManager$OnOpNotedCallback:public android.app.AppOpsManager$OnOpNotedCallback();
		public OnOpNotedCallback() {}
	
		// GEN member:android.app.AppOpsManager$OnOpNotedCallback:public abstract void onNoted(android.app.SyncNotedAppOp);
		public abstract void onNoted(android.app.SyncNotedAppOp arg0);
	
		// GEN member:android.app.AppOpsManager$OnOpNotedCallback:public abstract void onSelfNoted(android.app.SyncNotedAppOp);
		public abstract void onSelfNoted(android.app.SyncNotedAppOp arg0);
	
		// GEN member:android.app.AppOpsManager$OnOpNotedCallback:public abstract void onAsyncNoted(android.app.AsyncNotedAppOp);
		public abstract void onAsyncNoted(android.app.AsyncNotedAppOp arg0);
	}


	// GEN member:android.app.AppOpsManager:public void setOnOpNotedCallback(java.util.concurrent.Executor, android.app.AppOpsManager$OnOpNotedCallback);
	public void setOnOpNotedCallback(java.util.concurrent.Executor arg0, android.app.AppOpsManager.OnOpNotedCallback arg1) {}
}
