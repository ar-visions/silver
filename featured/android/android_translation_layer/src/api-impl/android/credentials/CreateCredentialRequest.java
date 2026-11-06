package android.credentials;

// android.jar's shape, made by gen.py
public final class CreateCredentialRequest implements android.os.Parcelable {
	// GEN member:android.credentials.CreateCredentialRequest:public static final android.os.Parcelable$Creator<android.credentials.CreateCredentialRequest> CREATOR;
	public static final android.os.Parcelable.Creator<android.credentials.CreateCredentialRequest> CREATOR = null;

	// GEN member:android.credentials.CreateCredentialRequest:public java.lang.String getType();
	public java.lang.String getType() { return null; }

	// GEN member:android.credentials.CreateCredentialRequest:public android.os.Bundle getCredentialData();
	public android.os.Bundle getCredentialData() { return null; }

	// GEN member:android.credentials.CreateCredentialRequest:public android.os.Bundle getCandidateQueryData();
	public android.os.Bundle getCandidateQueryData() { return null; }

	// GEN member:android.credentials.CreateCredentialRequest:public boolean isSystemProviderRequired();
	public boolean isSystemProviderRequired() { return false; }

	// GEN member:android.credentials.CreateCredentialRequest:public boolean alwaysSendAppInfoToProvider();
	public boolean alwaysSendAppInfoToProvider() { return false; }

	// GEN member:android.credentials.CreateCredentialRequest:public java.lang.String getOrigin();
	public java.lang.String getOrigin() { return null; }

	// GEN member:android.credentials.CreateCredentialRequest:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.credentials.CreateCredentialRequest:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.credentials.CreateCredentialRequest:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected CreateCredentialRequest() {}

	// GEN class:android.credentials.CreateCredentialRequest$Builder
	public final class Builder {
		// GEN member:android.credentials.CreateCredentialRequest$Builder:public android.credentials.CreateCredentialRequest$Builder(java.lang.String, android.os.Bundle, android.os.Bundle);
		public Builder(java.lang.String arg0, android.os.Bundle arg1, android.os.Bundle arg2) {}
	
		// GEN member:android.credentials.CreateCredentialRequest$Builder:public android.credentials.CreateCredentialRequest$Builder setAlwaysSendAppInfoToProvider(boolean);
		public android.credentials.CreateCredentialRequest.Builder setAlwaysSendAppInfoToProvider(boolean arg0) { return this; }
	
		// GEN member:android.credentials.CreateCredentialRequest$Builder:public android.credentials.CreateCredentialRequest$Builder setIsSystemProviderRequired(boolean);
		public android.credentials.CreateCredentialRequest.Builder setIsSystemProviderRequired(boolean arg0) { return this; }
	
		// GEN member:android.credentials.CreateCredentialRequest$Builder:public android.credentials.CreateCredentialRequest$Builder setOrigin(java.lang.String);
		public android.credentials.CreateCredentialRequest.Builder setOrigin(java.lang.String arg0) { return this; }
	
		// GEN member:android.credentials.CreateCredentialRequest$Builder:public android.credentials.CreateCredentialRequest build();
		public android.credentials.CreateCredentialRequest build() { return null; }
	
		protected Builder() {}
	}

}
