package android.bluetooth.le;

// android.jar's shape, made by gen.py
public final class ScanResult implements android.os.Parcelable {
	// GEN member:android.bluetooth.le.ScanResult:public static final android.os.Parcelable$Creator<android.bluetooth.le.ScanResult> CREATOR;
	public static final android.os.Parcelable.Creator<android.bluetooth.le.ScanResult> CREATOR = null;

	// GEN member:android.bluetooth.le.ScanResult:public static final int DATA_COMPLETE = 0;
	public static final int DATA_COMPLETE = 0;

	// GEN member:android.bluetooth.le.ScanResult:public static final int DATA_TRUNCATED = 2;
	public static final int DATA_TRUNCATED = 2;

	// GEN member:android.bluetooth.le.ScanResult:public static final int PERIODIC_INTERVAL_NOT_PRESENT = 0;
	public static final int PERIODIC_INTERVAL_NOT_PRESENT = 0;

	// GEN member:android.bluetooth.le.ScanResult:public static final int PHY_UNUSED = 0;
	public static final int PHY_UNUSED = 0;

	// GEN member:android.bluetooth.le.ScanResult:public static final int SID_NOT_PRESENT = 255;
	public static final int SID_NOT_PRESENT = 255;

	// GEN member:android.bluetooth.le.ScanResult:public static final int TX_POWER_NOT_PRESENT = 127;
	public static final int TX_POWER_NOT_PRESENT = 127;

	// GEN member:android.bluetooth.le.ScanResult:public android.bluetooth.le.ScanResult(android.bluetooth.BluetoothDevice, android.bluetooth.le.ScanRecord, int, long);
	public ScanResult(android.bluetooth.BluetoothDevice arg0, android.bluetooth.le.ScanRecord arg1, int arg2, long arg3) {}

	// GEN member:android.bluetooth.le.ScanResult:public android.bluetooth.le.ScanResult(android.bluetooth.BluetoothDevice, int, int, int, int, int, int, int, android.bluetooth.le.ScanRecord, long);
	public ScanResult(android.bluetooth.BluetoothDevice arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, android.bluetooth.le.ScanRecord arg8, long arg9) {}

	// GEN member:android.bluetooth.le.ScanResult:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.bluetooth.le.ScanResult:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public android.bluetooth.BluetoothDevice getDevice();
	public android.bluetooth.BluetoothDevice getDevice() { return null; }

	// GEN member:android.bluetooth.le.ScanResult:public android.bluetooth.le.ScanRecord getScanRecord();
	public android.bluetooth.le.ScanRecord getScanRecord() { return null; }

	// GEN member:android.bluetooth.le.ScanResult:public int getRssi();
	public int getRssi() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public long getTimestampNanos();
	public long getTimestampNanos() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public boolean isLegacy();
	public boolean isLegacy() { return false; }

	// GEN member:android.bluetooth.le.ScanResult:public boolean isConnectable();
	public boolean isConnectable() { return false; }

	// GEN member:android.bluetooth.le.ScanResult:public int getDataStatus();
	public int getDataStatus() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int getPrimaryPhy();
	public int getPrimaryPhy() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int getSecondaryPhy();
	public int getSecondaryPhy() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int getAdvertisingSid();
	public int getAdvertisingSid() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int getTxPower();
	public int getTxPower() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int getPeriodicAdvertisingInterval();
	public int getPeriodicAdvertisingInterval() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.bluetooth.le.ScanResult:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.bluetooth.le.ScanResult:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected ScanResult() {}
}
