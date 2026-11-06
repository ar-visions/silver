package android.media;

import android.os.Handler;

public class AudioManager {
	public static final String PROPERTY_OUTPUT_FRAMES_PER_BUFFER = "android.media.property.OUTPUT_FRAMES_PER_BUFFER";
	public static final String PROPERTY_OUTPUT_SAMPLE_RATE = "android.media.property.OUTPUT_SAMPLE_RATE";

	public static final int STREAM_MUSIC = 0x3;

	private native void nativeSetStreamVolume(int volume);

	public boolean isBluetoothA2dpOn() {
		return false;
	}

	public String getProperty(String name) {
		switch (name) {
			case PROPERTY_OUTPUT_FRAMES_PER_BUFFER:
				return "256"; // FIXME arbitrary
			case PROPERTY_OUTPUT_SAMPLE_RATE:
				return "44100"; // FIXME arbitrary
			default:
				System.out.println("AudioManager.getProperty: >" + name + "< not handled");
				return "";
		}
	}

	public interface OnAudioFocusChangeListener {
	}

	public static final int AUDIOFOCUS_NONE = 0;
	public static final int AUDIOFOCUS_GAIN = 1;
	public static final int AUDIOFOCUS_GAIN_TRANSIENT = 2;
	public static final int AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK = 3;
	public static final int AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE = 4;

	public static final int AUDIOFOCUS_LOSS = -1 * AUDIOFOCUS_GAIN;
	public static final int AUDIOFOCUS_LOSS_TRANSIENT = -1 * AUDIOFOCUS_GAIN_TRANSIENT;
	public static final int AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK = -1 * AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;

	public static final int AUDIOFOCUS_FLAG_DELAY_OK = 1 << 0;
	public static final int AUDIOFOCUS_FLAG_PAUSES_ON_DUCKABLE_LOSS = 1 << 1;
	public static final int AUDIOFOCUS_FLAG_LOCK = 1 << 2;

	public static final int AUDIOFOCUS_REQUEST_FAILED = 0;
	public static final int AUDIOFOCUS_REQUEST_GRANTED = 1;
	public static final int AUDIOFOCUS_REQUEST_DELAYED = 2;

	public static final int GET_DEVICES_INPUTS = 0x0001;
	public static final int GET_DEVICES_OUTPUTS = 0x0002;
	public static final int GET_DEVICES_ALL = GET_DEVICES_OUTPUTS | GET_DEVICES_INPUTS;

	public int getRingerMode() {
		return 0;
	}

	public int getStreamVolume(int streamType) {
		return 0; // arbitrary, shouldn't matter too much?
	}

	public int getStreamMaxVolume(int streamType) {
		return 100;
	}

	public int requestAudioFocus(OnAudioFocusChangeListener listener, int streamType, int durationHint) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public int abandonAudioFocus(OnAudioFocusChangeListener listener) {
		return /*AUDIOFOCUS_REQUEST_GRANTED*/ 1;
	}

	public int requestAudioFocus(AudioFocusRequest focusRequest) {
		if (focusRequest == null) {
			throw new NullPointerException("Illegal null AudioFocusRequest");
		}
		return AUDIOFOCUS_REQUEST_GRANTED;
	}

	public int abandonAudioFocusRequest(AudioFocusRequest focusRequest) {
		if (focusRequest == null) {
			throw new IllegalArgumentException("Illegal null AudioFocusRequest");
		}
		return AUDIOFOCUS_REQUEST_GRANTED;
	}

	public boolean isWiredHeadsetOn() {
		return false;
	}

	public void setStreamVolume(int streamType, int index, int flags) {
		nativeSetStreamVolume(index);
	}

	public boolean isStreamMute(int streamType) {
		return false;
	}

	public boolean isMusicActive() {
		return false;
	}

	public void setSpeakerphoneOn(boolean on) {}

	public boolean isSpeakerphoneOn() {
		return false;
	}

	public void setBluetoothScoOn(boolean on) {}

	public boolean isBluetoothScoOn() {
		return false;
	}

	public void stopBluetoothSco() {}

	public void setMode(int mode) {}

	public int getMode() {
		return /*MODE_NORMAL*/ 0;
	}

	public boolean isMicrophoneMute() {
		return false;
	}

	public void setMicrophoneMute(boolean on) {
		System.out.println("AudioManager.setMicrophoneMute(" + on + ")");
	}
	public void unloadSoundEffects() {}

	public int generateAudioSessionId() {
		return 0;
	}

	public AudioDeviceInfo[] getDevices(int mode) {
		return new AudioDeviceInfo[0];
	}

	public void registerAudioDeviceCallback(AudioDeviceCallback callback, Handler handler) {}

	public void unregisterAudioDeviceCallback(AudioDeviceCallback callback) {}

	public void playSoundEffect(int effectType) {}

	public boolean isVolumeFixed() {
		return false;
	}

	public boolean isBluetoothScoAvailableOffCall() {
		return false;
	}

	public void startBluetoothSco() {}

	public AudioDeviceInfo getCommunicationDevice() {
		return null;
	}

	// GEN class:android.media.AudioManager$AudioPlaybackCallback
	public abstract class AudioPlaybackCallback {
		// GEN member:android.media.AudioManager$AudioPlaybackCallback:public android.media.AudioManager$AudioPlaybackCallback();
		public AudioPlaybackCallback() {}
	
		// GEN member:android.media.AudioManager$AudioPlaybackCallback:public void onPlaybackConfigChanged(java.util.List<android.media.AudioPlaybackConfiguration>);
		public void onPlaybackConfigChanged(java.util.List<android.media.AudioPlaybackConfiguration> arg0) {}
	}


	// GEN class:android.media.AudioManager$AudioRecordingCallback
	public abstract class AudioRecordingCallback {
		// GEN member:android.media.AudioManager$AudioRecordingCallback:public android.media.AudioManager$AudioRecordingCallback();
		public AudioRecordingCallback() {}
	
		// GEN member:android.media.AudioManager$AudioRecordingCallback:public void onRecordingConfigChanged(java.util.List<android.media.AudioRecordingConfiguration>);
		public void onRecordingConfigChanged(java.util.List<android.media.AudioRecordingConfiguration> arg0) {}
	}


	// GEN class:android.media.AudioManager$OnCommunicationDeviceChangedListener
	public interface OnCommunicationDeviceChangedListener {
		// GEN member:android.media.AudioManager$OnCommunicationDeviceChangedListener:public abstract void onCommunicationDeviceChanged(android.media.AudioDeviceInfo);
		default void onCommunicationDeviceChanged(android.media.AudioDeviceInfo arg0) {}
	}


	// GEN class:android.media.AudioManager$OnModeChangedListener
	public interface OnModeChangedListener {
		// GEN member:android.media.AudioManager$OnModeChangedListener:public abstract void onModeChanged(int);
		default void onModeChanged(int arg0) {}
	}


	// GEN member:android.media.AudioManager:public void addOnCommunicationDeviceChangedListener(java.util.concurrent.Executor, android.media.AudioManager$OnCommunicationDeviceChangedListener);
	public void addOnCommunicationDeviceChangedListener(java.util.concurrent.Executor arg0, android.media.AudioManager.OnCommunicationDeviceChangedListener arg1) {}

	// GEN member:android.media.AudioManager:public void addOnModeChangedListener(java.util.concurrent.Executor, android.media.AudioManager$OnModeChangedListener);
	public void addOnModeChangedListener(java.util.concurrent.Executor arg0, android.media.AudioManager.OnModeChangedListener arg1) {}

	// GEN member:android.media.AudioManager:public void clearCommunicationDevice();
	public void clearCommunicationDevice() {}

	// GEN member:android.media.AudioManager:public java.util.List<android.media.AudioDeviceInfo> getAudioDevicesForAttributes(android.media.AudioAttributes);
	public java.util.List<android.media.AudioDeviceInfo> getAudioDevicesForAttributes(android.media.AudioAttributes arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.media.AudioManager:public java.util.List<android.media.AudioDeviceInfo> getAvailableCommunicationDevices();
	public java.util.List<android.media.AudioDeviceInfo> getAvailableCommunicationDevices() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.AudioManager:public static int getDirectPlaybackSupport(android.media.AudioFormat, android.media.AudioAttributes);
	public static int getDirectPlaybackSupport(android.media.AudioFormat arg0, android.media.AudioAttributes arg1) { return 0; }

	// GEN member:android.media.AudioManager:public java.util.List<android.media.AudioProfile> getDirectProfilesForAttributes(android.media.AudioAttributes);
	public java.util.List<android.media.AudioProfile> getDirectProfilesForAttributes(android.media.AudioAttributes arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.media.AudioManager:public java.lang.String getParameters(java.lang.String);
	public java.lang.String getParameters(java.lang.String arg0) { return null; }

	// GEN member:android.media.AudioManager:public static int getPlaybackOffloadSupport(android.media.AudioFormat, android.media.AudioAttributes);
	public static int getPlaybackOffloadSupport(android.media.AudioFormat arg0, android.media.AudioAttributes arg1) { return 0; }

	// GEN member:android.media.AudioManager:public android.media.Spatializer getSpatializer();
	public android.media.Spatializer getSpatializer() { return null; }

	// GEN member:android.media.AudioManager:public int getStreamMinVolume(int);
	public int getStreamMinVolume(int arg0) { return 0; }

	// GEN member:android.media.AudioManager:public static boolean isOffloadedPlaybackSupported(android.media.AudioFormat, android.media.AudioAttributes);
	public static boolean isOffloadedPlaybackSupported(android.media.AudioFormat arg0, android.media.AudioAttributes arg1) { return false; }

	// GEN member:android.media.AudioManager:public void registerAudioPlaybackCallback(android.media.AudioManager$AudioPlaybackCallback, android.os.Handler);
	public void registerAudioPlaybackCallback(android.media.AudioManager.AudioPlaybackCallback arg0, android.os.Handler arg1) {}

	// GEN member:android.media.AudioManager:public void removeOnCommunicationDeviceChangedListener(android.media.AudioManager$OnCommunicationDeviceChangedListener);
	public void removeOnCommunicationDeviceChangedListener(android.media.AudioManager.OnCommunicationDeviceChangedListener arg0) {}

	// GEN member:android.media.AudioManager:public void removeOnModeChangedListener(android.media.AudioManager$OnModeChangedListener);
	public void removeOnModeChangedListener(android.media.AudioManager.OnModeChangedListener arg0) {}

	// GEN member:android.media.AudioManager:public void setAllowedCapturePolicy(int);
	public void setAllowedCapturePolicy(int arg0) {}

	// GEN member:android.media.AudioManager:public boolean setCommunicationDevice(android.media.AudioDeviceInfo);
	public boolean setCommunicationDevice(android.media.AudioDeviceInfo arg0) { return false; }

	// GEN member:android.media.AudioManager:public void unregisterAudioPlaybackCallback(android.media.AudioManager$AudioPlaybackCallback);
	public void unregisterAudioPlaybackCallback(android.media.AudioManager.AudioPlaybackCallback arg0) {}
}
