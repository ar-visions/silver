package android.hardware.display;

import android.os.Handler;
import android.view.Display;

public final class DisplayManager {
	public static interface DisplayListener {}

	public Display getDisplay(int dummy) {
		return new Display();
	}

	public void registerDisplayListener(DisplayListener listener, Handler handler) {
	}

	public void unregisterDisplayListener(DisplayListener listener) {}

	public Display[] getDisplays() {
		return new Display[0];
	}

	// GEN member:android.hardware.display.DisplayManager:public android.hardware.display.VirtualDisplay createVirtualDisplay(java.lang.String, int, int, int, android.view.Surface, int);
	public android.hardware.display.VirtualDisplay createVirtualDisplay(java.lang.String arg0, int arg1, int arg2, int arg3, android.view.Surface arg4, int arg5) { return null; }
}
