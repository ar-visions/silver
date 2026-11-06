package android.credentials;

// android.jar's shape, made by gen.py
public final class GetCredentialRequest implements android.os.Parcelable {
	// GEN member:android.credentials.GetCredentialRequest:public static final android.os.Parcelable$Creator<android.credentials.GetCredentialRequest> CREATOR;
	public static final android.os.Parcelable.Creator<android.credentials.GetCredentialRequest> CREATOR = null;

	// GEN member:android.credentials.GetCredentialRequest:public java.util.List<android.credentials.CredentialOption> getCredentialOptions();
	public java.util.List<android.credentials.CredentialOption> getCredentialOptions() { return new java.util.ArrayList<>(); }

	// GEN member:android.credentials.GetCredentialRequest:public android.os.Bundle getData();
	public android.os.Bundle getData() { return null; }

	// GEN member:android.credentials.GetCredentialRequest:public java.lang.String getOrigin();
	public java.lang.String getOrigin() { return null; }

	// GEN member:android.credentials.GetCredentialRequest:public boolean alwaysSendAppInfoToProvider();
	public boolean alwaysSendAppInfoToProvider() { return false; }

	// GEN member:android.credentials.GetCredentialRequest:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.credentials.GetCredentialRequest:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.credentials.GetCredentialRequest:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected GetCredentialRequest() {}

	// GEN class:android.credentials.GetCredentialRequest$Builder
	public final class Builder {
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest$Builder(android.os.Bundle);
		public Builder(android.os.Bundle arg0) {}
	
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest$Builder addCredentialOption(android.credentials.CredentialOption);
		public android.credentials.GetCredentialRequest.Builder addCredentialOption(android.credentials.CredentialOption arg0) { return this; }
	
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest$Builder setAlwaysSendAppInfoToProvider(boolean);
		public android.credentials.GetCredentialRequest.Builder setAlwaysSendAppInfoToProvider(boolean arg0) { return this; }
	
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest$Builder setCredentialOptions(java.util.List<android.credentials.CredentialOption>);
		public android.credentials.GetCredentialRequest.Builder setCredentialOptions(java.util.List<android.credentials.CredentialOption> arg0) { return this; }
	
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest$Builder setOrigin(java.lang.String);
		public android.credentials.GetCredentialRequest.Builder setOrigin(java.lang.String arg0) { return this; }
	
		// GEN member:android.credentials.GetCredentialRequest$Builder:public android.credentials.GetCredentialRequest build();
		public android.credentials.GetCredentialRequest build() { return null; }
	
		protected Builder() {}
	}

}
