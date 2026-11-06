package android.window;

// android.jar's shape, made by gen.py
public interface OnBackInvokedDispatcher {
	// GEN member:android.window.OnBackInvokedDispatcher:public static final int PRIORITY_DEFAULT = 0;
	int PRIORITY_DEFAULT = 0;

	// GEN member:android.window.OnBackInvokedDispatcher:public static final int PRIORITY_OVERLAY = 1000000;
	int PRIORITY_OVERLAY = 1000000;

	// GEN member:android.window.OnBackInvokedDispatcher:public abstract void registerOnBackInvokedCallback(int, android.window.OnBackInvokedCallback);
	default void registerOnBackInvokedCallback(int arg0, android.window.OnBackInvokedCallback arg1) {}

	// GEN member:android.window.OnBackInvokedDispatcher:public abstract void unregisterOnBackInvokedCallback(android.window.OnBackInvokedCallback);
	default void unregisterOnBackInvokedCallback(android.window.OnBackInvokedCallback arg0) {}
}
