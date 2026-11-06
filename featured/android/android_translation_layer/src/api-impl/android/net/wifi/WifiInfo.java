package android.net.wifi;

public class WifiInfo implements android.net.TransportInfo, android.os.Parcelable {
	public String getMacAddress() {
		return "";
	}

	public String getBSSID() {
		return "";
	}

	public int getIpAddress() {
		return -1;
	}
}
