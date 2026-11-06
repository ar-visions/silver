package android.app;

public class KeyguardManager {
	public boolean inKeyguardRestrictedInputMode() {
		return false;
	}

	public boolean isKeyguardLocked() {
		return false;
	}

	public boolean isKeyguardSecure() {
		return true;
	}

	public KeyguardLock newKeyguardLock(String tag) {
		return new KeyguardLock();
	}

	public boolean isDeviceSecure() {
		return false;
	}

	public class KeyguardLock {
		public void disableKeyguard() {}
		public void reenableKeyguard() {}
	}

	// GEN class:android.app.KeyguardManager$KeyguardDismissCallback
	public abstract class KeyguardDismissCallback {
		// GEN member:android.app.KeyguardManager$KeyguardDismissCallback:public android.app.KeyguardManager$KeyguardDismissCallback();
		public KeyguardDismissCallback() {}
	
		// GEN member:android.app.KeyguardManager$KeyguardDismissCallback:public void onDismissError();
		public void onDismissError() {}
	
		// GEN member:android.app.KeyguardManager$KeyguardDismissCallback:public void onDismissSucceeded();
		public void onDismissSucceeded() {}
	
		// GEN member:android.app.KeyguardManager$KeyguardDismissCallback:public void onDismissCancelled();
		public void onDismissCancelled() {}
	}


	// GEN member:android.app.KeyguardManager:public void requestDismissKeyguard(android.app.Activity, android.app.KeyguardManager$KeyguardDismissCallback);
	public void requestDismissKeyguard(android.app.Activity arg0, android.app.KeyguardManager.KeyguardDismissCallback arg1) {}
}
