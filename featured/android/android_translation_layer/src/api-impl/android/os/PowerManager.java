package android.os;

public final class PowerManager {
	public final class WakeLock {
		private boolean held;
		private boolean refCounted = true;
		private int count;

		public void setReferenceCounted(boolean referenceCounted) {
			refCounted = referenceCounted;
		}

		public void acquire() {
			if (refCounted) {
				count++;
			}
			held = true;
		}

		public void release() {
			if (refCounted) {
				count--;
				if (count > 0)
					return;
			}
			held = false;
		}

		public boolean isHeld() {
			return held;
		}

		public void acquire(long timeout) {
			acquire();
			new Thread(new Runnable() {
				public void run() {
					try {
						Thread.sleep(timeout);
					} catch (InterruptedException e) {
					}
					release();
				}
			}).start();
		}

		public void release(int flags) {
			release();
		}
	
	// GEN member:android.os.PowerManager$WakeLock:public void setWorkSource(android.os.WorkSource);
	public void setWorkSource(android.os.WorkSource arg0) {}
}

	public WakeLock newWakeLock(int levelAndFlags, String tag) {
		return new WakeLock();
	}

	public void goToSleep(long time) {
		// This requires DEVICE_POWER permission which is only possible for system processes. Any normal application gets a SecurityException.
		throw new SecurityException();
	}

	public void userActivity(long dummy, boolean dummy2) {}

	public static final int FULL_WAKE_LOCK = 0x1a;

	public boolean isPowerSaveMode() {
		return false;
	}

	public boolean isScreenOn() {
		return true;
	}

	public boolean isIgnoringBatteryOptimizations(String packageName) {
		return true;
	}

	public boolean isDeviceIdleMode() {
		return false;
	}

	public boolean isInteractive() {
		return true;
	}

	public boolean isWakeLockLevelSupported(int level) {
		return false;
	}

	public interface OnThermalStatusChangedListener {
		void onThermalStatusChanged(int status);
	}

	// THERMAL_STATUS_NONE
	public int getCurrentThermalStatus() {
		return 0;
	}

	public void addThermalStatusListener(OnThermalStatusChangedListener listener) {}

	public void addThermalStatusListener(java.util.concurrent.Executor executor, OnThermalStatusChangedListener listener) {}

	public void removeThermalStatusListener(OnThermalStatusChangedListener listener) {}

	// GEN member:android.os.PowerManager:public boolean isDeviceLightIdleMode();
	public boolean isDeviceLightIdleMode() { return false; }

	// GEN member:android.os.PowerManager:public boolean isLowPowerStandbyEnabled();
	public boolean isLowPowerStandbyEnabled() { return false; }

	// GEN member:android.os.PowerManager:public int getLocationPowerSaveMode();
	public int getLocationPowerSaveMode() { return 0; }
}
