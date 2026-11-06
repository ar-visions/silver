package android.view.textclassifier;

// android.jar's shape, made by gen.py
public final class TextClassification implements android.os.Parcelable {
	// GEN member:android.view.textclassifier.TextClassification:public static final android.os.Parcelable$Creator<android.view.textclassifier.TextClassification> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.textclassifier.TextClassification> CREATOR = null;

	// GEN member:android.view.textclassifier.TextClassification:public java.lang.String getText();
	public java.lang.String getText() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public int getEntityCount();
	public int getEntityCount() { return 0; }

	// GEN member:android.view.textclassifier.TextClassification:public java.lang.String getEntity(int);
	public java.lang.String getEntity(int arg0) { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public float getConfidenceScore(java.lang.String);
	public float getConfidenceScore(java.lang.String arg0) { return 0; }

	// GEN member:android.view.textclassifier.TextClassification:public java.util.List<android.app.RemoteAction> getActions();
	public java.util.List<android.app.RemoteAction> getActions() { return new java.util.ArrayList<>(); }

	// GEN member:android.view.textclassifier.TextClassification:public android.graphics.drawable.Drawable getIcon();
	public android.graphics.drawable.Drawable getIcon() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public java.lang.CharSequence getLabel();
	public java.lang.CharSequence getLabel() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public android.content.Intent getIntent();
	public android.content.Intent getIntent() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public android.view.View$OnClickListener getOnClickListener();
	public android.view.View.OnClickListener getOnClickListener() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.view.textclassifier.TextClassification:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.view.textclassifier.TextClassification:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected TextClassification() {}

	// GEN class:android.view.textclassifier.TextClassification$Request
	public final class Request implements android.os.Parcelable {
		// GEN member:android.view.textclassifier.TextClassification$Request:public java.lang.CharSequence getText();
		public java.lang.CharSequence getText() { return null; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public int getStartIndex();
		public int getStartIndex() { return 0; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public int getEndIndex();
		public int getEndIndex() { return 0; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public android.os.LocaleList getDefaultLocales();
		public android.os.LocaleList getDefaultLocales() { return null; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public java.time.ZonedDateTime getReferenceTime();
		public java.time.ZonedDateTime getReferenceTime() { return null; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public java.lang.String getCallingPackageName();
		public java.lang.String getCallingPackageName() { return null; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public android.os.Bundle getExtras();
		public android.os.Bundle getExtras() { return null; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected Request() {}
	
	// GEN class:android.view.textclassifier.TextClassification$Request$Builder
	public final class Builder {
		// GEN member:android.view.textclassifier.TextClassification$Request$Builder:public android.view.textclassifier.TextClassification$Request$Builder(java.lang.CharSequence, int, int);
		public Builder(java.lang.CharSequence arg0, int arg1, int arg2) {}
	
		// GEN member:android.view.textclassifier.TextClassification$Request$Builder:public android.view.textclassifier.TextClassification$Request$Builder setDefaultLocales(android.os.LocaleList);
		public android.view.textclassifier.TextClassification.Request.Builder setDefaultLocales(android.os.LocaleList arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request$Builder:public android.view.textclassifier.TextClassification$Request$Builder setReferenceTime(java.time.ZonedDateTime);
		public android.view.textclassifier.TextClassification.Request.Builder setReferenceTime(java.time.ZonedDateTime arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request$Builder:public android.view.textclassifier.TextClassification$Request$Builder setExtras(android.os.Bundle);
		public android.view.textclassifier.TextClassification.Request.Builder setExtras(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.view.textclassifier.TextClassification$Request$Builder:public android.view.textclassifier.TextClassification$Request build();
		public android.view.textclassifier.TextClassification.Request build() { return null; }
	
		protected Builder() {}
	}

}

}
