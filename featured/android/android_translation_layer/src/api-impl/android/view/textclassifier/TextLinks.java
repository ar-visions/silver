package android.view.textclassifier;

// android.jar's shape, made by gen.py
public final class TextLinks implements android.os.Parcelable {
	// GEN member:android.view.textclassifier.TextLinks:public static final int APPLY_STRATEGY_IGNORE = 0;
	public static final int APPLY_STRATEGY_IGNORE = 0;

	// GEN member:android.view.textclassifier.TextLinks:public static final int APPLY_STRATEGY_REPLACE = 1;
	public static final int APPLY_STRATEGY_REPLACE = 1;

	// GEN member:android.view.textclassifier.TextLinks:public static final android.os.Parcelable$Creator<android.view.textclassifier.TextLinks> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.textclassifier.TextLinks> CREATOR = null;

	// GEN member:android.view.textclassifier.TextLinks:public static final int STATUS_DIFFERENT_TEXT = 3;
	public static final int STATUS_DIFFERENT_TEXT = 3;

	// GEN member:android.view.textclassifier.TextLinks:public static final int STATUS_LINKS_APPLIED = 0;
	public static final int STATUS_LINKS_APPLIED = 0;

	// GEN member:android.view.textclassifier.TextLinks:public static final int STATUS_NO_LINKS_APPLIED = 2;
	public static final int STATUS_NO_LINKS_APPLIED = 2;

	// GEN member:android.view.textclassifier.TextLinks:public static final int STATUS_NO_LINKS_FOUND = 1;
	public static final int STATUS_NO_LINKS_FOUND = 1;

	// GEN member:android.view.textclassifier.TextLinks:public static final int STATUS_UNSUPPORTED_CHARACTER = 4;
	public static final int STATUS_UNSUPPORTED_CHARACTER = 4;

	// GEN member:android.view.textclassifier.TextLinks:public java.lang.CharSequence getText();
	public java.lang.CharSequence getText() { return null; }

	// GEN member:android.view.textclassifier.TextLinks:public java.util.Collection<android.view.textclassifier.TextLinks$TextLink> getLinks();
	public java.util.Collection<android.view.textclassifier.TextLinks.TextLink> getLinks() { return new java.util.ArrayList<>(); }

	// GEN member:android.view.textclassifier.TextLinks:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.view.textclassifier.TextLinks:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.view.textclassifier.TextLinks:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.view.textclassifier.TextLinks:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected TextLinks() {}

	// GEN class:android.view.textclassifier.TextLinks$Request
	public final class Request implements android.os.Parcelable {
		// GEN member:android.view.textclassifier.TextLinks$Request:public java.lang.CharSequence getText();
		public java.lang.CharSequence getText() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public android.os.LocaleList getDefaultLocales();
		public android.os.LocaleList getDefaultLocales() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public java.time.ZonedDateTime getReferenceTime();
		public java.time.ZonedDateTime getReferenceTime() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public java.lang.String getCallingPackageName();
		public java.lang.String getCallingPackageName() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public android.os.Bundle getExtras();
		public android.os.Bundle getExtras() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected Request() {}
	
	// GEN class:android.view.textclassifier.TextLinks$Request$Builder
	public final class Builder {
		// GEN member:android.view.textclassifier.TextLinks$Request$Builder:public android.view.textclassifier.TextLinks$Request$Builder(java.lang.CharSequence);
		public Builder(java.lang.CharSequence arg0) {}
	
		// GEN member:android.view.textclassifier.TextLinks$Request$Builder:public android.view.textclassifier.TextLinks$Request$Builder setDefaultLocales(android.os.LocaleList);
		public android.view.textclassifier.TextLinks.Request.Builder setDefaultLocales(android.os.LocaleList arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request$Builder:public android.view.textclassifier.TextLinks$Request$Builder setExtras(android.os.Bundle);
		public android.view.textclassifier.TextLinks.Request.Builder setExtras(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request$Builder:public android.view.textclassifier.TextLinks$Request$Builder setReferenceTime(java.time.ZonedDateTime);
		public android.view.textclassifier.TextLinks.Request.Builder setReferenceTime(java.time.ZonedDateTime arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextLinks$Request$Builder:public android.view.textclassifier.TextLinks$Request build();
		public android.view.textclassifier.TextLinks.Request build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.view.textclassifier.TextLinks$TextLink
	public final class TextLink implements android.os.Parcelable {
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public int getStart();
		public int getStart() { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public int getEnd();
		public int getEnd() { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public int getEntityCount();
		public int getEntityCount() { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public java.lang.String getEntity(int);
		public java.lang.String getEntity(int arg0) { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public float getConfidenceScore(java.lang.String);
		public float getConfidenceScore(java.lang.String arg0) { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public android.os.Bundle getExtras();
		public android.os.Bundle getExtras() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.textclassifier.TextLinks$TextLink:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected TextLink() {}
	}

}
