package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class GetSchemaResponse implements android.os.Parcelable {
	// GEN member:android.app.appsearch.GetSchemaResponse:public static final android.os.Parcelable$Creator<android.app.appsearch.GetSchemaResponse> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.GetSchemaResponse> CREATOR = null;

	// GEN member:android.app.appsearch.GetSchemaResponse:public int getVersion();
	public int getVersion() { return 0; }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Set<android.app.appsearch.AppSearchSchema> getSchemas();
	public java.util.Set<android.app.appsearch.AppSearchSchema> getSchemas() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Set<java.lang.String> getSchemaTypesNotDisplayedBySystem();
	public java.util.Set<java.lang.String> getSchemaTypesNotDisplayedBySystem() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.PackageIdentifier>> getSchemaTypesVisibleToPackages();
	public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.PackageIdentifier>> getSchemaTypesVisibleToPackages() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Map<java.lang.String, java.util.Set<java.util.Set<java.lang.Integer>>> getRequiredPermissionsForSchemaTypeVisibility();
	public java.util.Map<java.lang.String, java.util.Set<java.util.Set<java.lang.Integer>>> getRequiredPermissionsForSchemaTypeVisibility() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Map<java.lang.String, android.app.appsearch.PackageIdentifier> getPubliclyVisibleSchemas();
	public java.util.Map<java.lang.String, android.app.appsearch.PackageIdentifier> getPubliclyVisibleSchemas() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.SchemaVisibilityConfig>> getSchemaTypesVisibleToConfigs();
	public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.SchemaVisibilityConfig>> getSchemaTypesVisibleToConfigs() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.GetSchemaResponse:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.GetSchemaResponse:public final int describeContents();
	public final int describeContents() { return 0; }

	protected GetSchemaResponse() {}
}
