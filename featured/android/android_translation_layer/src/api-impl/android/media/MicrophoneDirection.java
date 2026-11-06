package android.media;

// android.jar's shape, made by ifacegen.py
public interface MicrophoneDirection {
	public static final int MIC_DIRECTION_AWAY_FROM_USER = 2;
	public static final int MIC_DIRECTION_EXTERNAL = 3;
	public static final int MIC_DIRECTION_TOWARDS_USER = 1;
	public static final int MIC_DIRECTION_UNSPECIFIED = 0;
	default boolean setPreferredMicrophoneDirection(int arg0) { return false; }
	default boolean setPreferredMicrophoneFieldDimension(float arg0) { return false; }
}
