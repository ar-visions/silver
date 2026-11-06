package android.appwidget;

import android.content.ComponentName;
import android.content.Context;

public class AppWidgetManager {

	public static AppWidgetManager getInstance(Context context) {
		return new AppWidgetManager();
	}

	public int[] getAppWidgetIds(ComponentName provider) {
		return new int[0];
	}

	public void notifyAppWidgetViewDataChanged(int appWidgetId, int viewId) {}

	public void notifyAppWidgetViewDataChanged(int[] appWidgetIds, int viewId) {}

	// GEN member:android.appwidget.AppWidgetManager:public android.appwidget.AppWidgetProviderInfo getAppWidgetInfo(int);
	public android.appwidget.AppWidgetProviderInfo getAppWidgetInfo(int arg0) { return null; }

	// GEN member:android.appwidget.AppWidgetManager:public android.os.Bundle getAppWidgetOptions(int);
	public android.os.Bundle getAppWidgetOptions(int arg0) { return null; }

	// GEN member:android.appwidget.AppWidgetManager:public java.util.List<android.appwidget.AppWidgetProviderInfo> getInstalledProviders();
	public java.util.List<android.appwidget.AppWidgetProviderInfo> getInstalledProviders() { return new java.util.ArrayList<>(); }

	// GEN member:android.appwidget.AppWidgetManager:public boolean isRequestPinAppWidgetSupported();
	public boolean isRequestPinAppWidgetSupported() { return false; }

	// GEN member:android.appwidget.AppWidgetManager:public boolean requestPinAppWidget(android.content.ComponentName, android.os.Bundle, android.app.PendingIntent);
	public boolean requestPinAppWidget(android.content.ComponentName arg0, android.os.Bundle arg1, android.app.PendingIntent arg2) { return false; }

	// GEN member:android.appwidget.AppWidgetManager:public void updateAppWidget(int[], android.widget.RemoteViews);
	public void updateAppWidget(int[] arg0, android.widget.RemoteViews arg1) {}

	// GEN member:android.appwidget.AppWidgetManager:public void updateAppWidget(int, android.widget.RemoteViews);
	public void updateAppWidget(int arg0, android.widget.RemoteViews arg1) {}

	// GEN member:android.appwidget.AppWidgetManager:public void partiallyUpdateAppWidget(int, android.widget.RemoteViews);
	public void partiallyUpdateAppWidget(int arg0, android.widget.RemoteViews arg1) {}

	// GEN member:android.appwidget.AppWidgetManager:public void updateAppWidgetOptions(int, android.os.Bundle);
	public void updateAppWidgetOptions(int arg0, android.os.Bundle arg1) {}
}
