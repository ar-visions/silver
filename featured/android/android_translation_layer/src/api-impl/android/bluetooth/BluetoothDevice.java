package android.bluetooth;

// android.jar's shape, made by gen.py
public final class BluetoothDevice implements android.os.Parcelable {
	// GEN member:android.bluetooth.BluetoothDevice:public static final int ADDRESS_TYPE_ANONYMOUS = 255;
	public static final int ADDRESS_TYPE_ANONYMOUS = 255;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int ADDRESS_TYPE_PUBLIC = 0;
	public static final int ADDRESS_TYPE_PUBLIC = 0;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int ADDRESS_TYPE_RANDOM = 1;
	public static final int ADDRESS_TYPE_RANDOM = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int ADDRESS_TYPE_UNKNOWN = 65535;
	public static final int ADDRESS_TYPE_UNKNOWN = 65535;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int BOND_BONDED = 12;
	public static final int BOND_BONDED = 12;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int BOND_BONDING = 11;
	public static final int BOND_BONDING = 11;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int BOND_NONE = 10;
	public static final int BOND_NONE = 10;

	// GEN member:android.bluetooth.BluetoothDevice:public static final android.os.Parcelable$Creator<android.bluetooth.BluetoothDevice> CREATOR;
	public static final android.os.Parcelable.Creator<android.bluetooth.BluetoothDevice> CREATOR = null;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int DEVICE_TYPE_CLASSIC = 1;
	public static final int DEVICE_TYPE_CLASSIC = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int DEVICE_TYPE_DUAL = 3;
	public static final int DEVICE_TYPE_DUAL = 3;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int DEVICE_TYPE_LE = 2;
	public static final int DEVICE_TYPE_LE = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int DEVICE_TYPE_UNKNOWN = 0;
	public static final int DEVICE_TYPE_UNKNOWN = 0;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int ERROR = -2147483648;
	public static final int ERROR = -2147483648;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PAIRING_VARIANT_PASSKEY_CONFIRMATION = 2;
	public static final int PAIRING_VARIANT_PASSKEY_CONFIRMATION = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PAIRING_VARIANT_PIN = 0;
	public static final int PAIRING_VARIANT_PIN = 0;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_1M = 1;
	public static final int PHY_LE_1M = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_1M_MASK = 1;
	public static final int PHY_LE_1M_MASK = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_2M = 2;
	public static final int PHY_LE_2M = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_2M_MASK = 2;
	public static final int PHY_LE_2M_MASK = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_CODED = 3;
	public static final int PHY_LE_CODED = 3;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_LE_CODED_MASK = 4;
	public static final int PHY_LE_CODED_MASK = 4;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_OPTION_NO_PREFERRED = 0;
	public static final int PHY_OPTION_NO_PREFERRED = 0;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_OPTION_S2 = 1;
	public static final int PHY_OPTION_S2 = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int PHY_OPTION_S8 = 2;
	public static final int PHY_OPTION_S8 = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int TRANSPORT_AUTO = 0;
	public static final int TRANSPORT_AUTO = 0;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int TRANSPORT_BREDR = 1;
	public static final int TRANSPORT_BREDR = 1;

	// GEN member:android.bluetooth.BluetoothDevice:public static final int TRANSPORT_LE = 2;
	public static final int TRANSPORT_LE = 2;

	// GEN member:android.bluetooth.BluetoothDevice:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothDevice:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.bluetooth.BluetoothDevice:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.bluetooth.BluetoothDevice:public java.lang.String getAddress();
	public java.lang.String getAddress() { return null; }

	// GEN member:android.bluetooth.BluetoothDevice:public int getAddressType();
	public int getAddressType() { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public java.lang.String getName();
	public java.lang.String getName() { return null; }

	// GEN member:android.bluetooth.BluetoothDevice:public int getType();
	public int getType() { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public java.lang.String getAlias();
	public java.lang.String getAlias() { return null; }

	// GEN member:android.bluetooth.BluetoothDevice:public int setAlias(java.lang.String);
	public int setAlias(java.lang.String arg0) { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public boolean createBond();
	public boolean createBond() { return false; }

	// GEN member:android.bluetooth.BluetoothDevice:public int getBondState();
	public int getBondState() { return 0; }

	// GEN member:android.bluetooth.BluetoothDevice:public boolean fetchUuidsWithSdp();
	public boolean fetchUuidsWithSdp() { return false; }

	// GEN member:android.bluetooth.BluetoothDevice:public boolean setPin(byte[]);
	public boolean setPin(byte[] arg0) { return false; }

	// GEN member:android.bluetooth.BluetoothDevice:public boolean setPairingConfirmation(boolean);
	public boolean setPairingConfirmation(boolean arg0) { return false; }

	protected BluetoothDevice() {}
}
