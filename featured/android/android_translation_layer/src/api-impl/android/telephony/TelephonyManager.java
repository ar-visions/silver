package android.telephony;

import java.util.ArrayList;
import java.util.List;

public class TelephonyManager {
	public static TelephonyManager getDefault() {
		return new TelephonyManager();
	}

	// FIXME: can we return null instead of ""?
	public String getNetworkOperator() {
		return "";
	}

	public String getNetworkOperatorName() {
		return "";
	}

	public String getSimOperator() {
		return "";
	}

	public String getSubscriberId() {
		return "";
	}

	public int getPhoneType() {
		return 0; // PHONE_TYPE_NONE
	}

	public String getNetworkCountryIso() { return ""; }
	public String getSimCountryIso() { return ""; }

	public List getNeighboringCellInfo() {
		return new ArrayList(0);
	}

	public CellLocation getCellLocation() {
		return new CellLocation();
	}

	public boolean isNetworkRoaming() {
		return false;
	}

	public void listen(PhoneStateListener listener, int events) {}

	public int getNetworkType() {
		return 0; // NETWORK_TYPE_UNKNOWN
	}

	public String getLine1Number() {
		return null;
	}

	public int getSimState() {
		return 0; // SIM_STATE_UNKNOWN
	}

	public String getSimOperatorName() {
		return null;
	}

	public int getCallState() {
		return 0; // CALL_STATE_IDLE
	}

	public String getDeviceId() {
		return "";
	}

	// NETWORK_TYPE_UNKNOWN, DATA_DISCONNECTED: no modem here
	public int getDataNetworkType() {
		return 0;
	}

	public int getDataState() {
		return 0;
	}

	// GEN member:android.telephony.TelephonyManager:public android.telephony.TelephonyManager createForSubscriptionId(int);
	public android.telephony.TelephonyManager createForSubscriptionId(int arg0) { return null; }

	// GEN member:android.telephony.TelephonyManager:public int getSimCarrierId();
	public int getSimCarrierId() { return 0; }

	// GEN member:android.telephony.TelephonyManager:public boolean isDataEnabled();
	public boolean isDataEnabled() { return false; }

	// GEN member:android.telephony.TelephonyManager:public boolean isDataRoamingEnabled();
	public boolean isDataRoamingEnabled() { return false; }

	// GEN member:android.telephony.TelephonyManager:public void registerTelephonyCallback(java.util.concurrent.Executor, android.telephony.TelephonyCallback);
	public void registerTelephonyCallback(java.util.concurrent.Executor arg0, android.telephony.TelephonyCallback arg1) {}

	// GEN member:android.telephony.TelephonyManager:public void unregisterTelephonyCallback(android.telephony.TelephonyCallback);
	public void unregisterTelephonyCallback(android.telephony.TelephonyCallback arg0) {}
}
