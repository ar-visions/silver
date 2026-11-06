package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class SetSchemaResponse implements android.os.Parcelable {
	// GEN member:android.app.appsearch.SetSchemaResponse:public static final android.os.Parcelable$Creator<android.app.appsearch.SetSchemaResponse> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.SetSchemaResponse> CREATOR = null;

	// GEN member:android.app.appsearch.SetSchemaResponse:public java.util.List<android.app.appsearch.SetSchemaResponse$MigrationFailure> getMigrationFailures();
	public java.util.List<android.app.appsearch.SetSchemaResponse.MigrationFailure> getMigrationFailures() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SetSchemaResponse:public java.util.Set<java.lang.String> getDeletedTypes();
	public java.util.Set<java.lang.String> getDeletedTypes() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SetSchemaResponse:public java.util.Set<java.lang.String> getMigratedTypes();
	public java.util.Set<java.lang.String> getMigratedTypes() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SetSchemaResponse:public java.util.Set<java.lang.String> getIncompatibleTypes();
	public java.util.Set<java.lang.String> getIncompatibleTypes() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.SetSchemaResponse:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.SetSchemaResponse:public final int describeContents();
	public final int describeContents() { return 0; }

	protected SetSchemaResponse() {}

	// GEN class:android.app.appsearch.SetSchemaResponse$MigrationFailure
	public class MigrationFailure implements android.os.Parcelable {
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public android.app.appsearch.SetSchemaResponse$MigrationFailure(java.lang.String, java.lang.String, java.lang.String, android.app.appsearch.AppSearchResult<?>);
		public MigrationFailure(java.lang.String arg0, java.lang.String arg1, java.lang.String arg2, android.app.appsearch.AppSearchResult<?> arg3) {}
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public java.lang.String getNamespace();
		public java.lang.String getNamespace() { return null; }
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public java.lang.String getDocumentId();
		public java.lang.String getDocumentId() { return null; }
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public java.lang.String getSchemaType();
		public java.lang.String getSchemaType() { return null; }
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public android.app.appsearch.AppSearchResult<java.lang.Void> getAppSearchResult();
		public android.app.appsearch.AppSearchResult<java.lang.Void> getAppSearchResult() { return null; }
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		// GEN member:android.app.appsearch.SetSchemaResponse$MigrationFailure:public final int describeContents();
		public final int describeContents() { return 0; }
	
		protected MigrationFailure() {}
	}

}
