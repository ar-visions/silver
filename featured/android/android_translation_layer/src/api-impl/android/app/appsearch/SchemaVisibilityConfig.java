package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class SchemaVisibilityConfig implements android.os.Parcelable {
	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public static final android.os.Parcelable$Creator<android.app.appsearch.SchemaVisibilityConfig> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.SchemaVisibilityConfig> CREATOR = null;

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public java.util.List<android.app.appsearch.PackageIdentifier> getAllowedPackages();
	public java.util.List<android.app.appsearch.PackageIdentifier> getAllowedPackages() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public java.util.Set<java.util.Set<java.lang.Integer>> getRequiredPermissions();
	public java.util.Set<java.util.Set<java.lang.Integer>> getRequiredPermissions() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public android.app.appsearch.PackageIdentifier getPubliclyVisibleTargetPackage();
	public android.app.appsearch.PackageIdentifier getPubliclyVisibleTargetPackage() { return null; }

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.app.appsearch.SchemaVisibilityConfig:public final int describeContents();
	public final int describeContents() { return 0; }

	protected SchemaVisibilityConfig() {}

	// GEN class:android.app.appsearch.SchemaVisibilityConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder();
		public Builder() {}
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder addAllowedPackage(android.app.appsearch.PackageIdentifier);
		public android.app.appsearch.SchemaVisibilityConfig.Builder addAllowedPackage(android.app.appsearch.PackageIdentifier arg0) { return this; }
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder clearAllowedPackages();
		public android.app.appsearch.SchemaVisibilityConfig.Builder clearAllowedPackages() { return this; }
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder addRequiredPermissions(java.util.Set<java.lang.Integer>);
		public android.app.appsearch.SchemaVisibilityConfig.Builder addRequiredPermissions(java.util.Set<java.lang.Integer> arg0) { return this; }
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder clearRequiredPermissions();
		public android.app.appsearch.SchemaVisibilityConfig.Builder clearRequiredPermissions() { return this; }
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig$Builder setPubliclyVisibleTargetPackage(android.app.appsearch.PackageIdentifier);
		public android.app.appsearch.SchemaVisibilityConfig.Builder setPubliclyVisibleTargetPackage(android.app.appsearch.PackageIdentifier arg0) { return this; }
	
		// GEN member:android.app.appsearch.SchemaVisibilityConfig$Builder:public android.app.appsearch.SchemaVisibilityConfig build();
		public android.app.appsearch.SchemaVisibilityConfig build() { return null; }
	}

}
