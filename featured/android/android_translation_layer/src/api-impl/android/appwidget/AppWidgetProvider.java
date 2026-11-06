package android.appwidget;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Slog;

public class AppWidgetProvider extends BroadcastReceiver {

	@Override
	public void onReceive(Context context, Intent intent) {
		Slog.i("AppWidgetProvider", "onReceive(" + intent + ") called");
	}

	// GEN member:android.appwidget.AppWidgetProvider:public void onAppWidgetOptionsChanged(android.content.Context, android.appwidget.AppWidgetManager, int, android.os.Bundle);
	public void onAppWidgetOptionsChanged(android.content.Context arg0, android.appwidget.AppWidgetManager arg1, int arg2, android.os.Bundle arg3) {}

	// GEN member:android.appwidget.AppWidgetProvider:public void onDeleted(android.content.Context, int[]);
	public void onDeleted(android.content.Context arg0, int[] arg1) {}
}
