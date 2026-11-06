package android.os;

import android.os.strictmode.Violation;
import java.util.concurrent.Executor;

// policies are kept, never enforced: no violation is raised
public final class StrictMode {
	public static void setThreadPolicy(final ThreadPolicy policy) {}
	public static void setVmPolicy(final VmPolicy policy) {}
	public static VmPolicy getVmPolicy() { return VmPolicy.LAX; }
	public static ThreadPolicy allowThreadDiskWrites() { return ThreadPolicy.LAX; }
	public static ThreadPolicy allowThreadDiskReads() { return ThreadPolicy.LAX; }
	public static ThreadPolicy getThreadPolicy() { return ThreadPolicy.LAX; }
	public static void noteSlowCall(String tag) {}
	public static void enableDefaults() {}

	public interface OnThreadViolationListener {
		default void onThreadViolation(Violation v) {}
	}

	public interface OnVmViolationListener {
		default void onVmViolation(Violation v) {}
	}

	public static final class ThreadPolicy {
		public static final ThreadPolicy LAX = new ThreadPolicy(null, null);
		final OnThreadViolationListener listener;
		final Executor callbackExecutor;

		private ThreadPolicy(OnThreadViolationListener listener, Executor executor) {
			this.listener = listener;
			this.callbackExecutor = executor;
		}

		public static final class Builder {
			private OnThreadViolationListener listener;
			private Executor executor;

			public Builder() {}

			public Builder(ThreadPolicy policy) {
				if (policy != null) {
					listener = policy.listener;
					executor = policy.callbackExecutor;
				}
			}

			public Builder detectAll() { return this; }
			public Builder detectNetwork() { return this; }
			public Builder detectDiskReads() { return this; }
			public Builder detectDiskWrites() { return this; }
			public Builder detectCustomSlowCalls() { return this; }
			public Builder detectResourceMismatches() { return this; }
			public Builder detectUnbufferedIo() { return this; }
			public Builder detectExplicitGc() { return this; }
			public Builder detectActivityLeaks() { return this; }
			public Builder detectLeakedSqlLiteObjects() { return this; }
			public Builder detectLeakedClosableObjects() { return this; }
			public Builder detectLeakedRegistrationObjects() { return this; }
			public Builder detectFileUriExposure() { return this; }
			public Builder detectCleartextNetwork() { return this; }
			public Builder detectContentUriWithoutPermission() { return this; }
			public Builder detectUntaggedSockets() { return this; }
			public Builder detectNonSdkApiUsage() { return this; }
			public Builder detectCredentialProtectedWhileLocked() { return this; }
			public Builder detectImplicitDirectBoot() { return this; }
			public Builder detectIncorrectContextUse() { return this; }
			public Builder detectUnsafeIntentLaunch() { return this; }
			public Builder detectBlockedBackgroundActivityLaunch() { return this; }
			public Builder permitAll() { return this; }
			public Builder permitNetwork() { return this; }
			public Builder permitDiskReads() { return this; }
			public Builder permitDiskWrites() { return this; }
			public Builder permitCustomSlowCalls() { return this; }
			public Builder permitResourceMismatches() { return this; }
			public Builder permitUnbufferedIo() { return this; }
			public Builder permitExplicitGc() { return this; }
			public Builder permitNonSdkApiUsage() { return this; }
			public Builder permitUnsafeIntentLaunch() { return this; }
			public Builder permitActivityLeaks() { return this; }
			public Builder permitLeakedClosableObjects() { return this; }
			public Builder penaltyLog() { return this; }
			public Builder penaltyDeath() { return this; }
			public Builder penaltyDialog() { return this; }
			public Builder penaltyDropBox() { return this; }
			public Builder penaltyFlashScreen() { return this; }
			public Builder penaltyDeathOnNetwork() { return this; }
			public Builder penaltyDeathOnCleartextNetwork() { return this; }
			public Builder penaltyDeathOnFileUriExposure() { return this; }
			public Builder penaltyListener(Executor executor, OnThreadViolationListener listener) {
				this.listener = listener;
				this.executor = executor;
				return this;
			}
			public ThreadPolicy build() {
				return new ThreadPolicy(listener, executor);
			}
		}
	}

	public static final class VmPolicy {
		public static final VmPolicy LAX = new VmPolicy();

		public static final class Builder {
			private OnVmViolationListener listener;
			private Executor executor;

			public Builder() {}

			public Builder(VmPolicy policy) {}

			public Builder detectAll() { return this; }
			public Builder detectNetwork() { return this; }
			public Builder detectDiskReads() { return this; }
			public Builder detectDiskWrites() { return this; }
			public Builder detectCustomSlowCalls() { return this; }
			public Builder detectResourceMismatches() { return this; }
			public Builder detectUnbufferedIo() { return this; }
			public Builder detectExplicitGc() { return this; }
			public Builder detectActivityLeaks() { return this; }
			public Builder detectLeakedSqlLiteObjects() { return this; }
			public Builder detectLeakedClosableObjects() { return this; }
			public Builder detectLeakedRegistrationObjects() { return this; }
			public Builder detectFileUriExposure() { return this; }
			public Builder detectCleartextNetwork() { return this; }
			public Builder detectContentUriWithoutPermission() { return this; }
			public Builder detectUntaggedSockets() { return this; }
			public Builder detectNonSdkApiUsage() { return this; }
			public Builder detectCredentialProtectedWhileLocked() { return this; }
			public Builder detectImplicitDirectBoot() { return this; }
			public Builder detectIncorrectContextUse() { return this; }
			public Builder detectUnsafeIntentLaunch() { return this; }
			public Builder detectBlockedBackgroundActivityLaunch() { return this; }
			public Builder permitAll() { return this; }
			public Builder permitNetwork() { return this; }
			public Builder permitDiskReads() { return this; }
			public Builder permitDiskWrites() { return this; }
			public Builder permitCustomSlowCalls() { return this; }
			public Builder permitResourceMismatches() { return this; }
			public Builder permitUnbufferedIo() { return this; }
			public Builder permitExplicitGc() { return this; }
			public Builder permitNonSdkApiUsage() { return this; }
			public Builder permitUnsafeIntentLaunch() { return this; }
			public Builder permitActivityLeaks() { return this; }
			public Builder permitLeakedClosableObjects() { return this; }
			public Builder penaltyLog() { return this; }
			public Builder penaltyDeath() { return this; }
			public Builder penaltyDialog() { return this; }
			public Builder penaltyDropBox() { return this; }
			public Builder penaltyFlashScreen() { return this; }
			public Builder penaltyDeathOnNetwork() { return this; }
			public Builder penaltyDeathOnCleartextNetwork() { return this; }
			public Builder penaltyDeathOnFileUriExposure() { return this; }
			public Builder penaltyListener(Executor executor, OnVmViolationListener listener) {
				this.listener = listener;
				this.executor = executor;
				return this;
			}
			public Builder setClassInstanceLimit(Class klass, int instanceLimit) { return this; }
			public VmPolicy build() {
				return new VmPolicy();
			}
		}
	}
}
