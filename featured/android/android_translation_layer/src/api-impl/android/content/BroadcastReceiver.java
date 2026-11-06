package android.content;

public abstract class BroadcastReceiver {

	public static class PendingResult {

		public void setResultCode(int resultCode) {}

		public void finish() {}
	
	// GEN member:android.content.BroadcastReceiver$PendingResult:public final void setResultExtras(android.os.Bundle);
	public final void setResultExtras(android.os.Bundle arg0) {}

	// GEN member:android.content.BroadcastReceiver$PendingResult:public final void setResult(int, java.lang.String, android.os.Bundle);
	public final void setResult(int arg0, java.lang.String arg1, android.os.Bundle arg2) {}
}

	public abstract void onReceive(Context context, Intent intent);

	public boolean isOrderedBroadcast() {
		return true;
	}

	public PendingResult goAsync() {
		return new PendingResult();
	}

	// GEN member:android.content.BroadcastReceiver:public final boolean isInitialStickyBroadcast();
	public final boolean isInitialStickyBroadcast() { return false; }

	// GEN member:android.content.BroadcastReceiver:public final void setResultCode(int);
	public final void setResultCode(int arg0) {}
}
