package android.media;

public class AudioRecord implements android.media.AudioRecordingMonitor, android.media.AudioRouting, android.media.MicrophoneDirection {

	public static final int RECORDSTATE_STOPPED = 1;
	public static final int RECORDSTATE_RECORDING = 3;

	public static final int ERROR_BAD_VALUE = -2;

	private long pcm_handle;
	private int channels; // set by native constructor
	private int recordingState = RECORDSTATE_STOPPED;
	private int sampleRateInHz;

	private native long native_constructor(int streamType, int sampleRateInHz, int num_channels, int audioFormat, int bufferSizeInBytes);
	private native void native_record(long pcm_handle);
	private native void native_stop(long pcm_handle);
	private native int native_read(long pcm_handle, short[] audioData, int offsetInShorts, int framesToWrite);
	private native void native_release(long pcm_handle);

	public AudioRecord(int streamType, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes) {
		this.sampleRateInHz = sampleRateInHz;
		pcm_handle = native_constructor(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes);
	}

	public static native int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat);

	public int getSampleRate() {
		return sampleRateInHz;
	}

	public int getState() {
		return /*STATE_INITIALIZED*/ 1;
	}

	public int getRecordingState() {
		return recordingState;
	}

	public void startRecording() {
		native_record(pcm_handle);
		recordingState = RECORDSTATE_RECORDING;
	}

	public int read(short[] audioData, int offsetInShorts, int sizeInShorts) {
		/* sanity check the parameters before calling native_write */
		if ((audioData == null)
		    || (offsetInShorts < 0) || (sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts < 0)
		    || (offsetInShorts + sizeInShorts > audioData.length)) {
			return ERROR_BAD_VALUE;
		}

		return native_read(pcm_handle, audioData, offsetInShorts, sizeInShorts / channels) * channels;
	}

	public void stop() {
		native_stop(pcm_handle);
		recordingState = RECORDSTATE_STOPPED;
	}

	public void release() {
		native_release(pcm_handle);
		pcm_handle = 0;
	}

	public static class Builder {

		private int audioSource;
		private AudioFormat audioFormat;

		public Builder setAudioSource(int audioSource) {
			this.audioSource = audioSource;
			return this;
		}

		public Builder setAudioFormat(AudioFormat audioFormat) {
			this.audioFormat = audioFormat;
			return this;
		}

		public AudioRecord build() {
			return new AudioRecord(audioSource, audioFormat.sampleRate, audioFormat.channelMask, audioFormat.encoding, 32768);
		}
	
	// GEN member:android.media.AudioRecord$Builder:public android.media.AudioRecord$Builder setBufferSizeInBytes(int) throws java.lang.IllegalArgumentException;
	public android.media.AudioRecord.Builder setBufferSizeInBytes(int arg0) throws java.lang.IllegalArgumentException { return this; }

	// GEN member:android.media.AudioRecord$Builder:public android.media.AudioRecord$Builder setContext(android.content.Context);
	public android.media.AudioRecord.Builder setContext(android.content.Context arg0) { return this; }

	// GEN member:android.media.AudioRecord$Builder:public android.media.AudioRecord$Builder setPrivacySensitive(boolean);
	public android.media.AudioRecord.Builder setPrivacySensitive(boolean arg0) { return this; }
}

	public int getAudioSessionId() {
		return 0;
	}

	// GEN member:android.media.AudioRecord:public android.media.AudioRecordingConfiguration getActiveRecordingConfiguration();
	public android.media.AudioRecordingConfiguration getActiveRecordingConfiguration() { return null; }

	// GEN member:android.media.AudioRecord:public android.media.AudioFormat getFormat();
	public android.media.AudioFormat getFormat() { return null; }

	// GEN member:android.media.AudioRecord:public int getTimestamp(android.media.AudioTimestamp, int);
	public int getTimestamp(android.media.AudioTimestamp arg0, int arg1) { return 0; }

	// GEN member:android.media.AudioRecord:public int read(byte[], int, int);
	public int read(byte[] arg0, int arg1, int arg2) { return 0; }

	// GEN member:android.media.AudioRecord:public int read(java.nio.ByteBuffer, int);
	public int read(java.nio.ByteBuffer arg0, int arg1) { return 0; }

	// GEN member:android.media.AudioRecord:public void registerAudioRecordingCallback(java.util.concurrent.Executor, android.media.AudioManager$AudioRecordingCallback);
	public void registerAudioRecordingCallback(java.util.concurrent.Executor arg0, android.media.AudioManager.AudioRecordingCallback arg1) {}

	// GEN member:android.media.AudioRecord:public boolean setPreferredDevice(android.media.AudioDeviceInfo);
	public boolean setPreferredDevice(android.media.AudioDeviceInfo arg0) { return false; }

	// GEN member:android.media.AudioRecord:public void unregisterAudioRecordingCallback(android.media.AudioManager$AudioRecordingCallback);
	public void unregisterAudioRecordingCallback(android.media.AudioManager.AudioRecordingCallback arg0) {}
}
