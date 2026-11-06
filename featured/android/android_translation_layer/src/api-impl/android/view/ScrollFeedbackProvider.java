package android.view;

// android.jar's shape, made by gen.py
public interface ScrollFeedbackProvider {
	// GEN member:android.view.ScrollFeedbackProvider:public static android.view.ScrollFeedbackProvider createProvider(android.view.View);
	static android.view.ScrollFeedbackProvider createProvider(android.view.View arg0) { return null; }

	// GEN member:android.view.ScrollFeedbackProvider:public abstract void onSnapToItem(int, int, int);
	default void onSnapToItem(int arg0, int arg1, int arg2) {}

	// GEN member:android.view.ScrollFeedbackProvider:public abstract void onScrollLimit(int, int, int, boolean);
	default void onScrollLimit(int arg0, int arg1, int arg2, boolean arg3) {}

	// GEN member:android.view.ScrollFeedbackProvider:public abstract void onScrollProgress(int, int, int, int);
	default void onScrollProgress(int arg0, int arg1, int arg2, int arg3) {}
}
