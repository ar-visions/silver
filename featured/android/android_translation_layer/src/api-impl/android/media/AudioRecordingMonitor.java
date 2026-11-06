package android.media;

// android.jar's shape, made by ifacegen.py
public interface AudioRecordingMonitor {
	default void registerAudioRecordingCallback(java.util.concurrent.Executor arg0, android.media.AudioManager.AudioRecordingCallback arg1) { }
	default void unregisterAudioRecordingCallback(android.media.AudioManager.AudioRecordingCallback arg0) { }
	default android.media.AudioRecordingConfiguration getActiveRecordingConfiguration() { return null; }
}
