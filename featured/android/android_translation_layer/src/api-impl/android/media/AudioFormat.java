package android.media;

public class AudioFormat implements android.os.Parcelable {

	int sampleRate = 44100;
	int channelMask;
	int encoding;

	public static class Builder {

		private AudioFormat audioFormat = new AudioFormat();

		public Builder setSampleRate(int sampleRate) {
			audioFormat.sampleRate = sampleRate;
			return this;
		}

		public Builder setChannelMask(int channelMask) {
			audioFormat.channelMask = channelMask;
			return this;
		}

		public Builder setEncoding(int encoding) {
			audioFormat.encoding = encoding;
			return this;
		}

		public AudioFormat build() {
			return audioFormat;
		}
	}

	// GEN member:android.media.AudioFormat:public int getChannelCount();
	public int getChannelCount() { return 0; }

	// GEN member:android.media.AudioFormat:public int getSampleRate();
	public int getSampleRate() { return 0; }
}
