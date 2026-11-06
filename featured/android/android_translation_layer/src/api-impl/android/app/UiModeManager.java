package android.app;

import android.content.Context;
import android.content.res.Configuration;

public class UiModeManager {

	public int getCurrentModeType() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_TYPE_MASK;
	}

	public int getNightMode() {
		return Context.sys_config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
	}

	// GEN class:android.app.UiModeManager$ContrastChangeListener
	public interface ContrastChangeListener {
		// GEN member:android.app.UiModeManager$ContrastChangeListener:public abstract void onContrastChanged(float);
		default void onContrastChanged(float arg0) {}
	}


	// GEN member:android.app.UiModeManager:public void addContrastChangeListener(java.util.concurrent.Executor, android.app.UiModeManager$ContrastChangeListener);
	public void addContrastChangeListener(java.util.concurrent.Executor arg0, android.app.UiModeManager.ContrastChangeListener arg1) {}

	// GEN member:android.app.UiModeManager:public float getContrast();
	public float getContrast() { return 0; }

	// GEN member:android.app.UiModeManager:public void removeContrastChangeListener(android.app.UiModeManager$ContrastChangeListener);
	public void removeContrastChangeListener(android.app.UiModeManager.ContrastChangeListener arg0) {}

	// GEN member:android.app.UiModeManager:public void setApplicationNightMode(int);
	public void setApplicationNightMode(int arg0) {}
}
