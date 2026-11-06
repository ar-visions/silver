package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class SetSchemaRequest {
	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_ASSISTANT_APP_SEARCH_DATA = 6;
	public static final int READ_ASSISTANT_APP_SEARCH_DATA = 6;

	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_CALENDAR = 2;
	public static final int READ_CALENDAR = 2;

	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_CONTACTS = 3;
	public static final int READ_CONTACTS = 3;

	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_EXTERNAL_STORAGE = 4;
	public static final int READ_EXTERNAL_STORAGE = 4;

	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_HOME_APP_SEARCH_DATA = 5;
	public static final int READ_HOME_APP_SEARCH_DATA = 5;

	// GEN member:android.app.appsearch.SetSchemaRequest:public static final int READ_SMS = 1;
	public static final int READ_SMS = 1;

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Set<android.app.appsearch.AppSearchSchema> getSchemas();
	public java.util.Set<android.app.appsearch.AppSearchSchema> getSchemas() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Set<java.lang.String> getSchemasNotDisplayedBySystem();
	public java.util.Set<java.lang.String> getSchemasNotDisplayedBySystem() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.PackageIdentifier>> getSchemasVisibleToPackages();
	public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.PackageIdentifier>> getSchemasVisibleToPackages() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Map<java.lang.String, java.util.Set<java.util.Set<java.lang.Integer>>> getRequiredPermissionsForSchemaTypeVisibility();
	public java.util.Map<java.lang.String, java.util.Set<java.util.Set<java.lang.Integer>>> getRequiredPermissionsForSchemaTypeVisibility() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Map<java.lang.String, android.app.appsearch.PackageIdentifier> getPubliclyVisibleSchemas();
	public java.util.Map<java.lang.String, android.app.appsearch.PackageIdentifier> getPubliclyVisibleSchemas() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.SchemaVisibilityConfig>> getSchemasVisibleToConfigs();
	public java.util.Map<java.lang.String, java.util.Set<android.app.appsearch.SchemaVisibilityConfig>> getSchemasVisibleToConfigs() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SetSchemaRequest:public boolean isForceOverride();
	public boolean isForceOverride() { return false; }

	// GEN member:android.app.appsearch.SetSchemaRequest:public int getVersion();
	public int getVersion() { return 0; }

	protected SetSchemaRequest() {}

	// GEN class:android.app.appsearch.SetSchemaRequest$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder();
		public Builder() {}
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder addSchemas(android.app.appsearch.AppSearchSchema...);
		public android.app.appsearch.SetSchemaRequest.Builder addSchemas(android.app.appsearch.AppSearchSchema... arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder addSchemas(java.util.Collection<android.app.appsearch.AppSearchSchema>);
		public android.app.appsearch.SetSchemaRequest.Builder addSchemas(java.util.Collection<android.app.appsearch.AppSearchSchema> arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder setSchemaTypeDisplayedBySystem(java.lang.String, boolean);
		public android.app.appsearch.SetSchemaRequest.Builder setSchemaTypeDisplayedBySystem(java.lang.String arg0, boolean arg1) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder addRequiredPermissionsForSchemaTypeVisibility(java.lang.String, java.util.Set<java.lang.Integer>);
		public android.app.appsearch.SetSchemaRequest.Builder addRequiredPermissionsForSchemaTypeVisibility(java.lang.String arg0, java.util.Set<java.lang.Integer> arg1) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder clearRequiredPermissionsForSchemaTypeVisibility(java.lang.String);
		public android.app.appsearch.SetSchemaRequest.Builder clearRequiredPermissionsForSchemaTypeVisibility(java.lang.String arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder setSchemaTypeVisibilityForPackage(java.lang.String, boolean, android.app.appsearch.PackageIdentifier);
		public android.app.appsearch.SetSchemaRequest.Builder setSchemaTypeVisibilityForPackage(java.lang.String arg0, boolean arg1, android.app.appsearch.PackageIdentifier arg2) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder setPubliclyVisibleSchema(java.lang.String, android.app.appsearch.PackageIdentifier);
		public android.app.appsearch.SetSchemaRequest.Builder setPubliclyVisibleSchema(java.lang.String arg0, android.app.appsearch.PackageIdentifier arg1) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder addSchemaTypeVisibleToConfig(java.lang.String, android.app.appsearch.SchemaVisibilityConfig);
		public android.app.appsearch.SetSchemaRequest.Builder addSchemaTypeVisibleToConfig(java.lang.String arg0, android.app.appsearch.SchemaVisibilityConfig arg1) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder clearSchemaTypeVisibleToConfigs(java.lang.String);
		public android.app.appsearch.SetSchemaRequest.Builder clearSchemaTypeVisibleToConfigs(java.lang.String arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder setForceOverride(boolean);
		public android.app.appsearch.SetSchemaRequest.Builder setForceOverride(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest$Builder setVersion(int);
		public android.app.appsearch.SetSchemaRequest.Builder setVersion(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SetSchemaRequest$Builder:public android.app.appsearch.SetSchemaRequest build();
		public android.app.appsearch.SetSchemaRequest build() { return null; }
	}

}
