package android.media;

public class MediaCodecInfo {

	private String name;
	private String mime;

	public MediaCodecInfo(String name, String mime) {
		this.name = name;
		this.mime = mime;
	}

	public String getName() {
		return name;
	}

	public boolean isEncoder() {
		return false;
	}

	public String[] getSupportedTypes() {
		return new String[] {mime};
	}

	public CodecCapabilities getCapabilitiesForType(String type) {
		CodecCapabilities caps = new CodecCapabilities();
		if ("video/avc".equals(type)) {
			caps.profileLevels = new CodecProfileLevel[] {
				new CodecProfileLevel(CodecProfileLevel.AVCProfileBaseline, CodecProfileLevel.AVCLevel61),
				new CodecProfileLevel(CodecProfileLevel.AVCProfileMain, CodecProfileLevel.AVCLevel61),
			};
		}
		return caps;
	}

	public static class CodecCapabilities {

		public CodecProfileLevel[] profileLevels;

		public boolean isFeatureSupported(String feature) {
			System.out.println("CodecCapabilities.isFeatureSupported(" + feature + ")");
			return false;
		}

		public boolean isFeatureRequired(String feature) {
			System.out.println("CodecCapabilities.isFeatureRequired(" + feature + ")");
			return false;
		}

		public AudioCapabilities getAudioCapabilities() {
			return new AudioCapabilities();
		}

		public VideoCapabilities getVideoCapabilities() {
			return new VideoCapabilities();
		}

		public int getMaxSupportedInstances() {
			return 1;
		}
	}

	public static final class CodecProfileLevel {

		public int profile;
		public int level;

		public CodecProfileLevel() {}

		public CodecProfileLevel(int profile, int level) {
			this.profile = profile;
			this.level = level;
		}

		// AVC profiles
		public static final int AVCProfileBaseline = 0x01;
		public static final int AVCProfileMain = 0x02;
		public static final int AVCProfileExtended = 0x04;
		public static final int AVCProfileHigh = 0x08;
		public static final int AVCProfileHigh10 = 0x10;
		public static final int AVCProfileHigh422 = 0x20;
		public static final int AVCProfileHigh444 = 0x40;

		// AVC levels
		public static final int AVCLevel1 = 0x01;
		public static final int AVCLevel1b = 0x02;
		public static final int AVCLevel11 = 0x04;
		public static final int AVCLevel12 = 0x08;
		public static final int AVCLevel13 = 0x10;
		public static final int AVCLevel2 = 0x20;
		public static final int AVCLevel21 = 0x40;
		public static final int AVCLevel22 = 0x80;
		public static final int AVCLevel3 = 0x100;
		public static final int AVCLevel31 = 0x200;
		public static final int AVCLevel32 = 0x400;
		public static final int AVCLevel4 = 0x800;
		public static final int AVCLevel41 = 0x1000;
		public static final int AVCLevel42 = 0x2000;
		public static final int AVCLevel5 = 0x4000;
		public static final int AVCLevel51 = 0x8000;
		public static final int AVCLevel52 = 0x10000;
		public static final int AVCLevel6 = 0x20000;
		public static final int AVCLevel61 = 0x40000;
		public static final int AVCLevel62 = 0x80000;

		// VP9 profiles
		public static final int VP9Profile0 = 0x01;
		public static final int VP9Profile1 = 0x02;
		public static final int VP9Profile2 = 0x04;
		public static final int VP9Profile3 = 0x08;
		public static final int VP9Profile2HDR10Plus = 0x1000;
		public static final int VP9Profile3HDR10Plus = 0x2000;

		// VP9 levels
		public static final int VP9Level1 = 0x1;
		public static final int VP9Level11 = 0x2;
		public static final int VP9Level2 = 0x4;
		public static final int VP9Level21 = 0x8;
		public static final int VP9Level3 = 0x10;
		public static final int VP9Level31 = 0x20;
		public static final int VP9Level4 = 0x40;
		public static final int VP9Level41 = 0x80;
		public static final int VP9Level5 = 0x100;
		public static final int VP9Level51 = 0x200;
		public static final int VP9Level52 = 0x400;
		public static final int VP9Level6 = 0x800;
		public static final int VP9Level61 = 0x1000;
		public static final int VP9Level62 = 0x2000;

		// VP8 profile/level
		public static final int VP8ProfileMain = 0x01;
		public static final int VP8LevelMain = 0x01;

		// HEVC profiles
		public static final int HEVCProfileMain = 0x01;
		public static final int HEVCProfileMain10 = 0x02;
		public static final int HEVCProfileMain10HDR10 = 0x1000;
		public static final int HEVCProfileMainStill = 0x04;

		// HEVC levels
		public static final int HEVCMainTierLevel1 = 0x1;
		public static final int HEVCMainTierLevel2 = 0x2;
		public static final int HEVCMainTierLevel21 = 0x4;
		public static final int HEVCMainTierLevel3 = 0x8;
		public static final int HEVCMainTierLevel31 = 0x10;
		public static final int HEVCMainTierLevel4 = 0x20;
		public static final int HEVCMainTierLevel41 = 0x40;
		public static final int HEVCMainTierLevel5 = 0x80;
		public static final int HEVCMainTierLevel51 = 0x100;
		public static final int HEVCMainTierLevel52 = 0x200;
		public static final int HEVCMainTierLevel6 = 0x400;
		public static final int HEVCMainTierLevel61 = 0x800;
		public static final int HEVCMainTierLevel62 = 0x1000;
		public static final int HEVCHighTierLevel1 = 0x1;
		public static final int HEVCHighTierLevel2 = 0x2;
		public static final int HEVCHighTierLevel21 = 0x4;
		public static final int HEVCHighTierLevel3 = 0x8;
		public static final int HEVCHighTierLevel31 = 0x10;
		public static final int HEVCHighTierLevel4 = 0x20;
		public static final int HEVCHighTierLevel41 = 0x40;
		public static final int HEVCHighTierLevel5 = 0x80;
		public static final int HEVCHighTierLevel51 = 0x100;
		public static final int HEVCHighTierLevel52 = 0x200;
		public static final int HEVCHighTierLevel6 = 0x400;
		public static final int HEVCHighTierLevel61 = 0x800;
		public static final int HEVCHighTierLevel62 = 0x1000;

		// AAC profiles
		public static final int AACObjectMain = 1;
		public static final int AACObjectLC = 2;
		public static final int AACObjectSSR = 3;
		public static final int AACObjectLTP = 4;
		public static final int AACObjectHE = 5;
		public static final int AACObjectScalable = 6;
		public static final int AACObjectERLC = 17;
		public static final int AACObjectERScalable = 20;
		public static final int AACObjectLD = 23;
		public static final int AACObjectHE_PS = 29;
		public static final int AACObjectELD = 39;
		public static final int AACObjectXHE = 42;
	}

	public static class VideoCapabilities {

		public int getWidthAlignment() {
			return 1;
		}

		public int getHeightAlignment() {
			return 1;
		}

		public boolean areSizeAndRateSupported(int width, int height, double frameRate) {
			return width > 0 && height > 0 && frameRate > 0;
		}
	
	// GEN class:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint
	public final class PerformancePoint {
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint(int, int, int);
		public PerformancePoint(int arg0, int arg1, int arg2) {}
	
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public int hashCode();
		public int hashCode() { return 0; }
	
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public boolean covers(android.media.MediaFormat);
		public boolean covers(android.media.MediaFormat arg0) { return false; }
	
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public boolean covers(android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint);
		public boolean covers(android.media.MediaCodecInfo.VideoCapabilities.PerformancePoint arg0) { return false; }
	
		// GEN member:android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		protected PerformancePoint() {}
	}


	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Double> getAchievableFrameRatesFor(int, int);
	public android.util.Range<java.lang.Double> getAchievableFrameRatesFor(int arg0, int arg1) { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getBitrateRange();
	public android.util.Range<java.lang.Integer> getBitrateRange() { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getSupportedFrameRates();
	public android.util.Range<java.lang.Integer> getSupportedFrameRates() { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getSupportedHeightsFor(int);
	public android.util.Range<java.lang.Integer> getSupportedHeightsFor(int arg0) { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getSupportedHeights();
	public android.util.Range<java.lang.Integer> getSupportedHeights() { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public java.util.List<android.media.MediaCodecInfo$VideoCapabilities$PerformancePoint> getSupportedPerformancePoints();
	public java.util.List<android.media.MediaCodecInfo.VideoCapabilities.PerformancePoint> getSupportedPerformancePoints() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getSupportedWidthsFor(int);
	public android.util.Range<java.lang.Integer> getSupportedWidthsFor(int arg0) { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public android.util.Range<java.lang.Integer> getSupportedWidths();
	public android.util.Range<java.lang.Integer> getSupportedWidths() { return null; }

	// GEN member:android.media.MediaCodecInfo$VideoCapabilities:public boolean isSizeSupported(int, int);
	public boolean isSizeSupported(int arg0, int arg1) { return false; }
}

	public static class AudioCapabilities {

		public boolean isSampleRateSupported(int sampleRate) {
			return true;
		}

		public int getMaxInputChannelCount() {
			return 2;
		}
	
	// GEN member:android.media.MediaCodecInfo$AudioCapabilities:public android.util.Range<java.lang.Integer> getBitrateRange();
	public android.util.Range<java.lang.Integer> getBitrateRange() { return null; }
}

	// GEN member:android.media.MediaCodecInfo:public boolean isAlias();
	public boolean isAlias() { return false; }

	// GEN member:android.media.MediaCodecInfo:public boolean isHardwareAccelerated();
	public boolean isHardwareAccelerated() { return false; }

	// GEN member:android.media.MediaCodecInfo:public boolean isSoftwareOnly();
	public boolean isSoftwareOnly() { return false; }

	// GEN member:android.media.MediaCodecInfo:public boolean isVendor();
	public boolean isVendor() { return false; }
}
