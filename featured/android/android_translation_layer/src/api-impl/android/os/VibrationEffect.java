package android.os;

public class VibrationEffect implements android.os.Parcelable {

	public static VibrationEffect createOneShot(long milliseconds, int amplitude) {
		return new VibrationEffect();
	}

	public static VibrationEffect createWaveform(long[] pattern, int repeat) {
		return new VibrationEffect();
	}

	// GEN member:android.os.VibrationEffect:public static android.os.VibrationEffect createPredefined(int);
	public static android.os.VibrationEffect createPredefined(int arg0) { return null; }

	// GEN member:android.os.VibrationEffect:public static android.os.VibrationEffect createWaveform(long[], int[], int);
	public static android.os.VibrationEffect createWaveform(long[] arg0, int[] arg1, int arg2) { return null; }
}
