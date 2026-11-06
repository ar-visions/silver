package android.bluetooth;

// android.jar's shape, made by gen.py
public final class BluetoothHeadset implements android.bluetooth.BluetoothProfile {
	// GEN member:android.bluetooth.BluetoothHeadset:public static final int AT_CMD_TYPE_ACTION = 4;
	public static final int AT_CMD_TYPE_ACTION = 4;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int AT_CMD_TYPE_BASIC = 3;
	public static final int AT_CMD_TYPE_BASIC = 3;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int AT_CMD_TYPE_READ = 0;
	public static final int AT_CMD_TYPE_READ = 0;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int AT_CMD_TYPE_SET = 2;
	public static final int AT_CMD_TYPE_SET = 2;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int AT_CMD_TYPE_TEST = 1;
	public static final int AT_CMD_TYPE_TEST = 1;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int STATE_AUDIO_CONNECTED = 12;
	public static final int STATE_AUDIO_CONNECTED = 12;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int STATE_AUDIO_CONNECTING = 11;
	public static final int STATE_AUDIO_CONNECTING = 11;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final int STATE_AUDIO_DISCONNECTED = 10;
	public static final int STATE_AUDIO_DISCONNECTED = 10;

	// GEN member:android.bluetooth.BluetoothHeadset:public static final java.lang.String VENDOR_RESULT_CODE_COMMAND_ANDROID = "+ANDROID";
	public static final java.lang.String VENDOR_RESULT_CODE_COMMAND_ANDROID = "+ANDROID";

	// GEN member:android.bluetooth.BluetoothHeadset:public static final java.lang.String VENDOR_SPECIFIC_HEADSET_EVENT_COMPANY_ID_CATEGORY = "android.bluetooth.headset.intent.category.companyid";
	public static final java.lang.String VENDOR_SPECIFIC_HEADSET_EVENT_COMPANY_ID_CATEGORY = "android.bluetooth.headset.intent.category.companyid";

	// GEN member:android.bluetooth.BluetoothHeadset:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.bluetooth.BluetoothHeadset:public java.util.List<android.bluetooth.BluetoothDevice> getConnectedDevices();
	public java.util.List<android.bluetooth.BluetoothDevice> getConnectedDevices() { return new java.util.ArrayList<>(); }

	// GEN member:android.bluetooth.BluetoothHeadset:public java.util.List<android.bluetooth.BluetoothDevice> getDevicesMatchingConnectionStates(int[]);
	public java.util.List<android.bluetooth.BluetoothDevice> getDevicesMatchingConnectionStates(int[] arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.bluetooth.BluetoothHeadset:public int getConnectionState(android.bluetooth.BluetoothDevice);
	public int getConnectionState(android.bluetooth.BluetoothDevice arg0) { return 0; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean isNoiseReductionSupported(android.bluetooth.BluetoothDevice);
	public boolean isNoiseReductionSupported(android.bluetooth.BluetoothDevice arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean isVoiceRecognitionSupported(android.bluetooth.BluetoothDevice);
	public boolean isVoiceRecognitionSupported(android.bluetooth.BluetoothDevice arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean startVoiceRecognition(android.bluetooth.BluetoothDevice);
	public boolean startVoiceRecognition(android.bluetooth.BluetoothDevice arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean stopVoiceRecognition(android.bluetooth.BluetoothDevice);
	public boolean stopVoiceRecognition(android.bluetooth.BluetoothDevice arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean isAudioConnected(android.bluetooth.BluetoothDevice);
	public boolean isAudioConnected(android.bluetooth.BluetoothDevice arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothHeadset:public boolean sendVendorSpecificResultCode(android.bluetooth.BluetoothDevice, java.lang.String, java.lang.String);
	public boolean sendVendorSpecificResultCode(android.bluetooth.BluetoothDevice arg0, java.lang.String arg1, java.lang.String arg2) { return false; }

	protected BluetoothHeadset() {}
}
