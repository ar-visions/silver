package android.bluetooth;

import java.util.List;

public interface BluetoothProfile {
	public static final int A2DP = 2;
	public static final int CSIP_SET_COORDINATOR = 25;
	public static final String EXTRA_PREVIOUS_STATE = "android.bluetooth.profile.extra.PREVIOUS_STATE";
	public static final String EXTRA_STATE = "android.bluetooth.profile.extra.STATE";
	public static final int GATT = 7;
	public static final int GATT_SERVER = 8;
	public static final int HAP_CLIENT = 28;
	public static final int HEADSET = 1;
	public static final int HEALTH = 3;
	public static final int HEARING_AID = 21;
	public static final int HID_DEVICE = 19;
	public static final int LE_AUDIO = 22;
	public static final int SAP = 10;
	public static final int STATE_CONNECTED = 2;
	public static final int STATE_CONNECTING = 1;
	public static final int STATE_DISCONNECTED = 0;
	public static final int STATE_DISCONNECTING = 3;

	public List<BluetoothDevice> getConnectedDevices();
	public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states);
	public int getConnectionState(BluetoothDevice device);

	public interface ServiceListener {
		public void onServiceConnected(int profile, BluetoothProfile proxy);
		public void onServiceDisconnected(int profile);
	}
}
