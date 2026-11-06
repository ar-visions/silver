package android.service.chooser;

// android.jar's shape, made by gen.py
public final class ChooserResult implements android.os.Parcelable {
	// GEN member:android.service.chooser.ChooserResult:public static final int CHOOSER_RESULT_COPY = 1;
	public static final int CHOOSER_RESULT_COPY = 1;

	// GEN member:android.service.chooser.ChooserResult:public static final int CHOOSER_RESULT_EDIT = 2;
	public static final int CHOOSER_RESULT_EDIT = 2;

	// GEN member:android.service.chooser.ChooserResult:public static final int CHOOSER_RESULT_SELECTED_COMPONENT = 0;
	public static final int CHOOSER_RESULT_SELECTED_COMPONENT = 0;

	// GEN member:android.service.chooser.ChooserResult:public static final int CHOOSER_RESULT_UNKNOWN = -1;
	public static final int CHOOSER_RESULT_UNKNOWN = -1;

	// GEN member:android.service.chooser.ChooserResult:public static final android.os.Parcelable$Creator<android.service.chooser.ChooserResult> CREATOR;
	public static final android.os.Parcelable.Creator<android.service.chooser.ChooserResult> CREATOR = null;

	// GEN member:android.service.chooser.ChooserResult:public int getType();
	public int getType() { return 0; }

	// GEN member:android.service.chooser.ChooserResult:public android.content.ComponentName getSelectedComponent();
	public android.content.ComponentName getSelectedComponent() { return null; }

	// GEN member:android.service.chooser.ChooserResult:public boolean isShortcut();
	public boolean isShortcut() { return false; }

	// GEN member:android.service.chooser.ChooserResult:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.service.chooser.ChooserResult:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.service.chooser.ChooserResult:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.service.chooser.ChooserResult:public int hashCode();
	public int hashCode() { return 0; }

	protected ChooserResult() {}
}
