package android.view;

// android.jar's shape, made by gen.py
public final class ContentInfo implements android.os.Parcelable {
	// GEN member:android.view.ContentInfo:public static final android.os.Parcelable$Creator<android.view.ContentInfo> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.ContentInfo> CREATOR = null;

	// GEN member:android.view.ContentInfo:public static final int FLAG_CONVERT_TO_PLAIN_TEXT = 1;
	public static final int FLAG_CONVERT_TO_PLAIN_TEXT = 1;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_APP = 0;
	public static final int SOURCE_APP = 0;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_AUTOFILL = 4;
	public static final int SOURCE_AUTOFILL = 4;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_CLIPBOARD = 1;
	public static final int SOURCE_CLIPBOARD = 1;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_DRAG_AND_DROP = 3;
	public static final int SOURCE_DRAG_AND_DROP = 3;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_INPUT_METHOD = 2;
	public static final int SOURCE_INPUT_METHOD = 2;

	// GEN member:android.view.ContentInfo:public static final int SOURCE_PROCESS_TEXT = 5;
	public static final int SOURCE_PROCESS_TEXT = 5;

	// GEN member:android.view.ContentInfo:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.view.ContentInfo:public android.content.ClipData getClip();
	public android.content.ClipData getClip() { return null; }

	// GEN member:android.view.ContentInfo:public int getSource();
	public int getSource() { return 0; }

	// GEN member:android.view.ContentInfo:public int getFlags();
	public int getFlags() { return 0; }

	// GEN member:android.view.ContentInfo:public android.net.Uri getLinkUri();
	public android.net.Uri getLinkUri() { return null; }

	// GEN member:android.view.ContentInfo:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.view.ContentInfo:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.view.ContentInfo:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected ContentInfo() {}

	// GEN class:android.view.ContentInfo$Builder
	public final class Builder {
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder(android.view.ContentInfo);
		public Builder(android.view.ContentInfo arg0) {}
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder(android.content.ClipData, int);
		public Builder(android.content.ClipData arg0, int arg1) {}
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder setClip(android.content.ClipData);
		public android.view.ContentInfo.Builder setClip(android.content.ClipData arg0) { return this; }
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder setSource(int);
		public android.view.ContentInfo.Builder setSource(int arg0) { return this; }
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder setFlags(int);
		public android.view.ContentInfo.Builder setFlags(int arg0) { return this; }
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder setLinkUri(android.net.Uri);
		public android.view.ContentInfo.Builder setLinkUri(android.net.Uri arg0) { return this; }
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo$Builder setExtras(android.os.Bundle);
		public android.view.ContentInfo.Builder setExtras(android.os.Bundle arg0) { return this; }
	
		// GEN member:android.view.ContentInfo$Builder:public android.view.ContentInfo build();
		public android.view.ContentInfo build() { return null; }
	
		protected Builder() {}
	}

}
