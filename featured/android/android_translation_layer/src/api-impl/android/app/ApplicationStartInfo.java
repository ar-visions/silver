package android.app;

// android.jar's shape, made by gen.py
public final class ApplicationStartInfo implements android.os.Parcelable {
	// GEN member:android.app.ApplicationStartInfo:public static final android.os.Parcelable$Creator<android.app.ApplicationStartInfo> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.ApplicationStartInfo> CREATOR = null;

	// GEN member:android.app.ApplicationStartInfo:public static final int LAUNCH_MODE_SINGLE_INSTANCE = 2;
	public static final int LAUNCH_MODE_SINGLE_INSTANCE = 2;

	// GEN member:android.app.ApplicationStartInfo:public static final int LAUNCH_MODE_SINGLE_INSTANCE_PER_TASK = 4;
	public static final int LAUNCH_MODE_SINGLE_INSTANCE_PER_TASK = 4;

	// GEN member:android.app.ApplicationStartInfo:public static final int LAUNCH_MODE_SINGLE_TASK = 3;
	public static final int LAUNCH_MODE_SINGLE_TASK = 3;

	// GEN member:android.app.ApplicationStartInfo:public static final int LAUNCH_MODE_SINGLE_TOP = 1;
	public static final int LAUNCH_MODE_SINGLE_TOP = 1;

	// GEN member:android.app.ApplicationStartInfo:public static final int LAUNCH_MODE_STANDARD = 0;
	public static final int LAUNCH_MODE_STANDARD = 0;

	// GEN member:android.app.ApplicationStartInfo:public static final int STARTUP_STATE_ERROR = 1;
	public static final int STARTUP_STATE_ERROR = 1;

	// GEN member:android.app.ApplicationStartInfo:public static final int STARTUP_STATE_FIRST_FRAME_DRAWN = 2;
	public static final int STARTUP_STATE_FIRST_FRAME_DRAWN = 2;

	// GEN member:android.app.ApplicationStartInfo:public static final int STARTUP_STATE_STARTED = 0;
	public static final int STARTUP_STATE_STARTED = 0;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_ALARM = 0;
	public static final int START_REASON_ALARM = 0;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_BACKUP = 1;
	public static final int START_REASON_BACKUP = 1;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_BOOT_COMPLETE = 2;
	public static final int START_REASON_BOOT_COMPLETE = 2;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_BROADCAST = 3;
	public static final int START_REASON_BROADCAST = 3;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_CONTENT_PROVIDER = 4;
	public static final int START_REASON_CONTENT_PROVIDER = 4;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_JOB = 5;
	public static final int START_REASON_JOB = 5;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_LAUNCHER = 6;
	public static final int START_REASON_LAUNCHER = 6;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_LAUNCHER_RECENTS = 7;
	public static final int START_REASON_LAUNCHER_RECENTS = 7;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_OTHER = 8;
	public static final int START_REASON_OTHER = 8;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_PUSH = 9;
	public static final int START_REASON_PUSH = 9;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_SERVICE = 10;
	public static final int START_REASON_SERVICE = 10;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_REASON_START_ACTIVITY = 11;
	public static final int START_REASON_START_ACTIVITY = 11;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_APPLICATION_ONCREATE = 2;
	public static final int START_TIMESTAMP_APPLICATION_ONCREATE = 2;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_BIND_APPLICATION = 3;
	public static final int START_TIMESTAMP_BIND_APPLICATION = 3;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_FIRST_FRAME = 4;
	public static final int START_TIMESTAMP_FIRST_FRAME = 4;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_FORK = 1;
	public static final int START_TIMESTAMP_FORK = 1;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_FULLY_DRAWN = 5;
	public static final int START_TIMESTAMP_FULLY_DRAWN = 5;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_INITIAL_RENDERTHREAD_FRAME = 6;
	public static final int START_TIMESTAMP_INITIAL_RENDERTHREAD_FRAME = 6;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_LAUNCH = 0;
	public static final int START_TIMESTAMP_LAUNCH = 0;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_RESERVED_RANGE_DEVELOPER = 30;
	public static final int START_TIMESTAMP_RESERVED_RANGE_DEVELOPER = 30;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_RESERVED_RANGE_DEVELOPER_START = 21;
	public static final int START_TIMESTAMP_RESERVED_RANGE_DEVELOPER_START = 21;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_RESERVED_RANGE_SYSTEM = 20;
	public static final int START_TIMESTAMP_RESERVED_RANGE_SYSTEM = 20;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TIMESTAMP_SURFACEFLINGER_COMPOSITION_COMPLETE = 7;
	public static final int START_TIMESTAMP_SURFACEFLINGER_COMPOSITION_COMPLETE = 7;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TYPE_COLD = 1;
	public static final int START_TYPE_COLD = 1;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TYPE_HOT = 3;
	public static final int START_TYPE_HOT = 3;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TYPE_UNSET = 0;
	public static final int START_TYPE_UNSET = 0;

	// GEN member:android.app.ApplicationStartInfo:public static final int START_TYPE_WARM = 2;
	public static final int START_TYPE_WARM = 2;

	// GEN member:android.app.ApplicationStartInfo:public int getStartupState();
	public int getStartupState() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public int getPid();
	public int getPid() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public int getRealUid();
	public int getRealUid() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public int getPackageUid();
	public int getPackageUid() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public int getDefiningUid();
	public int getDefiningUid() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public java.lang.String getProcessName();
	public java.lang.String getProcessName() { return null; }

	// GEN member:android.app.ApplicationStartInfo:public int getReason();
	public int getReason() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public java.util.Map<java.lang.Integer, java.lang.Long> getStartupTimestamps();
	public java.util.Map<java.lang.Integer, java.lang.Long> getStartupTimestamps() { return new java.util.HashMap<>(); }

	// GEN member:android.app.ApplicationStartInfo:public int getStartType();
	public int getStartType() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public android.content.Intent getIntent();
	public android.content.Intent getIntent() { return null; }

	// GEN member:android.app.ApplicationStartInfo:public int getLaunchMode();
	public int getLaunchMode() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public boolean wasForceStopped();
	public boolean wasForceStopped() { return false; }

	// GEN member:android.app.ApplicationStartInfo:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.app.ApplicationStartInfo:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.ApplicationStartInfo:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.app.ApplicationStartInfo:public int hashCode();
	public int hashCode() { return 0; }

	protected ApplicationStartInfo() {}
}
