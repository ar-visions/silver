package android.media;

// android.jar's shape, made by gen.py
public class LoudnessCodecController implements java.lang.AutoCloseable {
	// GEN member:android.media.LoudnessCodecController:public static android.media.LoudnessCodecController create(int);
	public static android.media.LoudnessCodecController create(int arg0) { return null; }

	// GEN member:android.media.LoudnessCodecController:public boolean addMediaCodec(android.media.MediaCodec);
	public boolean addMediaCodec(android.media.MediaCodec arg0) { return false; }

	// GEN member:android.media.LoudnessCodecController:public void removeMediaCodec(android.media.MediaCodec);
	public void removeMediaCodec(android.media.MediaCodec arg0) {}

	// GEN member:android.media.LoudnessCodecController:public android.os.Bundle getLoudnessCodecParams(android.media.MediaCodec);
	public android.os.Bundle getLoudnessCodecParams(android.media.MediaCodec arg0) { return null; }

	// GEN member:android.media.LoudnessCodecController:public void close();
	public void close() {}

	protected LoudnessCodecController() {}

	// GEN class:android.media.LoudnessCodecController$OnLoudnessCodecUpdateListener
	public interface OnLoudnessCodecUpdateListener {
		// GEN member:android.media.LoudnessCodecController$OnLoudnessCodecUpdateListener:public default android.os.Bundle onLoudnessCodecUpdate(android.media.MediaCodec, android.os.Bundle);
		default android.os.Bundle onLoudnessCodecUpdate(android.media.MediaCodec arg0, android.os.Bundle arg1) { return null; }
	}

}
