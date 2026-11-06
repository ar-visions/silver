package android.os;

// android.jar's shape, made by gen.py
public abstract class VibratorManager {
	// GEN member:android.os.VibratorManager:public abstract int[] getVibratorIds();
	public abstract int[] getVibratorIds();

	// GEN member:android.os.VibratorManager:public abstract android.os.Vibrator getVibrator(int);
	public abstract android.os.Vibrator getVibrator(int arg0);

	// GEN member:android.os.VibratorManager:public abstract android.os.Vibrator getDefaultVibrator();
	public abstract android.os.Vibrator getDefaultVibrator();

	// GEN member:android.os.VibratorManager:public abstract void cancel();
	public abstract void cancel();

	protected VibratorManager() {}
}
