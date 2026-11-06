package android.companion.virtual;

// android.jar's shape, made by gen.py
public final class VirtualDeviceManager {
	// GEN member:android.companion.virtual.VirtualDeviceManager:public java.util.List<android.companion.virtual.VirtualDevice> getVirtualDevices();
	public java.util.List<android.companion.virtual.VirtualDevice> getVirtualDevices() { return new java.util.ArrayList<>(); }

	// GEN member:android.companion.virtual.VirtualDeviceManager:public android.companion.virtual.VirtualDevice getVirtualDevice(int);
	public android.companion.virtual.VirtualDevice getVirtualDevice(int arg0) { return null; }

	protected VirtualDeviceManager() {}

	// GEN class:android.companion.virtual.VirtualDeviceManager$VirtualDeviceListener
	public interface VirtualDeviceListener {
		// GEN member:android.companion.virtual.VirtualDeviceManager$VirtualDeviceListener:public default void onVirtualDeviceCreated(int);
		default void onVirtualDeviceCreated(int arg0) {}
	
		// GEN member:android.companion.virtual.VirtualDeviceManager$VirtualDeviceListener:public default void onVirtualDeviceClosed(int);
		default void onVirtualDeviceClosed(int arg0) {}
	}

}
