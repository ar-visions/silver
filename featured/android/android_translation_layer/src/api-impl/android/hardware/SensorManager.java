package android.hardware;

import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Handler;
import java.util.Arrays;
import java.util.List;

public class SensorManager {

	public static float GRAVITY_EARTH = 9.81f;

	public Sensor getDefaultSensor(int type) {
		return new Sensor(type);
	}

	public boolean registerListener(SensorEventListener listener, Sensor sensor, int samplingPeriodUs, Handler handler) {
		return true; // we could try saying that the sensor doesn't exist and hope the app just doesn't use it then, but as long as we never call the handler the app should leave this alone
	}

	public boolean registerListener(final SensorEventListener listener, final Sensor sensor, int samplingPeriodUs) {
		switch (sensor.getType()) {
			case Sensor.TYPE_ORIENTATION:
				new LocationManager().requestLocationUpdates(null, 0, 0, new LocationListener() {
					@Override
					public void onLocationChanged(Location location) {
						listener.onSensorChanged(new SensorEvent(new float[] {location.getBearing()}, sensor));
					}
				});
				return true;
			case Sensor.TYPE_ACCELEROMETER:
				register_accelerometer_listener_native(listener, sensor, samplingPeriodUs);
				return true;
			default:
				return false;
		}
	}

	public void unregisterListener(final SensorEventListener listener) {
		unregisterListener(listener, null);
	}

	public void unregisterListener(final SensorEventListener listener, Sensor sensor) {
		System.out.println("STUB: andoroid.hw.SensorManager.unregisterListener");
	}

	native void register_accelerometer_listener_native(SensorEventListener listener, Sensor sensor, int sampling_period);

	public List<Sensor> getSensorList(int type) {
		return Arrays.asList(getDefaultSensor(type));
	}

	// GEN member:android.hardware.SensorManager:public static float[] getOrientation(float[], float[]);
	public static float[] getOrientation(float[] arg0, float[] arg1) { return new float[0]; }

	// GEN member:android.hardware.SensorManager:public static void getQuaternionFromVector(float[], float[]);
	public static void getQuaternionFromVector(float[] arg0, float[] arg1) {}

	// GEN member:android.hardware.SensorManager:public static void getRotationMatrixFromVector(float[], float[]);
	public static void getRotationMatrixFromVector(float[] arg0, float[] arg1) {}

	// GEN member:android.hardware.SensorManager:public static boolean remapCoordinateSystem(float[], int, int, float[]);
	public static boolean remapCoordinateSystem(float[] arg0, int arg1, int arg2, float[] arg3) { return false; }

	// GEN member:android.hardware.SensorManager:public boolean flush(android.hardware.SensorEventListener);
	public boolean flush(android.hardware.SensorEventListener arg0) { return false; }

	// GEN member:android.hardware.SensorManager:public static float getAltitude(float, float);
	public static float getAltitude(float arg0, float arg1) { return 0; }

	// GEN member:android.hardware.SensorManager:public static boolean getRotationMatrix(float[], float[], float[], float[]);
	public static boolean getRotationMatrix(float[] arg0, float[] arg1, float[] arg2, float[] arg3) { return false; }

	// GEN member:android.hardware.SensorManager:public boolean registerListener(android.hardware.SensorEventListener, android.hardware.Sensor, int, int);
	public boolean registerListener(android.hardware.SensorEventListener arg0, android.hardware.Sensor arg1, int arg2, int arg3) { return false; }

	// GEN member:android.hardware.SensorManager:public boolean registerListener(android.hardware.SensorEventListener, android.hardware.Sensor, int, int, android.os.Handler);
	public boolean registerListener(android.hardware.SensorEventListener arg0, android.hardware.Sensor arg1, int arg2, int arg3, android.os.Handler arg4) { return false; }
}
