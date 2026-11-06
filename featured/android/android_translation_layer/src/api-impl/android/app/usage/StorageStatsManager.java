package android.app.usage;

// android.jar's shape, made by gen.py
public class StorageStatsManager {
	// GEN member:android.app.usage.StorageStatsManager:public long getTotalBytes(java.util.UUID) throws java.io.IOException;
	public long getTotalBytes(java.util.UUID arg0) throws java.io.IOException { return 0; }

	// GEN member:android.app.usage.StorageStatsManager:public long getFreeBytes(java.util.UUID) throws java.io.IOException;
	public long getFreeBytes(java.util.UUID arg0) throws java.io.IOException { return 0; }

	// GEN member:android.app.usage.StorageStatsManager:public android.app.usage.StorageStats queryStatsForPackage(java.util.UUID, java.lang.String, android.os.UserHandle) throws java.io.IOException, android.content.pm.PackageManager$NameNotFoundException;
	public android.app.usage.StorageStats queryStatsForPackage(java.util.UUID arg0, java.lang.String arg1, android.os.UserHandle arg2) throws java.io.IOException, android.content.pm.PackageManager.NameNotFoundException { return null; }

	// GEN member:android.app.usage.StorageStatsManager:public android.app.usage.StorageStats queryStatsForUid(java.util.UUID, int) throws java.io.IOException;
	public android.app.usage.StorageStats queryStatsForUid(java.util.UUID arg0, int arg1) throws java.io.IOException { return null; }

	// GEN member:android.app.usage.StorageStatsManager:public android.app.usage.StorageStats queryStatsForUser(java.util.UUID, android.os.UserHandle) throws java.io.IOException;
	public android.app.usage.StorageStats queryStatsForUser(java.util.UUID arg0, android.os.UserHandle arg1) throws java.io.IOException { return null; }

	protected StorageStatsManager() {}
}
