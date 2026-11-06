package android.app;

import android.os.Bundle;
import android.util.Pair;
import android.view.View;

public class ActivityOptions {

	public static ActivityOptions makeSceneTransitionAnimation(Activity activity, Pair<View, String>... pairs) {
		return new ActivityOptions();
	}

	public static ActivityOptions makeSceneTransitionAnimation(Activity activity, View view, String name) {
		return new ActivityOptions();
	}

	public static ActivityOptions makeBasic() {
		return new ActivityOptions();
	}

	public static ActivityOptions makeScaleUpAnimation(View source, int startX, int startY, int startWidth, int startHeight) {
		return new ActivityOptions();
	}

	public Bundle toBundle() {
		return null;
	}

	// GEN member:android.app.ActivityOptions:public static android.app.ActivityOptions makeCustomAnimation(android.content.Context, int, int);
	public static android.app.ActivityOptions makeCustomAnimation(android.content.Context arg0, int arg1, int arg2) { return null; }

	// GEN member:android.app.ActivityOptions:public android.app.ActivityOptions setLaunchDisplayId(int);
	public android.app.ActivityOptions setLaunchDisplayId(int arg0) { return null; }

	// GEN member:android.app.ActivityOptions:public void setPendingIntentBackgroundActivityLaunchAllowed(boolean);
	public void setPendingIntentBackgroundActivityLaunchAllowed(boolean arg0) {}

	// GEN member:android.app.ActivityOptions:public android.app.ActivityOptions setPendingIntentBackgroundActivityStartMode(int);
	public android.app.ActivityOptions setPendingIntentBackgroundActivityStartMode(int arg0) { return null; }

	// GEN member:android.app.ActivityOptions:public android.app.ActivityOptions setShareIdentityEnabled(boolean);
	public android.app.ActivityOptions setShareIdentityEnabled(boolean arg0) { return null; }
}
