package android.location;

public class Location implements android.os.Parcelable {

	private double latitude;
	private double longitude;
	private double altitude;
	private double accuracy;
	private double speed;
	private double bearing;
	private long timestamp;

	public Location(String provider) {}

	/* for internal use */
	public Location(double latitude,
	                double longitude,
	                double altitude,
	                double accuracy,
	                double speed,
	                double bearing,
	                long timestamp) {
		this.latitude = latitude;
		this.longitude = longitude;
		this.altitude = altitude;
		this.accuracy = accuracy;
		this.speed = speed;
		this.bearing = bearing;
		this.timestamp = timestamp;
	}

	public double getLatitude() {
		return latitude;
	}

	public void setLatitude(double latitude) {
		this.latitude = latitude;
	}

	public double getLongitude() {
		return longitude;
	}

	public void setLongitude(double longitude) {
		this.longitude = longitude;
	}

	public boolean hasAltitude() {
		return altitude != -Double.MAX_VALUE;
	}

	public double getAltitude() {
		return altitude;
	}

	public boolean hasAccuracy() {
		return true;
	}

	public float getAccuracy() {
		return (float)accuracy;
	}

	public void setAccuracy(float accuracy) {
		this.accuracy = accuracy;
	}

	public boolean hasSpeed() {
		return speed != -1;
	}

	public float getSpeed() {
		return (float)speed;
	}

	public boolean hasBearing() {
		return bearing != -1;
	}

	public float getBearing() {
		return (float)bearing;
	}

	public long getTime() {
		return timestamp;
	}

	public String getProvider() {
		return "fused";
	}

	// GEN member:android.location.Location:public static final android.os.Parcelable$Creator<android.location.Location> CREATOR;
	public static final android.os.Parcelable.Creator<android.location.Location> CREATOR = null;

	// GEN member:android.location.Location:public void setAltitude(double);
	public void setAltitude(double arg0) {}

	// GEN member:android.location.Location:public void setSpeed(float);
	public void setSpeed(float arg0) {}

	// GEN member:android.location.Location:public void setTime(long);
	public void setTime(long arg0) {}

	// GEN member:android.location.Location:public android.location.Location(android.location.Location);
	public Location(android.location.Location arg0) { this((java.lang.String)null); }

	// GEN member:android.location.Location:public static java.lang.String convert(double, int);
	public static java.lang.String convert(double arg0, int arg1) { return null; }

	// GEN member:android.location.Location:public static void distanceBetween(double, double, double, double, float[]);
	public static void distanceBetween(double arg0, double arg1, double arg2, double arg3, float[] arg4) {}

	// GEN member:android.location.Location:public float distanceTo(android.location.Location);
	public float distanceTo(android.location.Location arg0) { return 0; }

	// GEN member:android.location.Location:public float getBearingAccuracyDegrees();
	public float getBearingAccuracyDegrees() { return 0; }

	// GEN member:android.location.Location:public long getElapsedRealtimeNanos();
	public long getElapsedRealtimeNanos() { return 0; }

	// GEN member:android.location.Location:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.location.Location:public float getSpeedAccuracyMetersPerSecond();
	public float getSpeedAccuracyMetersPerSecond() { return 0; }

	// GEN member:android.location.Location:public float getVerticalAccuracyMeters();
	public float getVerticalAccuracyMeters() { return 0; }

	// GEN member:android.location.Location:public boolean hasBearingAccuracy();
	public boolean hasBearingAccuracy() { return false; }

	// GEN member:android.location.Location:public boolean hasSpeedAccuracy();
	public boolean hasSpeedAccuracy() { return false; }

	// GEN member:android.location.Location:public boolean hasVerticalAccuracy();
	public boolean hasVerticalAccuracy() { return false; }

	// GEN member:android.location.Location:public boolean isFromMockProvider();
	public boolean isFromMockProvider() { return false; }

	// GEN member:android.location.Location:public boolean isMock();
	public boolean isMock() { return false; }

	// GEN member:android.location.Location:public void setBearing(float);
	public void setBearing(float arg0) {}

	// GEN member:android.location.Location:public void setBearingAccuracyDegrees(float);
	public void setBearingAccuracyDegrees(float arg0) {}

	// GEN member:android.location.Location:public void setElapsedRealtimeNanos(long);
	public void setElapsedRealtimeNanos(long arg0) {}

	// GEN member:android.location.Location:public void setExtras(android.os.Bundle);
	public void setExtras(android.os.Bundle arg0) {}

	// GEN member:android.location.Location:public void setMock(boolean);
	public void setMock(boolean arg0) {}

	// GEN member:android.location.Location:public void setProvider(java.lang.String);
	public void setProvider(java.lang.String arg0) {}

	// GEN member:android.location.Location:public void setSpeedAccuracyMetersPerSecond(float);
	public void setSpeedAccuracyMetersPerSecond(float arg0) {}

	// GEN member:android.location.Location:public void setVerticalAccuracyMeters(float);
	public void setVerticalAccuracyMeters(float arg0) {}
}
