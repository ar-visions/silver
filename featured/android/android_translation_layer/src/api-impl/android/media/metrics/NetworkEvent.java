package android.media.metrics;

// android.jar's shape, made by gen.py
public final class NetworkEvent extends android.media.metrics.Event implements android.os.Parcelable {
	// GEN member:android.media.metrics.NetworkEvent:public static final android.os.Parcelable$Creator<android.media.metrics.NetworkEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.metrics.NetworkEvent> CREATOR = null;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_2G = 4;
	public static final int NETWORK_TYPE_2G = 4;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_3G = 5;
	public static final int NETWORK_TYPE_3G = 5;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_4G = 6;
	public static final int NETWORK_TYPE_4G = 6;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_5G_NSA = 7;
	public static final int NETWORK_TYPE_5G_NSA = 7;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_5G_SA = 8;
	public static final int NETWORK_TYPE_5G_SA = 8;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_ETHERNET = 3;
	public static final int NETWORK_TYPE_ETHERNET = 3;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_OFFLINE = 9;
	public static final int NETWORK_TYPE_OFFLINE = 9;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_OTHER = 1;
	public static final int NETWORK_TYPE_OTHER = 1;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_UNKNOWN = 0;
	public static final int NETWORK_TYPE_UNKNOWN = 0;

	// GEN member:android.media.metrics.NetworkEvent:public static final int NETWORK_TYPE_WIFI = 2;
	public static final int NETWORK_TYPE_WIFI = 2;

	// GEN member:android.media.metrics.NetworkEvent:public int getNetworkType();
	public int getNetworkType() { return 0; }

	// GEN member:android.media.metrics.NetworkEvent:public long getTimeSinceCreatedMillis();
	public long getTimeSinceCreatedMillis() { return 0; }

	// GEN member:android.media.metrics.NetworkEvent:public android.os.Bundle getMetricsBundle();
	public android.os.Bundle getMetricsBundle() { return null; }

	// GEN member:android.media.metrics.NetworkEvent:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.media.metrics.NetworkEvent:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.media.metrics.NetworkEvent:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.media.metrics.NetworkEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.media.metrics.NetworkEvent:public int describeContents();
	public int describeContents() { return 0; }

	protected NetworkEvent() {}

	// GEN class:android.media.metrics.NetworkEvent$Builder
	public final class Builder {
		// GEN member:android.media.metrics.NetworkEvent$Builder:public android.media.metrics.NetworkEvent$Builder();
		public Builder() {}
	
		// GEN member:android.media.metrics.NetworkEvent$Builder:public android.media.metrics.NetworkEvent$Builder setNetworkType(int);
		public android.media.metrics.NetworkEvent.Builder setNetworkType(int arg0) { return this; }
	
		// GEN member:android.media.metrics.NetworkEvent$Builder:public android.media.metrics.NetworkEvent$Builder setTimeSinceCreatedMillis(long);
		public android.media.metrics.NetworkEvent.Builder setTimeSinceCreatedMillis(long arg0) { return this; }
	
		// GEN member:android.media.metrics.NetworkEvent$Builder:public android.media.metrics.NetworkEvent$Builder setMetricsBundle(android.os.Bundle);
		public android.media.metrics.NetworkEvent.Builder setMetricsBundle(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.media.metrics.NetworkEvent$Builder:public android.media.metrics.NetworkEvent build();
		public android.media.metrics.NetworkEvent build() { return null; }
	}

}
