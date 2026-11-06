package android.net.http;

// android.jar's shape, made by gen.py
public class ConnectionMigrationOptions {
	// GEN member:android.net.http.ConnectionMigrationOptions:public static final int MIGRATION_OPTION_DISABLED = 2;
	public static final int MIGRATION_OPTION_DISABLED = 2;

	// GEN member:android.net.http.ConnectionMigrationOptions:public static final int MIGRATION_OPTION_ENABLED = 1;
	public static final int MIGRATION_OPTION_ENABLED = 1;

	// GEN member:android.net.http.ConnectionMigrationOptions:public static final int MIGRATION_OPTION_UNSPECIFIED = 0;
	public static final int MIGRATION_OPTION_UNSPECIFIED = 0;

	// GEN member:android.net.http.ConnectionMigrationOptions:public int getDefaultNetworkMigration();
	public int getDefaultNetworkMigration() { return 0; }

	// GEN member:android.net.http.ConnectionMigrationOptions:public int getPathDegradationMigration();
	public int getPathDegradationMigration() { return 0; }

	// GEN member:android.net.http.ConnectionMigrationOptions:public int getAllowNonDefaultNetworkUsage();
	public int getAllowNonDefaultNetworkUsage() { return 0; }

	protected ConnectionMigrationOptions() {}

	// GEN class:android.net.http.ConnectionMigrationOptions$Builder
	public final class Builder {
		// GEN member:android.net.http.ConnectionMigrationOptions$Builder:public android.net.http.ConnectionMigrationOptions$Builder();
		public Builder() {}
	
		// GEN member:android.net.http.ConnectionMigrationOptions$Builder:public android.net.http.ConnectionMigrationOptions$Builder setDefaultNetworkMigration(int);
		public android.net.http.ConnectionMigrationOptions.Builder setDefaultNetworkMigration(int arg0) { return this; }
	
		// GEN member:android.net.http.ConnectionMigrationOptions$Builder:public android.net.http.ConnectionMigrationOptions$Builder setPathDegradationMigration(int);
		public android.net.http.ConnectionMigrationOptions.Builder setPathDegradationMigration(int arg0) { return this; }
	
		// GEN member:android.net.http.ConnectionMigrationOptions$Builder:public android.net.http.ConnectionMigrationOptions$Builder setAllowNonDefaultNetworkUsage(int);
		public android.net.http.ConnectionMigrationOptions.Builder setAllowNonDefaultNetworkUsage(int arg0) { return this; }
	
		// GEN member:android.net.http.ConnectionMigrationOptions$Builder:public android.net.http.ConnectionMigrationOptions build();
		public android.net.http.ConnectionMigrationOptions build() { return null; }
	}

}
