package android.net.wifi;

import android.os.WorkSource;

// Android's WifiScanner (a system API): no scanner here, a scan fails
public class WifiScanner {

	public static final int WIFI_BAND_UNSPECIFIED = 0;
	public static final int WIFI_BAND_24_GHZ = 1;
	public static final int WIFI_BAND_5_GHZ = 2;
	public static final int WIFI_BAND_5_GHZ_DFS_ONLY = 4;
	public static final int WIFI_BAND_5_GHZ_WITH_DFS = 6;
	public static final int WIFI_BAND_BOTH = 3;
	public static final int WIFI_BAND_BOTH_WITH_DFS = 7;
	public static final int WIFI_BAND_6_GHZ = 8;
	public static final int WIFI_BAND_24_5_6_GHZ = 11;
	public static final int WIFI_BAND_24_5_WITH_DFS_6_GHZ = 15;
	public static final int REASON_SUCCEEDED = 0;
	public static final int REASON_UNSPECIFIED = -1;
	public static final int REASON_INVALID_LISTENER = -2;
	public static final int REASON_INVALID_REQUEST = -3;
	public static final int REASON_NOT_AUTHORIZED = -4;
	public static final int REASON_DUPLICATE_REQEUST = -5;
	public static final int REPORT_EVENT_AFTER_EACH_SCAN = 1;
	public static final int REPORT_EVENT_FULL_SCAN_RESULT = 2;

	public static class ScanSettings {
		public int band;
		public int periodInMs;
		public int reportEvents;
		public int numBssidsPerScan;
		public int maxScansToCache;
		public int type;
		public boolean hideFromAppOps;
		public boolean ignoreLocationSettings;
	}

	public static class ScanData {
		public int getId() { return 0; }
		public int getFlags() { return 0; }
		public ScanResult[] getResults() { return new ScanResult[0]; }
	}

	public interface ActionListener {
		void onSuccess();
		void onFailure(int reason, String description);
	}

	public interface ScanListener extends ActionListener {
		void onPeriodChanged(int periodInMs);
		void onResults(ScanData[] results);
		void onFullResult(ScanResult fullScanResult);
	}

	public boolean isScanning() { return false; }

	public void startScan(ScanSettings settings, ScanListener listener) {
		startScan(settings, listener, null);
	}

	public void startScan(ScanSettings settings, ScanListener listener, WorkSource workSource) {
		if (listener != null)
			listener.onFailure(REASON_UNSPECIFIED, "no scanner");
	}

	public void stopScan(ScanListener listener) {}

	public java.util.List<ScanResult> getSingleScanResults() {
		return new java.util.ArrayList<>();
	}
}
