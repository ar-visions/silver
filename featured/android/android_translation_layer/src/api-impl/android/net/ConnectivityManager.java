package android.net;

import android.os.Handler;
import android.os.Looper;

import java.util.ArrayList;
import java.util.List;

class ProxyInfo {}

public class ConnectivityManager {

	public static class NetworkCallback {
		public static final int FLAG_INCLUDE_LOCATION_INFO = 1;

		public NetworkCallback() {}

		public NetworkCallback(int flags) {}

		public void onPreCheck(Network network) {}

		public void onAvailable(Network network) {}

		public void onLosing(Network network, int maxMsToLive) {}

		public void onLost(Network network) {}

		public void onUnavailable() {}

		public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {}

		public void onLinkPropertiesChanged(Network network, LinkProperties linkProperties) {}

		public void onBlockedStatusChanged(Network network, boolean blocked) {}
	}

	// a registered callback, the handler its calls go on, the state it last heard
	private static class Watch {
		NetworkCallback cb;
		Handler handler;
		boolean up;
	}

	private static final List<Watch> watches = new ArrayList<>();
	private static boolean watching;
	// one network for the process: callbacks and getActiveNetwork hand out the same object
	private static final Network network = new Network();

	public NetworkInfo getNetworkInfo(int networkType) {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	public NetworkInfo getActiveNetworkInfo() {
		return new NetworkInfo(nativeGetNetworkAvailable());
	}

	public native boolean isActiveNetworkMetered();

	protected static native boolean nativeGetNetworkAvailable();

	// GLib's network monitor reports a change through networkChanged
	private static native void nativeWatch(Class<?> self);

	public NetworkInfo[] getAllNetworkInfo() {
		return new NetworkInfo[] {getActiveNetworkInfo()};
	}

	// null when there is no network, as Android answers
	public Network getActiveNetwork() {
		return nativeGetNetworkAvailable() ? network : null;
	}

	public Network[] getAllNetworks() {
		Network n = getActiveNetwork();
		return n == null ? new Network[0] : new Network[] {n};
	}

	public NetworkCapabilities getNetworkCapabilities(Network network) {
		// as in getActiveNetworkInfo, a null network means there is none to report on
		if (network == null || !nativeGetNetworkAvailable())
			return null;
		return capabilitiesNow();
	}

	private NetworkCapabilities capabilitiesNow() {
		NetworkCapabilities capabilities = new NetworkCapabilities();
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_SUSPENDED);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING);
		capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_TRUSTED);

		if (!isActiveNetworkMetered())
			capabilities.addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);

		capabilities.addTransportType(NetworkCapabilities.TRANSPORT_WIFI);

		return capabilities;
	}

	// Android's order on a network coming up: available, capabilities, link, blocked
	private void deliver(final Watch w, final boolean up) {
		w.up = up;
		final NetworkCallback cb = w.cb;
		final NetworkCapabilities caps = up ? capabilitiesNow() : null;
		w.handler.post(new Runnable() {
			public void run() {
				if (up) {
					cb.onAvailable(network);
					cb.onCapabilitiesChanged(network, caps);
					cb.onLinkPropertiesChanged(network, new LinkProperties());
					cb.onBlockedStatusChanged(network, false);
				} else {
					cb.onLost(network);
				}
			}
		});
	}

	private void watch(final NetworkCallback cb, Handler handler, int timeoutMs) {
		if (cb == null)
			return;
		Watch w = new Watch();
		w.cb = cb;
		w.handler = handler != null ? handler : new Handler(Looper.getMainLooper());
		synchronized (watches) {
			watches.add(w);
			if (!watching) {
				watching = true;
				nativeWatch(ConnectivityManager.class);
			}
		}
		if (nativeGetNetworkAvailable()) {
			deliver(w, true);
		} else if (timeoutMs > 0) {
			// requestNetwork with a timeout and no network: Android says so once
			w.handler.postDelayed(new Runnable() {
				public void run() {
					cb.onUnavailable();
				}
			}, timeoutMs);
		}
	}

	// called by the native change signal, on the main thread
	private static void networkChanged(boolean up) {
		ConnectivityManager cm = new ConnectivityManager();
		Watch[] all;
		synchronized (watches) {
			all = watches.toArray(new Watch[0]);
		}
		for (Watch w : all)
			if (w.up != up)
				cm.deliver(w, up);
	}

	public void registerNetworkCallback(NetworkRequest request, NetworkCallback callback) {
		watch(callback, null, 0);
	}

	public void registerNetworkCallback(NetworkRequest request, NetworkCallback callback, Handler handler) {
		watch(callback, handler, 0);
	}

	public void registerBestMatchingNetworkCallback(NetworkRequest request, NetworkCallback callback, Handler handler) {
		watch(callback, handler, 0);
	}

	public void registerDefaultNetworkCallback(NetworkCallback cb) {
		watch(cb, null, 0);
	}

	public void registerDefaultNetworkCallback(NetworkCallback cb, Handler hdl) {
		watch(cb, hdl, 0);
	}

	public void requestNetwork(NetworkRequest request, NetworkCallback callback) {
		watch(callback, null, 0);
	}

	public void requestNetwork(NetworkRequest request, NetworkCallback callback, Handler handler) {
		watch(callback, handler, 0);
	}

	public void requestNetwork(NetworkRequest request, NetworkCallback callback, int timeoutMs) {
		watch(callback, null, timeoutMs);
	}

	public void requestNetwork(NetworkRequest request, NetworkCallback callback, Handler handler, int timeoutMs) {
		watch(callback, handler, timeoutMs);
	}

	public void unregisterNetworkCallback(NetworkCallback callback) {
		synchronized (watches) {
			for (int i = watches.size() - 1; i >= 0; i--)
				if (watches.get(i).cb == callback)
					watches.remove(i);
		}
	}

	public ProxyInfo getDefaultProxy() { return null; }

	public LinkProperties getLinkProperties(Network network) {
		return network == null ? null : new LinkProperties();
	}

	// RESTRICT_BACKGROUND_STATUS_DISABLED: no data saver here
	public int getRestrictBackgroundStatus() {
		return 1;
	}

	public NetworkInfo getNetworkInfo(Network network) {
		return getActiveNetworkInfo();
	}

	// GEN member:android.net.ConnectivityManager:public void reportNetworkConnectivity(android.net.Network, boolean);
	public void reportNetworkConnectivity(android.net.Network arg0, boolean arg1) {}

	// GEN member:android.net.ConnectivityManager:public boolean getBackgroundDataSetting();
	public boolean getBackgroundDataSetting() { return false; }
}
