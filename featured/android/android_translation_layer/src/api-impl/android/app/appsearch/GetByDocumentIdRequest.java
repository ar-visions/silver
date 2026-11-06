package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class GetByDocumentIdRequest implements android.os.Parcelable {
	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public static final android.os.Parcelable$Creator<android.app.appsearch.GetByDocumentIdRequest> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.GetByDocumentIdRequest> CREATOR = null;

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public static final java.lang.String PROJECTION_SCHEMA_TYPE_WILDCARD = "*";
	public static final java.lang.String PROJECTION_SCHEMA_TYPE_WILDCARD = "*";

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public java.lang.String getNamespace();
	public java.lang.String getNamespace() { return null; }

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public java.util.Set<java.lang.String> getIds();
	public java.util.Set<java.lang.String> getIds() { return new java.util.HashSet<>(); }

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getProjections();
	public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getProjections() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.GetByDocumentIdRequest:public final int describeContents();
	public final int describeContents() { return 0; }

	protected GetByDocumentIdRequest() {}

	// GEN class:android.app.appsearch.GetByDocumentIdRequest$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.GetByDocumentIdRequest$Builder:public android.app.appsearch.GetByDocumentIdRequest$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.GetByDocumentIdRequest$Builder:public android.app.appsearch.GetByDocumentIdRequest$Builder addIds(java.lang.String...);
		public android.app.appsearch.GetByDocumentIdRequest.Builder addIds(java.lang.String... arg0) { return this; }
	
		// GEN member:android.app.appsearch.GetByDocumentIdRequest$Builder:public android.app.appsearch.GetByDocumentIdRequest$Builder addIds(java.util.Collection<java.lang.String>);
		public android.app.appsearch.GetByDocumentIdRequest.Builder addIds(java.util.Collection<java.lang.String> arg0) { return this; }
	
		// GEN member:android.app.appsearch.GetByDocumentIdRequest$Builder:public android.app.appsearch.GetByDocumentIdRequest$Builder addProjection(java.lang.String, java.util.Collection<java.lang.String>);
		public android.app.appsearch.GetByDocumentIdRequest.Builder addProjection(java.lang.String arg0, java.util.Collection<java.lang.String> arg1) { return this; }
	
		// GEN member:android.app.appsearch.GetByDocumentIdRequest$Builder:public android.app.appsearch.GetByDocumentIdRequest build();
		public android.app.appsearch.GetByDocumentIdRequest build() { return null; }
	
		protected Builder() {}
	}

}
