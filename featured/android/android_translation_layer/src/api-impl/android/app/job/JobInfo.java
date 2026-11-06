package android.app.job;

import android.content.ComponentName;
import android.net.Uri;
import android.os.PersistableBundle;

public class JobInfo implements android.os.Parcelable {

	private ComponentName service;
	long initialBackoffMillis;
	int backoffPolicy;
	private PersistableBundle extras;
	long periodicMillis;
	private int id;
	boolean running;
	long minLatencyMillis;

	public JobInfo() {}

	public ComponentName getService() {
		return service;
	}

	public PersistableBundle getExtras() {
		return extras;
	}

	public int getId() {
		return id;
	}

	public static long getMinPeriodMillis() {
		return 1000;
	}

	public String toString() {
		return "JobInfo{"
		     + "jobService=" + service
		     + ", initialBackoffMillis=" + initialBackoffMillis
		     + ", backoffPolicy=" + backoffPolicy
		     + ", extras=" + extras
		     + ", periodicMillis=" + periodicMillis
		     + ", id=" + id
		     + '}';
	}

	public static final class Builder {

		private JobInfo jobInfo;

		public Builder(int jobId, ComponentName jobService) {
			jobInfo = new JobInfo();
			jobInfo.id = jobId;
			jobInfo.service = jobService;
		}

		public Builder setBackoffCriteria(long initialBackoffMillis, int backoffPolicy) {
			jobInfo.initialBackoffMillis = initialBackoffMillis;
			jobInfo.backoffPolicy = backoffPolicy;
			return this;
		}

		public Builder setExtras(PersistableBundle extras) {
			jobInfo.extras = extras;
			return this;
		}

		public Builder setMinimumLatency(long minLatencyMillis) {
			jobInfo.minLatencyMillis = minLatencyMillis;
			return this;
		}

		public Builder setOverrideDeadline(long a) {
			return this;
		}

		public Builder setPeriodic(long dummy) {
			jobInfo.periodicMillis = dummy;
			return this;
		}

		public Builder setExpedited(boolean expedited) {
			return this;
		}

		public Builder setImportantWhileForeground(boolean important) {
			return this;
		}

		public Builder setRequiredNetwork(android.net.NetworkRequest request) {
			return this;
		}

		public Builder setTraceTag(String tag) {
			return this;
		}

		public Builder setTriggerContentMaxDelay(long ms) {
			return this;
		}

		public Builder setTriggerContentUpdateDelay(long ms) {
			return this;
		}

		public Builder setPersisted(boolean persisted) {
			return this;
		}

		public Builder setRequiredNetworkType(int networkType) {
			return this;
		}

		public Builder setRequiresCharging(boolean requires_charging) {
			return this;
		}

		public Builder setRequiresDeviceIdle(boolean requires_device_idle) {
			return this;
		}

		public Builder setRequiresBatteryNotLow(boolean requires_battery_not_low) {
			return this;
		}

		public Builder setRequiresStorageNotLow(boolean requires_storage_not_low) {
			return this;
		}

		public Builder addTriggerContentUri(TriggerContentUri triggerContentUri) {
			return this;
		}

		public JobInfo build() {
			return jobInfo;
		}
	
	// GEN member:android.app.job.JobInfo$Builder:public android.app.job.JobInfo$Builder setTransientExtras(android.os.Bundle);
	public android.app.job.JobInfo.Builder setTransientExtras(android.os.Bundle arg0) { return this; }

	// GEN member:android.app.job.JobInfo$Builder:public android.app.job.JobInfo$Builder setUserInitiated(boolean);
	public android.app.job.JobInfo.Builder setUserInitiated(boolean arg0) { return this; }
}

	public static class TriggerContentUri implements android.os.Parcelable {
		public TriggerContentUri(Uri uri, int flags) {}
	}

	// GEN member:android.app.job.JobInfo:public long getIntervalMillis();
	public long getIntervalMillis() { return 0; }

	// GEN member:android.app.job.JobInfo:public boolean isRequireCharging();
	public boolean isRequireCharging() { return false; }
}
