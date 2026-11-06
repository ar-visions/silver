package android.view.textclassifier;

// android.jar's shape, made by gen.py
public final class TextSelection implements android.os.Parcelable {
	// GEN member:android.view.textclassifier.TextSelection:public static final android.os.Parcelable$Creator<android.view.textclassifier.TextSelection> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.textclassifier.TextSelection> CREATOR = null;

	// GEN member:android.view.textclassifier.TextSelection:public int getSelectionStartIndex();
	public int getSelectionStartIndex() { return 0; }

	// GEN member:android.view.textclassifier.TextSelection:public int getSelectionEndIndex();
	public int getSelectionEndIndex() { return 0; }

	// GEN member:android.view.textclassifier.TextSelection:public int getEntityCount();
	public int getEntityCount() { return 0; }

	// GEN member:android.view.textclassifier.TextSelection:public java.lang.String getEntity(int);
	public java.lang.String getEntity(int arg0) { return null; }

	// GEN member:android.view.textclassifier.TextSelection:public float getConfidenceScore(java.lang.String);
	public float getConfidenceScore(java.lang.String arg0) { return 0; }

	// GEN member:android.view.textclassifier.TextSelection:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.view.textclassifier.TextSelection:public android.view.textclassifier.TextClassification getTextClassification();
	public android.view.textclassifier.TextClassification getTextClassification() { return null; }

	// GEN member:android.view.textclassifier.TextSelection:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.view.textclassifier.TextSelection:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.view.textclassifier.TextSelection:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.view.textclassifier.TextSelection:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected TextSelection() {}

	// GEN class:android.view.textclassifier.TextSelection$Request
	public final class Request implements android.os.Parcelable {
		// GEN member:android.view.textclassifier.TextSelection$Request:public java.lang.CharSequence getText();
		public java.lang.CharSequence getText() { return null; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public int getStartIndex();
		public int getStartIndex() { return 0; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public int getEndIndex();
		public int getEndIndex() { return 0; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public android.os.LocaleList getDefaultLocales();
		public android.os.LocaleList getDefaultLocales() { return null; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public java.lang.String getCallingPackageName();
		public java.lang.String getCallingPackageName() { return null; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public boolean shouldIncludeTextClassification();
		public boolean shouldIncludeTextClassification() { return false; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public android.os.Bundle getExtras();
		public android.os.Bundle getExtras() { return null; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected Request() {}
	
	// GEN class:android.view.textclassifier.TextSelection$Request$Builder
	public final class Builder {
		// GEN member:android.view.textclassifier.TextSelection$Request$Builder:public android.view.textclassifier.TextSelection$Request$Builder(java.lang.CharSequence, int, int);
		public Builder(java.lang.CharSequence arg0, int arg1, int arg2) {}
	
		// GEN member:android.view.textclassifier.TextSelection$Request$Builder:public android.view.textclassifier.TextSelection$Request$Builder setDefaultLocales(android.os.LocaleList);
		public android.view.textclassifier.TextSelection.Request.Builder setDefaultLocales(android.os.LocaleList arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request$Builder:public android.view.textclassifier.TextSelection$Request$Builder setIncludeTextClassification(boolean);
		public android.view.textclassifier.TextSelection.Request.Builder setIncludeTextClassification(boolean arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request$Builder:public android.view.textclassifier.TextSelection$Request$Builder setExtras(android.os.Bundle);
		public android.view.textclassifier.TextSelection.Request.Builder setExtras(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextSelection$Request$Builder:public android.view.textclassifier.TextSelection$Request build();
		public android.view.textclassifier.TextSelection.Request build() { return null; }
	
		protected Builder() {}
	}

}

}
