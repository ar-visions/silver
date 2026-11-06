package android.media;

// android.jar's shape, made by gen.py
public final class MediaRoute2Info implements android.os.Parcelable {
	// GEN member:android.media.MediaRoute2Info:public static final int CONNECTION_STATE_CONNECTED = 2;
	public static final int CONNECTION_STATE_CONNECTED = 2;

	// GEN member:android.media.MediaRoute2Info:public static final int CONNECTION_STATE_CONNECTING = 1;
	public static final int CONNECTION_STATE_CONNECTING = 1;

	// GEN member:android.media.MediaRoute2Info:public static final int CONNECTION_STATE_DISCONNECTED = 0;
	public static final int CONNECTION_STATE_DISCONNECTED = 0;

	// GEN member:android.media.MediaRoute2Info:public static final android.os.Parcelable$Creator<android.media.MediaRoute2Info> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.MediaRoute2Info> CREATOR = null;

	// GEN member:android.media.MediaRoute2Info:public static final int PLAYBACK_VOLUME_FIXED = 0;
	public static final int PLAYBACK_VOLUME_FIXED = 0;

	// GEN member:android.media.MediaRoute2Info:public static final int PLAYBACK_VOLUME_VARIABLE = 1;
	public static final int PLAYBACK_VOLUME_VARIABLE = 1;

	// GEN member:android.media.MediaRoute2Info:public static final int SUITABILITY_STATUS_NOT_SUITABLE_FOR_TRANSFER = 2;
	public static final int SUITABILITY_STATUS_NOT_SUITABLE_FOR_TRANSFER = 2;

	// GEN member:android.media.MediaRoute2Info:public static final int SUITABILITY_STATUS_SUITABLE_FOR_DEFAULT_TRANSFER = 0;
	public static final int SUITABILITY_STATUS_SUITABLE_FOR_DEFAULT_TRANSFER = 0;

	// GEN member:android.media.MediaRoute2Info:public static final int SUITABILITY_STATUS_SUITABLE_FOR_MANUAL_TRANSFER = 1;
	public static final int SUITABILITY_STATUS_SUITABLE_FOR_MANUAL_TRANSFER = 1;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_BLE_HEADSET = 26;
	public static final int TYPE_BLE_HEADSET = 26;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_BLUETOOTH_A2DP = 8;
	public static final int TYPE_BLUETOOTH_A2DP = 8;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_BUILTIN_SPEAKER = 2;
	public static final int TYPE_BUILTIN_SPEAKER = 2;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_DOCK = 13;
	public static final int TYPE_DOCK = 13;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_GROUP = 2000;
	public static final int TYPE_GROUP = 2000;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_HDMI = 9;
	public static final int TYPE_HDMI = 9;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_HDMI_ARC = 10;
	public static final int TYPE_HDMI_ARC = 10;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_HDMI_EARC = 29;
	public static final int TYPE_HDMI_EARC = 29;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_HEARING_AID = 23;
	public static final int TYPE_HEARING_AID = 23;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_AUDIO_VIDEO_RECEIVER = 1003;
	public static final int TYPE_REMOTE_AUDIO_VIDEO_RECEIVER = 1003;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_CAR = 1008;
	public static final int TYPE_REMOTE_CAR = 1008;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_COMPUTER = 1006;
	public static final int TYPE_REMOTE_COMPUTER = 1006;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_GAME_CONSOLE = 1007;
	public static final int TYPE_REMOTE_GAME_CONSOLE = 1007;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_SMARTPHONE = 1010;
	public static final int TYPE_REMOTE_SMARTPHONE = 1010;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_SMARTWATCH = 1009;
	public static final int TYPE_REMOTE_SMARTWATCH = 1009;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_SPEAKER = 1002;
	public static final int TYPE_REMOTE_SPEAKER = 1002;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_TABLET = 1004;
	public static final int TYPE_REMOTE_TABLET = 1004;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_TABLET_DOCKED = 1005;
	public static final int TYPE_REMOTE_TABLET_DOCKED = 1005;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_REMOTE_TV = 1001;
	public static final int TYPE_REMOTE_TV = 1001;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_UNKNOWN = 0;
	public static final int TYPE_UNKNOWN = 0;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_USB_ACCESSORY = 12;
	public static final int TYPE_USB_ACCESSORY = 12;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_USB_DEVICE = 11;
	public static final int TYPE_USB_DEVICE = 11;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_USB_HEADSET = 22;
	public static final int TYPE_USB_HEADSET = 22;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_WIRED_HEADPHONES = 4;
	public static final int TYPE_WIRED_HEADPHONES = 4;

	// GEN member:android.media.MediaRoute2Info:public static final int TYPE_WIRED_HEADSET = 3;
	public static final int TYPE_WIRED_HEADSET = 3;

	// GEN member:android.media.MediaRoute2Info:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.media.MediaRoute2Info:public java.lang.CharSequence getName();
	public java.lang.CharSequence getName() { return null; }

	// GEN member:android.media.MediaRoute2Info:public java.util.List<java.lang.String> getFeatures();
	public java.util.List<java.lang.String> getFeatures() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaRoute2Info:public int getType();
	public int getType() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public boolean isSystemRoute();
	public boolean isSystemRoute() { return false; }

	// GEN member:android.media.MediaRoute2Info:public android.net.Uri getIconUri();
	public android.net.Uri getIconUri() { return null; }

	// GEN member:android.media.MediaRoute2Info:public java.lang.CharSequence getDescription();
	public java.lang.CharSequence getDescription() { return null; }

	// GEN member:android.media.MediaRoute2Info:public int getConnectionState();
	public int getConnectionState() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public java.lang.String getClientPackageName();
	public java.lang.String getClientPackageName() { return null; }

	// GEN member:android.media.MediaRoute2Info:public int getVolumeHandling();
	public int getVolumeHandling() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public int getVolumeMax();
	public int getVolumeMax() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public int getVolume();
	public int getVolume() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public java.util.Set<java.lang.String> getDeduplicationIds();
	public java.util.Set<java.lang.String> getDeduplicationIds() { return new java.util.HashSet<>(); }

	// GEN member:android.media.MediaRoute2Info:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.media.MediaRoute2Info:public int getSuitabilityStatus();
	public int getSuitabilityStatus() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.MediaRoute2Info:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.media.MediaRoute2Info:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.media.MediaRoute2Info:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected MediaRoute2Info() {}
}
