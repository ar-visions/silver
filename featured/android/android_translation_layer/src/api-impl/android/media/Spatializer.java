package android.media;

// android.jar's shape, made by gen.py
public class Spatializer {
	// GEN member:android.media.Spatializer:public static final int SPATIALIZER_IMMERSIVE_LEVEL_MULTICHANNEL = 1;
	public static final int SPATIALIZER_IMMERSIVE_LEVEL_MULTICHANNEL = 1;

	// GEN member:android.media.Spatializer:public static final int SPATIALIZER_IMMERSIVE_LEVEL_NONE = 0;
	public static final int SPATIALIZER_IMMERSIVE_LEVEL_NONE = 0;

	// GEN member:android.media.Spatializer:public static final int SPATIALIZER_IMMERSIVE_LEVEL_OTHER = -1;
	public static final int SPATIALIZER_IMMERSIVE_LEVEL_OTHER = -1;

	// GEN member:android.media.Spatializer:public boolean isEnabled();
	public boolean isEnabled() { return false; }

	// GEN member:android.media.Spatializer:public boolean isAvailable();
	public boolean isAvailable() { return false; }

	// GEN member:android.media.Spatializer:public boolean isHeadTrackerAvailable();
	public boolean isHeadTrackerAvailable() { return false; }

	// GEN member:android.media.Spatializer:public int getImmersiveAudioLevel();
	public int getImmersiveAudioLevel() { return 0; }

	// GEN member:android.media.Spatializer:public boolean canBeSpatialized(android.media.AudioAttributes, android.media.AudioFormat);
	public boolean canBeSpatialized(android.media.AudioAttributes arg0, android.media.AudioFormat arg1) { return false; }

	protected Spatializer() {}

	// GEN class:android.media.Spatializer$OnSpatializerStateChangedListener
	public interface OnSpatializerStateChangedListener {
		// GEN member:android.media.Spatializer$OnSpatializerStateChangedListener:public abstract void onSpatializerEnabledChanged(android.media.Spatializer, boolean);
		default void onSpatializerEnabledChanged(android.media.Spatializer arg0, boolean arg1) {}
	
		// GEN member:android.media.Spatializer$OnSpatializerStateChangedListener:public abstract void onSpatializerAvailableChanged(android.media.Spatializer, boolean);
		default void onSpatializerAvailableChanged(android.media.Spatializer arg0, boolean arg1) {}
	}

}
