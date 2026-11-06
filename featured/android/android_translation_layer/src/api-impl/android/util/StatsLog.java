package android.util;

// android.jar's shape, made by gen.py
public final class StatsLog {
	// GEN member:android.util.StatsLog:public static boolean logStart(int);
	public static boolean logStart(int arg0) { return false; }

	// GEN member:android.util.StatsLog:public static boolean logStop(int);
	public static boolean logStop(int arg0) { return false; }

	// GEN member:android.util.StatsLog:public static boolean logEvent(int);
	public static boolean logEvent(int arg0) { return false; }

	// GEN member:android.util.StatsLog:public static boolean logBinaryPushStateChanged(java.lang.String, long, int, int, long[]);
	public static boolean logBinaryPushStateChanged(java.lang.String arg0, long arg1, int arg2, int arg3, long[] arg4) { return false; }

	// a system API (statsd): Cronet's telemetry writes atoms here
	public static void write(StatsEvent event) {}

	protected StatsLog() {}
}
