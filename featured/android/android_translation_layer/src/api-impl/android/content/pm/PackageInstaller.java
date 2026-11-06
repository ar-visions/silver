package android.content.pm;

import java.util.Collections;
import java.util.List;

public class PackageInstaller {

	public List getMySessions() {
		return Collections.emptyList();
	}

	public void registerSessionCallback(SessionCallback callback) {}

	public abstract static class SessionCallback {
		public abstract void onActiveChanged(int sessionId, boolean active);

		public abstract void onBadgingChanged(int sessionId);

		public abstract void onCreated(int sessionId);

		public abstract void onFinished(int sessionId, boolean success);

		public abstract void onProgressChanged(int sessionId, float progress);
	}

	// GEN class:android.content.pm.PackageInstaller$SessionInfo
	public class SessionInfo implements android.os.Parcelable {
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int INVALID_ID = -1;
		public static final int INVALID_ID = -1;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int SESSION_ACTIVATION_FAILED = 2;
		public static final int SESSION_ACTIVATION_FAILED = 2;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int SESSION_CONFLICT = 4;
		public static final int SESSION_CONFLICT = 4;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int SESSION_NO_ERROR = 0;
		public static final int SESSION_NO_ERROR = 0;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int SESSION_UNKNOWN_ERROR = 3;
		public static final int SESSION_UNKNOWN_ERROR = 3;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int SESSION_VERIFICATION_FAILED = 1;
		public static final int SESSION_VERIFICATION_FAILED = 1;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int STAGED_SESSION_ACTIVATION_FAILED = 2;
		public static final int STAGED_SESSION_ACTIVATION_FAILED = 2;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int STAGED_SESSION_CONFLICT = 4;
		public static final int STAGED_SESSION_CONFLICT = 4;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int STAGED_SESSION_NO_ERROR = 0;
		public static final int STAGED_SESSION_NO_ERROR = 0;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int STAGED_SESSION_UNKNOWN = 3;
		public static final int STAGED_SESSION_UNKNOWN = 3;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public static final int STAGED_SESSION_VERIFICATION_FAILED = 1;
		public static final int STAGED_SESSION_VERIFICATION_FAILED = 1;
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getSessionId();
		public int getSessionId() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public android.os.UserHandle getUser();
		public android.os.UserHandle getUser() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public java.lang.String getInstallerPackageName();
		public java.lang.String getInstallerPackageName() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public java.lang.String getInstallerAttributionTag();
		public java.lang.String getInstallerAttributionTag() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public float getProgress();
		public float getProgress() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isActive();
		public boolean isActive() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isSealed();
		public boolean isSealed() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getInstallReason();
		public int getInstallReason() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public java.lang.String getAppPackageName();
		public java.lang.String getAppPackageName() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public android.graphics.Bitmap getAppIcon();
		public android.graphics.Bitmap getAppIcon() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public java.lang.CharSequence getAppLabel();
		public java.lang.CharSequence getAppLabel() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public android.content.Intent createDetailsIntent();
		public android.content.Intent createDetailsIntent() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getMode();
		public int getMode() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getInstallLocation();
		public int getInstallLocation() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public long getSize();
		public long getSize() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public android.net.Uri getOriginatingUri();
		public android.net.Uri getOriginatingUri() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getOriginatingUid();
		public int getOriginatingUid() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public android.net.Uri getReferrerUri();
		public android.net.Uri getReferrerUri() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean getDontKillApp();
		public boolean getDontKillApp() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getPackageSource();
		public int getPackageSource() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isMultiPackage();
		public boolean isMultiPackage() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isStaged();
		public boolean isStaged() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isStagedSessionActive();
		public boolean isStagedSessionActive() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getParentSessionId();
		public int getParentSessionId() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean hasParentSessionId();
		public boolean hasParentSessionId() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int[] getChildSessionIds();
		public int[] getChildSessionIds() { return new int[0]; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isStagedSessionApplied();
		public boolean isStagedSessionApplied() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isStagedSessionReady();
		public boolean isStagedSessionReady() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isStagedSessionFailed();
		public boolean isStagedSessionFailed() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getStagedSessionErrorCode();
		public int getStagedSessionErrorCode() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public java.lang.String getStagedSessionErrorMessage();
		public java.lang.String getStagedSessionErrorMessage() { return null; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isCommitted();
		public boolean isCommitted() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public long getCreatedMillis();
		public long getCreatedMillis() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public long getUpdatedMillis();
		public long getUpdatedMillis() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getRequireUserAction();
		public int getRequireUserAction() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int getInstallerUid();
		public int getInstallerUid() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isApplicationEnabledSettingPersistent();
		public boolean isApplicationEnabledSettingPersistent() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isPreApprovalRequested();
		public boolean isPreApprovalRequested() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isRequestUpdateOwnership();
		public boolean isRequestUpdateOwnership() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public boolean isUnarchival();
		public boolean isUnarchival() { return false; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.content.pm.PackageInstaller$SessionInfo:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected SessionInfo() {}
	}


	// GEN member:android.content.pm.PackageInstaller:public java.util.List<android.content.pm.PackageInstaller$SessionInfo> getAllSessions();
	public java.util.List<android.content.pm.PackageInstaller.SessionInfo> getAllSessions() { return new java.util.ArrayList<>(); }

	// GEN member:android.content.pm.PackageInstaller:public android.content.pm.PackageInstaller$SessionInfo getSessionInfo(int);
	public android.content.pm.PackageInstaller.SessionInfo getSessionInfo(int arg0) { return null; }

	// GEN member:android.content.pm.PackageInstaller:public void unregisterSessionCallback(android.content.pm.PackageInstaller$SessionCallback);
	public void unregisterSessionCallback(android.content.pm.PackageInstaller.SessionCallback arg0) {}
}
