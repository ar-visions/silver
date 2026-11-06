package android.os;

// android.jar's shape, made by gen.py
public final class PerformanceHintManager {
	// GEN member:android.os.PerformanceHintManager:public long getPreferredUpdateRateNanos();
	public long getPreferredUpdateRateNanos() { return 0; }

	// GEN member:android.os.PerformanceHintManager:public android.os.PerformanceHintManager$Session createHintSession(int[], long);
	public android.os.PerformanceHintManager.Session createHintSession(int[] arg0, long arg1) { return null; }

	protected PerformanceHintManager() {}

	// GEN class:android.os.PerformanceHintManager$Session
	public class Session implements java.io.Closeable {
		// GEN member:android.os.PerformanceHintManager$Session:protected void finalize() throws java.lang.Throwable;
		protected void finalize() throws java.lang.Throwable {}
	
		// GEN member:android.os.PerformanceHintManager$Session:public void updateTargetWorkDuration(long);
		public void updateTargetWorkDuration(long arg0) {}
	
		// GEN member:android.os.PerformanceHintManager$Session:public void reportActualWorkDuration(long);
		public void reportActualWorkDuration(long arg0) {}
	
		// GEN member:android.os.PerformanceHintManager$Session:public void close();
		public void close() {}
	
		// GEN member:android.os.PerformanceHintManager$Session:public void setPreferPowerEfficiency(boolean);
		public void setPreferPowerEfficiency(boolean arg0) {}
	
		// GEN member:android.os.PerformanceHintManager$Session:public void setThreads(int[]);
		public void setThreads(int[] arg0) {}
	
		protected Session() {}
	}

}
