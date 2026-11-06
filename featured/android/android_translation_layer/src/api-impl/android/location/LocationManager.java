package android.location;

import android.atl.annotation.Export;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import java.lang.Runnable;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class LocationManager {

	static Set<LocationListener> listeners = new HashSet<>();

	public String getBestProvider(Criteria criteria, boolean enabledOnly) {
		return "xdgportal";
	}

	public Location getLastKnownLocation(String provider) {
		return null;
	}

	public void requestLocationUpdates(String provider, long minTimeMs, float minDistanceM, LocationListener listener) {
		listeners.add(listener);
		nativeGetLocation();
	}

	private native void nativeGetLocation();

	@Export
	private static void locationUpdated(double latitude,
	                                    double longitude,
	                                    double altitude,
	                                    double accuracy,
	                                    double speed,
	                                    double bearing,
	                                    long timestamp) {
		for (LocationListener locationListener : listeners) {
			locationListener.onLocationChanged(new Location(latitude, longitude, altitude, accuracy, speed, bearing, timestamp));
		}
	}

	public boolean sendExtraCommand(String provider, String command, Bundle extras) {
		return true;
	}

	public GpsStatus getGpsStatus(GpsStatus status) {
		if (status == null) {
			status = new GpsStatus();
		}
		return status;
	}

	public void removeUpdates(LocationListener listener) {
	}

	public List<String> getAllProviders() {
		return Collections.emptyList();
	}

	public List<String> getProviders(boolean enabledOnly) {
		return Collections.emptyList();
	}

	public boolean registerGnssStatusCallback(GnssStatus.Callback callback, Handler handler) {
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				callback.onSatelliteStatusChanged(new GnssStatus());
			}
		});
		return true;
	}

	public void unregisterGnssStatusCallback(GnssStatus.Callback callback) {
	}

	public boolean isProviderEnabled(String provider) {
		return System.getenv("ATL_UGLY_ENABLE_LOCATION") != null;
	}

	// GEN member:android.location.LocationManager:public android.location.GnssCapabilities getGnssCapabilities();
	public android.location.GnssCapabilities getGnssCapabilities() { return null; }

	// GEN member:android.location.LocationManager:public boolean isLocationEnabled();
	public boolean isLocationEnabled() { return false; }

	// GEN member:android.location.LocationManager:public boolean registerGnssMeasurementsCallback(android.location.GnssMeasurementRequest, java.util.concurrent.Executor, android.location.GnssMeasurementsEvent$Callback);
	public boolean registerGnssMeasurementsCallback(android.location.GnssMeasurementRequest arg0, java.util.concurrent.Executor arg1, android.location.GnssMeasurementsEvent.Callback arg2) { return false; }

	// GEN member:android.location.LocationManager:public void requestLocationUpdates(java.lang.String, long, float, android.location.LocationListener, android.os.Looper);
	public void requestLocationUpdates(java.lang.String arg0, long arg1, float arg2, android.location.LocationListener arg3, android.os.Looper arg4) {}

	// GEN member:android.location.LocationManager:public void unregisterGnssMeasurementsCallback(android.location.GnssMeasurementsEvent$Callback);
	public void unregisterGnssMeasurementsCallback(android.location.GnssMeasurementsEvent.Callback arg0) {}
}
