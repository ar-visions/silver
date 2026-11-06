package android.net;

public class NetworkInfo implements android.os.Parcelable {
	public enum State {
		CONNECTED,
		CONNECTING,
		DISCONNECTED,
		DISCONNECTING,
		SUSPENDED,
		UNKNOWN
	}

	public enum DetailedState {
		IDLE, SCANNING, CONNECTING, AUTHENTICATING, OBTAINING_IPADDR, CONNECTED, SUSPENDED,
		DISCONNECTING, DISCONNECTED, FAILED, BLOCKED, VERIFYING_POOR_LINK, CAPTIVE_PORTAL_CHECK
	}

	private State state = State.DISCONNECTED;

	public NetworkInfo(boolean available) {
		state = available ? State.CONNECTED : State.DISCONNECTED;
	}

	public NetworkInfo.State getState() {
		return state;
	}

	public int getType() {
		return state == State.CONNECTED ? /*TYPE_WIFI*/ 0x1 : 0x0;
	}

	public boolean isConnected() {
		return state == State.CONNECTED;
	}

	public boolean isConnectedOrConnecting() {
		return state == State.CONNECTED;
	}

	public boolean isFailover() {
		return false;
	}

	public int getSubtype() {
		return 0; // NETWORK_TYPE_UNKNOWN
	}

	public boolean isRoaming() {
		return false;
	}

	public String getTypeName() {
		return state == State.CONNECTED ? "WIFI" : "UNKNOWN";
	}

	public String getSubtypeName() {
		return "UNKNOWN";
	}

	public boolean isAvailable() {
		return state == State.CONNECTED;
	}

	public DetailedState getDetailedState() {
		return state == State.CONNECTED ? DetailedState.CONNECTED : DetailedState.DISCONNECTED;
	}

	public String getExtraInfo() {
		return null;
	}

	public String getReason() {
		return null;
	}
}
