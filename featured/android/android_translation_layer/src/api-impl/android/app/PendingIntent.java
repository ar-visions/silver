package android.app;

import android.atl.ATLLoadedApp;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.GestureDetector;

public class PendingIntent implements Parcelable {

	private int requestCode;
	Intent intent;
	int type; // 0: activity, 1: service, 2: broadcast

	private PendingIntent(int requestCode, Intent intent, int type) {
		this.requestCode = requestCode;
		this.intent = intent;
		this.type = type;
	}
	public static PendingIntent getBroadcast(Context context, int requestCode, Intent intent, int flags) {
		return new PendingIntent(requestCode, intent, 2);
	}

	public IntentSender getIntentSender() {
		return null;
	}

	public void send(Context context, int code, Intent intent) {}

	public void send() {
		Context context = ATLLoadedApp.getPrimaryApplication().getApplication();
		if (type == 0) { // type Activity
			context.startActivity(intent);
		} else if (type == 1) { // type Service
			context.startService(intent);
		} else if (type == 2) { // type Broadcast
			context.sendBroadcast(intent);
		}
	}

	public static PendingIntent getActivity(Context context, int requestCode, Intent intent, int flags) {
		return new PendingIntent(requestCode, intent, 0);
	}

	public static PendingIntent getService(Context context, int requestCode, Intent intent, int flags) {
		return new PendingIntent(requestCode, intent, 1);
	}

	public static PendingIntent getActivities(Context context, int requestCode, Intent[] intents, int flags, Bundle options) {
		return new PendingIntent(requestCode, intents[0], 0);
	}

	public String toString() {
		return "PendingIntent [requestCode=" + requestCode + ", intent=" + intent + ", type="
		     + new String[] {"activity", "service", "broadcast"}[type] + "]";
	}

	public void cancel() {}

	public class CanceledException extends android.util.AndroidException {
	}

	public String getCreatorPackage() {
		return ATLLoadedApp.getPrimaryApplication().pkg.packageName;
	}

	public int getCreatorUid() {
		return ATLLoadedApp.getPrimaryApplication().pkg.applicationInfo.uid;
	}

	public static PendingIntent getForegroundService(Context context, int requestCode, Intent intent, int flags) {
		return getService(context, requestCode, intent, flags);
	}

	public int describeContents() {
		return 0;
	}

	// one process: the intent itself rides the parcel
	public void writeToParcel(android.os.Parcel out, int flags) {
		out.writeInt(requestCode);
		out.writeInt(type);
		out.writeParcelable(intent, flags);
	}

	public static final android.os.Parcelable.Creator<android.app.PendingIntent> CREATOR = new android.os.Parcelable.Creator<android.app.PendingIntent>() {
		public PendingIntent createFromParcel(android.os.Parcel in) {
			int code = in.readInt();
			int kind = in.readInt();
			Intent i = (Intent)in.readParcelable(Intent.class.getClassLoader());
			return new PendingIntent(code, i, kind);
		}

		public PendingIntent[] newArray(int size) {
			return new PendingIntent[size];
		}
	};

	// GEN member:android.app.PendingIntent:public static android.app.PendingIntent getActivities(android.content.Context, int, android.content.Intent[], int);
	public static android.app.PendingIntent getActivities(android.content.Context arg0, int arg1, android.content.Intent[] arg2, int arg3) { return null; }

	// GEN member:android.app.PendingIntent:public static android.app.PendingIntent getActivity(android.content.Context, int, android.content.Intent, int, android.os.Bundle);
	public static android.app.PendingIntent getActivity(android.content.Context arg0, int arg1, android.content.Intent arg2, int arg3, android.os.Bundle arg4) { return null; }

	// GEN member:android.app.PendingIntent:public void send(android.os.Bundle) throws android.app.PendingIntent$CanceledException;
	public void send(android.os.Bundle arg0) throws android.app.PendingIntent.CanceledException {}

	// GEN member:android.app.PendingIntent:public java.lang.String getTargetPackage();
	public java.lang.String getTargetPackage() { return null; }

	// GEN class:android.app.PendingIntent$OnFinished
	public interface OnFinished {
		// GEN member:android.app.PendingIntent$OnFinished:public abstract void onSendFinished(android.app.PendingIntent, android.content.Intent, int, java.lang.String, android.os.Bundle);
		default void onSendFinished(android.app.PendingIntent arg0, android.content.Intent arg1, int arg2, java.lang.String arg3, android.os.Bundle arg4) {}
	}

}
