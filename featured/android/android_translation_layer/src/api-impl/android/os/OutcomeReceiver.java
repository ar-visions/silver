package android.os;

// android.jar's shape, made by gen.py
public interface OutcomeReceiver<R, E extends java.lang.Throwable> {
	// GEN member:android.os.OutcomeReceiver:public abstract void onResult(R);
	default void onResult(R arg0) {}

	// GEN member:android.os.OutcomeReceiver:public default void onError(E);
	default void onError(E arg0) {}
}
