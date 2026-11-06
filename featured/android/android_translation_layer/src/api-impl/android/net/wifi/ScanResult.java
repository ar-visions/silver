package android.net.wifi;

// android.jar's shape, made by gen.py
public final class ScanResult implements android.os.Parcelable {
	// GEN member:android.net.wifi.ScanResult:public java.lang.String BSSID;
	public java.lang.String BSSID;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_160MHZ = 3;
	public static final int CHANNEL_WIDTH_160MHZ = 3;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_20MHZ = 0;
	public static final int CHANNEL_WIDTH_20MHZ = 0;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_320MHZ = 5;
	public static final int CHANNEL_WIDTH_320MHZ = 5;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_40MHZ = 1;
	public static final int CHANNEL_WIDTH_40MHZ = 1;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_80MHZ = 2;
	public static final int CHANNEL_WIDTH_80MHZ = 2;

	// GEN member:android.net.wifi.ScanResult:public static final int CHANNEL_WIDTH_80MHZ_PLUS_MHZ = 4;
	public static final int CHANNEL_WIDTH_80MHZ_PLUS_MHZ = 4;

	// GEN member:android.net.wifi.ScanResult:public static final android.os.Parcelable$Creator<android.net.wifi.ScanResult> CREATOR;
	public static final android.os.Parcelable.Creator<android.net.wifi.ScanResult> CREATOR = null;

	// GEN member:android.net.wifi.ScanResult:public static final int PREAMBLE_EHT = 4;
	public static final int PREAMBLE_EHT = 4;

	// GEN member:android.net.wifi.ScanResult:public static final int PREAMBLE_HE = 3;
	public static final int PREAMBLE_HE = 3;

	// GEN member:android.net.wifi.ScanResult:public static final int PREAMBLE_HT = 1;
	public static final int PREAMBLE_HT = 1;

	// GEN member:android.net.wifi.ScanResult:public static final int PREAMBLE_LEGACY = 0;
	public static final int PREAMBLE_LEGACY = 0;

	// GEN member:android.net.wifi.ScanResult:public static final int PREAMBLE_VHT = 2;
	public static final int PREAMBLE_VHT = 2;

	// GEN member:android.net.wifi.ScanResult:public java.lang.String SSID;
	public java.lang.String SSID;

	// GEN member:android.net.wifi.ScanResult:public static final int UNSPECIFIED = -1;
	public static final int UNSPECIFIED = -1;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_BAND_24_GHZ = 1;
	public static final int WIFI_BAND_24_GHZ = 1;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_BAND_5_GHZ = 2;
	public static final int WIFI_BAND_5_GHZ = 2;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_BAND_60_GHZ = 16;
	public static final int WIFI_BAND_60_GHZ = 16;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_BAND_6_GHZ = 8;
	public static final int WIFI_BAND_6_GHZ = 8;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_11AC = 5;
	public static final int WIFI_STANDARD_11AC = 5;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_11AD = 7;
	public static final int WIFI_STANDARD_11AD = 7;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_11AX = 6;
	public static final int WIFI_STANDARD_11AX = 6;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_11BE = 8;
	public static final int WIFI_STANDARD_11BE = 8;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_11N = 4;
	public static final int WIFI_STANDARD_11N = 4;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_LEGACY = 1;
	public static final int WIFI_STANDARD_LEGACY = 1;

	// GEN member:android.net.wifi.ScanResult:public static final int WIFI_STANDARD_UNKNOWN = 0;
	public static final int WIFI_STANDARD_UNKNOWN = 0;

	// GEN member:android.net.wifi.ScanResult:public java.lang.String capabilities;
	public java.lang.String capabilities;

	// GEN member:android.net.wifi.ScanResult:public int centerFreq0;
	public int centerFreq0;

	// GEN member:android.net.wifi.ScanResult:public int centerFreq1;
	public int centerFreq1;

	// GEN member:android.net.wifi.ScanResult:public int channelWidth;
	public int channelWidth;

	// GEN member:android.net.wifi.ScanResult:public int frequency;
	public int frequency;

	// GEN member:android.net.wifi.ScanResult:public int level;
	public int level;

	// GEN member:android.net.wifi.ScanResult:public java.lang.CharSequence operatorFriendlyName;
	public java.lang.CharSequence operatorFriendlyName;

	// GEN member:android.net.wifi.ScanResult:public long timestamp;
	public long timestamp;

	// GEN member:android.net.wifi.ScanResult:public java.lang.CharSequence venueName;
	public java.lang.CharSequence venueName;

	// GEN member:android.net.wifi.ScanResult:public android.net.wifi.ScanResult(android.net.wifi.ScanResult);
	public ScanResult(android.net.wifi.ScanResult arg0) {}

	// GEN member:android.net.wifi.ScanResult:public android.net.wifi.ScanResult();
	public ScanResult() {}

	// GEN member:android.net.wifi.ScanResult:public int getApMloLinkId();
	public int getApMloLinkId() { return 0; }

	// GEN member:android.net.wifi.ScanResult:public int getWifiStandard();
	public int getWifiStandard() { return 0; }

	// GEN member:android.net.wifi.ScanResult:public boolean is80211mcResponder();
	public boolean is80211mcResponder() { return false; }

	// GEN member:android.net.wifi.ScanResult:public boolean is80211azNtbResponder();
	public boolean is80211azNtbResponder() { return false; }

	// GEN member:android.net.wifi.ScanResult:public boolean isPasspointNetwork();
	public boolean isPasspointNetwork() { return false; }

	// GEN member:android.net.wifi.ScanResult:public boolean isTwtResponder();
	public boolean isTwtResponder() { return false; }

	// GEN member:android.net.wifi.ScanResult:public static int convertChannelToFrequencyMhzIfSupported(int, int);
	public static int convertChannelToFrequencyMhzIfSupported(int arg0, int arg1) { return 0; }

	// GEN member:android.net.wifi.ScanResult:public static int convertFrequencyMhzToChannelIfSupported(int);
	public static int convertFrequencyMhzToChannelIfSupported(int arg0) { return 0; }

	// GEN member:android.net.wifi.ScanResult:public int[] getSecurityTypes();
	public int[] getSecurityTypes() { return new int[0]; }

	// GEN member:android.net.wifi.ScanResult:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.net.wifi.ScanResult:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.net.wifi.ScanResult:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}
}
