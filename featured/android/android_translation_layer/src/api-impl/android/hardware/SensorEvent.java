package android.hardware;

import android.atl.annotation.Export;

public class SensorEvent {

	public final float[] values;

	public Sensor sensor;

	public long timestamp;

	@Export
	public SensorEvent(float[] values, Sensor sensor) {
		this.values = values;
		this.sensor = sensor;
	}

	// GEN member:android.hardware.SensorEvent:public int accuracy;
	public int accuracy;
}
