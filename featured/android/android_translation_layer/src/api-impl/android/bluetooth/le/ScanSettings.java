package android.bluetooth.le;

// android.jar's shape, made by gen.py
public final class ScanSettings implements android.os.Parcelable {
	// GEN member:android.bluetooth.le.ScanSettings:public static final long AUTO_BATCH_MIN_REPORT_DELAY_MILLIS = 600000l;
	public static final long AUTO_BATCH_MIN_REPORT_DELAY_MILLIS = 600000L;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int CALLBACK_TYPE_ALL_MATCHES = 1;
	public static final int CALLBACK_TYPE_ALL_MATCHES = 1;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int CALLBACK_TYPE_ALL_MATCHES_AUTO_BATCH = 8;
	public static final int CALLBACK_TYPE_ALL_MATCHES_AUTO_BATCH = 8;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int CALLBACK_TYPE_FIRST_MATCH = 2;
	public static final int CALLBACK_TYPE_FIRST_MATCH = 2;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int CALLBACK_TYPE_MATCH_LOST = 4;
	public static final int CALLBACK_TYPE_MATCH_LOST = 4;

	// GEN member:android.bluetooth.le.ScanSettings:public static final android.os.Parcelable$Creator<android.bluetooth.le.ScanSettings> CREATOR;
	public static final android.os.Parcelable.Creator<android.bluetooth.le.ScanSettings> CREATOR = null;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int MATCH_MODE_AGGRESSIVE = 1;
	public static final int MATCH_MODE_AGGRESSIVE = 1;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int MATCH_MODE_STICKY = 2;
	public static final int MATCH_MODE_STICKY = 2;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int MATCH_NUM_FEW_ADVERTISEMENT = 2;
	public static final int MATCH_NUM_FEW_ADVERTISEMENT = 2;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int MATCH_NUM_MAX_ADVERTISEMENT = 3;
	public static final int MATCH_NUM_MAX_ADVERTISEMENT = 3;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int MATCH_NUM_ONE_ADVERTISEMENT = 1;
	public static final int MATCH_NUM_ONE_ADVERTISEMENT = 1;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int PHY_LE_ALL_SUPPORTED = 255;
	public static final int PHY_LE_ALL_SUPPORTED = 255;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int SCAN_MODE_BALANCED = 1;
	public static final int SCAN_MODE_BALANCED = 1;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int SCAN_MODE_LOW_LATENCY = 2;
	public static final int SCAN_MODE_LOW_LATENCY = 2;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int SCAN_MODE_LOW_POWER = 0;
	public static final int SCAN_MODE_LOW_POWER = 0;

	// GEN member:android.bluetooth.le.ScanSettings:public static final int SCAN_MODE_OPPORTUNISTIC = -1;
	public static final int SCAN_MODE_OPPORTUNISTIC = -1;

	// GEN member:android.bluetooth.le.ScanSettings:public int getScanMode();
	public int getScanMode() { return 0; }

	// GEN member:android.bluetooth.le.ScanSettings:public int getCallbackType();
	public int getCallbackType() { return 0; }

	// GEN member:android.bluetooth.le.ScanSettings:public int getScanResultType();
	public int getScanResultType() { return 0; }

	// GEN member:android.bluetooth.le.ScanSettings:public boolean getLegacy();
	public boolean getLegacy() { return false; }

	// GEN member:android.bluetooth.le.ScanSettings:public int getPhy();
	public int getPhy() { return 0; }

	// GEN member:android.bluetooth.le.ScanSettings:public long getReportDelayMillis();
	public long getReportDelayMillis() { return 0; }

	// GEN member:android.bluetooth.le.ScanSettings:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.bluetooth.le.ScanSettings:public int describeContents();
	public int describeContents() { return 0; }

	protected ScanSettings() {}

	// GEN class:android.bluetooth.le.ScanSettings$Builder
	public final class Builder {
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder();
		public Builder() {}
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setScanMode(int);
		public android.bluetooth.le.ScanSettings.Builder setScanMode(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setCallbackType(int);
		public android.bluetooth.le.ScanSettings.Builder setCallbackType(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setReportDelay(long);
		public android.bluetooth.le.ScanSettings.Builder setReportDelay(long arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setNumOfMatches(int);
		public android.bluetooth.le.ScanSettings.Builder setNumOfMatches(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setMatchMode(int);
		public android.bluetooth.le.ScanSettings.Builder setMatchMode(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setLegacy(boolean);
		public android.bluetooth.le.ScanSettings.Builder setLegacy(boolean arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings$Builder setPhy(int);
		public android.bluetooth.le.ScanSettings.Builder setPhy(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanSettings$Builder:public android.bluetooth.le.ScanSettings build();
		public android.bluetooth.le.ScanSettings build() { return null; }
	}

}
