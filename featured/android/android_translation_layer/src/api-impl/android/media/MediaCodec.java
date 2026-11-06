package android.media;

import android.media.MediaCodec.BufferInfo;
import android.os.Bundle;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayDeque;
import java.util.Queue;

public class MediaCodec {

	public static final int BUFFER_FLAG_END_OF_STREAM = 0x4;

	private String codecName;
	private ByteBuffer[] inputBuffers;
	private ByteBuffer[] outputBuffers;
	private long[] inputBufferTimestamps;
	private long native_codec;
	private boolean outputFormatSet = false;
	private MediaFormat mediaFormat;

	private Queue<Integer> freeOutputBuffers;
	private Queue<Integer> queuedInputBuffers;
	private Queue<Integer> freeInputBuffers;

	private MediaCodec(String codecName) throws IOException {
		this.codecName = codecName;
		native_codec = native_constructor(codecName);
		if (native_codec == 0) {
			throw new IOException("Unable to create MediaCodec: " + codecName);
		}
	}

	public static MediaCodec createByCodecName(String codecName) throws IOException {
		return new MediaCodec(codecName);
	}

	public void configure(MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
		System.out.println("MediaCodec.configure(" + format + ", " + surface + ", " + crypto + ", " + flags + "): codecName=" + codecName);
		this.mediaFormat = format;

		int maxInputSize = 262144;
		if (format.containsKey("max-input-size")) {
			maxInputSize = format.getInteger("max-input-size");
		}
		inputBuffers = new ByteBuffer[1];
		inputBufferTimestamps = new long[inputBuffers.length];
		freeInputBuffers = new ArrayDeque<>(inputBuffers.length);
		queuedInputBuffers = new ArrayDeque<>(inputBuffers.length);
		for (int i = 0; i < inputBuffers.length; i++) {
			inputBuffers[i] = ByteBuffer.allocate(maxInputSize).order(ByteOrder.LITTLE_ENDIAN);
			freeInputBuffers.add(i);
		}
		outputBuffers = new ByteBuffer[4];
		freeOutputBuffers = new ArrayDeque<>(outputBuffers.length);
		for (int i = 0; i < outputBuffers.length; i++) {
			outputBuffers[i] = ByteBuffer.allocate(16384).order(ByteOrder.LITTLE_ENDIAN);
			freeOutputBuffers.add(i);
		}

		if ("aac".equals(codecName) || "mp3".equals(codecName) || "opus".equals(codecName)) {
			native_configure_audio(native_codec, format.getByteBuffer("csd-0"), format.getInteger("sample-rate"), format.getInteger("channel-count"));
		} else if ("h264".equals(codecName) || "vp8".equals(codecName) || "vp9".equals(codecName)) {
			native_configure_video(native_codec, format.getByteBuffer("csd-0"), format.getByteBuffer("csd-1"), surface);
		} else {
			System.out.println("configure: format " + format + " not implemented");
			Thread.dumpStack();
			System.exit(1);
		}
	}

	public void start() {
		System.out.println("MediaCodec.start(): codecName=" + codecName);
		native_start(native_codec);
	}

	public ByteBuffer[] getInputBuffers() {
		return inputBuffers;
	}

	public ByteBuffer getInputBuffer(int index) {
		return inputBuffers[index];
	}

	public ByteBuffer[] getOutputBuffers() {
		return outputBuffers;
	}

	public ByteBuffer getOutputBuffer(int index) {
		return outputBuffers[index];
	}

	public int dequeueOutputBuffer(BufferInfo info, long timeoutUs) {
		if (!outputFormatSet) {
			outputFormatSet = true;
			return /*INFO_OUTPUT_FORMAT_CHANGED*/ -2;
		}
		Integer index = freeOutputBuffers.poll();
		if (index == null) {
			return /*INFO_TRY_AGAIN_LATER*/ -1;
		}
		outputBuffers[index].clear();
		int ret = native_dequeueOutputBuffer(native_codec, outputBuffers[index], info);
		if (ret == 0) {
			ret = index;
		} else {
			freeOutputBuffers.add(index);
			ret = /*INFO_TRY_AGAIN_LATER*/ -1;
		}
		return ret;
	}

	public void releaseOutputBuffer(int index, boolean render) {
		native_releaseOutputBuffer(native_codec, outputBuffers[index], render, 0);
		freeOutputBuffers.add(index);
	}

	// AOSP calls the parameter `presentationTimeUs`, but actually it is nanoseconds
	public void releaseOutputBuffer(int index, long releaseTimeNs) {
		native_releaseOutputBuffer(native_codec, outputBuffers[index], true, releaseTimeNs);
		freeOutputBuffers.add(index);
	}

	public MediaFormat getOutputFormat() {
		return mediaFormat;
	}

	public void flush() {
		native_flush(native_codec);
		freeInputBuffers.clear();
		for (int i = 0; i < inputBuffers.length; i++)
			freeInputBuffers.add(i);
		queuedInputBuffers.clear();
		freeOutputBuffers.clear();
		for (int i = 0; i < outputBuffers.length; i++)
			freeOutputBuffers.add(i);
		outputFormatSet = false;
	}

	private void tryProcessInputBuffer() {
		Integer index = queuedInputBuffers.peek();
		if (index != null) {
			int ret;
			if (index == -1) { // end of stream
				ret = native_queueInputBuffer(native_codec, null, 0);
			} else {
				ret = native_queueInputBuffer(native_codec, inputBuffers[index], inputBufferTimestamps[index]);
			}
			if (ret == 0) {
				queuedInputBuffers.remove(index);
				if (index != -1)
					freeInputBuffers.add(index);
			}
		}
	}

	public int dequeueInputBuffer(long timeoutUs) {
		tryProcessInputBuffer();
		Integer index = freeInputBuffers.poll();
		if (index == null) {
			return /*INFO_TRY_AGAIN_LATER*/ -1;
		} else {
			return index;
		}
	}

	public void queueInputBuffer(int index, int offset, int size, long presentationTimeUs, int flags) {
		if ((flags & BUFFER_FLAG_END_OF_STREAM) != 0) {
			queuedInputBuffers.add(-1);
			tryProcessInputBuffer();
			return;
		}
		inputBufferTimestamps[index] = presentationTimeUs;
		queuedInputBuffers.add(index);
		tryProcessInputBuffer();
	}

	public void setVideoScalingMode(int mode) {
		System.out.println("MediaCodec.setVideoScalingMode(" + mode + "): codecName=" + codecName);
	}

	public void stop() {}

	public void release() {
		System.out.println("MediaCodec.release(): codecName=" + codecName);
		if (native_codec != 0) {
			if (outputBuffers != null) {
				for (int i = 0; i < outputBuffers.length; i++) {
					if (!freeOutputBuffers.contains(i))
						releaseOutputBuffer(i, false);
				}
			}
			native_release(native_codec);
		}
		native_codec = 0;
	}

	public void setOutputSurface(Surface surface) {
	}

	public void setParameters(Bundle parameters) {}

	public static MediaCodec createEncoderByType(String type) throws IOException {
		System.out.println("MediaCodec.createEncoderByType(" + type + ")");
		throw new IOException("encoder not yet implemented");
	}

	@Override
	@SuppressWarnings("deprecation")
	protected void finalize() throws Throwable {
		try {
			super.finalize();
		} finally {
			release();
		}
	}

	private native long native_constructor(String codecName);
	private native void native_configure_audio(long codec, ByteBuffer extradata, int sampleRate, int channelCount);
	private native void native_configure_video(long codec, ByteBuffer csd0, ByteBuffer csd1, Surface surface);
	private native void native_start(long codec);
	private native int native_queueInputBuffer(long codec, ByteBuffer buffer, long presentationTimeUs);
	private native int native_dequeueOutputBuffer(long codec, ByteBuffer buffer, BufferInfo info);
	private native void native_releaseOutputBuffer(long codec, ByteBuffer buffer, boolean render, long releaseTimeNs);
	private native void native_flush(long codec);
	private native void native_release(long codec);

	public static final class CryptoInfo {
		public static final class Pattern {
			private int blocksToEncrypt;
			private int blocksToSkip;

			public Pattern(int blocksToEncrypt, int blocksToSkip) {
				this.blocksToEncrypt = blocksToEncrypt;
				this.blocksToSkip = blocksToSkip;
			}

			public void set(int blocksToEncrypt, int blocksToSkip) {
				this.blocksToEncrypt = blocksToEncrypt;
				this.blocksToSkip = blocksToSkip;
			}

			public int getEncryptBlocks() {
				return blocksToEncrypt;
			}

			public int getSkipBlocks() {
				return blocksToSkip;
			}
		}
	
	// GEN member:android.media.MediaCodec$CryptoInfo:public byte[] iv;
	public byte[] iv;

	// GEN member:android.media.MediaCodec$CryptoInfo:public byte[] key;
	public byte[] key;

	// GEN member:android.media.MediaCodec$CryptoInfo:public int mode;
	public int mode;

	// GEN member:android.media.MediaCodec$CryptoInfo:public int[] numBytesOfClearData;
	public int[] numBytesOfClearData;

	// GEN member:android.media.MediaCodec$CryptoInfo:public int[] numBytesOfEncryptedData;
	public int[] numBytesOfEncryptedData;

	// GEN member:android.media.MediaCodec$CryptoInfo:public int numSubSamples;
	public int numSubSamples;

	// GEN member:android.media.MediaCodec$CryptoInfo:public void setPattern(android.media.MediaCodec$CryptoInfo$Pattern);
	public void setPattern(android.media.MediaCodec.CryptoInfo.Pattern arg0) {}
}

	public static final class BufferInfo {
		public int size;
		public int flags;
		public int offset;
		public long presentationTimeUs;
	
	// GEN member:android.media.MediaCodec$BufferInfo:public void set(int, int, long, int);
	public void set(int arg0, int arg1, long arg2, int arg3) {}
}

	public static interface OnFrameRenderedListener {}

	// GEN class:android.media.MediaCodec$Callback
	public abstract class Callback {
		// GEN member:android.media.MediaCodec$Callback:public android.media.MediaCodec$Callback();
		public Callback() {}
	
		// GEN member:android.media.MediaCodec$Callback:public abstract void onInputBufferAvailable(android.media.MediaCodec, int);
		public abstract void onInputBufferAvailable(android.media.MediaCodec arg0, int arg1);
	
		// GEN member:android.media.MediaCodec$Callback:public abstract void onOutputBufferAvailable(android.media.MediaCodec, int, android.media.MediaCodec$BufferInfo);
		public abstract void onOutputBufferAvailable(android.media.MediaCodec arg0, int arg1, android.media.MediaCodec.BufferInfo arg2);
	
		// GEN member:android.media.MediaCodec$Callback:public void onOutputBuffersAvailable(android.media.MediaCodec, int, java.util.ArrayDeque<android.media.MediaCodec$BufferInfo>);
		public void onOutputBuffersAvailable(android.media.MediaCodec arg0, int arg1, java.util.ArrayDeque<android.media.MediaCodec.BufferInfo> arg2) {}
	
		// GEN member:android.media.MediaCodec$Callback:public abstract void onError(android.media.MediaCodec, android.media.MediaCodec$CodecException);
		public abstract void onError(android.media.MediaCodec arg0, android.media.MediaCodec.CodecException arg1);
	
		// GEN member:android.media.MediaCodec$Callback:public void onCryptoError(android.media.MediaCodec, android.media.MediaCodec$CryptoException);
		public void onCryptoError(android.media.MediaCodec arg0, android.media.MediaCodec.CryptoException arg1) {}
	
		// GEN member:android.media.MediaCodec$Callback:public abstract void onOutputFormatChanged(android.media.MediaCodec, android.media.MediaFormat);
		public abstract void onOutputFormatChanged(android.media.MediaCodec arg0, android.media.MediaFormat arg1);
	}


	// GEN class:android.media.MediaCodec$CodecException
	public final class CodecException extends java.lang.IllegalStateException {
		// GEN member:android.media.MediaCodec$CodecException:public static final int ERROR_INSUFFICIENT_RESOURCE = 1100;
		public static final int ERROR_INSUFFICIENT_RESOURCE = 1100;
	
		// GEN member:android.media.MediaCodec$CodecException:public static final int ERROR_RECLAIMED = 1101;
		public static final int ERROR_RECLAIMED = 1101;
	
		// GEN member:android.media.MediaCodec$CodecException:public boolean isTransient();
		public boolean isTransient() { return false; }
	
		// GEN member:android.media.MediaCodec$CodecException:public boolean isRecoverable();
		public boolean isRecoverable() { return false; }
	
		// GEN member:android.media.MediaCodec$CodecException:public int getErrorCode();
		public int getErrorCode() { return 0; }
	
		// GEN member:android.media.MediaCodec$CodecException:public java.lang.String getDiagnosticInfo();
		public java.lang.String getDiagnosticInfo() { return null; }
	
		protected CodecException() {}
	}


	// GEN class:android.media.MediaCodec$CryptoException
	public final class CryptoException extends java.lang.RuntimeException implements android.media.MediaDrmThrowable {
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_FRAME_TOO_LARGE = 8;
		public static final int ERROR_FRAME_TOO_LARGE = 8;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_INSUFFICIENT_OUTPUT_PROTECTION = 4;
		public static final int ERROR_INSUFFICIENT_OUTPUT_PROTECTION = 4;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_INSUFFICIENT_SECURITY = 7;
		public static final int ERROR_INSUFFICIENT_SECURITY = 7;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_KEY_EXPIRED = 2;
		public static final int ERROR_KEY_EXPIRED = 2;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_LOST_STATE = 9;
		public static final int ERROR_LOST_STATE = 9;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_NO_KEY = 1;
		public static final int ERROR_NO_KEY = 1;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_RESOURCE_BUSY = 3;
		public static final int ERROR_RESOURCE_BUSY = 3;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_SESSION_NOT_OPENED = 5;
		public static final int ERROR_SESSION_NOT_OPENED = 5;
	
		// GEN member:android.media.MediaCodec$CryptoException:public static final int ERROR_UNSUPPORTED_OPERATION = 6;
		public static final int ERROR_UNSUPPORTED_OPERATION = 6;
	
		// GEN member:android.media.MediaCodec$CryptoException:public android.media.MediaCodec$CryptoException(int, java.lang.String);
		public CryptoException(int arg0, java.lang.String arg1) {}
	
		// GEN member:android.media.MediaCodec$CryptoException:public int getErrorCode();
		public int getErrorCode() { return 0; }
	
		// GEN member:android.media.MediaCodec$CryptoException:public android.media.MediaCodec$CryptoInfo getCryptoInfo();
		public android.media.MediaCodec.CryptoInfo getCryptoInfo() { return null; }
	
		// GEN member:android.media.MediaCodec$CryptoException:public int getVendorError();
		public int getVendorError() { return 0; }
	
		// GEN member:android.media.MediaCodec$CryptoException:public int getOemError();
		public int getOemError() { return 0; }
	
		// GEN member:android.media.MediaCodec$CryptoException:public int getErrorContext();
		public int getErrorContext() { return 0; }
	
		protected CryptoException() {}
	}


	// GEN member:android.media.MediaCodec:public static android.media.MediaCodec createDecoderByType(java.lang.String) throws java.io.IOException;
	public static android.media.MediaCodec createDecoderByType(java.lang.String arg0) throws java.io.IOException { return null; }

	// GEN member:android.media.MediaCodec:public native android.view.Surface createInputSurface();
	public android.view.Surface createInputSurface() { return null; }

	// GEN member:android.media.MediaCodec:public static android.view.Surface createPersistentInputSurface();
	public static android.view.Surface createPersistentInputSurface() { return null; }

	// GEN member:android.media.MediaCodec:public void detachOutputSurface();
	public void detachOutputSurface() {}

	// GEN member:android.media.MediaCodec:public android.media.MediaCodecInfo getCodecInfo();
	public android.media.MediaCodecInfo getCodecInfo() { return null; }

	// GEN member:android.media.MediaCodec:public android.media.Image getInputImage(int);
	public android.media.Image getInputImage(int arg0) { return null; }

	// GEN member:android.media.MediaCodec:public void queueSecureInputBuffer(int, int, android.media.MediaCodec$CryptoInfo, long, int) throws android.media.MediaCodec$CryptoException;
	public void queueSecureInputBuffer(int arg0, int arg1, android.media.MediaCodec.CryptoInfo arg2, long arg3, int arg4) throws android.media.MediaCodec.CryptoException {}

	// GEN member:android.media.MediaCodec:public void reset();
	public void reset() {}

	// GEN member:android.media.MediaCodec:public void setCallback(android.media.MediaCodec$Callback, android.os.Handler);
	public void setCallback(android.media.MediaCodec.Callback arg0, android.os.Handler arg1) {}

	// GEN member:android.media.MediaCodec:public void setCallback(android.media.MediaCodec$Callback);
	public void setCallback(android.media.MediaCodec.Callback arg0) {}

	// GEN member:android.media.MediaCodec:public void setInputSurface(android.view.Surface);
	public void setInputSurface(android.view.Surface arg0) {}

	// GEN member:android.media.MediaCodec:public native void signalEndOfInputStream();
	public void signalEndOfInputStream() {}

	// GEN member:android.media.MediaCodec:public void subscribeToVendorParameters(java.util.List<java.lang.String>);
	public void subscribeToVendorParameters(java.util.List<java.lang.String> arg0) {}

	// GEN member:android.media.MediaCodec:public void unsubscribeFromVendorParameters(java.util.List<java.lang.String>);
	public void unsubscribeFromVendorParameters(java.util.List<java.lang.String> arg0) {}
}
