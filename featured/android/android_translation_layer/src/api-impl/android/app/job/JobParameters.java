package android.app.job;

import android.net.Uri;
import android.os.Parcelable;
import android.os.PersistableBundle;

public class JobParameters implements Parcelable {

	public static final Creator<JobParameters> CREATOR = null;

	JobInfo jobInfo;

	JobParameters(JobInfo jobInfo) {
		this.jobInfo = jobInfo;
	}

	public PersistableBundle getExtras() {
		return jobInfo.getExtras();
	}

	public Uri[] getTriggeredContentUris() {
		return new Uri[0];
	}

	public String[] getTriggeredContentAuthorities() {
		return new String[0];
	}

	public android.net.Network getNetwork() {
		return null;
	}

	// STOP_REASON_UNDEFINED
	public int getStopReason() {
		return 0;
	}

	// GEN member:android.app.job.JobParameters:public int getJobId();
	public int getJobId() { return 0; }

	// GEN member:android.app.job.JobParameters:public void completeWork(android.app.job.JobWorkItem);
	public void completeWork(android.app.job.JobWorkItem arg0) {}

	// GEN member:android.app.job.JobParameters:public android.app.job.JobWorkItem dequeueWork();
	public android.app.job.JobWorkItem dequeueWork() { return null; }

	// GEN member:android.app.job.JobParameters:public android.os.Bundle getTransientExtras();
	public android.os.Bundle getTransientExtras() { return null; }

	// GEN member:android.app.job.JobParameters:public boolean isOverrideDeadlineExpired();
	public boolean isOverrideDeadlineExpired() { return false; }
}
