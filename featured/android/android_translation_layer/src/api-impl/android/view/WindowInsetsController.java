package android.view;

import android.view.WindowInsets.Type.InsetsType;

public interface WindowInsetsController {
	void setSystemBarsAppearance(int appearance, int mask);
	void setSystemBarsBehavior(int behavior);
	void show(@InsetsType int types);
	void hide(@InsetsType int types);

	// GEN class:android.view.WindowInsetsController$OnControllableInsetsChangedListener
	public interface OnControllableInsetsChangedListener {
		// GEN member:android.view.WindowInsetsController$OnControllableInsetsChangedListener:public abstract void onControllableInsetsChanged(android.view.WindowInsetsController, int);
		default void onControllableInsetsChanged(android.view.WindowInsetsController arg0, int arg1) {}
	}


	// GEN member:android.view.WindowInsetsController:public abstract void addOnControllableInsetsChangedListener(android.view.WindowInsetsController$OnControllableInsetsChangedListener);
	default void addOnControllableInsetsChangedListener(android.view.WindowInsetsController.OnControllableInsetsChangedListener arg0) {}

	// GEN member:android.view.WindowInsetsController:public abstract int getSystemBarsAppearance();
	default int getSystemBarsAppearance() { return 0; }

	// GEN member:android.view.WindowInsetsController:public abstract void removeOnControllableInsetsChangedListener(android.view.WindowInsetsController$OnControllableInsetsChangedListener);
	default void removeOnControllableInsetsChangedListener(android.view.WindowInsetsController.OnControllableInsetsChangedListener arg0) {}
}
