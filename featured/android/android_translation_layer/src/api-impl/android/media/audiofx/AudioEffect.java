package android.media.audiofx;

public class AudioEffect {
	public static final int SUCCESS = 0;

	public static class Descriptor {
	// GEN member:android.media.audiofx.AudioEffect$Descriptor:public java.util.UUID type;
	public java.util.UUID type;

	// GEN member:android.media.audiofx.AudioEffect$Descriptor:public java.util.UUID uuid;
	public java.util.UUID uuid;
}

	public static Descriptor[] queryEffects() {
		return new Descriptor[0];
	}

	public boolean getEnabled() { return false; }

	public int setEnabled(boolean enable) { return SUCCESS; }

	// GEN member:android.media.audiofx.AudioEffect:public static final java.util.UUID EFFECT_TYPE_AEC;
	public static final java.util.UUID EFFECT_TYPE_AEC = null;

	// GEN member:android.media.audiofx.AudioEffect:public static final java.util.UUID EFFECT_TYPE_NS;
	public static final java.util.UUID EFFECT_TYPE_NS = null;

	// GEN member:android.media.audiofx.AudioEffect:public void release();
	public void release() {}

	// GEN member:android.media.audiofx.AudioEffect:public static final java.util.UUID EFFECT_TYPE_LOUDNESS_ENHANCER;
	public static final java.util.UUID EFFECT_TYPE_LOUDNESS_ENHANCER = null;
}
