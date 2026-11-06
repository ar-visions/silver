package android.app;

import android.media.AudioAttributes;
import android.net.Uri;

public class NotificationChannel implements android.os.Parcelable {

	public NotificationChannel(String id, CharSequence name, int importance) {}

	public void setLockscreenVisibility(int a) {}
	public void setShowBadge(boolean a) {}
	public void setGroup(String grp) {}
	public void enableLights(boolean en) {}
	public void setLightColor(int color) {}
	public void setVibrationPattern(long[] pattern) {}
	public void enableVibration(boolean en) {}
	public void setSound(Uri uri, AudioAttributes attrs) {}
	public boolean shouldShowLights() { return false; }
	public int getLightColor() { return 0; }
	public boolean shouldVibrate() { return false; }
	public Uri getSound() { return null; }
	public void setDescription(String description) {}
	public void setBypassDnd(boolean bypassDnd) {}

	// GEN member:android.app.NotificationChannel:public boolean canShowBadge();
	public boolean canShowBadge() { return false; }

	// GEN member:android.app.NotificationChannel:public java.lang.String getGroup();
	public java.lang.String getGroup() { return null; }

	// GEN member:android.app.NotificationChannel:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.app.NotificationChannel:public int getImportance();
	public int getImportance() { return 0; }

	// GEN member:android.app.NotificationChannel:public int getLockscreenVisibility();
	public int getLockscreenVisibility() { return 0; }

	// GEN member:android.app.NotificationChannel:public java.lang.CharSequence getName();
	public java.lang.CharSequence getName() { return null; }

	// GEN member:android.app.NotificationChannel:public void setName(java.lang.CharSequence);
	public void setName(java.lang.CharSequence arg0) {}

	// GEN member:android.app.NotificationChannel:public android.media.AudioAttributes getAudioAttributes();
	public android.media.AudioAttributes getAudioAttributes() { return null; }

	// GEN member:android.app.NotificationChannel:public java.lang.String getDescription();
	public java.lang.String getDescription() { return null; }
}
