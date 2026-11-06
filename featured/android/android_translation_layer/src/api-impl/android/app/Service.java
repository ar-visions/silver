package android.app;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.IBinder;

public abstract class Service extends ContextWrapper implements android.content.ComponentCallbacks2 {

	private int notification_id;

	public Service() {
		super(null);
	}

	public void onCreate() {
		System.out.println("Service.onCreate() called");
	}

	public void onDestroy() {
		System.out.println("Service.onDestroy() called");
	}

	public abstract IBinder onBind(Intent intent);

	public int onStartCommand(Intent intent, int flags, int startId) {
		System.out.println("Service.onStartCommand(" + intent + ", " + flags + ", " + startId + ") called");
		return 0;
	}

	public void startForeground(int id, Notification notification) {
		System.out.println("startForeground(" + id + ", " + notification + ") called");
		this.notification_id = id;
	}

	public void stopForeground(boolean remove) {
		System.out.println("stopForeground(" + remove + ") called");
		if (remove)
			new NotificationManager().cancel(notification_id);
	}

	public void stopForeground(int remove) {
		stopForeground(remove == 1);
	}

	public Application getApplication() {
		return this.get_atl_loaded_app().getApplication();
	}

	public void stopSelf(int startId) {
		System.out.println("Service.stopSelf(" + startId + ") called");
	}

	public void stopSelf() {
		System.out.println("Service.stopSelf() called");
	}

	public boolean stopSelfResult(int startId) {
		System.out.println("Service.stopSelfResult(" + startId + ") called");
		return true;
	}

	public void attachBaseContext(Context newBase) {
		super.attachBaseContext(newBase);
		System.out.println("Service.attachBaseContext(" + newBase + ") called");
	}

	// GEN member:android.app.Service:public void onConfigurationChanged(android.content.res.Configuration);
	public void onConfigurationChanged(android.content.res.Configuration arg0) {}

	// GEN member:android.app.Service:public void onStart(android.content.Intent, int);
	public void onStart(android.content.Intent arg0, int arg1) {}

	// GEN member:android.app.Service:public boolean onUnbind(android.content.Intent);
	public boolean onUnbind(android.content.Intent arg0) { return false; }

	// GEN member:android.app.Service:public final void startForeground(int, android.app.Notification, int);
	public final void startForeground(int arg0, android.app.Notification arg1, int arg2) {}

	// GEN member:android.app.Service:public void onRebind(android.content.Intent);
	public void onRebind(android.content.Intent arg0) {}

	// GEN member:android.app.Service:public void onTrimMemory(int);
	public void onTrimMemory(int arg0) {}
}
