package android.os;

public class UserManager {
	public boolean isUserUnlocked() {
		return true;
	}
	public static boolean supportsMultipleUsers() {
		return false;
	}

	public long getSerialNumberForUser(UserHandle user) {
		return user.getIdentifier();
	}

	// one user, the owner, with no restrictions set
	public boolean hasUserRestriction(String restrictionKey) {
		return false;
	}

	public Bundle getUserRestrictions() {
		return new Bundle();
	}

	public Bundle getApplicationRestrictions(String packageName) {
		return new Bundle();
	}

	public boolean isUserUnlocked(UserHandle user) {
		return true;
	}

	public boolean isSystemUser() {
		return true;
	}

	public boolean isManagedProfile() {
		return false;
	}

	public boolean isDemoUser() {
		return false;
	}

	public boolean isUserAGoat() {
		return false;
	}

	public boolean isUserRunning(UserHandle user) {
		return true;
	}

	public int getUserCount() {
		return 1;
	}

	public String getUserName() {
		return "owner";
	}

	public java.util.List<UserHandle> getUserProfiles() {
		return java.util.Collections.singletonList(Process.myUserHandle());
	}

	public UserHandle getUserForSerialNumber(long serialNumber) {
		return Process.myUserHandle();
	}
}
