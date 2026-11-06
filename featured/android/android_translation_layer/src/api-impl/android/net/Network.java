package android.net;

public class Network implements android.os.Parcelable {
	public long getNetworkHandle() {
		return 1L;
	}

	public java.net.URLConnection openConnection(java.net.URL url) throws java.io.IOException {
		return url.openConnection();
	}

	// GEN member:android.net.Network:public static final android.os.Parcelable$Creator<android.net.Network> CREATOR;
	public static final android.os.Parcelable.Creator<android.net.Network> CREATOR = null;

	// GEN member:android.net.Network:public static android.net.Network fromNetworkHandle(long);
	public static android.net.Network fromNetworkHandle(long arg0) { return null; }
}
