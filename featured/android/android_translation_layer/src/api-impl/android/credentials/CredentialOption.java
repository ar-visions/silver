package android.credentials;

// android.jar's shape, made by gen.py
public final class CredentialOption implements android.os.Parcelable {
	// GEN member:android.credentials.CredentialOption:public static final android.os.Parcelable$Creator<android.credentials.CredentialOption> CREATOR;
	public static final android.os.Parcelable.Creator<android.credentials.CredentialOption> CREATOR = null;

	// GEN member:android.credentials.CredentialOption:public java.lang.String getType();
	public java.lang.String getType() { return null; }

	// GEN member:android.credentials.CredentialOption:public android.os.Bundle getCredentialRetrievalData();
	public android.os.Bundle getCredentialRetrievalData() { return null; }

	// GEN member:android.credentials.CredentialOption:public android.os.Bundle getCandidateQueryData();
	public android.os.Bundle getCandidateQueryData() { return null; }

	// GEN member:android.credentials.CredentialOption:public boolean isSystemProviderRequired();
	public boolean isSystemProviderRequired() { return false; }

	// GEN member:android.credentials.CredentialOption:public java.util.Set<android.content.ComponentName> getAllowedProviders();
	public java.util.Set<android.content.ComponentName> getAllowedProviders() { return new java.util.HashSet<>(); }

	// GEN member:android.credentials.CredentialOption:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.credentials.CredentialOption:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.credentials.CredentialOption:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected CredentialOption() {}

	// GEN class:android.credentials.CredentialOption$Builder
	public final class Builder {
		// GEN member:android.credentials.CredentialOption$Builder:public android.credentials.CredentialOption$Builder(java.lang.String, android.os.Bundle, android.os.Bundle);
		public Builder(java.lang.String arg0, android.os.Bundle arg1, android.os.Bundle arg2) {}
	
		// GEN member:android.credentials.CredentialOption$Builder:public android.credentials.CredentialOption$Builder setIsSystemProviderRequired(boolean);
		public android.credentials.CredentialOption.Builder setIsSystemProviderRequired(boolean arg0) { return this; }
	
		// GEN member:android.credentials.CredentialOption$Builder:public android.credentials.CredentialOption$Builder addAllowedProvider(android.content.ComponentName);
		public android.credentials.CredentialOption.Builder addAllowedProvider(android.content.ComponentName arg0) { return this; }
	
		// GEN member:android.credentials.CredentialOption$Builder:public android.credentials.CredentialOption$Builder setAllowedProviders(java.util.Set<android.content.ComponentName>);
		public android.credentials.CredentialOption.Builder setAllowedProviders(java.util.Set<android.content.ComponentName> arg0) { return this; }
	
		// GEN member:android.credentials.CredentialOption$Builder:public android.credentials.CredentialOption build();
		public android.credentials.CredentialOption build() { return null; }
	
		protected Builder() {}
	}

}
