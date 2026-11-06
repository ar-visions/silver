package android.location;

// android.jar's shape, made by gen.py
public final class LocationRequest implements android.os.Parcelable {
	// GEN member:android.location.LocationRequest:public static final android.os.Parcelable$Creator<android.location.LocationRequest> CREATOR;
	public static final android.os.Parcelable.Creator<android.location.LocationRequest> CREATOR = null;

	// GEN member:android.location.LocationRequest:public static final long PASSIVE_INTERVAL = 9223372036854775807l;
	public static final long PASSIVE_INTERVAL = 9223372036854775807L;

	// GEN member:android.location.LocationRequest:public static final int QUALITY_BALANCED_POWER_ACCURACY = 102;
	public static final int QUALITY_BALANCED_POWER_ACCURACY = 102;

	// GEN member:android.location.LocationRequest:public static final int QUALITY_HIGH_ACCURACY = 100;
	public static final int QUALITY_HIGH_ACCURACY = 100;

	// GEN member:android.location.LocationRequest:public static final int QUALITY_LOW_POWER = 104;
	public static final int QUALITY_LOW_POWER = 104;

	// GEN member:android.location.LocationRequest:public int getQuality();
	public int getQuality() { return 0; }

	// GEN member:android.location.LocationRequest:public long getIntervalMillis();
	public long getIntervalMillis() { return 0; }

	// GEN member:android.location.LocationRequest:public long getDurationMillis();
	public long getDurationMillis() { return 0; }

	// GEN member:android.location.LocationRequest:public int getMaxUpdates();
	public int getMaxUpdates() { return 0; }

	// GEN member:android.location.LocationRequest:public long getMinUpdateIntervalMillis();
	public long getMinUpdateIntervalMillis() { return 0; }

	// GEN member:android.location.LocationRequest:public float getMinUpdateDistanceMeters();
	public float getMinUpdateDistanceMeters() { return 0; }

	// GEN member:android.location.LocationRequest:public long getMaxUpdateDelayMillis();
	public long getMaxUpdateDelayMillis() { return 0; }

	// GEN member:android.location.LocationRequest:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.location.LocationRequest:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.location.LocationRequest:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.location.LocationRequest:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.location.LocationRequest:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected LocationRequest() {}

	// GEN class:android.location.LocationRequest$Builder
	public final class Builder {
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder(long);
		public Builder(long arg0) {}
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder(android.location.LocationRequest);
		public Builder(android.location.LocationRequest arg0) {}
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setIntervalMillis(long);
		public android.location.LocationRequest.Builder setIntervalMillis(long arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setQuality(int);
		public android.location.LocationRequest.Builder setQuality(int arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setDurationMillis(long);
		public android.location.LocationRequest.Builder setDurationMillis(long arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setMaxUpdates(int);
		public android.location.LocationRequest.Builder setMaxUpdates(int arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setMinUpdateIntervalMillis(long);
		public android.location.LocationRequest.Builder setMinUpdateIntervalMillis(long arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder clearMinUpdateIntervalMillis();
		public android.location.LocationRequest.Builder clearMinUpdateIntervalMillis() { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setMinUpdateDistanceMeters(float);
		public android.location.LocationRequest.Builder setMinUpdateDistanceMeters(float arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest$Builder setMaxUpdateDelayMillis(long);
		public android.location.LocationRequest.Builder setMaxUpdateDelayMillis(long arg0) { return this; }
	
		// GEN member:android.location.LocationRequest$Builder:public android.location.LocationRequest build();
		public android.location.LocationRequest build() { return null; }
	
		protected Builder() {}
	}

}
