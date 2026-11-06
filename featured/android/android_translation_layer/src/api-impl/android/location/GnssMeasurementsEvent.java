package android.location;

// android.jar's shape, made by gen.py
public final class GnssMeasurementsEvent implements android.os.Parcelable {
	// GEN member:android.location.GnssMeasurementsEvent:public static final android.os.Parcelable$Creator<android.location.GnssMeasurementsEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.location.GnssMeasurementsEvent> CREATOR = null;

	// GEN member:android.location.GnssMeasurementsEvent:public android.location.GnssClock getClock();
	public android.location.GnssClock getClock() { return null; }

	// GEN member:android.location.GnssMeasurementsEvent:public java.util.Collection<android.location.GnssMeasurement> getMeasurements();
	public java.util.Collection<android.location.GnssMeasurement> getMeasurements() { return new java.util.ArrayList<>(); }

	// GEN member:android.location.GnssMeasurementsEvent:public java.util.Collection<android.location.GnssAutomaticGainControl> getGnssAutomaticGainControls();
	public java.util.Collection<android.location.GnssAutomaticGainControl> getGnssAutomaticGainControls() { return new java.util.ArrayList<>(); }

	// GEN member:android.location.GnssMeasurementsEvent:public boolean isFullTracking();
	public boolean isFullTracking() { return false; }

	// GEN member:android.location.GnssMeasurementsEvent:public boolean hasIsFullTracking();
	public boolean hasIsFullTracking() { return false; }

	// GEN member:android.location.GnssMeasurementsEvent:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.location.GnssMeasurementsEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.location.GnssMeasurementsEvent:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected GnssMeasurementsEvent() {}

	// GEN class:android.location.GnssMeasurementsEvent$Callback
	public abstract class Callback {
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public static final int STATUS_LOCATION_DISABLED = 2;
		public static final int STATUS_LOCATION_DISABLED = 2;
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public static final int STATUS_NOT_ALLOWED = 3;
		public static final int STATUS_NOT_ALLOWED = 3;
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public static final int STATUS_NOT_SUPPORTED = 0;
		public static final int STATUS_NOT_SUPPORTED = 0;
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public static final int STATUS_READY = 1;
		public static final int STATUS_READY = 1;
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public android.location.GnssMeasurementsEvent$Callback();
		public Callback() {}
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public void onGnssMeasurementsReceived(android.location.GnssMeasurementsEvent);
		public void onGnssMeasurementsReceived(android.location.GnssMeasurementsEvent arg0) {}
	
		// GEN member:android.location.GnssMeasurementsEvent$Callback:public void onStatusChanged(int);
		public void onStatusChanged(int arg0) {}
	}

}
