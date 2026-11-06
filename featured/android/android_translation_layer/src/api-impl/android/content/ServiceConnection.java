package android.content;

import android.os.IBinder;

public interface ServiceConnection {
	public void onServiceConnected(ComponentName name, IBinder service);

	// GEN member:android.content.ServiceConnection:public abstract void onServiceDisconnected(android.content.ComponentName);
	default void onServiceDisconnected(android.content.ComponentName arg0) {}
}
