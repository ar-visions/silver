package android.media;

// android.jar's shape, made by gen.py
public interface AudioRouting {
	// GEN member:android.media.AudioRouting:public abstract boolean setPreferredDevice(android.media.AudioDeviceInfo);
	default boolean setPreferredDevice(android.media.AudioDeviceInfo arg0) { return false; }

	// GEN member:android.media.AudioRouting:public abstract android.media.AudioDeviceInfo getPreferredDevice();
	default android.media.AudioDeviceInfo getPreferredDevice() { return null; }

	// GEN member:android.media.AudioRouting:public abstract android.media.AudioDeviceInfo getRoutedDevice();
	default android.media.AudioDeviceInfo getRoutedDevice() { return null; }

	// GEN member:android.media.AudioRouting:public abstract void addOnRoutingChangedListener(android.media.AudioRouting$OnRoutingChangedListener, android.os.Handler);
	default void addOnRoutingChangedListener(android.media.AudioRouting.OnRoutingChangedListener arg0, android.os.Handler arg1) {}

	// GEN member:android.media.AudioRouting:public abstract void removeOnRoutingChangedListener(android.media.AudioRouting$OnRoutingChangedListener);
	default void removeOnRoutingChangedListener(android.media.AudioRouting.OnRoutingChangedListener arg0) {}

	// GEN class:android.media.AudioRouting$OnRoutingChangedListener
	public interface OnRoutingChangedListener {
		// GEN member:android.media.AudioRouting$OnRoutingChangedListener:public abstract void onRoutingChanged(android.media.AudioRouting);
		default void onRoutingChanged(android.media.AudioRouting arg0) {}
	}

}
