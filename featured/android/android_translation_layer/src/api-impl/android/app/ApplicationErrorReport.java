package android.app;

import android.content.ComponentName;
import android.content.Context;

public class ApplicationErrorReport implements android.os.Parcelable {
	public static ComponentName getErrorReportReceiver(Context context, String packageName, int appFlags) {
		return new ComponentName(context, packageName);
	}

	// GEN member:android.app.ApplicationErrorReport:public static final android.os.Parcelable$Creator<android.app.ApplicationErrorReport> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.ApplicationErrorReport> CREATOR = null;

	// GEN member:android.app.ApplicationErrorReport:public java.lang.String packageName;
	public java.lang.String packageName;
}
