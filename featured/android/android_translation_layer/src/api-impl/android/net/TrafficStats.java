package android.net;

public class TrafficStats {
	public static void setThreadStatsTag(int dummy) {}

	public static int getThreadStatsTag() {
		return 0;
	}

	public static void clearThreadStatsTag() {}

	public static long getUidRxBytes(int uid) {
		return -1;
	}

	public static long getUidTxBytes(int uid) {
		return -1;
	}

	// GEN member:android.net.TrafficStats:public static void clearThreadStatsUid();
	public static void clearThreadStatsUid() {}
}
