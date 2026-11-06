package android.net;

public final class NetworkCapabilities implements android.os.Parcelable {
	public static final int NET_CAPABILITY_MMS = 0;
	public static final int NET_CAPABILITY_SUPL = 1;
	public static final int NET_CAPABILITY_DUN = 2;
	public static final int NET_CAPABILITY_FOTA = 3;
	public static final int NET_CAPABILITY_IMS = 4;
	public static final int NET_CAPABILITY_CBS = 5;
	public static final int NET_CAPABILITY_WIFI_P2P = 6;
	public static final int NET_CAPABILITY_IA = 7;
	public static final int NET_CAPABILITY_RCS = 8;
	public static final int NET_CAPABILITY_XCAP = 9;
	public static final int NET_CAPABILITY_EIMS = 10;
	public static final int NET_CAPABILITY_NOT_METERED = 11;
	public static final int NET_CAPABILITY_INTERNET = 12;
	public static final int NET_CAPABILITY_NOT_RESTRICTED = 13;
	public static final int NET_CAPABILITY_TRUSTED = 14;
	public static final int NET_CAPABILITY_NOT_VPN = 15;
	public static final int NET_CAPABILITY_VALIDATED = 16;
	public static final int NET_CAPABILITY_CAPTIVE_PORTAL = 17;
	public static final int NET_CAPABILITY_NOT_ROAMING = 18;
	public static final int NET_CAPABILITY_FOREGROUND = 19;
	public static final int NET_CAPABILITY_NOT_CONGESTED = 20;
	public static final int NET_CAPABILITY_NOT_SUSPENDED = 21;
	public static final int NET_CAPABILITY_OEM_PAID = 22;
	public static final int NET_CAPABILITY_MCX = 23;
	public static final int NET_CAPABILITY_PARTIAL_CONNECTIVITY = 24;
	public static final int NET_CAPABILITY_TEMPORARILY_NOT_METERED = 25;
	public static final int NET_CAPABILITY_OEM_PRIVATE = 26;
	public static final int NET_CAPABILITY_VEHICLE_INTERNAL = 27;
	public static final int NET_CAPABILITY_NOT_VCN_MANAGED = 28;
	public static final int NET_CAPABILITY_ENTERPRISE = 29;
	public static final int NET_CAPABILITY_VSIM = 30;
	public static final int NET_CAPABILITY_BIP = 31;
	public static final int NET_CAPABILITY_HEAD_UNIT = 32;
	public static final int NET_CAPABILITY_MMTEL = 33;
	public static final int NET_CAPABILITY_PRIORITIZE_LATENCY = 34;
	public static final int NET_CAPABILITY_PRIORITIZE_BANDWIDTH = 35;

	public static final int TRANSPORT_CELLULAR = 0;
	public static final int TRANSPORT_WIFI = 1;
	public static final int TRANSPORT_BLUETOOTH = 2;
	public static final int TRANSPORT_ETHERNET = 3;
	public static final int TRANSPORT_VPN = 4;
	public static final int TRANSPORT_WIFI_AWARE = 5;
	public static final int TRANSPORT_LOWPAN = 6;
	public static final int TRANSPORT_TEST = 7;
	public static final int TRANSPORT_USB = 8;
	public static final int TRANSPORT_THREAD = 9;

	private static final long NOT_RESTRICTED = 1L << NET_CAPABILITY_NOT_RESTRICTED;
	private static final long TRUSTED = 1L << NET_CAPABILITY_TRUSTED;
	private static final long NOT_VPN = 1L << NET_CAPABILITY_NOT_VPN;

	private static final int MIN_NET_CAPABILITY = NET_CAPABILITY_MMS;
	private static final int MAX_NET_CAPABILITY = NET_CAPABILITY_PRIORITIZE_BANDWIDTH;

	private static final int MIN_TRANSPORT = TRANSPORT_CELLULAR;
	private static final int MAX_TRANSPORT = TRANSPORT_THREAD;

	// AOSP sets these on every freshly constructed instance
	private static final long DEFAULT_CAPABILITIES = NOT_RESTRICTED | TRUSTED | NOT_VPN;

	private long mNetworkCapabilities = DEFAULT_CAPABILITIES;
	private long mTransportTypes;

	public boolean hasCapability(int capability) {
		if (!isValidCapability(capability))
			return false;
		return (mNetworkCapabilities & (1L << capability)) != 0;
	}

	public boolean hasTransport(int transportType) {
		return isValidTransport(transportType) && ((mTransportTypes & (1L << transportType)) != 0);
	}

	public NetworkCapabilities addCapability(int capability) {
		checkValidCapability(capability);
		mNetworkCapabilities |= 1L << capability;
		return this;
	}

	public NetworkCapabilities removeCapability(int capability) {
		checkValidCapability(capability);
		mNetworkCapabilities &= ~(1L << capability);
		return this;
	}

	public NetworkCapabilities addTransportType(int transportType) {
		checkValidTransportType(transportType);
		mTransportTypes |= 1L << transportType;
		return this;
	}

	public NetworkCapabilities removeTransportType(int transportType) {
		checkValidTransportType(transportType);
		mTransportTypes &= ~(1L << transportType);
		return this;
	}

	private static boolean isValidCapability(int capability) {
		return capability >= MIN_NET_CAPABILITY && capability <= MAX_NET_CAPABILITY;
	}

	private static void checkValidCapability(int capability) {
		if (!isValidCapability(capability))
			throw new IllegalArgumentException("NetworkCapability " + capability + " out of range");
	}

	private static boolean isValidTransport(int transportType) {
		return transportType >= MIN_TRANSPORT && transportType <= MAX_TRANSPORT;
	}

	private static void checkValidTransportType(int transportType) {
		if (!isValidTransport(transportType))
			throw new IllegalArgumentException("Invalid TransportType " + transportType);
	}

	// GEN member:android.net.NetworkCapabilities:public int getLinkDownstreamBandwidthKbps();
	public int getLinkDownstreamBandwidthKbps() { return 0; }
}
