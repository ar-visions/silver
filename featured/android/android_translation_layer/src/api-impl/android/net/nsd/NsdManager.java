package android.net.nsd;

public class NsdManager {

	// GEN class:android.net.nsd.NsdManager$DiscoveryListener
	public interface DiscoveryListener {
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onStartDiscoveryFailed(java.lang.String, int);
		default void onStartDiscoveryFailed(java.lang.String arg0, int arg1) {}
	
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onStopDiscoveryFailed(java.lang.String, int);
		default void onStopDiscoveryFailed(java.lang.String arg0, int arg1) {}
	
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onDiscoveryStarted(java.lang.String);
		default void onDiscoveryStarted(java.lang.String arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onDiscoveryStopped(java.lang.String);
		default void onDiscoveryStopped(java.lang.String arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onServiceFound(android.net.nsd.NsdServiceInfo);
		default void onServiceFound(android.net.nsd.NsdServiceInfo arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$DiscoveryListener:public abstract void onServiceLost(android.net.nsd.NsdServiceInfo);
		default void onServiceLost(android.net.nsd.NsdServiceInfo arg0) {}
	}


	// GEN class:android.net.nsd.NsdManager$RegistrationListener
	public interface RegistrationListener {
		// GEN member:android.net.nsd.NsdManager$RegistrationListener:public abstract void onRegistrationFailed(android.net.nsd.NsdServiceInfo, int);
		default void onRegistrationFailed(android.net.nsd.NsdServiceInfo arg0, int arg1) {}
	
		// GEN member:android.net.nsd.NsdManager$RegistrationListener:public abstract void onUnregistrationFailed(android.net.nsd.NsdServiceInfo, int);
		default void onUnregistrationFailed(android.net.nsd.NsdServiceInfo arg0, int arg1) {}
	
		// GEN member:android.net.nsd.NsdManager$RegistrationListener:public abstract void onServiceRegistered(android.net.nsd.NsdServiceInfo);
		default void onServiceRegistered(android.net.nsd.NsdServiceInfo arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$RegistrationListener:public abstract void onServiceUnregistered(android.net.nsd.NsdServiceInfo);
		default void onServiceUnregistered(android.net.nsd.NsdServiceInfo arg0) {}
	}


	// GEN class:android.net.nsd.NsdManager$ResolveListener
	public interface ResolveListener {
		// GEN member:android.net.nsd.NsdManager$ResolveListener:public abstract void onResolveFailed(android.net.nsd.NsdServiceInfo, int);
		default void onResolveFailed(android.net.nsd.NsdServiceInfo arg0, int arg1) {}
	
		// GEN member:android.net.nsd.NsdManager$ResolveListener:public abstract void onServiceResolved(android.net.nsd.NsdServiceInfo);
		default void onServiceResolved(android.net.nsd.NsdServiceInfo arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$ResolveListener:public default void onResolutionStopped(android.net.nsd.NsdServiceInfo);
		default void onResolutionStopped(android.net.nsd.NsdServiceInfo arg0) {}
	
		// GEN member:android.net.nsd.NsdManager$ResolveListener:public default void onStopResolutionFailed(android.net.nsd.NsdServiceInfo, int);
		default void onStopResolutionFailed(android.net.nsd.NsdServiceInfo arg0, int arg1) {}
	}


	// GEN member:android.net.nsd.NsdManager:public void discoverServices(java.lang.String, int, android.net.nsd.NsdManager$DiscoveryListener);
	public void discoverServices(java.lang.String arg0, int arg1, android.net.nsd.NsdManager.DiscoveryListener arg2) {}

	// GEN member:android.net.nsd.NsdManager:public void registerService(android.net.nsd.NsdServiceInfo, int, android.net.nsd.NsdManager$RegistrationListener);
	public void registerService(android.net.nsd.NsdServiceInfo arg0, int arg1, android.net.nsd.NsdManager.RegistrationListener arg2) {}

	// GEN member:android.net.nsd.NsdManager:public void resolveService(android.net.nsd.NsdServiceInfo, android.net.nsd.NsdManager$ResolveListener);
	public void resolveService(android.net.nsd.NsdServiceInfo arg0, android.net.nsd.NsdManager.ResolveListener arg1) {}

	// GEN member:android.net.nsd.NsdManager:public void stopServiceDiscovery(android.net.nsd.NsdManager$DiscoveryListener);
	public void stopServiceDiscovery(android.net.nsd.NsdManager.DiscoveryListener arg0) {}

	// GEN member:android.net.nsd.NsdManager:public void unregisterService(android.net.nsd.NsdManager$RegistrationListener);
	public void unregisterService(android.net.nsd.NsdManager.RegistrationListener arg0) {}
}
